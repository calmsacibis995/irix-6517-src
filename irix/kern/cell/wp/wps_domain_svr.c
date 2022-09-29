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
#ident "$Id: wps_domain_svr.c,v 1.8 1997/10/07 20:51:56 sp Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/cell/wp.h>
#include "wp_domain_mgr.h"
#include "wp_domain_svr.h"
#include "wp_private.h"
#include "invk_wpsc_stubs.h"

void wp_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
void wp_tsif_recalled(void *, tk_set_t, tk_set_t);
void wp_tsif_idle(void *, tk_set_t);

tks_ifstate_t wp_tserver_iface = {
	wp_tsif_recall,
	wp_tsif_recalled,
	wp_tsif_idle
};

typedef struct wp_entry_mgr {
	struct wp_entry		wpem_entry;
	wp_domain_t		wpem_domain;
	TKS_DECL(wpem_tserver, WP_NTOKENS);
} wp_entry_mgr_t;

void
wpss_register(
	wp_domain_t	wpd,
	cell_t		sender,
	wp_instance_t	*instance,
	wp_value_t	equiv,
	wp_instance_t	*oinstance,
	wp_value_t	*oequivp,
	cell_t		*ownerp,
	int		*result)
{
	avlnode_t	*avln;
	wp_entry_t	wpe;
	wp_entry_mgr_t	*wpem;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already;

	mrlock(&wpd->wpd_lock, MR_UPDATE, PZERO);
	avln = avl_findanyrange(&wpd->wpd_entries, instance->wpi_base,
				instance->wpi_limit + 1, 0);
	if (avln != 0) {
		wpem = (wp_entry_mgr_t *)avln;
		wpe = &wpem->wpem_entry;
		*oinstance = wpe->wp_instance;
		*oequivp = wpe->wp_equiv;
		*result = WP_FOUND;
		goto token_out;
	}
	wpem = kern_malloc(sizeof(struct wp_entry_mgr));
	ASSERT(wpem);
	wpem->wpem_domain = wpd;
	wpe = &wpem->wpem_entry;
	wpe->wp_instance = *instance;
	wpe->wp_equiv = equiv;
	wpe->wp_owner = sender;
	tks_create("wpe", wpem->wpem_tserver, wpem, &wp_tserver_iface,
			WP_NTOKENS, (void *)(__psint_t)wpd->wpd_name);
	tks_obtain(wpem->wpem_tserver, (tks_ch_t)cellid(),
			WP_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	tkc_create_local("wpe", wpe->wp_tclient, wpem->wpem_tserver,
			WP_NTOKENS, granted, granted,
			(void *)(__psint_t)wpd->wpd_name);
	avl_insert(&wpd->wpd_entries, &wpe->wp_list);
	*result = WP_NOTFOUND;

token_out:
	*ownerp = wpe->wp_owner;
	if (sender == cellid()) {
		WP_KTRACE10("wpss_register", wpd->wpd_name, "svr returns",
			*result, "\n\tbase", wpe->wp_base, "limit",
			wpe->wp_limit, "equiv", wpe->wp_equiv);
		mrunlock(&wpd->wpd_lock);
		return;
	}
	tks_obtain(wpem->wpem_tserver, (tks_ch_t)sender,
			WP_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	WP_KTRACE10("wpss_register", wpd->wpd_name, "svr returns", *result,
		"\n\tbase", wpe->wp_base, "limit", wpe->wp_limit, "equiv",
		wpe->wp_equiv);
	mrunlock(&wpd->wpd_lock);
	ASSERT(refused == TK_NULLSET);
	ASSERT(already == TK_NULLSET);
	ASSERT(granted == WP_EXISTENCE_TOKENSET);
}

/*
 * Look for an entry in the server. Returns;
 *	WP_NOTFOUND - entry is not present
 *	WP_FOUND - entry found, client should cache
 *	WP_AGAIN - Entry was found, client should not cache
 */
void
wpss_lookup(
	wp_domain_t	domain,
	cell_t		sender,
	ulong_t		value,
	wp_instance_t	*oinstance,
	wp_value_t	*oequivp,
	cell_t		*ownerp,
	int		*result)
{
	avlnode_t	*avln;
	wp_entry_t	wpe;
	wp_entry_mgr_t	*wpem;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already;

	/*
	 * If the caller is us, then there is no point in looking through
	 * our database again if we didn't find it the first time
	 */
	if (sender == cellid()) {
		*result = WP_NOTFOUND;
		return;
	}

	mrlock(&domain->wpd_lock, MR_ACCESS, PZERO);
	avln = avl_findrange(&domain->wpd_entries, value);
	if (avln == NULL) {
		mrunlock(&domain->wpd_lock);
		*result = WP_NOTFOUND;
		return;
	}
	wpem = (wp_entry_mgr_t *)avln;
	wpe = &wpem->wpem_entry;
	*oinstance = wpe->wp_instance;
	*oequivp = wpe->wp_equiv;
	*ownerp = wpe->wp_owner;

	tks_obtain(wpem->wpem_tserver, (tks_ch_t)sender,
			WP_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	mrunlock(&domain->wpd_lock);

	ASSERT(refused == TK_NULLSET);
	ASSERT((granted == WP_EXISTENCE_TOKENSET) ||
		(already == WP_EXISTENCE_TOKENSET));
	/*
	 * Indicate that there was a race and that someone else got
	 * there first
	 */
	if (already == WP_EXISTENCE_TOKENSET)
		*result = WP_AGAIN;
	else
		*result = WP_FOUND;
}

void
wpss_allocate(
	wp_domain_t	domain,
	cell_t		sender,
	ulong_t		range,
	wp_value_t	equiv,
	ulong_t		*basep,
	int		*result)
{
	wp_entry_mgr_t	*entry;
	wp_entry_mgr_t	*wpem;
	wp_entry_t	wpe;
	ulong_t		base;
	ulong_t		limit;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already;

	wpem = (wp_entry_mgr_t *)kern_malloc(sizeof(struct wp_entry_mgr));
	ASSERT(wpem);
	wpem->wpem_domain = domain;
	wpe = &wpem->wpem_entry;
	wpe->wp_equiv = equiv;
	wpe->wp_owner = sender;
	tks_create("wpe", wpem->wpem_tserver, wpem, &wp_tserver_iface,
			WP_NTOKENS, (void *)(__psint_t)domain->wpd_name);
	tks_obtain(wpem->wpem_tserver, (tks_ch_t)cellid(),
			WP_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	tkc_create_local("wpe", wpe->wp_tclient, wpem->wpem_tserver,
			WP_NTOKENS, granted, granted,
			(void *)(__psint_t)domain->wpd_name);

	base = 0;
	limit = range - 1;

	mrlock(&domain->wpd_lock, MR_UPDATE, PZERO);
	for (entry = (wp_entry_mgr_t *)avl_firstino(&domain->wpd_entries);
	     entry != NULL;
	     entry = (wp_entry_mgr_t *)avl_nextino(&entry->wpem_entry.wp_list)) {
		if (limit < entry->wpem_entry.wp_base)
			break;
		base = entry->wpem_entry.wp_limit + 1;
		limit = base + range - 1;
		/* XXX check for wrap */
	}

	wpe = &wpem->wpem_entry;
	wpe->wp_base = base;
	wpe->wp_limit = limit;
	avl_insert(&domain->wpd_entries, &wpe->wp_list);
	*basep = base;
	*result = WP_FOUND;
	if (sender == cellid()) {
		mrunlock(&domain->wpd_lock);
		return;
	}
	tks_obtain(wpem->wpem_tserver, (tks_ch_t)sender,
			WP_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	mrunlock(&domain->wpd_lock);
	ASSERT(refused == TK_NULLSET);
	ASSERT(already == TK_NULLSET);
	ASSERT(granted == WP_EXISTENCE_TOKENSET);
}

void
wpss_iterate(
	wp_domain_t	domain,
	cell_t		sender,
	ulong_t		*basep,
	ulong_t		*limitp,
	wp_value_t	*equiv,
	cell_t		*ownerp,
	int		*result)
{
	avlnode_t	*avln;
	wp_entry_t	wpe;
	wp_entry_mgr_t	*wpem;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already;

	mrlock(&domain->wpd_lock, MR_ACCESS, PZERO);
	avln = avl_findadjacent(&domain->wpd_entries, *basep, AVL_SUCCEED);
	/* we have the entry that *basep is in, we want the next one */
	if ((avln == NULL) || ((avln = avl_nextino(avln)) == NULL)) {
		mrunlock(&domain->wpd_lock);
		*result = WP_NOTFOUND;
		return;
	}
	wpem = (wp_entry_mgr_t *)avln;
	wpe = &wpem->wpem_entry;
	*basep = wpe->wp_base;
	*limitp = wpe->wp_limit;
	*equiv = wpe->wp_equiv;
	*ownerp = wpe->wp_owner;

	tks_obtain(wpem->wpem_tserver, (tks_ch_t)sender,
			WP_EXISTENCE_TOKENSET, &granted, &refused,
			&already);
	mrunlock(&domain->wpd_lock);

	ASSERT(refused == TK_NULLSET);
	ASSERT((granted == WP_EXISTENCE_TOKENSET) ||
		(already == WP_EXISTENCE_TOKENSET));
	if (already == WP_EXISTENCE_TOKENSET)
		*result = WP_AGAIN;
	else
		*result = WP_FOUND;
}

void
wpem_token_return(
	void		*obj,
	tks_ch_t	sender,
	tk_set_t	granted,
	tk_set_t	recalled)
{
	wp_entry_mgr_t	*wpem;

	ASSERT(recalled == TK_NULLSET);
	ASSERT(granted == WP_EXISTENCE_TOKENSET);
	wpem = (wp_entry_mgr_t *)obj;
	tks_return(wpem->wpem_tserver, (tks_ch_t)sender,
		granted, TK_NULLSET, TK_NULLSET, TK_CLIENT_INITIATED);
}

void
wpem_destroy(
	wp_entry_mgr_t	*wpem,
	cell_t		sender)
{
	wp_entry_t	wpe;
	tk_set_t	refused;

	wpe = &wpem->wpem_entry;
	tkc_release(wpe->wp_tclient, WP_EXISTENCE_TOKENSET);
	if (sender != cellid())
		tks_state(wpem->wpem_tserver, (tks_ch_t)sender,
						wpem_token_return);
	tks_recall(wpem->wpem_tserver, WP_EXISTENCE_TOKENSET, &refused);
	ASSERT(refused == TK_NULLSET);
	tkc_destroy_local(wpe->wp_tclient);
	tks_destroy(wpem->wpem_tserver);
	kmem_free(wpem, sizeof(struct wp_entry_mgr));
}

void
wpss_clear(
	wp_domain_t	wpd,
	cell_t		sender,
	ulong_t		base,
	ulong_t		limit,
	int		*result)
{
	ulong_t		start;
	avlnode_t	*avln;
	wp_entry_mgr_t	*wpem;
	wp_entry_t	wpe;
	int		deleted;

	start = base;
	deleted = 0;

	WP_KTRACE6("wpss_clear", wpd->wpd_name, "base", base, "limit", limit);
	mrlock(&wpd->wpd_lock, MR_UPDATE, PZERO);
	for (;;) {
		avln = avl_findadjacent(&wpd->wpd_entries, start,
					AVL_SUCCEED);
		if (avln == NULL)
			break;
		wpem = (wp_entry_mgr_t *)avln;
		wpe = &wpem->wpem_entry;
		if ((base <= wpe->wp_base) && wpe->wp_limit <= limit) {
			avl_delete(&wpd->wpd_entries, &wpe->wp_list);
			start = wpe->wp_limit + 1;
			WP_KTRACE6("wpem_destroy", wpd->wpd_name,
				   "base", wpe->wp_base,
				   "limit", wpe->wp_limit);
			mrunlock(&wpd->wpd_lock);
			wpem_destroy(wpem, sender);
			mrlock(&wpd->wpd_lock, MR_UPDATE, PZERO);
			deleted = 1;
		} else
			break;
	}
	mrunlock(&wpd->wpd_lock);
	*result = (deleted ? WP_FOUND : WP_NOTFOUND);
}

/*
 ****************************************************************************
 * Server side message implementation
 ****************************************************************************
 */

void
I_wpss_register(
	long		domain,
	cell_t		sender,
	wp_instance_t	*instance,
	wp_value_t	equiv,
	wp_instance_t	*oinstance,
	wp_value_t	*oequivp,
	cell_t		*ownerp,
	int		*result)
{
	wp_domain_t	wpd;

	wp_domain_lookup_local(domain, &wpd);

	wpss_register(wpd, sender, instance, equiv,
			oinstance, oequivp, ownerp, result);
}

void
I_wpss_lookup(
	long		domain,
	cell_t		sender,
	ulong_t		value,
	wp_instance_t	*instancep,
	wp_value_t	*equivp,
	cell_t		*ownerp,
	int		*result)
{
	wp_domain_t	wpd;

	wp_domain_lookup_local(domain, &wpd);
	wpss_lookup(wpd, sender, value, instancep, equivp, ownerp, result);
}

void
I_wpss_allocate(
	long		domain,
	cell_t		sender,
	ulong_t		range,
	wp_value_t	value,
	ulong_t		*basep,
	int		*result)
{
	wp_domain_t	wpd;

	wp_domain_lookup_local(domain, &wpd);

	wpss_allocate(wpd, sender, range, value, basep, result);
}

void
I_wpss_iterate(
	long		domain,
	cell_t		sender,
	ulong_t		*basep,
	ulong_t		*limitp,
	wp_value_t	*equiv,
	cell_t		*ownerp,
	int		*result)
{
	wp_domain_t	wpd;

	wp_domain_lookup_local(domain, &wpd);

	wpss_iterate(wpd, sender, basep, limitp, equiv, ownerp, result);
}

void
I_wpss_clear(
	long		domain,
	cell_t		sender,
	ulong_t		base,
	ulong_t		limit,
	int		*result)
{
	wp_domain_t	wpd;

	wp_domain_lookup_local(domain, &wpd);

	wpss_clear(wpd, sender, base, limit, result);
}

/*
 ****************************************************************************
 * Token Callout funtions
 ****************************************************************************
 */

/* ARGSUSED */
void
wp_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_recall,
	tk_disp_t	why)
{
	wp_entry_mgr_t	*wpem;
	wp_entry_t	wpe;
	service_t	svc;
	int		result;
	int		msgerr;

	wpem = (wp_entry_mgr_t *)obj;
	wpe = &wpem->wpem_entry;
	ASSERT(to_recall == WP_EXISTENCE_TOKENSET);
	if (client == cellid()) {
		tkc_recall(wpe->wp_tclient, to_recall, why);
		return;
	}
	SERVICE_MAKE(svc, (cell_t)client, SVC_WPD_SVR);
	msgerr = invk_wpsc_invalidate(svc, wpem->wpem_domain->wpd_name,
			wpe->wp_base, wpe->wp_limit, wpe->wp_equiv,
			&result);
	ASSERT(!msgerr);
	ASSERT(result != WP_AGAIN);
	if (result == WP_FOUND)
		tks_return(wpem->wpem_tserver, client, WP_EXISTENCE_TOKENSET,
				TK_NULLSET, TK_NULLSET, why);
	else /* result == WP_NOTFOUND */
		tks_return(wpem->wpem_tserver, client, TK_NULLSET, TK_NULLSET,
				WP_EXISTENCE_TOKENSET, why);
}

/* ARGSUSED */
void
wp_tsif_recalled(
	void		*obj,
	tk_set_t	recalled,
	tk_set_t	refused)
{
	panic("wp_tsif_recalled");
}

/* ARGSUSED */
void
wp_tsif_idle(
	void		*obj,
	tk_set_t	idle)
{
	panic("wp_tsif_idle");
}
