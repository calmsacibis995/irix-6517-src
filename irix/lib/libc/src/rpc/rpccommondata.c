/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN X.X
 */
#ifdef _KERNEL
#include "types.h"
#include "auth.h"
#include "clnt.h"
#else
#ifdef __STDC__
	#pragma weak svc_fdset     = _svc_fdset
	#pragma weak rpc_createerr = _rpc_createerr
#endif
#include "synonyms.h"
#include <rpc/rpc.h>
#endif
/*
 * This file should only contain common data (global data) that is exported 
 * by public interfaces 
 */
#ifndef _KERNEL
fd_set svc_fdset = {0};
#endif
struct opaque_auth _null_auth = {0};
struct rpc_createerr rpc_createerr = { (enum clnt_stat) 0};
