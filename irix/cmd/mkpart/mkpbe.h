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

#ident "irix/cmd/mkpart/mkpbe.h: $Revision: 1.3 $"

#ifndef __MKPBE_H__
#define __MKPBE_H__

#include <sys/partition.h>

/*
 * mkpbe - mkpart backend utility support structs.
 */

/*
 * MAX limits of certain data struct allocations.
 * Bump these in case of limit problems.
 */

#define MAX_DEV_NAME_LEN	256
#define MAX_NVRAM_STR_LEN	32
#define MAX_PCBE_RDATA_LEN	1024
#define MAX_NVDATA_LEN		32
#define MAX_BECMD_LEN   	1024
#define MAX_MODPAR_LEN  	32

/*
 * pathname of the msc nvram device of a module.
 */

#define HWG_MSC_NVRAM_PATH		"/hw/module/%d/elsc/nvram"
#define NVRAM_PARTITION 		0x70a  /* module's partition id */

/*
 * Used to construct the backend command.
 */

typedef struct modpar_s {
	moduleid_t	mid ;
	partid_t	pid ;
} modpar_t ;

typedef struct pcbe_s {
	int		cnt ;
	modpar_t	*mp ;
	int		nmp ;
} pcbe_t ;

#define pcbe_cnt(pcbe) 		(pcbe->cnt)
#define pcbe_mp(pcbe) 		(pcbe->mp)
#define pcbe_mid(pcbe,x)	(pcbe->mp[x].mid)
#define pcbe_pid(pcbe,x)	(pcbe->mp[x].pid)

/*
 * Status returned by the rexec command.
 */

typedef struct rstat_s {
	char 	r_status ;
	char	r_status_msg[1] ; 	/* This has to be the last field */
} rstat_t ;

typedef union pcbe_rdata {
	char 	buf[MAX_PCBE_RDATA_LEN] ;
	rstat_t	rstat ;
} pcbe_rdata_t ;

#define pcbe_ret_status(pr)	(pr->rstat.r_status)
#define pcbe_ret_msg(pr)	(pr->rstat.r_status_msg)

/* -1 for the trailing string 0 */
#define PCBE_RMSG_LEN_MAX	(sizeof(pcbe_rdata_t) - sizeof(rstat_t) - 1)
#define PCBE_RSTAT_LEN		(sizeof(rstat_t))

/*
 * Each partition executes this command using the 
 * backend utility.
 */

typedef struct becmd_s {
        struct becmd_s  *bec_next ;
        partid_t        bec_partid ;
        char            *bec_becl ;
        int             bec_becl_len ;
        char            *bec_full_cl ;
        int             bec_fcl_len ;
} becmd_t ;

#ifdef TEST
#define BECMD_NAME      "/usr/people/sprasad/mkpart/mkpbe     "
#else
#define BECMD_NAME      "/usr/sbin/mkpbe      "
/* The extraspaces are to insert any args like -s if needed */
#endif

#define RSHCMD_NAME     "/usr/bsd/rsh "

#endif /* __MKPBE_H__ */
