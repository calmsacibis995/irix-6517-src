/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module asn.h, release 1.6 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.asn.h
 *sccs
 *sccs    1.6	96/02/12 13:14:54 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.5	95/02/06 18:34:53 randy
 *sccs	avoid macro conflict with NT (PR#212)
 *sccs
 *sccs    1.4	95/01/24 13:00:01 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.3	93/08/19 13:40:02 randy
 *sccs	added primitive types needed by SNMPv2
 *sccs
 *sccs    1.2	92/11/12 16:33:23 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	91/09/03 16:51:34 randy
 *sccs	date and time created 91/09/03 16:51:34 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef ASNH					/* avoid recursion	*/
#define ASNH

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
 *	asn.h  -  common asn definitions for the AME
 *
 *	Note:	the macros for CONTEXT, APPLICATION, etc. are prefixed
 *		with PEER_ to avoid conflicts which occur under NT with
 *		Microsoft visual C 2.0
 *
 ************************************************************************/

/*
 *	Local definitions needed for building tags
 */
#define PEER_CONSTRUCT			0x20 |
#define PEER_PRIMITIVE			0x00 |

#define PEER_UNIVERSAL			0x00 |
#define PEER_APPLICATION		0x40 |
#define PEER_CONTEXT			0x80 |
#define PEER_PRIVATE			0xC0 |

/*
 *	Define interesting ASN.1 Basic Encoding Rule tags
 */
#define BER_EOC_TAG			(PEER_UNIVERSAL PEER_PRIMITIVE  0)
#define BER_BOOLEAN_TAG			(PEER_UNIVERSAL PEER_PRIMITIVE  1)
#define BER_INTEGER_TAG			(PEER_UNIVERSAL PEER_PRIMITIVE  2)
#define BER_BIT_STRING_TAG		(PEER_UNIVERSAL PEER_PRIMITIVE  3)
#define BER_OCTET_STRING_TAG		(PEER_UNIVERSAL PEER_PRIMITIVE  4)
#define BER_NULL_TAG			(PEER_UNIVERSAL PEER_PRIMITIVE  5)
#define BER_OBJECT_IDENTIFIER_TAG	(PEER_UNIVERSAL PEER_PRIMITIVE  6)
#define BER_SEQUENCE_TAG		(PEER_UNIVERSAL PEER_CONSTRUCT 16)

#endif
