/***********************************************************************\
 *       File:           net.c                                          *
 *                                                                      *
 *       Contains code for testing and configuring the network port in  *
 *       the local hub and remote routers.                              *
 *                                                                      *
 \***********************************************************************/

#ident "$Revision: 1.34 $"

#define TRACE_FILE_NO		4

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/IP27.h>

#include "ip27prom.h"
#include "net.h"
#include "rtc.h"
#include "libc.h"
#include "libasm.h"

#define AGE_CONTROL_DEFAULT	\
	(0L << 10 |		/* Virtual channel A (2 bits) */	\
	 0L << 8 |		/* Congestion control off */		\
	 0)			/* Message age field (8 bits) */

/*
 * net_node_get
 *
 *   Return the node field of the NI_STATUS_REV_ID register.
 */

int
net_node_get(void)
{
    return (int) GET_FIELD(LD(LOCAL_HUB(NI_STATUS_REV_ID)), NSRI_NODEID);
}

/*
 * net_node_set
 *
 *   Set the node field of the NI_STATUS_REV_ID register without
 *   affecting the other fields.
 *
 *   Changing the node ID is very dangerous.  If there are any
 *   outstanding hub transactions, and the node ID changes before
 *   the transaction is acknowledged, the acknowledgement will
 *   not reach the hub and it will hang.
 *
 *   Transactions to the NI section of the hub, and to Ualias,
 *   Lboot, and Ialias, do not have this problem.  But the PI and
 *   MD do.  The following code should work when running out of
 *   Ualias or cached memory.
 *
 *   When running from cached memory, the code tries to make sure
 *   everything is prefetched into the cache so that there won't be
 *   any outstanding cache fills (including speculative ones) going
 *   on.  It does 2 trial runs before the third one updates the ID.
 */

void
net_node_set(int node_id)
{
    __uint64_t		status1, status2, status3;
    int			i;
    int			loop_count;

    loop_count = 3;

    TRACE(node_id);

    status1 = LD(LOCAL_HUB(NI_STATUS_REV_ID));
    status2 = status1;
    status3 = status1;
    SET_FIELD(status3, NSRI_NODEID, node_id);

    for (i = 0; i < loop_count; i++) {
	sync_instr();
	SD(LOCAL_HUB(NI_STATUS_REV_ID), status1);
	sync_instr();

	delay(20);

	status1 = status2;
	status2 = status3;
    }

    TRACE(0);
}

/*
 * net_node_get_remote
 */

int net_node_get_remote(net_vec_t vec, nasid_t *nasid)
{
    int			r;
    __uint64_t		reg;

    if ((r = vector_read(vec, 0, NI_STATUS_REV_ID, &reg)) < 0)
	return r;

    *nasid = GET_FIELD(reg, NSRI_NODEID);

    return 0;
}

/*
 * net_node_set_remote
 */

int net_node_set_remote(net_vec_t vec, nasid_t nasid)
{
    int			r;
    __uint64_t		reg;

    if ((r = vector_read(vec, 0, NI_STATUS_REV_ID, &reg)) < 0)
	return r;

    SET_FIELD(reg, NSRI_NODEID, nasid);

    if ((r = vector_write(vec, 0, NI_STATUS_REV_ID, reg)) < 0)
	return r;

    return 0;
}

/*
 * net_init
 *
 *   Sets all HUB NI registers to their reset defaults.  The contents of
 *   error registers such as NI_PORT_ERROR should be saved before calling.
 *   Does not affect the SCRATCH registers or the router tables.
 */

void
net_init(void)
{
    hubreg_t rev;


    /*
     * It is VERY bad for the RTL simulation to access any registers
     * that are not initialized (have X's in them).  Unfortunately,
     * most of the registers are not initialized on reset.  We have to
     * initialize them all here.
     */

    TRACE(0);

#ifdef RTL
    /*
     * CAREFULLY zero out node ID.  In RTL simulation it may
     * be non-zero on reset.  Once it's zero, the other fields
     * may be set.
     */

    net_node_set(0);
#endif

    SD(LOCAL_HUB(NI_STATUS_REV_ID),
       MORE_MEMORY << NSRI_MORENODES_SHFT |
       REGIONSIZE_FINE << NSRI_REGIONSIZE_SHFT |
       0L << NSRI_NODEID_SHFT);

    /* Not touching NI_PORT_RESET */

    SD(LOCAL_HUB(NI_PROTECTION), NPROT_RESETOK);

    SD(LOCAL_HUB(NI_GLOBAL_PARMS),
       0x3ffL << NGP_MAXRETRY_SHFT |
       0xfffeL << NGP_TAILTOWRAP_SHFT |
       0xfL << NGP_CREDITTOVAL_SHFT |
       0xfL << NGP_TAILTOVAL_SHFT);

    /* Not initializing scratch registers here. */

    SD(LOCAL_HUB(NI_DIAG_PARMS), 0);

    /* Can't initialize NI_VECTOR_PARMS because that would send something */

    SD(LOCAL_HUB(NI_VECTOR), 0);
    SD(LOCAL_HUB(NI_VECTOR_DATA), 0);
    SD(LOCAL_HUB(NI_VECTOR_STATUS), 0);
    SD(LOCAL_HUB(NI_RETURN_VECTOR), 0);
    SD(LOCAL_HUB(NI_VECTOR_READ_DATA), 0);

    SD(LOCAL_HUB(NI_IO_PROTECT), ~0L); /* Disable prot. for all regions */

    SD(LOCAL_HUB(NI_AGE_CPU0_MEMORY), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_CPU0_PIO   ), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_CPU1_MEMORY), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_CPU1_PIO   ), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_GBR_MEMORY ), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_GBR_PIO	   ), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_IO_MEMORY  ), AGE_CONTROL_DEFAULT);
    SD(LOCAL_HUB(NI_AGE_IO_PIO	   ), AGE_CONTROL_DEFAULT);

    rev = ((LD(LOCAL_HUB(NI_STATUS_REV_ID)) & NSRI_REV_MASK) >> NSRI_REV_SHFT);

    if (rev == 2)
	SD(LOCAL_HUB(NI_PORT_PARMS), NPP_RESET_DFLT_HUB20);
    else
	SD(LOCAL_HUB(NI_PORT_PARMS), NPP_RESET_DEFAULTS);

    SD(LOCAL_HUB(NI_PORT_ERROR), 0);
    LD(LOCAL_HUB(NI_PORT_ERROR_CLEAR));	/* Clear errors */

    TRACE(0);
}

/*
 * net_link_reset
 *
 *   Reset link.
 */

void
net_link_reset(void)
{
    TRACE(0);

    SD(LOCAL_HUB(NI_PORT_RESET), 2); /* Link reset, resets NI */

    TRACE(0);
}

/*
 * net_link_up
 *
 *   Return values:
 *	0		Link is up
 *	non-zero	Link is down (failed after reset or still in reset)
 */

int
net_link_up(void)
{
    TRACE(0);

    return (int) GET_FIELD(LD(LOCAL_HUB(NI_STATUS_REV_ID)), NSRI_LINKUP);
}

/*
 * net_link_down_reason
 *
 *   Return values:
 *	0		Link failed after reset
 *	non-zero	Link never came out of reset
 */

int
net_link_down_reason(void)
{
    TRACE(0);

    return (int) GET_FIELD(LD(LOCAL_HUB(NI_STATUS_REV_ID)), NSRI_DOWNREASON);
}

/*
 * net_program
 *
 *   Given a vector from the current node to a remote node, this routine
 *   programs the local node's router tables, the remote node's router
 *   tables, and the intervening router's router tables to establish
 *   valid routing between the local and remote node.  If the remote node
 *   ID is also set, this will permit remote memory accesses to the node.
 *
 *   NOTE:  This routine programs router tables directly and is used only
 *   for diagnostic access to remote hubs.  The true router programming
 *   is calculated by discover_program and installed in main().
 *
 *   vec: Vector from the current node to a remote node
 *   src_nasid: intended NASID of local node
 *   dst_nasid: intended NASID of remote node
 *
 *   Returns 0 for success.
 *   Returns a negative error code (NET_ERROR_code) on error.
 */

int net_program(net_vec_t vec, nasid_t src_nasid, nasid_t dst_nasid)
{
    net_vec_t		retpath;
    net_vec_t		inport;
    __uint64_t		reg;
    int			len, i, r;

    len = vector_length(vec);

    if (len == 0) {
	db_printf("Zero hops; no programming is necessary.\n");
	r = 0;
	goto done;
    }

    /*
     * Program path from local node to remote node.
     * The return path is determined along the way.
     */

    retpath = 0;

    for (i = 0; i < len; i++) {
	db_printf("Reading router path 0x%llx status\n",
		  vector_prefix(vec, i));

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

	db_printf("Router path 0x%llx inport is %llx\n",
		  vector_prefix(vec, i), inport);

	retpath = retpath << 4 | inport;

	if (i == 0) {
	    /* Program local table entry */

	    db_printf("Set local table entry %d to %lld turns\n",
		      dst_nasid, RR_DISTANCE(inport, vector_get(vec, i)));

	    SD(LOCAL_HUB(NI_LOCAL_TABLE(dst_nasid)),
	       RR_DISTANCE(inport, vector_get(vec, i)));
	} else {
	    /* Program router table entry */

	    db_printf("Set router path 0x%llx "
		      "entry %d leaving %lld to %lld turns\n",
		      vector_prefix(vec, i - 1),
		      dst_nasid, vector_get(vec, i - 1),
		      RR_DISTANCE(inport, vector_get(vec, i)));

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

    db_printf("Return path is 0x%llx\n", retpath);

    /*
     * Program path from remote node to local node.
     * First do the remote table, then do the intervening routers.
     */

    db_printf("Set remote table entry %d to %lld turns\n",
	      src_nasid, RR_DISTANCE(vector_get(vec, len - 1),
				     vector_get(retpath, 0)));

    reg = RR_DISTANCE(vector_get(vec, len - 1),
		      vector_get(retpath, 0));

    if ((r = vector_write(vec, 0, NI_LOCAL_TABLE(src_nasid), reg)) < 0)
	goto done;

    for (i = 1; i < len; i++) {
	db_printf("Set router path 0x%llx entry %d "
		  "leaving %lld to %lld turns\n",
		  vector_prefix(vec, len - i), src_nasid,
		  vector_get(retpath, i - 1),
		  RR_DISTANCE(vector_get(vec, len - i - 1),
			      vector_get(retpath, i)));

	if ((r = vector_read(vector_prefix(vec, len - i), 0,
			      RR_LOCAL_TABLE(src_nasid), &reg)) < 0)
	    goto done;

	reg = vector_modify(reg, vector_get(retpath, i - 1) - 1,
			     RR_DISTANCE(vector_get(vec, len - i - 1),
					 vector_get(retpath, i)));

	if ((r = vector_write(vector_prefix(vec, len - i), 0,
			       RR_LOCAL_TABLE(src_nasid), reg)) < 0)
	    goto done;
    }

 done:

    if (r < 0)
	printf("Vector op failed: %s\n", net_errmsg(r));

    return r;
}
