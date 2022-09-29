/***********************************************************************\
 *	File:		discover.c					*
 *									*
 *	Contains the algorithm for discovering and managing the		*
 *	distributed system's topology.					*
 *									*
 \***********************************************************************/

#ident "$Revision: 1.110 $"

#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/router.h>
#include <sys/SN/gda.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/addrs.h>
#include <rtc.h>
#include <hub.h>
#include <libkl.h>
#include "ip27prom.h"
#include "discover.h"
#include "libasm.h"
#include "libc.h"
#include "rtr_diags.h"
#include "pvers.h"

#define DEBUG_NETWORK		0
#define MAX_RESET_TIME		(45 * 1000000)	/* usec */
#define DISCOVER_NETWORK	0

#if defined(DISCTEST) || DEBUG_NETWORK
#define db_printf printf
#endif

extern int router_led_error(pcfg_t *, int, char);

typedef struct vertex_elem_s {
    int		pcfg_indx;
    net_vec_t	vec;
    __uint64_t	status;
} vertex_elem_t;

typedef struct queue_s {
    char		head;
    char		tail;
    vertex_elem_t	array[MAX_ROUTERS];
} queue_t;

static void queue_init(queue_t *q)
{
    q->head = q->tail = 0;
}

static int queue_empty(queue_t *q)
{
    return (q->head == q->tail);
}

static void queue_push(queue_t *q, vertex_elem_t *elem)
{
    q->array[q->head].pcfg_indx = elem->pcfg_indx;
    q->array[q->head].vec = elem->vec;
    q->array[q->head++].status = elem->status;

    q->head %= MAX_ROUTERS;

    if (q->head == q->tail)
        printf("WARNING: Router vertex queue limit %d exceeded!\n",
		MAX_ROUTERS);
}

static void queue_pop(queue_t *q, vertex_elem_t *elem)
{
    elem->pcfg_indx = q->array[q->tail].pcfg_indx;
    elem->vec = q->array[q->tail].vec;
    elem->status = q->array[q->tail++].status;

    q->tail %= MAX_ROUTERS;
}

/*
 * get_chipid
 *
 *   This test is called by the discover_object function during
 *   discovery. It reads the STATUS_REV_ID register and returns
 *   the contents, and a successful diag_rc. If the read fails
 *   it tries several times, updates the relevant promcfg entry
 *   with bad path info, and returns the failing diag_rc.
 *
 *   This test is intrinsic to the prom code and runs at all
 *   times. Therefore it has no diag_mode variable passed in to it,
 *   and does not return diag_rc.
 *
 */

static int get_chipid(pcfg_t *p,
		      int from_index,		/* Index on source chip */
		      int from_port,		/* Valid if from router */
		      net_vec_t from_path,	/* Path to source chip */
		      net_vec_t path,		/* Path to new chip */
		      __uint64_t *status)
{
    int			r;
    pcfg_hub_t	       *ph;
    pcfg_router_t      *pr;

    /*
     * Perform read of status rev id reg to see what's out there.
     */

    r = vector_read(path, 0, NI_STATUS_REV_ID, status);

    if(verbose) {
    db_printf("get_chipid: path=0x%lx status=%y r=%d\n", path, *status, r);
    }

    if (r < 0) {

	printf("discover_object: vector read 0x%lx NSRI failed: %s\n",
	       path, net_errmsg(r));

	/* retry read */

	if ((r = vector_read(path, 0, NI_STATUS_REV_ID, status)) < 0) {
	    printf("discover_object: vector re-read 0x%lx NSRI failed: %s\n",
		   path, net_errmsg(r));

	    /* update promcfg entry on source chip with bad path */

	    if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

		ph = &p->array[from_index].hub;
		ph->flags |= PCFG_HUB_PORTSTAT1;

	    } /* if hub */

	    else { /* router */

		pr = &p->array[from_index].router;
		pr->portstat1 |= 1 << from_port;

	    } /* router */

	} /* if status on retry */

    } /* if status on 1st read */

    return r;
}

/*
 * Hub/router search algorithm
 *
 *   All nodes perform the search simultaneously during boot.
 *    Each node builds its own list of hubs and routers.
 *
 *   On entry:
 *    All hub NASIDs (physical node numbers) must be set to 0.
 *    All router RR_SCRATCH_REG0's and RR_SCRATCH_REG1's must be 0.
 *    >>> NB: RR_SCRATCH_REG1 is only 16 bits.
 *    NI_SCRATCH_REG0 in each hub must be set to the hub NIC.
 *
 *   Router NIC caching algorithm:
 *    Reading a router's NIC requires many vector ops and is SLOW.
 *    Therefore each router's NIC is cached in its RR_SCRATCH_REG0.
 *    Any time a hub encounters a router, it checks to see if the
 *    RR_SCRATCH_REG0 for that hub is zero, and if so, it locks the
 *    router by doing an exchange of a non-zero value into
 *    RR_SCRATCH_REG1, then it reads that router's NIC (takes a
 *    long time), copies the NIC into the router's RR_SCRATCH_REG0,
 *    and unlocks the router.  Thus the first hub to encounter any
 *    given router will assign the NIC into the router's scratch reg.
 *
 *   Any time a hub encounters another hub, it checks for the NIC
 *    in that hub's NI_SCRATCH_REG0.  If the NIC is not there, the
 *    hub is assumed defunct or disabled and is not included.
 *
 *   In the event of errors, messages are logged and the array is
 *    still initialized consistently.
 */

int discover_find_hub_nic(pcfg_t *p, nic_t nic)
{
    int			index;

    for (index = 0; index < p->count; index++)
	if (p->array[index].any.type == PCFG_TYPE_HUB &&
	    p->array[index].hub.nic == nic)
	    return index;

    return -1;
}

int discover_find_hub_nasid(pcfg_t *p, nasid_t nasid)
{
    int			index;

    for (index = 0; index < p->count; index++)
	if (p->array[index].any.type == PCFG_TYPE_HUB &&
	    p->array[index].hub.nasid == nasid)
	    return index;

    return -1;
}

int discover_find_router_nic(pcfg_t *p, nic_t nic)
{
    int			index;

    for (index = 0; index < p->count; index++)
	if (p->array[index].router.type == PCFG_TYPE_ROUTER &&
	    p->array[index].router.nic == nic)
	    return index;

    return -1;
}

static int add_hub(pcfg_t *p, nic_t nic)
{
    pcfg_hub_t	       *ph;
    int			i;

    if (p->count == p->alloc)
	return -1;

    ph = &p->array[p->count].hub;

    memset(ph, 0, sizeof (*ph));

    ph->type		= PCFG_TYPE_HUB;
    ph->port.index	= PCFG_INDEX_INVALID;
    ph->port.port	= PCFG_PORT_INVALID;
    ph->nasid		= PCFG_METAID_INVALID;
    ph->nic		= nic;

    if (nic == 0)
	ph->flags      |= PCFG_HUB_HEADLESS;

    for (i = 0; i < 16; i++)
	ph->htlocal[i] = 0xf;

    for (i = 0; i < 32; i++)
	ph->htmeta[i] = 0xf;

    p->count_type[PCFG_TYPE_HUB]++;

    return p->count++;
}

static int add_router(pcfg_t *p, nic_t nic)
{
    pcfg_router_t      *pr;
    int			i;

    if (p->count == p->alloc)
	return -1;

    pr		= &p->array[p->count].router;

    memset(pr, 0, sizeof (*pr));

    pr->type	= PCFG_TYPE_ROUTER;
    pr->metaid	= PCFG_METAID_INVALID;
    pr->nic	= nic;

    for (i = 1; i <= 6; i++) {
	pr->port[i].index	= PCFG_INDEX_INVALID;
	pr->port[i].port	= PCFG_PORT_INVALID;
    }

    for (i = 0; i < 16; i++)
	pr->rtlocal[i] = 0xffffff;

    for (i = 0; i < 32; i++)
	pr->rtmeta[i] = 0xffffff;

    p->count_type[PCFG_TYPE_ROUTER]++;

    return p->count++;
}

static int discover_hub(pcfg_t *, int, int,
			net_vec_t, net_vec_t, __uint64_t, int);
static int discover_router(pcfg_t *, int, int,
			   net_vec_t, net_vec_t, __uint64_t, int);

static void set_hub_data(pcfg_hub_t *ph, __uint64_t sr1)
{
    ph->promvers    = GET_FIELD(sr1, ADVERT_PROMVERS);
    ph->promrev	    = GET_FIELD(sr1, ADVERT_PROMREV);
    ph->slot	    = GET_FIELD(sr1, ADVERT_SLOTNUM);
    ph->module      = GET_FIELD(sr1, ADVERT_MODULE);

    if (sr1 & ADVERT_CONSOLE_MASK)
	ph->flags |= PCFG_HUB_BASEIO;

    if (sr1 & ADVERT_GMASTER_MASK)
	ph->flags |= PCFG_HUB_GMASTER;

    if (sr1 & ADVERT_LOCALPOD_MASK)
	ph->flags |= PCFG_HUB_LOCALPOD;

    if (sr1 & ADVERT_GLOBALPOD_MASK)
	ph->flags |= PCFG_HUB_GLOBALPOD;

    if (sr1 & ADVERT_POWERON_MASK)
	ph->flags |= PCFG_HUB_POWERON;

    if (sr1 & ADVERT_PREMDIR_MASK)
	ph->flags |= PCFG_HUB_PREMDIR;

    if (sr1 & ADVERT_BOOTED_MASK)
        ph->flags |= PCFG_HUB_REMOTE_SPACE;

    if (sr1 & ADVERT_PARTPROM_MASK)
        ph->flags |= PCFG_HUB_PARTPROM;

    if ((sr1 & ADVERT_EXCP_MASK) && !DIP_NODISABLE()) {
        ph->flags |= PCFG_HUB_EXCP;
        ph->flags |= PCFG_HUB_HEADLESS;
    }
}

static int discover_hub(pcfg_t *p,
			int from_index,
			int from_port,		/* Valid if from router */
			net_vec_t from_path,	/* Path coming from	*/
			net_vec_t path,		/* Path to discover	*/
			__uint64_t status,
			int diag_mode)
{
    pcfg_hub_t	       *ph;
    nic_t		nic;
    int			diag_rc, index, r, dg_rc1, dg_rc2;
    rtc_time_t		expire, next = 0;
    __uint64_t		sr0;

#ifndef DISCTEST
    __uint64_t		sr1;
#endif

    if(verbose) {
    db_printf("discover_hub: path=0x%lx stat=%y from=%d/%d\n",
	      path, status, from_index, from_port);
    }

    /*
     * Query remote hub's NIC, which resides in its NI_SCRATCH_REG0.
     * If the NIC is not already listed, add a new hub entry.
     *
     * All T5s immediately set their SCRATCH_REG0 to their NIC first thing
     * when they boot.  This is in plenty of time before any other Hubs
     * will try to read it through vector reads.
     *
     * If the NIC is zero, the hub being discovered is headless
     * and the NIC is left at zero until after global arbitration.
     *
     * If the NIC is ~0ULL, the hub being discovered is alive but has
     * not yet accessed its NIC -- allow it a certain amount of time
     * past system reset.
     *
     * 48 bits are the actual NIC. 8 bits are used by the partitionid.
     */

    expire = rtc_time() + MAX_RESET_TIME;

    while (1) {
	if ((r = vector_read(path, 0, NI_SCRATCH_REG0, &sr0)) < 0) {
	    printf("discover_hub: vec read 0x%lx SCR0 failed: %s\n",
		   path, net_errmsg(r));
	    return -1;
	}

	if (sr0 != ~0ULL)
	    break;

	if (rtc_time() > expire || kbintr(&next)) {
	    sr0 = 0;		/* Call it dead (headless) */
	    break;
	}

	rtc_sleep(100000);	/* Poll every 0.1 sec */
    }

    nic = (__uint64_t) GET_FIELD(sr0, ADVERT_HUB_NIC);

    if(verbose) {
    db_printf("discover_hub: nic=0x%x\n", nic);
    }

#ifndef DISCTEST
    if ((r = vector_read(path, 0, NI_SCRATCH_REG1, &sr1)) < 0) {
	printf("discover_hub: vec read 0x%lx NSR1 failed: %s\n",
	       path, net_errmsg(r));
	return -1;
    }

    if(verbose) {
    db_printf("discover_hub: sr0=%y sr1=%y\n", sr0, sr1);
    }
#endif

    if (nic == 0 ||
	(index = discover_find_hub_nic(p, nic)) < 0)
	if ((index = add_hub(p, nic)) < 0)
	    return -1;

    p->array[index].hub.partition = (partid_t) GET_FIELD(sr0, ADVERT_PARTITION);
    p->array[index].hub.version = GET_FIELD(status, NSRI_REV);

    /*
     * Add link from incoming router to this hub.
     */

    switch (p->array[from_index].any.type) {
    case PCFG_TYPE_HUB:
	p->array[from_index].hub.port.index = index;
	p->array[from_index].hub.port.port  = -1;
	break;
    case PCFG_TYPE_ROUTER:
	p->array[from_index].router.port[from_port].index = index;
	p->array[from_index].router.port[from_port].port  = -1;
	break;
    }

    /*
     * Add link from this hub back to incoming object.
     */

    ph			= &p->array[index].hub;
    ph->port.index	= from_index;
    ph->port.port	= from_port;

    /*
     * Test link into Hub.  from_index may be -1, indicating the
     * source is also a hub.
     */

#ifndef DISCTEST
    if(verbose) {
    db_printf("discover_hub: running diags\n");
    }

    dg_rc1 = hub_link_vector_diag(diag_mode, p, from_index,
				from_port, index,
				from_path, path);

    /*
     * Find out whether or not remote hub wants to be global master,
     * and mark as such.
     * Also copy other info that should be in promcfg (lower flag bits are
     * CPU mask).
     */

    if ((r = vector_read(path, 0, NI_SCRATCH_REG1, &sr1)) < 0) {
	printf("discover_hub: vec read 0x%lx NSR1 failed: %s\n",
	       path, net_errmsg(r));
	return -1;
    }

    set_hub_data(ph, sr1);

    /*
     * set_hub_data marks the HUB as belonging to reset or remote
     * partition. If remote, nasids are already programmed.
     */

    if (!IS_RESET_SPACE_HUB(&p->array[index].hub))
        p->array[index].hub.nasid = GET_FIELD(status, NSRI_NODEID);

    if (dg_rc1 != KLDIAG_PASSED)
        printf("*** hub_link_vector_diag: %S: SHOWED ERRORS\n",
		ph->module, ph->slot);

    if(verbose) {
    db_printf("discover_hub: sr1=%y, flags=0x%x, promvers=%d, slot=%d\n",
	      sr1, ph->flags, ph->promvers, ph->slot);
    }
#endif

    return index;
}

/*
 * cache_router_nic
 *
 *   Read RR_SCRATCH_REG0; if non-zero, return it as the NIC.
 *   Otherwise, "lock" the router via RR_SCRATCH_REG1, read the
 *   NIC, write the NIC into RR_SCRATCH_REG0, unlock the router,
 *   and return the NIC.
 */

static int
cache_router_nic(net_vec_t path,
		 nic_t *nicp)
{
    int			locked, r;
    nic_t		rnic;

    locked = 0;

    if ((r = vector_read(path, 0, RR_SCRATCH_REG0, nicp)) < 0) {
	printf("cache_router_nic: vector read 0x%lx SCR0 failed: %s\n",
	       path, net_errmsg(r));
	goto fail;
    }

    if (*nicp & RSCR0_NIC_MASK)		/* Done if got NIC in RR_SCRATCH_REG0 */
	return 0;

    if ((r = router_lock(path, 10000, 3000000)) < 0) {
	printf("cache_router_nic: could not lock router 0x%lx: %s\n",
	       path, net_errmsg(r));
	goto fail;
    }

    locked = 1;

    if ((r = vector_read(path, 0, RR_SCRATCH_REG0, nicp)) < 0) {
	printf("cache_router_nic: vector read 0x%lx SCR0 failed: %s\n",
	       path, net_errmsg(r));
	goto fail;
    }

    if (*nicp & RSCR0_NIC_MASK) {	/* Done while we were getting lock? */
	if ((r = router_unlock(path)) < 0) {
	    printf("cache_router_nic: could not unlock router 0x%lx: %s\n",
		   path, net_errmsg(r));
	    locked = 0;
	    goto fail;
	}

	return 0;
    }

    /*
     * router_nic_get takes 0.235 sec running cached, and over 1 second
     * if running in dex mode!  This is why we're caching the NIC.
     * If router_nic_get fails, it leaves a random number in *nicp.
     * This should be logged an error message, but the system should
     * be able to come up with a consistent "random" router NIC.
     */

    r = router_nic_get(path, &rnic);	/* Takes 0.235 sec running cached! */

    if (r < 0)
	printf("Warning: Unable to read router NIC (vector 0x%lx); "
	       "using random number.\n", path);

    SET_FIELD(*nicp, RSCR0_NIC, rnic);

    if ((r = vector_write(path, 0, /* Copy to RR_SCRATCH_REG0 */
			   RR_SCRATCH_REG0, *nicp)) < 0) {
	printf("cache_router_nic: vector write 0x%lx SCR0 failed: %s\n",
	       path, net_errmsg(r));
	goto fail;
    }

    if ((r = router_unlock(path)) < 0) {
	printf("cache_router_nic: could not unlock router 0x%lx: %s\n",
	       path, net_errmsg(r));
	locked = 0;
	goto fail;
    }

    return 0;

 fail:

    if (locked && (r = router_unlock(path)) < 0)
	printf("cache_router_nic: could not unlock router 0x%lx: %s\n",
	       path, net_errmsg(r));

    return -1;
}

static int discover_router(pcfg_t *p,
			   int from_index,
			   int from_port,	/* Valid if from router */
			   net_vec_t from_path,	/* Path coming from	*/
			   net_vec_t path,	/* Path to discover	*/
			   __uint64_t status,
			   int diag_mode)
{
    pcfg_port_t	       *pp;
    nic_t		nic;
    __uint64_t		rr_scratch, rr_protection;
    int			port, in_port;
    int			index;
    int			diag_rc, dg_rc1, dg_rc2;
    int			i, r = 0, f = 0;

    if(verbose) {
    db_printf("discover_router: path=0x%lx status=%y\n", path, status);
    }

    /*
     * Max vector length supported by the router is 15!
     * (16 nibbles with last nibble reqd to be 0).
     * If we have reached the max, signal error
     */
    if (vector_length(path) >= 15) {
        printf("WARNING: Max vector length 15 exceeded on path 0x%lx\n", path);
        return -1;
    }

    /*
     * Query the router's NIC and the input port to which we're talking.
     * If the NIC is not already listed, add a new router entry.
     */

    if (cache_router_nic(path, &rr_scratch) < 0)
	return -1;		/* Error message already printed */

    nic = GET_FIELD(rr_scratch, RSCR0_NIC);

    in_port	= (int) GET_FIELD(status, RSRI_INPORT);

    if ((index = discover_find_router_nic(p, nic)) < 0)
	if ((index = add_router(p, nic)) < 0)
	    return -1;

    p->array[index].router.version = GET_FIELD(status, RSRI_CHIPREV);
    if (~GET_FIELD(status, RSRI_CHIPIN) & 8)
	p->array[index].router.flags |= PCFG_ROUTER_META;

    p->array[index].router.slot = 
	SLOTNUM_GETSLOT(p->array[index].router.flags & PCFG_ROUTER_META 
	? meta_router_slotbits_to_slot(GET_FIELD(status, RSRI_CHIPIN)) 
	: router_slotbits_to_slot(GET_FIELD(status, RSRI_CHIPIN)));

    /*
     * Add link from incoming hub or router to this router.
     */

    switch (p->array[from_index].any.type) {
    case PCFG_TYPE_HUB:
	pp	= &p->array[from_index].hub.port;
	break;
    case PCFG_TYPE_ROUTER:
	pp	= &p->array[from_index].router.port[from_port];
	break;
    }

    pp->index	= index;
    pp->port	= in_port;

    /*
     * Add link from this router back to incoming hub or router.
     */

    pp		= &p->array[index].router.port[in_port];

    pp->index	= from_index;
    pp->port	= from_port;

   /*
    * saving router status info
    */

    if(verbose) {
    db_printf("discover_router: saving error info\n");
    }

    for (i = 1; i <= MAX_ROUTER_PORTS; i++)
        if ((r = vector_read(path, 0, RR_STATUS_ERROR(i),
				 &p->array[index].router.port[i].error)) < 0) {
            printf("discover_router: could not read "
		   "0x%lx error reg: %s\n", path, net_errmsg(r));
            return -1;
        }

#ifndef DISCTEST
   /*
    * Running router_link test
    */

    if(verbose) {
    db_printf("discover_router: running diags\n");
    }

    dg_rc1 = rtr_link_vector_diag(diag_mode, p, from_index,
				    from_port, index,
				    from_path, path);

    if (dg_rc1 != KLDIAG_PASSED) {

        if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

            /* Display error message */
            printf("*** rtr_link_vector_diag: SHOWED ERRORS: \n"); 
	    printf("***	From hub on   %S\n", 
			p->array[from_index].hub.module, 
			p->array[from_index].hub.slot);
            printf("***	To port %d on /hw/module/%d/slot/r%d \n", 
			in_port, p->array[from_index].hub.module, 
			p->array[index].router.slot);

	    
        } else {
            printf("*** rtr_link_vector_diag: SHOWED ERRORS: \n"); 
            printf("*** From port %d on /hw/module/Unknown/slot/r%d\n", 
			from_port, p->array[from_index].router.slot);
            printf("*** To   port %d on /hw/module/Unknown/slot/r%d\n", 
			in_port, p->array[index].router.slot);

            /* Light amber LED on source router board */
            router_led_error(p, from_index, (1 << (from_port-1)));
        }

        /* Light amber LED on router board under test */
	printf("***     Note: The router port(s) in error will have"
	       " amber LED(s) set.\n");
	router_led_error(p, index, (1 << (in_port-1)));
    }

#endif

   /*
    * Fill up port_status on each of the ports
    */

    if (vector_read(path, 0, RR_PROT_CONF, &rr_protection) < 0) {
        printf("discover_router: vector_read failed path = 0x%lx\n", path);
        rr_protection = 0x0;
    }

    if (rr_scratch & RSCR0_BOOTED_MASK) {
	p->array[index].router.metaid = 
	    GET_FIELD(rr_protection, RPCONF_METAID) << NASID_LOCAL_BITS |
		GET_FIELD(rr_scratch, RSCR0_LOCALID);
        p->array[index].router.flags |= PCFG_ROUTER_REMOTE_SPACE;
    }

    for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
        if (status & RSRI_LINKWORKING(i)) {
            SET_FIELD(p->array[index].router.port[i].flags, PCFG_PORT_UP, 1);

            if (~rr_protection & (1 << (i - 1)))
                SET_FIELD(p->array[index].router.port[i].flags, 
			PCFG_PORT_FENCE, 1);
            else
                SET_FIELD(p->array[index].router.port[i].flags, 
			PCFG_PORT_FENCE, 0);
        }
        else {
            SET_FIELD(p->array[index].router.port[i].flags, PCFG_PORT_UP,
			0);
            SET_FIELD(p->array[index].router.port[i].flags, 
			PCFG_PORT_FENCE, 0);
        }
    }

    return index;
}

static char *pname(pcfg_t *p, pcfg_port_t *pp, char *name)
{
    if (pp->index < 0)
	sprintf(name, "Not connected");
    else if (p->array[pp->index].any.type == PCFG_TYPE_HUB)
	sprintf(name, "Entry %d Hub %lx",
		pp->index, p->array[pp->index].hub.nic);
    else
	sprintf(name, "Entry %d Router %lx port %d",
		pp->index,
		p->array[pp->index].router.nic,
		pp->port);

    return name;
}

/*
 * discover_dump_promcfg: Dumps promcfg structure.
 *                        flags: DDUMP_PCFG_VERS dumps promvers/revnum only
 *                               DDUMP_PCFG_ALL  dumps all info.
 *                               DDUMP_PCFG_ROUTES dumps vector routes.
 */

void discover_dump_promcfg(pcfg_t *p,
			   int (*prf)(const char *fmt, ...), int flags)
{
    int			i, j, r;
    pcfg_hub_t	       *ph;
    pcfg_router_t      *pr;
    char		name[80];

    for (i = 0; i < p->count; i++) {
	net_vec_t	vec;

	vec = discover_route(p, 0, i, 0);

	switch (p->array[i].any.type) {
	case PCFG_TYPE_HUB:
	    ph = &p->array[i].hub;
	    prf("ENTRY %d: HUB(%lx)\n", i, ph->nic);
	    prf("    NASID=%d Mod=%d Slot=%d Flg=0x%x PROM=%d.%d Route=",
		ph->nasid,
		ph->module,
		ph->slot,
		ph->flags,
		ph->promvers,
		ph->promrev);
	    prf(i ? "0x%lx\n" : "N/A\n", vec);
            prf("    MODULE=%d PARTITION=%d SPACE=%s\n",
		ph->module, ph->partition,
		(IS_RESET_SPACE_HUB(ph) ? "RESET" : "NON-RESET"));
	    if (flags == DDUMP_PCFG_VERS)
		break;
	    prf("    Port 1 connection: %s\n",
		pname(p, &ph->port, name));
            prf("    Port status: %s\n", 
		((ph->port.flags & PCFG_PORT_FENCE_MASK) ? "FE" : "NF"));
	    break;
	case PCFG_TYPE_ROUTER:
	    if (flags == DDUMP_PCFG_VERS)
		break;
	    pr = &p->array[i].router;
	    prf("ENTRY %d: ROUTER(%lx)\n", i, pr->nic);
	    prf("    METAID=%d (%d) Slot=%d Flags=0x%x Route=0x%lx\n",
		NASID_GET_META(pr->metaid),
		pr->metaid,
		pr->slot,
		pr->flags,
		vec);
            prf("    MODULE=%d PARTITION=%d SPACE=%s\n",
		pr->module, pr->partition,
		(IS_RESET_SPACE_RTR(pr) ? "RESET" : "NON-RESET"));
	    for (j = 1; j <= 6; j++) {
		prf("    Port %d connection: %s\n",
		    j, pname(p, &pr->port[j], name));
                prf("    Port %d status: %s %s\n", j,
                    ((pr->port[j].flags & PCFG_PORT_UP_MASK) ? "UP" : "DN"),
                    ((pr->port[j].flags & PCFG_PORT_FENCE_MASK) ? "FE" : "NF"));
            }
	    break;
	default:
	    prf("Bad entry %d: type = %d\n", i, p->array[i].any.type);
	    break;
	}
    }
}

void discover_dump_routing(pcfg_t *p,
			   int (*prf)(const char *fmt, ...))
{
    int			i, j;
    pcfg_hub_t	       *ph;
    pcfg_router_t      *pr;

    for (i = 0; i < p->count; i++) {
	switch (p->array[i].any.type) {
	case PCFG_TYPE_HUB:
	    ph = &p->array[i].hub;
	    for (j = 0; j < 16; j++)
		if (ph->htlocal[j] != 0xf)
		    prf("Hub index %d nic 0x%lx nasid %d "
			"local entry %d = 0x%x\n",
			i, ph->nic, ph->nasid, j, ph->htlocal[j]);
	    for (j = 0; j < 32; j++)
		if (ph->htmeta[j] != 0xf)
		    prf("Hub index %d nic 0x%lx nasid %d "
			"meta entry %d = 0x%x\n",
			i, ph->nic, ph->nasid, j, ph->htmeta[j]);
	    break;
	case PCFG_TYPE_ROUTER:
	    pr = &p->array[i].router;
	    for (j = 0; j < 16; j++)
		if (pr->rtlocal[j] != 0xffffff)
		    prf("Router index %d nic 0x%lx "
			"local entry %d = 0x%06x\n",
			i, pr->nic, j, pr->rtlocal[j]);
	    for (j = 0; j < 32; j++)
		if (pr->rtmeta[j] != 0xffffff)
		    prf("Router index %d nic 0x%lx "
			"meta entry %d = 0x%06x\n",
			i, pr->nic, j, pr->rtmeta[j]);
	    break;
	}
    }
}

/*
 * Function:            discover_object -> probes NI on the port; If objext
 *                      found, adds it to pcfg; if router adds to rou_vrts
 * Args:                p -> pcfg ptr
 *                      pcfg_indx -> object to probe from
 *                      port -> port to probe on
 *                      from_path -> path upto here
 *                      rou_vrts -> rtr vertices ptr
 * Returns:             0 if successful
 *                      < 0 if failed
 */

int discover_object(pcfg_t *p,
                    int pcfg_indx,
                    int port,
                    net_vec_t from_path,
                    queue_t *rou_vrts,
                    int diag_mode)
{
    __uint64_t          status;
    net_vec_t           to_path;
    vertex_elem_t       new_rtr;
    int                 r;

    to_path = vector_concat(from_path, port);

    /*
     * Call get_chipid to read the remote status register with retries
     */

     if ((r = get_chipid(p, pcfg_indx, port, from_path,
                        to_path, &status)) < 0) {

        /*
         * get_chipid failed so we will just go on with discovery
         */

         printf("Discovery encountered bad link; continuing...\n");
     }
     else {

         switch (GET_FIELD(status, NSRI_CHIPID)) {
         case CHIPID_HUB:
             r = discover_hub(p, pcfg_indx, port, from_path,
                        to_path, status, diag_mode);
             break;
         case CHIPID_ROUTER:
             r = discover_router(p, pcfg_indx, port, from_path,
                        to_path, status, diag_mode);

             if ((r >= 0) && (GET_FIELD(p->array[r].router.flags, 
			PCFG_ROUTER_STATE) == PCFG_ROUTER_STATE_NEW)) {
                 new_rtr.pcfg_indx = r;
                 new_rtr.vec = to_path;
                 new_rtr.status = status;

                 queue_push(rou_vrts, &new_rtr);

                 SET_FIELD(p->array[r].router.flags, PCFG_ROUTER_STATE, 
			PCFG_ROUTER_STATE_SEEN);
             }
             break;
         default:
             printf("discover_object: unknown chip ID, path=0x%lx, "
		    "status=%y\n", to_path, status);
             r = 0;
             break;
         }
     }

     return r;
}

/*
 * Function:		discover_loop -> loops thro' all vertices on the
 *			rtr graph and discovers objects as they are found
 * Args:		p -> pcfg_t ptr
 *			rou_vrts -> rtr graph ptr
 * Returns:		0, if discovery successful
 *			< 0, if any errors encountered
 */

int discover_loop(pcfg_t *p, queue_t *rou_vrts, int diag_mode)
{
    while (!queue_empty(rou_vrts)) {
        int             port, index;
        vertex_elem_t   next_vertex;

        queue_pop(rou_vrts, &next_vertex);

        index = next_vertex.pcfg_indx;

        if (GET_FIELD(p->array[index].router.flags, PCFG_ROUTER_STATE) 
		!= PCFG_ROUTER_STATE_SEEN) {
            printf("discover_loop: Status of router nic 0x%lx path 0x%lx is "
		"not right\n", p->array[index].router.nic,
		next_vertex.vec);
            continue;
        }

        for (port = 1; port <= MAX_ROUTER_PORTS; port++)
            if (GET_FIELD(p->array[index].router.port[port].flags,
			PCFG_PORT_UP))
                discover_object(p, next_vertex.pcfg_indx, port,
                        next_vertex.vec, rou_vrts, diag_mode);

        SET_FIELD(p->array[next_vertex.pcfg_indx].router.flags, PCFG_ROUTER_STATE, 
			PCFG_ROUTER_STATE_DONE);
    }

    return 0;
}

int discover(pcfg_t *p, nic_t my_nic, int diag_mode)
{
    __uint64_t		status;
    int			r;
    queue_t		rou_vrts;

    if(verbose) {
    db_printf("discover: diag_mode=%d\n", diag_mode);
    }

    p->count = 0;

    memset(p->count_type, 0, sizeof (p->count_type));

    /*
     * Make an entry for the current hub.
     */

    if (add_hub(p, my_nic) < 0)
	return -1;

#ifndef DISCTEST
    status = LD(LOCAL_HUB(NI_STATUS_REV_ID));

    p->array[0].hub.partition = (partid_t) GET_FIELD(
				LOCAL_HUB_L(NI_SCRATCH_REG0), ADVERT_PARTITION);
    p->array[0].hub.version = GET_FIELD(status, NSRI_REV);

    set_hub_data(&p->array[0].hub, LD(LOCAL_HUB(NI_SCRATCH_REG1)));
#endif

    /*
     * Probe network interface to make sure it's up.
     * Then start discovery with null path from current hub.
     */

    if (! net_link_up()) {
	printf("*** Local network link down\n");
	return 0;
    }
    /*
     * For midplane diags, dont do craylink discovery
     */
     if (DIP_MIDPLANE_DIAG()) {
         printf("*** Skipping CrayLink discovery\n");
         return 0;
     }
 
    queue_init(&rou_vrts);

    if ((r = discover_object(p, 0, 0, 0, &rou_vrts, diag_mode)) < 0) {
        p->count = 1;           /* Just leave current hub */
        return r;
    }

    if ((r = discover_loop(p, &rou_vrts, diag_mode)) < 0)
	p->count = 1;		/* Just leave current hub */

    return r;
}

/*
 * discover_route (operating from promcfg only)
 *
 *   Determines a vector route from one entry in the promcfg
 *   (index=src) to another (index=dst).  The src_hub index must
 *   point to a hub.  The dst index may point to a hub or router.
 *   Uses a breadth-first search to find a shortest path.  The
 *   nth_alternate argument gets alternate paths of the same
 *   depth as the shortest path.  Returns NET_VEC_BAD if
 *   no path is possible or the alternates are exhausted.
 *   This algorithm has been tested on all the obscure corner cases.
 */

#define MAX_DEPTH	15

static net_vec_t try_router(pcfg_t *p,
			    int src_rtr, int dst,
			    int depth, net_vec_t path,
			    int *skip)
{
    net_vec_t		result;
    int			port;

    for (port = 6; port >= 1; port--) {
	int	next;

	if ((next = p->array[src_rtr].router.port[port].index) == dst &&
	    (*skip)-- == 0)
	    return vector_concat(port, path);

	if (depth > 1 &&
	    p->array[next].any.type == PCFG_TYPE_ROUTER &&
	    (result = try_router(p,
				 next, dst, depth - 1,
				 vector_concat(port, path),
				 skip)) != NET_VEC_BAD)
	    return result;
    }

    return NET_VEC_BAD;
}

net_vec_t __discover_route(pcfg_t *p,
			 int src, int dst, int nth_alternate)
{
    int			next, depth, skip;
    net_vec_t		result;

    if (src == dst)
        return NET_VEC_BAD;

    if (p->array[src].any.type == PCFG_TYPE_HUB) {
        if ((next = p->array[src].hub.port.index) < 0)
            return NET_VEC_BAD;
    }
    else
        next = src;

    if (next == dst && nth_alternate == 0)
	return 0;

    if (p->array[next].any.type == PCFG_TYPE_HUB)
	return NET_VEC_BAD;

    for (depth = 1; depth < MAX_DEPTH; depth++) {
	skip = nth_alternate;
	if ((result = try_router(p, next, dst,
				 depth, 0, &skip)) != NET_VEC_BAD)
	    return vector_reverse(result);
	if (skip < nth_alternate)
	    break;	/* In this case, higher-depth paths are meaningless */
    }

    return NET_VEC_BAD;
}

/* 
 * This subroutine wraps around the original __discover_route() (above).
 * On large systems, building the vector (even by BFS) takes between
 * 6-10 seconds for distant destinations.  Therefore, we have reserved
 * part of the address space at IP27PROM_DRT_CACHE_BASE (see 
 * irix/kern/sys/SN/SN0/addrs.h) to cache route discovery.
 *
 * This wrapper only caches results for src == 0 && nth_alternate == 0,
 * but this appears to be satisfied by most (if not all) of the requests
 * to this function.  This DOES NOT limit the use of this function;
 * if src != 0 || nth_alternate != 0 then the results of the original
 * __discover_route are returned.
 *
 * Cached results are stored indexed by dst.  Cache entries not yet
 * determined are marked by NET_VEC_BAD.
 *
 * One final note: IP27PROM_DRT_CACHE_SIZE is big enough for dst to
 * range from 0..1023.
 */
net_vec_t discover_route(pcfg_t *p, int src, int dst, int nth_alternate)
{
  static net_vec_t *rc = NULL;
  int i;

  if(!rc)
  {
    rc = (net_vec_t *)TO_NODE_UNCAC(get_nasid(), IP27PROM_DRT_CACHE_BASE);
    for(i = 0; i < IP27PROM_DRT_CACHE_SIZE / sizeof(net_vec_t); i++)
      rc[i] = NET_VEC_BAD;
  }
  rc = (net_vec_t *)TO_NODE_UNCAC(get_nasid(), IP27PROM_DRT_CACHE_BASE);

  if(src != 0 || nth_alternate != 0)
    return(__discover_route(p, src, dst, nth_alternate));

  if(src == dst)
    return(NET_VEC_BAD);

  /* lookup dst, nth_alternate in cache */
  if(dst >= (IP27PROM_DRT_CACHE_SIZE / sizeof(net_vec_t)))
  {
    db_printf("***WARNING: vector %d->%d exceeds table lookup space\n",
              src, dst);
    return(__discover_route(p, src, dst, nth_alternate));
  }
  if(rc[dst] == NET_VEC_BAD)
    rc[dst] = __discover_route(p, src, dst, nth_alternate);
  
  return(rc[dst]);
}

/*
 * discover_follow (operating from promcfg only)
 *
 *   Given promcfg, a source, and a vector route, follows the vector
 *   and returns the index of the destination.
 */

int discover_follow(pcfg_t *p, int src, net_vec_t vec)
{
    if (p->array[src].any.type == PCFG_TYPE_HUB)
	src = p->array[src].hub.port.index;

    while (vec) {
	src = p->array[src].router.port[vec & 7].index;
	vec >>= 4;
    }

    return src;
}

/*
 * discover_return (operating from promcfg only)
 *
 *   Given promcfg, a source hub, and a vector route to a destination
 *   hub, determines the vector route from the destination hub to the
 *   source hub (the return route).
 */

net_vec_t discover_return(pcfg_t *p, int src_hub, net_vec_t vec)
{
    net_vec_t		retvec;
    int			src;
    char		inport;

    inport = p->array[src_hub].hub.port.port;
    src	   = p->array[src_hub].hub.port.index;

    retvec = 0;

    while (vec) {
	pcfg_port_t    *pp;

	retvec = vector_concat(inport, retvec);

	if (p->array[src].any.type == PCFG_TYPE_HUB)
	    pp = &p->array[src].hub.port;
	else
	    pp = &p->array[src].router.port[vec & 7];

	inport = pp->port;
	src    = pp->index;

	vec  >>= 4;
    }

    return retvec;
}

/*
 * discover_program (operating from promcfg only)
 *
 *   Inserts routing table information into the promcfg structure to
 *   program a path between a source hub and a destination hub along
 *   a specified vector route.
 *
 *   If retvec_ptr is non-NULL, the vector return path is stored there.
 *   If dst_hub_ptr is non-NULL, the index of the dest. hub is stored there.
 */

void discover_program(pcfg_t *p,
		      int src_hub, nasid_t dst_nasid, net_vec_t vec,
		      net_vec_t *retvec_ptr, int *dst_hub_ptr)
{
    net_vec_t		retvec;
    net_vec_t		inport;
    int			len, i;
    int			cur;
    nasid_t		cur_nasid;
    pcfg_port_t	       *pp;
    uint		turns;
    int			leaving;

    if ((len = vector_length(vec)) == 0) {
	if (retvec_ptr)
	    *retvec_ptr	= 0;

	if (dst_hub_ptr)
	    *dst_hub_ptr = p->array[src_hub].hub.port.index;

	return;
    }

    /*
     * Program source hub table entry.
     */

    inport = p->array[src_hub].hub.port.port;
    cur	   = p->array[src_hub].hub.port.index;
    cur_nasid = p->array[src_hub].hub.nasid;

    turns  = (uint) RR_DISTANCE(inport, vector_get(vec, 0));

    if (NASID_GET_META(cur_nasid)==NASID_GET_META(dst_nasid)) {
      p->array[src_hub].hub.htlocal[NASID_GET_LOCAL(dst_nasid)] = turns;
    if(verbose) {
      db_printf("Set hub index %d nic 0x%lx nasid %d ltab %d to %d turns\n",
		src_hub, p->array[src_hub].hub.nic,
		p->array[src_hub].hub.nasid,
		NASID_GET_LOCAL(dst_nasid), turns);
    }
    }
    else {
      p->array[src_hub].hub.htmeta[NASID_GET_META(dst_nasid)] = turns;
    if(verbose) {
      db_printf("Set hub index %d nic 0x%lx nasid %d mtab %d to %d turns\n",
		src_hub, p->array[src_hub].hub.nic,
		p->array[src_hub].hub.nasid,
		NASID_GET_META(dst_nasid), turns);
    }
    }

    if (p->array[cur].any.type == PCFG_TYPE_HUB)
	pp = &p->array[cur].hub.port;
    else
	pp = &p->array[cur].router.port[vector_get(vec, 0)];

    retvec = inport;

    /*
     * Program router entries necessary to form the path from source node
     * to destination node.  The return path is determined along the way.
     */

    for (i = 1; i < len; i++) {
	uint	       *rent;

	inport = pp->port;
	retvec = vector_concat(inport, retvec);
	turns = (uint) RR_DISTANCE(inport, vector_get(vec, i));
	leaving = (int) vector_get(vec, i - 1);

	/* the router metaid field is as wide as a nasid.
	 */
	cur_nasid = p->array[cur].router.metaid;

	/*
	 * check for force local,
	 * or for a router metaid/dest metaid match
	 */

	if (NASID_GET_META(cur_nasid)==NASID_GET_META(dst_nasid) ||
	    p->array[cur].router.force_local) {
	  rent = &p->array[cur].router.rtlocal[NASID_GET_LOCAL(dst_nasid)];
	  if(verbose) {
	  db_printf("Set router index %d nic 0x%lx nasid %d ltab %d "
		    "leaving %d to %d turns\n", cur, p->array[cur].router.nic,
		    cur_nasid, NASID_GET_LOCAL(dst_nasid), leaving, turns);
	   }
	}
	else {
	  rent = &p->array[cur].router.rtmeta[NASID_GET_META(dst_nasid)];
	  if(verbose) {
	  db_printf("Set router index %d nic 0x%lx nasid %d mtab %d "
		    "leaving %d to %d turns\n", cur, p->array[cur].router.nic,
		    cur_nasid, NASID_GET_META(dst_nasid), leaving, turns);
	  }
	}

	*rent = (uint) vector_modify(*rent, leaving - 1, turns);

	cur = pp->index;

	if (p->array[cur].any.type == PCFG_TYPE_HUB)
	    pp = &p->array[cur].hub.port;
	else
	    pp = &p->array[cur].router.port[vector_get(vec, i)];
    }

    if (dst_hub_ptr)
	*dst_hub_ptr = pp->index;

    if (retvec_ptr)
	*retvec_ptr = retvec;
}

#define	STAR_RTR_PORTS	((1 << 1) | (1 << 2) | (1 << 3) | (1 << 6))

static int star_rtr(pcfg_t *p, pcfg_router_t *rtr)
{
    int		i;

    if (rtr->type != PCFG_TYPE_ROUTER)
        return 0;

    for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
        if (!(STAR_RTR_PORTS & (1 << i)))
            continue;

        if ((rtr->port[i].index != -1) && 
		(p->array[rtr->port[i].index].any.type == PCFG_TYPE_HUB))
            return 1;
    }

    return 0;
}

int rtr_ip27_indexes(pcfg_t *p, int rtr_indx, int rtr_ports, int *mod_indxs)
{
    int		i, n = 0;

    if (p->array[rtr_indx].any.type != PCFG_TYPE_ROUTER)
        return -1;

    for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
        if (!(rtr_ports & (1 << i)))
            continue;

        if ((p->array[rtr_indx].router.port[i].index != -1) && 
		(p->array[p->array[rtr_indx].router.port[i].index].any.type == 
		PCFG_TYPE_HUB))
            mod_indxs[n++] = p->array[rtr_indx].router.port[i].index;
    }

    return n;
}

/*
 * Function: discover_module_indexes    -> fills up array with hub indexes
 *                                         of the same module
 * Args:                                   p -> pcfg_t ptr
 *                                         mymod_indx -> this index
 *                                         mod_indxs -> array of indexes
 * Returns:                                nothing
 */

void discover_module_indexes(pcfg_t *p, int mymod_indx, int *mod_indxs)
{
    int		indx = 0, port_mask, rc, rtr_indx;

    if (SN00 || (p->array[mymod_indx].hub.port.index == -1)) {
        mod_indxs[indx] = mymod_indx;
        return;
    }

    /* Null rtr case */
    if (p->array[p->array[mymod_indx].hub.port.index].any.type == 
		PCFG_TYPE_HUB) {
        mod_indxs[indx++] = mymod_indx;
        mod_indxs[indx] = p->array[mymod_indx].hub.port.index;
        return;
    }
        
    port_mask = ((1 << 4) | (1 << 5));

    if ((rc = rtr_ip27_indexes(p, p->array[mymod_indx].hub.port.index,
		port_mask, &mod_indxs[indx])) < 0)
        return;
    else
        indx += rc;


    /* Star rtr */
    if (star_rtr(p, &p->array[p->array[mymod_indx].hub.port.index].router)) {
        port_mask = ((1 << 1) | (1 << 2) | (1 << 3) | (1 << 6));
        rtr_ip27_indexes(p, p->array[mymod_indx].hub.port.index, port_mask,
		 &mod_indxs[indx]);
        return;
    }

    rtr_indx = p->array[p->array[mymod_indx].hub.port.index].router.port[6].index;

    /* Link 6 is down */
    if (rtr_indx == -1)
        return;

    port_mask = ((1 << 4) | (1 << 5));

    rtr_ip27_indexes(p, rtr_indx, port_mask, &mod_indxs[indx]);
}

/*
 * Function: discover_vote_module       -> votes on moduleid based on pcfg
 * Args:                                last_module stored in ip27log
 * Returns:                             moduleid or -1
 * Assumptions:                         pcfg has been built
 */

int discover_vote_module(pcfg_t *p, int mymod_indx)
{
    int         module_indexes[NODESLOTS_PER_MODULE];
    int		compact_indx[NODESLOTS_PER_MODULE];
    int		votes[NODESLOTS_PER_MODULE];
    int         i, j, n_nodes;

    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        module_indexes[i] = -1;

    discover_module_indexes(p, mymod_indx, module_indexes);

    n_nodes = 0;

    /* Ignore headless nodes. They aren't voting. */
    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        if ((module_indexes[i] != -1) && 
		!(p->array[module_indexes[i]].hub.flags & PCFG_HUB_HEADLESS))
            compact_indx[n_nodes++] = module_indexes[i];

    for (i = 0; i < n_nodes; i++) {
        for (j = 0; j < n_nodes; j++)
            if (p->array[compact_indx[j]].hub.module == 
				p->array[compact_indx[i]].hub.module)
                votes[j] = 1;
            else
                votes[j] = 0;

        if (majority(n_nodes, votes))
            return p->array[compact_indx[i]].hub.module;
    }

    return -1;
}

/* Function: discover_touch_modids	-> updates pcfg of module with voted
 *					   modids
 * Arguments: p				-> pcfg_t *
 *            myindx			-> indx of one of the hubs in the mod
 *            modid			-> voted module id
 * Returns:   Nothing
 */

void discover_touch_modids(pcfg_t *p, int my_indx, int modid)
{
    int		i;
    int		module_indexes[NODESLOTS_PER_MODULE];

    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        module_indexes[i] = -1;

    discover_module_indexes(p, my_indx, module_indexes);

    for (i = 0; i < NODESLOTS_PER_MODULE; i++) {
        if (module_indexes[i] == -1)
            continue;

        p->array[module_indexes[i]].hub.module = modid;
    }
}

/*
 * Function:		discover_check_sn00 -> Check if SN0 are LegoNetted
 *			with SN00
 * Args:		pcfg_t * -> ptr
 * Returns:		>= 0 if all fine
 *			< 0, if mixed types found
 */
int discover_check_sn00(pcfg_t *p)
{
    int		i;
    uint	mytype = (SN00 ? SN00_MACH_TYPE : SN0_MACH_TYPE);
    partid_t	mypart = p->array[0].hub.partition;

    for (i = 0; i < p->count; i++) {
        uint	other_type;

        if (p->array[i].any.type != PCFG_TYPE_HUB)
            continue;

	if ((p->array[i].hub.flags & PCFG_HUB_HEADLESS) || 
		(p->array[i].hub.partition != mypart))
            continue;

        if ((p->array[i].hub.promvers != p->array[0].hub.promvers) ||
		(p->array[i].hub.promrev != p->array[0].hub.promrev))
            continue;

        other_type = (SN00_NODE(p->array[i].hub.nasid) ? SN00_MACH_TYPE :
			SN0_MACH_TYPE);

        if (other_type != mytype) {
            printf("ERROR: %S: is NOT %s\n", 
		p->array[i].hub.module, p->array[i].hub.slot, 
		(mytype ? "SN00" : "SN0"));
            return -1;
        }
    }

    return 0;
}
