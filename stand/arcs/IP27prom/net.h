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

#ident "$Revision: 1.14 $"

#ifndef	_NET_H_
#define	_NET_H_

#include <sys/SN/vector.h>

void		net_init(void);

void		net_link_reset(void);
int		net_link_up(void);
int		net_link_down_reason(void);

int		net_node_get(void);
void		net_node_set(int node_id);

int		net_node_get_remote(net_vec_t vec, nasid_t *nasid);
int		net_node_set_remote(net_vec_t vec, nasid_t nasid);

int		net_program(net_vec_t vec,
			    nasid_t nl,		/* Local NASID */
			    nasid_t nr);	/* Remote NASID */

char	       *net_errmsg(int rc);

#endif /*_NET_H_ */
