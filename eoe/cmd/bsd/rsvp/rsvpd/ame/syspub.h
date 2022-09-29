/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module syspub.h, release 1.19 dated 96/07/23
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.syspub.h
 *sccs
 *sccs    1.19	96/07/23 13:54:27 sshanbha
 *sccs	Removed warning with AIX g++
 *sccs
 *sccs    1.18	96/07/22 17:47:17 sshanbha
 *sccs	Compile cleanly on SGI IRIX and AIX with g++ (PR#376) (PR#732)
 *sccs
 *sccs
 *sccs    1.17	96/07/19 17:46:40 mappelba
 *sccs	MS defines alloca in malloc.h (PR#756)
 *sccs
 *sccs    1.16	96/07/18 17:26:23 sshanbha
 *sccs	Fix problems with alloca for HP-CC compilation (PR#691)
 *sccs
 *sccs
 *sccs    1.15	96/06/12 16:00:53 sshanbha
 *sccs	Included file needed for compilation with hpux 10.x cc (PR#730)
 *sccs
 *sccs
 *sccs
 *sccs    1.14	96/02/12 13:21:33 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.13	95/12/15 16:12:47 sam
 *sccs	Minor additions for the OS/2 port (PR#619)
 *sccs
 *sccs    1.12	95/01/24 13:01:59 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.11	94/11/28 12:24:52 randy
 *sccs	removed definition of perror for winsock environment (PR#102)
 *sccs
 *sccs    1.10	94/07/11 16:50:32 randy
 *sccs	merged winsock port mods
 *sccs
 *sccs    1.9	94/05/03 22:29:15 randy
 *sccs	VRTX port
 *sccs
 *sccs    1.8	93/06/02 22:13:37 randy
 *sccs	added include paths for AIX
 *sccs
 *sccs    1.7	93/05/28 02:16:14 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.6	93/05/27 22:44:30 randy
 *sccs	better dos tolerance
 *sccs
 *sccs    1.5	93/04/26 17:56:14 randy
 *sccs	psos support
 *sccs
 *sccs    1.4	93/04/15 22:04:45 randy
 *sccs	support compilation with g++
 *sccs
 *sccs    1.3	92/11/12 16:34:57 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:14:27 randy
 *sccs	added really paranoid re-inclusion protection
 *sccs
 *sccs    1.1	92/08/31 14:07:17 randy
 *sccs	date and time created 92/08/31 14:07:17 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_SYSPUBH				/* avoid re-inclusion	*/
#define AME_SYSPUBH				/* prevent re-inclusion */

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
 *	<ame/syspub.h> - include headers for standard library functions.
 *	These are encapsulated here to simplify porting in environments
 *	where the standards headers and prototypes aren't where they're
 *	supposed to be.	 The paths used here are the POSIX ones.
 */
#include "ame/machtype.h"

/*
 *	Psos lacks everything....
 */
#ifndef PEER_PSOS_PORT
EXTERN_BEGIN

#ifndef STDIOH_INCLUDED						/* STDIO */
#ifdef PEER_VXWORKS_PORT			/* VXWORKS */	/* STDIO */
#include <stdioLib.h>				/* VXWORKS */	/* STDIO */
#else								/* STDIO */
#include <stdio.h>						/* STDIO */
#endif								/* STDIO */
#define STDIOH_INCLUDED						/* STDIO */
#endif								/* STDIO */

#ifndef STDLIBH_INCLUDED
#include <stdlib.h>
#define STDLIBH_INCLUDED
#endif

#ifndef STRINGH_INCLUDED
#ifdef PEER_VXWORKS_PORT
#include <vxWorks.h>
#include <strLib.h>
#else
#include <string.h>
#endif
#define STRINGH_INCLUDED
#endif

#ifndef CTYPEH_INCLUDED
#include <ctype.h>
#define CTYPEH_INCLUDED
#endif

#ifndef ALLOCAH_INCLUDED
#ifdef PEER_HPUX_PORT
#define  alloca
#include <alloca.h>
#define ALLOCAH_INCLUDED
#else
#ifdef PEER_WINSOCK_PORT
#include <malloc.h>
#define ALLOCAH_INCLUDED
#endif
#endif
#endif

#ifdef  PEER_IRIX_PORT
#ifndef _SGI_SOURCE
#define _SGI_SOURCE
#endif
#ifndef _BSD_TYPES
#define _BSD_TYPES
#endif
#ifndef BSTRINGH_INCLUDED
#include <bstring.h>
#define BSTRINGH_INCLUDED
#endif
#ifndef PEER_NO_STATIC_FORWARD_DECL
#define PEER_NO_STATIC_FORWARD_DECL
#endif
#endif

#ifndef STRINGSH_INCLUDED
#ifdef PEER_AIX_PORT
#include <strings.h>
#define STRINGSH_INCLUDED
#endif
#endif

#ifndef UNISTDH_INCLUDED
#ifdef PEER_OS2_PORT
/* just so we can use GENERIC for everything else */
#else
#ifdef PEER_GENERIC_PORT
#ifndef PEER_VRTX_PORT
#include <unistd.h>
#endif
#endif
#ifdef PEER_LYNX_PORT
#include <unistd.h>
#endif
#ifdef PEER_AIX_PORT
#include <unistd.h>
#endif
#endif
#define UNISTDH_INCLUDED
#endif

EXTERN_END
#endif

#endif

