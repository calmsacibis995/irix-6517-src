#ifndef	__MS_CMS_INFO_H
#define	__MS_CMS_INFO_H

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
 * Data structure maintained by the cell membership daemon 
 */
typedef struct cms_info_s {
	cell_set_t	cms_local_recv_set;	/* Cells in receive set     */
	cell_set_t	cms_local_send_set;	/* Cells in send set 	    */
	cell_set_t	cms_potential_cell_set;	/* Cells that have 	    */
						/* membership channels 	    */
	cell_set_t	cms_membership;		/* Cells that are in the    */
						/* membership 		    */
	cell_set_t	cms_accept_set;		/* Cells that sent accept   */
	cell_set_t	cms_new_member_set;	/* Cells that want to join  */
	cell_set_t	cms_recover_done_set;	/* Cells that have not      */
						/* finished recovery        */
	cms_state_t	cms_state;		/* Membership daemon state  */
	cell_set_t	cms_recv_set[MAX_CELLS];/* Receive set of other     */
						/* cells sent during accept */
	ageno_t		cms_age[MAX_CELLS];	/* Age of all cells  	    */
	cell_t		cms_leader;		/* Current leader 	    */
	cell_t		cms_leave_cell;		/* Cell that wants to leave */
	int		cms_num_cells; 		/* No. of cells that can    */
						/* form the membership      */ 
	int		cms_follower_timeout;	/* Timeout for follower     */
	int		cms_leader_timeout;	/* Timeout for leader       */
	int		cms_nascent_timeout;	/* Timeout in nascent state */
	int		cms_tie_timeout;	/* Timeout if tie	    */
	int		ep_fd;			/* socket file descriptor   */
	int		cms_flags;		/* Assorted flags 	    */
	cms_message_t	cms_pushed_message; 	/* Pushed msg 		    */
	cms_message_t	cms_message_buf; 	/* Buf to store recvd msg   */
	seq_no_t	cms_exp_seq_no[MAX_CELLS]/* Expected seq. no of the */
			[CMS_NUM_MESG_TYPES];   /* msgs sent by other cells */
	lock_t		cms_lock;		/* Control signal variables */
	sv_t		cms_mesg_wait;		/* Message thread wait      */
	sv_t		cms_daemon_wait;	/* Daemon wait 		    */
	sv_t		cms_membership_wait;	/* Membership wait	    */
	int		cms_wakeup_reason;	/* Reason for wakeup	    */
	toid_t		cms_toid;		/* Timeout id 		    */
						
} cms_info_t;

/*
 * Defines for cms_flags.
 */
#define	CMS_PUSHED_MESSAGE		0x1  /* A message has been pushed */
#define	CMS_BUF_FULL			0x2  /* Cms buffer is full */
#define	CMS_DAEMON_WAIT			0x4  /* Daemon is in long term wait */
#define	CMS_TIMEOUT_PENDING		0x8  /* Message timeout  is pending */
#define	CMS_WAIT_FOR_INITIAL_MEMBERSHIP	0x10 /* Wait for init. membership */
#define	CMS_MEMBERSHIP_STABLE		0x20 /* Stable membership */
#define CMS_MEMBERSHIP_EXISTS           0x40 /* There seems to be an */
					     /* existing membership */


#define	CMS_NASCENT_TIME	10	/* Wait 10 secs */

/*
 * Get and set expected sequence number.
 */
#define	cms_get_exp_seq_no(x, type)	(cip->cms_exp_seq_no[(x)][(type)])

#define	cms_update_exp_seq_no(cell, type, seq)\
		(cip->cms_exp_seq_no[(cell)][(type)] = (seq))


#define	cms_set_state(x)	(cip->cms_state = (x))
#define	cms_get_state()		(cip->cms_state)

extern	cms_info_t	*cip;

extern void cms_init(void);
extern void cms(void);
extern void cms_get_config_info(void);
extern int cms_get_connectivity_set(cell_set_t *, cell_t);
extern void cms_wake_daemon(int, int);
extern cell_t cms_elect_leader(cell_set_t *);

#endif /* __MS_CMS_INFO_H */
