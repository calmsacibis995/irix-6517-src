#ifndef __NS_NISSERV_H__
#define __NS_NISSERV_H__

#include <sys/mac.h>

/*
** From <rpc/rpc_msg.h>
*/
#define CALL			0
#define REPLY			1

#define SUCCESS			0
#define PROG_UNAVAIL		1
#define PROG_MISMATCH		2
#define PROC_UNAVAIL		3
#define GARBAGE_ARGS		4
#define SYSTEM_ERR		5

#define RPC_MISMATCH		0
#define AUTH_ERROR		1

#define MSG_ACCEPTED		0
#define MSG_DENIED		1

#define RPC_MSG_VERSION		((u_long) 2)

#define AUTH_NONE		0
#define AUTH_UNIX		1
#define AUTH_SHORT		2
#define AUTH_DES		3

#define AUTH_OK			0
#define AUTH_BADCRED		1
#define AUTH_REJECTEDCRED	2
#define AUTH_BADVERF		3
#define AUTH_REJECTEDVERF	4
#define AUTH_TOOWEAK		5
#define AUTH_INVALIDRESP	6
#define AUTH_FAILED		7

/*
** From <rpc/pmap_prot.h>
*/
#define PMAPPORT		111
#define PMAPPROG		100000
#define PMAPVERS		2
#define PMAPPROC_SET		1
#define PMAPPROC_UNSET		2
#define PMAPPROC_CALLIT		5

/*
** From <rpcsvc/yp_prot.h>
*/
#define YPPROG			100004
#define YPVERS			2
#define YPMAXDOMAIN		64
#define YPMAXMAP		64
#define YPMAXPEER		256
#define YPMAXRECORD		1024

#define YPPROC_NULL		0
#define YPPROC_DOMAIN		1
#define YPPROC_DOMAIN_NONACK	2
#define YPPROC_MATCH		3
#define YPPROC_FIRST		4
#define YPPROC_NEXT		5
#define YPPROC_XFR		6
#define YPPROC_CLEAR		7
#define YPPROC_ALL		8
#define YPPROC_MASTER		9
#define YPPROC_ORDER		10
#define YPPROC_MAPLIST		11
#define YPPROC_NEWXFR		12

#define YP_NOMORE		2
#define YP_TRUE			1
#define TP_FALSE		0
#define YP_NOMAP		-1
#define YP_NODOM		-2
#define YP_NOKEY		-3
#define YP_BADOP		-4
#define YP_BADDB		-5
#define YP_YPERR		-6
#define YP_BADARGS		-7
#define YP_VERS			-8

#define YPPUSH_SUCC		1
#define YPPUSH_AGE		2
#define YPPUSH_NOMAP		-1
#define YPPUSH_NODOM		-2
#define YPPUSH_RSRC		-3
#define YPPUSH_RPC		-4
#define YPPUSH_MADDR		-5
#define YPPUSH_YPERR		-6
#define YPPUSH_BADARGS		-7
#define YPPUSH_DBM		-8
#define YPPUSH_FILE		-9
#define YPPUSH_SKEW		-10
#define YPPUSH_CLEAR		-11
#define YPPUSH_FORCE		-12
#define YPPUSH_XFRERR		-13
#define YPPUSH_REFUSED		-14

/*
** From <rpc/auth_unix.h>
*/
#define NGRPS 16

/*
** From <rpc/clnt.h>
*/
#define UDPMSGSIZE   8800


#define TOSPACE(p) for (; p && *p && !isspace(*p); p++)
#define SKIPSPACE(p) for (; p && *p && isspace(*p); p++)

#define YPMAXSTRING	256
typedef struct nisstring {
	u_int32_t	words;
	u_int32_t	len;
	char		string[YPMAXSTRING];
} nisstring_t;

typedef struct nisserv {
	int			xid;		/* transaction id */
	int			fd;		/* file descriptor for send */
	int			proto;		/* protocol over descriptor */
	int			status;		/* reintrancy pointer */
	struct sockaddr_in	sin;		/* reply-to address */
	struct in_addr		local;		/* local address */
	mac_t			lbl;		/* MAC label of sender */
	uint32_t		command;	/* incoming request command */
	nisstring_t		domain;		/* incoming request domain */
	nisstring_t		table;		/* incoming request table */
	nisstring_t		key;		/* incoming request key */
	nsd_callout_proc	*proc;		/* process pointer for cmd */
	char			*data;		/* generic data pointer */
	int			size;		/* length for data */
	int			len;		/* length of unsent data */
  	uint32_t		cmd;		/* cmd (if known) */
	nsd_file_t		*request;	/* back pointer to file */
	nsd_cred_t		*cred;		/* security credentials */
	struct nisserv		*next;		/* next structure */
} nisserv_t;

typedef struct hash_file {
	char			*domain;
	char			*table;
	char			*file;
	time_t			version;
	time_t			touched;
	MDBM			*map;
	int			secure;
	struct hash_file	*next;
} hash_file_t;

typedef struct securenets {
	struct in_addr		addr;
	struct in_addr		netmask;
	struct securenets	*next;
} securenets_t;

#define WORDS(l)	((((l) % 4) != 0) ? (((l) + (4 - ((l) % 4)))/sizeof(u_int32_t)) : ((l)/sizeof(u_int32_t)))

int nisserv_portmap(int, int, int, int);
void hash_clear(void);
int hash_reopen(hash_file_t *);
hash_file_t *hash_get_file(nsd_file_t *);
nisserv_t *nisserv_data_new(void);
void nisserv_data_clear(nisserv_t **);

#endif
