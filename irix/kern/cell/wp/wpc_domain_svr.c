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
#ident "$Id: wpc_domain_svr.c,v 1.8 1997/10/07 20:51:51 sp Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/cell/wp.h>
#include <sys/avl.h>
#include "wp_domain_mgr.h"
#include "wp_domain_svr.h"
#include "wp_private.h"
#include "invk_wpss_stubs.h"

void wp_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *);
void wp_tcif_recall(tkc_state_t, void *, tk_set_t, tk_set_t, tk_disp_t);

tkc_ifstate_t wp_tclient_iface = {
	wp_tcif_obtain,
	wp_tcif_recall
};

static __psunsigned_t
wpd_avl_start(avlnode_t *np)
{
	return((__psunsigned_t ) (((wp_entry_t)np)->wp_base));
}

static __psunsigned_t
wpd_avl_end(avlnode_t *np)
{
	return((__psunsigned_t ) (((wp_entry_t)np)->wp_limit) + 1);
}

avlops_t wpd_avlops = {
	wpd_avl_start,
	wpd_avl_end
};

void
wpc_domain_svr_init(
	wp_domain_t	domain)
{
	avl_init_tree(&domain->wpd_entries, &wpd_avlops);
}

static __inline int
wp_register_instance(
	wp_domain_t	domain,
	wp_instance_t	*instance,
	wp_value_t	equiv,
	wp_instance_t	*oinstance,
	wp_value_t	*oequivp)
{
	avlnode_t	*avln;
	int		result;
	int		retval;
	wp_entry_t	wpe;
	cell_t		owner;

	mrlock(&domain->wpd_lock, MR_ACCESS, PZERO);
	avln = avl_findanyrange(&domain->wpd_entries, instance->wpi_base,
				instance->wpi_limit + 1, 0);
	if (avln != 0) {
		wpe = (wp_entry_t)avln;
		*oinstance = wpe->wp_instance;
		*oequivp = wpe->wp_equiv;
		mrunlock(&domain->wpd_lock);
		return(1);
	}
	mrunlock(&domain->wpd_lock);
	
	if (cellid() == SERVICE_TO_CELL(domain->wpd_service)) {
		wpss_register(domain,  cellid(),
				instance, equiv,
				oinstance, oequivp, 
				&owner, &result);
		if (result == WP_FOUND)
			return(1);
		if (result == WP_NOTFOUND)
			return(0);
	} else {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_wpss_register(domain->wpd_service,
				domain->wpd_name,
				cellid(),
				instance, equiv,
				oinstance, oequivp, 
				&owner, &result);
		ASSERT(!msgerr);
	}
	
	ASSERT(result != WP_AGAIN);
	switch (result) {
	case	WP_FOUND:
		/*
		 * We have an overlapping entry, put it in our cache
		 */
		wpe = kern_malloc(sizeof(struct wp_entry));
		wpe->wp_instance = *oinstance;
		wpe->wp_equiv = *oequivp;
		wpe->wp_owner = owner;
		retval = 1;
		break;

	case	WP_NOTFOUND:
		/*
		 * The register suceeded.
		 */
		wpe = kern_malloc(sizeof(struct wp_entry));
		wpe->wp_instance = *instance;
		wpe->wp_equiv = equiv;
		wpe->wp_owner = cellid();
		retval = 0;
	}

	tkc_create("wpe", wpe->wp_tclient, wpe, &wp_tclient_iface, WP_NTOKENS,
			WP_EXISTENCE_TOKENSET, WP_EXISTENCE_TOKENSET,
			(void *)(__psint_t)wpe->wp_base);
	mrlock(&domain->wpd_lock, MR_UPDATE, PZERO);
	avl_insert(&domain->wpd_entries, &wpe->wp_list);
	mrunlock(&domain->wpd_lock);
	return(retval);
}

/*
 * Try to register a range of values and the white pages number in
 * a white pages domain. If any part of the range has already been registered
 * then the registration fails and the existing registration information is
 * returned. If there are multiple registrations covering this range then only
 * the first is returned.
 */
int
wp_register(
	wp_domain_t	domain,
	ulong_t		base,
	ulong_t		range,
	wp_value_t	equiv,
	ulong_t		*obasep,
	ulong_t		*orangep,
	wp_value_t	*oequivp)
{
	int		retval;
	wp_instance_t	instance;
	wp_instance_t	oinstance;

	instance.wpi_base = base;
	instance.wpi_limit = base + range - 1;

	WP_KTRACE8("wp_register", domain->wpd_name, "base", base, "limit",
			instance.wpi_limit, "equiv", equiv);

	retval = wp_register_instance(domain, &instance, equiv,
					      &oinstance, oequivp);

	if (retval) {
		*obasep = oinstance.wpi_base;
		*orangep = oinstance.wpi_limit - oinstance.wpi_base + 1;
	}
	return(retval);
}

int
wp_lookup(
	wp_domain_t	domain,
	ulong_t		value,
	wp_value_t	*equiv)
{
	avlnode_t	*avln;
	wp_entry_t	wpe;
	wp_instance_t	instance;
	wp_value_t	curequiv;
	cell_t		owner;
	int		result;

	mrlock(&domain->wpd_lock, MR_ACCESS, PZERO);
	avln = avl_findrange(&domain->wpd_entries, value);
	if (avln != NULL) {
		wpe = (wp_entry_t)avln;
		*equiv = wpe->wp_equiv;
		WP_KTRACE6("wp_lookup", domain->wpd_name, "value", value,
			"equiv", wpe->wp_equiv);
		mrunlock(&domain->wpd_lock);
		return(0);
	}
	mrunlock(&domain->wpd_lock);

	/*
	 * Not found locally, look on the server
	 */
	if (cellid() == SERVICE_TO_CELL(domain->wpd_service))
		wpss_lookup(domain,  cellid(), value, 
				&instance, &curequiv, 
				&owner, &result);
	else {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_wpss_lookup(domain->wpd_service, domain->wpd_name,
				cellid(), value,
				&instance, &curequiv, 
				&owner, &result);
		ASSERT(!msgerr);
	}

	WP_KTRACE10("wp_lookup", domain->wpd_name, "svr result", result,
			"\n\tbase", instance.wpi_base,
			"limit", instance.wpi_limit,
			"equiv", *equiv);

	switch (result) {
	case	WP_NOTFOUND:
		return(1);

	case	WP_AGAIN:
		*equiv = curequiv;
		break;

	case	WP_FOUND:
		wpe = kern_malloc(sizeof(struct wp_entry));
		wpe->wp_instance = instance;
		wpe->wp_equiv = curequiv;
		wpe->wp_owner = owner;
		tkc_create("wpe", wpe->wp_tclient, wpe, &wp_tclient_iface,
			WP_NTOKENS,
			WP_EXISTENCE_TOKENSET, WP_EXISTENCE_TOKENSET,
			(void *)(__psint_t)wpe->wp_base);
		mrlock(&domain->wpd_lock, MR_UPDATE, PZERO);
		avl_insert(&domain->wpd_entries, &wpe->wp_list);
		mrunlock(&domain->wpd_lock);
		*equiv = curequiv;
	}
	return(0);
}

int
wp_allocate(
	wp_domain_t	domain,
	ulong_t		range,
	wp_value_t	equiv,
	ulong_t		*basep)
{
	wp_entry_t	wpe;
	int		result;

	if (cellid() == SERVICE_TO_CELL(domain->wpd_service))
		wpss_allocate(domain,  cellid(), range, equiv, 
				basep, &result);
	else {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_wpss_allocate(domain->wpd_service,
				domain->wpd_name,
				cellid(), range, equiv,
				basep, &result);
		ASSERT(!msgerr);
	}

	WP_KTRACE10("wp_allocate", domain->wpd_name, "svr result", result,
			"\n\tbase", *basep, "limit", *basep + range - 1,
			"equiv", equiv);

	ASSERT(result != WP_AGAIN);
	if (result == WP_NOTFOUND)
		return(1);
	ASSERT(result == WP_FOUND);

	if (cellid() == SERVICE_TO_CELL(domain->wpd_service))
		return(0);
	
	wpe = kern_malloc(sizeof(struct wp_entry));
	wpe->wp_base = *basep;
	wpe->wp_limit = *basep + range - 1;
	wpe->wp_equiv = equiv;
	wpe->wp_owner = cellid();
	tkc_create("wpe", wpe->wp_tclient, wpe, &wp_tclient_iface,
		WP_NTOKENS, WP_EXISTENCE_TOKENSET, WP_EXISTENCE_TOKENSET,
		(void *)(__psint_t)wpe->wp_base);
	mrlock(&domain->wpd_lock, MR_UPDATE, PZERO);
	avl_insert(&domain->wpd_entries, &wpe->wp_list);
	mrunlock(&domain->wpd_lock);
	WP_KTRACE4("wp_allocate", domain->wpd_name, "created", wpe);
	return(0);
}

int
wp_getnext(
	wp_domain_t	domain,
	ulong_t		*basep,
	ulong_t		*rangep,
	wp_value_t	*equivp)
{
	wp_entry_t	wpe;
	int		result;
	ulong_t		limit;
	cell_t		owner;

	limit = *basep + *rangep - 1;

	if (cellid() == SERVICE_TO_CELL(domain->wpd_service))
		wpss_iterate(domain,  cellid(), basep, &limit, 
				equivp, &owner, &result);
	else {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_wpss_iterate(domain->wpd_service,
				domain->wpd_name,
				cellid(), basep, &limit,
				equivp, &owner, &result);
		ASSERT(!msgerr);
	}
	
	if (result == WP_AGAIN) {
		/*
		 * Found an entry. we already have in our cache
		 */
		*rangep = limit - *basep + 1;
		return(0);
	}
	if (result == WP_NOTFOUND) {
		/*
		 * End of iteration
		 */
		WP_KTRACE2("wp_getnext - end", domain->wpd_name);
		return(1);
	}

	ASSERT(result == WP_FOUND);

	*rangep = limit - *basep + 1;

	if (cellid() == SERVICE_TO_CELL(domain->wpd_service))
		return(0);
	
	wpe = kern_malloc(sizeof(struct wp_entry));
	wpe->wp_base = *basep;
	wpe->wp_limit = limit;
	wpe->wp_equiv = *equivp;
	wpe->wp_owner = owner;
	tkc_create("wpe", wpe->wp_tclient, wpe, &wp_tclient_iface,
		WP_NTOKENS, WP_EXISTENCE_TOKENSET, WP_EXISTENCE_TOKENSET,
		(void *)(__psint_t)wpe->wp_base);
	mrlock(&domain->wpd_lock, MR_UPDATE, PZERO);
	avl_insert(&domain->wpd_entries, &wpe->wp_list);
	mrunlock(&domain->wpd_lock);
	WP_KTRACE8("wp_getnext", domain->wpd_name, "base", *basep, "range",
				*rangep, "equiv", *equivp);
	return(0);
}

void
wpe_destroy(
	wp_entry_t	wpe)
{
	tk_set_t	retset;
	tk_disp_t	why;

	tkc_release(wpe->wp_tclient, WP_EXISTENCE_TOKENSET);
	tkc_returning(wpe->wp_tclient, WP_EXISTENCE_TOKENSET, &retset, &why, 0);
	ASSERT(retset == WP_EXISTENCE_TOKENSET);
	tkc_returned(wpe->wp_tclient, retset, TK_NULLSET);
	tkc_destroy(wpe->wp_tclient);
	kmem_free(wpe, sizeof(wp_entry_t *));
}

int
wp_clear(
	wp_domain_t	wpd,
	ulong_t		base,
	ulong_t		range)
{
	ulong_t		limit;
	ulong_t		start;
	avlnode_t	*avln;
	wp_entry_t	wpe;
	int		result;
	/* REFERENCED */
	int		msgerr;

	start = base;
	limit = base + range - 1;

	WP_KTRACE6("wp_clear", wpd->wpd_name, "base", base, "limit", limit);
	/*
	 * First delete all our cached entries. Don't do this if
	 * we are the server for this domain.
	 */
	if (cellid() != SERVICE_TO_CELL(wpd->wpd_service)) {
		mrlock(&wpd->wpd_lock, MR_UPDATE, PZERO);
		for (;;) {
			avln = avl_findadjacent(&wpd->wpd_entries, start,
								AVL_SUCCEED);
			if (avln == NULL)
				break;
			wpe = (wp_entry_t)avln;
			if ((base <= wpe->wp_base) && wpe->wp_limit <= limit) {
				avl_delete(&wpd->wpd_entries, &wpe->wp_list);
				start = wpe->wp_limit;
				WP_KTRACE6("wpe_destroy", wpd->wpd_name,
					   "base", wpe->wp_base,
					   "limit", wpe->wp_limit);
				wpe_destroy(wpe);
			} else
				break;
		}
		mrunlock(&wpd->wpd_lock);
		msgerr = invk_wpss_clear(wpd->wpd_service, wpd->wpd_name,
                                cellid(), base, limit, &result);
		ASSERT(!msgerr);
	} else
		wpss_clear(wpd, cellid(), base, limit, &result);

	return(result == WP_NOTFOUND);
}


/*
 ****************************************************************************
 * Client side message implementation
 ****************************************************************************
 */

void
I_wpsc_invalidate(
	long		domain,
	ulong_t		base,
	ulong_t		limit,
	wp_value_t	equiv,
	int		*result)
{
	wp_domain_t	wpd;
	avlnode_t	*avln;
	wp_entry_t	wpe;


	wp_domain_lookup_local(domain, &wpd);
	mrlock(&wpd->wpd_lock, MR_UPDATE, PZERO);
	avln = avl_findrange(&wpd->wpd_entries, base);
	if (avln != NULL) {
		wpe = (wp_entry_t)avln;
		ASSERT((base == wpe->wp_base) && 
			(limit == wpe->wp_limit) &&
			(equiv == wpe->wp_equiv));
		WP_KTRACE8("I_wpsc_invalidate", wpd->wpd_name, "base", base,
			"limit", limit, "equiv", equiv);
		avl_delete(&wpd->wpd_entries, &wpe->wp_list);
		*result = WP_FOUND;
	} else
		*result = WP_NOTFOUND;
	mrunlock(&wpd->wpd_lock);
}

/*
 ****************************************************************************
 * Token Callout funtions
 ****************************************************************************
 */

/* ARGSUSED */
void
wp_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	why_returning,
	tk_set_t	*refused)
{
	panic("wp_tcif_obtain");
}

/* ARGSUSED */
void
wp_tcif_recall(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_recalled,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	panic("wp_tcif_recall");
}
