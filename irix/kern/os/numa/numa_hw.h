/*****************************************************************************
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

/*
 * Contains the hardware dependent code for page migration/replication
 * in NUMA architecture (SN0 and beyond)
 * XXX Try to hide all NASID's here 
 */

#ifndef __NUMA_HW_H__
#define __NUMA_HW_H__

#if SN0


/* XXX Do we keep these masks here, or in sys/SN ? */
#define METAID_MASK 0x1f << 4
#define LOCALID_MASK 0xf 

/* 
 * Flags to indicate the direction of change on the migration threshold 
 */
#define INCREASE 0x1
#define DECREASE 0x2

/*
 * One SN0, one software page(16k) corresponds to 4 hardware pages(4k)
 */
#define SWPFN_HWPFN_SHFT_DIFF           (PNUMSHFT - MD_PAGE_NUM_SHFT)
#define SWPFN_TO_HWPFN_BASE(swpfn)      ((swpfn) << SWPFN_HWPFN_SHFT_DIFF)
#define PHYSADDR_TO_HWPFN_OFFSET(pa)    (((pa) >> MD_PAGE_NUM_SHFT) & \
                                        ((0x1 << SWPFN_HWPFN_SHFT_DIFF) - 1))
#define HWPFN_TO_SWPFN(hwpfn)           ((hwpfn) >> (PNUMSHFT - MD_PAGE_NUM_SHFT))
#define NUM_OF_HW_PAGES_PER_SW_PAGE()   (NBPP / MD_PAGE_SIZE)

/*
 * Get the type of DIMM used for directory in that node
 * membank flavor can only be one of two possible values 0(std) or 1(premium)
 */
#define NODE_DIRTYPE_GET(node) ((unsigned char) \
	NODEPDA(node)->membank_flavor ? DIRTYPE_PREMIUM : DIRTYPE_STANDARD)

#define MIGR_COUNTER_PREMIUM_MAX     MD_PPROT_REFCNT_WIDTH
#define MIGR_COUNTER_STANDARD_MAX    MD_SPROT_REFCNT_WIDTH

#define MIGR_COUNTER_MAX_GET(nodeid) \
        (NODEPDA_MCD((nodeid))->migr_system_kparms.migr_threshold_reference)

/*
 * Rel <-> Abs conversion
 */
#define MIGR_COUNTER_REL_TO_EFF(nodeid, rel_value) \
        ( ((rel_value) * MIGR_COUNTER_MAX_GET(nodeid)) / 100 )

#define MIGR_COUNTER_EFF_TO_REL(nodeid, eff_value) \
        ( ((eff_value) * 100) / MIGR_COUNTER_MAX_GET(nodeid) )

/* -------------------------------------------------------------------------
  
   Page reference counters
 
   ------------------------------------------------------------------------- */

#define MIGR_COUNT_REG_ADDR(hwpfn, nodeid) \
        BDPRT_ENTRY( ((__uint64_t)(hwpfn)) << MD_PAGE_NUM_SHFT, COMPACT_TO_NASID_NODEID(nodeid) )


/*
 * Get the whole counter register value.
 * hwpfn is a 4KB page frame number, which
 * is different from software pfns (16KB).
 * XXX safe cast -- 48/16 bits 
 */


#define MIGR_COUNT_REG_GET(hwpfn, nodeid) \
        (*(ulong *)MIGR_COUNT_REG_ADDR(hwpfn, nodeid))


/*
 * Standard & Premium have the same shifts for
 * refcnt, migrmode, protfield.
 * Mode comes already shifted (AHHHHH - UGLY, UGLY!) <lfsh>
 * Must preserve current page permissions.
 */

#if MD_SPROT_MASK != MD_PPROT_MASK
#   error Macro assumes SPROT and PPROT same offset
#endif

#define MIGR_COUNT_REG_SET(hwpfn, nodeid, val, mode) \
        ((*(ulong *)MIGR_COUNT_REG_ADDR(hwpfn, nodeid)) = \
        (((val) << MD_PPROT_REFCNT_SHFT) | (mode) | \
	 (MD_PROT_RW<<MD_SPROT_SHFT) | \
	 (MD_PROT_RW<<MD_PPROT_IO_SHFT)))

/*
 * Get the actual reference count.
 * from a full counter-register value
 */
#define MIGR_COUNT_GET(value, nodeid) ( \
	(NODEPDA_MCD(nodeid)->hub_dir_type == DIRTYPE_PREMIUM) ? \
	MD_PPROT_REFCNT_GET(value) : MD_SPROT_REFCNT_GET(value))

/*
 * Get the actual reference count.
 * from a full counter-register value
 */

#define MIGR_COUNT_GET_DIRECT(hwpfn, counter_nodeid, home_nodeid) ( \
	(NODEPDA_MCD(home_nodeid)->hub_dir_type == DIRTYPE_PREMIUM) ? \
	MD_PPROT_REFCNT_GET(MIGR_COUNT_REG_GET((hwpfn), (counter_nodeid))) : \
        MD_SPROT_REFCNT_GET(MIGR_COUNT_REG_GET((hwpfn), (counter_nodeid))))

/*
 * Get the count mode from
 * a full counter-register value.
 */
#define MIGR_MODE_GET(value, nodeid) ( \
	(NODEPDA_MCD(nodeid).hub_dir_type == DIRTYPE_PREMIUM) ? \
        MD_PPROT_MIGMD_GET(value) : MD_SPROT_MIGMD_GET(value))

/*
 * Get the count mode from
 * a full counter-register value.
 */

#define MIGR_MODE_GET_DIRECT(hwpfn, nodeid) ( \
	(NODEPDA_MCD(nodeid)->hub_dir_type == DIRTYPE_PREMIUM) ? \
        MD_PPROT_MIGMD_GET(MIGR_COUNT_REG_GET((hwpfn), (nodeid))) : \
        MD_SPROT_MIGMD_GET(MIGR_COUNT_REG_GET((hwpfn), (nodeid))))


/* -------------------------------------------------------------------------
  
   Page Migration Control Register
 
   ------------------------------------------------------------------------- */

/*
 * Maximum migration threshold values
 */
#define MIGR_DIFF_THRESHOLD_PREMIUM_MAX      MIGR_COUNTER_PREMIUM_MAX
#define MIGR_DIFF_THRESHOLD_STANDARD_MAX     MIGR_COUNTER_STANDARD_MAX

#define MIGR_DIFF_THRESHOLD_MAX_GET(nodeid)  MIGR_COUNTER_MAX_GET(nodeid)

/*
 * Rel <-> Abs conversion
 */
#define MIGR_DIFF_THRESHOLD_REL_TO_EFF(nodeid, rel_value) \
        ( ((rel_value) * MIGR_DIFF_THRESHOLD_MAX_GET(nodeid)) / 100 )

#define MIGR_DIFF_THRESHOLD_EFF_TO_REL(nodeid, abs_value) \
        ( ((abs_value) * 100) / MIGR_DIFF_THRESHOLD_MAX_GET(nodeid) )

/*
 * Get the current Differential Threshold
 */
#define MIGR_THRESHOLD_DIFF_GET(nodeid) \
        (NODEPDA_MCD(nodeid)->migr_as_kparms.migr_base_threshold)

/*
 * Enable the Differential Comparison
 */
#define MIGR_THRESHOLD_DIFF_ENABLE(nodeid) ( \
        MD_MIG_DIFF_THRESH_ENABLE(COMPACT_TO_NASID_NODEID(nodeid)))

/*
 * Disable the Differential Comparisons
 */
#define MIGR_THRESHOLD_DIFF_DISABLE(nodeid) ( \
        MD_MIG_DIFF_THRESH_DISABLE(COMPACT_TO_NASID_NODEID(nodeid)))

/*
 * Predicate to check if the register is enabled 
 */
#define MIGR_THRESHOLD_DIFF_IS_ENABLED(nodeid) ( \
        MD_MIG_DIFF_THRESH_IS_ENABLED(COMPACT_TO_NASID_NODEID(nodeid)))

/* -------------------------------------------------------------------------
  
   Migration absolute threshold register (currently not used)
 
   ------------------------------------------------------------------------- */
        
/*
 * Set the Absolute Threshold & Enable
 */
#define MIGR_THRESHOLD_ABS_SET(nodeid, value) ( \
	MD_MIG_VALUE_THRESH_SET(COMPACT_TO_NASID_NODEID(nodeid), value))

/*
 * Get the Absolute Theshold
 */
#define MIGR_THRESHOLD_ABS_GET(nodeid) ( \
        MD_MIG_VALUE_THRESH_GET(COMPACT_TO_NASID_NODEID(nodeid)))

/*
 * Enable the Absolute mode
 */
#define MIGR_THRESHOLD_ABS_ENABLE(nodeid) ( \
        MD_MIG_VALUE_THRESH_ENABLE(COMPACT_TO_NASID_NODEID(nodeid)))

/*
 * Disable the Absolute Comparisons
 */
#define MIGR_THRESHOLD_ABS_DISABLE(nodeid) ( \
        MD_MIG_VALUE_THRESH_DISABLE(COMPACT_TO_NASID_NODEID(nodeid)))

/*
 * Predicate to check if the register is enabled 
 */
#define MIGR_THRESHOLD_ABS_IS_ENABLED(nodeid) ( \
        MD_MIG_VALUE_THRESH_IS_ENABLED(COMPACT_TO_NASID_NODEID(nodeid)))

/* -------------------------------------------------------------------------
  
   Migration candidate register 
 
   ------------------------------------------------------------------------- */

/*
 * Get the contents of the candidate
 * register using the CLR address
 */
#define MIGR_CANDIDATE_GET(nodeid) ( \
        MD_MIG_CANDIDATE_GET(COMPACT_TO_NASID_NODEID(nodeid)))

/*
 * Get relative hwpfn to be migrated from
 * the given candidate value.
 */
#define MIGR_CANDIDATE_RELATIVE_HWPFN(value) (MD_MIG_CANDIDATE_HWPFN(value))

/*
 * Get relative swpfn to be migrated from
 * the given candidate value.
 * Due to a bug in the hub, the only pfn
 * we can really obtain is the swpfn.
 */
#define MIGR_CANDIDATE_RELATIVE_SWPFN(value) MIGR_CANDIDATE_RELATIVE_HWPFN(value)

/*
 * Get the destination nodeid from the
 * given candidate value.
 */
#define MIGR_CANDIDATE_INITR(value) ( \
        NASID_TO_COMPACT_NODEID(MD_MIG_CANDIDATE_NODEID(value)))

/*
 * Get the type of intr from the given
 * candidate value.
 */
#define MIGR_CANDIDATE_TYPE(value) (MD_MIG_CANDIDATE_TYPE(value))

#define MIGR_INTR_TYPE_ABS 0
#define MIGR_INTR_TYPE_REL 1

/*
 * Return non-zero if candidate is valid
 */
#define MIGR_CANDIDATE_VALID(value) (MD_MIG_CANDIDATE_VALID(value))

/*
 * Get overrun status for the  migration candidate
 */
#define MIGR_CANDIDATE_OVR(value)  ( \
	((value) & MD_MIG_CANDIDATE_OVERRUN_MASK) >> MD_MIG_CANDIDATE_OVERRUN_SHFT)

/* -------------------------------------------------------------------------
  
   Exported prototypes (to be used in os/numa)
 
   ------------------------------------------------------------------------- */

/*
 * Exported functions for SN0 NUMA architecture
 */

extern void 
migr_unpeg_count(pfn_t       /* Swpfn   */
		 cnodeid_t); /* Node id */

extern void 
migr_effthreshold_diff_set(cnodeid_t,      /* Node  */
		           uint);          /* Effective threshold */

#ifdef OLDOLD
extern void 
migr_threshold_adjust(cnodeid_t,  /* My_node_id */
		      int,        /* Flag       */
		      int);       /* Step       */

extern void 
migr_threshold_diff_set_lock(cnodeid_t, /* Node  */
			     int);      /* Value */

#endif /*OLDOLD*/
extern int 
migr_exit_port_get(cnodeid_t,  /* Source node      */
		   cnodeid_t); /* Destination node */

/* Migration hardware is not supported on Sable */
#if SABLE
#define migr_traffic_update(node)
#else /* !SABLE */
extern void migr_traffic_update(cnodeid_t node);
#endif /* SABLE */



/* 
 * Status of local router -- the router directly connected to 
 * local hub.
 */

/* cached router port utilization data */
typedef struct port_utilization_s {
	ushort free_running;
	ushort send;
	ushort receive;
} port_utilization_t;

typedef struct local_router_state_s {
	/* Need access STATUS_REV_ID register */
	port_no_t my_port_no;  /* Hub's port number on the local router */
	unsigned char port_status[MAX_ROUTER_PORTS]; 
	                       /* What port(s) are active */

	/* Need access six histogram registers */
	port_utilization_t port_utilization[MAX_ROUTER_PORTS];
} local_router_state_t;

#endif /* SN0 */

/* 
 * Exported functions for NUMA architecture
 */
extern void migr_pageref_reset(pfn_t swpfn, pfmsv_t pfmsv);
extern void migr_pageref_disable(pfn_t swpfn);
extern void migr_pageref_reset_abs(pfn_t swpfn, pfmsv_t pfmsv);


/* In the debug mode, we record how many interrupts are good and bad */
#if DEBUG
extern void migr_intr_verify(cnodeid_t, cnodeid_t, pfn_t);
#else 
#define migr_intr_verify(source_nodeid, dest_nodeid, swpfn)
#endif /* DEBUG */

#endif /* __NUMA_HW_H__ */




