/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module smitypes.h, release 1.11 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.smitypes.h
 *sccs
 *sccs    1.11	96/02/12 13:17:55 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.10	96/01/25 11:36:12 santosh
 *sccs	Support Unsigned32 as a syntax type (PR#635)
 *sccs
 *sccs    1.9	95/01/24 13:01:01 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.8	93/10/21 18:18:36 randy
 *sccs	added Integer32 type
 *sccs
 *sccs
 *sccs    1.7	93/08/19 13:41:21 randy
 *sccs	added types for preliminary snmpv2 support
 *sccs
 *sccs    1.6	92/11/12 16:34:03 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.5	92/07/25 18:06:20 randy
 *sccs	modified type definitions for better portability
 *sccs
 *sccs    1.4	91/12/13 13:59:02 timon
 *sccs	another syntax error
 *sccs
 *sccs    1.3	91/12/13 13:56:09 timon
 *sccs	fixed syntax error
 *sccs
 *sccs    1.2	91/12/13 13:22:48 timon
 *sccs	added derivative types defined in mib II
 *sccs
 *sccs    1.1	91/09/09 23:34:30 randy
 *sccs	date and time created 91/09/09 23:34:30 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SMITYPESH
#define SMITYPESH

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


#include "ame/stdtypes.h"

/*
 *	The following type is used for the lengths of things.  It may
 *	be signed or unsiged as meets optimization requirements, and
 *	must be able to represent at least 0..0ffff.  A system's native
 *	integer type is typically a good choice, except on 80x86 machines,
 *	where long is needed to get the required range.
 */
typedef int			Len_t;		/* 16 bits minimum	*/


/*
 *	All things of variable length are ultimately defined as the following
 */
struct octet_string
{
	Len_t	len;			/* number of bytes in value	*/
	ubyte	*val;			/* -> content, len bytes long	*/
};


/*
 *	We can handle real ASN.1 bit strings, as well as these wimpy SNMPv2
 *	ones.  Although the layout of this structure is identical to that
 *	of the octet string, note that the element semantics are NOT the same.
 */
struct bit_string
{
	Len_t	bit_cnt;		/* number of BITs in this string*/
	ubyte	*bit_val;		/* -> first byte of value	*/
};


/*
 *	low level primitive types.  User programs should NOT make
 *	use of these types.  Although snmpv1 counter and number representation
 *	is not specified, we align it here with snmpv2 usage to be 32 bits.
 */
typedef int32			Number_t;	/* 32 bits, signed	*/
typedef uint32			Counter_t;	/* 32 bits, unsigned	*/
typedef struct octet_string	ObjId_t;
typedef struct octet_string	Octets_t;
typedef uint32			IpAddr_t;	/* 32 bits worth	*/
typedef uint64			Counter64_t;	/* 64 bits, unsigned	*/
typedef struct bit_string	Bits_t;		/* bit strings		*/


/*
 *	Network Addresses in SNMP are badly screwed up.	 In SNMPv1 they
 *	could pretty much be collapsed onto the IP address space.  In 
 *	SNMPv2 their relationship to OSI NSAPs was clarified, requiring
 *	an octet string syntax.
 */
typedef IpAddr_t		NetAddr_t;
typedef Octets_t		NsapAddr_t;


/*
 *	user programs use these types when defining attribute variables.
 */
typedef Number_t	INTEGER;
typedef Octets_t	OCTETSTRING;
typedef ObjId_t		OBJECTIDENTIFIER;
typedef IpAddr_t	IpAddress;
typedef NetAddr_t	NetworkAddress;
typedef Counter_t	Counter;
typedef Counter_t	Gauge;
typedef Counter_t	TimeTicks;
typedef Octets_t	Opaque;
typedef Counter		Gauge32;		/* snmpv2 data type	*/
typedef uint32		UInteger32;		/* snmpv2 data type	*/
typedef uint32		Unsigned32;		/* snmpv2 data type	*/
typedef int32		Integer32;		/* snmpv2 data type	*/
typedef NsapAddr_t	NsapAddress;		/* snmpv2 data type	*/
typedef Bits_t		BITSTRING;		/* snmpv2 data type	*/
typedef Counter		Counter32;		/* snmpv2 data type	*/
typedef Counter64_t	Counter64;		/* snmpv2 data type	*/


/*
 *	MIB II defines the following types
 */
typedef OCTETSTRING	DisplayString;
typedef OCTETSTRING	PhysAddress;


#endif
