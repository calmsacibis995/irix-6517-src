/*
 * idbg.c - Integrated debugging module
 *
 */

#ident "symmon/idbg.c: $Revision: 1.68 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/idbg.h>
#include <setjmp.h>
#include <fault.h>
#include <arcs/spb.h>
#include <arcs/debug_block.h>
#include <sys/idbg.h>
#include "dbgmon.h"
#include "mp.h"
#include <libsc.h>
#include <libsk.h>
#include <sys/immu.h>
#include <sys/mapped_kernel.h>

static int ffunc(char *fname, dbgoff_t *table, dbgoff_t **fp);
static void copyregs(k_machreg_t *from, k_machreg_t *to);
static int _ubt(dbgoff_t *sp, int argc, char *argv[]);
int bp_try_stop_cpus(void);

k_machreg_t k_gp;

/* symbolic debugging */
struct dbstbl *dbstab;

char *nametab;
static dbgoff_t *isptr;
inst_t *lowest_text;
int symmax;

static void
bad_addr(void)
{
	register volatile struct generic_pda_s *gp;

	gp = (volatile struct generic_pda_s *)&GEN_PDA_TAB(cpuid());
	printf("Bad Address 0x%x, CAUSE 0x%x EPC 0x%x RA 0x%x\n",
		gp->regs[R_BADVADDR],
		gp->regs[R_CAUSE], gp->regs[R_EPC], gp->regs[R_RA]);
}

/*
 * The kernel and idbg communicate through a table pointed to by isptr.
 *
 * isidbg - return TRUE if the idbg area is valid
 * ALSO set k_gp to kernels gp
 *
 */
#ifdef NETDBX
int
#else
static int
#endif /* NETDBX */
isidbg(void)
{
	db_t *db = (db_t *)GDEBUGBLOCK;

	/* if kernel setup idbg_base from restart block then use that */
	if (db != NULL && (IS_KSEGDM(db->db_idbgbase) ||
			   IS_MAPPED_KERN_SPACE(db->db_idbgbase))) {
		isptr = (dbgoff_t *)db->db_idbgbase;
	} else {
		isptr = 0;
		return  0;
	}

	if (isptr->s_type == DO_ENV) {
		k_gp = isptr->s_gp;
		return(1);
	}
	return(0);
}

/*
 * Symmon looks for DO_ENV at the first location in the idbg
 * communications area to decide whether it can make idbg calls in the
 * kernel.  If the kernel has rebooted it may have left the idbg comm.
 * area in a valid state which could cause symmon to crash.  Therefore,
 * Symmon always clears the DO_ENV flag when it comes up.  The kernel
 * should only set up idbg area after it has called symmon.  In Simplex
 * case this is called in _dbgmon() when symmon first comes up.  In MP
 * case (IP5), this is called every time the Monitor is downloaded.
 *
 */
void
idbg_cleanup(void)
{
	db_t *db = (db_t *)GDEBUGBLOCK;

	validate_brkpts();
	isidbg();
	if (isptr)
		isptr->s_type = 0;
	if (db != NULL) {
		db->db_idbgbase = 0;		/* start of idbg table */
		db->db_brkpttbl = 0;		/* not yet */
		db->db_printf = 0;		/* symmon printf address */
	}
}

/*
 * SIGINT handler to allow kp commands to be interrupted.
 */
void
idbg_interrupted(void)
{
	db_t *db = (db_t *)GDEBUGBLOCK;

	symmon_spl();
	copyregs(private.sregs, private.regs);
	db->db_printf = 0; 
	private.pda_nofault = 0;
	printf("Interrupted\n");
	_restart();
}


/*
 * do_it - main routine
 *	convert	- if 1 then convert first arg from string to #
 * we assume that argv[0] is the name of the command
 */
static int
do_it(int argc, char *argv[], dbgoff_t *sp, int convert)
{
	numinp_t addr;
	long laddr;
	char *cp;
	jmp_buf pi_buf;
	db_t *db = (db_t *)GDEBUGBLOCK;
	extern int kpprintf();
	int (*oldkp)();

	if (!isidbg()) 
		return(1);
	if (argc == 0)
		return 1;
	oldkp = db->db_printf;
	/* skip over function name */
	argc--;
	argv++;
	if (argc == 0) {
		/* old behavior - pass -1 by default */
		laddr = -1L;
	} else if (convert) {
		cp = atob_s(*argv, &addr);
		/* only illegal if a partial number */
		if (cp != *argv) {
			/* might have converted something */
			if (cp && *cp) {
				printf("illegal addr: %s\n", *argv);
				return 1;
			}
			/* truncate address to register size */
			laddr = addr;
		} else {
			/* default first arg to argv[1] - this makes some old
			 * idbg commands that assumed a non-numeric first
			 * arg (nschain)
			 */
			laddr = (long)*argv;
		}
	} else
		laddr = (long)*argv;

	if (setjmp(pi_buf)) {
		symmon_spl();
		bad_addr();
		copyregs(private.sregs, private.regs);
		db->db_printf = oldkp;
		private.pda_nofault = 0;
		Signal(SIGINT, main_intr);
		return 1;
	}
	Signal(SIGINT, idbg_interrupted);
	db->db_printf = kpprintf;
	private.pda_nofault = pi_buf;
	copyregs(private.regs, private.sregs);

	_do_it(laddr, sp->s_func, argc, argv);

	db->db_printf = oldkp;
	copyregs(private.sregs, private.regs);
	private.pda_nofault = 0;
	Signal(SIGINT, main_intr);
	return(0);
}

/*
 * do_it2 - call a kernel function with 2 args 
 */
int
do_it2(char *func, __psint_t arg1, __psint_t arg2)
{
	dbgoff_t *sp;
	char *argv[4];

	if (!isidbg()) 
		return(1);
	if (ffunc(func, isptr, &sp) != 1)
		return(1);

	argv[0] = sp->ks_name;
	argv[1] = (char *)arg1;
	argv[2] = (char *)arg2;
	argv[3] = 0;

	return do_it(3, argv, sp, 0);
}

/*
 * _kp - general purpose interface for kernel routines
 * first arg should be a string, it is looked up in the function table
 * and all other args are passed to the function
 */
/*ARGSUSED*/
int
_kp(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	dbgoff_t *sp;
	register int nmatch;

	if (!isidbg()) 
		return(1);
	if (argc <= 1) {
		ffunc(0, isptr, 0);
		printf("ffunc returned 0x%x\n", isptr);
		return(0);
	}
	nmatch = ffunc(argv[1], isptr, &sp);
	if (nmatch == 0) {
		printf("Function:<%s> not found\n", argv[1]);
		return(1);
	} else if (nmatch > 1) {
		printf("Function:<%s> not unique\n", argv[1]);
		return(1);
	}
	argc--;
	argv++;

	/* need some special 'processing' for ubt - uggly */
	if (strcmp("ubt", sp->ks_name) == 0) {
		return(_ubt(sp, argc, argv));
	}
	/* special processing for "stop" command to avoid sending extra DEBUG
	 * to processors which are already stopped.
	 */
	if (strcmp("stop", sp->ks_name) == 0) {
		return(bp_try_stop_cpus());
	}
	return(do_it(argc, argv, sp, 1));
}

/*
 * _kpcheck - check if command is a kp command and optionally execute it
 */
int
_kpcheck(int argc, char *argv[], int execute)
{
	dbgoff_t *sp;
	register int nmatch;

	if (!isidbg()) 
		return(1);
	nmatch = ffunc(argv[0], isptr, &sp);
	if (nmatch == 0 || nmatch > 1)
		return 1;

	if (! execute)
	    return 0;

	/* need some special 'processing' for ubt - uggly */
	if (strcmp("ubt", sp->ks_name) == 0) {
		return(_ubt(sp, argc, argv));
	}
	/* special processing for "stop" command to avoid sending extra DEBUG
	 * to processors which are already stopped.
	 */
	if (strcmp("stop", sp->ks_name) == 0) {
		return(bp_try_stop_cpus());
	}
	(void)do_it(argc, argv, sp, 1);
	return 0;
}


/* 
 * This function will return the number of entries for the given command.
 */

int
_is_kp_func(char *cmd) {
	dbgoff_t *sp;

	if (!isidbg() || (cmd == NULL)) 
		return 0;

	return ffunc(cmd, isptr, &sp);
}


/*
 * bp_try_stop_cpus
 * 	Attempts to stop all other cpus when we hit a breakpoint by issuing
 *	a "kp stop" command, which lets the kernel issue a debug interrupt.
 *
 *	NOTE: We could make this a little faster by performing the "ffunc"
 *	      lookup once, then saving the value for the next time (or even
 *	      initializing it when we set the breakpoint).
 */
int
bp_try_stop_cpus()
{
#ifdef MULTIPROCESSOR
	dbgoff_t *sp;
	register int nmatch;
	char *aargv[3];
	char *stopcmd="stop";
	cpumask_t stoplist;
	int i;
	extern cpumask_t waiting_cpus;

	if (!isidbg()) 
		return(1);
	nmatch = ffunc(stopcmd, isptr, &sp);
	if (nmatch == 0 || nmatch > 1)
		return 1;

	/* Only need to stop cpu's which aren't already waiting.  If we do
	 * stop them, they will have a pending "debug interrupt" waiting
	 * when they are finally "continued" and they will not really
	 * restart.
	 * NOTE: There is still a timing window, we're just making it
	 * sufficiently small that it's not an issue most of the time.
	 */
	CPUMASK_CLRALL( stoplist );
	for (i=0; i<MAXCPU; i++)
		if ((i != cpuid()) && (!CPUMASK_TSTB( waiting_cpus, i)))
			CPUMASK_SETB(stoplist, i);

	aargv[0] = stopcmd;
	aargv[1] = (char *) &stoplist;
	aargv[2] = 0;
	return(do_it(2, aargv, sp, 0));
#else
	return 0;
#endif /* MULTIPROCESSOR */
}

/*
 * ffunc - find function given string
 * Returns - # of matches
 *	-1 if fname was NULL (prints out all available functions)
 */
static int
ffunc(char *fname, dbgoff_t *table, dbgoff_t **fp)
{
	register dbgoff_t *p;
	register dbgoff_t *partialp;
	register dbgoff_t *exactp;
	register dbglist_t *dl;
	register int nchars, nmatch, exact;

	/* Skip the DO_ENV record */
	table++;
	if (fname == 0) {
		p = table;
		printf("Available Functions:\n\t");
		while (p->s_type != DO_END) {
			printf("%s ", p->ks_name);
			p++;
		}

		for (dl = p->s_head->next; dl; dl = dl->next) {
#ifdef IP32
			if (IS_KSEG2(dl) && probe_tlb((caddr_t)dl, 0) < 0)
				map_k2seg_addr((void *)dl);
#endif
			printf("%s ", dl->dp.ks_name);
		}
		printf("\n");
		return(-1);
	}

	if (strlen(fname) == 0)
		return 0;
	/* search for function - ignore bogus 0 length names */
	nchars = strlen(fname);
	if (nchars > sizeof(p->us_name))
		nchars = sizeof(p->us_name);
	exact = 0;
	nmatch = 0;
	/*
	 * Always return the first exact or partial found
	 * (preferring exact, of course).
	 */
	exactp = NULL;
	partialp = NULL;
	for (p = table; p->s_type != DO_END; p++) {
		if (p->ks_name[0] == '\0')
			continue;
		/* check first for exact match */
		if (strncmp(fname, p->ks_name, sizeof(p->us_name)) == 0) {
			if (exactp == NULL)
				exactp = p;
			exact++;
		}
		/* check for partial match */
		if (strncmp(fname, p->ks_name, nchars) == 0) {
			if (partialp == NULL)
				partialp = p;
			nmatch++;
		}
	}
#ifdef IP32
	if (nmatch == 0) {
#endif
	for (dl = p->s_head->next; dl; dl = dl->next) {
#ifdef IP32
		if (IS_KSEG2(dl) && probe_tlb((caddr_t)dl, 0) < 0)
			map_k2seg_addr((void *)dl);
#endif
		if (dl->dp.ks_name[0] == '\0')
			continue;
		/* check first for exact match */
		if (strncmp(fname, dl->dp.ks_name, sizeof(p->us_name)) == 0) {
			if (exactp == NULL)
				exactp = &(dl->dp);
			exact++;
		}
		/* check for partial match */
		if (strncmp(fname, dl->dp.ks_name, nchars) == 0) {
			if (partialp == NULL)
				partialp = &(dl->dp);
			nmatch++;
		}
	}
#ifdef IP32
	}
#endif
	if (exact) {
		*fp = exactp;
	} else {
		*fp = partialp;
	}
	return exact ? exact : nmatch;
}

/*
 * in order to properly do a current proc backtrace we need to
 * pass to kernel the stopped regs from dbgmon
 * so we set up an area here and pass that address!
 * we also allow user typing in a sp/pc pair
 */
static int
_ubt(dbgoff_t *sp, int argc, char *argv[])
{
	__psunsigned_t stopreg[4];	
	numinp_t tmp;
	jmp_buf pi_buf;
	extern struct brkpt_table brkpt_table[];
	db_t *db = (db_t *)GDEBUGBLOCK;
	extern int kpprintf();
	int (*oldkp)();
	char *aargv[2];

	if (!isidbg()) 
		return(1);
	oldkp = db->db_printf;
	if (argc == 1) {
		/* do current stopped process */
		stopreg[0] = (__psint_t)private.regs[R_EPC];
		stopreg[1] = (__psint_t)private.regs[R_SP];
		stopreg[2] = (__psint_t)private.regs[R_RA];
		stopreg[3] = (__psunsigned_t)brkpt_table;
	} else if (argc == 2)  {
		/* plain old proc pointer/index version */
		if (argv[1][0] == '?')
			goto usage;
		stopreg[0] = -1;
		atob_s(argv[1], &tmp);		/* proc ptr/index */
		stopreg[1] = tmp;
		stopreg[2] = 0;
		stopreg[3] = (__psunsigned_t)brkpt_table;

	} else if (argc == 3) {
		atob_s(argv[1], &tmp);
		stopreg[0] = tmp;
		atob_s(argv[2], &tmp);
		stopreg[1] = tmp;
		stopreg[2] = 0;
		stopreg[3] = (__psunsigned_t)brkpt_table;
	} else {
usage:
		printf("Stack backtrace options:\n");
		printf("\tkp ubt - current stopped processes\n");
		printf("\tkp ubt # - rsav backtrace for proc #\n");
		printf("\tkp ubt pc sp - try trace using given info\n");
		return(1);
	}
	aargv[0] = (char *)&stopreg[0];
	aargv[1] = 0;

	if (setjmp(pi_buf)) {
		db->db_printf = oldkp;
		symmon_spl();
		bad_addr();
		copyregs(private.sregs, private.regs);
		private.pda_nofault = 0;
		return(0);
	}
	Signal(SIGINT, idbg_interrupted);
	private.pda_nofault = pi_buf;
	copyregs(private.regs, private.sregs);
	db->db_printf = kpprintf;
	_do_it((__psint_t)&stopreg[0], sp->s_func, 1, aargv);
	db->db_printf = oldkp;
	copyregs(private.sregs, private.regs);
	private.pda_nofault = 0;
	Signal(SIGINT, main_intr);
	return(0);
}

/*
 * trykfix - see if kernel can fix a tlbmiss thus fixing the cause of
 * the bad access, if so then continue execution
 * Return ONLY if it couldn't fix the problem
 */
int
trykfix(void)
{
	k_machreg_t sv_gp,sv_ra;	/* allows nesting _do_its */
	dbgoff_t *sp;
	k_machreg_t vaddr;
	unsigned long cause = private.exc_stack[E_CAUSE] & CAUSE_EXCMASK;
	char *argv[2];

	if (!isidbg()) 
		return(1);
	if (ffunc("ktlbfix", isptr, &sp) != 1)
		return(1);
        if (cause != EXC_MOD && cause != EXC_RMISS && cause != EXC_WMISS)
		return(2);
	vaddr = private.exc_stack[E_BADVADDR];
	sv_gp = private._sv_gp;
	sv_ra = private._sv_ra;
	argv[0] = (char *)vaddr;
	argv[1] = 0;
	if (_do_it((__psint_t)vaddr, sp->s_func, 1, argv)) {
		/* all set - lets resume */
		private._sv_gp = sv_gp;
		private._sv_ra = sv_ra;
		_resume_brkpt();
		/* NOTREACHED */
	}
	private._sv_gp = sv_gp;
	private._sv_ra = sv_ra;
	/* shucks it didn't work */
	printf("Couldn't recover from  bad address:%x\n", vaddr);
	return(1);
}

/*
 * copyregs - copy the saved register state
 */
static void
copyregs(k_machreg_t *from, k_machreg_t *to)
{
	register int i;
	for (i = 0; i < NREGS; i++)
		*to++ = *from++;
}

int
symtab(void)
{
	int size;
	struct dbstbl *symtab;
	char *nam;
	dbgoff_t *sp;
	db_t *db = (db_t *)GDEBUGBLOCK;

	if (!isidbg()) {
		/* 
		 * If we're not debugging kernel, find out if sash
		 * or someone else set up a symbol table.
		 */
		if (db != NULL &&
		    db->db_symtab && db->db_nametab) {
			dbstab = (struct dbstbl *)db->db_symtab;
			nametab = (char *)db->db_nametab;
			symmax = db->db_symmax;
		} else 
			return(1);
	} else {
		if (ffunc("symtab", isptr, &sp) != 1) 
			return(1);
		(*sp->s_func)(&symtab, &nam);
		dbstab = symtab;
		nametab = nam;
		if (ffunc("symsize", isptr, &sp) != 1) 
			return(1);
		(*sp->s_func)(&size,0);
		symmax = size;
	}
	lowest_text = (inst_t *)(dbstab[0].addr);
	return(0);
}


/*
 * map_k2seg_addr - see if kernel can map a K2 seg address
 */
void
map_k2seg_addr(void *vaddr)
{
	dbgoff_t *sp;
	char *argv[2];

	isidbg();
	if (ffunc("ktlbfix", isptr, &sp) != 1)
		_dbg_error("Couldn't fix tlb for K2 seg address: 0x%x", vaddr);
	argv[0] = vaddr;
	argv[1] = 0;
	if (!_do_it((__psint_t)vaddr, sp->s_func, 1, argv))
		_dbg_error("Couldn't recover from bad address:%x\n", vaddr);
}



#ifdef DEBUG
#define dprintf(x)  printf x
#else
#define dprintf(x)
#endif
#ifdef NETDBX

unsigned long
upcall(char *fname, char *arg)
{
	k_machreg_t sv_gp,sv_ra;	/* allows nesting _do_its */
	auto dbgoff_t *sp;
	unsigned long result;
	char *aargv[3];

	if (!isidbg()) {
		printf("upcall: <%s> no idbg to call\n", fname);
		return(-1);
	}
	if (ffunc(fname, isptr, &sp) != 1) {
		printf("upcall: no hook for <%s>\n", fname);
		return(-1);
	}
	/* XXX make this more general - can we either always
	 * call do_it or always call _do_it??
	 */
	if (arg) {
		char *stopcmd="stop";

		aargv[0] = stopcmd;
		aargv[1] = arg; 	/* stoplist */
		aargv[2] = 0;
		return(do_it(2, aargv, sp, 0));
	}
	sv_gp = private._sv_gp;
	sv_ra = private._sv_ra;
	aargv[0] = arg;
	aargv[1] = 0;
	if ((result = _do_it(0, sp->s_func, 1, aargv))) {
		/* all set - lets resume */
		private._sv_gp = sv_gp;
		private._sv_ra = sv_ra;
		return(result);
	}
	private._sv_gp = sv_gp;
	private._sv_ra = sv_ra;
	/* shucks it didn't work */
	dprintf(("(cpu %d), upcall <%s> failed\n", cpuid(), fname));
	return(-1);
}
#endif /* NETDBX */

/*
 * Capture cellid of cpu/thread whenever the user interface is taken;
 * Reset the cellid whenever the user interface is released
 */

static volatile int talking_cellid = -1;

void
begin_cell()
{
	dbgoff_t *sp;
	int	 cellid;

	if (isidbg() && (talking_cellid == -1)) {
		if (ffunc("__getcellid", isptr, &sp) == 1) {
			dprintf (("cpu %d begin cell A %d \n", cpuid(), cellid));
			_do_it((__psint_t)&cellid, sp->s_func, 1, NULL);
			talking_cellid = cellid;
			dprintf (("cpu %d begin cell B %d \n", cpuid(), cellid));
		}
	}
}

void
end_cell()
{
	dbgoff_t *sp;
	int	cellid;

	if (isidbg() && (talking_cellid > -1)) {
		if (ffunc("__setcellid", isptr, &sp) == 1) {
			cellid = talking_cellid;
			dprintf (("cpu %d end cell A %d \n", cpuid(), cellid));
			talking_cellid = -2;
			_do_it((__psint_t)&cellid, sp->s_func, 1, NULL);
			talking_cellid = -1;
			dprintf (("cpu %d end cell B %d \n", cpuid(), cellid));
		}
	}
}
