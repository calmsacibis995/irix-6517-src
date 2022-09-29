
/*
 * @(#) $Id: rsvp_rsrr.c,v 1.7 1998/11/25 08:43:14 eddiem Exp $
 */


/************************ rsvp_rsrr.c ****************************
 *                                                               *
 *  RSRR: Routing Support for Resource Reservations              *
 *  	This version is an interface to mrouted                  *
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

#include "rsvp_daemon.h"

#ifndef HOST_ONLY
/*	No RSRR code for HOST_ONLY
 */

/* External RSRR definitions */
extern struct rsrr_vif vif_list[];	        /* RSVP vif list */
extern char rsrr_recv_buf[];		        /* RSRR receive buffer */
extern char rsrr_send_buf[];		        /* RSRR send buffer */
extern int rsrr_send(), rsrr_send0();

/* External Declarations */
extern void Route_Update(Session *, PSB *, int, bitmap);
extern void finish_path(Session *, PSB *);

/* Extern debugging definitions. */
char *inet_fmt();
extern char s1[];
extern char s2[];
extern char s3[];
extern char s4[];

/* Internal RSRR global variables. */
struct rsrr_query_table *rsrr_qt[RSRR_QT_SIZE]; /* Query table */
u_long rsrr_qt_index = 0;		        /* Current query table index */

/* This is for bm_expand, also used by rsvp_path.c - mwang */
char s[RSRR_MAX_VIFS];

/*
 *	Forward Declarations
 */
void 	rsrr_accept_ir();
void 	rsrr_accept_rr();
void 	rsrr_update();
u_long 	rsrr_qt_next();
void 	rsrr_qt_cache();
struct rsrr_query_table *rsrr_qt_lookup();
void 	rsrr_qt_delete();
int	resv_exist();
char 	*bm_expand();
void	SetNotifyBit(PSB *, struct rsrr_header *);


/* Accept a message from the reservation protocol and take
 * appropriate action.
 * Return error only if the message is not of the type specified.
 */
int
rsrr_accept(recvlen,expected_type)
    int recvlen;
    u_char expected_type;
{
    struct rsrr_header *rsrr;
    struct rsrr_rr *route_reply;

    if (recvlen < RSRR_HEADER_LEN) {
	log(LOG_ERR, 0,
	    "Received RSRR packet of %d bytes, which is less than min size",
	    recvlen);
	return -1;
    }

    rsrr = (struct rsrr_header *) rsrr_recv_buf;
    
    if (rsrr->version > RSRR_MAX_VERSION) {
	log(LOG_ERR, 0,
	    "Received RSRR packet version %d, which I don't understand",
	    rsrr->version);
	return -1;
    }

    /* Check if expected type is specified and force match if it is. */
    if (expected_type != RSRR_ALL_TYPES && rsrr->type != expected_type) {
	log(LOG_ERR, 0,
	    "Received RSRR packet of type %d, but I'm expecting type %d\n",
	    rsrr->type, expected_type);
	return -1;
    }

    switch (rsrr->version) {
      case 1:
	switch (rsrr->type) {
	  case RSRR_INITIAL_REPLY:
	    /* Check size */
	    if (recvlen < (RSRR_HEADER_LEN + rsrr->num*RSRR_VIF_LEN)) {
		log(LOG_ERR, 0,
		    "Received Initial Reply of %d bytes, which is too small",
		    recvlen);
		break;
	    }
	    rsrr_accept_ir(rsrr);
	    break;
	  case RSRR_ROUTE_REPLY:
	    /* Check size */
	    if (recvlen < RSRR_RR_LEN) {
		log(LOG_ERR, 0,
		    "Received Route Reply of %d bytes, which is too small",
		    recvlen);
		break;
	    }
	    route_reply = (struct rsrr_rr *) (rsrr_recv_buf + RSRR_HEADER_LEN);
	    rsrr_accept_rr(rsrr,route_reply);
	    break;
	  default:
	    log(LOG_ERR, 0,
		"Received RSRR packet type %d, which I don't handle",
		rsrr->type);
	    break;
	}
	break;
	
      default:
	log(LOG_ERR, 0,
	    "Received RSRR packet version %d, which I don't understand",
	    rsrr->version);
	break;
    }
    return 0;
}

/* Send an Initial Query to routing */
int
rsrr_send_iq()
{
    struct rsrr_header *rsrr;
    int sendlen;

    /* Set up message */
    rsrr = (struct rsrr_header *) rsrr_send_buf;
    rsrr->version = 1;
    rsrr->type = RSRR_INITIAL_QUERY;
    rsrr->flags = 0;
    rsrr->num = 0;

    /* Get the size. */
    sendlen = RSRR_HEADER_LEN;

    if (IsDebug(DEBUG_RSRR))
	log(LOG_DEBUG, 0, "> Initial Query\n");

    /* Send it. */
    return (rsrr_send0(sendlen));
}

/* Accept an Initial Reply.  The reply contains the vifs that routing
 * is using; initiatlize our own vif structure so RSVP can keep track
 * of them.
 */
void
rsrr_accept_ir(rsrr_in)
    struct rsrr_header *rsrr_in;
{
    int vif,i;
    struct rsrr_vif *vifs;		/* vifs returned from routing */

    if (NoMroute) {
	/* We already timed out for hearing from routing.  This
	 * means routing was slow getting back to us.  Ignore for
	 * now, but investigate whether we can re-initialize.
	 */
	return;
    }

    vif_num = rsrr_in->num;
    if (IsDebug(DEBUG_RSRR))
	log(LOG_DEBUG, 0, "< Initial Reply with %d vifs\n",vif_num);
    if (vif_num <= 0)
	return;

    /* Setup permanent RSVP vif structure. */
    for (vif=0; vif< RSRR_MAX_VIFS; vif++)
	BIT_SET(vif_list[vif].status,RSRR_DISABLED_BIT);

    vifs = (struct rsrr_vif *) (rsrr_recv_buf + RSRR_HEADER_LEN);
    for (vif = 0; vif < vif_num; vif++) {
	/*  Skip any disabled VIF  */
	if (BIT_TST(vifs[vif].status,RSRR_DISABLED_BIT))
	    continue;
	for (i = 0; i < if_num; i++) {
	    if (vifs[vif].local_addr.s_addr == IF_toip(i)) {
		vif_toif[vifs[vif].id] = i;
		vif_list[vifs[vif].id].id = vifs[vif].id;
		vif_list[vifs[vif].id].threshold = vifs[vif].threshold;
		vif_list[vifs[vif].id].status = vifs[vif].status;
		vif_list[vifs[vif].id].local_addr.s_addr = 
		    vifs[vif].local_addr.s_addr;
		if (IsDebug(DEBUG_RSRR))
		    log(LOG_DEBUG, 0, "Configured vif %d with %s\n",
			vifs[vif].id,inet_fmt(vifs[vif].local_addr.s_addr,s1));
		break;
	    }
	}
	if (i == if_num) {
	    log(LOG_ERR, 0,"VIF config error\n");
	    exit(-1);
	}
    }
    vif_toif[vif_num] = if_num;  /* For API */
}

/* Send a Route Query to routing for a dest-source pair. */
void
rsrr_send_rq(dp,sp)
    Session *dp;
    PSB *sp;
{
    struct rsrr_header *rsrr;
    struct rsrr_rq *route_query;
    int sendlen;
    SENDER_TEMPLATE *sadp = sp->ps_templ;

    /* Set up message */
    rsrr = (struct rsrr_header *) rsrr_send_buf;
    rsrr->version = 1;
    rsrr->type = RSRR_ROUTE_QUERY;
    rsrr->flags = 0;
    BIT_SET(rsrr->flags,RSRR_NOTIFICATION_BIT);
    rsrr->num = 0;

    route_query = (struct rsrr_rq *) (rsrr_send_buf + RSRR_HEADER_LEN);
    route_query->dest_addr.s_addr = dp->d_addr.s_addr;
    route_query->source_addr.s_addr = sadp->filt_srcaddr.s_addr;
    route_query->query_id = rsrr_qt_next();
    
    /* Get the size. */
    sendlen = RSRR_RQ_LEN;

    if (IsDebug(DEBUG_RSRR)) {
	log(LOG_DEBUG, 0, "> Route Query (%s %s) Notify= %d\n",
	    inet_fmt(route_query->source_addr.s_addr,s1),
	    inet_fmt(route_query->dest_addr.s_addr,s2),
	    BIT_TST(rsrr->flags,RSRR_NOTIFICATION_BIT));
    }

    /* Send it. */
    if (rsrr_send(sendlen) >= 0)
	/* Cache the query. */
	rsrr_qt_cache(route_query->query_id,dp,sp);
}

/* Accept a Route Reply from routing.  Find the related session and sender
 * structures and issue a PATH refresh if needed.
 */
void
rsrr_accept_rr(rsrr_in,route_reply)
    struct rsrr_header *rsrr_in;
    struct rsrr_rr *route_reply;
{
    Session *dp;
    PSB *sp;
    int i;
    struct rsrr_query_table *qt;

    if (IsDebug(DEBUG_RSRR)) {
	log(LOG_DEBUG, 0,
	    "< Route Reply (%s %s) in vif %d out vifs %s Notify= %d\n",
	    inet_fmt(route_reply->source_addr.s_addr,s1),
	    inet_fmt(route_reply->dest_addr.s_addr,s2),
	    route_reply->in_vif,bm_expand(route_reply->out_vif_bm,s),
	    BIT_TST(rsrr_in->flags,RSRR_NOTIFICATION_BIT));
    }

    /* Reject errors. */
    if (BIT_TST(rsrr_in->flags,RSRR_ERROR_BIT)) {
	/* Delete any associated cache entry so that we can re-use
	 * the query id.
	 */
	rsrr_qt_delete(route_reply->query_id);
	if (IsDebug(DEBUG_RSRR))
	    log(LOG_DEBUG, 0,"Rejecting Route Reply because Error bit set.\n");
	return;
    }
    /* Check if query id is cached. */
    qt = rsrr_qt_lookup(route_reply->query_id);
    if (qt != NULL) {
	SetNotifyBit(qt->sp, rsrr_in);

	/* Update PSB with new route info, then complete process of
	 * path info with any needed refreshes.
	 */
	rsrr_update(route_reply, qt->dp, qt->sp);

	/* Delete the query id from the table.  We only use the table
	 * for pending queries.  Route change notification queries
	 * can suffer a longer lookup because they are rare.
	 */
	rsrr_qt_delete(qt->query_id);
	return;
    }

    /* None cached.  Must scan entire state list looking for affected
     * sessions and senders.
     */
    for (i= 0; i < SESS_HASH_SIZE; i++)
      for (dp = session_hash[i]; dp != NULL; dp = dp->d_next) {
        if (dp->d_addr.s_addr == route_reply->dest_addr.s_addr) {
            /* Found session with same group/dest address. */

            /* Find matching senders. If sender in RSRR is zero,
             * the tree is a shared tree, so match all senders.
             */
            for (sp = dp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
                if ((route_reply->source_addr.s_addr == 0) ||
                    (sp->ps_templ->filt_srcaddr.s_addr ==
		     			route_reply->source_addr.s_addr)) {

			SetNotifyBit(sp, rsrr_in);
			rsrr_update(route_reply, dp, sp);
		}
	    }
        }
    }
}


/*
 *	Update path state using route reply
 */
void
rsrr_update(route_reply, dp, sp)
    struct rsrr_rr *route_reply;
    Session *dp;
    PSB *sp;
{
    /* Remember that the multicast forwarding entry doesn't include
     * the vif over which data originates if the sender is local to
     * the router.   Set the bit now.
     */
    if (IsAPI(sp)) 
	BIT_SET(route_reply->out_vif_bm, sp->ps_originvif);

    /* Call common routine to update the state.
     */
    Route_Update(dp, sp, route_reply->in_vif, route_reply->out_vif_bm);
    finish_path(dp, sp);
}


/* Initialize the query table. */
void
rsrr_qt_init()
{
    int i;

    for (i=0; i<RSRR_QT_SIZE; i++)
	rsrr_qt[i] = (struct rsrr_query_table *) NULL;
}

/* Get the next query id.  We assume that query ids are only cached
 * while we are waiting for a route reply.  A route reply that comes
 * because of a route change notification is a relatively infrequent
 * event; in that case, we can just do a linear search.  Because of
 * these assumptions, we can just loop through all possible query ids,
 * without worrying about duplicates.  The id space is so large, it is
 * nearly impossible to have duplicates among outstanding queries.  If
 * we somehow manage to duplicate a query id, then a route reply for
 * the duplicated id will just have to do a linear search. */
u_long
rsrr_qt_next()
{
    u_long query_id;

    query_id = rsrr_qt_index;
    rsrr_qt_index++;
    if (rsrr_qt_index > RSRR_QT_MAX)
	rsrr_qt_index = 0;

    return query_id;
}

/* Cache a query id in the query table with its associated session/sender
 * combination.
 */
void
rsrr_qt_cache(query_id,dp,sp)
    u_long query_id;
    Session *dp;
    PSB *sp;
{
    struct rsrr_query_table *qt;
    int hash = RSRR_QT_HASH(query_id);

    qt = rsrr_qt[hash];
    while (qt) {
	if (qt->query_id == query_id) {
	    /* We somehow managed to duplicate a query id.  Amazing!
	     * Ditch the old one.
	     */
	    qt->query_id = query_id;
	    qt->dp = dp;
	    qt->sp = sp;
	    return;
	}
	qt = qt->next;
    }

    /* Cache entry doesn't already exist.  Create one and insert at
     * front of list.
     */
    qt = (struct rsrr_query_table *) malloc(sizeof(struct rsrr_query_table));
    if (qt == NULL) {
	Log_Mem_Full("RSRR cache");
	exit(-1);
    }
    qt->query_id = query_id;
    qt->dp = dp;
    qt->sp = sp;
    qt->next = rsrr_qt[hash];
    rsrr_qt[hash] = qt;
}

/* Lookup a query id in the table. */
struct rsrr_query_table *
rsrr_qt_lookup(query_id)
    u_long query_id;
{
    struct rsrr_query_table *qt;
    int hash = RSRR_QT_HASH(query_id);

    qt = rsrr_qt[hash];
    while(qt) {
	if (qt->query_id == query_id)
	    return qt;
	qt = qt->next;
    }

    return qt;
}

/* Delete a query id from the table. */
void
rsrr_qt_delete(query_id)
    u_long query_id;
{
    struct rsrr_query_table *qt, *qt_prev;
    int hash = RSRR_QT_HASH(query_id);

    qt = rsrr_qt[hash];
    while(qt) {
	if (qt->query_id == query_id) {
	    if (qt == rsrr_qt[hash])
		/* Deleting first entry. */
		rsrr_qt[hash] = qt->next;
	    else
		qt_prev->next = qt->next;
	    free(qt);
	    return;
	}
	qt_prev = qt;
	qt = qt->next;
    }
}


void
SetNotifyBit(PSB *psbp, struct rsrr_header *rsrr_in)
	{
	if (BIT_TST(rsrr_in->flags, RSRR_NOTIFICATION_BIT))
	    /* Routing can do route change notification.  Set bit in
	     * sender structure that indicates RSRR no longer needs to
	     * send queries.
	     */
	    BIT_SET(psbp->ps_rsrr_flags,  PSBF_RSRR_NOTIFY);
	else
	    /* Clear the bit just in case it was set. */
	    BIT_CLR(psbp->ps_rsrr_flags,  PSBF_RSRR_NOTIFY);
}

#endif /* ! HOST_ONLY */
