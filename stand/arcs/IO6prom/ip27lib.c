#ifdef SABLE
/***********************************************************************\
*	File:		hub.c						*
*									*
*	This file contains the IP27 PROM Utility Library routines for	*
*	manipulating miscellaneous features of the hub.			*
*									*
\***********************************************************************/

#ident "$Revision: 1.13 $"

#include <sys/types.h>
#include <sys/SN/agent.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/klconfig.h>

#include "sys/nic.h"
#include "ip27prom.h"
#include "libsk.h"

ulong	get_BSR(void);
void	set_BSR(ulong);

#define hub_cpu_get() \
	((int) (get_BSR() & BSR_CPUNUM))

#define BSR_GMASTER             0x00008000      /* am global master */

int ip27prom_epilogue(void) ;
void putchar(char) ;
extern int cpuid(void) ;

/*
 * The hub NIC is read once and kept cached since it's expensive to read.
 */

static	nic_t			hub_nic;

#define ARB_ALIVE_A             PI_RT_COMPARE_A
#define ARB_ALIVE_B             PI_RT_COMPARE_B

#ifdef SABLE
#define LOCAL_ARB_TIMEOUT       50
#else
#define LOCAL_ARB_TIMEOUT       5000000
#endif

#define SINGLE()                ((get_BSR() & BSR_SINGLE ) != 0)
#define MASTER()                ((get_BSR() & BSR_LMASTER) != 0)


/*
 * During boot, several scratch registers are needed for mutual exclusion
 * between the local CPUs.  They must be read/write, cleared to zero on
 * reset, not cause funny things to happen, and not get twiddled by the
 * chip, and not get used during initialization.  It's very difficult to
 * find registers fitting the bill.  We'll use the MD_PERF_CNTx registers,
 * which are not zero on reset, but are zeroed at the beginning of the
 * PROM before the local arbitration.
 */

#define CPU_A_WANT		MD_PERF_CNT0
#define CPU_B_WANT		MD_PERF_CNT1
#define CPU_B_FAVOR		MD_PERF_CNT2

#define BARRIER_A		MD_PERF_CNT3
#define BARRIER_B		MD_PERF_CNT4
#define IO6_HUB_LAUNCH		MD_PERF_CNT5

#define TWO_CPUS()		((get_BSR() & BSR_SINGLE) == 0)

/*
 * WASTE_TIME is the delay to avoid swamping the hub in polling loops.
 * In practice, both writing and reading a hub register appears to take
 * from 80 to 200 cycles!
 */

#define WASTE_TIME		100

void ip27_delay(int) ;

#define CTRL(x)		((x) & 0x1f)

#ifdef	SABLE
#define	DELAY_COUNTER	100
#else
#define	DELAY_COUNTER	0x30000
#endif

/*
 * hub_barrier
 *
 *  Closely synchronizes the CPUs on the local node at a barrier point.
 *  The algorithm follows.  The BARRIER_A and BARRIER_B registers
 *  are used for synchronization variables A and B.  They are both assumed
 *  to be 0 before using barriers.  Does nothing if only one CPU is active.
 *
 *	A = 1;			B = 1;
 *	while (B == 0);		while (A == 0);
 *	B = 0;			A = 0;
 *	while (A == 1);		while (B == 1);
 */

void
hub_barrier(void)
{
/*
    hub_led_set(PLED_BARRIER);
*/

    if (TWO_CPUS()) {
	if (hub_cpu_get() == 0) {
	    SD(LOCAL_HUB(BARRIER_A), 1);

	    while (LD(LOCAL_HUB(BARRIER_B)) == 0)
		ip27_delay(WASTE_TIME);

	    SD(LOCAL_HUB(BARRIER_B), 0);

	    while (LD(LOCAL_HUB(BARRIER_A)) != 0)
		ip27_delay(WASTE_TIME);
	} else {
	    SD(LOCAL_HUB(BARRIER_B), 1);

	    while (LD(LOCAL_HUB(BARRIER_A)) == 0)
		ip27_delay(WASTE_TIME);

	    SD(LOCAL_HUB(BARRIER_A), 0);

	    while (LD(LOCAL_HUB(BARRIER_B)) != 0)
		ip27_delay(WASTE_TIME);
	}
    }

/*
    hub_led_set(PLED_BARRIEROK);
*/
}

/*
 * hub_lock
 *
 *  Allows CPU A and CPU B to mutually exclude the hub from one another by
 *  obtaining a blocking lock.  Does nothing if only one CPU is active.
 *  Implements Peterson's Algorithm for two-process exclusion.  Utilizes
 *  three hub registers as communication scratch registers.
 */

void
hub_lock(void)
{
    if (TWO_CPUS()) {
	if (hub_cpu_get() == 0) {
	    SD(LOCAL_HUB(CPU_A_WANT ), 1);
	    SD(LOCAL_HUB(CPU_B_FAVOR), 1);
	    while (LD(LOCAL_HUB(CPU_B_WANT)) && LD(LOCAL_HUB(CPU_B_FAVOR)))
		ip27_delay(WASTE_TIME);
	} else {
	    SD(LOCAL_HUB(CPU_B_WANT ), 1);
	    SD(LOCAL_HUB(CPU_B_FAVOR), 0);
	    while (LD(LOCAL_HUB(CPU_A_WANT)) && ! LD(LOCAL_HUB(CPU_B_FAVOR)))
		ip27_delay(WASTE_TIME);
	}
    }
}

/*
 * hub_unlock
 *
 *  Counterpart to hub_lock
 */

void
hub_unlock(void)
{
    if (TWO_CPUS()) {
	if (hub_cpu_get() == 0)
	    SD(LOCAL_HUB(CPU_A_WANT), 0);
	else
	    SD(LOCAL_HUB(CPU_B_WANT), 0);
    }
}

/*
 * hub_nic_get
 *
 *   XXX - CLOCK_PHASE_BITS should depend on MHz
 */

#define CLOCK_PHASE_BITS    (0x31UL << 27 | 0x31UL << 20)

/*ARGSUSED*/
static uint
read_nic_ulan(nic_data_t data)
{
    return (uint) LD(LOCAL_HUB(MD_MLAN_CTL)) & 0xfffff;
}

/*ARGSUSED*/
static void
write_nic_ulan(nic_data_t data, uint value)
{
    SD(LOCAL_HUB(MD_MLAN_CTL),
       CLOCK_PHASE_BITS | (long) value);
}

int
hub_nic_get(nic_t *nic)
{
    nic_state_t		ns;

    if (hub_nic == 0) {
	hub_lock();

	nic_setup(&ns,
		  read_nic_ulan,
		  write_nic_ulan,
		  (nic_data_t) 0);

	hub_nic = 0;	    /* Clear two MS bytes */

	if (nic_next(&ns, (char *) &hub_nic + 2, (char *) 0, (char *) 0) < 0) {
	    hub_unlock();
	    return -1;
	}

	hub_unlock();
    }

    *nic = hub_nic;
    return 0;
}

/*
 * arb_local_master
 *
 *   BSR_LMASTER will be set in the BSR for only one of the CPUs.
 *   If only one CPU is present or responding, then BSR_SINGLE will
 *   also be set in that CPU's BSR.
 */

void arb_local_master(void)
{
    int		cpu_num;
    ulong	bsr;

    bsr	    = get_BSR() & ~(BSR_LMASTER | BSR_SINGLE);
    cpu_num = hub_cpu_get();

    /*
     * If only one CPU is present, it becomes the master.
     */

    if (! LD(LOCAL_HUB(PI_CPU_PRESENT_A)) ||
	! LD(LOCAL_HUB(PI_CPU_PRESENT_B))) {

	bsr |= (BSR_LMASTER | BSR_SINGLE);

    } else {
	int	i, my_alive, their_alive;

	/*
	 * Each CPU sets its own alive indicator.
	 */

	if (cpu_num == 0) {
	    my_alive	= ARB_ALIVE_A;
	    their_alive = ARB_ALIVE_B;
	} else {
	    my_alive	= ARB_ALIVE_B;
	    their_alive = ARB_ALIVE_A;
	}

	SD(LOCAL_HUB(my_alive), 0xbeef);

	/*
	 * If our own alive indicator has not turned on, flash LEDs and
	 * allow the other CPU to time out and become the master.

	if (LD(LOCAL_HUB(my_alive)) != 0xbeef)
	    hub_led_die(FLED_HUBLOCAL);
	 */

	/*
	 * Wait in a timeout loop for the other processor's indicator.
	 * We would like the timeout to be around 0.25 sec.
	 */

	for (i = 0; i < LOCAL_ARB_TIMEOUT; i++)
	    if (LD(LOCAL_HUB(their_alive)) == 0xbeef)
		break;

	if (i == LOCAL_ARB_TIMEOUT) {
	    /*
	     * Timed out waiting for other CPU.  Become single master.
	     */

	    bsr |= (BSR_LMASTER | BSR_SINGLE);
	} else {
	    /*
	     * Both CPUs okay; CPU A becomes master.
	     */

	    if (cpu_num == 0)
		bsr |= BSR_LMASTER;
	}

	SD(LOCAL_HUB(my_alive), 0);	/* Clean up */
    }

    set_BSR(bsr);

#if 0
/* IN sable IO6prom I have not yet set up stacks for each cpu.
   So, assume that there are 2 cpus and make cpu a the master. */

   if (! LD(LOCAL_HUB(PI_CPU_PRESENT_A)) ||
	! LD(LOCAL_HUB(PI_CPU_PRESENT_B))) 
		set_BSR(get_BSR() | (BSR_LMASTER | BSR_SINGLE));
   else {
   if (hub_cpu_get() == 0)
      set_BSR((get_BSR() & ~(BSR_LMASTER | BSR_SINGLE)) | BSR_LMASTER) ;
   else
      set_BSR(get_BSR() & ~(BSR_LMASTER | BSR_SINGLE)) ;
   }

    /*
     * If only one CPU is present, it becomes the master.
     */
#endif

}

#if 0
/*
 * Return the cpuid of the Global Master CPU.
 * Info obtained from ip27prom. XXX from where?? IP27PROM_CFG.
 */

int
global_master(void)
{
	if (get_BSR() & BSR_GMASTER)
		return(nasid()) ;
	else
		return -1 ;  /* XXX - make sure this is not used as an index */
}
#endif

int
ip27prom_simulate(void)
{
    ulong bsr  ;


    /*
     * Arbitrate the boot master CPU.  After this, we can use hub locks,
     * which are required before local resources shared between CPU A
     * and CPU B (RTC, UARTs, IIC, etc.) can be used.
     */

    arb_local_master();
    hub_barrier();       /* Both CPUs are in sync */

    if (MASTER())
        putchar('M') ;

    hub_barrier() ;

    if (cpuid() == 0) {  /* Make cpu 0 the global master */
	bsr = get_BSR() ;
	bsr |= BSR_GMASTER ;
	set_BSR(bsr) ;
	/* XXX also need to set MD_PERF_CNT3 */
    }

    return 1 ;
}

int
ip27prom_epilogue(void)
{
    /* The code upto this point is to bring the IO6prom environment
       to the 'end of IP27prom' environment. What follows is the
       actual epilogue code. */

    if (MASTER()) {
    	SD(LOCAL_HUB(IO6_HUB_LAUNCH), 0);
    }

    hub_barrier() ;

    return 1 ;
}

#endif /* SABLE */ 


