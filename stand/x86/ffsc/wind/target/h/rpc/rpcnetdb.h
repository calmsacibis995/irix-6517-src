/* rpcnetdb.h - definitions for rpc */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
01b,27oct89,hjb  added modification history and #ifndef's to avoid multiple
		 inclusion.
*/

#ifndef __INCrpcnetdbh
#define __INCrpcnetdbh

#ifdef __cplusplus
extern "C" {
#endif

struct rpcent {
	char	*r_name;	/* name of server for this rpc program */
	char	**r_aliases;	/* alias list */
	int	r_number;	/* rpc program number */
};

struct rpcent	*getrpcbyname(), *getrpcbynumber(), *getrpcent();

#ifdef __cplusplus
}
#endif

#endif /* __INCrpcnetdbh */
