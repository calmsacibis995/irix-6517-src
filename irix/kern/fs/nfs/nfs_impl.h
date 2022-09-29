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

#include <net/if.h>

#ifdef __cplusplus
extern "C" {
#endif
/*
 * This file contains definitions private to the IRIX NFS server.
 * It should *NOT* be installed into /usr/include for customers.
 */

#define	DRHASHSZ 255 /* now per-node */

/*
 * Array of pointers to per-node data structures.
 */
typedef struct pernfsq {
	struct ifqueue nfsq;		/* list of incoming packets */
	sv_t waiter;			/* to wake up an nfsd */
	int looking;			/* to avoid it! */
	int node;			/* index into this table */
	int dr_maxdupreqs;		/* size of table */
	struct dupreq *dr_table;	/* base address of the table */
	struct dupreq *drhashtbl[DRHASHSZ]; /* hash headers */
	int dr_index;			/* index into it */
	lock_t duplock;			/* protect above */
} pernfsq_t;
extern pernfsq_t **nfsq_table;
#pragma set type attribute pernfsq_t align=128

extern int max_nfsq;                    /* max number of nfsq's */
extern int nfsq_total;			/* total number of daemons */
extern int nfsq_init;			/* initial number of daemons */

/*
 * Number of bytes in the largest DEcoded NFS operation arguments.
 * Larger on 64-bit models because of pointers
 * XXX change when NFS v4 makes them even bigger!
 */
#if _MIPS_SZLONG == 64
#define NFS_MAX_ARGS 160
#else
#define NFS_MAX_ARGS 152
#endif

/*
 * Number of bytes in largest DEcoded NFS result
 */
#define NFS_MAX_RESULT 304

extern SVCXPRT *svckrpc_create(int node);
extern void svckudp_destroy(SVCXPRT   *xprt);
extern dev_t nfs_expdev(dev_t dev);
extern dev_t nfs_cmpdev(dev_t dev);
extern void rpc_decode_req(SVCXPRT *, struct mbuf *, struct pernfsq *);
extern int nfs_sendreply(SVCXPRT *xprt, xdrproc_ansi_t, caddr_t result,
			      int code, int lowvers, int highvers);
extern int rfs_dispatch(struct svc_req *, XDR *, caddr_t, caddr_t,
			struct pernfsq *);
extern void stop_nfsd(void);
extern void nfs_input(struct mbuf *);
extern struct in_addr *nfs_srcaddr(SVCXPRT *xprt);

#define xdr_nfsstat3(xdrs, objp) xdr_enum(xdrs, (enum_t *)objp)

/*
 * For duplicate request checking
 */

struct dupreq {
	int32_t		dr_xid;
	u_short		dr_proc;
	char		dr_status;	/* in progress? */
	struct sockaddr_in dr_addr;
	struct dupreq	*dr_chain;	/* hash chain */
	struct dupreq	*dr_prev;	/* hash chain back pointer */
	time_t		dr_rtime;	/* time entry was last recycled */
	char	   dr_resp[NFS_MAX_RESULT]; /* copied response */
};

#define DUP_INPROGRESS		0x01	/* request already going */
#define DUP_DONE		0x02	/* request done */

extern int32_t req_to_xid(struct svc_req *req);
extern void svckudp_init(void);
extern struct dupreq *svckudp_dup(struct svc_req *, 
				  struct pernfsq *p, caddr_t);
extern void svckudp_dupdone(struct dupreq *, struct svc_req *, 
			    struct pernfsq *p, caddr_t);
extern void krpc_toss(SVCXPRT *xprt);


/* RPC internal error codes */
#define NFS_RPC_OK 0
#define NFS_RPC_PROG 1
#define NFS_RPC_PROC 2
#define NFS_RPC_VERS 3
#define NFS_RPC_DECODE 4
#define NFS_RPC_AUTH 5
#define NFS_RPC_DUP -1

#ifdef __cplusplus
}
#endif
