#ident "$Header: "
#include <sys/types.h>
#include <stdlib.h>

#include <hwreg.h>

#define LBYTEU(caddr) \
	(uchar_t) ((*(__int64_t *) ((__psunsigned_t) (caddr) & ~7) <<	\
		    ((__psunsigned_t) (caddr) & 7) * 8) >> 56)

#define LHALFU(caddr) \
	(ushort_t) ((*(int *) ((__psunsigned_t) (caddr) & ~3) <<	\
		     ((__psunsigned_t) (caddr) & 3) * 8) >> 16)

#define GET(value, topbit, botbit) \
	 ((topbit) == 63 ? \
	  (value) >> (botbit) : \
	  (value) >> (botbit) & ((1ULL << (topbit) - (botbit) + 1) - 1))

/*
 * Access modes
 */

hwreg_accmode_t hwreg_accmodes[] = {
    {	"R",	"Readable"					},
    {	"RC",	"Readable, clears on read"			},
    {	"RS",	"Special action occurs on read"			},
    {	"W",	"Writable"					},
    {	"WX",	"Writable, value ignored"			},
    {	"WS",	"Special action occurs on write"		},
    {	"RW",	"Readable and writable"				},
    {	"RWC",	"Readable, write clears where 1 present"	},
    {	"RWA",	"Readable, write clears where 0 present (AND)"	},
    {	"RWS",	"Special action occurs on read or write"	},
    {	"0",	"Hardwired zeroes"				},
    {	"1",	"Hardwired ones"				},
    {	"X",	"Undefined behavior"				},
    {	0,	0,						},
};

/*
 * Utility routines
 */

#define islower(c)	((c) >= 'a' && (c) <= 'z')
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')

#define tolower(c)	((c) - 'A' + 'a')
#define toupper(c)	((c) - 'a' + 'A')

char *hwreg_upcase(char *dst, char *src)
{
    int			c;

    for (; (c = LBYTEU(src)) != 0; src++)
	*dst++ = islower(c) ? toupper(c) : c;

    *dst = 0;

    return dst;
}

void hwreg_getbits(char *s, uchar_t *topbit, uchar_t *botbit)
{
    char local[16];	/* s probably in PROM; ctoi does byte reads */
    strncpy(local, s, sizeof (local));
    local[sizeof (local) - 1] = 0;
    s = local;

    if (*s == '<')
	s++;
    while (*s == '0')
	s++;
    *topbit = hwreg_ctoi(s, 0);
    s = strchr(s, ':');	   /* Parse <m:n> or <n> */
    if (s) {
	s++;
	while (*s == '0')
	    s++;
	*botbit = hwreg_ctoi(s, 0);
    } else
	*botbit = *topbit;
}

char *hwreg_cvtbase(char *buf, int base, __uint64_t value, int ndigits)
{
    if (ndigits == 0)
	ndigits = 1;

    for (buf[ndigits] = 0; --ndigits >= 0; value /= base)
	if ((buf[ndigits] = '0' + (value % base)) > '9')
	    buf[ndigits] += ('a' - '9' - 1);;

    return buf;
}

void hwreg_val2string(char *buf, int base, __uint64_t val,
		      int bits, int altform)
{
    char	*s;

    switch (base) {
    case 2:
	if (altform) {
	    *buf++ = '0';
	    *buf++ = 'b';
	}
	hwreg_cvtbase(buf, 2, val, bits);
	break;
    case 8:
	if (altform)
	    *buf++ = '0';
	hwreg_cvtbase(buf, 8, val, (bits + 2) / 3);
	break;
    case 10:
	hwreg_cvtbase(buf, 10, val, 20);
	for (s = buf; s[0] == '0' && s[1]; s++)
	    ;
	while ((*buf++ = *s++) != 0)
	    ;
	break;
    case 16:
	if (altform) {
	    *buf++ = '0';
	    *buf++ = 'x';
	}
	hwreg_cvtbase(buf, 16, val, (bits + 3) / 4);
	break;
    default:
	*buf = 0;
    }
}

__int64_t hwreg_ctoi(char *s, char **end_s)
{
    __int64_t	n = 0;
    int		neg = 0;

    if (*s == '-') {
	neg = 1;
	s++;
    } else if (*s == '+')
	s++;

    if (*s >= '1' && *s <= '9')
	while (*s >= '0' && *s <= '9')
	    n = n * 10 + (*s++ - '0');
    else if (*s == '0') {
	if (*++s == 'x' || *s == 'X')
	    while (*++s >= 'a' && *s <= 'z' ||
		   *s >= 'A' && *s <= 'Z' ||
		   *s >= '0' && *s <= '9')
		n = n * 16 +
		    (*s >= 'a' ? *s - 'a' + 10 :
		     *s >= 'A' ? *s - 'A' + 10 :
		     *s - '0');
	else if (*s == 'b' || *s == 'B')
	    while (*++s == '0' || *s == '1')
		n = n * 2 + (*s - '0');
	else
	    while (*s >= '0' && *s <= '7')
		n = n * 8 + (*s++ - '0');
    }

    switch (*s) {
    case 'g':
	n *= 1024;
    case 'm':
	n *= 1024;
    case 'k':
	n *= 1024;
	s++;
    }

    if (end_s)
	*end_s = s;

    return (neg ? -n : n);
}

/*
 * hwreg_lookup_name
 *
 *   Looks up register by case-insensitive name.
 *
 *   If partial is 0, requires an exact match.
 *   If partial is non-zero, requires an unambiguous matching prefix.
 *
 *   Returns (hwreg_t *) 0 if no match found.
 */

hwreg_t *hwreg_lookup_name(hwreg_set_t *regset, char *name, int partial)
{
    char		name_caps[80];
    int			i;
    hwreg_t	       *regs	= regset->regs;
    char	       *strtab	= regset->strtab;

    hwreg_upcase(name_caps, name);

    if (partial) {
	int		len, found, nfound;

	len = strlen(name_caps);
	nfound = 0;
	found = 0;

	for (i = 0; regs[i].nameoff; i++)
	    if (strncmp(name_caps, strtab + regs[i].nameoff, len) == 0) {
		if (strcmp(name_caps, strtab + regs[i].nameoff) == 0)
		    return &regs[i];	/* Exact match wins */
		found = i;
		nfound++;
	    }

	return (nfound != 1) ? 0 : &regs[found];
    } else
	for (i = 0; regs[i].nameoff; i++)
	    if (strcmp(name_caps, strtab + regs[i].nameoff) == 0)
		return &regs[i];

    return 0;
}

/*
 * hwreg_lookup_addr
 *
 *   Look up register by address match
 *   Returns (hwreg_t *) 0 if no match found.
 */

hwreg_t *hwreg_lookup_addr(hwreg_set_t *regset, __uint64_t addr)
{
    int			i;
    hwreg_t	       *regs	= regset->regs;

    for (i = 0; regs[i].nameoff; i++)
	if (addr == regs[i].address)
	    return &regs[i];

    return 0;
}

/*
 * hwreg_lookup_field
 *
 *   Looks up field name by case-insensitive name.
 *
 *   If partial is 0, requires an exact match.
 *   If partial is non-zero, requires an unambiguous matching prefix.
 *
 *   Returns field pointer, 0 if not found.
 */

hwreg_field_t *hwreg_lookup_field(hwreg_set_t *regset, hwreg_t *r,
				  char *name, int partial)
{
    char		name_caps[80];
    int			i;
    hwreg_field_t      *fields	= regset->fields;
    char	       *strtab	= regset->strtab;
    int			nfield	= LBYTEU(&r->nfield);
    int			sfield	= LHALFU(&r->sfield);

    hwreg_upcase(name_caps, name);

    if (partial) {
	int		len, found, nfound;

	len = strlen(name_caps);
	nfound = 0;
	found = 0;

	for (i = 0; i < nfield; i++)
	    if (strncmp(name_caps,
			strtab + fields[sfield + i].nameoff,
			len) == 0) {
		if (strcmp(name_caps,
			   strtab + fields[sfield + i].nameoff) == 0)
		    return &fields[i];		/* Exact match wins */
		found = i;
		nfound++;
	    }

	return (nfound != 1) ? 0 : &fields[found];
    } else
	for (i = 0; i < nfield; i++)
	    if (strcmp(name_caps,
		       strtab + fields[sfield + i].nameoff) == 0)
		return &fields[i];

    return 0;
}

/*
 * hwreg_reset_default
 *
 *   Returns the default contents of a register on system reset.
 */

__uint64_t hwreg_reset_default(hwreg_set_t *regset, hwreg_t *r)
{
    int			i;
    __uint64_t		resdef;
    hwreg_field_t      *fields	= regset->fields;
    int			nfield	= LBYTEU(&r->nfield);
    int			sfield	= LHALFU(&r->sfield);

    resdef = 0;

    for (i = 0; i < nfield; i++)
	if (LBYTEU(&fields[sfield + i].reset))
	    resdef |= (fields[sfield + i].resetval <<
		       LBYTEU(&fields[sfield + i].botbit));

    return resdef;
}

/*
 * hwreg_encode_field
 *
 *   Takes a register definition and a string of the form "field=value".
 *   Shifts the value by the appropriate amount and stores it in result.
 *   Requires a printf()-type routine used to display error messages.
 *   Case-insensitive.
 */

__uint64_t hwreg_encode_field(hwreg_set_t *regset, hwreg_t *r, char *asst,
			      int (*prf)(const char *fmt, ...))
{
    char		*value;
    int			len;
    hwreg_field_t      *f;
    hwreg_field_t      *found;
    __uint64_t		result;
    int			bits;
    int			i;
    char		asst_caps[80];
    hwreg_field_t      *fields	= regset->fields;
    char	       *strtab	= regset->strtab;
    int			nfield	= LBYTEU(&r->nfield);
    int			sfield	= LHALFU(&r->sfield);

    hwreg_upcase(asst_caps, asst);

    if ((value = strchr(asst_caps, '=')) == 0) {
	(*prf)("Malformed field assignment '%s' ignored\n", asst_caps);
	return 0L;
    }

    len = value - asst_caps;
    *value++ = 0;
    found = 0;

    for (i = 0; i < nfield; i++) {
	f = &fields[sfield + i];

	if (strncmp(strtab + f->nameoff, asst_caps, len) == 0) {
	    if (strlen(strtab + f->nameoff) == len) {
		found = f;		/* Exact match */
		break;
	    }

	    if (found) {
		(*prf)("Ambiguous field name '*s' ignored\n", asst_caps);
		return 0L;
	    }

	    found = f;
	}
    }

    if (! found) {
	(*prf)("Unknown field name '%s' ignored\n", asst_caps);
	return 0L;
    }

    result = (__uint64_t) hwreg_ctoi(value, 0);

    bits = LBYTEU(&found->topbit) - LBYTEU(&found->botbit) + 1;

    if (bits != 64 && result >= (1ULL << bits))
	(*prf)("*** Warning: value 0x%llx overflows %d-bit field %s\n",
	       result, bits, strtab + found->nameoff);

    return (result << LBYTEU(&found->botbit));
}

/*
 * hwreg_encode
 *
 *   Takes a register definition and a string containing multiple
 *   assignments of the form "field=value" separated by commas.
 *   Requires a printf()-type routine used to display error messages.
 *   Case-insensitive.
 */

__uint64_t hwreg_encode(hwreg_set_t *regset, hwreg_t *r, char *assts,
			int (*prf)(const char *fmt, ...))
{
    __uint64_t		result;
    char		field[80], *s;

    result = 0;

    while (assts[0]) {
	if ((s = strchr(assts, ',')) == 0)
	    s = assts + strlen(assts);

	strncpy(field, assts, s - assts);
	field[s - assts] = 0;

	if (*s == ',')
	    s++;

	assts = s;

	if (field[0])
	    result |= hwreg_encode_field(regset, r, field, prf);
    }

    return result;
}

/*
 * hwreg_decode
 *
 *   Given a register description and register value, prints out
 *   the register's bit-field locations, names, and subvalues.
 *   Requires a printf()-type routine used to display the information.
 *
 *   If the shortform flag is true, output eill be in short format
 *   with "field=value,..." like the input to hwreg_encode.
 *
 *   If the showreset flag is true, the input value is ignored and the
 *   reset default value is used.
 *
 *   base gives the output base (2, 8, 10, or 16).
 */

void hwreg_decode(hwreg_set_t *regset, hwreg_t *r, int shortform,
		  int showreset, int base, __uint64_t value,
		  int (*prf)(const char *fmt, ...))
{
    int			i;
    int			nfield	= LBYTEU(&r->nfield);
    int			sfield	= LHALFU(&r->sfield);
    char	       *strtab	= regset->strtab;

    if (shortform)
	(*prf)("%c", '[');

    for (i = 0; i < nfield; i++) {
	char		buf[80], *s;
	__uint64_t	field;
	hwreg_field_t  *f	= &regset->fields[sfield + i];
	int		topbit	= LBYTEU(&f->topbit);
	int		botbit	= LBYTEU(&f->botbit);
	int		accmode	= LBYTEU(&f->accmode);
	int		reset	= LBYTEU(&f->reset);
	int		bits;

	bits = topbit - botbit + 1;
	field = showreset ? f->resetval : GET(value, topbit, botbit);

	if (showreset && ! reset)
	    strcpy(buf, "-");
	else
	    hwreg_val2string(buf, base, field, bits, 1);

	if (shortform)
	    (*prf)("%s=%s%s",
		   strtab + f->nameoff,
		   buf,
		   i < nfield - 1 ? "," : "]\n");
	else {
	    if (topbit == botbit)
		(*prf)("   <%02d>", topbit);
	    else
		(*prf)("<%02d:%02d>", topbit, botbit);

	    (*prf)(" %-3s %-24s  %s",
		   hwreg_accmodes[accmode].name,
		   strtab + f->nameoff,
		   buf);

	    /*
	     * If the field name has a bit range, show the value
	     * again, shifted by the low bit position.  E.g. for
	     * ADDRESS<31:03> it would show the value << 3.
	     */

	    if (field && (s = strchr(strtab + f->nameoff, '<')) != 0 &&
		(! showreset || reset)) {
		uchar_t		lo, hi;

		hwreg_getbits(s, &hi, &lo);

		if (lo) {
		    hwreg_val2string(buf, base, field << lo, bits + lo, 1);
		    (*prf)(" << %d = %s", lo, buf);
		}
	    }

	    (*prf)("\n");
	}
    }
}
