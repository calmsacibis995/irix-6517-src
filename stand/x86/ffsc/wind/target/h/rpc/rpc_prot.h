/* rpc_prot.h - header file for rpc_prot.c */

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

#ifndef __INCrpc_proth
#define __INCrpc_proth

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	  bool_t       xdr_opaque_auth (XDR *xdrs, struct opaque_auth *ap);
extern	  bool_t       xdr_deskey (XDR *xdrs, union des_block *blkp);
extern	  bool_t       xdr_accepted_reply (XDR *xdrs, struct accepted_reply *ar);
extern	  bool_t       xdr_rejected_reply (XDR *xdrs, struct rejected_reply *rr);
extern	  void	       accepted (enum accept_stat acpt_stat, struct rpc_err *error);
extern	  void	       rejected (enum reject_stat rjct_stat, struct rpc_err *error);

#else

extern	  bool_t       xdr_opaque_auth ();
extern	  bool_t       xdr_deskey ();
extern	  bool_t       xdr_accepted_reply ();
extern	  bool_t       xdr_rejected_reply ();
extern	  void	       accepted ();
extern	  void	       rejected ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrpc_proth */
