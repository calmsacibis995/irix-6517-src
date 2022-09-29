/*
 * os/numa/aspm.c
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

#include <sys/types.h>
#include <ksys/as.h>
#include <os/as/as_private.h> /* XXX */
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/prctl.h>
#include <sys/idbgentry.h>
#include <sys/pfdat.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/capability.h>
#include <os/as/pmap.h>
#include <sys/mmci.h>
#include <ksys/pid.h>
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
#include "numa_init.h"

/*
 * The dynamic memory allocation zone for aspm's
 */
static zone_t* aspm_zone = 0;

/*
 * aspm_create can only be called from this module
 */
static aspm_t* aspm_create(policy_set_t* policy_set);

/*
 * Attach the new defaults to the address space, replacing the old defaults.
 */
static void aspm_attach_defaults(pas_t *, ppas_t *, mem_type_t, pm_t *, pm_t *);
static void aspm_attach_default_preglist(pas_t *, ppas_t *, preg_t *, mem_type_t, pm_t *, pm_t *);

/*
 * The aspm initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
aspm_init()
{
        ASSERT(aspm_zone == 0);
        aspm_zone = kmem_zone_init(sizeof(aspm_t), "aspm");
        ASSERT(aspm_zone);

        return (0);
}

/*
 * Create a per address space policy base object.
 * This structure is used to group all the data
 * needed to do policy management for an address
 * space:
 *
 * + pmo_ns:
 *   a name space where we place handles
 *   visible to users for all the policy
 *   management objects created on behalf of
 *   this address space.
 *
 * + pm_default_ref:
 *   a direct reference to the default policy module
 *   for this address space.
 *
 * + pm_default_handle:
 *   the handle associated with the default policy module.
 *   We keep it here in order to have it easily available
 *   and identifiable when users ask for the default handle.
 */
static aspm_t*
aspm_create(policy_set_t* policy_set)
{
        aspm_t* aspm;
        pmo_ns_t* pmo_ns;
        pm_t* pm_default_ref;
        pmo_handle_t pm_default_hdl;

        ASSERT(aspm_zone);
        
        aspm = kmem_zone_alloc(aspm_zone, KM_SLEEP);
        ASSERT(aspm);

        pmo_ns = pmo_ns_create(PMO_NS_MAXENTRIES);
	pmo_incref(pmo_ns, aspm);
        ASSERT(pmo_ns);

        pm_default_hdl = pmo_kernel_pm_create(policy_set,
                                              pmo_ns,
                                              &pm_default_ref);

        /*
         * When called from this place, pmo_kernel_pm_create
         * cannot fail. Below we assert this is the case.
         */
        ASSERT(!pmo_iserrorhandle(pm_default_hdl));
        ASSERT(pm_default_ref);
        
        aspm->pmo_ns = pmo_ns;
        aspm->pm_default_hdl[MEM_STACK] = pm_default_hdl;
        aspm->pm_default_hdl[MEM_TEXT] = pm_default_hdl;
        aspm->pm_default_hdl[MEM_DATA] = pm_default_hdl;

        aspm->pm_default_ref[MEM_STACK] = pm_default_ref;
        pmo_incref(pm_default_ref, aspm);
        aspm->pm_default_ref[MEM_DATA] = pm_default_ref;
        pmo_incref(pm_default_ref, aspm);
        aspm->pm_default_ref[MEM_TEXT] = pm_default_ref;
        pmo_incref(pm_default_ref, aspm);
        spinlock_init(&aspm->lock, "aspm");

        
        return (aspm);
}

aspm_t *
aspm_dup(aspm_t *aspm)
{

	aspm_t *new_aspm;
	pm_t	*pm_default_ref;

        ASSERT(aspm_zone);
        
        new_aspm = kmem_zone_alloc(aspm_zone, KM_SLEEP);
        ASSERT(new_aspm);

	new_aspm->pmo_ns = aspm->pmo_ns;
	pmo_incref(aspm->pmo_ns, new_aspm);

        new_aspm->pm_default_hdl[MEM_TEXT] = aspm->pm_default_hdl[MEM_TEXT];
        new_aspm->pm_default_hdl[MEM_STACK] = aspm->pm_default_hdl[MEM_STACK];
        new_aspm->pm_default_hdl[MEM_DATA] = aspm->pm_default_hdl[MEM_DATA];

	pm_default_ref = aspm->pm_default_ref[MEM_STACK];
	pmo_incref(pm_default_ref, new_aspm);
        new_aspm->pm_default_ref[MEM_STACK] = pm_default_ref;

	pm_default_ref = aspm->pm_default_ref[MEM_TEXT];
	pmo_incref(pm_default_ref, new_aspm);
        new_aspm->pm_default_ref[MEM_TEXT] = pm_default_ref;

	pm_default_ref = aspm->pm_default_ref[MEM_DATA];
	pmo_incref(pm_default_ref, new_aspm);
        new_aspm->pm_default_ref[MEM_DATA] = pm_default_ref;

        spinlock_init(&new_aspm->lock, "aspm");
        
        return (new_aspm);
}
	
void
aspm_destroy(aspm_t* aspm)
{
        ASSERT (aspm);
        
        /*
         * The destruction of pmo_ns causes
         * the destruction of every pmo object
         * ever allocated for this aspm.
         * (It's really a decref that will usually
         * result in the destruction of the object
         * when done at this level).
         */
        pmo_decref(aspm->pm_default_ref[MEM_TEXT], aspm);
        pmo_decref(aspm->pm_default_ref[MEM_DATA], aspm);
        pmo_decref(aspm->pm_default_ref[MEM_STACK], aspm);
        pmo_decref(aspm->pmo_ns, aspm);
        spinlock_destroy(&aspm->lock);
        kmem_zone_free(aspm_zone, aspm);
}


/*
 * This method creates a new aspm for the process doing an exec,
 * sets the Memory Management Policies based on predefined defaults,
 * elf header information, enviroment variables, and replication state.
 */
struct aspm *
aspm_exec_create(struct vnode* vp, int is_mustrun)
{
        policy_set_t policy_set;
        aspm_t* aspm;
        
        /*
         * First we consider the default policy for the whole
         * address space. We have predefined default policies for
         * -- placement
         * -- fallback
         * -- migration
         * -- replication
         *
         * We also have defaults for
         * -- # of threads to be considered
         * -- radius for replication
         * -- preferred page size
         *
         * Each of these default policies and parameters  may be overriden via
         * elf header specifications or environment settings.
         */

        /*
         * Next we consider Policy Specifications for sections
         * of the address space (portion created at exec time).
         * This is done via information passed through the elf
         * header, and includes the following kinds of operations:
         * -- MLDs       mld1 size radius
         *               mld2 size radius
         * -- RAFFS      raff1 device radius
         *               raff2 device radius
         * -- MLDSETS    mldset1 topology mldlist rafflist adv/mand
         *               mldset2 topology mldlist rafflist adv/mand
         * -- PMs        pm1 placement fallback migration replication 
	 *		 page_size mldset
         *               pm2 placement fallback migration replication 
	 *		 page_size mldset
         * -- ATTACH     pm1 vaddr len
         *               pm2 vaddr len
         */

        /*
         * For now I'm just setting the predefined default:
         * -- PlacementDefault, 1 thread
         * -- FallbackDefault
         * -- ReplicationDefault
         * -- MigrationDefault
         * -- PagingDefault
         * -- _PAGESZ page size
         */

        if (is_mustrun) {
                /*
                 * This process is mustrun, use FirstTouch which
                 * will place the mld on the current node.
                 */
                policy_set.placement_policy_name = "PlacementFirstTouch";
                policy_set.placement_policy_args = NULL;
        } else {
                /*
                 * Just use the default, initialized for 1 thread.
                 */
                policy_set.placement_policy_name = "PlacementDefault";
                policy_set.placement_policy_args = (void*)1L;/* # of threads */
        }

        policy_set.fallback_policy_name = "FallbackDefault";
        policy_set.fallback_policy_args = NULL;        

        policy_set.replication_policy_name = "ReplicationDefault";
        policy_set.replication_policy_args = (void*)vp; /* vnode */

        policy_set.migration_policy_name = "MigrationDefault";
        policy_set.migration_policy_args = NULL;

        policy_set.paging_policy_name = "PagingDefault";
        policy_set.paging_policy_args = NULL;

        policy_set.page_size = _PAGESZ;
        policy_set.policy_flags = 0;
        policy_set.page_wait_timeout = 0;
        
        aspm = aspm_create(&policy_set);

        ASSERT(aspm);

        return (aspm);
}
        

/*
 * This method creates a new aspm for the init process.
 */
aspm_t *
aspm_base_create(void)
{
        policy_set_t policy_set;
        aspm_t* aspm;

        policy_set.placement_policy_name = "PlacementFirstTouch";
        policy_set.placement_policy_args = NULL;

        policy_set.fallback_policy_name = "FallbackDefault";
        policy_set.fallback_policy_args = NULL;        

        policy_set.replication_policy_name = "ReplicationOne";
        policy_set.replication_policy_args = NULL;

        policy_set.migration_policy_name = "MigrationDefault";
        policy_set.migration_policy_args = NULL;

        policy_set.paging_policy_name = "PagingDefault";
        policy_set.paging_policy_args = NULL;

        policy_set.page_size = _PAGESZ;
        policy_set.policy_flags = 0;
        policy_set.page_wait_timeout = 0;
        
        aspm = aspm_create(&policy_set);

        ASSERT(aspm);
	return aspm;
}


/*
 * The following methods are used when operating
 * on pregions.
 */

/*
 * This method acquires a reference to the default
 * pm for the address space represented by aspm.
 */
struct pm*
aspm_getdefaultpm_func(struct aspm* aspm, mem_type_t mem_type)
{
        pm_t* pm;
        int s;

        ASSERT(aspm);
        s = aspm_lock(aspm);
        pm = aspm->pm_default_ref[mem_type];
        ASSERT (pm);
        pmo_incref(pm, pmo_ref_keep);
        aspm_unlock(aspm, s);
        return (pm);
}

/*
 * This method acquires a reference to the default
 * pm for the address space represented by aspm.  It
 * assumes the pm will not disappear, and therefore
 * does not do an incref.
 */
struct pm*
aspm_getcurrentdefaultpm(struct aspm* aspm, mem_type_t mem_type)
{
        pm_t* pm;
#if DEBUG
	vasid_t vasid;

	as_lookup_current(&vasid);
#endif

	ASSERT(mrislocked_update(&(VASID_TO_PAS(vasid)->pas_aspacelock)));
        ASSERT(aspm);
        pm = aspm->pm_default_ref[mem_type];
        ASSERT (pm);
        return (pm);
}

/*
 * Fast routine to compare a pm against the default pm. 
 */
int
aspm_isdefaultpm_func(aspm_t *aspm, pm_t* pm, mem_type_t mem_type)
{
	ASSERT(pm);
        return (pm == aspm->pm_default_ref[mem_type]);
}

/*
 * return pmo for current AS
 */
pmo_ns_t *
curpmo_ns(void)
{
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	return pas->pas_aspm->pmo_ns;
}

/*
 * Public xface to convert pmo handles into kernel objects
 */
void*
curpmo_ns_find(pmo_handle_t pmo_handle, pmo_type_t pmo_type)
{
        return (pmo_ns_find(curpmo_ns(), pmo_handle, pmo_type));
}

/*
 * return pmo for arbitrary AS
 *
 * holds a ref on the address space
 */
pmo_ns_t *
getpmo_ns(asid_t asid)
{
	vasid_t vasid;
	pas_t *pas;

	if (as_lookup(asid, &vasid))
		return (NULL);
	pas = VASID_TO_PAS(vasid);
	return pas->pas_aspm->pmo_ns;
}

int
relpmo_ns(asid_t asid)
{
	vasid_t vasid;

	if (as_lookup(asid, &vasid))
		return -1;

	/*
	 * drop reference from previous getpmo_ns and current
	 * reference from as_lookup
	 */
	as_rele(vasid);
	as_rele(vasid);

	return 0;
}

/*
 * return aspm for arbitrary AS
 * NOTE: this takes a reference on the aspm, be careful to drop it!
 */
aspm_t *
getaspm(asid_t asid)
{
	vasid_t vasid;
	pas_t *pas;
	aspm_t *aspm;

	if (as_lookup(asid, &vasid))
		return NULL;
	pas = VASID_TO_PAS(vasid);
	aspm = pas->pas_aspm;
        pmo_incref(aspm, pmo_ref_keep);
	as_rele(vasid);
	return aspm;
}

/*
 * This xface method returns a handle to the default pm
 * for the calling process.
 */
pmo_handle_t
pmo_xface_aspm_getdefaultpm(mem_type_t mem_type)
{
        pmo_handle_t pm_handle;
        int s;
	vasid_t vasid;
	pas_t *pas;
	aspm_t *aspm;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	aspm = pas->pas_aspm;
        
        s = aspm_lock(aspm);
        pm_handle = aspm->pm_default_hdl[mem_type];
        aspm_unlock(aspm, s);
        return (pm_handle);
}

/*
 * This xface method returns the cnodemask
 * for the calling process.
 */
cnodemask_t 
pmo_xface_aspm_getcnodemask(void)
{
        cnodemask_t cnodemask;

	cnodemask = curthreadp->k_nodemask;

	return (cnodemask);
}
/*
 * This xface method sets the cnodemask
 * for the calling process.
 */
cnodemask_t 
pmo_xface_aspm_setcnodemask(cnodemask_t cnodemask)
{

	/* legalize mask */
	cnodemask_t tmp = CNODEMASK_BOOTED_MASK;
	ASSERT(CNODEMASK_IS_NONZERO(tmp));

	CNODEMASK_ANDM(cnodemask, tmp); 

	if ( !cap_able(CAP_MEMORY_MGT) ) {
		tmp = cnodemask;
		CNODEMASK_ANDM(tmp, curthreadp->k_nodemask);
		if ( CNODEMASK_NOTEQ(cnodemask, tmp) ) 
			CNODEMASK_CLRALL(cnodemask);
	}

	if (CNODEMASK_IS_NONZERO(cnodemask))
		curthreadp->k_nodemask = cnodemask;
	else 
		cnodemask = curthreadp->k_nodemask; /* do nothing for null mask */
        
        return (cnodemask);
}

/*
 * This xface method sets a new default pm for current process
 */
pmo_handle_t
pmo_xface_aspm_setdefaultpm(pmo_handle_t newpm_handle, mem_type_t mem_type)
{
        pm_t* newpm;
        pm_t* oldpm;
        aspm_t* aspm;
        int s;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;

        if ((uint)mem_type >= (uint)NUM_MEM_TYPES) {
                return (PMOERR_EINVAL);
        }
        
        if ((newpm = (pm_t*)pmo_ns_find(curpmo_ns(),
                                        newpm_handle,
                                        __PMO_PM)) == NULL) {
                return (PMOERR_INV_PM_HANDLE);
        }

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	aspm = pas->pas_aspm;

	mraccess(&pas->pas_aspacelock);
        s = aspm_lock(aspm);
        oldpm = aspm->pm_default_ref[mem_type];

        ASSERT (oldpm);
        pmo_decref(oldpm, aspm);
        aspm->pm_default_ref[mem_type] = newpm;
        aspm->pm_default_hdl[mem_type] = newpm_handle;


        /*
         * Acquire new ref for aspm, and release find ref
         * Note that we have to do this strictly unnecesary
         * incr and decr to keep the debugging trace sane.
         */
        pmo_incref(newpm, aspm);
        pmo_decref(newpm, pmo_ref_find);
        
        aspm_unlock(aspm, s);
	aspm_attach_defaults(pas, ppas, mem_type, oldpm, newpm);
	mrunlock(&pas->pas_aspacelock);

        return (0);
}

/*
 * Attach the new defaults to the address space replacing the old defaults.
 */
static void
aspm_attach_defaults(
	pas_t *pas,
	ppas_t *ppas,
	mem_type_t mem_type,
	pm_t *old_pm,
	pm_t *new_pm)
{
        preg_t *prp;

	prp = PREG_FIRST(ppas->ppas_pregions);

	aspm_attach_default_preglist(pas, ppas, prp, mem_type, old_pm, new_pm);

	prp = PREG_FIRST(pas->pas_pregions);
	aspm_attach_default_preglist(pas, ppas, prp, mem_type, old_pm, new_pm);
}

/* ARGSUSED */
static void
aspm_attach_default_preglist(
	pas_t *pas,
	ppas_t *ppas,
	preg_t *prp, 
	mem_type_t mem_type,
	pm_t *old_pm,
	pm_t *new_pm)
{
        attr_t* 	attr;

	for (; prp ;prp = PREG_NEXT(prp)) {
		reglock(prp->p_reg);

		if (((prp->p_type == PT_STACK) && mem_type != MEM_STACK) ||
			((prp->p_reg->r_flags & RG_TEXT) && mem_type != MEM_TEXT)) {

			regrele(prp->p_reg);
			continue;
		}

		if ((prp->p_type != PT_STACK) && !(prp->p_reg->r_flags & RG_TEXT) && 
			(mem_type != MEM_DATA)) {

			regrele(prp->p_reg);
			continue;
		}

		attr = &prp->p_attrs;
		while (attr != NULL) {
			if (attr_pm_get(attr) == old_pm) {

				if ((PMAT_GET_PAGESIZE(attr) > NBPP) && 
						(PMAT_GET_PAGESIZE(attr) != 
						PM_GET_PAGESIZE(new_pm)))
					pmap_downgrade_range(pas, ppas, prp, 
							attr->attr_start, 
							attr->attr_end -
							attr->attr_start);
				attr_pm_unset(attr);
				attr_pm_set(attr, new_pm);
			}
			attr = attr->attr_next;
		}


		regrele(prp->p_reg);
	}
}
#ifdef CKPT
void
aspm_ckpt_swap(void)
{
	vasid_t vasid;
	pas_t *pas;
	aspm_t *oaspm, *naspm;
	pmo_ns_t *pmo_ns;
	mem_type_t mtype;

	as_lookup_current(&vasid);
	VAS_LOCK(vasid, AS_EXCL);

	pas = VASID_TO_PAS(vasid);

	naspm = kmem_zone_alloc(aspm_zone, KM_SLEEP);
	ASSERT(naspm);

	spinlock_init(&naspm->lock, "aspm");

	oaspm = pas->pas_aspm;

	pmo_ns = pmo_ns_create(PMO_NS_MAXENTRIES);
	naspm->pmo_ns = pmo_ns;
	pmo_incref(pmo_ns, naspm);
	/* 
	 * Transfer refs to policy modules
	 */
	for (mtype = MEM_STACK; mtype < NUM_MEM_TYPES; mtype++) {
		pm_t *pm;

		pm = oaspm->pm_default_ref[mtype];

		pmo_incref(pm, oaspm);
		naspm->pm_default_ref[mtype] = pm;

		naspm->pm_default_hdl[mtype] =
				pmo_ns_pmo_lookup(naspm->pmo_ns, __PMO_PM, pm);

		if (naspm->pm_default_hdl[mtype] == PMOERR_ESRCH)  {
			/*
			 * Not already entered
			 */
			naspm->pm_default_hdl[mtype] =
				pmo_ns_insert( naspm->pmo_ns, __PMO_PM, pm);

			ASSERT(naspm->pm_default_hdl[mtype] != PMOERR_ESRCH);
		}
	}

	pas->pas_aspm = naspm;

	VAS_UNLOCK(vasid);

	aspm_destroy(oaspm);
}
#endif
#ifdef NUMA_BASE

void
aspm_relocate(aspm_t* aspm, cnodeid_t dest_node)
{
        pmo_ns_t* pmo_ns;
        pmo_ns_entry_t* entry;        
        pmo_ns_it_t it;
        int ns_ospl;

        /*
         * This procedure is intended to be called in
         * such a way that aspm cannot be destroyed
         * while executing it.
         */
        
        ASSERT(aspm);
        pmo_ns = aspm->pmo_ns;
        ASSERT(pmo_ns);

        ns_ospl = pmo_ns_lock(pmo_ns);
        
        /*
         * A first loop to unplace all mlds
         */
        pmo_ns_it_init(&it, pmo_ns);
        while ((entry = pmo_ns_it_getnext(&it)) != NULL) {
                if (pmo_gettype(entry->pmo) == __PMO_MLD) {
                        mld_t* mld = (mld_t*)entry->pmo;
                        mld_setunplaced(mld);
                }
        }

        /*
         * A second loop to relocate all mldsets
         */
        pmo_ns_it_init(&it, pmo_ns);
        while ((entry = pmo_ns_it_getnext(&it)) != NULL) {
                if (pmo_gettype(entry->pmo) == __PMO_MLDSET) {
                        mldset_t* mldset = (mldset_t*)entry->pmo;
                        ASSERT(mldset);
                        mldset_relocate(mldset);
                }
        }

        /*
         * A third loop to relocate all mlds not in any mldset
         * and therefore has not already been relocated
         */
        pmo_ns_it_init(&it, pmo_ns);
        while ((entry = pmo_ns_it_getnext(&it)) != NULL) {
                if (pmo_gettype(entry->pmo) == __PMO_MLD) {
                        mld_t* mld = (mld_t*)entry->pmo;
                        ASSERT(mld);
                        if (!mld_isplaced(mld)) {
                                mld_relocate(mld, dest_node);
                        }
                }
        }
        
        pmo_ns_unlock(pmo_ns, ns_ospl); 
}

#endif /* NUMA_BASE */
