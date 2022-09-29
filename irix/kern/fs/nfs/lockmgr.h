/*	@(#)lockmgr.h	2.1 88/05/24 4.0NFSSRC SMI	*/

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * From SUN 1.6
 */
/*
 * Header file for Kernel<->Network Lock-Manager implementation
 */

#include <sys/vfs.h>
#include "nfs.h"
#include <netinet/in.h>
#include <rpcsvc/nlm_prot.h>
#include <rpcsvc/sm_inter.h>

/* the lockhandle uniquely describes any file in a domain */
typedef struct {
	struct vnode *lh_vp;			/* vnode of file */
	char *lh_servername;			/* file server machine name */
	int lh_timeo;				/* initial timeout */
	struct netobj lh_id;
} lockhandle_t;

struct owner_handle {
	pid_t       oh_pid;
	char        oh_hostname[1];
};

union nlm1_args_u  {
	nlm_testargs	testargs;
	nlm_unlockargs	unlockargs;
	nlm_lockargs	lockargs;
	nlm_cancargs	cancelargs;
};

union nlm1_reply_u {
	nlm_res		statreply;
	nlm_testres	testreply;
};

union nlm4_args_u  {
	nlm4_testargs	testargs;
	nlm4_unlockargs	unlockargs;
	nlm4_lockargs	lockargs;
	nlm4_cancargs	cancelargs;
};

union nlm4_reply_u {
	nlm4_res		statreply;
	nlm4_testres	testreply;
};

struct lockreq {
	void					*lr_res_wait;
	void					*lr_blocked_wait;
	lockhandle_t			*lr_handle;
	int						lr_proc;
	xdrproc_t				lr_xdrargs;
	xdrproc_t				lr_xdrreply;
	union {
		union nlm1_reply_u	lr_reply_1;
		union nlm4_reply_u	lr_reply_4;
	}						lr_reply_u;
	union {
		union nlm1_args_u	lr_args_1;
		union nlm4_args_u	lr_args_4;
	}						lr_args_u;
};
typedef struct lockreq lockreq_t;
#define nlm1_args	lr_args_u.lr_args_1
#define nlm4_args	lr_args_u.lr_args_4
#define nlm1_reply	lr_reply_u.lr_reply_1
#define nlm4_reply	lr_reply_u.lr_reply_4

extern int nlm_granted_timeout;

#define lm_backoff(tim)	((((tim) << 1) > 900) ? 900 : ((tim) << 1))

/* define 'well-known' information */
#define KLM_PROTO	IPPROTO_UDP

extern struct vnode *Statmonvp;
extern char *Statmon_dir;

/* define public routines */
extern int      klm_lockctl(lockhandle_t *, struct flock *, int,
			struct cred *);
extern void		klm_init(void);
extern void		klm_shutdown(void);
extern int  nfs_reclock(struct vnode *, struct flock *, int, int, struct cred *);
extern int	nlm1_async_call(lockhandle_t *lh, struct flock *ld, int cmd,
	struct cred *cred, int reclaim);
extern int	nlm4_async_call(lockhandle_t *lh, struct flock *ld, int cmd,
	struct cred *cred, int reclaim);
extern void lockd_cleanlocks(__uint32_t sysid);
extern void *get_nlm_response_wait(long cookie, struct in_addr *ipaddr);
extern void *get_nlm_blocked_wait(pid_t pid, struct in_addr *ipaddr);
extern void release_nlm_wait(void *wp);
extern int await_nlm_response(void *wp, struct in_addr *ipaddr,
	struct flock *ld, enum nlm4_stats *status, int timeo);
extern int send_nlm_cancel(lockreq_t *lrp, int vers, struct cred *cred, long cookie,
	int reclaim, int timeo);
extern void wake_nlm_requests(struct in_addr *ipaddr);
extern int get_address(u_long, long, struct sockaddr_in *, bool_t);
extern void nlm_dup_flush(u_long);
