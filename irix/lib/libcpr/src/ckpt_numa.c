/*
 * ckpt_numa.c
 *
 *      Routines for saving a processes memory policy modules and 
 *	memory locality domain (mld)info.
 *
 *
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
 */

#ident "$Revision: 1.12 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/ckpt_sys.h>
#include <sys/SN/SN0/arch.h>
#include <sys/migr_parms.h>
#include <invent.h>
#include "ckpt.h"
#include "ckpt_internal.h"

#ifdef DEBUG_NUMA
#undef IFDEB1
#define	IFDEB1
#endif
/*
 * rqmode is currently ignored by pocess_mldlink
 */
#ifdef DEBUG
#define	RQMODE	(rqmode_t)(-1)
#else
#define RQMODE	RQMODE_ADVISORY
#endif

static void ckpt_rafflist_free(int, raff_info_t *);
static void ckpt_mldlink_insert(ch_t *, ckpt_mldlist_t *);
static ckpt_mldlist_t *ckpt_mldlink_lookup(ch_t *, caddr_t id);

#define	HWGRAPH_NODENUM	"/hw/nodenum"
#define CNODEID_NONE (cnodeid_t)-1

static void ckpt_get_pmi_info(ckpt_obj_t *co, ckpt_seg_t *seg);
static void ckpt_pmolist_insert(ch_t *ch, ckpt_pmolist_t *nlist);
static ckpt_pmolist_t *ckpt_pmolist_lookup(ch_t *ch, pmo_handle_t pm);
static ckpt_mldlist_t *ckpt_mldlink_pmi_lookup(ch_t *ch,
						ckpt_pmilist_t *pmilist);
extern void ckpt_pmolist_free(ch_t *ch);
static void ckpt_pmi_insert(ch_t *ch, ckpt_pmilist_t *pmi);
static int ckpt_pg_buff_init(caddr_t base, ulong len);
static int ckpt_pg_buff_next(ckpt_obj_t *co, pm_pginfo_t *pg_info);
/*
 * Called before setting up targets NUMA info.  Destroy any existing
 * handles so that the namespace is open.
*/
static int
ckpt_pmo_destroy(pmo_type_t pmo_type, int (*destroy)(pmo_handle_t), char *str)
{
	pmo_handle_t pmo_handle;

        pmo_handle = 0;

        while (1)  {
                pmo_handle = (pmo_handle_t)syssgi(SGI_CKPT_SYS,
						  CKPT_PMOGETNEXT,
						  pmo_type,
						  pmo_handle);
		if (pmo_handle < 0) {
			if (oserror() != ESRCH) {
				cerror("failed to get %s handle (%s)\n", 
					str, STRERR);
				return (-1);
			}
			/*
			 * got the last one
			 */
			break;
		}
		IFDEB1(cdebug("destroy handle %d type %s\n", pmo_handle, str));

		if ((*destroy)(pmo_handle) != 0) {
			cerror("failed to destroy %s %d (%s)\n",
					str, pmo_handle, STRERR);
			return (-1);
		}
		pmo_handle++;
	}
	return (0);
}


int
ckpt_numa_init(ckpt_ta_t *ta)
{
	if ((ta->pi.cp_psi.ps_flags & CKPT_PMO) == 0)
		return (0);

	IFDEB1(cdebug("Pid %d creating new pmo ns and destroying handles\n",
		getpid()));

	if (syssgi(SGI_CKPT_SYS, CKPT_PM_CREATE) < 0) {
		cerror("numa create (%s)\n", STRERR);
		return (-1);
	}
	if (ckpt_pmo_destroy(__PMO_PM, pm_destroy, "pm") < 0) {
		return (-1);
	}
	if (ckpt_pmo_destroy(__PMO_MLDSET, mldset_destroy, "mldset") < 0) {
		return (-1);
	}
	if (ckpt_pmo_destroy(__PMO_MLD, mld_destroy, "mld") < 0) {
		return (-1);
	}
	return (0);
}

int
ckpt_restore_pmdefault(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_pmo_default_t ckpt_pmo;

	if (read(tp->sfd, &ckpt_pmo, sizeof(ckpt_pmo)) < 0) {
		cerror("Failed to read policy module %s)\n", STRERR);
		return (-1);
	}
	if (ckpt_pmo.pmo_magic != CKPT_MAGIC_PMDEFAULT) {
		setoserror(EINVAL);
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&ckpt_pmo.pmo_magic, &magic);
		return (-1);
	}
	if (pm_setdefault(ckpt_pmo.pmo_handle, ckpt_pmo.pmo_memtype) < 0) {
		cerror("Failed to set default policy module, type %d (%s)\n",
				ckpt_pmo.pmo_memtype, STRERR);
		return (-1);
	}
	return (0);
}
/*
 * PMO name spaces are shared amoungst fork'd procs (sharing broken at exec),
 * but pmdefaults are unique per forked process.  So, save pmdefault
 * for each process.
 */

#define WRITE_PMDEFAULT(type) { \
	pmdefault.pmo_memtype = type; \
	CWRITE(co->co_sfd, (caddr_t)&pmdefault, sizeof(pmdefault), 1, pp, rc);\
	if (rc < 0) { \
		cerror("failed to write policy module (%s)\n", STRERR); \
		return (-1); \
	}}
int
ckpt_save_pmdefault(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_pmo_default_t pmdefault;
	ckpt_getpmonext_t pmonext;
	ckpt_pminfo_arg_t pmo_info;
	ckpt_pmo_t ckpt_pmo;
	pmo_handle_t pmhandle;
	long rc;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	/*
	 * Since modules are tied to address space, only do it once for 
	 * shared address space entities
	 */
	if (IS_SPROC(co->co_pinfo) && IS_SADDR(co->co_pinfo)) {
		int rc;

		if ((rc = ckpt_pmdefaults_handled(co)) < 0)
			return (-1);
		if (rc > 0)
			/*
			 * Already dealt with
			 */
			return (0);
	}
	/*
	 * Get targets policy module defaults
	 */
	pmdefault.pmo_magic = CKPT_MAGIC_PMDEFAULT;
	ckpt_pmo.pmo_magic = CKPT_MAGIC_PMODULE;
	pmonext.pmo_type = __PMO_PM;
	pmonext.pmo_handle = 0;
	while (1)  {
		pmhandle = ioctl(co->co_prfd, PIOCCKPTPMOGETNEXT, &pmonext);
		if (pmhandle < 0) {
			if (oserror() != ESRCH) {
				cerror("failed to get pm handle (%s)\n",STRERR);
				return (-1);
			}
			/*
			 * already got the last one
			 */
			break;
		}
		pmo_info.ckpt_pmhandle = pmhandle;
		pmo_info.ckpt_pminfo = &ckpt_pmo.pmo_info;
		pmo_info.ckpt_pmo = &ckpt_pmo.pmo_pmo;

		if (ioctl(co->co_prfd, PIOCCKPTPMINFO, &pmo_info) < 0) {
			cerror("PIOCCKPTPMINFO (%s)\n", STRERR);
			return (-1);
		}
		pmdefault.pmo_handle = pmhandle;

		if(ckpt_pmo.pmo_info.policy_flags&POLICY_DEFAULT_MEM_STACK) {
			IFDEB1(cdebug("STACK:pmdefault handle %d\n",pmhandle));
			WRITE_PMDEFAULT(MEM_STACK);
		}
		if(ckpt_pmo.pmo_info.policy_flags&POLICY_DEFAULT_MEM_TEXT) {
			IFDEB1(cdebug("TEXT:pmdefault handle %d\n",pmhandle));
			WRITE_PMDEFAULT(MEM_TEXT);
		}
		if(ckpt_pmo.pmo_info.policy_flags&POLICY_DEFAULT_MEM_DATA) {
			IFDEB1(cdebug("DATA:pmdefault handle %d\n",pmhandle));
			WRITE_PMDEFAULT(MEM_DATA);
		}
		pmonext.pmo_handle = pmhandle + 1;
	}
	return (0);
}

int
ckpt_restore_pmodule(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_pmo_t *ckpt_pmo;
	policy_set_t policy;
	pmo_handle_t required[2];
	migr_policy_uparms_t migr_uparms;
	migr_as_kparms_t *migr_kparms;

	tp->pmo = ckpt_pmo = (ckpt_pmo_t *)malloc(sizeof(ckpt_pmo_t));
	if (tp->pmo == NULL) {
		cerror("malloc(%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, ckpt_pmo, sizeof(ckpt_pmo_t)) < 0) {
		cerror("Failed to read policy module %s)\n", STRERR);
		return (-1);
	}
	if (ckpt_pmo->pmo_magic != CKPT_MAGIC_PMODULE) {
		setoserror(EINVAL);
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&ckpt_pmo->pmo_magic, &magic);
		return (-1);
	}
#ifdef DEBUG_NUMA
	cdebug("Pid [%d] handle %d\n", getpid(), ckpt_pmo->pmo_handle);
	cdebug("\tplacement name: %s arg: %x pmo: %d\n",
		ckpt_pmo->pmo_info.placement_policy_name,
		ckpt_pmo->pmo_info.placement_policy_args,
		ckpt_pmo->pmo_pmo);
	cdebug("\tfallback name: %s arg: %x\n", 
		ckpt_pmo->pmo_info.fallback_policy_name,
		ckpt_pmo->pmo_info.fallback_policy_args);
	cdebug("\treplication name: %s arg: %x\n",
			ckpt_pmo->pmo_info.replication_policy_name,
			ckpt_pmo->pmo_info.replication_policy_args);
	cdebug("\tmigration name: %s arg: %lx\n", 
		ckpt_pmo->pmo_info.migration_policy_name,
		ckpt_pmo->pmo_info.migration_policy_args);
	cdebug("\tpaging name: %s arg: %x\n", 
		ckpt_pmo->pmo_info.paging_policy_name,
		ckpt_pmo->pmo_info.paging_policy_args);
	cdebug("\tpage_size: %ld\n", 
		ckpt_pmo->pmo_info.page_size);
#endif
	policy.placement_policy_name = ckpt_pmo->pmo_info.placement_policy_name;
	policy.placement_policy_args = ckpt_pmo->pmo_info.placement_policy_args;
	policy.fallback_policy_name = ckpt_pmo->pmo_info.fallback_policy_name;
	policy.fallback_policy_args = ckpt_pmo->pmo_info.fallback_policy_args;
	policy.replication_policy_name = ckpt_pmo->pmo_info.replication_policy_name;
	policy.replication_policy_args = ckpt_pmo->pmo_info.replication_policy_args;
	policy.migration_policy_name = ckpt_pmo->pmo_info.migration_policy_name;

	migr_kparms = (migr_as_kparms_t *)&ckpt_pmo->pmo_info.migration_policy_args;

	if (migr_kparms->migr_base_mode == MIGRATION_MODE_ON) {
		migr_uparms.migr_base_enabled = 1;
		migr_uparms.migr_base_threshold =
					migr_kparms->migr_base_threshold;
		migr_uparms.migr_freeze_enabled =
					migr_kparms->migr_freeze_enabled;
		migr_uparms.migr_freeze_threshold =
					migr_kparms->migr_freeze_threshold;
		migr_uparms.migr_melt_enabled =
					migr_kparms->migr_melt_enabled;
		migr_uparms.migr_melt_threshold =
					migr_kparms->migr_melt_threshold;
		migr_uparms.migr_enqonfail_enabled =
					migr_kparms->migr_enqonfail_enabled;
		migr_uparms.migr_dampening_enabled =
					migr_kparms->migr_dampening_enabled;
		migr_uparms.migr_dampening_factor =
					migr_kparms->migr_dampening_factor;
	} else
		migr_uparms.migr_base_enabled = 0;

	if (migr_kparms->migr_refcnt_mode == REFCNT_MODE_ON)
		migr_uparms.migr_refcnt_enabled = 1;
	else
		migr_uparms.migr_refcnt_enabled = 0;

	policy.migration_policy_args = (void *)&migr_uparms;

	policy.paging_policy_name = ckpt_pmo->pmo_info.paging_policy_name;
	policy.paging_policy_args = ckpt_pmo->pmo_info.paging_policy_args;
	policy.page_size = ckpt_pmo->pmo_info.page_size;
	policy.policy_flags = ckpt_pmo->pmo_info.policy_flags & ~(POLICY_DEFAULT_MEM_STACK|POLICY_DEFAULT_MEM_TEXT|POLICY_DEFAULT_MEM_DATA);

	required[0] = ckpt_pmo->pmo_handle;
	required[1] = ckpt_pmo->pmo_pmo;

	if (pm_create_special(&policy, required)  < 0) {
		/*
		 * we're going to assume that a busy handle is one of
		 * the system defaults inherited by this process
		 */
#ifdef DEBUG_NUMA
		cdebug("Error %d creating policy module handle %d\n",
				oserror(), ckpt_pmo->pmo_handle);
#endif
		cerror("Failed to create policy module %d (%s)\n",
				ckpt_pmo->pmo_handle, STRERR);
		return (-1);
	}
	return (0);
}

int
ckpt_save_pmodule(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_getpmonext_t pmonext;
	pmo_handle_t pmhandle;
	ckpt_pminfo_arg_t pmo_info;
	ckpt_pmo_t ckpt_pmo;
	long rc;
	ckpt_pmolist_t *nlist;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	if ((co->co_pinfo->cp_psi.ps_flags & CKPT_PMO) == 0) {
		IFDEB1(cdebug("Skipping pmodule for proc %d\n", co->co_pid));
		return (0);
	}
	/*
	 * Since modules are tied to address space, only do it once for 
	 * shared address space entities
	 */
	if (IS_SPROC(co->co_pinfo) && IS_SADDR(co->co_pinfo)) {
		int rc;

		if ((rc = ckpt_pmodules_handled(co)) < 0)
			return (-1);
		if (rc > 0)
			/*
			 * Already dealt with
			 */
			return (0);
	}
	ckpt_pmo.pmo_magic = CKPT_MAGIC_PMODULE;

	pmonext.pmo_type = __PMO_PM;
	pmonext.pmo_handle = 0;

	while (1)  {
		pmhandle = ioctl(co->co_prfd, PIOCCKPTPMOGETNEXT, &pmonext);
		if (pmhandle < 0) {
			if (oserror() != ESRCH) {
				cerror("failed to get pm handle (%s)\n", STRERR);
				return (-1);
			}
			/*
			 * already got the last one
			 */
			break;
		}
		pmo_info.ckpt_pmhandle = pmhandle;
		pmo_info.ckpt_pminfo = &ckpt_pmo.pmo_info;
		pmo_info.ckpt_pmo = &ckpt_pmo.pmo_pmo;

		if (ioctl(co->co_prfd, PIOCCKPTPMINFO, &pmo_info) < 0) {
			cerror("PIOCCKPTPMINFO (%s)\n", STRERR);
			return (-1);
		}
		ckpt_pmo.pmo_handle = pmhandle;
#ifdef DEBUG_NUMA
		cdebug("Pid [%d] handle %d\n", co->co_pid, pmhandle);
		cdebug("\tplacement name: %s arg: %x pmo: %d\n",
			ckpt_pmo.pmo_info.placement_policy_name,
			ckpt_pmo.pmo_info.placement_policy_args,
			ckpt_pmo.pmo_pmo);
		cdebug("\tfallback name: %s arg: %x\n", 
			ckpt_pmo.pmo_info.fallback_policy_name,
			ckpt_pmo.pmo_info.fallback_policy_args);
		cdebug("\treplication name: %s arg: %x\n",
				ckpt_pmo.pmo_info.replication_policy_name,
				ckpt_pmo.pmo_info.replication_policy_args);
		cdebug("\tmigration name: %s arg: %x\n", 
			ckpt_pmo.pmo_info.migration_policy_name,
			ckpt_pmo.pmo_info.migration_policy_args);
		cdebug("\tpaging name: %s arg: %x\n", 
			ckpt_pmo.pmo_info.paging_policy_name,
			ckpt_pmo.pmo_info.paging_policy_args);
		cdebug("\tpage_size: %ld\n", 
			ckpt_pmo.pmo_info.page_size);
#endif
		CWRITE(co->co_sfd, (caddr_t)&ckpt_pmo, sizeof(ckpt_pmo_t), 1, pp, rc);
		if (rc < 0) {
			cerror("failed to write policy module (%s)\n", STRERR);
			return (-1);
		}
		nlist = (ckpt_pmolist_t *)malloc(sizeof(ckpt_pmolist_t));
		if(nlist == NULL) {
			cerror("malloc ckpt_pmolist_t: (%s)\n", STRERR);
			return -1;
		}
		nlist->pmo_handle = pmhandle;
		strcpy(nlist->pmo_name,ckpt_pmo.pmo_info.placement_policy_name);
		ckpt_pmolist_insert(co->co_ch, nlist);
		pmonext.pmo_handle = pmhandle + 1;
	}
	return	(0);
}

int
ckpt_get_pmodule(ckpt_obj_t *co, ckpt_seg_t *seg, int is_shared)
{
	vrange_t vrange;
	pmo_handle_list_t pmo_list;
	pmo_handle_t pmhandles[2];
	ckpt_pmgetall_arg_t arg;
	int count;
	/*
	 * Should only be 1 handle for this range.  Build a struct that can
	 * handle 2 and make sure only get 1 back
	 */
	pmo_list.handles = pmhandles;
	pmo_list.length = 2;

	vrange.base_addr = seg->cs_vaddr;
	vrange.length = seg->cs_len;

	arg.ckpt_vrange = &vrange;
	arg.ckpt_handles = &pmo_list;

	if ((count = ioctl(co->co_prfd, PIOCCKPTPMGETALL, &arg)) < 0) {
		cerror("PIOCCKPTPMGETALL (%s)\n", STRERR);
		return (-1);
	}
	if ((count == 0) || count > 1) {
		cnotice("Pid [%d] vaddr, len %lx, %d: policy module count %d\n",
			co->co_pid, seg->cs_vaddr, seg->cs_len, count);
		seg->cs_pmhandle = (pmo_handle_t)-1;
		return (0);
	}
	seg->cs_pmhandle = pmhandles[0];
	if(is_shared && (co->co_pinfo->cp_psi.ps_flags & CKPT_PMO)) {
		ckpt_get_pmi_info(co, seg);
		IFDEB1(cdebug("Shared Seg: %llx, len %llx, pm handle %d\n", 
			seg->cs_vaddr, seg->cs_len, seg->cs_pmhandle));
	}

	return (0);
}

/*
 * get page migration info.
 */
static void ckpt_get_pmi_info(ckpt_obj_t *co, ckpt_seg_t *seg)
{
	ch_t *ch = co->co_ch;
	ckpt_pmolist_t *plist;
	cnodeid_t	nodeid1, nodeid2;
	pm_pginfo_t	tmp_pg_buf;
	ckpt_pmilist_t *pmi, tmp_pmi;
	int ret = 0;

	plist = ckpt_pmolist_lookup(ch, seg->cs_pmhandle);
	if(plist == NULL ||
 	  (strcmp(plist->pmo_name, "PlacementDefault") != 0 && 
	   strcmp(plist->pmo_name, "PlacementFirstTouch") != 0)) {
		return;
	}
	/* we need to replace this two policies with a fixed policy
	 * when we first brough up the memory at restart.
	 */ 

	IFDEB1(cdebug("name %s, handle %d \n",
			plist->pmo_name, seg->cs_pmhandle));

	nodeid1 = CNODEID_NONE;
	tmp_pmi.pmi_magic = CKPT_MAGIC_PMI;
	tmp_pmi.pmi_owner = co->co_pid;
	tmp_pmi.pmi_base = seg->cs_vaddr;

	IFDEB1(cdebug("seg vaddr %x len %d\n", seg->cs_vaddr, seg->cs_len));

	/* init page info buffer */
	if(ckpt_pg_buff_init((caddr_t)seg->cs_vaddr, seg->cs_len) <0) 
		return;
	do {
 		ret = ckpt_pg_buff_next(co, &tmp_pg_buf);
		nodeid2 = (cnodeid_t)tmp_pg_buf.node_dev;
		if((nodeid1 == nodeid2) && 
		   (tmp_pmi.pmi_eaddr >= tmp_pg_buf.vaddr)) {
			/* in case page_size is large page, page_size
			 * is multiple basic pages, just ignore the next
			 * pages that are still within the large page.
			 */
			if(tmp_pmi.pmi_eaddr == tmp_pg_buf.vaddr) {
				tmp_pmi.pmi_eaddr = 
					tmp_pg_buf.vaddr+tmp_pg_buf.page_size;
			}
			continue;
		}
		if(nodeid1 != CNODEID_NONE) {
			/* save this seg */
			tmp_pmi.pmi_nodeid  = nodeid1;
			pmi = (ckpt_pmilist_t *) malloc(sizeof(ckpt_pmilist_t));
			if(pmi == NULL) 
				return;
			*pmi = tmp_pmi;
			ckpt_pmi_insert(ch, pmi);
		}
#ifdef DEBUG_NUMA
		cdebug("nodeid1 %d nodeid2 %d\n", nodeid1, nodeid2);
#endif
		tmp_pmi.pmi_vaddr = tmp_pg_buf.vaddr;
		tmp_pmi.pmi_eaddr = tmp_pg_buf.vaddr + tmp_pg_buf.page_size;
		nodeid1 = nodeid2;
	} while(ret >=0);

	return;
}

#define PG_BUF_SIZE	100
static int 		pg_buf_end, pg_buf_index;
static caddr_t 		pg_info_base, pg_info_end;
static pm_pginfo_t 	*pg_buf;

static int ckpt_pg_buff_init(caddr_t base, ulong len)
{

	pg_info_base = base;
	pg_info_end = pg_info_base + len;
	pg_buf_index = 0;
	pg_buf_end = 0;	
	/* this buf is free at the last call to ckpt_pg_buff_next */
	pg_buf = (pm_pginfo_t *)malloc(sizeof(pm_pginfo_t)*PG_BUF_SIZE);
	if(pg_buf == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return -1;
	}
	return 0;
}

static int ckpt_pg_buff_next(ckpt_obj_t *co, pm_pginfo_t *pg_info)
{

	ckpt_pm_pginfo_t 	ckpt_pginfo;
	int n;

	/* buffer has all been read */
	if(pg_info_base >=pg_info_end) {
		pg_info->node_dev = CNODEID_NONE;
		if(pg_buf)
			free(pg_buf);
		pg_buf = NULL;
		return -1;
	}

	if(pg_buf_index >= pg_buf_end) {
		/* buffer's empty, read again */
		ckpt_pginfo.vrange.base_addr = pg_info_base;
		ckpt_pginfo.pginfo_list.pm_pginfo = pg_buf;
		ckpt_pginfo.pginfo_list.length = PG_BUF_SIZE;
		ckpt_pginfo.vrange.length = pg_info_end - pg_info_base;
		n = ioctl(co->co_prfd, PIOCCKPTPMPGINFO, &ckpt_pginfo);
		IFDEB1(cdebug("curr vaddr %x, len %x, n %d\n", 
	     	   ckpt_pginfo.vrange.base_addr, ckpt_pginfo.vrange.length,n));
		if(n == 0)
			return -1;
		else if(n < 0) {
			IFDEB1(cdebug("n=%d PIOCCKPTPMPGINFO(%s)\n",n,STRERR));
			return -1;
		}
		pg_buf_index = 0;
		pg_buf_end = n;
	}

	*pg_info = pg_buf[pg_buf_index];
	pg_info_base = pg_info->vaddr + pg_info->page_size;

#ifdef DEBUG_NUMA 
	cdebug("index %d node %d vaddr %x, len %x, base_addr %x\n", 
		pg_buf_index, (int)pg_info->node_dev, pg_info->vaddr, 
		pg_info->page_size, pg_info_base);
#endif
	pg_buf_index++;
	return 0;
}

/*
 * Routines for dealing with memory localilty domains (mlds) and
 * mld sets.  Normally, mlds are "placed" by puttig them in an mldset and
 * placing the set.   It's possible though, to have an mld that was placed 
 * by the kernel  or one that was in an mldset that's been destroyed.
 * So, first, save mldsets and log the mlds found in those.  Then save mlds and
 * for check each against the log.   For an mld that appears "placed" that's
 * not in an mldset, we'll have to do something special.
 */
/*
 *  Log a list of mlds
 */
static int
ckpt_mldlog_insert(ckpt_obj_t *co,  int mldcount, pmo_handle_t *mldlist)
{
	ckpt_mldlog_t *mldlog;

	mldlog = (ckpt_mldlog_t *)malloc(sizeof(ckpt_mldlog_t));
	if (mldlog == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	mldlog->ml_count = mldcount;
	mldlog->ml_list = mldlist;
	mldlog->ml_next = co->co_mldlog;
	co->co_mldlog = mldlog;
	return (0);
}
/*
 * Lookup an mld in the mld log
 */
static int
ckpt_mldlog_lookup(ckpt_obj_t *co, pmo_handle_t mld)
{
	ckpt_mldlog_t *mldlog;
	int i;

	for (mldlog = co->co_mldlog; mldlog; mldlog = mldlog->ml_next) {
		for (i = 0; i < mldlog->ml_count; i++) {
			if (mld == mldlog->ml_list[i])
				return(1);
		}
	}
	return (0);
}

void
ckpt_mldlog_free(ckpt_obj_t *co)
{
	ckpt_mldlog_t *cur, *next;

	cur = co->co_mldlog;
	while (cur) {
		free(cur->ml_list);
		next = cur->ml_next;
		free(cur);
		cur = next;
	}
}

static int
ckpt_mld_place(pmo_handle_t mld, ckpt_mld_t *ckpt_mld)
{
	pmo_handle_t mldset;
	raff_info_t raff;
	int rv;

	mldset = mldset_create(&mld, 1);
	if (mldset < 0) {
		cerror("Failed to create mldset for mld %d (%s)\n",
				ckpt_mld->mld_handle, STRERR);
		return (-1);
	}
	raff.resource = ckpt_mld->mld_hwpath;
	raff.reslen = (ushort_t)strlen(ckpt_mld->mld_hwpath);
	raff.restype = RAFFIDT_NAME;
	raff.radius = ckpt_mld->mld_info.mld_radius;
	raff.attr = RAFFATTR_ATTRACTION;  

	rv = mldset_place(mldset, TOPOLOGY_PHYSNODES, &raff, 1, RQMODE_ADVISORY);
	if (rv < 0) {
		cerror("Failed to place mld %d (%s)\n", ckpt_mld->mld_handle, STRERR);
		return (-1);
	}
	(void)mldset_destroy(mldset);

	return (0);
}

int
ckpt_restore_mld(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_mld_t ckpt_mld;
	pmo_handle_t mld;

	if (read(tp->sfd, &ckpt_mld, sizeof(ckpt_mld)) < 0) {
		cerror("Failed to read memory locality domain (%s)\n", STRERR);
		return (-1);
	}
	if (ckpt_mld.mld_magic != CKPT_MAGIC_MLD) {
		setoserror(EINVAL);
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&ckpt_mld.mld_magic, &magic);
		return (-1);
	}
#ifdef DEBUG_NUMA
	cdebug("restore mld %d radius %d size %d path %s\n", 
				ckpt_mld.mld_handle,
			       	ckpt_mld.mld_info.mld_radius,
			       	ckpt_mld.mld_info.mld_size,
				ckpt_mld.mld_hwpath);
#endif
	if (tp->pmo && (tp->pmo->pmo_pmo == ckpt_mld.mld_handle)) {
		IFDEB1(cdebug("skip mld create on %d, pmodule pmo\n", 
				ckpt_mld.mld_handle));
		return (0);
	}
	if ((mld = mld_create_special(ckpt_mld.mld_info.mld_radius,
			       	      ckpt_mld.mld_info.mld_size,
			       	      ckpt_mld.mld_handle))  < 0) {
#ifdef DEBUG_NUMA
		cdebug("Error %d creating MLD handle %d\n",
				oserror(), ckpt_mld.mld_handle);
#endif
		cerror("Failed to create mld %d (%s)\n",
				ckpt_mld.mld_handle, STRERR);
		return (-1);
	}
	/*
	 * If we looged a hwgrph pathname for this mld, it was placed, but
	 * not in an mldset.  So place it now using a simple mldset, then
	 * destroy the mldset
	 */
	if (ckpt_mld.mld_hwpath[0]) {
		if (ckpt_mld_place(mld, &ckpt_mld) < 0)
			return (-1);
	}
	if (ckpt_mld.mld_handle == (pmo_handle_t)-1) {
		
		if (process_mldlink(0, mld, RQMODE) < 0) {
			cerror("process mldlink (%s)\n", STRERR);
			return (-1);
		}
		mld_destroy(mld);
	}
	return (0);
}

int
ckpt_save_mld(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_getpmonext_t pmonext;
	pmo_handle_t mldhandle;
	ckpt_mldinfo_arg_t mld_info;
	ckpt_mldlist_t *mldlist;
	ckpt_mld_t ckpt_mld;
	char nodepath[PATH_MAX];
	int cnt;
	long rc;
	ckpt_obj_t *co2;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	if ((co->co_pinfo->cp_psi.ps_flags & CKPT_PMO) == 0) {
		IFDEB1(cdebug("Skipping mld for proc %d\n", co->co_pid));
		return (0);
	}
	/*
	 * Since modules are tied to address space, only do it once for 
	 * shared address space entities
	 */
	if (IS_SPROC(co->co_pinfo) && IS_SADDR(co->co_pinfo)) {
		int rc;

		if ((rc = ckpt_mlds_handled(co)) < 0)
			return (-1);
		if (rc > 0)
			/*
			 * Already dealt with
			 */
			return (0);
	}
	ckpt_mld.mld_magic = CKPT_MAGIC_MLD;

	pmonext.pmo_type = __PMO_MLD;
	pmonext.pmo_handle = 0;

	while (1)  {
		mldhandle = ioctl(co->co_prfd, PIOCCKPTPMOGETNEXT, &pmonext);
		if (mldhandle < 0) {
			if (oserror() != ESRCH) {
				cerror("failed to get mld handle (%s)\n", STRERR);
				return (-1);
			}
			/*
			 * already got the last one
			 */
			break;
		}
		mld_info.ckpt_mldhandle = mldhandle;
		mld_info.ckpt_mldinfo = &ckpt_mld.mld_info;

		if (ioctl(co->co_prfd, PIOCCKPTMLDINFO, &mld_info) < 0) {
			cerror("PIOCCKPTMLDINFO (%s)\n", STRERR);
			return (-1);
		}
		ckpt_mld.mld_handle = mldhandle;
#ifdef DEBUG_NUMA
		cdebug("Pid [%d] MLD handle %d\n", co->co_pid, mldhandle);
		cdebug("Cnodeid %d:Radius %d:Size %d\n",
			ckpt_mld.mld_info.mld_nodeid,
			ckpt_mld.mld_info.mld_radius,
			ckpt_mld.mld_info.mld_size);
#endif
		mldlist = (ckpt_mldlist_t *)malloc(sizeof(ckpt_mldlist_t));
		if (mldlist == NULL) {
			cerror("malloc (%s)\n", STRERR);
			return (-1);
		}
		mldlist->mld_owner = co->co_pid;
		mldlist->mld_handle = mldhandle;
		mldlist->mld_id = ckpt_mld.mld_info.mld_id;
		mldlist->mld_link_proc = 0;
		mldlist->mld_nodeid = ckpt_mld.mld_info.mld_nodeid;

		if ((ckpt_mld.mld_info.mld_nodeid != CNODEID_NONE)&&
		    (!ckpt_mldlog_lookup(co, mldhandle))) {

			sprintf(nodepath, "%s/%d", HWGRAPH_NODENUM,
						ckpt_mld.mld_info.mld_nodeid);

			cnt = readlink(nodepath, ckpt_mld.mld_hwpath, PATH_MAX);
			if (cnt < 0) {
				cerror("readlink of %s failed (%s)\n",
						nodepath, STRERR);
				return (-1);
			}
			ckpt_mld.mld_hwpath[cnt] = '\0';

			IFDEB1(cdebug("node %s path %s\n", nodepath,
							ckpt_mld.mld_hwpath));

		} else
			ckpt_mld.mld_hwpath[0] = '\0';

		ckpt_mldlink_insert(co->co_ch, mldlist);

		CWRITE(co->co_sfd, (caddr_t)&ckpt_mld, sizeof(ckpt_mld_t), 1, pp, rc);
		if (rc < 0) {
			cerror("failed to write mld (%s)\n", STRERR);
			return (-1);
		}
		pmonext.pmo_handle = mldhandle + 1;
	}
	/*
	 * See if our mldlink'd mld has been discovred yet.  If not, it's
	 * not in our pmo namespace and might not be in anyone elses.
	 * To guard against that eventuality, save away an mld that is
	 * consistent with our mldlink.  At rstart, create, place and
	 * link it, then remove from namespace.   If actual mldlink
	 * comes along later, it'll override this mldlink.
	 */
	if ((co->co_pinfo->cp_psi.ps_mldlink != NULL) &&
	    (ckpt_mldlink_lookup(co->co_ch, co->co_pinfo->cp_psi.ps_mldlink) == NULL)) {

		ckpt_mld.mld_handle = (pmo_handle_t)-1;

		if (ioctl(co->co_prfd, PIOCCKPTMLDLINKINFO, &ckpt_mld.mld_info) < 0) {
			cerror("PIOCCKPTMLDLINKINFO (%s)\n", STRERR);
			return (-1);
		}
		if (ckpt_mld.mld_info.mld_nodeid == CNODEID_NONE) {
			/*
			 * Should not happen
			 */
			cerror("Pid %d linked to unplaced mld\n", co->co_pid);
			setoserror(ECKPT);
			return (-1);
		}
		sprintf(nodepath, "%s/%d", HWGRAPH_NODENUM,
					ckpt_mld.mld_info.mld_nodeid);

		cnt = readlink(nodepath, ckpt_mld.mld_hwpath, PATH_MAX);
		if (cnt < 0) {
			cerror("readlink of %s failed (%s)\n",
					nodepath, STRERR);
			return (-1);
		}
		ckpt_mld.mld_hwpath[cnt] = '\0';

		IFDEB1(cdebug("node %s path %s\n", nodepath,
						ckpt_mld.mld_hwpath));

		CWRITE(co->co_sfd, (caddr_t)&ckpt_mld, sizeof(ckpt_mld_t), 1, pp, rc);
		if (rc < 0) {
			cerror("failed to write mld (%s)\n", STRERR);
			return (-1);
		}
	}

	/* mark the mlds that used as mldlink */
	FOR_EACH_PROC(co->co_ch->ch_first, co2) {
		if (co2->co_pinfo->cp_psi.ps_mldlink) {
			mldlist = ckpt_mldlink_lookup(co->co_ch, 
					co2->co_pinfo->cp_psi.ps_mldlink);
			if (mldlist == NULL)
				continue;
			mldlist->mld_link_proc = co2->co_pid;
			IFDEB1(cdebug("mldlink pid %d mld %d\n", co2->co_pid,
				mldlist->mld_handle));
		}
	}
	return	(0);
}

int
ckpt_restore_mldset(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_mldset_t ckpt_mldset;
	pmo_handle_t *mldlist;
	int raffcnt;
	raff_info_t *rafflist = NULL;
	int i;

	if (read(tp->sfd, &ckpt_mldset, sizeof(ckpt_mldset)) < 0) {
		cerror("Failed to read mldset (%s)\n", STRERR);
		return (-1);
	}
	if (ckpt_mldset.mldset_magic != CKPT_MAGIC_MLDSET) {
		setoserror(EINVAL);
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&ckpt_mldset.mldset_magic, &magic);
		return (-1);
	}
	assert(ckpt_mldset.mldset_count > 0);

	if (tp->pmo && (tp->pmo->pmo_pmo == ckpt_mldset.mldset_handle)) {
		IFDEB1(cdebug("skip mldset create on %d, pmodule pmo\n", 
				ckpt_mldset.mldset_handle));
		return (0);
	}
	mldlist = (pmo_handle_t *)
			malloc(ckpt_mldset.mldset_count * sizeof(pmo_handle_t));
	if (mldlist == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, mldlist, ckpt_mldset.mldset_count * sizeof(pmo_handle_t)) < 0) {
		cerror("Failed to read mldset (%s)\n", STRERR);
		return (-1);
	}
	IFDEB1(cdebug("restore mldset %d cnt %d top %d rqmode %d raffcnt %d\n", 
				  ckpt_mldset.mldset_handle,
				  ckpt_mldset.mldset_count,
				  ckpt_mldset.mldset_topology,
				  ckpt_mldset.mldset_rqmode,
				  ckpt_mldset.mldset_raffcnt));
			
#ifdef DEBUG_NUMA
	for(i=0; i<ckpt_mldset.mldset_count; i++) {
		IFDEB1(cdebug("%d\n", mldlist[i]));
	}
#endif
	if (mldset_create_special(mldlist,
				  ckpt_mldset.mldset_count,
				  ckpt_mldset.mldset_handle)  < 0) {
		/*
		 * we're going to assume that a busy handle is one of
		 * the system defaults inherited by this process
		 */
#ifdef DEBUG_NUMA
		cdebug("Error %d creating MLD handle %d\n",
				oserror(), ckpt_mldset.mldset_handle);
#endif
		cerror("Failed to create mldset %d (%s)\n",
				ckpt_mldset.mldset_handle, STRERR);
		free(mldlist);
		return (-1);
	}
	free(mldlist);
	/*
	 * Now that set is created, place it
	 */
	if (ckpt_mldset.mldset_topology == (topology_type_t)-1) {
		/*
		 * Wasn't placed
		 */
		assert(ckpt_mldset.mldset_rqmode == (rqmode_t)-1);

		return (0);
	}
	/*
	 * Read and convert raff info
	 */
	if ((raffcnt = ckpt_mldset.mldset_raffcnt) == 0)
		return (0);

	rafflist = (raff_info_t *)calloc(raffcnt, sizeof(raff_info_t));
	if (rafflist == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, rafflist, raffcnt * sizeof(raff_info_t)) < 0) {
		cerror("Failed to read raff headers (%s)\n", STRERR);
		free(rafflist);
		return (-1);
	}
	for(i = 0; i < raffcnt; i++) {

		rafflist[i].resource = malloc(rafflist[i].reslen);
		if (rafflist[i].resource == NULL) {
			cerror("malloc (%s)\n", STRERR);
			ckpt_rafflist_free(i, rafflist);
			return (-1);
		}
		if (read(tp->sfd, rafflist[i].resource, rafflist[i].reslen)<0){
			cerror("Failed to read raff resource (%s)\n", STRERR);
			ckpt_rafflist_free(i, rafflist);
			return (-1);
		}
		IFDEB1(cdebug("raff entry %d resource %s\n", i, 
						rafflist[i].resource));
	}
	if (mldset_place(ckpt_mldset.mldset_handle,
			 ckpt_mldset.mldset_topology,
			 rafflist, raffcnt, ckpt_mldset.mldset_rqmode) < 0) {
		/* try it again without rafflist. In case the machine configs
		 * differently after checkpoint.
		 */
		if (mldset_place(ckpt_mldset.mldset_handle,
			 ckpt_mldset.mldset_topology,
			 NULL, 0, ckpt_mldset.mldset_rqmode) < 0) {
		
			cerror("Failed to place mldset, handle %d (%s)\n",
					ckpt_mldset.mldset_handle, STRERR);
			ckpt_rafflist_free(raffcnt, rafflist);
			return (-1);
		}
	}
	ckpt_rafflist_free(raffcnt, rafflist);

	return (0);
}

/*
 * Save info regarding mld sets.  This includes the list of mlds in the set
 * and placement info, if any, for the set
 */

/*
 * Retrieve mld handles in mldset
 */
static int
ckpt_mldset_mldlist(ckpt_obj_t *co, ckpt_mldset_t *ckpt_mldset, pmo_handle_t **mldlist)
{
	ckpt_mldset_info_arg_t arg;
	int count;
	pmo_handle_t mldsethandle;

	mldsethandle = ckpt_mldset->mldset_handle;
	/*
	 * PIOCCKPTMLDSETINFO returns # of entries regardless of
	 * the size of our buffer.
	 */
	arg.mldset_handle = mldsethandle;
	arg.mldset_info.mldlist_len = 0;
	arg.mldset_info.mldlist = NULL;

	count = ioctl(co->co_prfd, PIOCCKPTMLDSETINFO, &arg);
	if (count < 0) {
		cerror("PIOCCKPTMLDSETINFO 1 (%s)\n", STRERR);
		return (-1);
	}
	assert(count > 0);

	IFDEB1(cdebug("pid %d mldset: %d count %d \n", 
				co->co_pid, mldsethandle, count));
	/*
	 * Now call PIOCCKPTMLDSETINFO with correctly sized buffer
	 */
	arg.mldset_info.mldlist_len = count;
	arg.mldset_info.mldlist =
		(pmo_handle_t *)malloc(count * sizeof(pmo_handle_t));

	if (arg.mldset_info.mldlist == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	count = ioctl(co->co_prfd, PIOCCKPTMLDSETINFO, &arg);
	if (count < 0) {
		cerror("PIOCCKPTMLDSETINFO 2 (%s)\n", STRERR);
		return (-1);
	}
	if (count != arg.mldset_info.mldlist_len) {
		free(arg.mldset_info.mldlist);
		cerror("mldset count mis-match\n");
		setoserror(ECKPT);
		return (-1);
	}
	ckpt_mldset->mldset_count = count;
	*mldlist = arg.mldset_info.mldlist;


#ifdef DEBUG_NUMA
	for(count = 0; count < ckpt_mldset->mldset_count; count++) {
		IFDEB1(cdebug("%d \n", arg.mldset_info.mldlist[count]));
	}
#endif
	return (0);
}

static int
ckpt_mldset_placement(ckpt_obj_t *co, ckpt_mldset_t *ckpt_mldset, raff_info_t **raffinfo)

{
	pmo_handle_t mldsethandle;
	mldset_placement_info_t place;
	int count;
	int i;
	ckpt_raffopen_t raffopen;

	mldsethandle = ckpt_mldset->mldset_handle;
	/*
	 * Fetch placement info.  If there's a rafflist, 
	 * PIOCCKPTMLDPLACEINFO returns # of entries regardless
	 * of our list size
	 */
	place.mldset_handle = mldsethandle;
	place.rafflist = NULL;
	place.rafflist_len = 0;

	count = ioctl(co->co_prfd, PIOCCKPTMLDPLACEINFO, &place);
	if (count < 0) {
		cerror("PIOCCKPTMLDPLACEINFO (%s)\n", STRERR);
		return (-1);
	}
	ckpt_mldset->mldset_raffcnt = count;

	if (place.mldset_handle != mldsethandle)  {
		/*
		 * Not placed!
		 */
		assert(count == 0);

		ckpt_mldset->mldset_topology = (topology_type_t)-1;
		ckpt_mldset->mldset_rqmode = (rqmode_t)-1;
	} else {
		ckpt_mldset->mldset_topology = place.topology_type;
		ckpt_mldset->mldset_rqmode = place.rqmode;
	}
	if (count == 0) {
		*raffinfo = NULL;
		return (0);
	}
	*raffinfo = (raff_info_t *)malloc(count * sizeof(raff_info_t));
	if (*raffinfo == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	place.rafflist = *raffinfo;
	place.rafflist_len = count;

	count = ioctl(co->co_prfd, PIOCCKPTMLDPLACEINFO, &place);
	if (count < 0) {
		cerror("PIOCCKPTMLDPLACEINFO (%s)\n", STRERR);
		free(*raffinfo);
		return (-1);
	}
	assert(count == place.rafflist_len);
	/*
	 * Now get pathnames for node affinity
	 */
	raffopen.mldset_handle = mldsethandle;

	for (i = 0; i < count; i++) {
		int raff_fd;

		raffopen.mldset_element = i;

		raff_fd = ioctl(co->co_prfd, PIOCCKPTRAFFOPEN, &raffopen);
		if (raff_fd < 0) {
			cerror("Failed to open rafflist element (%s)\n",
				STRERR);
			ckpt_rafflist_free(i, *raffinfo);
			return (-1);
		}
		if (ckpt_pathname(co, raff_fd, (char **)&(*raffinfo)->resource) < 0) {
			close(raff_fd);
			ckpt_rafflist_free(i, *raffinfo);
			return (-1);
		}
		(*raffinfo)->reslen =
		    (unsigned short)strlen((char *)((*raffinfo)->resource)) + 1;
		(*raffinfo)->restype = RAFFIDT_NAME;

		close(raff_fd);

		IFDEB1(cdebug("raff entry %d resource %s\n", i, (*raffinfo)->resource));
	}
	return (0);
}

static int
ckpt_mldset_dump(ckpt_obj_t *co, ckpt_prop_t *pp, ckpt_mldset_t *ckpt_mldset,
			pmo_handle_t *mldlist, raff_info_t * raff)
{
	long rc;
	int i;
	/*
	 * Dump to the statefile...header, mld list, rafflist
	 */
	CWRITE(co->co_sfd, ckpt_mldset, sizeof(ckpt_mldset_t), 1, pp, rc);
	if (rc < 0) {
		cerror("failed to write mldset (%s)\n", STRERR);
		return (-1);
	}
	CWRITE(	co->co_sfd,
		mldlist,
		ckpt_mldset->mldset_count * sizeof(pmo_handle_t),
		0,
		pp,
		rc);
	if (rc < 0) {
		cerror("failed to write mldset (%s)\n", STRERR);
		return (-1);
	}
	if (ckpt_mldset->mldset_raffcnt == 0)
		return (0);
	/*
	 * Raff structs
	 */
	CWRITE(	co->co_sfd,
		raff,
		ckpt_mldset->mldset_raffcnt * sizeof(raff_info_t),
		0,
		pp,
		rc);
	if (rc < 0) {
		cerror("failed to write raff headers (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Raff pathnames
	 */
	for (i = 0; i < ckpt_mldset->mldset_raffcnt; i++)  {

		CWRITE(co->co_sfd, raff[i].resource, raff[i].reslen, 0, pp, rc);
		if (rc < 0) {
			cerror("failed to write raff resources (%s)\n", STRERR);
			return (-1);
		}
	}
	return	(0);
}

int
ckpt_save_mldset(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_mldset_t ckpt_mldset;
	ckpt_getpmonext_t pmonext;
	pmo_handle_t mldsethandle, *mldlist;
	raff_info_t *raffinfo;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	if ((co->co_pinfo->cp_psi.ps_flags & CKPT_PMO) == 0) {
		IFDEB1(cdebug("Skipping mldset for proc %d\n", co->co_pid));
		return (0);
	}
	/*
	 * Since modules are tied to address space, only do it once for 
	 * shared address space entities
	 */
	if (IS_SPROC(co->co_pinfo) && IS_SADDR(co->co_pinfo)) {
		int rc;

		if ((rc = ckpt_mldset_handled(co)) < 0)
			return (-1);
		if (rc > 0)
			/*
			 * Already dealt with
			 */
			return (0);
	}
	ckpt_mldset.mldset_magic = CKPT_MAGIC_MLDSET;

	pmonext.pmo_type = __PMO_MLDSET;
	pmonext.pmo_handle = 0;

	while (1)  {

		raffinfo = NULL;
#ifdef DEBUG
		mldlist = NULL;
#endif
		mldsethandle = ioctl(co->co_prfd, PIOCCKPTPMOGETNEXT, &pmonext);
		if (mldsethandle < 0) {
			if (oserror() != ESRCH) {
				cerror("failed to get mld handle (%s)\n", STRERR);
				return (-1);
			}
			/*
			 * already got the last one
			 */
			break;
		}
		ckpt_mldset.mldset_handle = mldsethandle;
#ifdef DEBUG_NUMA
		cdebug("Pid [%d] MLDSET handle %d\n", co->co_pid, mldsethandle);
#endif
		/*
		 * First, list of mlds
		 */
		if (ckpt_mldset_mldlist(co, &ckpt_mldset, &mldlist) < 0)
			return (-1);
		/*
		 * Now the placement info
		 */
		if (ckpt_mldset_placement(co, &ckpt_mldset, &raffinfo) < 0) {
			assert(mldlist);
			free(mldlist);
			return (-1);
		}
		if (ckpt_mldset_dump(co, pp, &ckpt_mldset, mldlist, raffinfo) < 0) {
			assert(mldlist);
			free(mldlist);
			if (raffinfo)
				ckpt_rafflist_free(ckpt_mldset.mldset_raffcnt, raffinfo);
			return (-1);
		}
		assert(mldlist);
		/*
	 	 * Log placed mlds.
		 */
		if (raffinfo && raffinfo->reslen) {
			if (ckpt_mldlog_insert(	co,
						ckpt_mldset.mldset_count,
						mldlist) < 0) {
				assert(mldlist);
				free(mldlist);
				ckpt_rafflist_free(ckpt_mldset.mldset_raffcnt,
						   raffinfo);
				return (-1);
			}
			/*
			 * mldlist freed during checkpoint cleanup
			 */
		} else
			free(mldlist);

		if (raffinfo)
			ckpt_rafflist_free(ckpt_mldset.mldset_raffcnt, raffinfo);
		pmonext.pmo_handle = mldsethandle + 1;
	}
	return (0);
}

int
ckpt_save_mldlink(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_obj_t *co;
	ckpt_mldlink_t mldlink;
	ckpt_mldlist_t *mldlist;
	long rc;

	mldlink.mlink_magic = CKPT_MAGIC_MLDLINK;

	FOR_EACH_PROC(ch->ch_first, co) {
		if (co->co_pinfo->cp_psi.ps_mldlink) {
			mldlist = ckpt_mldlink_lookup(ch, co->co_pinfo->cp_psi.ps_mldlink);
			if (mldlist == NULL)
				continue;
			mldlink.mlink_owner = mldlist->mld_owner;
			mldlink.mlink_mld = mldlist->mld_handle;
			mldlink.mlink_link = co->co_pid;

			IFDEB1(cdebug("mldlink %lx owner %d handle %d link %d\n", 
				co->co_pinfo->cp_psi.ps_mldlink,
				mldlink.mlink_owner,
				mldlink.mlink_mld,
				mldlink.mlink_link));

			CWRITE(ch->ch_sfd, &mldlink, sizeof(mldlink), 1, pp, rc);
			if (rc < 0) {
				cerror("mldlink write (%s)\n", STRERR);
				return (-1);
			}
		}
	}
	return (0);
}

/* ARGSUSED */
int
ckpt_restore_mldlink(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	ckpt_mldlink_t	mldlink;
	pid_t mypid = *((pid_t *)misc);

	if (read(tp->ssfd, &mldlink, sizeof(mldlink)) < 0) {
		cerror("mldlink read (%s)\n", STRERR);
		return (-1);
	}
	if (mldlink.mlink_magic != magic) {
		cerror("mldlink bad magic\n");
		return (-1);
	}
	IFDEB1(cdebug("mldlink owner %d handle %d link %d\n", 
			mldlink.mlink_owner,
			mldlink.mlink_mld,
			mldlink.mlink_link));
	if (vtoppid(mldlink.mlink_owner) == mypid) {
		if (process_mldlink(vtoppid(mldlink.mlink_link),
				    mldlink.mlink_mld,
				    RQMODE) < 0) {
			cerror("process mldlink (%s)\n", STRERR);
			return (-1);
		}
	}

	return (0);
}

/*
 * save page migration info.
 */
int ckpt_save_pmi(ch_t *ch, ckpt_prop_t *pp)
{
	int rc;
	ckpt_pmilist_t	*pmilist = ch->ch_pmilist;
	ckpt_mldlist_t *mldlist;

	while(pmilist != NULL) {
		mldlist = ckpt_mldlink_pmi_lookup(ch, pmilist);
		if(mldlist == NULL) {
			pmilist = pmilist->pmi_next;
			continue;
		}
		pmilist->pmi_phandle = mldlist->mld_handle;
		IFDEB1(cdebug("ckpt_save_pmi: owner %d base %0llx vaddr %llx"
			" eaddr %llx, mld %d, link_proc %d\n", 
			pmilist->pmi_owner, pmilist->pmi_base, 
			pmilist->pmi_vaddr, pmilist->pmi_eaddr, 
			mldlist->mld_handle, mldlist->mld_link_proc));

		CWRITE(ch->ch_sfd, (void *)pmilist, 
				sizeof(ckpt_pmilist_t), 1, pp, rc);
		if (rc < 0) {
			cerror("pmilist write (%s)\n", STRERR);
			return (-1);
		}
		pmilist = pmilist->pmi_next;
	}			
	return 0;
}

/*
 * Routine to free a partially constructed raff_info list
 */
static void
ckpt_rafflist_free(int count, raff_info_t *raffinfo)
{
	int i;
	raff_info_t *raff;

	assert(count >= 0);
	assert(raffinfo != NULL);

	for (i = 0, raff = raffinfo; i < count; i++, raff++) {
		assert(raff->resource);
		free(raff->resource);
	}
	free(raffinfo);
}
/*
 * Lists to maintain links between processes
 */
static void
ckpt_mldlink_insert(ch_t *ch, ckpt_mldlist_t *mldlist)
{
	mldlist->mld_next = ch->ch_mldlist;
	ch->ch_mldlist = mldlist;
}

static ckpt_mldlist_t *
ckpt_mldlink_lookup(ch_t *ch, caddr_t id)
{
	ckpt_mldlist_t *mldlist;

	for (mldlist = ch->ch_mldlist; mldlist; mldlist = mldlist->mld_next)
		if (mldlist->mld_id == id)
			return (mldlist);

	return (NULL);
}

/* 
 * check if p1 and p2 are depdent on each other.
 * return 1 if yes. 0 otherwise.
 */

static int ckpt_is_depdent(pid_t p1, pid_t p2)
{
	ckpt_dpl_t *dpl = NULL;
	int i;
	while (dpl=ckpt_get_depend(dpl)) {
		for(i=0; i<dpl->dpl_count; i++) {
			if(dpl->dpl_depend[0].dp_magic != CKPT_MAGIC_DEPEND)
				return 0;
			if(dpl->dpl_depend[i].dp_spid == p1 && 
			   dpl->dpl_depend[i].dp_dpid == p2)
				return 1;
			else if(dpl->dpl_depend[i].dp_spid == p2 && 
			        dpl->dpl_depend[i].dp_dpid == p1)
				return 1;
		}
	}
	return 0;
}

/*
 * to find out the mld this seg to follow (to migrate to after restart):
 *  Condition:
 *   1. the same cnodeid.
 *   2. mld_link_proc and pmi_owner (pid) must depdent on each other.
 */

static ckpt_mldlist_t *
ckpt_mldlink_pmi_lookup(ch_t *ch, ckpt_pmilist_t *pmilist)
{
	ckpt_mldlist_t *mldlist;
	for (mldlist = ch->ch_mldlist; mldlist; mldlist = mldlist->mld_next) {
		if ((mldlist->mld_nodeid == pmilist->pmi_nodeid) && 
	    	    (mldlist->mld_link_proc > 0) && 
    		    ckpt_is_depdent(mldlist->mld_link_proc, pmilist->pmi_owner))
			return (mldlist);
	}

	return (NULL);
}

/*
 * Lists to maintain pmolist.
 */
static void
ckpt_pmolist_insert(ch_t *ch, ckpt_pmolist_t *pmolist)
{
	pmolist->pmo_next = ch->ch_pmolist;
	ch->ch_pmolist = pmolist;
}

static ckpt_pmolist_t *
ckpt_pmolist_lookup(ch_t *ch, pmo_handle_t pmo_handle)
{
	ckpt_pmolist_t *pmolist;

	for (pmolist = ch->ch_pmolist; pmolist; pmolist = pmolist->pmo_next)
		if (pmolist->pmo_handle == pmo_handle)
			return (pmolist);

	return (NULL);
}

static void ckpt_pmi_insert(ch_t *ch, ckpt_pmilist_t *pmilist)
{
	pmilist->pmi_next = ch->ch_pmilist;
	ch->ch_pmilist = pmilist;
	IFDEB1(cdebug("ckpt_pmi_insert: owner %d base %0llx vaddr %llx"
			" eaddr %llx\n", pmilist->pmi_owner, pmilist->pmi_base, 
			pmilist->pmi_vaddr, pmilist->pmi_eaddr));
}

