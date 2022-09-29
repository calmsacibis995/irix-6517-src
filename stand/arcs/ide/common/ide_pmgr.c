

#ident "$Revision: 1.32 $"

/*
 * arcs/ide/ide_pmgr.c: routines used by the ide_master to launch 
 * and control processes, on both remote and local processors
 */

#include <arcs/types.h>
#include <arcs/signal.h>
#include <arcs/io.h>

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/immu.h>
#ifdef EVEREST
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everest.h>
#endif
#include <genpda.h>
#include <ide_msg.h>

#include <uif.h>
#include "ide.h"
#include "ide_sets.h"
#include "ide_mp.h"

#include <libsk.h>
#include <libsc.h>


extern int	_do_local(int);
extern int	_do_remote(uint, uint);

#if MULTIPROCESSOR
static void	ide_slave(void);
static int	ide_slave_wait(void);
extern void	ide_slave_boot(void);
#endif /* MULTIPROCESSOR */

#if EVEREST
extern int	_flash_cc_leds(int,int);
extern int	_toggle_cc_leds(int,int);
extern int	_set_cc_leds(int);
#endif

/*
 * register-format descriptions
 */
extern struct reg_desc sr_desc[], cause_desc[], config_desc[];


char *cmdstrs[] =	{ SBADCMD_STR, SWAKE_STR, SCALL_STR, 
			  SGOHOME_STR, SSWTCH_STR };

char *statstrs[] =	{ "?", "pending", "done", "INTERRUPTED", 
			  "SLAVE_ERROR" };

int global_toldem=0;


/* 
 * _do_launch() invokes _do_local() directly for all launches occurring
 * on the ide_master's processor; since these are single-threaded
 * function calls no additional synchronization is necessary. 
 * However, when ide_slave() invokes _do_local() it isn't executing on
 * the master's processor, it must therefore coordinate with _do_remote(),
 * which handles the master's launching responsibilities during remote
 * launches.
 */

extern int doing_exec;

/*
 * async version of do_launch, used to allow slave CPUs to run tests in
 * parallel.  Almost identical to do_launch, but does not run on the
 * master CPU, and, after waking up the slave CPU, leaves it free running
 * This is a VERY dangerous function - alter with great caution.
 *
 * extern int _do_slave_asynch(uint, volvp, argdesc_t *);
 */

#define	REMOTE_SYNCH	0
#define REMOTE_ASYNCH	1

/* maximum wait on slave internal commands */
#define	LAUNCH_TOUT	20

/* 
 * if launch vid==master's, the launch turns into a procedure call to
 * do_local.  Otherwise, the master calls do_remote, which wakes up the
 * slave on the remote cpu and the slave does a procedure call of do_local.
 * In the latter case, however, the master and slave must syncronize at
 * key points of the execution, meaning that do_local--always executed 
 * on the 'launch' cpu--must be told whether to do the handshaking or
 * not.  MUST_SYNCRONIZE is passed into do_local as a parameter.
 */
#define MUST_SYNCHRONIZE	(GPME(my_virtid) != _ide_info.master_vid)


#define CMD_STATUS(_SLAVE,_CMD,_STATUS)				\
	_CMD = IDE_PRIVATE(_SLAVE,slave_cmd);			\
	_STATUS = IDE_PRIVATE(_SLAVE,doit_done);

#define SPRINTF_CS(_BP,_SLAVE)					\
    {								\
	voll_t __cmd = IDE_PRIVATE(_SLAVE,slave_cmd);	\
	voll_t _status = IDE_PRIVATE(_SLAVE,doit_done);\
	sprintf(_BP,"cmd 0x%x==`%s': %s", __cmd,		\
	    (BADSCMD(__cmd) ? cmdstrs[0] : cmdstrs[__cmd]),	\
	    (BADSSTAT(_status)?statstrs[0]:statstrs[_status]));	\
    }

#define ISSUE_CMD(_SLAVE,_CMD)					\
	IDE_PRIVATE(_SLAVE,slave_cmd) = _CMD;			\
	IDE_PRIVATE(_SLAVE,doit_done) = STAT_SDOIT;

#define AWAIT_DONE(_SLAVE)					\
    {								\
	volatile uint count=0;					\
								\
	while (IDE_PRIVATE(_SLAVE,doit_done) != STAT_SDONE) {	\
	    us_delay(10000);					\
								\
	    if ((IDE_PRIVATE(_SLAVE,doit_done)==STAT_SINTER) ||	\
		(IDE_PRIVATE(_SLAVE,doit_done)==STAT_SERROR))	\
			break;					\
	    if ((GPME(my_virtid)==0)&&(++count & 0xfff)==0xfff) \
		msg_printf(DBG,					\
		    "\rmaster(%d) AWAITING v%d...\n",		\
		    GPME(my_virtid),_SLAVE);			\
	}							\
    }

/*
 * added for the parallel execution mode, though might be nice
 * for serial execution mode as well - times out after waiting
 * MAXTIME seconds, prints an error message, and returns
 * either 0: no timeout or 1: timed out
 */
#define T_AWAIT_DONE(_SLAVE,MAXTIME,TOUT)			\
    {								\
	volatile uint count=0;					\
	volatile unsigned long start_time;			\
								\
	start_time = get_tod();					\
	TOUT = 0;						\
								\
	while (IDE_PRIVATE(_SLAVE,doit_done) !=	 STAT_SDONE) {	\
	    us_delay(10000);					\
								\
	    if ((IDE_PRIVATE(_SLAVE,doit_done)==STAT_SINTER) ||	\
		(IDE_PRIVATE(_SLAVE,doit_done)==STAT_SERROR))	\
			break;					\
	    if ((GPME(my_virtid)==0)&&(++count & 0xfff)==0xfff) \
		msg_printf(DBG,					\
		    "\rmaster(%d) AWAITING v%d...\n",		\
		    GPME(my_virtid),_SLAVE);			\
	    if ((get_tod() - start_time) >= MAXTIME) {		\
		msg_printf(ERR,					\
	"\rmaster(%d) timed out on vid %d after %d seconds\n",	\
	GPME(my_virtid),_SLAVE,(get_tod()-start_time));		\
		TOUT = 1;					\
		break;						\
	    }							\
	}							\
    }
	
#define IS_DONE(_SLAVE)						\
	( (IDE_PRIVATE(_SLAVE,doit_done)==STAT_SINTER) ||	\
	  (IDE_PRIVATE(_SLAVE,doit_done)==STAT_SERROR) ||	\
	  (IDE_PRIVATE(_SLAVE,doit_done)==STAT_SDONE) ) 
	
#define AWAIT_CMD(_SLAVE,_CMD)					\
	while (IDE_PRIVATE(_SLAVE,doit_done) != STAT_SDOIT)	\
	    us_delay(10000);					\
	_CMD = IDE_PRIVATE(_SLAVE,slave_cmd);

#define ACK_DONE(_SLAVE)					\
	IDE_PRIVATE(_SLAVE,doit_done) = STAT_SDONE;

#define ACK_INTERRUPT(_SLAVE)					\
	IDE_PRIVATE(_SLAVE,doit_done) = STAT_SINTER;


#if EVEREST
static char _d_name[] = IDE_PMGR;
#endif

/* ide_dispatch options:
 *
 * flags to override ide_env settings: 
 * -q Quick_Mode:	Diagnostics run in abbreviated mode
 * -c Cont_After_Err:	Diag continues iterative execution in spite of errors
 * -i Run_Diags_ICached:run diags in k0seg instead of (linked) k1seg addr
 * Run_Prims_ICached:	ide support routines (called by diags) run icached
 * ECC_Enabled:
 * Init_HW_State:
 * 
 * options requesting special execution features or alter numerical values:
 * -t <ms timeout>	allow this interval before concluding diag timed-out.
 * -s <RunSet,ReturnedSet,ResultSet>
 *	sequential execution of diagnostic across a set of cpus.
 * -p <RunSet,ReturnedSet,ResultSet>
 *	parallel execution of diagnostic across a set of cpus.
 */
int
ide_dispatch(__psint_t (*func)(int, void **), int argc, char **argv)
{
    char sbuf[SETBUFSIZ];
    setmask_t mask;
    set_t runset=0, hungset=0, errorset=0;
    char *strptrs[MAXARGC+1];
    argdesc_t fnargs;
    optlist_t optlist;
    option_t *setoptp;
    setlist_t slist;
    sym_t *symptr = NULL;
    int setcnt=0, rc, launch_vid;
    int vid_me = cpuid();
    int itsadiag=0, local_toldem=0;
    volatile __psunsigned_t addr;
    int do_setx=0, spx=0;
    char *optptr = &(optlist.opts[0]);
    int sindex=-1;
    char opt;
    int logidx, logit = error_logging();
#ifdef ARG_DEBUG
    int mlevel;
#endif
#ifdef MULTIPROCESSOR
    char *test_result;				/* set to PASS, FAIL, SKIP */
    int do_para = 0;
#endif

    if (Reportlevel & DISPDBG) {
	msg_printf(VDBG, "\n    dispatcher argvs:  ");
	_dump_argvs(argc, argv);
	msg_printf(VDBG, "\n");
    }

    if (!(symptr = findsym(argv[0]))) {
	msg_printf(IDE_ERR, "ide_dispatch: symbol `%s' not found\n",
	    argv[0]);
	ide_panic("ide_dispatch");
    }

    /* 
     * if argv[1] == "--", diag needs special stuff.  _get_disopts checks
     * and strips leading args until a terminating "--" is found.  Upon
     * successful return (== 0) the duplicated <argc,argv> combo is as the
     * called diag expects, and the optlist struct contains the set of
     * special options the user desires.  
     */
    if ((argc > 1) && argv[1] && !strcmp(argv[1], "--")) {
	int loop, res;

	/*
	 * keep original array of stringptrs to allow caller to free them
	 * upon return.  All arg-swapping will occur in a duplicate array.
	 * (the strings themselves are not modified).
	 */
	ASSERT(argc <= MAXARGC);
	for (loop = 0; loop < argc; loop++)
	    strptrs[loop] = argv[loop];
	strptrs[loop] = NULL;
	fnargs.argc = argc;
	fnargs.argv = &strptrs[0];
	if (res = _get_disopts(&fnargs, &optlist)) 
	    return(res);	/* _get_disopts displayed error msg */

#ifdef ARG_DEBUG
	mlevel = (VDBG|OPTDBG|DISPDBG);
/*	if (Reportlevel & mlevel)*/
	if (Reportlevel >= VDBG ) {
	    msg_printf(JUST_DOIT, "    %d dispatcher option%s:\n      ", 
		    optlist.optcount, (optlist.optcount == 1 ? "" : "s"));
	    _dump_optlist(&optlist);
	    msg_printf(JUST_DOIT, "\n");
	}
#endif

	while (opt = *optptr) {
 	    switch(opt) {

	    case 's':	/* set execution */
		sindex = _opt_index(opt, optlist.opts);
		setoptp = (option_t *)&optlist.options[sindex];
		res = _check_sets(setoptp, &slist, 0);
		/*
		 * if anything went wrong in the set check, or if 
		 * we don't have 3 good sets, bail
		 * WITH LOGGING WE DON'T NEED 3 SETS ANY MORE, JUST THE
		 * RUNSET.
		 */
		if (res) {
		    msg_printf(USER_ERR, "%s: bad set args (error %d)\n",
			argv[0], res);
		    return(res);
		} 
		if (slist.setcnt != 1 && slist.setcnt != 3) {
		    msg_printf(USER_ERR, "set execution requires a runset;\n");
		    msg_printf(USER_ERR,
			"      'hungset' & 'errorset' are optional\n");
		    msg_printf(USER_ERR,
			"        (_check_sets returned %d, setcnt %d)\n",
			res, slist.setcnt);
		    return(BADSETARGS);
		} else {
		    runset = slist.setptrs[RUN_SET]->sym_set;
		    setcnt = slist.setcnt;
		    sprintf_cset(sbuf, runset);
		    msg_printf(DBG, "    dispatcher, %d set%s: runset %s\n",
			setcnt, _PLURAL(setcnt), sbuf);
		    /* 
		     * now that we have a good set, we can plan on doing 
		     * set execution
		     */
		    do_setx = 1;
		    spx++;
		}

#ifdef NOTDEF
		if ((Reportlevel & DISPDBG) || (Reportlevel >= DBG)) {
		    msg_printf(DBG, "%d set%s verified:%s", setcnt,
			_PLURAL(setcnt), (Reportlevel>DBG?"\n":" "));
		    for (i = 0; i < setcnt; i++) {
			if (Reportlevel > DBG)
			    _display_set(slist.setptrs[i], D_SILENT);
			else
			    msg_printf(DBG, "%s %s",slist.setptrs[i]->sym_name,
				(i ? "," : " "));
		    }
		    msg_printf(INFO, "\n");
		}
#endif

		if ((Reportlevel & DISPDBG) || (Reportlevel >= INFO)) {
		    sprintf_cset(sbuf, slist.setptrs[RUN_SET]->sym_set);
		    msg_printf(DBG, "runset `%s': %s\n",
			slist.setptrs[RUN_SET]->sym_name, sbuf);
		}
		break;

	    case 'p':
#ifdef MULTIPROCESSOR
		msg_printf(DBG, "Enabling Parallel Execution\n");
		do_para = 1;
#endif
		break;

	    case 'i':
		if ((Reportlevel & DISPDBG) || (Reportlevel > DBG)) {
		    msg_printf(JUST_DOIT, " + icached +\n");
		}
		(void)_opt_index(opt, optlist.opts);
		spx++;
		addr = (long)func;

		if (IS_KSEG0(addr)) {
		    msg_printf(JUST_DOIT,
			"NOTE: -i flag ignored: fn pc already in K0SEG!\n");
		} else
		if (IS_KSEG2(addr)) {
		    msg_printf(JUST_DOIT,
			"NOTE: fn pc is in K2SEG!...Ignoring -i flag\n");
		} else
		if (IS_KUSEG(addr)) {
		    msg_printf(JUST_DOIT,
			"WARNING: fn pc was in KUSEG!  Launching icached\n");
		    addr = PHYS_TO_K0(addr);
		} else {
		    msg_printf(DBG, " -run icached-\n");
		    addr = K1_TO_K0(addr);
		}

		func = (__psint_t (*)(int, void **))addr;
		break;

	    default:
		msg_printf(IDE_ERR, "ide_dispatch: bad opt `%c'\n",opt);
		ide_panic("ide_dispatch");
		/*NOTREACHED*/
	    }
	    optptr++;
	}

    } else {	/* no spx help needed by diag */
	fnargs.argv = argv;
	fnargs.argc = argc;
    }

    hungset = errorset = 0;

    /*
     * at this point fnargs.arg{v,c} are correctly initialized 
     * regardless of whether spx flags were used or not.  Handle
     * single-processor execution identically to a set execution 
     * spread across just one processor.
     */ 

    if (!do_setx) {
	msg_printf(VDBG,"  fake setex: set vid %d's bit\n",GPME(my_virtid));
	runset = (SET_ZEROBIT >> GPME(my_virtid));
	if (Reportlevel >= DBG+3) {
	    sprintf_cset(sbuf, runset);
	    msg_printf(JUST_DOIT, "     dispatch, fake runset: %s\n", sbuf);
	}
    }

    if (spx && ((Reportlevel & DISPDBG) || Reportlevel >= VDBG)) {
	msg_printf(JUST_DOIT, "\n    dispatch:  fn args:  ");
	_dump_argvs(fnargs.argc, fnargs.argv);
	msg_printf(JUST_DOIT, "\n");
    }
    
#if EVEREST
    /*
     * if we're Everest and launching a diagnostic or want lots of gab, 
     * display the function's name and result of the execution.  IP20 and
     * IP22 diags handle this themselves.
     */
    sprintf_cset(sbuf, runset);
    if (ID_DIAG(symptr->sym_flags)) {
	msg_printf(JUST_DOIT, "  %s",symptr->sym_name);
	if (do_setx) {
	    if (Reportlevel >= DBG) {
		msg_printf(JUST_DOIT, ", runset: %s\n", sbuf);
	    } else
		msg_printf(JUST_DOIT, ":\n");
	} else {
	    msg_printf(JUST_DOIT, "(vid %d): ", GPME(my_virtid));
#ifdef ARG_DEBUG
	    if (Reportlevel >= VDBG) {
		msg_printf(JUST_DOIT, "\n      symbol `%s':  ", argv[0]);
		_dumpsym(symptr);
		msg_printf(JUST_DOIT,"\n");
	    }
#endif /* ARG_DEBUG */
	}
    }
#endif /* EVEREST */

    mask = SET_ZEROBIT;
    if (DIAG_EXEC(symptr->sym_flags)) {
	itsadiag = 1;
	logidx = symptr->sym_logidx;
    } else {
	itsadiag = 0;
	logidx = -1;
    }
    msg_printf(DBG, "    \"%s\" is %s: idx #%d\n", symptr->sym_name,
	(itsadiag ? "a diagnostic" : "not a diagnostic"), logidx);

#ifdef MULTIPROCESSOR

    /* parallel execution code */
    if (do_para) {

	/*
	 * run selected diagnostic in parallel execution mode
	 * only diagnostics may be parallelized - anything else will
	 * return an error
	 */
#if !IP30
	msg_printf (JUST_DOIT, "parallel execution mode\n");
#else
	msg_printf (VRB, "parallel execution mode\n");
#endif	/* !IP30 */

	if (!itsadiag) {
	    msg_printf(USER_ERR,
		"\"%s\" can not be run in parallel mode - only diags allowed\n",
		symptr->sym_name);
	    return (1);
	}

	/*
	 * launch the diagnostics on all CPUs in the test set
	 * there is a bit of a hack here - we run the master CPU last
	 * if it is one of members of the defined set
	 */
	for (launch_vid = 0; launch_vid < MAXSETSIZE; launch_vid++, mask>>=1)
	{
	    local_toldem += check_state(vid_me, local_toldem);

	    if (launch_vid == cpuid())
		continue;

	    if (mask & runset) {
		rc = _do_slave_asynch(launch_vid, (volvp)func, &fnargs);
		/* may want to add code to check if launch was OK */
		if (ID_DIAG(symptr->sym_flags) && do_setx)
		    msg_printf(JUST_DOIT, "   vid %d -\n", launch_vid);

	    }
	}

	/*
	 * run the test on the local CPU, if it's in the runset
	 */
	for (launch_vid=0,mask=SET_ZEROBIT; launch_vid<cpuid(); launch_vid++)
	    mask >>= 1;

	if (mask & runset) {
	    rc = _do_launch(launch_vid, (volvp)func, &fnargs);
	    if (ID_DIAG(symptr->sym_flags) && do_setx)
		msg_printf(JUST_DOIT, "   vid %d -\n", launch_vid);

	}
	else
	    rc = 0;

	msg_printf(DBG,"vids launched - synchronizing\n");

	/*
	 * now wait till all processors return from the launch
	 * as each one returns, record its pass/fail status and
	 * put it back in wait mode
	 * 
	 * the first pass at this is really crude - wait on each
	 * processor we KNOW ran, so that if one hangs they all
	 * need to wait for it
	 */
	mask = SET_ZEROBIT;
	for (launch_vid = 0; launch_vid < MAXSETSIZE; launch_vid++, mask>>=1)
	{
	    if (!(mask & runset))
		continue;
	    
	    if (launch_vid != cpuid()) {
		char bbuf[128];
		volatile uint cmd, status;

		/* just let T_AWAIT_DONE flag timeouts, for now */
		T_AWAIT_DONE(launch_vid,Slave_Timeout,status);

		/* do_xx stores fn's return value in its fn_return field */
		rc = IDE_PRIVATE(launch_vid,fn_return);

		CMD_STATUS(launch_vid,cmd,status);
		SPRINTF_CS(bbuf,launch_vid);

		if (status != STAT_SDONE) {
		    msg_printf(DBG,
			"  master: !!!slave(v%d)'s %s!!  retval %d\n",
			launch_vid, bbuf, rc);
		    if (status == STAT_SINTER) {
			ISSUE_CMD(launch_vid,SLAVE_GO_HOME);
			AWAIT_DONE(launch_vid);
			SPRINTF_CS(bbuf,launch_vid);
			msg_printf(DBG, "slave v%d ACKed `GOHOME' cmd, (%s)\n",
			    launch_vid, bbuf);
		    } else {
			sprintf(sbuf,
			    "_do_remote(v%d): bad status field - %d",
			    launch_vid, status);
			ide_panic(sbuf);
		    }
		}

		ISSUE_CMD(launch_vid,SLAVE_GO_HOME);
		AWAIT_DONE(launch_vid);
	    }
	    else {
		/* do_xx stores fn's return value in its fn_return field */
		rc = IDE_PRIVATE(launch_vid,fn_return);
	    }

	    if (rc && (rc != TEST_SKIPPED))
		errorset |= (SET_ZEROBIT >> launch_vid);

	    if (logit) {
		switch (rc)
		{
		    case TEST_PASSED:
			statlog[logidx].passcnt[launch_vid]++;
			statlog[logidx].pass_tot++;
			test_result = "PASSED";
			break;

			case TEST_SKIPPED:
			    statlog[logidx].skipcnt[launch_vid]++;
			    statlog[logidx].skip_tot++;
			    test_result = "SKIPPED";
			    break;

			/*
			 * any value other than above is failure
			 */
			default:
			    statlog[logidx].failcnt[launch_vid]++;
			    statlog[logidx].fail_tot++;
			    test_result = "FAILED";
		}
	    }
	    if (ID_DIAG(symptr->sym_flags)) {
		int spaces = 0;

		if (Reportlevel > ERR || (Reportlevel == ERR && rc))
		    spaces=1;
		msg_printf(JUST_DOIT, "vid %d %s%s\n", launch_vid,
		    (spaces ? "      " : "-- "), test_result);
	    }

	    local_toldem += check_state(vid_me, local_toldem);
	}

    }
    else
#endif /* MULTIPROCESSOR */
    for (launch_vid = 0; launch_vid < MAXSETSIZE; launch_vid++) {

	if (itsadiag)
	    local_toldem += check_state(vid_me, local_toldem);

	if (mask & runset) {
#if EVEREST
	    if (ID_DIAG(symptr->sym_flags) && do_setx)
		msg_printf(JUST_DOIT, "   vid %d -", launch_vid);
#endif
	    rc = _do_launch(launch_vid, (void *)func, &fnargs);

	    if (rc && (rc != TEST_SKIPPED))
		errorset |= (SET_ZEROBIT >> launch_vid);

	    if (itsadiag && logit) {
		switch (rc)
		{
		    case TEST_PASSED:
			statlog[logidx].passcnt[launch_vid]++;
			statlog[logidx].pass_tot++;
#ifdef MULTIPROCESSOR
			test_result = "PASSED";
#endif
			break;

		    case TEST_SKIPPED:
			statlog[logidx].skipcnt[launch_vid]++;
			statlog[logidx].skip_tot++;
#ifdef MULTIPROCESSOR
			test_result = "SKIPPED";
#endif
			break;

		    /*
		     * any value other than above is failure
		     */
		    default:
			statlog[logidx].failcnt[launch_vid]++;
			statlog[logidx].fail_tot++;
#ifdef MULTIPROCESSOR
			test_result = "FAILED";
#endif
		}
	    }
#if EVEREST
	    if (ID_DIAG(symptr->sym_flags)) {
		int spaces = 0;

		if (Reportlevel > ERR || (Reportlevel == ERR && rc))
		    spaces=1;
		msg_printf(JUST_DOIT, "%s%s\n", (spaces ? "      " : "-- "),
			test_result);
	    }
#endif
	}
#if EVEREST
	if (itsadiag)
	    local_toldem += check_state(vid_me, local_toldem);
#endif /* EVEREST */

	mask >>= 1;
    }

    if (do_setx) {
	if (hungset != 0 || errorset != 0)
	    rc = 1;
    }

#ifdef ARG_DEBUG
    msg_printf(VDBG, "    ide_dispatch: fn %s's final retval = %d\n",
	fnargs.argv[0], rc);
#endif

    return(rc);

} /* ide_dispatch */


#if EVEREST
int
check_state(int vid, int toldem)
{
    int vid_me = cpuid();
    uint SR, cause;
    int ip, altpend;
    int err=0;

    cause = get_cause();
    cause &= ~CAUSE_IP8;	/* ignore count/compare interrupts */

    if (!(cause & CAUSE_IPMASK))	/* no other interrupts are pending */
	return(0);

    if (altpend = cpu_intr_pending()) {
	msg_printf((DBG|NAUSEOUS), "check_state(v%d): ", vid_me);
	msg_printf((DBG|NAUSEOUS), "cpu_intr_pending returned 0x%x\n", altpend);
	err++;
    }
    altpend &= ~CAUSE_IP8;

    if ((cause & CAUSE_IPMASK) || altpend) {
	int pending = ((CAUSE_IPMASK & cause) >> CAUSE_IPSHIFT);
	int code = ((CAUSE_EXCMASK & cause) >> CAUSE_EXCSHIFT);

	SR = get_sr();
	msg_printf((DBG|NAUSEOUS),
		"WARNING: Interrupts pending: 0x%x code %d (cause 0x%x: %R)\n",
		pending, code, cause, cause, cause_desc);
	msg_printf((DBG|NAUSEOUS),
		"SR: %R\n\n  check_state: pending interrupts (0x%x): ",
		SR, sr_desc, pending);

	for (ip = 7; ip > 0; ip--) {
	    if (pending & (0x1 << ip))
		msg_printf((DBG|NAUSEOUS), "IP%d ", ip+1);
	}
	return(1);
    } else
	return(0);

} /* check_state */
#else	/* non-EVEREST version is a stub for now */
/*ARGSUSED*/
int
check_state(int vid, int toldem)
{
	return(0);

} /* check_state */
#endif /* EVEREST */


#if MULTIPROCESSOR
/* 
 * ide_slave() oversees the process control, communication, and
 * resources on its processor.
 */
#define SLAVE_DEBUG	1
static void
ide_slave()
{
    char bbuf[128];
    int my_vid = cpuid();
    uint cmd, status;
    int retval;

    CMD_STATUS(my_vid,cmd,status);
    msg_printf(DBG, "  +>ide_slave(v%d): initial cmd 0x%x, d/d 0x%x\n",
	my_vid, cmd, status);
    /*
     * The slaves' slave_cmd fields are initialized to SLAVE_GO_HOME, and
     * their doit_done fields set to STAT_SDOIT: STAT_SDOIT indicates an
     * outstanding command and can only be changed by slaves, and 
     * SLAVE_GO_HOME sends the slaves into ide_slave_wait().  The master
     * won't begin issuing commands to a slave until the slave sets 
     * doit_done to STAT_SDOIT.
     */
    for (;;) {

	CMD_STATUS(my_vid,cmd,status);
	SPRINTF_CS(bbuf, my_vid);
	msg_printf(DBG+1,"  +>ide_slave(v%d) pre-switch: %s\n", my_vid, bbuf);

	switch(IDE_ME(slave_cmd)) {

	  case SLAVE_GO_HOME:
	    cmd = ide_slave_wait();

	    SPRINTF_CS(bbuf, my_vid);
	    msg_printf(DBG, "  +>ide_slave(v%d): after wait, %s\n", 
		my_vid, bbuf);
	    break;

	  case SLAVE_CALL_FN:

	    SPRINTF_CS(bbuf, my_vid);
	    msg_printf(DBG, "  +>ide_slave(v%d): %s\n", my_vid, bbuf);

	    retval = _do_local(MUST_SYNCHRONIZE);

	    CMD_STATUS(my_vid,cmd,status);
	    SPRINTF_CS(bbuf, my_vid);
	    msg_printf(DBG, "  +>ide_slave(v%d) after CALL_FN: fn rc %d, %s\n",
		my_vid, retval, bbuf);
	    break;

	  case SLAVE_SWITCH_CPUS:
	    msg_printf(IDE_ERR,
		"ide_slave(v%d): switch cpus not implemented!\n", my_vid);
	    break;

	  default:
	    msg_printf(IDE_ERR,
		"ide_slave(v%d): invalid task (0x%x)!\n", my_vid, cmd);
	    break;
	}
    }
    /*NOTREACHED*/
    ide_panic("ide_slave out of 'for'!");

} /* ide_slave */
#endif /* MULTIPROCESSOR */



/*
 * _do_launch: called by ide_dispatch() and exec() to coordinate
 * the local or remote launching of a routine on virtual processor
 * 'launch_vid'.  Whether launching locally or remotely, _do_launch
 * copies the fn address and its single arg into the pda of the target
 * processor.  If a local launch, it then invokes the routine via a
 * procedure call; else it notifies the remote ide_slave (idling in
 * ide_wait()) that work is pending.  (This notification may be polled
 * or via interrupt).
 */
int
_do_launch(
    uint l_vid,			/* virtual processor on which to launch job */
    volvp func,			/* function to launch */
    argdesc_t *acvp)		/* ptr to <argc, argv> arg struct for fn */
{
    uint vid_me = cpuid();	/* fetch master's vid */ 
    uint retcode;
    int cacheit = icached_exec();
    volvp k0func;
#if defined(LAUNCH_DEBUG)
    uint phys_idx;
#endif

    /*
     * Range-check l_vid using the macro that verifies against
     * the currently active cpus (not just MAXSETSIZE)
     */
    if (VID_OUTARANGE((int)l_vid)) {
	msg_printf(USER_ERR, "(vid%d): invalid vid '%d' (0 <= vid <= %d)\n",
		vid_me, l_vid, _ide_info.vid_max);
	return(ERR_VP_RANGE);
    }

#if defined(LAUNCH_DEBUG)
#if EVEREST
    phys_idx = vid_to_phys_idx[l_vid];
    msg_printf(V4DBG, "(vid%d) -->%s (vid %d == phys_idx %d)\n", vid_me,
	(l_vid==vid_me ? "call function locally" : "launch job remotely on"),
	l_vid, phys_idx);
#else
    msg_printf(V4DBG, "(vid%d) -->%s vid %d\n", vid_me,
	(l_vid==vid_me ? "call function locally" : "launch job remotely on"),
	l_vid);
#endif /* EVEREST */
#endif /* LAUNCH_DEBUG */

    if (IDE_PRIVATE(l_vid,ide_pdamagic) != IDE_PDAMAGIC) {
	msg_printf(IDE_ERR,
	    "_do_launch(v%d): vid %d's pdamagic is 0x%x (need 0x%x)\n",
	    vid_me, l_vid, IDE_PRIVATE(l_vid,ide_pdamagic), IDE_PDAMAGIC);
	return(IEBAD_IDE_PDA);
    }

#if NO_ARG_COPY
    IDE_PRIVATE(l_vid,fn_argcv) = *acvp;
#else
	/*
	 * copy argv ptr array and the strings to which they point into
	 * argv_str struct pointed to by the argv_str field in l_vid's PDA
	 */
	msg_printf(VDBG+1, "    d-l: vid %d's pda 0x%x, argv_str 0x%x\n",
		l_vid, (__psunsigned_t)&(ide_pda_tab[l_vid]),
		(__psunsigned_t)IDE_PRIVATE(l_vid,argv_str));

/* 	ide_initargv(&acvp->argc, &acvp->argv, IDE_ME(argv_str)); */

 	ide_initargv(&(acvp->argc),&(acvp->argv), IDE_PRIVATE(l_vid,argv_str) );
	IDE_PRIVATE(l_vid,fn_argcv.argc) = IDE_PRIVATE(l_vid,argv_str->strcnt);
	IDE_PRIVATE(l_vid,fn_argcv.argv) = IDE_PRIVATE(l_vid,argv_str->strptrs);
#endif /* NO_ARG_COPY */

    
    if (cacheit) {
	k0func = (volvp)icache_it((void *)func);
	msg_printf(VRB, "    do_launch: func 0x%x, k0func 0x%x\n",
		(__psunsigned_t)func, (__psunsigned_t)k0func);
	func = k0func;
    }

    IDE_PRIVATE(l_vid,fn_addr) = func;


#if defined(LAUNCH_DEBUG)
    if (Reportlevel >= V3DBG) {
	msg_printf(JUST_DOIT, "  do_launch(v%d): fnaddr 0x%x, finalargs\n  -->",
		vid_me, IDE_PRIVATE(l_vid,fn_addr));
	_dump_argvs(IDE_PRIVATE(l_vid,fn_argcv.argc),
		IDE_PRIVATE(l_vid,fn_argcv.argv));
    }
#endif /* LAUNCH_DEBUG */

    if (l_vid == vid_me) {
	msg_printf(V3DBG,"  (v%d)-->target vid %d: do a LOCAL fncall\n",
		vid_me, l_vid);
	retcode = _do_local(MUST_SYNCHRONIZE);

    } else {
	msg_printf(V3DBG,"  (v%d)-->target vid %d: do a REMOTE fncall\n",
		vid_me, l_vid);
	retcode = _do_remote(l_vid, REMOTE_SYNCH);
    }

#if defined(LAUNCH_DEBUG)
    msg_printf(V3DBG,"  (v%d)-->_do_launch: fn returned %d\n", vid_me,retcode);
#endif /* LAUNCH_DEBUG */

    return(retcode);

} /* _do_launch */


/*
 * _do_slave_asynch: called by ide_dispatch() to coordinate the remote
 * launching of a routine on virtual processor * 'launch_vid'. It 
 * copies the fn address and its single arg into the pda of the target
 * processor.  
 * Notifies the remote ide_slave (idling in * ide_wait()) that work is
 * pending.  (This notification may be polled * or via interrupt).
 *
 * Please note - a "success" return code just means that the test function
 * was launched - ide_dispatch needs to wait for all launched slaves to
 * return before continueing
 */
int
_do_slave_asynch(
    uint l_vid,			/* virtual processor on which to launch job */
    volvp func,			/* function to launch */
    argdesc_t *acvp)		/* ptr to <argc, argv> arg struct for fn */
{
    uint vid_me = cpuid();	/* fetch master's vid */ 
    uint retcode;
    int cacheit = icached_exec();
    volvp k0func;

    /*
     * Range-check l_vid using the macro that verifies against
     * the currently active cpus (not just MAXSETSIZE)
     */
    if (VID_OUTARANGE((int)l_vid)) {
	msg_printf(USER_ERR, "(vid%d): invalid vid '%d' (0 <= vid <= %d)\n",
		vid_me, l_vid, _ide_info.vid_max);
	return(ERR_VP_RANGE);
    }

    /*
     * if local cpu is the selected target, just skip- this is only for
     * launching the slaves
     */

     if (vid_me == l_vid)
	return(TEST_SKIPPED);

#if defined(LAUNCH_DEBUG)
#if EVEREST
    msg_printf(V4DBG, "(vid%d) -->%s (vid %d == phys_idx %d)\n", vid_me,
	"launch job remotely on", l_vid, vid_to_phys_idx[l_vid]);
#else
    msg_printf(V4DBG, "(vid%d) -->%s vid %d\n", vid_me,
	"launch job remotely on", l_vid);
#endif /* EVEREST */
#endif /* LAUNCH_DEBUG */

    if (IDE_PRIVATE(l_vid,ide_pdamagic) != IDE_PDAMAGIC) {
	msg_printf(IDE_ERR,
	    "_do_slave_asynch(v%d): vid %d's pdamagic is 0x%x (need 0x%x)\n",
	    vid_me, l_vid, IDE_PRIVATE(l_vid,ide_pdamagic), IDE_PDAMAGIC);
	return(IEBAD_IDE_PDA);
    }

#if NO_ARG_COPY
    IDE_PRIVATE(l_vid,fn_argcv) = *acvp;
#else
	/*
	 * copy argv ptr array and the strings to which they point into
	 * argv_str struct pointed to by the argv_str field in l_vid's PDA
	 */
	msg_printf(VDBG+1, "    d-l: vid %d's pda 0x%x, argv_str 0x%x\n",
		l_vid, (__psunsigned_t)&(ide_pda_tab[l_vid]),
		(__psunsigned_t)IDE_PRIVATE(l_vid,argv_str));

/* 	ide_initargv(&acvp->argc, &acvp->argv, IDE_ME(argv_str)); */

 	ide_initargv(&(acvp->argc),&(acvp->argv), IDE_PRIVATE(l_vid,argv_str) );
	IDE_PRIVATE(l_vid,fn_argcv.argc) = IDE_PRIVATE(l_vid,argv_str->strcnt);
	IDE_PRIVATE(l_vid,fn_argcv.argv) = IDE_PRIVATE(l_vid,argv_str->strptrs);
#endif /* NO_ARG_COPY */

    
    if (cacheit) {
	k0func = (volvp)icache_it((void *)func);
	msg_printf(VRB, "    do_launch: func 0x%x, k0func 0x%x\n",
		(__psunsigned_t)func, (__psunsigned_t)k0func);
	func = k0func;
    }

    IDE_PRIVATE(l_vid,fn_addr) = func;


#if defined(LAUNCH_DEBUG)
    if (Reportlevel >= V3DBG) {
	msg_printf(JUST_DOIT, "  do_slave_asynch(v%d): fnaddr 0x%x, finalargs\n  -->",
		vid_me, IDE_PRIVATE(l_vid,fn_addr));
	_dump_argvs(IDE_PRIVATE(l_vid,fn_argcv.argc),
		IDE_PRIVATE(l_vid,fn_argcv.argv));
    }
#endif /* LAUNCH_DEBUG */

    msg_printf(V3DBG,"  (v%d)-->target vid %d: do a REMOTE fncall\n",
		vid_me, l_vid);
    retcode = _do_remote(l_vid, REMOTE_ASYNCH);

#if defined(LAUNCH_DEBUG)
    msg_printf(V3DBG,"  (v%d)-->_do_slave_async: fn returned %d\n", vid_me,retcode);
#endif /* LAUNCH_DEBUG */

    return(retcode);

} /* _do_slave_async*/


/* 
 * _do_local() actually invokes the function via a local procedure
 * call; therefore it always executes on the target processor.
 * If the target processor is that of the ide_master, _do_local is
 * called directly by _do_launch and no syncronization is necessary.
 * If the launch is remote, _do_launch() calls _do_remote(), which
 * nudges ide_slave()--the monitoring process on the target processor,
 * idling in ide_slave_wait()--which then invokes _ide_local() to call
 * the function.  In both cases _do_launch() stores the function addr
 * and <argc,argv> struct in the target processor's pda before 
 * _do_local() runs.
 */
int
_do_local(int do_protocol)
{
    int callshared(int,char**,int (*)(int,char **));
    char	bbuf[128];
    int (*func)(int, char **);
    int vid_me = cpuid();
    varcs_reg_t my_gpvid = GPME(my_virtid);
    argdesc_t argcv;
    volatile long cmd;
    int rc;

    ASSERT(vid_me == my_gpvid);

    argcv = IDE_ME(fn_argcv);
    func = (int (*)())IDE_ME(fn_addr);

    SPRINTF_CS(bbuf, vid_me);
    if (do_protocol)
	msg_printf(DBG, "  +>_do_local(v%d): %s\n", vid_me, bbuf);
    else
	msg_printf(DBG, "  +>_do_local(v%d)\n", vid_me);

    msg_printf(DBG+1,
	"    ++> _do_local(v%d <%s>): call fn %s (addr 0x%x), argc %d\n",
	vid_me, (do_protocol ? "SYNC" : "NOSYNC"), argcv.argv[0],
	(__psunsigned_t)func, argcv.argc);

    /* For nonshared -> shared IP30 modular IDE */
#if IP30
    rc = callshared(argcv.argc,argcv.argv,func);
#else
    rc = func(argcv.argc,argcv.argv);
#endif

    IDE_ME(fn_return) = rc;	/* must do this before ACKing command */

    if (do_protocol) {	/* ACK master's SLAVE_CALL_FN command */
	ACK_DONE(vid_me);
    }

    /* wait while master grabs the results from my pda */
    if (do_protocol) {
	AWAIT_CMD(vid_me,cmd);
    }

    msg_printf(DBG,"    ++> _do_local(v%d <%s>): fn rc %d\n",
	vid_me, (do_protocol ? "SYNC" : "NOSYNC"), rc);

    if (do_protocol) {
	SPRINTF_CS(bbuf, vid_me);
	msg_printf(DBG, "  +>_do_local(v%d), exits: %s\n", vid_me, bbuf);
    }

    return(rc);

} /* _do_local */


/* 
 * _do_remote() handles the dispatch side of remote function calls:
 * i.e. function launches whose target processors are not the same as
 * that of the ide_master;  therefore it always executes on the master's
 * processor.  _do_remote() is invoked by _do_launch(), and interacts
 * with _do_local(), which is executing on the remote (target) processor
 * and actually invokes the launch routine.
 * A simple handshaking protocol is used for master-slave synchronization.
 * _do_launch stores the function addr and <argc,argv> struct in
 * the remote processor's pda before calling _do_remote(), but hasn't yet
 * signaled the remote slave (which is idling in ide_slave_wait()).
 */
int
_do_remote(uint launch_vid, uint syncmode)
{
    char	bbuf[128];
    uint	vid_me = cpuid();	/* Virtual processor ID of this CPU */
    varcs_reg_t	my_gpvid = GPME(my_virtid);	/* sanity check */
    volatile long cmd, status;
    volatile long retval;
    volatile uint addr;

    ASSERT(vid_me == my_gpvid);

    msg_printf(V3DBG,
	"  _do_remote(v%d):  remote vid %d\n", vid_me,launch_vid);

    CMD_STATUS(launch_vid,cmd,status);
    SPRINTF_CS(bbuf,launch_vid);
    msg_printf(DBG, "  ->_do_remote(v%d): v%d's incoming cmd/stat: %s\n",
	vid_me, launch_vid, bbuf);

    if (status != STAT_SDONE) {
	msg_printf(DBG,
	    "_do_remote(v%d)--v%d's d/d 0x%x, not %s (0x%x): I'll wait...",
	    vid_me, launch_vid, status, statstrs[STAT_SDONE], STAT_SDONE);
	msg_printf(ERR,
	    "  IDE(v%d): slave v%d status not done"
#if EVEREST /* IP30 has no CPU led */
	    ", check LED for dead v%d"
#endif
	    ".",

	    vid_me, launch_vid, launch_vid);
	AWAIT_DONE(launch_vid);
	CMD_STATUS(launch_vid,cmd,status);
	if (status == STAT_SDONE) {
	    msg_printf(ERR, "IDE(v%d): slave v%d's status ok now!\n",vid_me,launch_vid);
	} else
	if (status == STAT_SINTER) {	/* not fully-baked yet */
	    msg_printf(ERR,
		"IDE(v%d): slave v%d was INTERRUPTED: abort\n",
		vid_me, launch_vid);
	    ide_panic("master: child got INTERRUPT before fn");
	} else {  /* currently IDE can only recover from interrupts */
	    ide_panic("_do_remote: bad status field");
	}
    }

    /* wake the slave up and confirm it's ready */
    ISSUE_CMD(launch_vid,SLAVE_WAKEUP);
    AWAIT_DONE(launch_vid);
    msg_printf(DBG, "    ->_do_remote(v%d): slave v%d is awake\n",
	vid_me,launch_vid);

    ISSUE_CMD(launch_vid,SLAVE_CALL_FN);

    /* THIS IS FOR PARALLEL MODE ONLY */
    if (syncmode == REMOTE_ASYNCH)
	return(0);	/* success! - but only that we have launched */

    T_AWAIT_DONE(launch_vid,Slave_Timeout,status);

    /* slave already stored fn's return value in its fn_return field */
    retval = IDE_PRIVATE(launch_vid,fn_return);
    CMD_STATUS(launch_vid,cmd,status);
    SPRINTF_CS(bbuf,launch_vid);

    if (status != STAT_SDONE) {
	msg_printf(DBG,
	    "  master: !!!slave(v%d)'s %s!!  retval %d\n",
	    launch_vid, bbuf, retval);
	if (status == STAT_SINTER) {
	    ISSUE_CMD(launch_vid,SLAVE_GO_HOME);
	    AWAIT_DONE(launch_vid);
	    SPRINTF_CS(bbuf,launch_vid);
	    msg_printf(DBG, "slave v%d ACKed `GOHOME' cmd, (%s)\n",
		launch_vid, bbuf);
	    return(SLAVE_INTER);
	} else {
	    ide_panic("_do_remote: bad status field");
	}
    }

    ISSUE_CMD(launch_vid,SLAVE_GO_HOME);
    AWAIT_DONE(launch_vid);
    msg_printf(DBG,
	"  ->_do_remote(v%d):  slave(v%d) return val %d: %s\n",
	vid_me, launch_vid, retval, bbuf);

    return((int)retval);		/* only keep 32-bits for now */
} /* _do_remote */



#if MULTIPROCESSOR
/*
 * ide_slave_wait()
 *	IDE slave-processor idle-wait function.  The slaves periodically
 *	check to see if the ide_master has written an order into the
 *	fn_handshake field of their pda.  If so, return.
 *	This allows the waiting to be polled or interrupt driven.
 */
static int 
ide_slave_wait(void)
{
    char	bbuf[128];
    uint	vid_me = cpuid();	/* Virtual processor ID of this CPU */
    uint	my_pidx;
    uint	cmd;
    /*REFERENCED*/
    uint	status;

#if EVEREST
    my_pidx = vid_to_phys_idx[vid_me];
#else
    my_pidx = vid_me;
#endif

    CMD_STATUS(vid_me,cmd,status);
    SPRINTF_CS(bbuf,vid_me);
    msg_printf(DBG, "    +>slave_wait(v%d/pidx%d): %s\n",
	vid_me, my_pidx, bbuf);
#ifdef EVEREST
    _1flash_cc_leds(vid_me, 0);
#endif

    ACK_DONE(vid_me);	/* ACK SLAVE_GO_HOME cmd */

    AWAIT_CMD(vid_me,cmd);
    status = IDE_PRIVATE(vid_me,doit_done);
    SPRINTF_CS(bbuf,vid_me);
    msg_printf(DBG, "    +>slave_wait(v%d): I'm awake, %s!\n", 
	    vid_me, bbuf);

    ACK_DONE(vid_me);	/* ACK SLAVE_WAKEUP cmd */

    AWAIT_CMD(vid_me,cmd);
    SPRINTF_CS(bbuf,vid_me);
    msg_printf(DBG,"    +>slave_wait(v%d/pidx %d) exits: %s\n",
	    vid_me, my_pidx, bbuf);

    return(cmd);
} /* ide_slave_wait */
#endif /* MULTIPROCESSOR */


#if MULTIPROCESSOR
void
ide_slave_boot()
{
    char bbuf[128];
    char setbuffer[SETBUFSIZ];
    char cbuf[MAXLINELEN], *cptr=&cbuf[0];
    volatile int vid_me = cpuid();
    volatile int jval;
    set_t vset;
    uint sp;

    sp = get_sp();

#ifdef EVEREST
    if(isgraphic(StandardOut))
	evslave_gfxinit(vid_me);
#endif

#ifdef	SLAVE_DEBUG
    msg_printf(DBG,"    ==> ide_slave_boot <vid %d>: sp 0x%x\n", vid_me);
    msg_printf(VDBG,"        stack 0x%x, &arcspda 0x%x, &idepda 0x%x\n", 
	sp, &(GPME(gpda_magic)), &(IDE_ME(ide_pdamagic)));
#endif

    if (GPME(my_virtid) != vid_me)
	msg_printf(IDE_ERR, "my_virtid (%d) != vid_me (%d)\n",
		GPME(my_virtid), vid_me);
    if (VID_OUTARANGE(vid_me)) {
	msg_printf(IDE_ERR, "slave %d's cpuid() returned %d (OUT OF RANGE)\n",
		GPME(my_virtid), vid_me);
    }
    if (IDE_ME(ide_mode) != IDE_SLAVE)
	msg_printf(IDE_ERR, "slave %d's ide_mode (%d) not SLAVE (%d)\n",
		GPME(my_virtid), IDE_ME(ide_mode), IDE_SLAVE);

    msg_printf(SUM, "    ");
    ide_whoami(cptr);
    msg_printf(JUST_DOIT, "%s", cbuf);
    cbuf[0] = '\0';

    if (jval = setjmp(IDE_ME(slave_jbuf))) {
	volatile uint cmd, status;

	vid_me = cpuid();
	ide_whoami(NULL);	/* sanity-check everything */

	if (jval == SLAVE_INT_RESTART) {
	    msg_printf(DBG, "  slave %d longjmp'ed from handler\n", vid_me);
	    CMD_STATUS(vid_me,cmd,status);
	    if (cmd == SLAVE_CALL_FN) {
		msg_printf(ERR, "--> slave interrupted, ACK and abort!\n");
		ACK_INTERRUPT(vid_me); /* tell master we were interrupted */
		AWAIT_CMD(vid_me,cmd);
		SPRINTF_CS(bbuf,vid_me);
		msg_printf(DBG, "  slave(v%d), setjmp: master's %s\n",
		    vid_me, bbuf);
		if (cmd != SLAVE_GO_HOME) /* do it anyway */
		    msg_printf(IDE_ERR,"slave(v%d): unexpected cmd %d!\n",
			vid_me,cmd);
	    }
	    /* slave shouldn't ever change its cmd field, but...*/
	    IDE_PRIVATE(vid_me,doit_done) = STAT_SDOIT; /* block cmds */
	    IDE_PRIVATE(vid_me,slave_cmd) = SLAVE_GO_HOME;
	} else {
	    IDE_PRIVATE(vid_me,doit_done) = STAT_SDOIT;
	    IDE_PRIVATE(vid_me,slave_cmd) = SLAVE_GO_HOME;
	    msg_printf(IDE_ERR,
		"slave(v%d)setjmp: cmd %d unexpected! (stat 0x%x)\n",
		vid_me, cmd, status);
	}
    } /* setjmp */

/*    LOCK_IDE_INFO(); */
    _ide_info.waiting_vids |= (SET_ZEROBIT >> vid_me);
    vset = _ide_info.waiting_vids;
/*    UNLOCK_IDE_INFO(); */

    if (Reportlevel >= VDBG) {
	sprintf_cset(setbuffer, vset);
	msg_printf(JUST_DOIT, "    waiting-vid set: %s\n", setbuffer);
    }

    flush_cache();	/* cpu-local primary I&D + secondary */

    ide_slave();
    /* NOTREACHED */

} /* ide_slave_boot */
#endif /* MULTIPROCESSOR */

