/* svc_auth_uni.h - header file for svc_auth_uni.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCsvc_auth_unih
#define __INCsvc_auth_unih

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	enum auth_stat	_svcauth_unix (struct svc_req *rqst, struct rpc_msg *msg);
extern	enum auth_stat	_svcauth_short (struct svc_req *rqst, struct rpc_msg *msg);

#else

extern	enum auth_stat	_svcauth_unix ();
extern	enum auth_stat	_svcauth_short ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsvc_auth_unih */
