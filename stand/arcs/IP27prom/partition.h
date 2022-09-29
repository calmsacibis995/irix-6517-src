/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _PARTITION_H_
#define	_PARTITION_H_

#include <sys/SN/promcfg.h>

int		pttn_validate(pcfg_t *p);

int		pttn_pcfg_fence(pcfg_t *p);

int		pttn_wallin(pcfg_t *p);

void		pttn_erect_fences(pcfg_t *p);

int		pttn_pcfg_nasid_update(pcfg_t *to_p, pcfg_t *from_p);

int		pttn_setup_meta_routers(pcfg_t *p, partid_t partition);

int		pttn_pcfg_clear_fences(pcfg_t *p);

int		pttn_clear_fences(pcfg_t *p);

int		pttn_router_partid(pcfg_t *p);

int		pttn_partprom(pcfg_t *p);

partid_t	pttn_vote_partitionid(pcfg_t *p, int my_indx);

void		pttn_hub_partid(pcfg_t *p, int headless_only);

void		pttn_update_module_partid(pcfg_t *p);

void		pttn_clean_partids(pcfg_t *p);

#endif /* _PARTITION_H_ */
