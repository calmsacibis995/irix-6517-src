#ifndef __RPC_AUTH_H__
#define __RPC_AUTH_H__
#ident "$Revision: 2.20 $"
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

/*	@(#)auth.h	1.3 90/07/17 4.1NFSSRC SMI	*/
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

/*
 * auth.h, Authentication interface.
 *
 * The data structures are completely opaque to the client.  The client
 * is required to pass a AUTH * to routines that create rpc
 * "sessions".
 */


#ifdef _KERNEL
#include "xdr.h"
#else
#include <rpc/xdr.h>
#endif


#define MAX_AUTH_BYTES	400
#define MAXNETNAMELEN	255	/* maximum length of network user's name */

/*
 * Status returned from authentication check
 */
enum auth_stat {
	AUTH_OK=0,
	/*
	 * failed at remote end
	 */
	AUTH_BADCRED=1,			/* bogus credentials (seal broken) */
	AUTH_REJECTEDCRED=2,		/* client should begin new session */
	AUTH_BADVERF=3,			/* bogus verifier (seal broken) */
	AUTH_REJECTEDVERF=4,		/* verifier expired or was replayed */
	AUTH_TOOWEAK=5,			/* rejected due to security reasons */
	/*
	 * failed locally
	*/
	AUTH_INVALIDRESP=6,		/* bogus response verifier */
	AUTH_FAILED=7			/* some unknown reason */
};

typedef __uint32_t u_int32;	/* 32-bit unsigned integers */

union des_block {
	struct {
		u_int32 high;
		u_int32 low;
	} key;
	char c[8];
};
typedef union des_block des_block;
extern bool_t xdr_des_block(XDR *, des_block *);

/*
 * Authentication info.  Opaque to client.
 */
struct opaque_auth {
	enum_t	oa_flavor;		/* flavor of auth */
	caddr_t	oa_base;		/* address of more auth stuff */
	u_int	oa_length;		/* not to exceed MAX_AUTH_BYTES */
};

struct __auth_s;
struct auth_ops {
	void	(*ah_nextverf)(struct __auth_s *);

	/* nextverf & serialize */
	int	(*ah_marshal)(struct __auth_s *, XDR *);

	/* validate verifier */
#ifdef _SVR4_TIRPC
	int	(*ah_validate)(struct __auth_s *, struct opaque_auth *);
#else
	int	(*ah_validate)(struct __auth_s *, struct opaque_auth);
#endif

	/* refresh credentials */
	int	(*ah_refresh)(struct __auth_s *);

	/* destroy this structure */
	void	(*ah_destroy)(struct __auth_s *);
};

/*
 * Auth handle, interface to client side authenticators.
 */
typedef struct __auth_s {
	struct	opaque_auth	ah_cred;
	struct	opaque_auth	ah_verf;
	union	des_block	ah_key;
	struct	auth_ops  	*ah_ops;
	caddr_t ah_private;
} AUTH;


/*
 * Authentication ops.
 * The ops and the auth handle provide the interface to the authenticators.
 *
 * AUTH	*auth;
 * XDR	*xdrs;
 * struct opaque_auth verf;
 */
#define AUTH_NEXTVERF(auth)		\
		((*((auth)->ah_ops->ah_nextverf))(auth))
#define auth_nextverf(auth)		\
		((*((auth)->ah_ops->ah_nextverf))(auth))

#define AUTH_MARSHALL(auth, xdrs)	\
		((*((auth)->ah_ops->ah_marshal))(auth, xdrs))
#define auth_marshall(auth, xdrs)	\
		((*((auth)->ah_ops->ah_marshal))(auth, xdrs))

#define AUTH_VALIDATE(auth, verf)	\
		((*((auth)->ah_ops->ah_validate))((auth), verf))
#define auth_validate(auth, verf)	\
		((*((auth)->ah_ops->ah_validate))((auth), verf))

#define AUTH_REFRESH(auth)		\
		((*((auth)->ah_ops->ah_refresh))(auth))
#define auth_refresh(auth)		\
		((*((auth)->ah_ops->ah_refresh))(auth))

#define AUTH_DESTROY(auth)		\
		((*((auth)->ah_ops->ah_destroy))(auth))
#define auth_destroy(auth)		\
		((*((auth)->ah_ops->ah_destroy))(auth))


extern struct opaque_auth _null_auth;


/*
 * These are the various implementations of client side authenticators.
 */

/*
 * Unix style authentication
 * AUTH *authunix_create(machname, uid, gid, len, aup_gids)
 *	char *machname;
 *	int uid;
 *	int gid;
 *	int len;
 *	int *aup_gids;
 */
#ifdef _KERNEL
extern AUTH *authkern_create(void);
#else
#ifdef _SVR4_TIRPC
extern AUTH *authsys_create(char *, uid_t, gid_t, int, gid_t *);
extern AUTH *authsys_create_default(void);
#undef  authunix_create
#define authunix_create         authsys_create
#undef  authunix_create_default
#define authunix_create_default authsys_create_default
#else
extern AUTH *authunix_create(const char *, uid_t, gid_t, int, gid_t *);
extern AUTH *authunix_create_default(void);
#endif
extern AUTH *authnone_create(void);
#endif

/*
 * DES style authentication
 * AUTH *authdes_seccreate(servername, window, timehost, ckey)
 *	char *servername;		- network name of server
 *	u_int window;			- time to live
 *	char *timehost;			- optional hostname to sync with
 *	des_block *ckey;		- optional conversation key to use
 */

struct sockaddr_in;
extern AUTH *authdes_create(
		       const char *, u_int, struct sockaddr_in *, des_block *);
extern AUTH *authdes_seccreate(const char *, u_int, const char *, des_block *);

extern int host2netname(char [], char *, const char *);
extern int user2netname(char [], uid_t, const char *);

extern int host2netname(char [], char *, const char *);
extern int user2netname(char [], uid_t, const char *);

#define AUTH_NONE	0		/* no authentication */
#define	AUTH_NULL	0		/* backward compatibility */
#define	AUTH_UNIX	1		/* unix style (uid, gids) */
#define	AUTH_SYS	1		/* unix style (uid, gids) */
#define	AUTH_SHORT	2		/* short hand unix style */
#define AUTH_DES	3		/* des style (encrypted timestamps) */

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_AUTH_H__ */
