/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: md_perf.c
 *	md performance stuff.
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/proc.h>
#include <sys/nodepda.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <ksys/hwperf.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>

lock_t 	mdp_lock;	/* A global lock for global data structures */
ushort  mdp_mode;	/* Is the monitoring in global (SYS) mode? Or in node mode? */
int     mdp_cnt;	/* How many nodes are monitoring locally? */
 
md_perf_monitor_t global_mdp_mon; /* global monitor structure used for
				     centralization of collected data
				     and some flags */

#define MDP_ENABLED	0x1
#define MDP_MODE_SYS	0x2  	/* Global monitor mode */
#define MDP_MODE_NODE	0x4	/* Node monitor node */

#define MDP_MODE_MASK   (MDP_MODE_NODE | MDP_MODE_SYS)

#define MD_PERF_LOCK()		s = mutex_spinlock(&mdp_lock);
#define MD_PERF_UNLOCK()	mutex_spinunlock(&mdp_lock, s);
#define LOCK_MDP(_mdp)		mutex_lock(&(_mdp)->mpm_mutex, PZERO);
#define UNLOCK_MDP(_mdp)	mutex_unlock(&(_mdp)->mpm_mutex);

#define REG_IS_PEGGED(_r)	((_r) == ((1 << 20) - 1))

#define INVALID_NODE(_node) \
             ((_node >= maxnodes) || (_node < 0)|| (_node == CNODEID_NONE))



#ifdef MDPERF_DEBUG
#define FLUSH_LIMIT 500
int print_md = 1;

#define MDPERF_PRINT(x) { if (print_md) printf##x; }

/* Useful debugging macros when checking for an overflow bug */
#define F_VALUES_EMPTY(vals, function) { \
    int ii,jj; \
    for (ii=0; ii < MD_PERF_SETS; ii++) { \
	for ( jj=0; jj < MD_PERF_COUNTERS; jj++) { \
	    if ((vals)->mpv_count[ii][jj].mpr_overflow != 0) { \
		 printf("\nFound it in %s at [%d][%d]!\n\n",function,ii,jj); \
	    } \
	} \
    } \
}

#else 
#define MDPERF_PRINT(x)
#define F_VALUES_EMPTY(val, function) 
#endif



/*
 * Function   : md_perf_init
 * Parameters : <none>
 *
 * Purpose : To setup the global (global_mdp_mon) and node local
 * structures for performance monitoring.
 * 
 */

void
md_perf_init(void)
{
	int i;
	md_perf_monitor_t *mdp_mon;

	spinlock_init(&mdp_lock, "mdperf_lock");
	mdp_mode = 0;

	/* global performance monitor */
	init_mutex(&global_mdp_mon.mpm_mutex, MUTEX_DEFAULT, "mdperf", 0);
	global_mdp_mon.mpm_current = -1;
	global_mdp_mon.mpm_gen = 0;
	global_mdp_mon.mpm_flags = 0;

	/* The node by node performance monitors */
	for (i = 0; i < maxnodes; i++) {
		mdp_mon = NODEPDA_MDP_MON(i);
		mdp_mon->mpm_nasid = COMPACT_TO_NASID_NODEID(i);
		mdp_mon->mpm_cnode = i;
		mdp_mon->mpm_gen = 0;
		mdp_mon->mpm_flags = 0;
		init_mutex(&mdp_mon->mpm_mutex, MUTEX_DEFAULT, "mdperf", i);
	}
}

/*
 * Function   : md_perf_turnon_mode
 * 
 * Parameters : ctrl  -> tests the performance monitor should collect
 * 		cnode -> node to turn on performance monitoring
 *		flag  -> whether it is SYS or NODE MODE
 * 
 * Purpose    : To enable the performance monitor, if it is available.
 */ 

int
md_perf_turnon_node(md_perf_control_t *ctrl, cnodeid_t cnode, int flag)
{
	int i, s, cnt;
	md_perf_monitor_t *mdp_mon;
	cpuid_t cpu;

	if (INVALID_NODE(cnode))
	    return EINVAL;

	if ((cpu = CNODE_TO_CPU_BASE(cnode)) == CPU_NONE)
	    return EINVAL;

	MDPERF_PRINT(("MD:Turn On, Ctrl: 0x%x, Node %d, Flag %d\n",
		      ctrl, cnode, flag));

	/* Retrieve the performance monitor */
	mdp_mon = NODEPDA_MDP_MON(cnode);

	/* Lock it, if it is being used then just quit */
	LOCK_MDP(mdp_mon);
	if (mdp_mon->mpm_flags & MDP_ENABLED) {
		UNLOCK_MDP(mdp_mon);
		return EBUSY;
	}

	/* Init the structure */
	mdp_mon->mpm_plex = mdp_mon->mpm_flags = 0;
	bzero(&mdp_mon->mpm_vals, sizeof(md_perf_values_t));

	F_VALUES_EMPTY(&mdp_mon->mpm_vals,"Turnon Top")

	mdp_mon->mpm_current = -1;

	/* Set up the control bits specified */
	mdp_mon->mpm_ctrl = *ctrl;

	/* The loop below is finding out which tests we are collecting
	 data for. The ctrl bitmask is being converted recorded with
	 the for loop index.

	 The first index is assigned as the the current test and all
	 subsequent tests are recorded by incrementing the mpm_plex
	 variable. (See the md_perf_multiplex function below) */

	for (i = 0; i < MD_PERF_SETS; i++) {
	    if (mdp_mon->mpm_ctrl.ctrl_bits & (1 << i)) {
		if (mdp_mon->mpm_current == -1) 
		    mdp_mon->mpm_current = i;
		else
		    mdp_mon->mpm_plex++;
	    }
	}

	MDPERF_PRINT(("MD Turn On: Test Selected: %d\n", mdp_mon->mpm_current));

	/* If we are not testing anything then quit. */
	if (mdp_mon->mpm_current == -1) {
		UNLOCK_MDP(mdp_mon);
		return EINVAL;
	}
	
	/* Are we global or just a local performance monitor? */
	mdp_mon->mpm_flags |= (flag & MDP_MODE_MASK);


	MD_PERF_LOCK();

	/* If we are globally busy then no lock allowed */
	if (mdp_mon->mpm_flags & MDP_MODE_NODE) {
	    if (mdp_mode == MDP_MODE_SYS) {
		MD_PERF_UNLOCK();
		UNLOCK_MDP(mdp_mon);
		return EBUSY;
	    }
	    
	    /* Otherwise record that someone is doing a node based
               test */
	    mdp_mode = MDP_MODE_NODE;
	    mdp_cnt++;
	    mdp_mon->mpm_gen++;
	}
	MD_PERF_UNLOCK();

	F_VALUES_EMPTY(&mdp_mon->mpm_vals,"Turn On before check");
	for (i = 0; i < MD_PERF_SETS; i++)
	    for (cnt = 0; cnt < MD_PERF_COUNTERS; cnt++)
		mdp_mon->mpm_vals.mpv_count[i][cnt].mpr_overflow = 0;

	mdp_mon->mpm_flags |= MDP_ENABLED;	

	/* Now that the kernel is setup alert the hardware.*/
	md_perf_start_count(mdp_mon);

	UNLOCK_MDP(mdp_mon);

	pdaindr[cpu].pda->p_mdperf = 1;	
	MDPERF_PRINT(("MD:Turned On Monitor, Node %d, Monitor flags:0x%x\n",
		      cnode,mdp_mon->mpm_flags));
	F_VALUES_EMPTY(&mdp_mon->mpm_vals,"Turnon Exit");
	return 0;
}

/*
 * Function   : md_perf_turnoff_node
 * Parameters : cnode -> the node to affect
 *              mode  -> whether the turnoff is MDP_MODE_SYS (global)
 *    	                 or MDP_MODE_NODE (local)
 * Purpose : To turn off one specific node's data collection and clear
 * out the kernel structures associated with that region. It collects
 * the data one more time before turning it off.  
 */

int
md_perf_turnoff_node(cnodeid_t cnode, int mode)
{
	md_perf_monitor_t *mdp_mon;
	cpuid_t cpu;
	int s;
	MDPERF_PRINT(("Enter Turn Off Monitor, Node:%d Mode:0x%x\n",
		      cnode, mode));

	if (INVALID_NODE(cnode))
	    return EINVAL;

	if ((cpu = CNODE_TO_CPU_BASE(cnode)) == CPU_NONE)
	    return -1;

	mdp_mon = NODEPDA_MDP_MON(cnode);

	MDPERF_PRINT(("MD Turn Off:Monitor flags:0x%x\n", mdp_mon->mpm_flags));
	
	LOCK_MDP(mdp_mon);

	/* If this node was never marked as enabled something is wrong */
	if ((mdp_mon->mpm_flags & MDP_ENABLED) == 0) {
		UNLOCK_MDP(mdp_mon);
		return -1;
	}

	/* If a global turnoff is trying to execute a node turn off
           (or vice versa) something is wrong */
	if ((mdp_mon->mpm_flags & MDP_MODE_MASK) != mode) {
		UNLOCK_MDP(mdp_mon);
		return EINVAL;
	}
	    
	MD_PERF_LOCK();
	/*If it is a node unlock then we should decrement the counter
          of monitoring nodes */
	if (mdp_mon->mpm_flags & MDP_MODE_NODE) {
	    ASSERT(mdp_mode != MDP_MODE_SYS);
	    if (--mdp_cnt == 0)
		mdp_mode = 0;
	    mdp_mon->mpm_gen++;
	}
	MD_PERF_UNLOCK();
	
	mdp_mon->mpm_flags &= ~MDP_ENABLED;
	
	md_perf_stop_count(mdp_mon);
	md_perf_update_count(mdp_mon);
	
	UNLOCK_MDP(mdp_mon);

	MDPERF_PRINT(("MD: Exiting Turn Off: Monitor's flags:0x%x\n",
		      mdp_mon->mpm_flags));
	
	pdaindr[cpu].pda->p_mdperf = 0;
	return 0;
}

/*
 * Function   : md_perf_enable
 * Parameters : ctrl  -> the modes for data collection
 * 		rvalp -> a return value
 * Purpose : To turn on monitoring for all the nodes at one time, and
 * mark the sub-system for global monitoring.  
 */


int
md_perf_enable(md_perf_control_t *ctrl, int *rvalp)
{
	int s, i, error;
	md_perf_monitor_t *mdp_mon;
	
	if (!_CAP_ABLE(CAP_DEVICE_MGT))
	    return EPERM;

	mdp_mon = &global_mdp_mon;

	LOCK_MDP(mdp_mon);
	MD_PERF_LOCK();
	
	/* If anyone is doing any performance monitoring then we
           cannot do any now. */
	if (mdp_mode) {
	    MD_PERF_UNLOCK();
	    UNLOCK_MDP(mdp_mon);
	    return EBUSY;
	}

	/* Other wise mark the mode to say we are dong a global
           monitoring operation */
	mdp_mode = MDP_MODE_SYS;

	MD_PERF_UNLOCK();
	
	/* Turn on every node for global monitoring. */
	for (i = 0; i < maxnodes; i++) {
		if (error = md_perf_turnon_node(ctrl, i, MDP_MODE_SYS)) {
			UNLOCK_MDP(mdp_mon);
			md_perf_disable();
			return error;
	    }
	}
	mdp_mon->mpm_gen++;
	mdp_mon->mpm_ctrl = *ctrl;
	mdp_mon->mpm_flags |= MDP_ENABLED;
	UNLOCK_MDP(mdp_mon);

	*rvalp = mdp_mon->mpm_gen;
	return 0;
}

/* 
 * Function : md_perf_disable 
 * Parameters : <none> 
 * Purpose : To stop all nodes from gathering performance data.  
 */

int
md_perf_disable()
{
    	int s, i, error;
	md_perf_monitor_t *mdp_mon = &global_mdp_mon;

	if (!_CAP_ABLE(CAP_DEVICE_MGT))
	    return EPERM;

	LOCK_MDP(mdp_mon);

	if (mdp_mode != MDP_MODE_SYS) {
	    UNLOCK_MDP(mdp_mon);
	    return EINVAL;
	}
	
	for (i = 0; i < maxnodes; i++) {
	    error = md_perf_turnoff_node(i, MDP_MODE_SYS);
	    ASSERT_ALWAYS(error == 0);
	}

	MD_PERF_LOCK();
	mdp_mode = 0;
	mdp_mon->mpm_flags &= ~MDP_ENABLED;
	MD_PERF_UNLOCK();

	mdp_mon->mpm_gen++;

	UNLOCK_MDP(mdp_mon);
	return 0;
}

/*
 * Function   : md_perf_node_enable
 * Parameters : ctrl  -> the modes for data collection
 * 		rvalp -> a return value
 * Purpose : To turn on monitoring for one node at a time, and mark
 * the sub-system for global monitoring.  
 *
 * If the node is marked CNODEID_NONE it operates on the whole system.
 *
 */

int
md_perf_node_enable(md_perf_control_t *ctrl, cnodeid_t cnode, int *rvalp)
{
	int error;
       
	if (cnode == CNODEID_NONE) {
	  return md_perf_enable(ctrl,rvalp);
	}

	MDPERF_PRINT(("MD: Node Enable, Ctrl:0x%x Node:%d\n",
		      ctrl,cnode));

	error = md_perf_turnon_node(ctrl, cnode, MDP_MODE_NODE);

	if (error) {
		MDPERF_PRINT(("MD: Turn ON returned error: %d\n",error));
		return error;
	}

	*rvalp = (NODEPDA_MDP_MON(cnode))->mpm_gen;

	return 0;
}

/*
 * Function   : md_perf_node_disable
 * Parameters : cnode -> Node to halt
 *
 * Purpose    : To stop one node from gathering performance data.
 *
 * If the node is marked CNODEID_NONE it operates on the whole system.
 *
 */ 

int 
md_perf_node_disable(cnodeid_t cnode)
{
    int	rv = 0;

    if (cnode == CNODEID_NONE) {
      return md_perf_disable();
    }

    rv = md_perf_turnoff_node(cnode, MDP_MODE_NODE);

    return(rv);
}

/*
 * Function   : md_perf_transfer_values
 * Parameters : retVal -> The destination for values
 *		src    -> The src of values
 *
 * Purpose    : Just an update of the destinations values from the src's contents.
 */ 

void
md_perf_transfer_values(md_perf_values_t *retVal, md_perf_values_t* src) 
{
    int set, cnt;
    
    for (set = 0; set < MD_PERF_SETS; set++) {

	retVal->mpv_timestamp[set] = src->mpv_timestamp[set];

	for (cnt = 0; cnt < MD_PERF_COUNTERS; cnt++) {
	    /* Assign the value */
	    retVal->mpv_count[set][cnt].mpr_value += src->mpv_count[set][cnt].mpr_value;
	    
	    /* Assign and check for overflow. */
	    retVal->mpv_count[set][cnt].mpr_overflow |= 
		src->mpv_count[set][cnt].mpr_overflow;


	    /* Check for overflow, clear if necessary */
	    if (src->mpv_count[set][cnt].mpr_overflow) {
		src->mpv_count[set][cnt].mpr_overflow = 0;
	    }
	}
    }
}

/*
 * Function   : md_perf_get_count
 * Parameters : val   -> return value holding all the counter's contents.
 *              rvalp -> A return value
 * Purpose : Retrieving the data collected by the counter 
 */

int
md_perf_get_count(md_perf_values_t *val, int *rvalp)
{
	md_perf_monitor_t *mdp_node;
	int i;

	bzero(val, sizeof(md_perf_values_t));

	if (mdp_mode != MDP_MODE_SYS) {
		*rvalp = -1;
		return -1;
	}

	F_VALUES_EMPTY(val,"get count");

	for (i = 0; i < maxnodes; i++) {
	    mdp_node = NODEPDA_MDP_MON(i);
	    md_perf_transfer_values(val,&mdp_node->mpm_vals);
	}
	
	*rvalp = global_mdp_mon.mpm_gen;
	return 0;
}

/*
 * Function   : md_perf_node_count
 * Parameters : cnode -> The node desired
 *		val   -> Return value, holding all of the counter's contents
 *              rvalp -> A return value
 * Purpose : Retrieving the data collected by the counter.
 */


int
md_perf_get_node_count(cnodeid_t cnode, md_perf_values_t *val, int *rvalp)
{
	md_perf_monitor_t *mdp_mon;

	if (cnode == CNODEID_NONE) {
	  return md_perf_get_count(val,rvalp);
	}

	/* clear out the out-arg */
	bzero(val, sizeof(md_perf_values_t));
	F_VALUES_EMPTY(val,"User struct(getNodeCount)");
	
	if (INVALID_NODE(cnode)) {
	  return EINVAL;
	}

	/* grab the monitor */
	mdp_mon = NODEPDA_MDP_MON(cnode);

	F_VALUES_EMPTY(&mdp_mon->mpm_vals,"Monitor(getNodecount)");
	
	md_perf_transfer_values(val,&mdp_mon->mpm_vals);
	
	if ((mdp_mon->mpm_flags & MDP_MODE_MASK) == MDP_MODE_NODE)
	    *rvalp = mdp_mon->mpm_gen;
	else
	    *rvalp = global_mdp_mon.mpm_gen;

	MDPERF_PRINT(("Exiting Node Count Node %d,flags:0x%x\n",
		      cnode,mdp_mon->mpm_flags));

	return 0;
}

/*
 * Function   : md_perf_get_ctrl
 * Parameters : ctrl  -> return value for user
 * 		rvalp -> return value (used for syssgi)
 *
 * Purpose : To retrieve the global performance monitor and see what
 * we are collecting data for.  
 */

int
md_perf_get_ctrl(md_perf_control_t *ctrl, int *rvalp)
{
    	md_perf_monitor_t *mdp_mon;

	mdp_mon = &global_mdp_mon;
	LOCK_MDP(mdp_mon);
	if ((mdp_mon->mpm_flags & MDP_ENABLED) == 0) {
	    UNLOCK_MDP(mdp_mon);
	    return EINVAL;
	}
	*ctrl = mdp_mon->mpm_ctrl;
	*rvalp = mdp_mon->mpm_gen;

	UNLOCK_MDP(mdp_mon);

	return 0;
}

/*
 * Function   : md_perf_get_node_ctrl
 * Parameters : cnode -> node to get perf control for
 *		ctrl  -> what we are trying to retrieve <out-arg>
 * 		rvalp -> return value (used for syssgi)
 * 
 * Purpose : To retrieve the memory performance control structure for
 * a specific node.  First check if it is being used, if so return
 * err. Then ensure the performance monitor is a for a node and not a
 * global one.  
 *
 * If the node is marked CNODEID_NONE it operates on the whole system.
 *
 */

int
md_perf_get_node_ctrl(cnodeid_t cnode, md_perf_control_t *ctrl, int *rvalp)
{
    	md_perf_monitor_t *mdp_mon;
	
	if (cnode == CNODEID_NONE) {
	  return md_perf_get_ctrl(ctrl,rvalp);
	}

	/* valid node? */
	if (INVALID_NODE(cnode)) {
	    return EINVAL;
	}

	/* Get the performance monitor for the node */
	mdp_mon = NODEPDA_MDP_MON(cnode);
	LOCK_MDP(mdp_mon); /* Lock it */

	/* If it is already collecting data return nothing */
	if ((mdp_mon->mpm_flags & MDP_ENABLED) == 0) {
	    UNLOCK_MDP(mdp_mon);
	    return EINVAL;
	}
	
	*ctrl = mdp_mon->mpm_ctrl;
	if ((mdp_mon->mpm_flags & MDP_MODE_MASK) == MDP_MODE_NODE)
	    *rvalp = mdp_mon->mpm_gen;
	else
	    *rvalp = global_mdp_mon.mpm_gen;

	UNLOCK_MDP(mdp_mon);

	return 0;
}

/*
 * Function   : md_perf_start_count
 * Parameters : mdp_mon -> performance monitor
 *
 * Purpose : To tell the hardware of the performance monitor to start
 * collecting data by writing the enable, and data collection mode to
 * the appropriate register 
 */


void
md_perf_start_count(md_perf_monitor_t *mdp_mon)
{
	int	base = MD_PERF_CNT0;
	md_perf_sel_t mpsel;
	int i;

	/* Collect each counter's value, checking for overflow */
	for (i = 0; i < MD_PERF_COUNTERS; i++, base += 8) {
	  /* Clear each counter */
	  REMOTE_HUB_S(mdp_mon->mpm_nasid, base, 0);
	}
	/* Now set the control */
	mpsel.perf_sel_reg = 0;
	mpsel.perf_sel_bits.perf_en = 1;
	mpsel.perf_sel_bits.perf_sel = mdp_mon->mpm_current;
	
	REMOTE_HUB_S(mdp_mon->mpm_nasid, MD_PERF_SEL, mpsel.perf_sel_reg);
}

/*
 * Function   : md_perf_stop_count
 * Parameters : mdp_mon -> performance monitor for specific node
 * 
 * Purpose    : To tell the hardware to stop collecting performance data
 */ 

void
md_perf_stop_count(md_perf_monitor_t *mdp_mon)
{
	REMOTE_HUB_S(mdp_mon->mpm_nasid, MD_PERF_SEL, 0);
}

		     
/*
 * Function   : md_perf_update_count
 * Parameters : mdp_mon -> The performance monitor to be updated.
 *
 * Purpose : The function accpets a perf_monitor and then increments
 * its counters by the values the hub collected into it (checking for
 * overflow). When finished it clears the register in order to keep
 * collecting.
 * 
 */

void 
md_perf_update_count(md_perf_monitor_t *mdp_mon)
{
	int	base = MD_PERF_CNT0;
	int 	i;
	__uint64_t rval;

	/* Time stamp this retrieval, clobbering earlier value. */
	mdp_mon->mpm_vals.mpv_timestamp[mdp_mon->mpm_current] = lbolt;

	/* Collect each counter's value, checking for overflow */
	for (i = 0; i < MD_PERF_COUNTERS; i++, base += 8) {
	    rval = REMOTE_HUB_L(mdp_mon->mpm_nasid, base);
	    mdp_mon->mpm_vals.mpv_count[mdp_mon->mpm_current][i].mpr_value += rval;
	    if (REG_IS_PEGGED(rval)) {
		mdp_mon->mpm_vals.mpv_count[mdp_mon->mpm_current][i].mpr_overflow = 1;
	    }
	    
	}

}

/*
 * Function : md_perf_multiplex
 * Parameters : cnode -> node performance counter works on
 *
 * Purpose : To allow a performance counter to be set for more then
 * one data collection modes at once. md_perf_multiplex will have the
 * monitor stop counting data, collect the data and then start
 * counting data for another test. 
 *
 * Note: The monitor must already be setup for this use by having its
 * mode bits set for more than one test.  
 * 
 * This function is called from the tick_actions function with every
 * clock tick.  
 */

void
md_perf_multiplex(cnodeid_t cnode)
{
    	md_perf_monitor_t *mdp_mon;
	int next;

	mdp_mon = NODEPDA_MDP_MON(cnode);
	
	md_perf_stop_count(mdp_mon);
	md_perf_update_count(mdp_mon);

	if ((mdp_mon->mpm_flags & MDP_ENABLED) == 0)
	    return;

	if (mdp_mon->mpm_plex) {
	    next = mdp_mon->mpm_current;

	    do {
		if (++next == MD_PERF_COUNTERS) next = 0;
		if (mdp_mon->mpm_ctrl.ctrl_bits & (1 << next))
		    break;
	    } while (next != mdp_mon->mpm_current);
	    ASSERT(next != mdp_mon->mpm_current);
	    mdp_mon->mpm_current = next;
	}
	
	md_perf_start_count(mdp_mon);
}


	
       



