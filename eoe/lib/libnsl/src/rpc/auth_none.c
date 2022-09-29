/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:auth_none.c	1.2.4.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
/*
 * auth_none.c
 * Creates a client authentication handle for passing "null"
 * credentials and verifiers to remote systems.
 */
#if defined(__STDC__) 
        #pragma weak authnone_create	= _authnone_create
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#define	MAX_MARSHEL_SIZE 20

static struct auth_ops *authnone_ops();

static struct authnone_private {
	AUTH	no_client;
	char	marshalled_client[MAX_MARSHEL_SIZE];
	u_int	mcnt;
} *authnone_private;


AUTH *
authnone_create()
{
	register struct authnone_private *ap = authnone_private;
	XDR xdr_stream;
	register XDR *xdrs;

	if (ap == NULL) {
		ap = (struct authnone_private *)calloc(1, sizeof (*ap));
		if (ap == NULL)
			return ((AUTH *)NULL);
		authnone_private = ap;
	}
	if (!ap->mcnt) {
		ap->no_client.ah_cred = ap->no_client.ah_verf = _null_auth;
		ap->no_client.ah_ops = authnone_ops();
		xdrs = &xdr_stream;
		xdrmem_create(xdrs, ap->marshalled_client,
			(u_int)MAX_MARSHEL_SIZE, XDR_ENCODE);
		(void)xdr_opaque_auth(xdrs, &ap->no_client.ah_cred);
		(void)xdr_opaque_auth(xdrs, &ap->no_client.ah_verf);
		ap->mcnt = XDR_GETPOS(xdrs);
		XDR_DESTROY(xdrs);
	}
	return (&ap->no_client);
}

/*ARGSUSED*/
static bool_t
authnone_marshal(client, xdrs)
	AUTH *client;
	XDR *xdrs;
{
	register struct authnone_private *ap = authnone_private;

	if (ap == NULL)
		return (FALSE);
	return ((*xdrs->x_ops->x_putbytes)(xdrs,
			ap->marshalled_client, ap->mcnt));
}

/*ARGSUSED*/
static void
authnone_verf(AUTH *a)
{
}

/*ARGSUSED*/
static bool_t
authnone_validate(AUTH *a, struct opaque_auth *b)
{
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
authnone_refresh(AUTH *a)
{
	return (FALSE);
}

/*ARGSUSED*/
static void
authnone_destroy(AUTH *a)
{
}

static struct auth_ops *
authnone_ops()
{
	static struct auth_ops ops;

	if (ops.ah_nextverf == NULL) {
		ops.ah_nextverf = authnone_verf;
		ops.ah_marshal = authnone_marshal;
		ops.ah_validate = authnone_validate;
		ops.ah_refresh = authnone_refresh;
		ops.ah_destroy = authnone_destroy;
	}
	return (&ops);
}
