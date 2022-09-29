/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.44 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/loaddrs.h"
#include "sys/sbd.h"
#include "sys/systm.h"
#include "string.h"
#include "sys/arcs/spb.h"
#ifdef MAPPED_KERNEL
#include "sys/mapped_kernel.h"
#endif /* MAPPED_KERNEL */

#if defined (SN)
#include "sys/SN/arch.h"
#include "sys/SN/klconfig.h"
#endif
/*
 * Code to check if the debugger should be auto-loaded.
 */

extern int	dbgmon_loadstop, dbgmon_symstop, dbgmon_nmiprom;

extern int	start();
extern int	arcs_load(char *, __psunsigned_t, int (**)(), __psunsigned_t *);

#ifdef _K64PROM32
#include "sys/EVEREST/evaddrmacros.h"
#define STR_ADDR(_x)		(char *)K032TOKPHYS(_x)
#define CONVERT_BACK(_x)	(__uint32_t)KPHYSTO32K0(_x)
#else
#define STR_ADDR(_x)		(_x)
#define CONVERT_BACK(_x)	(_x)
#endif /* _K64PROM32 */

#if _MIPS_SIM == _ABI64 && !defined(_K64PROM32)
#define KXBASE	0x8000000000000000
#else
#define KXBASE	K0BASE
#endif

static char *
chkenv(char *str, __promptr_t *environ)
{
	int len;

	if (environ == 0 || (__psunsigned_t)STR_ADDR(*environ) < KXBASE)
		return(0);
	len = strlen(str);
	while (*environ) {
		if (strncmp(str, STR_ADDR(*environ), len) == 0
		    && (STR_ADDR(*environ))[len] == '=') {
			return(&(STR_ADDR(*environ))[len+1]);
		}
		environ++;
	}
	return(0);
}

/*
 * check_dbg.c -- check for $dbgmon in environ or filename ending
 * in .dbg, if so load debug monitor and transfer control to it
 */

#define	MAXSTRINGS	48		/* max number of strings */
#define	STRINGBYTES	1024		/* max total length of strings */

/*
 * string lists are used to maintain argv and environment string lists
 */
struct string_list {
	char *strptrs[MAXSTRINGS];	/* vector of string pointers */
	char strbuf[STRINGBYTES];	/* strings themselves */
	char *strp;			/* free ptr in strbuf */
	int strcnt;			/* number of strings in strptrs */
};

#define	streq(a,b) (strcmp(a,b)==0)

extern void dprintf(char *, ...);

static struct string_list dbg_argv;
static struct string_list dbg_environ;

static void init_str(struct string_list *);
static new_str1(__promptr_t, register struct string_list *);
static char *fill_dbg(register char *, register char *);
short dbgmon_always = 1;

/* symmon names to use if we try to get over the net */
#ifdef IP19
# if _MIPS_SIM == _ABI64
#  define _SYMMON "symmon64.IP19"
# else
#  define _SYMMON "symmon.IP19"
# endif
#else
#endif
#ifdef IP20
# define _SYMMON "symmon.IP20"
#endif
#ifdef IP21
# define _SYMMON "symmon.IP21"
#endif
#ifdef IP22
#if _MIPS_SIM == _ABI64
# define _SYMMON "symmon64.IP22"
#else
# define _SYMMON "symmon.IP22"
#endif
#endif
#ifdef IP25
#  define _SYMMON "symmon.IP25"
#endif
#ifdef IP26
# define _SYMMON "symmon.IP26"
#endif
#ifdef IP27
# define _SYMMON "symmon.IP27"
#endif
#ifdef IP28
# define _SYMMON "symmon.IP28"
#endif
#ifdef IP30
# define _SYMMON "symmon.IP30"
#endif
#ifdef IP32
# define _SYMMON "symmon.IP32"
#endif
#ifdef IPMHSIM
# define _SYMMON "symmon.IPMHSIM"
#endif
#ifdef IP33
# define _SYMMON "symmon.IP33"
#endif


/* disk locations of symmon to try if can't be found otherwise */
char *dsk_symmon[] = {
	"dksc(0,1,8)symmon",
	"dksc(1,1,8)symmon",
	"dksc(0,2,8)symmon",
	"dksc(1,2,8)symmon",
	"dksc(0,3,8)symmon",
	"dksc(1,3,8)symmon",
	0
};

void
_check_dbg(int argc, __promptr_t *argv, __promptr_t *environ, void *start_pc)
{
	register char *cp, *cp2;
	register __promptr_t *wp;
	register char *argv0;
	int filelen;
	int trydisks = 0;
	int nettry = 0;
	int (*init_pc)(int, char **, char **, void *);
#define MAX_BOOT_STR 256
	char bootfile[MAX_BOOT_STR];

	char pcstring[40];
	__psunsigned_t lowaddr;

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
	if (( KL_CONFIG_CH_CONS_INFO(get_nasid())->nasid) != get_nasid())
	    return;
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */
#if SABLE && SABLE_SYMMON
	/*
	 * Define SABLE_SYMMON for loading symmon when running under
	 * sable. There are no environment variables in sable and we
	 * don't really want to load the symmon image from disk or
	 * net. We assume that the sable startup file has been modified
	 * to load symmon into memory and that SABLE_SYMMON_INITPC
	 * contains the correct pc to jump to.
	 */
	if ((*(int *)SABLE_SYMMON_INITPC) == 0)
		return;
	init_pc = (int (*)())SABLE_SYMMON_INITPC;
	dbgmon_symstop = 1;
#else
	if (argc == 0 || (__psunsigned_t)argv < KXBASE)
	{
		return;
	}
	if (chkenv("nmiprom", environ))
	    dbgmon_nmiprom = 1;
	/* KLUDGE: don't load symmon if we've loaded ourselves at same
	 * spot as symmon
	 */
	if (chkenv("nodbgmon", environ) ||
	    KDM_TO_PHYS(start) < (PHYS_RAMBASE + 0x41000))
		return; 	/* debugging not requested */

	/*
	 * Filenames that end in .dbg force loading of the debugger
	 */
	argv0 = STR_ADDR(argv[0]);
	filelen = strlen(argv0);
	cp = &(argv0[filelen-4]);
	if (filelen > 4) {
		if (streq(".dbg", cp) || streq(".DBG", cp)) {
			if (streq(".dbg", cp))
				strcpy(cp, ".DBG");
			goto load_debugger;
		}
	}
	if (!chkenv("dbgmon", environ) && !dbgmon_always)
		return;	/* debugging not requested */

	{
	   static char cbuf[100];
		strcpy(cbuf, argv0);
		strcat(cbuf, ".DBG");
		argv0 = cbuf;
		argv[0] = CONVERT_BACK(argv0);
	}
load_debugger:

#endif /* SABLE && SABLE_SYMMON */

	/*
	 * copy args and environment
	 */
	init_str(&dbg_argv);
	for (wp = argv; wp && *wp; wp++) {
		if (new_str1(*wp, &dbg_argv))
			return;
	}

	/*
	 * put the addr for the debugger to return to into the environ
	 */
	lo_sprintf (pcstring, "startpc=0x%lx", start_pc);
	if (new_str1(CONVERT_BACK(pcstring), &dbg_argv))
		return;

	init_str(&dbg_environ);
	for (wp = environ; wp && *wp; wp++)
		if (new_str1(*wp, &dbg_environ))
			return;

#if !(SABLE && SABLE_SYMMON)
	/*
	 * boot in the debug monitor, it had better not overlay us!
	 * If dbgname is set, use that as the name of the debugger.
	 * This is useful for forcing a boot from a disk or a specific
	 * machine over the net, particularly when some new debugger
	 * feature is required.
	 * Else if we were booted over the net, try to find a symmon out on
	 * the network checking the disk list if we fail, otherwise try
	 * the vh on the drive we booted from, and finally the disk list.
	 */
	bzero(bootfile, MAX_BOOT_STR);
	if(cp=chkenv("dbgname", environ)) {
		strcpy(bootfile, cp);
	} else {
		/* get the device and possibly machine name out of the string */
		cp = argv0;
		cp = &cp[strlen(cp)];
		while ((cp > argv0) && *cp != '/' && *cp != ':'
		       && *cp != ')')
			--cp;
		if(cp == argv0)
			goto retrydisk;

		/* construct the symmon boot string */
		if(strncmp(argv0, "bootp", 5) == 0 ||
			strncmp(argv0, "bfs", 3) == 0) {
			strncpy(bootfile, argv0, (int)((cp+1)-argv0));
			strcat(bootfile, _SYMMON);
		}
		else {
			/* replace "...0)unix.DBG" with "...8)symmon" */
			strcpy(bootfile, fill_dbg(bootfile, argv0));

			/*  A ',' means that the last item in the pathname
			 * was a classic style disk pathname.  A path ending
			 * in "part*(" means the last item is a ARCS style
			 * pathname.  These pathnames need to have the
			 * vh partition added to the end of their path.
			 *
			 *  Otherwise the last item was a user specified
			 * type such as xfs().
			 */
			if ((bootfile[strlen(bootfile) - 1] == ',') ||
			    ((cp=strstr(bootfile,"part")) &&
			     (cp=strchr(cp,'(')) && (*(cp+1) == '\0')))
				strcat(bootfile, "8)symmon");
			else
				strcat(bootfile, "()symmon");
		}
	}
	cp = bootfile;
	if (strncmp(argv0, "bootp", 5) == 0)
		nettry = 1;
retrydisk:
	dprintf("Attempting to load debugger at \"%s\" ...\r\n", cp);

#ifdef SN0
        if (arcs_load(cp, SYMMON_LOADADDR(get_nasid()), &init_pc, &lowaddr))
#else
        if (arcs_load(cp, 0, &init_pc, &lowaddr))
#endif /* SN0 */
		init_pc = (int (*)()) -1L;
#ifdef _K64PROM32
	else
		init_pc = (int (*)())K032TOKPHYS((__psint_t)init_pc>>32);
#endif /* _K64PROM32 */

	if ((__psint_t)init_pc == -1) {
		/* eat the machine name out of path and try bootp()symmon.IPXX */
		if (nettry) {
			cp2 = strchr(cp, ')'); cp2++;
			while (*cp2 != ':' && *cp2 != '/') cp2++; cp2++;
			strcpy(cp, "bootp()");
			strcat(cp, cp2);
			nettry = 0;
			goto retrydisk;
		}
		/* can't use rootdev, and root might not be in
		 * environment; kernel arg list not yet available, so
		 * just try a compiled in list... */
		if(trydisks==0)
			dprintf("Trying alternate list\r\n");
		if(dsk_symmon[trydisks]) {
			cp = dsk_symmon[trydisks++];
			goto retrydisk;
		}
		dprintf("Couldn't find symmon there either.  No debugger loaded!\r\n");
		kdebug = 0;	/* no debugger... */
		return;
	}
#endif /* !(SABLE && SABLE_SYMMON) */

	/*
	 * See if we want to halt as soon as loaded.
	 */
	if (chkenv("dbgstop", environ))
		dbgmon_loadstop = 1;

	/* See if we want to halt after symbols are loaded and
	 * initialization is complete.
	 */
	if (chkenv("symstop", environ))
		dbgmon_symstop = 1;

	/*
	 * transfer control to dbgmon
	 * give the debugger our argc, argv, environ and the pc of our
	 * main routine so it can properly start us back up.
	 */
	kdebug = 1; /* set explicitly, since we know it's here! */

#if _K64PROM32
	{
		/* fix up environment as if it came from an ARCS PROM32, since
		 * symmon can be launched via prom or via unix.kdebug
		 */

		char **p;
		__int32_t *p2;

		p = dbg_argv.strptrs;
		for(p2 = (__int32_t *)p;
		   *p2 = (__int32_t)(__psint_t)(*p++); p2++) {
			if(*p2 != 0) {
				/* make it a 32-bit-K0seg addr */
				if(IS_KSEG0(*p)) {
					*p2 |= 0x80000000;
				}
				else {
					*p2 |= 0xa0000000;
				}
			}
		}

		p = dbg_environ.strptrs;
		for(p2 = (__int32_t *)p;
		   *p2 = (__int32_t)(__psint_t)(*p++); p2++) {
			if(*p2 != 0) {
				/* make it a 32-bit-K0seg addr */
				if(IS_KSEG0(*p)) {
					*p2 |= 0x80000000;
				}
				else {
					*p2 |= 0xa0000000;
				}
			}
		}
	}
#endif	/* _K64PROM32 */

#if defined(EVEREST) && defined(MAPPED_KERNEL) && defined(MULTIKERNEL)
	if (evmk_cellid) {
		/* If we're not executing in cell 0, then copy the
		 * dbg environment back to cell 0 so symmon can access it
		 * using simple translation.
		 * Useful for debug of EVMK_RELOCATE_CELL kernels.
		 */
		bcopy( &dbg_argv,
		       (void *)PHYS_TO_K0((__psunsigned_t)&dbg_argv & 0x00ffffffff),
		       sizeof(dbg_argv));

		bcopy( &dbg_environ,
		       (void *)PHYS_TO_K0((__psunsigned_t)&dbg_environ &0x00ffffffff),
		       sizeof(dbg_environ));
	}
#endif
	(*init_pc)(dbg_argv.strcnt, dbg_argv.strptrs, dbg_environ.strptrs, 0);

	/* shouldn't return */
}

static char *
fill_dbg(register char *bp, register char *cp)
{
	/* 
	 * ARCS paths look like "scsi()disk(1)rdisk()partition(0)/unix", while
	 * the "old" paths look like "dksc(0,1,0)unix", thus the different
	 * pieces of code here to manipulate the strings.
	 */
	register char *p;

	/*
	 * chop the partition number, ')' and the unix filename off of the
	 * end of the string - "0)unix".
	 */

	int cnt = 0;

	p = cp;
	p = &p[strlen(cp)];
	while (p > cp && *p != ')') {
		cnt++; p--;
	}
	cnt++;
	cnt = (strlen(cp)-cnt);
	strncpy (bp, cp, cnt);
	return (bp);
}

/*
 * init_str -- initialize string_list
 */
static void
init_str(register struct string_list *slp)
{
	slp->strp = slp->strbuf;
	slp->strcnt = 0;
	slp->strptrs[0] = 0;
}

/*
 * new_str1 -- add new string to string list
 */
static
new_str1(__promptr_t strp, register struct string_list *slp)
{
	register int len;

	if (slp->strcnt >= MAXSTRINGS - 1) {
		dprintf("too many args\n");
		return(-1);
	}

	len = strlen(STR_ADDR(strp)) + 1;
	if (slp->strp + len >= &slp->strbuf[STRINGBYTES]) {
		dprintf("args too long\n");
		return(-1);
	}

	slp->strptrs[slp->strcnt++] = slp->strp;
	slp->strptrs[slp->strcnt] = 0;
	strcpy(slp->strp, STR_ADDR(strp));
	slp->strp += len;
	return(0);
}
