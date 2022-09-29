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

#ifdef IOPERF_DEBUG
int print_ioperf = 0;
int print_ioperf_clk = 0;
#define IO_PERF_PRINT(x) 	{ if (print_ioperf) printf##x; }
#define IO_PERF_CLKPRINT(x) 	{ if (print_ioperf_clk) printf##x; }
#else
#define IO_PERF_PRINT(x) 
#define IO_PERF_CLKPRINT(x) 
#endif

lock_t iop_lock;
ushort iop_mode;	/* Is the monitoring in global (SYS) mode? Or in node mode? */
int    iop_cnt;		/* How many nodes are monitoring locally? */

io_perf_monitor_t global_iop_mon; /* global monitor structre used for
				     centralization of collected data and some flags */

#define IO_PERF_ONE_MEG 1048576
#define LFSR_TABLE_SIZE (IO_PERF_ONE_MEG*4)

unsigned int *lfsr_table = NULL;

#define IPPR1_BASE 	16
#define IPPR_DISABLE	0x0F 

#define IOP_ENABLED		0x1
#define IOP_MODE_SYS		0x2  	/* Global monitor mode */
#define IOP_MODE_NODE		0x4	/* Node monitor node */

#define IOP_MODE_MASK   	(IOP_MODE_NODE | IOP_MODE_SYS)

#define IO_PERF_LOCK()		s = mutex_spinlock(&iop_lock);
#define IO_PERF_UNLOCK()	mutex_spinunlock(&iop_lock, s);

#define LOCK_IOP(_iop)		mutex_lock(&(_iop)->iopm_mutex, PZERO);
#define UNLOCK_IOP(_iop)	mutex_unlock(&(_iop)->iopm_mutex);

#define INVALID_NODE(_node) \
             ((_node >= maxnodes) || (_node < 0)|| (_node == CNODEID_NONE))

#if defined IOPERF_DEBUG
int 
lfsr_val(int lfsrVal) {
  if (lfsr_status == NO_LFSR_TABLE) {
    return 0;
  } else {
    return lfsr_table[lfsrVal];
  }	
}
#endif

/*
 * Function   : io_perf_init
 * Parameters : <none>
 *
 * Purpose : To setup the global (global_iop_mon) and node local
 * structures for performance monitoring.
 * 
 */
void 
io_perf_init(void)
{
	int i;
	io_perf_monitor_t *iop_mon;

	spinlock_init(&iop_lock, "ioperf_lock");
	iop_mode = 0;

	/* global performance monitor */
	init_mutex(&global_iop_mon.iopm_mutex, MUTEX_DEFAULT, "ioperf", 0);
	global_iop_mon.iopm_current = -1;
	global_iop_mon.iopm_gen = 0;
	global_iop_mon.iopm_flags = 0;

	/* The node by node performance monitors */
	for (i = 0; i < maxnodes; i++) {
		iop_mon = NODEPDA_IOP_MON(i);
		iop_mon->iopm_nasid = COMPACT_TO_NASID_NODEID(i);
		iop_mon->iopm_cnode = i;
		iop_mon->iopm_flags = 0;
		iop_mon->iopm_gen = 0;
		init_mutex(&iop_mon->iopm_mutex, MUTEX_DEFAULT, "ioperf", i);
	}

	/* Constructing lfsr Table */
	io_perf_init_lfsr();

}

/* The performance counters for II are not normal counters, instead
   they are Linear Feedback Shift Registers. Which act much like
   deterministic random number generators. The only way to translate
   their values into counts is by creating a lookup table of each LFSR
   value to its true count.

   Which explains the four megs of space allocated in
   io_perf_init_lfsr which is used to construct this table.

*/
void 
io_perf_init_lfsr(void) {

	int i, highbit;
	unsigned curval = 1;

	IO_PERF_PRINT(("io_perf_init_lfsr::Making table\n"));
	
	lfsr_table = 
	  (unsigned int*)kmem_alloc_node(LFSR_TABLE_SIZE, KM_NOSLEEP,
					 cnodeid());
	
	if (lfsr_table == NULL) {
	  printf("io_perf_init::Could not allocate table of size %d\n",
		 LFSR_TABLE_SIZE);
	  return;
	}

	/* Now fill in the values */
	for (i=0; i< IO_PERF_ONE_MEG-1; i++) {	

		lfsr_table[curval] = i;
		
		/* IPPRO[19] = IPPRO[3] XOR IPPRO[0] */
		highbit = ((curval & 0x8) >> 3) ^ (curval & 0x1); 

		/* IPPRO[18:0] = IPPRO[19:1] */
		if (highbit)
			curval = (curval >> 1) | 0x80000;
		else	
			curval = curval >> 1;
	}

	IO_PERF_PRINT(("io_perf_init_lfsr::Table complete\n"));
}

/*
 * Function   : io_perf_turnon_mode
 * 
 * Parameters : ctrl -> tests the performance monitor should collect
 * 		cnode -> node to turn on performance monitoring
 *		flag -> whether it is SYS or NODE MODE
 * 
 * Purpose    : To enable the performance monitor, if it is available.
 */ 
int
io_perf_turnon_node(io_perf_control_t *ctrl, cnodeid_t cnode, int flag)
{
	int i, s;
	io_perf_monitor_t *iop_mon;
	cpuid_t cpu;

	if (INVALID_NODE(cnode))
	    return EINVAL;

	if ((cpu = CNODE_TO_CPU_BASE(cnode)) == CPU_NONE)
	    return 0;

	if (IO_PERF_CTRLBITS_INVALID(ctrl->ctrl_bits)) {
		printf("Control bits [15:11] & [27:32] may not be set: 0x%X\n",
		       ctrl->ctrl_bits);
		return EINVAL;
	}

	IO_PERF_PRINT(("io_perf_turnon_node::Constructing monitor\n"));
	
	/* Retrieve the performance monitor */
	iop_mon = NODEPDA_IOP_MON(cnode);

	/* Lock it, if it is being used then just quit */
	LOCK_IOP(iop_mon);
	if (iop_mon->iopm_flags & IOP_ENABLED) {
		IO_PERF_PRINT(("Node %lld is already monitoring\n",cnode));
		UNLOCK_IOP(iop_mon);
		return EBUSY;
	}

	/* Init the structure */
	iop_mon->iopm_plex = iop_mon->iopm_flags = 0;
	bzero(&iop_mon->iopm_vals, sizeof(io_perf_values_t));
	iop_mon->iopm_current = -1;

	/* Set up the control bits specified */
	iop_mon->iopm_ctrl = *ctrl;

	IO_PERF_PRINT(("Control bits are %lx, test is %ld\n",ctrl,
		       iop_mon->iopm_current));

	/* The loop below is finding out which tests we are collecting
	 data for. The ctrl bitmask is being converted recorded with
	 the for loop index.

	 The first index is assigned as the the current test and all
	 subsequent tests are recorded by incrmenting the iopm_plex
	 variable. (See the io_perf_multiplex function below) */

	for (i = 0; i < IO_PERF_SETS; i++) {
	    if (iop_mon->iopm_ctrl.ctrl_bits & (1 << i)) {
		if (iop_mon->iopm_current == -1) 
		    iop_mon->iopm_current = i;
		else
		    iop_mon->iopm_plex++;
	    }
	}

	IO_PERF_PRINT(("Control bits are %lx, test is %ld\n",ctrl,
		       iop_mon->iopm_current));

	/* If we are not testing anything then quit */
	if (iop_mon->iopm_current == -1) {
		UNLOCK_IOP(iop_mon);
		return EINVAL;
	}
	
	/* Are we global or just a local performance monitor? */
	iop_mon->iopm_flags |= (flag & IOP_MODE_MASK);


	IO_PERF_LOCK();

	/* If we are globally busy then no lock allowed */
	if (iop_mon->iopm_flags & IOP_MODE_NODE) {
	    if (iop_mode == IOP_MODE_SYS) {
		IO_PERF_UNLOCK();
		UNLOCK_IOP(iop_mon);
		return EBUSY;
	    }
	    
	    /* Otherwise record that someone is doing a node based
               test */
	    iop_mode = IOP_MODE_NODE;
	    iop_cnt++;
	    iop_mon->iopm_gen++;
	}

	IO_PERF_UNLOCK();

	iop_mon->iopm_flags |= IOP_ENABLED;	

	/* Now that the kernel is setup alert the hardware.*/
	io_perf_start_count(iop_mon);

	UNLOCK_IOP(iop_mon);

	pdaindr[cpu].pda->p_ioperf = 1;	

	IO_PERF_PRINT(("Monitoring has begun for node %lld\n",cnode));

	return 0;
}

/*
 * Function   : io_perf_turnoff_node
 * Parameters : cnode -> the node to affect
 *              mode -> whether the turnoff is IOP_MODE_SYS (global)
 *    	                or IOP_MODE_NODE (local)
 * Purpose : To turn off one specific node's data collection and clear
 * out the kernel structures associated with that region. It collects
 * the data one more time before turning it off.  
 */
int
io_perf_turnoff_node(cnodeid_t cnode, int mode)
{
	io_perf_monitor_t *iop_mon;
	cpuid_t cpu;
	int s;

	IO_PERF_PRINT(("Entering io_perf_turnoff_node\n"));

	if (INVALID_NODE(cnode))
		return EINVAL;

	if ((cpu = CNODE_TO_CPU_BASE(cnode)) == CPU_NONE)
		return 0;
	
	iop_mon = NODEPDA_IOP_MON(cnode);
	
	LOCK_IOP(iop_mon);

	/* If this node was never marked as enabled something is wrong */
	if ((iop_mon->iopm_flags & IOP_ENABLED) == 0) {
		UNLOCK_IOP(iop_mon);
		return 0;
	}

	/* If a global turnoff is trying to execute a node turn off
           (or vice versa) something is wrong */
	if ((iop_mon->iopm_flags & IOP_MODE_MASK) != mode) {
		UNLOCK_IOP(iop_mon);
		return EINVAL;
	}
	    
	IO_PERF_LOCK();
	/*If it is a node unlock then we should decrement the counter
          of monitoring nodes */

	if (iop_mon->iopm_flags & IOP_MODE_NODE) {

	    if(iop_mode == IOP_MODE_SYS) {
		    IO_PERF_UNLOCK();
		    UNLOCK_IOP(iop_mon);
		    return EBUSY;
	    }

	    if (--iop_cnt == 0) {
		iop_mode = 0;
	    }
	    iop_mon->iopm_gen++;
	}
	IO_PERF_UNLOCK();
	
	iop_mon->iopm_flags &= ~IOP_ENABLED;

	io_perf_stop_count(iop_mon);
	io_perf_update_count(iop_mon);

	UNLOCK_IOP(iop_mon);

	pdaindr[cpu].pda->p_ioperf = 0;
	
	IO_PERF_PRINT(("Exiting io_perf_turnoff_node\n"));

	return 0;
}

/*
 * Function   : io_perf_enable
 * Parameters : ctrl -> the modes for data collection
 * 		rvalp -> a return value
 * Purpose : To turn on monitoring for all the nodes at one time, and
 * mark the sub-system for global monitoring.  
 */

int
io_perf_enable(io_perf_control_t *ctrl, int *rvalp)
{
	int s, i, error;
	io_perf_monitor_t *iop_mon;

	IO_PERF_PRINT(("io_perf_enable called\n"));	
	
	if (!_CAP_ABLE(CAP_DEVICE_MGT))
	    return EPERM;

	iop_mon = &global_iop_mon;

	LOCK_IOP(iop_mon);
	IO_PERF_LOCK();
	
	/* If anyone is doing any performance monitoring then we
           cannot do any now. */
	if (iop_mode) {
		IO_PERF_PRINT(("Cannot globally monitor while monitoring is "
			       "occurring.\n"));
		IO_PERF_UNLOCK();
		UNLOCK_IOP(iop_mon);
		return EBUSY;
	}

	/* Other wise mark the mode to say we are dong a global
           monitoring operation */
	iop_mode = IOP_MODE_SYS;

	IO_PERF_UNLOCK();
	
	/* Turn on every node for global monitoring. */
	for (i = 0; i < maxnodes; i++) {
	    if (error = io_perf_turnon_node(ctrl, i, IOP_MODE_SYS)) {
		UNLOCK_IOP(iop_mon);
		io_perf_disable();
		return error;
	    }
	}
	
	iop_mon->iopm_gen++;
	UNLOCK_IOP(iop_mon);

	*rvalp = iop_mon->iopm_gen;
	
	IO_PERF_PRINT(("io_perf_monitor is complete\n"));

	return 0;
}

/* 
 * Function : io_perf_disable 
 * Parameters : <none> 
 * Purpose : To stop all nodes from gathering performance data.  
 */
io_perf_disable(void)
{
    	int s, i, error;

	io_perf_monitor_t *iop_mon = &global_iop_mon;

	IO_PERF_PRINT(("io_perf_disable called\n"));

	if (!_CAP_ABLE(CAP_DEVICE_MGT))
	    return EPERM;

	LOCK_IOP(iop_mon);

	if (iop_mode != IOP_MODE_SYS) {
	    UNLOCK_IOP(iop_mon);
	    return EINVAL;
	}
	
	for (i = 0; i < maxnodes; i++) {
	    error = io_perf_turnoff_node(i, IOP_MODE_SYS);
	    ASSERT_ALWAYS(error == 0);
	}

	IO_PERF_LOCK();
	iop_mode = 0;

	IO_PERF_PRINT(("Nodes no longer monitoring and table is removed \n"));
	IO_PERF_UNLOCK();

	iop_mon->iopm_gen++;

	UNLOCK_IOP(iop_mon);

	IO_PERF_PRINT(("io_perf_disable is complete\n"));

	return 0;
}


/*
 * Function   : io_perf_node_enable
 * Parameters : ctrl -> the modes for data collection
 * 		rvalp -> a return value
 * Purpose : To turn on monitoring for one node at a time, and mark
 * the sub-system for global monitoring.  
 *
 * If the node is marked CNODEID_NONE it operates on the whole system.
 *
 */
int
io_perf_node_enable(io_perf_control_t *ctrl, cnodeid_t cnode, int *rvalp)
{
	int error;

	IO_PERF_PRINT(("io_perf_node_enable called for node %ld with control "
		       "0x%llx\n",cnode, ctrl));

	if (cnode == CNODEID_NONE) {
		return io_perf_enable(ctrl,rvalp);
	} 

	error = io_perf_turnon_node(ctrl, cnode, IOP_MODE_NODE);

	if (error)
	    return error;

	*rvalp = (NODEPDA_IOP_MON(cnode))->iopm_gen;

	return 0;
}


/*
 * Function   : io_perf_node_disable
 * Parameters : cnode -> Node to halt
 *
 * Purpose    : To stop one node from gathering performance data.
 *
 * If the node is marked CNODEID_NONE it operates on the whole system.
 *
 */ 
int
io_perf_node_disable(cnodeid_t cnode)
{
	if (cnode == CNODEID_NONE) {
		return io_perf_disable();
	} else {
		return io_perf_turnoff_node(cnode, IOP_MODE_NODE);
	}
}

/*
 * Function   : io_perf_get_count
 * Parameters : al    -> return value holding all the counter's contents.
 *              rvalp -> A return value
 * Purpose : Retrieving the data collected by the counter 
 */
int
io_perf_get_count(io_perf_values_t *val, int *rvalp)
{

   	io_perf_monitor_t *iop_node;
	int ii, set;

	IO_PERF_PRINT(("io_perf_get_count called\n"));

	if (iop_mode != IOP_MODE_SYS) {
		*rvalp = -1;
		return -1;
	}

	bzero(val, sizeof(io_perf_values_t));

	for (ii = 0; ii < maxnodes; ii++) {
	    iop_node = NODEPDA_IOP_MON(ii);
	    for (set = 0; set < IO_PERF_SETS; set++) {
	      val->iopv_timestamp[set] = iop_node->iopm_vals.iopv_timestamp[set];
	      val->iopv_count[set] += iop_node->iopm_vals.iopv_count[set];
	    }
	}	
	
	*rvalp = global_iop_mon.iopm_gen;
	IO_PERF_PRINT(("io_perf_get_count complete\n"));
	return 0;
}

/*
 * Function   : io_perf_node_count
 * Parameters : cnode -> The node desired
 *		val -> Return value, holding all of the counter's contents
 *              rvalp -> A return value
 * Purpose : Retrieving the data collected by the counter.
 */
int
io_perf_get_node_count(cnodeid_t cnode, io_perf_values_t *val, int *rvalp)
{
	io_perf_monitor_t *iop_mon;
	int ii;
	
	IO_PERF_PRINT(("Entering io_perf_get_node for node %lld, maxnode %lld\n",
		       cnode,maxnodes));
	
	/* Do we want a global check? */
	if (cnode == CNODEID_NONE) {
		return io_perf_get_count(val,rvalp);
	}

	if (INVALID_NODE(cnode)) {
	  return -1;
	}

	/* clear out the out-arg */
	bzero(val, sizeof(io_perf_values_t));


	/* grab the monitor */
	iop_mon = NODEPDA_IOP_MON(cnode);
	
	/* Running through all the counters in the monitor and
	   assigning the value to the passed in array */
	for (ii = 0; ii  < IO_PERF_SETS; ii++) {
		val->iopv_timestamp[ii] = iop_mon->iopm_vals.iopv_timestamp[ii];
		val->iopv_count[ii] = iop_mon->iopm_vals.iopv_count[ii];
	}	 
	
	if ((iop_mon->iopm_flags & IOP_MODE_MASK) == IOP_MODE_NODE)
	    *rvalp = iop_mon->iopm_gen;
	else
	    *rvalp = global_iop_mon.iopm_gen;

	IO_PERF_PRINT(("Exiting io_perf_get_node\n"));

	return 0;
}


/*
 * Function   : io_perf_get_ctrl
 * Parameters : ctrl  -> return value for user
 * 		rvalp -> return value (used for syssgi)
 *
 * Purpose : To retrieve the global performance monitor and see what
 * we are collecting data for.  
 */
int
io_perf_get_ctrl(io_perf_control_t *ctrl, int *rvalp)
{
	io_perf_monitor_t *iop_mon;

	iop_mon = &global_iop_mon;
	LOCK_IOP(iop_mon);
	if ((iop_mon->iopm_flags & IOP_ENABLED) == 0) {
	    UNLOCK_IOP(iop_mon);
	    return EINVAL;
	}
	*ctrl = iop_mon->iopm_ctrl;
	*rvalp = iop_mon->iopm_gen;

	UNLOCK_IOP(iop_mon);

	return 0;
}


/*
 * Function   : io_perf_get_node_ctrl
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
io_perf_get_node_ctrl(cnodeid_t cnode, io_perf_control_t *ctrl, int *rvalp)
{
    	io_perf_monitor_t *iop_mon;
	
	if (cnode == CNODEID_NONE) {
	  return io_perf_get_ctrl(ctrl,rvalp);
	}

	/* valid node? */
	if (INVALID_NODE(cnode)) {
	    return EINVAL;
	}

	/* Get the performance monitor for the node */
	iop_mon = NODEPDA_IOP_MON(cnode);
	LOCK_IOP(iop_mon); /* Lock it */

	/* If it is already collecting data return nothing */
	if ((iop_mon->iopm_flags & IOP_ENABLED) == 0) {
	    UNLOCK_IOP(iop_mon);
	    return EINVAL;
	}
	
	*ctrl = iop_mon->iopm_ctrl;
	if ((iop_mon->iopm_flags & IOP_MODE_MASK) == IOP_MODE_NODE)
	    *rvalp = iop_mon->iopm_gen;
	else
	    *rvalp = global_iop_mon.iopm_gen;

	UNLOCK_IOP(iop_mon);

	return 0;
}

/*
 * Function   : io_perf_start_count
 * Parameters : iop_mon -> performance monitor
 *
 * Purpose : To tell the hardware of the performance monitor to start
 * collecting data by writing the enable, and data collection mode to
 * the appropriate register 
 */
void
io_perf_start_count(io_perf_monitor_t *iop_mon)
{
	io_perf_sel_t iosel;

	/* First collect the current state so we can retrieve the ICCT */
	iosel.perf_sel_reg = REMOTE_HUB_L(iop_mon->iopm_nasid,IIO_IPCR);
	IO_PERF_CLKPRINT(("Start: Read Node:%d Addr: 0x%lx Value: 0x%lx \n",
	       iop_mon->iopm_nasid,IIO_IPCR,iosel.perf_sel_reg));
	
	/* We have to set the one field we want to test and then
           disable the other or else risk getting counts for BOTH,
	   Note we also leave the ICCT untouched. */

	if (iop_mon->iopm_current < IPPR1_BASE) {
		iosel.perf_sel_bits.perf_ippr1 = IPPR_DISABLE;
		iosel.perf_sel_bits.perf_ippr0 = iop_mon->iopm_current;
	} else {
		iosel.perf_sel_bits.perf_ippr1 = iop_mon->iopm_current - IPPR1_BASE;
		iosel.perf_sel_bits.perf_ippr0 = IPPR_DISABLE;
	}
	
	/* This read will clear the counter to logical zero (or LFSR 1) */
	IO_PERF_CLKPRINT(("Start: Clearing Node: %d Addr: 0x%lx\n",	
			  iop_mon->iopm_nasid, IIO_IPPR));
	REMOTE_HUB_L(iop_mon->iopm_nasid, IIO_IPPR);

	/* Actually do the update */
	IO_PERF_CLKPRINT(("Start: Writing Node: %d Addr:0x%lx Value:0x%lx\n\n",
			  iop_mon->iopm_nasid,IIO_IPCR,iosel.perf_sel_reg));
	REMOTE_HUB_S(iop_mon->iopm_nasid, IIO_IPCR, iosel.perf_sel_reg);

}


/*
 * Function   : io_perf_stop_count
 * Parameters : iop_mon -> performance monitor for specific node
 * 
 * Purpose    : To tell the hardware to stop collecting performance data
 */ 
void
io_perf_stop_count(io_perf_monitor_t *iop_mon)
{	
	io_perf_sel_t iosel;

	/* Init the bytes */
	iosel.perf_sel_reg = REMOTE_HUB_L(iop_mon->iopm_nasid,IIO_IPCR);
	IO_PERF_CLKPRINT(("Stop:Writing Node: %d Addr:0x%lx Value 0x%lx\n",
			  iop_mon->iopm_nasid,IIO_IPCR,iosel.perf_sel_reg));


	iosel.perf_sel_bits.perf_ippr0 = IPPR_DISABLE;
	iosel.perf_sel_bits.perf_ippr1 = IPPR_DISABLE;

	IO_PERF_CLKPRINT(("Stop:Writing Node: %d Addr:0x%lx Value 0x%lx\n\n",
			  iop_mon->iopm_nasid,IIO_IPCR,iosel.perf_sel_reg));
	REMOTE_HUB_S(iop_mon->iopm_nasid, IIO_IPCR, iosel.perf_sel_reg);

}

/*
 * Function   : io_perf_update_count
 * Parameters : iop_mon -> The performance monitor to be updated.
 *
 * Purpose : The function accpets a perf_monitor and then increments
 * its counters by the values the hub collected into it (checking for
 * overflow). Since the regs are read clear they are zero(or one) at
 * the end of the function
 *  
 */
int
io_perf_update_count(io_perf_monitor_t *iop_mon)
{
	/* Retrieve the counter */
	io_perf_cnt_t io_cnt;
	
	if (lfsr_table == NULL) {
		IO_PERF_PRINT(("No LFSR TABLE!!!\n"));
		return -1;
	}

	/* Get the mixed up LFSR count */
	/* This read will clear the register to logical zero (or LFSR 1) */
	io_cnt.perf_cnt = REMOTE_HUB_L(iop_mon->iopm_nasid, IIO_IPPR);

	IO_PERF_CLKPRINT(("Update: Read Node: %d Addr: 0x%lx Value 0x%lx, "
			  "Test %d\n",
			  iop_mon->iopm_nasid,IIO_IPPR,
			  io_cnt.perf_cnt,iop_mon->iopm_current));

	IO_PERF_CLKPRINT(("Update: LFSR is 0x%x\n",
			  lfsr_table[io_cnt.perf_cnt_bits.perf_cnt]));

	/* Index the LFSR table to find out what the true
	   number is, then increment the current count */
	iop_mon->iopm_vals.iopv_count[iop_mon->iopm_current] += 
		lfsr_table[io_cnt.perf_cnt_bits.perf_cnt];

	/* Retrieve that time stamp */
	iop_mon->iopm_vals.iopv_timestamp[iop_mon->iopm_current] = lbolt;

	return 0;
}

/*
 * Function : io_perf_multiplex
 * Parameters : cnode -> node performance counter works on
 *
 * Purpose : To allow a performance counter to be set for more then
 * one data collection modes at once. io_perf_multiplex will have the
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
io_perf_multiplex(cnodeid_t  cnode)
{
	io_perf_monitor_t *iop_mon;
	int next;

	iop_mon = NODEPDA_IOP_MON(cnode);
	
	io_perf_stop_count(iop_mon);
	io_perf_update_count(iop_mon);

	if ((iop_mon->iopm_flags & IOP_ENABLED) == 0) {
		return;
	}

	if (iop_mon->iopm_plex) {
	    next = iop_mon->iopm_current;

	    do {
		    if (++next == IO_PERF_SETS) {
			    next = 0;
		    }

		    if (iop_mon->iopm_ctrl.ctrl_bits & (1 << next)) {
			    break;
		    }

	    } while (next != iop_mon->iopm_current);

	    

	    ASSERT(next != iop_mon->iopm_current);
	    iop_mon->iopm_current = next;
	}

	io_perf_start_count(iop_mon);	

}



