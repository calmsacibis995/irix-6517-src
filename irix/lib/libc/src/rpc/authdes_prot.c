/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.6 88/02/08 
 */

/*
 * authdes_prot.c, XDR routines for DES authentication
 */

#ifdef __STDC__
	#pragma weak xdr_authdes_cred = _xdr_authdes_cred
	#pragma weak xdr_authdes_verf = _xdr_authdes_verf
#endif
#include "synonyms.h"

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_des.h>

#define ATTEMPT(xdr_op) if (!(xdr_op)) return (FALSE)

bool_t
xdr_authdes_cred(xdrs, cred)
	XDR *xdrs;
	struct authdes_cred *cred;
{
	/*
	 * Unrolled xdr
	 */
	ATTEMPT(xdr_enum(xdrs, (enum_t *)&cred->adc_namekind));
	switch (cred->adc_namekind) {
	case ADN_FULLNAME:
		ATTEMPT(xdr_string(xdrs, &cred->adc_fullname.name, MAXNETNAMELEN));
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->adc_fullname.key, sizeof(des_block)));
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->adc_fullname.window, sizeof(cred->adc_fullname.window)));
		return (TRUE);
	case ADN_NICKNAME:
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->adc_nickname, sizeof(cred->adc_nickname)));
		return (TRUE);
	default:
		return (FALSE);
	}
}


bool_t
xdr_authdes_verf(xdrs, verf)
	register XDR *xdrs;
	register struct authdes_verf *verf;	
{
	/*
 	 * Unrolled xdr
 	 */
	ATTEMPT(xdr_opaque(xdrs, (caddr_t)&verf->adv_xtimestamp, sizeof(des_block)));
	ATTEMPT(xdr_opaque(xdrs, (caddr_t)&verf->adv_int_u, sizeof(verf->adv_int_u)));
	return (TRUE);
}
