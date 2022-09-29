
#ifdef NETDBX

#include "mp.h"
#include "dbgmon.h"
#include <sys/sbd.h>
#include <libsc.h>
#include "coord.h"

#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif


char *restart_argv[] = {
	"debug",
	"-R",
	"serial(0)",
	0
};
#define	RESTART_ARGC	(sizeof(restart_argv)/sizeof(restart_argv[0]) - 1)

volatile int	ncpus; /* intialized by _rdebug() */
volatile int	nwkdown = 0;
volatile int	keepnwkdown;

/*
 * Forward declarations.
 */
static int 	getnwk(void);

#ifdef MULTIPROCESSOR
/*
 * intialized by init_mp()
 */
volatile int 	symmon_owner = 0;
lock_t 		symmon_owner_lock;

#define 	GET_SYMMON_OWNER(j)					\
		{							\
			j = *(volatile int *) &symmon_owner;		\
		}

#define 	SET_SYMMON_OWNER(j)					\
		{							\
			*(volatile int *) &symmon_owner = j;		\
		}
			
volatile CpuState	cpustate[MAXCPU];
inst_t			*brkptaddr[MAXCPU];
lock_t 			cpustate_lock;
volatile int		lastturn = 0;
#ifdef SAVEDIE
volatile int		savedie = 0;
#endif
volatile int		cpugettingnwk = 0;


/*------------------------ Trace package ------------------------------------*/
char 		junk[512];		/* paranoid, take care of cache line */
char		tracebuf[40][512];
char		*tbp[40];
int		counter[40];

void
inittracebuf()
{
	int i;

	for (i = 0; i < 40; i++) {
		tbp[i] = &tracebuf[i][0];
		counter[i] = 0;
	}
}

void
logtrace(char *s)
{
	int me = cpuid();
	int l;
	char 	foo[128];

	sprintf(foo, "%d: %s", counter[me]++, s);
	l = strlen(foo);

	if (tbp[me] + l >= &tracebuf[me+1][0]) {
		for (; tbp[me] < &tracebuf[me+1][0]; tbp[me]++)
			*tbp[me] = NULL;
		tbp[me] = &tracebuf[me][0];
	}

	strncpy(tbp[me], foo, l); /* don't copy the NULL */
	tbp[me] += l;
}

void
showtrace()
{
	int me = cpuid();
	char c;

	tracebuf[me][511] =  NULL; /* make sure we don't print the next cpu's buf */
	if (*tbp[me] != NULL)
		printf("%s", tbp[me]);

	c = *tbp[me];
	*tbp[me] = NULL;
	printf("%s", tracebuf[me]);
	*tbp[me] = c;
	printf("Current Record #: %d\n", counter[me]);
}
/*------------------------ Trace package ------------------------------------*/



/*------------------------ CpuState manipulation ----------------------------*/
CpuState
getcpustate(int cpuid)
{
	int	i, j;
	i = splockspl(cpustate_lock, splhi);
	j = cpustate[cpuid];
	spunlockspl(cpustate_lock, i);
	return j;
}

void
setcpustate(int cpuid, CpuState s)
{
	int	i;
	i = splockspl(cpustate_lock, splhi);
	cpustate[cpuid] = s;
	spunlockspl(cpustate_lock, i);
}

char *
itocs(CpuState s)
{
	switch (s) {
	case EnteredSymmon: 	return("EnteredSymmon");
	case ReadyToResume: 	return("ReadyToResume");
	case OutOfSymmon: 	return("OutOfSymmon");
	case CSInvalid: 	return("Invalid");
	}
	return("Unknown");
}
/*------------------------ CpuState manipulation ----------------------------*/


int
safely_check_zingbp(void)
{
	k_machreg_t a0 = private.regs[R_A0];

	if (IS_KSEGDM(a0)) {
		jmp_buf 	a0_buf;
		jmpbufptr	oldnf;

		if (setjmp(a0_buf)) {
			printf("scz: Can't determine if 'ring' brkpt@ epc=0x%x, a0=0x%x\n", private.regs[R_EPC], a0);
			printf("scz: called from (ra) = 0x%x\n", private.regs[E_RA]);
			private.pda_nofault = oldnf;
			return -1;
		} else {
			oldnf = private.pda_nofault;
			private.pda_nofault = a0_buf;
			if (strncmp((char *)private.regs[R_A0],"zing",4) == 0) {
				private.pda_nofault=oldnf;
				/* Do an implicit step over the brk inst. */
				return 0;
			} else  {
				private.pda_nofault=oldnf;
				return 1;
			}
		}
	} else return 1;
}

int
waitedtoolong(register int *count, int limit)
{
	++(*count);
	if (inrdebug) *count = 0; /* racey code! */
	return(!inrdebug && ((*count) > limit) && (((*count) % 5000) == 0));
}

void
check_dumb_breakpoint(void)
{
	dprintf(("cpu %d, checking zing\n", cpuid()));
	logtrace("Checking zing\n");
	if (safely_check_zingbp() == 0) {
		dprintf(("cpu %d, IS zing\n", cpuid()));
		logtrace("IS zing\n");
		private.regs[R_EPC] += 4;
		private.regs[R_A0] = NULL; /* turn off zing */

		if (getcpustate(cpuid()) == ForcedContinue)
			return; /* don't leave rdebug loop yet */

		dprintf(("(cpu %d), ignoring zing breakpoint.\n", cpuid()));
		logtrace("Ignoring zing breakpoint.\n");

		/* force back to client mode*/
		private.dbg_modesav = MODE_CLIENT;

		/* no rdebug loop,  just book-keeping */
		leave_rdebug_loop(NO_CPUID);

		_resume_brkpt();
		/* NOT REACHED */
	}
	dprintf(("cpu %d, NOT zing\n", cpuid()));
	logtrace("NOT zing\n");
}

void
call_cmd_parser(char *msg)
{
	extern struct cmd_table ctbl[];

	printf("cpu %d, %s\n", cpuid(), msg);
	Signal (SIGINT, main_intr);

	symmon_spl();
	command_parser(ctbl, PROMPT, 0, 0);
}

static int
freeze_procs(void)
{
	cpumask_t stoplist;
	int i;

	CPUMASK_CLRALL( stoplist );
	for (i=0; i<ncpus; i++)
		if (getcpustate(i) != EnteredSymmon)
			CPUMASK_SETB(stoplist, i);

	if (upcall("stop", (char *)&stoplist) != -1)
		return(0);
	dprintf(("cpu %d, Couldn't stop processes\n", cpuid()));
	return(1);
}

void
enter_rdebug_loop(int crdbl) 	/* crdbl = 1 => call rdebug loop, else return */
{
	int vid_me = cpuid();
	int i, j, count;
	volatile int exowner;

	if (!netrdebug)
		return;
	dprintf(("Cpu %d in symmon\n", vid_me));
	logtrace("in symmon\n");
	if (getcpustate(vid_me) != OutOfSymmon) {
		printf("Cpu %d; already in symmon\n", vid_me);
		printf("Cpu %d: cause = 0x%x, badvaddr = 0x%x, pc = 0x%x\n",
			vid_me, private.exc_stack[E_CAUSE] & CAUSE_EXCMASK,
			private.exc_stack[E_BADVADDR],private.exc_stack[E_EPC]);
		if (private.dbg_mode == MODE_IDBG)
			printf("cpu %d, was in mode idbg\n", vid_me);
		return;
	}
	setcpustate(vid_me,EnteredSymmon);
		
	i = splockspl(symmon_owner_lock, splhi);
	exowner = symmon_owner; 		/* copy old owner */
	if (symmon_owner == NO_CPUID) 		/* if noone owns it, i own it */
		symmon_owner = vid_me;
	spunlockspl(symmon_owner_lock, i);

	dprintf(("Cpu %d => exowner = %d\n", vid_me, exowner));

	if (safely_check_zingbp() == 0)
		brkptaddr[vid_me] = 0;
	else
		brkptaddr[vid_me] = (inst_t *) private.regs[R_EPC];

	if (exowner == vid_me) {
		/*
		 * Must have got here as a result of exception in
		 * dbgmon mode ... nothing special. Or, zing'ed while
		 * in single step (ownership is retained while singlestepping).
		 */
		dprintf(("cpu %d, Normal processing\n", vid_me));
		logtrace("Normal processing\n");
		if (safely_check_zingbp() == 0) {
			dprintf(("cpu %d, zing'ed in single step!\n", vid_me));
			logtrace("zing'ed in single step!\n");
			private.regs[R_EPC] += 4;
			private.dbg_modesav = MODE_CLIENT;/* force back to client mode*/
			if (singlestepping != 1) {
				dprintf(("cpu %d, zing'ed, while not in single step!\n", vid_me));
				logtrace("zing'ed,while not in single step!\n");
				singlestepping = 1;
			}
			_resume_brkpt();
		}

		if (singlestepping) {
#ifdef SAVEDIE
		    if (savedie) {
			/*
			 * Restore interrupt enable (disabled
			 * in single stepping)
			 */
			private.regs[R_SR] |= 0x1;
			savedie = 0x0;
			logtrace("singlestep->restored IE\n");
		    }
#endif
		    /* enter rdebug loop */
		    dprintf(("cpu %d:about to enter rdebug on sstep\n",vid_me));
		    logtrace("About to enter rdebug on sstep\n");
		    _pdbx_clean(0); /* 0 = don't clean PERM brkpts */
		    _rdebug(RESTART_ARGC, restart_argv, 0, 0);
		}

		return; /* exception in dbgmon mode */
	} else if (exowner == NO_CPUID) {
		/*
		 * freeze all other processors first so we make
		 * sure we wind up with a single thread of control
		 * so we get bothered when bringing down the network 
		 */
		  

		/*
		 * Freeze other processors (by zing), if any of the
		 * other processors not in symmon.
		 */
		logtrace("Freezing procs ");
		for (i = 0; i < ncpus; i++)
			if (getcpustate(i) != EnteredSymmon) break;

		if (i != ncpus) { /* some processor out of symmon */
			logtrace("Freezing procs\n");
			freeze_procs();
		} else
			logtrace("all procs. in symmon\n");


		/*
		 * Wait till all processors in symmon.
		 */
	        if (!getenv("keepcpusup")) {
		    j = 0; count = 0;
		    dprintf(("Waiting for procs to enter symmon\n"));
		    logtrace("Waiting for procs to enter symmon\n");
		    while (j != ncpus) {
			for (i = 0, j = 0; i < ncpus; i++)
				j += (getcpustate(i) == EnteredSymmon);

			if (waitedtoolong(&count,10000)) {
				printf("(Cpu %d), Only %d cpus in symmon\n",
					vid_me, j);
				break;
			}
			if (count == 1000)
			    printf("(Cpu %d), waiting for procs to enter symmon\n", vid_me);
		    }
		    dprintf(("All procs in symmon\n"));
		    logtrace("All procs in symmon\n");
		    for (i = ncpus - 1; i >= 0; i--)
		        if (getcpustate(i) == EnteredSymmon) {
			    lastturn = i;
			    if (i != (ncpus-1))
				printf("(cpu %d), Last turn = %d\n", vid_me, i);
			    break;
			}
		    /*
		     * By default have cpu 1 as the one that will
		     * resume the nwk; this reduces the chance of
		     * finding the interface (et0) locked in the kernel.
		     * If cpu 1 is not in symmon, then take a chance
		     * and have arbitray cpu resume the nwk.
		     */
		    if (getcpustate(1) != EnteredSymmon)
			cpugettingnwk = vid_me;
		    else
			cpugettingnwk = 1;
		}

		dprintf(("after freezing processors\n"));
		if (!keepnwkdown) {
			dprintf(("(cpu %d) Getting nwk ...\ ", vid_me));
			logtrace("Getting nwk ... ");
			count = 0;
			while (getnwk() != 0) {
			    if (waitedtoolong(&count,1000)) {
			        printf("(Cpu %d), getting nwk (try %d)\n",
					vid_me, count);
				printf("*** Warning! Symmon will assume exclusive ownership of network; may not be able to cont. with kernel execution\n");
					goto donenwk;
			    }
			}
			if (count != 0)
				printf("cpu %d, got nwk after %d tries\n", vid_me, count);
			dprintf((" Got nwk.\n"));
			logtrace(" Got nwk.\n");
		} else {
			printf("(cpu %d), dont have to get nwk\n", vid_me);
		}
		dprintf(("cpu %d, got nwk\n", vid_me));
		logtrace("got nwk\n");
	donenwk:;
	} else {
		/*
		 *  Ha. Didn't get here in time, wait till no one owns.
		 */
		logtrace("Waiting for owner change\n");
		count = 0;
		while (exowner != vid_me) {
			GET_SYMMON_OWNER(exowner);
			if (waitedtoolong(&count,1000000000)) {
				printf("(Cpu %d), still waiting for owner change\n", vid_me);
				logtrace("Still waiting for owner change\n");
				call_cmd_parser("still waiting for owner change");
			}
		}
		dprintf(("cpu %d => got ownership\n", vid_me));
		logtrace("Got ownership\n");
		if (shouldexit == 2) {
			leave_rdebug_loop(NO_CPUID);
			setcpustate(vid_me,OutOfSymmon);
			private.bp_nofault = 0;
			return;
		}
	}
	if (crdbl == 0)
		return;
	dprintf(("cpu %d: checking breakpoint\n", vid_me));
	check_dumb_breakpoint(); /* may not return if brkpt was zing */
	dprintf(("cpu %d: about to enter rdebug\n", vid_me));
	logtrace("About to enter rdebug\n");
	_rdebug(RESTART_ARGC, restart_argv, 0, 0);
	printf("cpu %d: unexpected return from rdebug\n", vid_me);
}

void
leave_rdebug_loop(int cpu2continue)
{
	int 		vid_me = cpuid();
	volatile 	int town;
	jmpbufptr 	jbp;
	int		i, count;
	int		gaveupownership = 0;
	char		junk[128];

	if (!netrdebug)
		return;
	/*
	 * Mark self as on the wayout ...
	 */
	dprintf(("cpu %d, resume\n", vid_me));
	logtrace("Resume\n");
	setcpustate(vid_me,ReadyToResume);
	
	/*
	 * Sanity check. Only one cpu will be 'active' in symmon (not counting
	 * cpus waiting turn), and this active cpu better be the owner!
	 */
	GET_SYMMON_OWNER(town);
	if (town != vid_me) {
		printf("*** Cpu %d => out of symmon, interesting! owner = %d\n",
			vid_me, town);
		return;
	}
	if (cpu2continue == -2)  {
		/* while booting */
		dprintf(("Cpu %d, booting ... going to out\n", vid_me));
		goto out;
	}

	/*
	 * If necessry switch to particular cpu. If not,
	 * prepare to go out --- get all cpu's to be in resume state.
	 */
	dprintf(("Cpu to be continued is %d\n", cpu2continue));
	sprintf(junk, "Cpu to be continued is %d\n", cpu2continue);
	logtrace(junk);

	if ((cpu2continue & 0x3f) == vid_me) {
		/* Only this cpu will continue,
		 * rest will wait turn at various points
		 */
		if (cpu2continue & 0x80) {
			/*
			 * funny continue for step over function
			 */
			printf("Cpu %d, continue for stepping over function\n");
			singlestepping = 1;
			return;
		}
		printf("Cpu to be continued is %d\n", cpu2continue & 0x3f);
		if (private.bp_nofault) {
			printf("Cpu %d, switching back to self\n", vid_me);
			jbp = private.pda_nofault;
			private.pda_nofault = 0;
			longjmp(jbp, 1);
			/* NOT REACHED */
		} else {
			printf("*** pu %d, NO pdanofault to jump to!\n",
				vid_me);
			return;
		}
	} else if (cpu2continue != NO_CPUID) {
		/*
		 * Some specific cpu to be continued ...
		 */
		printf("Cpu to be continued is %d\n", cpu2continue);
		setcpustate(cpu2continue, ForcedContinue);
		SET_SYMMON_OWNER(cpu2continue);
		gaveupownership = 1;
	} else {
		/* (cpu2continue == NO_CPUID) */
		/*
		 * If there exists a cpu not on the way out pass baton (any)
		 * such cpu. That cpu will pass to any remaining, and so on ...
		 */
		for (i = 0; i < ncpus; i++) {
			if (getcpustate(i) == OutOfSymmon)
				printf("(cpu %d), cpu %d out of symmon!!\n",
					vid_me, i);
			if (getcpustate(i) == EnteredSymmon) {
				dprintf(("cpu %d, setting owner to %d\n", vid_me, i));
				sprintf(junk, "cpu %d, setting owner to %d\n", vid_me, i);
				logtrace(junk);
				SET_SYMMON_OWNER(i);
				gaveupownership = 1;
				break;
			}
		}
	}
	if (gaveupownership) {
		dprintf(("Cpu %d, waiting for turn\n", vid_me));
		logtrace("Waiting for turn after gaveupownership\n");
		count = 0;
		do {
			GET_SYMMON_OWNER(town);
			if (waitedtoolong(&count,1000000))
				printf("Cpu %d, waiting owner change\n",vid_me);
		} while ((town != vid_me)  && (town != NO_CPUID));

		dprintf(("Cpu %d, got turn\n", vid_me));
		logtrace("Got turn\n");
		if (town == vid_me) {
			if (private.bp_nofault) {
				printf("Cpu %d, switching back\n", vid_me);
				jbp = private.pda_nofault;
				private.pda_nofault = 0;
				longjmp(jbp, 1);
			} else {
				printf("*** pu %d, NO pdanofault to jump to!\n",
					vid_me);
				return;
			}
		} else return; /* No cpu is owner, all done */
	} else {
		/* Did not have to giveup ownership */
		/*
		 * Check that all procs to are in resume
		 */
		int done;

		done = 0;
		for (i = 0; i < ncpus; i++) {
			if (getcpustate(i) == ReadyToResume) done++;
			if (getcpustate(i) == OutOfSymmon)
				printf("Cpu %d, found cpu %d out of symmon\n",
					vid_me, i);
			if (getcpustate(i) == EnteredSymmon)
				printf("Cpu %d, found cpu %d EnteredSymmon\n",
					vid_me, i);
		}
		if (done != ncpus)
			printf("Cpu %d, all procs NOT in resume\n", vid_me);
out:
		dprintf(("cpu %d, resetting _owner\n", vid_me));
		logtrace("Resetting _owner\n");
		SET_SYMMON_OWNER(NO_CPUID);
	}
}

#ifdef EVEREST
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/epc.h>

static unsigned long	currtbase = ENETBUFS_BASE;
#endif

static int
getnwk(void)
{
	unsigned long newtbase;

	if ((newtbase = upcall("ec0_down", 0)) != -1) {
		nwkdown = 1;
#ifdef EVEREST
		if ((newtbase != 2) && (newtbase != currtbase)) {
			unsigned long newrbase;

			if ((newrbase = upcall("getrbase", 0)) != -1)
				ee_config(newtbase, EPC_256BUFS, newrbase);
			else {
				printf("getnwk: no getrbase to call, will assume normal rbase\n");
				ee_config(newtbase, EPC_256BUFS, 0uL);
			}
			currtbase = newtbase;
		}
#endif
		return(0);
	}
	dprintf(("(cpu %d), Couldn't get network\n", cpuid()));
	return(1);
}


int
resumenwk(void)
{
	int 		vid_me = cpuid();
	int		i, count;
	char		junk[80];

	if (!netrdebug)
		return 0;
	dprintf(("Cpu %d: resumenwk => Enter\n", vid_me));
	logtrace("resumenwk => Enter\n");
	if (singlestepping || kprinting) {
		dprintf(("cpu %d, singlestep or kpprint\n", vid_me));
		logtrace("Singlestep or kpprint\n");
		setcpustate(vid_me,OutOfSymmon);
#ifdef SAVEDIE
		if (singlestepping) {
			dprintf(("cpu %d, turnoff singlestep\n", vid_me));
			logtrace("Singlestep->savedie\n");
			savedie = (int) private.regs[R_SR] & 0x1;
			private.regs[R_SR] &= 0xfffffffe;
		}
#endif
		return 0;
	}
	/*
	 * Cpu 0 is the one that has to get the nwk back up.
	 */
	if (vid_me != cpugettingnwk) {
		dprintf(("Cpu %d: waiting for nwk to come up\n", vid_me));
		logtrace("Waiting for nwk to come up\n");
		count = 0;
		while (nwkdown)  {
			if (waitedtoolong(&count,10000000))
				call_cmd_parser("still waiting for nwkup");
		}	

		dprintf(("Cpu %d: waiting lastturn\n",vid_me));
		logtrace("Waiting lastturn\n");
		count = 0;
		while (lastturn != vid_me)  {
			if (waitedtoolong(&count,10000000))
				call_cmd_parser("still waiting for lastturn");
		}

		dprintf(("Cpu %d: got lastturn\n", vid_me));
		logtrace("Got Lastturn\n");

		i = vid_me-1; /* next cpu to get turn */
		while (i >= 0 && getcpustate(i) == OutOfSymmon)
			i--;
		dprintf(("Cpu %d, transferring control to cpu %d\n",vid_me,i));
		sprintf(junk, "Transferring control to cpu %d\n", i);
		logtrace(junk);

		lastturn = i;

		while (lastturn != -1);
		/* don't set out of symmon yet. other processors may be not have
		 * yet seen us in resume. Since proc 0 is the last one to get
		 * lastturn (cpus get lastturn in sequence (ncpus-1), ... 1, 0),
		 * if we loop till lastturn == 0, we know everyone checked our
		 * resume value, and hence we can set out of symmon
		 * at that point.
		 */
		dprintf(("Cpu %d: nwk is up, going to userland\n",vid_me));
		logtrace("Nwk is up, going to userland\n");
		setcpustate(vid_me,OutOfSymmon);
		if (shouldexit)
			private.bp_nofault = 0;
		return(0);
	}
	if (keepnwkdown) {
		printf("cpu %d, assuming nwk up, keeping nwk down\n", vid_me);
		goto AssumeNwkUp;
	}
	if (!isidbg()) {
		setcpustate(vid_me,OutOfSymmon);
		dprintf(("cpu %d, no idbg to call, return\n", vid_me));
		return(1);
	}

	dprintf(("Cpu %d: getting nwk back up\n", vid_me));
	logtrace("Getting nwk back up\n");
	if (upcall("ec0_up", 0)) {
		dprintf(("Cpu %d: nwk is back up\n", vid_me));
		logtrace("Kernel's Nwk is up\n");
AssumeNwkUp:
		nwkdown = 0;
		dprintf(("Cpu %d: waiting lastturn\n",vid_me));
		logtrace("Waiting lastturn\n");
		i = count = 0;
		while (lastturn != vid_me) {
			if (waitedtoolong(&count,10000000))
				call_cmd_parser("still waiting for lastturn");
		}
		i = vid_me-1; /* next cpu to get turn */
		while (i >=0 && getcpustate(i) == OutOfSymmon)
			i--;
		sprintf(junk,"Cpu %d, transferring control to %d\n", vid_me, i);
		logtrace(junk);

		lastturn = i;
		getcpustate(vid_me);
		while (lastturn != -1); /* synchronize on proc 0. */
		dprintf(("Cpu %d: got lastturn\n",vid_me));
		logtrace("Got lastturn\n");
		setcpustate(vid_me,OutOfSymmon);
		if (shouldexit) {
			private.bp_nofault = 0;
			netrdebug = 0;
		}
		return(0);
	}
	dprintf(("Couldn't get network\n"));
	return(1);
}

#else /* !MULTIPROCESSOR */ 

/* ARGSUSED */
void
enter_rdebug_loop(int crdbl)
{

	if (!netrdebug) 
		return;
	dprintf(("Cpu in symmon ... "));
	if (nwkdown) 
		dprintf(("nwk is already down\n"));
	else {
		dprintf(("Getting nwk ... "));
		getnwk();
		dprintf((" Got nwk.\n"));
	}
}

static int
getnwk(void)
{
	if (upcall("ec0_down", 0) != -1) {
		nwkdown = 1;
		return(0);
	}
	dprintf(("(cpu %d), Couldn't get network\n", cpuid()));
	return(1);
}

int
resumenwk(void)
{
	int 		vid_me = cpuid();
	char		junk[80];

	if (!netrdebug)
		return 0;
	dprintf(("Cpu %d: resumenwk => Enter\n", vid_me));
	if (singlestepping || kprinting) {
		dprintf(("cpu %d, singlestep or kpprint\n", vid_me));
		if (singlestepping) {
			dprintf(("cpu %d, turnoff singlestep\n", vid_me));
			singlestepping = 0;
		}
		return 0;
	}
	if (keepnwkdown) {
		printf("cpu %d, assuming nwk up, keeping nwk down\n", vid_me);
		goto AssumeNwkUp;
	}
	if (!isidbg()) {
		dprintf(("cpu %d, no idbg to call, return\n", vid_me));
		return(1);
	}
	dprintf(("Cpu %d: getting nwk back up\n", vid_me));
	if (upcall("ec0_up", 0)) {
		dprintf(("Cpu %d: nwk is back up\n", vid_me));
AssumeNwkUp:
		nwkdown = 0;
		if (shouldexit) {
			private.bp_nofault = 0;
			netrdebug = 0;
		}
		return(0);
	}
	dprintf(("Couldn't get network\n"));
	return(1);
}

/* ARGSUSED */
void
leave_rdebug_loop(int x)
{
	/* nothing here */
}
#endif

#endif /* NETDBX */
