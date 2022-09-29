/*
 *  @(#) $Id: rsvp_api.h,v 1.6 1998/11/25 08:43:36 eddiem Exp $
 */

/***********************************************************************
 *	rsvp_api.h -- Define API interface to RSVP daemon
 *
 ***********************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996

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
#ifndef __rsvp_api_h__
#define __rsvp_api_h__

#include <sys/types.h>
#include <sys/param.h>

/*	Version for the protocol used across the Unix socket
 *	between the client library routines and the RSVP daemon.
 *	History:
 *	    Vers 1: Fixed-size structure for req, resp.
 *	    Vers 2: Msgs contain vbl-len objects for flowspecs,
 *			filter specs, etc.
 *	    Vers 3: Update formats to match ID07 protocol spec.
 *	    Vers 4: Install code developed by Joshua Garm @ BBN,
 *			to use single Unix socket per client process.
 */
#define VERSION_API	4

#define RAPI_DEF_TIMEOUT 	30000
#define MAX_MSG			4000	/* Max length of API message */

/*
 *	Define API filter spec, flowspec, and adspec formats
 *		 (to exactly match RAPI object formats).
 */
typedef rapi_filter_t	API_FilterSpec;

typedef rapi_flowspec_t API_Flowspec;

typedef rapi_tspec_t	API_TSpec;

typedef rapi_adspec_t	API_Adspec;

typedef struct {
	API_FilterSpec	api_filt;
	API_Flowspec	api_flow;
}  API_FlowDesc;

/*
 *	Macros to map between API object sizes and RSVP object sizes
 */
#define size_api2d(L) ((L)-sizeof(rapi_hdr_t)+sizeof(Object_header))
#define size_d2api(L) ((L)+sizeof(rapi_hdr_t)-sizeof(Object_header))

/*
 *	Format of API request packet, sent by client.
 */
typedef struct Rsvp_req {
	u_char		rq_version;	/* API version */
	u_char          rq_type;	/* request type (see rsvp_var.h)*/
#define API2_REGISTER	3
#define API2_RESV	4
#define API2_STAT	5
#define API_CLOSE	254
#define	API_DEBUG	253

	u_char          rq_flags;	/* RAPI flags */
#define API_DIR_SEND	0x80
#define API_DIR_RECV	0x40
/*	(These flags are actually obsolete within request)
 */
	u_char		rq_protid;	/* Protocol Id			*/
	int             rq_a_sid;	/* sid of the client side	*/
	int             rq_pid;		/* process id of the application*/

	struct sockaddr_in rq_dest;	/* destination rsvp addrsss	*/
	struct sockaddr_in rq_host;	/* sender/receiver address	*/

	int	        rq_unused;	/* (Unused)			*/
	u_char		rq_style;	/* reservation style  (V2) */
	u_char		rq_ttl;		/* TTL for Path messages */
	u_short         rq_nflwd;	/* number of flow desciptors	*/
	rapi_policy_t	rq_policy[1];	/* Policy data	*/

	/* After rq_policy in API request comes list of pairs:
	 *		<F=filter spec, Q= flowspec>
	 *	where one member of pair may be empty (NUL).  Specifically,
	 *	if rq_nflwd = n:
	 *		Style=WF: <NUL,Q>
	 *		Style=FF: <F1,Q1>, <F2,Q2>, ... <Fn,Qn>
	 *		Style=SE: <F1,Q>, <F2,NUL>, <F3,NUL>, ... <Fn,NUL>
	 *      For a Sender request, n=1 and there is a single pair:
	 *		<Sender Template, Sender Tspec>
	 *	optionally followed by an Adspec.
	 */
}               rsvp_req;

#define API_REGIS_BUF_LEN	sizeof(rsvp_req) + sizeof(API_FlowDesc) +\
				MAX_ADSPEC_LEN

/*
 *	Format of asynchronous response packet, to notify client of
 *		event or error.
 *	Note that event upcall reports entire path/resv state; it is NOT
 *	incremental [Not clear whether incremental would be better].
 *	Thus, resp_nflwd == 0 is a possible upcall following a teardown.
 */
typedef struct Rsvp_resp {
	u_char		resp_version;
	u_char          resp_type;	/* RAPI_EVENT_xxxx (see rapi_lib.h) */
	u_char		resp_flags;
	u_char		resp_protid;	/* Protocol Id			*/

	int             resp_a_sid;	/* session id in the client side*/
	int		resp_style;	/* style id			*/
	struct sockaddr_in
			resp_dest;	/* destination rsvp addrsss	*/
	struct sockaddr_in
			resp_errnode;/* IP addr of node of error	*/
	u_char		resp_errflags;	/* Error flags			*/
	u_char          resp_errcode;	/* Error code			*/
	u_int16_t       resp_errval;	/* Error value			*/

	u_short         resp_nflwd;	/* number of flow desciptors	*/
	API_FlowDesc    resp_flows[1];	/* Variable-length (F,Q) list in*/
					/* form	shown above for requests*/
					/* For Path Event, Adspec list	*/
					/* may follow.			*/
}    rsvp_resp;

/*
 *  Advance ptr past given API object.
 */
#define APIObj_Size(op)  RAPIObj_Size(op)
#define After_APIObj(op) After_RAPIObj(op)


/*
 *	[R]API Management interface definitions
 *
 *	These management functions are specific to the ISI implementation,
 *	which is why there are defined here and not in rapi_lib.h.
 *	Someday they should be replaced by SNMP-based controls.
 */
typedef enum {
	RAPI_CMD_DEBUG_MASK = 1,
	RAPI_CMD_DEBUG_LEVEL = 2,
	RAPI_CMD_FILTER_ON =  3,
	RAPI_CMD_FILTER_OFF = 4,
	RAPI_CMD_ADD_FILTER = 5,
	RAPI_CMD_DEL_FILTER = 6
} rapi_cmd_type_t;

typedef struct {
	u_char rapi_cmd_len;
	u_char rapi_cmd_type;
	u_short	rapi_filler;
	u_int	rapi_data[1];
	/* VL data goes here ... */
} rapi_cmd_t;


#endif /* __rsvp_api_h__ */
