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

#ident "irix/cmd/mkpart/mkpd.h: $Revision: 1.9 $"

#ifndef __MKPD_H__
#define __MKPD_H__

#define SN0 1

#include <sys/SN/arch.h>
#include <sys/param.h>

/*
 * mkpart daemon
 */

#define MKPD_SLEEP_TIME			2	/* seconds */
#if 0
#define MKPD_XPLINK_RAW_PATH 		"/hw/xplink/raw/"
#else
#define MKPD_XPLINK_RAW_PATH 		"/hw/xplink/admin/"
#endif
#define MKPD_XPLINK_RAW_PATH_1 		"/partition"

/*
 * Data struct max limits.
 */

/* Max len of the partition 'raw' file. It is 2 bytes now
 * since we have only upto max 64 parts.
 */

/* XXX this should follow some kernel defn */
#define MKPD_PART_FNAME_LEN		2
#define MKPD_MAX_XRAW_FNAME_LEN		256
#define MKPD_MAX_HOST_NAME_LEN		MAXHOSTNAMELEN
#define MKPD_MAX_HOST_ADDR_LEN		MAXHOSTNAMELEN
#define MKPD_MAX_IPADDR_ASCII_LEN	32

#define MKPD_XPNET_NAME_PREFIX		""

/* 
 * A bunch of structs for managing Open and New partition
 * raw file descriptors in the daemon.
 */

/* Partition - file descriptor */

typedef struct pfd_s {
	part_info_t	*pfd_pi ;	/* part info for this part */
	int		pfd_fd  ;	/* Open file descp of pstream */
	int		pfd_sts ;	/* File handle status */

#define PFD_OPEN	0x1
#define PFD_CLSE	0x2
#define PFD_RTRY	0x4

	struct pfd_s	*pfd_next ; 	/* Future use */
} pfd_t ;

/* Array of Partition file handle */

typedef struct pfdi_s {
	int	pfdi_cnt ;			/* valid pfhs */
	pfd_t	pfdi_pfd[MAX_PARTITIONS] ;	/* actual fileh info */
	int	pfdi_size ;			/* max alloced */
} pfdi_t ;

#define pfdiPartPi(pfdi,i) 	(pfdi->pfdi_pfd[i].pfd_pi)
#define pfdiPartId(pfdi,i) 	(pfdiPartPi(pfdi,i)->pi_partid)
#define pfdiPartFd(pfdi,i) 	(pfdi->pfdi_pfd[i].pfd_fd)
#define pfdiPartSts(pfdi,i) 	(pfdi->pfdi_pfd[i].pfd_sts)
#define PART_OPEN(pfdi,i)	(pfdiPartSts(pfdi,i) == PFD_OPEN)
#define PART_SET_OPEN(pfdi,i)	(pfdiPartSts(pfdi,i) = PFD_OPEN)
#define PART_SET_CLOSE(pfdi,i)	(pfdiPartSts(pfdi,i) = PFD_CLSE)
#define PART_SET_RTRY(pfdi,i)   (pfdiPartSts(pfdi,i) = PFD_RTRY)
#define pfdiPartNext(pfdi,i) 	(pfdi->pfdi_pfd[i].pfd_next)

#define pfdiSize(pfdi)		(pfdi->pfdi_size)
#define pfdiCnt(pfdi)		(pfdi->pfdi_cnt)
#define pfdiCntIncr(pfdi)	(pfdi->pfdi_cnt++)

/*
 * Socket related definitions. The mkpart utility
 * uses sockets to communicate with the local mkpd daemon
 * which uses the xplink raw stream to talk to the
 * remote partition.
 */

#define MKPD_MAX_SOCKET			32

#ifdef TEST
#define	MKPD_SOCKET_NAME		".mkpd_socket"
#else
#define	MKPD_SOCKET_NAME		"/tmp/.mkpd_socket"
#endif

/*
 * Some simple definitions for a very simple protocol used
 * to exchange info between 2 mkpds.
 */

typedef int	mkpd_cnt_t ;

typedef struct mkpd_reqbuf_s {
	char		version ;
	char 		op ;
	char		part ;
	mkpd_cnt_t	cnt ;
	char		flag ;
} mkpd_reqbuf_t ;

/* sizeof mkpd_reqbuf_t should be less than PACKET_DATA_LEN. XXX */

#define	OPCODE_GET_IPADDR	1
#define	OPCODE_GET_ROUCFG	2
#define	OPCODE_LOCK_PART 	3
#define	OPCODE_UNLOCK_PART	4

typedef struct mkpd_packet_s {
	char		version ;
	char		ptype ;
#define PACKET_TYPE_UNKNOWN	0
#define PACKET_TYPE_ERR		1
#define PACKET_TYPE_OPCODE	2
#define PACKET_TYPE_COUNT	3
	char		op ;
	mkpd_cnt_t	cnt ;
	partid_t	partid ;
	char		flag ;
} mkpd_packet_t ;

#define PACKET_REQ_LEN		sizeof(mkpd_packet_t)
#define PACKET_DATA_LEN		1024
#define ROUCFG_DATA_LEN		PACKET_DATA_LEN

int            mkpd_read_req(int fd, mkpd_packet_t *, char type) ;

typedef struct part_ipaddr {
	partid_t	partid ;
        char 		ipaddr[MKPD_MAX_IPADDR_ASCII_LEN] ;
	int		flags ;
#define PART_REBOOT		0x1
#define PART_REBOOT_DONE	0x2
} part_ipaddr_t ;

#endif /* __MKPD_H__ */
