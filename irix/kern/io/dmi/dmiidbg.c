/**************************************************************************
 *									  *
 *	 	Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/idbgentry.h>

#include "dmi_private.h"


static void	idbg_dmi_equeue (dm_eventq_t *);
static void	idbg_dmi_event (dm_tokevent_t *);
static void	idbg_dmi_help (void);
static void	idbg_dmi_regs(__psint_t);
static void	idbg_dmi_sessions(__psint_t);

#define	VD	(void (*)())

static struct xif {
	char	*name;
	void	(*func)();
	char	*help;
} dmiidbg_funcs[] = {
    "dmieq",	VD idbg_dmi_equeue,	"Print DMI messages in an event queue",
    "dmiev",	VD idbg_dmi_event,	"Print DMI event message",
    "dmihelp",	VD idbg_dmi_help,	"Print help for idbg-dmi",
    "dmiregs",	VD idbg_dmi_regs,	"Print DMI registration lists",
    "dmisess",	VD idbg_dmi_sessions,	"Print active DMI sessions",
    0,		0,	0
};

/*
 * Initialization routine.
 */
void
dmiidbginit(void)
{
	struct xif	*p;

	for (p = dmiidbg_funcs; p->name; p++)
		idbg_addfunc(p->name, p->func);
}

int
dmiidbgunload(void)
{
	struct xif	*p;

	for (p = dmiidbg_funcs; p->name; p++)
		idbg_delfunc(p->func);
	return(0);
}


static	struct {
        char            *name;		/* name of event */
        dm_eventtype_t  value;		/* value of event */
} ev_names[] = {
	{ "DM_EVENT_CANCEL",		DM_EVENT_CANCEL		},
	{ "DM_EVENT_MOUNT",		DM_EVENT_MOUNT		},
	{ "DM_EVENT_PREUNMOUNT",	DM_EVENT_PREUNMOUNT	},
	{ "DM_EVENT_UNMOUNT",		DM_EVENT_UNMOUNT	},
	{ "DM_EVENT_DEBUT",		DM_EVENT_DEBUT		},
	{ "DM_EVENT_CREATE",		DM_EVENT_CREATE		},
	{ "DM_EVENT_CLOSE",		DM_EVENT_CLOSE		},
	{ "DM_EVENT_POSTCREATE",	DM_EVENT_POSTCREATE	},
	{ "DM_EVENT_REMOVE",		DM_EVENT_REMOVE		},
	{ "DM_EVENT_POSTREMOVE",	DM_EVENT_POSTREMOVE	},
	{ "DM_EVENT_RENAME",		DM_EVENT_RENAME		},
	{ "DM_EVENT_POSTRENAME",	DM_EVENT_POSTRENAME	},
	{ "DM_EVENT_LINK",		DM_EVENT_LINK		},
	{ "DM_EVENT_POSTLINK",		DM_EVENT_POSTLINK	},
	{ "DM_EVENT_SYMLINK",		DM_EVENT_SYMLINK	},
	{ "DM_EVENT_POSTSYMLINK",	DM_EVENT_POSTSYMLINK	},
	{ "DM_EVENT_READ",		DM_EVENT_READ		},
	{ "DM_EVENT_WRITE",		DM_EVENT_WRITE		},
	{ "DM_EVENT_TRUNCATE",		DM_EVENT_TRUNCATE	},
	{ "DM_EVENT_ATTRIBUTE",		DM_EVENT_ATTRIBUTE	},
	{ "DM_EVENT_DESTROY",		DM_EVENT_DESTROY	},
	{ "DM_EVENT_NOSPACE",		DM_EVENT_NOSPACE	},
	{ "DM_EVENT_USER",		DM_EVENT_USER		}
};

static	int	ev_namecnt = sizeof(ev_names) / sizeof(ev_names[0]);

#ifdef	NOT_CURRENTLY_NEEDED

static dm_eventtype_t
ev_name_to_value(
	char		*name)
{
	int		i;

	for (i = 0; i < ev_namecnt; i++) {
		if (!strcmp(name, ev_names[i].name)) 
			return(ev_names[i].value);
	}
	return(DM_EVENT_INVALID);
}
#endif	/* NOT_CURRENTLY_NEEDED */

static char *
ev_value_to_name(
	dm_eventtype_t	event)
{
	int		i;

	for (i = 0; i < ev_namecnt; i++) {
		if (event == ev_names[i].value)
			return(ev_names[i].name);
	}
	return(NULL);
}


static char *
right_to_name(
	dm_right_t	right)
{
	switch (right) {
	case DM_RIGHT_NULL:
		return("DM_RIGHT_NULL");
	case DM_RIGHT_SHARED:
		return("DM_RIGHT_SHARED");
	case DM_RIGHT_EXCL:
		return("DM_RIGHT_EXCL");
	default:
		return("(invalid right)");
	}
}


static char *
state_to_name(
	dm_fsstate_t	state)
{
	switch (state) {
	case DM_STATE_MOUNTING:
		return("DM_STATE_MOUNTING");
	case DM_STATE_MOUNTED:
		return("DM_STATE_MOUNTED");
	case DM_STATE_UNMOUNTING:
		return("DM_STATE_UNMOUNTING");
	case DM_STATE_UNMOUNTED:
		return("DM_STATE_UNMOUNTED");
	default:
		return("(invalid state)");
	}
}



static void
hex_to_ascii(
	void		*inp,
	char		*outp,
	int		len)
{
        u_char          *ip = (u_char *)inp;
	int		i;

	for (i = 0; i < len; i++) {
		*outp++ = "0123456789abcdef"[ip[i] >> 4];
		*outp++ = "0123456789abcdef"[ip[i] & 0xf];
	}
	*outp = '\0';
}

/*
 *  Print all of the messages in a queue.
 */
static void
idbg_dmi_equeue (dm_eventq_t * qp)
{
	int		i = 0;
	dm_tokevent_t	*tevp;

	for (tevp = qp->eq_head; tevp; tevp = tevp->te_next, i++)
		idbg_dmi_event (tevp);
	if (qp->eq_count != i)
		qprintf("Warning: eq_count = %d, found %d\n",
			qp->eq_count, i);
}

/*
 *  Print an event message.
 */
static void
idbg_dmi_event(
	dm_tokevent_t	*tevp)
{
	dm_tokdata_t	*tdp;
	char		buffer[100];

	qprintf("Event 0x%x\n", tevp);
	qprintf("	te_next		0x%x\n", tevp->te_next);
	qprintf("	te_lock		0x%x\n", tevp->te_lock);
	qprintf("	te_evt_queue	0x%x\n", tevp->te_evt_queue);
	qprintf("	te_app_queue	0x%x\n", tevp->te_app_queue);
	qprintf("	te_evt_ref	0x%x\n", tevp->te_evt_ref);
	qprintf("	te_app_ref	0x%x\n", tevp->te_app_ref);
	qprintf("	te_app_slp	0x%x\n", tevp->te_app_slp);
	qprintf("	te_reply	%d\n",   tevp->te_reply);
	qprintf("	te_flags	0x%x\n", tevp->te_flags);
	qprintf("	te_allocsize	%d\n",   tevp->te_allocsize);
	qprintf("	te_tdp		0x%x\n", tevp->te_tdp);
	qprintf("	te_msg.ev_token	%d\n",   tevp->te_msg.ev_token);
	qprintf("	te_msg.ev_type	%s\n",
		ev_value_to_name(tevp->te_msg.ev_type));

	for (tdp = tevp->te_tdp; tdp; tdp = tdp->td_next) {
		qprintf("	Handle 0x%x\n", tdp);
		if (tdp->td_type == DM_TDT_VFS) {
			hex_to_ascii(&tdp->td_handle, buffer, FSHSIZE);
		} else {
			hex_to_ascii(&tdp->td_handle, buffer,
				HSIZE(tdp->td_handle));
		}
		qprintf("		td_next       0x%x\n", tdp->td_next);
		qprintf("		td_tevp       0x%x\n", tdp->td_tevp);
		qprintf("		td_app_ref    %d\n",   tdp->td_app_ref);
		qprintf("		td_orig_right %s\n",
			right_to_name(tdp->td_orig_right));
		qprintf("		td_right      %s\n",
			right_to_name(tdp->td_right));
		qprintf("		td_flags      0x%x\n", tdp->td_flags);
		qprintf("		td_type       0x%x\n", tdp->td_type);
		qprintf("		td_bdp        0x%x\n", tdp->td_bdp);
		qprintf("		td_vcount     %d\n",   tdp->td_vcount);
		qprintf("		td_handle     %s\n",   buffer);
	}
}


/*
 * Print out the help messages for these functions.
 */
static void
idbg_dmi_help (void)
{
	struct xif	*p;

	for (p = dmiidbg_funcs; p->name; p++)
		qprintf("%s %s\n", padstr(p->name, 16), p->help);
}

static void
print_one_session(
	dm_session_t	*sessp)
{
	qprintf("Session #%d, address 0x%x (\"%s\"):\n",
		sessp->sn_sessid, sessp, sessp->sn_info);
	qprintf("	sn_flags      0x%x\n", sessp->sn_flags);
	qprintf("	sn_qlock      0x%x\n", sessp->sn_qlock);
	qprintf("	sn_readerq    0x%x\n", sessp->sn_readerq);
	qprintf("	sn_readercnt  %d\n", sessp->sn_readercnt);
	qprintf("	sn_writerq    0x%x\n", sessp->sn_writerq);
	qprintf("	sn_writercnt  %d\n", sessp->sn_writercnt);
	qprintf("	sn_newq       0x%x; %d items\n",
		&sessp->sn_newq, sessp->sn_newq.eq_count);
	qprintf("	sn_delq       0x%x; %d items\n",
		&sessp->sn_delq, sessp->sn_delq.eq_count);
}

/*
 *  Print a list of active sessions.  If called with no parameters, "dmisess"
 *  will print every active session that exists.  "dmisess 0x8c5d23a4" will
 *  print the dm_session_t structure at address 0x8c5d23a4, and "dmisess 3"
 *  will print the dm_session_t structure whose dm_sessid_t is 3.
 */
static void
idbg_dmi_sessions (__psint_t x)
{
	dm_session_t	*sessp;

	if (x == -1L) {
		/* go through all of the structures and print them. */
		for (sessp = dm_sessions; sessp; sessp = sessp->sn_next) {
			print_one_session(sessp);
		}
	} else if (x < 0) {
		/* use as address */
		print_one_session((dm_session_t *)x);
	} else {
		/* go through list looking for match on sid. */
		for (sessp = dm_sessions; sessp; sessp = sessp->sn_next) {
			if (sessp->sn_sessid == (dm_sessid_t )x) {
				print_one_session(sessp);
				break;
			}
		}
	}
}

static void
print_one_reglist(
	dm_fsreg_t	*fsrp)
{
	char		buffer[100];
	int		i;

	qprintf("Registrations for dm_fsreg_t 0x%x:\n", fsrp);

	hex_to_ascii(&fsrp->fr_fsid, buffer, FSHSIZE);

	qprintf("	fr_next     0x%x\n", fsrp->fr_next);
	qprintf("	fr_vfsp     0x%x\n", fsrp->fr_vfsp);
	qprintf("	fr_tevp     0x%x\n", fsrp->fr_tevp);
	qprintf("	fr_fsid     %s\n",   buffer);
	qprintf("	fr_msg      0x%x\n", fsrp->fr_msg);
	qprintf("	fr_msgsize  %d\n",   fsrp->fr_msgsize);
	qprintf("	fr_state    %s\n",   state_to_name(fsrp->fr_state));
	qprintf("	fr_dispq    0x%x\n", fsrp->fr_dispq);
	qprintf("	fr_dispcnt  %d\n",   fsrp->fr_dispcnt);
	qprintf("	fr_queue    0x%x\n", fsrp->fr_queue);
	qprintf("	fr_lock     0x%x\n", fsrp->fr_lock);
	qprintf("	fr_hdlcnt   %d\n",   fsrp->fr_hdlcnt);
	qprintf("	fr_vfscnt   %d\n",   fsrp->fr_vfscnt);
	qprintf("	fr_unmount  %d\n",   fsrp->fr_unmount);

	if (fsrp->fr_rattr.an_chars[0] != '\0') {
		qprintf("	fr_rattr    %s\n", fsrp->fr_rattr.an_chars);
	}
	qprintf("	Session		Event\n");
	for (i = 0; i < DM_EVENT_MAX; i++) {
		if (fsrp->fr_sessp[i]) {
			qprintf("	0x%x	%s\n", fsrp->fr_sessp[i],
				ev_value_to_name((dm_eventtype_t)i));
		}
	}
}

/*
 *  Print the file system registration lists.  Shows for each file system
 *  the events being monitored, and the session receiving them.  If no
 *  parameters are provided, "dmiregs" prints the registration lists for all
 *  filesystems.  If an address is provided, "dmiregs" will print the
 *  dm_fsret_t structure at that address.
 */
static void
idbg_dmi_regs(__psint_t x)
{
	dm_fsreg_t	*fsrp;

	if (x == -1L) {
		/* go through all of the structures and print them. */
		for (fsrp = dm_registers; fsrp; fsrp = fsrp->fr_next) {
			print_one_reglist(fsrp);
		}
	} else {
		/* use as address */
		print_one_reglist((dm_fsreg_t *)x);
	}
}
