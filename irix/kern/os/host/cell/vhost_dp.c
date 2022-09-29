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
#ident "$Id: vhost_dp.c,v 1.10 1997/08/27 21:38:51 sp Exp $"

/*
 * Cellular Virtual Host Management
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>

#include <ksys/cell/service.h>
#include <ksys/cell/tkm.h>
#include <ksys/cell/wp.h>
#include <ksys/vhost.h>
#include <ksys/cell/cell_set.h>
#include <ksys/cell/recovery.h>

#include "dhost.h"
#include "vhost_private.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dchost_stubs.h"
#include "I_dshost_stubs.h"

vhost_t		*vhost;
static void vhost_recovery(cell_set_t *, void *);

service_t	host_service;
wp_domain_t	host_wp_domain;

void
vhost_cell_init()
{
	ulong_t		curbase, currange;
	wp_value_t	existing_value;

	SERVICE_MAKE(host_service, cellid(), SVC_HOST);

	mesg_handler_register(dchost_msg_dispatcher, DCHOST_SUBSYSID);
	mesg_handler_register(dshost_msg_dispatcher, DSHOST_SUBSYSID);

	if (wp_domain_create(HOST_WP_DOMAIN, &host_wp_domain))
		panic("vhost_cell_init: no host wp domain");

	/*
	 * Race to register.
	 * In R1, the boot or golden cell will win.
	 */
	if (wp_register(host_wp_domain,
			HOST_SERVICE_WPBASE, HOST_SERVICE_WPRANGE,
			SERVICE_TO_WP_VALUE(host_service),
			&curbase, &currange, &existing_value) == 0) {
		HOST_TRACE6("vhost_cell_init:server", cellid(),
			    "cell", SERVICE_TO_CELL(host_service),
			    "svcnum", SERVICE_TO_SVCNUM(host_service))

		/*
		 * Create the virtual/physical host.
		 */
		vhost = vhost_create();

		/*
		 * Nothing further here. The DS will be interposed
		 * in the usual way when the first client calls.
		 */
	} else {
		/* REFERENCED */
		service_t	registered_svc;
		SERVICE_FROMWP_VALUE(registered_svc, existing_value);
		HOST_TRACE6("vhost_cell_init:client", cellid(),
			    "cell", SERVICE_TO_CELL(registered_svc),
			"svcnum", SERVICE_TO_SVCNUM(registered_svc));

		/*
		 * Create and initialized a DC.
		 */
		vhost = vhost_create();
		dchost_create(vhost);

		/*
		 * Register for host service calls.
		 */
		VHOST_REGISTER(VH_SVC_ALL);
	}

	crs_register_callout(vhost_recovery, NULL);
}

vhost_t *
vhost_local(void)
{
	ASSERT(vhost);
	return vhost;
}

service_t
vhost_service(void)
{
	wp_value_t	value;
	service_t	svc;

	if (wp_lookup(host_wp_domain, HOST_SERVICE_WPBASE, &value))
		panic("vhost_service: host server not registered with WP");
	else
		SERVICE_FROMWP_VALUE(svc, value);
	return(svc);
}

/*
 * vhost_recovery:
 * Recovery interface for vhosts.
 */
/* ARGSUSED */
void
vhost_recovery(cell_set_t *fail_set, void *arg)
{
	cell_t	cell;

	if (SERVICE_TO_CELL(vhost_service()) == cellid()) {
		for (cell = 0; cell < CELL_SET_SIZE; cell++)
			if (set_is_member(fail_set, cell))
				dshost_recovery(cell);
	}
}

/*
 * This table and mapping function converts a mask of host services
 * to the corresponding set of tokens.
 */
vh_svc_tk_map_t	vh_svc_tk_map[] = VHOST_SVC_TK_MAP;
#define VHOST_SVC_TK_MAP_SIZE sizeof(vh_svc_tk_map)/sizeof(vh_svc_tk_map_t)
tk_set_t
vh_svc_to_tokenset(
	int		vh_svc_mask)
{
	int		i;
	tk_set_t	svc_tokenset = TK_NULLSET;

	for (i = 0; i < VHOST_SVC_TK_MAP_SIZE; i++) {
		if (TK_IS_IN_SET(vh_svc_tk_map[i].svc, vh_svc_mask))
			TK_COMBINE_SET(svc_tokenset, vh_svc_tk_map[i].tk);
	}
	return svc_tokenset;
}

#ifdef DEBUG
void
idbg_vhost_bhv_print(
	vhost_t	*vhp)
{
	bhv_desc_t	*bhv;

	bhv = VHOST_BHV_FIRST(vhp);

	switch (BHV_POSITION(bhv)) {
	case VHOST_BHV_DS:
		idbg_dshost_print(BHV_PDATA(bhv));
		return;
	case VHOST_BHV_DC:
		idbg_dchost_print(BHV_PDATA(bhv));
		return;
	case VHOST_BHV_PP:
		idbg_phost((__psint_t)BHV_PDATA(bhv));
		return;
	default:
		qprintf("Unknown behavior position %d\n", BHV_POSITION(bhv));
	}
}
#endif
