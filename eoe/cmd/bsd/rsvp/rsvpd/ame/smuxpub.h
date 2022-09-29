/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module smuxpub.h, release 1.7 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.smuxpub.h
 *sccs
 *sccs    1.7	96/02/12 13:18:40 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.6	95/01/24 13:01:12 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.5	93/10/26 15:38:02 randy
 *sccs	changed function prototype
 *sccs
 *sccs    1.4	93/04/08 17:28:56 randy
 *sccs	better use of READONLY construct
 *sccs
 *sccs    1.3	92/11/12 16:34:14 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:16:19 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/07/25 18:04:08 randy
 *sccs	date and time created 92/07/25 18:04:08 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_SMUXPUBH			/* avoid re-inclusion		*/
#define AME_SMUXPUBH			/* prevent re-inclusion		*/

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
 *	smuxpub.h - entry points into the smux library
 *
 ************************************************************************/

#include "ame/machtype.h"
#include "ame/stdtypes.h"		/* get needed data types	*/


#ifndef USE_PROTOTYPES

extern ubyte	*enc_smux_pdu();	/* encode SMUX pdu		*/
extern ubyte	*dec_smux_pdu();	/* decode SMUX pdu		*/

#else

#include "ame/smitypes.h"
#include "ame/smuxpdu.h"		/* get needed data types	*/

/*
 *	enc_smux_pdu - given the information content, generate a SMUX PDU
 */
extern ubyte	*enc_smux_pdu(ubyte		*buf_ptr,
			      Len_t		*len_ptr,
			      struct smux_pdu	*val_ptr);

/*
 *	dec_smux_pdu - decode a pdu of the SMUX protocol.
 */
extern ubyte	*dec_smux_pdu(ubyte		*buf_ptr,
			      Len_t		*len_ptr,
			      struct smux_pdu	*val_ptr);
#endif
#endif
