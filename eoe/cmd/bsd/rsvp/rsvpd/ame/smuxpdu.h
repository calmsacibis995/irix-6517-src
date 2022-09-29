/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module smuxpdu.h, release 1.9 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.smuxpdu.h
 *sccs
 *sccs    1.9	96/02/12 13:18:25 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.8	95/10/16 15:48:40 randy
 *sccs	moved smux close reason codes out of private ode header (PR#578)
 *sccs
 *sccs    1.7	95/01/24 13:01:09 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.6	93/11/02 15:42:11 randy
 *sccs	support for NCR protocol
 *sccs
 *sccs    1.5	93/08/19 13:42:21 randy
 *sccs	added definitions to allow support of snmpv2
 *sccs
 *sccs    1.4	92/11/12 16:34:10 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.3	92/09/11 19:16:15 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.2	92/03/20 11:28:08 randy
 *sccs	flatten dependencies (some preprocessors choke on heavy nesting)
 *sccs
 *sccs    1.1	91/09/03 16:51:41 randy
 *sccs	date and time created 91/09/03 16:51:41 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SMUX_PDUH				/* avoid revursion	*/
#define SMUX_PDUH

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
 *	smuxpdu.h  -  definitions of the SMUX protocol PDUs
 *
 ************************************************************************/

#include "ame/smitypes.h"
#include "ame/snmppdus.h"
#include "ame/snmpmsg.h"

/*
 *	define the internal codes for the SMUX pdu types.  these are
 *	contiguous with the codes for the SNMP types as defined in
 *	ame/snmppdus.h	Be sure to check common/ports/.../display.c and
 *	common/smux/.../smuxpdus.c before messing with these.
 */
#ifdef SNMPV2_SUPPORTED
#define SNMP_MSG_PDU	5+3
#define SMUX_OPEN_PDU	6+3
#define SMUX_CLOSE_PDU	7+3
#define SMUX_RREQ_PDU	8+3
#define SMUX_RRSP_PDU	9+3
#define SMUX_SOUT_PDU	10+3
#ifdef ALLOW_NCR_SMUX
#define BOGUS_SMUX_RRSP_PDU	11+3
#endif
#else
#define SNMP_MSG_PDU	5
#define SMUX_OPEN_PDU	6
#define SMUX_CLOSE_PDU	7
#define SMUX_RREQ_PDU	8
#define SMUX_RRSP_PDU	9
#define SMUX_SOUT_PDU	10
#ifdef ALLOW_NCR_SMUX
#define BOGUS_SMUX_RRSP_PDU	11
#endif
#endif


/*
 *	The following structures are direct mappings from what is defined
 *	in RFC 1227.
 */
struct smux_simple_open
{
	Number_t		version;	/* protocol version number*/
	struct octet_string	identity;	/* register subagent's ID */
	struct octet_string	description;	/* text describe subagent */
	struct octet_string	password;	/* trivial authentication */
};

/*
 *	To permit extenisbility, various flavors of open are provided
 *	for in the protocol.  The only one described in RFC 1227 is
 *	the "simple" open.
 */
struct smux_open_pdu
{
	int	kind;				/* discriminator	*/

	union
	{
		struct smux_simple_open simple_open;
	} variant;
};

/*
 *	The close pdu allows the party requesting termination of a session
 *	to provide a reason.  The reason code definitions follow.
 */
struct smux_close_pdu
{
	Number_t	reason;
};

/*
 *	Smux Close reasons
 */
#define SMUX_GOING_DOWN			0
#define SMUX_UNSUPPORTED_VERSION	1
#define SMUX_PACKET_FORMAT		2
#define SMUX_PROTOCOL_ERROR		3
#define SMUX_INTERNAL_ERROR		4
#define SMUX_AUTHENTICATION_FAILURE	5

/*
 *	The registration / deregistration request PDU carries a subtree
 *	identifier, a subtree priority, and an indication of whether the
 *	request is to register or to dergister a subtree.
 */
struct smux_rreq_pdu
{
	ObjId_t			subtree;
	Number_t		priority;
	Number_t		operation;
};

/*
 *	The registration response PDU contains a single field; if its
 *	value is positive, it indicates the priority of the subtree
 *	operated on in the corresponding registration request; if
 *	negative, it indicates that a failure has occurred.
 */
struct smux_rrsp_pdu
{
	Number_t	priority;	/* < 0 => failure		*/
};

/*
 *	The smux_sout_pdu represents commit and rollback requests
 */
struct smux_sout_pdu
{
	Number_t	operation;	/* 0=> commit, 1=> rollback	*/
};


/*
 *	We represent the various kinds of SMUX pdus as a dscriminated
 *	union.  The specific PDU types are described above.  A common
 *	variant of the SMUX protocol uses raw SNMP messages (rather 
 *	than just the PDU portion of an SNMP message), and is provided
 *	for here as a PEER extension.
 */
struct smux_pdu
{
	int	kind;				/* discriminator	*/

	union
	{
		struct smux_open_pdu	open;	/* session establishment*/
		struct smux_close_pdu	close;	/* session termination	*/
		struct smux_rreq_pdu	rreq;	/* (de)registration	*/
		struct smux_rrsp_pdu	rrsp;	/* registration response*/
		struct smux_sout_pdu	sout;	/* commit / rollback	*/
		struct snmp_message	snmp;	/* Peer extension	*/
	} variant;
};

#endif
