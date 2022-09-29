/* DALLAS nvram code */

/* This file contains dallas specific functions replacing those
 * in IP12nvram.c.  Ideally the common routines that aren't different
 * should be segrated into a 3rd file, but I didn't want to disturb
 * the other functionalities too much.
 */

/*
 * Current layout:
 *
 * 0xbfbe0000..0xbfbe0050: dallas real time clock
 * 0xbfbe0100: compatible nvram:	NVLEN_MAX bytes
 * 0xbfbe0500: shadow nvram:		NVLEN_MAX bytes	(for debugging)
 * 0xbfbe0900: persistent nvram:	NVLEN_PERSIST bytes
 */

#if defined(IP22)	/* whole file */

#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/ds1286.h>			/* for base of NVRAM */
#include <arcs/errno.h>
#include <ctype.h>
#include <libsc.h>
#include <libsk.h>

/* Exports */
#define	NVRAM_FUNC(type,name,args,pargs,cargs,return)	type dallas_ ## name pargs;
#include <PROTOnvram.h>

/* This was removed from sys/IP22nvram.h because that file is
 * shipped on customer's systems.  Not exactly what we want for
 * "hiding" the lock bit.
 */
#define NV_REVISION_MASK	0x3F	/* use only 3f so to */
#define NV_REVISION_SPARE	0x40	/* allow a spare here */
#define NV_REVISION_LOCK	0x80	/* this is the NVRAM "lock" */

#ifndef	NVOFF_PERSIST
#define NVOFF_PERSIST   2*NVLEN_MAX
#endif

#ifndef NVLEN_PERSIST
#define NVLEN_PERSIST   2048		/* 2k bytes of variables */
					/* (includes '=''s and nulls) */
#endif

#ifndef	MAX_PERSIST_NAME_LEN
#define	MAX_PERSIST_NAME_LEN	128
#endif
#ifndef	MAX_PERSIST_DATA_LEN
#define	MAX_PERSIST_DATA_LEN	128
#endif

#define	NVRAM_PERSIST_END ((uint *)NVRAM_ADRS(NVOFF_PERSIST+NVLEN_PERSIST))

#ifdef NVRAM_CACHE
static char *nvram_read;	/* pointer to malloc'd area */
#endif
#ifdef VERBOSE
#define	VPRINTF(arg)	printf arg
int chksum_prt;			/* watch checksum accumulating */
#else
#define	VPRINTF(arg)
#endif

#ifdef DEBUG
/* Shadow real nvram contents immediately after real data
 * this routine compares each byte and reports differences.
 */
static void
dallas_nvshadow_check()
{
	register int i;

	for (i = 0; i < NVLEN_MAX; i++) {
	    if ((char)NVRAM_DATA(i) != (char)NVRAM_DATA(i + NVLEN_MAX)) {
		VPRINTF(("\t0x%x:%x=>%x ", NVRAM_ADRS(i),
		    (char)NVRAM_DATA(i+NVLEN_MAX), (char)NVRAM_DATA(i)));
		NVRAM_DATA(i+NVLEN_MAX) = (char)NVRAM_DATA(i);
	    }
	}
}
#endif

/*
 * nvchecksum -- calculate new checksum for non-volatile RAM
 */
static char
nvchecksum(void)
{
	register int i;
	register signed char checksum = 0xa5;

#ifdef DEBUG
	dallas_nvshadow_check();
#endif
	VPRINTF(("nvchecksum: "));
#ifdef NVRAM_CACHE
	if (!nvram_read) {
	    if (nvram_read = (char *)malloc(NVLEN_MAX))
		for (i = 0; i < NVLEN_MAX; i++)
		    nvram_read[i] = (char)NVRAM_DATA(i);
	}
#endif
	for (i = 0; i < NVLEN_MAX; i++) {
	    if (i != NVOFF_CHECKSUM)
		checksum ^= 
#ifdef NVRAM_CACHE
		    nvram_read ? nvram_read[i] :
#endif
		    NVRAM_DATA(i) & 0xff;

	    /* following is a tricky way to rotate */
	    if (i & 1)
		checksum = (checksum << 1) | (checksum < 0);
#ifdef VERBOSE
	    if (chksum_prt)
		VPRINTF(("%x ", (char)checksum));
#endif
	}
#ifdef NVRAM_CACHE
	VPRINTF(("::=0x%x 0x%x\n", checksum, nvram_read));
#endif
	return (char)checksum;
}

/*
 * cpu_get_nvram_buf -- the same as cpu_get_nvram, but put it in a buffer
 *                      of our choice
 */
void
dallas_cpu_get_nvram_buf(int nv_off, int nv_len, char buf[])
{
#ifdef NVRAM_CACHE
	if (nvram_read) {
	    VPRINTF(("\nGETC: %d = ", nv_off));
	    while (nv_len-- > 0) {
		VPRINTF(("%x ", nvram_read[nv_off++]));
		*buf++ = nvram_read[nv_off++];
	    }
	}
	else
#endif
	{
	    register uint *nvramptr = (uint *)NVRAM_ADRS(nv_off);
	    int len = nv_len; /* XXX */

	    VPRINTF(("\nGETD: 0x%x = ", nvramptr));
	    while (nv_len-- > 0) {
		VPRINTF(("%x ", *nvramptr & 0xff));
		*buf++ = *nvramptr++;
	    }
	    while (len-- > 0) {
		VPRINTF(("%x ", (char)NVRAM_DATA(nv_off++)));
	    }
	}
	*buf = 0;
}

/*
 * cpu_get_nvram -- read string from nvram at an offset
 */
char *
dallas_cpu_get_nvram_offset(int nv_off, int nv_len)
{
	static char buf[128];

	dallas_cpu_get_nvram_buf(nv_off, nv_len, buf);

	return buf;
}


/*
 * cpu_set_nvram_offset -- write a string to nvram at an offset
 */
int
dallas_cpu_set_nvram_offset(int nv_off, int nv_len, char *string)
{
	register uint *nvramptr = (uint *)NVRAM_ADRS(nv_off);
	int nv_off_save = nv_off;
	char checksum[2];

	VPRINTF(("\nSETD: 0x%x = ", nvramptr));
	while (nv_len-- > 0) {
		VPRINTF(("%x ", *string & 0xff));
#ifdef NVRAM_CACHE
		nvram_read[nv_off++] = 
#endif
		*nvramptr++ = *string++;
	}
	if (nv_off_save != NVOFF_CHECKSUM) {
		checksum[0] = nvchecksum();
		checksum[1] = 0;
		return dallas_cpu_set_nvram_offset(
		    NVOFF_CHECKSUM, NVLEN_CHECKSUM, checksum);
	}
	return (0);
}

/*
 * Persistent variables are very simply stored as:
 *
 *      name=string\0name=string\0\0
 *
 * This routine can be fooled quite easily when you have an '='
 * character in the name, but it doesn't seem worth the effort
 * to deal with it.
 *
 * Note: Ignores character case to be compatible with "fixed"
 *       NVRAM variables.
 */
static uint *
lookup_persist(char *string, uint **data)
{
	register uint *nvramptr = (uint *)NVRAM_ADRS(NVOFF_PERSIST);
	register char *s, c;
	uint *nvptr;

	VPRINTF(("lookup_persist(%s,0x%x)\n", string, data));
	while ((nvptr = nvramptr) && *nvramptr) {
	    VPRINTF(("looking at 0x%x: %c ... ", nvramptr, (char)*nvramptr));
	    for (s = string;
		tolower(*s++) == (c = tolower((char)*nvramptr++));
		)
		if (c == '=')
		    break;

	    if (*(s-1) == '\0' && (char)*(nvramptr-1) == '=') {
		if (data)
		    *data = nvramptr;
		VPRINTF(("found at 0x%x\n", nvptr));
		return nvptr;
	    }
	    else {
		while ((char) *nvramptr++)
		    ;
		VPRINTF(("not found skipping to 0x%x\n", nvramptr));
	    }
	}
	if (data)
	    *data = nvramptr;

	VPRINTF(("... exit at 0x%x\n", nvramptr));
	return (uint *)0;
}
    
void
dallas_cpu_set_nvram_persist(char *match, char *newstring)
{
    uint *name, *data, *nxtdata;

    if (name = lookup_persist(match, &data)) {
	VPRINTF(("... return 0x%x, 0x%x\n", name, data));
	for (nxtdata = data; *nxtdata++;)
	    ;

	while (*name++ = *nxtdata++)
	    while (*name++ = *nxtdata++)
		;
    } else {
	VPRINTF(("... alt return 0x%x\n", data));
	name = data;
    }
    
    if (newstring && *newstring) {
#ifdef VERBOSE
	uint *end = (uint *)NVRAM_ADRS(NVOFF_PERSIST + NVLEN_PERSIST);
	VPRINTF(("end=0x%x, pos=0x%x, len=0x%x  ",
	    end, name, (strlen(match) + strlen(newstring) + 2)));
#endif
	if (strlen(match)     <= MAX_PERSIST_NAME_LEN
	&&  strlen(newstring) <= MAX_PERSIST_DATA_LEN) {

	    if ((strlen(match) + strlen(newstring) + 2)
	    < (NVRAM_PERSIST_END - name)) {

		VPRINTF(("... adding from 0x%x ", name));
		while (*name++ = *match++)
		    ;
		*(name-1) = '=';
		while (*name++ = *newstring++)	/* includes end of str. */
		    ;
		*name++ = '\0';			/* add end of file */
		VPRINTF(("to 0x%x ", name));
		return;
	    }
	    printf("No more room for persistent variables.\n");
	    return;
	}
	printf("Name or Data too long, max is %d and %d.\n",
	    MAX_PERSIST_NAME_LEN, MAX_PERSIST_NAME_LEN);
    }
}


static uint *
next_persist_var(uint *nvramptr, char *name, char *data)
{
    register char *p;
    int len;

    VPRINTF(("next_persist_var(%x %c): ", nvramptr, *nvramptr));
    while (nvramptr < NVRAM_PERSIST_END && (char)*nvramptr) {
	
	for (p = name, len = 0; (char)*nvramptr && (char)*nvramptr != '=';) {
	    if (nvramptr >= NVRAM_PERSIST_END
	    || ++len >= MAX_PERSIST_NAME_LEN) {
		goto error;
	    }
	    *p++ = (char)*nvramptr++;
	}
	*p = '\0';

	if (!*nvramptr++) {
	    goto error;
	}
	for (p = data, len = 0; *p++ = (char)*nvramptr++;) {
	    if (nvramptr >= NVRAM_PERSIST_END
	    || ++len >= MAX_PERSIST_DATA_LEN) {
		goto error;
	    }
	}
	VPRINTF(("finds %s\n", name));
	return nvramptr;
    }
    return (uint *)0;

error:
    VPRINTF(("Mangled persistent variable area.\n"));
    return (uint *)0;
}

void
dallas_cpu_get_nvram_persist(int (*putstr)(char *,char *,struct string_list *),
			     struct string_list *env)
{
	register uint *nvramptr = (uint *)NVRAM_ADRS(NVOFF_PERSIST);
	char name[MAX_PERSIST_NAME_LEN], data[MAX_PERSIST_DATA_LEN];

	VPRINTF(("dallas_cpu_get_nvram_persist()\n"));
	while (nvramptr = next_persist_var(nvramptr, name, data)) {
	    VPRINTF(("Add_string(%s,%s)\n", name, data));
	    if ((*putstr)(name, data, env))
		break;
	}
}

/*
 * cpu_set_nvram - set nvram string to value string
 *	XXX reset of nvram code needs to move out of prom area
 *	to someplace reasonable (here?)
 */
int
dallas_cpu_set_nvram(char *match, char *newstring)
{
    extern int _prom;
    int rc = 0;

    VPRINTF(("dallas_cpu_set_nvram(%s,%s) ", match,newstring,_prom)); /* XXX */
    if (_prom) {
	rc = setenv_nvram (match, newstring);

	/* if we're clearing a variable try to delete it from
	 * persistent area if present.
	 */
	if (newstring == 0 || *newstring == '\0')
	    dallas_cpu_set_nvram_persist(match, newstring);
    }
    return rc;
}

int
dallas_cpu_is_nvvalid(void)
{
    char is = (char)*dallas_cpu_get_nvram_offset(NVOFF_CHECKSUM, NVLEN_CHECKSUM);
    char sb = (char)nvchecksum();

    if (is != sb) {
	VPRINTF(("Checksum mismatch: is 0x%x, s/b 0x%x\n", is, sb));
	return 0;
    }
    is = (char)*dallas_cpu_get_nvram_offset(NVOFF_REVISION, NVLEN_REVISION)
       & NV_REVISION_MASK;

    if (is != NV_CURRENT_REV) {
	VPRINTF(("Revision mismatch: is 0x%x, s/b 0x%x\n", is, NV_CURRENT_REV));
	return 0;
    }
    return 1;
}

void
dallas_cpu_set_nvvalid(void (*delstr)(char *,struct string_list *),
		       struct string_list *env)
{
    char csum[2];
    register uint *nvramptr = (uint *)NVRAM_ADRS(NVOFF_PERSIST);
    char name[MAX_PERSIST_NAME_LEN], data[MAX_PERSIST_DATA_LEN];

    /* preserve "lock" bit */
    dallas_cpu_get_nvram_buf(NVOFF_REVISION, NVLEN_REVISION, csum);
    csum[0] = (csum[0] & ~NV_REVISION_MASK) | NV_CURRENT_REV;
    dallas_cpu_set_nvram_offset(NVOFF_REVISION, NVLEN_REVISION, csum);

    csum[0] = nvchecksum();
    dallas_cpu_set_nvram_offset(NVOFF_CHECKSUM, NVLEN_CHECKSUM, csum);

    /* Clear out persistent variables as a support aid for "resetenv" */
    if (delstr)
	while (nvramptr = next_persist_var(nvramptr, name, data)) {
	    VPRINTF(("Del_string(%s)\n", name));
	    (*delstr)(name, env);
	}
    nvramptr = (uint *)NVRAM_ADRS(NVOFF_PERSIST);
    *nvramptr++ = '\0';
    *nvramptr   = '\0';
}

/* Lock the "lockable" NVRAM variables by setting a bit in the
 * revision.  This is a little hocky but has the advantage that
 * the bit is under the checksum coverage so changing it by hand
 * will screw up the checksum.
 */
void
dallas_cpu_nv_lock(int lock)
{
	char buf[NVLEN_REVISION+1];

	VPRINTF(("dallas_cpu_nv_lock(%d)\r\n", lock));
	dallas_cpu_get_nvram_buf(NVOFF_REVISION, NVLEN_REVISION, buf);
	if (lock)
	    buf[0] |= NV_REVISION_LOCK;
	else
	    buf[0] &= ~NV_REVISION_LOCK;

	/* call outer routine so checksum is set. */
	dallas_cpu_set_nvram_offset(NVOFF_REVISION, NVLEN_REVISION, buf);
}


int
dallas_nvram_is_protected(void)
{
	char buf[NVLEN_REVISION+1];

	dallas_cpu_get_nvram_buf(NVOFF_REVISION, NVLEN_REVISION, buf);
	VPRINTF(("nvram_is_protected()=%d\r\n", buf[0] & NV_REVISION_LOCK));
	return buf[0] & NV_REVISION_LOCK;
}

/* NVRAM users */
/* the PROM uses this routine for the PROM_EADDR entry point */
void
dallas_cpu_get_eaddr(u_char eaddr[])
{
	dallas_cpu_get_nvram_buf(NVOFF_ENET, NVLEN_ENET, (char *)eaddr);
}

/* initialize factory fresh parts */
void
dallas_cpu_nvram_init(void)
{
	register uint *nvramptr = (uint *)NVRAM_ADRS(0);

	/* Indy needs to explicitly initialize new dallas parts.
	 * (variables set in init_env())
	 */
	/* clear all nvram */
	printf("New RTC/NVRAM: clearing NVRAM memory.\n");

	while (nvramptr < (uint *)NVRAM_ADRS(DS1386_NVRAM_SIZE))
	    *nvramptr++ = 0;

}
#endif	/* IP22 */

