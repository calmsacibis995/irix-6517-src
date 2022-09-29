/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module time.h, release 1.10 dated 96/04/30
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.time.h
 *sccs
 *sccs    1.10	96/04/30 14:54:50 rpresuhn
 *sccs	added PEER_NEED_TIME_H for systems that need it (PR#388) (PR#660)
 *sccs
 *sccs    1.9	96/02/12 13:21:48 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.8	95/01/24 13:02:03 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.7	94/11/23 18:16:51 randy
 *sccs	no sys/time.h with winsock (PR#99)
 *sccs
 *sccs    1.6	93/06/04 18:15:56 randy
 *sccs	get rid of obnoxious psos compiler warning
 *sccs
 *sccs    1.5	93/04/26 17:56:11 randy
 *sccs	psos support
 *sccs
 *sccs    1.4	93/04/15 22:04:48 randy
 *sccs	support compilation with g++
 *sccs
 *sccs    1.3	92/11/12 16:35:00 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:14:57 randy
 *sccs	added really paranoid re-inclusion protection
 *sccs
 *sccs    1.1	92/01/14 13:36:49 randy
 *sccs	date and time created 92/01/14 13:36:49 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AMETIMEH
#define AMETIMEH

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
 *	ame/time.h - header file to get representation of system time
 *
 *	The SNMP world operates in terms of ticks (0.01 second) since
 *	system reboot.	Everything else (practically) has a representation
 *	of system time, usually an offset from some arbitrary point in time.
 *	In the Unix world, this is a pair (seconds and microseconds) since
 *	the beginning of the year 1970.
 *
 ************************************************************************/

#include "ame/machtype.h"

#ifndef NEED_OWN_TIME
#ifndef SYS_TIMEH_INCLUDED
EXTERN_BEGIN				/* for C++ to C linkage		*/


#ifdef PEER_PSOS_PORT						/* PSOS */
#include <pna.h>						/* PSOS */
/*								   PSOS	  
 *	define a timezone structure SOLELY to keep the compiler	   PSOS
 *	happy with function prototypes.	 this structure is NEVER   PSOS
 *	actually used for anything!				   PSOS
 */								/* PSOS */
struct timezone							/* PSOS */
{								/* PSOS */
	int	tz_minuteswest; /* minutes west of Greenwich	   PSOS */
	int	tz_dsttime;	/* type of dst correction	   PSOS */
};								/* PSOS */
#else
#ifdef PEER_WINSOCK_PORT
/*								   WINSOCK
 *	under winsock, the time definitions come from the	   WINSOCK
 *	winsock header files, of all places.			   WINSOCK
 */								/* WINSOCK */
#else
#include <sys/time.h>			/* get unix definitions		*/
#ifdef PEER_NEED_TIME_H
#include <time.h>			/* sometimes needed for tracing	*/
#endif
#endif
#endif

EXTERN_END				/* for C++ to C linkage		*/

#define SYS_TIMEH_INCLUDED

#endif
#else
/*
 *	The timeval structure is returned by gettimeofday(2) system call,
 *	and used in other calls.  Specifically, snmpdt.c uses this structure
 *	to convert the current time and some specified starting time into
 *	an interval in units of SnmpTicks.  This is also used to represent
 *	timeout values for mgmt_poll(), select(), and so on.
 */
struct timeval
{
	long	tv_sec;		/* seconds				*/
	long	tv_usec;	/* and microseconds			*/
};

/*
 *	The timezone structure is only needed to keep lint happy
 */
struct timezone
{
	int	tz_minuteswest; /* minutes west of Greenwich		*/
	int	tz_dsttime;	/* type of dst correction		*/
};

#endif

#endif
