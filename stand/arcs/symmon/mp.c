#include <setjmp.h>
#include <sys/types.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#include <sys/systm.h>
#include "fault.h"
#include "sys/sbd.h"
#include "sys/loaddrs.h"
#include "mp.h"
#include "dbgmon.h"
#include <libsc.h>
#include <parser.h>
#if SN
#include <sys/SN/addrs.h>
#include <sys/SN/launch.h>
#include <sys/SN/gda.h>
#include <sys/SN/klconfig.h>
#if SABLE
#include <arcs/spb.h>
#endif
#endif
#if SN || IP30
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/ioc3.h>
#endif	/* SN || IP30 */
#if defined(EVEREST) && defined(MULTIKERNEL)
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#endif


/* private bss data per cpu not in genpda */
static	k_machreg_t private_sregs[MAXCPU][NREGS];
static	char private_cbuf[MAXCPU][128];	/* buffer for line processing */
jmp_buf	private_jbuf[MAXCPU];
jmp_buf private_step_jmp[MAXCPU];

extern int cpu_enabled(cpuid_t);

lock_t user_interface_lock;
int ui_spl;

#if DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif
#ifdef NETDBX
#include "coord.h"
extern lock_t 	symmon_owner_lock;
extern lock_t 	cpustate_lock;
void inittracebuf(void);
#endif /* NETDBX */

extern struct cmd_table ctbl[];		/* generic command table */
extern struct cmd_table pd_ctbl[];	/* platform-dependent command table */
#if MULTIPROCESSOR
extern void _do_continue(void *);
extern void start_slave(void);
extern void slave_wait(void);
extern void init_brkpt_lock(void);
cpumask_t waiting_cpus;			/* mask of cpus currently in debug */
int loaner_cpuid = NO_CPUID;
volatile int talking_cpuid = NO_CPUID;
volatile enum swtype {noowner, symmon, client} ui_owner = noowner;
volatile int client_cpuid = NO_CPUID;
extern volatile int suspend_cpu;
static void nmi_console_steal(int);
#endif /* MULTIPROCESSOR */
extern void begin_cell(void);
extern void end_cell(void);

#if defined(SN) && defined(SABLE)
extern int fake_prom;
extern void propagate_lowmem(gda_t *, __int64_t);

typedef struct sabsym_mpconf_blk {	
	unchar	phys_id;	/* Physical ID of the processor */ 
	unchar	virt_id;	/* CPU virtual ID */	
	char	*launchaddr;	/* routine to start bootstrap */
	char	*stackaddr;	/* Stack pointer */
} sabsym_mpconf_t;

volatile sabsym_mpconf_t	sabsymblk[MAXCPUS];

int
sabsym_launch_slave(int virt_id, void (*lnch_fn)(void), void *stack)
{
	/* Check to see if virtual ID is current processor */
	if (virt_id == cpuid())	{
		/* Call launch */
		lnch_fn();
	} else {
		/* Make sure virt_id is valid */
		if (virt_id >= MAXCPUS || virt_id < 0) {
	    		printf("launch_slave: error: virt_id (0x%x) is too big.\n", 
				virt_id);
	    		return -1;
		}

		/* Set up the MP Block entry */
		sabsymblk[virt_id].stackaddr = stack;

		/* Do this last! - It will launch immediately. */

		sabsymblk[virt_id].launchaddr = (void *)lnch_fn;
	}
	return 0;
}
int 
sabsym_slave_loop(void)
{
	uint		vpid = cpuid();	/* Virtual processor ID of this CPU */
	void 	 	(*launch)(void);	/* Launch function */
	volatile sabsym_mpconf_t *mp;	/* Pointer to cpu's MPCONF block */

	mp = &(sabsymblk[vpid]);

	for (;;) {
	        us_delay(1000);

		if (!mp->launchaddr)
			continue;

#if 0
		/* Make sure this is a valid MPCONF block */
		if (mp->virt_id != vpid) {
			mp->launchaddr = 0;
			printf("slave_loop: Error: Invalid MPCONF block\n");
			continue;
		}
#endif /* 0 */

		/* Call the launch routine */
		launch = (void (*)(void))mp->launchaddr;
		mp->launchaddr = 0;
		if (IS_KSEG0((__psint_t)launch))
			flush_cache();
		if (mp->stackaddr == (void *)0)
			launch();
		else
			call_coroutine(launch, 0, (uint *)mp->stackaddr);

	}
}

#endif /* SN || SABLE */

/* Initialize bottom half of genpda.  Most of pda is initialized by genpda.c.
 */
void
init_symmon_gpda(void)
{
	int my_id = cpuid();
   	register int i;

	/* set up pda for all cpu */
	for (i = 0; i < MAXCPU; i++) {
		register struct generic_pda_s *pda;

		/* set up dbgmon stack pointer */
		/* XXX fixme - on sn0, we need to divide this into 
		 * a node and a CPU slice to get the stack address.
		 * It also involves fixing the way we number CPUs in symmon.
 		 */
#if defined(SN)
		{
			int nasid, cpu, cnode;
			extern nasid_t compact_to_nasid_node[];

			gen_pda_tabp[i] = NULL;

			cnode = get_cpu_cnode(i);
			if (cnode == INVALID_CNODEID)
				continue;

			nasid = compact_to_nasid_node[cnode];
			if (nasid == INVALID_NASID)
				continue;
			cpu = get_cpu_slice(i);
			/*
			 * if cpu not enabled, we will get -1 as cpu slice.
			 */
			if (cpu == -1)
			        continue;

			gen_pda_tabp[i] = (gen_pda_t *)GPDA_ADDR(nasid) + cpu;
			pda = (struct generic_pda_s *)&GEN_PDA_TAB(i).gdata;
			bzero(pda, sizeof(gen_pda_t));
			pda->dbg_stack = (int *)(SYMMON_STK_ADDR(nasid,cpu) +
				SYMMON_STK_SIZE(nasid) - 8);
#if SABLE
			printf("init_symmon_gpda: cpu%d nasid %d cpu %d  stack 0x%x\n",
				i, nasid, cpu, pda->dbg_stack);
#endif /* SABLE */
		}
#else
		pda = (struct generic_pda_s *)&gen_pda_tab[i].gdata;
		pda->dbg_stack = (int *)SYMMON_STACK_ADDR(i);
#endif

		/* set up jmp_bufs */
		pda->_restart_buf = private_jbuf[i];
		pda->step_jmp = private_step_jmp[i];
		pda->step_count = 0;

		/* clear nofault */
		pda->pda_nofault = 0;
		pda->bp_nofault = 0;

		/* set up sregs[] */
		pda->sregs = private_sregs[i];

		/* set up cbuf */
		pda->cbuf = private_cbuf[i];

		if (i == my_id)
			pda->dbg_mode = MODE_DBGMON;
		else
			pda->dbg_mode = MODE_CLIENT;
	}
}
 
#if MULTIPROCESSOR

#if EVEREST || SN || IP30
#ifndef SABLE
static	volatile int slave_ready[MAXCPU];
#else	/* SABLE */
	volatile int slave_ready[MAXCPU];
#endif /* SABLE */
#endif

void
_cont_slave(int whichcpu)
{
	while (suspend_cpu != NO_CPUID) {
		DPRINTF(("_cont_slave: wait for susp cpu %d starting cpu %d\n",
		       suspend_cpu, whichcpu));
	}

	if (whichcpu == NO_CPUID) {
		int i;

		if (loaner_cpuid != NO_CPUID) {
			/* If there is already a loaner_cpuid() then we're
			 * probably embedded within another "c all" and
			 * one of the continued CPUs has hit a breakpoint
			 * before we finished processing the command.
			 */
			printf("cpu%d: only continuing self, cpu%d still executing slave_funcs\n", cpuid(), loaner_cpuid);
			printf("probably a nested 'c all' command\n");

			slave_func(cpuid(), _do_continue, (void *)NULL);
			/* NOTREACHED */
			_fatal_error("_cont_slave: NOT REACHED\n");
		}

		for (i=0; i<MAXCPU; i++) {
			if ((i != cpuid()) && (i != loaner_cpuid))
				slave_func(i, _do_continue, (void *)NULL);
		}
		/* Continue myself */
		slave_func(cpuid(), _do_continue, (void *)1LL);
	} else
		slave_func(whichcpu, _do_continue, (void *)NULL);
}

#if defined(SABLE) && defined(SN)
int	sable_symmon_hwinit=0;
#endif

void
init_mp(void)
{
	int i;
#if EVEREST || SN || IP30
	uint	timeout;
	uint	maxcpu;
#ifdef SN
	gda_t *gdap = GDA;
#endif
#if IP30
	extern int cpu_physical(int);
#else	/* !IP30 */
	extern void cpu_physical(int, int *, int *);
#endif	/* !IP30 */

#if SABLE
	maxcpu = 4;		/* to speed things along */
#else	/* !SABLE */
	maxcpu = MAXCPU;
#endif	/* !SABLE */
#endif	/* EVEREST || SN || IP30 */

#if defined(SABLE) && defined(SN)
	sable_symmon_hwinit = 1;
	if (fake_prom) {
		/* first parameter should be GDA_ADDR(master_nasid), but
		 * for sable we know it's node zero.
		 * Propagate the GDA.
		 */
		propagate_lowmem((gda_t*)GDA_ADDR(0), (__int64_t)SPBADDR);
	}
#endif /* SABLE && SN */

	initlock(&user_interface_lock);
	init_brkpt_lock();
#ifdef NETDBX
	initlock(&symmon_owner_lock);
	initlock(&cpustate_lock);
	symmon_owner = 0; /* the boot cpu # */

	inittracebuf();

	for (i=0; i<maxcpu; i++)
		cpustate[i] = OutOfSymmon;
#endif /* NETDBX */

/* 
 * Symmon divides its stack area into TWO separate stacks!
 * It expects to be able to setjmp/longjmp between them.
 */
#define S_SIZE	(SYMMON_STACK_SIZE/2)

	for (i=0; i<maxcpu; i++) {
#if SN 
		int nasid, slice;
		cnodeid_t cnode;

		cnode = get_cpu_cnode(i);
		if (cnode == INVALID_CNODEID)
		    break;

		nasid = gdap->g_nasidtable[cnode];

		if (nasid == INVALID_NASID)
			break;

		slice = get_cpu_slice(i);
		
		if (slice == -1)
		    continue;

		if (i != cpuid()) {
#if defined(SABLE)
			if (fake_prom) {
				sabsym_launch_slave(i, start_slave,
				(char *)(GEN_PDA_TAB(i).gdata.dbg_stack)-S_SIZE);
			} else
#endif /* SABLE */
 			if ( cpu_enabled(i)) {
#if DEBUG
				printf("Launching CPU %d, nasid %d, slice %d\n",
					i, nasid, slice);
#endif
				LAUNCH_SLAVE(nasid, slice,
					(launch_proc_t)start_slave,
					/* param */ 0, 
					(char *)(GEN_PDA_TAB(i).gdata.dbg_stack)-S_SIZE,
					/* gp */ 0);
			}
		} else {
			if (!cpu_enabled(i))
				printf("WARNING: Master processor disabled.\n");
		}
#else /* !SN */
		if (i != cpuid())
			launch_slave(i, (void (*)(void *))start_slave, 0, 0, 0,
				(char *)(GEN_PDA_TAB(i).gdata.dbg_stack)-S_SIZE);
#endif

	}
#undef S_SIZE

#if EVEREST || SN || IP30
 	/*
 	 * Wait for all CPUs to signal they have started, before 
 	 * we continue and do something we shouldn't.
 	 */
#if SN
	timeout = 5000; /* ELSC takes a long time. */
#else
 	timeout = 1000;
#endif
#if SABLE || EMULATION
	timeout = 20000;
#endif	
 	for (i = 0; (i < maxcpu) && (timeout > 0); i++) {
 	        if ((cpuid() != i) && cpu_enabled(i)) {
 		        while (!slave_ready[i] && (timeout > 0)) {
			        us_delay(1000);
				timeout--;
 			}
 		}
#if defined(SN) && defined(SABLE)
		else if (fake_prom && (cpuid() != i) && slave_ready[i])
			printf("cpu%d slave_ready but NOT enabled in config info!\n", i);
#endif /* SABLE */
	}

 	if (timeout == 0) {		/* time out folks ... */
#if EVEREST || SN
 	        int	slot, slice;
#endif	/* EVEREST || SN */
 		
 	        printf("WARNING: symmon did not see CPU(s):");
 		for (i = 0; i < maxcpu; i++) {
 		        if (cpu_enabled(i)&&(i!=cpuid())&&!slave_ready[i]) {
#if IP30
 			        printf("%d ", cpu_physical(i));
#else
 			        cpu_physical(i, &slot, &slice);
 			        printf("%d[0x%x/0x%x] ", i, slot, slice);
#endif	/* IP30 */
 			}
 		}
 		printf("\nWARNING: system may not boot correctly\n");
 		printf("WARNING: continuing operation ...\n");
 	}
#endif /* EVEREST || SN || IP30 */
}

static int early_exit_check(void);

#ifdef EVEREST
extern volatile int z8530cons_was_polled;
extern int nmi_z8530cons_steal(int, int);

#define	CONSOLE_WAS_POLLED	z8530cons_was_polled
#define	NMI_CONS_STEAL		nmi_z8530cons_steal
#endif /* EVEREST */

#if SN || IP30
extern volatile int ns16550cons_was_polled;
extern int nmi_ns16550cons_steal(int, int);

#define	CONSOLE_WAS_POLLED	ns16550cons_was_polled
#define	NMI_CONS_STEAL		nmi_ns16550cons_steal
#endif /* SN */

#if SN || IP30
volatile ioc3reg_t *sscr_addr;
ioc3reg_t save_sscr;
int cons_hw_faulted = 0;

/* save IOC3's state and pause DMA. This should be called any time
 * ui_owner transitions from not-symmon to symmon
 */
static void
acquire_console_hw(void)
{
    jmp_buf cons_hw_jmpbuf;

    /*
     * Due to known hub II problems, we can take data bus errors when
     * reading/writing IOC3 registers. We don't want to go into an
     * infinite loop when trying to print stuff out. Catch the exception
     * so we can switch to alternate console.
     */
    if (cons_hw_faulted)
	return;

    if (setjmp(cons_hw_jmpbuf)) {
	cons_hw_faulted = 1;
	return;
    }

    private.pda_nofault = cons_hw_jmpbuf;

    if (sscr_addr == 0) {
#if SN
	extern nasid_t master_nasid;

	sscr_addr = &((ioc3_mem_t*)
		KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base)->port_a.sscr;

#else	/* IP30 */
	sscr_addr = (ioc3reg_t *)(IOC3_PCI_DEVIO_K1PTR + IOC3_SSCR_A_P);
#endif	/* SN */
    }
    save_sscr = PCI_INW(sscr_addr);

    if ((save_sscr & SSCR_DMA_EN) && !(save_sscr & SSCR_PAUSE_STATE)) {
	PCI_OUTW(sscr_addr, save_sscr | SSCR_DMA_PAUSE);
	while((PCI_INW(sscr_addr) & SSCR_PAUSE_STATE) == 0)
	    ;
    }

    private.pda_nofault = 0;
}

/* restore saved IOC3 state. Should be called any time ui_owner
 * transitions from symmon to not-symmon
 */
static void
release_console_hw(void)
{
    jmp_buf cons_hw_jmpbuf;

    if (cons_hw_faulted)
	return;

    if (setjmp(cons_hw_jmpbuf)) {
	cons_hw_faulted = 1;
	return;
    }

    private.pda_nofault = cons_hw_jmpbuf;

    PCI_OUTW(sscr_addr, save_sscr);
    while(PCI_INW(sscr_addr) != save_sscr)
	;

    private.pda_nofault = 0;
}

#define ACQUIRE_CONSOLE_HW() acquire_console_hw()
#define RELEASE_CONSOLE_HW() release_console_hw()

/* Entry point used by client (usually kernel) to lock the
 * user interface for a "short time".  Note that if the
 * client 'crashes' or enters symmon while holding this
 * lock, symmon will be unable to obtain control of
 * the user interface.
 */

void
cons_lock( int unlock )
{

again:
	LOCK_USER_INTERFACE();

	if (unlock) {	/* unlock */
		/* ui_owner SHOULD be client, unless client is
		 * incorrectly coded.
		 */

		if (ui_owner == client)
			ui_owner = noowner;

		client_cpuid = NO_CPUID;

	} else {	/* lock */
		if (ui_owner == noowner) {
			ui_owner = client;
			client_cpuid = cpuid();
		} else {
			UNLOCK_USER_INTERFACE();
			while (ui_owner != noowner)
				;
			goto again;
		}
	}

	UNLOCK_USER_INTERFACE();
}
#else

#define ACQUIRE_CONSOLE_HW()
#define RELEASE_CONSOLE_HW()

#endif /* SN || IP30 */
/*
 * Functions to acquire and release control of the single
 * UART shared by multiple processors.
 */
void
ACQUIRE_USER_INTERFACE(void)
{
	int initial_uart_count, delay_count=0, initial_talker, stoleit=0;
	int initial_ui_owner=NO_CPUID;

#ifdef NETDBX
	if (netrdebug)
		return;
#endif /* NETDBX */
	initial_uart_count = CONSOLE_WAS_POLLED;
	initial_talker = talking_cpuid;

again:
	LOCK_USER_INTERFACE();
 
	/* rip UI from client if we see an NMI */
	if (private.nmi_int_flag && (ui_owner == client)) {
		initial_ui_owner = ui_owner;
		ui_owner = noowner;
	}
		
	if (talking_cpuid == NO_CPUID) {
		if (ui_owner == client) {
			UNLOCK_USER_INTERFACE();
			goto again;
		}
		if (ui_owner != symmon)
			ACQUIRE_CONSOLE_HW();
		else
			printf("ERROR: ACQUIRE_UI, no cpuid & ui_owner %d\n",
			       ui_owner);

		ui_owner = symmon;
		talking_cpuid = cpuid();
		printf("(CPU %d) ", cpuid());
		if (initial_ui_owner != NO_CPUID)
			printf(" ui_owner at NMI was %d ", initial_ui_owner);
	}
	if (talking_cpuid != cpuid()) {
		CPUMASK_SETB( waiting_cpus, cpuid());
#if defined(EVEREST) && defined(MULTIKERNEL)
		/* No reason to have pending DEBUG interrupt if we're
		 * entering the debugger already.
		 */
		EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_DEBUG);
#endif /* EVEREST && MULTIKERNEL */
		UNLOCK_USER_INTERFACE();
		while (talking_cpuid != cpuid()) {
			if (cpuid() == 0) {
				if (early_exit_check()) {
					LOCK_USER_INTERFACE();
					CPUMASK_CLRB( waiting_cpus, cpuid());
					UNLOCK_USER_INTERFACE();
					goto again;
				}
			}
			/* Following check allow a cpu which has received an
			 * NMI interrupt to attempt to acquire the UI after
			 * it has been released.
			 */

			if (private.nmi_int_flag &&
			    (initial_uart_count != 0) &&
			    (initial_uart_count == CONSOLE_WAS_POLLED) &&
			    (initial_talker == talking_cpuid))  {
				delay_count++;
				/* Don't let all cpus timeout at the same
				 * time.  Empirical data shows that 100000
				 * delay_counts on an IP19 (150 MHz) takes
				 * about 1.3 seconds.
				 */
				if (delay_count > (100000*(cpuid()+1))) {
					talking_cpuid = cpuid();
					loaner_cpuid = NO_CPUID;
					stoleit = 1;
					nmi_console_steal( initial_talker );
					goto again;
				}
			} else
				delay_count = 0;
		}
		LOCK_USER_INTERFACE();
		CPUMASK_CLRB( waiting_cpus, cpuid() );
		UNLOCK_USER_INTERFACE();
		begin_cell();
		if (private.launch) {
			void (*volatile launch)(void *);
			launch = private.launch;
			private.launch = 0;
			/* Call requested routine */
			launch(private.launch_arg); 

			/* In case the launch routine returns */
			RELEASE_USER_INTERFACE(NO_CPUID);
			goto again;
		}

		return;
	}
	UNLOCK_USER_INTERFACE();
	begin_cell();

	if (private.nmi_int_flag) {
		private.nmi_int_flag = 0;
		if (stoleit)
			printf("(cpu%d) stealing UI from cpu%d after NMI\n",
				cpuid(), initial_talker);
	}
}

void
RELEASE_USER_INTERFACE(int id)
{
#ifdef NETDBX
	if (netrdebug)
		return;
#endif /* NETDBX */
	end_cell();
	LOCK_USER_INTERFACE();

	/*
	 * If someone just loaned the user interface to another
	 * processor, give it back now.
	 */
	if ((id == NO_CPUID) && (loaner_cpuid != NO_CPUID)) {
		talking_cpuid = loaner_cpuid;
		loaner_cpuid = NO_CPUID;
	} else
		talking_cpuid = id;

	if (talking_cpuid == NO_CPUID) {
		if (ui_owner == symmon)
			RELEASE_CONSOLE_HW();
		else
			printf("ERROR: RELEASE_UI but ui_owner %d\n",ui_owner);
		ui_owner = noowner;
	}
	UNLOCK_USER_INTERFACE();
}

/* following routine is just like RELEASE_USER_INTERFACE but is used in
 * one special case ("c all") where the current "loaner_cpid" would
 * ordinarily want to RELEASE, but there are already stopped cpus
 * and we instead want to give the interface to one of them.
 * NOTE: Not sure we really need this, but makes good debugging code.
 */
void
TRANSFER_USER_INTERFACE(int id)
{
#ifdef NETDBX
	if (netrdebug)
		return;
#endif /* NETDBX */
	end_cell();
	LOCK_USER_INTERFACE();

	if (loaner_cpuid == NO_CPUID) {
		talking_cpuid = id;
	} else {

		printf("DEBUG...TRANSFER: cpu %d  loaner %d TO %d\n", cpuid(), loaner_cpuid, id);
		/*
		 * If someone just loaned the user interface to another
		 * processor, give it back now.
		 */
		if (loaner_cpuid != cpuid())
			_fatal_error("TRANSFER_UI: loaner != cpuid()\n");
		else {
			printf("DEBUG...TRANSFER_USER_INTERFACE: from %d to %d\n",
			       loaner_cpuid, id);
			loaner_cpuid = NO_CPUID;
			talking_cpuid = id;
		}
	}

	if (talking_cpuid == NO_CPUID) {
		if (ui_owner == symmon)
			RELEASE_CONSOLE_HW();
		else
			printf("ERROR: TRANSFER_UI but ui_owner %d\n", ui_owner);
		ui_owner = noowner;
	}
	UNLOCK_USER_INTERFACE();
}

int
slave_func(int whichcpu, void (*func)(void *), void *arg)
{
	/* If trying to launch self, just call the routine directly */
	if (whichcpu == cpuid()) {
		func(arg); 
		return(0);
	}
		
	/* Insure we're launching someone who's currently stopped */
	if (!CPUMASK_TSTB( waiting_cpus, whichcpu ))
		return(-1);

	/* 
	 * Don't allow us to launch others from a launched routine. 
	 * It'd get too confusing to keep track of who's currently got
	 * access to the console.  This keeps it simple.
	 */
	if (loaner_cpuid != NO_CPUID)
#ifndef EXTRA_DEBUG_OFF
	{
	  printf("DEBUG...Can't call slave_func (0x%x) from a launched routine\n",
		 func);
	  printf("DEBUG...my cpuid == %d  loaner cpuid == %d launching == %d\n",
		 cpuid(), loaner_cpuid, whichcpu);
	}
#else	/* !EXTRA_DEBUG_OFF */
		printf("Can't call slave_func from a launched routine\n");
#endif /* !EXTRA_DEBUG_OFF */
	else
		loaner_cpuid = cpuid();

	/* we setup the launch_arg and launch function before dropping the
	 * UI since otherwise it's possible for the other cpu to obtain
	 * the UI before we're setup the function call and then it grabs
	 * the interface without invoking the slave function.
	 */
	GEN_PDA_TAB(whichcpu).gdata.launch_arg = arg;
	GEN_PDA_TAB(whichcpu).gdata.launch = func;
	RELEASE_USER_INTERFACE(whichcpu);
	ACQUIRE_USER_INTERFACE();
	return(0);	/* successfully launched */
}

void
do_command_parser(void)
{
	private.dbg_mode = MODE_DBGMON;

	ACQUIRE_USER_INTERFACE();

	if (setjmp(private._restart_buf)) {
		ACQUIRE_USER_INTERFACE();
		putchar('\n');
	}

	Signal (SIGINT, main_intr);

	symmon_spl();
	command_parser(ctbl, PROMPT, 0, 0);
}

#ifdef MULTIPROCESSOR
extern int consgetc(int);
static int
early_exit_check(void)
{
	int retval = 0;

	LOCK_USER_INTERFACE();
	if ((talking_cpuid == NO_CPUID) && (ui_owner != client)) {
		if ((char)consgetc(0) == 1 /* CNTRL-A */) {
			retval = 1;
		}
	}
	UNLOCK_USER_INTERFACE();

	return(retval);
}


static void
nmi_console_steal(int from_cpuid)
{

	LOCK_USER_INTERFACE();

	if (NMI_CONS_STEAL(0, from_cpuid))
		printf("nmi_console_steal: duart lock was held and broken!\n");

	UNLOCK_USER_INTERFACE();

	return;
}
#endif

#ifndef SN
void
slave_wait(void)
{
	private.dbg_mode = MODE_CLIENT;

	slave_loop();

	/*
	 * We got here because the user interrupted our sleep.
	 */
	private.dbg_mode = MODE_DBGMON;
	longjmp (private._restart_buf, 1);
}
#endif /* SN */


void
slave_boot(void)
{
#if SN
	extern void symmon_nmi();
	extern void elscuart_setup(void);
	extern void setup_nmi_handler(void (*)());
	extern nasid_t compact_to_nasid_node[];

	/* Each node has its own copy of the SPB, so we need to
	 * update the DebugBlock pointer.
	 */
	init_slave_debugblock_ptr();

	/*
	 * initialize the per cpu nmi structures.
	 */
	setup_nmi_handler(symmon_nmi);

	/*
	 * Set up the entry-level system controller
	 */
	elscuart_setup();

#endif
	if (setjmp(private._restart_buf)) {
		ACQUIRE_USER_INTERFACE();	/* must reacquire */
		putchar('\n');
		Signal (SIGINT, main_intr);
		symmon_spl();
		command_parser(ctbl, PROMPT, 0, 0);
	}

	symmon_spl();

#if EVEREST || SN || IP30
	slave_ready[cpuid()] = 1;
#endif

#if defined(SN)
	/* On sn0 systems we simply return to the prom after initializing */
	return;
#else	
	slave_wait();
	/* NOTREACHED */
#endif
}


static void
print_stopped(void)
{
	int cpu;
	cpumask_t	stopped_cpus = waiting_cpus;

	CPUMASK_SETB( stopped_cpus, cpuid());

	printf("stopped processors: ");
	for (cpu=0; cpu<MAXCPU; cpu++) {
		if (CPUMASK_TSTB( stopped_cpus, cpu )) {
			printf("%d ", cpu);
		}
	}
	printf("\n");
}


static void
print_running(void)
{
	int cpu;
	cpumask_t	stopped_cpus = waiting_cpus;

	CPUMASK_SETB( stopped_cpus, cpuid());

	printf("running processors: ");
	for (cpu=0; cpu<MAXCPU; cpu++) {
		if (cpu_enabled(cpu) && !CPUMASK_TSTB( stopped_cpus, cpu )) {
			printf("%d ", cpu);
		}
	}
	printf("\n");
}

/*
 * Pass control of the user interface to the specified
 * processor, and wait 'till someone tells us what to do.
 */
int
_switch_cpus(int argc, char **argv)
{
	numinp_t newid;
	int id = cpuid();

	if (argc == 1) {
#if EVEREST || SN
		printf("Currently connected to cpu %d (slot=0x%x proc=0x%x)\n", 
			id, cpuid_to_slot[id], cpuid_to_cpu[id]);
		if ((loaner_cpuid != NO_CPUID) || (suspend_cpu != NO_CPUID))
			printf("loaner cpu %d  suspend cpu %d\n", loaner_cpuid,
				suspend_cpu);
#elif IP30
		printf("Currently connected to cpu %d\n",id);
#endif

		print_stopped();
		print_running();
		return(0);
	}

	if (argc != 2)
		return(1);

	argv++;
	if (parse_sym(*argv, &newid) && *atob_s(*argv, &newid))
		_dbg_error("Illegal cpuid: %s", *argv);
		/* doesn't return */

	if ((newid < 0) || (newid > MAXCPU)) {
		printf("cpuid 0x%lx out of range\n", newid);
		return(1);
	}

	if (newid == id) {
		printf("already talking with cpu %d\n", id);
		return(0);
	}

	if (!CPUMASK_TSTB( waiting_cpus, newid )) {
		printf("cpu %s is not stopped\n", *argv);
		print_stopped();
		return(1);
	}

	RELEASE_USER_INTERFACE(newid);
	ACQUIRE_USER_INTERFACE();

	return(0);
}
#if defined(SABLE) && defined(SN)
void
sable_symmon_launch_kernel(int cpuid, char *launchaddr, char *stackptr)
{
	sabsym_launch_slave( cpuid, (void (*)(void))launchaddr, stackptr );
}
#endif /* SABLE && SN */
#endif /* MULTIPROCESSOR */
