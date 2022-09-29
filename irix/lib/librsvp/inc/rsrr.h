/*
 * @(#) $Id: rsrr.h,v 1.4 1998/11/25 08:43:36 eddiem Exp $
 */

/*************************** rsrr.h ******************************
 *                                                               *
 *  RSRR: Routing Support for Resource Reservations              *
 *           Define rsvpd:routing IPC formats                    *
 *                                                               *
 *****************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		RSRR written by: Daniel Zappala, April 1995

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
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

#define RSRR_SERV_PATH "/tmp/.rsrr_svr"
/* Note this needs to be 14 chars for 4.3 BSD compatibility */
#define RSRR_CLI_PATH "/tmp/.rsrr_cli"

#define RSRR_MAX_LEN 2048
#define RSRR_HEADER_LEN (sizeof(struct rsrr_header))
#define RSRR_RQ_LEN (RSRR_HEADER_LEN + sizeof(struct rsrr_rq))
#define RSRR_RR_LEN (RSRR_HEADER_LEN + sizeof(struct rsrr_rr))
#define RSRR_SQ_LEN (RSRR_HEADER_LEN + sizeof(struct rsrr_sqr))
#define RSRR_SR_LEN RSRR_SQ_LEN
#define RSRR_VIF_LEN (sizeof(struct rsrr_vif))

/* Current maximum number of vifs. */
#define RSRR_MAX_VIFS 32

/* Maximum acceptable version */
#define RSRR_MAX_VERSION 1

/* RSRR message types */
#define RSRR_ALL_TYPES     0
#define RSRR_INITIAL_QUERY 1
#define RSRR_INITIAL_REPLY 2
#define RSRR_ROUTE_QUERY   3
#define RSRR_ROUTE_REPLY   4
#define RSRR_SERVICE_QUERY 5
#define RSRR_SERVICE_REPLY 6


/* RSRR Initial Reply (Vif) Status bits.
 * 
 * 0 = disabled bit, set if the vif is administratively disabled.
 */
#define RSRR_DISABLED_BIT 0
#define RSRR_is_disabled(vif) (BIT_TST(vif_list[vif].status, RSRR_DISABLED_BIT))

/* RSRR Route Query/Reply flag bits.
 *
 * 0 = Route Change Notification bit, set if the reservation protocol
 *     wishes to receive notification of a route change for the
 *     source-destination pair listed in the query. Notification is in the
 *     form of an unsolicitied Route Reply.
 * 1 = Error bit, set if routing doesn't have a routing entry for
 *     the source-destination pair.
 */
#define RSRR_NOTIFICATION_BIT 0
#define RSRR_ERROR_BIT 1

/* RSRR Service Query/Reply flag bits.
 *
 * 0 = Vif bit, set if the service query includes a downstream vif on which
 *     the router should begin service.
 * 1 = Error bit, same as above.
 * 2 = Stability bit, set if the reservation protocol wishes to keep the
 *     routing entry for a source-destination pair stable.
 * 3 = Delegation bit, set if the reservation protocol can wants
 *     stability, but doesn't care how routing provides it.  If the
 *     stability bit is set, but the delegation bit is cleared, then the
 *     routing protocol should pin the route.  Otherwise it may choose to
 *     use a smooth transition to another route that can provide the same
 *     service.
 *     XXX Currently not implemented.
 * 4 = Reserved it.  XXX currently not implemented.
 */
#define RSRR_VIF_BIT 0
#define RSRR_STABILITY_BIT 2
#define RSRR_DELEGATION_BIT 3
#define RSRR_RESERVED_BIT 4

/* Definition of an RSRR message header.
 * An Initial Query uses only the header, and an Initial Reply uses
 * the header and a list of vifs.
 */
struct rsrr_header {
    u_char version;			/* RSRR Version, currently 1 */
    u_char type;			/* type of message, as defined above */
    u_char flags;			/* flags; defined by type */
    u_char num;				/* number; defined by type */
};

/* Definition of a vif as seen by the reservation protocol.
 *
 * Routing gives the reservation protocol a list of vifs in the
 * Initial Reply.
 *
 * We explicitly list the ID because we can't assume that all routing
 * protocols will use the same numbering scheme.
 * 
 * The status is a bitmask of status flags, as defined above.  It is the
 * responsibility of the reservation protocol to perform any status checks
 * if it uses the MULTICAST_VIF socket option.
 *
 * The threshold indicates the ttl an outgoing packet needs in order to
 * be forwarded. The reservation protocol must perform this check itself if
 * it uses the MULTICAST_VIF socket option.
 *
 * The local address is the address of the physical interface over which
 * packets are sent.
 */
struct rsrr_vif {
    u_char id;				/* vif id */
    u_char threshold;			/* vif threshold ttl */
    u_short status;			/* vif status bitmask */
    struct in_addr local_addr;		/* vif local address */
};

/* Definition of an RSRR Route Query.
 * 
 * The query asks routing for the forwarding entry for a particular
 * source and destination.  The query ID uniquely identifies the query
 * for the reservation protocol.  Thus, the combination of the client's
 * address and the query ID forms a unique identifier for routing.
 * Flags are defined above.
 */ 
struct rsrr_rq {
    struct in_addr dest_addr;		/* destination */
    struct in_addr source_addr;		/* source */
    u_long query_id;			/* query ID */
};

/* Definition of an RSRR Route Reply.
 *
 * Routing uses the reply to give the reservation protocol the
 * forwarding entry for a source-destination pair.  Routing copies the
 * query ID from the query and fills in the incoming vif and a bitmask
 * of the outgoing vifs.
 * Flags are defined above.
 */
struct rsrr_rr {
    struct in_addr dest_addr;		/* destination */
    struct in_addr source_addr;		/* source */
    u_long query_id;			/* query ID */
    u_short in_vif;			/* incoming vif */
    u_short reserved;			/* reserved */
    u_long out_vif_bm;			/* outgoing vif bitmask */
};

/* Definition of an RSRR Service Query/Reply.
 * 
 * The query asks routing to perform a service for a particular
 * source/destination combination.  The query also lists the vif
 * that the service applies to.
 */
struct rsrr_sqr {
    struct in_addr dest_addr;		/* destination */
    struct in_addr source_addr;		/* source */
    u_long query_id;			/* query ID */
    u_short vif;			/* vif */
    u_short reserved;			/* reserved */
};
