/*
 * Copyright 1991 by Silicon Graphics, Inc.
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *
 * ncvt.c: nlm rpc arg conversion routines.
 */

#include <stdio.h>
#include <sys/fcntl.h>
#include "prot_lock.h"

extern int 		debug;
extern SVCXPRT 		*nlm_transp;

#define	off2base(offset)  ((((int)offset) < 0) ? 0x7fffffff : offset)
#define	base2off(base)    ((((int)base) < 0)   ? 0x7fffffff : base)


int
reclocktonlm_lockargs(from, to)
	struct reclock *from;
	nlm_lockargs   *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	if (from->block)
		to->block = TRUE;
	else
		to->block = FALSE;
	if (from->lck.lox.type == F_WRLCK)
		to->exclusive = TRUE;
	else
		to->exclusive = FALSE;
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	to->alock.svid = from->alock.lox.pid;
	to->alock.l_offset = base2off(from->alock.lox.base);
	to->alock.l_len = from->alock.lox.length;
	to->reclaim = from->reclaim;
	to->state = from->state;

	return(0);
}

int
reclocktonlm_cancargs(from, to)
	struct reclock *from;
	nlm_cancargs   *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	if (from->block)
		to->block = TRUE;
	else
		to->block = FALSE;
	if (from->lck.lox.type == F_WRLCK)
		to->exclusive = TRUE;
	else
		to->exclusive = FALSE;
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	to->alock.svid = from->alock.lox.pid;
	to->alock.l_offset = base2off(from->alock.lox.base);
	to->alock.l_len = from->alock.lox.length;

	return(0);
}

int
reclocktonlm_unlockargs(from, to)
	struct reclock *from;
	nlm_unlockargs *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;

	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	to->alock.svid = from->alock.lox.pid;
	to->alock.l_offset = base2off(from->alock.lox.base);
	to->alock.l_len = from->alock.lox.length;

	return(0);
}

int
reclocktonlm_testargs(from, to)
	struct reclock *from;
	nlm_testargs   *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	if (from->lck.lox.type == F_WRLCK)
		to->exclusive = TRUE;
	else
		to->exclusive = FALSE;
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	to->alock.svid = from->alock.lox.pid;
	to->alock.l_offset = base2off(from->alock.lox.base);
	to->alock.l_len = from->alock.lox.length;

	return(0);
}

int
nlm_lockargstoreclock(from, to)
	nlm_lockargs   *from;
	struct reclock *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	if (from->block)
		to->block = TRUE;
	else
		to->block = FALSE;
	if (from->exclusive) {
		to->exclusive = TRUE;
		to->alock.lox.type = F_WRLCK;
	} else {
		to->exclusive = FALSE;
		to->alock.lox.type = F_RDLCK;
	}
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	to->alock.lox.base = off2base(from->alock.l_offset);
	to->alock.lox.length = from->alock.l_len;
	to->alock.lox.pid = from->alock.svid;
	to->alock.lox.ipaddr = nlm_transp->xp_raddr.sin_addr.s_addr;
	to->reclaim = from->reclaim;

	return(0);
}

int
nlm_unlockargstoreclock(from, to)
	nlm_unlockargs *from;
	struct reclock *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len ) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	to->alock.lox.base = off2base(from->alock.l_offset);
	to->alock.lox.length = from->alock.l_len;
	to->alock.lox.pid = from->alock.svid;
	to->alock.lox.ipaddr = nlm_transp->xp_raddr.sin_addr.s_addr;

	return(0);
}

int
nlm_cancargstoreclock(from, to)
	nlm_cancargs   *from;
	struct reclock *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	to->block = from->block;
	if (from->exclusive) {
                to->exclusive = TRUE;
                to->alock.lox.type = F_WRLCK;
        } else {
                to->exclusive = FALSE;
                to->alock.lox.type = F_RDLCK;
        }
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;

	to->alock.lox.base = off2base(from->alock.l_offset);
	to->alock.lox.length = from->alock.l_len;
	to->alock.lox.pid = from->alock.svid;
	to->alock.lox.ipaddr = nlm_transp->xp_raddr.sin_addr.s_addr;

	return(0);
}

int
nlm_testargstoreclock(from, to)
	nlm_testargs   *from;
	struct reclock *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	if (from->exclusive) {
                to->exclusive = TRUE;
                to->alock.lox.type = F_WRLCK;
        } else {
                to->exclusive = FALSE;
                to->alock.lox.type = F_RDLCK;
        }
	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) == NULL)
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(-1);
	} else
		to->alock.oh.n_len = 0;
	
	to->alock.lox.base = off2base(from->alock.l_offset);
	to->alock.lox.length = from->alock.l_len;
	to->alock.lox.pid = from->alock.svid;
	to->alock.lox.ipaddr = nlm_transp->xp_raddr.sin_addr.s_addr;

	return(0);
}

int
nlm_testrestoremote_result(from, to)
	nlm_testres   *from;
	remote_result *to;
{
	if (debug)
		printf("enter nlm_testrestoremote_result..\n");

	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	to->stat.stat = from->stat.stat;
	if (from->stat.nlm_testrply_u.holder.exclusive)
		to->stat.nlm_testrply_u.holder.exclusive = TRUE;
	else
		to->stat.nlm_testrply_u.holder.exclusive = FALSE;
	to->stat.nlm_testrply_u.holder.svid =
		from->stat.nlm_testrply_u.holder.svid;
	if (obj_copy(&to->stat.nlm_testrply_u.holder.oh,
			&from->stat.nlm_testrply_u.holder.oh) == -1)
		return(-1);
	to->stat.nlm_testrply_u.holder.l_offset =
		from->stat.nlm_testrply_u.holder.l_offset;
	to->stat.nlm_testrply_u.holder.l_len =
		from->stat.nlm_testrply_u.holder.l_len;

	return(0);
}

int
nlm_restoremote_result(from, to)
	nlm_res *from;
	remote_result *to;
{
	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(-1);
	} else
		to->cookie.n_bytes= NULL;
	to->stat.stat = from->stat.stat;

	return(0);
}

/*
 * This procedure is ostensibly for mapping from VERS to VERSX.
 * In practice it _probably_ only applies to PC-NFS clients. However
 * the protocol specification specifically allows the length of
 * a lock to be an unsigned 32-bit integer, so it is possible to
 * imagine other cases (especially non-Unix).
 *
 * Even this mapping is inadequate. A better version would simply
 * test the sign bit, and treat ALL "negative" values as 0. We
 * may still get there in this bugfix.
 *
 * Geoff
 */
void
nlm_versx_conversion(procedure, from)
	int	procedure;
	char   *from;
{
	switch (procedure) {
	case NLM_LOCK:
	case NLM_NM_LOCK:
		if(((struct nlm_lockargs *)from)->alock.l_len == 0xffffffff) 
			((struct nlm_lockargs *)from)->alock.l_len= 0;
		return;
	case NLM_UNLOCK:
		if(((struct nlm_unlockargs *)from)->alock.l_len == 0xffffffff) 
			((struct nlm_unlockargs *)from)->alock.l_len= 0;
		return;
	case NLM_CANCEL:
		if(((struct nlm_cancargs *)from)->alock.l_len == 0xffffffff) 
			((struct nlm_cancargs *)from)->alock.l_len= 0;
		return;
	case NLM_TEST:
		if(((struct nlm_testargs *)from)->alock.l_len == 0xffffffff) 
			((struct nlm_testargs *)from)->alock.l_len= 0;
		return;
	default:
		return;
	}

}
