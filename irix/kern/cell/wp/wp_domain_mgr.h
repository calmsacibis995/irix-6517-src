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
#ident "$Id: wp_domain_mgr.h,v 1.5 1997/10/07 20:51:44 sp Exp $"

#ifndef	_WP_DOMAIN_MGR_H_
#define	_WP_DOMAIN_MGR_H_ 1

#include <sys/sema.h>
#include <sys/avl.h>
#include <ksys/cell/service.h>
#include <ksys/cell/tkm.h>

#define WPD_TOKEN_EXIST	0
#define WPD_NTOKENS	1

#define	WPD_EXISTENCE_TOKENSET	TK_MAKE(WPD_TOKEN_EXIST, TK_READ)

struct wp_domain {
	struct wp_domain	*wpd_next;
	mrlock_t		wpd_lock;
	ulong_t			wpd_name;
	ulong_t			wpd_type;
	avltree_desc_t		wpd_entries;
	service_t		wpd_service;
	TKC_DECL(wpd_tclient, WPD_NTOKENS);
};

#define WP_DOMAIN_NULL	((struct wp_domain *)0)

extern mrlock_t		wpd_list_lock;
extern wp_domain_t	wpd_list;

extern void wpds_domain_create(ulong_t, ulong_t, cell_t, int *, service_t *);

#endif	/* _WP_DOMAIN_MGR_H_ */
