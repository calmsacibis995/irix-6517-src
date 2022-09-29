#ifndef __NS_NIS_H__
#define __NS_NIS_H__

#include "dstring.h"
#include "htree.h"

/*
** From <rpc/rpc_msg.h>
*/
#define CALL	0
#define REPLY	1

#define SUCCESS		0
#define PROG_UNAVAIL	1
#define PROG_MISMATCH	2
#define PROC_UNAVAIL	3

#define RPC_MISMATCH	0

#define MSG_ACCEPTED	0
#define MSG_DENIED	1

#define RPC_MSG_VERSION	((u_long) 2)

/*
** From <rpc/pmap_prot.h>
*/
#define PMAPPORT	((u_short)111)
#define PMAPPROG	((u_long)100000)
#define PMAPVERS	((u_long)2)
#define PMAPPROC_SET	((u_long)1)
#define PMAPPROC_UNSET	((u_long)2)
#define PMAPPROC_CALLIT	((u_long)5)
#define PMAP_MULTICAST_INADDR   ((u_long)0xe0000202)

/*
** From <rpcsvc/yp_prot.h>
*/
#define YPPROG			((u_long)100004)
#define YPVERS			((u_long)2)
#define YPMAXDOMAIN		((u_long)64)
#define YPMAXMAP		((u_long)64)
#define YPMAXPEER		((u_long)256)
#define YPMAXRECORD		((u_long)1024)
#define YPPROC_NULL		((u_long)0)
#define YPPROC_DOMAIN		((u_long)1)
#define YPPROC_DOMAIN_NONACK	((u_long)2)
#define YPPROC_MATCH		((u_long)3)
#define YPPROC_FIRST		((u_long)4)
#define YPPROC_NEXT		((u_long)5)
#define YPPROC_XFR		((u_long)6)
#define YPPROC_CLEAR		((u_long)7)
#define YPPROC_ALL		((u_long)8)
#define YPPROC_MASTER		((u_long)9)
#define YPPROC_ORDER		((u_long)10)
#define YPPROC_MAPLIST		((u_long)11)
#define YP_TRUE		1
#define YP_NOMORE	2
#define YP_NOMAP	-1
#define YP_NOKEY	-3

#define YPBINDPROG		((u_long)100007)
#define YPBINDVERS		((u_long)2)
#define YPBINDPROC_NULL		((u_long)0)
#define YPBINDPROC_DOMAIN	((u_long)1)
#define YPBINDPROC_SETDOM	((u_long)2)

#define YPBIND_SUCC_VAL		1
#define YPBIND_FAIL_VAL		2

#define YPBIND_ERR_NOSERV	2

/*
** From <rpc/auth.h>
*/
#define AUTH_UNIX       1

/*
** From <rpc/auth_unix.h>
*/
#define NGRPS 16

/*
** From <rpc/clnt.h>
*/
#define UDPMSGSIZE   8800


#define NIS_BINDING_DIR "/var/yp/binding"
#define NIS_SERVERS_FILE "ypservers"
#define RETRIES         4
#define BIND_TIME       4
#define TOSPACE(p) for (; p && *p && !isspace(*p); p++)
#define SKIPSPACE(p) for (; p && *p && isspace(*p); p++)

#define YPMAXSTRING	256
typedef struct nisstring {
	u_int32_t	words;
	u_int32_t	len;
	char		string[YPMAXSTRING];
} nisstring_t;

#define WORDS(l)	((((l) % 4) != 0) ? (((l) + (4 - ((l) % 4)))/sizeof(u_int32_t)) : ((l)/sizeof(u_int32_t)))
#define COMMAND(rq) rq->r_argv[0]
#define DOMAIN(rq) rq->r_argv[1]
#define MAP(rq) rq->r_argv[2]
#define KEY(rq) rq->r_argv[3]
#define XDR_STRLEN(s) ((strlen(s)+1))

#define MAX_INTERFACES	100
#define DEFAULT_MULTICAST_TTL 2

/* Flags for NIS servers */
#define NO_GRP_BYMBR  (1<<0)  /* Don't know group.bymember map. */
#define NO_RPC_BYNAME (1<<1)  /* Don't know rpc.byname map. */
#define NO_SERVICES   (1<<2)  /* Don't know services map. */
#define NO_SERVICES2  (1<<3)  /* Don't know services.byservicename map. */
#define NO_NETGROUP   (1<<4)  /* Don't know netgroup.by* maps */

typedef struct {
	time_t		version;
	ht_tree_t	byname;
	ht_tree_t	byhost;
	ht_tree_t	byuser;
} nisnetgr_t;

typedef struct nis_server {
	char			domain[256];
	struct sockaddr_in	sin;
	time_t			again;
	int			count;
	int			multicast_ttl;
	uint32_t		tcp_port;
	uint32_t                flags;
	struct nis_server	*next;
	nisnetgr_t		netgroup;
	int			bound_xid;
} nis_server_t;

typedef struct nisrequest {
	struct nisrequest	*next;
	nis_server_t		*server;
	nsd_file_t		*request;
	char			*result;
	int			(*proc)(nsd_file_t *);
	int			(*nomap_proc)(nsd_file_t *);
	int			(*post)(char *);
	uint32_t		last;
	uint32_t		xid;
	int			fd;
	int			len;
	int			count;
	int			state;
	int			result_len;
	nisstring_t		domain;
	nisstring_t		map;
	nisstring_t		key;
	nisstring_t		lastkey;
	uint32_t                flags;
} nisrequest_t;

/* Flags for nisrequest_t.flags */
#define NIS_ENUMERATE_KEY     (1<<0) /* Prepend the key to the data in list */
#define NIS_NULL_EXTEND_KEY   (1<<1) /* The key is null-extended */
				   
/* init.c */
int nis_init(char *);
nis_server_t *nis_domain_server(char *);
int nis_xid_new(int, nsd_file_t *, nsd_callout_proc *);
int nis_xid_lookup(int, nsd_file_t **, nsd_callout_proc **);

/* bind.c */
int nis_bind_broadcast(nsd_file_t *);

/* lookup.c */
int nis_host_normalize(char *);
int nis_match_timeout(nsd_file_t **, nsd_times_t *);
int lookup(nsd_file_t *);

/* next.c */
int next(nsd_file_t *);

/* netgroup.c */
int nis_netgroup_parse(char *, nisnetgr_t *);
void nis_netgroup_clear(nisnetgr_t *);

#endif /* __NS_NIS_H__ */
