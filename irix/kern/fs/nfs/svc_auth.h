#ifndef __RPC_SVC_AUTH_H__
#define __RPC_SVC_AUTH_H__
#ident "$Revision: 2.6 $"
/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*	@(#)svc_auth.h	1.2 90/07/17 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */


/*
 * svc_auth.h, Service side of rpc authentication.
 */


/*
 * Server side authenticator
 */
extern enum auth_stat _authenticate(struct svc_req *, struct rpc_msg *);

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_SVC_AUTH_H__ */
