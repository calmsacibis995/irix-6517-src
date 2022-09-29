/***********************************************************************\
 *	File:		router_war.c					*
 *									*
 *	Workaround for router GRAM problem.				*
 *	See commands inside routines.					*
 *									*
 \***********************************************************************/

#ident "$Revision: 1.11 $"

#include <sys/types.h>
#include <sys/SN/agent.h>
#include <sys/SN/promcfg.h>
#include <libkl.h>
#include "ip27prom.h"
#include "prom_leds.h"
#include "discover.h"
#include "net.h"
#include "libc.h"
#include "hub.h"
#include "rtc.h"

#ifdef ROUTER1_WAR

#define LDEBUG		0	/* Set to 1 for debug compilation */

#define FAKE_NODE		15
#define SINGLE_LOOP_SIZE	15
#define DOUBLE_LOOP_SIZE	30
#define INJECT_ADDR		TO_NODE(FAKE_NODE, 0x9600000003700080UL)
#define INJECT_DATA		0xa5a5deadfacebeef
					/* if interpreted as a vector,
					   local block will throw it away.*/

static net_vec_t war_route(pcfg_t *p, int dst)
{
    int			alt, i;
    net_vec_t		vec;

    /*
     * Search for alternate paths to dst until one is found that doesn't
     * involve any router ports 1 or 2.
     */

    for (alt = 0; ; alt++) {
	if ((vec = discover_route(p, 0, dst, alt)) == NET_VEC_BAD)
	    break;

	for (i = 0; i < vector_length(vec); i++)
	    if (vector_get(vec, i) == 1 || vector_get(vec, i) == 2)
		break;

	if (i == vector_length(vec))
	    break;
    }

    return vec;
}

static int war_path(net_vec_t vec, nasid_t dst_nasid)
{
    net_vec_t		inport;
    __uint64_t		reg;
    int			len, i, r;

    printf("Programming path 0x%lx for router loop\n", vec);

    len = vector_length(vec);

    for (i = 0; i < len; i++) {
#if LDEBUG
	printf("Reading router path 0x%llx status\n", vector_prefix(vec, i));
#endif
	if ((r = vector_read(vector_prefix(vec, i), 0,
			     RR_STATUS_REV_ID, &reg)) < 0)
	    goto done;

	if (GET_FIELD(reg, RSRI_CHIPID) != CHIPID_ROUTER) {
	    printf("Path 0x%llx is not a router.\n", vector_prefix(vec, i));
	    r = NET_ERROR_VECTOR;
	    goto done;
	}

	if ((reg & RSRI_LINKWORKING(vector_get(vec, i))) == 0) {
	    printf("Router path 0x%llx link %lld is down.\n",
		   vector_prefix(vec, i), vector_get(vec, i));
	    r = NET_ERROR_VECTOR;
	    goto done;
	}

	inport = GET_FIELD(reg, RSRI_INPORT);
#if LDEBUG
	printf("Router path 0x%llx inport is %llx\n",
	       vector_prefix(vec, i), inport);
#endif
	if (i == 0) {
	    /* Program local table entry */
#if LDEBUG
	    printf("Set local table entry %d to %lld turns\n",
		   dst_nasid, RR_DISTANCE(inport, vector_get(vec, i)));
#endif
	    SD(LOCAL_HUB(NI_LOCAL_TABLE(dst_nasid)),
	       RR_DISTANCE(inport, vector_get(vec, i)));
	} else {
	    /* Program router table entry */
#if LDEBUG
	    printf("Set router path 0x%llx "
		   "entry %d leaving %lld to %lld turns\n",
		   vector_prefix(vec, i - 1),
		   dst_nasid, vector_get(vec, i - 1),
		   RR_DISTANCE(inport, vector_get(vec, i)));
#endif
	    if ((r = vector_read(vector_prefix(vec, i - 1), 0,
				 RR_LOCAL_TABLE(dst_nasid), &reg) < 0))
		goto done;

	    reg = vector_modify(reg, vector_get(vec, i - 1) - 1,
				RR_DISTANCE(inport, vector_get(vec, i)));

	    if ((r = vector_write(vector_prefix(vec, i - 1), 0,
				  RR_LOCAL_TABLE(dst_nasid), reg)) < 0)
		goto done;
	}
    }

    r = 0;

 done:

    return r;
}

/*
 * router_war_needed
 */

int router_war_needed(void)
{
    pcfg_t	       *p;
    int			i;

    p = PCFG(get_nasid());

    for (i = 0; i < p->count; i++)
	if (p->array[i].any.type == PCFG_TYPE_ROUTER &&
	    p->array[i].router.version < 2)
	    return 1;

    return 0;
}

/*
 * router_war_channel
 */

int router_war_channel(int chan)
{
    printf("%T: Setting all virtual channels to %d\n", chan);

    SD(LOCAL_HUB(NI_AGE_CPU0_MEMORY), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_CPU0_PIO   ), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_CPU1_MEMORY), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_CPU1_PIO   ), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_GBR_MEMORY ), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_GBR_PIO	   ), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_IO_MEMORY  ), chan << NAGE_VCH_SHFT);
    SD(LOCAL_HUB(NI_AGE_IO_PIO	   ), chan << NAGE_VCH_SHFT);

    return 0;
}

/*
 * router_war_part1
 *
 *   Called from global master only to set up a deadlocked loop between
 *   each pair of routers.
 */

int router_war_part1(void)
{
    pcfg_t	       *p;
    int			ri, i, r;
    __uint64_t		age_reg;
    int			rc = -1;

    age_reg = (__uint64_t) LOCAL_HUB(hub_cpu_get() == 0 ?
				     NI_AGE_CPU0_PIO :
				     NI_AGE_CPU1_PIO);

    p = PCFG(get_nasid());

    /*
     * To implement this work-around, each router in the system must be
     * paired off with another router such that their ports 1 and 2 are
     * cross-wired together to form a loop.
     */

    for (ri = 0; ri < p->count; ri++) {
	pcfg_router_t	       *pr, *rr;
	net_vec_t		vec;

	if (p->array[ri].any.type != PCFG_TYPE_ROUTER)
	    continue;

	pr = &p->array[ri].router;

	/*
	 * This check will pass if ports A and B are directly connected.
	 */
	if (pr->port[1].index == PCFG_INDEX_INVALID ||
	    p->array[pr->port[1].index].any.type != PCFG_TYPE_ROUTER ||
	    pr->port[2].index != pr->port[1].index) {
	    printf("Router index %d ports 1 and 2 are not directly connected "
		   "or connected to ports 2 and 1 of another router.\n", ri);
	    goto done;
	}

	rr = &p->array[pr->port[1].index].router;

	/*
	 * this shouldn't be based on the NIC,
	 * but rather on which router is closer, to use as few
	 * unfixed hops as possible.
	 */
	if (pr->nic > rr->nic)	/* Will get this pair later */
	    continue;

	if (pr->nic == rr->nic) {
	    /* loopback case
	     */
	    printf("Found a looped router: index %d.\n", ri);

	    /* loopback = 1; */
	    /*
	     * First direction (leaving port 1):
	     * Establish a PIO path from the global master through the
	     * first router to the second and back to the first.
	     */

	    if ((vec = war_route(p, ri)) == NET_VEC_BAD) {
		printf("Internal error: could not find route to index %d\n", ri);
		goto done;
	    }

	    if ((r = war_path(vector_concat(vec, 0x11), FAKE_NODE)) < 0) {
	    vecerr:
		printf("Vector operation failed: %s\n", net_errmsg(r));
		goto done;
	    }

	    /*
	     * Inject PIOs into the loop to take up all the DAMQ slots.
	     * The age must be adjusted up on each one.
	     */

	    printf("Injecting %d writes to %y to fill %d DAMQ entries.\n",
		   (SINGLE_LOOP_SIZE + 1) / 2, INJECT_ADDR, SINGLE_LOOP_SIZE);

	    for (i = 0; i < (SINGLE_LOOP_SIZE + 1) / 2; i++) {

		if (i%4==0) {
		    printf("Setting port A sderr in router vec=0x%lx.\n", vec);
		    if ((r = vector_write(vec, 0,
					   RR_DIAG_PARMS, RDPARM_SENDERROR(1))) < 0)
			goto vecerr;
		}

		SD(age_reg, i * 4);
		SD(INJECT_ADDR, INJECT_DATA);
	    }

	    printf("A->B Loop filled.\n");

	    /*
	     * Second direction (leaving port 2):
	     */

	    if ((vec = war_route(p, ri)) == NET_VEC_BAD) {
		printf("Internal error: could not find route to index %d\n", ri);
		goto done;
	    }

	    if ((r = war_path(vector_concat(vec, 0x22), FAKE_NODE)) < 0)
		goto vecerr;

	    /*
	     * Inject PIOs into the loop to take up all the DAMQ slots.
	     * The age must be adjusted up on each one.
	     */
	    printf("Injecting %d writes to %y to fill %d DAMQ entries.\n",
		   (SINGLE_LOOP_SIZE + 1) / 2, INJECT_ADDR, SINGLE_LOOP_SIZE);

	    for (i = 0; i < (SINGLE_LOOP_SIZE + 1) / 2; i++) {

		if (i%4==0) {
		    printf("Setting port B sderr in router vec=0x%lx\n", vec);
		    if ((r = vector_write(vec, 0,
					   RR_DIAG_PARMS, RDPARM_SENDERROR(2))) < 0)
			goto vecerr;
		}

		SD(age_reg, i * 4);
		SD(INJECT_ADDR, INJECT_DATA);
	    }

	    printf("B->A Loop filled.\n");

	}
	else {
	    printf("Found pair of looped routers: indices %d and %d.\n",
		   ri, pr->port[1].index);

	    /*
	     * First direction (leaving port 1):
	     * Establish a PIO path from the global master through the
	     * first router to the second and back to the first.
	     */

	    vec = war_route(p, ri);

	    if (vec == NET_VEC_BAD) {
		if ((vec = war_route(p, pr->port[1].index)) == NET_VEC_BAD) {
		    printf("\n***Internal error: could not route to index %d or %d",
			   ri, pr->port[1].index);
		    printf("without using ports A, B***\n\n");
		    goto done;
		}
	    }

	    if ((r = war_path(vector_concat(vec, 0x111), FAKE_NODE)) < 0)
		goto vecerr;

	    /*
	     * Inject PIOs into the loop to take up all the DAMQ slots.
	     * The age must be adjusted up on each one.
	     */

	    printf("Injecting %d writes to %y to fill %d DAMQ entries.\n",
		   (DOUBLE_LOOP_SIZE + 1) / 2, INJECT_ADDR, DOUBLE_LOOP_SIZE);

	    for (i = 0; i < (DOUBLE_LOOP_SIZE + 1) / 2; i++) {

		if (i%4==0) {
		    printf("Setting port A sderr in router vec=0x%lx.\n", vec);
		    if ((r = vector_write(vec, 0,
					  RR_DIAG_PARMS, RDPARM_SENDERROR(1))) < 0)
			goto vecerr;
		}

		SD(age_reg, i * 4);
		SD(INJECT_ADDR, INJECT_DATA);
	    }

	    printf("A->B Loop filled.\n");

	    /*
	     * Second direction (leaving port 2):
	     * Establish a PIO path from the global master through the
	     * first router to the second and back to the first.
	     */

	    vec = war_route(p, ri);

	    if (vec == NET_VEC_BAD) {
		if ((vec = war_route(p, pr->port[1].index)) == NET_VEC_BAD) {
		    printf("\n***Internal error: could not route to index %d or %d",
			   ri, pr->port[1].index);
		    printf("without using ports A, B***\n\n");
		    goto done;
		}
	    }

	    if ((r = war_path(vector_concat(vec, 0x222), FAKE_NODE)) < 0)
		goto vecerr;

	    /*
	     * Inject PIOs into the loop to take up all the DAMQ slots.
	     * The age must be adjusted up on each one.
	     */

	    printf("Injecting %d writes to fill %d DAMQ entries in loop.\n",
		   (DOUBLE_LOOP_SIZE + 1) / 2, DOUBLE_LOOP_SIZE);

	    for (i = 0; i < (DOUBLE_LOOP_SIZE + 1) / 2; i++) {

		if (i%4==0) {
		    printf("Setting port B sderr in router vec=0x%lx\n", vec);
		    if ((r = vector_write(vec, 0,
					  RR_DIAG_PARMS, RDPARM_SENDERROR(2))) < 0)
			goto vecerr;
		}

		SD(age_reg, i * 4);
		SD(INJECT_ADDR, INJECT_DATA);
	    }

	    printf("B->A Loop filled.\n");
	}
    }

    rc = 0;

 done:

    if (rc < 0) {
	char	buf[8];
	printf("\n***ERROR: ROUTER WORKAROUND NOT IMPLEMENTED***\n\n");
	printf("Would you like to continue? [n] ");
	gets_timeout(buf, sizeof (buf), 5000000, "n");
	if (buf[0] == 'N' || buf[0] == 'n')
	  hub_led_flash(FLED_ROUTER_CONFIG); /* does not return. */
    }

    return rc;
}

/*
 * router_war_part2
 *
 *   Called from every node to set up a deadlocked entry in each input
 *   port of each router that's connected to a hub.  Does this by sending
 *   a PIO to FAKE_NODE, directing the request out of a deadlocked
 *   router port.
 *
 *   Assumes part 1 has already been done.
 */

int router_war_part2(void)
{
    pcfg_t	       *p;
    pcfg_router_t      *pr;
    net_vec_t		inport, inportx;
    __uint64_t		statusx;
    net_vec_t		turns;
    __uint64_t		data;
    int			rc = -1;
    int			x, r;
    __uint64_t		age_reg;

    age_reg = (__uint64_t) LOCAL_HUB(hub_cpu_get() == 0 ?
				     NI_AGE_CPU0_PIO :
				     NI_AGE_CPU1_PIO);

    /*
     * assume that we (the hub) are at index 0 in the pcfg array,
     * and that the router we are directly connected to is at index 1.
     */
    p = PCFG(get_nasid());
    inport = p->array[0].hub.port.port;

    printf("Fix router input port %d, to which I'm attached\n", inport);

    SD(LOCAL_HUB(NI_LOCAL_TABLE(FAKE_NODE)), RR_DISTANCE(inport, 1));

    SD(age_reg, 0 << NAGE_VCH_SHFT);
    SD(INJECT_ADDR, 0xdeed);
    SD(age_reg, 1 << NAGE_VCH_SHFT);

    /*
     * only send a msg through a port if is
     * connected to another router.
     * ports A, B form our deadlock loop;
     * use pcfg to determine what ports C-F are connected to.
     */
    pr = &p->array[1].router;

    for (x = 3; x <= 6; x += 3) {
	if ((pr->port[x].index != PCFG_INDEX_INVALID) &&
	    (p->array[pr->port[x].index].any.type == PCFG_TYPE_ROUTER)) {

	    printf("Fix input port attached to my router's port %d\n", x);

	    if ((r = vector_read(x, 0, RR_STATUS_REV_ID, &statusx)) < 0) {
	    vecerr:
		printf("Vector read failed: %s\n", net_errmsg(r));
		goto done;
	    }
	    inportx = GET_FIELD(statusx, RSRI_INPORT);

	    if ((r = vector_read(0, 0,
				 RR_LOCAL_TABLE(FAKE_NODE), &data)) < 0)
		    goto vecerr;

	    data = vector_modify(data, x - 1, RR_DISTANCE(inportx, 1));

	    if ((r = vector_write(0, 0,
				  RR_LOCAL_TABLE(FAKE_NODE), data)) < 0)
		    goto vecerr;

	    SD(LOCAL_HUB(NI_LOCAL_TABLE(FAKE_NODE)), RR_DISTANCE(inport, x));

	    SD(age_reg, 0 << NAGE_VCH_SHFT);
	    SD(INJECT_ADDR, 0xfeed);
	    SD(age_reg, 1 << NAGE_VCH_SHFT);
	}
    }

    rtc_sleep(100000);
#if LDEBUG
    printf("Clearing PI_ERR_INT_PEND\n");
#endif
    SD(LOCAL_HUB(PI_ERR_INT_PEND), 0xffffffffffffffffUL);

    rc = 0;

 done:

    return rc;
}

#endif /* ROUTER1_WAR */
