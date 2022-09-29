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
#ident "$Id: wps_domain_mgr.c,v 1.5 1997/10/07 20:51:54 sp Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <ksys/cell/wp.h>
#include "wp_private.h"
#include "wp_domain_mgr.h"
#include "wp_domain_svr.h"
#include "I_wpds_stubs.h"

void wpd_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
void wpd_tsif_recalled(void *, tk_set_t, tk_set_t);
void wpd_tsif_idle(void *, tk_set_t);

tks_ifstate_t wpd_tserver_iface = {
        wpd_tsif_recall,
        wpd_tsif_recalled,
        wpd_tsif_idle
};

typedef struct wp_domain_mgr {
	struct wp_domain	wpdm_domain;
	TKS_DECL(wpdm_tserver, WPD_NTOKENS);
} wp_domain_mgr_t;

void
wpds_domain_create(
	ulong_t		domain,
	ulong_t		domain_type,
	cell_t		sender,
	int		*result,
	service_t	*server)
{
	wp_domain_t	wpd;
	wp_domain_mgr_t	*domain_mgr;
	wp_domain_t	new_domain;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already;

	if (sender == cellid())
		*result = WP_AGAIN;
	else
		*result = WP_FOUND;

	mrlock(&wpd_list_lock, MR_UPDATE, PZERO);
	for (wpd = wpd_list; wpd != WP_DOMAIN_NULL; wpd = wpd->wpd_next) {
		if (wpd->wpd_name == domain) {
			if (wpd->wpd_type != domain_type) {
				*result = WP_ERROR;
				mrunlock(&wpd_list_lock);
				return;
			}
				
			*server = wpd->wpd_service;
			domain_mgr = (wp_domain_mgr_t *)wpd;
			mrunlock(&wpd_list_lock);
			goto token_out;
		}
	}
	/*
	 * there is no server for this domain. We have to create one.
	 */
	domain_mgr = kern_malloc(sizeof(wp_domain_mgr_t));
	ASSERT(domain_mgr);
	new_domain = &domain_mgr->wpdm_domain;
	mrinit(&new_domain->wpd_lock, "wp_domain");
	new_domain->wpd_name = domain;
	new_domain->wpd_type = domain_type;
	wpc_domain_svr_init(new_domain);
	SERVICE_MAKE(*server, sender, SVC_WPD_SVR);
	new_domain->wpd_service = *server;
	tks_create("wpd", domain_mgr->wpdm_tserver, domain_mgr,
			&wpd_tserver_iface, WPD_NTOKENS,
			(void *)(__psint_t)domain);
	tks_obtain(domain_mgr->wpdm_tserver, (tks_ch_t)cellid(), 
			WPD_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	tkc_create_local("wpd", new_domain->wpd_tclient,
			domain_mgr->wpdm_tserver, WPD_NTOKENS,
			granted, granted, (void *)(__psint_t)domain);
	new_domain->wpd_next = wpd_list;
	wpd_list = new_domain;
	mrunlock(&wpd_list_lock);

	if (sender == cellid()) {
		*result = WP_AGAIN;
		return;
	}

token_out:
	tks_obtain(domain_mgr->wpdm_tserver, (tks_ch_t)sender, 
			WPD_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	ASSERT(refused == TK_NULLSET);
	ASSERT((granted == WPD_EXISTENCE_TOKENSET) ||
		(already == WPD_EXISTENCE_TOKENSET));
	/*
	 * Indicate that there was a race and that someone else got
	 * there first
	 */
	if (already == WPD_EXISTENCE_TOKENSET)
		*result = WP_AGAIN;
}

/*
 ****************************************************************************
 * Server side message implementation
 ****************************************************************************
 */

void
I_wpds_domain_create(
	ulong_t		domain,
	ulong_t		domain_type,
	cell_t		sender,
	int		*result,
	service_t	*wp_domain_svr)
{
	wpds_domain_create(domain, domain_type, sender, result, wp_domain_svr);
}

/*
 ****************************************************************************
 * Token Callout funtions
 ****************************************************************************
 */

/* ARGSUSED */
void
wpd_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_recall,
	tk_disp_t	why)
{
	panic("wpd_tsif_recall");
}

/* ARGSUSED */
void
wpd_tsif_recalled(
	void		*obj,
	tk_set_t	recalled,
	tk_set_t	refused)
{
	panic("wpd_tsif_recalled");
}

/* ARGSUSED */
void
wpd_tsif_idle(
	void		*obj,
	tk_set_t	idle)
{
	panic("wpd_tsif_idle");
}
