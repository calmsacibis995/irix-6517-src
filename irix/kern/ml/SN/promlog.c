/***********************************************************************\
*	File:		promlog.c					*
*									*
*	Implements environment variables and logs in certain types	*
*	of flash PROMs such as the AM29F080.  These FPROMs allow two	*
*	operations: clearing any individual bit from 1 to 0, or		*
*	setting an entire sector of the PROM to all 1s.			*
*									*
*	This module is layered on top of the fprom module.		*
*									*
*	NOTE: Please keep in sync with stand version in			*
*		stand/arcs/lib/libkl/ml/promlog.c			*
*									*
\***********************************************************************/

#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/SN/promlog.h>

#ifdef _STANDALONE
#ifdef EMULATE
#include <stdio.h>
#else
#include "libc.h"
#endif
#else
#include <sys/systm.h>
#endif

#define LDEBUG		0
#define TEST_SHORT	0

#if LDEBUG

static int debug_mode = 1;

static void LDUMP(char *s, promlog_t *l)
{
    if (debug_mode)
	printf("%s: act=%d ls=0x%x le=0x%x as=0x%x ae=0x%x pos=0x%x\n",
	       s, l->active,
	       l->log_start, l->log_end,
	       l->alt_start, l->alt_end, l->pos);
}

static void EDUMP(char *s, promlog_ent_t *e)
{
    if (debug_mode)
	printf("%s: v=%d type=%d key_len=%d cpu=%d value_len=%d\n",
	       s, e->valid, e->type, e->key_len, e->cpu_num, e->value_len);
}

static void HDUMP(char *s, promlog_header_t *h)
{
    if (debug_mode)
	printf("%s: magic=%x version=%d sequence=%d\n",
	       s, h->magic, h->version, h->sequence);
}

#define PRINT(a)	if (debug_mode) printf a

#else /* LDEBUG */

#define LDUMP(s, l)
#define EDUMP(s, e)
#define HDUMP(s, h)
#define PRINT(a)

#endif /* LDEBUG */

/*
 * get_entry (INTERNAL)
 */

static int get_entry(promlog_t *l)
{
    int			r;

    LDUMP("get_entry", l);

    if ((l->pos < l->alt_start || l->pos >= l->alt_end) &&
	(l->pos < l->log_start || l->pos >= l->log_end)) {
	r = PROMLOG_ERROR_CORRUPT;
	goto done;
    }

    if ((r = fprom_read(&l->f, l->pos,
			(char *) &l->ent, sizeof (l->ent))) < 0) {
	printf("promlog: Error reading entry at 0x%x: %s\n",
	       l->pos, fprom_errmsg(r));
	r = PROMLOG_ERROR_PROM;
	goto done;
    }

    EDUMP("get_entry", &l->ent);

    r = 0;

 done:

    PRINT(("get_entry: r=%d\n", r));

    return r;
}

/*
 * get_header (INTERNAL)
 */

static int get_header(promlog_t *l, int which, promlog_header_t *h)
{
    int			r;

    LDUMP("get_header", l);
    PRINT(("get_header: which=%d\n", which));

    if ((r = fprom_read(&l->f, (l->sector_base + which) * FPROM_SECTOR_SIZE,
			(char *) h, sizeof (*h))) < 0) {
	printf("promlog: Error reading header %d: %s\n",
	       which, fprom_errmsg(r));
	r = PROMLOG_ERROR_PROM;
	goto done;
    }

    HDUMP("get_header", h);

    r = 0;

 done:

    PRINT(("get_header: r=%d\n", r));

    return r;
}

/*
 * put_header (INTERNAL)
 */

static int put_header(promlog_t *l, int which, promlog_header_t *h)
{
    int			r;

    LDUMP("put_header", l);
    PRINT(("put_header: which=%d\n", which));
    HDUMP("put_header", h);

    if ((r = fprom_write(&l->f, (l->sector_base + which) * FPROM_SECTOR_SIZE,
			 (char *) h, sizeof (*h))) < 0) {
	printf("promlog: Error writing header %d: %s\n",
	       which, fprom_errmsg(r));
	r = PROMLOG_ERROR_PROM;
	goto done;
    }

    r = 0;

 done:

    PRINT(("put_header: r=%d\n", r));

    return r;
}

/*
 * swap_log_alt (INTERNAL)
 */

static void swap_log_alt(promlog_t *l)
{
    int			i;

    i			= l->log_start;
    l->log_start	= l->alt_start;
    l->alt_start	= i;

    i			= l->log_end;
    l->log_end		= l->alt_end;
    l->alt_end		= i;

    l->active		= 1 - l->active;

    PRINT(("swap_log_alt: active now %d\n", l->active));
}

/*
 * set_ranges (INTERNAL)
 */

static void set_ranges(promlog_t *l)
{
    PRINT(("set_ranges\n"));

    l->log_start	= (l->sector_base * FPROM_SECTOR_SIZE +
			   PROMLOG_OFFSET_ENTRY0);
    l->log_end		= (l->sector_base + 1) * FPROM_SECTOR_SIZE;

    l->alt_start	= ((l->sector_base + 1) * FPROM_SECTOR_SIZE +
			   PROMLOG_OFFSET_ENTRY0);
    l->alt_end		= (l->sector_base + 2) * FPROM_SECTOR_SIZE;

#if TEST_SHORT
    l->log_end		= l->log_start + 0x40;
    l->alt_end		= l->alt_start + 0x40;
#endif /* TEST_SHORT */
}

/*
 * promlog_init
 *
 *   Initializes the promlog_t handle for accessing a specified prom
 *   log.  Identifies the active sector and sets the current position
 *   to the first log entry (or end marker if log is empty).
 *
 *   Fprom_base is a pointer to the base of the PROM containing the
 *   log.  Sector_base is the first sector in the PROM to use
 *   (typically 14).  Fprom_type is FPROM_DEV_HUB or FPROM_DEV_IO6.
 */

int promlog_init(promlog_t *l, fprom_t *f,
		 int sector_base, int cpu_num)
{
    promlog_header_t	h1, h2;
    int			r;

#if defined (_STANDALONE)
    memcpy(&l->f, f, sizeof (*f));
#else  /* _STANDALONE */
    bcopy (f, &l->f, sizeof (*f));
#endif /* _STANDALONE */

    l->sector_base	= sector_base;
    l->cpu_num		= cpu_num;

    l->active		= 0;

    set_ranges(l);

    LDUMP("promlog_init", l);

    if ((r = get_header(l, 0, &h1)) < 0 ||
	(r = get_header(l, 1, &h2)) < 0)
	goto done;

    /*
     * Determine the active sector.
     */

    if (h1.magic != PROMLOG_MAGIC && h2.magic != PROMLOG_MAGIC) {
	r = PROMLOG_ERROR_MAGIC;
	goto done;
    }

    if (h1.magic != PROMLOG_MAGIC ||
	h2.magic == PROMLOG_MAGIC && h2.sequence > h1.sequence)
	swap_log_alt(l);

    l->pos	= l->log_start;

    LDUMP("promlog_init", l);

    r = 0;

 done:

    PRINT(("promlog_init: r=%d\n", r));

    return r;
}

/*
 * promlog_clear
 *
 *   Flashes the entire PROM log to empty and writes the magic,
 *   version, and sequence number.  The sequence number is guaranteed
 *   to be larger than any previous sequence number (if there was any).
 */

int promlog_clear(promlog_t *l, int init)
{
    promlog_header_t	h1, h2;
    int			mask, r;
    int			sequence;

    LDUMP("promlog_clear", l);

    if (init) {
        if ((r = get_header(l, 0, &h1)) < 0 ||
                (r = get_header(l, 1, &h2)) < 0)
        goto done;

        HDUMP("promlog_clear", &h1);
        HDUMP("promlog_clear", &h2);

        if (h1.magic == PROMLOG_MAGIC && h2.magic == PROMLOG_MAGIC)
            sequence = h1.sequence > h2.sequence ? h1.sequence : h2.sequence;
        else if (h1.magic == PROMLOG_MAGIC)
            sequence = h1.sequence;
        else if (h2.magic == PROMLOG_MAGIC)
            sequence = h2.sequence;
        else
            sequence = 0;

        memset(&h1, 0xff, sizeof (h1));

        h1.magic                = PROMLOG_MAGIC;
        h1.version              = PROMLOG_VERSION;
        h1.sequence             = sequence + 1;

        mask = ((1 << l->sector_base + 2) -
            (1 << l->sector_base    ));

        PRINT(("promlog: Flashing log sectors, please wait.\n"));

        if ((r = fprom_flash_sectors(&l->f, mask)) < 0) {
            printf("promlog: Could not flash sectors: %s\n", fprom_errmsg(r));
            r = PROMLOG_ERROR_PROM;
            goto done;
        }

        l->active = 0;

        set_ranges(l);

        l->pos = l->log_start;

        if ((r = put_header(l, 0, &h1)) < 0)
            goto done;

        r = 0;
    }
    else {
        if ((r = promlog_compact(l)) < 0)
            goto done;

        r = promlog_compact(l);
    }

 done:

    PRINT(("promlog_clear: r=%d\n", r));

    return r;
}

/*
 * promlog_type
 *
 *   Returns the type of the entry at the current position
 *   (PROMLOG_TYPE_xxx)
 */

int promlog_type(promlog_t *l)
{
    int			r;

    LDUMP("promlog_type", l);

    if ((r = get_entry(l)) >= 0)
	r = l->ent.valid ? l->ent.type : PROMLOG_ERROR_POS;

    PRINT(("promlog_type: r=%d\n", r));

    return r;
}

/*
 * promlog_first
 *
 *   Sets the current position to the first valid entry in the log.
 */

int promlog_first(promlog_t *l, int type)
{
    int			r;

    LDUMP("promlog_first", l);
    PRINT(("promlog_first: type=%d\n", type));

    l->pos	= (type == PROMLOG_TYPE_LOG) ? l->alt_start : l->log_start;

    if ((r = get_entry(l)) < 0)
	goto done;

    if (! l->ent.valid || (type != PROMLOG_TYPE_ANY && l->ent.type != type)) {
	r = promlog_next(l, type);
	goto done;
    }

    r = 0;

 done:

    PRINT(("promlog_first: r=%d\n", r));

    return r;
}

/*
 * promlog_prev
 *
 *   Moves the current position back until it reaches a valid entry of
 *   the specified type (or PROMLOG_TYPE_ANY).  The technique used is
 *   to search backwards for bit 7 being set (bit 7 can only be set in
 *   the first byte of an entry).  If the pointer is already at the
 *   start of the log, returns PROMLOG_ERROR_BOL.
 */

int promlog_prev(promlog_t *l, int type)
{
    int			alt, r;

    LDUMP("promlog_prev", l);
    PRINT(("promlog_prev: type=%d\n", type));

    alt = (l->pos >= l->alt_start && l->pos < l->alt_end);

    if (alt && type != PROMLOG_TYPE_LOG && type != PROMLOG_TYPE_ANY) {
	l->pos = l->log_start;
	alt = 0;
    }

    while (1) {
	if (alt) {
	    if (l->pos == l->alt_start)
		break;
	} else if (l->pos == l->log_start) {
	    if (type != PROMLOG_TYPE_LOG || type != PROMLOG_TYPE_ANY)
		break;
	    l->pos = l->alt_start;
	    alt = 1;
	    while (1) {
		if ((r = get_entry(l)) < 0)
		    goto done;
		if (l->ent.valid && l->ent.type == PROMLOG_TYPE_END)
		    break;
		l->pos += 4;
	    }
	    if (l->pos == l->alt_start)
		break;
	}

	l->pos -= 4;

	if ((r = get_entry(l)) < 0)
	    goto done;

	if (l->ent.valid &&
	    (type == PROMLOG_TYPE_ANY || l->ent.type == type)) {
	    r = 0;
	    goto done;
	}
    }

    r = PROMLOG_ERROR_BOL;

 done:

    LDUMP("promlog_prev", l);
    PRINT(("promlog_prev: r=%d\n", r));

    return r;
}

/*
 * promlog_next
 *
 *   Moves the current position forward until it reaches a valid entry
 *   of the specified type (or PROMLOG_TYPE_ANY or PROMLOG_TYPE_END).
 *   If the pointer is already at the end marker, returns PROMLOG_ERROR_EOL.
 */

int promlog_next(promlog_t *l, int type)
{
    int			alt, r, first;

    LDUMP("promlog_next", l);
    PRINT(("promlog_next: type=%d\n", type));

    alt = (l->pos >= l->alt_start && l->pos < l->alt_end);

    if (alt && type != PROMLOG_TYPE_LOG && type != PROMLOG_TYPE_ANY) {
	l->pos = l->log_start;
	alt = 0;
    }

    first = 1;

    while (1) {
	if ((r = get_entry(l)) < 0)
	    goto done;

	if (l->ent.type == PROMLOG_TYPE_END) {
	    if (first && ! alt) {
		r = PROMLOG_ERROR_EOL;
		goto done;
	    }

	    if (type == PROMLOG_TYPE_ANY || l->ent.type == type)
		break;		/* Was looking for end and found it */

	    if (! alt) {
		r = PROMLOG_ERROR_EOL;
		goto done;
	    }

	    l->pos = l->log_start;
	    alt = 0;
	} else if (! first && l->ent.valid &&
		   (type == PROMLOG_TYPE_ANY || l->ent.type == type))
	    break;		/* Found */
	else
	    l->pos = (l->pos + 2 + l->ent.key_len + l->ent.value_len + 3) & ~3;

	first = 0;
    }

    r = 0;

 done:

    LDUMP("promlog_next", l);
    PRINT(("promlog_next: r=%d\n", r));

    return r;
}

/*
 * promlog_last
 *
 *   Sets the current position to the end marker.  This should be used
 *   sparingly as it has to search past all the entries in the active
 *   sector.
 */

int promlog_last(promlog_t *l)
{
    int			r;

    LDUMP("promlog_last", l);

    l->pos = l->log_start;

    if ((r = get_entry(l)) < 0)
	return r;

    if (l->ent.type == PROMLOG_TYPE_END)
	r = 0;
    else
	r = promlog_next(l, PROMLOG_TYPE_END);

    PRINT(("promlog_last: r=%d\n", r));

    return r;
}

/*
 * promlog_get
 *
 *   Retrieves the key and value at the current position and stores
 *   them in the key and value buffers.  Current position should be
 *   pointing to something valid since no error checking is done.
 */

int promlog_get(promlog_t *l, char *key, char *value)
{
    int			r;

    LDUMP("promlog_get", l);

    if ((r = get_entry(l)) < 0)
	goto done;

    if (! l->ent.valid || l->ent.type == PROMLOG_TYPE_END) {
	r = PROMLOG_ERROR_POS;
	goto done;
    }

    if (key) {
	if ((r = fprom_read(&l->f, l->pos + 2,
			    key, l->ent.key_len)) < 0) {
	    printf("promlog: Error reading data at 0x%x: %s\n",
		   l->pos + 2, fprom_errmsg(r));
	    r = PROMLOG_ERROR_PROM;
	    goto done;
	}

	key[l->ent.key_len] = 0;
    }

    if (value) {
	if ((r = fprom_read(&l->f, l->pos + 2 + l->ent.key_len,
			    value, l->ent.value_len)) < 0) {
	    printf("promlog: Error reading data at 0x%x: %s\n",
		   l->pos + 2 + l->ent.key_len, fprom_errmsg(r));
	    r = PROMLOG_ERROR_PROM;
	    goto done;
	}

	value[l->ent.value_len] = 0;
    }

    PRINT(("promlog_get: key=%s value=%s\n", key, value));

    r = 0;

 done:

    PRINT(("promlog_get: r=%d\n", r));

    return r;
}

/*
 * promlog_find
 *
 *   Starts searching at the current position for a key of a given
 *   type.  If found, the current position points to the entry and 0
 *   is returned.  Otherwise, the current position is left at the end
 *   marker and PROMLOG_ERROR_EOL is returned.
 *
 *   To find the next occurrence of the same key in a list, use
 *   promlog_next to advance to the following position, then use
 *   promlog_find again.
 */

int promlog_find(promlog_t *l, char *key, int type)
{
    int			alt, i, r;

    LDUMP("promlog_find", l);
    PRINT(("promlog_find: key=%s type=%d\n", key, type));

    alt = (l->pos >= l->alt_start && l->pos < l->alt_end);

    if (alt && type != PROMLOG_TYPE_LOG && type != PROMLOG_TYPE_ANY) {
	l->pos = l->log_start;
	alt = 0;
    }

    if ((i = promlog_type(l)) == PROMLOG_TYPE_END) {
	r = PROMLOG_ERROR_EOL;
	goto done;
    }

    if (i == PROMLOG_TYPE_INVALID ||
	(type != PROMLOG_TYPE_ANY && type != i))
	if ((r = promlog_next(l, type)) < 0)
	    goto done;

    while (1) {
	char		match[PROMLOG_KEY_MAX];

	if ((r = promlog_get(l, match, 0)) < 0)
	    break;

	if (strcmp(match, key) == 0) {
	    r = 0;		/* Found */
	    break;
	}

	if ((r = promlog_next(l, type)) < 0)
	    break;		/* Fails at EOF */
    }

 done:

    PRINT(("promlog_find: r=%d\n", r));

    return r;
}

/*
 * promlog_lookup
 *
 *   Searches the entire log for a variable entry with the specified
 *   key.  If found, stores the value in the value buffer and returns
 *   0.  If not found, or an error occurs accessing the log, copies
 *   defl (if non-NULL) to value and returns an error code.  Leaves
 *   the current position at the variable if found, end marker if not
 *   found.
 */

int promlog_lookup(promlog_t *l, char *key, char *value, char *defl)
{
    int			r;

    LDUMP("promlog_lookup", l);
    PRINT(("promlog_lookup: key=%s defl=%s\n", key, defl));

    if ((r = promlog_first(l, PROMLOG_TYPE_VAR)) < 0)
	goto done;

    if ((r = promlog_find(l, key, PROMLOG_TYPE_VAR)) < 0)
	goto done;

    r = promlog_get(l, 0, value);

 done:

    if (r < 0 && value) {
	if (defl)
	    strcpy(value, defl);
	else
	    value[0] = 0;
    }

    PRINT(("promlog_lookup: r=%d\n", r));

    return r;
}

/*
 * validate_or_replace (internal)
 *
 *   Builds a header structure for an entry that can potentially be
 *   overwritten, and either checks if the overwrite is possible
 *   (validate) or does the overwrite (replace).
 *
 *   If validating: returns TRUE if it would be possible to replace
 *   the entry at the current position with the new data without
 *   having to make a new log entry.  This is true if the replacement
 *   is of the replacement entry is the same length, and would only
 *   require changing 1's into 0's and not vice-versa.
 */

static int validate_or_replace(promlog_t *l,
			       char *key, char *value, int type,
			       int do_replace)
{
    int		      (*fn)(fprom_t *, fprom_off_t, char *, int);
    int			r, kl, vl;

    LDUMP("validate_or_replace", l);

    kl = strlen(key);
    vl = strlen(value);

    if (kl >= PROMLOG_KEY_MAX || vl >= PROMLOG_VALUE_MAX) {
	r = PROMLOG_ERROR_ARG;
	goto done;
    }

    l->ent.valid	= 1;
    l->ent.type		= type;
    l->ent.cpu_num	= l->cpu_num;
    l->ent.key_len	= kl;
    l->ent.value_len	= vl;

    EDUMP("validate_or_replace", &l->ent);

    fn = do_replace ? fprom_write : fprom_validate;

    if ((r = (*fn)(&l->f, l->pos,          (char *) &l->ent, 2 )) >= 0 &&
	(r = (*fn)(&l->f, l->pos + 2,      key,              kl)) >= 0 &&
	(r = (*fn)(&l->f, l->pos + 2 + kl, value,            vl)) >= 0) {
	r = do_replace ? 0 : 1;
	goto done;
    }

    if (! do_replace && r == FPROM_ERROR_CONFLICT) {
	r = 0;
	goto done;
    }

    printf("promlog: Flash PROM %s error: %s\n",
	   do_replace ? "write" : "validate", fprom_errmsg(r));

    r = PROMLOG_ERROR_PROM;

 done:

    PRINT(("validate_or_replace: r=%d\n", r));

    return r;
}

/*
 * promlog_replace
 *
 *   Verifies that a log entry can be written at the specified
 *   position, and if so, writes it.
 */

int promlog_replace(promlog_t *l, char *key, char *value, int type)
{
    int			r;

    LDUMP("promlog_replace", l);

    if ((r = validate_or_replace(l, key, value, type, 0)) < 0)
	goto done;

    if (r == 0) {
	r = PROMLOG_ERROR_REPLACE;
	goto done;
    }

    if ((r = validate_or_replace(l, key, value, type, 1)) < 0)
	goto done;

    r = 0;

 done:

    PRINT(("promlog_replace: r=%d\n", r));

    return r;
}

/*
 * promlog_room
 *
 *   Checks if there would be enough room to make an insertion
 *   of a specified key and value.
 *
 *   Returns:
 *	0 if there is room
 *	PROMLOG_ERROR_COMPACT if a promlog_compact would create enough room
 *	PROMLOG_ERROR_FULL if log is full
 */

int promlog_room(promlog_t *l, char *key, char *value)
{
    int			r;
    int			need, used;

    LDUMP("promlog_room", l);

    if ((r = promlog_last(l)) < 0)
	goto done;

    need = 2 + strlen(key) + strlen(value);

    if (l->pos + need <= l->log_end - 4) {
	r = 0;
	goto done;
    }

    /*
     * Add up the total space currently in use to see if a
     * compaction would be futile.
     */

    l->pos = l->log_start;
    used = 0;

    while (1) {
	if ((r = get_entry(l)) < 0)
	    goto done;

	if (l->ent.type == PROMLOG_TYPE_END)
	    break;

	if (l->ent.valid &&
	    (l->ent.type == PROMLOG_TYPE_VAR ||
	     l->ent.type == PROMLOG_TYPE_LIST)) {

	    int		len = 2 + l->ent.key_len + l->ent.value_len;

	    used = (used + len + 3) & ~3;
	}

	if ((r = promlog_next(l, PROMLOG_TYPE_ANY)) < 0)
	    goto done;
    }

    if (l->alt_start + used + need > l->alt_end - 4)
	r = PROMLOG_ERROR_FULL;
    else
	r = PROMLOG_ERROR_COMPACT;

 done:

    PRINT(("promlog_room: r=%d\n", r));

    return r;
}

/*
 * promlog_compact
 *
 *   Does a sector swap compaction.  Leaves the current position at the
 *   end of the log.
 */

int promlog_compact(promlog_t *l)
{
    int			mask, r;
    char		buf[PROMLOG_KEY_MAX + PROMLOG_VALUE_MAX];
    int			altpos;
    promlog_header_t	h_old, h_new;

    PRINT(("promlog: Compacting log\n"));

    /*
     * Flash alternate sector
     */

    mask = 1 << (l->sector_base + (1 - l->active));

    PRINT(("promlog_compact: flash mask %x\n", mask));

    if ((r = fprom_flash_sectors(&l->f, mask)) < 0)
	goto done;

    /*
     * Copy variable and list entries from active sector into alternate.
     */

    l->pos = l->log_start;
    altpos = l->alt_start;

    while (1) {
	if ((r = get_entry(l)) < 0)
	    goto done;

	if (l->ent.type == PROMLOG_TYPE_END)
	    break;

	if (l->ent.valid &&
	    (l->ent.type == PROMLOG_TYPE_VAR ||
	     l->ent.type == PROMLOG_TYPE_LIST)) {

	    int		len = 2 + l->ent.key_len + l->ent.value_len;

	    if ((r = fprom_read(&l->f, l->pos, buf, len)) < 0) {
		printf("promlog: Error reading PROM during compact: %s\n",
		       fprom_errmsg(r));
		r = PROMLOG_ERROR_PROM;
		goto done;
	    }

	    if ((r = fprom_write(&l->f, altpos, buf, len)) < 0) {
		printf("promlog: Error reading PROM during compact: %s\n",
		       fprom_errmsg(r));
		r = PROMLOG_ERROR_PROM;
		goto done;
	    }

	    altpos = (altpos + len + 3) & ~3;
	}

	if ((r = promlog_next(l, PROMLOG_TYPE_ANY)) < 0)
	    goto done;
    }

    /*
     * Write valid header into alternate sector and make it the active one.
     */

    if ((r = get_header(l, l->active, &h_old)) < 0)
	goto done;

    memset(&h_new, 0xff, sizeof (h_new));

    h_new.magic		= PROMLOG_MAGIC;
    h_new.version	= PROMLOG_VERSION;
    h_new.sequence	= h_old.sequence + 1;

    if ((r = put_header(l, 1 - l->active, &h_new)) < 0)
	goto done;

    swap_log_alt(l);

    r = promlog_last(l);

 done:

    if (r < 0)
	printf("promlog: Failed (%s)\n", promlog_errmsg(r));
    else
	PRINT(("promlog: Done\n"));

    PRINT(("promlog_compact: r=%d\n", r));

    return r;
}

/*
 * promlog_put_var
 *
 *   Adds new variable entry.  Fails if bit 7 is set in any key or
 *   value byte (see Log Format).  Searches PROM to see if key already
 *   present If so, Can entry can be patched (only change 1's to 0's)
 *   If so, does it and returns.  Else changes entry to cancelled.
 *   Room for new entry?  If not, swaps log (takes about 2 seconds).
 *   Leaves current position at end of log.
 */

int promlog_put_var(promlog_t *l, char *key, char *value)
{
    int			r;

    LDUMP("promlog_put_var", l);

    r = promlog_first(l, PROMLOG_TYPE_VAR);

    if (r < 0 && r != PROMLOG_ERROR_EOL)
	goto done;

    if (r >= 0) {
	r = promlog_find(l, key, PROMLOG_TYPE_VAR);

	if (r < 0 && r != PROMLOG_ERROR_EOL)
	    goto done;
    }

    if (r >= 0) {
	if ((r = validate_or_replace(l, key, value,
				     PROMLOG_TYPE_VAR, 0)) < 0)
	    goto done;

	if (r) {
	    r = validate_or_replace(l, key, value,
				    PROMLOG_TYPE_VAR, 1);
	    goto done;
	}

	if ((r = promlog_delete(l)) < 0)
	    goto done;
    }

    if ((r = promlog_room(l, key, value)) != 0) {
	if (r != PROMLOG_ERROR_COMPACT)
	    goto done;
	if ((r = promlog_compact(l)) < 0)
	    goto done;
    }

    r = validate_or_replace(l, key, value, PROMLOG_TYPE_VAR, 1);

 done:

    PRINT(("promlog_put_var: r=%d\n", r));

    return r;
}

/*
 * promlog_put_list
 *
 *   Adds new list entry.  Fails if bit 7 is set in any key or value
 *   byte.
 */

int promlog_put_list(promlog_t *l, char *key, char *value)
{
    int			r;

    LDUMP("promlog_put_list", l);

    if ((r = promlog_room(l, key, value)) != 0) {
	if (r != PROMLOG_ERROR_COMPACT)
	    goto done;
	if ((r = promlog_compact(l)) < 0)
	    goto done;
    }

    r = validate_or_replace(l, key, value, PROMLOG_TYPE_LIST, 1);

 done:

    PRINT(("promlog_put_list: r=%d\n", r));

    return r;
}

/*
 * promlog_put_log
 *
 *   Adds new log entry.  Fails if bit 7 is set in any key or value
 *   byte.  Functions similarly to promlog_put_var.
 */

int promlog_put_log(promlog_t *l, char *key, char *value)
{
    int			r;

    LDUMP("promlog_put_log", l);

    if ((r = promlog_room(l, key, value)) != 0) {
	if (r != PROMLOG_ERROR_COMPACT)
	    goto done;
	if ((r = promlog_compact(l)) < 0)
	    goto done;
    }

    r = validate_or_replace(l, key, value, PROMLOG_TYPE_LOG, 1);

 done:

    PRINT(("promlog_put_log: r=%d\n", r));

    return r;
}

/*
 * promlog_delete
 *
 *   Changes the current entry to the cancelled state.
 */

int promlog_delete(promlog_t *l)
{
    int			r;

    LDUMP("promlog_delete", l);

    if ((r = get_entry(l)) < 0)
	goto done;

    if (l->ent.type == PROMLOG_TYPE_END) {
	r = PROMLOG_ERROR_POS;
	goto done;
    }

    l->ent.valid = 0;

    if ((r = fprom_write(&l->f, l->pos, (char *) &l->ent, 2)) < 0) {
	printf("promlog: Error deleting entry: %s\n", fprom_errmsg(r));
	r = PROMLOG_ERROR_PROM;
	goto done;
    }

    r = 0;

 done:

    PRINT(("promlog_delete: r=%d\n", r));

    return r;
}

/*
 * promlog_dump
 *
 *   Print out information in the promlog structure
 */

static char *type_names[4] = {
    "Var", "List", "Log", "End",
};

int promlog_dump(promlog_t *l)
{
    int			r, type;
    char		key[PROMLOG_KEY_MAX];
    char		value[PROMLOG_VALUE_MAX];
#if LDEBUG
    int			old_mode = debug_mode;
    debug_mode = 0;
#endif

    printf("PROMLOG DUMP: act=%d ls=0x%x le=0x%x as=0x%x ae=0x%x pos=0x%x\n",
	   l->active,
	   l->log_start, l->log_end,
	   l->alt_start, l->alt_end, l->pos);

    printf("  INACTIVE:\n");

    swap_log_alt(l);

    if ((r = promlog_first(l, PROMLOG_TYPE_ANY)) < 0) {
	if (r == PROMLOG_ERROR_EOL)
	    goto empty1;
	goto done;
    }

    while ((type = promlog_type(l)) != PROMLOG_TYPE_END) {
	if ((r = promlog_get(l, key, value)) < 0)
	    goto done;

	printf("   OFFSET(0x%x) TYPE(%d=%s) KEY(%s) VALUE(%s)\n",
	       l->pos, type, type_names[type], key, value);

	if ((r = promlog_next(l, PROMLOG_TYPE_ANY)) < 0) {
	    if (r == PROMLOG_ERROR_EOL)
		break;
	    goto done;
	}
    }

    printf("   END(0x%x)\n", l->pos);

 empty1:

    printf("  ACTIVE:\n");

    swap_log_alt(l);

    if ((r = promlog_first(l, PROMLOG_TYPE_ANY)) < 0) {
	if (r == PROMLOG_ERROR_EOL)
	    goto empty1;
	goto done;
    }

    while ((type = promlog_type(l)) != PROMLOG_TYPE_END) {
	if ((r = promlog_get(l, key, value)) < 0)
	    goto done;

	printf("   OFFSET(0x%x) TYPE(%d=%s) KEY(%s) VALUE(%s)\n",
	       l->pos, type, type_names[type], key, value);

	if ((r = promlog_next(l, PROMLOG_TYPE_ANY)) < 0) {
	    if (r == PROMLOG_ERROR_EOL)
		break;
	    goto done;
	}
    }

    printf("   END(0x%x)\n", l->pos);

#if LDEBUG
    debug_mode = old_mode;
#endif

    r = (r == PROMLOG_ERROR_EOL) ? 0 : r;

 done:

    PRINT(("promlog_dump: r=%d\n", r));

    return r;
}

/*
 * promlog_errmsg
 *
 *   Converts an error code to an error message.
 */

char *promlog_errmsg(int errcode)
{
    switch (errcode) {
    case PROMLOG_ERROR_NONE:
	return "No error";
    case PROMLOG_ERROR_PROM:
	return "Flash PROM error";
    case PROMLOG_ERROR_MAGIC:
	return "PROM does not contain valid log";
    case PROMLOG_ERROR_CORRUPT:
	return "Inconsistent PROM log";
    case PROMLOG_ERROR_BOL:
	return "Reached beginning of log";
    case PROMLOG_ERROR_EOL:
	return "Reached end of log";
    case PROMLOG_ERROR_POS:
	return "Illegal position";
    case PROMLOG_ERROR_REPLACE:
	return "Illegal replacement attempt";
    case PROMLOG_ERROR_COMPACT:
	return "Compaction required";
    case PROMLOG_ERROR_FULL:
	return "PROM log full";
    case PROMLOG_ERROR_ARG:
	return "Invalid argument";
    default:
	return "Unknown error code";
    }
}
