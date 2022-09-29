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
#ident "$Id: wp_domain_svr.h,v 1.4 1997/10/07 20:51:45 sp Exp $"

#ifndef	_WP_DOMAIN_SVR_H_
#define	_WP_DOMAIN_SVR_H_ 1

#include <ksys/cell/tkm.h>
#include <sys/avl.h>

#define WP_TOKEN_EXIST	0
#define WP_NTOKENS	1

#define WP_EXISTENCE_TOKENSET  TK_MAKE(WP_TOKEN_EXIST, TK_READ)

typedef union {
	struct {
		ulong_t	wpud_base;
		ulong_t	wpud_limit;
	} wpu_default;
	char	wpul_value[16];
} wp_instance_t;

#define wpi_base	wpu_default.wpud_base
#define wpi_limit	wpu_default.wpud_limit

typedef struct wp_entry {
	avlnode_t	wp_list;
	wp_instance_t	wp_instance;
	wp_value_t	wp_equiv;
	cell_t		wp_owner;
	TKC_DECL(wp_tclient, WP_NTOKENS);
} *wp_entry_t;

#define wp_base		wp_instance.wpi_base
#define wp_limit	wp_instance.wpi_limit

extern int wp_domain_lookup_local(ulong_t, wp_domain_t *);
extern void wpc_domain_svr_init(wp_domain_t);
extern void wpss_register(wp_domain_t, cell_t, wp_instance_t *, wp_value_t,
				wp_instance_t *, wp_value_t *, cell_t *,
				int *);
extern void wpss_lookup(wp_domain_t, cell_t, ulong_t, wp_instance_t *,
				wp_value_t *, cell_t *, int *);
extern void wpss_allocate(wp_domain_t, cell_t, ulong_t, wp_value_t, ulong_t *,
				int *);
extern void wpss_iterate(wp_domain_t, cell_t, ulong_t *, ulong_t *,
				wp_value_t *, cell_t *, int *);
extern void wpss_clear(wp_domain_t, cell_t, ulong_t, ulong_t, int *);

#endif	/* _WP_DOMAIN_SVR_H_ */
