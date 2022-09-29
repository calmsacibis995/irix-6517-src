/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module netdbs.h, release 1.11 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.netdbs.h
 *sccs
 *sccs    1.11	96/02/12 13:16:40 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.10	95/01/24 13:00:37 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.9	93/06/02 22:13:30 randy
 *sccs	added include paths for AIX
 *sccs
 *sccs    1.8	93/05/28 02:16:06 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.7	93/04/26 17:54:18 randy
 *sccs	psos support
 *sccs
 *sccs    1.6	93/04/15 22:04:34 randy
 *sccs	support compilation with g++
 *sccs
 *sccs    1.5	93/03/05 09:34:45 randy
 *sccs	added manifests for target-specific versions
 *sccs
 *sccs    1.4	92/11/12 16:33:45 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.3	92/09/11 19:09:16 randy
 *sccs	really paranoid re-inclusion protection for Ultrix
 *sccs
 *sccs    1.2	92/08/31 14:25:46 randy
 *sccs	initial MSDOS support
 *sccs
 *sccs    1.1	92/03/20 11:27:15 randy
 *sccs	date and time created 92/03/20 11:27:15 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_NETDBSH			/* avoid re-inclusion		*/
#define AME_NETDBSH			/* if already included somewhere*/

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
 *	ame/netdbs.h - isolate references to unix version specific files
 *	that define structures used to access services in the network
 *	database.  In some environments, this file will be superfluous.
 *
 *	note the extensive re-inclusion protection.  this is necessary
 *	with some systems and c compilers that get confused if they
 *	see the same typdef or define twice.
 */
#include "ame/machtype.h"
#include "ame/socket.h"
#include "ame/appladdr.h"

#ifndef NETDBH_INCLUDED

#ifdef PEER_LYNX_PORT						/*LYNX*/
EXTERN_BEGIN							/*LYNX*/
#include <netdb.h>						/*LYNX*/
EXTERN_END							/*LYNX*/
#endif								/*LYNX*/

#ifdef PEER_VXWORKS_PORT					/*VXWORKS*/
EXTERN_BEGIN							/*VXWORKS*/
#include <netdb.h>						/*VXWORKS*/
EXTERN_END							/*VXWORKS*/
#endif								/*VXWORKS*/

#ifdef PEER_AIX_PORT						/*AIX*/
EXTERN_BEGIN							/*AIX*/
#include <netdb.h>						/*AIX*/
EXTERN_END							/*AIX*/
#endif								/*AIX*/

#ifdef PEER_GENERIC_PORT					/*GENERIC*/
EXTERN_BEGIN							/*GENERIC*/
#include <netdb.h>						/*GENERIC*/
EXTERN_END							/*GENERIC*/
#endif								/*GENERIC*/

#define NETDBH_INCLUDED
#endif


#endif
