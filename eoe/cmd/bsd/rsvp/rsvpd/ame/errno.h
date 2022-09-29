/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module errno.h, release 1.16 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.errno.h
 *sccs
 *sccs    1.16	96/02/12 13:15:22 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.15	95/12/21 18:49:55 randy
 *sccs	fixed unbalanced comment (PR#627)
 *sccs
 *sccs    1.14	95/12/15 16:11:48 sam
 *sccs	Minor additions for the OS/2 port (PR#619)
 *sccs
 *sccs    1.13	95/08/30 01:53:56 randy
 *sccs	corrected typo for winsock
 *sccs
 *sccs    1.12	95/08/29 20:15:01 sam
 *sccs	Added EWOULDBLOCK mapping for winsock.
 *sccs
 *sccs    1.11	95/08/25 15:55:44 sam
 *sccs	Interim checkin for customer support.
 *sccs
 *sccs    1.10	95/07/20 17:27:02 sam
 *sccs	Minor additions for Winsock support.
 *sccs
 *sccs    1.9	95/01/24 13:00:14 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.8	94/11/28 12:13:37 randy
 *sccs	do not define three error codes if under winsock
 *sccs
 *sccs    1.7	93/10/21 11:19:09 randy
 *sccs	added error code for systems with blocking connect
 *sccs
 *sccs    1.6	93/05/28 02:16:02 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.5	93/04/26 17:54:11 randy
 *sccs	psos support
 *sccs
 *sccs    1.4	93/03/22 10:17:58 randy
 *sccs	added additional values missing in some environments
 *sccs
 *sccs    1.3	92/11/12 16:33:30 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/01/14 13:39:28 randy
 *sccs	modified conditions to work with systems with deficient errno files
 *sccs
 *sccs    1.1	91/12/26 12:19:48 randy
 *sccs	date and time created 91/12/26 12:19:48 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AMEERRNOH
#define AMEERRNOH

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
 *	This include file serves only to minimize the number of source
 *	changes necessary when porting to environments lacking the standard
 *	/usr/include/errno.h (or /usr/include/sys/errno.h) headers.
 */

/*
 * In the OS/2 environment, network errors are re/defined in the file
 * NERROR.H
 */
#ifdef PEER_OS2_PORT
#include <nerrno.h>
#endif
#ifdef PEER_PSOS_PORT						/* PSOS */
#include <pna.h>						/* PSOS */
#define Sock_errno	pna_errno()				/* PSOS */
#else
#include <errno.h>
#ifdef PEER_WINSOCK_PORT                                        /* WINSOCK */
#define Sock_errno      WSAGetLastError()                       /* WINSOCK */
#else
#define Sock_errno	errno
#endif                                                          /* WINSOCK */
#endif		                                                /* PSOS */


/*
 *	The following values are those used in SunOs.  They are pretty stable
 *	throughout Unix land.  The only requirements are:
 *		1)  the values must be non-zero (zero is Peer's convention
 *		    for indicating the absence of error)
 *		2)  if the perror() function is used, its tables must agree
 *		    with these values.
 *	Even if <errno.h> is included, the following tests should still be made.
 *	Some versions of errno, especially in DOS land, do not have all values.
 */
#ifndef ENOENT
#define ENOENT		2		/* No such file or directory	*/
#endif

/*
 *	EINTR is particularly important - make sure this is in fact what
 *	select and friends return when interrupted by HUP (if used)
 */
#ifndef EINTR
#define EINTR		4		/* Interrupted system call	*/
#endif

#ifndef E2BIG
#define E2BIG		7		/* Arg list too long		*/
#endif

#ifndef ENOMEM
#define ENOMEM		12		/* Not enough core		*/
#endif

#ifndef EFAULT
#define EFAULT		14		/* Bad address			*/
#endif

#ifndef EBUSY
#define EBUSY		16		/* Mount device busy		*/
#endif

#ifndef EEXIST
#define EEXIST		17		/* File exists			*/
#endif

/************************************************************************
 *									*
 *	Note: <winsock.h> in windows environments defines the following *
 *	symbols, rather than the <errno.h> for that environment.  It's	*
 *	bogus, but the simplest way of dealing with it is to not define *
 *	those error codes if we're building for that target.		*
 *									*
 ************************************************************************/

#ifndef PEER_WINSOCK_PORT
#ifndef EINPROGRESS
#define EINPROGRESS	36		/* Operation now in progress	*/
#endif

#ifndef EALREADY
#define EALREADY	37		/* Operation already in progress*/
#endif

#ifndef ENOTEMPTY
#define ENOTEMPTY	66		/* Directory not empty		*/
#endif
#else
#ifndef EINPROGRESS
#define EINPROGRESS     WSAEINPROGRESS  /* Operation now in progress    */
#endif

#ifndef EALREADY
#define EALREADY        WSAEALREADY     /* Operation already in progress*/
#endif

#ifndef ENOTEMPTY
#define ENOTEMPTY       WSAENOTEMPTY    /* Directory not empty          */
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK	WSAEWOULDBLOCK	/* Operation would block        */
#endif

#endif


#endif

