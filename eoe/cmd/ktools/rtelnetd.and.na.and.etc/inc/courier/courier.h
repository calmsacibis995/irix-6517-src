/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *
 *	Courier protocol definitions
 *
 * Original Author: Rob Drelles		Created on: 85/01/07
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/courier/RCS/courier.h,v 1.2 1996/10/04 12:03:01 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/courier/RCS/courier.h,v $
 *
 * Revision History:
 *
 * $Log: courier.h,v $
 * Revision 1.2  1996/10/04 12:03:01  cwilson
 * latest rev
 *
 * Revision 2.8  1993/02/05  17:54:34  carlson
 * COURRPN_SECURITY was typo-ed going from 2.2 to 2.3.
 *
 * Revision 2.7  92/01/16  17:18:22  reeve
 * Added CMJ_UNSUPPARM for when Annex doesn't support a parameter.
 * 
 * Revision 2.6  92/01/16  11:05:02  reeve
 * Added error codes for when the Annex does not support NA (CMJ_NOANXSUP)
 * and when the Annex does not support a specific value (CMJ_UNSUPVAL).
 * 
 * Revision 2.5  89/04/05  12:21:37  loverso
 * Changed copyright notice
 * 
 * Revision 2.4  88/07/08  14:07:30  harris
 * New reject type NAE_SESSION - means use SRPC with sessions (keepalive).
 * 
 * Revision 2.3  88/05/04  23:38:24  harris
 * Added courier reject reason code - CMJ_SRPC - requires secure interface.
 * 
 * Revision 2.2  87/06/10  18:13:04  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.1  86/06/11  11:33:59  harris
 * Synchronize with Release_1 1.1.1.2.
 * 
 * Revision 1.1.1.2  86/06/10  18:38:46  harris
 * Added defines for sizes of structs which are not zero mod 4.
 * 
 * Revision 1.1.1.1  86/02/20  15:57:42  parker
 * Branch for Release #1.
 * 
 * Revision 1.1  86/02/20  15:36:55  brennan
 * Initial revision
 * 
 * Revision 1.7  85/08/26  19:09:17  parker
 * base level 2 merge.
 * 
 * Revision 1.6.1.1  85/08/15  14:23:43  parker
 * Added Host Remote Program number definition.
 * 
 * Revision 1.6  85/06/20  06:37:34  taylor
 * Modified declaration of cmc_rpnum from u_long to u_short[2] to solve
 * structure alignment problems when used with erpc.
 * 
 * Revision 1.5  85/06/10  16:59:31  brennan
 * Fixed definition of cma_type and cma_tid.
 * 
 * Revision 1.4  85/05/31  10:23:22  goodmon
 * added erpc-based remote program numbers
 * 
 * Revision 1.3  85/05/27  14:48:31  brennan
 * Added definition for COURRPN_SRP.
 * 
 * Revision 1.2  85/04/15  11:58:35  taylor
 * Merge with UMAX definitions
 * Add RDRP remote program number
 * 
 * Revision 1.1  85/04/04  09:02:44  taylor
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:03:01 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */

/*
 * Supported Courier version.
 */
#define COU_VER		3

/*
 * Generic Courier message header.
 */
typedef struct cm_generic {

	u_short cm_type;
	u_short cm_tid;

} COUR_MSG;

/* message types: */

#define C_CALL		0
#define C_REJECT	1
#define C_RETURN	2
#define C_ABORT		3

/* call message - invoke remote procedure */

typedef struct cm_call {

	COUR_MSG cmc_gen;
	u_short	cmc_rpnum[2];
	u_short	cmc_rpver;
	u_short	cmc_rproc;

} CMCALL;

#define cmc_type	cmc_gen.cm_type
#define cmc_tid		cmc_gen.cm_tid

/* reject message - couldn't invoke remote procedure */

typedef struct cm_reject {

	COUR_MSG cmj_gen;
	u_short	cmj_det;

} CMREJECT;
#define CMJSIZE 6

#define cmj_type	cmj_gen.cm_type
#define cmj_tid		cmj_gen.cm_tid

#define	CMJ_NOPROG	0
#define	CMJ_NOVERS	1
#define	CMJ_NOPROC	2
#define	CMJ_INVARG	3
#define	CMJ_SRPC	4
#define CMJ_SESSION	5
#define CMJ_NOANXSUP	6
#define CMJ_UNSUPVAL	7
#define CMJ_UNSUPPARM	8
#define	CMJ_UNSPEC	0xffff

/* return message - successful return from remote procedure */

typedef struct cm_return {

	COUR_MSG cmr_gen;

} CMRETURN;

#define cmr_type	cmr_gen.cm_type
#define cmr_tid		cmr_gen.cm_tid

/* abort message - error return from remote procedure */

typedef struct cm_abort {

	COUR_MSG cma_gen;
	u_short	cma_err;

} CMABORT;
#define CMASIZE 6

#define cma_type	cma_gen.cm_type
#define cma_tid		cma_gen.cm_tid

/*
 * Remote program numbers.  Lower numbers are assigned to erpc remote
 * programs;  higher numbers are assigned to mxl-based remote programs.
 */
#define COURRPN_DLP		(u_long)0x0	/* obsoleted by bfs */
#define COURRPN_BFS		(u_long)0x1
#define COURRPN_NETADM		(u_long)0x2
#define COURRPN_SECURITY	(u_long)0x3

#define COURRPN_HRP		(u_long)0x08004c00
#define COURRPN_SRP		(u_long)0x08004c01
#define COURRPN_RDRP		(u_long)0x08004c02

