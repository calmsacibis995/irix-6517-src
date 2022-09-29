/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * sysctlr.c -
 * 	This file contains the code to interact with the SN00/SN0
 * 	entry-level system controller (MSC) and handle system
 *	controller interrupts that indicate environmental problems.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/nodepda.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/ksignal.h>
#include <sys/sema.h>
#include <sys/schedctl.h>
#include <sys/runq.h>
#include <sys/clksupport.h>

#define LLP_PFAIL_TEST_SYSSGI	1

#include <sys/SN/vector.h>
#include <sys/SN/module.h>

#include <ksys/sthread.h>
#include <ksys/elsc.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/signal.h>
#include <sys/kthread.h>
#include <ksys/vproc.h>
#include <ksys/partition.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/slotnum.h>

#include "sn0_private.h"

/****************************************************************************
 * Private routines and data
 ***************************************************************************/

#define LDEBUG		0

#define DPRINTF		if (LDEBUG) printf

#define SD_THREAD_PRI	35		/* Prio. of shutdown thread         */
#define HBT_THREAD_PRI	NDPHIMAX	/* Prio. of heartbeat sender thread */

#define RECV_INTERVAL	30
#define SEND_INTERVAL	(RECV_INTERVAL / 3)
#define NMI_RST_DELAY	20

#define INT_TO_PTR(x)	((void *) (__psunsigned_t) (x))
#define PTR_TO_INT(x)	((int) (__psunsigned_t) (x))

#define TIME		(GET_LOCAL_RTC * NSEC_PER_CYCLE / 1000000000)
			/* Interrupts are off; must use RTC directly */

#define TIME_USEC	(GET_LOCAL_RTC * NSEC_PER_CYCLE / 1000)
			/* Interrupts are off; must use RTC directly */

#define NUMMODULES	(private.p_sn00 ? 1 : nummodules)


static int		shutdown_started;
static int		intrs_pending;

extern int		ignore_sysctlr_intr;	/* mtune/kernel */
extern int		powerfail_cleanio;	/* mtune/kernel */
extern int		powerfail_routerwar;	/* mtune/kernel */
extern int		clean_shutdown_time;	/* mtune/kernel */
extern int		heartbeat_enable;	/* mtune/kernel */
extern int		intr_revive_delay;	/* mtune/kernel */

extern int		power_off_flag;
extern void 		klhwg_add_rps(vertex_hdl_t, cnodeid_t, int );
extern void 		klhwg_add_xbox_rps(vertex_hdl_t, cnodeid_t, int );
extern void 		klhwg_update_rps(cnodeid_t, int );
extern void 		klhwg_update_xbox_rps(cnodeid_t, int );

#if LDEBUG
char			flatline;		/* Poke to simulate death */
#endif

/*
 * SN0 MSC message table
 */

#define ACT_IGNORE	0		/* Message should be ignored     */
#define ACT_WARN	1		/* Print warning message only    */
#define ACT_SHUTDN	2		/* Shut down the partition       */
#define ACT_PFAIL	3		/* Special power-fail handling   */
#define ACT_MAINT	4		/* Request hardware maintenance  */
#define	ACT_RST		5		/* Special reset handling  	 */
#define ACT_RPSFAIL	6		/* RPS fail handling 	 	 */
#define ACT_RPSOK	7		/* RPS revival handling 	 */
#define	ACT_SLAVE_RPSFAIL	8	/* RPS fail handling for O200	 */
#define ACT_SLAVE_RPSOK		9	/* RPS revival handling for O200 */

struct msc_msg_s {
    char	       *msg;
    char	       *desc;
    int			action;
} msgs[] = {
#define MSG_NONE		0
{ "",	      "MSC interrupt without message",		    ACT_IGNORE	},
#define MSG_AC			1
{ "ac",	      "AC power lost",				    ACT_PFAIL	},
#define MSG_FAN_FAIL		2
{ "fan fail", "Single fan failure (fan speed increased)",   ACT_MAINT	},
#define MSG_FAN_TEMP		3
{ "fan temp", "High temperature (fan speed increased)",	    ACT_IGNORE	},
#define MSG_NMI			4
{ "nmi",      "External non-maskable interrupt",	    ACT_WARN	},
#define MSG_NMI_HBT		5
{ "nmi hbt",  "Missed heartbeat (NMI pending)",		    ACT_WARN	},
#define MSG_PD_CYCLE		6
{ "pd cycle", "Remote power-cycle command",		    ACT_PFAIL	},
#define MSG_PD_OFF		7
{ "pd off",   "Module keyswitch turned OFF",		    ACT_SHUTDN	},
#define MSG_PD_PSFL		8
{ "pd psfl",  "Power supply failure",			    ACT_PFAIL	},
#define MSG_PD_PSOT		9
{ "pd psot",  "Power supply over-temperature",		    ACT_SHUTDN	},
#define MSG_PD_REMOTE		10
{ "pd remote","Remote power-down command",		    ACT_SHUTDN	},
#define MSG_PD_TEMP		11
{ "pd temp",  "Temperature limit exceeded",		    ACT_SHUTDN	},
#define MSG_PD_FAN		12
{ "pd fan",   "Multiple fan outage",			    ACT_SHUTDN	},
#define MSG_POK_1		13
{ "pok 1",    "Slot n1 power loss",			    ACT_PFAIL	},
#define MSG_POK_2		14
{ "pok 2",    "Slot n2 power loss",			    ACT_PFAIL	},
#define MSG_POK_3		15
{ "pok 3",    "Slot n3 power loss",			    ACT_PFAIL	},
#define MSG_POK_4		16
{ "pok 4",    "Slot n4 power loss",			    ACT_PFAIL	},
#define MSG_RST			17
{ "rst",      "External system reset",			    ACT_RST	},
#define MSG_RPWR_OUT		18
{ "rpwr out", "Power Supply Redundancy is Lost",	    ACT_RPSFAIL	},
#define MSG_RPWR_OK		19
{ "rpwr ok",  "Power Supply Redundancy is Restored", 	    ACT_RPSOK	},
#define MSG_SLAVE_RPWR_OUT	20	
{ "rpwr s out", "Power Supply Redundancy is Lost",	    ACT_SLAVE_RPSFAIL },
#define MSG_SLAVE_RPWR_OK	21	
{ "rpwr s ok",  "Power Supply Redundancy is Restored",      ACT_SLAVE_RPSOK }, 
#define MSG_COUNT		22
{ 0,	      			0,			    0		},
};

/*
 * msc_owner_cpu
 *
 *   Returns ID of the CPU that is handling raw sysctlr interrupts
 *   (at splerr) for a module.  This CPU's NASID should be used to
 *   access the MSC.
 */

static cpuid_t msc_owner_cpu(module_t *m)
{
    int			i;
    cpuid_t		cpu;

    for (i = 0; i < m->nodecnt; i++)
	if ((cpu = CNODE_TO_CPU_BASE(m->nodes[i])) != CPU_NONE)
	    return cpu;

    cmn_err(CE_PANIC, "sysctlr: module with all headless nodes");

    /*NOTREACHED*/
    return CPU_NONE;
}

static cnodeid_t msc_owner_cnode(module_t *m)
{
    int 		i;

    for (i=0; i < m->nodecnt; i++)
	if(m->nodes[i] != CNODEID_NONE)
		return m->nodes[i];

    return CNODEID_NONE;
}

/*
 * Router LLP power failure workaround
 */

static	volatile int		llp_pfail_mode;
static	volatile int		llp_pfail_barr1;
static	volatile int		llp_pfail_barr2;
static	lock_t			llp_pfail_lock;

#define CHIPID(status_rev_id) \
	((status_rev_id)  >> RSRI_CHIPID_SHFT & \
	 RSRI_CHIPID_MASK >> RSRI_CHIPID_SHFT)

#define P123_LLP8		(RDPARM_LLP8BIT(1) | \
				 RDPARM_LLP8BIT(2) | \
				 RDPARM_LLP8BIT(3))

#define P123_RESET		(RPRESET_LINK(1) | \
				 RPRESET_LINK(2) | \
				 RPRESET_LINK(3))

void llp_pfail_barrier(void)
{
#if LDEBUG
    __uint64_t		expire = TIME_USEC + 60000000;
#else
    __uint64_t		expire = TIME_USEC + 500000;
#endif

    atomicAddInt(&llp_pfail_barr1, 1);

    while (llp_pfail_barr1 % numcpus)
	if (TIME_USEC >= expire) {
	    expire = 0;
	    break;
	}

    atomicAddInt(&llp_pfail_barr2, 1);

    while (llp_pfail_barr2 % numcpus)
	if (TIME_USEC >= expire) {
	    expire = 0;
	    break;
	}

    if (expire == 0)
	cmn_err(CE_WARN, "llp_pfail_barrier: expired\n");
}

int llp_pfail_getmode(void)
{
    __uint64_t		sr, x;
    int			mode	= 16;

    if (vector_read(0, 0, RR_STATUS_REV_ID, &sr) < 0)
	sr = 0;

    if (CHIPID(sr) == CHIPID_HUB && private.p_sn00) {
	vector_read(0, 0, NI_DIAG_PARMS, &x);
	if (x & NDP_LLP8BITMODE)
	    mode = 8;
    } else if (CHIPID(sr) == CHIPID_ROUTER) {
	vector_read(0, 0, RR_DIAG_PARMS, &x);
	if ((x & P123_LLP8) == P123_LLP8)
	    mode = 8;
    }

    DPRINTF("llp_pfail_getmode: mode=%d\n", mode);

    return mode;
}

/*ARGSUSED*/
void llp_pfail_intr(void *arg)
{
    __uint64_t		sr0, sr1, x;
    cpuid_t		cpu = cpuid();
    cmoduleid_t		cm;
    module_t	       *m;
    int			mode;

    DPRINTF("llp_pfail_intr %d: start\n", cpuid());

    /*
     * Determine if this CPU is a module owner.
     * If so, m is set to the module; otherwise, m remains NULL.
     */

    m = 0;

    for (cm = 0; cm < NUMMODULES; cm++)
	if (cpu == msc_owner_cpu(modules[cm])) {
	    m = modules[cm];
	    break;
	}

    DPRINTF("llp_pfail_intr %d: m=%x\n", cpuid(), m);

    if (vector_read(0, 0, RR_STATUS_REV_ID, &sr0) < 0)
	sr0 = 0;		/* sr0 == 0 if local link is down */

    /*
     * NOTE: Must not reference memory on other nodes after the
     * 	     barrier, precluding printfs and most everything else.
     */

    mode = llp_pfail_mode;	/* Don't reference llp_pfail_mode later */

    llp_pfail_barrier();	/* Wait for all CPUs to get here */

    /*
     * Re-negotiate module's external links.  Delays are inserted to
     * produce the following approximate timeline, beginning as soon
     * as the barrier is passed.  Delay from 100 to 200 is so everyone
     * can have set DIAG_PARMS before anyone starts to reset any links.
     * We can't continue until link reset is complete, or an NI error
     * interrupt will occur.  Link reset takes as long as 1250 usec,
     * but only in worst case when link negotiation fails, which should
     * never happen.
     *
     * 0   100  200  300   ~1600  <== Time (usec)
     *
     * +--------------+           Non-owner delay
     * +----+                     Owner delay
     *      +                     Owner sets DIAG_PARMs
     *      +----+                Owner delay
     *           +                Owner resets links
     *           +----+           Owner delay
     *                +           Everyone clears error
     *                +------+    Everyone waits for error to clear
     */

    if (m == 0) {
	DELAY(300);
	goto done;
    }

    DELAY(100);

    if (CHIPID(sr0) == CHIPID_HUB && private.p_sn00) {
	/*
	 * Origin200 and directly connected device is a Hub.
	 * Adjust external network link mode.
	 * We also process the remote end, just in case.
	 */

	vector_read(0, 0, NI_DIAG_PARMS, &x);
	if (mode == 8)
	    x |= NDP_LLP8BITMODE;
	else
	    x &= ~NDP_LLP8BITMODE;
	vector_write(0, 0, NI_DIAG_PARMS, x);

	x = LOCAL_HUB_L(NI_DIAG_PARMS);
	if (mode == 8)
	    x |= NDP_LLP8BITMODE;
	else
	    x &= ~NDP_LLP8BITMODE;
	LOCAL_HUB_S(NI_DIAG_PARMS, x);

	DELAY(100);

	LOCAL_HUB_S(NI_PORT_RESET, NPR_LINKRESET);
    } else if (CHIPID(sr0) == CHIPID_ROUTER) {
	if (vector_read(6, 0, RR_STATUS_REV_ID, &sr1) < 0)
	    sr1 = 0;

	if (CHIPID(sr1) == CHIPID_ROUTER) {
	    vector_read(6, 0, RR_DIAG_PARMS, &x);
	    if (mode == 8)
		x |= P123_LLP8;
	    else
		x &= ~P123_LLP8;
	    vector_write(6, 0, RR_DIAG_PARMS, x);
	}

	vector_read(0, 0, RR_DIAG_PARMS, &x);
	if (mode == 8)
	    x |= P123_LLP8;
	else
	    x &= ~P123_LLP8;
	vector_write(0, 0, RR_DIAG_PARMS, x);

	DELAY(100);

	if (CHIPID(sr1) == CHIPID_ROUTER)
	    vector_write(6, 0, RR_PORT_RESET, P123_RESET);

	vector_write(0, 0, RR_PORT_RESET, P123_RESET);
    } else /* ...if not a router, may be SUMAC or vector read error */
	DELAY(100);

    DELAY(100);

done:

    /*
     * Clear NI port error.  Keep trying until LINK RESET error is gone.
     */

    while (LOCAL_HUB_L(NI_PORT_ERROR_CLEAR) & NPE_LINKRESET)
	;

    DPRINTF("llp_pfail_intr %d: done\n", cpuid());
}

/*
 * llp_pfail_switch
 *
 *   Quiesces system CPU activity by sending a high priority interrupt
 *   to every CPU so they go to llp_pfail_intr.
 *
 *   There, the owner CPU of each module negotiates all module-external
 *   links into 8-bit or 16-bit mode according to whether 'mode' is 8
 *   or 16.  The work must be done on a local CPU because it uses vector
 *   reads and writes, hence the high-priority interrupt scheme.
 *
 *   This routine locks at spl7 in case multiple CPUs get here on a
 *   powerfail interrupt, and to prevent the CPU that's calling this
 *   routine from taking the high priority interrupt until it's done
 *   sending all of them.
 */

void llp_pfail_switch(int mode)
{
    cpuid_t		cpu;
    int			s;

    s = mutex_spinlock_spl(&llp_pfail_lock, spl7);

    DPRINTF("llp_pfail_switch: cpu=%d mode=%d\n", cpuid(), mode);

    /* llp_pfail_mode is for passing mode to intr handler */

    llp_pfail_mode = mode;

    if (llp_pfail_getmode() != mode)
	for (cpu = 0; cpu < maxcpus; cpu++)
	    if (pdaindr[cpu].CpuId != -1) {
		DPRINTF("llp_pfail_switch: int %d\n", cpu);
		REMOTE_HUB_SEND_INTR(cputonasid(cpu),
				     INT_PEND1_BASELVL +
				     LLP_PFAIL_INTR_A + cputoslice(cpu));
	    }

    DPRINTF("llp_pfail_switch: cpu=%d done\n", cpuid());

    SYNCHRONIZE();	/* Make sure all hubs received their intrs */

    mutex_spinunlock(&llp_pfail_lock, s);
}

/*
 * The following are some routines for workaround testing that are
 * meant to be called via syssgi (only in a private version of syssgi.c).
 */

#if LLP_PFAIL_TEST_SYSSGI

void syssgi_pf8(void)		/* Switch all ext. links to 8-bit mode */
{
    llp_pfail_switch(8);
}

void syssgi_pf16(void)		/* Switch all ext. links to 16-bit mode */
{
    llp_pfail_switch(16);
}

#define PFMODE(sr, link)	(((sr) & RSRI_LINK8BIT(link)) ? 8 : 16)

void syssgi_pfshow(void)	/* Show port status (runon to target module) */
{
    __uint64_t		sr;

    printf("PFSHOW: cpu        = %d\n", cpuid());
    vector_read(0, 0, RR_STATUS_REV_ID, &sr);
    if (CHIPID(sr) == CHIPID_HUB) {
	printf("PFSHOW: local hub  = 0x%x\n", sr);
	printf("PFSHOW:              (%d)\n",
	       (sr & NSRI_8BITMODE_MASK) ? 8 : 16);
    } else {
	printf("PFSHOW: local rtr  = 0x%x\n", sr);
	printf("PFSHOW:              (%d, %d, %d, %d, %d, %d)\n",
	       PFMODE(sr, 1), PFMODE(sr, 2), PFMODE(sr, 3),
	       PFMODE(sr, 4), PFMODE(sr, 5), PFMODE(sr, 6));
	vector_read(6, 0, RR_STATUS_REV_ID, &sr);
	printf("PFSHOW: remote rtr = 0x%x\n", sr);
	printf("PFSHOW:              (%d, %d, %d, %d, %d, %d)\n",
	       PFMODE(sr, 1), PFMODE(sr, 2), PFMODE(sr, 3),
	       PFMODE(sr, 4), PFMODE(sr, 5), PFMODE(sr, 6));
    }
    sr = LOCAL_HUB_L(NI_STATUS_REV_ID);
    printf("PFSHOW: local ni   = 0x%x\n", sr);
    printf("PFSHOW:              (%d)\n",
	   (sr & NSRI_8BITMODE_MASK) ? 8 : 16);
}

static void showall(int vec)
{
    __uint64_t		x;
    int			i;

    vector_read(vec, 0, RR_STATUS_REV_ID, &x);
    printf("%d:rr_status_rev_id = 0x%x\n", vec, x);
    vector_read(vec, 0, RR_GLOBAL_PARMS, &x);
    printf("%d:rr_global_parms = 0x%x\n", vec, x);
    vector_read(vec, 0, RR_DIAG_PARMS, &x);
    printf("%d:rr_diag_parms = 0x%x\n", vec, x);
    for (i = 1; i <= 6; i++) {
	vector_read(vec, 0, RR_PORT_PARMS(i), &x);
	printf("%d:rr_port_parms(%d) = 0x%x\n", vec, i, x);
	vector_read(vec, 0, RR_STATUS_ERROR(i), &x);
	printf("%d:rr_status_error(%d) = 0x%x\n", vec, i, x);
    }
}

void syssgi_pfshowall(void)	/* Show more status (runon to target module) */
{
    showall(0);
    showall(6);
}

#endif /* LLP_PFAIL_TEST_SYSSGI */

/*
 * do_powerfail_wars
 *
 *   WORKAROUND 1 (PV #521950)
 *
 *   This is a workaround to a problem with LLP links.  When power is
 *   lost to one side of a link, the entire router connected to the
 *   other side is nearly 100% likely to hang until it is reset.
 *   This does not occur if the link is running in 8-bit mode.
 *
 *   This workaround negotiates all module external links (router links
 *   1, 2, and 3, or in the case of Origin200, the NI link) to 8-bit mode
 *   in the remaining milliseconds before power is lost, or before a
 *   controlled power down.
 *
 *   Links all go back to 16 bits on reset.  The IP27prom sets the
 *   external links back to 8 bits as soon as possible, so that the
 *   IP27prom and IO6prom run completely in 8-bit mode.  This handles
 *   the common case where a module is shut off while in the PROM.
 *   The kernel sets all links back to 16 bits in sysctlr_init.
 *
 *   Notes: 8-bit mode is unsupported.  Links get about 3600 SN
 *   errors per minute while running the kernel in 8-bit mode.
 *   This workaround is only performed (by PROM and kernel) in a
 *   partitioned system (at least one non-zero partition ID).
 *
 *   WORKAROUND 2
 *
 *   This is a workaround to a little-understood hardware problem.
 *
 *   The hardware problem is that when a module unexpectedly loses
 *   power, disk writes in progress continue and eventually write
 *   bad data (like 2k of zeroes) to the disk.  This is fatal to
 *   database qualification tests, which require graceful power
 *   failure recovery.
 *
 *   The solution is to detect the system controller power failure
 *   interrupt.  If it occurs, we immediately send a warm reset down
 *   the Hub II LLP on every node in the partition.  This has been shown
 *   to propagate, reset all PCI devices in the partition, and prevent
 *   the bad data from making it to the disk.
 *
 *   We have about 5 milliseconds after the power failure actually
 *   occurs.  It takes about 100 usec for the system controller to
 *   send the interrupt, and about 1 msec to read the interrupt
 *   reason.
 */

static void do_powerfail_wars(void)
{
    cnodeid_t		node;
    nasid_t		nasid;
    static int		wars_done	= 0;

    if (atomicAddInt(&wars_done, 1) != 1)
	return;

    /*
     * Negotiate links to 8-bit mode BEFORE resetting the I/O.
     * This is necessary because the I/O reset results in an IO error
     * interrupt, which takes so long it doesn't return before the
     * power goes off (it probably tries panicking).
     */

    if (powerfail_routerwar)
	llp_pfail_switch(8);

    for (node = 0; node < numnodes; node++) {
	nasid = COMPACT_TO_NASID_NODEID(node);
	REMOTE_HUB_S(nasid, IIO_LLP_CSR,
		     REMOTE_HUB_L(nasid, IIO_LLP_CSR) | 0x100);
	REMOTE_HUB_S(nasid, MD_LED0, 0);	/* All LEDs on */
    }
}

/* 
 * do_reset_wars
 *
 *  FIX (PV 572324)
 * 
 * Reset can cause lots of messages in flight to end up in a bad state. 
 * Eventually we should be able to recover from all of them, but this
 * fix goes a LONG way to making sure that a message either gets through 
 * completely or gets dropped. This is done by "zapping" all of the 
 * routing entires in the local partitions NIs to remote partitions. 
 */

static void do_reset_wars(void)
{
    nasid_t	n;
    cnodeid_t	cn;
    partid_t	pid = part_id();

    if (pid <= 0) {
	return;
    }

    for (n = 0; n < MAX_NASIDS; n++) {
	if (nasid_to_partid(n) != pid) {
	    for (cn = 0; cn < maxnodes; cn++) {
		REMOTE_HUB_S(COMPACT_TO_NASID_NODEID(cn), 
			     NI_LOCAL_TABLE((n & 0xf)), 0xf);
		REMOTE_HUB_S(COMPACT_TO_NASID_NODEID(cn), 
			     NI_META_TABLE(((n >> 4) & 0xf)), 0xf);
	    }
	}
    }
}    

/*
 * msc_message
 *
 *   Iterates through all modules in the partition and displays a string.
 *   If 'acp' is true, the string is sent to the serial (alt. console)
 *   port.  Otherwise, it is sent to the 8-character LED display.
 *   If 'sdonly' is true, it only iterates through modules for which
 *   m->shutdown is true.
 */

static void msc_message(char *mesg, int acp, int sdonly)
{
    cmoduleid_t		cm;
    module_t	       *m;
    int			s;

    for (cm = 0; cm < NUMMODULES; cm++) {
	m = modules[cm];

	if (! sdonly || m->shutdown) {
	    DPRINTF("msc_message: m%d acp=%d mesg=%s\n", m->id, acp, mesg);

	    s = mutex_spinlock_spl(&m->elsclock, spl7);

	    if (acp) {
		_elscuart_puts(&m->elsc, mesg);
		_elscuart_flush(&m->elsc);
	    } else
		elsc_display_mesg(&m->elsc, mesg);

	    mutex_spinunlock(&m->elsclock, s);
	}
    }
}

/*
 * msc_extend
 *
 *   Iterates through all modules in the partition and extends the shutdown
 *   time to 'sec' seconds by issuing a "pwr d xxx" command.  If 'sec' is
 *   zero, the partition powers down immediately.  If 'sdonly' is true, it
 *   only iterates through modules for which m->shutdown is true.
 */

static void msc_extend(int sec, int sdonly)
{
    cmoduleid_t		cm;
    module_t	       *m;

    DPRINTF("msc_extend: sec=%d\n", sec);

    for (cm = 0; cm < NUMMODULES; cm++) {
	m = modules[cm];

	if (! sdonly || m->shutdown) {
	    int			r, s;

	    DPRINTF("msc_extend: m%d sec=%d\n", m->id, sec);

	    s = mutex_spinlock_spl(&m->elsclock, spl7);
	    r = elsc_power_down(&m->elsc, sec);
	    mutex_spinunlock(&m->elsclock, s);

	    if (r < 0)
		cmn_err(CE_WARN,
			"module %d MSC: Unable to delay shutdown: %s",
			m->id, elsc_errmsg(r));

	}
    }

    DPRINTF("msc_extend: done\n");
}

/*
 * msc_heartbeat
 */

static void msc_heartbeat(module_t *m)
{
    DPRINTF("msc_heartbeat: m%d\n", m->id);

    setmustrun(master_procid);

    while (1) {
#if 0
	DPRINTF("msc_heartbeat: m%d keepalive\n", m->id);
#endif

	sysctlr_keepalive();

	delay(HZ * SEND_INTERVAL);
    }
}

/*
 * msc_shutdown
 *
 *   A thread created to handle system shutdown.
 */

static char sd_msg[] =
    "Shutting\000"
    "  Down  \000"
    " Please \000"
    "  Wait  \000"
    "        \000"
    ;

/*ARGSUSED*/
static void msc_shutdown(module_t *m)
{
    char	       *sd_pos;
    int			i;

    if (powerfail_routerwar)
	llp_pfail_switch(8);

    /*
     * Wait for up to 1.5 seconds for sysctlr interrupts from all the
     * modules to be processed.  This must be done because the msc_extend
     * we're about to perform would destroy other modules' interrupt
     * messages.  The msc_extend must be done within 3 seconds.
     */

    for (i = 0; i < 15; i++)
	if (intrs_pending)
	    delay(HZ / 10);

    /*
     * Set the power-off flag and send a signal to init to cause the
     * equivalent of an "init 0".
     */

    power_off_flag = 1;

    if (sigtopid(INIT_PID, SIGINT, SIG_ISKERN, 0, 0, 0))
	sysctlr_power_off(0);

    msc_extend(8, 1);
    msc_message("System is shutting down.\n", 1, 1);

    /*
     * Set up a max shutdown time in case init wedges.
     * Rotate a shutdown message on the MSC display(s), and continue
     * to extend the shutdown time in small increments.
     */

    timeout(sysctlr_power_off, INT_TO_PTR(1), clean_shutdown_time * HZ);

    for (;;) {
	msc_extend(8, 1);
	delay(HZ);	/* Pause to allow time for MSC "PWR DN" message */

	for (sd_pos = sd_msg; *sd_pos; sd_pos += 9) {
	    msc_message(sd_pos, 0, 0);
	    delay(HZ);
	}
    }
}

/*
 * process_message
 */

static void process_message(module_t *m)
{
    char		buf[80];
    int			i, s, msg, cm;
    module_t 		*slave;

    DPRINTF("process_message: m%d cpu=%d\n", m->id, cpuid());

    /*
     * Read the message associated with the interrupt
     */

    s = mutex_spinlock_spl(&m->elsclock, spl7);

    elsc_process(&m->elsc);

    if ((i = elsc_msg_check(&m->elsc, buf, sizeof (buf))) <= 0) {
	if (i < 0)
	    cmn_err(CE_WARN,
		    "module %d MSC: Error reading message: %s",
		    m->id, elsc_errmsg(i));
	buf[0] = 0;	/* MSG_NONE */
    }

    mutex_spinunlock(&m->elsclock, s);

    /*
     * Interpret message
     */

    for (msg = 0; msgs[msg].msg; msg++)
	if (strcmp(msgs[msg].msg, buf) == 0)
	    break;

    DPRINTF("process_message: m%d msg=%d\n", m->id, msg);

    if (msgs[msg].msg == 0) {
	cmn_err(CE_WARN,
		"module %d MSC: Ignored unknown message <%s>",
		m->id, buf);
	goto done;
    }

    /*
     * Some older system controllers send lots of false fan temp
     * messages.  We won't report these if powerfail_cleanio is turned
     * on, because then check_too_many_intrs doesn't apply and they
     * would fill up SYSLOG.
     */

    if (powerfail_cleanio && msg == MSG_FAN_TEMP)
	goto done;

    /*
     * With some MSCs, "pwr d" commands from the Hub result in additional
     * sysctlr interrupts.  Ignore these if we're already shutting down.
     */

    if (shutdown_started && msg == MSG_PD_REMOTE)
	goto done;

    switch (msgs[msg].action) {
    case ACT_IGNORE:
	goto done;
    case ACT_PFAIL:
	do_powerfail_wars();
	break;
    case ACT_RST:
	do_powerfail_wars();
	do_reset_wars();
	/* FALLTHROUGH */
    case ACT_WARN:
	cmn_err(CE_WARN, "module %d MSC: %s",
		m->id, msgs[msg].desc);
	break;
    case ACT_SLAVE_RPSOK:
	for (cm = 0; cm < nummodules; cm++) {
	    slave = modules[cm];
	    if (slave->id != m->id)
		break;
	}
	klhwg_update_rps(msc_owner_cnode(slave), 1);
	cmn_err(CE_WARN, "module %d MSC: %s",
		slave->id, msgs[msg].desc);
	break;
    case ACT_RPSOK:
	klhwg_update_rps(msc_owner_cnode(m), 1);
	cmn_err(CE_WARN, "module %d MSC: %s",
		m->id, msgs[msg].desc);
	break;
    case ACT_SLAVE_RPSFAIL:
	for (cm = 0; cm < nummodules; cm++) {
	    slave = modules[cm];
	    if (slave->id != m->id)
		break;
	}
	klhwg_update_rps(msc_owner_cnode(slave), 0);
	cmn_err(CE_ALERT | CE_MAINTENANCE, "module %d MSC: %s",
		slave->id, msgs[msg].desc);
	break;
    case ACT_RPSFAIL:
	klhwg_update_rps(msc_owner_cnode(m), 0);
	/* FALLTHROUGH */
    case ACT_MAINT:
	cmn_err(CE_ALERT | CE_MAINTENANCE, "module %d MSC: %s",
		m->id, msgs[msg].desc);
	break;
    case ACT_SHUTDN:
	cmn_err(CE_WARN, "module %d MSC: %s (shutdown pending)",
		m->id, msgs[msg].desc);

	m->shutdown = 1;

	if (! shutdown_started) {
	    extern int msc_shutdown_pri;

	    shutdown_started = 1;
	    sthread_create("msc_shutdown", 0, 4096, 0,
			   msc_shutdown_pri, KT_PS,
			   (st_func_t *) msc_shutdown,
			   (void *) m, 0, 0, 0);
	}

	break;
    }
done:

    DPRINTF("process_message: m%d done\n", m->id);

    return;
}

/*
 * message_intr
 *
 *   This thread always runs on the master CPU.  It is kicked off when
 *   the high-priority msc_intr handler sends a low-priority interrupt
 *   to the master CPU.
 *
 *   This routine goes to spl7 for extended periods (2 msec), something
 *   the I2C driver must do because it uses a timing-based protocol.
 */

/*ARGSUSED*/
static void message_intr(void *arg)
{
    cmoduleid_t		cm;
    module_t	       *m;

    DPRINTF("message_intr: cpu=%d\n", cpuid());

    ASSERT(cpuid() == master_procid);

    /*
     * Loop as long as any module has a message pending.
     */

    do
	for (cm = 0; cm < NUMMODULES; cm++) {
	    m = modules[cm];

	    if (m->mesgpend) {
		atomicAddInt(&m->mesgpend, -1);
		atomicAddInt(&intrs_pending, -1);

		process_message(m);

		break;
	    }
	}
    while (cm < NUMMODULES);

    DPRINTF("message_intr: cpu=%d done\n", cpuid());
}

/*
 * msc_intr_unblock
 *
 *  Unblocks interrupt monitoring after a delay.
 */

void msc_intr_unblock(module_t *m)
{
#if 0
    if(m->disable_alert == 0)
        cmn_err(CE_ALERT,
        "module %d MSC: Interrupt monitoring re-enabled",
	    m->id);
#endif
    m->disable_alert = 1;
    intr_unblock_bit(msc_owner_cpu(m),
	   INT_PEND1_BASELVL + MSC_PANIC_INTR);
}

/*
 * sysctlr_check_reenable
 * 
 * checks if it is time to reenable the interrupts
 */

void sysctlr_check_reenable(void)
{
    module_t *m;

    m = nodepda->module;
    if(cpuid() != msc_owner_cpu(m))
	return;
    if(m->count_down == 0)
	return;   /* timer not enabled */
    
    m->count_down--;
    if(m->count_down == 0)
	msc_intr_unblock(m);
}

/*
 * msc_intr
 *
 *   Every time a system controller interrupt occurs, one CPU in the
 *   module will arrive here, record the interrupt, and send off a
 *   MSC_MESG_INTR to the master CPU.  This starts an interrupt thread
 *   on the master CPU that processes the interrupt(s).
 *
 *   This is done because the system controller interrupt is at such
 *   a high priority that very few kernel structures can be touched.
 *   Also, the high-priority interrupt lasts a very short time so it
 *   doesn't impact real-time response.
 */

/*ARGSUSED*/
static void msc_intr(void *arg)
{
    DPRINTF("msc_intr: cpu=%d\n", cpuid());

    if (! ignore_sysctlr_intr) {
	module_t	       *m;
	int			i, numrecent;

	m = module_lookup(nodepda->module_id);

	ASSERT(m);

	/*
	 * Check if MSC is sending so many interrupts it's crippling to
	 * the system.  This is especially true of some old MSCs with a
	 * noise problem.  We disable the sysctlr interrupt for an MSC
	 * that sends more than MODULE_HIST_CNT interrupts in a single
	 * minute.  Note:  This must not be done if powerfail_cleanio is
	 * true.  Database app machines must have working MSC.
	 */

	m->intrhist[m->histptr] = time;
	m->histptr = (m->histptr + 1) % MODULE_HIST_CNT;

	for (i = numrecent = 0; i < MODULE_HIST_CNT; i++)
	    if (time < m->intrhist[i] + 60)
		numrecent++;

	if (numrecent == MODULE_HIST_CNT && ! powerfail_cleanio) {
	    if(m->disable_alert)
		;
	    else {
	    	cmn_err(CE_ALERT | CE_MAINTENANCE,
		    "module %d MSC: Too many over temperature interrupts",
		    m->id);
	    	cmn_err(CE_ALERT | CE_MAINTENANCE,
		    "module %d MSC: Switching to deferred monitoring mode",
		    m->id);

	    }

	    intr_block_bit(msc_owner_cpu(m),
			   INT_PEND1_BASELVL + MSC_PANIC_INTR);
	    m->count_down = intr_revive_delay * HZ;
	}

	/*
	 * Register the interrupt for future processing.
	 */

	atomicAddInt(&m->mesgpend, 1);
	atomicAddInt(&intrs_pending, 1);

	DPRINTF("msc_intr: m%d sending\n", m->id);

	REMOTE_HUB_SEND_INTR(cputonasid(master_procid),
			     INT_PEND0_BASELVL + MSC_MESG_INTR);
    }
}

/*
 * sysctlr_rps_query
 *
 *   Searches for presence of RPS 
 */

void sysctlr_rps_query(elsc_t *e, cnodeid_t cnode)
{
	vertex_hdl_t node_vertex;
	int r;
        if(cnode == CNODEID_NONE)
		return;                                                        
	node_vertex = cnodeid_to_vertex(cnode);
	r = elsc_rpwr_query(e, (cnode == cputocnode(master_procid)));

#ifdef	MSC_RPS_DEBUG
	cmn_err(CE_WARN, "elsc_rpwr_query returned %d for cnode %d\n", r, cnode);
#endif	/* MSC_RPS_DEBUG */
	if( r >= 0)
		klhwg_add_rps(node_vertex, cnode, r);
}

/*
 * sysctlr_init
 *
 *   Sets up all threads and interrupt vectors for MSC handling.
 */

void sysctlr_init(void)
{
    cmoduleid_t		cm;
    module_t	       *m;
    extern int		msc_heartbeat_pri;
    int			cpu;

    /*
     * Interrupts can be completely ignored for emergency cases.
     */

    DPRINTF("sysctlr_init: cpu=%d\n", cpuid());

    if (ignore_sysctlr_intr)
	return;


    for (cm = 0; cm < nummodules; cm++) {
	m = modules[cm];
	sysctlr_rps_query(&m->elsc, msc_owner_cnode(m));
    }
    /*
     * Enable sysctlr interrupt for one CPU in each module.
     */

    for (cm = 0; cm < NUMMODULES; cm++) {
	m = modules[cm];
	cpu = msc_owner_cpu(m);

	DPRINTF("sysctlr_init: m%d connect %d\n", m->id, cpu);

	m->disable_alert = 0;
	m->count_down = 0;

	intr_connect_level(cpu,
			   INT_PEND1_BASELVL + MSC_PANIC_INTR,
			   INTR_SWLEVEL_NOTHREAD_DEFAULT,
			   msc_intr, 0, NULL);
    }

    /*
     * Enable power-fail LLP reset intr for all CPUs
     */

    for (cpu = 0; cpu < maxcpus; cpu++)
	if (pdaindr[cpu].CpuId != -1)
	    intr_connect_level(cpu,
			       INT_PEND1_BASELVL +
			       LLP_PFAIL_INTR_A + cputoslice(cpu),
			       INTR_SWLEVEL_NOTHREAD_DEFAULT,
			       llp_pfail_intr, 0, NULL);

    spinlock_init(&llp_pfail_lock, "modulelock");

    /*
     * Message interrupt thread will always run on master processor.
     */

    intr_connect_level(master_procid,
		       INT_PEND0_BASELVL + MSC_MESG_INTR,
		       SD_THREAD_PRI,
		       message_intr, 0, NULL);

    sthread_create("msc_heartbeat", 0, 4096, 0,
		   msc_heartbeat_pri, KT_PS,
		   (st_func_t *) msc_heartbeat,
		   (void *) modules[0], 0, 0, 0);

    /*
     * Switch external ports to 16-bit mode at this point.
     * The switch routine does nothing if the mode is already 16-bit.
     */

    llp_pfail_switch(16);

    DPRINTF("sysctlr_init: done\n");
}

/*
 * sysctlr_power_off
 *
 *   Powers off all modules in the partition, except if 'sdonly' is true,
 *   it powers off only modules for which m->shutdown is true.
 */

void sysctlr_power_off(int sdonly)
{
    /*
     * Powers off all modules marked m->shutdown.
     */

    if (powerfail_routerwar)
	llp_pfail_switch(8);

    msc_message("PowerOff", 0, sdonly);
    msc_message("Removing power.\n", 1, sdonly);
    msc_extend(1, sdonly);

    for (;;)
	;

    /*NOTREACHED*/
}

/*
 * sysctlr_keepalive
 *
 *   Do the right thing for heartbeats:
 *
 *   - If heartbeat_enable becomes set, activate heartbeats to master MSC.
 *   - If heartbeat_enable becomes clear, deactivate heartbeats to master MSC.
 *   - If heartbeats are already active, send a heartbeat to master MSC.
 *
 *   This should be called periodically from the main heartbeat thread,
 *   and also from elsewhere in the kernel when the heartbeat thread may
 *   not be running, such as while dumping.
 *
 *   This can be called as often as desired, since it only sends a
 *   heartbeat if it's been long enough since the last one was sent.
 */

void sysctlr_keepalive(void)
{
    module_t	       *m;
    int			r, s;
    __uint64_t		tm	= TIME;
    int			h_enable, h_disable, h_send;

    m = modules[0];

    h_enable  = (  heartbeat_enable && ! m->hbt_active);
    h_disable = (! heartbeat_enable &&	 m->hbt_active);
    h_send    = (! h_enable &&
		 ! h_disable &&
		 m->hbt_active &&
#if LDEBUG
		 ! flatline &&
#endif
		 tm >= m->hbt_last + SEND_INTERVAL);

    if (h_enable) {
	if (kdebug) {
	    cmn_err(CE_NOTE,
		    "module %d MSC: Heartbeats not enabled "
		    "(debugger installed)", m->id);
	    heartbeat_enable = 0;
	    h_enable = 0;
	} else
	    cmn_err(CE_NOTE,
		    "module %d MSC: Enabling heartbeats", m->id);
    } else if (h_disable)
	cmn_err(CE_NOTE,
		"module %d MSC: Disabling heartbeats", m->id);

#if LDEBUG
    if (h_send)
	printf("module %d MSC: Sending heartbeat\n", m->id);
#endif

    if (! (h_enable | h_disable | h_send))
	return;

    s = mutex_spinlock_spl(&m->elsclock, spl7);

    if (h_enable) {
	r = elsc_hbt_enable(&m->elsc, RECV_INTERVAL, NMI_RST_DELAY);
	if (r >= 0)
	    m->hbt_active = 1;
    } else if (h_disable) {
	r = elsc_hbt_disable(&m->elsc);
	if (r >= 0)
	    m->hbt_active = 0;
    } else if (h_send) {
	r = elsc_hbt_send(&m->elsc);
	if (r >= 0)
	    m->hbt_last = TIME;
    } else
	r = 0;

    mutex_spinunlock(&m->elsclock, s);

    if (r < 0) {
	cmn_err(CE_WARN,
		"module %d MSC: Error %s heartbeat: %s",
		m->id,
		h_enable ? "enabling" :
		h_disable ? "disabling" : "sending",
		elsc_errmsg(r));

	/*
	 * There is an unfortunate bug in early MSCs where the "hbt"
	 * command is refused unless MSC security is defeated.
	 */

	if (r == ELSC_ERROR_RESP_FORMAT)
	    cmn_err(CE_WARN | CE_CONFIGERROR,
		    "module %d MSC: Keyswitch must be in diagnostic "
		    "position for heartbeats", m->id);

	if (h_enable)
	    heartbeat_enable = 0;		/* Give up turning on */
    }
}

/*======================================================================
 *		XBOX SYSTEM CONTROLLER SUPPORT
 *======================================================================
 */
/*
 * The following code provides support to deal with the xbox system
 * controller.
 */

extern int   xbox_sysctlr_poll_interval;	/* Time in seconds
						 * between successive
						 * xbox sysctlr status polls
						 */
extern int   clean_shutdown_time;               /* Time in seconds
                                                 * needed to do a clean
                                                 * shutdown.
                                                 */
extern int   ignore_xsc;                        /* xbox system controller
                                                 * override
                                                 */
/*-----------------------------------------------------------------------
 *	Private declarations
 *-----------------------------------------------------------------------
 */
#define XBOX_POLL_THREAD_PRI	40	/* priority of the xbox system
					 * controller polling thread
					 */

static void             xbox_sysctlr_poll(void *, void *);
static void             xbox_sysctlr_status_check(uint16_t *, nasid_t);
static void             xbox_sysctlr_system_shutdown(void);
static void             xbox_sysctlr_nasid_init(nasid_t);
static int              shutdown_pending = 0;

static elsc_t		e;
static int 		oldstatus[MAX_NASIDS];
static int		mfgmode_printed = 0;

/* xbox system controller event flags */

#define E_IGNORE	(1 << 0)	/* Ignore the event */
#define E_SHUTDOWN	(1 << 1)	/* Shut down the system on this event
					 */

typedef struct
{
	char	*desc;
	int	event_flag;
} xbox_sysctlr_event_t;

xbox_sysctlr_event_t xbox_event[] = {
#define EVENT_BLOWER_FAIL       0
	{ "Gigachannel blower failure", E_SHUTDOWN },

#define EVENT_UNUSED1           1
	{ "Gigachannel unknown system controller event", E_IGNORE },

#define EVENT_UNUSED2           2
	{ "Gigachannel unknown system controller event", E_IGNORE},

#define EVENT_OVER_TEMP		3
	{ "Power supply over temperature", E_SHUTDOWN},

#define EVENT_RPS_FAIL		4
	{ "Backup power supply kicked in - redundancy lost", E_IGNORE},

#define EVENT_MFG_MODE		5
	{ "Manufacturing mode",	E_IGNORE}

};

/*
 * force_system_shutdown
 *      Shutdown the system unconditionally and immediately.
 */
void
force_system_shutdown(void)
{
        cmn_err(CE_WARN,"Powering off the system.\n");
        DELAY(500000);  /* give them 1/2 second to see it */
        if (elsc_system_reset(&e) < 0) {

                /* Command did not complete correctly.
                 * Force a warm reset
                 */
                printf("MSC Reset command failed.Doing warm reset\n");
                us_delay(1000000);  /* wait a sec.. */
                LOCAL_HUB_S(NI_PORT_RESET, NPR_PORTRESET | NPR_LOCALRESET);
        }

}

/*
 * xbox_sysctlr_system_shutdown
 *	Reset the entire system
 */
void
xbox_sysctlr_system_shutdown(void)
{

        cmn_err(CE_CONT,"Shutting down the system\n");

        /* This is the equivalent of doing a "init 0" */
	sigtopid(INIT_PID, SIGINT, SIG_ISKERN, 0, 0, 0);

        /* If the init wedges timeout and do a forcible powerdown */
        timeout(force_system_shutdown,(void *)0,clean_shutdown_time * HZ);
}
/*
 * xbox_sysctlr_status_check
 *	Read the system controller status port and check for
 *	failures.
 */
void
xbox_sysctlr_status_check(uint16_t *sysctlr_status, nasid_t nasid)
{
        uint16_t        status;
        int             event;
        kthread_t       *kt = curthreadp;
	sthread_t	*st;
        char            xsc_name[40];

        /* This is the default name of ths sthread used in status messages */
        sprintf(xsc_name,"xscd");
        if (kt && KT_ISSTHREAD(kt)) {
		st = KT_TO_ST(kt);
		if (st->st_name)
			sprintf(xsc_name,"%s",st->st_name); 
	}

        /* Read the xbox system controller status */
        status = *sysctlr_status;
        status &= 0xff; /* Mask of the upper byte */
	
        DPRINTF("xbox_sysctlr_status_check: status(0x%llx) = 0x%x\n",
               sysctlr_status,status);

        /* Check if the status indicates any kind of faliures */
        for (event = EVENT_BLOWER_FAIL ; event <= EVENT_MFG_MODE; event++)
                if (status & (1 << event)) {
			/* update kernel information on RPS status. 
			 * status bit == 0 means both power supplies OK
			 */
			if ((event == EVENT_RPS_FAIL) && 
			    ((status & (1 << event)) != oldstatus[nasid])) {
			    klhwg_update_xbox_rps(NASID_TO_COMPACT_NODEID(nasid), !(status & (1 << event)));
			    oldstatus[nasid] = (status & (1 << event));
			    cmn_err((status & (1 << event)) ? 
				    (CE_ALERT | CE_MAINTENANCE) : CE_WARN,
				    "Gigachannel System Controller[%s]: %s\n",
				    xsc_name,xbox_event[event].desc);
			} else if (event == EVENT_MFG_MODE) {
			    if (!mfgmode_printed) {
				cmn_err(CE_WARN,
				    "Gigachannel System Controller[%s]: %s\n",
				    xsc_name,xbox_event[event].desc);
				mfgmode_printed = 1;
			    }
			} else if (event != EVENT_RPS_FAIL)
			    cmn_err(CE_WARN,
				    "Gigachannel System Controller[%s]: %s\n",
				    xsc_name,xbox_event[event].desc);

                        /* If the status indicates a failure which
                         * necessitates a shutdown then reset the
                         * system.  Use shutdown pending to prevent
                         * multiple resets in the face of multiple
                         * failures.
                         */
                        if ((xbox_event[event].event_flag & E_SHUTDOWN) &&
                            !shutdown_pending) {
                                shutdown_pending = 1;
                                xbox_sysctlr_system_shutdown();
                        }

                }
}

/*
 * xbox_sysctlr_poll
 *	This is the actual thread that polls the system controller
 *	status for the xbox.
 */
void
xbox_sysctlr_poll(void *status, void *nasid)
{
	for (;;) {
		/* wait for the poll interval seconds */
		delay(xbox_sysctlr_poll_interval * HZ);
		/* Read the sysctlr status port and check for
		 * any failures.
		 */
		xbox_sysctlr_status_check((uint16_t *)status, 
					  (nasid_t)(int64_t)nasid);
	}
}

/*
 * xbox_sysctlr_nasid_init
 *      Sets up the xbox system controller polling thread on
 *      corresponding to the xbox for this node.
 */
void
xbox_sysctlr_nasid_init(nasid_t nasid)
{
        char            poll_thread_name[30];
        uint16_t        *xbox_sysctlr_status_addr;
	extern int	xbox_poll_pri;


        /* Start this only on a speedo with an xbox */
        if (nasid == INVALID_NASID)
                return;

        /* Initialize the status address */
        xbox_sysctlr_status_addr =
                (uint16_t *)(NODE_SWIN_BASE(nasid,XBOX_BRIDGE_WID) +
                                 FLASH_PROM1_BASE);
	/* Create the name for the xbox polling thread */
        sprintf(poll_thread_name,"xscd%d",nasid+1);
        /* Create the xbox system controller status polling thread */
        sthread_create(poll_thread_name, 0, 4096, 0,
                       xbox_poll_pri, KT_PS ,
                       (st_func_t *)xbox_sysctlr_poll,
                       (void *)xbox_sysctlr_status_addr, 
		       (void *)(int64_t)nasid, 0, 0);
}

/*
 * xbox_sysctlr_rps_query
 *
 *   Searches for presence of RPS
 */

void xbox_sysctlr_rps_query(cnodeid_t cnode)
{
    vertex_hdl_t node_vertex;
    nasid_t nasid;
    uint16_t status;

    if(cnode == CNODEID_NONE)
	return;

    nasid = COMPACT_TO_NASID_NODEID(cnode);
    if (nasid == INVALID_NASID)
	return;

    node_vertex = cnodeid_to_vertex(cnode);

    status = *(uint16_t*)((NODE_SWIN_BASE(nasid,XBOX_BRIDGE_WID) 
			  + FLASH_PROM1_BASE));

    if (status & XBOX_RPS_EXISTS)
	klhwg_add_xbox_rps(node_vertex, cnode, 
			   !(status & XBOX_RPS_FAIL));
    oldstatus[nasid] = (status & XBOX_RPS_FAIL);
}

/*
 * xbox_sysctlr_init
 *      Sets up the thread which periodically monitors the
 *      xbox system controller status to check for any failures.
 */
void
xbox_sysctlr_init(void)
{
        extern  int     numnodes;
        cnodeid_t       cnode;
        nasid_t         nasid;

        /* There is no xbox for Origin 2000s */
        if (!SN00)
                return;

        /* Xbox system controller override turned on */
        if (ignore_xsc)
                return;

	elsc_init(&e,xbox_nasid_get());

        /* Iterate through all the modules/nodes in the O200 */
        for (cnode = 0; cnode < numnodes ; cnode++) {
                /* Check if there is an xbox connected to this node/module
                 * in O200.
                 */
                nasid = COMPACT_TO_NASID_NODEID(cnode);
                if (nasid == INVALID_NASID)
                        continue;
                if (!is_xbox_config(nasid))
                        continue;

		/* Check if this xbox has a redundant power supply */
	 	xbox_sysctlr_rps_query(cnode);

                /* Start off the xbox system controller status polling
                 * thread.
                 */
                xbox_sysctlr_nasid_init(nasid);
        }

}
