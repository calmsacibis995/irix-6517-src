/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident $Id: mesg_xpc.c,v 1.1 1997/10/11 01:07:59 sp Exp $

#include <ksys/cell/mesg.h>
#include <ksys/cell/membership.h>
#include "mesg_private.h"

#include <sys/errno.h>

void
mesg_dup(
	idl_msg_t	*from,
	idl_msg_t	*to)
{
	int		nbytes;
	int		i;
	ssize_t		totalbytes;
	__uint32_t	nxpmbs;
	xpmb_t		*xpmb;
	xpmb_t		*xpmb_to;
	alenlist_t	alist;
	alenlist_t	alistbuf = NULL;
	xpc_t		rc;

	nbytes = sizeof(xpm_t) +
		 from->msg_xpm.xpm_cnt * sizeof(xpmb_t) +
		 from->msg_xpmb.xpmb_cnt;
	bcopy(from, to, nbytes);
	/*
	 * Now copy across any indirect data
	 */
	nxpmbs = from->msg_xpm.xpm_cnt - 1;
	if (nxpmbs == 0)
		return;

	alist = alenlist_create(0);
	totalbytes = 0;
	to->msg_xpm.xpm_cnt = 1;

	for (xpmb = &from->msg_xpmb + 1, xpmb_to = &to->msg_xpmb + 1;
	     xpmb != &from->msg_xpmb + from->msg_xpm.xpm_cnt;
	     xpmb++) {
		caddr_t	buf;

		nbytes = xpmb->xpmb_cnt;
		if (nbytes == 0) {
			*xpmb_to = *xpmb;
		} else {
			ASSERT(xpmb->xpmb_flags & XPMB_F_PTR);
			for (i = 0; xpmb[i].xpmb_flags & XPMB_F_BC; i++)
				nbytes += xpmb[i+1].xpmb_cnt;
			xpmb_to->xpmb_flags = XPMB_F_PTR;
			buf = kmem_alloc(nbytes, KM_SLEEP);
			xpmb_to->xpmb_ptr = (paddr_t)buf;
			xpmb_to->xpmb_cnt = nbytes;
			alistbuf = kvaddr_to_alenlist(alistbuf, buf, nbytes, 0);
			alenlist_concat(alistbuf, alist);
			totalbytes += nbytes;
		}
		xpmb_to++;
		to->msg_xpm.xpm_cnt++;
		while (xpmb->xpmb_flags & XPMB_F_BC)
			xpmb++;
	}
	alenlist_done(alistbuf);

	if (totalbytes) {
		rc = xpc_receive_xpmb(&from->msg_xpmb + 1, &nxpmbs, NULL, alist,
				&totalbytes);
		ASSERT(rc == xpcSuccess);
	}
	ASSERT(totalbytes == 0);
	alenlist_done(alist);
}

void
mesg_notify(
	xpc_t		reason,
	xpchan_t	xpchan,
	partid_t	partid,
	int		channid,
	void		*key)
{
	void		*mesg_copy;
	idl_msg_t	*idl_mesg = (idl_msg_t *)key;
	chaninfo_t	*chaninfo;
	mesg_channel_t	channel;
	int		channo;
	cell_t		cell;

	if (reason != xpcAsync)
		panic("mesg_notify: can't handle notification");

	mesg_copy = kmem_alloc(MESG_MAX_SIZE, KM_SLEEP);
	mesg_dup(idl_mesg, (idl_msg_t *)mesg_copy);

	/*
	 * Wait for initial membership to be delivered before
	 * accepting messages.
	 */
	channo = channid - XP_RPC_BASE;
	if (channo == MESG_CHANNEL_MAIN)
		cms_wait_for_initial_membership();

	cell = PARTID_TO_CELLID(partid);
	channel = cell_to_channel(cell, channo);
	if (channel == NULL) {
		cmn_err(CE_WARN, "Dropping message received from "
				 "Cell %d:not in membership\n",
					cell);
		return;
	}

	chaninfo = (chaninfo_t *)channel;
	ASSERT(chaninfo->ci_chanid == xpchan);
	xpc_done(chaninfo->ci_chanid, &idl_mesg->msg_xpm);
	idl_mesg = (idl_msg_t *)mesg_copy;

	mesg_demux(cell, channo, channel, idl_mesg);
}

int
mesg_xpc_connect(
	cell_t		cell,
	int		channelno,
	xpchan_t	*channel)
{
	int	err;

	err = xpc_connect(CELLID_TO_PARTID(cell),
			XP_RPC_BASE+channelno,
			MESG_MAX_SIZE, NBPP/MESG_MAX_SIZE, mesg_notify,
			XPC_CONNECT_DEFAULT, channel);

	if (err == xpcBusy)
		return(EBUSY);

	if (err != xpcSuccess) {
		cmn_err(CE_WARN,
			"mesg_connect: xpc_connect to cell %d returns %d\n",
				cell, err);
		return(ECONNREFUSED);
	}
	return(0);
}

/* ARGSUSED */
void
mesg_xpc_free(
	mesg_channel_t	channel,
	idl_msg_t	*msg)
{
	int	i;
	xpm_t	*xpm;
	xpmb_t	*xpmb;

	xpm = &msg->msg_xpm;
	xpmb = &msg->msg_xpmb;
	for (i = 1; i < xpm->xpm_cnt; i++)
		if (xpmb[i].xpmb_cnt != 0 && xpmb[i].xpmb_ptr != 0)
			panic("mesg_free: what's this data I've found?");
	kmem_free(msg, MESG_MAX_SIZE);
}
