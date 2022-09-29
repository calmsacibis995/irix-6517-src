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
#ident "$Id: dshost.c,v 1.22 1997/09/25 21:01:41 henseler Exp $"

/*
 * Server side for the process group subsystem
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/idbgentry.h>
#include <sys/conf.h>
#include <sys/atomic_ops.h>
#include <sys/utsname.h>

#include <ksys/behavior.h>
#include <ksys/cell/tkm.h>
#include <ksys/vhost.h>
#include <ksys/cell/wp.h>
#include <fs/cfs/cfs.h>

#include "dhost.h"
#include "vhost_private.h"
#include "invk_dchost_stubs.h"
#include "I_dshost_stubs.h"

#ifdef DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif

/*
 * Protos for client-side distributed layer.
 */
PRIVATE void	dshost_register(bhv_desc_t *, int);
PRIVATE void	dshost_deregister(bhv_desc_t *, int);
PRIVATE void	dshost_sethostid(bhv_desc_t *, int);
PRIVATE void	dshost_gethostid(bhv_desc_t *, int *);
PRIVATE void	dshost_sethostname(bhv_desc_t *, char *, size_t *);
PRIVATE void	dshost_gethostname(bhv_desc_t *, char *, size_t *);
PRIVATE void	dshost_setdomainname(bhv_desc_t *, char *, size_t);
PRIVATE void	dshost_getdomainname(bhv_desc_t *, char *, size_t *);
PRIVATE void	dshost_killall(bhv_desc_t *, int, pid_t, pid_t, pid_t);
PRIVATE void	dshost_reboot(bhv_desc_t *, int, char *);
PRIVATE void	dshost_sync(bhv_desc_t *, int);
PRIVATE int	dshost_sysget(bhv_desc_t *, int, char *, int *, int,
			sgt_cookie_t *, sgt_info_t *);
PRIVATE int	dshost_systune(bhv_desc_t *, int, int, uint64_t);
PRIVATE void	dshost_credflush(bhv_desc_t *, cell_t);
PRIVATE void	dshost_credflush_wait(bhv_desc_t *, cell_t);
PRIVATE void	dshost_sysacct(bhv_desc_t *, int, cfs_handle_t *);
PRIVATE void	dshost_set_time(bhv_desc_t *, cell_t);
PRIVATE void	dshost_adj_time(bhv_desc_t *, long, long *);
PRIVATE void	dshost_recovery_cell(void *, tks_ch_t, tk_set_t, tk_set_t);

PRIVATE	void	credflush_init(void);
PRIVATE void	credflush_register(cell_t);
PRIVATE void	credflush_deregister(cell_t);

vhost_ops_t dshost_ops = {
	BHV_IDENTITY_INIT_POSITION(VHOST_BHV_DS),
	dshost_register,
	dshost_deregister,
	dshost_sethostid,
	dshost_gethostid,
	dshost_sethostname,
	dshost_gethostname,
	dshost_setdomainname,
	dshost_getdomainname,
	dshost_killall,
	dshost_reboot,
	dshost_sync,
	dshost_sysget,
	dshost_systune,
	dshost_credflush,
	dshost_credflush_wait,
	dshost_sysacct,
	dshost_set_time,
	dshost_adj_time,
};

static void dshost_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void dshost_tsif_recalled(void *, tk_set_t, tk_set_t);

static tks_ifstate_t dshost_tserver_iface = {
		dshost_tsif_recall,
		dshost_tsif_recalled,
		NULL,
};

/*
 * A remote client has requested access to the previously local-only vhost
 * set up and interpose the DS layer.
 */
void
vhost_interpose(vhost_t *vhost)
{
	dshost_t	*dsp;
	tk_set_t	wanted, granted, refused, already_obtained;
	/* REFERENCED */
	int		error;

	dsp = kern_malloc(sizeof(*dsp));

	tks_create("dhost", dsp->dsh_tserver, dsp, &dshost_tserver_iface,
		   VHOST_NTOKENS, (void *)(__psint_t)cellid());

	wanted = TK_ADD_SET(vh_svc_to_tokenset(VH_SVC_ALL),
			    VHOST_MEMBER_TOKENSET);
	tks_obtain(dsp->dsh_tserver, (tks_ch_t)cellid(),
		   wanted, &granted, &refused,
		   &already_obtained);
	ASSERT(granted == wanted);
	ASSERT(already_obtained == TK_NULLSET);
	ASSERT(refused == TK_NULLSET);

	tkc_create_local("dhost", dsp->dsh_tclient, dsp->dsh_tserver,
			 VHOST_NTOKENS, granted, granted,
			 (void *)(__psint_t)cellid());
	bhv_desc_init(&dsp->dsh_bhv, dsp, vhost, &dshost_ops);
	error = bhv_insert(&vhost->vh_bhvh, &dsp->dsh_bhv);
	ASSERT(!error);

	credflush_init();

	HOST_TRACE6("vhost_interpose", cellid(), "vhost", vhost, "dsp", dsp);
}

/*
 * Client request to server to obtain tokens.
 * This means we potentially need to change from a local vhost to
 * a distributed vhost
 */
 /* ARGSUSED */
void
I_dshost_obtain(
	int		*error,
	cell_t		sender,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*already_obtained,
	tk_set_t	*granted,
	tk_set_t	*refused,
	int		r_hostid,
	char		*r_hostnm,
	size_t		r_hostnmlen,
	char		*r_domainnm,
	size_t		r_domainnmlen,
	int		*g_hostidp,
	char		**g_hostnmp,
	size_t		*g_hostnmlenp,
	char		**g_domainnmp,
	size_t		*g_domainnmlenp,
	objid_t		*objid,
	cell_t		*redirect,
	void		**bufferof_desc)
{
	vhost_t		*vhost = vhost_local();
	dshost_t	*dsp;
	bhv_desc_t	*bdp;
	bhv_desc_t	*phost_bdp;


	HOST_TRACE6("I_dshost_obtain", sender, "obtain", to_be_obtained,
		    "objid", *objid);

	if (vhost == NULL) {
		*refused = to_be_obtained;
		*granted = TK_NULLSET;
		*already_obtained = TK_NULLSET;
		*error = ENOENT;
		return;
	}

	/*
	 * Need the read lock at this stage to be assured that
	 * no migration is in progress.
	 */
	BHV_READ_LOCK(VHOST_BHV_HEAD_PTR(vhost));
	bdp = VHOST_BHV_FIRST(vhost);

	/*
	 * need to lock out other threads that might also be
	 * in here - only one can transition us from local to distributed
	 */
	if (BHV_POSITION(bdp) == VHOST_BHV_PP) {
		/*
		 * We need to add a distributed layer
		 */
		BHV_READ_UNLOCK(VHOST_BHV_HEAD_PTR(vhost));
		BHV_WRITE_LOCK(VHOST_BHV_HEAD_PTR(vhost));
		/*
		 * re-set bdp to 'top' level (since another thread might
		 * have beat us to the lock and already done the interposition
		 * (and conceivably even migrated the thing elsewhere)
		 */
		bdp = VHOST_BHV_FIRST(vhost);
		if (BHV_POSITION(bdp) == VHOST_BHV_PP)
			vhost_interpose(vhost);
		BHV_WRITE_TO_READ(VHOST_BHV_HEAD_PTR(vhost));
		/* re-set bdp to 'top' level */
		bdp = VHOST_BHV_FIRST(vhost);
	}

	/*
	 * Could reach this point but find that the dshost/phost has migrated
	 * away from its origin cell (here). The dchost is guaranteed - so
	 * return redirection to new service (i.e. cell).
	 */
	if (BHV_POSITION(bdp) == VHOST_BHV_DC) {
		dchost_t	*dcp = BHV_TO_DCHOST(bdp);
		*redirect = SERVICE_TO_CELL(dcp->dch_handle.h_service);
		BHV_READ_UNLOCK(VHOST_BHV_HEAD_PTR(vhost));
		*error = EMIGRATED;
		return;
	}

	ASSERT(BHV_POSITION(bdp) == VHOST_BHV_DS);

	/*
	 * since we only have 2 levels max - dshost is always the head
	 * and we want to return the pointer to the behavior descriptor 
	 * that represents the dshost layer.
	 */
	*objid = bdp;

	dsp = BHV_TO_DSHOST(bdp);
	phost_bdp = BHV_NEXT(bdp);

	/*
	 * Set any value associated with returning write tokens.
	 */
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_WRITE_TOKENSET)) {
		phost_sethostid(phost_bdp, r_hostid);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_WRITE_TOKENSET)) {
		size_t	len = r_hostnmlen;	/* phost_sethostname returns
						 * "new" length - toss it */
		phost_sethostname(phost_bdp, r_hostnm, &len);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_WRITE_TOKENSET)) {
		phost_setdomainname(phost_bdp, r_domainnm, r_domainnmlen);
	}

	if (to_be_returned != TK_NULLSET)
		tks_return(dsp->dsh_tserver, sender, to_be_returned, 
			   TK_NULLSET, TK_NULLSET, dofret);

	tks_obtain(dsp->dsh_tserver, sender,
		   to_be_obtained, granted, refused, already_obtained);

	/*
	 * Get any value associated with a granted token.
	 */
	if (TK_IS_IN_SET(*granted, VHOST_HOSTID_WRITE_TOKENSET) ||
	    TK_IS_IN_SET(*granted, VHOST_HOSTID_READ_TOKENSET)) {
		phost_gethostid(phost_bdp, g_hostidp);
	}
	if (TK_IS_IN_SET(*granted, VHOST_HOSTNAME_WRITE_TOKENSET) ||
	    TK_IS_IN_SET(*granted, VHOST_HOSTNAME_READ_TOKENSET)) {
		*g_hostnmlenp = _SYS_NMLN;
		*g_hostnmp = (char *) kmem_alloc(*g_hostnmlenp, KM_SLEEP);
		phost_gethostname(phost_bdp, *g_hostnmp, g_hostnmlenp);
	}
	else {
		*g_hostnmlenp = 0;
	}
	if (TK_IS_IN_SET(*granted, VHOST_DOMAINNAME_WRITE_TOKENSET) ||
	    TK_IS_IN_SET(*granted, VHOST_DOMAINNAME_READ_TOKENSET)) {
		*g_domainnmlenp = _SYS_NMLN;
		*g_domainnmp = (char *) kmem_alloc(*g_domainnmlenp, KM_SLEEP);
		phost_getdomainname(phost_bdp, *g_domainnmp, g_domainnmlenp);
	}
	else {
		*g_domainnmlenp = 0;
	}


	if (TK_IS_IN_SET(*granted, VHOST_TOKEN_CRED))
		credflush_register(sender);

	BHV_READ_UNLOCK(VHOST_BHV_HEAD_PTR(vhost));

	*error = 0;
	return;
}

/* ARGSUSED */
void
I_dshost_obtain_done(
	char		*g_hostnm,
	size_t		g_hostnmlen,
	char		*g_domainnm,
	size_t		g_domainnmlen,
	void		*bufferof_desc)
{
	/*
	 * Free any allocated space used for values associated with
	 * granted tokens. The variable sizes are ignored because
	 * fixed, maximum length buffers were originally allocated.
	 */
	if (g_hostnm != NULL)
		kmem_free((void *) g_hostnm, _SYS_NMLN);
	if (g_domainnm != NULL)
		kmem_free((void *) g_domainnm, _SYS_NMLN);
}

/*
 * server side of client returning tokens
 */
/* ARGSUSED */
void
I_dshost_return(
	cell_t		sender,
	objid_t		objid,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why,
	int		r_hostid,
	char		*r_hostnm,
	size_t		r_hostnmlen,
	char		*r_domainnm,
	size_t		r_domainnmlen)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	bhv_desc_t	*phost_bdp = BHV_NEXT(bdp);
	/* REFERENCED */
	vhost_t		*vpg = BHV_TO_VHOST(bdp);
 
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("I_dshost_return", sender, "tokens", to_be_returned);

	/*
	 * Set any value associated with returning write tokens.
	 */
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_WRITE_TOKENSET)) {
		phost_sethostid(phost_bdp, r_hostid);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_WRITE_TOKENSET)) {
		size_t	len = r_hostnmlen;
		phost_sethostname(phost_bdp, r_hostnm, &len);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_WRITE_TOKENSET)) {
		phost_setdomainname(phost_bdp, r_domainnm, r_domainnmlen);
	}

	tks_return(dsp->dsh_tserver, sender, to_be_returned,
		   TK_NULLSET, unknown, why);

	if (TK_IS_IN_SET(to_be_returned, VHOST_TOKEN_CRED))
		credflush_deregister(sender);

}


/*
 * If the tokens to be recalled (server initiated returns) are read 
 * tokens, the client(say A) lends the tokens instead of returning them.         * The server recalls the read tokens and allows another client (B) to
 * get the write token.  After B has finished the  operation, the
 * server obtains the read token on behalf of A and returns it along
 * with the associated data to A. A then calls its token client and
 * indicates that it has refused to return the tokens (by passing
 * them in the refused set of tkc_returned()).
 * This mechanism allows us to brodcast any writes from any client (B)
 * to all other clients who have read tokens immediately. We want to do this 
 * aggressive brodcast so that if a client with the write token fails it does
 * not lose the data with it. It will be at the least also be present in the 
 * server.
 */

/* ARGSUSED */
void
I_dshost_lend(
	cell_t		sender,
	objid_t		objid,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why,
	int		r_hostid,
	char		*r_hostnm,
	size_t		r_hostnmlen,
	char		*r_domainnm,
	size_t		r_domainnmlen,
	int		*g_hostidp,
	char		**g_hostnmp,
	size_t		*g_hostnmlenp,
	char		**g_domainnmp,
	size_t		*g_domainnmlenp,
	void		**bufferof_desc)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	bhv_desc_t	*phost_bdp = BHV_NEXT(bdp);
	/* REFERENCED */
	vhost_t		*vpg = BHV_TO_VHOST(bdp);
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	held;
 
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("I_dshost_return", sender, "tokens", to_be_returned);

	/*
	 * Set any value associated with returning write tokens.
	 */
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_WRITE_TOKENSET)) {
		phost_sethostid(phost_bdp, r_hostid);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_WRITE_TOKENSET)) {
		size_t	len = r_hostnmlen;
		phost_sethostname(phost_bdp, r_hostnm, &len);
	}
	if (TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_WRITE_TOKENSET)) {
		phost_setdomainname(phost_bdp, r_domainnm, r_domainnmlen);
	}

	tks_return(dsp->dsh_tserver, sender, to_be_returned,
		   TK_NULLSET, unknown, why);

        if (TK_IS_IN_SET(to_be_returned, VHOST_HOSTID_READ_TOKENSET) ||
                TK_IS_IN_SET(to_be_returned, VHOST_HOSTNAME_READ_TOKENSET) ||
                TK_IS_IN_SET(to_be_returned, VHOST_DOMAINNAME_READ_TOKENSET)) {
		tks_obtain(dsp->dsh_tserver, sender, to_be_returned,
				&granted, &refused, &held);
		/*
		 * Get any value associated with a granted token.
		 */
		if (TK_IS_IN_SET(granted, VHOST_HOSTID_WRITE_TOKENSET) ||
		    TK_IS_IN_SET(granted, VHOST_HOSTID_READ_TOKENSET)) {
			phost_gethostid(phost_bdp, g_hostidp);
		}
		if (TK_IS_IN_SET(granted, VHOST_HOSTNAME_WRITE_TOKENSET) ||
		    TK_IS_IN_SET(granted, VHOST_HOSTNAME_READ_TOKENSET)) {
			*g_hostnmlenp = _SYS_NMLN;
			*g_hostnmp = (char *) kmem_alloc(*g_hostnmlenp, 
							KM_SLEEP);
			phost_gethostname(phost_bdp, *g_hostnmp, g_hostnmlenp);
		} else {
			*g_hostnmlenp = 0;
		}
		if (TK_IS_IN_SET(granted, VHOST_DOMAINNAME_WRITE_TOKENSET) ||
		    TK_IS_IN_SET(granted, VHOST_DOMAINNAME_READ_TOKENSET)) {
			*g_domainnmlenp = _SYS_NMLN;
			*g_domainnmp = (char *) kmem_alloc(*g_domainnmlenp, 
						KM_SLEEP);
			phost_getdomainname(phost_bdp, *g_domainnmp, 
							g_domainnmlenp);
		} else {
			*g_domainnmlenp = 0;
		}
		ASSERT(granted == to_be_returned);
		ASSERT(refused == TK_NULLSET);
		ASSERT(held == TK_NULLSET);
	}

	if (TK_IS_IN_SET(to_be_returned, VHOST_TOKEN_CRED))
		credflush_deregister(sender);

}

/* ARGSUSED */
void
I_dshost_lend_done(
	char		*g_hostnm,
	size_t		g_hostnmlen,
	char		*g_domainnm,
	size_t		g_domainnmlen,
	void		*bufferof_desc)
{
	/*
	 * Free any allocated space used for values associated with
	 * granted tokens. The variable sizes are ignored because
	 * fixed, maximum length buffers were originally allocated.
	 */
	if (g_hostnm != NULL)
		kmem_free((void *) g_hostnm, _SYS_NMLN);
	if (g_domainnm != NULL)
		kmem_free((void *) g_domainnm, _SYS_NMLN);
}

static void
dshost_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_be_recalled,
	tk_disp_t	why)
{
	dshost_t	*dsp = (dshost_t *)obj;

	HOST_TRACE4("dshost_tsif_recall", cellid(),
		    "to_be_recalled", to_be_recalled);

	if (client == cellid()) {
		tkc_recall(dsp->dsh_tclient, to_be_recalled, why);
	} else {
		service_t svc;
		/* REFERENCED */
		int msgerr;

		SERVICE_MAKE(svc, (cell_t)client, SVC_HOST);
		msgerr = invk_dchost_recall(svc, to_be_recalled, why);
		ASSERT(!msgerr);
	}
}

/* ARGSUSED */
static void
dshost_tsif_recalled(void *obj, tk_set_t done, tk_set_t ndone)
{
	panic("dshost_tsif_recalled: callout not implemented");
}


/************************************************************************
 * Routines performing iterations over client cells to request actions.	*
 ************************************************************************/

/*
 * Structure used to communicate parameters to the general
 * client iterator routine.
 */
typedef struct {
	void	(*fn)(service_t, void *);
					/* fn called per iteration */
	void	*fn_params;		/* parameter block for fn */
	tk_set_t tokens;		/* tokenset iterated over */
	cell_t	caller_cell;		/* requesting client */
	int	caller_skipped;		/* set if requesting cell is skipped */
} iterator_t;


/*
 * General client iterator.
 * Visits all cells granted a given set of tokens, skipping both
 * the local cell and the requesting cell.
 */
/* ARGSUSED */
PRIVATE tks_iter_t
client_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	service_t	svc;
	iterator_t	*it;

	it = va_arg(args, iterator_t *);
	if (!TK_IS_IN_SET(it->tokens, tokens_owned))
		return TKS_CONTINUE;

	if (client == it->caller_cell) {
		it->caller_skipped = 1;
	} else {
		SERVICE_MAKE(svc, (cell_t)client, SVC_HOST);
		(*it->fn)(svc, it->fn_params);
	}

	return TKS_CONTINUE;
}

/* Parameter block to communicate with killall send message function */
typedef struct {
	int	signo;
	pid_t	pgid;
	pid_t	callers_pid;
	pid_t	callers_sid;
} killall_args_t;

PRIVATE void
client_killall(
	service_t	svc,
	void		*params)
{
	killall_args_t	*ra = (killall_args_t *) params;

	if (SERVICE_TO_CELL(svc) == cellid()) {
		phost_killall(VHOST_BHV_FIRST(vhost_local()),
			      ra->signo, ra->pgid,
			      ra->callers_pid, ra->callers_sid);
	} else {
		/* REFERENCED */
		int	msgerr;
		msgerr = invk_dchost_killall(svc, SERVICE_TO_CELL(svc),
					     ra->signo, ra->pgid,
					     ra->callers_pid, ra->callers_sid);
		ASSERT(!msgerr);
	}
}

PRIVATE void
iterate_killall(
	dshost_t	*dsp,
	cell_t		caller_cell,
	int		signo,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid,
	int		*caller_included)
{
	/* REFERENCED */
	int		result;
	iterator_t	it;		/* iteration parameters */
	killall_args_t	ra;		/* killall-specific args */

	/*
	 * Specify iteration parameters:
	 */
	it.fn		= client_killall;
	it.fn_params	= (void *) &ra;
	it.caller_cell  = caller_cell;
	it.tokens	= VHOST_KILLALL_TOKENSET;
	it.caller_skipped = 0;

	/*
	 * Specify parameters for remote killall RPC
	 */
	ra.signo	= signo;
	ra.pgid		= pgid;
	ra.callers_pid	= callers_pid;
	ra.callers_sid	= callers_sid;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver,
			     it.tokens, TKS_STABLE, client_iterator, &it);
	ASSERT(result == TKS_CONTINUE);

	*caller_included = it.caller_skipped;
}

/* Parameter block to communicate with sync send message function */
typedef struct {
	int	flags;
} sync_args_t;

PRIVATE void
client_sync(
	service_t	svc,
	void		*params)
{
	sync_args_t	*ra = (sync_args_t *) params;
	if (SERVICE_TO_CELL(svc) == cellid()) {
		phost_sync(VHOST_BHV_FIRST(vhost_local()), ra->flags);
	} else {
		/* REFERENCED */
		int		msgerr;
		msgerr = invk_dchost_sync(svc, SERVICE_TO_CELL(svc), ra->flags);
		ASSERT(!msgerr);
	}
}

PRIVATE void
iterate_sync(
	dshost_t	*dsp,
	cell_t		caller_cell,
	int		flags,
	int		*caller_included)
{
	/* REFERENCED */
	int		result;
	iterator_t	it;		/* iteration parameters */
	sync_args_t	ra;		/* sync-specific args */

	/*
	 * Specify iteration parameters:
	 */
	it.fn		= client_sync;
	it.fn_params	= (void *) &ra;
	it.caller_cell  = caller_cell;
	it.tokens	= VHOST_SYNC_TOKENSET;
	it.caller_skipped = 0;

	/*
	 * Specify parameter for remote sync RPC
	 */
	ra.flags	= flags;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver, 
		 	it.tokens, TKS_STABLE, client_iterator, &it);
	ASSERT(result == TKS_CONTINUE);

	*caller_included = it.caller_skipped;
}

/* Parameter block to communicate with set_time send message function */
typedef struct {
	int	source_cell;
} set_time_args_t;

PRIVATE void
client_set_time(
	service_t	svc,
	void		*params)
{
	set_time_args_t	*ra = (set_time_args_t *) params;
	if (SERVICE_TO_CELL(svc) == cellid()) {
		phost_set_time(VHOST_BHV_FIRST(vhost_local()), ra->source_cell);
	} else {
		/* REFERENCED */
		int		msgerr;
		msgerr = invk_dchost_set_time(svc, SERVICE_TO_CELL(svc), ra->source_cell);
		ASSERT(!msgerr);
	}
}

PRIVATE void
iterate_set_time(
	dshost_t	*dsp,
	cell_t		caller_cell,
	cell_t		source_cell,
	int		*caller_included)
{
	/* REFERENCED */
	int		result;
	iterator_t	it;		/* iteration parameters */
	set_time_args_t	ra;		/* set_time-specific args */

	/*
	 * Specify iteration parameters:
	 */
	it.fn		= client_set_time;
	it.fn_params	= (void *) &ra;
	it.caller_cell  = caller_cell;
	it.tokens	= VHOST_SET_TIME_TOKENSET;
	it.caller_skipped = 0;

	/*
	 * Specify parameter for remote set_time RPC
	 */
	ra.source_cell = source_cell;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver, 
		 	it.tokens, TKS_STABLE, client_iterator, &it);
	ASSERT(result == TKS_CONTINUE);

	*caller_included = it.caller_skipped;
}

/* Parameter block to communicate with adj_time send message function */
typedef struct {
	long	adjustment;
	long	*odelta;
} adj_time_args_t;

PRIVATE void
client_adj_time(
	service_t	svc,
	void		*params)
{
	adj_time_args_t	*ra = (adj_time_args_t *) params;
	if (SERVICE_TO_CELL(svc) == cellid()) {
		phost_adj_time(VHOST_BHV_FIRST(vhost_local()), ra->adjustment, 
			ra->odelta);
	} else {
		/* REFERENCED */
		int		msgerr;
		msgerr = invk_dchost_adj_time(svc, SERVICE_TO_CELL(svc), 
			ra->adjustment);
		ASSERT(!msgerr);
	}
}

PRIVATE void
iterate_adj_time(
	dshost_t	*dsp,
	cell_t		caller_cell,
	long		adjustment,
	long		*odelta,
	int		*caller_included)
{
	/* REFERENCED */
	int		result;
	iterator_t	it;		/* iteration parameters */
	adj_time_args_t	ra;		/* adj_time-specific args */

	/*
	 * Specify iteration parameters:
	 */
	it.fn		= client_adj_time;
	it.fn_params	= (void *) &ra;
	it.caller_cell  = caller_cell;
	it.tokens	= VHOST_SET_TIME_TOKENSET;
	it.caller_skipped = 0;

	/*
	 * Specify parameter for remote adj_time RPC
	 */
	ra.adjustment = adjustment;
	ra.odelta = odelta;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver, 
		 	it.tokens, TKS_STABLE, client_iterator, &it);
	ASSERT(result == TKS_CONTINUE);

	*caller_included = it.caller_skipped;
}

/* Parameter block to communicate with reboot send message function */
typedef struct {
	int	fcn;
	char	*mdep;
} reboot_args_t;

PRIVATE void
client_reboot(
	service_t	svc,
	void		*params)
{
	reboot_args_t	*ra = (reboot_args_t *) params;
	if (SERVICE_TO_CELL(svc) == cellid()) {
		phost_reboot(VHOST_BHV_FIRST(vhost_local()), ra->fcn, ra->mdep);
	} else {
		/* REFERENCED */
		int		msgerr;
		msgerr = invk_dchost_reboot(svc, SERVICE_TO_CELL(svc),
					    ra->fcn, ra->mdep);
		ASSERT(!msgerr);
	}
}

PRIVATE void
iterate_reboot(
	dshost_t	*dsp,
	cell_t		caller_cell,
	int		fnc,
	char		*mdep,
	int		*caller_included)
{
	/* REFERENCED */
	int		result;
	iterator_t	it;		/* iteration parameters */
	reboot_args_t	ra;		/* reboot-specific args */

	/*
	 * Specify iteration parameters:
	 */
	it.fn		= client_reboot;
	it.fn_params	= (void *) &ra;
	it.caller_cell  = caller_cell;
	it.tokens	= VHOST_MEMBER_TOKENSET;
	it.caller_skipped = 0;

	/*
	 * Specify parameter for remote reboot RPC
	 */
	ra.fcn	= fnc;
	ra.mdep	= mdep;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver,
			     it.tokens, TKS_STABLE, client_iterator, &it);
	ASSERT(result == TKS_CONTINUE);

	*caller_included = it.caller_skipped;
}

/* Parameter block to communicate with sysget send message function */
typedef struct {
	int		name;
	char		*buffer;
	int		buflen;
	int		flags;
	sgt_cookie_t 	*cookie_p;
	sgt_info_t	*info_p;
	int		error;
} sysget_args_t;

/*
 * sysget client iterator.
 * Visits all cells granted a given set of tokens, skipping cells
 * not specified by the requesting cell.  We do all aggregation 
 * and summing up of results here (if needed).  If this is a continuation
 * call by convention the first word of the opaque field of the cookie is
 * the cellid where the call is to continue from.
 */
/* ARGSUSED */
PRIVATE tks_iter_t
sysget_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	service_t	svc;
	sysget_args_t	*ap;
	sgt_info_t	info;
	int		buflen;
	char		*kbuf = (char *)0;
	size_t		kbuflen = 0;

	if (!TK_IS_IN_SET(VHOST_MEMBER_TOKENSET, tokens_owned)) {

		/* Not a registered cell */

		return TKS_CONTINUE;
	}

	ap = va_arg(args, sysget_args_t *);

	/*
	 * Based on the cookie try and select the correct cell to
         * process the request.  Currently VHOST only knows cell ids
         * so other types of requests may result in many cells being
         * queried before the object is found.  The buflen value returned
         * from the cell indicates if a query was succesful.
	 */

	if (ap->cookie_p->sc_type == SC_CELLID && 
			ap->cookie_p->sc_id.cellid != SC_CELL_ALL && 
			ap->cookie_p->sc_id.cellid != client) {

		/* Not the specified cell */

		return TKS_CONTINUE;
	}

	if (ap->cookie_p->sc_status == SC_CONTINUE && 
			*(cell_t *)ap->cookie_p->sc_opaque != client) {

		/* Not the cell we left off at according to the cookie */

		return TKS_CONTINUE;
	}

	buflen = ap->buflen;
	if (!buflen && !(ap->flags & SGT_INFO)) {

		/* We ran out of buffer space before iterating across
		 * all cells. Set the cookie to indicate this and
		 * then stop. SGT_INFO doesn't use the buffer and uses
		 * buflen only as a flag to indicate a successful match.
		 * We don't stop iteration in this case.
		 */

		ap->error = 0;
		ap->cookie_p->sc_status = SC_CONTINUE;
		*(cell_t *)ap->cookie_p->sc_opaque = client;
		return TKS_STOP;
	}

	if (client == cellid()) {

		/* local cell */

		ap->error = phost_sysget(VHOST_BHV_FIRST(vhost_local()),
			ap->name, ap->buffer, &buflen, ap->flags, ap->cookie_p,
			&info);
	} else {
		/* REFERENCED */
		int		msgerr;

		if (!(ap->flags & SGT_UADDR)) {

			/* ap->buffer is a kernel buffer so we use the in/out
			 * buffers to move the data.
			 */

			kbuf = ap->buffer;
			kbuflen = buflen;
		}
		SERVICE_MAKE(svc, (cell_t)client, SVC_HOST);
		msgerr = invk_dchost_sysget(svc, ap->name, ap->buffer,
				&buflen, ap->flags, ap->cookie_p, &info,
				kbuf, kbuflen, &kbuf, &kbuflen, &ap->error);
		ASSERT(!msgerr);
	}

	if (ap->error) {
		return TKS_STOP;
	}

	if (!buflen) {

		/* No match was made on this cell. Try the next one */

		return TKS_CONTINUE;
	}

	if (ap->flags & SGT_INFO) {

		/* Add the info from the cell with the others */

		ap->info_p->si_size = info.si_size; 	/* can't change */
		ap->info_p->si_num += info.si_num;
		ap->info_p->si_hiwater += info.si_hiwater;
		if (ap->buflen) {

			/* We only decrement buflen once as a flag to 
			 * indicate at least one successful request. 
			 */

			ap->buflen -= buflen;
		}

		if (ap->flags & SGT_SUM) {

			/* Only one cell has to be asked since the values
			 * should be the same on all of them.
			 */

			return TKS_STOP;
		}
	}
	else if (!(ap->flags & SGT_SUM)) {

		/* increment the buffer settings for the next cell to use */

		ap->buffer += buflen;
		ap->buflen -= buflen;
	}

	if (ap->cookie_p->sc_status == SC_CONTINUE) {

		/* Client indicated that the buffer space was insufficient so
		 * the call could be completed. The cookie will indicate where
		 * it can be re-started.
		 */

		return TKS_STOP;
	}

	if (ap->cookie_p->sc_type != SC_CELLID && buflen) {
		/* We can stop because a single object was specified and a
		 * result was returned indicating the ID matched.
		 */
		return TKS_STOP;
	}

	if (ap->cookie_p->sc_type == SC_CELLID && 
			ap->cookie_p->sc_id.cellid != SC_CELL_ALL) {

		/* We can stop here because a single cell was specified */

		return TKS_STOP;
	}

	return TKS_CONTINUE;
}

PRIVATE int
iterate_sysget(
	dshost_t	*dsp,
	int		name,
	char		*buffer,
	int		*buflen_p,
	int		flags,
	sgt_cookie_t	*cookie_p,
	sgt_info_t	*info_p)
{
	/* REFERENCED */
	int		result;
	sysget_args_t	args;		/* iteration parameters */

	/*
	 * Specify iteration parameters:
	 */
	args.name	= name;
	args.buflen	= *buflen_p;
	args.flags	= flags;
	args.info_p	= info_p;

	HOST_TRACE8("iter_sysget", cellid(), "type", cookie_p->sc_type,
		"id", cookie_p->sc_id.cellid, "flags", flags);

	if (flags & SGT_INFO) {

		/* Initialize info since we may be accumulating results */

		bzero(info_p, sizeof(sgt_info_t));
	}
	else {
		args.buffer = buffer;
	}
	args.cookie_p	= cookie_p;
	args.error	= ESRCH;	/* default in case cookie is bad */

	/*
	 * Visit all registered cells. 
	 */

	result = tks_iterate(dsp->dsh_tserver, VHOST_MEMBER_TOKENSET,
		 TKS_STABLE, sysget_iterator, &args);
	ASSERT(result == TKS_CONTINUE || result == TKS_STOP);

	if (args.error) {

		/* Some error occurred while iterating */

		return(args.error);
	}

	if ((flags & SGT_SUM) && !(flags & SGT_INFO)) {

		/* Just return the buflen we started with */

		*buflen_p = args.buflen;
	}
	else {
		/* When the iterator returns the buflen argument will
		 * contain the number of bytes not-used in the buffer.
		 * We return the used-bytes count instead.
		 */

		*buflen_p -= args.buflen;
	}
	return(0);
}

/* ARGSUSED */
PRIVATE tks_iter_t
systune_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	int group_index, tune_index, *errorp;
	cell_t caller_cell;
	uint64_t value;
	service_t svc;

	if (!TK_IS_IN_SET(VHOST_MEMBER_TOKENSET, tokens_owned)) {

		/* Not a registered cell */

		return TKS_CONTINUE;
	}

	caller_cell = va_arg(args, cell_t);
	if (client == caller_cell) {

		/* Caller already took care of itself */

		return TKS_CONTINUE;
	}

	group_index = va_arg(args, int);
	tune_index = va_arg(args, int);
	value = va_arg(args, uint64_t);
	errorp = va_arg(args, int *);

	if (client == cellid()) {
		*errorp = phost_systune(VHOST_BHV_FIRST(vhost_local()),
			group_index, tune_index, value);
	} else {
		/* REFERENCED */
		int		msgerr;

		SERVICE_MAKE(svc, (cell_t)client, SVC_HOST);
		msgerr = invk_dchost_systune(svc, group_index, tune_index,
			 value, errorp);
		ASSERT(!msgerr);
	}
	if (*errorp) {
		return TKS_STOP;
	}
	return TKS_CONTINUE;
}

PRIVATE int
iterate_systune(
	dshost_t	*dsp,
	cell_t		caller_cell,
	int		group_index,
	int		tune_index,
	uint64_t	value)
{

	int result, error;

	/*
	 * Visit all registered cells. 
	 */

	result = tks_iterate(dsp->dsh_tserver, VHOST_MEMBER_TOKENSET,
		 TKS_STABLE, systune_iterator, caller_cell, group_index,
		 tune_index, value, &error);
	ASSERT(result == TKS_CONTINUE || result == TKS_STOP);
	return(error);
}

/* ARGSUSED */
PRIVATE tks_iter_t
sysacct_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	cell_t caller_cell;
	int enable;
	cfs_handle_t *handlep;
	service_t svc;

	if (!TK_IS_IN_SET(VHOST_MEMBER_TOKENSET, tokens_owned)) {

		/* Not a registered cell */

		return TKS_CONTINUE;
	}

	caller_cell = va_arg(args, cell_t);
	if (client == caller_cell) {

		/* Caller already took care of itself */

		return TKS_CONTINUE;
	}

	enable = va_arg(args, int);
	handlep = va_arg(args, cfs_handle_t *);

	if (client == cellid()) {
		phost_sysacct(VHOST_BHV_FIRST(vhost_local()), enable, handlep);
	} else {
		/* REFERENCED */
		int		msgerr;

		SERVICE_MAKE(svc, (cell_t)client, SVC_HOST);
		msgerr = invk_dchost_sysacct(svc, enable, handlep);
		ASSERT(!msgerr);
	}
	return TKS_CONTINUE;
}

PRIVATE void
iterate_sysacct(
	dshost_t	*dsp,
	cell_t		caller_cell,
	int		enable,
	cfs_handle_t	*handlep)
{
	int		result;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver, 
		 	VHOST_MEMBER_TOKENSET, TKS_STABLE, sysacct_iterator,
			caller_cell, enable, handlep);
	ASSERT(result == TKS_CONTINUE);
}

/*
 * Support for flushing client credid caches
 */
typedef struct {
	cell_t	server;
	int	count;
	sema_t	sema;
} credflush_args_t;

PRIVATE lock_t		 credflush_lock;
PRIVATE credflush_args_t *credflushargs;

PRIVATE void
credflush_init(void)
{
	int i;

	spinlock_init(&credflush_lock, "credflush");

	credflushargs = (credflush_args_t *)
		      kmem_alloc(MAX_CELLS*sizeof(credflush_args_t), KM_SLEEP);

	for (i = 0; i < MAX_CELLS; i++) {
		credflushargs[i].server = CELL_NONE;
		init_sema(&credflushargs[i].sema, 0, "creds", i);
	}
}

PRIVATE credflush_args_t *
credflush_lookup(cell_t server)
{
	int i;

	for (i = 0; i < MAX_CELLS; i++) {
		if (credflushargs[i].server == server)
			return (&credflushargs[i]);
	}
	return (NULL);
}
/*
 * Assign a credflushasrgs struct to a cell
 */
PRIVATE void
credflush_register(cell_t	server)
{
	int i;
	int s;

	s = mutex_spinlock(&credflush_lock);
	for (i = 0; i < MAX_CELLS; i++) {
		if (credflushargs[i].server == CELL_NONE) {

			credflushargs[i].server = server;
			mutex_spinunlock(&credflush_lock, s);
			return;
		}
	}
	mutex_spinunlock(&credflush_lock, s);
	ASSERT(0);
}

PRIVATE void
credflush_deregister(cell_t server)
{
	credflush_args_t *args = credflush_lookup(server);

	ASSERT(args);

	args->server = CELL_NONE;
}

PRIVATE void
client_credflush(
	service_t	svc,
	void	 	*param)
{
	credflush_args_t *ra = (credflush_args_t *) param;

	atomicAddInt(&ra->count, 1);

	if (SERVICE_TO_CELL(svc) == cellid()) {
		/*
		 * do credflush after iterate is complete to enhance
		 * parallelism
		 */
		return;
	} else {
		/* REFERENCED */
		int		msgerr;
		msgerr = invk_dchost_credflush(svc, cellid(), ra->server, ra);
		ASSERT(!msgerr);
	}
}

PRIVATE void
iterate_credflush(
	dshost_t	*dsp,
	cell_t		caller_cell,
	cell_t		server)
{
	/* REFERENCED */
	int		result;
	iterator_t	it;		/* iteration parameters */
	credflush_args_t *ra;		/* sync-specific args */
	/*
	 * Specify parameter for remote sync RPC
	 */
	ASSERT(credflushargs);

	ra		= credflush_lookup(server);

	ASSERT(ra);

	ra->count	= 0;
	/*
	 * Specify iteration parameters:
	 */
	it.fn		= client_credflush;
	it.fn_params	= (void *) ra;
	it.caller_cell  = caller_cell;
	it.tokens	= VHOST_CRED_TOKENSET;
	it.caller_skipped = 0;

	/*
	 * Visit all registered cells.
	 */
	result = tks_iterate(dsp->dsh_tserver, 
		 	it.tokens, TKS_STABLE, client_iterator, &it);
	ASSERT(result == TKS_CONTINUE);

	if (it.caller_skipped) {
		phost_credflush(VHOST_BHV_FIRST(vhost_local()), server);

		if (atomicAddInt(&ra->count, -1) == 0)
			(void)vsema(&ra->sema);
	}
}

PRIVATE void
credflush_wait(
	cell_t		server)
{
	credflush_args_t *args = credflush_lookup(server);
	ASSERT(args);
	(void)psema(&args->sema, PZERO);
}

/************************************************************************
 *	Message interface from clients.					*
 ************************************************************************/


/* ARGSUSED */
void
I_dshost_reboot(
	objid_t		objid,
	cell_t		caller_cell,
	int		fnc,
	char		*mdep)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("dshost_reboot", cellid(), "fnc", fnc);

	dshost_reboot(bdp, fnc, mdep);
}

void
I_dshost_killall(
	objid_t		objid,
	cell_t		caller_cell,
	int		signo,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid,
	int		*caller_included)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE10("dchost_killall", cellid(), "signo", signo, "pgid", pgid,
		     "callers_pid", callers_pid, "callers_sid", callers_sid);

	iterate_killall(BHV_TO_DSHOST(bdp), caller_cell,
		        signo, pgid, callers_pid, callers_sid,
			caller_included);
}

void
I_dshost_sync(
	objid_t		objid,
	cell_t		caller_cell,
	int		flags,
	int		*caller_included)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("dshost_sync", cellid(), "flags", flags);

	iterate_sync(BHV_TO_DSHOST(bdp), caller_cell, flags, caller_included);
}

void
I_dshost_set_time(
	objid_t		objid,
	cell_t		caller_cell,
	cell_t		source_cell,
	int		*caller_included)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("dshost_set_time", cellid(), "source", source_cell);

	iterate_set_time(BHV_TO_DSHOST(bdp), caller_cell, source_cell, caller_included);
}

void
I_dshost_adj_time(
	objid_t		objid,
	cell_t		caller_cell,
	long		adjustment,
	int		*caller_included)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	long		odelta;
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("dshost_adj_time", cellid(), "adjustment", adjustment);

	iterate_adj_time(BHV_TO_DSHOST(bdp), caller_cell, adjustment, &odelta, 
		caller_included);
}

/* ARGSUSED */
void
I_dshost_sysget(
	objid_t		objid,
	int		name,
	char		*buffer,
	int		*buflen_p,
	int		flags,
	sgt_cookie_t	*cookie_p,
	sgt_info_t	*info_p,
	char		*in_buf,
	size_t		in_buflen,
	char		**out_buf,
	size_t		*out_buflen,
	int		*error_p,
	void		**bufdesc)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("I_dshost_sysget", cellid(), "flags", flags);

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
	*error_p = iterate_sysget(BHV_TO_DSHOST(bdp), name, buffer, buflen_p,
		 flags, cookie_p, info_p);
}

/* ARGSUSED */
void
I_dshost_sysget_done(
        char	*out_buf,
        size_t  out_buf_count,
        void	*bufdesc)
{
	if (out_buf) {
		kmem_free(out_buf, out_buf_count);
	}
}

void
I_dshost_systune(
	objid_t		objid,
	cell_t		caller_cell,
	int		group_index,
	int		tune_index,
	uint64_t	value,
	int		*errorp)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("I_dshost_systune", cellid(), "index", tune_index);

	*errorp = iterate_systune(BHV_TO_DSHOST(bdp), caller_cell, group_index,
		tune_index, value);    
}

void
I_dshost_credflush(
	objid_t		objid,
	cell_t		caller_cell,
	cell_t		server)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("I_dshost_credflush", cellid(), "server", server);

	iterate_credflush(BHV_TO_DSHOST(bdp), caller_cell,  server);
}

void
I_dshost_credflush_wait(
	objid_t		objid,
	cell_t		server)
{
	/* REFERENCED */
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("I_dshost_credflush", cellid(), "server", server);

	credflush_wait(server);
}

/* ARGSUSED */
void
I_dshost_credflush_ack(
	cell_t		responder,
	void		*arg)
{
	credflush_args_t *ra = (credflush_args_t *)arg;

	if (atomicAddInt(&ra->count, -1) == 0)
		(void)vsema(&ra->sema);
}

void
I_dshost_sysacct(
	objid_t		objid,
	cell_t		caller_cell,
	int		enable,
	cfs_handle_t	*handlep)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	ASSERT(VHOST_BHV_FIRST(vhost_local()) == bdp);

	HOST_TRACE4("dshost_sysacct", cellid(), "enable", enable);
	iterate_sysacct(BHV_TO_DSHOST(bdp), caller_cell, enable, handlep);
}

/************************************************************************
 *	Server-side operations						*
 ************************************************************************/

/* ARGSUSED */
PRIVATE void
dshost_register(
	bhv_desc_t	*bdp,
	int		vh_svc_mask)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	tk_set_t	wanted, acquired;

	HOST_TRACE4("dshost_register", cellid(), "vh_svc_mask", vh_svc_mask);

	wanted = vh_svc_to_tokenset(vh_svc_mask);
	tkc_acquire(dsp->dsh_tclient, wanted, &acquired);
	ASSERT(acquired == wanted);
}

/* ARGSUSED */
PRIVATE void
dshost_deregister(
	bhv_desc_t	*bdp,
	int		vh_svc_mask)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	tk_set_t	to_release = vh_svc_to_tokenset(vh_svc_mask);
	tk_set_t	to_return;
	tk_disp_t	why;

	HOST_TRACE4("dshost_deregister", cellid(), "vh_svc_mask", vh_svc_mask);

	tkc_release(dsp->dsh_tclient, to_release);
	/*
	 * Return cached tokens immediately.
	 */
	tkc_returning(dsp->dsh_tclient, to_release, &to_return, &why, 0);
	if (to_return != TK_NULLSET) {
		tks_return(dsp->dsh_tserver, cellid(),
			   to_return, TK_NULLSET, TK_NULLSET, why);
		tkc_returned(dsp->dsh_tclient, to_return, TK_NULLSET);
	}

}

/* ARGSUSED */
PRIVATE void
dshost_sethostid(
	bhv_desc_t	*bdp,
	int		id)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE4("dshost_sethostid", cellid(), "id", id);

	tkc_acquire1(dsp->dsh_tclient, VHOST_HOSTID_WRITE_TOKEN);
	phost_sethostid(BHV_NEXT(bdp), id);
	tkc_release1(dsp->dsh_tclient, VHOST_HOSTID_WRITE_TOKEN);
}

/* ARGSUSED */
PRIVATE void
dshost_gethostid(
	bhv_desc_t	*bdp,
	int		*id)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE4("dshost_gethostid", cellid(), "id", id);

	tkc_acquire1(dsp->dsh_tclient, VHOST_HOSTID_READ_TOKEN);
	phost_gethostid(BHV_NEXT(bdp), id);
	tkc_release1(dsp->dsh_tclient, VHOST_HOSTID_READ_TOKEN);
}

/* ARGSUSED */
PRIVATE void
dshost_sethostname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE6("dshost_sethostname", cellid(), "name", name, "*len", *len);

	tkc_acquire1(dsp->dsh_tclient, VHOST_HOSTNAME_WRITE_TOKEN);
	phost_sethostname(BHV_NEXT(bdp), name, len);
	tkc_release1(dsp->dsh_tclient, VHOST_HOSTNAME_WRITE_TOKEN);
}

/* ARGSUSED */
PRIVATE void
dshost_gethostname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE6("dshost_gethostname", cellid(), "name", name, "*len", *len);

	tkc_acquire1(dsp->dsh_tclient, VHOST_HOSTNAME_READ_TOKEN);
	phost_gethostname(BHV_NEXT(bdp), name, len);
	tkc_release1(dsp->dsh_tclient, VHOST_HOSTNAME_READ_TOKEN);
}

/* ARGSUSED */
PRIVATE void
dshost_setdomainname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		len)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE6("dshost_setdomainname", cellid(), "name", name, "len", len);

	tkc_acquire1(dsp->dsh_tclient, VHOST_DOMAINNAME_WRITE_TOKEN);
	phost_setdomainname(BHV_NEXT(bdp), name, len);
	tkc_release1(dsp->dsh_tclient, VHOST_DOMAINNAME_WRITE_TOKEN);
}

/* ARGSUSED */
PRIVATE void
dshost_getdomainname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE6("dshost_getdomainname", cellid(), "name", name, "*len", *len);

	tkc_acquire1(dsp->dsh_tclient, VHOST_DOMAINNAME_READ_TOKEN);
	phost_getdomainname(BHV_NEXT(bdp), name, len);
	tkc_release1(dsp->dsh_tclient, VHOST_DOMAINNAME_READ_TOKEN);
}

/*
 * The following ops iterate over clients cells holding tokens
 * associated with a host service. The calling cell, if holding
 * the token, maybe skipped during iteration but is requested
 * explicitly to invoke the physical operation directly. This
 * avoids an extra roundtrip message.
 */
/* ARGSUSED */
PRIVATE void
dshost_killall(
	bhv_desc_t	*bdp,
	int		signo,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int		me_too;

	HOST_TRACE10("dshost_killall", cellid(), "signo", signo, "pgid", pgid,
		     "callers_pid", callers_pid, "callers_sid", callers_sid);
		 
	/* iterate over all registered cells */
	iterate_killall(dsp, CELL_NONE,
		        signo, pgid, callers_pid, callers_sid,
			&me_too);
}

/* ARGSUSED */
PRIVATE void
dshost_reboot(
	bhv_desc_t	*bdp,
	int		fcn,
	char		*mdep)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int		me_too;

	HOST_TRACE4("dshost_reboot", cellid(), "fcn", fcn);

	/* iterate over all registered cells except the local cell */
	iterate_reboot(dsp, cellid(), fcn, mdep, &me_too);

	/* finally me too */
	ASSERT(me_too);
	if (me_too)
		phost_reboot(BHV_NEXT(bdp), fcn, mdep);
}

/* ARGSUSED */
PRIVATE void
dshost_sync(
	bhv_desc_t	*bdp,
	int		flags)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int		me_too;

	HOST_TRACE4("dshost_sync", cellid(), "flags", flags);

	/* iterate over all registered cells */
	iterate_sync(dsp, CELL_NONE, flags, &me_too);
}

/* ARGSUSED */
PRIVATE void
dshost_set_time(
	bhv_desc_t	*bdp,
	cell_t		source_cell)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int		me_too;

	HOST_TRACE2("dshost_set_time", cellid());

	/* iterate over all registered cells */
	iterate_set_time(dsp, cellid(), source_cell, &me_too);
}

/* ARGSUSED */
PRIVATE void
dshost_adj_time(
	bhv_desc_t	*bdp,
	long		adjustment,
	long		*odelta)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int		me_too;

	HOST_TRACE2("dshost_adj_time", cellid());

	/* iterate over all registered cells */
	iterate_adj_time(dsp, CELL_NONE, adjustment, odelta, &me_too);
}

/* ARGSUSED */
PRIVATE int
dshost_sysget(
	bhv_desc_t	*bdp,
	int		name,
	char		*buffer,
	int		*buflen_p,
	int		flags,
	sgt_cookie_t	*cookie_p,
	sgt_info_t	*info_p)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int		error;

	HOST_TRACE6("dshost_sysget", cellid(), "name", name, "flags", flags);

	/* iterate over all registered cells */

	error = iterate_sysget(dsp, name, buffer, buflen_p, flags, cookie_p,
		info_p); 
	return(error);
}

/* ARGSUSED */
PRIVATE int
dshost_systune(
	bhv_desc_t	*bdp,
	int		group_index,
	int		tune_index,
	uint64_t	value)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);
	int error;

	HOST_TRACE4("dshost_systune", cellid(), "index", tune_index);

	/* iterate over all registered cells */

	error = iterate_systune(dsp, cellid(), group_index, tune_index, value);
	return(error);
}

/* ARGSUSED */
PRIVATE void
dshost_credflush(
	bhv_desc_t	*bdp,
	cell_t		server)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE4("dshost_credflush", cellid(), "server", server);

	/* iterate over all registered cells */

	iterate_credflush(dsp, CELL_NONE, server);
}

/* ARGSUSED */
PRIVATE void
dshost_credflush_wait(
	bhv_desc_t	*bdp,
	cell_t		server)
{
	HOST_TRACE4("dshost_credflush_wait", cellid(), "server", server);

	/* iterate over all registered cells */

	credflush_wait(server);
}

/* ARGSUSED */
PRIVATE void
dshost_sysacct(
	bhv_desc_t	*bdp,
	int		enable,
	cfs_handle_t	*handlep)
{
	dshost_t	*dsp = BHV_TO_DSHOST(bdp);

	HOST_TRACE4("dshost_sysacct", cellid(), "enable", enable);

	/* iterate over all registered cells */

	iterate_sysacct(dsp, cellid(), enable, handlep);
}

void
dshost_recovery(cell_t cell)
{
	bhv_desc_t      *bdp;
	dshost_t	*dsp;
	vhost_t		*vhost;

	vhost = vhost_local();
	bdp = VHOST_BHV_FIRST(vhost);
	if (BHV_POSITION(bdp) == VHOST_BHV_PP)
		return;
	dsp = BHV_TO_DSHOST(bdp);
	tks_state(dsp->dsh_tserver, cell, dshost_recovery_cell);
}

/* ARGSUSED */
void
dshost_recovery_cell(
	void 	*obj,
	tks_ch_t client,
	tk_set_t grants,
	tk_set_t recalls)
{
	dshost_t        *dsp = (dshost_t *)obj;
	tk_disp_t	why;

	tks_make_dispall(grants, TK_CLIENT_INITIATED, &why);
	tks_return(dsp->dsh_tserver, client, grants, TK_NULLSET, 
			TK_NULLSET, why);
}


#ifdef DEBUG
void
idbg_dshost_print(
	dshost_t	*dsp)
{
	qprintf("dshost 0x%x:\n", dsp);
	qprintf("    token state - client 0x%x server 0x%x\n",
		dsp->dsh_tclient, dsp->dsh_tserver);
	qprintf("    bhv 0x%x\n", &dsp->dsh_bhv);
	idbg_phost((__psint_t)BHV_PDATA(BHV_NEXT(&dsp->dsh_bhv)));
}
#endif
