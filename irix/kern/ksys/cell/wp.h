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

#ifndef	_KSYS_WP_H_
#define	_KSYS_WP_H_	1
#ident "$Id: wp.h,v 1.6 1997/10/07 20:51:58 sp Exp $"

#ifndef CELL
#error included by non-CELL configuration
#endif

/*
 * WP types
 */
typedef	struct wp_domain *wp_domain_t;
typedef	unsigned long wp_value_t;

/*
 * Interface prototypes
 */
#define	wp_domain_create(name, domain) \
		wp_domain_lookup(name, WP_DOMAIN_DEFAULT, domain)

int wp_domain_lookup(ulong_t, ulong_t, wp_domain_t *);
int wp_lookup(wp_domain_t, ulong_t, wp_value_t *);
int wp_register(wp_domain_t, ulong_t, ulong_t, wp_value_t, ulong_t *, ulong_t *,
								wp_value_t *);
int wp_clear(wp_domain_t, ulong_t, ulong_t);
int wp_allocate(wp_domain_t, ulong_t, wp_value_t, ulong_t *);
int wp_getnext(wp_domain_t, ulong_t *, ulong_t *, wp_value_t *);

/*
 * WP domain names
 * Be sure to update cell/wp/wpc_domain_mgr.c:wp_domain_names when changing
 */
#define SHM_WP_KEYDOMAIN	1
#define SHM_WP_IDDOMAIN		2
#define PID_WP_DOMAIN		3
#define HOST_WP_DOMAIN		4
#define CXFS_WP_SDOMAIN         5
#define CXFS_WP_MDOMAIN         6

#define WP_ID_LAST		6

/*
 * WP domain types for wp_domain_create
 */
#define WP_DOMAIN_DEFAULT	1
#define WP_DOMAIN_LONG		2

#endif	/* _KSYS_WP_H_ */
