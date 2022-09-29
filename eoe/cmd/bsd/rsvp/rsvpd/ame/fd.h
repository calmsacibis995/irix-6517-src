/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module fd.h, release 1.10 dated 96/05/29
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.fd.h
 *sccs
 *sccs    1.10	96/05/29 19:25:41 rpresuhn
 *sccs	added PEER_FDSET_IS_INT to support HPUX 10 (PR#730)
 *sccs
 *sccs    1.9	96/02/12 13:15:39 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.8	95/12/15 16:12:02 sam
 *sccs	Minor additions for the OS/2 port (PR#619)
 *sccs
 *sccs    1.7	95/06/27 13:44:42 randy
 *sccs	QNX needs additional include file (PR#397)
 *sccs
 *sccs    1.6	95/01/24 13:00:18 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.5	93/06/02 22:13:22 randy
 *sccs	added include paths for AIX
 *sccs
 *sccs    1.4	93/05/28 02:15:55 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.3	93/04/26 17:54:15 randy
 *sccs	psos support
 *sccs
 *sccs    1.2	92/11/12 16:33:34 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	92/09/11 19:06:22 randy
 *sccs	date and time created 92/09/11 19:06:22 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_FDH
#define AME_FDH

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
 *	fd.h - get defintions of file descriptor mask structure and
 *	supporting macros.
 *
 ************************************************************************/

#include "ame/machtype.h"

#ifdef PEER_MSDOS_PORT

/*
 *	Under ms-dos as a tsr, the macros don't really need to do anything
 */
#define FD_ZERO(X)
#define FD_SET(X, Y)
#define FD_CLR(X, Y)
#define FD_ISSET(X, Y)	1

typedef long	fd_set;

#else

#ifdef PEER_QNX_PORT
#include <sys/select.h>
#endif

#ifndef SYS_TYPESH_INCLUDED
#ifdef PEER_VXWORKS_PORT
#include <types.h>
#endif
#ifdef PEER_LYNX_PORT
#include <sys/types.h>
#endif
#ifdef PEER_AIX_PORT
#include <sys/types.h>
#endif
#ifdef PEER_GENERIC_PORT
#include <sys/types.h>
#endif
#ifdef PEER_OS2_PORT
#define BSD_SELECT
#include "ame\time.h"
#include <sys\select.h>
#endif
#define SYS_TYPESH_INCLUDED
#endif

#endif

/*
 *	On some systems, the prototype for select() is given incorrectly.
 *	To accomodate these, we need to define a silly cast to make things
 *	work with fussy compilers that don't know that their own header
 *	files are broken.
 */
#ifdef PEER_FDSET_IS_INT
#define PEER_FDSET_CAST	(int *)
#define PEER_FDSET_NULL (int *) NULL
#else
#define PEER_FDSET_CAST	/* no cast needed */
#define PEER_FDSET_NULL (fd_set *) NULL
#endif

#endif

