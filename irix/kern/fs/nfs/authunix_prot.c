/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.17 88/02/08
*/

/*
 * authunix_prot.c
 * XDR for UNIX style authentication parameters for RPC
 */

#ifdef _KERNEL
#include "types.h"
#include <sys/cred.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/utsname.h>
#include "auth.h"
#include "auth_unix.h"
#include "xdr.h"
#else
#ifdef __STDC__
	#pragma weak xdr_authunix_parms = _xdr_authunix_parms
#endif
#include "synonyms.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#endif

/*
 * XDR for unix authentication parameters.
 */
bool_t
xdr_authunix_parms(xdrs, p)
	register XDR *xdrs;
	register struct authunix_parms *p;
{

	if (xdr_u_long(xdrs, &(p->aup_time))
	    && xdr_string(xdrs, &(p->aup_machname), MAX_MACHINE_NAME)
	    && xdr_int(xdrs, (int *)&(p->aup_uid))
	    && xdr_int(xdrs, (int *)&(p->aup_gid))
	    && xdr_array(xdrs, (caddr_t *)&(p->aup_gids),
		    &(p->aup_len), NGRPS, sizeof(int), (xdrproc_t) xdr_int) ) {
		return (TRUE);
	}
	return (FALSE);
}

#ifdef _KERNEL
/*
 * XDR kernel unix auth parameters.
 * NOTE: this is an XDR_ENCODE only routine.
 */
int
xdr_authkern(xdrs)
	register XDR *xdrs;
{
	cred_t *crp = get_current_cred();
	register gid_t	*gp = crp->cr_groups;
	int	 uid = (int)crp->cr_uid;
	int	 gid = (int)crp->cr_gid;
	int	 len, end;
	int	 groups[NGRPS];
	char	*name = hostname;
	extern int rpc_ngroups_max;

	if (xdrs->x_op != XDR_ENCODE) {
		return (FALSE);
	}

	end = MIN(crp->cr_ngroups, rpc_ngroups_max);

	for (len = 0; len < end; len++) {
		groups[len] = (int)*gp++;
	}

	if (xdr_time_t(xdrs, &time)
            && xdr_string(xdrs, &name, MAX_MACHINE_NAME)
            && xdr_int(xdrs, &uid)
            && xdr_int(xdrs, &gid)
	    && xdr_array(xdrs, (caddr_t *)groups, (u_int *)&len, NGRPS, 
		sizeof (int), xdr_int) ) {
            		return (TRUE);
	}
	return (FALSE);
}
#endif
