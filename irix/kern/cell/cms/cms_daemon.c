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
#include "cms_membership.h"
#include "cms_trace.h"
#ifdef	_KERNEL
#include "I_cms_stubs.h"
#include "invk_cms_stubs.h"
#include "ksys/sthread.h"
#include "ksys/cell.h"
#include "ksys/cell/recovery.h"
#include "sys/kopt.h"
#endif

/* 
 * Timeout after sending a membership query. Useful if we know the other
 * cell exists but does not know about us. This is a small timeout value.
 */
#define	MEMBERSHIP_QUERY_TIMEOUT	5*HZ	

cms_info_t *cip;	/* Local cell membership service info. structure */

static void cms_nascent(void);
static void cms_follower(void);
static void cms_leader(void);
static void cms_stable(void);
static boolean_t cms_declare_membership(void);
static void cms_set_membership(cms_membership_t *);
static int cms_ignore_proposal(cms_message_t *);
static int cms_membership_exists(cms_membership_reply_t *);
static int cms_mark_accept_message(cms_accept_t *);
static void cms_initialize_recv_sets(void);
static boolean_t cms_new_member_join(void);
static void cms_reset_cells(cell_set_t *, cell_set_t *);
static void cms_reset_exp_seq_nos(cell_set_t *, cell_set_t *);
static void cms_deliver_membership(void);
static void cms_cell_cleanup(cell_set_t *);
static boolean_t cms_recovery_complete(void);

extern void cms_reset_cell(cell_t);

/*
 * This is defined in a user specific file for user level test.
 * Its a stub inside the kernel.
 */
extern void cms_dead(void);

cell_set_t	cell_membership;

int	cms_ignore_membership_check = 0;


/*
 * cms_init:
 * Basic membership initialization. Allocate kernel data structures.
 */
void
cms_init(void)
{
	cell_t	i;
	extern int cmsd_pri;

#ifdef	_KERNEL
	cip = kmem_zalloc(sizeof(cms_info_t), KM_SLEEP);
#endif
	spinlock_init(&cip->cms_lock, "cms_lock");
	sv_init(&cip->cms_daemon_wait, SV_DEFAULT, "Daemon wait");
	sv_init(&cip->cms_mesg_wait, SV_DEFAULT, "Message wait");
	cip->cms_leader = CELL_NONE;	/* No leader yet */
	cip->cms_leave_cell = CELL_NONE;	/* No leader yet */
	cms_get_config_info();
	cms_set_state(CMS_NASCENT);
	cms_trace_init();
	for (i = 0; i < MAX_CELLS; i++)
		set_add_member(&set_universe, i);
	set_add_member(&cip->cms_potential_cell_set,  cellid());
	if (cms_comb_dynamic_init() == -1) 
		cmn_err(CE_PANIC, 
			"cms_comb_dynamic_init cannot alloc memory\n");
	cmn_err(CE_NOTE,"Initializing cms\n");

	mesg_handler_register(cms_msg_dispatcher, CMS_SVC_SUBSYSID);
        /*
         * Start the membership daemon. This should be done before any
         * real cellular communication begins.
         */

        sthread_create("cmsd", 0, 0, 0, cmsd_pri, KT_PS,
                        (st_func_t *)cms, 0, 0, 0, 0);
}



/*
 * cms:
 * Basic daemon. Its a state machine and goes from one state to another.
 */
void
cms(void)
{
	cmn_err(CE_NOTE,"cell %d: Membership daemon started\n", cellid());
	for(;;) {
		cms_trace(CMS_TR_NEW_STATE, cip->cms_state, 0, 0);
		switch (cip->cms_state) {

		case	CMS_NASCENT:
			cms_nascent();
			break;

		case	CMS_FOLLOWER:
			cms_follower();
			break;

		case	CMS_LEADER:
			cms_leader();
			break;

		case	CMS_STABLE:
			cms_stable();
			break;

		case	CMS_DEAD:
			cms_dead();
			break;
		}
	}
}


/*
 * cms_nascent:
 * First state  the membership daemon gets into after initialization.
 * It sends a query to the cells it can talk to and awaits a reply.
 * If the reply says that there is a membership it enters the follower state
 * and awaits a new proposal. If not it timesout and sees if its can be the
 * leader and send a proposal. If it cannot be the leader it sends the
 * request for proposal to the real leader.
 */

void
cms_nascent(void)
{
	int		timeout_val;
	/* REFERENCED */
	int		ret;
	cms_message_t	message;
	time_t		start_tick;

	/*
	 * Sleep until a majority is available. This helps prevent
	 * membership storms (a series of aborted incomplete memberships).
	 */
	while (cms_quorum_check(QUORUM_TYPE_RUN, &cip->cms_potential_cell_set) 
							== QUORUM_MINORITY)
		delay(CMS_NASCENT_TIME);

	cms_send_membership_query(&(cip->cms_potential_cell_set));

	timeout_val = cip->cms_nascent_timeout;


	for(;;) {
		start_tick = lbolt;
		cms_trace(CMS_TR_TIMEOUT, timeout_val, lbolt, start_tick);
		switch(ret = cms_recv_message(&message, timeout_val)) {

		case CMS_MESSAGE_RECEIVED:

			switch(cms_get_mesg_type(&message)) {
			case	CMS_MESG_MEMBERSHIP_REPLY:

				/*
				 * This cell can talk to somebody in the 
				 * membership. Wait for a proposal.
 				 * Change state to follower. Also copy
			 	 * the age. This will help in deciding the
				 * leader.
				 */

				if (cms_membership_exists(&message.mr)) {
					cip->cms_flags |= CMS_MEMBERSHIP_EXISTS;
					timeout_val = MEMBERSHIP_QUERY_TIMEOUT;
					continue;
				} else {
					timeout_val -= (lbolt - start_tick);
					continue;
				}

			case	CMS_MESG_MEMBERSHIP_QUERY:
				timeout_val -= (lbolt - start_tick);
				continue;

			case	CMS_MESG_PROPOSAL:
				/*
				 * If somebody else sent us a proposal
				 * just accept it even if I can be the leader.
				 * XXX This prevents membership storms.
				 */
				cms_pushback_message(&message);
				cms_set_state(CMS_FOLLOWER);
				return;

			default:
				cmn_err(CE_PANIC, "Invalid message %d\n",
					cms_get_mesg_type(&message));
			}
			break;

			/*
			 * If all cells are up send a query to see if anybody
			 * is in the membership. We send a membership query 
			 * and wait for a smaller timeout value to see if
			 * anybody sends a reply.
			 */
		case	CMS_ALL_CELLS_UP:
			cms_send_membership_query(
				&(cip->cms_potential_cell_set));
			timeout_val = MEMBERSHIP_QUERY_TIMEOUT;
			break;

		case	CMS_MESSAGE_TIMEOUT:
		case	CMS_FAILURE_DETECTED:

			/*
			 * If there is an existing membership, try
			 * sending the query again. Its might be just
 			 * that the proposal is delayed.
			 */
			if (cip->cms_flags & CMS_MEMBERSHIP_EXISTS) {
				cip->cms_flags &= ~CMS_MEMBERSHIP_EXISTS;
				cms_send_membership_query(
					&(cip->cms_potential_cell_set));
				timeout_val = MEMBERSHIP_QUERY_TIMEOUT;
				continue;
			}

			/*
			 * Get the connectivity set.
			 * This will tell if I am the leader.
			 * I can send a proposal but others might ignore it
			 * if its not in their receive set. I will never
		 	 * be the leader if there is already a membership 
			 * in the system. The age will ensure that I am not
			 * elected the leader. I will be elected the leader
			 * only if nobody else has a membership (age == 0) 
			 * and I am the lowest cellid in the send set.
			 */
			cms_trace(CMS_TR_FAILURE, ret, cip->cms_local_send_set,
					cip->cms_local_recv_set);
			cip->cms_leader = cms_elect_leader(
						&(cip->cms_potential_cell_set));
			if (cip->cms_leader == cellid()) {
				cms_get_connectivity_set(
					&(cip->cms_potential_cell_set), 
					CELL_NONE);
				cip->cms_leader = cellid();
				cms_set_state(CMS_LEADER);
				return;
			} else {
				cms_send_membership_query(
					&(cip->cms_potential_cell_set));
				timeout_val = MEMBERSHIP_QUERY_TIMEOUT;
				continue;
			}

		case	CMS_CELL_DIED:
			cms_set_state(CMS_DEAD);
			return;

		default:
			break;
		}
	}
}

/*
 * cms_follower:
 * Enter the follower state. Wait for a proposal and reply with an accept.
 * If there  is a failure or timeout and start another membership phase
 * by either sending a request for proposal or a proposal.  After sending
 * an accept wait for a membership message.
 */
void
cms_follower(void)
{
	cms_message_t	message;
	/* REFERENCED */
	int		ret;
	cell_set_t	myself;

	set_init(&myself);
	set_add_member(&myself, cellid());

	for(;;) {
		switch(ret = cms_recv_message(&message,
					cip->cms_follower_timeout)) {
		case CMS_MESSAGE_RECEIVED:
			switch(cms_get_mesg_type(&message)) {
			case	CMS_MESG_PROPOSAL:

				/*
				 * If this is another proposal then ignore it
				 * if need be.
				 */
				if (cms_ignore_proposal(&message))
					continue;
				cip->cms_leader = message.cms_src_cell;

				/*
				 * Clear the entries in the new member set
				 * for which we already have a proposal.
				 * Otherwise if a membership is delivered
				 * and the cells in the new member set 
				 * are not in the membership we try
				 * another round of membership which can
				 * lead to an infinite loop.
				 */
				set_exclude(&cip->cms_new_member_set,
					&message.p.cms_new_members_set);
				cms_get_connectivity_set(
					&message.p.cms_new_members_set,
					message.p.cms_leave_cell);
				cms_send_accept(cip->cms_leader, 
					&cip->cms_local_recv_set, 
					&cip->cms_local_send_set, 
					message.cms_mesg_seq_no);

				continue;

			case	CMS_MESG_MEMBERSHIP:
				cms_set_membership(
					(cms_membership_t *)&message);
				cms_set_state(CMS_STABLE);
				return;

			case	CMS_MESG_ACCEPT:
				continue;

			case	CMS_MESG_MEMBERSHIP_QUERY:
				set_add_member(&cip->cms_new_member_set,
						message.cms_src_cell);
				cms_send_membership_reply(
					(cms_membership_query_t *)&message);
				continue;

			case	CMS_MESG_MEMBERSHIP_REPLY:

				/*
				 * Ignore other membership replies.
				 */
				continue;

			case    CMS_MESG_REQ_FOR_PROPOSAL:

				/*
				 * If this is a req for proposal from a new 
				 * member drop it. This can happen if the new 
				 * member times out and thinks he is the leader.
				 * We don't want new members to join in the
				 * middle of MS computation.
				 * This comment is for completeness.

				if (!set_is_member(message.cms_src_cell, 
					&cip->cms_local_recv_set))
					continue;
				*/
				continue;

			default:
				cmn_err(CE_PANIC, "Invalid message\n");
			}
			break;

		case	CMS_MESSAGE_TIMEOUT:
		case	CMS_FAILURE_DETECTED:

			cms_get_connectivity_set(0, CELL_NONE);
			cip->cms_leader = cms_elect_leader(
						&cip->cms_local_send_set);

			cms_trace(CMS_TR_FAILURE, ret, cip->cms_local_send_set,
					cip->cms_local_recv_set);
				
			if (cip->cms_leader == cellid()) {
				cms_set_state(CMS_LEADER);
				return;
			} else {
				if (set_is_empty(&cip->cms_membership))
					cms_send_req_for_proposal(
						cip->cms_leader, &myself);
				else
					cms_send_req_for_proposal(
						cip->cms_leader, 
						&cip->cms_new_member_set);
			}
			break;

		case	CMS_CELL_DIED:
			cms_set_state(CMS_DEAD);
			return;

		default:
			break;
		}
	}
}

/*
 * cms_leader:
 * Daemon in this states sends a proposal, receives accepts and delivers
 * membership.
 */
void
cms_leader(void)
{
	cms_message_t	message;
	time_t		timeout_val;
	time_t		start_tick;

	cip->cms_leader = cellid();

	/*
	 * Initialize the accept set to include all known members.
	 */
	set_assign(&cip->cms_accept_set, &cip->cms_new_member_set);
	if (!set_is_empty(&cip->cms_membership))
		set_union(&cip->cms_accept_set, &cip->cms_membership);
	else
		set_union(&cip->cms_accept_set, &cip->cms_potential_cell_set);
	set_del_member(&cip->cms_accept_set, cellid());

	/*
	 * Zero all the receive sets
	 * and send a proposal to include the new members if any. Clear
 	 * the new member set as the proposal includes them.
	 */
	cms_initialize_recv_sets();
	cms_send_proposal(&cip->cms_new_member_set, cip->cms_leave_cell);
	set_init(&cip->cms_new_member_set);


	timeout_val = cip->cms_leader_timeout;
	start_tick = lbolt;
	for(;;) {
		timeout_val -= (lbolt - start_tick);
		cms_trace(CMS_TR_TIMEOUT, timeout_val, lbolt, 
			start_tick);
		start_tick = lbolt;
		switch(cms_recv_message(&message, timeout_val)) {

		case CMS_MESSAGE_RECEIVED:
			switch(cms_get_mesg_type(&message)) {
			case	CMS_MESG_PROPOSAL:

				/*
				 * If this is a proposal from a new member
				 * drop it. This can happen if the new member
			 	 * times out and thinks he is the leader.
				 */

				if (!set_is_member(&cip->cms_local_recv_set,
					message.cms_src_cell))
					continue;
				/*
				 * If this is another proposal then ignore it
				 * if necessary.
				 */
				if (!cms_ignore_proposal(&message)) {
					cms_pushback_message(&message);
					cms_set_state(CMS_FOLLOWER);
					return;
				}
				continue;

			case	CMS_MESG_ACCEPT:

				/*
				 * If this is not intended for the latest
				 * proposal reject it.
				 */
				if (message.a.cms_proposal_seq_no <
					cms_get_exp_seq_no(cellid(), 
						CMS_MESG_PROPOSAL))
					continue;

				/*
				 * If all accepts received break.
				 */
				if (cms_mark_accept_message(&message.a)) {
					if (cms_declare_membership() == B_FALSE)
						cms_set_state(CMS_DEAD);
					else
						cms_set_state(CMS_STABLE);
					return;
				}
				continue;

			case	CMS_MESG_MEMBERSHIP_QUERY:
				set_add_member(&cip->cms_new_member_set,
						message.cms_src_cell);
				cms_send_membership_reply(&message.mq);
				continue;
	
			case	CMS_MESG_REQ_FOR_PROPOSAL:

				continue;

			default:
				cmn_err(CE_PANIC, "Invalid message\n");
			}
			break;

		case	CMS_MESSAGE_TIMEOUT:
			if (cms_declare_membership() == B_FALSE)
				cms_set_state(CMS_DEAD);
			else
				cms_set_state(CMS_STABLE);
			return;

		case	CMS_FAILURE_DETECTED:
			cms_get_connectivity_set(0, CELL_NONE);
			cms_trace(CMS_TR_FAILURE, CMS_FAILURE_DETECTED, 
					cip->cms_local_send_set,
					cip->cms_local_recv_set);
			cip->cms_leader = cms_elect_leader(
						&cip->cms_local_send_set);
			if (cip->cms_leader == cellid()) {
				cms_set_state(CMS_LEADER);
				return;
			} else {
				cms_send_req_for_proposal(cip->cms_leader, 
						&cip->cms_new_member_set);
				cms_set_state(CMS_FOLLOWER);
				return;
			}

		case	CMS_CELL_DIED:
			cms_set_state(CMS_DEAD);
			return;

		default:
			break;
		}
	}
}

/*
 * cms_stable:
 * Membership has been delivered. Enter the stable state. If the daemon
 * receives a request for proposal or gets a membership query it will
 * enter either the leader or the follower state and start another membership
 * phase.
 */
void
cms_stable(void)
{
	cms_message_t	message;

	/*
	 * If new members want to join check to see if they are alreay
	 * in the membership set. If so nothing to do.
 	 * Else if the local cell is the leader then send a proposal 
	 * with the new members else send a req for proposal and become
	 * a follower.
	 */


	cms_deliver_membership();
	cip->cms_leader = CELL_NONE;	/* Leader is not necessary anymore */
	cip->cms_leave_cell = CELL_NONE;

	if (cms_new_member_join()) {
		return;
	}
		
	for (;;) {
		switch(cms_recv_message(&message, CMS_NOTIMEOUT)) {

		case CMS_MESSAGE_RECEIVED:
			switch(cms_get_mesg_type(&message)) {
			case	CMS_MESG_PROPOSAL:

				cms_pushback_message(&message);
				cms_set_state(CMS_FOLLOWER);

				/*
				 * We are delivering a new membership. Clear 
				 * the flag.
				 */

				return;

			case	CMS_MESG_MEMBERSHIP:
				cmn_err(CE_PANIC, 
					"Membership without proposal\n");
				return;

				/* Accept coming too late. Ignore. */
			case	CMS_MESG_ACCEPT:
				continue;

			case	CMS_MESG_MEMBERSHIP_QUERY:
				set_add_member(&cip->cms_new_member_set,
						message.cms_src_cell);
				cms_send_membership_reply(&message.mq);
				if (cms_new_member_join()) {
					return;
				}
				continue;

			case	CMS_MESG_REQ_FOR_PROPOSAL:
				set_union(&cip->cms_new_member_set, 
					&message.rp.cms_new_members_set);
				cms_get_connectivity_set(
					&cip->cms_new_member_set, CELL_NONE);
				cms_set_state(CMS_LEADER);
				return;


			case	CMS_MESG_MEMBERSHIP_LEAVE:
				cip->cms_leave_cell = message.cms_src_cell;
				cms_set_state(CMS_LEADER);
				return;

			default:
				cmn_err(CE_PANIC, "Invalid message\n");
			}
			break;

		case	CMS_FAILURE_DETECTED:

			cms_trace(CMS_TR_FAILURE, CMS_FAILURE_DETECTED, 
					cip->cms_local_send_set,
					cip->cms_local_recv_set);

			cms_get_connectivity_set(0, CELL_NONE);
			cip->cms_leader = 
				cms_elect_leader(&cip->cms_local_send_set);

			if (cip->cms_leader == cellid()) {
				cms_set_state(CMS_LEADER);
				return;
			} else {
				cms_send_req_for_proposal(cip->cms_leader, 0);
				cms_set_state(CMS_FOLLOWER);
				return;
			}

		case	CMS_CELL_LEAVE:
			/*
			 * Reset the age to zero to exclude local cell from the
			 * membership.
			 */
			cip->cms_age[cellid()] = 0;
			cip->cms_leader = cms_elect_leader(
						&cip->cms_local_send_set);
			cms_send_membership_leave();
			cms_set_state(CMS_DEAD);
			return;

		case	CMS_CELL_DIED:

			/*
			 * We are delivering a new membership. Clear 
			 * the flag.
			 */
			cms_set_state(CMS_DEAD);
			return;

		case	CMS_RECOVERY_COMPLETE:

			/*
			 * Cell recovery has been done. Check if any
		 	 * new memberships are pending.
			 */
			if (cms_new_member_join())
				return;
			else
				continue;

		default:
			break;
		}
	}
}


#ifdef	_KERNEL

void
cms_dead(void)
{
	cmn_err(CE_NOTE, "Cell %d in Dead state \n", cellid());
	sthread_exit();
}

#endif

/*
 * Receive an accept message. Mark the cell from which we received an
 * accept message. Also copy its receive set.
 */
int
cms_mark_accept_message(cms_accept_t *mesg)
{
	set_del_member(&cip->cms_accept_set, mesg->cms_src_cell);
	set_assign(&(cip->cms_recv_set[mesg->cms_src_cell]), 
				&mesg->cms_recv_set);

	cip->cms_age[mesg->cms_src_cell] = mesg->cms_age; 

	/*
	 * If all cells in the membership have sent accepts.
 	 * we are done. This includes cells in the previous membership
	 * and the new cells that want to join.
	 */
	if (set_is_empty(&cip->cms_accept_set))
		return B_TRUE;

	return B_FALSE;
}

/*
 * cms_declare_memebrship:
 * Declare a new membership. It computes the best possible cliques from
 * available receive sets. After it computes the receive sets it will
 * reset the cells that are in the old membership but not in the new one.
 * Also bumps the age of the cells in the new membership by one.
 */
boolean_t
cms_declare_membership(void)
{
	cms_message_t		msg;
	cms_membership_t	*memb_msg;
	cell_t			cell;
	int			i;
	cell_set_t		old_membership;

	memb_msg = &msg.m;

	set_assign(&cip->cms_recv_set[cellid()], &cip->cms_local_recv_set);

	for (i = 0; i < MAX_CELLS; i++) 
		if (cip->cms_recv_set[i])
			cms_trace(CMS_TR_MEMB_RECV_SET, i, 
					cip->cms_recv_set[i], 0);

	set_assign(&old_membership, &cip->cms_membership);
	cms_membership_compute(&cip->cms_membership, B_FALSE);;

	if (cms_membership_validate(&cip->cms_membership, B_FALSE) == B_FALSE) {
		cms_trace(CMS_TR_MEMB_FAIL, 0, 0, 0);
		return B_FALSE;
	}

	/*
	 * If the current cell is not in the membership then it should
	 * not deliver the membership as it has to be forwarded
	 * It can avoid forwarding by not delivering the membership and just
	 * resetting itself.
	 */
	if (!set_is_member(&cip->cms_membership, cellid())){
		cms_trace(CMS_TR_MEMB_FAIL, 0, 0, 0);
		return B_FALSE;
	}

	/*
	 * Reset cells that are in the old membership but not in the
	 * new one.
	 */

	cms_reset_cells(&old_membership, &cip->cms_membership);

	cms_reset_exp_seq_nos(&old_membership, &cip->cms_membership);

	/*
	 * Remove the non-members from our send and receive sets.
 	 */
	set_intersect(&cip->cms_local_send_set, &cip->cms_membership);
	set_intersect(&cip->cms_local_recv_set, &cip->cms_membership);

	cms_trace(CMS_TR_MEMB_SUCC, cip->cms_membership, 0, 0);
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(&cip->cms_membership, cell))  {
			set_assign(&memb_msg->cms_members, 
					&cip->cms_membership);
			cip->cms_age[cell]++;
			memb_msg->cms_age[cell] = cip->cms_age[cell];
			cms_trace(CMS_TR_AGE, cell, cip->cms_age[cell], 0);
		}
	}

	cms_send_membership(&msg);
	return B_TRUE;
}

/*
 * cms_reset_cells:
 * Reset cells in the old membership but not in the new one.
 */
void
cms_reset_cells(cell_set_t *old_membership, cell_set_t *new_membership)
{
	cell_t	cell;

	for (cell = 0; cell < MAX_CELLS; cell++)
		if (set_is_member(old_membership, cell) && 
			(!set_is_member(new_membership, cell))) {
			cms_trace(CMS_TR_RESET_CELLS, cell, *old_membership,
							*new_membership);
			cms_reset_cell(cell);
		}
}

/*
 * cms_set_membership:
 * Stores the new membership info delivered by the leader. The message contains
 * the new membership and the ages of all the cells.
 */
void
cms_set_membership(cms_membership_t *msg)
{
	cell_t		cell;
	cell_set_t	old_membership;

	set_assign(&old_membership, &cip->cms_membership);
	set_assign(&cip->cms_membership, &msg->cms_members);
	cms_reset_exp_seq_nos(&old_membership, &cip->cms_membership);
	
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member( &cip->cms_membership, cell)) {
			cip->cms_age[cell] = msg->cms_age[cell];
			cms_trace(CMS_TR_AGE, cell, cip->cms_age[cell], 0);
		}
	}

	/*
	 * Remove the non-members from our send and receive sets.
 	 */
	set_intersect(&cip->cms_local_send_set, &cip->cms_membership);
	set_intersect(&cip->cms_local_recv_set, &cip->cms_membership);
}

/*
 * cms_reset_exp_seq_nos:
 * Reset the expected sequence numbers of the cells which are not in the 
 * membership. As we reset the cells that are not in the membership the
 * first sequence number will be zero.
 */
void
cms_reset_exp_seq_nos(cell_set_t *old_membership, cell_set_t *new_membership)
{
	cms_mesg_type_t i;
	cell_t	cell;
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(old_membership, cell) && 
			(!set_is_member(new_membership, cell)))
				for (i = CMS_MESG_REQ_FOR_PROPOSAL; 
						i < CMS_NUM_MESG_TYPES; i++)
					cms_update_exp_seq_no(cell, i, 0);
	}

}

/* 
 * cms_elect_leader:
 * Elect the oldest member from the choice of cells . If the age is the same
 * the cell with the lowest id is the leader.
 */
cell_t
cms_elect_leader(cell_set_t *candidate_set)
{
	cell_t	leader = CELL_NONE;
	cell_t	cell;

	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(candidate_set, cell)) {
			if (leader != CELL_NONE) {
				if (cip->cms_age[leader] <
					cip->cms_age[cell])
					leader = cell;
			} else
				leader = cell;
		}
	}
	return leader;
}

/*
 * cms_get_connectivity_set:
 * Get the send and receive sets. It restricts the send and receive
 * sets to the cells in the current membership and any new cells
 * that are passed in the new_members set. It also excludes any cells that
 * want to leave. If there is a change in
 * the send/receive sets from the previous one it returns CMS_FAILURE_DETECTED
 * or CMS_CELL_JOIN else it returns 0.
 * If there is no existing membership it accepts everybody in the potential 
 * cell set. The potential cell set is one to which the local cells has set up
 * a membership channel.
 */
int
cms_get_connectivity_set(cell_set_t *new_members, cell_t leaving_cell)
{
	cell_set_t	send_set, recv_set, known_set;
	int		ret;
	cell_t		cell;

	/*
	 * Get the connections setup and start heart beat to the new cells
	 */
	if (new_members) {
		for (cell = 0; cell < CELL_SET_SIZE; cell++)
			/*
			 * Try to open the main channel and heart beat.
			 * Do it only if our transport layer can talk to it.
		 	 * i.e., the membership channel is open.
			 */ 
			if (set_is_member(new_members, cell) && 
				(set_is_member(&cip->cms_potential_cell_set, 
								cell))) {
			        service_t       svc;

				if (cell != cellid()) {
					/* REFERENCED */
					int	msgerr;
					/*
					 * Disconnect previous opened
					 * channels.
					 */
					SERVICE_MAKE(svc, cell, SVC_CMSID);
					msgerr = invk_cms_mesg_connect(svc, cellid());
					ASSERT(!msgerr);
					cell_connect(cell);
				}
			}
	}


	/*
	 * Check for cells that are in the membership and that want to
	 * join. If there is no existing membership try to heart beat cells
	 * that can potentially join the membership. Note that this is
	 * done once while booting to form the first membership. After that
	 * the connectivity set is restricted to the existing membership
	 * and any new members.
	 */
	if (!cip->cms_membership) {
		set_assign(&known_set, &cip->cms_potential_cell_set);
		for (cell = 0; cell < CELL_SET_SIZE; cell++)
			if (set_is_member(&known_set, cell)) {
			        service_t       svc;

				if (cell != cellid()) {
					/* REFERENCED */
					int	msgerr;

					SERVICE_MAKE(svc, cell, SVC_CMSID);
					msgerr = invk_cms_mesg_connect(svc, cellid());
					ASSERT(!msgerr);
					cell_connect(cell);
				}
			}
	} else 
		set_assign(&known_set, &cip->cms_membership);

	if (new_members) 
		set_union(&known_set, new_members);

	/*
	 * Get the cells that have successful heart beats.
	 */
	hb_get_connectivity_set(&send_set, &recv_set);

	/*
	 * Exclude the leaving cell from the connectivity set.
	 */

	if (leaving_cell != CELL_NONE)
		set_del_member(&known_set, leaving_cell);

	set_intersect(&send_set, &known_set);
	set_intersect(&recv_set, &known_set);

	ret = 0;
	if ((!set_equal(&send_set, &cip->cms_local_send_set)) ||
		(!set_equal(&recv_set, &cip->cms_local_recv_set))) {

		if (set_is_subset(&cip->cms_local_send_set, &send_set) 
			&& set_is_subset(&cip->cms_local_recv_set, &recv_set))
			ret =  CMS_FAILURE_DETECTED;
		 else	
			ret =  CMS_CELL_JOIN;

		set_assign(&cip->cms_local_send_set, &send_set);
		set_assign(&cip->cms_local_recv_set, &recv_set);
	} 

	if (ret)
		cms_trace(CMS_TR_CONN_SET, send_set, recv_set, cip->cms_membership);
	return ret;
}

void
I_cms_mesg_connect(cell_t src_cell)
{
	mesg_connect(src_cell, MESG_CHANNEL_MAIN);
}

void
I_cms_mesg_disconnect(cell_t src_cell)
{
	mesg_disconnect(src_cell, MESG_CHANNEL_MAIN);
}

/*
 * cms_ignore_proposal:
 * Test to see if this proposal can be ignored. It can be ignored the
 * new leader is younger in terms of age or has a higher cell number if
 * its of the same age as the current leader.
 */
int
cms_ignore_proposal(cms_message_t *msg)
{
	if ((cip->cms_leader == CELL_NONE) ||
	 	(cip->cms_age[msg->cms_src_cell] 
			>= cip->cms_age[cip->cms_leader]) ||
		(msg->cms_src_cell < cip->cms_leader)) {
		/* Don't ignore */
		return 0;
	}

	cms_trace(CMS_TR_PROPOSAL_IGNORE, msg->cms_src_cell, 
		cip->cms_age[msg->cms_src_cell], 
		cip->cms_age[cip->cms_leader]);

	return 1;
}


/*
 * cms_membership_exists:
 * Tests to see if a membership exists by looking at the 
 * membership reply message (in response to a query).
 * The reply also returns the ages of the cells in the membership.
 * This will help us in computing the right leader.
 */
static int
cms_membership_exists(cms_membership_reply_t *msg)
{
	cell_t	cell;

	if (msg->cms_members == 0)
		return B_FALSE;

	for (cell = 0; cell < MAX_CELLS; cell++)
		if (set_is_member(&msg->cms_members, cell))
			cip->cms_age[cell] = msg->cms_age[cell];
	return (B_TRUE);
}


/*
 * cms_initialize_recv_sets:
 * Initialize the receive sets to zero prior to receiving accepts.
 */
static void
cms_initialize_recv_sets(void)
{
	cell_t	i;

	for (i = 0; i < MAX_CELLS;  i++)
		set_init(&cip->cms_recv_set[i]);
}

/*
 * cms_new_member_join:
 * Looks at the new member set and checks if a new membership phase has to
 * be started to include the new member. If the new member is already in the
 * set we just clear the new member set. This can happen if multiple 
 * cells get a membership query and one of them proposes the membership.
 * If recovery is in progress ignore membership requests to grow the
 * membership.
 */
static boolean_t
cms_new_member_join(void)
{
	if (!cms_recovery_complete())
		return B_FALSE;

	if (cip->cms_new_member_set) {  /* New members want to join. */
		if (!set_is_subset(&cip->cms_membership, 
				&cip->cms_new_member_set)) {
			cip->cms_leader = cms_elect_leader(&cip->cms_membership);
			
			/*
			 * Get the connectivity set to include the 
			 * new members
			 */
			cms_get_connectivity_set(&cip->cms_new_member_set, 
								CELL_NONE);
			if (cip->cms_leader != cellid()) {
				cms_send_req_for_proposal(cip->cms_leader,
						&cip->cms_new_member_set);
				cms_set_state(CMS_FOLLOWER);
			} else {
				cms_set_state(CMS_LEADER);
			}
			return B_TRUE;
		} else
			set_init(&cip->cms_new_member_set);
	}
	return B_FALSE;
}

#ifdef	_KERNEL
#if	defined(EVEREST) && defined(MULTIKERNEL)
#include "sys/EVEREST/everest.h"
#endif

/*
 * cms_get_config_info:
 * The leader timeout should be less than the nascent timeout.
 */
void
cms_get_config_info()
{
#if     defined(EVEREST) && defined(MULTIKERNEL)
	cip->cms_num_cells = evmk.numcells;
#elif defined(CELL_IRIX)
	cip->cms_num_cells = MAX_CELLS;
#else
	if (is_specified(arg_num_cells)) {
		cmn_err(CE_WARN, "Membership services configuring %d cells\n",
				atoi(arg_num_cells)); 
		cip->cms_num_cells = atoi(arg_num_cells);
	} else {
		cmn_err(CE_WARN,"Cannot get number of cells setting to 2\n"
			"Use the environment variable numcells \n");
		cip->cms_num_cells = 2;
	}
#endif
	/* Large enough to receive all accepts */
	cip->cms_leader_timeout = 10*HZ; 

	/*
	 * The follower timeout should be > leader timeout.
	 * Otherwise it might send req for proposal again before the
	 * leader timeouts accepts and can send the membership.
	 */
	cip->cms_follower_timeout = 15*HZ; /* Large enough to get a proposal */
#ifdef	CELL_ARRAY
	cip->cms_nascent_timeout = 80*HZ;
#else
	cip->cms_nascent_timeout = 10*HZ;
#endif
	cip->cms_tie_timeout = 60*HZ;
}

void
cms_reset_cell(cell_t cell)
{
	cmn_err(CE_NOTE,"Resetting cell %d\n", cell);
}	

#endif

#ifdef	_KERNEL
/*
 * cms_wait_for_initial_membership:
 * Wait for an initial membership to be delivered.
 */
void
cms_wait_for_initial_membership()
{
	int	spl;

	/*
 	 * If we have delivered at least one membership
	 * return.
	 */

	spl = mp_mutex_spinlock(&cip->cms_lock);

	if (cip->cms_flags & CMS_MEMBERSHIP_STABLE) {
		mp_mutex_spinunlock(&cip->cms_lock, spl);
		return;
	}


	cip->cms_flags |= CMS_WAIT_FOR_INITIAL_MEMBERSHIP;
	sv_wait(&cip->cms_membership_wait, PZERO, &cip->cms_lock, spl);
	ASSERT(cip->cms_age[cellid()] > 0);
}

/*
 * cms_deliver_membership:
 * Deliver the membership to outside world. Wake up threads waiting for 
 * membership to be delivered.
 */
static void
cms_deliver_membership()
{
	
	int	spl;
	cell_t	cell;
	char	memb_str[80];
	char	cell_str[20];
	cell_set_t	old_membership;

	memb_str[0] = '\0';
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(&cip->cms_membership, cell)) {
			sprintf(cell_str, "%d(%d) ", cell, cip->cms_age[cell]);
			strcat(memb_str, cell_str);
		}
	}

	cmn_err(CE_NOTE,"Membership delivered. Membership contains %s cells", 
					memb_str);

	spl = mp_mutex_spinlock(&cip->cms_lock);

	/*
	 * Deliver it by copying into exported global.
	 */
	set_assign(&old_membership, &cell_membership);
	set_assign(&cell_membership, &cip->cms_membership);

	cip->cms_flags |= CMS_MEMBERSHIP_STABLE;

	if (cip->cms_flags & CMS_WAIT_FOR_INITIAL_MEMBERSHIP) {
		cip->cms_flags &= ~CMS_WAIT_FOR_INITIAL_MEMBERSHIP;
		sv_broadcast(&cip->cms_membership_wait);
	}
	mp_mutex_spinunlock(&cip->cms_lock, spl);
	cms_cell_cleanup(&old_membership);

}

void
cms_notify_cell(cell_t cell, boolean_t up)
{
	if (up) 
		set_add_member(&cip->cms_potential_cell_set,  cell);
	else
		set_del_member(&cip->cms_potential_cell_set,  cell);
	/*
	 * If all cells are up wake up the daemon.
	 */
	if ((set_member_count(&cip->cms_potential_cell_set) 
						== cip->cms_num_cells) &&
		(cms_get_state() == CMS_NASCENT))
		cms_wake_daemon(CMS_ALL_CELLS_UP, B_FALSE);
}

/*
 * cms_cell_cleanup:
 * Clean up from cells not in the membership but were there in the
 * old membership. 
 * If its a clean shutdown then nothing needs to be done.
 * If the previous  membership is null check the potential cell set.
 * This happens at init time. 
 * If a cell is in the old membership but not in the new membership that
 * cell has failed and we have to recover from that cell's failure.
 * If a cell is in the old membership but not in the new one and that
 * cell is leaving then we just need to shutdown the message subsystem
 * and stop the heart beat.
 * If there was no prior membership then just shutdown the messaging and
 * the heart beat.
 * Unfreeze channels to cells which are in the new membership.
 */

static void
cms_cell_cleanup(cell_set_t *old_membership)
{
	cell_t	cell;
	service_t	svc;
	cell_set_t	recover_set;
	cell_set_t	excluded_set;

	set_init(&recover_set);
	set_complement(&excluded_set, &cell_membership);
	if (!set_is_empty(old_membership))
		set_intersect(&excluded_set, old_membership);
	else
		set_intersect(&excluded_set, &cip->cms_potential_cell_set);
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (set_is_member(&excluded_set, cell)) {

			SERVICE_MAKE(svc, cell, SVC_CMSID);
			/*
			printf("sending disconnect message. Not necessary\n");
			msgerr = invk_cms_mesg_disconnect(svc, cellid());
			*/
			if (set_is_member(old_membership, cell)) {
				if (cell == cip->cms_leave_cell)
					cell_disconnect(cell);
				else
					set_add_member(&recover_set, cell);
			} else {
				cell_disconnect(cell);
			}
		}

		/*
	 	 * Unfreeze channels for new members.
		 */
		if (set_is_member(&cell_membership, cell))
			mesg_channel_set_state(cell, MESG_CHANNEL_MAIN,
							MESG_CHAN_READY);
	}

	if (!set_is_empty(&recover_set)) {
		if (cms_elect_leader(&cell_membership) == cellid())
			set_assign(&cip->cms_recover_done_set, 
						&cell_membership);
		crs_initiate_recovery(recover_set);
	}
}

/*
 * cms_mark_recovery_complete:
 * Signal end of recovery. If not leader send a message to the leader
 * indicating that recovery is done. If leader wait until everybody in the
 * membership has cleaned up and wake up daemon to initiate any new
 * memberships.
 */ 
void
cms_mark_recovery_complete()
{
	int		spl;
	service_t	svc;
	cell_t		leader;


	leader = cms_elect_leader(&cell_membership);
	if (leader != cellid()) {
		/* REFERENCED */
		int	msgerr;

		SERVICE_MAKE(svc, leader, SVC_CMSID);
		msgerr = invk_cms_recovery_complete(svc, cellid());
		ASSERT(!msgerr);
	} else {
		spl = mutex_spinlock(&cip->cms_lock);
		set_del_member(&cip->cms_recover_done_set, cellid());
		if (set_is_empty(&cip->cms_recover_done_set))
			cms_wake_daemon(CMS_RECOVERY_COMPLETE, B_TRUE);
		mutex_spinunlock(&cip->cms_lock, spl);
	}
}


/*
 * cms_recovery_complete:
 * Returns if the recovery set is empty (recovery is complete.
 */
static boolean_t
cms_recovery_complete()
{
	cms_trace(CMS_TR_RECOVERY_COMP, 
			set_is_empty(&cip->cms_recover_done_set), 0, 0);
	return (set_is_empty(&cip->cms_recover_done_set));
}

/* 
 * I_cms_recovery_complete:
 * Informs the leader that recovery is complete.
 */
void
I_cms_recovery_complete(cell_t src_cell)
{
	int	spl;

	spl = mutex_spinlock(&cip->cms_lock);
	set_del_member(&cip->cms_recover_done_set, src_cell);	
	cms_trace(CMS_TR_REC_COMP_MSG, src_cell, cip->cms_recover_done_set, 0); 
	if (set_is_empty(&cip->cms_recover_done_set)) {
		cms_wake_daemon(CMS_RECOVERY_COMPLETE, B_TRUE);
	}
	mutex_spinunlock(&cip->cms_lock, spl);
}

#else
static void
cms_deliver_membership(void)
{
	cell_t	cell;
	char	memb_str[80], cell_str[20];

	memb_str[0] = '\0';
	for (cell = 0; cell < MAX_CELLS;  cell++) {
		if (set_is_member(&cip->cms_membership, cell)) {
			sprintf(cell_str, "%d(%d) ", cell, cip->cms_age[cell]);
			strcat(memb_str, cell_str);
		}
	}

	printf("===========================================================\n");
	cmn_err(CE_NOTE,"Membership delivered. Membership contains %s cells\n", 
					memb_str);
}

/* ARGSUSED */
static void
cms_cell_cleanup(cell_set_t *old_membership)
{
}

static boolean_t
cms_recovery_complete(void)
{
	return B_TRUE;
}

void
cms_notify_cell(cell_t cell, boolean_t up)
{
	if (up) 
		set_add_member(&cip->cms_potential_cell_set,  cell);
	else
		set_del_member(&cip->cms_potential_cell_set,  cell);
}
#endif
