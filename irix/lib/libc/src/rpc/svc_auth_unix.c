/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.28 88/02/08 
 */


/*
 * svc_auth_unix.c
 * Handles UNIX flavor authentication parameters on the service side of rpc.
 * There are two svc auth implementations here: AUTH_UNIX and AUTH_SHORT.
 * _svcauth_unix does full blown unix style uid,gid+gids auth,
 * _svcauth_short uses a shorthand auth to index into a cache of longhand auths.
 * Note: the shorthand has been gutted for efficiency.
 */

#ifdef _KERNEL
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/time.h"
#include "netinet/in.h"
#include "types.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include "rpc_msg.h"
#include "svc.h"
#include "auth_unix.h"
#include "svc_auth.h"
#else
#include "synonyms.h"
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/errorhandler.h>
#include <bstring.h> 		/* prototype for bcopy() */
#endif

/*
 * Unix longhand authenticator
 */
enum auth_stat
_svcauth_unix(register struct svc_req *rqst, register struct rpc_msg *msg)
{
	register enum auth_stat stat;
	XDR xdrs;
	register struct authunix_parms *aup;
	register xdr_long_t *buf;
	struct area {
		struct authunix_parms area_aup;
		char area_machname[MAX_MACHINE_NAME+1];
		gid_t area_gids[NGRPS];
	} *area;
	u_int auth_len;
	int str_len, gid_len;
	register int i;

	area = (struct area *) rqst->rq_clntcred;
	aup = &area->area_aup;
	aup->aup_machname = area->area_machname;
	aup->aup_gids = area->area_gids;
	auth_len = (u_int)msg->rm_call.cb_cred.oa_length;
	xdrmem_create(&xdrs, msg->rm_call.cb_cred.oa_base, auth_len,XDR_DECODE);
	buf = XDR_INLINE(&xdrs, (int)auth_len);
	if (buf != NULL) {
		aup->aup_time = (unsigned long)IXDR_GET_LONG(buf);
		str_len = (int)IXDR_GET_U_LONG(buf);
		if (str_len > MAX_MACHINE_NAME) {
			stat = AUTH_BADCRED;
			goto done;
		}
		bcopy((caddr_t)buf, aup->aup_machname, str_len);
		aup->aup_machname[str_len] = 0;
		str_len = RNDUP(str_len);
		buf += str_len / (int)sizeof (xdr_long_t);
		aup->aup_uid = (uid_t)IXDR_GET_LONG(buf);
		aup->aup_gid = (gid_t)IXDR_GET_LONG(buf);
		gid_len = (int)IXDR_GET_U_LONG(buf);
		if (gid_len > NGRPS) {
			stat = AUTH_BADCRED;
			goto done;
		}
		aup->aup_len = (unsigned int)gid_len;
		for (i = 0; i < gid_len; i++) {
			aup->aup_gids[i] = (gid_t)IXDR_GET_LONG(buf);
		}
		/*
		 * five is the smallest unix credentials structure -
		 * timestamp, hostname len (0), uid, gid, and gids len (0).
		 */
		if ((uint)((5 + gid_len) * BYTES_PER_XDR_UNIT + str_len) >
								auth_len) {
#ifdef        _KERNEL
			printf("bad auth_len gid %d str %d auth %d",
				gid_len, str_len, auth_len);
#else
			_rpc_errorhandler(LOG_ERR,
					  "bad auth_len gid %d str %d auth %d",
					   gid_len, str_len, auth_len);
#endif
			stat = AUTH_BADCRED;
			goto done;
		}
	} else if (! xdr_authunix_parms(&xdrs, aup)) {
		xdrs.x_op = XDR_FREE;
		(void)xdr_authunix_parms(&xdrs, aup);
		stat = AUTH_BADCRED;
		goto done;
	}
	rqst->rq_xprt->xp_verf.oa_flavor = AUTH_NULL;
	rqst->rq_xprt->xp_verf.oa_length = 0;
	stat = AUTH_OK;
done:
	XDR_DESTROY(&xdrs);
	return (stat);
}


/*
 * Shorthand unix authenticator
 * Looks up longhand in a cache.
 */
/* ARGSUSED */
enum auth_stat 
_svcauth_short(struct svc_req *rqst, struct rpc_msg *msg)
{
	return (AUTH_REJECTEDCRED);
	/* NOTREACHED */
	/* this is just here to hush a compiler warning bug... */
	rqst->rq_xprt->xp_verf.oa_length = (u_int)msg->rm_call.cb_cred.oa_length;
}
