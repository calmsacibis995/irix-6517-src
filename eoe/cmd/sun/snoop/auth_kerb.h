/*
 * auth_kerb.h, Protocol for Kerberos style authentication for RPC
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/auth_kerb.h,v 1.2 1996/07/06 17:41:59 nn Exp $"

#ifndef	_RPC_AUTH_KERB_H
#define	_RPC_AUTH_KERB_H

#include "krb.h"
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef _KERNEL
#include <sys/t_kuser.h>
#include <rpc/svc.h>
#endif	/* _KERNEL */


#ifdef	__cplusplus
extern "C" {
#endif

/*
 * There are two kinds of "names": fullnames and nicknames
 */
enum authkerb_namekind {
	AKN_FULLNAME,
	AKN_NICKNAME
};
/*
 * A fullname contains the ticket and the window
 */
struct authkerb_fullname {
	KTEXT_ST ticket;
	u_long window;		/* associated window */
};

/*
 *  cooked credential stored in rq_clntcred
 */
struct authkerb_clnt_cred {
	/* start of AUTH_DAT */
	unsigned char k_flags;	/* Flags from ticket */
	char    pname[ANAME_SZ]; /* Principal's name */
	char    pinst[INST_SZ];	/* His Instance */
	char    prealm[REALM_SZ]; /* His Realm */
	unsigned long checksum;	/* Data checksum (opt) */
	C_Block session;	/* Session Key */
	int	life;		/* Life of ticket */
	unsigned long time_sec;	/* Time ticket issued */
	unsigned long address;	/* Address in ticket */
	/* KTEXT_ST reply;	Auth reply (opt) */
	/* end of AUTH_DAT */
	unsigned long expiry;	/* time the ticket is expiring */
	u_long nickname;	/* Nickname into cache */
	u_long window;		/* associated window */
};

typedef struct authkerb_clnt_cred authkerb_clnt_cred;

/*
 * A credential
 */
struct authkerb_cred {
	enum authkerb_namekind akc_namekind;
	struct authkerb_fullname akc_fullname;
	u_long akc_nickname;
};

/*
 * A kerb authentication verifier
 */
struct authkerb_verf {
	union {
		struct timeval akv_ctime;	/* clear time */
		des_block akv_xtime;		/* crypt time */
	} akv_time_u;
	u_long akv_int_u;
};

/*
 * des authentication verifier: client variety
 *
 * akv_timestamp is the current time.
 * akv_winverf is the credential window + 1.
 * Both are encrypted using the conversation key.
 */
#ifndef akv_timestamp
#define	akv_timestamp	akv_time_u.akv_ctime
#define	akv_xtimestamp	akv_time_u.akv_xtime
#define	akv_winverf	akv_int_u
#endif
/*
 * des authentication verifier: server variety
 *
 * akv_timeverf is the client's timestamp + client's window
 * akv_nickname is the server's nickname for the client.
 * akv_timeverf is encrypted using the conversation key.
 */
#ifndef akv_timeverf
#define	akv_timeverf	akv_time_u.akv_ctime
#define	akv_xtimeverf	akv_time_u.akv_xtime
#define	akv_nickname	akv_int_u
#endif

#ifdef _KERNEL
/*
 * Register the service name, instance and realm.
 */
extern int	authkerb_create(char *, char *, char *, u_int,
			struct netbuf *, int *, struct knetconfig *,
			int, AUTH **);
extern bool_t	xdr_authkerb_cred(XDR *, struct authkerb_cred *);
extern bool_t	xdr_authkerb_verf(XDR *, struct authkerb_verf *);
extern int	svc_kerb_reg(SVCXPRT *, u_long, u_long, char *, char *, char *);
extern enum auth_stat _svcauth_kerb(struct svc_req *, struct rpc_msg *);

extern kmutex_t	authkerb_lock;
extern kmutex_t	svcauthkerbstats_lock;
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPC_AUTH_KERB_H */
