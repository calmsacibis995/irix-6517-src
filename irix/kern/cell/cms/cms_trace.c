#if DEBUG
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

#define	CMS_TRACE_ENTRIES	1024
/*
 * The kernel trace buffer entry is cast to this structure before 
 * retreiving the values to be printed.
 */
typedef struct cms_trace_s {
	long		cms_opcode;
	long		cms_state;
	void		*cms_arg1;
	void		*cms_arg2;
	void 		*cms_arg3;
} cms_trace_t;

static void 	cms_print_entry(cms_trace_t *);

#ifdef	_KERNEL
static void 	idbg_cms_trace(__psunsigned_t);
static void 	idbg_cms_info(void);
ktrace_t	*cms_trace;

void
cms_trace_init(void)
{
	cms_trace = ktrace_alloc(CMS_TRACE_ENTRIES, 0);	
	idbg_addfunc("cms_trace", idbg_cms_trace);
	idbg_addfunc("cms_info", idbg_cms_info);
}

#else 

cms_trace_t	cms_trace_buf[CMS_TRACE_ENTRIES];
int		cms_trace_print_on; 
int		cms_trace_index;
void
cms_trace_enter(int opcode, void *arg1, void *arg2, void *arg3)
{
	cms_trace_t	*p;

	p = &cms_trace_buf[cms_trace_index];
	cms_trace_index++;
	if (cms_trace_index == CMS_TRACE_ENTRIES)
		cms_trace_index = 0;

	p->cms_opcode = opcode;
	p->cms_arg1 = arg1;
	p->cms_state = cip->cms_state;
	p->cms_arg2 = arg2;
	p->cms_arg3 = arg3;
	if (cms_trace_print_on)
		cms_print_entry(p);
}
#endif

char *cms_mesg_type_str[] = {
	"Req for proposal",
	"Proposal",
	"Membership",
	"Accept",
	"Membership query",
	"Membership reply",
	"Membership leave"
};

char *cms_state_str[] = {
        "nascent ",
        "follower",
        "leader  ",
        "stable  ",
        "dead    "
};

char	*cms_fail_str[] = {
	"No failure",
	"Message timeout",
	"Failure detected",
	"Cell died",
	"New member join",
	"All cells up"
};

static void
cms_print_entry(cms_trace_t *p)
{
	if (p->cms_opcode == 0)
		return;

	qprintf("CELL %d %s:", cellid(), (p->cms_state) <= CMS_DEAD ?  
			cms_state_str[p->cms_state] : "Invalid state");
	switch(p->cms_opcode) {
	case	CMS_TR_ACCEPT:
		qprintf("Sending accept leader %d receive set %x send set %x\n",
				p->cms_arg1, p->cms_arg2, p->cms_arg3);
		break;

	case	CMS_TR_PROPOSAL:
		qprintf("Sending proposal send %x new member set %x\n",
				p->cms_arg1, p->cms_arg2);	
		break;

	case	CMS_TR_REQ_PROPOSAL:
		qprintf("Send req for proposal send set %x new member set %x\n",
				p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_MEMB_QUERY:
		qprintf("Sending membership query send set %x to cell %d\n", 
				p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_MEMB_REPLY:
		qprintf("Sending membership reply src cell %x membership %x\n",
				p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_RETRV_MESG:
		qprintf("Retrieving pushed back message from src cell %d\n", 
			p->cms_arg1);
		break;

	case	CMS_TR_RECV_MESG_INV_CELL:
		qprintf("Received message from invalid cell %d\n", p->cms_arg1);
		break;

	case	CMS_TR_RECV_MESG_INV_SEQ_NO:
		qprintf("Dropping message with lower seq no %d "
			"expected %d src cell %d\n", 
			p->cms_arg1, p->cms_arg2, p->cms_arg3);
		break;

	case	CMS_TR_RECV_MESG:
		qprintf("Receive message from %d seq no %d type %s\n",
			p->cms_arg1, p->cms_arg2, 
			((long)p->cms_arg3 <= CMS_MESG_MEMBERSHIP_REPLY) ?
			cms_mesg_type_str[(long)p->cms_arg3] : 
			"Invalid message");
		break;

	case	CMS_TR_FRWD_DIR:
		qprintf("Forwarding message directly to %d from %d\n",
				p->cms_arg2, p->cms_arg1);
		break;

	case	CMS_TR_FRWD_INDIR:
		qprintf("Forwarding message indirectly from %d to %d via %d\n",
			p->cms_arg1, p->cms_arg2, p->cms_arg3);
		break;
		
	case	CMS_TR_MEMB_MESG:
		qprintf("Sending membership message to %d membership %x\n",
			p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_BROAD_MESG:
		qprintf("Sending broadcast message to %d\n", p->cms_arg1);
		break;

	case	CMS_TR_PUSHB_MESG:
		qprintf("Pushing back message from %d\n",p->cms_arg1);
		break;

	case	CMS_TR_FAILURE:
		qprintf("Message not received due to %s "
			"send set %x recev set %x\n",
			cms_fail_str[(long)p->cms_arg1], 
			p->cms_arg2, p->cms_arg3);
		break;

	case	CMS_TR_MEMB_RECV_SET:
		qprintf("Membership compute: Receive set of %d is %x\n",
			p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_MEMB_FAIL:
		qprintf("Membership compute failed \n");
		break;

	case	CMS_TR_MEMB_SUCC:
		qprintf("Membership compute successful %x\n", p->cms_arg1);
		break;

	case	CMS_TR_PROPOSAL_IGNORE:
		qprintf("Ignore proposal src cell %d age of src cell %d "
			"Leader's age %d\n",p->cms_arg1, p->cms_arg2, 
							p->cms_arg3);
		break;

	case	CMS_TR_MEMB_MIN:
		qprintf("Minority membership no quorum %x\n", p->cms_arg1);
		break;

	case	CMS_TR_DELAY_FSTOP:
		qprintf("Split membership, delaying fail stop %d\n", 
								p->cms_arg1);
		break;

	case	CMS_TR_NEW_STATE:
		qprintf("New state change\n");
		break;

	case	CMS_TR_CONN_SET:
		qprintf("Get conn set send set %x recv set %x memb set %x\n",
			p->cms_arg1, p->cms_arg2, p->cms_arg3);
		break;

	case	CMS_TR_RESET_CELLS:
		qprintf("Resetting cell %d old membership %x  new membership %x\n",
			p->cms_arg1, p->cms_arg2, p->cms_arg3);
		break;

	case	CMS_TR_AGE:
		qprintf("Age of cell %d is %d\n", p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_RECOVERY_COMP:
		qprintf("Recovery is %s\n", p->cms_arg1 ? "complete" :
						"not complete");
		break;

	case	CMS_TR_REC_COMP_MSG:
		qprintf("Recovery complete message received from %d "
			"recover set 0x%x\n", p->cms_arg1, p->cms_arg2);
		break;

	case	CMS_TR_TIMEOUT:
		qprintf("Timeout pending %d lbolt 0x%x starttick 0x%x\n",
			p->cms_arg1, p->cms_arg2, p->cms_arg3);
		break;

	default  :
		qprintf("Invalid trace entry %d\n", p->cms_opcode);
		break;
	}
}

#if defined(_KERNEL)
void
idbg_cms_trace(__psunsigned_t count)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	int		nentries;
	int		skip_entries;

	if (cms_trace == NULL) {
		qprintf("Cms trace buffer not initialized\n");
		return;
	}

	nentries = ktrace_nentries(cms_trace);
	if (count == -1) {
		count = nentries;
	}
        if ((count <= 0) || (count > nentries)) {
                qprintf("Invalid count.  There are %d entries.\n", nentries);
                return;
        }
	ktep = ktrace_first(cms_trace, &kts);
	if (count != nentries) {
		/*
                 * Skip the total minus the number to look at minus one
                 * for the entry returned by ktrace_first().
                 */
                skip_entries = nentries - count - 1;
                ktep = ktrace_skip(cms_trace, skip_entries, &kts);
                if (ktep == NULL) {
                        qprintf("Skipped them all\n");
                        return;
                }
	}

	while (ktep != NULL) {
		cms_print_entry((cms_trace_t *)ktep);
		ktep = ktrace_next(cms_trace, &kts);
	}
}

char    *tab_cmsflags[] = {
	"pushed message      ",	/* 0x000001 */
	"message buf full    ",	/* 0x000002 */
	"Daemon waiting      ",	/* 0x000004 */
	"Timeout pending     ",	/* 0x000008 */
	"Wait for membership ",	/* 0x000010 */
	"Membership stable   ",	/* 0x000020 */
	0
};

void
idbg_cms_info(void)
{
	extern	void _printflags(unsigned int, char **, char *);
	cell_t	i;

	qprintf("Membership info. for cell %d\n", cellid());
	qprintf("Timeouts Nascent %d follower %d leader %d tie %d\n",
		cip->cms_nascent_timeout, cip->cms_follower_timeout,
		cip->cms_leader_timeout, cip->cms_tie_timeout);
	qprintf("Potential Connectivity set 0x%x send set 0x%x recv set 0x%x\n",
			cip->cms_potential_cell_set,
			cip->cms_local_send_set,
			cip->cms_local_recv_set);
	qprintf("Recover set 0x%x\n", cip->cms_recover_done_set);
	_printflags((unsigned)cip->cms_flags, tab_cmsflags, "cms_flags");
	qprintf("\nNum cells %d  State %s\n", cip->cms_num_cells, 
				(cip->cms_state <= CMS_DEAD) ? 
			cms_state_str[cip->cms_state] : "Invalid state");
	qprintf("Leader %d Current membership 0x%x new members 0x%x\n",
		cip->cms_leader, cip->cms_membership, cip->cms_new_member_set);
	for (i = 0; i < MAX_CELLS; i++) {
		if (set_is_member(&set_universe, i)) {
			qprintf("Age of cell %d is %d\n", i, cip->cms_age[i]);
			qprintf("Receive set of cell %d is %x\n", 
					i, cip->cms_recv_set[i]);
		}
	}
}
#endif
#endif
