/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.28 88/02/08
 */
#ident "$Revision: 3.81 $"

/*
 * svc_kudp.c
 * Server side for UDP/IP based RPC in the kernel.
 * This is shared by nfs_server.c and lockd_server.c.
 */

#include "types.h"
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/mbuf.h>
#include <sys/mbuf_private.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <net/route.h>
#include <net/if.h>
#include <net/netisr.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp_var.h>
#include <netinet/ip.h>
#include <sys/tcpipstats.h>
#include <sys/cmn_err.h>
#include <sys/sesmgr.h>
#include "tcp-param.h"
#include "nfs.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include "rpc_msg.h"
#include "svc.h"
#include "nfs_stat.h"
#include "nfs_impl.h"
#include <rpcsvc/nlm_prot.h>
#include "lockd_impl.h"
#include "xattr.h"

/*
 * Transport private data.
 * Kept in xprt->xp_p1.
 */
struct udp_data {
	XDR	ud_xdr;				/* xdr state */
	struct opaque_auth ud_cred;
	char	ud_credbody[MAX_AUTH_BYTES];	/* incoming credential */
	struct opaque_auth ud_verf;
	char	ud_verfbody[MAX_AUTH_BYTES];	/* incoming verifier */
	char	ud_rverfbody[MAX_AUTH_BYTES];	/* response verifier */
	struct mbuf *ud_m;			/* to recycle */
	struct route ud_route;			/* route cache */
	struct in_addr ud_src;			/* cached source */
	int32_t	ud_xid;				/* id */
	union {
		__uint64_t f1_pad;		/* Force dbl word alignment */
		char    f1_args[NFS_MAX_ARGS];	/* for DEcoded args */
	} ud_f1;
	union {
		__uint64_t f2_pad; 		/* Force dbl word alignment */
		char    f2_response[NFS_MAX_RESULT]; /* for DEcoded reply */
	} ud_f2;
	char	ud_path[NFS_MAXNAMLEN];		/* to avoid path mallocs */
};

#define ud_args		ud_f1.f1_args
#define ud_response	ud_f2.f2_response

/*
 * Fast path down into mbuf code
 */
extern bool_t xdrmbuf_getbytes(XDR *xdrs, caddr_t p, u_int size);

#if (_MIPS_SZLONG != _MIPS_SZINT)
#define XDRMBUF_GETINT(x, i)	xdrmbuf_getint((x), (int *)(i))
extern bool_t xdrmbuf_getint(XDR *xdrs, int *ip);
#else
#define XDRMBUF_GETINT(x, i)	xdrmbuf_getlong((x), (long *)(i))
extern bool_t xdrmbuf_getlong(XDR *xdrs, long *p);
#endif

/*
 * Create a server transport handle and private data.
 */
SVCXPRT *
svckrpc_create(int node)
{
	register SVCXPRT *xprt;
	register struct udp_data *ud;

	xprt = kmem_zalloc_node_hint(sizeof(SVCXPRT), KM_SLEEP, node);
	ud = kmem_zalloc_node_hint(sizeof(struct udp_data), KM_SLEEP, node);
	xprt->xp_raddr.sin_family = AF_INET;
	xprt->xp_p1 = (caddr_t)ud;
	xprt->xp_verf.oa_base = ud->ud_rverfbody;
	xprt->xp_ipattr = NULL;
	return (xprt);
}
 
/*
 * Destroy a transport record.
 * Frees the space allocated for a transport record.
 */
void
svckudp_destroy(xprt)
	register SVCXPRT   *xprt;
{
	register struct udp_data *ud = (struct udp_data *)xprt->xp_p1;

	_SESMGR_NFS_M_FREEM(xprt->xp_ipattr);
	kmem_free(ud, sizeof(struct udp_data));
	kmem_free(xprt, sizeof(SVCXPRT));
}

#define HEADER_OFFSET 64 /* max size of transport and data link header */
#define RPC_HEADER_BYTES 24 /* size of RPC call & reply headers */

#if _MIPS_SIM == _ABI64
/*
 * Define an rpc_msg structure that can be overlayed onto an mbuf
 * and get proper offsets for use by nfs_run.
 */
#define _RPC_MSG	_app32_rpc_callmsg
struct __app32_call_body {
	__uint32_t	 cb_rpcvers;	/* must be equal to two */
	__uint32_t	 cb_prog;
	__uint32_t	 cb_vers;
	__uint32_t	 cb_proc;
};

struct _app32_rpc_callmsg {
	__uint32_t	rm_xid;
	__uint32_t	rm_direction;
	union {
		struct __app32_call_body RM_cmb;
	} ru;
};
#else /* _ABI64 */
#define _RPC_MSG	rpc_msg
#endif

/*
 * Return the source IP address from udp_data.
 */
struct in_addr *
nfs_srcaddr(SVCXPRT *xprt)
{
	return(&(((struct udp_data *)xprt->xp_p1)->ud_src));
}

/*
 * UDP specific server input: save address and call generic
 */
void
nfs_svckudp(SVCXPRT *xprt, struct mbuf *m, struct pernfsq *p)
{
    register struct udpiphdr *ui;
    register struct udp_data *ud = (struct udp_data *)xprt->xp_p1;

    _SESMGR_NFS_M_FREE(xprt->xp_ipattr);
    _SESMGR_NFS_GET_IP_ATTRS(m, xprt);
    xprt->xp_type = XPRT_UDP;
    ui = mtod(m, struct udpiphdr *);
    /* grab sender and destination addresses */
    xprt->xp_raddr.sin_addr = ui->ui_src;
    ud->ud_src = ui->ui_dst;
    xprt->xp_raddr.sin_port = ui->ui_sport;
    m->m_len -= sizeof (struct udpiphdr);
    m->m_off += sizeof (struct udpiphdr);
    rpc_decode_req(xprt, m, p);
}

/*
 * Re-use an incoming mbuf if possible.
 * returns with m_len at the END, like m_vget does.
 * m_off gets reset to the START. Only do it for
 * simple mbufs that we know about that are "clusters" and 
 * DON'T have more than one user (as could happen with tcp or snoop).
 */
static struct mbuf *
m_recycle(struct mbuf *m)
{
	if ((m == NULL) || (m->m_refp != &m->m_ref) || (*m->m_refp > 1) ||
	    ((m->m_flags & (M_LOANED|M_SHARED|M_CLUSTER)) != M_CLUSTER)) {
		m_freem(m);
		return (m_vget(M_WAIT, MBUFCL_SIZE, MT_DATA));
	}

	m_freem(m->m_next);
	m->m_next = NULL;
	m->m_flags &= ~M_COPYFLAGS;
	m->m_len = m->m_u.m_us.mu_size;
	m->m_off =  (__psunsigned_t)m->m_u.m_us.mu_p - 
	            (__psunsigned_t)m;
	return (m);
}

/*
 * Send rpc reply.
 * Serialize the reply packet into the output buffer.
 * For UDP, short-circuit the UDP layer, and call ip_output.
 * Returns zero on success, error number on failure.
 */
int
nfs_sendreply(xprt, xdr_proc, result, code, lowvers, highvers)
	register SVCXPRT *xprt;
	xdrproc_ansi_t xdr_proc;
    	char *result; 
    	int code; 
	int lowvers;
	int highvers;
{
	register struct udp_data *ud = (struct udp_data *)xprt->xp_p1;
	register XDR *xdrs = &(ud->ud_xdr);
	struct ifnet *ifp;
	register int slen;
	int error = 0;
	struct mbuf *m, *mtmp;
	u_int *msg;
	int rtcksum = 0;
	register struct udpiphdr *ui;

	m = m_recycle(ud->ud_m);
	if (xprt->xp_type == XPRT_TCP) {
	    /* 
	     * room for rpc record mark
	     */
	    m->m_len -= sizeof(u_int);
	    m->m_off += sizeof(u_int);
	} else {
	/*
	 * Put a little head-room in the buffer for the lower level headers.
	 * Then do an in-line construction of the RPC reply header.
	 * Finally, XDR encode the results.
	 */
	    m->m_len -= HEADER_OFFSET;
	    m->m_off += HEADER_OFFSET;
	}

	xdrmbuf_init(xdrs, m, XDR_ENCODE);
	msg = mtod(m, u_int *);
	*msg++ = ud->ud_xid;	/* msg->rm_xid */
	*msg++ = REPLY; 	/* msg->rm_direction */
	if (code == NFS_RPC_AUTH) {
	  *msg++ = MSG_DENIED;  /* msg->rm_reply.rp_stat */
	  *msg++ = AUTH_ERROR;	/* msg->rm_reply.rjcted_rply.rj_stat */
	  *msg++ = AUTH_TOOWEAK;/* msg->rm_reply.rjcted_rply.rj_why */
	  xdrs->x_private += 5 * BYTES_PER_XDR_UNIT;
	  xdrs->x_handy   -= 5 * BYTES_PER_XDR_UNIT;
	} else {
	  *msg++ = MSG_ACCEPTED;/* msg->rm_reply.rp_stat */
	  *msg++ = AUTH_NONE;	/* msg->rm_reply.rp_acpt.ar_verf.oa_flavor */
	  *msg++ = 0;		/* msg->rm_reply.rp_acpt.ar_verf.oa_length */
	  switch (code) {
	  case  NFS_RPC_PROG:	/* msg->rm_reply.rp_acpt.ar_stat */
	    *msg++ = PROG_UNAVAIL;
	    break;
	  case  NFS_RPC_PROC:	/* msg->rm_reply.rp_acpt.ar_stat */
	    *msg++ = PROC_UNAVAIL;
	    break;
	  case  NFS_RPC_VERS:
	    *msg++ = PROG_MISMATCH;
	    *msg++ = lowvers;
	    *msg++ = highvers;
	    xdrs->x_private += 2 * BYTES_PER_XDR_UNIT;
	    xdrs->x_handy   -= 2 * BYTES_PER_XDR_UNIT;
	    break;
	  case  NFS_RPC_DECODE:
	    *msg++ = GARBAGE_ARGS;
	    break;
	  default:
	    *msg++ = SUCCESS;
	    break;
	  }
	  xdrs->x_private += RPC_HEADER_BYTES;
	  xdrs->x_handy   -= RPC_HEADER_BYTES;
	}

	if (code || xdr_proc(xdrs, result)) {
		slen = 0;
		for (mtmp = m; mtmp; mtmp = mtmp->m_next) {
			if (mtmp->m_next == 0) {
			/*
			 * Complete the job that XDR should have done!
			 * getpos returns the size used in the LAST mbuf only.
			 */
				mtmp->m_len = XDR_GETPOS(xdrs);
			}
			slen += mtmp->m_len;
		}
		if (xprt->xp_type == XPRT_TCP) {
		    extern int nfstcp_sendreply(SVCXPRT *, struct mbuf *);
		    return(nfstcp_sendreply(xprt, m));
		}

		/*
		 * We need to route now, so we know the outgoing IP address.
		 * Check our route cache, and only lookup if changed or down.
		 * The normal case should NOT require a route lock for speed.
		 */
		if (ud->ud_route.ro_rt == (struct rtentry *)0) {
		  ROUTE_RDLOCK();
		} else {
		    if (((struct sockaddr_in *)(&(ud->ud_route.ro_dst)))
					   ->sin_addr.s_addr !=
				       xprt->xp_raddr.sin_addr.s_addr ||
			     (ud->ud_route.ro_rt->rt_flags & RTF_UP) == 0) {
		      ROUTE_RDLOCK();
		      rtfree_needpromote(ud->ud_route.ro_rt);
		      ud->ud_route.ro_rt = (struct rtentry *)0;
		    }
		}
		if (ud->ud_route.ro_rt == (struct rtentry *)0) {
			ud->ud_route.ro_dst = *(struct sockaddr *)&xprt->xp_raddr;
			((struct sockaddr_in*)&ud->ud_route.ro_dst)->sin_port = 0;
			rtalloc(&ud->ud_route);
			if (ud->ud_route.ro_rt == (struct rtentry *)0 ||
			    ud->ud_route.ro_rt->rt_ifp == (struct ifnet *)0) {
				error = ENETUNREACH;
				ROUTE_UNLOCK();
				goto err;
			}
			ifp = ud->ud_route.ro_rt->rt_ifp;
			ASSERT(ifp);
			ROUTE_UNLOCK();
		} else {
			ifp = ud->ud_route.ro_rt->rt_ifp;
		}
		rtcksum = (ud->ud_route.ro_rt->rt_flags & RTF_CKSUM) ||
			  (ifp->if_flags & IFF_CKSUM);
		/*
		 * Fill in mbuf with extended UDP header
		 * and addresses and length.
		 */
		m->m_off -= sizeof (struct udpiphdr);
		m->m_len += sizeof (struct udpiphdr);
		ui = mtod(m, struct udpiphdr *);
		ui->ui_next = ui->ui_prev = 0;
		ui->ui_x1 = 0;
		ui->ui_pr = IPPROTO_UDP;
		slen += sizeof (struct udphdr);
		ui->ui_len = htons((u_short)slen);
		ui->ui_src = ud->ud_src;
		ui->ui_sport = NFS_PORT;
		ui->ui_dst = xprt->xp_raddr.sin_addr;
		ui->ui_dport = xprt->xp_raddr.sin_port;
		ui->ui_ulen = ui->ui_len;
		slen += sizeof(struct ip);
		ui->ui_sum = 0;
		if (slen <= ifp->if_mtu && !sesmgr_enabled) {
		  /*
		   * packet is small enough - send without fragmenting
		   * Let the link layer compute the checksum if it can
		   * We also know it has no options, so header is fixed.
		   */
			ulong cksum;
			struct sockaddr *dst;			

			if (rtcksum) {
			  m->m_flags |= M_CKSUMMED;
			  ui->ui_sum = 0xffff;
			} else {
			  if ((ui->ui_sum = in_cksum(m, slen)) == 0) {
			    ui->ui_sum = 0xffff;
			  }
			}
#define IPP ((struct ip *)ui)
			IPP->ip_len = slen;
			IPP->ip_ttl = UDP_TTL;	/* tos too! */
			IPP->ip_v = IPVERSION;
			IPP->ip_id = htons(atomicAddIntHot(&ip_id, 1));
			IPP->ip_hl = 5;
			IPSTAT(ips_localout);
			NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			       NETEVENT_UDPDOWN, slen, NETRES_NULL);
			UDPSTAT(udps_opackets);
#define ckp ((ushort*)ui)
			cksum = (ckp[0] + ckp[1] + ckp[2] + ckp[3] + ckp[4]
				+ ckp[6] + ckp[7] + ckp[8] + ckp[9]);
			cksum = (cksum & 0xffff) + (cksum >> 16);
			cksum = (cksum & 0xffff) + (cksum >> 16);
			IPP->ip_sum = cksum ^ 0xffff;
#			undef ckp
#			undef IPP

			if (ud->ud_route.ro_rt->rt_flags & RTF_GATEWAY) {
			  dst = ud->ud_route.ro_rt->rt_gateway;
			} else {
			  dst = (struct sockaddr *)&(xprt->xp_raddr);
			}
			IFNET_UPPERLOCK(ifp);
			error = (*ifp->if_output)
			  (ifp, m, dst, ud->ud_route.ro_rt);
			IFNET_UPPERUNLOCK(ifp);
		} else {
		  struct ipsec *ipattr;
		  /*
		   * Packet will be fragmented - send through normal IP path
		   */
		  if ((ui->ui_sum = in_cksum(m, slen)) == 0) {
		    ui->ui_sum = 0xffff;
		  }
		  ((struct ip *)ui)->ip_len = slen;
		  ((struct ip *)ui)->ip_ttl = UDP_TTL;	/* tos too! */
		  NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			 NETEVENT_UDPDOWN, slen, NETRES_NULL);
		  UDPSTAT(udps_opackets);
		  ipattr = _SESMGR_NFS_SOATTR_COPY(mtod(xprt->xp_ipattr,
							struct ipsec *));
  		  error = ip_output(m, (struct mbuf *)0, &ud->ud_route,
  				    0, 0, ipattr);
	  }
	} else {
#pragma mips_frequency_hint NEVER
err:
		m_freem(m);
	}
	if (error != ENOBUFS ||	ud->ud_route.ro_rt == NULL ||
	    ifp == NULL) {
		return (error);
	}
	/*
	 * More informative error when output queue fills up
	 */
	(void) cmn_err(CE_WARN, "NFS server: %s%d output queue full",
			       ifp->if_name, ifp->if_unit);
	return (0);
}

/*
 * Fast path for arguments to directory operations like lookup
 * Only called on decoding, when using xdr_mbuf module.
 * Strictly speaking, this should be in nfs_server.c, but it knows
 * about struct udp_data, so it must be here. Note that the server
 * dispatch code thinks it is calling us with an XDR pointer. But
 * we happen to know in this module that the XDR data is just
 * the first field of the ud structure, so we can go ahead and
 * de-reference the pointer to get internal fields.
 */
__inline void never_hint(void) {return;}
#pragma mips_frequency_hint NEVER  never_hint
#define XDR_ERROR_RETURN { never_hint(); return (FALSE); }

bool_t
xdr_fast_diropargs(struct udp_data *ud,	struct nfsdiropargs *da)
{
	u_int length;
	XDR *xdrs;

	xdrs = &ud->ud_xdr;
	da->da_name = ud->ud_path;
	ASSERT(xdrs->x_op == XDR_DECODE);

	if (!xdrmbuf_getbytes(xdrs, (caddr_t)&da->da_fhandle,
			      sizeof (fhandle_t))) {
		XDR_ERROR_RETURN;
	}
	if (!XDRMBUF_GETINT(xdrs, &length)) {
		XDR_ERROR_RETURN;
	}
	if (length > NFS_MAXNAMLEN || length == 0) {
		XDR_ERROR_RETURN;
	}
	if (!xdrmbuf_getbytes(xdrs, da->da_name, RNDUP(length))) {
		XDR_ERROR_RETURN;
	}
	da->da_name[length] = '\0';	
	return (TRUE);
}

/* same thing for NFSv3 */
extern bool_t        xdr_nfs_fh3(register XDR *, nfs_fh3 *);

bool_t
xdr_fast_3(struct udp_data *ud,	struct diropargs3 *da)
{
	u_int length;
	XDR *xdrs;

	xdrs = &ud->ud_xdr;
	da->name = ud->ud_path;
	ASSERT(xdrs->x_op == XDR_DECODE);

	if (!xdr_nfs_fh3(xdrs, &da->dir)) {
		XDR_ERROR_RETURN;
	}
	if (!XDRMBUF_GETINT(xdrs, &length)) {
		XDR_ERROR_RETURN;
	}
	if (length > NFS_MAXNAMLEN || length == 0) {
		XDR_ERROR_RETURN;
	}
	if (!xdrmbuf_getbytes(xdrs, da->name, RNDUP(length))) {
		XDR_ERROR_RETURN;
	}
	da->name[length] = '\0';	
	return (TRUE);
}

/*
 * the duplicate elimination routines below provide a log of
 * transactions.  NFS uses this to detect
 * retransmissions and re-send a response if appropriate.
 */

/*
 * Hash the request into one of the following buckets
 */
#define	DR_HASH(xid, ip, port)	( ((u_int)(xid + ip + port)) % DRHASHSZ)
#define	REQTOXID(req)	((struct udp_data *)((req)->rq_xprt->xp_p1))->ud_xid

extern int svc_maxdupreqs; /* tunable from master.d/snfs file */

static int last_complaint;
#define COMPLAINT_LIMIT 60*10*HZ /* only one every ten minutes */

extern void svcktcp_init(void);

struct pernfsq **nfsq_table;
int max_nfsq;

int32_t
req_to_xid(struct svc_req *req)
{
	return(REQTOXID(req));
}

/*
 * Initialize the locks and queues
 * Called on system boot.
 */
void
svckudp_init()
{
	int i;

	/* The queue number is the node number in the system.
	 * It could potentially be changed to the number of netprocs
	 * that are running on the system for LADDIS performance, 
	 * but the basic idea is to have multiple queues for
	 * scalability. */
#ifdef SN0
	max_nfsq = numnodes;
#else
	max_nfsq = (numcpus + 1)/2;
#endif
	ASSERT(max_nfsq > 0);

	nfsq_table = (struct pernfsq **)kmem_zalloc( sizeof(struct pernfsq *)
					   * max_nfsq, KM_SLEEP);
	for (i = 0; i < max_nfsq; i++) {
		nfsq_table[i] = (struct pernfsq *)kmem_zalloc_node(
			 sizeof(struct pernfsq), KM_NOSLEEP, i);
		ASSERT_ALWAYS(nfsq_table[i]);
		nfsq_table[i]->node = i;
		spinlock_init(&nfsq_table[i]->nfsq.ifq_lock, "nfsq_lock");
		spinlock_init(&nfsq_table[i]->duplock, "dupreq");
		sv_init(&nfsq_table[i]->waiter, SV_LIFO, "nfsq");
	}
	svcktcp_init();
}

/*
 * svckudp_dup searches the request log. If it is found, then it
 * returns the state of the request (or NULL if in progress) and
 * the status or attributes that were part of the original reply.
 */
struct dupreq *
svckudp_dup(struct svc_req *req, struct pernfsq *p,
	caddr_t res)
{
	struct sockaddr_in *sin = svc_getcaller(req->rq_xprt);
	struct dupreq *dr;
	int32_t xid;
	int hashval;
	int s;

	xid = REQTOXID(req);
	hashval = DR_HASH(xid, sin->sin_addr.s_addr,  sin->sin_port);

	s = mutex_spinlock(&p->duplock);
	if (!p->dr_table) {
		int i;

#pragma mips_frequency_hint NEVER
		if (svc_maxdupreqs == 0) {
			/*
			 * Auto-configure based on physical memory.
			 * Use the minimum for 16MB and 32 MB systems, then
			 * scale up to about 500 MB where we hit the max.
			 */
			i = physmem;
			i -= 4096;	/* min pages on small machines */
			i /= 30;	/* pages per request entry */
			if (i < 50) {
				i = 50; /* min size of table */
			}
			if (i > 4096) {
				i = 4096; /* max size of table */
			}
			svc_maxdupreqs = i;
		}

		p->dr_maxdupreqs = svc_maxdupreqs/max_nfsq;
		p->dr_table = (struct dupreq *) kmem_zalloc_node_hint(
			  sizeof (struct dupreq) * p->dr_maxdupreqs,
					     KM_NOSLEEP, p->node);
		dr = p->dr_table;
		if (!dr) {
			mutex_spinunlock(&p->duplock, s);
			return (dr);
		}
		(void) bzero(p->drhashtbl, sizeof(p->drhashtbl));
		for (i = 0; i < p->dr_maxdupreqs; i++) {
		/*
		 * Add each entry in the table to some hash chain
		 * (Doesn't matter which) to get things going.
		 */
			dr->dr_xid = i;
			dr->dr_chain = p->drhashtbl[DR_HASH(i, 0, 0)];
			dr->dr_rtime = lbolt;
			if (dr->dr_chain) {
				dr->dr_chain->dr_prev = dr;
			}
			p->drhashtbl[DR_HASH(i, 0, 0)] = dr;
			dr++;
	}
	}

	dr = p->drhashtbl[hashval];
	while (dr != NULL) { 
		if (dr->dr_xid == xid &&
		    dr->dr_proc == req->rq_proc &&
		    dr->dr_addr.sin_addr.s_addr == sin->sin_addr.s_addr &&
		    dr->dr_addr.sin_port == sin->sin_port) {
#pragma mips_frequency_hint NEVER
			RSSTAT.rsduphits++;
			bcopy(dr->dr_resp, res, sizeof (dr->dr_resp));
			if (dr->dr_status & DUP_INPROGRESS)
			  dr = NULL;
			mutex_spinunlock(&p->duplock, s);
			return (dr);
		}
		dr = dr->dr_chain;
	}

	/*
	 * At this point, we know it is NOT a duplicate. 
	 * Recycle the oldest entry in the table (round robin)
	 */
	dr = &p->dr_table[p->dr_index];
	if (++p->dr_index >= p->dr_maxdupreqs) {
#pragma mips_frequency_hint NEVER
		p->dr_index = 0;
		if (p->dr_maxdupreqs != svc_maxdupreqs/max_nfsq) {
			/*
			 * if the size has changed after reboot, throw old away
			 * and start over. Better than crashing.
			 */
			kmem_free(p->dr_table, sizeof (struct dupreq)
				  * p->dr_maxdupreqs);
			p->dr_table = NULL;
			mutex_spinunlock(&p->duplock, s);
			return (NULL);
		}
	}

	if (dr->dr_prev) {
		/*
		 * remove from not the front
		 */
		ASSERT(dr->dr_prev->dr_chain == dr);
		dr->dr_prev->dr_chain = dr->dr_chain;
	} else {
		/*
		 * Remove from front
		 */
		p->drhashtbl[DR_HASH(dr->dr_xid, dr->dr_addr.sin_addr.s_addr,
				  dr->dr_addr.sin_port)] = dr->dr_chain;
	}
	if (dr->dr_chain) {
		/*
		 * Remove from not the end
		 */
		ASSERT(dr->dr_chain->dr_prev == dr);
		dr->dr_chain->dr_prev = dr->dr_prev;
	}
	/*
	 * Compute the smoothed age of these cache entries
	 * with a simple shift-add recursive filter.
	 */
	RSSTAT.rsdupage = (( lbolt - dr->dr_rtime) + RSSTAT.rsdupage*7)/8;

	dr->dr_status = DUP_INPROGRESS;
	dr->dr_xid = xid;
	dr->dr_proc = req->rq_proc;
	dr->dr_addr = req->rq_xprt->xp_raddr;
	dr->dr_rtime = lbolt;
	dr->dr_chain = p->drhashtbl[hashval];
	dr->dr_prev = NULL;
	p->drhashtbl[hashval] = dr;
	if (dr->dr_chain) {
		/*
		 * If it is not the only element in the chain,
		 * keep a previous pointer so unlinks happen fast.
		 */
		dr->dr_chain->dr_prev = dr;
	}

	mutex_spinunlock(&p->duplock, s);
	return (dr);
}

/*
 * svckudp_dupdone marks the request done (DUP_DONE) and stores the
 * response.  If the log entry has already been recycled, complain.
 */
void
svckudp_dupdone(struct dupreq *dr, struct svc_req *req,
	struct pernfsq *p, caddr_t res)
{
	struct sockaddr_in *sin = svc_getcaller(req->rq_xprt);
	int s;

	s = mutex_spinlock(&p->duplock);
	if ((__psunsigned_t)dr < (__psunsigned_t)p->dr_table ||
	    (__psunsigned_t)dr > (__psunsigned_t)
	                         (p->dr_table+p->dr_maxdupreqs)) {
#pragma mips_frequency_hint NEVER
		mutex_spinunlock(&p->duplock, s);
		return;
	}
	if (dr->dr_xid == REQTOXID(req) &&
	    dr->dr_proc == req->rq_proc &&
	    dr->dr_addr.sin_addr.s_addr == sin->sin_addr.s_addr &&
	    dr->dr_addr.sin_port == sin->sin_port) {
		dr->dr_status = DUP_DONE;
		bcopy(res, dr->dr_resp, sizeof (dr->dr_resp));
		mutex_spinunlock(&p->duplock, s);
		return;
	}
	if (lbolt - last_complaint < COMPLAINT_LIMIT) {
		mutex_spinunlock(&p->duplock, s);
		return;
	}
	last_complaint = lbolt;
	mutex_spinunlock(&p->duplock, s);
	printf("NFS server: increase svc_maxdupreqs from %d\n", 
		       svc_maxdupreqs);
}

/*
 * Decode rpc request.
 * Common code for both udp and tcp
 */
void
rpc_decode_req(SVCXPRT *xprt, struct mbuf *m, struct pernfsq *p)
{
    register struct udp_data *ud = (struct udp_data *)xprt->xp_p1;
    register XDR     *xdrs = &(ud->ud_xdr);
    struct svc_req r;
    struct _RPC_MSG *msg;
    int error;

    if (m->m_len < RPC_HEADER_BYTES) {
#pragma mips_frequency_hint NEVER
	    m = m_pullup(m, RPC_HEADER_BYTES);
	    if (m == NULL) {
		    RSSTAT.rsbadlen++;
		    return;
	    }

    }
    ud->ud_m = m;
    msg = mtod(m, struct _RPC_MSG *);
    RSSTAT.rscalls++;
    m->m_len -= RPC_HEADER_BYTES;
    m->m_off += RPC_HEADER_BYTES;
    ud->ud_xid = msg->rm_xid;

    xdrmbuf_init(xdrs, m, XDR_DECODE);
    ud->ud_cred.oa_base = ud->ud_credbody;
    ud->ud_verf.oa_base = ud->ud_verfbody;
    if (!XDRMBUF_GETINT(xdrs, &ud->ud_cred.oa_flavor) ||
	!XDRMBUF_GETINT(xdrs, &ud->ud_cred.oa_length) ||
	ud->ud_cred.oa_length > MAX_AUTH_BYTES ||
	!xdrmbuf_getbytes(xdrs, ud->ud_credbody,
			   RNDUP(ud->ud_cred.oa_length)) ||
	!XDRMBUF_GETINT(xdrs, &ud->ud_verf.oa_flavor) ||
	!XDRMBUF_GETINT(xdrs, &ud->ud_verf.oa_length) ||
	!(ud->ud_verf.oa_length == 0 ||
	  (ud->ud_verf.oa_length <= MAX_AUTH_BYTES &&
	   xdrmbuf_getbytes(xdrs, ud->ud_verfbody,
			    RNDUP(ud->ud_verf.oa_length)))))
    {
#pragma mips_frequency_hint NEVER
	RSSTAT.rsbadcalls++;
	printf("NFS server XDR error from %s port %d len %d\n",
	   inet_ntoa(xprt->xp_raddr.sin_addr), xprt->xp_raddr.sin_port, m->m_len+RPC_HEADER_BYTES);
	printf(" XDR ... to %s port %d next %d\n",
	       inet_ntoa(ud->ud_src), 0,
	       m-> m_next ? m->m_next->m_len : 0);
	m_freem(m);
	return;
    }
    r.rq_prog = msg->rm_call.cb_prog;
    r.rq_vers = msg->rm_call.cb_vers;
    r.rq_proc = msg->rm_call.cb_proc;
    r.rq_xprt = xprt;
    r.rq_cred = ud->ud_cred;
    switch (r.rq_prog) {
	case NFS_PROGRAM:
	    error = rfs_dispatch(&r, xdrs, ud->ud_args, ud->ud_response, p);
	    break;
	case NLM_PROG:
#pragma mips_frequency_hint NEVER
		/*
		 * NLM program requests come to the NFS port also.
		 */
		error = lockd_dispatch(&r, xdrs, ud->ud_args, ud->ud_response);
		break;
	case XATTR_PROGRAM:
#pragma mips_frequency_hint NEVER
		error = xattr_dispatch(&r, xdrs, ud->ud_args, ud->ud_response);
		break;
	default:
#pragma mips_frequency_hint NEVER
		error = ENOPKG;
		break;
    }
    if (error == ENOPKG) {
	    /*
	     * Happens when an unknown protocol gets sent to this
	     * well-known port, as well as when lockd is configured
	     * out of the kernel (which is why we cannot just put
	     * this code in the "default" case above).
	     */
#pragma mips_frequency_hint NEVER
	    error = nfs_sendreply(xprt, (xdrproc_ansi_t)xdr_void, NULL,
		    NFS_RPC_PROG, 0, 0);
	    if (error) {
		    (void) cmn_err(CE_WARN, "NFS server: bad program number %d\n", r.rq_prog);
	    }
    }
}

/*
 * Free up the incoming mbuf when NOT sending a reply
 */
void
krpc_toss(SVCXPRT *xprt)
{
	struct udp_data *ud = (struct udp_data *)xprt->xp_p1;

	m_freem(ud->ud_m);

}

