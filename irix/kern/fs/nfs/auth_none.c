/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.20 88/02/08
 */


/*
 * auth_none.c
 * Creates a client authentication handle for passing "null" 
 * credentials and verifiers to remote systems. 
 */

#ifdef _KERNEL
#include "types.h"	/* <> */
#include "xdr.h"	/* <> */
#include "auth.h"	/* <> */
#include "clnt.h"
#include "rpc_msg.h"
#else
#ifdef __STDC__
	#pragma weak authnone_create = _authnone_create
#endif
#include "synonyms.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>		/* prototype for xdr_opaque_auth() */
#include <rpc/rpc_msg.h>
#include <pthread.h>
#include <mplib.h>
#endif
#define MAX_MARSHEL_SIZE 20

#ifdef _KERNEL
#define _INITBSS
#define _INITBSSS
#endif

/*
 * Authenticator operations routines
 */
static void	authnone_verf();
static void	authnone_destroy();
static bool_t	authnone_marshal();
static bool_t	authnone_validate();
static bool_t	authnone_refresh();

static struct auth_ops ops = {
	authnone_verf,
	authnone_marshal,
	authnone_validate,
	authnone_refresh,
	authnone_destroy
};

static struct authnone_private {
	AUTH	no_client;
	char	marshalled_client[MAX_MARSHEL_SIZE];
	u_int	mcnt;
} *authnone_private _INITBSS;

AUTH *
authnone_create()
{
	register struct authnone_private *ap;
	XDR xdr_stream;
	register XDR *xdrs;
	extern pthread_mutex_t authnone_lock;

#ifndef _KERNEL
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &authnone_lock) );  
#endif
	ap = authnone_private;
	if (ap == 0) {
#ifdef _KERNEL
		ap = kmem_zalloc(sizeof (*ap), KM_SLEEP);
#else
		ap = (struct authnone_private *)calloc(1, sizeof (*ap));
#endif
		if (ap == 0) {
#ifndef _KERNEL
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &authnone_lock) );
#endif
			return (0);
		}
		authnone_private = ap;
	}
	if (!ap->mcnt) {
		ap->no_client.ah_cred = ap->no_client.ah_verf = _null_auth;
		ap->no_client.ah_ops = &ops;
		xdrs = &xdr_stream;
		xdrmem_create(xdrs, ap->marshalled_client, (u_int)MAX_MARSHEL_SIZE,
		    XDR_ENCODE);
		(void)xdr_opaque_auth(xdrs, &ap->no_client.ah_cred);
		(void)xdr_opaque_auth(xdrs, &ap->no_client.ah_verf);
		ap->mcnt = XDR_GETPOS(xdrs);
		XDR_DESTROY(xdrs);
	}
#ifndef _KERNEL
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &authnone_lock) );  
#endif
	return (&ap->no_client);
}

/*ARGSUSED*/
static bool_t
authnone_marshal(client, xdrs)
	AUTH *client;
	XDR *xdrs;
{
	register struct authnone_private *ap;
	bool_t dummy;
	extern pthread_mutex_t authnone_lock;

#ifndef _KERNEL
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &authnone_lock) );  
#endif
	ap = authnone_private;
	if (ap == 0) {
#ifndef _KERNEL
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &authnone_lock) ); 
#endif
		return (0);
	}
	dummy = (*xdrs->x_ops->x_putbytes)(xdrs,
			ap->marshalled_client, ap->mcnt);
#ifndef _KERNEL
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &authnone_lock) ); 
#endif
	return(dummy);
}

static void 
authnone_verf()
{
}

static bool_t
authnone_validate()
{

	return (TRUE);
}

static bool_t
authnone_refresh()
{

	return (FALSE);
}

static void
authnone_destroy()
{
}
