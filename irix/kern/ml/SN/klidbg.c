/**************************************************************************
 *									  *
 *	 	Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.47 $"

/*
 * This file contains idbg functions for the SN0 platform.  Prototypes
 * are in sn_private.h.
 *
 * If your idbg function calls functions in other code that are only
 * available in DEBUG kernels, your idbg function should also only be included
 * in klidbg_funcs on DEBUG kernels.  This file compiles into klidbg.o
 * which can be linked into DEBUG or non-DEBUG kernels.
 *
 * If you want to share code with regular kernel printing routines, the
 * functions you call should take a print function as a parameter.
 * As an example, idbg_dumphubspool() calls dump_error_spool() which can
 * be called by error code or idbg so the idbg invocation passes qprintf()
 * as an argument.  Other invocations pass printf().
 *
 */


#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/runq.h>
#include <sys/SN/agent.h>
#include <sys/SN/gda.h>
#include <sys/SN/klkernvars.h>
#include <sys/SN/xtalkaddrs.h>
#include <sys/SN/router.h>
#include <sys/SN/vector.h>
#if defined (SN0)
#include "sys/SN/SN0/bte.h"
#endif
#include "sn_private.h"
#include "error_private.h"

void idbg_dump_gda(void);
#ifdef MAPPED_KERNEL
void idbg_dump_kernvars(cnodeid_t);
#endif
void idbg_dumphubspool(cpuid_t);
void idbg_crbx(cnodeid_t);
void idbg_intvec(cnodeid_t);
void idbg_routerstat(router_info_t *);
void idbg_hubstat(hubstat_t *);
void idbg_cpuleds(void);
void idbg_nodeleds(void);
void idbg_modleds(void);
#if defined (DEBUG)
void idbg_dir_dmp(nasid_t, int);
#endif
void idbg_dir_state(__psunsigned_t);
void idbg_dir_addr(__psunsigned_t);
void idbg_decode_xtalk(__psunsigned_t);
void idbg_memconfig(void);
void idbg_layout(void);

#ifdef GROUPINTR_DEBUG
#include <sys/SN/groupintr.h>
void idbg_allocgrp(void);
void idbg_freegrp(void);
void idbg_join(cpuid_t cpu);
void idbg_unjoin(cpuid_t cpu);
void idbg_grpsend(void); 
#endif
void idbg_rstat(net_vec_t);
void idbg_scline(int);
void idbg_scdump(int);
void idbg_picline(int);
void idbg_picdump(int);
void idbg_pdcline(int);
void idbg_pdcdump(int);
void idbg_bteinfo(__psint_t);

void idbg_memcheck           (void);
void idbg_cache_save_info    (void);
void idbg_cache_save         (void);
void idbg_ic_check           (void);
void idbg_dc_check           (void);
void idbg_sc_check           (void);
void idbg_sc_line            (__uint64_t addr);
void idbg_sc_line_save       (__uint64_t addr);


void memcheck                (void);


#define	VD	(void (*)())

static struct klidbg_funcs_s {
	char	*name;
	void	(*func)(int);
	char	*help;
} klidbg_funcs[] = {
    "dumpspool", VD idbg_dumphubspool, "dumpspool CPU: Dump hub error spool",
    "crbx", VD idbg_crbx, "crbx cnode: Dump hub CRBs",
    "intvec", VD idbg_intvec, "intvec cnode: Dump interrupt vector info",
#if defined (DEBUG)
    "dir_dump", VD idbg_dir_dmp, "dir_dump nasid state: Dump directory info",
#endif /* DEBUG */
    "dir_state", VD idbg_dir_state, "dir_state addr : Show directory info",
    "dir_addr", VD idbg_dir_addr, "dir_addr addr : Show directory address",
    "gda", VD idbg_dump_gda, "gda: Show GDA contents",
#ifdef MAPPED_KERNEL
    "kernvars", VD idbg_dump_kernvars, "kernvars cnode: Show kern vars contents",
#endif
    "decode_xtalk", VD idbg_decode_xtalk, "decode_xtalk: Decode a crosstalk address",
    "routerstat", VD idbg_routerstat, "routerstat: Display router status",
    "hubstat", VD idbg_hubstat, "hubstat: Display hub link status",
    "cpuleds", VD idbg_cpuleds, "cpuleds: Put CPU IDs on the LEDs",
    "nodeleds", VD idbg_nodeleds, "nodeleds: Put cnode + nasid on the LEDs",
    "modleds", VD idbg_modleds, "modleds: Put module id on the LEDs",
    "layout", VD idbg_layout, "layout: Display layout information",
    "memconfig", VD idbg_memconfig, "memconfig: Display physical memory configuration",
#ifdef GROUPINTR_DEBUG
    "allocgrp", VD idbg_allocgrp, "",
    "freegrp", VD idbg_freegrp, "",
    "join", VD idbg_join, "",
    "unjoin", VD idbg_unjoin, "",
    "grpsend", VD idbg_grpsend, "",
#endif
    "rstat", VD idbg_rstat, "rstat vector: Display router registers",
    "scache_line", VD idbg_scline, "scache_line: show scache line",
    "scache_dump", VD idbg_scdump, "scache_dump: arg = 0, dump scache. Else dump bad lines.",
    "picache_line", VD idbg_picline, "picache_line: show picache line",
    "picache_dump", VD idbg_picdump, "picache_dump: arg = 0 dump picache. Else dump bad lines",
    "pdcache_line", VD idbg_pdcline, "pdcache_line: show pdcache line",
    "pdcache_dump", VD idbg_pdcdump, "pdcache_dump: arg = 0 dump pdcache. Else dump bad lines",
    "bteinfo", VD idbg_bteinfo, "bteinfo: Dump BTE specific data",

    "memcheck"   ,VD idbg_memcheck   , "memcheck : check all memory",
    "cache_save_info" ,VD idbg_cache_save_info,"cache_save_info : display info",    "cache_save"      ,VD idbg_cache_save     ,"cache_save : save caches",
    "ic_check"    ,VD idbg_ic_check           ,"ic_check : check instruction $",    "dc_check"    ,VD idbg_dc_check           ,"dc_check : check data $",
    "sc_check"    ,VD idbg_sc_check           ,"sc_check : check saved L2$",
    "sc_line"     ,VD idbg_sc_line            ,"sc_line addr : current",
    "sc_line_save",VD idbg_sc_line_save       ,"sc_line_save addr : saved",


    0,		0,	0
};


/*
 * install_functions
 */
void
install_klidbg_functions(void)
{
	struct klidbg_funcs_s *p;

	for (p = klidbg_funcs; p->func; p++)
		idbg_addfunc(p->name, (void (*)())(p->func));

}


/*
 * Initialization routine.
 */
void
klidbginit(void)
{

#ifndef DEBUG
	/* On debug kernels, these are already installed. */
	install_klidbg_functions();
#endif

}

/*
 * Call a function to dump the error spool for a CPU.
 * dump_error_spool() is available on both dbeug and non-debug kernels.
 */
void
idbg_dumphubspool(cpuid_t cpu)
{
	if (cpu == -1)
		cpu = getcpuid();
	if (cputonasid(cpu) == INVALID_NASID) {
		qprintf("dumpspool: Invalid CPU number: %d\n", cpu);
		return ;
	}

	dump_error_spool(cpu, qprintf);
}

/*
 * Call a function to dump the Hub's CRBs
 */
void
idbg_crbx(cnodeid_t cnode)
{
	nasid_t nasid;

	if (cnode == -1)
		cnode = get_compact_nodeid();
	if ((nasid = COMPACT_TO_NASID_NODEID(cnode)) == INVALID_NASID) {
		qprintf("crbx: Invalid compact node: %d\n", cnode);
		return ;
	}

	crbx(nasid, qprintf);
}

/*
 * Call a function to dump a hub's interrupt vectors.
 */
void
idbg_intvec(cnodeid_t cnode)
{
	if (cnode == -1)
		cnode = get_compact_nodeid();

	intr_dumpvec(cnode, qprintf);
}


void
idbg_dir_state(__psunsigned_t addr)
{
	paddr_t paddr;

	if (IS_COMPATK0(addr) || IS_COMPATK1(addr))
	    paddr = COMPAT_TO_PHYS(addr);
	else if (IS_KSEG0(addr) || IS_KSEG1(addr))
	    paddr  = K0_TO_PHYS(addr);
	else paddr = addr;
	show_dir_state(paddr, qprintf);
}


void
idbg_dir_addr(__psunsigned_t addr)
{
	paddr_t paddr;

	if (IS_COMPATK0(addr) || IS_COMPATK1(addr))
	    paddr = COMPAT_TO_PHYS(addr);
	else if (IS_KSEG0(addr) || IS_KSEG1(addr))
	    paddr  = K0_TO_PHYS(addr);
	else paddr = addr;

	qprintf("paddr 0x%x, dir addr 0x%x\n", paddr, BDDIR_ENTRY_LO(paddr));
}


void
idbg_dir_dmp(nasid_t nasid, int in_state)
{

	if (NASID_TO_COMPACT_NODEID(nasid) != INVALID_NASID)
	    check_dir_state(nasid, in_state, qprintf);
}

void
idbg_decode_xtalk(__uint64_t addr)
{
	if (addr & (HX_IO_BIT << HX_ACCTYPE_SHIFT)) {
		/* IO space */
		qprintf("NASID: %d\n", (addr >> HX_NODE_SHIFT) & 0x1ff);
	} else {
		/* Memory space */

	}
}


void
idbg_dump_gda(void)
{
	gda_t *gdap = GDA;
	int i;
	nasid_t nasid;

	qprintf("GDA (0x%x):\n", gdap);
	qprintf(" magic 0x%x (0x%x)  vers 0x%x  promop 0x%x  masterid 0x%x\n",
		gdap->g_magic, GDA_MAGIC, gdap->g_version, gdap->g_promop,
		gdap->g_masterid);
	qprintf(" VDS 0x%x, hooked_norm 0x%x\n", gdap->g_vds,
		gdap->g_hooked_norm);
	qprintf(" hooked_utlb 0x%x, hooked_xtlb 0x%x\n",
		gdap->g_hooked_utlb, gdap->g_hooked_xtlb);
	qprintf(" partition %d  symmax %d  dbstab 0x%x  nametab 0x%x\n",
		gdap->g_partid, gdap->g_symmax, gdap->g_dbstab,
		gdap->g_nametab);
#ifdef MAPPED_KERNEL
	qprintf(" Replication mask ptr 0x%x\n", gdap->g_ktext_repmask);
#endif /* MAPPED_KERNEL */

	for (i = 0; ((nasid = gdap->g_nasidtable[i]) != INVALID_NASID) &&
		    (i < MAX_COMPACT_NODES); i++) {
		qprintf("    cnode %d => nasid %d\n", i, nasid);
	}
}


#ifdef MAPPED_KERNEL
void
idbg_dump_kernvars(cnodeid_t cnode)
{
	kern_vars_t *kvp;

	if (cnode == -1)
		cnode = cnodeid();

	if ((cnode < 0) || (cnode > maxnodes)) {
		qprintf("Bad node number: %d\n", cnode);
		return;
	}

	kvp = (kern_vars_t *)KERN_VARS_ADDR(COMPACT_TO_NASID_NODEID(cnode));

	qprintf("Node %d kern vars (0x%x):\n", cnode, kvp);
	qprintf(" magic 0x%x (0x%x)\n",
		kvp->kv_magic, KV_MAGIC);
	qprintf(" Read-only NASID %d, read-write NASID %d\n",
		kvp->kv_ro_nasid, kvp->kv_rw_nasid);
	qprintf(" Read-only base 0x%x, read-write base 0x%x\n",
		kvp->kv_ro_baseaddr, kvp->kv_rw_baseaddr);
}
#endif /* MAPPED_KERNEL */

static char *util_names[RP_NUM_UTILS] = {
	"Bypass",
	"Receive",
	"Send"
};

void
idbg_routerstat(router_info_t *rip)
{
	router_port_info_t *rpp;
	int port;
	int bucket;
	char slotname[SLOTNUM_MAXLENGTH];

	qprintf("Router structure 0x%x: %s\n", rip, rip->ri_name);

	get_slotname(rip->ri_slotnum, slotname);
	qprintf("  module %d, slot %s, brd 0x%x\n", rip->ri_module, slotname,
			rip->ri_brd);
	qprintf("  cnode %d, vector 0x%x, portmask 0x%x, leds 0x%x\n",
		rip->ri_cnode, rip->ri_vector, rip->ri_portmask, rip->ri_leds);
	qprintf("  stat_rev_id 0x%x, prot_conf 0x%x, writeid %d\n",
		rip->ri_stat_rev_id, rip->ri_prot_conf, rip->ri_writeid);
	qprintf("  timebase %d  timestamp %d",
		rip->ri_timebase, rip->ri_timestamp);
#ifdef DEBUG
	qprintf("  delta %d\n", rip->ri_deltatime);
#endif
	qprintf("\n");
	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		rpp = &rip->ri_port[port - 1];
		qprintf("Port %d\n", port);
		qprintf("  histograms 0x%x, port error 0x%x\n    ",
			rpp->rp_histograms, rpp->rp_port_error);
		for (bucket = 0; bucket < 4; bucket++) {
			qprintf("Bucket[%d] %d, ", bucket,
				RHIST_GET_BUCKET(bucket, rpp->rp_histograms));
		}
		qprintf("\n  ");
		for (bucket = 0; bucket < RP_NUM_UTILS; bucket++)
			qprintf("  %s util %d/%d", util_names[bucket],
				rpp->rp_util[bucket], RR_UTIL_SCALE);
		qprintf("\n    retries %d, sn errors %d, cb errors %d, overflows %d\n",
			rpp->rp_retry_errors, rpp->rp_sn_errors,
			rpp->rp_cb_errors, rpp->rp_overflows);
	}
}


void
idbg_hubstat(hubstat_t *hsp)
{
	qprintf("Hubstat structure 0x%x: %s\n", hsp, hsp->hs_name);
	qprintf("  version %d, cnode %d, nasid %d\n",
		hsp->hs_version, hsp->hs_cnode, hsp->hs_nasid);
	qprintf("  stat_rev_id 0x%x\n",
		hsp->hs_ni_stat_rev_id);
	qprintf("  timebase %d  timestamp %d\n",
		hsp->hs_timebase, hsp->hs_timestamp);
	qprintf("\n NI retries %d, sn errors %d, cb errors %d, overflows %d\n",
			hsp->hs_ni_retry_errors,
			hsp->hs_ni_sn_errors,
			hsp->hs_ni_cb_errors,
			hsp->hs_ni_overflows);
	qprintf("\n II sn errors %d, cb errors %d, overflows %d\n",
			hsp->hs_ii_sn_errors,
			hsp->hs_ii_cb_errors,
			hsp->hs_ii_overflows);
}


#if 0	/* XXX- not currently used. */
/* unifdef this if you need it;
 * remove it if you know it will
 * never again be needed. Thanks!
 */
static void
klidbg_delay(int usec)
{
	hubreg_t start;

	start = GET_LOCAL_RTC;

	while (GET_LOCAL_RTC < start + usec)
		;
}
#endif

#ifdef GROUPINTR_DEBUG

intrgroup_t* group;

void
idbg_allocgrp()
{
	group = intrgroup_alloc();
	qprintf("Allocated 0x%x\n", group);
}

void
idbg_freegrp()
{
	qprintf("Freeing 0x%x\n", group);
	intrgroup_free(group);
}

void
idbg_join(cpuid_t cpu)
{
	qprintf("CPU %d joining group 0x%x\n", cpu, group);
	intrgroup_join(group, cpu);	
}

void
idbg_unjoin(cpuid_t cpu)
{
	qprintf("CPU %d unjoining group 0x%x\n", cpu, group);
	intrgroup_unjoin(group, cpu);
}

void
idbg_grpsend()
{
	qprintf("Sending interrupt to group 0x%x\n", group);
	sendgroupintr(group);
}

#endif /* GROUPINTR_DEBUG */

void
idbg_cpuleds(void)
{
	int i, j;

	for (i = 0; i < maxnodes; i++) 
		for (j = 0; j < CPUS_PER_NODE; j++)
		    SET_CPU_LEDS(COMPACT_TO_NASID_NODEID(i), j, 0xf);
	    
	for (i = 0; i < maxcpus; i++)
		if (cpu_enabled(i))
			SET_CPU_LEDS(cputonasid(i), cputoslice(i), i);
}

void
idbg_nodeleds(void)
{
	int i;

	qprintf("Cnode on left, NASID on right.\n");

	for (i = 0; i < maxnodes; i++) {
		nasid_t nasid = COMPACT_TO_NASID_NODEID(i);
		if (private.p_sn00) {
			REMOTE_HUB_S(nasid, MD_UREG1_0, i);
			REMOTE_HUB_S(nasid, MD_UREG1_4, nasid);
		} else {
			REMOTE_HUB_S(nasid, MD_LED0, i);
			REMOTE_HUB_S(nasid, MD_LED1, nasid);
		}
	}
}

void
idbg_modleds(void)
{
	int i;

	qprintf("MSB on left, LSB on right.\n");

	for (i = 0; i < maxnodes; i++) {
		nasid_t nasid = COMPACT_TO_NASID_NODEID(i);
		if (private.p_sn00) {
			REMOTE_HUB_S(nasid, MD_UREG1_0,
				     NODEPDA(i)->module_id >> 8);
			REMOTE_HUB_S(nasid, MD_UREG1_4,
				     NODEPDA(i)->module_id & 0xff);
		} else {
			REMOTE_HUB_S(nasid, MD_LED0,
				     NODEPDA(i)->module_id >> 8);
			REMOTE_HUB_S(nasid, MD_LED1,
				     NODEPDA(i)->module_id & 0xff);
		}
	}
}

void
idbg_layout(void)
{
	int i;
	char slotname[SLOTNUM_MAXLENGTH];

	for (i = 0; i < maxcpus; i++) {
		cnodeid_t cnode;

		if (!cpu_enabled(i))
			continue;

		cnode  = cputocnode(i);
		get_slotname(NODEPDA(cnode)->slotdesc, slotname);
		qprintf(" CPU %d: Cnode %d, NASID %d, Module %d, Slot %s\n",
			i, cnode, COMPACT_TO_NASID_NODEID(cnode),
			NODEPDA(cnode)->module_id, slotname);
	}
}

void
idbg_memconfig()
{
	int slot;
	cnodeid_t cnode;

	for (cnode = 0; cnode < numnodes; cnode++) {
		qprintf("Memory on Node %d (Nasid %d)\n", cnode,
				COMPACT_TO_NASID_NODEID(cnode));
		for (slot = 0; slot < node_getnumslots(cnode); slot++) {
			paddr_t	addr, size;
			size  = ctob(slot_getsize(cnode, slot));
			if (size == 0)
				continue;
			addr = ctob(slot_getbasepfn(cnode, slot));
			qprintf("\tslot %d start 0x%lx end 0x%lx\n",
					slot, addr, addr+size);
		}
	}
}


void idbg_rstat(net_vec_t vec)
{
    router_reg_t		reg, hist;
    struct rr_status_error_fmt	rserr;
    int				i, r;

    if ((r = vector_read(vec, 0, RR_STATUS_REV_ID, &reg)) < 0) {
    neterr:
	qprintf("Network vector read error: %d\n", r);
	return;
    }

    if (((reg & NSRI_CHIPID_MASK) >> NSRI_CHIPID_SHFT) != CHIPID_ROUTER) {
	qprintf("Destination must be a router.\n");
	return;
    }

    qprintf("Router local status: 0x%lx\n",
	   (reg & RSRI_LOCAL_MASK) >> RSRI_LOCAL_SHFT);

    qprintf("llp field values: 1:Link Died, 2: Link alive, 5:Link never came up\n");
    qprintf("Port llp Oflw bdDir dedTO tailTO retry cb_err sn_err  "
	   "Bkt3  Bkt2  Bkt1  Bkt0\n");

    for (i = 1; i <= 6; i++) {
	if ((r = vector_read(vec, 0,
			      RR_ERROR_CLEAR(i), (__uint64_t *) &rserr)) < 0)
	    goto neterr;

	if ((r = vector_read(vec, 0,
			      RR_HISTOGRAM(i), &hist)) < 0)
	    goto neterr;

	qprintf("%-3d   %1x   %1d    %1d     %1x     %1x   %4d   %4d   %4d    "
	       "%4x  %4x  %4x  %4x\n",
	       i,
	       (reg&RSRI_LSTAT_MASK(i))>>RSRI_LSTAT_SHFT(i),
	       rserr.rserr_fifooverflow,
	       rserr.rserr_illegalport,
	       rserr.rserr_deadlockto,
	       rserr.rserr_recvtailto,
	       rserr.rserr_retrycnt,
	       rserr.rserr_cberrcnt,
	       rserr.rserr_snerrcnt,
	       hist >> RHIST_BUCKET_SHFT(3) & 0xffff,
	       hist >> RHIST_BUCKET_SHFT(2) & 0xffff,
	       hist >> RHIST_BUCKET_SHFT(1) & 0xffff,
	       hist >> RHIST_BUCKET_SHFT(0) & 0xffff);

	if ((r = vector_write(vec, 0,
			       RR_HISTOGRAM(i), 0ULL)) < 0)
	    goto neterr;
    }
}

void
idbg_scline(int idx)
{
	int i;
	for (i = 0; i < SCACHE_WAYS; i++)
	    ecc_scache_line_dump(idx, i, qprintf);
}



void
idbg_scdump(int check)
{
	ecc_scache_dump(qprintf, check);
}


void
idbg_picline(int idx)
{
	int i;
	for (i = 0; i < PCACHE_WAYS; i++)
	    ecc_picache_line_dump(idx, i, qprintf);
}



void
idbg_picdump(int check)
{
	ecc_picache_dump(qprintf, check);
}

void
idbg_pdcline(int idx)
{
	int i;
	for (i = 0; i < PCACHE_WAYS; i++)
	    ecc_pdcache_line_dump(idx, i, qprintf);
}



void
idbg_pdcdump(int check)
{
	ecc_pdcache_dump(qprintf, check);
}

void
dump_bteinfo(void *info)
{
	bteinfo_t	*bteinfo;

	bteinfo = (bteinfo_t *) info;

	qprintf("bteinfo @0x%x Cpu %d cnode %d %s %s %s xfer %d\n",
			bteinfo, bteinfo->bte_cpuid, bteinfo->bte_cnode,
			(bteinfo->bte_enabled ? "enabled" : "disabled"),
			(bteinfo->bte_thread_pinned ? "pinned" : "unpinned"),
			(ismrlocked(&bteinfo->bte_lock,0)? "locked": "free"),
			bteinfo->bte_xfer_count);
	qprintf("  base 0x%x zpage 0x%x retry_stat 0x%x ctxt 0x%x\n", 
		bteinfo->bte_base, bteinfo->bte_zpage, 
		bteinfo->bte_retry_stat, K0_TO_PHYS(bteinfo->bte_context));
}

void
idbg_bteinfo(__psint_t arg)
{
	cpuid_t 	cpu;
	if (arg == -1L){
		for (cpu = 0; cpu < maxcpus; cpu++) {
			if (pdaindr[cpu].CpuId == -1)
				continue;
			dump_bteinfo(pdaindr[cpu].pda->p_bte_info);
		}
		return;
	}

	if ((arg > maxcpus) || (pdaindr[cpu].CpuId == -1)) {
		qprintf("bteinfo: %d invalid cpu \n", maxcpus);
		return;
	}

	dump_bteinfo(pdaindr[arg].pda->p_bte_info);
	
}



void idbg_memcheck()
{
        memcheck();
}


void memcheck()
{
        int slot;
        cnodeid_t cnode;

        for (cnode = 0; cnode < numnodes; cnode++) {
                qprintf("Node %d (Nasid %d)\n", cnode,
                                COMPACT_TO_NASID_NODEID(cnode));
                for (slot = 0; slot < node_getnumslots(cnode); slot++) {
                        paddr_t p, addr, end, size;
                        size  = ctob(slot_getsize(cnode, slot));
                        if (size == 0)
                                continue;
                        addr = TO_UNCAC(ctob(slot_getbasepfn(cnode,slot)));
			end  = addr + size;
                        qprintf("  slot %2d start 0x%lx end 0x%x\n",
                                        slot, addr, end);

        		for (p = addr ; p < end ; p += 8)
                		*(volatile int *)(p);
                }
        }
}


/*
 *    cache debugging
 */

r10k_cache_debug_t r10k_cachdbg[MAXCPUS];

extern int r10k_cache_debug;		/* mtune/kernel */

int           cache_save_state		(char *,__uint32_t,int);
void          chk_icache_line   	(r10k_il_t *,int *,int *);
void          chk_dcache_line   	(r10k_dl_t *,int *,int *);
void          chk_scache_line   	(r10k_sl_t *,int *,int *);
void   	      scache_print_line 	(r10k_sl_t *,int,int);
unsigned char scache_tag_ecc_check	(r10k_sl_t *sl);
uint          ecc_compute_scdata_ecc    (__uint64_t,__uint64_t);
int           ecc_scache_bits_in_error  (__uint32_t);
uint          ecc_compute_sctag_ecc     (__uint64_t);
void  	      icache_save             	(int);
void          dcache_save             	(int);
void          scache_save             	(int);
void  	      icachecheck               (int);
void  	      dcachecheck               (int);
void  	      scachecheck               (int);



void 
idbg_cache_save_info(void)
{
	int  cpu = getcpuid();

	qprintf("Cache Save Buffer Allocation on CPU %d\n",cpu);

	qprintf("r10k_il_t        = 0x%x\n",r10k_cachdbg[cpu].il);
	qprintf("r10k_dl_t        = 0x%x\n",r10k_cachdbg[cpu].dl);
	qprintf("r10k_sl_t        = 0x%x\n",r10k_cachdbg[cpu].sl);
	qprintf("iCacheMem        = 0x%x\n",r10k_cachdbg[cpu].iCacheMem);
	qprintf("dCacheMem        = 0x%x\n",r10k_cachdbg[cpu].dCacheMem);
	qprintf("sCacheMem        = 0x%x\n",r10k_cachdbg[cpu].sCacheMem);
}

void
idbg_cache_save()
{
	int cpu = getcpuid();
	
	if (r10k_cachdbg[cpu].il == NULL ||
	    r10k_cachdbg[cpu].dl == NULL ||
            r10k_cachdbg[cpu].sl == NULL) {
		qprintf("cache buffer not allocated on CPU %d\n",cpu);
		return;
	}
 
	icache_save(cpu);
	dcache_save(cpu);
	scache_save(cpu);
}

void idbg_dc_check()
{
	dcachecheck(getcpuid());
}

void idbg_ic_check()
{
	icachecheck(getcpuid());
}

void
idbg_sc_check()
{
	scachecheck(getcpuid());
}


void
idbg_sc_line_save(__uint64_t addr)
{
	int         cpu = getcpuid();
	r10k_sl_t  *sl;
	__uint64_t  line;
	int idx;

	if (cache_save_state("L2$",r10k_cachdbg[cpu].sl_stat,cpu) < 0)
		return;

	line = (addr & (sCacheSize() -1)) / (CACHE_SLINE_SIZE * 2);
	idx  = addr & ((private.p_scachesize >> 1) - 1);
	
	qprintf("line = %d %x\n",line,line);
	qprintf("idx  = %d %x\n",idx ,idx );

	sl = &r10k_cachdbg[cpu].sl[line];

	scache_print_line(sl,addr,0);
	sl++;
	scache_print_line(sl,addr,1);
}


void
idbg_sc_line(__uint64_t addr)
{
	r10k_sl_t sl;

	addr |= K0BASE;

	sLine(addr    ,&sl);
	scache_print_line(&sl,0,0);
	sLine(addr | 1,&sl);
	scache_print_line(&sl,0,1);
}


void 
print_cacherr(r10k_cacherr_t cerr)
{
	switch(cerr.error_type.err) {
	case R10K_CACHERR_PIC :
		qprintf("Primary Instruction Cache Error\n");
		qprintf("EW:%x D:%x TA:%x TS:%x PIdx:0x%x\n",
			cerr.pic.ew,
			cerr.pic.data_array_err,
			cerr.pic.tag_addr_err,
			cerr.pic.tag_state_err,
			cerr.pic.pidx);
		break;

	case R10K_CACHERR_PDC :
		qprintf("Primary Data Cache Error\n");
		qprintf("EW:%x EE:%x D:%x TA:%x TS:%x TM:%x PIdx:0x%x\n",
			cerr.pdc.ew,
			cerr.pdc.ee,
			cerr.pdc.data_array_err,
			cerr.pdc.tag_addr_err,
			cerr.pdc.tag_state_err,
			cerr.pdc.tag_mod_array_err,
			cerr.pdc.pidx);
		break;

	case R10K_CACHERR_SC  :
		qprintf("Secondary Cache Error\n");
		qprintf("EW:%x D:%x TA:%x SIdx:0x%x\n",
			cerr.sc.ew,
			cerr.sc.data_array_err,
			cerr.sc.tag_array_err,
			cerr.sc.sidx);
			
		break;

	default :
		qprintf("invalid cache error\n");
		break;
	}
}



/*
 * returns the number of bytes reserved for cache debugging 
 */
__uint64_t 
cache_debug_space(int scache_size)
{
        __uint64_t val;

        val  = (R10K_ICACHESIZE / CACHE_ILINE_SIZE) * sizeof (r10k_il_t);
        val += (R10K_DCACHESIZE / CACHE_DLINE_SIZE) * sizeof (r10k_dl_t);
        val += (scache_size     / CACHE_SLINE_SIZE) * sizeof (r10k_sl_t);

#if 0
	/*
	 * we could save the memory contents as well when the error
	 * occurs, so we really would have all the information in a
	 * system dump. It takes a lot of space and it's not really
	 * clear if we need this
	 */

        if (r10k_cache_debug >= 2) 
                val += (R10K_ICACHESIZE + 
                        R10K_DCACHESIZE + 
                        scache_size    );
#endif

        return val;
}


/*
 * allocate space for cache debug buffers. First try local node, if this
 * fails try remote node. 
 */
__uint64_t
cache_debug_alloc(int node,__uint64_t size)
{
	int     flags;
	pgno_t  p;

	flags = VM_NOSLEEP | VM_DIRECT;
	size  = btoc(size);

	if ((p = contmemall_node(node,size,1,flags)) == NULL) 
		if ((p = contmemall(size,1, flags)) == NULL)
			return NULL;

	ASSERT_ALWAYS(p);

	p = PHYS_TO_K0(ctob(p));

	bzero((void *) p,ctob(size));

	return p;
}



/*
 * 
 */
void cache_debug_init(void)
{
	cnodeid_t    cnode;
	cpuid_t      cpu;
	cpu_cookie_t cookie;
	int          i;
	__uint64_t   addr;
	__uint64_t   size;	
	pda_t       *pda;

        extern cpu_cookie_t setnoderun(cnodeid_t);

	if (! r10k_cache_debug)
                return ;


	bzero(r10k_cachdbg,MAXCPUS * sizeof (r10k_cache_debug_t));

	for (i = 0 ; i < MAXCPUS ; i++)	{
		r10k_cachdbg[i].il_stat = R10K_CACHSAVE_NO;
		r10k_cachdbg[i].dl_stat = R10K_CACHSAVE_NO;
		r10k_cachdbg[i].sl_stat = R10K_CACHSAVE_NO;
	}

 	for (cnode = 0; cnode < numnodes; cnode++) {
		for (i = 0; i < CPUS_PER_NODE; i++) {

			if ((cpu = cnode_slice_to_cpuid(cnode, i)) == CPU_NONE)
				continue;

			/*
	 		 * run on this CPU to read and save the 
			 * processor ID and configuration register
			 */
			cookie = setmustrun(cpu);
			r10k_cachdbg[cpu].cpu_conf.val   = get_cpu_cnf();
			r10k_cachdbg[cpu].cpu_ri.ri_uint = get_cpu_irr(); 
			restoremustrun(cookie);

			pda = pdaindr[cpu].pda;

			ASSERT_ALWAYS(cpu   == pda->p_cpuid);
			ASSERT_ALWAYS(cnode == pda->p_nodeid);

			size  = cache_debug_space(pda->p_scachesize);
			addr  = TO_UNCAC(cache_debug_alloc(cnode,size));

			qprintf("cache debug CPU %d @ 0x%x size %d\n",
						cpu,addr,size);

			r10k_cachdbg[cpu].il = (r10k_il_t *) addr;
			addr += ((R10K_ICACHESIZE / CACHE_ILINE_SIZE)
							* sizeof (r10k_il_t));

			r10k_cachdbg[cpu].dl = (r10k_dl_t *) addr;
			addr += ((R10K_DCACHESIZE / CACHE_DLINE_SIZE)
							* sizeof (r10k_dl_t));

			r10k_cachdbg[cpu].sl_size = pda->p_scachesize;
			r10k_cachdbg[cpu].sl = (r10k_sl_t *) addr;
			addr += ((pda->p_scachesize / CACHE_SLINE_SIZE)
							* sizeof (r10k_sl_t));

			if (r10k_cache_debug > 1) {
				r10k_cachdbg[cpu].iCacheMem=(__uint32_t *)addr;
				addr += R10K_ICACHESIZE;
				r10k_cachdbg[cpu].dCacheMem=(__uint32_t *)addr;
				addr += R10K_DCACHESIZE;
 				r10k_cachdbg[cpu].sCacheMem=(__uint32_t *)addr;
				addr += pda->p_scachesize;
			}	
		}
	}
}
	


void icachecheck(int cpu)
{
	int tag_err  = 0;
	int data_err = 0;
	int i;
	r10k_il_t *il = r10k_cachdbg[cpu].il;

	if (cache_save_state("I$",r10k_cachdbg[cpu].il_stat,cpu) < 0)
		return;
	else
		qprintf("checking saved I$ on CPU %d\n",cpu);

	for (i = 0 ; i < R10K_ICACHELINES ; i++ , il++)
		chk_icache_line(il,&tag_err,&data_err);

	if (tag_err || data_err) 
		qprintf("%d tag errors, %d data errors\n",tag_err,data_err);
	else
		qprintf("no errors found\n");
	
}

void dcachecheck(int cpu)
{
	int tag_err  = 0;
	int data_err = 0;
	int i;
	r10k_dl_t *dl = r10k_cachdbg[cpu].dl;

	if (cache_save_state("D$",r10k_cachdbg[cpu].dl_stat,cpu) < 0)
		return;
	else
		qprintf("checking saved D$ on CPU %d\n",cpu);

	for (i = 0 ; i < R10K_DCACHELINES ; i++ , dl++) 
		chk_dcache_line(dl,&tag_err,&data_err);  

	if (tag_err || data_err) 
		qprintf("%d tag errors, %d data errors\n",tag_err,data_err);
	else
		qprintf("no errors found\n");
}


void scachecheck(int cpu)
{
	int tag_err;
	int data_err;
	int tag_err_total  = 0;
	int data_err_total = 0;
	int i;
	r10k_sl_t *sl = r10k_cachdbg[cpu].sl;

	if (cache_save_state("L2$",r10k_cachdbg[cpu].sl_stat,cpu) < 0)
		return;
	else
		qprintf("checking saved L2$ on CPU %d\n",cpu);

	for (i = 0 ; i < R10K_SCACHELINES ; i++ , sl++) {
		tag_err = data_err = 0;
		chk_scache_line(sl,&tag_err,&data_err);  

		if (tag_err || data_err) {
			scache_print_line(sl,i / 2,i % 2);

			tag_err_total  += tag_err;
			data_err_total += data_err;
		}	
	}

	qprintf("checked %d (0x%x) secondary cache lines\n",i,i);

	if (tag_err_total || data_err_total) 
		qprintf("%d tag errors, %d data errors found\n",
			tag_err_total,data_err_total);
	else
		qprintf("no errors found\n");
}



int cache_save_state(char *name,__uint32_t state,int cpu)
{
	int ret;

	switch (state) {
	case R10K_CACHSAVE_NO :
		qprintf("%s not saved on CPU %d\n",name,cpu);
		ret = -1;
		break;
	case R10K_CACHSAVE_PART :
		qprintf("WARNING : %s only partly saved on CPU %d\n",name,cpu);
		ret = 0;
		break;
	case R10K_CACHSAVE_OK :
		ret = 0;
		break;
	default :
		qprintf("ERROR : invalid state for saved %s on CPU %d\n",
								name,cpu);
		ret = -1;
	}

	return ret;
}



void chk_icache_line(r10k_il_t *il,int *tag_err,int *data_err)
{
	int i;

	if ((ecc_bitcount(il->il_tag.t.tag) & 0x1) != il->il_tag.t.tagpar) {
		qprintf("I$ tag parity error\n");
		(*tag_err)++;
	}

	if (il->il_tag.t.state !=  il->il_tag.t.statepar) {
		qprintf("I$ tag state parity error\n");
		(*tag_err)++;
	}

	for (i = 0; i < IL_ENTRIES; i++) {
		if ((ecc_bitcount(il->il_data[i]) & 0x1) != il->il_parity[i]) {
			qprintf("i=%d, I$ data error, %x\n",i,il->il_data[i]);
			(*data_err)++;
		}
	}

}

void chk_dcache_line(r10k_dl_t *dl,int *tag_err,int *data_err)
{
	int           i,j;
	unsigned char state_par;
        unsigned char databyte;
       	unsigned char paritybit;

	if ((ecc_bitcount(dl->dl_tag.t.tag) & 0x1) != dl->dl_tag.t.tagpar) {
		qprintf("D$ tag parity error\n");
		(*tag_err)++;
	}

	state_par = ecc_bitcount(
			(dl->dl_tag.t.state << 1) | dl->dl_tag.t.scway) & 0x01;

	if (state_par != dl->dl_tag.t.statepar) {
		qprintf(
		"D$ tag state parity error state=%2x scway=%1x statepar=%1x\n",
			dl->dl_tag.t.state,
			dl->dl_tag.t.scway,
			dl->dl_tag.t.statepar);
		(*tag_err)++;
	}

	if (ecc_bitcount(dl->dl_tag.t.statemod) > 1) {
		qprintf("D$ Tag state modifier error %x\n",
			dl->dl_tag.t.statemod);
		(*tag_err)++;
	}	

	for (i = 0; i < DL_ENTRIES; i++) {
        	for (j = 0 ; j < 4 ; j++) {
                	paritybit = (dl->dl_parity[i] >> j)       & 0x1;
                	databyte  = (dl->dl_data  [i] >> (j * 8)) & 0xff;

                	if ((ecc_bitcount(databyte)&0x1) != (paritybit&0x1)) {
				qprintf("D$ parity error, (%d/%d) 0x%x\n",
					i,j,databyte);
				(*data_err)++;
			}
		}
	}
}


void chk_scache_line(r10k_sl_t *sl,int *tag_err,int *data_err)
{
	int        i,j;
	__uint16_t    ecc_rcv,ecc_exp;
	unsigned char par_rcv,par_exp;

	if (scache_tag_ecc_check(sl) != (unsigned char ) sl->sl_tag.t.ecc) 
	 	(*tag_err)++;

	for (i = j = 0 ; j < SL_ENTRIES  ; i += 2 , j++)  {
		ecc_rcv = sl->sl_ecc[j].v;
		ecc_exp = ecc_compute_scdata_ecc(sl->sl_data[i],
						 sl->sl_data[i + 1]);
		par_rcv = sl->sl_ecc[j].t.parity;
		par_exp = ecc_bitcount(sl->sl_data[i] ^
                                       sl->sl_data[i + 1]) & 0x1;

                if (ecc_rcv != ecc_exp || par_rcv != par_exp)
			(*data_err)++;
	
	}
	
}


#define	FMT1 "Tag %6x (%16x) MRU %1x state %2x virtind %2x  ECC rcv %2x exp %2x %s\n"
#define FMT2 "Off ---------     Data     ----------  ECC rcv exp syn BitErr Par\n"
#if 0
#define FMT3 "%2x  %16x %16x      %3x %3x %3x     %1x   %1x %s%s%s\n"
#endif
#define FMT3 "%2x  %16x %16x      %3x %3x     %1x   %1x %s%s%s\n"

void
scache_print_line(r10k_sl_t *sl,int set_index,int set_way)
{
	int i,j;

	unsigned char tagecc;
	unsigned char exp_par;
	__uint16_t    exp_ecc;

	tagecc  = scache_tag_ecc_check(sl);

	qprintf("Set Index 0x%x  Way %d\n",
			set_index,
			set_way);

	qprintf(FMT1,
		sl->sl_tag.t.tag,
		(sl->sl_tag.t.tag << 18) | K0BASE,
		sl->sl_tag.t.mru,
		sl->sl_tag.t.state,
		sl->sl_tag.t.virtind,
		sl->sl_tag.t.ecc,
		tagecc,
		sl->sl_tag.t.ecc != tagecc ?
		"ECC ERROR" : "");

	qprintf(FMT2);

        for (i = j = 0 ; i < (SL_ENTRIES * 2) ; i += 2 , j++) {

		exp_ecc = ecc_compute_scdata_ecc(sl->sl_data[i],
						 sl->sl_data[i + 1]);


		exp_par = ecc_bitcount(sl->sl_data[i] ^ 
			 	       sl->sl_data[i + 1]) & 0x1;

		qprintf(FMT3,
			i * 8,
			sl->sl_data[i],
			sl->sl_data[i + 1],
			sl->sl_ecc[j].v,
                        exp_ecc,
#if 0
			exp_ecc ^ sl->sl_ecc[j].t.ecc,
#endif
                        ecc_scache_bits_in_error(exp_ecc ^ sl->sl_ecc[j].t.ecc),
			sl->sl_ecc[j].t.parity,
			sl->sl_ecc[j].v != exp_ecc 	 || 
			sl->sl_ecc[j].t.parity != exp_par ?
				"BAD" : "" ,
			sl->sl_ecc[j].v != exp_ecc ?
				" ECC" : "" ,
			sl->sl_ecc[j].t.parity != exp_par ?
				" Parity" : ""
		);
			
        }

	qprintf("\n");
}


/*
 * returns expected 7 bit tag ECC for a L2$ line
 */
unsigned char
scache_tag_ecc_check(r10k_sl_t *sl)
{
	__uint64_t tag;

        tag = (sl->sl_tag.t.tag     << 4) |
              (sl->sl_tag.t.state       ) |
              (sl->sl_tag.t.virtind << 2) ;


	return ecc_compute_sctag_ecc(tag);
}



void
icache_save(int cpu)
{
	int i;
	r10k_il_t *il = r10k_cachdbg[cpu].il;

	r10k_cachdbg[cpu].il_stat = R10K_CACHSAVE_PART;

	for (i = 0 ; i < R10K_ICACHELINES ; i += 2) {
		iLine(ICACHE_ADDR(i,0),il++);
		iLine(ICACHE_ADDR(i,1),il++);

	}

	r10k_cachdbg[cpu].il_stat = R10K_CACHSAVE_OK;
}

void
dcache_save(int cpu)
{
	int i;
	r10k_dl_t *dl = r10k_cachdbg[cpu].dl;

	r10k_cachdbg[cpu].dl_stat = R10K_CACHSAVE_PART;

	for (i = 0 ; i < R10K_DCACHELINES ; i += 2) {
		dLine(DCACHE_ADDR(i,0),dl++);
		dLine(DCACHE_ADDR(i,1),dl++);

	}

	r10k_cachdbg[cpu].dl_stat = R10K_CACHSAVE_OK;
}

void
scache_save(int cpu)
{
	int i;
	r10k_sl_t *sl = r10k_cachdbg[cpu].sl;

	r10k_cachdbg[cpu].sl_stat = R10K_CACHSAVE_PART;

	for (i = 0 ; i < R10K_SCACHELINES ; i += 2) {
		sLine(SCACHE_ADDR(i,0),sl++);
		sLine(SCACHE_ADDR(i,1),sl++);
	}

	r10k_cachdbg[cpu].sl_stat = R10K_CACHSAVE_OK;
}
