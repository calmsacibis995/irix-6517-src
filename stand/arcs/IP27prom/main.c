/***********************************************************************\
 *	File:		main.c					       *
 *								       *
 *	Continuation of the boot process once the stack has been set   *
 *	up and we are running in C.  				       *
 *								       *
\***********************************************************************/

#define TRACE_FILE_NO		1

#include <sys/types.h>
#include <sys/nic.h>
#include <sys/sbd.h>
#include <sys/SN/arch.h>
#include <sys/SN/agent.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/gda.h>
#include <sys/SN/nvram.h>
#include <sys/SN/promhdr.h>
#include <sys/SN/launch.h>
#include <sys/SN/nmi.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/addrs.h>

#include <sys/PCI/bridge.h>	/* For IO6 fprom */

#include <libkl.h>
#include <ksys/elsc.h>
#include <rtc.h>
#include <hub.h>
#include <report.h>
#include <prom_msgs.h>
#include <klcfg_router.h>

#include "ip27prom.h"
#include "bist.h"
#include "pod.h"
#include "rtr_diags.h"
#include "hub_diags.h"
#include "prom_leds.h"
#include "libasm.h"
#include "tlb.h"
#ifdef SABLE
#include "hubuart.h"
#endif
#include "ioc3uart.h"
#include "junkuart.h"
#include "mdir.h"
#include "mtest.h"
#include "memory.h"
#include "discover.h"
#include "partition.h"
#include "nasid.h"
#include "cache.h"
#include "segldr.h"
#include "router.h"
#include "prom_error.h"
#include "pvers.h"

#define IMMEDIATE_POD		0
#define TEST_LAUNCH		0
#define COPY_JUMP		1
#define SABLE_MEM_TEST		1
#define SABLE_MEM_INIT		1
#define DEBUG_NIC		0
#define DEBUG_NETWORK		0
#define SIMPLE_NASIDS		0
#define SIMPLE_ROUTES		0

#define	IP27_CPU_EARLY_INIT_CNT_MAX	1000
#define MAX_METAID MAX_ROUTERS/16

/*
 * During local arb, two scratch registers are needed to communicate
 * between the local CPUs.  These registers must be cleared to zero
 * on reset.  The PI_RT_COMPARE_A and PI_RT_COMPARE_B registers are
 * used for this purpose.
 */

#define DISCOVER_TIME_MAX	(120 * 1000000)

#ifdef SABLE
#define LOCAL_ARB_TIMEOUT	20000
#else
#define LOCAL_ARB_TIMEOUT	200000
#endif

#define ELSC_TOKEN_TIMEOUT	5000000

int _symmon = 0;	/* Senator, I know symmon and you are no symmon. */

__uint64_t	   diag_malloc_brk = DIAG_BASE;
int	   verbose;

/* these were used to determine where 512p boot was taking its sweet time.
   For production PROMs the calls to ATP and NTP are defined as nothing. */
#ifdef DEBUG_PROM_BOOT_TIME
void TP_common(char *s, int *tctr)
{
  unsigned long long ct = rtc_time();
  unsigned long long secs = ct / 1000000;
  unsigned long long usecs = ct % 1000000;
  unsigned long long tenths = usecs / 100000;
  unsigned long long minutes = secs / 60;

  printf("[%s]: time(%d) = %d:%02d.%d\n", s, *tctr, minutes, secs%60, tenths);
}

void ATP(char *s, int *tctr)
{
  TP_common(s, tctr);
  (*tctr)++;
}

void NTP(char *s, int *tctr)
{
  TP_common(s, tctr);
}
#else
#define ATP(s, tctr)
#define NTP(s, tctr)
#endif

extern void reset_system(void);
extern void encode_str_serial(const char *src, char *dest);
extern int elsc_mfg_nic_get(elsc_t *e, char *nic_info, int verbose);
extern int config_find_nic_router_all(pcfg_t *, nic_t, lboard_t **,
					klrou_t **, partid_t, int) ;
extern int hubless_rtr(pcfg_t *, pcfg_router_t *, int) ;

static void router_search_pcfg( pcfg_t *p, int i);
static int find_router_module(pcfg_t *p, pcfg_port_t *ppt);
static void pcfg_rou_mod_update(pcfg_t *p) ;
static void pcfg_memless_update(pcfg_t *p) ;
static void dump_in_elsc(lboard_t *l);
static void write_config_elsc(int pn_new);
static int read_midplane_mfgnic(void);
static void klconfig_disabled_update(pcfg_t *p, partid_t partition);
static void check_memory_regions(pcfg_t *p) ;
extern lboard_t * init_klcfg_ip27_disabled(__psunsigned_t hub_base, int flag, int good_nasid);

extern int router_test(int port_no, nasid_t nasid, net_vec_t vec);
extern void router_test_update(pcfg_t *p);
extern check_other_consoles(pcfg_t *p);
extern void elsc_nvram_init(nasid_t nasid, uchar_t *);
extern void elsc_nvram_copy(uchar_t *);
void ust_change(pcfg_t *p);
void Speedo(int);

static void xbox_nasid_set(pcfg_t *p);
/*
 * Define terminal I/O devices for libc
 */

static int nulluart(void)
{
    return 0;
}

libc_device_t dev_nulluart = {
    (void (*)(void *)) nulluart,
    nulluart,
    nulluart,
    nulluart,
    (int (*)(int)) nulluart,
    (int (*)(char *)) nulluart,
    0,
    0,
    "Null",
};

#ifdef SABLE

libc_device_t dev_hubuart = {
    hubuart_init,
    hubuart_poll,
    hubuart_readc,
    hubuart_getc,
    hubuart_putc,
    hubuart_puts,
    0,
    HUBUART_FLASH,
    "Hub",
};

libc_device_t dev_junkuart = {
    hubuart_init,
    hubuart_poll,
    hubuart_readc,
    hubuart_getc,
    hubuart_putc,
    hubuart_puts,
    0,
    HUBUART_FLASH,
    "Hub",
};

#else /* SABLE */

libc_device_t dev_junkuart = {
    junkuart_init,
    junkuart_poll,
    junkuart_readc,
    junkuart_getc,
    junkuart_putc,
    junkuart_puts,
    0,
    JUNKUART_FLASH,
    "Junk",
};

#endif /* SABLE */

libc_device_t dev_ioc3uart = {
    ioc3uart_init,
    ioc3uart_poll,
    ioc3uart_readc,
    ioc3uart_getc,
    ioc3uart_putc,
    ioc3uart_puts,
    0,
    IOC3UART_FLASH,
    "IOC3",
};

libc_device_t dev_elscuart = {
    elscuart_init,
    elscuart_poll,
    elscuart_readc,
    elscuart_getc,
    elscuart_putc,
    elscuart_puts,
    elscuart_flush,
    ELSCUART_FLASH,
    "MSC",
};

/*
 * init_kldir
 */

static void init_kldir(nasid_t nasid)
{
    kldir_ent_t	*ke;

    ke = KLD_LAUNCH(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_LAUNCH_OFFSET;
    ke->size   = IP27_LAUNCH_SIZE;
    ke->count  = IP27_LAUNCH_COUNT;
    ke->stride = IP27_LAUNCH_STRIDE;

    ke = KLD_NMI(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_NMI_OFFSET;
    ke->size   = IP27_NMI_SIZE;
    ke->count  = IP27_NMI_COUNT;
    ke->stride = IP27_NMI_STRIDE;

    ke = KLD_KLCONFIG(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_KLCONFIG_OFFSET;
    ke->size   = IP27_KLCONFIG_SIZE;
    ke->count  = IP27_KLCONFIG_COUNT;
    ke->stride = IP27_KLCONFIG_STRIDE;

    ke = KLD_PI_ERROR(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_PI_ERROR_OFFSET;
    ke->size   = IP27_PI_ERROR_SIZE;
    ke->count  = IP27_PI_ERROR_COUNT;
    ke->stride = IP27_PI_ERROR_STRIDE;

    ke = KLD_SYMMON_STK(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_SYMMON_STK_OFFSET;
    ke->size   = IP27_SYMMON_STK_SIZE;
    ke->count  = IP27_SYMMON_STK_COUNT;
    ke->stride = IP27_SYMMON_STK_STRIDE;

    ke = KLD_FREEMEM(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_FREEMEM_OFFSET;
    ke->size   = IP27_FREEMEM_SIZE;
    ke->count  = IP27_FREEMEM_COUNT;
    ke->stride = IP27_FREEMEM_STRIDE;
}

static void init_nmi(nasid_t nasid, int cpu)
{
    nmi_t	       *nmi;

    nmi			= (nmi_t *) NMI_ADDR(nasid, cpu);
    nmi->magic		= NMI_MAGIC;

    /* Make sure it's not a mapped address since we can't depend on tlb */

    nmi->call_addr	= (addr_t *) 0;		/* No handler */
    nmi->call_addr_c	= (addr_t *) (~(__psunsigned_t) nmi->call_addr);
    nmi->call_parm	= 0;
}

/*
 * arb_local_master
 *
 *   If only one CPU is present or responding, the other will be
 *   disabled to prevent it from mucking around.  hub_num_local_cpus()
 *   also relies on the dead processor being disabled.
 *
 *   If there is an alive peer, or no peer, returns 0.
 *   If there is a dead peer, returns its progress LED indicator PLED_*.
 */

static int arb_local_master(void)
{
    int		cpu_num;
    int		rc	= 0;
    int		single = 0;

    SET_PROGRESS(PLED_LOCALARB);

    cpu_num = hub_cpu_get();

    /*
     * If only one CPU is present, it becomes the master.
     */

    if (! LD(LOCAL_HUB(PI_CPU_PRESENT_A)) ||
	! LD(LOCAL_HUB(PI_CPU_PRESENT_B))) {

	single = 1;

    } else {
	int			i;
	volatile hubreg_t      *my_progress, *peer_progress;

	if (cpu_num == 0) {
	    my_progress	  = LOCAL_HUB(PI_RT_COMPARE_A);
	    peer_progress = LOCAL_HUB(PI_RT_COMPARE_B);
	} else {
	    my_progress	  = LOCAL_HUB(PI_RT_COMPARE_B);
	    peer_progress = LOCAL_HUB(PI_RT_COMPARE_A);
	}

	/*
	 * If our own progress indicator is screwed up,
	 * allow the other CPU to time out and become the master.
	 */

	if (LD(my_progress) != PLED_LOCALARB)
	    fled_die(FLED_HUBLOCAL, FLED_NOTTY);

	/*
	 * Wait in a timeout loop for the other processor to reach local
	 * arbitration.
	 */

	for (i = 0; i < LOCAL_ARB_TIMEOUT; i++)
	    if (LD(peer_progress) == PLED_LOCALARB)
		break;

	if (i == LOCAL_ARB_TIMEOUT) {
	    /*
	     * Timed out waiting for other CPU.  Become single master.
	     */

	    rc = LD(peer_progress);

	    single = 1;

	}

	/* If both CPUs make it here, CPU A will be the master */
    }

    /* Disable failed or missing CPU */

    if (single) {
	SD(LOCAL_HUB(cpu_num == 0 ? PI_CPU_ENABLE_B : PI_CPU_ENABLE_A), 0);
	hub_alive_set(cpu_num, 1);
    } else if (cpu_num == 0) {
	hub_alive_set(0, 1);		/* Non-atomic register */
	hub_alive_set(1, 1);
    } else
        rtc_sleep(1000);

    return rc;
}

/*
 * arb_master
 *
 *   The global master is chosen to be the node in the highest priority
 *   group with the lowest NIC.
 *
 *   The priorities are:
 *
 *   4. An SN00 with the master dip switch set.
 *   3. Nodes requesting to be GlobalMaster and having a console.
 *   2. Nodes requesting to be GlobalMaster only.
 *   1. Nodes with a console only.
 *   0. All other nodes.
 *
 *   The local master on the global master node is the global master CPU.
 *   The index of the global master is written into the promcfg header.
 *
 *   We only arbitrate the reset space.
 *
 *   if (arbitrate == GLOBAL_ARB) we arbitrate on a global basis
 *   else if (arbitrate == PARTITION_ARB) we arbirtrate on a partition basis
 */

#define	GLOBAL_ARB	1
#define	PARTITION_ARB	2

static void arb_master(int arbitrate)
{
    pcfg_t	       *p;
    pcfg_hub_t	       *ph1, *ph2;
    int			pri1, pri2;
    int			best, i;
    partid_t		my_partition;
    char		buf[16];

    p = PCFG(get_nasid());

    if (p->count < 2) {
	printf("No other nodes present; becoming %s master\n",
		((arbitrate == GLOBAL_ARB) ? "global" : "partition"));
	p->gmaster = p->pmaster = 0;
	goto arb_done;
    }

    my_partition = p->array[0].hub.partition;

    best = -1;
    if (SN00) {
	for (i = 0; i < p->count; i++)
	     if (p->array[i].hub.slot == 1)
		  if (best == -1)
		       best = i;
		  else {
			  best = -1;
			  break;
		  }
    }
    if (best == -1) {
	 for (i = 0; i < p->count; i++)
	      if (p->array[i].any.type == PCFG_TYPE_HUB &&
		  !(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
                  IS_RESET_SPACE_HUB(&p->array[i].hub)) {

                      if ((arbitrate == PARTITION_ARB) &&
			  (p->array[i].hub.partition != my_partition))
                          	continue;

		      if (best < 0)
			   best = i;
		      else {
			      ph1 = &p->array[   i].hub;
			      ph2 = &p->array[best].hub;
			      
			      pri1 = ((ph1->flags & PCFG_HUB_GMASTER ? 2 : 0) +
				      (ph1->flags & PCFG_HUB_BASEIO  ? 1 : 0));
			      pri2 = ((ph2->flags & PCFG_HUB_GMASTER ? 2 : 0) +
				      (ph2->flags & PCFG_HUB_BASEIO  ? 1 : 0));
			      
/* this used to be a test that was only for small systems, however,
   the large test should work on the small systems as well */
#ifndef SN0XXL
                              if (pri1 > pri2 || pri1 == pri2 && ph1->nic < ph2->nic)
				   best = i;
#else
                              /*
                               * Make global master default to lowest
                               * module and slot in the system or partition.
                               */
                              if (pri1 > pri2 ||
                                     (pri1 == pri2 &&
                                      (ph1->module > 0 &&
				       ph1->module < ph2->module ||
				       (ph1->module == ph2->module &&
                                       ph1->slot < ph2->slot)))) {
				   best = i;
			      }
#endif
		      }
	      }
    }

    ph1 = &p->array[best].hub;

    db_printf("%s master NIC is 0x%lx\n", 
	((arbitrate == GLOBAL_ARB) ? "Global" : "Partition"), ph1->nic);

    if (ph1->flags & PCFG_HUB_GMASTER)
	printf("*** %s master %S selected by GlobalMaster variable\n",
		((arbitrate == GLOBAL_ARB) ? "Global" : "Partition"),
		ph1->module, ph1->slot);

    if ((ph1->flags & PCFG_HUB_BASEIO) == 0)
	printf("*** %s master %S does not have a console\n",
		((arbitrate == GLOBAL_ARB) ? "Global" : "Partition"),
		ph1->module, ph1->slot);

    ((arbitrate == GLOBAL_ARB) ? (p->gmaster = best) : (p->pmaster = best));

 arb_done:
    if ((arbitrate == GLOBAL_ARB) && (p->gmaster == 0))
        hub_global_master_set(1);
    else if ((arbitrate == PARTITION_ARB) && (p->pmaster == 0)) {
        hub_partition_master_set(1);
	if (SN00) {
	    sprintf(buf, "%d", 1);
	    ip27log_setenv(get_nasid(), IP27LOG_MODULE_KEY, buf, 0);
	}
    }
}

#if SIMPLE_NASIDS
/*
 * simple_assign_nasids
 */

static int simple_assign_nasids(pcfg_t *p)
{
    int			i;
    nasid_t		nasid		= 0;

    for (i = 0; i < p->count; i++)
	if (p->array[i].any.type == PCFG_TYPE_HUB)
	    p->array[i].hub.nasid = nasid++;

    return 0;
}
#endif /* SIMPLE_NASIDS */

#if SIMPLE_ROUTES
/*
 * calculate_routes
 *
 *   Program the router tables using a simple algorithm that will
 *   result in optimal programming for small configurations only.
 */

static int calculate_routes(pcfg_t *p)
{
    int			src, dst;
    int			rc = 0;

    printf("Calculating router tables\n");

    for (src = 0; src < p->count; src++) {
#if 0
	pcfg_hub_t	*sh;
#endif

	if (p->array[src].any.type != PCFG_TYPE_HUB)
	    continue;

#if 0
	sh = &p->array[src].hub;
#endif

	/*
	 * The following loop used to go from src+1 to p->count-1 and
	 * use the return vectors.  Now it goes from 0 to p->count-1
	 * and doesn't use return vectors.  This causes the algorithm
	 * to perform "dimensional routing" which avoids deadlock on
	 * standard system configurations up to 32p.
	 */

	for (dst = 0; dst < p->count; dst++) {
	    pcfg_hub_t	       *dh;
	    net_vec_t		vec, retvec;

	    if (dst == src)
	      continue;

	    if (p->array[dst].any.type != PCFG_TYPE_HUB)
		continue;

	    dh = &p->array[dst].hub;

#if DEBUG_NETWORK
	    db_printf("From index %d, nic 0x%lx, nasid %d "
		      "to index %d, nic 0x%lx, nasid %d\n",
		      src, sh->nic, sh->nasid,
		      dst, dh->nic, dh->nasid);
#endif
	    if ((vec = discover_route(p, src, dst, 0)) == NET_VEC_BAD) {
		printf("No route available\n");
		rc = -1;
	    } else {
#if DEBUG_NETWORK
		db_printf("Outgoing route: 0x%llx\n", vec);
#endif

		discover_program(p, src, dh->nasid, vec, &retvec, 0);

#if DEBUG_NETWORK
		db_printf("Return route: 0x%llx\n", retvec);
#endif

		discover_program(p, dst, sh->nasid, retvec, 0, 0);
	    }
	}
    }

 done:
    return rc;
}
#endif /* SIMPLE_ROUTES */

/*
 * node_vec_write
 *
 *   Does a vector write to a node or router specified by PCFG index.
 */

static int node_vec_write(pcfg_t *p, int dst, __uint64_t address,
			  __uint64_t value)
{
    net_vec_t		vec;
    int			rc = 0;
    int			r;

    if ((vec = discover_route(p, 0, dst, 0)) == NET_VEC_BAD) {
	printf("Internal error: could not find route to dest %d\n", dst);
	rc = -1;
    } else if ((r = vector_write(vec, 0, address, value)) < 0) {
	printf("Vector write error: %s (dst=%d, vec=0x%lx, addr=0x%lx)\n",
	       net_errmsg(r), dst, vec, address);
	rc = -1;
    }

    return rc;
}

/*
 * node_vec_read
 *
 *   Does a vector read from a node or router specified by PCFG index.
 */

static int node_vec_read(pcfg_t *p, int dst, __uint64_t address,
			 __uint64_t *value_ptr)
{
    net_vec_t		vec;
    int			rc = 0;
    int			r;

    if ((vec = discover_route(p, 0, dst, 0)) == NET_VEC_BAD) {
	printf("Internal error: could not find route to dest %d\n", dst);
	rc = -1;
    } else if ((r = vector_read(vec, 0, address, value_ptr)) < 0) {
	printf("Vector read error: %s (dst=%d, vec=0x%lx, addr=0x%lx)\n",
	       net_errmsg(r), dst, vec, address);
	rc = -1;
    }

    return rc;
}

/* 
 * router_led_error
 *
 *   lights the swleds specified to indicate a problem.
 */

int router_led_error(pcfg_t *p, int index, char port_mask)
{
    __uint64_t	reg;

    /* RMW the status_rev register
     */
    if (node_vec_read(p, index, RR_STATUS_REV_ID, &reg) < 0)
	goto done;
    
    SET_FIELD(reg, RSRI_SWLED, port_mask); 

    if (node_vec_write(p, index, RR_STATUS_REV_ID, reg) < 0)
	goto done;

    return(0);

 done:
    return(-1);
}
/*
 * distribute_tables
 *
 *   Writes the full contents of all routing tables for all hubs and
 *   routers described in promcfg, using vector writes.
 *
 *   Also distributes the Region Size (coarse mode) bit.
 */

int distribute_tables(void)
{
    pcfg_t	       *p;
    int			ent;
    int			i, locked = 0;
    int			rc = -1;
    pcfg_hub_t	       *ph;
    pcfg_router_t      *pr;
    __uint64_t		reg;
    net_vec_t		vec;

    p = PCFG(get_nasid());

    db_printf("Distributing routing tables\n");

    ph = &p->array[0].hub;

    for (ent = 0; ent < NI_LOCAL_ENTRIES; ent++)
	SD(LOCAL_HUB(NI_LOCAL_TABLE(ent)), ph->htlocal[ent]);

    for (ent = 0; ent < NI_META_ENTRIES; ent++)
	SD(LOCAL_HUB(NI_META_TABLE(ent)), ph->htmeta[ent]);

    for (i = 1; i < p->count; i++) {
	switch (p->array[i].any.type) {
	case PCFG_TYPE_HUB:
	    ph = &p->array[i].hub;

	    db_printf("Set up Hub: index %d, nic 0x%lx, nasid %d\n",
		      i, ph->nic, ph->nasid);

	    for (ent = 0; ent < NI_LOCAL_ENTRIES; ent++)
		if (node_vec_write(p, i, NI_LOCAL_TABLE(ent),
				   (__uint64_t) ph->htlocal[ent]) < 0)
		    goto done;

	    for (ent = 0; ent < NI_META_ENTRIES; ent++)
		if (node_vec_write(p, i, NI_META_TABLE(ent),
				   (__uint64_t) ph->htmeta[ent]) < 0)
		    goto done;

	    break;

	case PCFG_TYPE_ROUTER:
	    pr = &p->array[i].router;

	    db_printf("Set up Router: index %d, nic 0x%lx, "
		      "meta %d, floc %d\n",
		      i,
		      pr->nic,
		      NASID_GET_META(pr->metaid),
		      pr->force_local);

	    /*
	     * Set up global parms. Change the AGE WRAP
	     */
	    if (node_vec_read(p, i, RR_GLOBAL_PARMS, &reg) < 0)
		goto done;

	    /* this is a hack to get 512p working - disabling Bypass Enable
	       in the core router arbitration logic seems to allow it to
	       work.  The number 64 was empirically observed - it allows
	       Bypass Enable on 128p, but disables it on anything greater
	       than that (failure *was* observed on 1/2 of a 512p with
	       ther other half powered down).
	     */
	    if(p->count_type[PCFG_TYPE_HUB] > 64)
	    {
	      SET_FIELD(reg, RGPARM_TTOWRAP, 0xafeL);
	      SET_FIELD(reg, RGPARM_AGEWRAP, 0xfe);
	      SET_FIELD(reg, RGPARM_BYPEN, 0);
	    }
	    else
	      SET_FIELD(reg, RGPARM_AGEWRAP, RGPARM_AGEWRAP_DEFAULT);

	    if (node_vec_write(p, i, RR_GLOBAL_PARMS, reg) < 0)
		goto done;

	    /*
	     * Set up nasid-wide router id, BOOTED bit in RR_SCRATCH_REG0
	     * for use in pttn system recovery. 
	     */

            if ((vec = discover_route(p, 0, i, 0)) == NET_VEC_BAD)
		goto done;

            if ((rc = router_lock(vec, 10000, 3000000)) < 0)
		goto done;

            locked = 1;

	    if ((rc = node_vec_read(p, i, RR_SCRATCH_REG0, &reg)) < 0)
		goto done;

	    SET_FIELD(reg, RSCR0_LOCALID, 
		      NASID_GET_LOCAL(pr->metaid));
            reg |= RSCR0_BOOTED_MASK;

	    if ((rc = node_vec_write(p, i, RR_SCRATCH_REG0, reg)) < 0)
		goto done;

            if ((rc = router_unlock(vec)) < 0)
		goto done;

            locked = 0;

	    /*
	     * Set up metaid and force_local bit
	     */

	    if (node_vec_read(p, i, RR_PROT_CONF, &reg) < 0)
		goto done;

	    reg |= (pr->force_local << RPCONF_FLOCAL_SHFT);
	    reg |= RPCONF_METAIDVALID;

	    /* XXX */
	    if (pr->metaid<0) {
		printf("*** metaid(%d)==-1!\n", i);
		pr->metaid=0;
	    }

	    SET_FIELD(reg, RPCONF_METAID,
		      NASID_GET_META(pr->metaid));

	    if (node_vec_write(p, i, RR_PROT_CONF, reg) < 0)
		goto done;

	    for (ent = 0; ent < RR_LOCAL_ENTRIES; ent++)
		if (node_vec_write(p, i, RR_LOCAL_TABLE(ent),
				   (__uint64_t) pr->rtlocal[ent]) < 0)
		    goto done;

	    for (ent = 0; ent < RR_META_ENTRIES; ent++)
		if (node_vec_write(p, i, RR_META_TABLE(ent),
				   (__uint64_t) pr->rtmeta[ent]) < 0)
		    goto done;

	    break;
	}
    }

    rc = 0;

done:
    if (locked)
        router_unlock(vec);
    return rc;
}

/*
 * distribute_nasids
 *
 *   To be run only on global master CPU.
 */

int distribute_nasids(void)
{
    pcfg_t	       *p;
    int			i;
    int			rc = -1;

    db_printf("Distributing NASIDs\n");

    p = PCFG(get_nasid());

    SD(LOCAL_HUB(NI_SCRATCH_REG1),
       LD(LOCAL_HUB(NI_SCRATCH_REG1)) & ~ADVERT_NASID_MASK |
       (__uint64_t) p->array[0].hub.nasid);

    for (i = 1; i < p->count; i++) {
	pcfg_hub_t	       *ph;
	net_vec_t		vec;
	__uint64_t		data;

	if (p->array[i].any.type != PCFG_TYPE_HUB)
	    continue;

	ph = &p->array[i].hub;

	if (ph->flags & PCFG_HUB_HEADLESS) {
	    /*
	     * Headless node -- set its real NASID
	     */

	    db_printf("> Headless Hub: "
		      "index %d, nic 0x%lx, nasid %d\n",
		      i, ph->nic, ph->nasid);

	    if (node_vec_read(p, i, NI_STATUS_REV_ID, &data) < 0)
		goto done;

	    SET_FIELD(data, NSRI_NODEID, ph->nasid);

	    if (node_vec_write(p, i, NI_STATUS_REV_ID, data) < 0)
		goto done;

	} else {
	    /*
	     * Headful node -- set its NASID in its scratch register
	     */

	    db_printf("> Notifying Hub: "
		      "index %d, nic 0x%lx, nasid %d\n",
		      i, ph->nic, ph->nasid);

	    if (node_vec_read(p, i, NI_SCRATCH_REG1, &data) < 0)
		goto done;

	    SET_FIELD(data, ADVERT_NASID, ph->nasid);

	    if (node_vec_write(p, i, NI_SCRATCH_REG1, data) < 0)
		goto done;
	}
    }

    rc = 0;

done:
    return rc;
}

/*
 * wait_for_node (used for global_barrier)
 */

#define MAX_BARRIER_WARN	10

static int wait_for_node(pcfg_t *p, int index, int set_or_clear, int line)
{
    pcfg_hub_t	       *ph;
    __uint64_t		data;
    int			rc = -1, t;
    rtc_time_t		next	= 0;
    static int		warns	= 0;
    int			wait_count = 0;
    int			first = 1;

    ph = &p->array[index].hub;

    db_printf("> Waiting for %s %s: "
	      "index %d, nic 0x%lx, nasid %d\n",
	      (index == p->gmaster) ? "master" : "slave",
	      set_or_clear ? "set" : "clear",
	      index, ph->nic, ph->nasid);

    while (1) {
	if (node_vec_read(p, index, NI_SCRATCH_REG1, &data) < 0)
	    goto done;

	Speedo(0);
	if ((data & ADVERT_BARRIER_MASK) != 0 && set_or_clear != 0 ||
	    (data & ADVERT_BARRIER_MASK) == 0 && set_or_clear == 0 ||
	    kbintr(&next))
	    break;

	Speedo(0);
        if ((data & ADVERT_EXCP_MASK) && !DIP_NODISABLE()) {
            if (index == p->gmaster)
                goto done;

            ph->flags |= PCFG_HUB_EXCP;
            ph->flags |= PCFG_HUB_HEADLESS;
            break;
        }

        if (GET_FIELD(data, ADVERT_CPUMASK) == 0xc) {
            if (index == p->gmaster)
                goto done;

            ph->flags |= PCFG_HUB_HEADLESS;
            break;
        }

	wait_count = (wait_count + 1)%100;
	if(!wait_count)
        {
	    if(first)
	    {
		printf("waiting for node with nic %x at module %d slot %d at global barrier...",ph->nic, ph->module, ph->slot);
		first = 0;
	    }
	    else
	    {
		printf("..");
	    }
	}
	rtc_sleep(100000);
	Speedo(0);
    }
    if(!first)
	printf("\n");

    if (line > 0 &&
        line != (t = (int) GET_FIELD(data, ADVERT_BARRLINE)) &&
	warns < MAX_BARRIER_WARN) {
	printf("*** Barrier sync warning: local=%d, "
	       "NIC 0x%x=%d (promrev mismatch?)\n",
	       line, GET_FIELD(ph->nic, ADVERT_HUB_NIC), t);
	if (++warns == MAX_BARRIER_WARN)
	    printf("*** Not printing any more barrier warnings\n");
    }

    rc = 0;

 done:

    if (rc < 0)
	printf("> Global barrier failed at line %d node %d\n", line, p->array[index].hub.nasid);

    return rc;
}

/*
 * global_barrier
 *
 *   This is based on the same algorithm as hub_barrier, with A being
 *   the global master and B being all of the other CPUs.  Does nothing
 *   when called by a local slave.
 *
 *   "line" is passed in so we can verify the two CPUs are at the
 *   same point in their boot process (same global barrier!).
 */

static int global_barrier(int line)
{
    int			i, rc;
    __uint64_t		sr1	= (__uint64_t) LOCAL_HUB(NI_SCRATCH_REG1);

    if (hub_local_master()) {
	pcfg_t	       *p;

	db_printf("Global barrier (line %d) ", line);
	Speedo(1);

	p = PCFG(get_nasid());

	if (hub_global_master()) {

	    SD(sr1,
	       LD(sr1) & ~ADVERT_BARRLINE_MASK |
	       (__uint64_t) line << ADVERT_BARRLINE_SHFT |
	       ADVERT_BARRIER_MASK);

	    for (i = 1; i < p->count; i++)
		if (p->array[i].any.type == PCFG_TYPE_HUB &&
		    !(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
                    IS_RESET_SPACE_HUB(&p->array[i].hub)) {
		    if ((rc = wait_for_node(p, i, 1, line)) < 0)
			goto done;

                    if ((p->array[i].hub.flags & PCFG_HUB_EXCP) 
			&& (p->array[i].hub.slot != 0))
                        printf("*** %S has taken an "
			       "exception! Continuing...\n",
			       p->array[i].hub.module, 
			       p->array[i].hub.slot);
                }

	    SD(sr1, LD(sr1) & ~ADVERT_BARRIER_MASK);

	    for (i = 1; i < p->count; i++)
		if (p->array[i].any.type == PCFG_TYPE_HUB &&
		    !(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
                    IS_RESET_SPACE_HUB(&p->array[i].hub)) {
		    if ((rc = wait_for_node(p, i, 0, -1)) < 0)
			goto done;

                    if ((p->array[i].hub.flags & PCFG_HUB_EXCP)
			&& (p->array[i].hub.slot != 0))
                        printf("*** %S has taken an "
			       "exception! Continuing...\n",
			       p->array[i].hub.module, 
			       p->array[i].hub.slot);
                }
	} else {
	    SD(sr1,
	       LD(sr1) & ~ADVERT_BARRLINE_MASK |
	       (__uint64_t) line << ADVERT_BARRLINE_SHFT);

	    if ((rc = wait_for_node(p, p->gmaster, 1, line)) < 0)
		goto done;

	    SD(sr1, LD(sr1) | ADVERT_BARRIER_MASK);

	    if ((rc = wait_for_node(p, p->gmaster, 0, -1)) < 0)
		goto done;

	    SD(sr1, LD(sr1) & ~ADVERT_BARRIER_MASK);
	}
	printf("\b");

	db_printf("Global barrier passed.\n");
    }

    rc = 0;

 done:

    return rc;
}

static void
sync_ids(nasid_t nasid)
{
    char	buf[8];
    int		id;

    if (SN00) {
        ip27log_getenv(nasid, IP27LOG_PARTITION, buf, "0", 0);

        if (ip27log_getenv(nasid, IP27LOG_DOMAIN, NULL, NULL, 0) < 0)
            ip27log_setenv(nasid, IP27LOG_DOMAIN, buf, 0);

        if (ip27log_getenv(nasid, IP27LOG_CLUSTER, NULL, NULL, 0) < 0)
            ip27log_setenv(nasid, IP27LOG_CLUSTER, buf, 0);

        if (ip27log_getenv(nasid, IP27LOG_CELL, NULL, NULL, 0) < 0)
            ip27log_setenv(nasid, IP27LOG_CELL, buf, 0);
    }
    else {
        elsc_t	e;

        elsc_init(&e, nasid);

        if ((id = elsc_domain_get(&e)) >= 0) {
            sprintf(buf, "%d", id);
            ip27log_setenv(nasid, IP27LOG_DOMAIN, buf, 0);
        }

        if ((id = elsc_cluster_get(&e)) >= 0) {
            sprintf(buf, "%d", id);
            ip27log_setenv(nasid, IP27LOG_CLUSTER, buf, 0);
        }

        if ((id = elsc_cell_get(&e)) >= 0) {
            sprintf(buf, "%d", id);
            ip27log_setenv(nasid, IP27LOG_CELL, buf, 0);
        }
    }
}

/*
 * init_headless
 *
 *   Remote initialization of memory and I/O on nodes that don't have
 *   any CPU that is working and enabled.
 */

static int init_headless(nasid_t nasid, int diag_mode)
{
    pcfg_t	       *p;
    int			i;
    int			rc = 0;
    __uint64_t		data;
    int			diag_settings;
    char		mem_disable[MD_MEM_BANKS + 1];
    char		buf[8], bank_sz[8];
    elsc_t		remote_elsc;
    u_short		prem_mask;

    diag_settings = diag_mode | (DIP_VERBOSE() ? DIAG_FLAG_VERBOSE : 0);

    p = PCFG(nasid);

    for (i = 0; i < p->count; i++)
	if (p->array[i].any.type == PCFG_TYPE_HUB &&
            IS_RESET_SPACE_HUB(&p->array[i].hub) &&
	    (p->array[i].hub.flags & PCFG_HUB_HEADLESS)) {
	    pcfg_hub_t	       *ph;
	    lboard_t	       *brd_ptr;
	    int			iio_link_up, wr ;
	    hubreg_t		ii_csr, ii_csr1;
            klinfo_t           *cpu_ptr;
            __uint64_t		sr0;

	    ph = &p->array[i].hub;

            printf("Initializing headless node at nasid %d\n", ph->nasid);

	    /*
	     * Kill remote CPUs
	     */

	    SD(REMOTE_HUB(ph->nasid, PI_CPU_ENABLE_A), 0);
	    SD(REMOTE_HUB(ph->nasid, PI_CPU_ENABLE_B), 0);

	    /*
	     * Set up remote FPROM parameters.
	     * Avoid bogus fprom_wr (XXX temporary)
	     */

	    wr = IP27CONFIG.fprom_wr ? IP27CONFIG.fprom_wr : 1;

	    data = LD(REMOTE_HUB(ph->nasid, MD_MEMORY_CONFIG));
	    data &= ~(MMC_FPROM_CYC_MASK | MMC_FPROM_WR_MASK
		      | MMC_IGNORE_ECC | MMC_UCTLR_WR_MASK);
	    data |=
		((__uint64_t) IP27CONFIG.fprom_cyc << MMC_FPROM_CYC_SHFT |
		 (__uint64_t) wr << MMC_FPROM_WR_SHFT |
		 (__uint64_t) 0xa << MMC_UCTLR_WR_SHFT);

	    if (! IP27CONFIG.ecc_enable)
		data |= MMC_IGNORE_ECC;

	    SD(REMOTE_HUB(ph->nasid, MD_MEMORY_CONFIG), data);

	    /*
	     * Read NIC.
	     */
	    if (hub_nic_get(ph->nasid, 0, &ph->nic) < 0)
		printf("Could not read remote NIC for nasid %d.\n", ph->nasid);

	    SD(REMOTE_HUB(ph->nasid, PI_CRB_SFACTOR), 0x2000);
	    SD(REMOTE_HUB(ph->nasid, PI_CALIAS_SIZE), 0);

	    /*
	     * Probe memory size, configure memory, and test/init memory.
	     * XXX We should give some of the init work to slaves.
	     */

	    mdir_init(ph->nasid, IP27CONFIG.freq_hub);
	    mdir_config(ph->nasid,&prem_mask);
    	    sprintf(buf,"%x",prem_mask);
    	    ip27log_setenv(ph->nasid, PREMIUM_BANKS, buf, 0);
            mdir_sz_get(ph->nasid, bank_sz);

            ip27log_getenv(ph->nasid, DISABLE_MEM_MASK, mem_disable, 0, 0);

	    memory_init_all(ph->nasid, 0, mem_disable, diag_mode) ;

            if (strchr(mem_disable, '0') || (bank_sz[0] == MD_SIZE_EMPTY)) {
                if (!strchr(mem_disable, '1') && (bank_sz[1] !=
			MD_SIZE_EMPTY)) {
		    if (MDIR_DIMM0_SEL(ph->nasid) < 2) {
                        printf("*** Swapping bank 0 with bank 1 on headless "
                                "node nasid %d\n", ph->nasid);
                        swap_memory_banks(ph->nasid, 2);
                    }
                }
                else {
                    ph->flags |= PCFG_HUB_MEMLESS ;

		    /* Dont continue. We still need to do all stuff
		     * for this headless node, that does not depend
	    	     * on memory, like read part and mod num off elsc.
		     */
                    /* continue ; */
                }
            }

	    if (! DIP_NODISABLE())
	    	memory_disable(ph->nasid, mem_disable);
            memory_empty_enable(ph->nasid);

            ip27_inventory(ph->nasid);

            /*
             * If CPU(s) died due to a unexpected exception, disable 'em here!
             */
            if (ph->flags & PCFG_HUB_EXCP) {
                char	led, buf[64];

                /* XXX: This LED value is useless ? */
                led = (char) REMOTE_HUB_L(ph->nasid, PI_RT_COMPARE_A);

                sprintf(buf, "%d: %x; %s", KLDIAG_UNEXPEC_EXCP, led,
			get_diag_string(KLDIAG_UNEXPEC_EXCP));

                ed_cpu_mem(ph->nasid, DISABLE_CPU_A, buf, buf, 0, 0);
                ed_cpu_mem(ph->nasid, DISABLE_CPU_B, buf, buf, 0, 0);
            }

	    /*
	     * Initialize KLDIR and KLCONFIG, and do I/O discovery.
	     */

	    if (!IS_HUB_MEMLESS(ph)) {
	    	init_kldir(ph->nasid);
	    	init_klcfg(ph->nasid);

	    	if ((brd_ptr = init_klcfg_ip27((__psunsigned_t)
					   REMOTE_HUB(ph->nasid, 0),
					   0)) == NULL)
                	printf("Cannot not init klconfig for node %d\n", 
				ph->nasid);
            	else {
		       /* 
		 	* init_klcfg_cpuinfo updates info about only one
		 	* cpu. We need to update info about the other one too.
		 	*/
		   update_klcfg_cpuinfo(ph->nasid, 1 - LD(LOCAL_HUB(PI_CPU_NUM)));
		   if (ph->flags & PCFG_HUB_EXCP) {
                	klcpu_t		*cpu;

                	cpu = (klcpu_t *) find_first_component(
						brd_ptr, KLSTRUCT_CPU);

                	if (cpu)
                    		cpu->cpu_info.diagval = KLDIAG_UNEXPEC_EXCP;

                	cpu = (klcpu_t *) find_component(brd_ptr, 
						(klinfo_t *) cpu, KLSTRUCT_CPU);

                	if (cpu)
                    		cpu->cpu_info.diagval = KLDIAG_UNEXPEC_EXCP;
            	    }
		}

            /*
             * For SN00, init a dummy midplane structure. This holds the
             * module serial number. Pass widget base of widget 0.
	     * Make sure that this is not a SN00 with an xbox.
             */

            	if ((SN00 || CONFIG_12P4I) && !is_xbox_config(ph->nasid)) {
                	init_klcfg_midplane(SN0_WIDGET_BASE(ph->nasid, 0),
                                    brd_ptr) ;
            	}

	    	iio_link_up = hubii_init((__psunsigned_t)
				     REMOTE_HUB(ph->nasid, 0));

	    	if (iio_link_up <= 0) {
                	klcfg_hubii_stat(ph->nasid);
                	ph->flags &= ~PCFG_HUB_XBOW ;
            	} else {
                	ph->flags |= PCFG_HUB_XBOW ;
			xtalk_init(ph->nasid, diag_settings, 1);
			io_discover(brd_ptr, diag_settings);
	    	}
	    } /* NOT a memless hub */

            /*
             * Try to fill up as much info. as possible in pcfg
             */

            if (SN00) {
                char	buf[8];

                ip27log_getenv(ph->nasid, IP27LOG_MODULE_KEY, buf, "0", 0);
                ph->module = strtoull(buf, 0, 0);
                ip27log_getenv(ph->nasid, IP27LOG_PARTITION, buf, "0", 0);
                ph->partition = strtoull(buf, 0, 0);
            }
            else {

                /*
                 * NOTE: There should be no use of the local ELSC betwene the two 
                 * elsc_inits
                 */

                elsc_init(&remote_elsc, ph->nasid);
                ph->module = elsc_module_get(&remote_elsc);
                ph->partition = elsc_partition_get(&remote_elsc);

                if ((signed char) ph->module < 0) {
                    ph->module = discover_vote_module(p, i);
                    if ((signed char) ph->module < 0) {
                        printf("Slot n%d, nasid %d : Module ID %d\n",
                        	ph->slot, ph->nasid, ph->module);
                        ph->module = 0;
                    }
                    else {
                        db_printf("Voted for moduleid for headless HUB. "
			   "module = %d\n", ph->module);
                    }
                }

                if ((signed char) ph->partition < 0) {
                    ph->partition = pttn_vote_partitionid(p, i);
                    printf("Voted for pttnid for headless HUB. pttn = %d\n",
			ph->partition);
                    if ((signed char) ph->partition < 0)
                        ph->partition = 0;
                }
            }

            sprintf(buf, "%d", ph->module);
            ip27log_setenv(ph->nasid, IP27LOG_MODULE_KEY, buf, 0);
            sprintf(buf, "%d", ph->partition);
            ip27log_setenv(ph->nasid, IP27LOG_PARTITION, buf, 0);

            sr0 = REMOTE_HUB_L(ph->nasid, NI_SCRATCH_REG0);
            SET_FIELD(sr0, ADVERT_PARTITION, (__uint64_t) ph->partition);
            REMOTE_HUB_S(ph->nasid, NI_SCRATCH_REG0, sr0);

            ph->slot = SLOTNUM_GETSLOT(get_node_slotid(ph->nasid));
            ph->promvers = IP27CONFIG_NODE(ph->nasid).pvers_vers;
            ph->promrev = IP27CONFIG_NODE(ph->nasid).pvers_rev;

	    if (!IS_HUB_MEMLESS(ph))
	    	klconfig_nasid_update(ph->nasid, 1);

            sync_ids(ph->nasid);
    }

    for (i = 0; i < p->count; i++)
        if (p->array[i].any.type == PCFG_TYPE_HUB &&
            IS_RESET_SPACE_HUB(&p->array[i].hub) &&
            (p->array[i].hub.flags & PCFG_HUB_HEADLESS) != 0) {
            pcfg_hub_t         *ph;
            ph = &p->array[i].hub;
	    if (IS_HUB_MEMLESS(ph))
		continue ;
            if (ph->flags & PCFG_HUB_XBOW) /* reset only if xbow was found */
	    	xbow_reset_arb_nasid(ph->nasid) ;
	}

    return rc;
}

/*
 * init_headless_worker
 *
 *   Called while the master is initializing headless nodes.
 *   Waits for the master to hand it work to do.
 */

static int init_headless_worker(nasid_t nasid)
{
    return 0;
}

/*
 * main
 *
 *   Entry() jumps here ASAP after the cache-stack is initialized.
 */

#if TEST_LAUNCH

static void test_fn(__int64_t x)
{
    int			nasid	= net_node_get();
    int			cpu	= (int) hub_cpu_get();

    hub_led_set(x + nasid * 2 + cpu);
}

static void test_poll(int nasid, int cpu)
{
    int			i;

    do {
	hub_led_set(160);

	i = LAUNCH_POLL(nasid, cpu);

	hub_led_set(165 + i);
    } while (i != LAUNCH_STATE_DONE);

    hub_led_set(169);
}

static void test_launch(void)
{
    int			nasid	= net_node_get();
    int			cpu	= (int) hub_cpu_get();
    int			gmaster = (nasid == 0 && cpu == 0);
    __int64_t		stack[128];

#ifdef SABLE
    if (cpu == 0) {
	libc_device(&dev_hubuart);
	libc_init(&console);
    }
#endif

    if (gmaster) {
	init_kldir((nasid_t) 0);
	init_kldir((nasid_t) 1);
    }

    hub_led_set(120 + nasid * 2 + cpu);

    if (gmaster) {
	delay(1000);
    } else
	delay(500);

    hub_led_set(130 + nasid * 2 + cpu);

    if (! gmaster)
	LAUNCH_LOOP();

    hub_led_set(140 + nasid * 2 + cpu);

    LAUNCH_SLAVE(1, 1, test_fn, 150,
		 (char *) stack + sizeof (stack), (void *) get_gp());

    test_poll(1, 1);

    hub_led_set(170 + nasid * 2 + cpu);

    LAUNCH_SLAVE(1, 0, test_fn, 180,
		 (char *) stack + sizeof (stack), (void *) get_gp());

    LAUNCH_SLAVE(1, 1, test_fn, 190,
		 (char *) stack + sizeof (stack), (void *) get_gp());

    hub_led_set(200 + nasid * 2 + cpu);

    LAUNCH_WAIT(1, 0, 100000);
    LAUNCH_WAIT(1, 1, 100000);

    while (1)
	;
}

#endif /* TEST_LAUNCH */

/*
 * read_dip_switches
 *
 *   Refreshes the Debug Switch settings into the BSR.  There are 16
 *   switches -- 8 virtual and 8 physical.  These are read from the system
 *   controller "dbg" settings.  The hardware DIP switches are also read
 *   from the system controller and are XORed into the physical settings.
 *
 *   If the switches aren't accessible, leaves the old settings.
 */

void read_dip_switches(uchar_t *nvram_buffer)
{
    int			r;
    u_char		dbg_p, dbg_v;
    elsc_t	       *e	= get_elsc();

	if (nvram_buffer != 0) {
		dbg_v = nvram_buffer[5];
		dbg_p = nvram_buffer[6];
	} else if ((r = elsc_debug_get(e, &dbg_v, &dbg_p)) < 0) {
		dbg_v = dbg_p = 0;
	}

    if ((r = elsc_dip_switches(e)) >= 0)
	set_BSR(get_BSR() & ~BSR_DIPMASK |
		(__uint64_t) dbg_v << (BSR_DIPSHFT + 8) |
		(__uint64_t) (dbg_p ^ (u_char) r) << BSR_DIPSHFT |
		BSR_DIPSAVAIL | ((dbg_v | dbg_p) ? BSR_DBGNOT0 : 0));
}

void dip_set_diag_mode(int diag_mode)
{
    int		dip_diag_switch0, dip_diag_switch1;

    dip_diag_switch0 = diag_mode & (1 << 0);
    dip_diag_switch1 = diag_mode & (1 << 1);

    dip_diag_switch0 ? DIP_SWITCH_SET(DIP_NUM_DIAG_MODE0) :
		DIP_SWITCH_CLEAR(DIP_NUM_DIAG_MODE0);
    dip_diag_switch1 ? DIP_SWITCH_SET(DIP_NUM_DIAG_MODE1) :
		DIP_SWITCH_CLEAR(DIP_NUM_DIAG_MODE1);
}

/*
 * load_execute_segment
 *
 *   flag bit 0: If true, load out of the IO6 card PROM.  Otherwise,
 *  		 load out of the backup copy of the IO6 prom that's
 *  		 present in the IP27 prom.
 *
 *   flag bit 1: load prom but don't jump to it
 *
 *   flag bit 2: If booting out of the IO6 card PROM fails for any
 *  		 reason, fall back and retry out of the IP27 prom.
 *
 *   If seg_name is NULL, just lists all of the segments and returns.
 */

int load_execute_segment(char *seg_name, int flag, int diag_mode)
{
    promhdr_t	       *ph;
    __uint64_t		load_addr;
    __uint64_t		entry_addr;
    __uint64_t		sum, length;
    nasid_t		nasid;
    extern char		io6prom[];
    char	       *dataptr;
    int			segno;
    promseg_t	       *seg;
    __uint64_t		tmpbuf;
    __uint64_t		tmpsize;
    jmp_buf		fault_buf;
    void	       *old_buf;
    console_t	       *kl_cons;
    int			wid;
    fprom_t		f;
    int			mcode, dcode;
    nasid_t		xbox_nasid;

    nasid	= get_nasid();


    /* Check if the configuration has an xbox  */
    xbox_nasid = xbox_nasid_get();

    tmpbuf	= TO_NODE(nasid, IP27PROM_DECOMP_BUF);
    tmpsize	= IP27PROM_DECOMP_SIZE;

    f.dev	= FPROM_DEV_IO6_P0;
    f.afn	= 0;

    if (flag & 1) {
	/* Read in the Flash IO6 PROM header */
	/* xbox_nasid is the nasid of the node connected to the xbox
	 * If it is invalid that means that we don't have an xbox
	 * config then look for ioprom on the baseio.
	 */
	if (xbox_nasid == INVALID_NASID) {
#if 0
	kl_cons = &(KL_CONFIG_HDR(get_nasid())->ch_cons);
#endif
	kl_cons = KL_CONFIG_CH_CONS_INFO(get_nasid()) ;

	if (kl_cons->uart_base == 0) {
	    printf("No BASEIO board? skipping BASEIO PROM load\n");
	    return -1;
	} else {
	    wid = WIDGETID_GET(kl_cons->uart_base);
	}

	f.base	= (void *) TO_NODE(nasid,
				   SN0_WIDGET_BASE(kl_cons->nasid, wid) +
				   BRIDGE_EXTERNAL_FLASH);
	} else {
        /*  We are reading out of the flashprom on the bridge in the 
	 *  in the xbox.
	 */
	f.base =  (void *) TO_NODE(nasid,
				   SN0_WIDGET_BASE(xbox_nasid, 
						   XBOX_FLASHPROM_WID) +
				   BRIDGE_EXTERNAL_FLASH);
	}

	ph	= (promhdr_t *) TO_NODE(nasid, IP27PROM_FLASH_HDR);

	if (setfault(fault_buf, &old_buf)) {
	    restorefault(old_buf);

	    printf("Bus error trying to access BASEIO PROM at 0x%lx\n", f.base);
	    goto fail;
	}

	/*
	 * Probe the IO FLASH PROM. Probe also sets f.dev to
	 * its right value based on the type of the IO PROM.
	 */

	((bridge_t *) ((__psunsigned_t)f.base & 0xffffffffff000000))
	    ->b_wid_control |= (0x1 << 31) ;

	if (fprom_probe(&f, &mcode, &dcode) < 0) {
	    /*
	     * The probe routine for F080 usually works for
	     * LV800 also. Just in case it did not we try this.
	     */
	    f.dev	= FPROM_DEV_IO6_P1 ;
	    if (fprom_probe(&f, &mcode, &dcode) < 0) {
		restorefault(old_buf);
		printf("Error: Unknown type of BASEIO PROM\n") ;
		goto fail;
	    }
	}

	fprom_read(&f, 0, (char *) ph, PROM_DATA_OFFSET);

	restorefault(old_buf);
    } else
	ph	= (promhdr_t *) io6prom;

#if defined(SABLE)
    ph = (promhdr_t *) ((__uint64_t) LBOOT_BASE + 0x800000);
#endif

    if (segldr_check(ph) < 0) {
	printf("Magic number is not valid.\n");
	goto fail;
    }

    if (seg_name == 0) {
	segldr_list(ph);
	return 0;
    }

    if ((segno = segldr_lookup(ph, seg_name)) < 0) {
	printf("Segment %s not found.\n", seg_name);
	goto fail;
    }

    seg = &ph->segs[segno];

    if (seg->flags & SFLAG_COMPMASK) {
	length	= seg->length_c;
	sum	= seg->sum_c;
    } else {
	length	= seg->length;
	sum	= seg->sum;
    }

    /* If we are booting a old io prom, we need to change some
       of the data structs for compatibility. */

    segldr_old_ioprom(ph, seg) ;

    if (flag & 1) {
	/* Read in the segment data and verify pre-decompress sum */

	db_printf("Transferring contents of BASEIO PROM to memory\n");

	dataptr = (char *) TO_NODE(nasid, IP27PROM_FLASH_DATA);

	fprom_read(&f, seg->offset, dataptr, length);

	if (memsum(dataptr, length) != sum) {
	    printf("Segment data checksum incorrect.\n");
	    result_fail("prom_checksum", KLDIAG_PROM_CHKSUM_BAD, 
			get_diag_string(KLDIAG_PROM_CHKSUM_BAD));
	    goto fail;
	}
    } else
	dataptr = io6prom + seg->offset;

    /*
     * This converts the virtual load and entry address to
     * usable physical addresses.
     */

    if (IS_KSEGDM(seg->loadaddr)) {
	load_addr = seg->loadaddr & NODE_ADDRSPACE_MASK;
	load_addr = TO_NODE(nasid, PHYS_TO_K0(load_addr));

	entry_addr = seg->entry & NODE_ADDRSPACE_MASK;
	entry_addr = TO_NODE(nasid, PHYS_TO_K0(entry_addr));
    }
    else {
	load_addr = TO_NODE(nasid, PHYS_TO_K0(seg->loadaddr & 0xfffffff));
	entry_addr = TO_NODE(nasid, PHYS_TO_K0(seg->entry & 0xfffffff));
    }

    {
	char *c = 0 ;
        /* Change only the name on printf on the console. */
	if (!strcmp(seg_name, "io6prom"))
		c = "BASEIO prom" ;
	else
		c = seg_name ;
    	db_printf("%s segment %s\n",
	   (seg->flags & SFLAG_COMPMASK) ? "Uncompressing" : "Copying",
	   c);
    }

    if (segldr_load(ph, segno,
		    (__uint64_t) dataptr, load_addr,
		    tmpbuf, tmpsize) < 0) {
	printf("Segment load failed\n");
	return -1;
    }

    if (flag & 2) {
	printf("Would have jumped to %y\n", entry_addr);
	return 0;
    }

    /*
     * prom is now loaded in cached memory. Flush it out in case we ever
     * invalidate memory.
     */

    cache_flush();

    if (segldr_jump(ph, segno, entry_addr, diag_mode, 0) < 0)
	printf("Failed to jump to PROM\n");

 fail:

    if ((flag & 1) && (flag & 4)) {
	printf("Retrying using the IP27 PROM internal copy of BASEIO PROM\n");
	return load_execute_segment(seg_name, flag & ~1, diag_mode);
    }

    return -1;
}

/*
 * Function:	split_brain -> Detects if the node causes a split brain system
 * Returns:	1 if split_brain
 *		0 if not
 * Note:	split_brain systems are those which can't reach their own peer
 *		on the xbow.
 *		For n1 & n3 xbow peers are at vector 0x56
 *		For n2 & n4 xbow peers are at vector 0x46
 *		WARNING: Totally slot/port assignment dependent!
 */

static int
split_brain(nasid_t nasid)
{
    int         slot_odd;
    net_vec_t   peer_vec;
    __uint64_t  scratch;

    if (!peer_hub_on_xbow(nasid))
        return 0;
    slot_odd = SLOTNUM_GETSLOT(get_node_slotid(nasid)) & 0x1;
    if (SLOTNUM_GETCLASS(get_node_slotid(nasid)) == SLOTNUM_KNODE_CLASS) {
	/* KEGO */
	if(vector_read(0,0, NI_STATUS_REV_ID,&scratch) < 0)
		return 1; 
        if(GET_FIELD(scratch, NSRI_CHIPID)==CHIPID_HUB)
		return 0; /* null router system.. we can see that hub is present */	
	peer_vec = slot_odd ? 4 : 5;
    }
    else
        peer_vec = vector_concat(6, (slot_odd ? 5 : 4));

    return (vector_read(peer_vec, 0, NI_SCRATCH_REG0, &scratch) < 0);
}

#define LINK_TIMEOUT		1000000		/* usec */

/*
 * run_discover
 */

int run_discover(int diag_mode)
{
    rtc_time_t		expire, disctime;
    int			okay, r;
    pcfg_t	       *p;
    nic_t		my_nic = (nic_t) GET_FIELD(
				LOCAL_HUB_L(NI_SCRATCH_REG0), ADVERT_HUB_NIC);

    expire = rtc_time() + LINK_TIMEOUT;
    okay = 0;

    while (rtc_time() < expire)
	if (net_link_up()) {
	    okay = 1;
	    break;
	}

    if (! okay)
	printf("\nLocal hub CrayLink is down.\n");

    p		= PCFG(get_nasid());

    p->magic	= PCFG_MAGIC;
    p->alloc	= ((IP27PROM_PCFG_SIZE - sizeof (pcfg_t)) /
		   sizeof (pcfg_ent_t)) + 1;
    p->gmaster	= 0;

    /*
     * Generate network discovery graph
     */

    disctime	= rtc_time();

    if ((r = discover(p, my_nic, diag_mode)) < 0) {
	result_fail("discover", KLDIAG_DISCOVERY_FAILED,
			get_diag_string(KLDIAG_DISCOVERY_FAILED));
	db_printf("LLP link was up but could not talk to router.\n");
	return r;
    }

    disctime	= rtc_time() - disctime;

    printf("DONE\n") ;
    printf("Found %d objects (%d hubs, %d routers) in %ld usec\n",
	   p->count,
	   p->count_type[PCFG_TYPE_HUB],
	   p->count_type[PCFG_TYPE_ROUTER],
	   disctime);

    return r;
}

#define	DISC_FAIL_NONE		0
#define	DISC_FAIL_INACCESS	1
#define	DISC_FAIL_COUNT		2
#define	DISC_FAIL_NODONE	3
#define	DISC_FAIL_SPLIT_BRAIN	4

static char	*disc_fail_mesg[] = {
	"No problems",
	"HUB inaccessible",
	"Showed wrong count",
	"HUB not done yet",
	"Split brain system"
};

/*
 * robust_discover
 *
 *   Iterates the discovery process until consistent results are
 *   achieved across all nodes.
 *
 *   If the number of objects discovered by some node is different
 *   than the number discovered on the local node, or a previously
 *   discovered node is no longer accessible, some link(s) are
 *   assumed to have gone down, and the discovery is re-run.
 *
 *   There is an overall time limit of a couple minutes to prevent
 *   the inevitable infinite loop.
 *
 *   Returns:
 *       Two values ORed in:
 *           RC field -> 0 everything went perfectly the first time.
 *                       1 there were problems that eventually stabilized.
 *                       2 timed out without stabilization.
 */

#define	MAX_DISCOVER_RERUN	3

int robust_discover(pcfg_t *p, int diag_mode)
{
    rtc_time_t		expire;
    rtc_time_t		next = 0;
    int			i, t;
    __uint64_t		sr1;
    int			rc, rerun;
    int			fail_type, first = 1;

    printf("Discovering CrayLink connectivity .........\t\t") ;
    DM_PRINTF(("\n")) ;

    rerun = 0;

 rerun_disc:

    rerun++;
    rc = 0;
    expire = rtc_time() + DISCOVER_TIME_MAX;
    fail_type = DISC_FAIL_NONE;

 disc_again:

    if (rc) {
	printf("*** A link may have gone down. Rediscovering network.\n");
        rtc_sleep(8*1000000);
    }

    run_discover(diag_mode);

    SD(LOCAL_HUB(NI_SCRATCH_REG1),
       LD(LOCAL_HUB(NI_SCRATCH_REG1)) |
       ADVERT_DISCDONE_MASK |
       (__uint64_t) p->count);		/* ADVERT_OBJECT_MASK */

 check_again:

    if (rtc_time() > expire || kbintr(&next)) {
	printf("*** Repeatedly inconsistent discovery results; timing out\n");
	printf("*** Timed out waiting for %S, NIC 0x%lx "
	       "reason: %s\n", p->array[i].hub.module, p->array[i].hub.slot,
	       p->array[i].hub.nic, disc_fail_mesg[fail_type]);
	rc = 2;
	goto done;
    }

    /*
     * See if any other node is still discovering.
     */

    if (first) {
	printf("Waiting for peers to complete discovery....\t\t");
	first = 0;
    }


    /*
     * Detect split brain systems
     */

    if (split_brain(get_nasid())) {
        printf("\n*** %S: Peer HUB detected on Xbow but no "
	       "Craylink connection\n", p->array[0].hub.module,
	       p->array[0].hub.slot);
	fail_type = DISC_FAIL_SPLIT_BRAIN;
	rc = 1;
	goto disc_again; /* Link down; rediscover */
    }

    /*
     * Do consistency checking...
     */

    for (i = 1; i < p->count; i++)
	if (p->array[i].any.type == PCFG_TYPE_HUB &&
	    !(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
            IS_RESET_SPACE_HUB(&p->array[i].hub)) {

	    if (node_vec_read(p, i, NI_SCRATCH_REG1, &sr1) < 0) {
		printf("*** Hub %S with NIC 0x%lx is no longer accessible.\n",
			p->array[i].hub.module, p->array[i].hub.slot,
			p->array[i].hub.nic);
                fail_type = DISC_FAIL_INACCESS;
		rc = 1;
		goto disc_again; /* Link down; rediscover */
	    }

	    if ((sr1 & ADVERT_DISCDONE_MASK) == 0) {
	    	rtc_sleep(2*1000000);
                fail_type = DISC_FAIL_NODONE;
		goto check_again; /* Not done yet */
	    }

	    t = (int) (sr1 & ADVERT_OBJECTS_MASK);

	    if (t != p->count) {
		printf("*** Hub %S with NIC 0x%lx found %d objects, not %d.\n",
			p->array[i].hub.module, p->array[i].hub.slot,
			p->array[i].hub.nic, t, p->count);
                fail_type = DISC_FAIL_COUNT;
		rc = 1;
		goto disc_again; /* Differing object count */
	    }
	}

    /*
     * Everything is kosher. Check for headless that are active now or
     * nodes that got reset now after a delay. If there are any, rerun
     * discovery. Do this 3 times and give up
     */

    rtc_sleep(500000);

    for (i = 1; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB)) {
            __uint64_t	ni_sc0, ni_sc1;

            if (node_vec_read(p, i, NI_SCRATCH_REG0, &ni_sc0) < 0)
                continue;
            if (node_vec_read(p, i, NI_SCRATCH_REG1, &ni_sc1) < 0)
                continue;

            if ((!p->array[i].hub.nic && (ni_sc0 & ADVERT_HUB_NIC_MASK) &&
		!(ni_sc1 & ADVERT_EXCP_MASK)) ||
		(!IS_RESET_SPACE_HUB(&p->array[i].hub) &&
		!(ni_sc1 & ADVERT_BOOTED_MASK))) {
                if (rerun < MAX_DISCOVER_RERUN) {
                    db_printf("*** Hub %S: pcfg flags 0x%x: sc0 0x%lx: "
			"sc1 0x%lx\n", p->array[i].hub.module,
			p->array[i].hub.slot, p->array[i].hub.flags,
			ni_sc0, ni_sc1);
                    db_printf("Rerunning CrayLink discovery %d time\n", rerun);
                    goto rerun_disc;
                }
                else {
	            printf("*** Repeatedly inconsistent discovery results; "
			"timing out\n");
                    printf("*** Hub %S: pcfg flags 0x%x: sc0 0x%lx: "
			"sc1 0x%lx\n", p->array[i].hub.module,
			p->array[i].hub.slot, p->array[i].hub.flags,
			ni_sc0, ni_sc1);
                    rc = 2;
                    goto done;
                }
            }
        }

    for (i = 1; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		IS_RESET_SPACE_HUB(&p->array[i].hub) &&
		(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
		(p->array[i].hub.flags & PCFG_HUB_EXCP) &&
		(p->array[i].hub.slot != 0))
            printf("*** Hub %S with NIC 0x%lx took an "
		   "exception! Disabling and continuing...\n", 
		   p->array[i].hub.module, p->array[i].hub.slot, 
		   p->array[i].hub.nic);

#if 0
    db_printf("Discovery results:\n");
    discover_dump_promcfg(p, db_printf, DDUMP_PCFG_ALL);
#endif

done:
    printf("DONE\n") ;
    return rc;
}

/*
 * match_prom_versions
 *
 *   Compare PROM major version numbers and print a warning message
 *   if they don't match.  This will help explain why a system may
 *   not be able to boot past global arbitration.
 */

static int match_prom_versions(pcfg_t *p, int nasids_valid)
{
    int		i, r;

    r = 0;

    for (i = 1; i < p->count; i++)
	if (p->array[i].any.type == PCFG_TYPE_HUB &&
	    p->array[i].hub.nic != 0 &&
	    p->array[i].hub.promvers != p->array[0].hub.promvers) {
	    if (nasids_valid)
		printf("*** PROM version mismatch "
		       "(NASID %d vers %d, NASID %d vers %d)\n",
		       p->array[0].hub.nasid,
		       p->array[0].hub.promvers,
		       p->array[i].hub.nasid,
		       p->array[i].hub.promvers);
	    else
		printf("*** PROM version mismatch "
		       "(nic 0x%lx vers %d, nic 0x%lx vers %d)\n",
		       p->array[0].hub.nic,
		       p->array[0].hub.promvers,
		       p->array[i].hub.nic,
		       p->array[i].hub.promvers);
	    r = -1;
	}

    return r;
}

/*
 * sync_bsr
 *
 *   Copies the masked bits in the local master's BSR into the
 *   corresponding bits in the local slave's BSR.  Unfortunately
 *   the PERF regs only hold 20 bits.
 */

static void sync_bsr(__uint64_t mask)
{
    __uint64_t		pass;

    pass = get_BSR();

    if (hub_local_master()) {
	SD(LOCAL_HUB(MD_PERF_CNT2), pass >> 60);
	SD(LOCAL_HUB(MD_PERF_CNT3), pass >> 40);
	SD(LOCAL_HUB(MD_PERF_CNT4), pass >> 20);
	SD(LOCAL_HUB(MD_PERF_CNT5), pass      );
    }

    hub_barrier();

    if (! hub_local_master()) {
	pass = ((LD(LOCAL_HUB(MD_PERF_CNT2)) & 0x0000f) << 60 |
		(LD(LOCAL_HUB(MD_PERF_CNT3)) & 0xfffff) << 40 |
		(LD(LOCAL_HUB(MD_PERF_CNT4)) & 0xfffff) << 20 |
		(LD(LOCAL_HUB(MD_PERF_CNT5)) & 0xfffff));

	set_BSR(get_BSR() & ~mask | pass & mask);
    }
}

/*
 * cb_elsc
 *
 *   ELSC interrupt message callback handler.
 */

/*ARGSUSED*/
static void cb_elsc(void *junk, char *msg)
{
    if (junkuart_probe()) {
	junkuart_puts("Received MSC interrupt message:\n");
	junkuart_puts(msg);
    }
}

/*
 * pcfg_router_nic_access
 *     router nic access routine. Uses the pcfg structure
 *     to build the vector to the router. This needs to
 *     here in main.c since node_vec_write is static.
 *     Also, ideally we do not want this routine in nic.c
 */

static int pcfg_router_nic_access(nic_data_t data,
				  int pulse, int sample, int delay)
{
	__uint64_t	    val;

	val = nic_get_phase_bits() | MCR_PACK(pulse, sample);

	node_vec_write(PCFG(get_nasid()), (int)data, RR_NIC_ULAN, val);

	while (node_vec_read(PCFG(get_nasid()),
			(int)data, RR_NIC_ULAN, &val) >= 0)
	    if (MCR_DONE(val))
		break;

	rtc_sleep(delay);

	return MCR_DATA(val);
}

/*
 * pcfg_get_router_nic_info
 *     Get the router mfg nic header from the pcfg index
 */

static int pcfg_get_router_nic_info(int pcfg_index,char *info)
{
	return nic_info_get(pcfg_router_nic_access,
			    (nic_data_t)pcfg_index, info);
}

/*
 * read_local_router_mfgnic
 *
 *    Read the MFG nic header for each hub's local router.
 *    Store it in that nodes klconfig area.
 *    Use pcfg entries for getting to our local router.
 *    Instead of locking the router, only the hub with
 *    highest nasid will read the router nic.
 *
 * Assumptions:
 *    klcfg is fully done for routers, duplicate routers
 *    are flagged.
 */

static void read_local_router_mfgnic(void)
{
	int		i, j, rindex = PCFG_INDEX_INVALID ;
	pcfg_t		*p = PCFG(get_nasid()) ;
	pcfg_hub_t	*h ;
	pcfg_router_t	*r = NULL ;
	char		router_mfg_nic[512] ;
	int		high_rport = -1, my_rport = 0 ;
	klconf_off_t	rnic_off = NULL ;
	lboard_t	*l, *l1, *l2 ;
	klrou_t		*k = 0, *k2 = 0 ;

	*router_mfg_nic = 0 ;
	for (i = 0; i < p->count; i++) {
	    /* The first hub in pcfg is our hub */
	    if (p->array[i].any.type == PCFG_TYPE_HUB) {
		h = &p->array[i].hub;
		if (h->port.index != PCFG_INDEX_INVALID &&
		    p->array[h->port.index].any.type == PCFG_TYPE_ROUTER) {
			/* our hub has a router and this is the local router */
			rindex = h->port.index ;
			my_rport = h->port.port ;
			/*
			 * Scan this router for the hub on the
			 * highest port number.
			 */
			r = &p->array[h->port.index].router ;
			for (j = 1; j <= MAX_ROUTER_PORTS; j++) {
			    if (r->port[j].index ==
					PCFG_INDEX_INVALID) continue;
			    if (p->array[r->port[j].index].any.type
							== PCFG_TYPE_HUB) {
				if (j > high_rport) high_rport = j ;
			    }
			}

			if ((high_rport != -1) && (high_rport == my_rport))
				break ;
			else
				return ;
		}
		break ; /* Do not go beyond the first hub */
	    }
	    Speedo(0);
	}
	if (rindex == PCFG_INDEX_INVALID)
		return ;

	pcfg_get_router_nic_info(rindex, router_mfg_nic) ;
	db_printf("*** ROUTER MFG NIC %s\n", router_mfg_nic) ;
	/* Find a place to store the NICs */
	if (config_find_nic_router(get_nasid(), r->nic, &l, &k) == 0) {
	    if (k) {
		if (!(l->brd_flags & DUPLICATE_BOARD)) {
		    k->rou_info.nasid = get_nasid() ;
		    if (k->rou_mfg_nic = klmalloc_nic_off(get_nasid(),
					strlen(router_mfg_nic)))
			    strcpy((char *)NODE_OFFSET_TO_K1(get_nasid(),
				k->rou_mfg_nic), router_mfg_nic) ;
		} /* DUPLICATE_BOARD else */
		for (i=1; i<=MAX_ROUTER_PORTS; i++) {
		    if ((k->rou_port[i].port_nasid == INVALID_NASID) ||
			(k->rou_port[i].port_nasid == get_nasid()))
			continue ;
		    l1 = (lboard_t *)NODE_OFFSET_TO_K1(k->rou_port[i].port_nasid,
					   k->rou_port[i].port_offset) ;
		    if (l1->brd_type == KLTYPE_IP27)
			if (config_find_nic_router(l1->brd_nasid,
				r->nic, &l2, &k2) == 0)
			    if (k2) {
				k2->rou_info.nasid = l1->brd_nasid ;
				if (k2->rou_mfg_nic = klmalloc_nic_off(
							l1->brd_nasid,
							strlen(router_mfg_nic)))
					strcpy((char *)NODE_OFFSET_TO_K1(
							l1->brd_nasid,
							k2->rou_mfg_nic),
						router_mfg_nic) ;
			    }
		}
	    }
	}
}

#ifndef PARALLEL_NIC_READ
/*
 * read_other_router_mfgnic
 *
 *     Read in MFG NICs of all 'other' routers.
 *     These are the meta routers and hubless
 *     routers.
 *     Called only on the partition master.
 *     klcfg for all these are already alloced on any
 *     nasid in the system.
 *
 */

static void read_other_router_mfgnic(pcfg_t *p, partid_t partition)
{
    int			i, j, found_hub = 0 ;
    pcfg_router_t	*r ;
    char		mr_nic[512] ;
    char		*c;
    lboard_t		*l ;
    klrou_t		*k ;

    /* Scan all routers in pcfg */

    *mr_nic = 0 ;
    for (i=0; i < p->count; i++) {
	if ((p->array[i].any.type == PCFG_TYPE_ROUTER) && 
	    (p->array[i].router.partition == partition)) {
	    r = &p->array[i].router;
	    if (!hubless_rtr(p, r, 1))
		continue ;
	    /* 
	     * pcfg index i is a router with no active cpu 
	     * connected to it. read its nic 
	     */
	    pcfg_get_router_nic_info(i, mr_nic) ;
	    db_printf("*** M ROUTER MFG NIC %s\n", mr_nic) ;
	    l = 0 ;
	    k = 0 ;
            if (config_find_nic_router_all(p, r->nic, &l, &k,partition, 0)) {
		/* Failed to find router's lboard */
		continue ;
	    }

	    if (l) {
		k->rou_info.nasid = l->brd_nasid ;
		if (k->rou_mfg_nic = klmalloc_nic_off(l->brd_nasid,
					strlen(mr_nic))) {
		    strcpy((char *)NODE_OFFSET_TO_K1(l->brd_nasid,
				k->rou_mfg_nic), mr_nic) ;
		}
	    }
	    Speedo(0);
	}
    }
}

#else
static void 
read_other_single_router_mfgnic(pcfg_t *p, partid_t partition, int i)
{
  char mr_nic[512];
  pcfg_router_t *r;
  lboard_t *l;
  klrou_t *k;

  if(p->array[i].any.type == PCFG_TYPE_ROUTER &&
     p->array[i].router.partition == partition)
  {
    r = &p->array[i].router;
    if(!hubless_rtr(p, r, 1))
      return;
  }
  pcfg_get_router_nic_info(i, mr_nic);
  mr_nic[511] = 0;
  l = NULL;
  k = NULL;
  if(config_find_nic_router_all(p, r->nic, &l, &k, partition, 0))
    return;

  if(l)
  {
    k->rou_info.nasid = l->brd_nasid;
    if(k->rou_mfg_nic = klmalloc_nic_off(l->brd_nasid, strlen(mr_nic)))
      strcpy((char *)NODE_OFFSET_TO_K1(l->brd_nasid, k->rou_mfg_nic), mr_nic);
  }
}
 

#define ROUTER_IDX 0
#define MROUTER_IDX 1
static void
ror_find_near_parts(pcfg_t *p, partid_t partition, short index,
                    short part_idx[2], short hub_idx[MAX_ROUTER_PORTS+1],
		    short rtr_part_idx[MAX_ROUTER_PORTS+1])
{
  int i;

  part_idx[ROUTER_IDX] = part_idx[MROUTER_IDX] = PCFG_INDEX_INVALID;

  for(i = 0; i < MAX_ROUTER_PORTS+1; i++)
  {
    hub_idx[i] = PCFG_INDEX_INVALID;
    rtr_part_idx[i] = PCFG_INDEX_INVALID;
  }

  /* if supplied index is not a hub index or not in our partition, ignore */
  if(p->array[index].any.type != PCFG_TYPE_HUB ||
     p->array[index].hub.partition != partition)
    return;

  /* get attached router, if any */
  if((part_idx[ROUTER_IDX] = p->array[index].hub.port.index) == 
      PCFG_INDEX_INVALID)
    return;
  
  /* if attached router is not in our partition (!) ignore */
  if(p->array[part_idx[ROUTER_IDX]].router.partition != partition)
  {
    part_idx[ROUTER_IDX] = PCFG_INDEX_INVALID;
    return;
  }

  /* find other hub(s), attached metarouter if they are enabled & in our
     partition */
  for(i = 1; i <= MAX_ROUTER_PORTS; i++)
  {
    short port_idx = p->array[part_idx[ROUTER_IDX]].router.port[i].index;

    if(port_idx != PCFG_INDEX_INVALID)
    {
      if(p->array[port_idx].any.type == PCFG_TYPE_HUB &&
         p->array[port_idx].hub.partition == partition &&
	 !(p->array[port_idx].hub.flags & (PCFG_HUB_EXCP | PCFG_HUB_HEADLESS)))
	hub_idx[i] = port_idx;

      if(p->array[port_idx].any.type == PCFG_TYPE_ROUTER &&
         p->array[port_idx].router.partition == partition &&
	 (p->array[port_idx].router.flags & PCFG_ROUTER_META))
	part_idx[MROUTER_IDX] = port_idx;
    }
  }

  if(part_idx[MROUTER_IDX] != PCFG_INDEX_INVALID)
  {
    for(i = 1; i <= MAX_ROUTER_PORTS; i++)
    {
      short port_idx = p->array[part_idx[MROUTER_IDX]].router.port[i].index;

      /* are there any other routers attached that aren't metarouters,
         and are in our partition? */
      if(port_idx != part_idx[ROUTER_IDX] &&
         p->array[port_idx].any.type == PCFG_TYPE_ROUTER &&
	 p->array[port_idx].router.partition == partition &&
	 !(p->array[port_idx].router.flags & PCFG_ROUTER_META))
        rtr_part_idx[i] = port_idx;
    }
  }
}

#define DNIC_MAGIC 0x4E49434D41474943 /* "NICMAGIC" */
typedef struct dnic_s
{
  uint64_t	magic;		/* indicates structure is valid */
  char		mrnic[512];	/* metarouter nic that was read */
} dnic_t;

static void read_other_router_mfgnic(pcfg_t *p, partid_t partition)
{
  short idx, part_idx[2], hub_idx[MAX_ROUTER_PORTS+1];
  short rtr_part_idx[MAX_ROUTER_PORTS+1];
  int   participate = 1, i;
  dnic_t *nics;

  /* new strategy:

     phase 1:

       each local_master CPU in each set of 4 negotiates for right to
       read local router - lowest numbered active CPU slot wins

       This CPU also examines pcfg structure to determine which
       metarouter to read - it should be the metarouter connected to
       the local router

       global_barrier()

       winning CPU reads pair of routers, stores info 
       losing CPUs zero out structure

       global_barrier()

       CPU 0 reads all NICs from all CPUs - valid magic, index numbers,
       NIC entries
  */

  /* find my NASID */
  if(hub_local_master())
  {
    /* initialize magic, force nothing to be there yet */
    nics = (dnic_t *)TO_NODE_UNCAC(get_nasid(), IP27PROM_DNIC_BASE);
    nics->magic = 0ULL;

    /*db_printf("Router NIC read phase 1 - arbitration\n");*/
    idx = discover_find_hub_nasid(p, get_nasid());
    if(idx == PCFG_INDEX_INVALID)
    {
#if 0
      db_printf("*** Could not find my own nasid!  Skipping router NIC read\n");
#endif
      participate = 0;
    }

    /* find attached parts */
    ror_find_near_parts(p, partition, idx, part_idx, hub_idx, rtr_part_idx);
    
    /* is there actually a metarouter to read? */
    if(part_idx[MROUTER_IDX] == PCFG_INDEX_INVALID)
      participate = 0;

    /* if other hub is present and enabled and in our partition, and it
       has a nasid lower than mine, let it do the NIC reading */
    for(i = 1; i <= MAX_ROUTER_PORTS && participate; i++)
    {
      if(hub_idx[i] != PCFG_INDEX_INVALID &&
         p->array[hub_idx[i]].hub.nasid < get_nasid())
        participate = 0;
    }
    for(i = 1; i <= MAX_ROUTER_PORTS && participate; i++)
    {
      if(rtr_part_idx[i] != PCFG_INDEX_INVALID)
      {
        int k;

	for(k = 1; k <= MAX_ROUTER_PORTS && participate; k++)
	{
	  short pidx = p->array[rtr_part_idx[i]].router.port[k].index;

	  if(p->array[pidx].any.type == PCFG_TYPE_HUB &&
	     p->array[pidx].hub.partition == partition &&
	     !(p->array[pidx].hub.flags & (PCFG_HUB_EXCP | PCFG_HUB_HEADLESS))&&
             p->array[pidx].hub.nasid < get_nasid())
	    participate = 0;
	}
      }
    }

/*
    if(participate)
      printf("Index %d (NASID %d) doing mr index %d\n",
                idx, get_nasid(), part_idx[MROUTER_IDX]);
    else
      printf("Index %d (NASID %d) arbitrated out\n", idx, get_nasid());
*/
  }

  global_barrier(__LINE__);

  /* read the metarouter if we're supposed to */
  if(hub_local_master() && participate)
  {
    int status;

    /*db_printf("Router NIC read phase 2 - read metarouter NIC\n");*/
    memset(nics->mrnic, 0, 512);
    status = pcfg_get_router_nic_info(part_idx[MROUTER_IDX], nics->mrnic);
    nics->mrnic[511] = 0;
/*
    printf("NASID %d (index %d) read NIC index %d (status %d):\n%s\n",
              get_nasid(), idx, part_idx[MROUTER_IDX], status, nics->mrnic);
*/
    nics->magic = DNIC_MAGIC;
  }

  global_barrier(__LINE__);

  /* this is actually kind of sneaky as pcfg_t is based on per-node
     discovery.  Index values in pcfg are not consistent across nodes.
     This is a major doh.

     So, the master has to search through it's pcfg space to find the
     proper NIC of the node owning the read data, then find the
     connected router & metarouter of these boards. */

  if(hub_partition_master())
  {
    int read_nic[p->count];

    memset(read_nic, 0, sizeof(int) * p->count);
    /*db_printf("Router NIC reads, phase 3 (collection)\n");*/
    Speedo(1);
    for(i = 0; i < p->count; i++)
    {
      Speedo(0);
      if(p->array[i].any.type == PCFG_TYPE_HUB &&
         p->array[i].hub.partition == partition)
      {
        dnic_t *rnic = 
	  (dnic_t *)TO_NODE_UNCAC(p->array[i].hub.nasid, IP27PROM_DNIC_BASE);

        if(rnic->magic == DNIC_MAGIC)
	{
	  lboard_t *lb;
	  klrou_t  *kl;
	  int j, k;

	  /* have to find actual index  - will be in part_idx[MROUTER_IDX] */
	  ror_find_near_parts(p, partition, i, part_idx, hub_idx, rtr_part_idx);
	  read_nic[part_idx[MROUTER_IDX]] = 1;
/*
	  printf("From index %d (NASID %d):\n%s\n", i,
	            p->array[i].hub.nasid, rnic->mrnic);
*/
	  kl = NULL;
	  lb = NULL;
	  if(!config_find_nic_router_all(p, 
	                            p->array[part_idx[MROUTER_IDX]].router.nic,
				    &lb, &kl, partition, 0))
	  {
	    if(lb)
	    {
	      kl->rou_info.nasid = lb->brd_nasid;
	      if(kl->rou_mfg_nic = klmalloc_nic_off(lb->brd_nasid,
	                                            strlen(rnic->mrnic)))
                strcpy((char *)NODE_OFFSET_TO_K1(lb->brd_nasid,
		                                 kl->rou_mfg_nic),
                       rnic->mrnic);
	    } /* if(lb) */
	  } /* config_find_nic_router_all() */
	} /* if(rnic->magic == DNIC_MAGIC) */
      } /* if(PCFG_TYPE_HUB) */
    } /* for(i == 0..p->count) */

    for(i = 0; i < p->count; i++)
    {
      if(p->array[i].any.type == PCFG_TYPE_ROUTER && 
         (p->array[i].router.flags & PCFG_ROUTER_META) &&
         p->array[i].router.partition == partition &&
         !read_nic[i])
      {
        if(!hubless_rtr(p, &p->array[i].router, 1))
	  continue;

        db_printf("parallel NIC read did not read rtr idx %d (reading now)\n",
	       i);
	read_other_single_router_mfgnic(p, partition, i);
      }
    }
  } /* hub_partition_master() */

  global_barrier(__LINE__);
}
#endif

static void dump_router_klcfg(void)
{
    pcfg_t          *p = PCFG(get_nasid()) ;
    pcfg_hub_t      *h ;
    int 	    i, j ;
    nasid_t	    nasid ;
    lboard_t        *lb;
    klrou_t 	    *router;

    printf("n	type	off			nic	dup	port\n") ;

    for (i=0; i < p->count; i++) {
        if (p->array[i].any.type != PCFG_TYPE_HUB)
	    continue ;

        h = &p->array[i].hub;
        nasid = h->nasid ;

        lb = (lboard_t *)KL_CONFIG_INFO(nasid) ;
        while (lb) {
            lb = find_lboard_class(lb, KLTYPE_ROUTER);
            if (!lb)
                break ;
	    printf("%d	%x	%x	%x	%d	",
		lb->brd_nasid, lb->brd_type, lb, lb->brd_nic, 
		lb->brd_flags&DUPLICATE_BOARD) ;
            router = (klrou_t *)NODE_OFFSET_TO_K1(NASID_GET(lb),
                                              lb->brd_compts[0]);
	    for (j=1; j<=MAX_ROUTER_PORTS; j++) {
		nasid_t n ;
		lboard_t *l ;
		n = router->rou_port[j].port_nasid ;
	  	if (n != INVALID_NASID) {
		    l = (lboard_t *)NODE_OFFSET_TO_K1(n, 
					router->rou_port[j].port_offset) ;
	 	    if (l)
		    if (l->brd_type == KLTYPE_IP27)
			printf("%xH, ", n) ;
		    else if (l->brd_type == KLTYPE_ROUTER)
			printf("%xR, ", n) ;
		    else
			printf("%x.%x, ", n, l->brd_type) ;
		}
	 	else
		    printf("%c, ", 'x') ;
	    }
	    printf("\n") ;

            lb = KLCF_NEXT(lb);
        }
    }

}

/*
 * disable_cpus
 *
 *   Checks if the DisableA and/or DisableB environment variable
 *   is set, and disables the appropriate CPU(s).
 *
 *   This should be called after local arbitration.  It may switch
 *   the local master if the existing local master gets disabled.
 */

void disable_cpus(void)
{
    int		cpu, disA, disB, eicntA, eicntB;
    char	bufA[64], bufB[64], cnt_bufA[8], cnt_bufB[8], *ptr;

    /*
     * Try to predict whether or not we're going to end up a headless
     * node, and if so, clear NI_SCRATCH_REG0 to allow the other nodes'
     * discoveries to complete.  If we predict wrong, the other nodes
     * will still time out -- this is just an optimization.
     */

    cpu	 = hub_cpu_get();
    disA = (ip27log_getenv(get_nasid(), DISABLE_CPU_A, bufA, 0, 0) >= 0);
    disB = (ip27log_getenv(get_nasid(), DISABLE_CPU_B, bufB, 0, 0) >= 0);

#if IP27_CPU_EARLY_INIT_WAR
    /*
     * XXX: Hack to let CPUs continue even after they are disabled due to the
     * EARLY INIT problem; If disable not due to early init or due to early init     * and cnt >= IP27_CPU_EARLY_INIT_CNT_MAX, disable
     */

    ip27log_getenv(get_nasid(), EARLY_INIT_CNT_A, cnt_bufA, "0", 0);
    ip27log_getenv(get_nasid(), EARLY_INIT_CNT_B, cnt_bufB, "0", 0);

    eicntA = atoi(cnt_bufA);
    eicntB = atoi(cnt_bufB);

    if (ptr = strchr(bufA, ':')) {
        *ptr = 0;
        disA &= ((atoi(bufA) != KLDIAG_EARLYINIT_FAILED) ||
		((atoi(bufA) == KLDIAG_EARLYINIT_FAILED) &&
		(eicntA >= IP27_CPU_EARLY_INIT_CNT_MAX)));
    }

    if (ptr = strchr(bufB, ':')) {
        *ptr = 0;
        disB &= ((atoi(bufB) != KLDIAG_EARLYINIT_FAILED) ||
		((atoi(bufB) == KLDIAG_EARLYINIT_FAILED) &&
		(eicntB >= IP27_CPU_EARLY_INIT_CNT_MAX)));
    }
#endif

    if (disA && disB ||
	disA && LD(LOCAL_HUB(PI_CPU_PRESENT_B)) == 0 ||
	disB && LD(LOCAL_HUB(PI_CPU_PRESENT_A)) == 0)
	SD(LOCAL_HUB(NI_SCRATCH_REG0), 0);

    /*
     * Get the CPUs close together in time before one or both commits
     * suicide, then delay so the CPU(s) are guaranteed * to be dead
     * in time for the next hub_barrier, hub_local_master, or whatever.
     */

    hub_barrier();

    if (disA && cpu == 0 || disB && cpu != 0) {
	SET_PROGRESS(FLED_DISABLED);
	hub_alive_set(cpu, 0);
	SD(cpu ?
	   LOCAL_HUB(PI_CPU_ENABLE_B) :
	   LOCAL_HUB(PI_CPU_ENABLE_A), 0);
    }

    delay(200);
}

static int
pvers_all_partition(pcfg_t *p)
{
    int		i;

    /*
     * Noone except the gmaster have the headless node's promvers, so
     * ignore headless nodes
     */

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		!(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
		(p->array[i].hub.promvers < PARTITION_PVERS))
            return 0;

    return 1;
}

/*
 * llp_pfail_war
 *
 *   See PV #521950 for more information about this work-around.
 *   The corresponding kernel portion is mostly in ml/SN/SN0/sysctlr.c.
 *
 *   The PROM negotiates all links external to its module into 8-bit mode,
 *   and the kernel restores it back to 16 bits.
 *
 *   This may happen up to 4 times in parallel since there is no lockout
 *   between nodes.  It is presumed that vector retries will prevent
 *   this from being a problem.
 */

#define CHIPID(status_rev_id) \
	((status_rev_id)  >> RSRI_CHIPID_SHFT & \
	 RSRI_CHIPID_MASK >> RSRI_CHIPID_SHFT)

void llp_pfail_war(int mode)
{
    __uint64_t		sr0, sr1, x;

    delay(100);		/* In case link is still coming out of reset */

    if (vector_read(0, 0, RR_STATUS_REV_ID, &sr0) < 0) {
	printf("*** llp_pfail_war vector read failed\n");
	return;
    }

    if (CHIPID(sr0) == CHIPID_HUB && SN00) {
	/*
	 * Origin200 and directly connected device is a Hub.
	 * Put external network link in 8-bit mode.
	 */

	x = LOCAL_HUB_L(NI_DIAG_PARMS);
	if (mode == 8)
	    x |= NDP_LLP8BITMODE;
	else
	    x &= ~NDP_LLP8BITMODE;
	LOCAL_HUB_S(NI_DIAG_PARMS, x);

	LOCAL_HUB_S(NI_PORT_RESET, NPR_LINKRESET);

	delay(100);
    } else if (CHIPID(sr0) == CHIPID_ROUTER) {
	__uint64_t		p123_llp8  = (RDPARM_LLP8BIT(1) |
					      RDPARM_LLP8BIT(2) |
					      RDPARM_LLP8BIT(3));

	__uint64_t		p123_reset = (RPRESET_LINK(1) |
					      RPRESET_LINK(2) |
					      RPRESET_LINK(3));

	if (vector_read(6, 0, RR_STATUS_REV_ID, &sr1) < 0)
	    sr1 = 0;

	if (CHIPID(sr1) == CHIPID_ROUTER) {
	    vector_read(6, 0, RR_DIAG_PARMS, &x);
	    if (mode == 8)
		x |= p123_llp8;
	    else
		x &= ~p123_llp8;
	    vector_write(6, 0, RR_DIAG_PARMS, x);
	    vector_read(6, 0, RR_DIAG_PARMS, &x);
	}

	vector_read(0, 0, RR_DIAG_PARMS, &x);
	if (mode == 8)
	    x |= p123_llp8;
	else
	    x &= ~p123_llp8;
	vector_write(0, 0, RR_DIAG_PARMS, x);
	vector_read(0, 0, RR_DIAG_PARMS, &x);

	delay(100);

	if (CHIPID(sr1) == CHIPID_ROUTER)
	    vector_write(6, 0, RR_PORT_RESET, p123_reset);

	vector_write(0, 0, RR_PORT_RESET, p123_reset);
    } /* ...if not a router, may be SUMAC or vector read error */

    delay(100);

    LOCAL_HUB_L(NI_PORT_ERROR_CLEAR);	/* Clear LINK RESET error */
}

#if 0
void new_main(void)
{
	int i;
	__uint64_t on_value, off_value, led_addr;
	volatile __uint64_t *led_a, *led_b;
	int c;

	junkuart_init(NULL);
	junkuart_putc('\r');
	junkuart_putc('\n');
	junkuart_putc('H');
	junkuart_putc('i');

	while(1) {
		c = junkuart_getc();
		if (c)
			junkuart_putc(c);
		rtc_sleep(100000);
	}


    /*NOTREACHED*/
}
#endif

void main(void)
{
#if defined(RTL) || defined(GATE)
    void		testing(void);
#endif
    void		main_cont(int, int);
    __uint64_t		stack;
    __uint64_t		mc;
    __uint64_t		ignore_ecc;
    __uint64_t		disable_reg;
    int			premium, i, r, r2, ll, bank, peer_pled;
    int			iio_link_up;
    hubreg_t		reg;
    char		bank_str[16];
    char		bank_sz[MD_MEM_BANKS], mem_disable[MD_MEM_BANKS + 1];
    int			diag_mode, diag_stop, fastboot;
    console_t		console;
    int			module, modprom;
    u_short 		prem_mask;
    char 		buf[8];
    uchar_t		elsc_nvram_data[16];
#ifdef DEBUG_PROM_BOOT_TIME
    int tctr = 1000;
#endif

    SET_PROGRESS(PLED_INMAIN);

#if 0
#ifdef RTL
    hub_access_regs(HUB_REGS_NI);
#endif
#endif

#if TEST_LAUNCH
    test_launch();
#endif

#if defined(RTL)
    testing();
#endif

    /*
     * Disable IO if requested.
     */

    if (ip27log_getenv(get_nasid(), IP27LOG_DISABLE_IO, 0, 0, 0) >= 0)
	hub_io_disable_set(1);

    libc_device(&dev_nulluart);		/* Until a device is selected */

    /*
     * Clear perf. and interrupt registers prior to local arbitration so
     * they can be used as scratch registers that are initialized to 0.
     */

    SD(LOCAL_HUB(MD_PERF_SEL), 0);

    SD(LOCAL_HUB(MD_PERF_CNT0), 0);
    SD(LOCAL_HUB(MD_PERF_CNT1), 0);
    SD(LOCAL_HUB(MD_PERF_CNT2), 0);
    SD(LOCAL_HUB(MD_PERF_CNT3), 0);
    SD(LOCAL_HUB(MD_PERF_CNT4), 0);
    SD(LOCAL_HUB(MD_PERF_CNT5), 0);

    /*
     * Test to make sure real time clock is incrementing.
     */

    mc = rtc_time();

    delay(500);

    if (rtc_time() <= mc) {
	result_fail("rt_clock_test", KLDIAG_RTC_ERROR,
			get_diag_string(KLDIAG_RTC_ERROR));
	fled_die(FLED_RTC, FLED_NOTTY);
    }

    /*
     * Arbitrate the local master CPU.  After this, we can use hub locks,
     * which are required before local resources shared between CPU A
     * and CPU B (RTC, UARTs, IIC, etc.) can be used.
     */

    peer_pled = arb_local_master();

    hub_led_set(PLED_BARRIER);
    hub_barrier();
    hub_led_set(PLED_BARRIEROK);

    SET_PROGRESS(0);		/* Done with compare regs; clean them up */

    /*
     * Initialize I2C bus and give the ELSC some time to start sending
     * tokens.  This time may be up to 5 seconds, if the ELSC has just
     * been confused by the reset.
     */

    if (hub_local_master()) {
	hub_led_set(PLED_I2CINIT);
	i2c_init(get_nasid());
	i2c_probe(get_nasid(), ELSC_TOKEN_TIMEOUT);
	hub_led_set(PLED_I2CDONE);
    }

    hub_barrier();

    /*
     * Initialize the ELSC driver temp space.  It resides in a special
     * place in the dirty cache until later moved to RAM.  This
     * initialization doesn't actually access the ELSC.
     */

    set_elsc((elsc_t *) POD_ELSCVADDR);
    elsc_init(get_elsc(), get_nasid());
    elsc_msg_callback(get_elsc(), cb_elsc, 0);

    /* initialize NVRAM -- we cache reads to NVRAM */
    SD(LOCAL_HUB(NI_PROTECTION), ~NPROT_RESETOK);
    elsc_nvram_init(get_nasid(), elsc_nvram_data);
    SD(LOCAL_HUB(NI_PROTECTION), NPROT_RESETOK);

    /* 
     * No master should get so far behind others at this point that having
     * each clear the display will erase anyone else's status. This allows
     * us to clear the display without having to depend on things like having 
     * a node in slot 1.
     */
    if (hub_local_master())
	elsc_display_mesg(get_elsc(), "        "); /* elsc display is 8 chars (now)*/



    /* 
     * PV #556238
     * Advertise our partition and module number to the MMSC so that the
     * MMSC is coherent with the rest of the system partitioning info.
     */
    if (hub_local_master()){
      char mmMsg[48];           
      sprintf(mmMsg, "P%2d M%2d", elsc_nvram_data[12], elsc_nvram_data[8]);
      elsc_display_mesg(get_elsc(), mmMsg);
    }
    

    /*
     * Read the DIP switches.  If the system controller access fails,
     * the DIP switch values will be requested from the console later.
     * Read the power-on status.  Only the first Hub to do this gets
     * the answer, so this information is propagated throughout the
     * system later.
     */

    read_dip_switches(elsc_nvram_data);

    hub_have_elsc_set((get_BSR() & BSR_DIPSAVAIL) != 0);

    /*
     * Disable CPUs according to the Disable[AB] environment variables.
     */

    if (! DIP_NODISABLE())
	disable_cpus();

    if (hub_local_master()) {
        if (SN00) {
	    char buf[16];
            /* 
             * If SN00, module number is the slot number.
             */
           
            modprom = 0;
	    ip27log_getenv(get_nasid(), IP27LOG_MODULE_KEY, buf, "0", 0);
	    module = strtoull(buf, 0, 0);
        }
        else {
	    /*
	     * Read module number from elsc.  If inaccessible, use the last
	     * one found in the PROM, and record that we've done that in
	     * modprom.  The module number may be zero if not yet assigned.
	     */

	    modprom = (module = elsc_nvram_data[8]) <= 0;

	    if (modprom) {
	        char buf[16];
	        ip27log_getenv(get_nasid(), IP27LOG_MODULE_KEY, buf, "0", 0);
	        module = strtoull(buf, 0, 0);
	    }

        }
    }

    if (hub_local_master()) {
	/*
	 * Determine if the system has just been powered on.  Only the
	 * first node to do the power query after power-on will get True.
	 * This information is later propagated to the other nodes.
	 */

	if (elsc_power_query(get_elsc()) > 0)
	    set_BSR(get_BSR() | BSR_POWERON);
    }

    /*
     * Select pre-ioc3 system console
     */

    pod_console(&console, "jen", 0, 0, elsc_nvram_data);

    hub_barrier();

    /*
     * If DIP switches were not available, ask for them on the console.
     */

    if (hub_local_master() && (get_BSR() & BSR_DIPSAVAIL) == 0) {
	char		buf[5];

	printf("*** MSC DIP switches unavailable\n");
	printf("01:no diag      02:heavy diag    03:mfg diag    04:verbose\n");
	printf("08:global pod   10:local pod     18:no mem pod\n");
	printf("20:default env  40:bypass io6    80:no global master\n");
	printf("DIP switches [0000]: ");

	gets_timeout(buf, sizeof (buf), 5000000, "0000");

	set_BSR(get_BSR() |
		strtoull(buf, 0, 16) << BSR_DIPSHFT |
		BSR_DIPSAVAIL);
    }

    sync_bsr(BSR_DIPMASK | BSR_DIPSAVAIL);

    /*
     * Dont do io discovery if we are in the midplane diag mode.
     */
    if (DIP_MIDPLANE_DIAG()) {
        printf("*** DIP switch %d set. Will skip IO and CrayLink discovery\n",
		DIP_NUM_MIDPLANE_DIAG);
        hub_io_disable_set(1);
    }

    set_libc_verbosity(DIP_VERBOSE());

    if (hub_local_master()) {
	int		wr, ll;

	/*
 	 * Clear the register we're using to determine how many resets we've
	 * seen.
 	 */

	SD(LOCAL_HUB(PI_ERR_STACK_ADDR_B), 0);

	/*
	 * Copy the diag mode into C0_LLADDR
	 */

        ll = LD(LOCAL_HUB_ADDR(PROMOP_REG));

	diag_set_lladdr(ll);

        if ((ll & PROMOP_MAGIC_MASK) == PROMOP_MAGIC &&
                (ll & PROMOP_SKIP_DIAGS)) {
            dip_set_diag_mode(DIAG_MODE_NONE);
            diag_mode = DIAG_MODE_NONE;
        }

	/*
	 * Adjust the PROM access time (this will generally speed it up
	 * since the default is somewhat conservative).
	 * Enable ECC if supposed to (memory tests temporarily disable it).
	 */
	hub_led_elsc_set(PLED_SPEEDUP);

	/*
	 * Avoid bogus fprom_wr (XXX temporary)
	 */

	wr = IP27CONFIG.fprom_wr ? IP27CONFIG.fprom_wr : 1;

	mc = LD(LOCAL_HUB(MD_MEMORY_CONFIG));
	mc &= ~(MMC_FPROM_CYC_MASK | MMC_FPROM_WR_MASK | MMC_IGNORE_ECC |
		MMC_UCTLR_WR_MASK);
	mc |= ((__uint64_t) IP27CONFIG.fprom_cyc << MMC_FPROM_CYC_SHFT |
	       (__uint64_t) wr << MMC_FPROM_WR_SHFT |
	       (__uint64_t) 0xa << MMC_UCTLR_WR_SHFT);

	if (! IP27CONFIG.ecc_enable)
	    mc |= MMC_IGNORE_ECC;

	SD(LOCAL_HUB(MD_MEMORY_CONFIG), mc);

	hub_led_elsc_set(PLED_SPEEDUPOK);

	/*
	 * Save the pi and md error registers in local fp registers before
	 * they are cleared. After memory is initialized, these will be
	 * copied into the klconfig error structure.  Enable error
	 * indicators.
	 */

	error_save_reset();
	hub_led_elsc_set(PLED_STASH2);
	error_clear_hub_regs();
	hub_led_elsc_set(PLED_STASH3);
	error_enable_checking(SYSAD_ALL, ERROR_INTR, 1);
	hub_led_elsc_set(PLED_STASH4);
    }

    /*
     * Initialize the NI portion of the hub.
     * Wait for the link to come up.
     */

    if (hub_local_master()) {
	int		pttn;

	net_init();

	/*
	 * Perform LLP power fail work-around if machine is partitioned
	 * (partition ID is accessible and in the range 1..63)
	 */

	if ((pttn = elsc_nvram_data[12]) > 0 &&
	    pttn < 64 &&
	    ip27log_getenv(get_nasid(), "NoPFwar", 0, 0, 0) < 0)
	    llp_pfail_war(8);
    }

    /*
     * Change allocated bandwidth timeout from reset default (0xffff).
     * Note: these registers changed between Hub 1 and Hub 2 (bug #402533).
     */

    SD(LOCAL_HUB(CORE_MD_ALLOC_BW), ((0x8000ULL / 2) << 48));
    SD(LOCAL_HUB(CORE_ND_ALLOC_BW), ((0x8000ULL / 2) << 48));

    /*
     * Set up PI timeout register to be sufficiently high.
     */

    SD(LOCAL_HUB(PI_CRB_SFACTOR), 0x2000);

    hub_barrier();

#if defined(GATE)
    testing();
#endif

    /*
     * Kernel can override DIP switch settings (done when the system
     * shuts down cleanly). Read and reset "fastboot" flag for next boot.
     * Only do this if the PROMOP is valid.
     */

    ll = diag_get_lladdr();

    fastboot = ((ll & PROMOP_MAGIC_MASK) == PROMOP_MAGIC &&
		(ll & PROMOP_SKIP_DIAGS));

    diag_set_lladdr(ll & ~PROMOP_SKIP_DIAGS);

    /*
     * Initialize I/O section of Hub
     */

    if (hub_local_master()) {
	if (! hub_io_disable()) {
	    hub_led_elsc_set(PLED_INITIO);

            /*
             * hubii_init would print the now famous
             * "IO reset delay ..." to signal the start
             * boot, the first printf. We now do this
             * here instead.
             */

            printf("Starting PROM Boot process\n") ;
	    iio_link_up = hubii_init((__psunsigned_t) LOCAL_HUB(0));

	    if (iio_link_up < 0 )
		fled_die(FLED_HUBIO_INIT, FLED_POD);

	    hub_led_elsc_set(PLED_INITIODONE);
	} else
	    iio_link_up = 0;
    }

    hub_barrier();

    /*
     * Probe for IOC3 UART.  This must always be done because it
     * does all of the bridge and crossbow initialization (!!).
     * Also, initialize the UART whether we use it or not, since
     * it will be used by io6prom.
     */

    if (hub_local_master() && iio_link_up > 0) {
	int diag_settings;

	hub_led_elsc_set(PLED_CONSOLE_GET);
	diag_settings = DIP_DIAG_MODE() |
		(DIP_VERBOSE() ? DIAG_FLAG_VERBOSE : 0);
	xtalk_init(get_nasid(), diag_settings, 0);
	/* Pass the dip switch and module value also */
        /* the dip switch is used to determine the first
	   console and module is used to match ConsolePath */
	r = get_local_console(&console, diag_settings, 
			      DIP_DEFAULT_CONS(), module, 0);
	hub_led_elsc_set(PLED_CONSOLE_GET_OK);

	if (r >= 0) {
	    hub_have_ioc3_set(1);
	    ioc3uart_init(&console);
	}
    }

    hub_barrier();

    pod_console(&console, "jien", DIP_DEFAULT_CONS(), 0, elsc_nvram_data);

    hub_barrier();

    if (hub_local_master()) {
	/*
	 * Master prints main PROM header.
	 */

	printf("\n\nIP27 PROM %s\n", prom_version);
	db_printf("Local master CPU %C revision: %x\n", get_cop0(C0_PRID));
    }

    hub_barrier();

    if (! hub_local_master())
	db_printf("Local slave CPU %C revision: %x\n", get_cop0(C0_PRID));

    hub_barrier();

    if (hub_local_master()) {
	db_printf("PROM length: 0x%lx, BSS length: 0x%lx, flash count: %d\n",
	       TEXT_LENGTH, BSS_LENGTH, IP27CONFIG.flash_count);
	db_printf("Configured hub clock: %d.%d MHz\n",
		(int) (IP27CONFIG.freq_hub / 1000000),
		(int) (IP27CONFIG.freq_hub % 1000000));
	db_printf("Status of Hub IO: 0x%x 0x%lx\n", iio_link_up,
	       LD(LOCAL_HUB(IIO_LLP_CSR)));
	db_printf("Hub Rev: %lld, Module: %d from %s, Slot: %lld\n",
	       GET_FIELD(LD(LOCAL_HUB(NI_STATUS_REV_ID)), NSRI_REV),
	       module, modprom ? "PROM backup" : "MSC",
	       hub_slot_get());
	db_printf("On PROM entry: ERR_EPC=%y (%P)\n",
	       get_cop0(C0_ERROR_EPC), get_cop0(C0_ERROR_EPC));

	if (peer_pled > 0) {
	    result_fail("peer_pled", KLDIAG_OTHER_CPU_DEAD,
			 get_diag_string(KLDIAG_OTHER_CPU_DEAD));
	    db_printf("*** The other CPU on this node died with LEDs 0x%02x\n",
		   peer_pled);
	}
    }

    hub_barrier();

    /*
     * Initialize memory DIMM control registers
     * Size the local memory
     * Set up the Hub MD configuration registers
     */

    if (hub_local_master()) {
	db_printf("Configuring memory\n");

	TRACE(0);

	mdir_init(get_nasid(), IP27CONFIG.freq_hub);

	TRACE(0);

	mdir_config(0,&prem_mask);

	TRACE(0);
    }

    hub_barrier();

    premium = (LD(LOCAL_HUB(MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    /*
     * See <sys/SN/kldiag.h> for diag_mode/diag_stop values.
     * NOTE: Keep this in sync with the same code below in main_cont().
     */

    diag_mode = DIP_DIAG_MODE();
    diag_stop = DIP_DIAG_STOP();

    if (diag_mode == DIAG_MODE_MFG) {
	set_BSR(get_BSR() | BSR_MANUMODE);

	if (hub_local_master())
	    hubprintf("Manufacturing mode output initialized.\n");
    }

    hub_barrier();

    if (hub_local_master()) {
	char   *mn[4] = { "Normal", "Heavy", "None", "Manufacturing" };
	char   *sn[4] = { "Never", "Local", "Global", "No mem POD" };

	if (fastboot) {
	    printf("*** Skipping diags as requested by kernel\n");
	    diag_mode = DIAG_MODE_NONE;
	}

	if (get_BSR() & BSR_DBGNOT0)
	    printf("*** Warning: MSC debug (dbg) switches are non-zero\n");

	if (diag_mode)
	    printf("*** Diag level set to %s (%d)\n",
		   mn[diag_mode], diag_mode);
	if (DIP_VERBOSE())
	    printf("*** Info level set to verbose\n");
	if (diag_stop)
	    printf("*** Boot stop requested at %s (%d)\n",
		   sn[diag_stop], diag_stop);
	if (DIP_DEFAULT_ENV())
	    printf("*** Ignoring env. vars/using defaults\n");
	if (DIP_BYPASS_IO6())
	    printf("*** Bypassing first IO6\n");
	if (DIP_ABDICATE())
	    printf("*** Giving up global master status\n");
    }

    hub_barrier();

    if (diag_stop == DIAG_STOP_DEXPOD)
	pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "Requested by DIP switches");

#if defined (HUB_ERR_STS_WAR)
    if (hub_local_master()) {
	if (do_hub_errsts_war() == 0) {
	    printf("PANIC: HUB_ERR_STS_WAR failed\n");
	    pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "HUB_ERR_STS_WAR failed");
	}
    }
    hub_barrier();

    if (! hub_local_master()) {
	if (do_hub_errsts_war() == 0) {
	    printf("PANIC: HUB_ERR_STS_WAR failed\n");
	    pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "HUB_ERR_STS_WAR failed");
	}
    }
    hub_barrier();
#endif /* defined (HUB_ERR_STS_WAR)  */

    /*
     * The current node's NI_SCRATCH_REG0 is currently 0xffffffffffffffff,
     * as set early in entry.s.  This notified other Hubs that the current
     * node is not headless.  The other Hubs will wait until we finish
     * initializing and preparing the NI_SCRATCH_REG1 advertisement info
     * and subsequently write our NIC into NI_SCRATCH_REG0.
     */

    if (hub_local_master()) {
	nic_t		nic;
	int		gmaster;
	char		buf[32];

	/*
	 * Allow an environment variable to override the NIC.
	 */

	if (ip27log_getenv(get_nasid(), IP27LOG_OVNIC, buf, 0, 0) >= 0)
	    nic = strtoull(buf, 0, 0);
	else if (hub_nic_get(get_nasid(), 0, &nic) < 0 ||
	    nic == 0 || nic == ~0ULL) {
	    /*
	     * Hack for boards with broken NICs: use the NIC from
	     * the BaseIO card.  This allows a system with a broken
	     * NIC to function almost normally.  If the BaseIO NIC
	     * is also missing, use a random number.
	     */

	    db_printf("*** Hub NIC broken; ");

	    if (! hub_io_disable() && (nic = console.baseio_nic) != 0)
		printf("using BaseIO nic\n");
	    else {
		nic = hub_slot_get() * rtc_time();
		printf("no BaseIO, using random value\n");
	    }
	}

	db_printf("Hub NIC: 0x%lx\n", nic);

	gmaster = (ip27log_getenv(get_nasid(), IP27LOG_GMASTER, 0, 0, 0) >= 0) || (ip27log_getenv(get_nasid(),"ForceConsole", 0, 0, 0) >= 0);
	SD(LOCAL_HUB(NI_SCRATCH_REG1),
	   LD(LOCAL_HUB(NI_SCRATCH_REG1)) |
	   (hub_have_ioc3() ? ADVERT_CONSOLE_MASK : 0) |
	   (gmaster ? ADVERT_GMASTER_MASK : 0) |
	   (get_BSR() & BSR_POWERON ? ADVERT_POWERON_MASK : 0) |
	   ((diag_stop == DIAG_STOP_LOCALPOD) << ADVERT_LOCALPOD_SHFT) |
	   ((diag_stop == DIAG_STOP_GLOBALPOD) << ADVERT_GLOBALPOD_SHFT) |
	   (__uint64_t) hub_slot_get() << ADVERT_SLOTNUM_SHFT |
	   (premium ? ADVERT_PREMDIR_MASK : 0) |
	   (__uint64_t) prom_versnum << ADVERT_PROMVERS_SHFT |
	   (__uint64_t) prom_revnum << ADVERT_PROMREV_SHFT |
	   (__uint64_t) modprom << ADVERT_MODPROM_SHFT |
	   (__uint64_t) (module & 0xff) << ADVERT_MODULE_SHFT);


	db_printf("SR1 set to %y\n", LD(LOCAL_HUB(NI_SCRATCH_REG1)));

	/*
	 * Signal remote Hubs to continue.
	 */

        SD(LOCAL_HUB(NI_SCRATCH_REG0),
		(nic & ADVERT_HUB_NIC_MASK) << ADVERT_HUB_NIC_SHFT);

	db_printf("SR0 set to %y\n", LD(LOCAL_HUB(NI_SCRATCH_REG0)));
    }

    hub_barrier();

    if (! hub_local_master()) {
	module = GET_FIELD(LOCAL_HUB_L(NI_SCRATCH_REG1), ADVERT_MODULE);
	error_enable_checking(SYSAD_ALL, ERROR_INTR, 0);
    }

    hub_barrier();

    /*
     *  ALL hub bist tests are commented out of main and
     *  will only be run by converse (see bug 441289). 
     */

#if 0 /* Hub bist tests commented out */

    if (hub_local_master() && (diag_mode == DIAG_MODE_MFG)) {

	/* get current BIST status from elsc nvram */

	bist_status = get_hub_bist_status(hub_slot_get());

	if (bist_status == HUB_NO_BIST_RAN) {

	    /* write LBIST_RAN to elsc nvram and run lbist */

	    set_hub_bist_status(hub_slot_get(), HUB_LBIST_RAN);

	    printf("Running HUB LBIST test (resets the machine) ...\n");
	    lbist_hub_execute(LOCAL_BIST); /* no return--resets system */

	} else if (bist_status == HUB_LBIST_RAN) {

	    /* check lbist results */
	    r = lbist_hub_results(LOCAL_BIST, diag_mode);
	    if (r)
		fled_die(FLED_HUBBIST, FLED_CONT);

	    /* write abist_ran to elsc nvram and run abist */
	    set_hub_bist_status(hub_slot_get(), HUB_ABIST_RAN);

	    printf("Running HUB ABIST test (resets the machine) ...\n");
	    abist_hub_execute(LOCAL_BIST); /* no return--resets system */

	} else if (bist_status == HUB_ABIST_RAN) {

	    /* clear out elsc nvram */
	    set_hub_bist_status(hub_slot_get(), HUB_NO_BIST_RAN);

	    /* check abist results */

	    r = abist_hub_results(LOCAL_BIST, diag_mode);

	    if (r)
		fled_die(FLED_HUBBIST, FLED_CONT);

	} else {

	    /* ELSC nvram has illegal value in it */
	    printf("Warning: Can't determine if Bist was run; continuing boot.\n");
	}
    }

#endif /* HUB Bist tests commented out */

    /*
     * Test hub interrupts (one CPU at a time).
     */

    hub_led_elsc_set(PLED_TEST_INTS);

    if (hub_local_master()) {
	if (diag_mode != DIAG_MODE_NONE) {
	    if (hub_intrpt_diag(diag_mode) != KLDIAG_PASSED)
                printf("hub_intrpt_diag: %S/node/cpu/%d: "
		       "FAILED\n", module, hub_slot_get(), hub_cpu_get());
	}
    }

    hub_barrier();

    if (! hub_local_master()) {
	if (diag_mode != DIAG_MODE_NONE) {
	    if (hub_intrpt_diag(diag_mode) != KLDIAG_PASSED)
                printf("hub_intrpt_diag: %S/node/cpu/%d: "
		       "FAILED\n", module, hub_slot_get(), hub_cpu_get());
	}
    }

    hub_barrier();

    /*
     * Basic test of memory banks, from both CPUs.
     * Temporarily change IGNORE_ECC to 1 if not already.
     */

    mc = LD(LOCAL_HUB(MD_MEMORY_CONFIG));
    mdir_sz_get(get_nasid(), bank_sz);

    if (hub_local_master()) {
	ignore_ecc = mc & MMC_IGNORE_ECC;
	if (ignore_ecc == 0)
	    SD(LOCAL_HUB(MD_MEMORY_CONFIG), mc | MMC_IGNORE_ECC);

	disable_reg = 0;

	/*if(DIP_NODISABLE())
		ip27log_setenv(get_nasid(), NODISABLE_DIP, "1", 0);
	else
		ip27log_setenv(get_nasid(), NODISABLE_DIP, "\0",  0); */
		

	if(! DIP_NODISABLE())
	if (ip27log_getenv(get_nasid(), DISABLE_MEM_MASK, mem_disable, 0, 0)
			>= 0)
	    disable_reg = make_mask_from_str(mem_disable) ;

        LOCAL_HUB_S(MD_PERF_CNT1, disable_reg);
    }

    /* 512p time here (on global master) is 19.2 seconds */
    ATP("Memory test", &tctr);

    for (i = 0; i < 2; i++) {

        if (i == 0 && hub_local_master() || i == 1 && ! hub_local_master()) {

	    printf("Testing/Initializing memory ...............\t\t") ;

            for (bank = 0; bank < MD_MEM_BANKS; bank++) {
                __uint64_t      base, length;
                int             slot;
                int             is256MB = 0;
		int         	cpu_speed;

		/*
		 * Need to know if this is an IP27 or IP31 board when
		 * testing for 256MB dimm banks.  256MB dimms are
		 * not supported on IP27 node boards and should
		 * be disabled if found.
		 */
		cpu_speed = (int)(IP27CONFIG_NODE(get_nasid()).freq_cpu / 1000000);

                mdir_bank_get(get_nasid(), bank, &base, &length);
                slot = hub_slot_get();

                is256MB = (length == (256 * 0x100000)) ? 1 : 0;

                if (is256MB && (cpu_speed == 180 || cpu_speed == 195)) {
                        printf("\n*** WARNING: module %d slot %d: memory bank %d: 256 MB dimm banks not allowed on IP27 node boards\n", 
				module, slot, bank);

                        disable_reg = LOCAL_HUB_L(MD_PERF_CNT1);

                        disable_reg = SET_DISABLE_BIT(disable_reg);
                        disable_reg = SET_DISABLE_BANK(disable_reg, bank);

                        LOCAL_HUB_S(MD_PERF_CNT1, disable_reg);

                        printf("*** WARNING: Disabling memory bank %d in module %d slot %d\n", 
				bank, module, slot);
			continue;
                }

                if ((length > 0) && !GET_DISABLE_BANK(LOCAL_HUB_L(MD_PERF_CNT1), 
			bank)) {
		    DM_PRINTF(("\n")) ;
                    DM_PRINTF(("Memory Test: Slot n%d, Bank %d ... ",
                                slot, bank)) ;
                    if (mtest_basic(premium, TO_UNCAC(base), 0) < 0) {

                        printf("\n") ;
                        printf("%C: +----------------------------------+\n");
                        printf("%C: | Memory problems found in bank %d  |\n",
                                bank);
                        printf("%C: |     on Node board in slot n%d.     |\n",
                                slot);
                        printf("%C: +----------------------------------+\n");

                        disable_reg = LOCAL_HUB_L(MD_PERF_CNT1);

                        disable_reg = SET_DISABLE_BIT(disable_reg);
                        disable_reg = SET_DISABLE_BANK(disable_reg, bank);

                        LOCAL_HUB_S(MD_PERF_CNT1, disable_reg);

                        printf("*** WARNING: Disabling memory bank %d in module %d slot %d\n", 
				bank, module, slot);
                    } /* if mtest_basic */
                    else
                        DM_PRINTF(("PASS\n")) ;
                } /* if length > 0 */
            }
	    printf("DONE\n") ;
        }
        hub_barrier();
    }

    /* MD_MEMORY_CONFIG could have changed between last read and here */
    if (hub_local_master() && ignore_ecc == 0) {
	mc = LD(LOCAL_HUB(MD_MEMORY_CONFIG));
	SD(LOCAL_HUB(MD_MEMORY_CONFIG), mc & ~MMC_IGNORE_ECC);
    }

    hub_barrier();

    mc = LD(LOCAL_HUB(MD_MEMORY_CONFIG));
    disable_reg = LOCAL_HUB_L(MD_PERF_CNT1);

    if (GET_DISABLE_BANK(disable_reg, 0) || ((mc & MMC_BANK_MASK(0))
	>> MMC_BANK_SHFT(0) == MD_SIZE_EMPTY)) {
        if (((mc & MMC_BANK_MASK(1)) >> MMC_BANK_SHFT(1) != MD_SIZE_EMPTY) &&
		!GET_DISABLE_BANK(disable_reg, 1)) {
            if (hub_local_master()) {
                printf("*** Swapping bank 0 with bank 1\n");
                swap_memory_banks(get_nasid(), 2);
            }
        }
        else {
            printf("*** No useable RAM installed. Need working and enabled "
                "memory in bank 0 or 1\n");
            printf("*** Add working and enabled memory present in bank 0 "
                "or 1 and reset the system\n");
            ip27_die(FLED_NOMEM);
        }
    }

    hub_barrier();

    /* Dump disable results stored in MD_PERF_CNT1 into local master stack.
     * Cannot use MD_PERF_CNT1 until this is done. For eg. can't do a sync_bsr
     * between this and the mtest_basic loop above
     */

    if (hub_local_master()) {
	int	n = 0;

        if (GET_DISABLE_BIT(LOCAL_HUB_L(MD_PERF_CNT1)))
            for (bank = 0; bank < MD_MEM_BANKS; bank++)
                if (GET_DISABLE_BANK(LOCAL_HUB_L(MD_PERF_CNT1), bank))
                    mem_disable[n++] = '0' + bank;

	mem_disable[n] = 0;
    }

    hub_barrier();

    if (diag_mode != DIAG_MODE_NONE && diag_mode != DIAG_MODE_NORMAL)
	set_BSR(get_BSR() | BSR_MEMTESTS);

    sync_bsr(BSR_MEMTESTS);

#ifdef COPY_JUMP
    /*
     * Master inits a small region of memory and copies PROM there.
     */

    /* 512p time here on global master is 19.4 seconds - dt = 0.2 seconds*/
    NTP("Copy PROM code", &tctr);

    if (hub_local_master())
    	printf("Copying PROM code to memory ...............\t\t");
    hub_barrier() ;

    if (hub_local_master() && 
	(memory_copy_prom(CONFIG_12P4I ? DIAG_MODE_NONE: diag_mode) < 0)) {
	result_fail("download_prom", KLDIAG_PROM_CPY_FAILED,
		    get_diag_string(KLDIAG_PROM_CPY_FAILED));
        ip27_die(FLED_DOWNLOAD);
    }

    if (hub_local_master())
     	printf("DONE\n") ;

    hub_barrier();

    /*
     * Jump to the copy of the PROM code in uncached RAM.
     * Both CPUs use the same copy of the text.
     * Still cannot use BSS section until stack is moved to RAM.
     *
     * Running out of uncached RAM is faster than running out of the
     * PROM and will speed up the secondary cache test.
     */

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C switching to UALIAS\n");
    hub_unlock(HUB_LOCK_PRINT);

    hub_led_elsc_set(PLED_JUMPRAMU);
    tlb_ram_ualias();
    hub_led_elsc_set(PLED_JUMPRAMUOK);

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C now running out of UALIAS\n");
    hub_unlock(HUB_LOCK_PRINT);
#endif /* COPY_JUMP */

    /*
     * Now we're running out of uncached memory, which, while slow, is
     * far faster than running out of the PROM.  Initialize some more
     * memory areas, including the BSS area (zeroed) and stack.
     */

    if (hub_local_master()) {
#if IP27_CPU_EARLY_INIT_WAR
        char	buf[64], *ptr;

        if (ip27log_getenv(get_nasid(), DISABLE_CPU_A, buf, 0, 0) >= 0) {
            if (ptr = strchr(buf, ':')) {
                *ptr = 0;
                if (atoi(buf) == KLDIAG_EARLYINIT_FAILED) {
                    int		eicntA;
                    char	reason[128], cnt_bufA[8];

                    ip27log_getenv(get_nasid(), EARLY_INIT_CNT_A, cnt_bufA, "0",
			0);
                    eicntA = atoi(cnt_bufA);

                    sprintf(reason, "Enabling CPU A: Disabled %d times due "
			"to EARLY INIT problem", eicntA);
                    ed_cpu_mem(get_nasid(), DISABLE_CPU_A, NULL, reason, 0, 1);

                    sprintf(cnt_bufA, "%d", eicntA + 1);
                    ip27log_setenv(get_nasid(), EARLY_INIT_CNT_A, cnt_bufA, 0);
                }
            }
        }

        if (ip27log_getenv(get_nasid(), DISABLE_CPU_B, buf, 0, 0) >= 0) {
            if (ptr = strchr(buf, ':')) {
                *ptr = 0;
                if (atoi(buf) == KLDIAG_EARLYINIT_FAILED) {
                    int		eicntB;
                    char	reason[128], cnt_bufB[8];

                    ip27log_getenv(get_nasid(), EARLY_INIT_CNT_B, cnt_bufB, "0",
			0);
                    eicntB = atoi(cnt_bufB);

                    sprintf(reason, "Enabling CPU B: Disabled %d times due "
			"to EARLY INIT problem", eicntB);
                    ed_cpu_mem(get_nasid(), DISABLE_CPU_B, NULL, reason, 0, 1);

                    sprintf(cnt_bufB, "%d", eicntB + 1);
                    ip27log_setenv(get_nasid(), EARLY_INIT_CNT_B, cnt_bufB, 0);
                }
            }
        }
#endif

        ip27log_unsetenv(get_nasid(), "ForceConsole", 0);

	if (!modprom && module) {
	    char buf[16];
	    sprintf(buf, "%d", module);
	    /* (setenv only writes PROM if value changes) */
	    ip27log_setenv(get_nasid(), IP27LOG_MODULE_KEY, buf, 0);
	}

	if (memory_clear_prom(diag_mode) < 0) {
	    result_fail("download_prom", KLDIAG_PROM_CPY_FAILED,
			get_diag_string(KLDIAG_PROM_CPY_FAILED));
            ip27log_printf(IP27LOG_FATAL, "PROM clear failed: unuseable "
		"bank 0: mc 0x%lx", LOCAL_HUB_L(MD_MEMORY_CONFIG));
            ip27_die(FLED_DOWNLOAD);
	}
    }

    hub_barrier();

    /*
     * Flush console output since we're going to move the console and
     * ELSC structures.
     */

    flush();

    hub_barrier();

    /* find if we are requested to be too verbose */
    if(hub_local_master()) {
	verbose = 0;
        if (ip27log_getenv(get_nasid(), IP27LOG_ROUTE_VERBOSE, 0, 0, 0) >= 0) {
		verbose = 1; 
	}
    }
    hub_barrier();


    /* Set variables DisableMemMask in the ip27log */

    if (! DIP_NODISABLE())
    if (hub_local_master() && strlen(mem_disable))
         memory_disable(get_nasid(), mem_disable);

    hub_barrier();
    if (hub_local_master()) {
        sprintf(buf,"%x",prem_mask);
        ip27log_setenv(0, PREMIUM_BANKS, buf, 0);
    }
    hub_barrier();
    /*
     * Move some structures into uncached memory.
     * Its uncached because they need to work if we go back to DEX mode.
     */

    hub_barrier() ;

    set_elsc((elsc_t *) TO_UNCAC(hub_cpu_get() == 0 ?
				 IP27PROM_ELSC_BASE_A :
				 IP27PROM_ELSC_BASE_B));

    elsc_init(get_elsc(), get_nasid());

    if (hub_local_master()) {
	error_move_reset();

	/*
	 * Copy the console structure into known memory space.
	 * This needs to be copied out to the KLCONFIG space in main_cont.
	 */

	memcpy((char *) TO_UNCAC(TO_NODE(get_nasid(), IP27PROM_CONSOLE)),
	       &console, sizeof(console_t));
    }

    hub_barrier();

#ifndef RTL
    /*
     * Test the secondary cache (both CPUs) in parallel.
     */

    if (diag_mode == DIAG_MODE_NONE) {
	hub_lock(HUB_LOCK_PRINT);
	db_printf("Skipping secondary cache diags\n");
	hub_unlock(HUB_LOCK_PRINT);
    } else {
	cache_test_eframe_t 	ct_eframe;

	hub_lock(HUB_LOCK_PRINT);
	db_printf("CPU %C testing secondary cache (%d MB)\n",
	       cache_size_s() >> 20);
	hub_unlock(HUB_LOCK_PRINT);

	hub_led_elsc_set(PLED_TESTSCACHE);

	if ((r = cache_test_s(&ct_eframe)) != 0) {
	    /*
             * Disabling CPU and Resetting system after secy. cache failure
             * Need a delay for say 2 secs, to ensure we don't send a reset
             * while some other CPU might also be disabling
             * WARNING: There is one case, where a node board for some reason
             * starts very late, fails the scache test and tries to write
             * into the PROM and gets fried by the reset. But the probability
             * of this happening is VERY low
             */

	    char	*key, *disable_cpu_why, slot[4], disable_cpu_str[64];

	    disable_cpu_why = get_diag_string(r);
	    sprintf(disable_cpu_str, "%d: %s", r, disable_cpu_why);
	    key = hub_cpu_get() ? DISABLE_CPU_B : DISABLE_CPU_A;
	    get_slotname(get_my_slotid(), slot);

	    hub_lock(HUB_LOCK_PRINT);

            ed_cpu_mem(get_nasid(), key, disable_cpu_str,
		disable_cpu_why, 0, 0);

	    result_fail("scache_test", -r, disable_cpu_why);
            printf("scache_test: %S/node/cpu/%d: FAILED. "
		" Disabling and resetting system...\n", module, hub_slot_get(),
		hub_cpu_get());

	    /* Print the register snapshot at the time of occurrence of
	     * cache test failure.
	     */
	    cache_test_eframe_print(ct_eframe);
	    hub_unlock(HUB_LOCK_PRINT);

	    hub_led_elsc_set(FLED_SCACHE);

            rtc_sleep(2*1000000);
	    reset_system();
	} else
	    result_pass("scache_test", diag_mode);
    }

    cache_inval_s();

    hub_barrier();
#endif

    /*
     * Move the stack pointer into mapped space, and hence into UALIAS.
     * When we make this jump, we invalidate the D-cache and the
     * S-cache, so the dirty-exclusive cache data will disappear.
     */

    stack = (IP27PROM_BASE_MAPPED +
	     (IP27PROM_STACK_A - IP27PROM_BASE) +
	     (hub_cpu_get() << IP27PROM_STACK_SHFT));

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C switching stack into UALIAS and invalidating D-cache\n");
    hub_unlock(HUB_LOCK_PRINT);

    hub_barrier();

    hub_led_elsc_set(PLED_STACKRAM);

    elsc_nvram_copy(elsc_nvram_data);

    jump_inval((__uint64_t) main_cont, iio_link_up, diag_mode,
	       JINV_DCACHE | JINV_SCACHE,
	       stack + IP27PROM_STACK_SIZE);

    /*NOTREACHED*/
}

/*
 * main_cont
 *
 *   Continuation of main() after stack pointer is switched into RAM.
 */

void main_cont(int iio_link_up, int diag_mode)
{
    int			i;
    nasid_t		nasid;
    int			premium;
    __uint64_t		reg;
    pcfg_t	       *p;
    rtc_time_t		next	= 0;
    int			diag_settings;
    lboard_t 		*brd_ptr;
    int			module, pvallp, partition, partprom;
    int			pn_new;
    int			rou_mod_updated = 0 ;
    int			has_headless = 0 ;
#ifdef DEBUG_PROM_BOOT_TIME
    int tctr = 2000;
#endif

    hub_led_elsc_set(PLED_STACKRAMOK);

    /*
     * NOTE: Keep this in sync with the same code above in main().
     */

    diag_settings = diag_mode | (DIP_VERBOSE() ? DIAG_FLAG_VERBOSE : 0);

    premium = (LD(LOCAL_HUB(MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    module = GET_FIELD(LOCAL_HUB_L(NI_SCRATCH_REG1), ADVERT_MODULE);

    /*
     * Now that the PROM is copied, the dirty-exclusive cache is gone,
     * and the S-cache is ready, the data and BSS sections are writable.
     * The next step is to move PC and SP into cached RAM.
     */

    hub_barrier();

#ifdef COPY_JUMP
    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C switching into node 0 cached RAM\n");
    hub_unlock(HUB_LOCK_PRINT);

    hub_led_elsc_set(PLED_JUMPRAMC);
    tlb_ram_cac();
    hub_led_elsc_set(PLED_JUMPRAMCOK);

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C running cached\n");
    hub_unlock(HUB_LOCK_PRINT);
#endif /* COPY_JUMP */

    hub_barrier();

    /*
     * Initialize all memory in the first N MB of bank 0, some of
     * which is used by the PROM.  This will be faster now that we're
     * running from cached RAM.  We're still doing a fairly small
     * amount of memory because we're going to have to re-initialize
     * it all once the node ID is set.
     */

    if (hub_local_master() && (memory_init_low(diag_mode) < 0)) {
        printf("*** ERROR: Failed initializing bank 0\n");
        ip27log_printf(IP27LOG_FATAL, "memory_init_low failed: Unuseable "
		"bank 0: mc 0x%lx\n", LOCAL_HUB_L(MD_MEMORY_CONFIG));
        ip27_die(FLED_NOMEM);
    }

    hub_barrier();

    /*
     * Flush caches since they may contain areas of memory that
     * will always be accessed uncached from now on (e.g., KLCONFIG).
     */

    cache_flush();

    hub_barrier();

    /*
     * Initialize the KLCONFIG structure in low memory.
     */

    if (hub_local_master()) {
	console_t      *kl_cons;
	char	       *cons_dump;

	db_printf("Initializing kldir.\n");

	init_kldir(get_nasid());

	db_printf("Done initializing kldir.\n");

	error_setup_pi_stack(get_nasid());

	db_printf("Initializing klconfig.\n");

	init_klcfg(INVALID_NASID);

	if (iio_link_up > 0) {
	    cons_dump = (char *) TO_NODE(get_nasid(), IP27PROM_CONSOLE);
#if 0
	    kl_cons = &(KL_CONFIG_HDR(0)->ch_cons);
#endif
	    kl_cons = KL_CONFIG_CH_CONS_INFO(get_nasid()) ;

	    memcpy(kl_cons, cons_dump, sizeof(console_t));
	}

	db_printf("Done initializing klconfig.\n");
    }

    hub_barrier();

    /*
     * Initialize the NMI areas.
     */

    hub_led_elsc_set(PLED_NMI_INIT);

    init_nmi(get_nasid(), (int) hub_cpu_get());

    hub_barrier();

    set_BSR(get_BSR() | BSR_INITCONFIG);

    if (hub_local_master()) {
	/*
	 * Do IO discovery here. Initialize the klconfig structures for
	 * the node board and all other boards other than the router.
	 */

	if ((brd_ptr = init_klcfg_ip27((__psunsigned_t)
				       REMOTE_HUB(get_nasid(), 0),
				       0)) == NULL) {
            printf("*** %S: Can't init klconfig\n", module, hub_slot_get());
	    fled_die(FLED_CONFIG_INIT, FLED_POD);
	}
	/*
	 * For SN00, init a dummy midplane structure. This holds the
	 * module serial number. Pass widget base of widget 0.
	 * Make sure that this is not a SN00 with an xbox.
	 */

	if ((SN00 || CONFIG_12P4I) && !is_xbox_config(brd_ptr->brd_nasid)) {
		init_klcfg_midplane(SN0_WIDGET_BASE(brd_ptr->brd_nasid, 0),
					brd_ptr) ;
	}

	if (iio_link_up > 0) {
	    hub_led_elsc_set(PLED_IODISCOVER);
	    io_discover(brd_ptr, diag_settings);
	    hub_led_elsc_set(PLED_IODISCOVER_DONE);
	}
        else
            klcfg_hubii_stat(get_nasid());

	/*
	 * Move the stashed pi/md error registers into the error log.
	 */

	error_log_reset(brd_ptr);
    }

    hub_barrier();

    /*
     * Store information about this processor
     */

    if (! hub_local_master()) {
	update_klcfg_cpuinfo(get_nasid(), LD(LOCAL_HUB(PI_CPU_NUM)));
    }
    else
    {
	char bufA[2], bufB[2];
	int cpunum = LD(LOCAL_HUB(PI_CPU_NUM));
        int disA = (ip27log_getenv(get_nasid(), DISABLE_CPU_A, bufA, 0, 0) >= 0);
    	int disB = (ip27log_getenv(get_nasid(), DISABLE_CPU_B, bufB, 0, 0) >= 0);
 	if((cpunum == 0) && !(!disB && LD(LOCAL_HUB(PI_CPU_PRESENT_B))))
	    update_klcfg_cpuinfo(get_nasid(), 1);
 	if((cpunum == 1) && !(!disA &&LD(LOCAL_HUB(PI_CPU_PRESENT_A))))
	    update_klcfg_cpuinfo(get_nasid(), 0);
    }
  
	

    hub_barrier();

    p = PCFG(get_nasid());	/* Initially points to node 0 (local) */

    /*
     * Run BTE test on local hub before starting network discovery
     */

    if (hub_local_master()) {
	if (diag_mode != DIAG_MODE_NONE) {
	    printf("\0\0\0\0\0\0\0\0\0\0") ;  /* some delay */
	    /* XXX Removing this printf causes bte to hang. WHY ?? */
	    if (hub_bte_diag(diag_mode) != KLDIAG_PASSED)
                printf("*** hub_bte_diag: %S: FAILED\n", 
		       module, hub_slot_get());
	}
    }

    hub_barrier();

    /* 512p time here (global master) is 65.5 seconds - dt = 46.1 seconds */
    ATP("Network discovery", &tctr);

    if (hub_local_master()) {
	int		r;
        __uint64_t	ni_scratch;
	/*
	 * Local Link Tests
	 */

	if (diag_mode != DIAG_MODE_NONE) {
            int		rc;
            klinfo_t	*hub_ptr;

	    rc = hub_local_link_diag(diag_mode);

            if (brd_ptr && (hub_ptr = find_first_component(brd_ptr, 
							KLSTRUCT_HUB)))
                hub_ptr->diagval = rc;
	}

	/*
	 * Network discovery algorithm
	 *
	 *   The discovery algorithm generates promcfg, which resides in
	 *   cached memory.
	 *
	 *   Each node generates its own (different) version of promcfg in
	 *   order to find all the other nodes and their NICs.  The NIC
	 *   data allows a global master to be arbitrated.
	 */

	r = robust_discover(p, diag_mode);

	if (r) {
	    printf("*** %S: Discovery process encountered errors\n",
		   module, hub_slot_get());
	    fled_die(FLED_NET_DISCOVER, FLED_CONT);
	}

	/*
	 * Compare PROM major version numbers and print a warning message
	 * if they don't match.  This will help explain why a system may
	 * not be able to continue from here on.
	 */

	match_prom_versions(p, 0);

    }

    hub_barrier();
/* router test */
    if(hub_local_master())
    {
    	pcfg_port_t		*pp;
    	pcfg_hub_t		*hub;
	int			r;
    	if(p->count > 0)
    	{
	    		hub = &p->array[0].hub;
	    		pp = &hub->port;
                        if (pp->index != PCFG_INDEX_INVALID && p->array[pp->index].any.type == PCFG_TYPE_ROUTER)
			{
				r = router_test(pp->port, get_nasid(), 0);
				if(r < 0)
				{
					printf("Back-plane speed calculation failed.. going to die\n");
					fled_die(FLED_BACK_CALC, FLED_CONT);
				}
			}
    			else if(p->count == 2)
    			{
				__uint64_t	niparms;
				niparms = LD(LOCAL_HUB(NI_PORT_PARMS));
				niparms &= ~NPP_MAXBURST_MASK;
				niparms |= (0x12 & NPP_MAXBURST_MASK);
				SD(LOCAL_HUB(NI_PORT_PARMS),niparms);
    			}
    	}
    }
    hub_barrier();
/* router test */

    /* 512p time here (global master) is 69.96 seconds - dt = 4.5 seconds */
    ATP("Global arbitration", &tctr);
 
    if (hub_local_master()) {
        __uint64_t	sr0;
        char		buf[8];
	/*
	 * Global Arbitration
	 *
	 *   Determine the global master of all CPUs in the system (or
	 *   in the current partition if only our partition was reset).
	 */

	arb_master(GLOBAL_ARB);


	db_printf("Global master is entry %d, NIC 0x%lx, %S\n",
	       p->gmaster,
	       p->array[p->gmaster].hub.nic,
	       p->array[p->gmaster].hub.module,
	       p->array[p->gmaster].hub.slot);

        printf("Global master is %S\n", p->array[p->gmaster].hub.module,
		p->array[p->gmaster].hub.slot);

	global_barrier(__LINE__);

	SD(LOCAL_HUB(NI_SCRATCH_REG1),
	   LD(LOCAL_HUB(NI_SCRATCH_REG1)) | ADVERT_NASID_NONE);

        /*
         * Advertize partitionids. Everyone has read the HUB NIC, so we can
         * use the upper 16 bits in NI_SCRATCH_REG0
         */

        if (SN00) {
            ip27log_getenv(get_nasid(), IP27LOG_PARTITION, buf, "0", 0);
            partition = atoi(buf);
            partprom = 0;
        }
        else {
            partprom = ((partition = elsc_partition_get(get_elsc())) < 0);
            if (partprom) {
                ip27log_getenv(get_nasid(), IP27LOG_PARTITION, buf, "0", 0);
                partition = atoi(buf);
            }
        }

        p->array[0].hub.partition = partition;
        sr0 = LOCAL_HUB_L(NI_SCRATCH_REG0);
        SET_FIELD(sr0, ADVERT_PARTITION, (__uint64_t) partition);
        LOCAL_HUB_S(NI_SCRATCH_REG0, sr0);

	global_barrier(__LINE__);

        pttn_hub_partid(p, 0);

        if (partprom)
            partition = pttn_vote_partitionid(p, 0);

        if (partition < 0) {
            partition = 0;
            printf("*** %S failed to vote for pttnid\n", p->array[0].hub.module,
		p->array[0].hub.slot);
        }

        sprintf(buf, "%d", partition);
        ip27log_setenv(get_nasid(), IP27LOG_PARTITION, buf, 0);
        p->array[0].hub.partition = partition;
    }

    /* 512p time here is (global master) 79.5 seconds - dt = 10 seconds */
    ATP("Info prop.", &tctr);

    /*
     * Propagate some of the globally advertised information from
     * promcfg into local BSRs.
     */

    if (hub_local_master()) {
	reg = get_BSR() | BSR_ALLPREM;

	for (i = 0; i < p->count; i++) {
	    pcfg_hub_t	   *ph	= &p->array[i].hub;

	    if (ph->type == PCFG_TYPE_HUB &&
		!(ph->flags & PCFG_HUB_HEADLESS)) {
		if (ph->flags & PCFG_HUB_LOCALPOD)
		    reg |= BSR_LOCALPOD;
		if (ph->flags & PCFG_HUB_GLOBALPOD)
		    reg |= BSR_GLOBALPOD;
		if (ph->flags & PCFG_HUB_POWERON)
		    reg |= BSR_POWERON;
		if (~ph->flags & PCFG_HUB_PREMDIR)
		    reg &= ~BSR_ALLPREM;
	    }
	}

	set_BSR(reg);
    }

    sync_bsr(BSR_LOCALPOD | BSR_GLOBALPOD | BSR_POWERON | BSR_ALLPREM);

    /* Arbitrate module #s if we got it out of ip27log */

    if (hub_local_master() && (LOCAL_HUB_L(NI_SCRATCH_REG1) &
			ADVERT_MODPROM_MASK)) {
        int		m;
        __uint64_t	reg;

        module = discover_vote_module(p, 0);

        printf("Voted for module number. modid = %d\n", module);

        if (module < 0) {
           printf("*** %S: Module voting failed! Assign "
		"module #s to MSC\n", 0, hub_slot_get());
           fled_die(FLED_NO_MODULEID, FLED_CONT);
        }
        else {
            char	buf[8];

            reg = LOCAL_HUB_L(NI_SCRATCH_REG1);
            SET_FIELD(reg, ADVERT_MODULE, module);
            LOCAL_HUB_S(NI_SCRATCH_REG1, reg);

            discover_touch_modids(p, 0, module);

            sprintf(buf, "%d", module);
            ip27log_setenv(get_nasid(), IP27LOG_MODULE_KEY, buf, 0);

	    elsc_module_set(get_elsc(), module);
	}
    }

#if defined (HUB_ERR_STS_WAR)

    if (hub_local_master()) {
	if (do_hub_errsts_war() == 0) {
	    printf("PANIC: HUB_ERR_STS_WAR failed\n");
	    pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "HUB_ERR_STS_WAR failed");
	}
    }

    hub_barrier();

    if (! hub_local_master()) {
	if (do_hub_errsts_war() == 0) {
	    printf("PANIC: HUB_ERR_STS_WAR failed\n");
	    pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "HUB_ERR_STS_WAR failed");
	}
    }

    hub_barrier();

#endif /* defined (HUB_ERR_STS_WAR)  */

#ifdef ROUTER1_WAR
    if (hub_local_master() && p->count > 2) {	/* At least one router */
	extern	int	router_war_needed(void);
	extern	int	router_war_part1(void);
	extern	int	router_war_part2(void);
	extern	int	router_war_channel(int chan);
	int		do_war;

	reg = (__uint64_t) LOCAL_HUB(NI_SCRATCH_REG1);

	do_war = 0;

	if (hub_global_master()) {
	    if (router_war_needed()) {
		char	buf[2];
		printf("Implement router workaround? [y] ");
		gets_timeout(buf, sizeof (buf), 5000000, "y");
		if (buf[0] == 'Y' || buf[0] == 'y')
		    do_war = 1;
	    } else
		printf("Router workaround unnecessary.\n");

	    if (do_war)
		SD(reg, LD(reg) | ADVERT_ROUTERWAR_MASK);
	}

	global_barrier(__LINE__);

	if (! hub_global_master())
	    do_war = (node_vec_read(p, p->gmaster,
				    NI_SCRATCH_REG1, &reg) >= 0 &&
		      (reg & ADVERT_ROUTERWAR_MASK) != 0);

	global_barrier(__LINE__);

	if (do_war) {
	    rtc_sleep(100000);		/* Quiesce barrier traffic */

	    if (hub_global_master()) {
		printf("Implementing router workaround part 1.\n");
		router_war_part1();
		printf("Implementing router workaround part 2.\n");
		router_war_channel(1);
		router_war_part2();
		printf("Sleeping for 8 seconds\n");
		rtc_sleep(8*1000000);
	    } else {
		printf("Sleeping for 8 seconds\n");
		rtc_sleep(8*1000000);
		printf("Implementing router workaround part 2.\n");
		router_war_channel(1);
		router_war_part2();
	    }

	    global_barrier(__LINE__);		/* Uses new channel */
	}
    }
#endif /* ROUTER1_WAR */

    hub_barrier();

    /** Putting the codes for updating router's module number ***/
 
	/*
 	 * XXX
	 *
	 * Problem: If N partitions are reset simultaneously,
	 * only one of the cpus will be the GLOBAL MASTER. So,
	 * N-1 partitions will never have hub_global_master()
	 * return true. After a few steps below, after we 
 	 * execute arb_master(PARTITION_ARB), hub_partition_master()
	 * is valid. There are a few loops from here to there which
	 * use, hub_global_master. These need to be checked to see
	 * if they are OK as it is.
	 * This is leading to a situation where, under simultaneous
	 * partition reboot, the loop below is not entered, and hence
	 * all routers in all non-master partitions are getting a 
	 * module id of 0. This could also
	 * be the situation where meta routers are not getting their
	 * duplicate flag set or something leading to an inconsistent
	 * router klcfg. This gets reflected in kernel hwgraph for
	 * routers. See PV # 524970. 
	 * To fix this problem now, I will set a flag to indicate
	 * if we have gone into this loop or not. And if we have 
	 * not, I will do the same loop after arb_master(PARTITION_ARB).
	 * as if (hub_partition_master()).
	 * If the PROM Part experts confirm, we can remove this 
	 * completely and retain the latter loop. - sprasad
 	 */
   if(hub_global_master()) {
	pcfg_rou_mod_update(p) ;
	rou_mod_updated = 1 ;
    }
				  
    if (hub_global_master()) {
	char		buf[64];
	nasid_t		nasid_offset = 0;
        __uint64_t	sr1;
	int		r, i;

	/*
	 * Choose a NASID for each Hub in promcfg and update
	 * the nasid fields in promcfg.
	 */

	db_printf("Master System Topology Graph (pre-nasid_assign):\n");
	discover_dump_promcfg(p, db_printf, DDUMP_PCFG_ALL);

#if SIMPLE_NASIDS
	r = simple_assign_nasids(p);
#else
	r = nasid_assign(p, 0);
#endif

	if (r < 0) {
	    printf("*** %S: NASID calculation failed\n",
		   module, hub_slot_get());
	    fled_die(FLED_NASID_CALC, FLED_CONT);
	}

	/*
	 * If the environment variable NASIDOffset is set, add its value
	 * to each NASID that was assigned.  This results in an inconsistent
	 * system and is useful for special testing purposes only.
	 * 
	 * Note:  This doesn't work on larger systems (32p or greater).
	 *
	 * Also, determine if we will be running with coarse RegionSize.
	 * This is the case if any NASID is greater than 63.
	 */

        if (!DIP_DEFAULT_ENV()) {
		ip27log_getenv(get_nasid(), IP27LOG_NASIDOFFSET, buf, "0", 0);
	   	nasid_offset = strtoull(buf, 0, 0);
	}

	if (nasid_offset)
	    printf("*** Warning: NASIDOffset set to %d\n", nasid_offset);

	for (i = 0; i < p->count; i++) {
	    if (p->array[i].any.type == PCFG_TYPE_HUB) {
		p->array[i].hub.nasid += nasid_offset;

		if (p->array[i].hub.nasid > 63)
		    set_BSR(get_BSR() | BSR_COARSEMODE);

	    }

	}

	db_printf("Master System Topology Graph:\n");
	discover_dump_promcfg(p, db_printf, DDUMP_PCFG_ALL);

	/*
	 * Calculate the router table programming information
	 */

#if SIMPLE_ROUTES
	r = calculate_routes(p);
#else
	r = route_assign(p);
#endif

	if (r < 0) {
            printf("*** %S: Router table calculation failed\n",
		   module, hub_slot_get());
            fled_die(FLED_ROUTE_CALC, FLED_CONT);
	}

	/*
         * Distribute the router table programming information
         * to the routers and slaves constituting this partition.
         */

	if (distribute_tables() < 0) {
	    printf("*** %S: Router table distribution failed\n",
		   module, hub_slot_get());
            fled_die(FLED_ROUTE_DIST, FLED_CONT);
        }

	/*
	 * Verify memory premiumness
	 *
	 *   If any node has a nasid greater than 15, premium DIMMs
	 *   are required.  The system will stop booting here if any
	 *   node has standard DIMMs, and the nodes that don't have
	 *   premium DIMMs will be displayed.
         *   N.B.: This check is necessary and sufficient while we are
         *   coming up with a global topology knowledge.
	 */

	if ((get_BSR() & BSR_ALLPREM) == 0)
	    for (i = 0; i < p->count; i++)
		if (p->array[i].any.type == PCFG_TYPE_HUB &&
		    NASID_GET_META(p->array[i].hub.nasid) != 0) {
		    printf("*** This configuration requires "
			   "all nodes to have premium DIMMS.\n");

		    for (i = 0; i < p->count; i++) {
			pcfg_hub_t *ph = &p->array[i].hub;

			if (ph->type == PCFG_TYPE_HUB &&
			    (ph->flags & PCFG_HUB_PREMDIR) == 0)
			    printf("*** %S not premium\n",
				   ph->module, ph->slot);
		    }

                    printf("Going to die...\n");
		    fled_die(FLED_PREM_DIR_REQ, FLED_POD);
		}

        /* Set BOOTED bit in local & headless */

        sr1 = LOCAL_HUB_L(NI_SCRATCH_REG1);
        SET_FIELD(sr1, ADVERT_BOOTED, 1);
        LOCAL_HUB_S(NI_SCRATCH_REG1, sr1);

        for (i = 0; i < p->count; i++)
            if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
		IS_RESET_SPACE_HUB(&p->array[i].hub)) {
                if (node_vec_read(p, i, NI_SCRATCH_REG1, &sr1) >= 0)
                    node_vec_write(p, i, NI_SCRATCH_REG1,
				sr1 | ADVERT_BOOTED_MASK);
                else
                    printf("*** Headless HUB inaccessible\n");
            }

	/*
	 * Distribute the node IDs to the other local masters.
	 * Each will wait in the loop below to receive its node ID.
	 */

	if (distribute_nasids() < 0) {
	    printf("*** %S: Node ID distribution failed\n",
		   module, hub_slot_get());
            fled_die(FLED_NASID_DIST, FLED_CONT);
	}

    } else if (hub_local_master()) {
        __uint64_t	sr1;

	db_printf("Local System Topology Graph:\n");
	discover_dump_promcfg(p, db_printf, DDUMP_PCFG_ALL);

        /* Set BOOTED bit */

        sr1 = LOCAL_HUB_L(NI_SCRATCH_REG1);
        SET_FIELD(sr1, ADVERT_BOOTED, 1);
        LOCAL_HUB_S(NI_SCRATCH_REG1, sr1);

	/*
	 * Wait for our NASID to be assigned.
	 */

	db_printf("Local Master: Waiting for my NASID ...\n");

	reg = (__uint64_t) LOCAL_HUB(NI_SCRATCH_REG1);

	while ((LD(reg) & ADVERT_NASID_MASK) == ADVERT_NASID_NONE) {
            if (kbintr(&next))
                break;
	    rtc_sleep(100000);
	}
    }
    else {
	db_printf("Local Slave : Waiting for my NASID ...\n");
    }

    hub_barrier();

    nasid = (nasid_t) (LD(LOCAL_HUB(NI_SCRATCH_REG1)) & ADVERT_NASID_MASK);

    if (hub_local_master()) {
        char	buf[8];

	if (nasid < 0) {
	    printf("*** NASID not assigned by master\n");
	    fled_die(FLED_NO_NASID, FLED_CONT);
	    nasid = 0;
	} else {
	    db_printf("*** NASID assigned to %d\n", nasid);
 	}

        sprintf(buf, "%d", nasid);
        ip27log_setenv(get_nasid(), IP27LOG_NASID, buf, 0);

	/*
	 * Do a second version number check (see similar code above) but
	 * this time, display the NASIDs for convenience.
	 */

	if (hub_global_master())
	    match_prom_versions(p, 1);
    }
    else
	if (nasid < 0)
	    nasid = 0;

    hub_barrier();

    /*
     * Set the local node ID.  It's necessary to switch to running
     * out of UALIAS while the node ID is being changed.  The stack is
     * still in the BSS and changes along with the PC.
     */

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C switching to UALIAS\n");
    hub_unlock(HUB_LOCK_PRINT);

    hub_led_elsc_set(PLED_JUMPRAMU);
    tlb_ram_ualias();
    hub_led_elsc_set(PLED_JUMPRAMUOK);

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C running in UALIAS\n");
    hub_unlock(HUB_LOCK_PRINT);

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C Flushing and invalidating caches\n");

    hub_unlock(HUB_LOCK_PRINT);

    cache_flush();

    cache_inval_s();
    cache_inval_d();
    cache_inval_i();

    set_BSR(get_BSR() & ~BSR_DEX);

    /* time here (global master) is 149.6 seconds - dt = 70 seconds */
    /*
     * Set coarse protection mode 
     */
    if (hub_global_master()) {  
	if (set_coarse_mode() < 0) {
            printf("*** %S: Coarse mode distribution failed\n",
                   module, hub_slot_get());
            fled_die(FLED_ROUTE_DIST, FLED_CONT);
        }
    }

    /*
     * WARNING:  Time-critical region; do not add printf's
     *
     * Change the node ID.  Make sure the slave is occupied in a tight
     * internal loop at the time of the change.  Make sure the nodes are
     * closely synchronized to avoid message traffic when the node IDs
     * change.
     */

    NTP("Post- set coarse mode", &tctr);

    if (hub_local_master()) {
	db_printf("Changing node ID to %d\n", nasid);
	flush();
	global_barrier(__LINE__);
    }

    /* time here (global master) is 1168.4 seconds --
       about 520 seconds (8 minutes, 40 seconds) from behind barrier. */
    ATP("Post- set coarse mode barrier", &tctr);

    hub_barrier();

    if (hub_local_master()) {
	delay(4240);	/* about 10 msec when running uncached */
	net_node_set(nasid);
	delay(4240);	/* about 10 msec when running uncached */
    } else
	delay(8480);	/* about 20 msec when running uncached */

    hub_barrier();

    /*
     * Handle node ID change on master, then on slave.
     */

    if (hub_local_master() && nasid != 0) {
	console_t	console;

	/*
	 * Update ELSC and IOC3UART bases on the local master.
	 */

	set_elsc((elsc_t *) TO_NODE(get_nasid(), (__psunsigned_t) get_elsc()));
	elsc_init(get_elsc(), get_nasid());

	set_ioc3uart_base(TO_NODE(get_nasid(), get_ioc3uart_base()));

	/*
	 * Since the node ID changed, the directory entries are all
	 * marked owned by node ID 0, so it's necessary to reinitialize
	 * them back to unowned.
	 */

	db_printf("Reinitializing directories for " MIN_BANK_STRING " MB\n");

	mdir_init_bddir(premium,
			TO_NODE_UNCAC(get_nasid(), 0),
			TO_NODE_UNCAC(get_nasid(), MIN_BANK_SIZE));
    }

    hub_barrier();

    p = PCFG(get_nasid());			/* Address of PCFG moved */

    if (! hub_local_master() && nasid != 0) {
	console_t	console;

	/*
	 * Update ELSC and IOC3UART bases on the local slave.
	 */

	set_elsc((elsc_t *) TO_NODE(get_nasid(), (__psunsigned_t) get_elsc()));
	elsc_init(get_elsc(), get_nasid());

	set_ioc3uart_base(TO_NODE(get_nasid(), get_ioc3uart_base()));
    }

    hub_barrier();

    /*
     * Perform a global synchronization prior to accessing remote memory.
     * NOTE:  Only the global master's promcfg has valid NASID fields.
     */

    NTP("Back into cached memory (barrier)", &tctr);

    global_barrier(__LINE__);

    ATP("Back into cached memory", &tctr);

    set_BSR(get_BSR() | BSR_NETCONFIG);		/* SN0net fully configured */

    hub_barrier();

    /*
     * Move back into cached memory, except this time the
     * node ID will be part of the cached address.
     */

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C switching to node %d cached RAM\n", nasid);
    hub_unlock(HUB_LOCK_PRINT);

    hub_led_elsc_set(PLED_JUMPRAMC);
    tlb_ram_cac_node(nasid);
    hub_led_elsc_set(PLED_JUMPRAMCOK);

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C running cached\n");
    hub_unlock(HUB_LOCK_PRINT);

    hub_barrier();

    if (hub_local_master()) {
        char	mem_disable[MD_MEM_BANKS + 1];
        char	old_mem_disable[MD_MEM_BANKS + 1];

        /* 
         * Verify complete memory regions when running coarse mode.
         */ 
        if (get_BSR() & BSR_COARSEMODE) {
    	    check_memory_regions(p);
        }

	ip27log_getenv(get_nasid(), DISABLE_MEM_MASK, mem_disable, 0, 0);
        strcpy(old_mem_disable, mem_disable);

        /*
         * If fresh problems are discovered in bank 0, die
         */

	    printf("Testing/Initializing all memory ...") ;

	if (memory_init_all(get_nasid(), 1, mem_disable, diag_mode) < 0) {
	    printf("\n") ;
            if (strchr(mem_disable, '0') && (strchr(mem_disable, '1') ||
			!strchr(old_mem_disable, '0'))) {
                printf("*** No useable RAM installed. Need working and enabled "                        "memory in bank 0 or 1\n");
                printf("*** Add working and enabled memory present in bank 0 "
                        "or 1 and reset the system\n");
                ip27log_printf(IP27LOG_FATAL, "mem_init_all failed: Unuseable "
			"bank 0: mc 0x%lx\n", LOCAL_HUB_L(MD_MEMORY_CONFIG));
                ip27_die(FLED_NOMEM);
            }
        }
	printf("\t\tDONE\n") ;
	if (! DIP_NODISABLE())
        	memory_disable(get_nasid(), mem_disable);
        memory_empty_enable(get_nasid());
    }

    hub_barrier();

#ifdef MIXED_SPEEDS
    if (hub_global_master())
        router_setup_mixed_speeds(p);
#endif

    /*
     * Update local masters' nasid picture in promcfg
     */

    if (hub_local_master() && !hub_global_master()) {
        __uint64_t	ni_status_rev_id;

        if (node_vec_read(p, p->gmaster, NI_STATUS_REV_ID, &ni_status_rev_id)
		!= -1) {
            nasid_t	master_nasid = (nasid_t) GET_FIELD(ni_status_rev_id,
						NSRI_NODEID);
            pttn_pcfg_nasid_update(p, PCFG(master_nasid));
        }
        else
            printf("*** Can't find global master nasid!\n");
    }

    /*
     * Sync up the domainid, cellid and clusterid info.
     */

    if (hub_local_master())
        sync_ids(get_nasid());

    /* Check if SN0's and SN00's are CrayLinked together */
        /*
         * XXX
	 * When N partitions boot simultaneously ...
         * The global master(for all partitions) calls init headless
	 * for all headless nodes. We would have preferred the partition 
	 * master to do it, but we do not know the partition number 
	 * of the headless node.
	 * To get the partition number, from elsc or promlog, init_headless
	 * has to do a little bit of very basic hub init.
	 * So, we let the global master do init_headless, but we ensure 
	 * that any references to global master in the init_headless path
	 * means the partition master and not the currently running cpu.
         */
    if (hub_global_master()) 			/* XXX or partition master */
	init_headless(get_nasid(), diag_mode);
    else
	init_headless_worker(get_nasid());

    hub_barrier();

    if (hub_local_master())
        ip27_inventory(get_nasid());

    /*
     * Don't let anyone continue before the headless nodes are
     * initialized by the global master.
     */

    NTP("Pre-partition barrier", &tctr);

    global_barrier(__LINE__);

    ATP("Pre-partition starts", &tctr);

    /*
     * Check for partition information and setup fences in promcfg
     * rc from discover_partition
     * 0 -> partition discovered and valid
     * 1 -> partition discovered but invalid or fences with no partition
     */

    if (hub_local_master() && (pvallp = pvers_all_partition(p))) {
        int	rc;

        pttn_hub_partid(p, 1);
        pttn_router_partid(p);

        if ((rc = pttn_validate(p)) == PARTITION_YES) {
            set_BSR(get_BSR() | BSR_PART);

            pttn_pcfg_fence(p);

	    /* After this, hub_partition_master() is valid. */

            arb_master(PARTITION_ARB);

	    if (p->array[p->pmaster].hub.partition > 0) {
               printf("Partition master is %S\n", p->array[p->pmaster].hub.module,
		   p->array[p->pmaster].hub.slot);
	    }

            db_printf("*** After partitioning ***\n");
            discover_dump_promcfg(p, db_printf, DDUMP_PCFG_ALL);
        }
        else if (rc == PARTITION_INVAL) {
            printf("*** WARNING: %S: Found partition info.\n",
		   p->array[0].hub.module, p->array[0].hub.slot);
	    printf("*** WARNING: Illegal partition config.\n");
            pttn_wallin(p);
            diag_set_lladdr((diag_get_lladdr() & ~PROMOP_CMD_MASK) |
		PROMOP_RESTART);	/* Stop Autoboot */
        }
    }

    ATP("Checked partition info", &tctr);

    sync_bsr(BSR_PART);

    if ((get_BSR() & BSR_PART) && hub_global_master())
        pttn_erect_fences(p);

    if (hub_local_master() && !pvallp)
        pttn_clean_partids(p);

    ATP("Did fences in promcfg, clean partids", &tctr);

    /*
     * Ref comments above about router mod ids not being updated.
     * PV # 524970.
     * This condition should really be
     * if (hub_partition_master != hub_global_master)
     */
    if (hub_partition_master() && (!rou_mod_updated)) {
	pcfg_rou_mod_update(p) ;
    }

    ATP("Updated router modids", &tctr);

    /*
     * Wall off meta routers so that common meta routers don't point to a remote
     * partition. This is done so that klconfig doesn't span partitions
     */

    if (hub_partition_master()) {
	check_other_consoles(p);
        pttn_setup_meta_routers(p, p->array[p->pmaster].hub.partition);
        if (discover_check_sn00(p) < 0) {
            printf("*** Origin200 CrayLinked with Origin2000. Configuration "
			"unsupported!\n");
            fled_die(FLED_MIXED_SN00, FLED_POD);
        }
    }

    /*
     * The klconfig structures have incorrect nasids.  Update them now.
     */

    if (hub_local_master())
	klconfig_nasid_update(get_nasid(), 0);

    hub_barrier();

    ATP("Walled off metarouters, updated klconfig nasids", &tctr);

    /*
     * Update the config data structures, to have the correct nasids and
     * the discovery port information.  One node does this.
     */

    if (hub_partition_master()) {
	db_printf("Update config for routers connected to hubs\n");

	hub_led_elsc_set(PLED_ROUTER_CONFIG);

	/* We need to update pcfg with info about memless nodes. */
	/* If 2 partitions boot simultaneously, global master != part master */

	pcfg_memless_update(p) ;
	
	if (add_router_config(p, p->array[0].hub.partition) == -1) {
	    printf("*** %S: Freezing node (can't init router config)\n",
		   module, hub_slot_get());
	    fled_die(FLED_ROUTER_CONFIG, FLED_POD);
	}

	db_printf("Update config for hubs and hubless routers\n");

	hub_led_elsc_set(PLED_HUB_CONFIG);

	if (klconfig_discovery_update(p, p->array[0].hub.partition) < 0) {
	    printf("*** %S: Freezing node (can't init klconfig)\n",
		   module, hub_slot_get());
	    fled_die(FLED_HUB_CONFIG, FLED_POD);
	}
    }

    ATP("Updated config data structures for correct NASIDs", &tctr);

    hub_barrier();

    /*
     * Read router mfg nics and store it in klconfig area
     * along with the other nics. This is needed by prom hinv
     * and to find meta routers
     */

    if (hub_local_master())
	read_local_router_mfgnic();

    hub_barrier();

    if (hub_local_master())
	pn_new = read_midplane_mfgnic();

    hub_barrier();

    /*
     * On the global master, read nics of headless nodes
     * and meta routers.
     */

#ifndef PARALLEL_NIC_READ
    if (hub_partition_master())
#else
    /* this is now done in distributed fashion with 1/4 of the CPUs taking
       part; each CPU reads the NIC off of its local router and possibly the
       attached metarouter */
#endif
      read_other_router_mfgnic(p, p->array[0].hub.partition) ;

    hub_barrier() ;

    hub_lock(HUB_LOCK_PRINT);
    db_printf("CPU %C flushing cache\n");
    hub_unlock(HUB_LOCK_PRINT);

    cache_flush();

    hub_barrier();

    if(hub_local_master())
        write_config_elsc(pn_new);

    hub_barrier();

    ATP("Did NIC stuff", &tctr);

    /*
     * Now show the error state only on the global master.
     */

    if (hub_partition_master() && DIP_ERROR_SHOW()) {
	if (get_BSR() & BSR_POWERON)
	    printf("Suppressing error state display "
		   "(system just powered on).\n");
	else
	    error_show_reset();
    }

    /*
     * Mark off all nodes that died after klconfig init
     */

    if (hub_partition_master()) {
        klconfig_failure_update(p, p->array[0].hub.partition);
	klconfig_disabled_update(p,p->array[0].hub.partition);
	router_test_update(p);
	ust_change(p);
	/* 
	 * PV : 514190
	 * Log disabled cpu and mem info into promlog so that
	 * they later get xfered to syslog. We could have done
	 * this in the IOprom but writing to promlog causes
	 * an exception on the slave processor. When we boot
	 * unix it fails with a "cannot start all cpus".
	 */
	pcfg_syslog_info(p) ;
	/* Set the xbox_nasid in the promlog for speedos with xbox */
	xbox_nasid_set(p);
    }

    /* 
     * PV 669589.  If memory banks 0 and 1 are swapped, the correct
     * behavior is for the prom to also swap all other pairs of banks.
     * New versions of the proms know how to do this.  The IO prom must be
     * forwards and backwards compatible with all versions of the CPU
     * prom.  If we are loading an older version of the IO prom, it
     * expects the old behavior (only banks 0 and 1 swapped).  Unswap the
     * other banks to create the expected behavior.  Newer versions of the
     * IO prom know to swap the other banks back to the new behavior.
     * 
     * Each local master does this for its own node, rather than having
     * the global master do it for all nodes.  The reason for this is if
     * the global master did all the swapping and for some reason the
     * global master has an old IP27prom (while another node with a
     * swapped configuration has a new IP27prom) then the unswapping of
     * the other board would not happen and the kernel hangs.  Here, each
     * node board swaps and unswaps its own memory.
     *
     * Do this before the global barrier to ensure that all boards unswap
     * their data before the IO6prom re-swaps it.
     */ 

    if (hub_local_master())
	unswap_some_memory_banks();

    /*
     * Final barriers (no more allowed)
     * WARNING:
     *	This is the last chance before anything global needs
     * 	to be synced up.
     *	Eg: Setting a promlog variable which might be used 
     *	    by the other slaves.
     */

    NTP("Final barrier", &tctr);

    global_barrier(__LINE__);
    hub_barrier();

    ATP("Final barrier complete", &tctr);

    /* Set prom rev & ver if partitioned machine */

    if (hub_local_master()) {
        __uint64_t	sr1;

        sr1 = LOCAL_HUB_L(NI_SCRATCH_REG1);

        SET_FIELD(sr1, ADVERT_PROMVERS, prom_versnum);
        SET_FIELD(sr1, ADVERT_PROMREV, prom_revnum);

        LOCAL_HUB_S(NI_SCRATCH_REG1, sr1);
    }

    if (hub_partition_master()) {
        int		i;

        for (i = 0; i < p->count; i++) {
            if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
		(p->array[i].hub.partition == p->array[0].hub.partition))
                REMOTE_HUB_S(p->array[i].hub.nasid, NI_SCRATCH_REG1,
			REMOTE_HUB_L(p->array[i].hub.nasid, NI_SCRATCH_REG1) |
			(__uint64_t) (((ip27config_t *) IP27CONFIG_ADDR_NODE(
				p->array[i].hub.nasid))->pvers_vers) << 
				ADVERT_PROMVERS_SHFT |
			(__uint64_t) (((ip27config_t *) IP27CONFIG_ADDR_NODE(
				p->array[i].hub.nasid))->pvers_rev) << 
				ADVERT_PROMREV_SHFT);
        }
		
    }

    /*
     * Determine which CPUs should go into slave mode, which should
     * go into POD mode, and which should load the IO6 PROM.
     */

    if (get_BSR() & BSR_LOCALPOD) {
	/* Any CPU with console access goes to POD mode. */
	if (libc_device(0) == &dev_elscuart)
	    goto pod;
	if (hub_local_master())
	    goto pod;
	goto slave;
    }

    if (get_BSR() & BSR_GLOBALPOD) {
	if (hub_partition_master())
	    goto pod;
	goto slave;
    }

    if (! hub_partition_master())
	goto slave;


    /*
     * If DIP_MIDPLANE_DIAG is on, load the default segment which is
     * expected to be the midplane diag.
     */
    if (DIP_MIDPLANE_DIAG()) {
        printf("*** Loading built in midplane diag as requested by DIP "
			"switch %d\n", DIP_NUM_MIDPLANE_DIAG);
	load_execute_segment(DEFAULT_SEGMENT, 5 & ~1, diag_mode);
    }

    /*
     * Global master loads IO6 PROM.
     */

    if (iio_link_up <= 0) {
	int link_stat;

	link_stat =
	    GET_FIELD(LD(LOCAL_HUB(IIO_LLP_CSR)), IIO_LLP_CSR_LLP_STAT);
	if (link_stat == 0) {
	    printf("HubIO Link is down. Failed after reset\n");
	    printf("Cannot talk to IO board. ^C to enter POD\n");
	    fled_die(FLED_LLP_FAIL, FLED_POD);
	}
	else if (link_stat == 1) {
	    printf("HubIO Link is down. Never came out of reset\n");
	    printf("Cannot talk to IO board. ^C to enter POD\n");
	    fled_die(FLED_LLP_NORESET, FLED_POD);
	}
	else {
	    /* XXX io discovery switch temporary; remove this too */
	    printf("HubIO Link is up. "
		   "No hubIO init as requested by DIP switches?\n");
	}
    } else if (! hub_have_ioc3()) {
	moduleid_t	cons_module;
	int		cons_slot;
	nasid_t		cons_nasid;
	void 	restore_restart_state(pcfg_t *) ;

        printf("*** No console found. Searching for console...\n");

	if (get_next_console(p, &cons_module, &cons_slot, &cons_nasid) >= 0) {
	    printf("*** Found console on /hw/module/%d/slot/io%d.\n", 
			cons_module, cons_slot);
	    printf("*** Setting ForceConsole variable and resetting.\n");
            ip27log_setenv(cons_nasid, "ForceConsole", "1", 0);
	    rtc_sleep(2*1000000);
	       /*
	        * The PROM boot up process is not always
	        * smooth. It may decide to reset and start
	        * allover in the following places: 
	        *		scsi timeout
	        *		console not found
	        * 		module id probls
	        * 		many more to come ... :-(
	        * If this happens PI_ERR_STACK_A = PROMOP_REG
	        * gets a bad value due to the exception we take
	        * here. After reset, this gets loaded as the
	        * PROMOP_RESTART value - the reboot value.
	        * To workaround this store the ll value back into
	        * pi_err_stack only to be loaded back into ll :-)
	        */
	    restore_restart_state(p) ;
	    reset_system();
        } else {
            printf("*** No console found. You need a console to proceed.\n");
            printf("*** To recover: Add a BASEIO board and reset.\n");
        }
    } else {
	char 		*c = 0 ;

	/* Change the printf output if it is io6prom */
	if (!strcmp(DEFAULT_SEGMENT, "io6prom"))
		c = "BASEIO prom" ;
	else
		c = DEFAULT_SEGMENT ;
	
	if (xbox_nasid_get() == INVALID_NASID) {
		printf("Loading %s .......................\t\t", c);
		if (SN00) 
			load_execute_segment(DEFAULT_SEGMENT, 5 & ~1, 
					     diag_mode);
		else
			load_execute_segment(DEFAULT_SEGMENT, 5, diag_mode);
	} else {
		printf("Loading %s from XBOX Flashprom....\t\t", c);
		load_execute_segment(DEFAULT_SEGMENT, 5, diag_mode);
	}
	printf("Failed to load segment\n");
    }

 pod:
    pod_mode(-POD_MODE_CAC, KLDIAG_PASSED, "Entering POD mode");

 slave:
    hub_lock(HUB_LOCK_PRINT);
    printf("Local %s entering slave loop\n",
	   hub_local_master() ? "master" : "slave");
    hub_unlock(HUB_LOCK_PRINT);

    LAUNCH_LOOP();
}

void
restore_restart_state(pcfg_t *p)
{
	int	i ;

        for (i = 0; i < p->count; i++)
            if (p->array[i].any.type == PCFG_TYPE_HUB) 
            	SD(	REMOTE_HUB_ADDR(p->array[i].hub.nasid, PROMOP_REG), 
			diag_get_lladdr());

}

static
int read_midplane_mfgnic(void)
{
	pcfg_t *p;
	int r,i,nasid,module;
	klmod_serial_num_t  *serial_comp;
	klhub_t *hubp;
	lboard_t	*l;
	int least_nasid = 1024;
	char nic[1024];
	char *c;
	int pn = 0;
	int memless = 0;

	

	module = elsc_module_get(get_elsc());
	p = PCFG(get_nasid());
	nasid = p->array[p->gmaster].hub.nasid;
	
	p = PCFG(nasid);
	for(i=0; i<p->count; i++)
	{
		if(p->array[i].any.type == PCFG_TYPE_HUB)
		{
			if(p->array[i].hub.module != module)
				continue;
			if(p->array[i].hub.slot == 1) 
			{
			    if(!(p->array[i].hub.flags & PCFG_HUB_MEMLESS))
			    {
				return 0;
			    }
			    else
			    {
				memless = 1;
			    }
			}
			if(p->array[i].hub.nasid < least_nasid && !(p->array[i].hub.flags & PCFG_HUB_HEADLESS))
				least_nasid = p->array[i].hub.nasid;
		}	
	}
	if(least_nasid != get_nasid())
		return 0;
	
	if (!(l = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
                        KLTYPE_MIDPLANE))) {
                printf("Can't find MIDPLANE lboard on nasid %d\n", nasid);
                return 0;
        }

        serial_comp = NULL;

        if (!(serial_comp = GET_SNUM_COMP(l))) {
                printf("WARNING: Cannot find serial # comp. on midplane brd "
                        "on nasid %d\n", nasid);
		return 0;
	}
	else
	{
	printf("Reading module NIC ...........\t\t");
    	if ((r = elsc_mfg_nic_get(get_elsc(), nic, 1)) < 0)
	{
		printf("Could not get module NIC: %s\n", elsc_errmsg(r));
		return 0;
	}
	if (!(l = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
                        KLTYPE_IP27))) {
                printf("Can't find MIDPLANE lboard on nasid %d\n", nasid);
                return 0;
        }
        if(!memless && (hubp = (klhub_t *)find_component(l, NULL, KLSTRUCT_HUB))) {
	    if(hubp->hub_mfg_nic)
	    {
		strcat(nic,(char *)NODE_OFFSET_TO_K1(nasid,hubp->hub_mfg_nic));
	    }
	    else
		printf("WARNING no mfg nic for nasid %d\n",nasid);
	    if(hubp->hub_mfg_nic = klmalloc_nic_off(nasid, strlen(nic)))
			strcpy((char *)NODE_OFFSET_TO_K1(nasid, hubp->hub_mfg_nic), nic) ;
        }
        

	if (c = strstr(nic, "MODULEID")) {
             int i;
             char temp[MAX_SERIAL_NUM_SIZE];

                c = strstr(c, "Serial:") ;

                for (i = 0;i<MAX_SERIAL_NUM_SIZE &&
                             (*(c+7+i) != ';');i++) {
                        temp[i] = *(c+7+i);
                }
                if (i<MAX_SERIAL_NUM_SIZE)
                        temp[i] = '\0';

                DB_PRINTF(("Serial Number: %s\n",
                       temp)) ;
                encode_str_serial(temp,serial_comp->snum.snum_str);
                        }

	}
	if(c = strstr(nic,"MPLN"))
	{
		while(*c != ';')
			c--;
		c-= 3;
		pn = htol(c);
		if(pn < 9)
			pn = 0;
	}

	printf("DONE\n");
	return pn;
}


static void router_search_pcfg( pcfg_t *p, int i)
{
    int j,k;
    pcfg_port_t *ppt, *ppt1;
	/** check it's port 4 */	
	  ppt = &(p->array[i].router.port[4]);
	  if((j=find_router_module(p, ppt))>0)
	  {
		p->array[i].router.module = j;
	  }
	  else
	  {
	  	ppt = &(p->array[i].router.port[5]);
	  	if((j=find_router_module(p, ppt))>0)
		{
		     p->array[i].router.module = j;
		}
		else
		{
	  		ppt = &(p->array[i].router.port[6]);
			j = ppt->index;
			if(j == -1)
				printf("ERROR: unconnected router\n");
			else if(p->array[j].any.type == PCFG_TYPE_HUB)
			{
				p->array[i].router.module = p->array[j].hub.module;
			}
			else
			{
				ppt1 = &(p->array[j].router.port[4]);

	  			if((k=find_router_module(p, ppt1))>0)
				{
		     			p->array[i].router.module = k;
		     			p->array[j].router.module = k;
				}
	  			else
	  			{
	  				ppt1 = &(p->array[j].router.port[5]);
	  				if((k=find_router_module(p, ppt1))>0)
					{
		     				p->array[i].router.module = k;
		     				p->array[j].router.module = k;
					}
					else
					{
						printf("ERROR: router connected to unconnected router\n");
					}
				}
			}
		}
	}
}

static 
int find_router_module(pcfg_t *p, pcfg_port_t *ppt)
{
	if(ppt->index<0)
		return(ppt->index);
	else
		return(p->array[ppt->index].hub.module);
}

static void
pcfg_rou_mod_update(pcfg_t *p)
{
        int i;

        for(i=0; i<p->count; i++) {
                if(p->array[i].any.type == PCFG_TYPE_ROUTER) {
                        if(p->array[i].router.flags & PCFG_ROUTER_META)
                                continue;
                        router_search_pcfg(p, i);
                }
        }
}

static void
pcfg_memless_update(pcfg_t *p)
{
	char 		bank_sz[MD_MEM_BANKS+1] ;
	int		i ;
	pcfg_hub_t	*ph ;

	ForAllPcfg(p,i) {
		if (pcfgIsHub(p,i) && IS_RESET_SPACE_HUB(&p->array[i].hub)) {
			ph = pcfgGetHub(p,i) ;
			mdir_sz_get(ph->nasid, bank_sz) ;
			if (	(bank_sz[0] == MD_SIZE_EMPTY) &&
				(bank_sz[1] == MD_SIZE_EMPTY)) {
				ph->flags |= PCFG_HUB_MEMLESS ;
			}
		}
	}
}

static
void write_config_elsc(int pn_new)
{
	pcfg_t *p;
	int r,i,nasid,module;
	lboard_t *brd_ptr;
        int nas_array[4];
        int nas_count = 0;
        int chosen_nasid = 5000; /* arbitrarily large number */
        int slot_one = -1;
	klhub_t *hubp = NULL;
	char *c;
        __uint64_t pn = 0;
	__uint64_t tmp;
	pcfg_hub_t	*ph ;

	nasid = get_nasid();
	
	p = PCFG(nasid);
	module = elsc_module_get(get_elsc());
        if(module<0)
           printf("%s\n",elsc_errmsg(module));
	for(i=0; i<p->count; i++)
	{
		if (p->array[i].any.type == PCFG_TYPE_HUB)
		{
			ph = (pcfg_hub_t *)&p->array[i].hub ;
			if ((p->array[i].hub.module != module) ||
			    (IS_HUB_MEMLESS(ph)))
				continue;
			if(p->array[i].hub.slot == 1)
				slot_one = nas_count;
			nas_array[nas_count++] = p->array[i].hub.nasid;
   			if((p->array[i].hub.nasid < chosen_nasid) && !(p->array[i].hub.flags & PCFG_HUB_HEADLESS ))
			        chosen_nasid = p->array[i].hub.nasid;
		}
	}
        if(nasid != chosen_nasid)
	   return;
	/* here also check the MAXBURST value for the midplane */
	if(slot_one >= 0)
	{
		brd_ptr = (lboard_t *)KL_CONFIG_INFO(nas_array[slot_one]);
		if(brd_ptr)
			hubp = (klhub_t *)find_component(brd_ptr, NULL,
                                KLSTRUCT_HUB);
                if(hubp)
		   if(hubp->hub_mfg_nic)
                   {
			if(c = strstr((char*)NODE_OFFSET_TO_K1(
                                nas_array[slot_one],
                                hubp->hub_mfg_nic), "MPLN"))
			{
				while(*c != ';')
					c--;
				c-= 3;
				pn = htol(c);
				if(pn < 9)
					pn = 0;
				
			}	
				
		   }
		    
	}
        for(i=0; i<nas_count; i++)
        {
	brd_ptr = (lboard_t *)KL_CONFIG_INFO(nas_array[i]);
	if(pn || pn_new)
	{
		tmp = LD(REMOTE_HUB(nas_array[i],IIO_ILCSR));
		tmp &= 0xffffffffull;
		tmp |= 0x18ull<<32;
		SD(REMOTE_HUB(nas_array[i],IIO_ILCSR),tmp);
	}
 	       while(brd_ptr)
        	{
        		if ( !(brd_ptr->brd_flags & DUPLICATE_BOARD))
            			dump_in_elsc(brd_ptr);
        		brd_ptr = KLCF_NEXT(brd_ptr);
        	}
	}


}

#define MSC_NVRAM_LENGTH	0x700
#define MSC_NVRAM_MSG_MAX_LEN	256

static
void dump_in_elsc(lboard_t *l)
{
	char                buf[MSC_NVRAM_MSG_MAX_LEN] ;
	char 		    buf1[MSC_NVRAM_MSG_MAX_LEN];
	static int 	    start_addr = 0;
	int 		    i;
	klinfo_t	    *k;

	switch(KLCLASS(l->brd_type)) {
           case KLCLASS_NODE : 
    		sprintf(buf,"NODE BOARD nasid %d prom-version %d ",
            		l->brd_nasid, l->brd_promver);
                break;
           case KLCLASS_IO :
                if(l->brd_type == KLTYPE_BASEIO)
    			sprintf(buf,"BASEIO BOARD prom-version %d ", 
				l->brd_promver);
                else
    			sprintf(buf,"IO BOARD ");
                break;
	   case KLCLASS_ROUTER :	
    		sprintf(buf,"ROUTER BOARD ");
                break;
	   case KLCLASS_MIDPLANE :	
    		sprintf(buf,"MIDPLANE BOARD ");
                break;
	   case KLCLASS_GFX :	
    		sprintf(buf,"GRAPHICS BOARD ");
                break;
	   case KLCLASS_UNKNOWN :	
    		sprintf(buf,"UNKNOWN BOARD ");
                break;
        }

    	sprintf(buf1,"nic %lx revision %d \n\r", l->brd_nic, l->brd_brevision);
        strcat(buf,buf1);
     	if(start_addr+strlen(buf)<MSC_NVRAM_LENGTH)
    		elsc_nvram_write(get_elsc(),start_addr,buf,strlen(buf)+1);
    	start_addr += strlen(buf);
}


static
void klconfig_disabled_update(pcfg_t *p, partid_t partition)
{
	int r,i, nasid, module, prtid;
	char disable_string[64];
	int disN;
	
        for(i=0; i<p->count; i++)
	{
		if ((p->array[i].any.type == PCFG_TYPE_HUB ) &&
		   (p->array[i].hub.partition == partition ))
		{
			nasid = p->array[i].hub.nasid;
			if(p->array[i].hub.flags & PCFG_HUB_MEMLESS)
			{
				init_klcfg_ip27_disabled((__psunsigned_t)
                                       REMOTE_HUB(nasid, 0),
                                       0,p->array[0].hub.nasid);
			}
		}
	}
			
}

void ust_change(pcfg_t *p)
{
    elsc_t *e;
    int r;
    char c;
    db_printf("Entering ust_change\n");
    if(SN00)
	return;
    if(p->count == 2 && p->array[0].hub.nasid == get_nasid() 
    && p->array[1].any.type == PCFG_TYPE_HUB)
    {
	/* two hubs with null router */
	r = elsc_ust_read(get_elsc(), &c); 
	if( r < 0)
		printf("Error in ust_change: %s\n",elsc_errmsg(r)); 
        if(p->array[0].hub.slot == 1 || p->array[0].hub.slot == 3)
		r = elsc_ust_write(get_elsc(), (c | 0x40) & 0x7f); 
        if(p->array[0].hub.slot == 2 || p->array[0].hub.slot == 4)
		r = elsc_ust_write(get_elsc(), c & 0x3f); 
	if( r < 0)
		printf("Error in ust_change: %s\n",elsc_errmsg(r)); 
    }
}
	
/*
 * xbox_nasid_set	
 * 	This routine sets the xbox_nasid promlog variable to the nasid
 * 	of the node attached to the xbox for all the nodes in our
 * 	partition.
 */
static void
xbox_nasid_set(pcfg_t	*p)
{
	int 		i;
	nasid_t		nasid,xbox_nasid = INVALID_NASID;
	int		my_partition;
	
	/* If we are not a speedo then there is no question 
	 * of an xbox being attached.
	 */
	if (!SN00)
		return;

	/* Get this node's partition id */
	my_partition = p->array[0].hub.partition;

	/* In the first pass try to figure out if there
	 * is a nasid attached to the xbox in this partition.
	 * There will one such nasid if at all.
	 */
        for (i = 0; i < p->count; i++) {
		/* If we are looking at a hub in the same partition as 
		 * ours check if there is an xbox attached
		 */
		if (p->array[i].any.type == PCFG_TYPE_HUB &&
		    p->array[i].hub.partition == my_partition) {
			nasid = p->array[i].hub.nasid;
			/* Clear the previous xbox_nasid promlog value */
			ip27log_xbox_nasid_clear(nasid);
			if (is_xbox_config(nasid))
				xbox_nasid = nasid;
		}
	}
	/* There is no xbox in this partition. No need to set the
	 * promlog variable.
	 */

	if (xbox_nasid == INVALID_NASID)
		return;
	/* In the second pass set the xbox_nasid promlog variable.
	 */
        for (i = 0; i < p->count; i++) {
		/* If we are looking at a hub in the same partition as 
		 * ours set the xbox_nasid.
		 */
		if (p->array[i].any.type == PCFG_TYPE_HUB &&
		    p->array[i].hub.partition == my_partition) {
			ip27log_xbox_nasid_set(p->array[i].hub.nasid,
					       xbox_nasid);
		}
	}
}

int 
set_coarse_mode(void)
{
    pcfg_t             *p;
    int                 i;
    int                 rc = -1;
    pcfg_hub_t         *ph;
    __uint64_t          reg;

    unsigned envval = 0;
    char buf[64];

    if (!DIP_DEFAULT_ENV()) {
	ip27log_getenv(get_nasid(), "force_coarse", buf, 0, 0);
    	envval = (unsigned)strtoull(buf, 0, 0);
        if (envval)
	   printf ("*** Warning: force_coarse = %d\n", envval);
    }

    if (!envval && !(get_BSR() & BSR_COARSEMODE)) {
	return 0;
    }

    printf("Note: Setting coarse protection mode ");
    Speedo(1);

    p = PCFG(get_nasid());
    ph = &p->array[0].hub;

    SD(LOCAL_HUB(NI_STATUS_REV_ID),
           LD(LOCAL_HUB(NI_STATUS_REV_ID)) & ~NSRI_REGIONSIZE_MASK);

    for (i = 1; i < p->count; i++) {
        if ((p->array[i].any.type == PCFG_TYPE_HUB) && 
	    IS_RESET_SPACE_HUB(&p->array[i].hub)) {
            ph = &p->array[i].hub;

            db_printf("Set coarse mode at Hub: index %d, nic 0x%lx, nasid %d\n",
                      i, ph->nic, ph->nasid);

            if (node_vec_read(p, i, NI_STATUS_REV_ID, &reg) < 0)
                    goto done;

	    if (ph->flags & PCFG_HUB_HEADLESS) {
	       /*
	        * Headless node -- set its real NASID
	        */

	       db_printf("> Headless Hub: "
	   	         "index %d, nic 0x%lx, nasid %d\n",
		         i, ph->nic, ph->nasid);


	       SET_FIELD(reg, NSRI_NODEID, ph->nasid);
	    }

            SET_FIELD(reg, NSRI_REGIONSIZE, REGIONSIZE_COARSE);

            if (node_vec_write(p, i, NI_STATUS_REV_ID, reg) < 0)
                    goto done;
    	    Speedo(0);
	}
    }

    rc = 0;

done:
    printf("......\t\tDONE\n");
    return rc;
}

void
Speedo(int initflag)
{
#ifdef SN0XXL
   if (hub_global_master()) {
        static int i = 0;

        if (initflag) i = 0;

        switch (++i%4) {
                case 0:
                        printf("|\b");
                        break;
                case 1:
                        printf("\\\b");
                        break;
                case 2:
                        printf("-\b");
                        break;
                case 3:
                        printf("/\b");
                        break;
        }
    }
#endif
}

/* 
 * Verify complete memory regions when running coarse mode.
 * 
 * There are 8 nodes per region in coarse mode.  They must
 * consist of 8 consecutive NASIDs.  We also cannot run with 
 * headless nodes when running in coarse mode.
 */
void
check_memory_regions(pcfg_t *p)
{
	int i;
	int has_headless = 0;
	int bad_region = 0;
	unsigned region1;
	unsigned region2;
    	unsigned regions_32p[MAX_METAID]; 

	for (i=0; i < MAX_METAID; i++)
		regions_32p[i] = 0;

	/*
	 * If BSR_COARSEMODE, then we need to verify that all
	 * memory regions are complete (some number of 8 node
   	 * sets each with consecutive NASIDs).  
   	 *
	 * Should we try and recover automatically here?
	 */
        for (i = 0; i < p->count; i++) {
            	if (p->array[i].any.type == PCFG_TYPE_HUB) {
                   unsigned meta, local;
                   unsigned module, slot;
    		   elsc_t   remote_elsc;
	           /*
	            * Coarse mode requires that all hubs have at least
	            * one cpu.  If we have a headless node, we cannot initialize
                    * the memory region to which it belongs.  Issue an error
	            * and abort.
	            *
	            * Should we try and recover automatically here?
	            */
                   if (p->array[i].hub.flags & PCFG_HUB_HEADLESS) {

                      elsc_init(&remote_elsc, p->array[i].hub.nasid);
                      module = elsc_module_get(&remote_elsc);
		      slot = SLOTNUM_GETSLOT(get_node_slotid(p->array[i].hub.nasid));


                      printf("*** Incomplete memory region: Cause: headless node NASID %d\n",
		   	   	p->array[i].hub.nasid);
                      printf("*** Enable/replace the node or power down the rack and reset the system\n");
		      if (module > 0)
                         printf("***     Headless node found at %S\n", 
				module, slot);
                      has_headless = 1;
		   }

                   /*
                    * Keep track of the local nodes included in each
                    * 32p region.
                    */
                   meta = NASID_GET_META(p->array[i].hub.nasid);
                   local = NASID_GET_LOCAL(p->array[i].hub.nasid);
                   regions_32p[meta] |= (1<<local);
               }

        }
	/*
	 * Check for complete memory regions (8 contiguous nasids per region)
	 */
	for (i=0; i < MAX_METAID; i++) {
	      region1 = regions_32p[i] & 0xff;
	      region2 = (regions_32p[i]>>8) & 0xff;
	      if ((region1 && (region1 != 0xff)) ||
	          (region2 && (region2 != 0xff))) {   
	   	     unsigned nasidlow, nasidhi;
		     /* incomplete region */
		     nasidlow = (i<<4);
		     nasidhi  = nasidlow + 7;
	             if (!region1 || (region1 == 0xff)) {
		         nasidlow += 8;
	             }
	             if (region2 && (region2 != 0xff)) {   
		         nasidhi += 8;
	             }
                     printf("*** Incomplete memory region: Cause: missing nodes\n");
                     printf("***     Incomplete memory region NASIDs: %d-%d\n", nasidlow, nasidhi);
				
                     printf("***     Coarse mode requires a multiple of 8 nodes in each 32p cube\n");
                     printf("*** Replace the node(s) or power down the rack(s) and reset the system\n");
		     bad_region = 1;
	       }
	}
	if (has_headless || bad_region) {
    		pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "Going to POD");
	}
}
