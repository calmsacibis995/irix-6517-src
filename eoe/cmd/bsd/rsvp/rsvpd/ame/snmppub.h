/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmppub.h, release 1.9 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmppub.h
 *sccs
 *sccs    1.9	96/02/12 13:19:53 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.8	95/01/24 13:01:31 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.7	93/10/26 15:37:27 randy
 *sccs	changed function prototype
 *sccs
 *sccs    1.6	93/08/19 13:38:50 randy
 *sccs	moved prototypes to support snmpv2
 *sccs
 *sccs    1.5	93/04/08 17:28:59 randy
 *sccs	better use of READONLY construct
 *sccs
 *sccs    1.4	92/11/12 16:34:31 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.3	92/10/23 15:29:02 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.2	92/09/11 19:16:22 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/07/25 18:04:06 randy
 *sccs	date and time created 92/07/25 18:04:06 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SNMPPUBH			/* avoid re-inclusion		*/
#define SNMPPUBH			/* prevent re-inclusion		*/

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
 *	snmppub.h - entry points into the snmp library
 *
 ************************************************************************/

#include "ame/machtype.h"
#include "ame/stdtypes.h"		/* get necessary data types	*/


#ifndef USE_PROTOTYPES

extern ubyte	*dec_int();		/* decode a signed integer	*/
extern ubyte	*dec_int_wtag();	/* decode integer with tag	*/
extern ubyte	*dec_oct_wtag();	/* decode tag, length, string	*/
extern ubyte	*dec_snmp_msg();	/* decode snmp message		 */
extern ubyte	*dec_snmp_pdu();	/* decode snmp pdu (from message)*/
extern ubyte	*dec_snmp_pdus();	/* decode snmp pdus		*/
extern ubyte	*dec_snmp_trap();	/* decode snmp trap from message */
extern ubyte	*dec_tag_and_len();	/* decode Tag and Length in BER */
extern ubyte	*dec_uint_wtag();	/* decode integer with tag	*/
extern ubyte	*enc_int();		/* encode number types		*/
extern ubyte	*enc_int_wtag();	/* encode number, len, tag	*/
extern ubyte	*enc_oct_wtag();	/* encode octet string, len, tag*/
extern ubyte	*enc_snmp_msg();	/* encode snmp message		*/
extern ubyte	*enc_snmp_pdu();	/* encode snmp PDU		*/
extern ubyte	*enc_snmp_pdus();	/* generic snmp encoder		*/
extern ubyte	*enc_snmp_trap();	/* encode snmp trap		*/
extern ubyte	*enc_tag_and_len();	/* encode BER tag and length	*/
extern ubyte	*enc_uint_wtag();	/* encode counter, len, tag	*/

#else

/*
 *	If we're putting in prototypes, we need more type information
 */
#include "ame/smitypes.h"
#include "ame/snmptype.h"		/* get required data types	*/
#include "ame/snmpvbl.h"
#include "ame/snmppdus.h"
#include "ame/snmpmsg.h"

/*
 *	decode a signed integer
 */
extern ubyte	*dec_int(ubyte		*buf_ptr,
			 Len_t		*len_ptr,
			 Number_t	*val_ptr);

/*
 *	decode an integer value with tag/length
 */
extern ubyte	*dec_int_wtag(ubyte		*buf_ptr,
			      Len_t		*len_ptr,
			      Number_t		*val,
			      READONLY Tag_t	expect);

/*
 *	decode an octet string with its tag and length
 */
extern ubyte	*dec_oct_wtag(ubyte			*buf_ptr,
			      Len_t			*len_ptr,
			      struct octet_string	*val,
			      READONLY Tag_t		expect);

/*
 *	Decode an SNMP message
 */
extern ubyte	*dec_snmp_msg(ubyte			*buf_ptr,
			      Len_t			*len_ptr,
			      struct snmp_message	*val_ptr);

/*
 *	Decode an SNMP PDU (contained within SNMP message)
 */
extern ubyte	*dec_snmp_pdu(ubyte		*buf_ptr,
			      Len_t		*len_ptr,
			      struct snmp_pdus	*val_ptr);

/*
 *	dec_snmp_pdus - an snmp or smux message has been unwrapped to the
 *			PDU level.  this function is resposible for
 *			recognizing the specific pdu type and parsing it
 */
extern ubyte	*dec_snmp_pdus(ubyte		*buf_ptr,
			       Len_t		*len_ptr,
			       struct snmp_pdus *val_ptr);

/*
 *	Decode an SNMP TRAP (contained within SNMP message)
 */
extern ubyte	*dec_snmp_trap(ubyte		*buf_ptr,
			       Len_t		*len_ptr,
			       struct snmp_pdus *val_ptr);

/*
 *	decode Tag and Length in BER
 */
extern ubyte	*dec_tag_and_len(ubyte		*buf_ptr,
				 Len_t		*len_ptr,
				 Tag_t		*tag,
				 Len_t		*len,
				 READONLY Tag_t expect);

/*
 *	decode an unsigned integer value with tag/length
 */
extern ubyte	*dec_uint_wtag(ubyte		*buf_ptr,
			      Len_t		*len_ptr,
			      Counter_t		*val,
			      READONLY Tag_t	expect);

/*
 *	Encoded value portion of an signed number using BER
 */
extern ubyte	*enc_int(ubyte			*buf_ptr,
			 Len_t			*len_ptr,
			 READONLY Number_t	value);

/*
 *	Encode, TAG, length, and value of signed number in BER (working
 *	backwards)
 */
extern ubyte	*enc_int_wtag(ubyte		*buf_ptr,
			      Len_t		*len_ptr,
			      READONLY Number_t value,
			      READONLY Tag_t	tag);

/*
 *	Encode, TAG, length, and value of octet string in BER (working
 *	backwards)
 */
extern ubyte	*enc_oct_wtag(ubyte				*buf_ptr,
			      Len_t				*len_ptr,
			      READONLY struct octet_string	*val_ptr,
			      READONLY Tag_t			tag);

/*
 *	Encode an SNMP message using BER, working backwards
 */
extern ubyte	*enc_snmp_msg(ubyte				*buf_ptr,
			      Len_t				*len_ptr,
			      struct snmp_message		*val_ptr);

/*
 *	Encode an SNMP PDU (will be embedded within message or smux, usually)
 *	use BER, work backwards.
 */
extern ubyte	*enc_snmp_pdu(ubyte			*buf_ptr,
			      Len_t			*len_ptr,
			      READONLY struct snmp_pdus *val_ptr);


/*
 *	enc_snmp_pdus - generic encoder for all snmp pdu types.
 */
extern ubyte	*enc_snmp_pdus(ubyte			*buf_ptr,
			       Len_t			*len_ptr,
			       READONLY struct snmp_pdus *val_ptr);

/*
 *	Encode an SNMP TRAP (will be embedded within message or smux, usually)
 *	use BER, work backwards.
 */
extern ubyte	*enc_snmp_trap(ubyte			*buf_ptr,
			       Len_t			*len_ptr,
			       READONLY struct snmp_pdus *val_ptr);

/*
 *	Encode tag and length using BER rules (working backwards)
 */
extern ubyte	*enc_tag_and_len(ubyte		*buf_ptr,
				 Len_t		*len_ptr,
				 READONLY Tag_t tag,
				 READONLY Len_t len);

/*
 *	Encode, TAG, length, and value of unsigned counter in BER (working
 *	backwards)
 */
extern ubyte	*enc_uint_wtag(ubyte			*buf_ptr,
			       Len_t			*len_ptr,
			       READONLY Counter_t	value,
			       READONLY Tag_t		tag);

#endif
#endif
