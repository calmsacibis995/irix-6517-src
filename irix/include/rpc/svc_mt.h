/*
 * Copyright (c) 1986 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_SVC_MT_H
#define	_RPC_SVC_MT_H

/*
 * Private service definitions
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SVC flags
 */
#define	SVC_VERSQUIET	0x0001	/* keep quiet about version mismatch */
#define	SVC_DEFUNCT	0x0002	/* xprt is defunct, release asap */
#define	SVC_DGRAM	0x0004	/* datagram type */
#define	SVC_RENDEZVOUS	0x0008	/* rendezvous */
#define	SVC_CONNECTION	0x000c	/* connection */
#define	SVC_DOOR	0x0010	/* door ipc */
#define	SVC_TYPE_MASK	0x001c	/* type mask */
#define	SVC_FAILED	0x0020	/* send/receive failed, used for VC */
#define	SVC_ARGS_CHECK	0x0040	/* flag to check for argument completion */

#define	svc_flags(xprt)		(SVCEXT(xprt)->flags)
#define	svc_defunct(xprt)	((svc_flags(xprt) & SVC_DEFUNCT) ? TRUE : FALSE)
#define	svc_failed(xprt)	((svc_flags(xprt) & SVC_FAILED) ? TRUE : FALSE)
#define	svc_type(xprt)		(svc_flags(xprt) & SVC_TYPE_MASK)

/*
 * The xp_p3 field the the service handle points to the SVCXPRT_EXT
 * extension structure.
 */
typedef struct svcxprt_list_t {
	struct svcxprt_list_t	*next;
	SVCXPRT			*xprt;
} SVCXPRT_LIST;

typedef struct svcxprt_ext_t {
	int		flags;		/* VERSQUIET, DEFUNCT flag */
	SVCXPRT		*parent;	/* points to parent (NULL in parent) */

	struct rpc_msg	*msg;		/* message */
	struct svc_req	*req;		/* request */
	char		*cred_area;	/* auth work area */
	int		refcnt;		/* number of parent references */
	SVCXPRT_LIST	*my_xlist;	/* list header for this copy */
	struct opaque_auth	xp_auth;	/* raw response verifier */
} SVCXPRT_EXT;

#define	SVCEXT(xprt)		((SVCXPRT_EXT *)((xprt)->xp_p3))

/*
 * Global/module private data and functions
 */
extern XDR **svc_xdrs;
extern int svc_mt_mode;
extern int svc_nfds;
extern int svc_nfds_set;
extern int svc_max_fd;
extern int svc_pipe[2];
extern bool_t svc_polling;
extern SVCXPRT **xports;

SVCXPRT *svc_xprt_alloc(void);
SVCXPRT *svc_copy(SVCXPRT *);
SVCXPRT *svcudp_xprtcopy(SVCXPRT *);
void svc_xprt_free(SVCXPRT *);
void svc_xprt_destroy(SVCXPRT *);
void svc_args_done(SVCXPRT *);
void _svcudp_destroy_private(SVCXPRT *);
void _svc_destroy_private(SVCXPRT *);
int _libc_rpc_mt_init(void);

#ifdef __cplusplus
}
#endif

#endif /* !_RPC_SVC_MT_H */
