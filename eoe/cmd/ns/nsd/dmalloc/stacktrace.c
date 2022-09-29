/*
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that (i) the above copyright notices and this
 * permission notice appear in all copies of the software and related
 * documentation, and (ii) the name of Silicon Graphics may not be
 * used in any advertising or publicity relating to the software
 * without the specific, prior written permission of Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SILICON GRAPHICS BE LIABLE FOR ANY SPECIAL,
 * INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY
 * THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


/*
    stacktrace.c

    Link with -lmld
*/
/*
    XXX THIS IS COMPLETELY WRONG AND DEFUNCT

    Old functions in this file (somebody wrote these a long time ago)
	void initstacktrace(argv) -- sets up signal handlers so that signals
				SEGV,BUS,ILL,QUIT produce a stack trace
				to stdout and exit(sig_number).
				argv[0] must stick around forever.
				argv[0] must also be the exact name
				of the executable file (bogus!)
	int stacktrace (filename, startpc, startsp, regs, getword)
			     -- prints a stack trace.  Opens the a.out
				file called filename for symbols,
				starts with startsp and regs,
				and uses getword() to get a word
				from an address (either from memory
				or from a core file I guess).
				Closes the a.out properly (didn't before).
				Reads register variables properly (I
				think it didn't before).
				Returns 0 on success (didn't before),
				-1 on failure.
    New functions:
	int stacktrace_print(int skip)
		Front end to stacktrace().  Prints the current stack trace.

	int simple_stacktrace_get(int skip, int n, void *trace[])
		Gets a stack trace consisting soly of pc's
		into the trace array.  At most n pc's are retrieved.
		If skip is 0, the trace will start at the return
		address from simple_stacktrace_get(); if it is 1,
		it will start at return address from the calling function, etc..
		The function returns the number of pc's actually retrieved.
		If it got lost due to a non-stack frame pointer
		(whatever that means) then the last entry in the
		trace will be zero.

	int simple_stacktrace_write(int fd, char *fmt, char *executable,	
							int n, void *trace[])
		Prints to fd the result of simple_stacktrace_get(), using
		symbols from the file executable.  executable is not
		closed (in case many stack traces need to be printed).
		If NULL is passed as executable,
		simple_stacktrace_get_argv0() is called to try to retrieve it.
		fmt is used to print the index in the stack trace,
		prepended to each line.
		XXX This is not very versatile.  For example,
		the caller might want to indent by a certain amount
		but leave out the line numbers.  I'll think about it...

	void stacktrace_cleanup()
		Closes the executable file opened by simple_stacktrace_write()
		if there is one open.

	char *simple_stacktrace_get_argv0()
		Attempts to get argv[0] of the currently running program.
		This is not reliable unless you know the
		main procedure did not overwrite its arguments.
		Of course it also will not produce a valid filename
		unless the program is in the current directory
		or was invoked with an absolute pathname.
*/
/*
    To Do:
	-- Doesn't follow past signal handler (dbx knows how).
	-- Can't find some functions (dbx finds them).
*/

#include "stacktrace.h"

 /* ------------------------------------------------------------------ */
 /* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
 /* | Reserved.  This software contains proprietary and confidential | */
 /* | information of MIPS and its suppliers.  Use, disclosure or     | */
 /* | reproduction is prohibited without the prior express written   | */
 /* | consent of MIPS.                                               | */
 /* ------------------------------------------------------------------ */
 /*  stacktrace.c 1.5 */

#undef NDEBUG /* don't even think about it!! */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <a.out.h>
#include <ar.h>

#include <ldfcn.h>	/* for definition of LDFILE and ldopen() */
/*
 * These are routines that are [currently] internal to libmld.  Either they
 * should be promoted to external routines or something that fulfills their
 * functions should be ...
 */
extern int ld_ifd_symnum(LDFILE *ldptr, int symnum);
extern char *st_str_ifd_iss(int ifd, int iss);

#include <sys/ptrace.h>	/* for definition of GPR_BASE */
#include <exception.h>	/* for definition of find_rpd() */
#include <sys/param.h>	/* for definition of MAXPATHLEN */
#include <sex.h>

#include <rld_interface.h>


static int ipd_adr (LDFILE *ldptr, unsigned long adr);
static int ipd_adr_all(int nldptrs, LDFILE *ldptrs[], long ldoffsets[], LDFILE **Whichldptr, long *Whichoffset, long adr);
static long get_ldptr_offset(LDFILE *ldptr);
static LDFILE *ldptrs[100];
static int nldptrs = 0;

static char ldfilename[4096 * 4];	/* must be big enough to hold all the names */ 
static char *ldfilename_end = ldfilename;/* after end of last name */
static char *ldfilenames[100];
static long ldoffsets[100];		/* add to symbol to get address */

static char *myname;

static int
agetword(unsigned int addr)
{
    return *(int *)addr;
}

#define myprintf printf

static char *regnames[] = {
		"r0/zero",	"r1/at",	"r2/v0",	"r3/v1",
		"r4/a0",	"r5/a1",	"r6/a2",	"r7/a3",
		"r8/t0",	"r9/t1",	"r10/t2",	"r11/t3",
		"r12/t4",	"r13/t5",	"r14/t6",	"r15/t7",
		"r16/s0",	"r17/s1",	"r18/s2",	"r19/s3",
		"r20/s4",	"r21/s5",	"r22/s6",	"r23/s7",
		"r24/t8",	"r25/t9",	"r26/k0",	"r27/k1",
		"r28/gp",	"r29/sp",	"r30/fp",	"r31/ra",
};
/*===================== New stuff starts here ================================*/

       /* just to keep my head clear-- changing to void * will take some work */
typedef long address;
#define numberof(sextoys) (sizeof(sextoys)/sizeof(*(sextoys)))
#define streq(s,t) (strcmp(s, t) == 0)
#define BITS(A)         (sizeof(A) * 8)
#define BIT(A,i)        (((A)[(i)/BITS(*(A))] >> ((i)%BITS(*(A)))) & 1)
#define A0 4
#define A1 5
#define SP 29
#define RA 31

#ifdef DO_IT_IN_C
/*
 * Don't laugh, it works... (but it's really slow).
 */
static address *_startpc, *_startsp, *_regs;

static void
_get_startpc_and_startsp(int sig, int code, struct sigcontext *scp)
{
    *_startpc = scp->sc_pc;
    *_startsp = scp->sc_regs[GPR_BASE+SP];
    bcopy(scp->sc_regs+GPR_BASE, _regs, 32 * sizeof(*_regs));
}

static int
get_startpc_and_startsp_and_regs(address *Startpc, address *Startsp,
				 address *regs[32])
{
    /* to make life easier, use SIGHUP because dbx ignores it */
    void (*ohandler)() = signal(SIGHUP, _get_startpc_and_startsp);
    _startpc = Startpc;
    _startsp = Startsp;
    _regs = regs;
    kill(getpid(), SIGHUP);
    signal(SIGHUP, ohandler);
    return 3;	/* # of extra levels to skip */
}
#endif /* DO_IT_IN_C */

#define XXX_SHOWMASK	/* keep this here in case I get curious */
#ifdef XXX_SHOWMASK
#include <stdio.h>

static void
showmask(address pc, address sp, address fp, unsigned long mask)
{
    int i;
    int printed = 0;
    fprintf(stderr, "pc=%#x, fp=%#x, sp=%#x, mask=%#x(", pc, sp, fp, mask);
    for (i = 0; i < 32; ++i)
	if (BIT(&mask, i)) {
	    if (printed)
		fprintf(stderr, ", ");
	    fprintf(stderr, "%d", i);
	    printed = 1;
	}
    fprintf(stderr, ")\n");
}

static char *
masktoa(int mask)
{
    int i, printed = 0;
    static char buf[32*3];
    buf[0] = 0;
    for (i = 0; i < 32; ++i)
	if (BIT(&mask, i)) {
	    if (printed)
		sprintf(buf+strlen(buf), ",");
	    sprintf(buf+strlen(buf), "%d", i);
	    printed = 1;
	}
    return buf;
}

#endif /* XXX_SHOWMASK */

/* use kipp's find_runtime_pdr, since libexc's find_pdr has
   assertions and its cache size is too small. */
#ifndef PDR_CACHE_SIZE
#define PDR_CACHE_SIZE  1024
#endif


#ifdef DEFUNCT
static RPDR* cache[PDR_CACHE_SIZE];

static RPDR *
find_runtime_pdr(unsigned long pc)
{
    RPDR *p, *end;
    u_int base, limit, hash, midPoint;

    /* see if pc is in cache */
    hash = (pc >> 2) & (PDR_CACHE_SIZE-1);
    end = &_procedure_table[PSIZE];
    p = cache[hash];
    if (p && (pc >= p->adr)) {
	if (p+1 < end) {
	    if (pc < (p+1)->adr) {
		return p;
	    }
	} else {
	    return p;
	}
    }

    /* do a binary search through the pdr table */
    base = 0;
    limit = PSIZE-1;
    while (base <= limit) {
	midPoint = (base + limit)>>1;
	p = &_procedure_table[midPoint];
	if (pc < p->adr) {
	    limit = midPoint - 1;
	} else {
	    if (p+1 < end) {
		if (pc < (p+1)->adr) {
		    cache[hash] = p;
		    return p;
		}
	    } else {
		cache[hash] = p;
		return p;
	    }
	    base = midPoint + 1;
	}
    }
    return 0;
}
#endif /* DEFUNCT */

#if _MIPS_SIM == _MIPS_SIM_ABI32
static RPDR *
find_runtime_pdr(unsigned long pc)
{
    static RPDR* cache[PDR_CACHE_SIZE];
    RPDR *p, *first, *last;
    unsigned long lo, hi, mid, hash;

    hash = (pc/4) & (PDR_CACHE_SIZE-1);
    first = &_procedure_table[0];
    last = &_procedure_table[PSIZE-1];
    p = cache[hash];

    /* see if pc is in cache */
    if (p)
	if (pc >= p->adr && (p == last || pc < (p+1)->adr))
	    return p;
	/* otherwise there was a collision and the wrong thing is in the cache*/

    /* so we don't need to be paranoid about exceeding bounds... */
    if (pc < (first+1)->adr)
	return cache[hash] = first;
    if (pc >= last->adr)
	return cache[hash] = last;

    /* do a binary search through the pdr table */
    lo = 1;		/* always lowest possible */
    hi = PSIZE-2;	/* always highest possible */
    while (lo <= hi) {
	mid = (lo + hi) / 2;
	p = &_procedure_table[mid];
	if (pc < p->adr)
	    hi = mid - 1;
	else if (pc >= (p+1)->adr)
	    lo = mid+1;
	else
	    return cache[hash] = p;
    }

    /* if we get here, the table is out of order
       and this algorithm needs to be rethought-- abort. */
    assert(lo <= hi);
}
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */

#define GETWORD(addr) (*(address *)(addr))
/*
 * The following routine just gets the pc's of a stack trace of depth at most n,
 * without looking up symbol names or anything.
 * Returns the number of pc's actually retrieved.
 * If it gets lost (due to non-sp frame pointer or bad pc)
 * the last entry filled in will be zeros.
 */
static int
notsosimple_stacktrace_get(int skip, int n, void *pcs[],
					    void *sps[],
					    void *fps[],
					    address regs[32])
{
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return 0;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */
    extern address _stacktrace_get_pc(),
		   _stacktrace_get_sp(), _stacktrace_get_regs(address *);
    address sp,pc,fp, regsbuf[32], regmask;
    int i;
    int ireg,nreg;

    static int use_find_rpd = -1;
    if (use_find_rpd == -1)
	use_find_rpd = !!getenv("_USE_FIND_RPD");	/* XXX */

    if (!n)
	return 0;		/* don't waste time */

    if (!regs)
	regs = regsbuf;

#ifdef DO_IT_IN_C
    skip += 1 + get_startpc_and_startsp_and_regs(&pc, &sp, regs);
#else
    pc = _stacktrace_get_pc();
    sp = _stacktrace_get_sp();
    _stacktrace_get_regs(regs);
    skip++;
#endif

    for (fp = 1, i = -skip; sp != 0 && fp != 0 && pc != 0 && i < n; i++) {
	/*
	 * This is balogna... we should be able to figure out this
	 * stuff just by looking at the local stack
	 */
	struct runtime_pdr *prpd;

	/*
	 * bleah!  not only that, but find_rpd() has assertions!!
	 * So we gotta circumvent them by using our own version, which is faster anyway.
	 */


	if (use_find_rpd)
	    prpd = find_rpd(pc);
	else
	    prpd = find_runtime_pdr(pc);

	if (!prpd || prpd == (struct runtime_pdr *)-1) {
	    static int complained_already = 0;
	    if (!complained_already) {
		fprintf(stderr,
		      "Can't find runtime procedure descriptor for %#x!\n", pc);
		fprintf(stderr, "You're using shared libraries, AREN'T YOU?\n");
	    }
	    /* fill in the last entry with NULL so it's evident that something went wrong */
	    if (i >= 0) {
		if (pcs) pcs[i] = NULL;
		if (sps) sps[i] = NULL;
		if (fps) fps[i] = NULL;
	    }
	    i++;
	    break;
	}

	if (prpd->framereg != GPR_BASE+29) {	/* XXX this happens-- should try to understand */
	    /*
	    myprintf("%#x has a non-sp framereg (%d)\n", prpd->adr,
		prpd->framereg);
	    */
	    if (i >= 0) {
		if (pcs) pcs[i] = NULL;
		if (sps) sps[i] = NULL;
		if (fps) fps[i] = NULL;
	    }
	    i++;
	    break;
	}

	fp = sp + prpd->frameoffset;

	if (i >= 0) {
	    if (pcs) pcs[i] = (void *)((int *)pc-1);
	    if (sps) sps[i] = (void *)sp;
	    if (fps) fps[i] = (void *)fp;
	}

#ifdef XXX_SHOWMASK
	{
	    static int env_showmask = -1;
	    if (env_showmask == -1)
		env_showmask = !!getenv("_STACKTRACE_SHOWMASK"); /* XXX */
	    if (env_showmask)
		showmask(pc, sp, fp, prpd->regmask);
	}
#endif /* XXX_SHOWMASK */

next:
	sp = fp;

	/* restore regs */
	regmask = prpd->regmask;
	for (nreg = 0, ireg = 31; ireg >=0; ireg--) {
	    /* if (BIT(&prpd->regmask, ireg) != 0) */
	    if (regmask & (1<<ireg)) {
		regs[ireg] = GETWORD(fp+prpd->regoffset-(nreg*4));
		nreg++;
	    }
	    if (regs == regsbuf)
		break;	/* caller doesn't want regs; all we care about is RA */
	}
	if (prpd->pcreg == 0)
	    pc = GETWORD(fp-4);
	else
	    pc = regs[prpd->pcreg];	/* always RA if anything */
    } /* for */
    return i < 0 ? 0 : i;
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
}

extern int
stacktrace_get(int skip, int n, void *pcs[])
{
    return notsosimple_stacktrace_get(skip+1, n, pcs, NULL,NULL, NULL);
}

#include <dem.h>

static void
funname_to_human_readable_funname(char *fun, char *hrfun, int siz)
{
    char buf[MAXDBUF];
    int result;
    if (siz > MAXDBUF)
	siz = MAXDBUF;

    result = demangle(fun, buf);
    strncpy(hrfun, buf, siz-1);
    hrfun[siz-1] = 0;
}











/*=============== Begin some portable stuff ==============================*/
/*===== (someday, separate out the functions that are machine-specific) ==*/

/* main() can alter or clobber its argv, so it may
   be impossible to get argv[0] by tracing the stack.
   Allow the application to tell us what argv[0] is explicitly
   if it is going to be so barbaric.
*/
static char saved_argv0[MAXPATHLEN+1];
static char saved_executable[MAXPATHLEN+1];

extern void
stacktrace_set_executable(const char *executable)
{
    if (executable)
	strncpy(saved_executable, executable, MAXPATHLEN);
    else
	saved_executable[0] = '\0';
}

static char *
find_in_path(char *path, char *filename, int mode, char fullname[MAXPATHLEN+1])
{
    char *p = path, *dir;

    if (strchr(filename, '/'))
	return strncpy(fullname, filename, MAXPATHLEN);
    else {
	for (dir = p; dir && *dir; dir = p) {
	    while (*p && *p != ':')
		p++;
	    if (!strncmp(dir, ".", p - dir)) /* also satisfied if p==dir */
		strcpy(fullname, filename);
	    else
		sprintf(fullname, "%.*s/%s", p - dir, dir, filename);
	    /* not right under setuid, but FUCK IT */
	    if (access(fullname, mode) != -1)
		return fullname;
	    if (*p)
		p++;
	}
	fullname[0] = '\0';
	return NULL;
    }
}

extern char *
stacktrace_get_executable()
{
    char *argv0;

    if (saved_executable[0])
	return saved_executable;
    
    argv0 = stacktrace_get_argv0();

    if (argv0 && argv0[0]) {
	return find_in_path(getenv("PATH"), argv0, X_OK, saved_executable);
    } else
	return NULL;
}

extern void
stacktrace_set_argv0(const char *argv0)
{
    if (argv0)
	strncpy(saved_argv0, argv0, MAXPATHLEN);
    else
	saved_argv0[0] = '\0';
    saved_executable[0] = '\0';	/* XXX ? should do this? should recalc? */
}
/*========================== End of portable stuff =========================*/
#define INRANGE(foo,bar,baz) ((foo(bar))&&((bar)baz))
#define STACK_END_NORMAL 0x80000000
#define STACK_END_R8000  0x7fff8000
#define IS_STACK_ADDRESS(p) \
	INRANGE((unsigned long)(p) <, *(unsigned long *)(p), < stack_end)

extern char *
stacktrace_get_argv0()
{
    int argc;
    unsigned long *p;
    extern char **environ;
    unsigned long stack_end;

    if (saved_argv0[0])
	return saved_argv0;

    if (getenv("_STACKTRACE_ARGV0"))
	return strcpy(saved_argv0, getenv("_STACKTRACE_ARGV0"));

    /*
	Assume the stack looks like this (walking backwards from the end)
	    zeros
	    garbage
	    env strings
	    argv strings
	    garbage
	    zero (terminating envp array)
	    envp array
	    zero (terminating argv array)
	    argv array
	    argc
    */

    stack_end = ((unsigned long) environ < STACK_END_R8000 ? STACK_END_R8000
							   : STACK_END_NORMAL);
#define FIX_BUS_ERROR_324830
#ifdef FIX_BUS_ERROR_324830
    /* Weird bus error on some 6.2 alphas
       if we access near the end of the stack... see bug #324830 */
    if (!getenv("_MALLOC_DONT_FIX_BUS_ERROR_324830"))
	stack_end -= 4*4096;
#endif

    p = (unsigned long *)stack_end - 1;	  /* end of stack, hopefully */

    while (!IS_STACK_ADDRESS(p))
	p--;

    /* p is now pointing at last element of envp array */
    while (*p)	/* not IS_STACK_ADDRESS(p), since putenv() messes that up */
	p--;

    /* p is now hopefully poing at the zero terminating the argv array */
    if (*p != 0)
	return NULL;	/* shouldn't happen */

    p--;

    /* p is now pointing at the last element of argv array */

    argc = 0;
    while (IS_STACK_ADDRESS(p))
	argc++, p--;

    /* p is now pointing at something which should contain argc */
    if (*p != argc)
	return 0;	/* shouldn't happen */

    if (argc == 0) {
	/* fprintf(stderr, "Program was run with no args???\n"); */
	return 0;
    }

    p++;

if (getenv("_STACKTRACE_VERBOSE_WHICH_STACK_END"))
fprintf(stderr, "%d: line %d\n", getpid(), __LINE__);
    /* p is now pointing at first element of argv array */

    assert(strlen((char *)*p) + 1 <= sizeof(saved_argv0));
    return strcpy(saved_argv0, (char *)*p);
}

extern void
stacktrace_cleanup()
{
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */
    while (nldptrs > 0) {
	nldptrs--;

	fprintf(stderr, "Closing \"%s\"... ", ldfilenames[nldptrs]);

/* XXX ldaclose leaks like a sieve!  (see leak2 program and bug #176726)
   ldclose doesn't leak quite as badly... */
if (!getenv("_STACKTRACE_USE_LDACLOSE"))
    ldclose(ldptrs[nldptrs]);
else

	ldaclose(ldptrs[nldptrs]);
	ldptrs[nldptrs] = NULL;

	fprintf(stderr, "done.\n");
    }

    ldfilename[0] = '\0';
    ldfilename_end = ldfilename;
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
}

static char *
save_ldfilename(char *name)
{
    char *old_end = ldfilename_end;
    ldfilename_end += strlen(name) + 1;
    assert(ldfilename_end <= ldfilename + sizeof(ldfilename));
    strcpy(old_end, name);
    return old_end;
}

/*
 * Open executable and all rld libs.
 * (the name is defunct; it is left over from when
 * we opened the executable and read its library list
 * rather than asking rld)
 */
static int
add_object_and_libs(char *filename, int do_children)
{
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return 0;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */
    fprintf(stderr, "Opening \"%s\"... ", filename);
    assert(nldptrs < numberof(ldptrs));
    if (!(ldptrs[nldptrs] = ldopen(filename, NULL))) {
	fprintf(stderr, "Can't open object \"%s\"\n", filename);
	return 0;	/* failure */
    }
    fprintf(stderr, "done.\n");
    ldfilenames[nldptrs] = save_ldfilename(filename);
    ldoffsets[nldptrs] = 0;	/* main executable doesn't get relocated */
    nldptrs++;

    if (do_children) {
	char *so_name;
	for (so_name = (char *)_rld_new_interface(_RLD_FIRST_PATHNAME);
	     so_name;
	     so_name = (char *)_rld_new_interface(_RLD_NEXT_PATHNAME)) {
	    if (streq(so_name, "MAIN")) {
		fprintf(stderr, "Skipping MAIN in dso list.\n");
		continue;
	    }
	    fprintf(stderr, "Opening \"%s\" ...", so_name);
	    assert(nldptrs < numberof(ldptrs));
	    if (ldptrs[nldptrs] = ldopen(so_name, NULL)) {
		ldfilenames[nldptrs] = save_ldfilename(so_name);
		ldoffsets[nldptrs] = get_ldptr_offset(ldptrs[nldptrs]);

		nldptrs++;
	    } else
		fprintf(stderr, "(couldn't open %s)", so_name);
	    fprintf(stderr, "done.\n");
	}
    }

    return 1;	/* success-- opened at least this one */
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
}

/*
    XXX must see how expensive this is, and maybe do it less often...
*/
static int
ldfiles_are_in_sync_with_rld(char *assumed_rld_main_executable)
{
    int i;
    char *so_name;

    for (i = 0, so_name = (char *)_rld_new_interface(_RLD_FIRST_PATHNAME);
	 i < nldptrs && so_name;
	 i++, so_name = (char *)_rld_new_interface(_RLD_NEXT_PATHNAME)) {
	if (streq(so_name, "MAIN"))
	    so_name = assumed_rld_main_executable;
	if (!streq(so_name, ldfilenames[i]))
	    break;
    }
    if (i < nldptrs || so_name)
	return 0;
    return 1;
}

static int
simple_stacktrace_write_init(char *filename)
{
    if (!filename)
	return 0;	/* failure */
    if (!ldfiles_are_in_sync_with_rld(filename)) {
	if (nldptrs)
	    fprintf(stderr, "Symbol table out of sync; reinitializing.\n");
	stacktrace_cleanup();

	add_object_and_libs(filename, 1);
    }
    if (nldptrs == 0)
	return 0;	/* failure */

    
    return 1;	/* success */
}

extern int
simple_stacktrace_write(int fd, char *fmt, char *filename, int n, void *trace[])
{
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return 0;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */
    LDFILE *ldptr;
    char nicename[1000];
    char buf[MAXPATHLEN+100];
    address pc;
    int i, iproc, ifd;
    struct pdr pd;
    SYMR asym;
    char *pname;
    int line;
    long offset;

    if (n <= 0)
	return 0;	/* success, even if we can't open the file */

    buf[0] = 0;

    if (filename == NULL)
	filename = stacktrace_get_executable();
    if (filename == NULL) {
	sprintf(buf+strlen(buf), "(pid=%d, cannot find name of executable, setenv _STACKTRACE_ARGV0 filename)\n", getpid());
	write(fd, buf, strlen(buf));
	return -1;
    }

    if (!simple_stacktrace_write_init(filename)) {
	sprintf(buf+strlen(buf), "(cannot read in %s)", filename);
	write(fd, buf, strlen(buf));
	return -1;	/* failure */
    } 

    if (!fmt)
	fmt = "%4d ";
    for (i = 0; i < n; ++i) {
	sprintf(buf, fmt, i);

	pc = (address) trace[i];
	iproc = ipd_adr_all(nldptrs, ldptrs, ldoffsets, &ldptr, &offset, pc);
	if (ldgetpd(ldptr, iproc, &pd) == FAILURE) {
	    sprintf(buf+strlen(buf), "(ldgetpd failed)\n");
	    write(fd, buf, strlen(buf));
	    continue;
	}

	if (ldtbread(ldptr,pd.isym,&asym) == FAILURE) {
	    sprintf(buf+strlen(buf), "(cannot read symbol %d)\n", pd.isym);
	    write(fd, buf, strlen(buf));
	    continue;
	}

	pname = (char *)ldgetname(ldptr, &asym);
	if (pname == NULL) {
	    sprintf(buf+strlen(buf), "(ldgetname failed)\n");
	    write(fd, buf, strlen(buf));
	    pname = "???";
	}

	ifd = ld_ifd_symnum(ldptr, pd.isym);

	funname_to_human_readable_funname(pname, nicename, sizeof(nicename));
	sprintf(buf+strlen(buf), "%s(", nicename);

	if (pd.iline+((pc-pd.adr)/4) > SYMTAB(ldptr)->hdr.ilineMax)
	    line = -1;
	else
	    line = (int)SYMTAB(ldptr)->pline[pd.iline+((pc-pd.adr)/4)];
	    
	sprintf(buf+strlen(buf), ") [%s:%d, 0x%x]\n", st_str_ifd_iss(ifd, 1),
	    line,
	    pc);
	write(fd, buf, strlen(buf));
    }
    return 0;		/* success */
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
}


/*
 * Get function,file,line from a pc
 */
extern void
stacktrace_get_ffl(void *_pc, char *fun, char *file, int *line,
			  int funbufsiz, int filebufsiz)
{
    address pc = (address) _pc;
    char buf[100];
    struct pdr pd;
    SYMR asym;
    int iproc, ifd;
    char *pname;
    LDFILE *ldptr;
    long offset;

    if (fun && funbufsiz > 0) fun[0] = fun[funbufsiz-1] = '\0';
    if (file && filebufsiz > 0) file[0] = file[filebufsiz-1] = '\0';
    if (line) *line = -1;
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */

    if (!simple_stacktrace_write_init(stacktrace_get_executable())) {
	    /* XXX do a descriptive error message! */
	    sprintf(buf, "(couldn't get symbol table???)");
	    strncpy(fun?fun:file?file:buf, buf,
		   (fun?funbufsiz:file?filebufsiz:numberof(buf)) - 1);
	    return;
    }

    iproc = ipd_adr_all(nldptrs, ldptrs, ldoffsets, &ldptr, &offset, pc);
    if (ldgetpd(ldptr, iproc, &pd) == FAILURE) {
	sprintf(buf, "(ldgetpd failed)");
	strncpy(fun?fun:file?file:buf, buf,
	       (fun?funbufsiz:file?filebufsiz:numberof(buf)) - 1);
	return;
    }
    if (pd.isym == -1) {
	sprintf(buf, "???(ipd_adr=%d)", ipd_adr(ldptr, pc-offset));
	strncpy(fun?fun:file?file:buf, buf,
	       (fun?funbufsiz:file?filebufsiz:numberof(buf)) - 1);
	return;
    }

    if (fun) {
	if (ldtbread(ldptr,pd.isym,&asym) == FAILURE) {
	    sprintf(buf, "(cannot read symbol %d)\n", pd.isym);
	    strncpy(fun, buf, funbufsiz-1);
	} else if ((pname = (char *)ldgetname(ldptr, &asym)) == NULL) {
	    sprintf(buf, "(ldgetname failed)\n");
	    strncpy(fun, buf, funbufsiz-1);
	} else {
	    funname_to_human_readable_funname(pname, fun, funbufsiz-1);
	    if (getenv("_DUMP_PROCEDURE_TABLE"))
		sprintf(fun+strlen(fun), "{{{%d}}}", ipd_adr(ldptr, pc-offset));
	}
    }

    if (file) {
	ifd = ld_ifd_symnum(ldptr, pd.isym);
	strncpy(file, (char *)st_str_ifd_iss(ifd, 1), filebufsiz-1);
    }

    if (line)
	if (pd.iline+((pc-offset-pd.adr)/4) > SYMTAB(ldptr)->hdr.ilineMax)
	    *line = -1;
	else
	    *line = (int)SYMTAB(ldptr)->pline[pd.iline+((pc-offset-pd.adr)/4)];
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
}

/* real easy front end to stacktrace */
extern int
stacktrace_print(int skip)
{
    void *pcs[1], *sps[1];
    address regs[32];
    int n;
    n = notsosimple_stacktrace_get(skip+1, 1, pcs, sps, NULL, regs);
    if (n == 1)
	return stacktrace(stacktrace_get_executable(),
		          (int)pcs[0], (int)sps[0], (int *)regs,
		          agetword);
    else
	return -1;
}

extern int
simple_stacktrace_print(int fd, char *fmt, int skip, int n)
{
    void *trace[1000];
    if (n < 0 || n > numberof(trace))
	n = numberof(trace);
    n = stacktrace_get(skip, n, trace);
    if (n < 0)
	return n;
    return simple_stacktrace_write(fd, fmt, NULL, n, trace);
}



/*================== Begin stuff I found in old dbx source ==================*/



/*
    The rest of this file is adapted from stuff I found
    in old dbx source.
    It doesn't demangle, but it does print function arguments (crudely)
    when printing the stack trace.
    Also, the above stuff uses ipd_adr() from below.
*/

extern int
stacktrace(filename, startpc, startsp, regs, getword)
char * filename;
int *regs;
int (*getword)(unsigned);
{
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return 0;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */
    char nicename[1000];
    LDFILE *ldptr;
    unsigned sp,pc,fp,istack,value;
    int isym,ifd,ireg,nreg;
    char *pname;
    SYMR asym;
    PDR apd;
    pPDR ppd = &apd;
    AUXU aux,auxpc,*ldgetaux(LDFILE *, int);	/* missing from 5.3 headers */
    int hostsex = gethostsex();
    int i,j;

    myprintf("Registers on entry:\n");
    for (i = 0; i < 32; i += 3) {
	    for (j = i; j < 32 && j < i+3; j++)
		    myprintf("%s: 0x%08x\t", regnames[j], regs[j]);
	    myprintf("\n");
    }

    auxpc.ti.bt = btUChar;
    auxpc.ti.tq0 = tqPtr;

    ldptr = ldopen(filename, NULL);
    if (ldptr == NULL) {
	myprintf("cannot read in %s", filename);
	return (-1);
    } /* if */

    myprintf("Stack trace -- last called first\n");
    for (pc = startpc, sp = startsp, fp = 1, istack = 0; sp != 0 && fp != 0 && pc != 0; istack++) {
	ldgetpd(ldptr, ipd_adr(ldptr, pc), ppd);
	isym = ppd->isym;
	if (ldtbread(ldptr,isym,&asym) == FAILURE) {
	    myprintf("cannot read %d symbol\n", ppd->isym);
	    goto next;
	} /* if */
	pname = (char *)ldgetname(ldptr, &asym);
	funname_to_human_readable_funname(pname, nicename, sizeof(nicename));
	if (ppd->framereg != GPR_BASE+29) {
	    myprintf("%s has a non-sp framereg (%d)\n", nicename,
		ppd->framereg);
	    ldaclose(ldptr);
	    return (-1);
	} /* if */

	ifd = ld_ifd_symnum(ldptr, ppd->isym);
	fp = sp + ppd->frameoffset;
	myprintf("%4d %s(", istack, nicename);
	if (asym.index == indexNil) {
	    myprintf ("0x%x, 0x%x, 0x%x, 0x%x", (*getword)(fp), (*getword)(fp+4), 
		(*getword)(fp+8), (*getword)(fp+12));
	} else {
	    do {
		if (ldtbread(ldptr, ++isym, &asym) == FAILURE)
		    break;
		if (asym.st == stBlock || asym.st == stEnd || asym.st == stProc
		    || asym.st == stStaticProc)
		    break;
		if (asym.st != stParam)
		    continue;
		if (isym != ppd->isym+1)
		    myprintf(", ");
		myprintf("%s = ", ldgetname(ldptr, &asym));
		if (asym.sc == scAbs)
		    myprintf("%d", value=(*getword)(fp+asym.value));
		else if (asym.sc == scRegister && asym.value < 32)
		    myprintf("%d", value=regs[asym.value]);
		else if (asym.sc == scVarRegister && asym.value < 32)
		    myprintf("(*%d) %d", regs[asym.value], value=(*getword)(regs[asym.value]));
		else if (asym.sc == scVar) {
		    value = (*getword)(fp+asym.value);
		    myprintf("(*%d) %d", value=(*getword)(value));
		} /* if */
		if (asym.index != indexNil) {
		    aux = *ldgetaux(ldptr, asym.index);
		    if (PFD(ldptr)[ifd].fBigendian != (hostsex == BIGENDIAN))
			swap_aux(&aux, ST_AUX_TIR, hostsex);
		    if (aux.isym == auxpc.isym) {
			int j, buf[11];
			for (j = 0; j < 10; j++)
			    buf[j] = (*getword)(value + (j*4));
			buf[10] = 0;
			myprintf (" \"%s\"", buf);
		    } /* if */
		} /* if */
	    } while (isym < PFD(ldptr)[ifd].csym + PFD(ldptr)[ifd].isymBase);
	} /* if */

if (getenv("_STACKTRACE_VERBOSE"))	/* XXX clean this up */
	myprintf (") [%s:%d, pc=0x%x, sp=0x%x, fp=0x%x, mask=%s]\n", st_str_ifd_iss(ifd, 1),
	    SYMTAB(ldptr)->pline[ppd->iline+((pc-ppd->adr)/4)],
	    pc, sp, fp, masktoa(ppd->regmask));
else
	myprintf (") [%s:%d, 0x%x]\n", st_str_ifd_iss(ifd, 1),
	    SYMTAB(ldptr)->pline[ppd->iline+((pc-ppd->adr)/4)],
	    pc);

	/* set up for next time */
next:
	sp = fp;
	/* restore regs */
	for (nreg = 0, ireg = 31; ireg >=0; ireg--) {
	    if (((1<<ireg)&ppd->regmask) != 0) {
		regs[ireg] = (*getword)(fp+ppd->regoffset-(nreg*4));
		/*
		 * NOTE: The original code was missing the following line.
		 * It didn't get us lost (since pc was always the first
		 * register) but it probably printed some wrong values.
		 */
		nreg++;
	    } /* if */
	} /* for */
	if (ppd->pcreg == 0)
	    pc = (*getword)(fp-4);
	else
	    pc = regs[31];
    } /* for */
    ldaclose(ldptr);
    return 0;
#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
} /* stacktrace */


int static
ipd_adr (ldptr, adr)
LDFILE *ldptr;
unsigned long adr;

{
#if _MIPS_SIM != _MIPS_SIM_ABI32
    return -1;
#else /* _MIPS_SIM == _MIPS_SIM_ABI32 */
    int		ilow;
    int		ihigh;
    int		ihalf;
    int		ilowold;
    int		ihighold;
    int		ipd;
    PDR		apd;


    ilow = 0;
    ihigh = SYMHEADER(ldptr).ipdMax;
    /* binary search proc table */
    while (ilow < ihigh) {
	ihalf = (ilow + ihigh) / 2;
	ilowold = ilow;
	ihighold = ihigh;
	ldgetpd(ldptr, ihalf, &apd);
	if (adr < apd.adr)
	    ihigh = ihalf;
	else if (adr > apd.adr)
	    ilow = ihalf;
	else {
	    ilow = ihigh = ihalf;
	    break;
	} /* if */
	if (ilow == ilowold && ihigh == ihighold)
	    break;
    } /* while */

    ipd =  ((ilow < ihigh || ihigh < 0) ? ilow : ihigh);

    return (ipd);

#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
} /* ipd_adr */

/*
    Get symbol table value of first extern function in the object file.
    Get its name.
    Ask rld for the address of that name.
    Offset is the address minus the symbol table value.
*/
static long
get_ldptr_offset(LDFILE *ldptr)
{
    int ilow, ihigh, i;
    unsigned long symbol_table_value, address_in_memory;
    struct pdr pd;
    SYMR asym;
    char *name;

    ilow = 0;
    ihigh = SYMHEADER(ldptr).ipdMax;
    for (i = ilow; i <= ihigh; ++i) {
	    if (ldgetpd(ldptr, i, &pd) == FAILURE) {
		fprintf(stderr, "\nget_ldptr_offset: ldgetpd(i=%d) failed!\n", i);
		return 0;
	    }

	if (pd.isym == -1) {
	    fprintf(stderr, "\nget_ldptr_offset: pd.isym == -1!\n");
	    return 0;
	}
	if (ldtbread(ldptr,pd.isym,&asym) == FAILURE) {
	    fprintf(stderr, "\nget_ldptr_offset: ldtbread failed!\n");
	    return 0;
	}
	/* XXX should move this down below, but need name for diagnostic msg */
	if ((name = (char *)ldgetname(ldptr, &asym)) == NULL) {
	    fprintf(stderr, "\nget_ldptr_offset: ldgetname failed!\n");
	    return 0;
	}

	if (asym.st != stProc) {
	    if (getenv("_STACKTRACE_LDPTR_DEBUG"))
		fprintf(stderr, "(%s isn't a global procedure; going on to next)\n", name);
	    continue;
	}
	break;
    }

    if (i > ihigh) {
	fprintf(stderr, "\nget_ldptr_offset: everything was static??\n");
	return 0;
    }

    if (getenv("_STACKTRACE_LDPTR_DEBUG"))
	fprintf(stderr, "get_ldptr_offset: getting offset by comparing address with symbol value of \"%s\"\n", name);

    symbol_table_value = pd.adr;
    address_in_memory = (unsigned long) _rld_new_interface(_RLD_NAME_TO_ADDR, name);

    if (address_in_memory == 0) {
	fprintf(stderr, "(couldn't figure out delta, assuming 0) ");
	return 0;
    }

    if (address_in_memory != symbol_table_value) {
	fprintf(stderr, "\n======= SURPRISE! ======\n");
	fprintf(stderr, "\tValue of %s in file is %#x, in memory it's %#x\n",
		name, symbol_table_value, address_in_memory);
    }

    return address_in_memory - symbol_table_value;
}

/*
 * Search through all objects in the ldptrs array
 * looking for the procedure which adr falls inside.
 * Returns the procedure index, and sets *Whichldptr to
 * the ldptr it was found in.
 *
 * XXX this is WRONG-- this assumes that a procedure's
 * address in memory is the same as the value of the corresponding symbol.
 * This seems to be true in every case I've seen so far (the values
 * in a given .so begin at a seemingly arbitrary address, and none
 * of them happen to overlap), but obviously this is unreliable.
 * I assume that if the addresses of two .so's overlap, then
 * rld relocates one of them to a new area; I don't know how this
 * works, and I've never seen it happen.
 *
 * (actually, now I try to calculate the offset, but it doesn't quite work yet
 * for dlopened objects, and I've never seen it in action...)
 * 
 * XXX should make a better procedure that doesn't duplicate work
 */
int static
ipd_adr_all(nldptrs, ldptrs, ldoffsets, Whichldptr, Whichoffset, adr)
int nldptrs;
LDFILE *ldptrs[], **Whichldptr;
long ldoffsets[], *Whichoffset;
address adr;
{
    int i;
    PDR pd;

    assert(nldptrs > 0);
    for (i = 0; i < nldptrs; ++i) {
	ldgetpd(ldptrs[i], 0, &pd);
	if (adr < pd.adr + ldoffsets[i])
	    continue;
	ldgetpd(ldptrs[i], SYMHEADER(ldptrs[i]).ipdMax-1, &pd);
#define ROUNDUP(a,b) (((a)+(b)-1)/(b)*(b)) /* round up a to a multiple of b */
	if (adr > ROUNDUP((unsigned)pd.adr+1,4096) + ldoffsets[i])
	       /* XXX WRONG WRONG WRONG!!!!!! */
	       /* NEED TO FIND THE ADDRESS OF THE END OF THE LAST PROCEDURE!! */
	    continue;
	*Whichldptr = ldptrs[i];
	*Whichoffset = ldoffsets[i];
	return ipd_adr(*Whichldptr, adr - ldoffsets[i]) + ldoffsets[i];
    }
    /* do something arbitrary if we are totally lost */
    i = 0;
    if (nldptrs >= 2 && !SYMTAB(ldptrs[i]))
	i = 1;
    *Whichldptr = ldptrs[i];
    *Whichoffset = ldoffsets[i];
    return ipd_adr(*Whichldptr, adr - ldoffsets[i]) + ldoffsets[i];
}

void static
dumpscp(scp)
register struct sigcontext *scp;
{
	register int i, j;

	myprintf("sigcontext\n");
	myprintf("PC: 0x%08x, CAUSE: 0x%08x, BADVADDR: 0x%08x\n",
	    scp->sc_pc, scp->sc_cause, scp->sc_badvaddr);

	myprintf("OWNEDFP: %d, FP_CSR: 0x%08x\n", scp->sc_ownedfp,
	    scp->sc_fpc_csr);

	if (scp->sc_ownedfp == 0)
	    return;
	myprintf("fp regs\n");
	for (i = 0; i < 32; i += 4) {
		for (j = i; j < i+4; j++)
			myprintf("%2d: 0x%08x\t", j, scp->sc_fpregs[j]);
		myprintf("\n");
	}
}

void static
handler(sig, code, scp)
int sig, code;
struct sigcontext *scp;
{
	int regs[32];
	int i;

	switch(sig){
	case SIGSEGV:
	    myprintf("SIGSEGV signal caught\n");
	    break;
	case SIGBUS:
	    myprintf("SIGBUS signal caught\n");
	    break;
	case SIGILL:
	    myprintf("SIGILL signal caught\n");
	    break;
	case SIGQUIT:
	    myprintf("SIGQUIT signal caught\n");
	    break;
	default:
	    myprintf("Unknown signal caught: signo = %d\n",sig);
	    break;
	}
	if (getenv("_STACKTRACE_DONT_TRACE"))
	    return;

	dumpscp(scp);
	/* XXX ARGH-- evidently this messes up the stack, so can't return */

	/* in sherwood, scp->sc_regs is an array of 64-bit quantities,
	   but the stacktrace program is old and expects an array of ints. */
	for (i = 0; i < 32; ++i)
	    regs[i] = (int)*(long long *)&scp->sc_regs[i];

	stacktrace(myname,
		   (int)*(int *)&scp->sc_pc,
		   (int)*(int *)&scp->sc_regs[GPR_BASE+29],
		   regs,
		   agetword);
	if (sig != SIGQUIT)
	    exit(sig);
}

#ifdef NOTUSED
static int myfd;

ugmyprintf(format, a, b, c, d)
char *format;
{
    char buf[512];

    sprintf (buf, format, a, b, c, d);
    write(myfd, buf, strlen(buf));
}
#endif /* NOTUSED */

#ifdef __cplusplus
typedef void (*sigret)(...);
#else
typedef void (*sigret)();
#endif

extern void
initstacktrace(argv)
char **argv;
{

	
#ifdef NOTUSED
    myfd = open("/dev/tty", O_RDWR);
    if (myfd < 0)
	perror("initstacktrace cannot open /dev/tty");
#endif /* NOTUSED */

    struct sigcontext *scp;

    scp = NULL;	/* suppress stupid compiler warning */

    if (BITS(scp->sc_regs[0]) != 32 && !getenv("_TRY_SHERWOOD_STACKTRACE")) {
	/* fprintf(stderr, "Can't init signal handler to do stack traces \non SEGV and BUS (but I still love sherwood)\n"); */
	return;
    }

    if (argv)
	myname = argv[0];
    else
	myname = stacktrace_get_executable();
    signal(SIGSEGV, (sigret)handler);
    signal(SIGBUS, (sigret)handler);
    signal(SIGILL, (sigret)handler);
    signal(SIGQUIT, (sigret)handler);
}
/*================== End stuff I found in old dbx source ==================*/
