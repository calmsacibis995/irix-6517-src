/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "irix/cmd/mkpart/mkprou.h: $Revision: 1.9 $"

#ifndef __MKPROU_H__
#define __MKPROU_H__

#define SN0 	1

#include <sys/SN/arch.h>
#include <sys/partition.h>
#include "mkpart.h"

typedef __uint64_t              nic_t;

#define MAX_PARTITION_PER_SYSTEM	512
#define MAX_ROU_PER_MOD			2
#define MAX_ROU_PER_PART		256
#define MAX_HWG_PATH_LEN		256

/*
 * Picture of routers info layout in sys_rou_map_t

			sys_rou_map

	+-------------------------------------------------+
	|                |           |            |       |
	|  part_rou_map  |           |  . . .     |       |
	|                |           |            |       |
	+-------------------------------------------------+
                 |
                 |
                 |
        +----------------+
	|                |
	|     rou_map    |
	|                |               rou_map
	|----------------|            +------------+
	|                |            |            |
	|                |            |    nic     |
	|                |            |            |
	|                |            |            |  port info
	|----------------|            |            +----------+
	|                |            |            |          |
	|                |            |            |          |
	|                |            |            |          |
	|                |            |            |          |
	|                |            |            |          |
        +----------------+            +------------+----------+

	A port element of the router is given by the tuple

	<part_ind, rmap_ind, port_ind>

 */

typedef struct rou_map_ind_s {
	int	part_ind ;		/* part struct index */
	int	rmap_ind ;		/* router index */
	int	port_ind ;		/* port */
} rou_map_ind_t ;

typedef struct rou_map_s {
	int			myid	;
	char 			name[MAX_HWG_PATH_LEN] 	;
	sn0drv_fence_t		fence 	;
	sn0drv_router_map_t	portmap ;
	partid_t		part  	;
	moduleid_t		mod   	;
	nic_t			nic   	;
	rou_map_ind_t		linkind[MAX_ROUTER_PORTS] ;
	nic_t			portnic[MAX_ROUTER_PORTS] ;
	partid_t		new_part ;
	short			flags 	;
#define	RMAP_VISITED		0x1
	short			type	; 	/* router or meta router */
} rou_map_t ;

#define MAX_ROU_MAP	MAX_MODS_PER_PART * MAX_ROU_PER_MOD

typedef struct part_rou_map_s {
	int		cnt ;
	int		sze ;
	rou_map_t	roumap[MAX_ROU_MAP] ;
} part_rou_map_t ;

#define PART_ROU_MAP_HDR_SIZE	\
		(sizeof(part_rou_map_t) - (MAX_ROU_MAP * sizeof(rou_map_t)))

#define PART_ROU_MAP_SIZE	(sizeof(pmap->roumap)/sizeof(rou_map_t))

typedef struct sys_rou_map_s {
	short		ver ;
	int		cnt ;
	int		sze ;
	part_rou_map_t	*pr_map[MAX_PARTITION_PER_SYSTEM] ;
} sys_rou_map_t ;

/* XXX 1 is for Object of type router. Need macro for it. */

#define IsRouterPort(m,port)	((port > 0) && (port <= MAX_ROUTER_PORTS) && \
						((m)->type[port-1] == 1))

#define ForEachRouterPort(i)					\
		for ((i) = 1; (i) <= MAX_ROUTER_PORTS; (i)++) 	\

/* m is sn0drv_router_map_t rou_map->portmap */

#define ForEachValidRouterPort(m,i)				\
		ForEachRouterPort((i))				\
			if (((m)) && (((m)->type[(i)-1]) == 1))	\

/* NOTE: port should be validated elsewhere before using this macro */

#define GetRouMapForPort(srm,rm,port)	\
		(&(srm->pr_map[rm->linkind[port-1].part_ind]-> 	\
			roumap[rm->linkind[port-1].rmap_ind]))
		
void	part_rou_map_init(part_rou_map_t *) ;
int	create_my_rou_map(partid_t, part_rou_map_t *, int) ;
int 	link_all_roumap(sys_rou_map_t *, char *) ;
int 	update_pmap(part_rou_map_t *, int) ;

#endif /* __MKPROU_H__ */
