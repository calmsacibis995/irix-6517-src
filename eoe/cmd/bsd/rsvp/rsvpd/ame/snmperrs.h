/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmperrs.h, release 1.7 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmperrs.h
 *sccs
 *sccs    1.7	96/02/12 13:19:08 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.6	95/01/24 13:01:20 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.5	94/10/05 14:37:36 randy
 *sccs	allow access to error codes even when built without v2 support
 *sccs
 *sccs    1.4	93/08/19 13:39:23 randy
 *sccs	added error codes used by SNMPv2
 *sccs
 *sccs    1.3	92/11/12 16:34:21 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	91/09/05 12:58:24 timon
 *sccs	fixed typo
 *sccs
 *sccs    1.1	91/09/04 19:50:32 timon
 *sccs	date and time created 91/09/04 19:50:32 by timon
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SNMP_ERRS				/* avoid re-inclusion	*/
#define SNMP_ERRS				/* prevent re-inclusion */

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

/*
 *	snmperrs.h  -  standard snmp error codes
 */

#define SNMP_ERR_NO_ERROR	0
#define SNMP_ERR_TOO_BIG	1
#define SNMP_ERR_NO_SUCH_NAME	2
#define SNMP_ERR_BAD_VALUE	3
#define SNMP_ERR_READ_ONLY	4
#define SNMP_ERR_GEN_ERR	5

#define SNMP_ERR_NO_ACCESS		6
#define SNMP_ERR_WRONG_TYPE		7
#define SNMP_ERR_WRONG_LENGTH		8
#define SNMP_ERR_WRONG_ENCODING		9
#define SNMP_ERR_WRONG_VALUE		10
#define SNMP_ERR_NO_CREATION		11
#define SNMP_ERR_INCONSISTENT_VALUE	12
#define SNMP_ERR_RESOURCE_UNAVAILABLE	13
#define SNMP_ERR_COMMIT_FAILED		14
#define SNMP_ERR_UNDO_FAILED		15
#define SNMP_ERR_AUTHORIZATION_ERROR	16
#define SNMP_ERR_NOT_WRITEABLE		17
#define SNMP_ERR_INCONSISTENT_NAME	18

#endif
