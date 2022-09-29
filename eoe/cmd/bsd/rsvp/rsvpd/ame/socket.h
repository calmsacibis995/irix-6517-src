/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module socket.h, release 1.14 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.socket.h
 *sccs
 *sccs    1.14	96/02/12 13:20:49 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.13	95/12/15 16:12:34 sam
 *sccs	Minor additions for the OS/2 port (PR#619)
 *sccs
 *sccs    1.12	95/01/24 13:01:47 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.11	94/11/02 10:21:41 randy
 *sccs	added support for NCR-specific transport
 *sccs
 *sccs    1.10	94/07/11 16:53:24 randy
 *sccs	merged winsock port
 *sccs
 *sccs    1.9	94/01/06 16:19:29 randy
 *sccs	added generic socket type
 *sccs
 *sccs    1.8	93/06/02 22:13:33 randy
 *sccs	added include paths for AIX
 *sccs
 *sccs    1.7	93/06/01 17:44:01 randy
 *sccs	add definition of a bad socket value for both dos and generic
 *sccs
 *sccs    1.6	93/05/28 14:28:11 randy
 *sccs	lynx support
 *sccs
 *sccs
 *sccs    1.5	93/05/28 02:16:10 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.4	93/04/26 17:56:18 randy
 *sccs	psos support
 *sccs
 *sccs    1.3	93/03/05 09:35:00 randy
 *sccs	added manifests for target-specific versions
 *sccs
 *sccs    1.2	92/11/12 16:34:46 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	92/08/31 14:14:28 randy
 *sccs	date and time created 92/08/31 14:14:28 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_SOCKETH				/* avoid re-inclusion	*/
#define AME_SOCKETH				/* prevent re-inclusion */

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
 *	socket.h  -  encapsulate system-dependent aspects of socket
 *	data types.  This file is structured to support treating sockets
 *	as "int" types (Unix) or as opaque pointers (for MS-DOS).  As
 *	part of your porting effort, you may chose to use some other
 *	convention or set of conditions.
 *
 *	Note that TCP and UDP sockets are considered distinct types at
 *	this level.  This simplifies porting to proprietary IPC mechanisms
 *	as replacements for the standard TCP/IP as a transport for SMUX.
 *
 ************************************************************************/

#include "ame/stdtypes.h"

#ifdef PEER_WINSOCK_PORT					/* WINSOCK */
#ifndef SYS_SOCKETH_INCLUDED					/* WINSOCK */
#include <winsock.h>						/* WINSOCK */
#define SYS_SOCKETH_INCLUDED					/* WINSOCK */
#endif								/* WINSOCK */
typedef SOCKET		Socket_type;				/* WINSOCK */
typedef Socket_type	UdpSocket_type;				/* WINSOCK */
typedef Socket_type	TcpSocket_type;				/* WINSOCK */
								/* WINSOCK */
/*								   WINSOCK
 *	see winsock spec clause 2.6.1				   WINSOCK
 */								/* WINSOCK */
#define BAD_SOCKET(X)	((X) == INVALID_SOCKET)			/* WINSOCK */
#define A_BAD_SOCKET	INVALID_SOCKET				/* WINSOCK */
								/* WINSOCK */
#else
#ifdef PEER_MSDOS_PORT						 /*MSDOS*/
								 /*MSDOS*/
typedef Void		*Socket_type;				 /*MSDOS*/
typedef Socket_type	UdpSocket_type;				 /*MSDOS*/
typedef Socket_type	TcpSocket_type;				 /*MSDOS*/
								 /*MSDOS*/
#define BAD_SOCKET(X)	((X) == NULL)				 /*MSDOS*/
#define A_BAD_SOCKET NULL					 /*MSDOS*/
								 /*MSDOS*/
#else

typedef int		Socket_type;
typedef Socket_type	UdpSocket_type;
typedef Socket_type	TcpSocket_type;

#define BAD_SOCKET(X)	((X) < 0)
#define A_BAD_SOCKET	-1

#endif
#endif


#ifndef SYS_TYPESH_INCLUDED

#ifdef PEER_VXWORKS_PORT
#include <types.h>
#endif

#ifdef PEER_LYNX_PORT
#include <types.h>
#endif

#ifdef PEER_AIX_PORT
#include <sys/types.h>
#endif

#ifdef PEER_GENERIC_PORT
#include <sys/types.h>
#endif

#define SYS_TYPESH_INCLUDED
#endif


#ifdef PEER_PSOS_PORT						 /*PSOS*/
								 /*PSOS*/
#include <pna.h>						 /*PSOS*/
								 /*PSOS*/
								 /*PSOS*/
/*								   PSOS
 * Structure used by kernel to store most			   PSOS
 * addresses.							   PSOS
 */								 /*PSOS*/
#define sockaddr sockaddr_in					 /*PSOS*/
#define sa_family sin_family					 /*PSOS*/
								 /*PSOS*/
#endif								 /*PSOS*/


#ifdef PEER_VXWORKS_PORT					 /*VXWORKS*/
#ifndef SYS_SOCKETH_INCLUDED					 /*VXWORKS*/
#include <socket.h>						 /*VXWORKS*/
#define SYS_SOCKETH_INCLUDED					 /*VXWORKS*/
#endif								 /*VXWORKS*/
#ifndef NETINET_INH_INCLUDED					 /*VXWORKS*/
#include <in.h>							 /*VXWORKS*/
#define NETINET_INH_INCLUDED					 /*VXWORKS*/
#endif								 /*VXWORKS*/
#endif								 /*VXWORKS*/

#ifdef PEER_LYNX_PORT						 /*LYNX*/
#ifndef SYS_SOCKETH_INCLUDED					 /*LYNX*/
#include "sys/uio.h"						 /*LYNX*/
#include "sys/socket.h"						 /*LYNX*/
#define SYS_SOCKETH_INCLUDED					 /*LYNX*/
#endif								 /*LYNX*/
#ifndef NETINET_INH_INCLUDED					 /*LYNX*/
#include "netinet/in.h"						 /*LYNX*/
#define NETINET_INH_INCLUDED					 /*LYNX*/
#endif								 /*LYNX*/
#endif								 /*LYNX*/

#ifdef PEER_AIX_PORT						 /*AIX*/
#ifndef SYS_SOCKETH_INCLUDED					 /*AIX*/
#include "sys/uio.h"						 /*AIX*/
#include "sys/socket.h"						 /*AIX*/
#define SYS_SOCKETH_INCLUDED					 /*AIX*/
#endif								 /*AIX*/
#ifndef NETINET_INH_INCLUDED					 /*AIX*/
#include "netinet/in.h"						 /*AIX*/
#define NETINET_INH_INCLUDED					 /*AIX*/
#endif								 /*AIX*/
#endif								 /*AIX*/

#ifdef PEER_GENERIC_PORT
#ifndef SYS_SOCKETH_INCLUDED
#ifndef PEER_OS2_PORT
#include "sys/uio.h"				/* to keep lint happy	*/
#else
#define BSD_SELECT
#include "ame\time.h"
#include <sys\select.h>
#include <utils.h>
#endif
#include "sys/socket.h"
#define SYS_SOCKETH_INCLUDED
#endif
#ifndef NETINET_INH_INCLUDED
#include "netinet/in.h"
#define NETINET_INH_INCLUDED
#endif
#endif

#ifdef PEER_NCR_PORT						/* NCR */
/*								   NCR
 *	A characteristic of the NCR environment is the use of	   NCR
 *	Unix domain sockets instead of the standard TCP/IP	   NCR
 *	transport mapping for SMUX sessions.			   NCR
 */								/* NCR */
#include "sys/un.h"						/* NCR */
#endif								/* NCR */

#endif

