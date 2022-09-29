/*
 *  @(#) $Id: rsvp_mstat.h,v 1.5 1998/11/25 08:43:36 eddiem Exp $
 */

/************************ rsvp_status.c **************************
 *                                                               *
 *         Format of status packets multicast by RSVP daemon     *
 *                                                               *
 *  (Version with variable-length flowspecs, filterspecs)        *
 *                                                               *
 *****************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		RSRR written by: Daniel Zappala, May 1996

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#ifndef __RSVP_STATUS_H
#define __RSVP_STATUS_H

#include "rsvp_types.h"		/* includes sys/types.h for us */
#include <sys/socket.h>
#include "rsvp.h"

/* Status multicast message types:
 */
#define TYPE_RSVP_VPATH		4	/* Send RSVP path state */
#define TYPE_RSVP_VRESV		5	/* Send RSVP reservation state */
#define TYPE_RSVP_KRESV		6	/* Send RSVP kernel reservation state*/
#define TYPE_RSVP_PROBE		8	/* Probe packet */

#define LOCAL_HOST		"local"
#define STAT_DEST_ADDR		"224.0.1.19.1427"
#define STAT_PROBE_PORT		9987	/* TEMPORARY! */
#define STAT_MCAST_TTL		63
#ifdef __sgi
/* 512 bytes for a status pkt is too small - mwang */
#define STAT_MSG_SIZE		5120
#else
#define STAT_MSG_SIZE		512
#endif

#ifndef NODE_LEN
#define NODE_LEN		16
#endif
#ifndef NAME_LEN
#define NAME_LEN		16
#endif

/*
 *   3rd Level Path info: Per Sender, within specific session
 */
typedef struct PathSenderInfo {
	char		pathS_in_if[IFNAMSIZ];	/* Incoming Interface name */
	struct in_addr	pathS_phop;		/* Previous Hop */
	u_int32_t	pathS_routes;		/* Bitmap of outgoing i'faces */
	u_int32_t	pathS_ttd;		/* Path state TTD: sender */
	POLICY_DATA	pathS_policy;		/* var len: policy data */
	FILTER_SPEC	pathS_template;		/* SENDER_TEMPLATE|NULL */
	SENDER_TSPEC	pathS_flowspec;		/* SENDER_TSPEC|NULL */
} PathSender_t;

/*
 *   2nd Level Path info: Per session
 */
typedef struct PathInfo {
	Session_IPv4	path_session;	/* Session (dest) */
	u_int32_t	path_R;		/* Path state refresh timeout */
	u_int32_t	nSender;	/* Number of Senders in Sesssion */
	PathSender_t	path_sender;	/* The first Sender info. */
	/* 
	 *   nSender-1  PathSender_t's follow ...
	 */
} PathInfo_t;


/*
 *	Structures for Reservation State message
 *
 */

/* 3rd Level: WF style reservation
 */
typedef struct ResvWF {
	char		WF_if[IFNAMSIZ];/* Interface name */
	u_int32_t	WF_ttd;		/* TTD */
	struct in_addr	WF_nexthop;	/* Next hop */
	FLOWSPEC	WF_flowspec;	/* FLOWSPEC|NULL */
	FILTER_SPEC	WF_filtspec;	/* FILTER_SPEC|NULL */
} ResvWF_t;


/* 3rd Level: FF style reservation
 */
typedef struct ResvFF {
	char		FF_if[IFNAMSIZ];/* Outgoing interface name */
	u_int32_t	FF_ttd;		/* TTD */
	struct in_addr	FF_nexthop;	/* Next hop */
	FLOWSPEC 	FF_flowspec;	/* FLOWSPEC|NULL */
	FILTER_SPEC	FF_filtspec;	/* FILTER_SPEC|NULL */
} ResvFF_t;


/* 3rd Level: SE style reservation
 */
typedef struct ResvSE {
	char		SE_if[IFNAMSIZ];/* Interface name */
	u_int32_t	SE_ttd;		/* Time to die */
	struct in_addr  SE_nexthop;	/* Next hop */
	u_int16_t	nSE_filts;	/* Number of filter specs */
	FLOWSPEC	SE_flowspec;	/* FLOWSPEC */
	/*
	 *  nSE_filts FilterSpec's follow ...
	FILTER_SPEC	SE_filtspec;	*//* FILTER_SPEC
	 */
} ResvSE_t;

/*
 *  2nd Level Resv info, all styles: per session
 */
typedef struct ResvInfo {
	int32_t		resv_style;	/* Reservation style */
	Session_IPv4	resv_session;	/* Session address */
	u_int32_t	resv_R;		/* Resv state refresh timeout */
	int32_t		nStruct;	/* Number of structs following */
	POLICY_DATA 	resv_policy;	/* POLICY_DATA|NULL */
	/*
	 *   nStruct 3rd-level resv structs follow:
	 */
	union {
		ResvSE_t resv_SEspec;
		ResvFF_t resv_FFspec;
		ResvWF_t resv_WFspec;
	} Session_resv[1];
} ResvInfo_t;


/*
 *
 *	 1st Level: packet format
 *
 */
typedef struct RSVPInfo {
	int32_t type;			/* Packet type: path/resv info */
	char name[NODE_LEN];		/* Node name */
	int32_t nSession;		/* Number of sessions */

	union {				/* First session info */
		PathInfo_t pathInfo;
		ResvInfo_t resvInfo;
	} Session_un;
	/*
	 *   nSession-1 sessions follow... 
	 */
} RSVPInfo_t;

#endif
