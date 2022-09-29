#ifndef __RPC_AUTH_SYS_H__
#define __RPC_AUTH_SYS_H__
#ident "$Revision: 1.2 $"
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

#ifndef _SVR4_TIRPC
#include <rpc/auth_unix.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

/*	@(#)auth_unix.h	1.2 90/07/17 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *  1.8 88/02/08 SMI   
 */


/*
 * auth_sys.h, Protocol for UNIX style authentication parameters for RPC
 */

/*
 * The system is very weak.  The client uses no encryption for  it
 * credentials and only sends null verifiers.  The server sends backs
 * null verifiers or optionally a verifier that suggests a new short hand
 * for the credentials.
 */

/* The machine name is part of a credential; it may not exceed 255 bytes */
#define MAX_MACHINE_NAME 255

/* gids compose part of a credential; there may not be more than 16 of them */
#define NGRPS 16

/*
 * "Unix" (sys) style credentials.
 */
#if !defined(_STYPES_LATER)
struct authsys_parms {
	u_long	 aup_time;
	char	*aup_machname;
	uid_t	 aup_uid;
	gid_t	 aup_gid;
	u_int	 aup_len;
	gid_t	*aup_gids;
};
#else
struct authsys_parms {
	u_long	 aup_time;
	char	*aup_machname;
	int	 aup_uid;
	int	 aup_gid;
	u_int	 aup_len;
	int	*aup_gids;
};
#endif

struct __xdr_s;
extern bool_t xdr_authsys_parms(struct __xdr_s *, struct authsys_parms *);

/* For backword compatibility */
#define	authunix_parms     authsys_parms
#undef xdr_authunix_parms
#define	xdr_authunix_parms xdr_authsys_parms

#ifdef NOTDEF
/* 
 * If a response verifier has flavor AUTH_SHORT, 
 * then the body of the response verifier encapsulates the following structure;
 * again it is serialized in the obvious fashion.
 */
struct short_hand_verf {
	struct opaque_auth new_cred;
};
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SVR4_TIRPC */
#endif /* !__RPC_AUTH_SYS_H__ */
