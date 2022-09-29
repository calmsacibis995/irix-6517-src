/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module smuxasn.h, release 1.6 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.smuxasn.h
 *sccs
 *sccs    1.6	96/02/12 13:18:10 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.5	95/02/06 18:34:58 randy
 *sccs	avoid macro conflict with NT (PR#212)
 *sccs
 *sccs    1.4	95/01/24 13:01:05 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.3	93/11/02 15:42:07 randy
 *sccs	support for NCR protocol
 *sccs
 *sccs    1.2	92/11/12 16:34:07 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	91/09/03 16:51:37 randy
 *sccs	date and time created 91/09/03 16:51:37 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SMUXASNH				/* avoid recursion	*/
#define SMUXASNH

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
 *	smuxasn.h  -  asn.1 definitions for the SMUX protocol
 *
 ************************************************************************/

#include "ame/asn.h"

#define SMUX_SIMPLE_OPEN_PDU_TAG	(PEER_APPLICATION PEER_CONSTRUCT 0)
#define SMUX_CLOSE_PDU_TAG		(PEER_APPLICATION PEER_PRIMITIVE 1)
#define SMUX_RREQ_PDU_TAG		(PEER_APPLICATION PEER_CONSTRUCT 2)
#define SMUX_RRSP_PDU_TAG		(PEER_APPLICATION PEER_PRIMITIVE 3)
#define SMUX_SOUT_PDU_TAG		(PEER_APPLICATION PEER_PRIMITIVE 4)

/*
 *	The following may be encountered when interacting with an
 *	NCR agent.  They got the ASN.1 wrong!
 */
#ifdef ALLOW_NCR_SMUX
#define BOGUS_SMUX_RRSP_PDU_TAG		(PEER_APPLICATION PEER_CONSTRUCT 3)
#endif

#endif
