#include <rpc/rpc.h>

#define RFINDPROG ((u_long)391008)
#define RFINDVERS ((u_long)1)
#define RFIND ((u_long)1)

struct rfindhdr {
	u_int sz;
	int   fd;
	char *bufp;
};

extern bool_t xdr_rfind (XDR *, struct rfindhdr *);

#define RPCBUFSIZ (8*1024)	/* collect stdio output into buffers of this size */
