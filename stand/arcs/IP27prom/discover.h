/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.25 $"

#ifndef	_DISCOVER_H_
#define	_DISCOVER_H_

#include <sys/SN/promcfg.h>
#include "router.h"

/* flags for discover_dump_promcfg */
#define	DDUMP_PCFG_ALL		0x0
#define	DDUMP_PCFG_VERS		0x1

int		discover(pcfg_t *p, nic_t my_nic, int diag_mode);

void		discover_dump_promcfg(pcfg_t *p,
				      int (*prf)(const char *fmt, ...), int flags);

void		discover_dump_routing(pcfg_t *p,
				      int (*prf)(const char *fmt, ...));

int		discover_find_hub_nic(pcfg_t *p, nic_t nic);

int		discover_find_hub_nasid(pcfg_t *p, nasid_t nasid);

int		discover_find_router_nic(pcfg_t *p, nic_t nic);

net_vec_t	discover_route(pcfg_t *p,
			       int src, int dst, int nth_alternate);

int		discover_follow(pcfg_t *p,
				int src, net_vec_t vec);

net_vec_t	discover_return(pcfg_t *p,
				int src_hub, net_vec_t vec);

void		discover_program(pcfg_t *p,
				 int src_hub, nasid_t dst_nasid, net_vec_t vec,
				 net_vec_t *retvec_ptr, int *dst_hub_ptr);

void		discover_module_indexes(pcfg_t *p, int mymod_indx,
				int *mod_indxs);

int		discover_vote_module(pcfg_t *p, int mymod_indx);

void		discover_touch_modids(pcfg_t *p, int mymod_indx, int modid);

int		discover_check_sn00(pcfg_t *p);

#endif /* _DISCOVER_H_ */
