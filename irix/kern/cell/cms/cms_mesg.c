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
#include "cms_base.h"
#include "cms_message.h"
#include "cms_info.h"
#include "cms_trace.h"
#ifdef	_KERNEL
#include "ksys/cell/service.h"
#include "ksys/cell/membership.h"
#include "invk_cms_stubs.h"
#include "I_cms_stubs.h"
#endif

static void cms_forward_message(cms_message_t *);

#ifdef	_KERNEL
static	int	cms_mesg_wait(cms_message_t *, int);
static	void	cms_mesg_timeout(void);
static __inline void
cms_send_mesg(cell_t cell, cms_message_t *msg)
{
	service_t       svc;
	int		error;

	SERVICE_MAKE(svc, cell, SVC_CMSID);
	error = invk_cms_send_message(svc, msg);
	if (error) { 	/* Mark the cell as down */
		cms_notify_cell(cell, B_FALSE);
	}
}

#endif


/*
 * cms_mesg_init:
 * Initialize a given message. It updates the sequence number, the type
 * and the source cell.
 */
void
cms_mesg_init(
	cms_message_t *msg, 
	cms_mesg_type_t type)
{
	seq_no_t seqno = ++cip->cms_exp_seq_no[cellid()][type];
	msg->cms_src_cell = cellid();
	msg->cms_mesg_type = type;
	msg->cms_mesg_seq_no = seqno;
}

/*
 * cms_send_accept:
 * Sends an accept message. It includes the local receive set/send set,
 * the proposal to which the accept is sent as a reply. This message is
 * forwarded to the leader as the local cell need not  be directly  connected
 * to the leader.
 */
void
cms_send_accept(
	cell_t leader, 
	cell_set_t *receive_set, 
	cell_set_t *send_set,
	seq_no_t proposal_seq_no)
{
	cms_message_t msg;
	cms_accept_t *accept_msg = &(msg.a);

	cms_mesg_init(&msg, CMS_MESG_ACCEPT);
	set_assign(&accept_msg->cms_recv_set, receive_set);
	set_assign(&accept_msg->cms_send_set, send_set);
	accept_msg->cms_age = cip->cms_age[cellid()];
	accept_msg->cms_dest_cell = leader;
	accept_msg->cms_proposal_seq_no = proposal_seq_no;
	cms_trace(CMS_TR_ACCEPT, leader, *receive_set, *send_set);
	cms_forward_message(&msg);
}

/*
 * cms_send_proposal:
 * Sends a proposal message for a new membership. It contains the new 
 * members who wish to join and the local send set.
 * This is used to optimize broadcasts. Proposal is broadcast to everybody
 * in the membership.
 */
void
cms_send_proposal(cell_set_t *new_members_set, cell_t leave_cell)
{
	cms_message_t msg;
	cms_proposal_t *proposal_msg;
	cell_t	cell;

	proposal_msg = &msg.p;
	cms_mesg_init(&msg, CMS_MESG_PROPOSAL);
	set_assign(&proposal_msg->cms_send_set, &cip->cms_local_send_set);
	set_assign(&proposal_msg->cms_new_members_set, new_members_set);
	proposal_msg->cms_leave_cell = leave_cell;
	cms_trace(CMS_TR_PROPOSAL, cip->cms_local_send_set,
						*new_members_set, leave_cell);
	for (cell = 0; cell < MAX_CELLS; cell++)
		if (set_is_member(&cip->cms_local_send_set, cell)) {
			if (cell == cellid()) continue;
			proposal_msg->cms_dest_cell = cell;
			cms_trace(CMS_TR_BROAD_MESG, cell,  0, 0);
			cms_send_mesg(cell, &msg);
		}
}

/*
 * cms_send_req_for_proposal:
 * Send a request to the leader to start a new membership computation phase.
 * The message contains a set of new members if they want to join.
 * Request for proposal is always a directed message.
 */
void
cms_send_req_for_proposal(
	cell_t leader, 
	cell_set_t *new_members_set)
{	
	cms_message_t msg;
	cms_req_for_proposal_t *rpm = &msg.rp;

	cms_mesg_init(&msg, CMS_MESG_REQ_FOR_PROPOSAL);
	rpm->cms_dest_cell = leader;
	if (new_members_set)
		set_assign(&rpm->cms_new_members_set, new_members_set);
	else 
		set_init(&rpm->cms_new_members_set);
	cms_trace(CMS_TR_REQ_PROPOSAL, cip->cms_local_send_set,
				(new_members_set ? *new_members_set : 0),0);

	/* It is assumed that the leader is directly connected to this cell. */
	cms_send_mesg(leader, &msg);
}

/*
 * cms_send_membership_query:
 * This is a probe message to every body in the query set to see if they
 * are in a membership.  If they are send a membership reply.
 */

void
cms_send_membership_query(cell_set_t *query_set)
{
	cms_message_t msg;
	cell_t	cell;

	cms_mesg_init((cms_message_t *)&msg, CMS_MESG_MEMBERSHIP_QUERY);
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(query_set, cell) && (cell != cellid())) {
			msg.cms_dest_cell = cell;
			cms_trace(CMS_TR_MEMB_QUERY, *query_set, cell, 0);
			cms_send_mesg(cell, &msg);
		}
	}
}

/*
 * cms_send_membership_reply:
 * Send a reply with the current membership and age of the members.
 * This should help the receiver decide on the leader.
 */
void
cms_send_membership_reply(
	cms_membership_query_t *qmsg) 
{
	cms_message_t msg;
	cms_membership_reply_t  *reply_mesg;
	cell_t	cell;

	cms_mesg_init((cms_message_t *)&msg, CMS_MESG_MEMBERSHIP_REPLY);
	reply_mesg = &msg.mr;
	for (cell = 0; cell < MAX_CELLS;  cell++)
		if (set_is_member(&cip->cms_membership, cell))
			reply_mesg->cms_age[cell] = cip->cms_age[cell];	
	set_assign(&reply_mesg->cms_members, &cip->cms_membership);
	cms_trace(CMS_TR_MEMB_REPLY, qmsg->cms_src_cell,cip->cms_membership, 0);
	reply_mesg->cms_dest_cell = qmsg->cms_src_cell;
	cms_send_mesg(qmsg->cms_src_cell, &msg);
}

/*
 * cms_send_membership:
 * Send a membership message that contains the new membership. There is no
 * forwarding needed as members are directly connected to each other.
 */
void
cms_send_membership(
	cms_message_t *memb_msg)
{
	cell_t	cell;

	cms_mesg_init(memb_msg, CMS_MESG_MEMBERSHIP);

	for (cell = 0; cell < MAX_CELLS; cell++)
                if (set_is_member( &cip->cms_membership, cell)) {
			if (cell == cellid()) continue;
			cms_trace(CMS_TR_MEMB_MESG, cell, 
				(memb_msg->m.cms_members), 0);
			cms_send_mesg(cell, memb_msg);
		}
}

/*
 * cms_send_membership_leave:
 * Send a message to the leader telling him that I am leaving the membership.
 */
void
cms_send_membership_leave(void)
{
	cms_message_t msg;

	cms_mesg_init((cms_message_t *)&msg, CMS_MESG_MEMBERSHIP_LEAVE);
	cms_send_mesg(cip->cms_leader, &msg);
}

/*
 * cms_recv_message:
 * Receive a message from the network and copy it into the msg buffer
 * If a message is not received it can timeout  after "mesg_timeout" secs
 * and return MESSAGE_TIMEOUT.
 * If there is a receive set change in between it will return with
 * CMS_FAILURE_DETECTED.
 * It rejects messages with same or old sequence numbers. It also
 * forwards messages to indirectly connected cells. 
 */ 
int
cms_recv_message(
	cms_message_t *msg, 
	int mesg_timeout)
{
	int	ret;

	/* If there is a push back message return that. */
	if (cip->cms_flags & CMS_PUSHED_MESSAGE) {
		bcopy(&cip->cms_pushed_message, msg, sizeof(cms_message_t));
		cip->cms_flags &= ~CMS_PUSHED_MESSAGE;
		cms_trace(CMS_TR_RETRV_MESG, msg->cms_src_cell, 0, 0);
		return CMS_MESSAGE_RECEIVED;
	} 

start:
	ret = cms_mesg_wait(msg, mesg_timeout);

	if (ret != CMS_MESSAGE_RECEIVED)
		return ret;

	/* If invalid cellid reject. */
	if (msg->cms_src_cell == CELL_NONE) {
		cms_trace(CMS_TR_RECV_MESG_INV_CELL, msg->cms_src_cell, 0, 0);
		cmn_err(CE_PANIC, "Message received from invalid cell\n");
	}

 	/* If not new sequence number reject. */
	if (msg->cms_mesg_seq_no <= cms_get_exp_seq_no(msg->cms_src_cell, 
						cms_get_mesg_type(msg)))  {
		cms_trace(CMS_TR_RECV_MESG_INV_SEQ_NO, 
			msg->cms_mesg_seq_no,
			(cms_get_exp_seq_no(msg->cms_src_cell, 
				cms_get_mesg_type(msg)) + 1),
			msg->cms_src_cell);
		goto start;
	}

	cms_update_exp_seq_no(msg->cms_src_cell, cms_get_mesg_type(msg), 
				msg->cms_mesg_seq_no);
	cms_forward_message(msg);
	cms_trace(CMS_TR_RECV_MESG, msg->cms_src_cell, msg->cms_mesg_seq_no, 
				cms_get_mesg_type(msg));

	return CMS_MESSAGE_RECEIVED;
}

/*
 * cms_forward_message:
 * Forwards a message to dest. The src_send set is the send set of the
 * sender of this message. local_send set is the send set of the local
 * cell. dest is the cell to which the message should be forwarded. 
 * If dest is BROADCAST_CELL then the message will broadcast to everybody in the
 * local_send_set.
 */
static void
cms_forward_message(
	cms_message_t *mesg)
{
	cell_t		cell, dest;
	cell_set_t	src_send_set;

	/*
	 * Forwarding is needed only for accepts and proposals.
 	 * For accept there is a clear destination.
	 */
	set_init(&src_send_set);
	if (cms_get_mesg_type(mesg) == CMS_MESG_ACCEPT) {
		dest = mesg->cms_dest_cell;
		if (mesg->cms_src_cell != cellid())
			set_assign(&src_send_set, &mesg->a.cms_send_set);
	} else if (cms_get_mesg_type(mesg) == CMS_MESG_PROPOSAL) {
		dest = BROADCAST_CELL;
		if (mesg->cms_src_cell != cellid())
			set_assign(&src_send_set, &mesg->p.cms_send_set);
	} else 
		return;	

	/*
	 * If the message is intended for the current cell, nothing
	 * needs to be done.
	 */
	if (dest == cellid())
		return;

	if (dest != BROADCAST_CELL) { /* If to be send to a specific cell. */
		if (set_is_member(&cip->cms_local_send_set, dest)) {
			cms_trace(CMS_TR_FRWD_DIR, 
				mesg->cms_src_cell, 
				dest, 0);
			cms_send_mesg(dest, mesg);
			return;
		}
	}

	/* Forward the message to all cells that are in the local send set. */
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(&cip->cms_local_send_set, cell)) {
			/*
			 * Skip the current cell 
			 * and the cells in the sender's send set.
			 */
			if (cell == cellid()) 
				continue;
			if (set_is_member(&src_send_set, cell))
				continue;
			cms_trace(CMS_TR_FRWD_INDIR, mesg->cms_src_cell, 
				dest, cell);
			cms_send_mesg(cell, mesg);
		}
	}
	return;
}


/*
 * cms_pushback_message:
 * This is a convenience function. It pushes back a message and will
 * be retrieved the next time somebody does a cms_recv_message().
 * Useful while making state transitions and have to look up the message
 * again.
 */

void
cms_pushback_message(
	cms_message_t *msg)
{
	cms_trace(CMS_TR_PUSHB_MESG, msg->cms_src_cell,  0, 0);
	bcopy(msg, &cip->cms_pushed_message, sizeof(cms_message_t));
	cip->cms_flags |= CMS_PUSHED_MESSAGE;
}

#ifdef	_KERNEL
void
I_cms_send_message(cms_message_t *message)
{
	int spl = mp_mutex_spinlock(&cip->cms_lock);

	while (cip->cms_flags & CMS_BUF_FULL) {
		sv_wait(&cip->cms_mesg_wait, PZERO, &cip->cms_lock, spl);
		spl = mp_mutex_spinlock(&cip->cms_lock);
	}

	cip->cms_flags |= CMS_BUF_FULL;
	bcopy(message, &cip->cms_message_buf, sizeof(cms_message_t));

	if (cip->cms_flags & CMS_DAEMON_WAIT) {
		cip->cms_flags &= ~CMS_DAEMON_WAIT;
		cip->cms_wakeup_reason = CMS_MESSAGE_RECEIVED;
		sv_signal(&cip->cms_daemon_wait);
	}
	if (cip->cms_flags & CMS_TIMEOUT_PENDING) {
		untimeout(cip->cms_toid);
		cip->cms_flags &= ~CMS_TIMEOUT_PENDING;
	}

	mp_mutex_spinunlock(&cip->cms_lock, spl);
}

/* ARGSUSED */
int
cms_mesg_wait(cms_message_t *msg, int msg_timeout)
{
	int spl = mp_mutex_spinlock(&cip->cms_lock);
	int ret;

	if (!msg_timeout) {
		mp_mutex_spinunlock(&cip->cms_lock, spl);
		return CMS_MESSAGE_TIMEOUT;
	}

	for (;;) {
		
		if (cip->cms_flags & CMS_BUF_FULL) {
			bcopy(&cip->cms_message_buf, msg, 
							sizeof(cms_message_t));
			cip->cms_flags &= ~CMS_BUF_FULL;
			sv_signal(&cip->cms_mesg_wait);
			mp_mutex_spinunlock(&cip->cms_lock, spl);
			return CMS_MESSAGE_RECEIVED;
		}

		if (cip->cms_wakeup_reason) {
			ret = cip->cms_wakeup_reason;
			cip->cms_wakeup_reason = 0;
			mp_mutex_spinunlock(&cip->cms_lock, spl);
			return ret;
		}

		cip->cms_flags |= CMS_DAEMON_WAIT;	
		cip->cms_wakeup_reason = 0;

		if (msg_timeout != CMS_NOTIMEOUT) {
			cip->cms_toid = 
				timeout(cms_mesg_timeout, 0, msg_timeout);
			cip->cms_flags |= CMS_TIMEOUT_PENDING;
		}

		sv_wait(&cip->cms_daemon_wait, PZERO|PLTWAIT, 
					&cip->cms_lock, spl);

		spl = mp_mutex_spinlock(&cip->cms_lock);
	}
}


static void
cms_mesg_timeout(void)
{
	int spl = mp_mutex_spinlock(&cip->cms_lock);

	cip->cms_flags &= ~CMS_TIMEOUT_PENDING;
	cip->cms_wakeup_reason = CMS_MESSAGE_TIMEOUT;

	if (cip->cms_flags & CMS_DAEMON_WAIT) { 
		cip->cms_flags &= ~CMS_DAEMON_WAIT;
		sv_signal(&cip->cms_daemon_wait);
	}
	mp_mutex_spinunlock(&cip->cms_lock, spl);
}

/*
 * cms_detected_cell_failure:
 * Called by the heart beat or the messaging service to 
 * inform of any cell failure.
 */
void
cms_detected_cell_failure(cell_t cell)
{
	
	int spl;

		
	mesg_channel_set_state(cell, MESG_CHANNEL_MAIN, MESG_CHAN_FROZEN);
	spl = mp_mutex_spinlock(&cip->cms_lock);

	if (cell == cellid())
		cip->cms_wakeup_reason = CMS_CELL_DIED;
	else
		cip->cms_wakeup_reason = CMS_FAILURE_DETECTED;

	if (cip->cms_flags & CMS_DAEMON_WAIT) { 
		cip->cms_flags &= ~CMS_DAEMON_WAIT;
		sv_signal(&cip->cms_daemon_wait);
	}

	if (cip->cms_flags & CMS_TIMEOUT_PENDING) {
		untimeout(cip->cms_toid);
		cip->cms_flags &= ~CMS_TIMEOUT_PENDING;
	}
	mp_mutex_spinunlock(&cip->cms_lock, spl);
}

/*
 * cms_wake_daemon:
 * Called to wakeup the daemon with a reason. Macros hide this function.
 */
void
cms_wake_daemon(int reason, boolean_t locked)
{
	int spl;

	if (!locked)
		spl = mp_mutex_spinlock(&cip->cms_lock);

	cip->cms_wakeup_reason = reason;

	if (cip->cms_flags & CMS_DAEMON_WAIT) { 
		cip->cms_flags &= ~CMS_DAEMON_WAIT;
		sv_signal(&cip->cms_daemon_wait);
	}

	if (cip->cms_flags & CMS_TIMEOUT_PENDING) {
		untimeout(cip->cms_toid);
		cip->cms_flags &= ~CMS_TIMEOUT_PENDING;
	}
	if (!locked)
		mp_mutex_spinunlock(&cip->cms_lock, spl);
}

void
cms_leave_membership(void)
{
	cms_wake_daemon(CMS_CELL_LEAVE, B_FALSE);
}

	
#endif
