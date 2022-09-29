/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TIMOD_H	/* wrapper symbol for kernel use */
#define _SYS_TIMOD_H	/* subject to change without notice */

#ident	"@(#)uts-3b2:net/transport/timod.h	1.3"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#include <sys/types.h>	/* REQUIRED */

#endif	/* _KERNEL */

#include <sys/stream.h>


/* Internal flags */
#define USED		0x0001	/* data structure in use          */
#define FATAL		0x0002	/* fatal error M_ERROR occurred   */
#define WAITIOCACK	0x0004	/* waiting for info for ioctl act */
#define MORE		0x0008	/* more data */
#define EXPEDITED	0x0010	/* processing expedited TSDU */
#define CLTS		0x0020	/* connectionless transport */
#define COTS		0x0040	/* connection-oriented transport */
#define OPENWAIT	0x0080	/* sleeping in open routine */
#define CONNWAIT	0x0100	/* waiting for connect confirmation */
#define LOCORDREL	0x0200	/* local end has orderly released */
#define REMORDREL	0x0400	/* remote end had orderly released */
#define NAMEPROC	0x0800	/* processing a NAME ioctl */

/* Internal buffer size (in bytes) pre-allocated for address fields */
#define PRADDRSZ	128

/* Sleep timeout in open */
#define TIMWAIT	(1*HZ)

/* Timod ioctls */
#define		TIMOD 		('T'<<8)
#if defined(_KERNEL) || !defined(_STYPES)	/* SVR4 definitions */
#define		TI_GETINFO	(TIMOD|140)
#define		TI_OPTMGMT	(TIMOD|141)
#define		TI_BIND		(TIMOD|142)
#define		TI_UNBIND	(TIMOD|143)
#else /* defined(_KERNEL)  SVR3 definitions */
#define         TI_GETINFO      (TIMOD|100)
#define         TI_OPTMGMT      (TIMOD|101)
#define         TI_BIND         (TIMOD|102)
#define         TI_UNBIND       (TIMOD|103)
#endif /* defined(_KERNEL) */
#define         _SVR3_TI_GETINFO (TIMOD|100)
#define         _SVR3_TI_OPTMGMT (TIMOD|101)
#define         _SVR3_TI_BIND   (TIMOD|102)
#define         _SVR3_TI_UNBIND (TIMOD|103)
#define		TI_GETMYNAME	(TIMOD|144)
#define		TI_GETPEERNAME	(TIMOD|145)
#define		TI_SETMYNAME	(TIMOD|146)
#define		TI_SETPEERNAME	(TIMOD|147)


/* TI interface user level structure - one per open file */

struct _ti_user {
	ushort_t	ti_flags;	/* flags              */
	int	ti_rcvsize;	/* rcv buffer size    */
	char   *ti_rcvbuf;	/* rcv buffer         */
	int	ti_ctlsize;	/* ctl buffer size    */
	char   *ti_ctlbuf;	/* ctl buffer         */
	char   *ti_lookdbuf;	/* look data buffer   */
	char   *ti_lookcbuf;	/* look ctl buffer    */
	int	ti_lookdsize;  /* look data buf size */
	int	ti_lookcsize;  /* look ctl buf size  */
	int	ti_maxpsz;	/* TIDU size          */
	long	ti_servtype;	/* service type       */
	int     ti_lookflg;	/* buffered look flag */
	int	ti_state;	/* user level state   */
	int	ti_ocnt;	/* # outstanding connect indications */
#ifdef _BUILDING_LIBXNET
	int	ti_qlen;	/* connect indication queue size */
	int	ti_tsdu;	/* the amount of TSDU supported by provider */
#endif
};

/* Old TI interface user level structure - needed for compatibility */

struct _oldti_user {
	ushort_t	ti_flags;	/* flags              */
	int	ti_rcvsize;	/* rcv buffer size    */
	char   *ti_rcvbuf;	/* rcv buffer         */
	int	ti_ctlsize;	/* ctl buffer size    */
	char   *ti_ctlbuf;	/* ctl buffer         */
	char   *ti_lookdbuf;	/* look data buffer   */
	char   *ti_lookcbuf;	/* look ctl buffer    */
	int	ti_lookdsize;  /* look data buf size */
	int	ti_lookcsize;  /* look ctl buf size  */
	int	ti_maxpsz;	/* TIDU size          */
	long	ti_servtype;	/* service type       */
	int     ti_lookflg;	/* buffered look flag */
};


#ifndef _KERNEL
/* This should be replaced */
#include <ulimit.h>
#define OPENFILES     ulimit(4, 0)
#endif

/*
 * Routine to be used by transport providers to process
 * TI_GETMYNAME and TI_GETPEERNAME ioctls.
 */
extern int	ti_doname(queue_t *, mblk_t *, void *, uint_t, void *, uint_t);

/*
 * Return values for ti_doname.
 */
#define DONAME_FAIL	0	/* failing ioctl (done) */
#define DONAME_DONE	1	/* done processing */
#define DONAME_CONT	2	/* continue proceesing (not done yet)*/

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_TIMOD_H */
