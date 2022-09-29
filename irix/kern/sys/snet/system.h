#ident "@(#)system.h	1.30	11/13/92"

/******************************************************************
 *
 *  SpiderTCP/X25 Generic Module Configuration File
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  SYSTEM.H
 *
 *    Global system include file to build SpiderTCP/X25.
 *
 *    PR/IPH
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.system.h
 *	@(#)system.h	1.30
 *
 *	Last delta created	14:17:45 1/31/92
 *	This file extracted	14:53:39 11/13/92
 *
 */

/*
 * Stream IDs - one per kernel/driver - used by strlog
 */

#define ECHO_STID	100

/*  X.25 Stream Modules IDs. */
#define	X25_STID		200
#define	LAPB_STID		201
#define	LLC_STID 		202
#define	XXX_STID		203
#define	XTY_STID		204
#define	XIN_STID		205
#define	XOUT_STID		206
#define	NPI_STID		207
#define	IXE_STID		208
#define	SLD_STID		209
#define	WAN_STID		210
#define	WLOOP_STID		211
#define	NLI1_STID		212
#define NLI2_STID		213
#define	GSCMON_STID		214
#define	GSCLOOP_STID		215
#define	WANMON_STID		216

/*
 * - Redefined - PR
 * was typedef queue_t *Q_PTR;
 */
#define Q_PTR queue_t *

/*
 * And now all the usual guff that's never
 * defined
 */

#ifndef NULL
#define NULL 0
#endif

#ifndef	TRUE
#define TRUE 1
#endif
#ifndef	FALSE
#define FALSE 0
#endif
#ifndef	true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* inna */
#ifndef _X25_MASTER_
#ifndef MIN
#define MIN(a,b)	((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b)	((a)<(b)?(b):(a))
#endif

#ifndef	bool
#define bool int
#endif
#endif /* _X25_ */

/*
 * Streams macros
 */


/* 
 * some useful defines
 */
/* inna */
#ifndef _X25_MASTER_
#define MLEN(x)	(((x)->b_wptr) - ((x)->b_rptr))
#define CLEN(x)	(((x)->b_cont->b_wptr) - ((x)->b_cont->b_rptr))
#define CMSG(x)	((x)->b_rptr = (x)->b_wptr = (x)->b_datap->db_base)
#define LBSIZE	sizeof(struct linkblk)
#define SZ_OK(m,t) (((m)->b_wptr - (m)->b_rptr) >= sizeof(t))
#define L(mp) ((mp)->b_wptr - (mp)->b_rptr)
#define RL(mp) ((mp)->b_datap->db_lim - (mp)->b_datap->db_base)
#define CRB(mp)	((mp)->b_cont->b_rptr)
#define CWB(mp)	((mp)->b_cont->b_wptr)
#endif /* _X25_MASTER_ */



