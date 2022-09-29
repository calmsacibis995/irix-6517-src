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
 *	Define ERPC-related constants
 *
 * Original Author: Sandy Palmer	Created on: 84/04/17 
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/erpc/RCS/erpc.h,v 1.2 1996/10/04 12:03:14 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/erpc/RCS/erpc.h,v $
 *
 * Revision History:
 *
 * $Log: erpc.h,v $
 * Revision 1.2  1996/10/04 12:03:14  cwilson
 * latest rev
 *
 * Revision 2.2  1989/04/05  12:21:47  loverso
 * Changed copyright notice
 *
 * Revision 2.1  86/06/11  11:30:45  harris
 * Synchronized with Release_1 version 1.7.1.2.
 * 
 * Revision 1.7.1.2  86/06/10  18:33:57  harris
 * Redefined stuctures to avoid nesting at a short boundary.
 * Included definitions for sizes of structs not MOD 4.
 * 
 * Revision 1.7.1.1  86/02/20  15:59:39  parker
 * Branch for Release #1.
 * 
 * Revision 1.7  86/01/22  12:06:25  taylor
 * Changed misleading comment for IPPORT_ERPCLISTEN.
 * 
 * Revision 1.6  86/01/21  11:58:43  brennan
 * Use NIC assigned ERPC port.
 * 
 * Revision 1.5  85/08/12  11:23:28  taylor
 * Moved pep definitions from xns.h and pep.h into here.
 * 
 * Revision 1.4  85/06/30  22:28:07  taylor
 * Remove definition of IPPORT_ERPCLISTEN from inside STANDALONE
 * conditional.
 * 
 * Revision 1.3  85/06/20  06:51:16  taylor
 * Changed names of ch_pnum, ch_ver, and ch_proc.
 * 
 * Revision 1.2  85/06/06  15:32:23  brennan
 * Use new courier header structure names.
 * 
 * Revision 1.1  85/04/04  09:29:30  taylor
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:03:14 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */

/*
 * Define contents of erpc message common header.
 */
struct pephdr {
	u_short	ph_id[2];		/* ID */
	u_short	ph_client;		/* client type */
};
#define PEPSIZE 6

/* client types: */

#define PET_LOXPER		040		/* lowest experimental */
#define	PET_ERPC		PET_LOXPER	/* for now */
#define PET_ERPR		PET_LOXPER+1	/* for now */

/* ERPC listener contact UDP port */

#define	IPPORT_ERPCLISTEN	121		/* official from NIC */

/*
 * Call message.
 */
typedef struct chdr {
	u_short ch_id[2];
	u_short ch_client;
	u_short ch_type;
	u_short ch_tid;
	u_short ch_rpnum[2];
	u_short ch_rpver;
	u_short ch_rproc;
} CHDR;
#define CHDRSIZE 18

/*
 * Reject message.
 */
typedef struct jhdr {
	u_short jh_id[2];
	u_short jh_client;
	u_short jh_type;
	u_short jh_tid;
	u_short jh_det;
} JHDR;

/*
 * Return message.
 */
typedef struct rhdr {
	u_short rh_id[2];
	u_short rh_client;
	u_short rh_type;
	u_short rh_tid;
} RHDR;
#define RHDRSIZE 10

/*
 * Abort message.
 */
typedef struct ahdr {
	u_short ah_id[2];
	u_short ah_client;
	u_short ah_type;
	u_short ah_tid;
	u_short ah_err;
} AHDR;

#ifdef STANDALONE

/*
 * Standalone erpc implementation requires contiguous packets.
 */

#define ERPC_EOFF	0
#define	ERPC_IOFF	(ERPC_EOFF + sizeof(struct ether_header))
#define ERPC_UOFF	(ERPC_IOFF + sizeof(struct ip))
#define ERPC_POFF	(ERPC_UOFF + sizeof(struct udphdr))
#define ERPC_COFF	(ERPC_POFF + PEPSIZE)

#define ERPC_CMIN	(ERPC_COFF + sizeof(struct cmc))
#define ERPC_RMIN	(ERPC_COFF + sizeof(struct cmr))

#endif
