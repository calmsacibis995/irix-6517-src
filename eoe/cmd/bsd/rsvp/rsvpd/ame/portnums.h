/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module portnums.h, release 1.9 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.portnums.h
 *sccs
 *sccs    1.9	96/02/12 13:17:10 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.8	95/01/24 13:00:46 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.7	94/07/11 16:47:20 randy
 *sccs	integrated mgr demo port assignment
 *sccs
 *sccs    1.6	93/07/29 15:36:24 muriel
 *sccs	#include socket.h from ame
 *sccs
 *sccs    1.5	93/05/28 02:16:17 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.4	93/04/26 17:56:48 randy
 *sccs	psos support
 *sccs
 *sccs    1.3	93/03/05 09:34:49 randy
 *sccs	added manifests for target-specific versions
 *sccs
 *sccs    1.2	92/11/12 16:33:52 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	92/09/11 19:05:46 randy
 *sccs	date and time created 92/09/11 19:05:46 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_PORTNUMSH				/* avoid re-inclusion	*/
#define AME_PORTNUMSH

/************************************************************************
 *                                                                      *
 *          PEER Networks, a division of BMC Software, Inc.             *
 *                   CONFIDENTIAL AND PROPRIETARY                       *
 *                                                                      *
 *	This product is the sole property of PEER Networks, a		*
 *	division of BMC Software, Inc., and is protected by U.S.	*
 *	and other copyright laws and the laws protecting trade		*
 *	secret and confidential information.				*
 *									*
 *	This product contains trade secret and confidential		*
 *	information, and its unauthorized disclosure is			*
 *	prohibited.  Authorized disclosure is subject to Section	*
 *	14 "Confidential Information" of the PEER Networks, a		*
 *	division of BMC Software, Inc., Standard Development		*
 *	License.							*
 *									*
 *	Reproduction, utilization and transfer of rights to this	*
 *	product, whether in source or binary form, is permitted		*
 *	only pursuant to a written agreement signed by an		*
 *	authorized officer of PEER Networks, a division of BMC		*
 *	Software, Inc.							*
 *									*
 *	This product is supplied to the Federal Government as		*
 *	RESTRICTED COMPUTER SOFTWARE as defined in clause		*
 *	55.227-19 of the Federal Acquisitions Regulations and as	*
 *	COMMERCIAL COMPUTER SOFTWARE as defined in Subpart		*
 *	227.401 of the Defense Federal Acquisition Regulations.		*
 *									*
 * Unpublished, Copr. PEER Networks, a division of BMC	Software, Inc.  *
 *                     All Rights Reserved				*
 *									*
 *	PEER Networks, a division of BMC Software, Inc.			*
 *	1190 Saratoga Avenue, Suite 130					*
 *	San Jose, CA 95129-3433 USA					*
 *									*
 ************************************************************************/


/*************************************************************************
 *
 *	portnums.h - define the standard snmp and smux port numbers
 *
 ************************************************************************/

#include "ame/machtype.h"

/*
 *	Get the definition of the htons macro.	If this macro does anything,
 *	it will flip bytes around to get from host byte order to the order
 *	used by the socket api.
 */
#ifdef PEER_MSDOS_PORT

#define htons(X)	(X)
#define htonl(X)	(X)

#else	/* else if not PEER_MSDOS_PORT */

#include "ame/socket.h"

#endif


#define SNMP_REQUEST_PORT	htons(161)
#define SNMP_TRAP_PORT		htons(162)
#define SMUX_LISTEN_PORT	htons(199)

/*
 *	The following is used by the manager demo / torpedo product
 *	Any port number which does not conflict with other assignments
 *	will work just as well, as long as both server and client use
 *	the same value.
 */
#define MGRMUX_LISTEN_PORT	htons(1198)


#endif
