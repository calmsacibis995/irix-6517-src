#ifndef __RPC_PMAP_RMT_H__
#define __RPC_PMAP_RMT_H__
#ident "$Revision: 1.5 $"
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

/*	@(#)pmap_rmt.h	1.2 90/07/17 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *	1.2 88/02/08 SMI	
 */

/*
 * Structures and XDR routines for parameters to and replies from
 * the portmapper remote-call-service.
 */

struct rmtcallargs {
	u_long prog, vers, proc, arglen;
	void *args_ptr;
	xdrproc_t xdr_args;
};

extern bool_t xdr_rmtcall_args(XDR *, struct rmtcallargs *);

struct rmtcallres {
	u_long *port_ptr;
	u_long resultslen;
	void *results_ptr;
	xdrproc_t xdr_results;
};

extern bool_t xdr_rmtcallres(XDR *, struct rmtcallres *);

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_PMAP_RMT_H__ */
