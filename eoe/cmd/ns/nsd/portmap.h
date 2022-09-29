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
#define PMAPPROC_GETPORT	3
#define PMAPPROC_DUMP		4
#define PMAPPROC_CALLIT		5

#define WORDS(l)	((((l) % 4) != 0) ? (((l) + (4 - ((l) % 4)))/sizeof(u_int32_t)) : ((l)/sizeof(u_int32_t)))
