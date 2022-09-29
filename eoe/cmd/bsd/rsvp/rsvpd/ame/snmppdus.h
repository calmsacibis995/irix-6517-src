/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmppdus.h, release 1.8 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmppdus.h
 *sccs
 *sccs    1.8	96/02/12 13:19:38 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.7	95/01/24 13:01:28 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.6	93/08/19 13:53:26 randy
 *sccs	added support for snmpv2
 *sccs
 *sccs    1.5	92/11/12 16:34:28 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.4	92/07/25 18:05:51 randy
 *sccs	changed manifest name to prevent re-inclusion
 *sccs
 *sccs    1.3	92/03/20 11:28:41 randy
 *sccs	flatten dependencies (some choke on heavy nesting)
 *sccs
 *sccs    1.2	91/09/09 23:33:46 randy
 *sccs	change structure element names for consistances, align with network address type changes
 *sccs
 *sccs    1.1	91/09/03 16:51:50 randy
 *sccs	date and time created 91/09/03 16:51:50 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_SNMPPDUSH				/* avoid recursion	*/
#define AME_SNMPPDUSH

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
 *	snmppdus.h - define the various snmp pdu types
 *
 ************************************************************************/

#include "ame/smitypes.h"
#include "ame/snmpvbl.h"

/*
 *	The following are the PDU `kind' used at higher levels.	 Do NOT
 *	confuse these with TAGs, used in encoding and decoding.
 */
#define SNMP_GET_PDU		0
#define SNMP_GET_NEXT_PDU	1
#define SNMP_GET_RESP_PDU	2
#define SNMP_SET_PDU		3
#define SNMP_TRAP_PDU		4

#ifdef SNMPV2_SUPPORTED
#define SNMPV2_BULK_PDU		5
#define SNMPV2_INFORM_PDU	6
#define SNMPV2_TRAP_PDU		7
#endif


/*
 *	The SNMP GET, GET-NEXT, GET-RESPONSE, and SET PDUs all have the
 *	same structure.
 */
struct snmp_pdu
{
	Number_t		request_id;
	Number_t		error_status;
	Number_t		error_index;
	struct var_bind_list	bindings;	/* variable bindings	*/
};


/*
 *	Allow access to get bulk fields by their familiar names.  The
 *	actual PDU layout is identical to the ordinary snmp_pdu.
 */
#define non_repeaters	error_status
#define max_repetitions error_index


/*
 *	The SNMP TRAP PDU has a different structure.  Beware the agent_addr
 *	element.  If the protocol is used over non-IP networks, this element
 *	should really be a structure with an address-type discriminator.
 */
struct snmp_trap_pdu
{
	struct octet_string	enterprise;
	NetAddr_t		agent_addr;
	Number_t		generic_trap;
	Number_t		specific_trap;
	Counter_t		time_stamp;
	struct var_bind_list	bindings;	/* variable bindings	*/
};

/*
 *	All SNMP pdus fall into one of two general formats
 */
struct snmp_pdus
{
	short	kind;			/* identifies Get/Set/ Etc.	*/

	union
	{
		struct snmp_pdu		pdu;
		struct snmp_trap_pdu	trap_pdu;
	} variant;
};

#endif
