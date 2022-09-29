/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmpvbl.h, release 1.10 dated 96/07/09
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmpvbl.h
 *sccs
 *sccs    1.10	96/07/09 18:22:51 rpresuhn
 *sccs	bumped minimum maximum message size default to 1500 bytes (PR#545)
 *sccs
 *sccs    1.9	96/02/12 13:20:35 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.8	95/01/24 13:01:43 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.7	94/05/03 22:29:50 randy
 *sccs	added manifest to track maximim v1 type
 *sccs
 *sccs    1.6	93/08/19 13:38:03 randy
 *sccs	support for SNMPv2 structures
 *sccs
 *sccs    1.5	92/11/12 16:34:42 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.4	92/09/11 19:16:29 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.3	92/08/31 14:08:46 randy
 *sccs	support network address type, even if it is bogus
 *sccs
 *sccs    1.2	91/09/19 17:24:44 randy
 *sccs	pdu size limits
 *sccs
 *sccs    1.1	91/09/03 16:51:56 randy
 *sccs	date and time created 91/09/03 16:51:56 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SNMPVBLH
#define SNMPVBLH

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
 *	snmpvbl.h  -  define the structure of SNMP variable bindings
 *
 ************************************************************************/

#include "ame/smitypes.h"
#include "ame/snmptype.h"

/*
 *	The object types can be referenced by SNMP-internal indexes
 *	or by the TAGs used in protocol.  Upper-level functions should
 *	use the indexes; only low-level protocol stuff should use the tags.
 *
 *	These indexes MUST be kept in alignment with the table in
 *	snmptype.c in the common/snmp/src directory
 *
 *	Note that some of these are error codes, indicating null information.
 */
#define SNMP_EMPTY	0		/* ASN.1 Null type		*/
#define SNMP_STRING	1		/* Simple Octet string		*/
#define SNMP_OBJECT	2		/* Object Identifier		*/
#define SNMP_NUMBER	3		/* Simple integer		*/
#define SNMP_ADDR	4		/* Application 0: 4-byte string */
#define SNMP_CNTER	5		/* Application 1: Integer	*/
#define SNMP_GUAGE	6		/* Application 2: Integer	*/
#define SNMP_TICKS	7		/* Application 3: Integer	*/
#define SNMP_OPAQUE	8		/* Application 4: Octet string	*/
#define SNMP_NETADDR	9		/* Obsolete Choice, Application0*/
#define MAX_V1_KIND	SNMP_NETADDR
					/* begin snmpv2 additions...	*/

#define SNMP_NSAP	10		/* Application 5: Octet string	*/
#define SNMP_CNTER64	11		/* Application 6: 64-bit int	*/
#define SNMP_UINT32	12		/* Application 7: 32 unsigned	*/
#define SNMP_NOSUCHOBJ	13		/* Context 0: no such name	*/
#define SNMP_NOSUCHINST 14		/* Context 1: no such instance	*/
#define SNMP_ENDOFMIB	15		/* Context 2: end of mib view	*/
#define SNMP_BITS	16		/* bit strings			*/


/*
 *	The snmp_object structure holds the data type and value for an
 *	attribute.
 */
struct	snmp_object
{
	int	kind;			/* upper-level semantics	*/

	union	
	{
	    Number_t	number;		/* Signed 32-bit Integer	*/

	    Counter_t	counter;	/* counters, guages, and ticks	*/
	    Counter_t	guage;		/* are all unsigned integers	*/
	    Counter_t	ticks;		/* requiring 32 bits		*/

	    IpAddr_t	address;	/* IP addresses go here...	*/

	    ObjId_t	object;		/* objects, strings, and opaques*/
	    Octets_t	string;		/* all have the same underlying */
	    Octets_t	arbitrary;	/* octet string representation	*/
	    NsapAddr_t	nsap_address;	/* OSI addresses		*/

	    Bits_t	bits;		/* bit strings			*/

	    Counter64_t counter64;	/* 64-bit counters		*/
	} variant;
};


/*
 *	a var_bind relates an attribute name (object identifier) to a value
 */
struct	var_bind
{
	ObjId_t			name;
	struct snmp_object	value;
};


/*
 *	The architectural limit for the number of bindings (and range
 *	constraint on fields such as max_repetitions) is specified in rfc1448.
 */
#define max_bindings	2147483647L

/*
 *	The maximum number of variable bindings assumes a very pathological
 *	pdu and binding element structure:
 *		SEQ Len=5 OBJID Len=1 X.Y  VAL Len=0
 *
 *	SNMP_MSG_MAX must never be configured to less that 484 bytes.
 */
#define SNMP_MSG_MAX	1500
#define VAR_BIND_MAX	(SNMP_MSG_MAX / 7)

struct	var_bind_list
{
	int		count;
	struct var_bind element[VAR_BIND_MAX];
};

#endif
