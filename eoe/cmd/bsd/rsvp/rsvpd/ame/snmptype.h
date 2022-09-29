/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmptype.h, release 1.7 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmptype.h
 *sccs
 *sccs    1.7	96/02/12 13:20:21 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.6	95/01/24 13:01:39 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.5	92/11/12 16:34:39 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.4	92/07/25 18:05:15 randy
 *sccs	changed manifest for reinclusion prevention
 *sccs
 *sccs    1.3	91/09/09 23:34:12 randy
 *sccs	restructure of standard types, smi types, and snmp types
 *sccs
 *sccs    1.2	91/09/09 19:48:47 timon
 *sccs	moved attribute type definitions to ode/smi_types.h
 *sccs
 *sccs    1.1	91/09/03 16:51:53 randy
 *sccs	date and time created 91/09/03 16:51:53 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_SNMPTYPEH
#define AME_SNMPTYPEH

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
 *	snmptype.h  -  define the basic types of snmp `objects'
 *
 *	most of these come from smi - additional ones kept here are to
 *	hide information from users.
 *
 ************************************************************************/

#include "ame/smitypes.h"

/*
 *	The following primitive types are used frequently in SNMP code.
 *	They are defined here to allow tuning for performance in ports.
 */
typedef int			Tag_t;		/* 8 bits minimum	*/

#endif
