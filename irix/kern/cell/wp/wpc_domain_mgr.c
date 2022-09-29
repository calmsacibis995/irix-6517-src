/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: wpc_domain_mgr.c,v 1.12 1997/10/07 20:51:49 sp Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/cell/wp.h>
#include "wp_private.h"
#include "wp_domain_mgr.h"
#include "wp_domain_svr.h"
#include "invk_wpds_stubs.h"

mrlock_t	wpd_list_lock;
wp_domain_t	wpd_list;
service_t	wp_domain_mgr_svc;

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_wpds_stubs.h"
#include "I_wpsc_stubs.h"
#include "I_wpss_stubs.h"

void wpd_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *);
void wpd_tcif_recall(tkc_state_t, void *, tk_set_t, tk_set_t, tk_disp_t);

tkc_ifstate_t wpd_tclient_iface = {
	wpd_tcif_obtain,
	wpd_tcif_recall
};

#ifdef	DEBUG
struct ktrace	*wp_trace;
long		wp_trace_id;

void idbg_wp_trace(__psint_t);
void idbg_wpdomain(__psint_t);
void idbg_wpentry(__psint_t);

char *wp_domain_names[] = {
	"INVALID NAME",
	"SHM_WP_KEYDOMAIN",
	"SHM_WP_IDDOMAIN",
	"PID_WP_DOMAIN",
	"HOST_WP_DOMAIN",
	"CXFS_WP_SDOMAIN",
	"CXFS_WP_MDOMAIN"
};
#endif

/*
 * Initialize white pages for a cell
 */
void
wp_init(void)
{
#ifdef	DEBUG
	idbg_addfunc("wptrace", idbg_wp_trace);
	idbg_addfunc("wpdomain", idbg_wpdomain);
	idbg_addfunc("wpentry", idbg_wpentry);
	wp_trace =  ktrace_alloc(1000, 0);
	wp_trace_id = -1;
#endif
	wpd_list = WP_DOMAIN_NULL;
	mrinit(&wpd_list_lock, "wp_domain");
	SERVICE_MAKE(wp_domain_mgr_svc, golden_cell, SVC_WPD_MGR);

	mesg_handler_register(wpds_msg_dispatcher, WPDS_SUBSYSID);
	mesg_handler_register(wpss_msg_dispatcher, WPSS_SUBSYSID);
	mesg_handler_register(wpsc_msg_dispatcher, WPSC_SUBSYSID);
}

int
wp_domain_lookup_local(
	ulong_t		domain,
	wp_domain_t	*domainid)
{
	wp_domain_t	wpd;

	mrlock(&wpd_list_lock, MR_ACCESS, PZERO);
	for (wpd = wpd_list; wpd != WP_DOMAIN_NULL; wpd = wpd->wpd_next) {
		if (wpd->wpd_name == domain) {
			*domainid = wpd;
			WP_KTRACE4("wp_domain_lookup", domain, "found", wpd);
			mrunlock(&wpd_list_lock);
			return(0);
		}
	}
	mrunlock(&wpd_list_lock);
	*domainid = WP_DOMAIN_NULL;
	return(EINVAL);
}

int
wp_domain_lookup(
	ulong_t		domain,
	ulong_t		domain_type,
	wp_domain_t	*domainid)
{
	wp_domain_t	wpd;
	service_t	wp_domain_svr;
	wp_domain_t	new_domain;
	int		result;

retry:
	mrlock(&wpd_list_lock, MR_ACCESS, PZERO);
	for (wpd = wpd_list; wpd != WP_DOMAIN_NULL; wpd = wpd->wpd_next) {
		if (wpd->wpd_name == domain) {
			if (wpd->wpd_type != domain_type) {
				mrunlock(&wpd_list_lock);
				*domainid = WP_DOMAIN_NULL;
				return(EINVAL);
			}
			*domainid = wpd;
			WP_KTRACE4("wp_domain_create", domain, "found", wpd);
			mrunlock(&wpd_list_lock);
			return(0);
		}
	}
	mrunlock(&wpd_list_lock);

	/*
	 * This domain is not known about locally, check with the server
	 */
	if (cellid() == SERVICE_TO_CELL(wp_domain_mgr_svc)) {
		wpds_domain_create(domain, domain_type, cellid(), &result,
					&wp_domain_svr);
	} else {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_wpds_domain_create(wp_domain_mgr_svc, domain,
					domain_type, cellid(), &result,
					&wp_domain_svr);
		ASSERT(!msgerr);
	}

	WP_KTRACE6("wp_domain_create", domain, "svr result", result, "svr",
			SERVICE_TO_WP_VALUE(wp_domain_svr));

	if (result == WP_AGAIN)
		goto retry;
	ASSERT(result == WP_FOUND);

	mrlock(&wpd_list_lock, MR_UPDATE, PZERO);
	for (wpd = wpd_list; wpd != WP_DOMAIN_NULL; wpd = wpd->wpd_next) {
		/*
		 * Someone else got there first
		 */
		if (wpd->wpd_name == domain) {
			if (wpd->wpd_type != domain_type) {
				mrunlock(&wpd_list_lock);
				*domainid = WP_DOMAIN_NULL;
				return(EINVAL);
			}
			*domainid = wpd;
			WP_KTRACE4("wp_domain_create", domain,
					"didn't create, found", wpd);
			mrunlock(&wpd_list_lock);
			return(0);
		}
	}

	new_domain = kern_malloc(sizeof(struct wp_domain));
	ASSERT(new_domain);
	mrinit(&new_domain->wpd_lock, "wp_domain");
	new_domain->wpd_name = domain;
	new_domain->wpd_type = domain_type;
	wpc_domain_svr_init(new_domain);
	new_domain->wpd_service = wp_domain_svr;
	tkc_create("wpd", new_domain->wpd_tclient, new_domain,
			&wpd_tclient_iface, WPD_NTOKENS,
			WPD_EXISTENCE_TOKENSET, WPD_EXISTENCE_TOKENSET,
			(void *)(__psint_t)domain);
	new_domain->wpd_next = wpd_list;
	wpd_list = new_domain;
	mrunlock(&wpd_list_lock);
	WP_KTRACE4("wp_domain_create", domain, "created", new_domain);
	*domainid = new_domain;
	return(0);
}

/*
 ****************************************************************************
 * Token Callout funtions
 ****************************************************************************
 */

/* ARGSUSED */
void
wpd_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	why_returning,
	tk_set_t	*refused)
{
	panic("wpd_tcif_obtain");
}

/* ARGSUSED */
void
wpd_tcif_recall(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_recalled,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	panic("wpd_tcif_recall");
}

#ifdef	DEBUG
extern void pm_trace_print(ktrace_entry_t *);

void
idbg_wp_trace(__psint_t x)
{
	__psint_t	id;
	int		idx;
	int		count;

	if (x == -1) {
		qprintf("Displaying all entries\n");
		idx = -1;
		count = 0;
	} else if (x < 0) {
		idx = -1;
		count = (int)-x;
		qprintf("Displaying last %d entries\n", count);
	} else {
		id = x;
		if (id <= 0 || id > WP_ID_LAST) {
			qprintf("invalid domain %d\n", id);
			return;
		}
		idx = 1;
		count = 0;
		qprintf("Displaying entries for domain %d(%s)\n",
					x, wp_domain_names[x]);
	}
		
	ktrace_print_buffer(wp_trace, id, idx, count);
}

void
idbg_wpentry(__psint_t x)
{
	wp_entry_t	wpe;
	extern void	idbg_avlnode(avlnode_t *);

	if (x == -1 || x >= 0) {
		qprintf("wpentry <address>\n");
		return;
	}

	wpe = (wp_entry_t)x;
	qprintf("WP entry @ 0x%x base 0x%x limit 0x%x equiv 0x%x\n",
		wpe, wpe->wp_base, wpe->wp_limit, wpe->wp_equiv);
	qprintf("\towner 0x%x token client @ 0x%x\n",
		(unsigned)wpe->wp_owner, &wpe->wp_tclient);
	idbg_avlnode(&wpe->wp_list);
}

void
idbg_wpdomain(__psint_t x)
{
	wp_domain_t	wpd;
	wp_entry_t	entry;

	if (x == -1 || x >= 0) {
		qprintf("wpdomain <address>\n");
		return;
	}

	wpd = (wp_domain_t)x;
	qprintf("WP domain name %s(0x%x) next 0x%x service 0x%x\n",
		wp_domain_names[wpd->wpd_name], wpd->wpd_name, wpd->wpd_next,
		SERVICE_TO_WP_VALUE(wpd->wpd_service));
	qprintf("\ttoken client @ 0x%x mrlock 0x%x\n", &wpd->wpd_tclient,
		&wpd->wpd_lock);
	qprintf("\tavltree 0x%x\n", &wpd->wpd_entries);
	for (entry = (wp_entry_t)avl_firstino(&wpd->wpd_entries);
	     entry != NULL;
	     entry = (wp_entry_t)avl_nextino(&entry->wp_list))
		idbg_wpentry((__psint_t)entry);
}
#endif
