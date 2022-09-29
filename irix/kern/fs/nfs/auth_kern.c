/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.23 88/02/08 
 */


/*
 * auth_kern.c, Implements UNIX style authentication parameters in the kernel. 
 *
 * Interfaces with svc_auth_unix on the server.  See auth_unix.c for the user
 * level implementation of unix auth.
 *
 */

#include "types.h"
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include "auth.h"
#include "auth_unix.h"
#include "xdr.h"
#include "clnt.h"
#include "rpc_msg.h"

/*
 * Unix authenticator operations vector
 */
void	authkern_nextverf();
bool_t	authkern_marshal();
bool_t	authkern_validate();
bool_t	authkern_refresh();
void	authkern_destroy();

static struct auth_ops auth_kern_ops = {
	authkern_nextverf,
	authkern_marshal,
	authkern_validate,
	authkern_refresh,
	authkern_destroy
};


/*
 * Create a kernel unix style authenticator.
 * Returns an auth handle.
 */
AUTH *
authkern_create()
{
	register AUTH *auth;

	/*
	 * Allocate and set up auth handle
	 */
	auth = kmem_alloc(sizeof(*auth), KM_SLEEP);
	auth->ah_ops = &auth_kern_ops;
	auth->ah_cred.oa_flavor = AUTH_UNIX;
	auth->ah_verf = _null_auth;
	return (auth);
}

/*
 * authkern operations
 */

/*ARGSUSED*/
void
authkern_nextverf(auth)
	AUTH *auth;
{
	/* no action necessary */
}

bool_t
authkern_marshal(auth, xdrs)
	AUTH *auth;
	XDR *xdrs;
{
	char	*sercred;
	XDR	xdrm;
	struct	opaque_auth *cred;
	bool_t	ret = FALSE;
	register gid_t *gp, *gpend;
	register int gidlen, credsize;
	register xdr_long_t *ptr;
	cred_t *crp;
	extern int rpc_ngroups_max;

	/*
	 * First we try a fast path to get through
	 * this very common operation.
	 */
	crp = get_current_cred();
	gp = crp->cr_groups;
	gpend = &crp->cr_groups[MIN(crp->cr_ngroups, rpc_ngroups_max)];
	gidlen = gpend - gp;
	credsize = 4 + 4 + roundup(hostnamelen, 4) + 4 + 4 + 4 + gidlen * 4;
	ptr = XDR_INLINE(xdrs, 4 + 4 + credsize + 4 + 4);
	if (ptr) {
		/*
		 * We can do the fast path.
		 */
		IXDR_PUT_LONG(ptr, AUTH_UNIX);	/* cred flavor */
		IXDR_PUT_LONG(ptr, credsize);	/* cred len */
		IXDR_PUT_LONG(ptr, time);	/* time in seconds */
		IXDR_PUT_LONG(ptr, hostnamelen);
		bcopy(hostname, ptr, hostnamelen);
		ptr += roundup(hostnamelen, 4) / 4;
		IXDR_PUT_LONG(ptr, (int)crp->cr_uid);
		IXDR_PUT_LONG(ptr, (int)crp->cr_gid);
		IXDR_PUT_LONG(ptr, gidlen);
		while (gp < gpend) {
			IXDR_PUT_LONG(ptr, (int) *gp++);
		}
		IXDR_PUT_LONG(ptr, AUTH_NULL);	/* verf flavor */
		IXDR_PUT_LONG(ptr, 0);	/* verf len */
		return (TRUE);
	}
	sercred = kmem_alloc(MAX_AUTH_BYTES, KM_SLEEP);
	/*
	 * serialize u struct stuff into sercred
	 */
	xdrmem_create(&xdrm, sercred, MAX_AUTH_BYTES, XDR_ENCODE);
	if (! xdr_authkern(&xdrm)) {
		printf("authkern_marshal: xdr_authkern failed\n");
		ret = FALSE;
		goto done;
	}

	/*
	 * Make opaque auth credentials that point at serialized u struct
	 */
	cred = &(auth->ah_cred);
	cred->oa_length = XDR_GETPOS(&xdrm);
	cred->oa_base = sercred;

	/*
	 * serialize credentials and verifiers (null)
	 */
	if ((xdr_opaque_auth(xdrs, &(auth->ah_cred)))
	    && (xdr_opaque_auth(xdrs, &(auth->ah_verf)))) {
		ret = TRUE;
	} else {
		ret = FALSE;
	}
done:
	kmem_free(sercred, MAX_AUTH_BYTES);
	return (ret);
}

/*ARGSUSED*/
bool_t
authkern_validate(auth, verf)
	AUTH *auth;
	struct opaque_auth verf;
{

	return (TRUE);
}

/*ARGSUSED*/
bool_t
authkern_refresh(auth)
	AUTH *auth;
{
	return (FALSE);
}

void
authkern_destroy(auth)
	register AUTH *auth;
{

	kmem_free(auth, sizeof(*auth));
}
