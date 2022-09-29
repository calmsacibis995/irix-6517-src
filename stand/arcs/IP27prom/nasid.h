/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.13 $"

#ifndef	_NASID_H_
#define	_NASID_H_

#include <sys/SN/promcfg.h>

/*
 * the LOCAL() macro determines whether 
 * r1 is in the same local cube as r2 (which is always an 8p router).
 */
#define LOCAL(_r1, _r2) (IS_META(_r1) ? 0 : \
			 (NASID_GET_META((_r1)->metaid)== \
			  NASID_GET_META((_r2)->metaid)))

/*
 * the SAME() macro determines whether r1 and r2 are both
 * meta routers, or both 8p routers in the same meta cube.
 */
#define SAME(_r1, _r2) (( IS_META(_r1)&& IS_META(_r2)) || \
			(!IS_META(_r1)&&!IS_META(_r2)&&   \
			 NASID_GET_META((_r1)->metaid)==  \
			 NASID_GET_META((_r2)->metaid)))

#define NEW_OK 0
#define NEW_FAIL (-1)

int 	bfs_run(pcfg_t *p, int root);
int     nasid_assign(pcfg_t *p, int from_index);
int 	touch_pcfg_nasids(pcfg_t *p);
int     route_assign(pcfg_t *p);
int     deadlock_check(pcfg_t *p);

#endif /* _NASID_H_ */
