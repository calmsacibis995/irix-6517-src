/*
 * os/numa/pm_idbg.c
 *
 *
 * Copyright 1995, 1996 Silicon Graphics, Inc.
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/prctl.h>
#include <sys/idbgentry.h>
#include <sys/pfdat.h>
#include <sys/pmo.h>
#include <sys/var.h>
#include <ksys/pid.h>
#include <ksys/as.h>
#include <os/as/as_private.h>	/* XXX */
#include <sys/nodepda.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "debug_levels.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "pfms.h"
#include "migr_control.h"
#include "repl_control.h"
#include "migr_queue.h"
#include "numa_hw.h"
#include "numa_stats.h"
#include "migr_engine.h"



extern int idbg_pmfind(__uint64_t xpm);
extern int idbg_mldprint(mld_t* mld);
extern int idbg_mldsetprint(mldset_t* mldset);
extern int idbg_mldproc(__psint_t);
extern void idbg_refcnt(pfn_t); 
extern int idbg_mldall(__psint_t p1);
extern int idbg_mldgroup(char* s);
extern void aspm_print(struct aspm* aspm);
extern void idbg_nodepda_numa(__psint_t);
#ifdef NOTYET
extern void idbg_process_with_aspm(__psint_t p1);
extern void idbg_process_with_ns(__psint_t p1);
#endif
        
#ifdef MMCI_REFCD
extern void idbg_refcd_trace_print(__psint_t p1);
extern void idbg_refcd_trace_enable(void);
extern void idbg_refcd_trace_disable(void);
extern void idbg_refcd_trace_clean(void);
extern void idbg_refcd_trace_entry_print(__psint_t p1);
#endif /* MMCI_REFCD */

extern void idbg_refcnt_state_print(__psint_t arg);
extern void idbg_pm_print(pm_t* pm);
extern void idbg_mldchunk_listprint(pm_t* pm);
extern void idbg_checktb(cnodeid_t node);
extern void idbg_resettb(void);
extern void idbg_print_memtick(cnodeid_t nodeid);

/*
 * Local functions
 */

void
numa_idbg_init(void)
{
        idbg_addfunc("pmfind", (void (*)())idbg_pmfind);
        idbg_addfunc("mld", (void (*)())idbg_mldprint);
        idbg_addfunc("mldset", (void (*)())idbg_mldsetprint);
        idbg_addfunc("mldproc", (void (*)())idbg_mldproc);
        idbg_addfunc("refcnt", (void (*)())idbg_refcnt);
        idbg_addfunc("mldall",  (void (*)())idbg_mldall);
        idbg_addfunc("mldgroup",  (void (*)())idbg_mldgroup);
        idbg_addfunc("aspm",  (void (*)())aspm_print);
        idbg_addfunc("nodenuma",  (void (*)())idbg_nodepda_numa);
#ifdef NOTYET
        idbg_addfunc("paspm",  (void (*)())idbg_process_with_aspm);
        idbg_addfunc("pns",  (void (*)())idbg_process_with_ns);        
#endif
                
#ifdef MMCI_REFCD        
        idbg_addfunc("refprint",  (void (*)())idbg_refcd_trace_print);
        idbg_addfunc("refenable",  (void (*)())idbg_refcd_trace_enable);
        idbg_addfunc("refdisable",  (void (*)())idbg_refcd_trace_disable);
        idbg_addfunc("refclean",  (void (*)())idbg_refcd_trace_clean);
        idbg_addfunc("refent",  (void (*)())idbg_refcd_trace_entry_print);        
#endif /* MMCI_REFCD */

        idbg_addfunc("mstat",  (void (*)())idbg_refcnt_state_print);
        idbg_addfunc("pm",  (void (*)())idbg_pm_print);
        idbg_addfunc("mldchunk",  (void (*)())idbg_mldchunk_listprint);
        idbg_addfunc("cktb", (void (*)())idbg_checktb);
        idbg_addfunc("rstb", (void (*)())idbg_resettb);
        idbg_addfunc("mtickpar", (void (*)())idbg_print_memtick);
}


void
aspm_print(struct aspm* aspm)
{
        if (aspm) {
                qprintf(" pmo_ns: 0x%x, pmref: 0x%x, pmhdl: 0x%x\n",
                        aspm->pmo_ns, aspm->pm_default_ref, aspm->pm_default_hdl);
        } else {
                qprintf(" <NULL aspm>\n");
        }
}

void
preg_pm_print(struct pm	*pm)
{
	pmo_base_t      *pmo;
	mld_t		*mld;
	mldset_t	*mldset;
	char		*policy_name;
	int		i;

	if (pm == NULL)
		return;
	pmo = pm->pmo;
	(*pm->plac_srvc)(pm, PM_SRVC_GETNAME, &policy_name);
	if (pmo->type == __PMO_MLD) {
		mld = pm->pmo;
		qprintf("     mld: %s, pagesize %d\n", 
			policy_name, pm->page_size);
		qprintf("          node %d, radius %d, esize: %d\n",
			mld->nodeid, mld->radius, mld->esize);
	} else if (pmo->type == __PMO_MLDSET) {
		mldset = pm->pmo;
		qprintf("     mldset: %s, pagesize %d, topology: 0x%x, rqmode: 0x%x\n", 
			policy_name, pm->page_size, mldset->topology_type, mldset->rqmode);
		for (i = 0; i < pmolist_getclen(mldset->mldlist); i++) {
			mld = (mld_t*)pmolist_getelem(mldset->mldlist, i);
			qprintf("        mld: node %d, radius %d, esize: %d\n",
				mld->nodeid, mld->radius, mld->esize);
		}
	} else {
		qprintf("    pmotype %d ???\n", pmo->type);
	}
}

#include "os/proc/pproc_private.h"	/* XXX bogus */

int
print_procpm(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
        struct pm	*pm;

	if (ctl == 1) {
		pm = arg;
                qprintf("<%d> ", p->p_pid);
		idbg_aspmfind(p, pm);
	}
	return(0);
}

/*
 * Find all the regions currently referencing a pm
 */
int
idbg_pmfind(__uint64_t xpm)
{
        struct pm *pm;

        /*
         *      Go through all active processes
         */
        pm = (struct pm*)xpm;
        qprintf("Searching for references to pm[0x%x]\n", pm);
	idbg_procscan(print_procpm, pm);
        return (0);
	
}

/*
 * Pmo Types
 */

char *pmo_type[] = {
        "PMO_ANY",
        "PMO_MLD",
        "PMO_RAFF",
        "PMO_MLDSET",
        "PMO_PM",
        "PMO_ASPM"
};

void
pmo_base_print(pmo_base_t* pmo_base)
{
        qprintf("pmo_base [0x%x]:\n", pmo_base);
	if (pmo_base) {
        	qprintf("incref 0x%x, decref 0x%x, destroy 0x%x\n",
                	pmo_base->incref, pmo_base->decref, pmo_base->destroy);
        	qprintf("type %s, refcount %d\n",
                	pmo_type[pmo_base->type], pmo_base->refcount);
	}
}

int
idbg_mldprint(mld_t* mld)
{
        qprintf("MLD [0x%x]:\n", mld);
        pmo_base_print(&mld->base);
        if (mld) 
		qprintf("cnodeid %d, radius %d, esize %d flags 0x%x\n",
                	mld->nodeid, mld->radius, mld->esize, mld->flags);

        return (0);
}

int
idbg_mldsetprint(mldset_t* mldset)
{
        int i;
        
        qprintf("MLDSET [0x%x]:\n", mldset);
        pmo_base_print(&mldset->base);

        qprintf("Topology: 0x%x, rqmode: 0x%x\n",
                mldset->topology_type, mldset->rqmode);

        qprintf("mldlist: 0x%x\n", mldset->mldlist);

        for (i = 0; i < pmolist_getclen(mldset->mldlist); i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldset->mldlist, i);
                idbg_mldprint(mld);
        }        

        return (0);
}


/*
 * Dump the content of page frame migration state descriptor
 */
void 
idbg_pfms(pfn_t swpfn)
{
        pfmsv_t pfmsv;

        pfmsv =  PFMS_GETV(PFN_TO_PFMS(swpfn));
        qprintf("  pfmsv[0x%x]: migrt 0x%x, frzt 0x%x, mltt 0x%x, dampenf 0x%x\n",
                pfmsv,
                PFMSV_MGRTHRESHOLD_GET(pfmsv),
                PFMSV_FRZTHRESHOLD_GET(pfmsv),
                PFMSV_MLTTHRESHOLD_GET(pfmsv),
                PFMSV_MIGRDAMPENFACTOR_GET(pfmsv));
        qprintf("  migr-on %d, frz-on %d, mlt-on %d, dampen-on %d, refcnt-on %d, testbit %d\n",
                PFMSV_IS_MIGR_ENABLED(pfmsv)?1:0,
                PFMSV_IS_FREEZE_ENABLED(pfmsv)?1:0,
                PFMSV_IS_MELT_ENABLED(pfmsv)?1:0,
                PFMSV_IS_MIGRDAMPEN_ENABLED(pfmsv)?1:0,
                PFMSV_IS_MIGRREFCNT_ENABLED(pfmsv)?1:0,
                PFMSV_IS_TESTBIT(pfmsv)?1:0);
        qprintf("  migr-cnt %d, frzn %d, dmp-cnt %d, q-fail %d, lcked: %d, st",
                PFMSV_MGRCNT_GET(pfmsv),
                PFMSV_IS_FROZEN(pfmsv)?1:0,
                PFMSV_MIGRDAMPENCNT_GET(pfmsv),
                PFMSV_IS_ENQONFAIL(pfmsv)?1:0,
                PFMSV_IS_LOCKED(pfmsv)?1:0);
        if (PFMSV_STATE_IS_UNMAPPED(pfmsv)) {
                qprintf("  unmapped\n");
        } else if (PFMSV_STATE_IS_MAPPED(pfmsv)) {
                qprintf("  mapped\n");
        } else if (PFMSV_STATE_IS_MIGRED(pfmsv)) {
                qprintf("  migred\n");
        } else if (PFMSV_STATE_IS_QUEUED(pfmsv)) {
                qprintf("  queued\n");
        }
}

/*
 * Check dampen factor is 3 for all pfms's
 */
void
idbg_checktb(cnodeid_t node)
{
        int slot;
        pfn_t pfn;
        int count;
        pfms_t pfms;
        int errcount = 0;
        
        for (slot = 0; slot < node_getnumslots(node); slot++) {
                for (pfn = slot_getbasepfn(node, slot), count = 0;
                     count < slot_getsize(node, slot);
                     pfn++, count++) {
                        if (pfn < pfdattopfn(PFD_LOW(node))) {
                                continue;
                        }
                        pfms = PFN_TO_PFMS(pfn);
                        if (!PFMS_IS_TESTBIT(pfms)) {
                                qprintf("ERROR(%d): node:%d, slot:%d, pfn=0x%x\n", errcount, node, slot, pfn);
                                errcount++;
                        }
                }
        }

}

/*
 * Reset dampen factor for all pfms's
 */
void
idbg_resettb(void)
{
        cnodeid_t node;
        int slot;
        pfn_t pfn;
        int count;
        pfms_t pfms;
        
        for (node = 0; node < numnodes; node++) {
                for (slot = 0; slot < node_getnumslots(node); slot++) {
                        for (pfn = slot_getbasepfn(node, slot), count = 0;
                             count < slot_getsize(node, slot);
                             pfn++, count++) {
                                 pfms = PFN_TO_PFMS(pfn);
                                 PFMS_TESTBIT_CLR(pfms);
                        }
                }
        }

        qprintf("OK\n");
}        


/*
 * mem_tick paramters
 */

void
idbg_print_memtick_node(cnodeid_t node)
{
        int slot;
        int eff_cpus_on_node;
        int max_cpus_on_node;
        int cpu;
        cpuid_t lcpuid;
        pda_t* pda;
        
        /*
         * Get max cpus on node
         */
        max_cpus_on_node = CNODE_NUM_CPUS(node);
        
        /*
         * Determine effective number of cpus available on this node
         */
        eff_cpus_on_node = 0;
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                if (cpu_enabled(lcpuid)) {
                        eff_cpus_on_node++;
                }
        }
        
        qprintf("Distribution of mem_tick work for node [%d] (%d active cpus):\n", node, eff_cpus_on_node);
        qprintf("Memory Configuration for node %d:\n", node);
        qprintf("LOWPFN: 0x%x, HIGHPFN: 0x%x\n",
               pfdattopfn(PFD_LOW(node)),  pfdattopfn(PFD_HIGH(node)));
        for (slot = 0; slot < node_getnumslots(node); slot++) {
                if (slot_getsize(node, slot) > 0) {
                        qprintf("S[%d]: 0x%x->0x%x, ",
                               slot,
                               slot_getbasepfn(node, slot),
                               slot_getbasepfn(node, slot) + slot_getsize(node, slot) -1);
                }
        }
        qprintf("\n");
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        qprintf("    cpu[%d]: startpfn: 0x%x, pfns: %d, pfns per tick for bouncectrl: %d, for unpegging: %d\n",
                               cpu,
                               pda->p_mem_tick_cpu_startpfn,
                               pda->p_mem_tick_cpu_numpfns,
                               pda->p_mem_tick_bounce_numpfns,
                               pda->p_mem_tick_unpegging_numpfns);
                }

        }
         
        qprintf("    Bounce control period: %d [10ms ticks], Unpegging Period: %d [10ms ticks]\n",
               NODEPDA_MCD(node)->migr_system_kparms.migr_bounce_control_interval,
               NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_interval);
}

void
idbg_print_memtick(cnodeid_t nodeid)
{
        cnodeid_t node;
        
        if (nodeid == (cnodeid_t)(-1)) {
                for (node = 0; node < numnodes; node++) {
                        idbg_print_memtick_node(node);
                }
        } else {
                idbg_print_memtick_node(nodeid);
        }
}


/*
 * Sync: not needed for symmon
 * Globals: none
 */
void
nodepda_mcdprint(nodepda_t *npda)
{
        qprintf("Migration Control Parameters, (mcd)@: 0x%x\n", npda->mcd);

        qprintf("\tMigration Per Address Space Default Kernel Parameters:\n");
        qprintf("\tmigration_base_mode: 0x%x\n", npda->mcd->migr_as_kparms.migr_base_mode);
        qprintf("\tmigr_base_threshold: 0x%x\n", npda->mcd->migr_as_kparms.migr_base_threshold);
        qprintf("\tmigr_freeze_enabled: 0x%x\n", npda->mcd->migr_as_kparms.migr_freeze_enabled);
        qprintf("\tmigr_freeze_threshold: 0x%x\n", npda->mcd->migr_as_kparms.migr_freeze_threshold);
        qprintf("\tmigr_melt_enabled: 0x%x\n", npda->mcd->migr_as_kparms.migr_melt_enabled);
        qprintf("\tmigr_melt_threshold: 0x%x\n", npda->mcd->migr_as_kparms.migr_melt_threshold);
        qprintf("\tmigr_enqonfail_enabled: 0x%x\n", npda->mcd->migr_as_kparms.migr_enqonfail_enabled);

        qprintf("\tMigration System Default Kernel Parameters:\n");
        qprintf("\tmemoryd_enabled: 0x%x\n", npda->mcd->migr_system_kparms.memoryd_enabled);
        qprintf("\tmigr_traffic_control_enabled: 0x%x\n", npda->mcd->migr_system_kparms.migr_traffic_control_enabled);
        qprintf("\tmigr_unpegging_control_enabled: 0x%x\n", npda->mcd->migr_system_kparms.migr_unpegging_control_enabled);
        qprintf("\tmigr_auto_migr_mech: 0x%x\n", npda->mcd->migr_system_kparms.migr_auto_migr_mech);
        qprintf("\tmigr_user_migr_mech: 0x%x\n", npda->mcd->migr_system_kparms.migr_user_migr_mech); 
        qprintf("\tmigr_coaldmigr_mech: 0x%x\n", npda->mcd->migr_system_kparms.migr_coaldmigr_mech); 
       
        qprintf("\tmigr_traffic_control_interval: 0x%x\n", npda->mcd->migr_system_kparms.migr_traffic_control_interval);
        qprintf("\tmigr_traffic_control_threshold: 0x%x\n", npda->mcd->migr_system_kparms.migr_traffic_control_threshold);
        qprintf("\tmigr_bounce_control_interval: 0x%x\n", npda->mcd->migr_system_kparms.migr_bounce_control_interval);
        qprintf("\tmigr_unpegging_control_interval: 0x%x\n", npda->mcd->migr_system_kparms.migr_unpegging_control_interval);
        qprintf("\tmigr_unpegging_control_threshold: 0x%x\n", npda->mcd->migr_system_kparms.migr_unpegging_control_threshold);

        qprintf("\tmigr_queue_control_enabled: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_control_enabled);
        qprintf("\tmigr_queue_traffic_trigger_enabled: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_traffic_trigger_enabled);
        qprintf("\tmigr_queue_capacity_trigger_enabled: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_capacity_trigger_enabled);
        qprintf("\tmigr_queue_time_trigger_enabled: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_time_trigger_enabled);        
        qprintf("\tmigr_queue_maxlen: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_maxlen);
        qprintf("\tmigr_queue_control_interval: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_control_interval);        
        qprintf("\tmigr_queue_control_mlimit: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_control_mlimit);
        qprintf("\tmigr_queue_traffic_trigger_threshold: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_traffic_trigger_threshold);     
        qprintf("\tmigr_queue_capacity_trigger_threshold: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_capacity_trigger_threshold);
        qprintf("\tmigr_queue_time_trigger_interval: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_time_trigger_interval);        
        qprintf("\tmigr_queue_control_max_page_age: 0x%x\n", npda->mcd->migr_system_kparms.migr_queue_control_max_page_age);
        qprintf("\tmigr_vehicle: 0x%x\n", npda->mcd->migr_system_kparms.migr_vehicle);

        qprintf("\tMemoryd State:\n");
        qprintf("\ttraffic_period_count: 0x%x\n", npda->mcd->migr_memoryd_state.traffic_period_count);
        qprintf("\tqueue_period_count: 0x%x\n", npda->mcd->migr_memoryd_state.queue_period_count);


#ifdef DEBUG
        qprintf("\tMigration Queue Contents:\n");
        idbg_migr_queue_print(npda->mcd->migr_queue);
#endif /* DEBUG */

        qprintf("DIRECTORY TYPE: %s\n",
                	((npda->mcd->hub_dir_type == DIRTYPE_PREMIUM) ? \
                         ("PREMIUM") : \
                         ("STANDARD")));
        qprintf("Max Counter Threshold: 0x%x\n",
                ((npda->mcd->hub_dir_type == DIRTYPE_PREMIUM) ? \
                 (MIGR_DIFF_THRESHOLD_PREMIUM_MAX) : \
                 (MIGR_DIFF_THRESHOLD_STANDARD_MAX)));

        qprintf("\tNUMA STATS:\n");
        qprintf("traffic_local_router: %d\n", npda->numa_stats->traffic_local_router);
        qprintf("migr_threshold: %d\n", npda->numa_stats->migr_threshold);
        qprintf("refcnt_interrupt_number: %d", 
		npda->numa_stats->refcnt_interrupt_number);
        qprintf("migr_pagemove_number BTE: %d, TLBSYNC: %d\n",
                npda->numa_stats->migr_pagemove_number[MIGR_VEHICLE_IS_BTE],
                npda->numa_stats->migr_pagemove_number[MIGR_VEHICLE_IS_TLBSYNC]);

        qprintf("migr_interrupt_failstate: %d\n", npda->numa_stats->migr_interrupt_failstate);
        qprintf("migr_interrupt_failenabled: %d\n", npda->numa_stats->migr_interrupt_failenabled);
        qprintf("migr_interrupt_failfrozen: %d\n", npda->numa_stats->migr_interrupt_failfrozen);
        
        qprintf("migr_auto_number_in: %d\n", npda->numa_stats->migr_auto_number_in);
        qprintf("migr_auto_number_out: %d\n", npda->numa_stats->migr_auto_number_out);
        qprintf("migr_auto_number_fail: %d\n", npda->numa_stats->migr_auto_number_fail);
        qprintf("migr_auto_number_queued: %d\n", npda->numa_stats->migr_auto_number_queued);
        qprintf("migr_queue_number_overflow: %d\n", npda->numa_stats->migr_queue_number_overflow);
        qprintf("migr_queue_number_failstate: %d\n", npda->numa_stats->migr_queue_number_failstate);
        qprintf("migr_queue_number_failq: %d\n", npda->numa_stats->migr_queue_number_failq);

        qprintf("migr_queue_number_in: %d\n", npda->numa_stats->migr_queue_number_in);
        qprintf("migr_queue_number_out: %d\n", npda->numa_stats->migr_queue_number_out);
        qprintf("migr_queue_number_fail: %d\n", npda->numa_stats->migr_queue_number_fail);

        /*qprintf("xxx: %d\n", npda->numa_stats->xxx); */
        
        qprintf("migr_user_number_in: %d\n", npda->numa_stats->migr_user_number_in);
        qprintf("migr_user_number_out: %d\n", npda->numa_stats->migr_user_number_out);
        qprintf("migr_user_number_queued: %d\n", npda->numa_stats->migr_user_number_queued);
        qprintf("migr_user_number_fail: %d\n", npda->numa_stats->migr_user_number_fail);
        qprintf("migr_coalescingd_number_in: %d\n", npda->numa_stats->migr_coalescingd_number_in);
        qprintf("migr_coalescingd_number_in: %d\n", npda->numa_stats->migr_coalescingd_number_in);
        qprintf("migr_coalescingd_number_fail: %d\n", npda->numa_stats->migr_coalescingd_number_fail);
        qprintf("pagealloc_lent_memory: %d\n", npda->numa_stats->pagealloc_lent_memory);

        qprintf("migr_unpegger_calls: %d\n", npda->numa_stats->migr_unpegger_calls);
        qprintf("migr_unpegger_npages: %d\n", npda->numa_stats->migr_unpegger_npages);
        qprintf("migr_unpegger_lastnpages: %d\n", npda->numa_stats->migr_unpegger_lastnpages);

        qprintf("migr_bouncecontrol_frozen_pages: %d\n", npda->numa_stats->migr_bouncecontrol_frozen_pages);
        qprintf("migr_bouncecontrol_calls: %d\n", npda->numa_stats->migr_bouncecontrol_calls);
        qprintf("migr_bouncecontrol_melt_pages: %d\n", npda->numa_stats->migr_bouncecontrol_melt_pages);
        qprintf("migr_bouncecontrol_last_melt_pages: %d\n", npda->numa_stats->migr_bouncecontrol_last_melt_pages);
        qprintf("migr_bouncecontrol_dampened_pages: %d\n", npda->numa_stats->migr_bouncecontrol_dampened_pages);
        
        qprintf("migr_queue_capacity_trigger_count: %d\n", npda->numa_stats->migr_queue_capacity_trigger_count);
        qprintf("migr_queue_time_trigger_count: %d\n", npda->numa_stats->migr_queue_time_trigger_count);
        qprintf("migr_queue_traffic_trigger_count: %d\n", npda->numa_stats->migr_queue_traffic_trigger_count);
        qprintf("repl_pagecount: %d\n", npda->numa_stats->repl_pagecount);
        qprintf("repl_page_destination: %d\n", npda->numa_stats->repl_page_destination);
        qprintf("repl_page_reuse: %d\n", npda->numa_stats->repl_page_reuse);
        qprintf("repl_control_refuse: %d\n", npda->numa_stats->repl_control_refuse);
        qprintf("repl_page_notavail: %d\n", npda->numa_stats->repl_page_notavail);

        qprintf("memoryd_periodic_activations: %d\n", npda->numa_stats->memoryd_periodic_activations);
        qprintf("memoryd_total_activations: %d\n", npda->numa_stats->memoryd_total_activations);        
        qprintf("migr_pressure_frozen_pages: %d\n", npda->numa_stats->migr_pressure_frozen_pages);

        qprintf("migr_distance_frozen_pages: %d\n", npda->numa_stats->migr_distance_frozen_pages);
        
 
}

void
nodepda_rcdprint(nodepda_t *npda)
{
	qprintf("\tReplication control data (rcd)@: %x\n", npda->rcd);
	qprintf("\tenabled %d highmark %d lowmark %d\n",
		npda->rcd->repl_control_enabled,
		npda->rcd->repl_traffic_highmark,
		npda->rcd->repl_mem_lowmark);
}

int
idbg_mlduthread(uthread_t* ut)
{
        kthread_t *kt = UT_TO_KT(ut);

        qprintf("uthread: 0x%llx, memaff: 0x%llx, mldlink: 0x%llx\n",
                ut, kt->k_maffbv, kt->k_mldlink);

        idbg_mldprint(kt->k_mldlink);

        return 0;
}

int
idbg_mldproc(__psint_t p1)
{
	proc_t *pp;

        uthread_t *ut;

	if (p1 == -1L) {
		pp = curprocp;
		if (pp == NULL) {
			qprintf("no current process\n");
			return -1;
		}
	} else if (p1 < 0L) {
		pp = (proc_t *) p1;
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:0x%x is not a valid proc address\n", pp);
		}
	} else if ((pp = idbg_pid_to_proc(p1)) == NULL) {
		qprintf("%d is not an active pid\n", (pid_t)p1);
		return -1;
	}

        for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		idbg_mlduthread(ut);
	}

        return (0);
}

typedef struct pdesc {
        char* pname;
        pid_t pid;
        uthreadid_t ut_id;
        struct pdesc* next;
} pdesc_t;

pdesc_t* mldplac[MAX_COMPACT_NODES];

/*ARGSUSED*/
static int
proc_mldplac(
	proc_t	*pp,
	void	*arg,
	int	ctl
	)
{
	pdesc_t*    pdesc;
	mld_t*      mld;
        uthread_t*  ut;

	if (ctl == 1) {
                for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {

                        if ((mld = UT_TO_KT(ut)->k_mldlink) == NULL) {
                                qprintf("idbg_mldall: NULL mldlink??\n");
                                return 0;
                        }

                        if (!mld_isplaced(mld)) {
                                qprintf("idbg_mldall: mld not placed??\n");
                                return 0;
                        }

                        pdesc = kern_malloc(sizeof(pdesc_t));
                        if (pdesc == NULL) {
                                qprintf("Can't malloc memory\n");
                                return (-1);
                        }
                        pdesc->pname = pp->p_comm;
                        pdesc->pid = pp->p_pid;
                        pdesc->ut_id = ut->ut_id;
                        pdesc->next = mldplac[mld_getnodeid(mld)];
                        mldplac[mld_getnodeid(mld)] = pdesc;
                }
	}

	return 0;
}


static int
fill_mldplac(void)
{
        int i;

        for (i = 0; i < MAX_COMPACT_NODES; i++)
                mldplac[i] = NULL;

	return idbg_procscan(proc_mldplac, 0);
}

static void
free_mldplac(void)
{
        int i;
        pdesc_t* pdesc;

        for (i = 0; i < numnodes; i++) {
                for (pdesc = mldplac[i]; pdesc != NULL; ) {
                        pdesc_t* victim = pdesc;
                        pdesc = pdesc->next;
                        kern_free(victim);
                }
        }
}

int
idbg_mldall(__psint_t p1)
{
        int i;
        pdesc_t* pdesc;

        if (fill_mldplac() != 0) {
                return (-1);
        }
        
        if (p1 == -1L) {
                /*
                 * Display placement for all processes
                 */
                for (i = 0; i < numnodes; i++) {
                        qprintf("Node[%d]:(freemem %d, assign: %d)",
                                i,
                                NODE_FREEMEM(i),
                                NODEPDA(i)->memfit_assign);
                        for (pdesc = mldplac[i]; pdesc != NULL; pdesc = pdesc->next) {
                                qprintf(" %s(%d ut_id: %d)", pdesc->pname, pdesc->pid, pdesc->ut_id);
                        }
                        qprintf("\n");
                }
        } else {
                cnodeid_t node = (cnodeid_t)p1;
                if (node < 0 || node >= numnodes) {
                        qprintf("Invalid node: %d\n", node);
                        free_mldplac();
                        return (-1);
                } else {
                        qprintf("Node[%d]:(freemem %d, assign: %d)",
                                node,
                                NODE_FREEMEM(node),
                                NODEPDA(node)->memfit_assign);
                        for (pdesc = mldplac[node]; pdesc != NULL; pdesc = pdesc->next) {
                                qprintf(" %s(ut_id: %d)", pdesc->pname, pdesc->ut_id); 
                        }
                        qprintf("\n");
                }
        }

        free_mldplac();
        return (0);
}

int
idbg_mldgroup(char* s)
{
        int i;
        pdesc_t* pdesc;
        
        if (s == 0 || *s == '\0') {
                qprintf("this function needs a string argument identifying processes\n");
                return (-1);
        }

        if (fill_mldplac() != 0) {
                return (-1);
        }
        
        for (i = 0; i < numnodes; i++) {
                qprintf("Node[%d]:(freemem %d, assign: %d)",
                        i,
                        NODE_FREEMEM(i),
                        NODEPDA(i)->memfit_assign);
                for (pdesc = mldplac[i]; pdesc != NULL; pdesc = pdesc->next) {
                        if (strcmp(s, pdesc->pname) == 0) {
                                qprintf(" %s(%d)", pdesc->pname, pdesc->pid);
                        }
                }
                qprintf("\n");
        }

        free_mldplac();
        return (0);        
}

 
/*
 * Print the reference counter readings for one pfn
 */
void
idbg_refcnt(pfn_t swpfn)
{
	cnodeid_t home_nodeid; 
	pfn_t hwpfn;
	int ref;
	int i, j;

	home_nodeid = PFN_TO_CNODEID(swpfn);

	qprintf("Reference counters for pfn 0x%x\n", swpfn);

	for (i = 0; i < numnodes; i++) {
		qprintf("(%3d)", i);
	}
	qprintf("\n");

	hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
	for (i = 0; i < NUM_OF_HW_PAGES_PER_SW_PAGE(); i++) {
		for (j = 0; j < numnodes; j++) {
			ref = MIGR_COUNT_GET_DIRECT(hwpfn, j, home_nodeid);
			qprintf("%5x", ref);
		}
		qprintf("\n");
		hwpfn++;
	}
}

#ifdef NOTYET
static int
idbg_print_proc_with_aspm(
	proc_t	*pp,
	void	*arg,
	int	ctl
	)
{
	vasid_t		vasid;
	aspm_t *	myaspm;
	pas_t *		pas;
	ppas_t *	ppas;

	if (ctl == 1) {
		/* pp -> vasid??? XXX */
		pas = VASID_TO_PAS(vasid);
		ppas = (ppas_t *)vasid.vas_pasid;
		myaspm = pas->pas_aspm;
		if (myaspm == (aspm_t *)arg)
			qprintf("Process: 0x%llx\n", pp);
	}

	return 0;
}


void
idbg_process_with_aspm(__psint_t p1)
{
        aspm_t* aspm = (aspm_t*)p1;

	idbg_procscan(idbg_print_proc_with_aspm, aspm);
}


static int
idbg_print_proc_with_ns(
	proc_t	*pp,
	void	*arg,
	int	ctl
	)
{
	vasid_t		vasid;
	aspm_t *	myaspm;
	pas_t *		pas;
	ppas_t *	ppas;

	if (ctl == 1) {
		/* pp -> vasid??? XXX */
		pas = VASID_TO_PAS(vasid);
		ppas = (ppas_t *)vasid.vas_pasid;
		myaspm = pas->pas_aspm;
		if (myaspm->pmo_ns == (pmo_ns_t *)arg)
			qprintf("Process: 0x%llx\n", pp);
	}

	return 0;
}


void
idbg_process_with_ns(__psint_t p1)
{
        pmo_ns_t* ns = (pmo_ns_t*)p1;

	idbg_procscan(idbg_print_proc_with_ns, ns);
}        
#endif /* NOTYET */

#if MMCI_REFCD

/*
 * Enable refcd tracing
 */
void
idbg_refcd_trace_enable(void)
{
        pmo_base_refcd_trace_activate();
}

/*
 * Disable refcd tracing
 */
void
idbg_refcd_trace_disable(void)
{
        pmo_base_refcd_trace_stop();
}

/*
 * Clean refcd trace buffer
 */
void
idbg_refcd_trace_clean(void)
{
        pmo_base_refcd_trace_clean();
}
                
        
/*
 * Print a rop_info entry
 */
void
idbg_refcd_trace_print_rop(rop_info_t* rop_info)
{
        ASSERT(rop_info);

        qprintf("    rop:0x%016llx, refc: %d, refop: %d, (%s:%d)\n",
                rop_info->rop,
                rop_info->refcount,
                rop_info->refcop,
                rop_info->file,
                rop_info->line);
}
        

/*
 * Print the valid entries in the refcd tracing buffer
 */

void
idbg_refcd_trace_print(__psint_t p1)
{
        pmo_base_t* pmo;
        int i;
        rop_info_t* p;
        int start;

        if (p1 == -1L) {
                start = 0;
        } else {
                start = (int)p1;
        }
        
        qprintf("Refcd trace buffer:\n");

        for (i = start; i < REFCD_TRACE_LEN; i++) {
                
                pmo =  (pmo_base_t*)refcd_trace.record[i].pmo;
                if (pmo == NULL) {
                        continue;
                }
                
                qprintf("%04d: [%s] REFC: %d (0x%llx)\n",
                        i,
                        pmo_type[pmo->type],
                        pmo->refcount,
                        pmo);

                if (pmo->type == __PMO_PM) {
                        continue;
                }
                
                for (p = refcd_trace.record[i].rop_info; p != NULL; p = p->next) {
                        idbg_refcd_trace_print_rop(p);
                }
        }
}


void
idbg_refcd_trace_entry_print(__psint_t p1)
{
        pmo_base_t* pmo_base = (pmo_base_t*)p1;
        pmo_base_t* pmo;
        int i;
        rop_info_t* p;
                
        for (i = 0; i < REFCD_TRACE_LEN; i++) {
                
                pmo =  (pmo_base_t*)refcd_trace.record[i].pmo;
                if (pmo == NULL) {
                        continue;
                }

                if (pmo == pmo_base) {
                        qprintf("%04d: [%s] REFC: %d (0x%llx)\n",
                                i,
                                pmo_type[pmo->type],
                                pmo->refcount,
                                pmo);

                        if (pmo->type == __PMO_PM) {
                                continue;
                        }
                
                        for (p = refcd_trace.record[i].rop_info; p != NULL; p = p->next) {
                                idbg_refcd_trace_print_rop(p);
                        }

                        break;
                }
        }

        if (i == REFCD_TRACE_LEN) {
                qprintf("Not found...\n");
        }
}




#endif /* MMCI_REFCD */                
                
void
idbg_refcnt_node_state_print(cnodeid_t node)
{
        ulong migration_candidate = MIGR_CANDIDATE_GET(node);

        qprintf("node(%d): MC:0x%llx, pfn: 0x%x, dn: %d, type: %d, ovr: %d, valid: %d\n",
                node,
                migration_candidate,
                mkpfn(COMPACT_TO_NASID_NODEID(node), MIGR_CANDIDATE_RELATIVE_SWPFN(migration_candidate)),
                MIGR_CANDIDATE_INITR(migration_candidate),
                MIGR_CANDIDATE_TYPE(migration_candidate),
                MIGR_CANDIDATE_OVR(migration_candidate),
                MIGR_CANDIDATE_VALID(migration_candidate));
}
                

void
idbg_refcnt_state_print(__psint_t arg)
{
        cnodeid_t node;
        if (arg == -1L) {
                for (node = 0; node < numnodes; node++) {
                        idbg_refcnt_node_state_print(node);
                }
        } else {
                idbg_refcnt_node_state_print((cnodeid_t)arg);
        }

}

void
idbg_pm_print(pm_t* pm)
{
        char* policy_name;
        
        qprintf("Policy Module: 0x%llx\n", pm);

        (*pm->plac_srvc)(pm, PM_SRVC_GETNAME, &policy_name);
        qprintf("Placement Policy: %s, plac_data: 0x%llx\n",
                policy_name, pm->plac_data);
	qprintf("Pagesize: %d, policy_flags: 0x%x, timeout %d, pmo: 0x%x\n",
		pm->page_size, pm->policy_flags, pm->page_alloc_wait_timeout, pm->pmo);
}


void
idbg_donode_numa(nodepda_t *npda)
{
	if (!npda)
		return;
	qprintf("Dumping node numa parameters at 0x%x\n", npda);
	qprintf("memoryd ");

#if     NUMA_MIGR_CONTROL
	nodepda_mcdprint(npda);
#endif  /* NUMA_MIGR_CONTROL */

#if     NUMA_REPL_CONTROL
	nodepda_rcdprint(npda);
#endif  /* NUMA_REPL_CONTROL */

	qprintf("\n");
}


void
idbg_nodepda_numa(__psint_t x)
{
	int     i;

	if (x >= 0)
		idbg_donode_numa(NODEPDA(x));
	else if (x == -1) {
		for (i = 0; i < numnodes; i++) {
			idbg_donode_numa(NODEPDA(i));
		}
	} else {
		idbg_donode_numa((nodepda_t *)x);
	}
}
