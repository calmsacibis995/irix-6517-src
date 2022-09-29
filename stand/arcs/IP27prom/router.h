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

#ident "$Revision: 1.13 $"

#ifndef	_ROUTER_H_
#define	_ROUTER_H_

#include <sys/SN/router.h>
#include "net.h"

/*
 * Definitions for router_fence_ports
 */

#define	SET	1
#define	CLEAR	2

#define	LOCALB	(1 << 0)
#define	PORT	(1 << 1)

#define	ALL_PORTS	0x3f

int	router_lock(net_vec_t dest,
		    int poll_usec,		/* Lock poll period */
		    int timeout_usec);		/* 0 for single try */

int	router_unlock(net_vec_t dest);

int	router_nic_get(net_vec_t dest,
		       nic_t *nic);

int	router_led_set(net_vec_t dest, int leds);
int	router_led_get(net_vec_t dest, int *leds);

int	router_chipio_set(net_vec_t dest, int chipout);
int	router_chipio_get(net_vec_t dest, int *chipin, int *chipout);

int	router_clear_tables(net_vec_t dest);

int	router_fence_ports(net_vec_t vec, __uint64_t fence_mask, int set_clear,
	int local_or_port, __uint64_t port_mask);

#ifdef MIXED_SPEEDS
void	router_setup_mixed_speeds(pcfg_t *p);
#endif

#endif /* _ROUTER_H_ */
