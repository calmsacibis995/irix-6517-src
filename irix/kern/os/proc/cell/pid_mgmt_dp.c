/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.19 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/var.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/cell/service.h>
#include <ksys/cell/wp.h>
#include <ksys/cell.h>
#include "pid_private.h"
#include "vproc_private.h"
#include "proc_trace.h"
#include "invk_dpid_stubs.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dpid_stubs.h"

#define	PID_RECYCLE_FACTOR	2048

static wp_domain_t	pid_wp_domain;

void
pid_init(void)
{
	service_t	pm_service;
	int		error;
	unsigned long	base;

	SERVICE_MAKE(pm_service, cellid(), SVC_PROCESS_MGMT);
	if (wp_domain_create(PID_WP_DOMAIN, &pid_wp_domain))
		panic("no pid wp domain");

	pidtabsz = v.v_proc;
	pid_wrap = pidtabsz * PID_RECYCLE_FACTOR;

	error = wp_allocate(pid_wp_domain, (ulong_t)pid_wrap,
			    SERVICE_TO_WP_VALUE(pm_service), &base);
	if (error)
		panic("no pids");

	pid_base = (pid_t)base;
	pidtab_init();

	mesg_handler_register(dpid_msg_dispatcher, DPID_SUBSYSID);
}

int
pid_is_local(
	pid_t	pid)
{
#ifdef	USEWP
	wp_value_t	value;
	service_t	pm_service;
	service_t	svc;

	if (wp_lookup(pid_wp_domain, pid, &value))
		panic("pid_is_local: no white pages");
	
	SERVICE_MAKE(pm_service, cellid(), SVC_PROCESS_MGMT);
	SERVICE_FROMWP_VALUE(svc, value);
	return(SERVICE_EQUAL(svc, pm_service));
#else
	if ((pid >= pid_base) && (pid < (pid_base + pid_wrap)))
		return(1);
	else
		return(0);
#endif
}

service_t
pid_to_service(
	pid_t	pid)
{
	wp_value_t	value;
	service_t	svc;

	if (wp_lookup(pid_wp_domain, pid, &value))
		SERVICE_MAKE_NULL(svc);
	else
		SERVICE_FROMWP_VALUE(svc, value);
	return(svc);
}

vproc_t *
pid_to_vproc_locked(
	pid_t	pid)
{
	vproc_t		*vp;
	pid_slot_t	*ps;
	pid_entry_t	*pe;
	int		spl;

	if (pid < 0 || pid > MAXPID)
		return 0;

	ps = PID_SLOT(pid);

	spl = pidslot_lock(ps);

	if (!pidslot_isactive(ps)) {
		pidslot_unlock(ps, spl);
		return(VPROC_NULL);
	}

	for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
		if (pe->pe_pid == pid) {
			int	held;
			int	need_dealloc = 0;

			vp = pe->pe_vproc;
			if (!vp) {
				/* The pid happens to be in the pid table,
				 * but no proc associated with it. No need
				 * to look more.
				 */
				break;
			}

			VPROC_NESTED_REFCNT_LOCK(vp);
			if (vp->vp_refcnt > 0) {
				pe->pe_nodealloc++;
				need_dealloc = 1;
			}
			pidslot_nested_unlock(ps);

			held = vproc_reference_exclusive(vp, &spl);

			VPROC_REFCNT_UNLOCK(vp, spl);

			if (need_dealloc) {
				spl = pidslot_lock(ps);
				pe->pe_nodealloc--;
				if ((pe->pe_nodealloc == 0) &&
				     pe->pe_dealloc) {
					pidslot_unlock(ps, spl);
					sv_broadcast(&pid_dealloc_sv);
				} else
					pidslot_unlock(ps, spl);
			}

			if (held) {
				PROC_TRACE8(PROC_TRACE_REFERENCE,
				      "pid_to_vproc_locked", vp->vp_pid,
				      "by", current_pid(),
				      "refcnt", vp->vp_refcnt,
				      "\n\tcalled from", __return_address);
				return(vp);
			} else
				return(VPROC_NULL);
		}
	}

	pidslot_unlock(ps, spl);
	return(VPROC_NULL);
}

int
pid_getlist(
        pidord_t *next,
        size_t *count,
        pid_t *pidbuf,
        pidord_t *ordbuf)
{
	service_t	svc;

        /*
         * Convert our starting pid ordinal to the service on which the 
         * corresponding pid is located.  Note that we can pass a pid
         * ordinal to pid_to_service since the mapping between pid ordinals
         * and pid's is set of permutations, one for each cell.  That is, the
         * inverse image of a given cell under the pid_ordinal to cell
         * mapping is the same as that under the pid to cell mapping.
         */
	svc = pid_to_service(*next);
        if (SERVICE_IS_NULL(svc)) {
                ulong_t base;
                ulong_t range;
                wp_value_t wpval;

		base = *next;
                range = 1;
        	if (wp_getnext(pid_wp_domain, &base, &range, &wpval)) {
			 *count = 0;
			return 1;
		}
		SERVICE_FROMWP_VALUE(svc, wpval);
	}

	/*
         * Now that we have the service, do the right thing.
	 */
	if (cellid() == SERVICE_TO_CELL(svc)) {
                if (pid_getlist_local(*next, count, pidbuf, ordbuf))
		        *next = pid_base + pid_wrap;
		else if (*count != 0) 
                	*next = *(ordbuf + (*count - 1));
	}
	else {
		/* REFERENCED */
		int	msgerr;
                size_t	count2 = *count;
                size_t	count3 = *count;

                msgerr = invk_dpid_getlist(svc, next, count, 
				  &pidbuf, &count2, &ordbuf, &count3);
		ASSERT(!msgerr);
		ASSERT(count2 == count3);
		ASSERT(*count == count2 ||
		       (*count == 0 && count2 == 1));
	}
	return 0;
}

