/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmpasn.h, release 1.6 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmpasn.h
 *sccs
 *sccs    1.6	96/02/12 13:18:54 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.5	95/02/06 18:35:03 randy
 *sccs	avoid macro conflict with NT (PR#212)
 *sccs
 *sccs    1.4	95/01/24 13:01:16 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.3	93/08/19 13:51:42 randy
 *sccs	added tags specific to snmpv2
 *sccs
 *sccs    1.2	92/11/12 16:34:17 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	91/09/03 16:51:45 randy
 *sccs	date and time created 91/09/03 16:51:45 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SNMPASNH
#define SNMPASNH

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
 *	snmpasn.h  -  asn.1 definitions for SNMOP support
 *
 ************************************************************************/

#include "ame/asn.h"

#define SNMP_EMPTY_TAG	BER_NULL_TAG		/* ASN.1 Null type	*/
#define SNMP_STRING_TAG BER_OCTET_STRING_TAG	/* Simple Octet string	*/
#define SNMP_OBJECT_TAG BER_OBJECT_IDENTIFIER_TAG/* Object Identifier	*/
#define SNMP_NUMBER_TAG BER_INTEGER_TAG		/* Simple integer	*/
#define SNMP_ADDR_TAG	(PEER_APPLICATION PEER_PRIMITIVE 0)/* 4 bytes	*/
#define SNMP_CNTER_TAG	(PEER_APPLICATION PEER_PRIMITIVE 1)/* Uns. Int.	*/
#define SNMP_GUAGE_TAG	(PEER_APPLICATION PEER_PRIMITIVE 2)/* Uns. Int.	*/
#define SNMP_TICKS_TAG	(PEER_APPLICATION PEER_PRIMITIVE 3)/* Uns. Int.	*/
#define SNMP_OPAQUE_TAG (PEER_APPLICATION PEER_PRIMITIVE 4)/* Octets	*/
						 /* snmpV2 additions	*/
#ifdef SNMPV2_SUPPORTED
#define SNMP_NSAP_TAG	(PEER_APPLICATION PEER_PRIMITIVE 5)/* Octets	*/
#define SNMP_CNTR64_TAG (PEER_APPLICATION PEER_PRIMITIVE 6)/* 64-bit cnt*/
#define SNMP_UINT_TAG	(PEER_APPLICATION PEER_PRIMITIVE 7)/* 32-bi uint*/
#define SNMP_BITS_TAG	BER_BIT_STRING_TAG	 /* bit string		*/
#define SNMP_NOOBJ_TAG	(PEER_CONTEXT PEER_PRIMITIVE 0)	 /* class unknown*/
#define SNMP_NOINST_TAG (PEER_CONTEXT PEER_PRIMITIVE 1)	 /* no such inst*/
#define SNMP_ENDMIB_TAG (PEER_CONTEXT PEER_PRIMITIVE 2)	 /* end MIB view*/
#endif



#define SNMP_GET_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 0)
#define SNMP_GET_NEXT_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 1)
#define SNMP_GET_RESP_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 2)
#define SNMP_SET_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 3)
#define SNMP_TRAP_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 4)

#ifdef SNMPV2_SUPPORTED
/*
 *	Define Tags for PDU types specific to SNMPv2
 */
#define SNMPV2_BULK_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 5)
#define SNMPV2_INFORM_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 6)
#define SNMPV2_TRAP_PDU_TAG	(PEER_CONTEXT PEER_CONSTRUCT 7)

/*
 *	Define Tags for components of management communications that are
 *	peculiar to SNMPv2.  The names correspond to those used in RFC 1445
 *	(Oddly enough, this stuff is never properly defined in an ASN.1
 *	module!	 There's only the `English' text of rfc 1445 and 1446.)
 */
#define SNMPV2_PRIV_MSG_TAG	(PEER_CONTEXT PEER_CONSTRUCT 1)
#define SNMPV2_PRIV_DATA_TAG	(PEER_CONTEXT PEER_PRIMITIVE 1)
#define SNMPV2_AUTH_MSG_TAG	(PEER_CONTEXT PEER_CONSTRUCT 1)
#define SNMPV2_MGMT_COM_TAG	(PEER_CONTEXT PEER_CONSTRUCT 2)
#define SNMPV2_AUTH_INFO_TAG	(PEER_CONTEXT PEER_CONSTRUCT 2)

#endif

#endif
