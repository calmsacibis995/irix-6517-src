#ifndef	__MS_CMS_MESG_H
#define	__MS_CMS_MESG_H

/*
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * Types of messages.
 */
typedef  enum {
	CMS_MESG_REQ_FOR_PROPOSAL,
	CMS_MESG_PROPOSAL,
	CMS_MESG_MEMBERSHIP,
	CMS_MESG_ACCEPT,
	CMS_MESG_MEMBERSHIP_QUERY,
	CMS_MESG_MEMBERSHIP_REPLY,
	CMS_MESG_MEMBERSHIP_LEAVE,
	CMS_NUM_MESG_TYPES
} cms_mesg_type_t;

/*
 * Information present in all messages.
 */
typedef struct cms_message_base_s {
	cell_t		src_cell;
	cell_t		dest_cell;
	seq_no_t	mesg_seq_no;
	cms_mesg_type_t	mesg_type;
} cms_mesg_base_t;

/*
 * Message formats for various types of messages. All of them
 * contain the base type in addition to the rest of the stuff.
 */

/*
 * Request for proposal. It contains base info. plus the set of 
 * new members who want to join the membership.
 */
typedef struct cms_req_for_proposal {
	cms_mesg_base_t	cms_mesg_base;
	cell_set_t	cms_new_members_set;	/* Cells that want to join */
} cms_req_for_proposal_t;

/*
 * Proposal from the leader. This contains new member set and the send set
 * of the leader. The send set is used to forward messages to indirectly
 * connected cells.
 */
typedef struct cms_proposal {
	cms_mesg_base_t	cms_mesg_base;
	cell_set_t	cms_send_set;		/* Receive set of proposer */
	cell_set_t	cms_new_members_set; 	/* Cells that want to join */
	cell_t		cms_leave_cell;		/* Cell that wants to leave */
} cms_proposal_t;

/*
 * Accept carries the sequence number of the proposal for which it is
 * the reply. This enables matching aceepts with proposals and reject 
 * accepts to old proposals.
 */
typedef struct cms_accept {
	cms_mesg_base_t	cms_mesg_base;
	cell_set_t	cms_recv_set;
	cell_set_t	cms_send_set;
	ageno_t		cms_age;
	seq_no_t	cms_proposal_seq_no;	/* Proposal seq no for   */
						/* which this accept is  */
						/* intended  		 */
} cms_accept_t;

/*
 * Membership message. Contains the membership and the updated age of the cells
 * in the membership.
 */
typedef struct cms_membership {
	cms_mesg_base_t	cms_mesg_base;
	cell_set_t 	cms_members; 
	ageno_t 	cms_age[MAX_CELLS];
} cms_membership_t;

/*
 * Membership query. This is used by a cell that is coming up to probe if
 * there is an existing membership.
 */
typedef struct cms_membership_query {
	cms_mesg_base_t	cms_mesg_base;
} cms_membership_query_t;

/*
 * This is a reply to the membership query. This is sent by a cell
 * in the existing membership in response to a query.
 */
typedef struct cms_membership_reply {
	cms_mesg_base_t	cms_mesg_base;
	cell_set_t	cms_members;
	ageno_t 	cms_age[MAX_CELLS];
} cms_membership_reply_t;

/*
 * This is a reply to the membership query. This is sent by a cell
 * in the existing membership in response to a query.
 */
typedef struct cms_membership_leave {
	cms_mesg_base_t	cms_mesg_base;
} cms_membership_leave_t;

/*
 * General message type. Used by functions that deal with general messages.
 */
typedef	union cms_message {
	cms_mesg_base_t		cms_mesg_base;
	cms_proposal_t		p;
	cms_accept_t		a;
	cms_membership_t	m;
	cms_req_for_proposal_t	rp;
	cms_membership_reply_t	mr;
	cms_membership_query_t	mq;
	cms_membership_leave_t	ml;
} cms_message_t;

#define	cms_src_cell 	cms_mesg_base.src_cell
#define	cms_dest_cell 	cms_mesg_base.dest_cell
#define	cms_mesg_seq_no cms_mesg_base.mesg_seq_no
#define	cms_mesg_type 	cms_mesg_base.mesg_type


#define	MESG_SIZE		sizeof(cms_message)

/*
 * Return values of cms_recv_message.
 */
#define	CMS_MESSAGE_RECEIVED	0	/* Message received */
#define	CMS_MESSAGE_TIMEOUT	1	/* Timeout waiting for mesg. */
#define	CMS_FAILURE_DETECTED	2	/* Failure detected */
#define	CMS_CELL_DIED		3	/* Cell died */
#define	CMS_CELL_JOIN		4	/* Cell wants to join */
#define	CMS_CELL_LEAVE		5	/* Cell wants to leave */
#define	CMS_ALL_CELLS_UP	6	/* All cells up (init time) */
#define	CMS_RECOVERY_COMPLETE	7	/* Recovery complete */

#define	CMS_NOTIMEOUT		-1

#define	BROADCAST_CELL		CELL_NONE
/*
 * Get message type.
 */
#define	cms_get_mesg_type(msg)	((msg)->cms_mesg_type)

#ifndef	_KERNEL
#include "cms_ep.h"

#define	cms_send_mesg(cell, msg) \
		ep_send_mesg((cell), (msg), sizeof(cms_message_t)) 

#define	cms_mesg_wait(msg, timeout) \
		ep_mesg_wait((msg), sizeof(cms_message_t), (timeout))
#endif

extern int cms_recv_message(cms_message_t *, int);
extern void cms_send_req_for_proposal(cell_t, cell_set_t *);
extern void cms_send_accept(cell_t, cell_set_t *, cell_set_t *, seq_no_t);
extern void cms_send_membership_reply(cms_membership_query_t *);
extern void cms_send_membership_query(cell_set_t *);
extern void cms_pushback_message(cms_message_t *);
extern void cms_send_proposal(cell_set_t *, cell_t);
extern void cms_send_membership(cms_message_t *);
extern void cms_send_membership_leave(void);

#endif /* __MS_CMS_MESG_H */
