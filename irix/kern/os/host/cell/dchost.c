/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: dchost.c,v 1.16 1997/10/02 17:16:36 sp Exp $"

/*
 * Client-side object management for virtual host.
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/idbgentry.h>
#include <sys/conf.h>
#include <sys/utsname.h>

#include <ksys/behavior.h>
#include <ksys/cell/tkm.h>
#include <ksys/vhost.h>
#include <ksys/cell/wp.h>

#include "dhost.h"
#include "vhost_private.h"
#include "invk_dshost_stubs.h"
#include "I_dchost_stubs.h"

#ifdef DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif

/*
 * Protos for client-side distributed layer.
 */
PRIVATE void	dchost_register(bhv_desc_t *, int);
PRIVATE void	dchost_deregister(bhv_desc_t *, int);
PRIVATE void	dchost_sethostid(bhv_desc_t *, int);
PRIVATE void	dchost_gethostid(bhv_desc_t *, int *);
PRIVATE void	dchost_sethostname(bhv_desc_t *, char *, size_t *);
PRIVATE void	dchost_gethostname(bhv_desc_t *, char *, size_t *);
PRIVATE void	dchost_setdomainname(bhv_desc_t *, char *, size_t);
PRIVATE void	dchost_getdomainname(bhv_desc_t *, char *, size_t *);
PRIVATE void	dchost_killall(bhv_desc_t *, int, pid_t, pid_t, pid_t);
PRIVATE void	dchost_reboot(bhv_desc_t *, int, char *);
PRIVATE void	dchost_sync(bhv_desc_t *, int);
PRIVATE int 	dchost_sysget(bhv_desc_t *, int, char *, int *, int,
			sgt_cookie_t *, sgt_info_t *);
PRIVATE int	dchost_systune(bhv_desc_t *, int, int, uint64_t);
PRIVATE void	dchost_credflush(bhv_desc_t *, cell_t);
PRIVATE void	dchost_credflush_wait(bhv_desc_t *, cell_t);
PRIVATE void	dchost_sysacct(bhv_desc_t *, int, cfs_handle_t *);
PRIVATE void	dchost_set_time(bhv_desc_t *, cell_t);
PRIVATE void	dchost_adj_time(bhv_desc_t *, long, long *);

vhost_ops_t dchost_ops = {
	BHV_IDENTITY_INIT_POSITION(VHOST_BHV_DC),
	dchost_register,
	dchost_deregister,
	dchost_sethostid,
	dchost_gethostid,
	dchost_sethostname,
	dchost_gethostname,
	dchost_setdomainname,
	dchost_getdomainname,
	dchost_killall,
	dchost_reboot,
	dchost_sync,
	dchost_sysget,
	dchost_systune,
	dchost_credflush,
	dchost_credflush_wait,
	dchost_sysacct,
	dchost_set_time,
	dchost_adj_time,
};


static void dchost_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
			       tk_set_t *);
static void dchost_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
			       tk_disp_t);

tkc_ifstate_t dchost_tclient_iface = {
		dchost_tcif_obtain,
		dchost_tcif_return
};

void
dchost_create(vhost_t *vhost)
{
	dchost_t	*dcp;

	/*
	 * We'll have a null object id until we obtain a token
	 * from the server for the first time. Since there's
	 * only one vhost per cell, we don't really need a handle
	 * at all, of course.
	 */
	dcp = (dchost_t *) kern_calloc(1, sizeof(dchost_t));
	HANDLE_MAKE(dcp->dch_handle, vhost_service(), NULL);
	tkc_create("dhost", dcp->dch_tclient, dcp, &dchost_tclient_iface,
		   VHOST_NTOKENS, TK_NULLSET, TK_NULLSET,
		   (void *)(__psint_t)cellid());

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&dcp->dch_bhv, dcp, vhost, &dchost_ops);
	BHV_WRITE_LOCK(&vhost->vh_bhvh);
	bhv_insert(&vhost->vh_bhvh, &dcp->dch_bhv);
	BHV_WRITE_UNLOCK(&vhost->vh_bhvh);

	/*
	 * Join the system at large.
	 */
	tkc_acquire1(dcp->dch_tclient, VHOST_MEMBER_TOKEN);

	HOST_TRACE4("dchost_create", cellid(), "vhost", vhost);
}

PRIVATE void
dchost_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*refused)
{
	/* REFERENCED */
	vhost_t		*vhost = vhost_local();
	dchost_t	*dcp = (dchost_t *)obj;
	bhv_desc_t	*phost_bdp;
	/* REFERENCED */
	tk_set_t	granted, already;
	cell_t		relocated_to;
	int		error;
	/* REFERENCED */
	int		msgerr;
	int		hostid;
	char		hostnm[_SYS_NMLN];
	char		*g_hostnmp = &hostnm[0];
	size_t		r_hostnmlen = 0;
	size_t		g_hostnmlen = _SYS_NMLN;
	char		domainnm[_SYS_NMLN];
	char		*g_domainnmp = &domainnm[0];
	size_t		r_domainnmlen = 0;
	size_t		g_domainnmlen = _SYS_NMLN;

	ASSERT(BHV_TO_VHOST(&dcp->dch_bhv) == vhost);

	HOST_TRACE6("dchost_tcif_obtain", cellid(), "vhost", 
		    vhost, "to_be_obtained", to_be_obtained);

	/*
	 * Get any value to be returned with token.
	 */
	phost_bdp = BHV_NEXT(&dcp->dch_bhv);
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_WRITE_TOKENSET)) {
		phost_gethostid(phost_bdp, &hostid);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_WRITE_TOKENSET)) {
		r_hostnmlen = _SYS_NMLN;
		phost_gethostname(phost_bdp, hostnm, &r_hostnmlen);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_WRITE_TOKENSET)) {
		r_domainnmlen = _SYS_NMLN;
		phost_getdomainname(phost_bdp, domainnm, &r_domainnmlen);
	}

	/*
	 * Call remote cell, redirection to a new cell is not expected
	 * since the object handle is guaranteed at this point.
	 */
	msgerr = invk_dshost_obtain(DCHOST_TO_SERVICE(dcp),
			   &error,
			   cellid(), to_be_obtained,
			   to_be_returned, dofret,
			   &already, &granted, refused,
			   hostid,
				hostnm, r_hostnmlen,
				domainnm, r_domainnmlen, 
			   &hostid,
				&g_hostnmp, &g_hostnmlen,
				&g_domainnmp, &g_domainnmlen, 
			   &dcp->dch_handle.h_objid,
			   &relocated_to);
	ASSERT(!msgerr);
	ASSERT(already == TK_NULLSET);
	ASSERT(error == 0 || *refused == to_be_obtained);
	ASSERT(error != 0 || granted == to_be_obtained);

	/*
	 * Set any value associated with granted tokens.
	 */
	if (TK_IS_IN_SET(granted, VHOST_HOSTID_WRITE_TOKENSET) ||
	    TK_IS_IN_SET(granted, VHOST_HOSTID_READ_TOKENSET)) {
		phost_sethostid(phost_bdp, hostid);
	}
	if (TK_IS_IN_SET(granted, VHOST_HOSTNAME_WRITE_TOKENSET) ||
	    TK_IS_IN_SET(granted, VHOST_HOSTNAME_READ_TOKENSET)) {
		phost_sethostname(phost_bdp, hostnm, &g_hostnmlen);
	}
	if (TK_IS_IN_SET(granted, VHOST_DOMAINNAME_WRITE_TOKENSET) ||
	    TK_IS_IN_SET(granted, VHOST_DOMAINNAME_READ_TOKENSET)) {
		phost_setdomainname(phost_bdp, domainnm, g_domainnmlen);
	}

	HOST_TRACE6("dchost_tcif_obtain", cellid(), "vhost", 
		    vhost, "granted", granted);
}

/*
 * token client return callout
 */
/* ARGSUSED */
static void
dchost_tcif_return(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	dchost_t	*dcp = (dchost_t *)obj;
	bhv_desc_t	*phost_bdp;
	/* REFERENCED */
	int		msgerr;
	int		hostid;
	char		hostnm[_SYS_NMLN];
	char		*g_hostnmp = &hostnm[0];
	size_t		r_hostnmlen = 0;
	size_t		g_hostnmlen = _SYS_NMLN;
	char		domainnm[_SYS_NMLN];
	char		*g_domainnmp = &domainnm[0];
	size_t		r_domainnmlen = 0;
	size_t		g_domainnmlen = _SYS_NMLN;
	

	ASSERT(tclient == dcp->dch_tclient);

	HOST_TRACE6("dchost_tcif_return", cellid(),
		    "obj", obj, "to_be_returned", to_be_returned);

	/*
	 * Get any value to be returned with token.
	 */
	phost_bdp = BHV_NEXT(&dcp->dch_bhv);
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_WRITE_TOKENSET)) {
		phost_gethostid(phost_bdp, &hostid);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_WRITE_TOKENSET)) {
		r_hostnmlen = _SYS_NMLN;
		phost_gethostname(phost_bdp, hostnm, &r_hostnmlen);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_WRITE_TOKENSET)) {
		r_domainnmlen = _SYS_NMLN;
		phost_getdomainname(phost_bdp, domainnm, &r_domainnmlen);
	}


	/*
	 * If the tokens to be recalled (server initiated returns) are read 
	 * tokens, the client(say A) lends the tokens instead of returning them.
 	 * The server recalls the read tokens and allows another client (B) to
	 * get the write token.  After B has finished the  operation, the
	 * server obtains the read token on behalf of A and returns it along
	 * with the associated data to A. A then calls its token client and
	 * indicates that it has refused to return the tokens (by passing
	 * them in the refused set of tkc_returned()). 
	 * This mechanism allows us to brodcast any writes from any client (B)
	 * to all other clients immediately. We want to do this aggressive
	 * brodcast so that if a client with the write token fails it does
	 * not lose the data with it.
	 */
	if ((TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_READ_TOKENSET) ||
		TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_READ_TOKENSET) ||
		TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_READ_TOKENSET))
		&&(TK_DISPOSITION(why, to_be_returned) != TK_CLIENT_INITIATED)){

		msgerr = invk_dshost_lend(DCHOST_TO_SERVICE(dcp),
				cellid(), 
				DCHOST_TO_OBJID(dcp),
				to_be_returned, TK_NULLSET, why,
				hostid, 
					hostnm, r_hostnmlen, 
					domainnm, r_domainnmlen,
				&hostid,
					&g_hostnmp, &g_hostnmlen,
					&g_domainnmp, &g_domainnmlen);
		ASSERT(!msgerr);


		tkc_returned(tclient, TK_NULLSET, to_be_returned);

		/*
		 * Set any values associated with lent tokens.
		 */
		if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_WRITE_TOKENSET) ||
		    TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_READ_TOKENSET)) {
			phost_sethostid(phost_bdp, hostid);
		}
		if (TK_IS_IN_SET(to_be_returned, 
				VHOST_HOSTNAME_WRITE_TOKENSET) || 
		    TK_IS_IN_SET(to_be_returned, 
				VHOST_HOSTNAME_READ_TOKENSET)) {
			phost_sethostname(phost_bdp, hostnm, &g_hostnmlen);
		}
		if (TK_IS_IN_SET(to_be_returned, 
					VHOST_DOMAINNAME_WRITE_TOKENSET) ||
		    TK_IS_IN_SET(to_be_returned, 
					VHOST_DOMAINNAME_READ_TOKENSET)) {
			phost_setdomainname(phost_bdp, domainnm, g_domainnmlen);
		}
	} else {
		msgerr = invk_dshost_return(DCHOST_TO_SERVICE(dcp),
				cellid(), 
				DCHOST_TO_OBJID(dcp),
				to_be_returned, unknown, why,
				hostid, hostnm, r_hostnmlen, 
				domainnm, r_domainnmlen);
		ASSERT(!msgerr);
		tkc_returned(tclient, to_be_returned, TK_NULLSET);
	}
}

/*
 * server initiated message to client to return a token - client side
 */
void
I_dchost_recall(
	tk_set_t	to_be_revoked,
	tk_disp_t	why)
{
	vhost_t		*vhost = vhost_local();
	bhv_desc_t	*bdp;
	dchost_t	*dcp;

	bdp = VHOST_BHV_FIRST(vhost);
	dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE4("I_dchost_recall", cellid(), "to_be_revoked", to_be_revoked);

	/*
	 * SVC tokens never get returned.
	 */
	ASSERT(!TK_IS_IN_SET(to_be_revoked, VHOST_SVC_ALL_TOKENSET));
	tkc_recall(dcp->dch_tclient, to_be_revoked, why);

	return;
}

/************************************************************************
 *	Client-side message handling					*
 ************************************************************************/

/* ARGSUSED */
void
I_dchost_killall(
	cell_t		target,
	int		signo,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)            /* 0 => global SIGKILL */
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);
	ASSERT(target == cellid());

	HOST_TRACE10("I_dchost_killall", cellid(), "signo", signo, "pgid", pgid,
		     "callers_pid", callers_pid, "callers_sid", callers_sid);

	/*
	 * Here to kill all local processes.
	 * To so, then reply.
	 */
	phost_killall(BHV_NEXT(bdp), signo, pgid, callers_pid, callers_sid);
}

/* ARGSUSED */
void
I_dchost_sync(
	cell_t		target,
	int		flags)
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);
	ASSERT(target == cellid());

	HOST_TRACE4("I_dchost_sync", cellid(), "flags", flags);

	/*
	 * Here to sync all locally mounted filesystems.
	 */
	phost_sync(BHV_NEXT(bdp), flags);
}

/* ARGSUSED */
void
I_dchost_set_time(
	cell_t		target,
	cell_t		source_cell)
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);
	ASSERT(target == cellid());

	HOST_TRACE4("I_dchost_set_time", cellid(), "source", source_cell);

	/*
	 * Synchronize local time with global time
	 */
	phost_set_time(BHV_NEXT(bdp), source_cell);
}

/* ARGSUSED */
void
I_dchost_adj_time(
	cell_t	target,
	long	adjustment)
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	long		odelta;
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);
	ASSERT(target == cellid());

	HOST_TRACE4("I_dchost_adj_time", cellid(), "adj", adjustment);

	/*
	 * Synchronize local time with global time
	 */
	phost_adj_time(BHV_NEXT(bdp), adjustment, &odelta);
}

/* ARGSUSED */
void
I_dchost_reboot(
	cell_t		target,
	int		fcn,
	char		*mdep)
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);
	ASSERT(target == cellid());

	HOST_TRACE4("I_dchost_reboot", cellid(), "fcn", fcn);

	/*
	 * This message is an IPC.
	 * Here to reboot the local cell.
	 * To so, but don't bother to reply ... the server doesn't
	 * expect it.
	 */
	phost_reboot(BHV_NEXT(bdp), fcn, mdep);
	/* NOTREACHED */
	panic("I_dchost_reboot: there is life after death");
}

/* ARGSUSED */
void
I_dchost_sysget(
	int	name,
	char	*buffer,
	int	*buflen_p,
	int	flags,
	sgt_cookie_t *cookie_p,
	sgt_info_t *info_p,
	char	*in_buf,
	size_t	in_buflen,
	char	**out_buf,
	size_t	*out_buflen,
	int	*error_p,
	void	**bufdesc)
	
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());

	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);
	HOST_TRACE4("I_dchost_sysget", cellid(), "flags", flags);

	/*
	 * Here to perform sysget on this cell.
	 */
	if (!(flags & SGT_UADDR)) {

		/* We use the in/out buffers to move data instead of
		 * referencing the user buffer directly. This allows
		 * a sum of all cells to be made or to eliminate a
		 * lot of unnecessary copyout traffic for small requests.
		 */

		ASSERT(*out_buflen != 0);
		ASSERT(*buflen_p == *out_buflen);
		*out_buf = kmem_alloc(*out_buflen, KM_SLEEP);
		bcopy(in_buf, *out_buf, *out_buflen);
		buffer = *out_buf;
	}
	else {
		ASSERT(*out_buflen == 0);
		ASSERT(in_buflen == 0);
	}
	*error_p = phost_sysget(BHV_NEXT(bdp), name, buffer, buflen_p,
		flags, cookie_p, info_p);
}

/* ARGSUSED */
void
I_dchost_sysget_done(
        char*           out_buf,
        size_t          out_buf_count,
        void *          bufdesc)
{
	if (out_buf) {
		kmem_free(out_buf, out_buf_count);
	}
}

/* ARGSUSED */
void
I_dchost_systune(
	int		group_index,
	int		tune_index,
	uint64_t	value,
	int		*errorp)
	
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);

	HOST_TRACE4("I_dchost_systune", cellid(), "index", tune_index);

	/*
	 * Here to perform systune on this cell.
	 */

	*errorp = phost_systune(BHV_NEXT(bdp), group_index, tune_index, value);
}

/* ARGSUSED */
void
I_dchost_credflush(
	cell_t		originator,
	cell_t		server,
	void		*arg)
	
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	service_t svc;
	/* REFERENCED */
	int msgerr;
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);

	HOST_TRACE4("I_dchost_credflush", cellid(), "cell", server);

	/*
	 * Here to perform credflush on this cell.
	 */

	phost_credflush(BHV_NEXT(bdp), server);

	SERVICE_MAKE(svc, originator, SVC_HOST);
	msgerr = invk_dshost_credflush_ack(svc, cellid(), arg);
	ASSERT(!msgerr);
}

void
I_dchost_sysacct(
	int		enable,
	cfs_handle_t	*handlep)
	
{
	bhv_desc_t	*bdp = VHOST_BHV_FIRST(vhost_local());
	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DC);

	HOST_TRACE4("I_dchost_sysacct", cellid(), "enable", enable);

	/*
	 * Here to perform sysacct on this cell.
	 */

	phost_sysacct(BHV_NEXT(bdp), enable, handlep);
}

/************************************************************************
 *	Client-side operations						*
 ************************************************************************/

/* ARGSUSED */
PRIVATE void
dchost_register(
	bhv_desc_t	*bdp,
	int		vh_svc_mask)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	tk_set_t	wanted, acquired;

	HOST_TRACE4("dchost_register", cellid(), "vh_svc_mask", vh_svc_mask);

	wanted = vh_svc_to_tokenset(vh_svc_mask);
	tkc_acquire(dcp->dch_tclient, wanted, &acquired);
	ASSERT(acquired == wanted);
}

/* ARGSUSED */
PRIVATE void
dchost_deregister(
	bhv_desc_t	*bdp,
	int		vh_svc_mask)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	tk_set_t	to_release, to_return;
	tk_disp_t	why;

	HOST_TRACE4("dchost_deregister", cellid(), "vh_svc_mask", vh_svc_mask);

	to_release = vh_svc_to_tokenset(vh_svc_mask);
	tkc_release(dcp->dch_tclient, to_release);

	/*
	 * Return cached tokens immediately.
	 */
	tkc_returning(dcp->dch_tclient, to_release, &to_return, &why, 0);
	if (to_return != TK_NULLSET)
		dchost_tcif_return(dcp->dch_tclient, (void *) dcp,
				   to_return, TK_NULLSET, why);
}

/* ARGSUSED */
void
dchost_sethostid(
	bhv_desc_t	*bdp,
	int		id)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE4("dchost_sethostid", cellid(), "id", id);

	tkc_acquire1(dcp->dch_tclient, VHOST_HOSTID_WRITE_TOKEN);
	phost_sethostid(BHV_NEXT(bdp), id);
	tkc_release1(dcp->dch_tclient, VHOST_HOSTID_WRITE_TOKEN);
}

/* ARGSUSED */
void
dchost_gethostid(
	bhv_desc_t	*bdp,
	int		*id)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE4("dchost_gethostid", cellid(), "id", id);

	tkc_acquire1(dcp->dch_tclient, VHOST_HOSTID_READ_TOKEN);
	phost_gethostid(BHV_NEXT(bdp), id);
	tkc_release1(dcp->dch_tclient, VHOST_HOSTID_READ_TOKEN);
}

/* ARGSUSED */
void
dchost_sethostname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE6("dchost_sethostname", cellid(), "name", name, "*len", *len);

	tkc_acquire1(dcp->dch_tclient, VHOST_HOSTNAME_WRITE_TOKEN);
	phost_sethostname(BHV_NEXT(bdp), name, len);
	tkc_release1(dcp->dch_tclient, VHOST_HOSTNAME_WRITE_TOKEN);
}

/* ARGSUSED */
void
dchost_gethostname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE6("dchost_gethostname", cellid(), "name", name, "*len", *len);

	tkc_acquire1(dcp->dch_tclient, VHOST_HOSTNAME_READ_TOKEN);
	phost_gethostname(BHV_NEXT(bdp), name, len);
	tkc_release1(dcp->dch_tclient, VHOST_HOSTNAME_READ_TOKEN);
}

/* ARGSUSED */
void
dchost_setdomainname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		len)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE6("dchost_setdomainname", cellid(), "name", name, "len", len);

	tkc_acquire1(dcp->dch_tclient, VHOST_DOMAINNAME_WRITE_TOKEN);
	phost_setdomainname(BHV_NEXT(bdp), name, len);
	tkc_release1(dcp->dch_tclient, VHOST_DOMAINNAME_WRITE_TOKEN);
}

/* ARGSUSED */
void
dchost_getdomainname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);

	HOST_TRACE6("dchost_getdomainname", cellid(),
		    "name", name, "*len", *len);

	tkc_acquire1(dcp->dch_tclient, VHOST_DOMAINNAME_READ_TOKEN);
	phost_getdomainname(BHV_NEXT(bdp), name, len);
	tkc_release1(dcp->dch_tclient, VHOST_DOMAINNAME_READ_TOKEN);
}

/* ARGSUSED */
PRIVATE void
dchost_killall(
	bhv_desc_t	*bdp,
	int		signo,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)            /* 0 => global SIGKILL */
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;
	int		me_too;

	HOST_TRACE10("dchost_killall", cellid(), "signo", signo, "pgid", pgid,
		     "callers_pid", callers_pid, "callers_sid", callers_sid);

	/*
	 * Call the server. If the local cell is registered for killall
	 * requests, the reply will tell us and we'll perform the physical op.
	 */
	msgerr = invk_dshost_killall(DCHOST_TO_SERVICE(dcp),
				     DCHOST_TO_OBJID(dcp), cellid(),
				     signo, pgid, callers_pid, callers_sid,
				     &me_too);
	ASSERT(!msgerr);
	if (me_too)
		phost_killall(BHV_NEXT(bdp),
			      signo, pgid, callers_pid, callers_sid);
}

/* ARGSUSED */
PRIVATE void
dchost_reboot(
	bhv_desc_t	*bdp,
	int		fcn,
	char		*mdep)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;

	HOST_TRACE4("dchost_reboot", cellid(), "fcn", fcn);

	/*
	 * Function-ship to server; no reply is expected, though.
	 * The server will send a separate request message to reboot us.
	 */
	msgerr = invk_dshost_reboot(DCHOST_TO_SERVICE(dcp),
				    DCHOST_TO_OBJID(dcp), cellid(),
				    fcn, mdep);
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dchost_sync(
	bhv_desc_t	*bdp,
	int		flags)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;
	int		me_too;

	HOST_TRACE4("dchost_sync", cellid(), "flags", flags);

	/*
	 * Call server. If the local cell is registered for sync requests,
	 * the reply will tell us and we'll perform the physical op.
	 */
	msgerr = invk_dshost_sync(DCHOST_TO_SERVICE(dcp),
				  DCHOST_TO_OBJID(dcp), cellid(),
				  flags, &me_too);
	ASSERT(!msgerr);
	if (me_too)
		phost_sync(BHV_NEXT(bdp), flags);
}

/* ARGSUSED */
PRIVATE void
dchost_set_time(
	bhv_desc_t	*bdp,
	cell_t		source_cell)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;
	int		me_too;

	HOST_TRACE2("dchost_set_time", cellid());

	/*
	 * Call server.   Local time has already been updated.
	 */
	msgerr = invk_dshost_set_time(DCHOST_TO_SERVICE(dcp),
				  DCHOST_TO_OBJID(dcp), cellid(), 
				  source_cell, &me_too);
	ASSERT(!msgerr);
	ASSERT(me_too);
	if (me_too)
		phost_set_time(BHV_NEXT(bdp), source_cell);
}

/* ARGSUSED */
PRIVATE void
dchost_adj_time(
	bhv_desc_t	*bdp,
	long		adjustment,
	long		*odelta)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;
	int		me_too;

	HOST_TRACE2("dchost_adj_time", cellid());

	/*
	 * Call server.
	 */
	msgerr = invk_dshost_adj_time(DCHOST_TO_SERVICE(dcp),
				  DCHOST_TO_OBJID(dcp), cellid(),
				  adjustment, &me_too);
	ASSERT(!msgerr);
	ASSERT(me_too);
	if (me_too)
		phost_adj_time(BHV_NEXT(bdp), adjustment, odelta);
}

/* ARGSUSED */
PRIVATE int
dchost_sysget(
	bhv_desc_t	*bdp,
	int		name,
	char		*buf,
	int		*buflen_p,
	int		flags,
	sgt_cookie_t 	*cookie_p,
	sgt_info_t	*info_p)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	char		*kbuf = (char *)0;
	size_t		kbuflen = 0;
	/* REFERENCED */
	int		msgerr, error;

	HOST_TRACE4("dchost_sysget", cellid(), "name", name);

	/*
	 * Call server. It will do all the work of aggregating across cells.
	 */

	if (!(flags & SGT_UADDR)) {

		/* The buffer is a kernel buffer */

		kbuf = buf;
		kbuflen = *buflen_p;
	}
	msgerr = invk_dshost_sysget(DCHOST_TO_SERVICE(dcp),
			DCHOST_TO_OBJID(dcp), name, buf, buflen_p, flags,
			cookie_p, info_p, kbuf, kbuflen, 
			&kbuf, &kbuflen, &error);
	ASSERT(!msgerr);
	return(error);
}

/* ARGSUSED */
PRIVATE int
dchost_systune(
        bhv_desc_t      *bdp,
	int		group_index,
        int             tune_index,
	uint64_t	value)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;
	int		error;
	extern struct tunename tunename[];

	HOST_TRACE4("dchost_systune", cellid(), "name", tune_index);

	/*
         * Call server. It will do all the work of aggregating across cells
	 * except ours since that has already been taken care of.
         */

	msgerr = invk_dshost_systune(DCHOST_TO_SERVICE(dcp),
		DCHOST_TO_OBJID(dcp), cellid(), group_index, tune_index,
		value, &error);
	ASSERT(!msgerr);
	return(error);
}

/* ARGSUSED */
PRIVATE void
dchost_credflush(
        bhv_desc_t      *bdp,
	cell_t		server)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;

	HOST_TRACE4("dchost_credflush", cellid(), "cell", server);

	/*
         * Call server. It will do all the work of aggregating across cells
	 * except ours since that has already been taken care of.
         */

	msgerr = invk_dshost_credflush(DCHOST_TO_SERVICE(dcp), 
				       DCHOST_TO_OBJID(dcp),
				       cellid(),
				       server);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE void
dchost_credflush_wait(
        bhv_desc_t      *bdp,
	cell_t		server)
{
	dchost_t	*dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int		msgerr;

	HOST_TRACE4("dchost_credflush", cellid(), "cell", server);

	/*
         * Call server. It will do all the work of aggregating across cells
	 * except ours since that has already been taken care of.
         */

	msgerr = invk_dshost_credflush_wait(DCHOST_TO_SERVICE(dcp), 
				       DCHOST_TO_OBJID(dcp),
				       server);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE void
dchost_sysacct(bhv_desc_t *bdp, int enable, cfs_handle_t *handlep)
{
	dchost_t        *dcp = BHV_TO_DCHOST(bdp);
	/* REFERENCED */
	int             msgerr;

	HOST_TRACE4("dchost_sysacct", cellid(), "enable", enable);

	msgerr = invk_dshost_sysacct(DCHOST_TO_SERVICE(dcp),
		DCHOST_TO_OBJID(dcp), cellid(), enable, handlep);
	ASSERT(!msgerr);
}

#ifdef DEBUG
void
idbg_dchost_print(
	dchost_t	*dcp)
{
	qprintf("dchost 0x%x:\n", dcp);
	qprintf("    token state - client 0x%x\n", dcp->dch_tclient);
	qprintf("    handle (0x%x, 0x%x)\n",
		HANDLE_TO_SERVICE(dcp->dch_handle),
		HANDLE_TO_OBJID(dcp->dch_handle));
	qprintf("    bhv 0x%x\n", &dcp->dch_bhv);
	idbg_phost((__psint_t)BHV_PDATA(BHV_NEXT(&dcp->dch_bhv)));
}
#endif
