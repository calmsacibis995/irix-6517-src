/* @(#)prot_lock.h	1.3 91/06/25 NFSSRC4.1 */

/*
 * This file consists of all structure information used by lock manager 
 */

#include <rpc/rpc.h>
#include <rpcsvc/nlm_prot.h>
#include <rpcsvc/klm_prot.h>
#include "lockf.h"

typedef struct nlm_testres remote_result;
#define lstat stat.stat
#define lholder stat.nlm_testrply_u.holder

#define NLM_LOCK_RECLAIM	16

#define MAXLEN		0x7fffffff	/* was ((1 << 31) -1) */
#define lck 		alock
#define fh_len		fh.n_len
#define fh_bytes	fh.n_bytes
#define oh_len		oh.n_len
#define oh_bytes	oh.n_bytes
#define cookie_bytes	cookie.n_bytes

#define denied		nlm_denied
#define nolocks 	nlm_denied_nolocks
#define blocking	nlm_blocked
#define grace		nlm_denied_grace_period
#define deadlck		nlm_deadlck

/*
 * warning:  struct alock consists of klm_lock and nlm_lock,
 * it has to be modified if either structure has been modified!!!
 */
struct alock {
	netobj cookie;

	/* from klm_prot.h */
        char *server_name;
        netobj fh;

	/* from lockf.h */
	struct data_lock lox;

	/* addition from nlm_prot.h */
	char *caller_name;
	netobj oh;

	/* addition from lock manager */
	char *clnt_name;
};

struct reclock {
	struct reclock *rnext;
	struct reclock *rprev; 

	int  (*rpctransp)();
	int rel;		/* true if can be freed */
	netobj cookie;
	bool_t block;		/* F_SETLK or F_SETLKW */
	bool_t exclusive;	/* F_RDLCK or F_WRLCK */
	bool_t reclaim;		/* if reclaiming a lock after server reboot */
	int state;
	struct alock alock;	/* lock info */

	/* auxiliary structure */
	SVCXPRT *transp; /* transport handle for delayed response due to */ 
			 /* blocking or grace period */

	/* Used by server LM to manage threads waiting for locks */
	struct {
	    int		fd;	/* shared fd of file to lock */
	    pid_t	pid;	/* pid of thread doing blocking fcntl */
	    int		stat;	/* status set by thread when fcntl returns */
		struct filock	*flp;	/* filock struct on blocked list */
	} blk;
};
typedef struct reclock reclock;

#define USES_TCP(x)	((x)->rpctransp == call_tcp)

struct reclocklist {
	struct reclock *rnext;
	struct reclock *rprev; 
};

#define FOREACH_RECLIST(l,t) \
		for (t=(l)->rnext; t!=(struct reclock *)l; t=t->rnext)
#define INIT_RECLIST(l) (l.rnext = l.rprev = (struct reclock *)&l)

/* Monitor entry */
struct lm_vnode {
	struct lm_vnode *next;
        char *server_name;
        struct reclocklist exclusive;	/* exclusive locks */
	struct reclocklist shared;	/* shared locks */
	struct reclocklist pending;	/* pending locks */
};

struct timer {
	/* timer goes off when exp == curr */
	int exp;
	int curr;
};

/*
 * msg passing structure
 */
struct msg_entry {
	struct msg_entry *nxt;
	struct msg_entry *prev;
	int proc; /* procedure name that req is sent to; needed when replying */
	reclock *req;
	remote_result *reply;
	struct timer t;
};
typedef struct msg_entry msg_entry;

struct priv_struct {
	int pid;
	int *priv_ptr;
};

/* Flag args to start_timer */
#define TIMER_MSGQ	0x1
#define TIMER_FDGC	0x2

#include "proto.h"
