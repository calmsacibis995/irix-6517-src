/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmpmsg.h, release 1.7 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmpmsg.h
 *sccs
 *sccs    1.7	96/02/12 13:19:23 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.6	95/01/24 13:01:23 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.5	94/05/03 22:30:24 randy
 *sccs	manifest for version 2 needed even in version 1 builds
 *sccs
 *sccs    1.4	93/08/19 13:52:41 randy
 *sccs	added structure elements for basic support of snmpv2
 *sccs
 *sccs    1.3	92/11/12 16:34:24 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/03/20 11:28:25 randy
 *sccs	flatten dependencies (some choke on heavy nesting)
 *sccs
 *sccs    1.1	91/09/03 16:51:48 randy
 *sccs	date and time created 91/09/03 16:51:48 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SNMPMSGH				/* avoid recursion	*/
#define SNMPMSGH

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
 *	snmpmsg.h - define outermost wrapper of SNMP protocol
 *
 ************************************************************************/

#include "ame/smitypes.h"
#include "ame/snmppdus.h"

/*
 *	Protocol version identifiers used internally
 */
#define SNMP_VERSION_1	0
#define SNMP_VERSION_2	2


#ifdef SNMPV2_SUPPORTED

/*
 *	The snmp_auth_msg struct provides the generic authentication wrapper.
 *	In total violation of ASN.1 rules, the structure of authInfo is
 *	(indirectly) defined by the privDst component in snmp_priv_msg.
 *
 *	The actual data structure for authInfo, along with the associated
 *	encoding and decoding functions, is packaged separately.  This
 *	simplifies adding new authentication algorithms as well as
 *	generating export versions.
 */
struct snmp_auth_msg
{
	Octets_t		authInfo;	/* defined by srcParty	*/
	Octets_t		authData;	/* plaintext SnmpMgmtCom*/
};


/*
 *	The generic structure for the privacy protection wrapper in
 *	snmp version two.
 */
struct snmp_priv_msg
{
	ObjId_t			privDst;	/* maps to key in table */
	Octets_t		privData;	/* encrypted SnmpAuthMsg*/
};

#endif



/*
 *	The snmp_message is the outermost wrapper in snmp version 1.
 *	In snmp version 2, it is encapsulated by privacy and authentication
 *	wrappers.  To simplify processing, this structure serves to hold
 *	both the V1 and the V2 information for SnmpMgmtCom.
 *
 *	The semantics of "community" maps to (dstPart,srcParty,context)
 */
struct snmp_message
{
	Number_t		version;		/* version one	*/
	struct octet_string	community;		/* version one	*/

#ifdef SNMPV2_SUPPORTED
	/*
	 *	These SNMPv2 constructs have been flattened from three
	 *	layers into one.  Doing so does not complicate processing,
	 *	and does permit greater re-use of common code.
	 */
	struct snmp_priv_msg	snmpPrivMsg;		/* outermost	*/
	struct snmp_auth_msg	snmpAuthMsg;		/* second layer */
	ObjId_t			dstParty;		/* in third layer*/
	ObjId_t			srcParty;		/* in third layer*/
	ObjId_t			context;		/* in third layer*/
#endif

	/*
	 *	We allow access to the raw data here since this is
	 *	needed for anyone who wants to do encryption in version one
	 *	NOTE: in snmpv1, the "data" element is the PDU.	 In the
	 *	snmpv2, it is the pdu element of SnmpMgmtCom, thus preserving
	 *	the semantics common to the protocols.
	 */
	struct octet_string	data;			/* BOTH		*/
	struct snmp_pdus	content;		/* BOTH		*/
};


#endif
