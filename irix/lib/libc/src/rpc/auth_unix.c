/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.20 88/02/08 
 */


/*
 * auth_unix.c, Implements UNIX style authentication parameters. 
 *
 * The system is very weak.  The client uses no encryption for its
 * credentials and only sends null verifiers.  The server sends back
 * null verifiers or optionally a verifier that suggests a new short hand
 * for the credentials.
 *
 */

#ifdef __STDC__
	#pragma weak authunix_create = _authunix_create
	#pragma weak authunix_create_default = _authunix_create_default
#endif
#include "synonyms.h"
#include <bstring.h>
#include <stdio.h>
#include <sys/time.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/errorhandler.h>
#include <rpc/clnt.h>		/* prototype for xdr_opaque_auth() */
#include <rpc/rpc_msg.h>	/* prototype for xdr_opaque_auth() */
#include <sys/types.h>
#include <unistd.h>		/* prototype for gethostname() */
#include <string.h>		/* prototype for strerror() */
#include <errno.h>


/*
 * Unix authenticator operations vector
 */
static void	authunix_nextverf();
static bool_t	authunix_marshal();
static bool_t	authunix_validate();
static bool_t	authunix_refresh();
static void	authunix_destroy();

/*
 * This struct is pointed to by the ah_private field of an auth_handle.
 */
struct audata {
	struct opaque_auth	au_origcred;	/* original credentials */
	struct opaque_auth	au_shcred;	/* short hand cred */
	u_long			au_shfaults;	/* short hand cache faults */
	char			au_marshed[MAX_AUTH_BYTES];
	u_int			au_mpos;	/* xdr pos at end of marshed */
};
#define	AUTH_PRIVATE(auth)	((struct audata *)auth->ah_private)

static void marshal_new_auth(AUTH *);


static struct auth_ops auth_unix_ops = {
	authunix_nextverf,
	authunix_marshal,
	authunix_validate,
	authunix_refresh,
	authunix_destroy
};

/*
 * Create a unix style authenticator.
 * Returns an auth handle with the given stuff in it.
 */
AUTH *
authunix_create(
	const char *machname,
	uid_t uid,
	gid_t gid,
	register int len,
	gid_t *aup_gids)
{
	struct authunix_parms aup;
	char mymem[MAX_AUTH_BYTES];
	struct timeval now;
	XDR xdrs;
	register AUTH *auth;
	register struct audata *au;

	/*
	 * Allocate and set up auth handle
	 */
	auth = (AUTH *)mem_alloc(sizeof(*auth));
	if (auth == NULL) {
		_rpc_errorhandler(LOG_ERR, "authunix_create: out of memory");
		return (NULL);
	}
	au = (struct audata *)mem_alloc(sizeof(*au));
	if (au == NULL) {
		_rpc_errorhandler(LOG_ERR, "authunix_create: out of memory");
		(void) mem_free((char *) auth, sizeof(*auth));
		return (NULL);
	}
	auth->ah_ops = &auth_unix_ops;
	auth->ah_private = (caddr_t)au;
	auth->ah_verf = au->au_shcred = _null_auth;
	au->au_shfaults = 0;

	/*
	 * fill in param struct from the given params
	 */
	(void)gettimeofday(&now, 0);
	aup.aup_time = (unsigned long)now.tv_sec;
	aup.aup_machname = (char *)machname;
	aup.aup_uid = uid;
	aup.aup_gid = gid;
	aup.aup_len = (u_int)len;
	aup.aup_gids = aup_gids;

	/*
	 * Serialize the parameters into origcred
	 */
	xdrmem_create(&xdrs, mymem, MAX_AUTH_BYTES, XDR_ENCODE);
	if (! xdr_authunix_parms(&xdrs, &aup)) {
		_rpc_errorhandler(LOG_ERR, "authunix_create: xdr_authunix_parms failed");
		(void) mem_free((char *) au, sizeof(*au));
		(void) mem_free((char *) auth, sizeof(*auth));
		return NULL;
	}
	au->au_origcred.oa_length = XDR_GETPOS(&xdrs);
	len = (int)XDR_GETPOS(&xdrs);
	au->au_origcred.oa_flavor = AUTH_UNIX;
	if ((au->au_origcred.oa_base = mem_alloc((u_int) len)) == NULL) {
		_rpc_errorhandler(LOG_ERR, "authunix_create: out of memory");
		(void) mem_free((char *) au, sizeof(*au));
		(void) mem_free((char *) auth, sizeof(*auth));
		return (NULL);
	}
	bcopy(mymem, au->au_origcred.oa_base, len);

	/*
	 * set auth handle to reflect new cred.
	 */
	auth->ah_cred = au->au_origcred;
	marshal_new_auth(auth);
	return (auth);
}

/*
 * Returns an auth handle with parameters determined by doing lots of
 * syscalls.
 */
AUTH *
authunix_create_default()
{
	register int len;
	char machname[MAX_MACHINE_NAME + 1];
	register uid_t uid;
	register gid_t gid;
	gid_t gids[NGRPS];

	if (gethostname(machname, MAX_MACHINE_NAME) < 0) {
		_rpc_errorhandler(LOG_ERR, "authunix_create_default:  gethostname failed:  %s", strerror(errno));
		bzero(machname, MAX_MACHINE_NAME);
	}
	machname[MAX_MACHINE_NAME] = 0;
	uid = geteuid();
	gid = getegid();
 	if ((len = BSDgetgroups(NGRPS, (int *)gids)) < 0) {
		_rpc_errorhandler(LOG_ERR, "authunix_create_default:  getgroups failed:  %s", strerror(errno));
		len = 0;
	}
	return (authunix_create(machname, uid, gid, len, gids));
}

/*
 * authunix operations
 */
/* ARGSUSED */
static void
authunix_nextverf(auth)
	AUTH *auth;
{
	/* no action necessary */
}

static bool_t
authunix_marshal(auth, xdrs)
	AUTH *auth;
	XDR *xdrs;
{
	register struct audata *au = AUTH_PRIVATE(auth);

	return (XDR_PUTBYTES(xdrs, au->au_marshed, au->au_mpos));
}

static bool_t
authunix_validate(auth, verf)
	register AUTH *auth;
	struct opaque_auth verf;
{
	register struct audata *au;
	XDR xdrs;

	if (verf.oa_flavor == AUTH_SHORT) {
		au = AUTH_PRIVATE(auth);
		xdrmem_create(&xdrs, verf.oa_base, verf.oa_length, XDR_DECODE);

		if (au->au_shcred.oa_base != NULL) {
			mem_free(au->au_shcred.oa_base,
			    au->au_shcred.oa_length);
			au->au_shcred.oa_base = NULL;
		}
		if (xdr_opaque_auth(&xdrs, &au->au_shcred)) {
			auth->ah_cred = au->au_shcred;
		} else {
			xdrs.x_op = XDR_FREE;
			(void)xdr_opaque_auth(&xdrs, &au->au_shcred);
			au->au_shcred.oa_base = NULL;
			auth->ah_cred = au->au_origcred;
		}
		marshal_new_auth(auth);
	}
	return (TRUE);
}

static bool_t
authunix_refresh(auth)
	register AUTH *auth;
{
	register struct audata *au = AUTH_PRIVATE(auth);
	struct authunix_parms aup;
	struct timeval now;
	XDR xdrs;
	register int stat;

	if (auth->ah_cred.oa_base == au->au_origcred.oa_base) {
		/* there is no hope.  Punt */
		return (FALSE);
	}
	au->au_shfaults ++;

	/* first deserialize the creds back into a struct authunix_parms */
	aup.aup_machname = NULL;
	aup.aup_gids = (gid_t *)NULL;
	xdrmem_create(&xdrs, au->au_origcred.oa_base,
	    au->au_origcred.oa_length, XDR_DECODE);
	stat = xdr_authunix_parms(&xdrs, &aup);
	if (! stat) 
		goto done;

	/* update the time and serialize in place */
	(void)gettimeofday(&now, 0);
	aup.aup_time = (unsigned long)now.tv_sec;
	xdrs.x_op = XDR_ENCODE;
	XDR_SETPOS(&xdrs, 0);
	stat = xdr_authunix_parms(&xdrs, &aup);
	if (! stat)
		goto done;
	auth->ah_cred = au->au_origcred;
	marshal_new_auth(auth);
done:
	/* free the struct authunix_parms created by deserializing */
	xdrs.x_op = XDR_FREE;
	(void)xdr_authunix_parms(&xdrs, &aup);
	XDR_DESTROY(&xdrs);
	return (stat);
}

static void
authunix_destroy(auth)
	register AUTH *auth;
{
	register struct audata *au = AUTH_PRIVATE(auth);

	mem_free(au->au_origcred.oa_base, au->au_origcred.oa_length);

	if (au->au_shcred.oa_base != NULL)
		mem_free(au->au_shcred.oa_base, au->au_shcred.oa_length);

	mem_free(auth->ah_private, sizeof(struct audata));

	if (auth->ah_verf.oa_base != NULL)
		mem_free(auth->ah_verf.oa_base, auth->ah_verf.oa_length);

	mem_free((caddr_t)auth, sizeof(*auth));

}

/*
 * Marshals (pre-serializes) an auth struct.
 * sets private data, au_marshed and au_mpos
 */
static void
marshal_new_auth(auth)
	register AUTH *auth;
{
	XDR		xdr_stream;
	register XDR	*xdrs = &xdr_stream;
	register struct audata *au = AUTH_PRIVATE(auth);

	xdrmem_create(xdrs, au->au_marshed, MAX_AUTH_BYTES, XDR_ENCODE);
	if ((! xdr_opaque_auth(xdrs, &(auth->ah_cred))) ||
	    (! xdr_opaque_auth(xdrs, &(auth->ah_verf)))) {
		_rpc_errorhandler(LOG_ERR, "marshal_new_auth - Fatal marshalling problem");
	} else {
		au->au_mpos = XDR_GETPOS(xdrs);
	}
	XDR_DESTROY(xdrs);
}
