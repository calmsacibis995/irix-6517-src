/*
 * Do InDom tricks.
 */

#ident "$Id: indom.c,v 1.50 1998/06/15 05:51:55 tes Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/fs/nfs_stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <syslog.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include "pmapi.h"
#include "impl.h"
#include "./cpu.h"
#include "./spindle.h"
#include "./shm.h"
#include "./cluster.h"
#include "./filesys.h"

#if !BEFORE_IRIX6_4
#include "./hwrouter.h"
#include "./numa_node.h"
#include "./xbow.h"

#include "./xlv.h"

#if !defined(IRIX6_4)
#include "./sem.h"
#endif /* 6.5 or later */

#else /* IRIX6.2 or IRIX6.3 */
int n_nodes = 0;
#endif

/*
 * Note: size of indomtab[] _must_ match the number of #defined PM_INDOM_FOO
 *	things in cluster.h
 */
pmInDomDef	indomtab[PM_INDOM_NEXT];
int		numindom = sizeof(indomtab)/sizeof(indomtab[0]);

/* IPC identifier */
#define ipc_id(slot, nslots, seq) (int)(slot + nslots * seq)

/*
 * NFS request types (not likely to change very often)
 */
#define N_NFSREQS 18

static imap_t nfsreq[] = {
    { 0,	"null"		},
    { 1,	"getattr"	},
    { 2,	"setattr"	},
    { 3,	"root"		},
    { 4,	"lookup"	},
    { 5,	"readlink"	},
    { 6,	"read"		},
    { 7,	"wrcache"	},
    { 8,	"write"		},
    { 9,	"create"	},
    { 10,	"remove"	},
    { 11,	"rename"	},
    { 12,	"link"		},
    { 13,	"symlink"	},
    { 14,	"mkdir"		},
    { 15,	"rmdir"		},
    { 16,	"readdir"	},
    { 17,	"fsstat"	},
};

/*
 * NFS3 request types (not likely to change very often)
 */
#define N_NFS3REQS 22

static imap_t nfs3req[] = {
    { 0,	"null"		},
    { 1,	"getattr"	},
    { 2,	"setattr"	},
    { 3,	"lookup"	},
    { 4,	"access"	},
    { 5,	"readlink"	},
    { 6,	"read"		},
    { 7,	"write"		},
    { 8,	"create"	},
    { 9,	"mkdir"		},
    { 10,	"symlink"	},
    { 11,	"mknod"		},
    { 12,	"remove"	},
    { 13,	"rmdir"		},
    { 14,	"rename"	},
    { 15,	"link"		},
    { 16,	"readdir"	},
    { 17,	"readdir+"	},
    { 18,	"fsstat"	},
    { 19,	"fsinfo"	},
    { 20,	"pathconf"	},
    { 21,	"commit"	},
};

/* mbuf types */

imap_t mbuftyp[] = {
    { 0,	"free"		},
    { 1,	"dynamic_data"	},
    { 2,	"pkt_header"	},
    { 3,	"sock_struct"	},
    { 4,	"protocol_cb"	},
    { 5,	"routing_tables"},
    { 6,	"IMP_hosts"	},
    { 7,	"ARP_tables"	},
    { 8,	"sock_name"	},
    { 9,	"audit_trail"	},
    { 10,	"sock_opt"	},
    { 11,	"frag_reass_hdr"},
    { 12,	"access_rights"	},
    { 13,	"interf_addr"	},
#if !BEFORE_IRIX6_5
    { 16,	"mcast_rinfo"	},
#else
    { 14,	"4DDN_dvr_blk_alloc"	},
    { 15,	"4DDN_blk_alloc"},
    { 16,	"4DDN_buf_2_mbuf"	},
#endif
    { 17,	"inet_mcast_opt"},
    { 18,	"mcast_routing_tables"	},
    { 19,	"sock_acl",	},
#if !BEFORE_IRIX6_5
    { 20,	"interf_addr6"	},
#endif
};

#if !BEFORE_IRIX6_5

/* sockstat open types */

static imap_t socktype[] = {
    {  0,	"illegal"	},
    {  1,	"dgram"		},
    {  2,	"stream"	},
    {  3,	"tpi_cots_ord"	},
    {  4,	"raw"		},
    {  5,	"rdm"		},
    {  6,	"seqpacket"	},
    {  7,	"tpi_cots"	},
};

/* sockstat tcp states */

static imap_t sockstate[] = {
    {  0,	"closed"	},
    {  1,	"listen"	},
    {  2,	"syn_sent"	},
    {  3,	"syn_recevied"	},
    {  4,	"established"	},
    {  5,	"close_wait"	},
    {  6,	"fin_wait_1"	},
    {  7,	"closing"	},
    {  8,	"last_ack"	},
    {  9,	"fin_wait_2"	},
    { 10,	"time_wait"	},
};

#endif /* IRIX 6.5 or later */

extern int reload_ifnet(void);		/* Check for new network interfaces */
extern int refresh_filesys(void);	/* Check for new filesystems */
extern int reload_shm(void);		/* scan for shm (shared memory) identifiers */
extern int reload_msg(void);		/* scan for msg (message queue) identifiers */
extern int reload_sem(void);		/* scan for sem (semaphore) identifiers */
#if !BEFORE_IRIX6_4
extern int reload_hwrouters(void);	/* scan for routers and router ports */
extern int reload_nodes(void);		/* Check for new nodes */
extern int reload_hubs(void);		/* Check for new hubs */
#endif
extern int reload_percpu(void);		/* Check for new cpus */
extern int reload_xbows(void);		/* Check for new xbows */
extern int reload_xbowports(void);	/* Check for new xbow ports */
extern int reload_xlv(void);		/* Check for new xlv volumes */

int
init_indom(int domain, int reset)
{

    extern int			semset_ctr;

#if !BEFORE_IRIX6_4
    extern _pm_router		*routers;
    extern int			n_routers;
    extern _pm_router_port	*routerports;
    extern int			n_routerports;
#endif

    if (!reset) {
/*LINTED*/
	pmInDom		indom;
	__pmInDom_int	*indomp;
/*LINTED*/
	indomp = (__pmInDom_int *)&indom;
	indomp->pad = 0;
	indomp->domain = domain;

	indomp->serial = 1;
	indomtab[PM_INDOM_CPU].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_DISK].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_NFSREQ].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_SWAP].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_LOADAV].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_NETIF].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_FILESYS].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_CNTRL].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_SHM].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_MSG].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_SEM].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_MBUF].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_NFS3REQ].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_SEMSET].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_ROUTER].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_ROUTERPORT].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_NODE].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_LPAGESIZE].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_XBOW].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_XBOWPORT].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_SOCKTYPE].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_SOCKSTATE].i_indom = indom;
	indomp->serial++;
	indomtab[PM_INDOM_XLV].i_indom = indom;
    }

    indomtab[PM_INDOM_CPU].i_numinst = reload_percpu();
    indomtab[PM_INDOM_DISK].i_numinst = spindle_stats_init(reset);
    indomtab[PM_INDOM_NFSREQ].i_numinst = N_NFSREQS;
    indomtab[PM_INDOM_SWAP].i_numinst = swapctl(SC_GETNSWP, NULL);
    indomtab[PM_INDOM_LOADAV].i_numinst = 3;
    indomtab[PM_INDOM_NETIF].i_numinst = reload_ifnet();
    indomtab[PM_INDOM_FILESYS].i_numinst = 0;
    if (indomtab[PM_INDOM_DISK].i_numinst > 0)
	indomtab[PM_INDOM_CNTRL].i_numinst = controller_stats_init(reset);
    else
	indomtab[PM_INDOM_CNTRL].i_numinst = 0;

    /* 
     * SHM, MSG, SEM and SEMSET are non-enumerable instance domains
     * reload_sem() must be called before using semset_ctr
     */
    indomtab[PM_INDOM_SHM].i_numinst = reload_shm();
    indomtab[PM_INDOM_MSG].i_numinst = reload_msg();
    indomtab[PM_INDOM_SEM].i_numinst = reload_sem();
    indomtab[PM_INDOM_MBUF].i_numinst = sizeof(mbuftyp) / sizeof(mbuftyp[0]);
    indomtab[PM_INDOM_NFS3REQ].i_numinst = N_NFS3REQS;
    indomtab[PM_INDOM_SEMSET].i_numinst = semset_ctr;

#if BEFORE_IRIX6_4
    indomtab[PM_INDOM_ROUTER].i_numinst = 0;
    indomtab[PM_INDOM_ROUTERPORT].i_numinst = 0;
    indomtab[PM_INDOM_NODE].i_numinst = 0;
    indomtab[PM_INDOM_LPAGESIZE].i_numinst = 0;
    indomtab[PM_INDOM_XBOW].i_numinst = 0;
    indomtab[PM_INDOM_XBOWPORT].i_numinst = 0;
#else
    (void)reload_hwrouters();
    indomtab[PM_INDOM_ROUTER].i_numinst = n_routers;
    indomtab[PM_INDOM_ROUTERPORT].i_numinst = n_routerports;
    indomtab[PM_INDOM_NODE].i_numinst = reload_nodes();
    indomtab[PM_INDOM_LPAGESIZE].i_numinst = 6;
    indomtab[PM_INDOM_XBOW].i_numinst = reload_xbows();
    indomtab[PM_INDOM_XBOWPORT].i_numinst = reload_xbowports();
#endif

#if BEFORE_IRIX6_5
    indomtab[PM_INDOM_SOCKTYPE].i_numinst = 0;
    indomtab[PM_INDOM_SOCKSTATE].i_numinst = 0;
#else
    indomtab[PM_INDOM_SOCKTYPE].i_numinst = sizeof(socktype) / sizeof(socktype[0]);
    indomtab[PM_INDOM_SOCKSTATE].i_numinst = sizeof(sockstate) / sizeof(sockstate[0]);
#endif

#if BEFORE_IRIX6_4
    indomtab[PM_INDOM_XLV].i_numinst = 0;
#else
    indomtab[PM_INDOM_XLV].i_numinst = reload_xlv();
#endif

    return 0;
}

int
irix_instance(pmInDom indom, int inst, char *name, __pmInResult **result)
{
    __pmInResult		*res;
    int			idx;
    int			j;
    int			sts;
    static char		namebuf[IFNAMSIZ+5];	/* enough for <= 9999 interfaces of the same type */
    char		*end;
    extern spindle	*disk_stats;
    extern swaptbl_t	*swaptable;
    extern struct ifnet	*_ifnet_table;
    extern int		_ifnet_n;
    extern controller	*cntrl_stats;

    extern _pm_cpu		*cpus;
    extern int			n_cpus;

    extern shm_map_t		*shm_map;
    extern int			max_shm_inst;

#if BEFORE_IRIX6_5
    extern struct semid_ds	**semds;
    extern struct msqid_ds	**msgds;
#endif

#if BEFORE_IRIX6_5
    extern struct semid_ds	*semds_instance;
    extern struct seminfo	seminfo;
#else
    extern sem_set		*semset_list;
#endif

    extern int			semset_ctr;

    extern struct msqid_ds	*msgds_instance;
    extern struct msginfo	msginfo;

#if !BEFORE_IRIX6_4
    extern _pm_router_port	*routerports;
    extern int			n_routerports;

    extern _pm_router		*routers;
    extern int			n_routers;

    extern _pm_node		*nodes;
    extern int			n_nodes;
#endif

    for (idx = 0; idx < numindom; idx++)
	if (indom == indomtab[idx].i_indom)
	    break;
    if (idx == numindom)
	return PM_ERR_INDOM;

    /* For instance domains that change size dynamically */
    switch (idx) {
	case PM_INDOM_NETIF:
	    if (_ifnet_n != indomtab[idx].i_numinst)
		indomtab[idx].i_numinst = reload_ifnet();
	    break;

	case PM_INDOM_SWAP:
	    /* TODO: first check the number of instances has not changed */
	    break;

	case PM_INDOM_FILESYS:
	    indomtab[idx].i_numinst = refresh_filesys();
	    break;
	
	case PM_INDOM_SHM:
	    indomtab[idx].i_numinst = reload_shm();
	    break;

	case PM_INDOM_SEM:
	    indomtab[idx].i_numinst = reload_sem();
	    break;

	case PM_INDOM_SEMSET:
	    (void)reload_sem();
	    indomtab[idx].i_numinst = semset_ctr;
	    break;

	case PM_INDOM_MSG:
	    indomtab[idx].i_numinst = reload_msg();
	    break;
	
	case PM_INDOM_XBOWPORT:
	    indomtab[idx].i_numinst = reload_xbowports();
	    break;

#if !BEFORE_IRIX6_4
	case PM_INDOM_XLV:
	    if (!xlv_initDone) {
		(void)xlv_setup();
		indomtab[idx].i_numinst = xlv_instLen;
	    }
	    break;
#endif

	default:
	    break;
    }

    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
	return -errno;
    res->indom = indom;
    res->instlist = NULL;
    res->namelist = NULL;
    sts = 0;

    if (name == NULL && inst == PM_IN_NULL)
	/* get everything ... how many? */
	res->numinst = indomtab[idx].i_numinst;
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
    }

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
	for (j = 0; j < res->numinst; j++)
	    res->namelist[j] = NULL;
    }

    switch (idx) {

	case PM_INDOM_NETIF:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    register struct ifnet *ifp = &_ifnet_table[j];
		    res->instlist[j] = ifp->if_index;
		    sprintf(namebuf, "%s%d", ifp->if_name, ifp->if_unit);
		    if ((res->namelist[j] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    register struct ifnet *ifp = &_ifnet_table[j];
		    if (ifp->if_index == inst) {
			sprintf(namebuf, "%s%d", ifp->if_name, ifp->if_unit);
			if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    register struct ifnet *ifp = &_ifnet_table[j];
		    sprintf(namebuf, "%s%d", ifp->if_name, ifp->if_unit);
		    if (strcmp(name, namebuf) == 0) {
			res->instlist[0] = ifp->if_index;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_NFSREQ:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = nfsreq[j].i_id;
		    if ((res->namelist[j] = strdup(nfsreq[j].i_name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (nfsreq[j].i_id == inst) {
			if ((res->namelist[0] = strdup(nfsreq[j].i_name)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, nfsreq[j].i_name) == 0) {
			res->instlist[0] = nfsreq[j].i_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_NFS3REQ:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = nfs3req[j].i_id;
		    if ((res->namelist[j] = strdup(nfs3req[j].i_name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (nfs3req[j].i_id == inst) {
			if ((res->namelist[0] = strdup(nfs3req[j].i_name)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, nfs3req[j].i_name) == 0) {
			res->instlist[0] = nfs3req[j].i_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_SWAP:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    register swapent_t *se = &swaptable->swt_ent[j];
		    res->instlist[j] = se->ste_lswap;
		    if ((res->namelist[j] = strdup(se->ste_path)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    register swapent_t *se = &swaptable->swt_ent[j];
		    if (se->ste_lswap == inst) {
			if ((res->namelist[0] = strdup(se->ste_path)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    register swapent_t *se = &swaptable->swt_ent[j];
		    if (strcmp(name, se->ste_path) == 0) {
			res->instlist[0] = se->ste_lswap;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_DISK:
	    /* TODO: first check the number of instances has not changed */
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = disk_stats[j].s_id;
		    if ((res->namelist[j] = strdup(disk_stats[j].s_dname)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (disk_stats[j].s_id == inst) {
			if ((res->namelist[0] = strdup(disk_stats[j].s_dname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, disk_stats[j].s_dname) == 0) {
			res->instlist[0] = disk_stats[j].s_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_CNTRL:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = cntrl_stats[j].c_id;
		    if ((res->namelist[j] = strdup(cntrl_stats[j].c_dname)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (cntrl_stats[j].c_id == inst) {
			if ((res->namelist[0] = strdup(cntrl_stats[j].c_dname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, cntrl_stats[j].c_dname) == 0) {
			res->instlist[0] = cntrl_stats[j].c_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_CPU:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = CPU_ID(&cpus[j]);
		    if ((res->namelist[j] = strdup(cpus[j].extname)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (CPU_ID(&cpus[j]) == inst) {
			if ((res->namelist[0] = strdup(cpus[j].extname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, cpus[j].extname) == 0) {
			res->instlist[0] = CPU_ID(&cpus[j]);
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_LOADAV:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		res->instlist[0] = 1;
		res->namelist[0] = strdup("1 minute");
		res->instlist[1] = 5;
		res->namelist[1] = strdup("5 minute");
		res->instlist[2] = 15;
		res->namelist[2] = strdup("15 minute");
		if (res->namelist[0] == NULL ||
		    res->namelist[1] == NULL ||
		    res->namelist[2] == NULL) {
		    __pmFreeInResult(res);
			return -errno;
		    }
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		if (inst == 1 || inst == 5 || inst == 15) {
		    sprintf(namebuf, "%d minute", inst);
		    if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
		else
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		inst = (int)strtol(name, &end, 10);
		if ((*end == '\0' || strcmp(end, " minute") == 0) && (inst == 1 || inst == 5 || inst == 15))
		    res->instlist[0] = inst;
		else
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_FILESYS:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		int ii, jj;
		for (ii = jj = 0; ii < nfsi; ii++)
		    if (fsilist[ii].status.mounted) {
			res->instlist[jj] = fsilist[ii].inst;
			res->namelist[jj] = strdup(fsilist[ii].fsname);
			if (res->namelist[jj] == NULL) {
			    sts = -errno;
			    break;
			}
			jj++;
		    }
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		int ii;
		for (ii = 0; ii < nfsi; ii++)
		    if (fsilist[ii].status.mounted && inst == fsilist[ii].inst) {
			res->namelist[0] = strdup(fsilist[ii].fsname);
			if (res->namelist[0] == NULL)
			    sts = -errno;
			break;
		    }
		if (ii == nfsi)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		int ii;
		for (ii = 0; ii < nfsi; ii++)
		    if (fsilist[ii].status.mounted &&
			strcmp(name, fsilist[ii].fsname) == 0) {
			res->instlist[0] = fsilist[ii].inst;
			break;
		    }
		if (ii == nfsi)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_SHM:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		int	k, count=0;
		for (k=0; k < max_shm_inst; k++) {
		    if (shm_map[k].id == -1)
			continue;
		    res->instlist[count] = shm_map[k].id;
		    if (shm_map[k].key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d", shm_map[k].id);
		    else
			sprintf(namebuf, "KEY_0x%08x", shm_map[k].key);
		    if ((res->namelist[count] = strdup(namebuf)) == NULL) {
			sts = -errno;
			break;
		    }
		    count++;
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		int	k;
		sts = PM_ERR_INST;
		for (k=0; k < max_shm_inst; k++) {
		    if (shm_map[k].id == -1)
			continue;
		    if (shm_map[k].id != inst) 
			continue;
		    if (shm_map[k].key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d", shm_map[k].id);
		    else
			sprintf(namebuf, "KEY_0x%08x", shm_map[k].key);
		    if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			sts = -errno;
			break;
		    }
		    sts = 0;
		    break;
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: shm(inst=%d): %s\n", inst, pmErrStr(sts));
#endif
	    }
	    else {
		/* find the instance for the given indom/name */
		sts = PM_ERR_INST;
		if (strncmp(name, "ID_", 3) == 0) {
		    /* IPC_PRIVATE, search for matching ID */
		    int		k;
		    int		id;
		    if (sscanf(&name[3], "%d", &id) == 1) {
			for (k=0; k < max_shm_inst; k++) {
			    if (shm_map[k].id == -1)
				continue;
			    if (id == shm_map[k].id) {
				res->instlist[0] = id;
				sts = 0;
				break;
			    }
			}
		    }
		}
		else if (strncmp(name, "KEY_0x", 6) == 0) {
		    /* public key, use this to find matching ID */
		    int		k;
		    key_t	key;
		    if (sscanf(&name[6], "%x", &key) == 1) {
			for (k=0; k < max_shm_inst; k++) {
			    if (shm_map[k].id == -1)
				continue;
			    if (key == shm_map[k].key) {
				res->instlist[0] = shm_map[k].id;
				sts = 0;
				break;
			    }
			}
		    }
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: shm(name=%s): %s\n", name, pmErrStr(sts));
#endif
	    }
	    break;

	case PM_INDOM_MSG:
#if BEFORE_IRIX6_5
#define MSG_VALID(k) (msgds[k] != NULL)
#else
#define MSG_VALID(k) (msgds_instance[k].msg_perm.mode & IPC_ALLOC)
#endif
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		int	k, id, count=0;
		key_t	key;
		for (k=0; k < msginfo.msgmni; k++) {
		    if (!MSG_VALID(k))
			continue;
		    key = msgds_instance[k].msg_perm.key;
		    id = ipc_id(k, msginfo.msgmni, msgds_instance[k].msg_perm.seq);
		    res->instlist[count] = id;
		    if (key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d", id);
		    else
			sprintf(namebuf, "KEY_0x%08x", key);
		    if ((res->namelist[count] = strdup(namebuf)) == NULL) {
			sts = -errno;
			break;
		    }
		    count++;
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		int	k, id;
		key_t	key;
		sts = PM_ERR_INST;
		for (k=0; k < msginfo.msgmni; k++) {
		    if (!MSG_VALID(k))
			continue;
		    id = ipc_id(k, msginfo.msgmni, msgds_instance[k].msg_perm.seq);
		    if (id != inst) 
			continue;
		    key = msgds_instance[k].msg_perm.key;
		    if (key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d", id);
		    else
			sprintf(namebuf, "KEY_0x%08x", key);
		    if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			sts = -errno;
			break;
		    }
		    sts = 0;
		    break;
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: msg(inst=%d): %s\n", inst, pmErrStr(sts));
#endif
	    }
	    else {
		/* find the instance for the given indom/name */
		sts = PM_ERR_INST;
		if (strncmp(name, "ID_", 3) == 0) {
		    /* IPC_PRIVATE, search for matching ID */
		    int		k;
		    int		id;
		    if (sscanf(&name[3], "%d", &id) == 1) {
			for (k=0; k < msginfo.msgmni; k++) {
			    if (!MSG_VALID(k))
				continue;
			    if (id == ipc_id(k, msginfo.msgmni, msgds_instance[k].msg_perm.seq)) {
				res->instlist[0] = id;
				sts = 0;
				break;
			    }
			}
		    }
		}
		else if (strncmp(name, "KEY_0x", 6) == 0) {
		    /* public key, use this to find matching ID */
		    int		k;
		    key_t	key;
		    if (sscanf(&name[6], "%x", &key) == 1) {
			for (k=0; k < msginfo.msgmni; k++) {
			    if (!MSG_VALID(k))
				continue;
			    if (key == msgds_instance[k].msg_perm.key) {
				res->instlist[0] = ipc_id(k, msginfo.msgmni, msgds_instance[k].msg_perm.seq);
				sts = 0;
				break;
			    }
			}
		    }
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: msg(name=%s): %s\n", name, pmErrStr(sts));
#endif
	    }
	    break;

/*
 * Note
 *	These IRIX-dependent macros are used for _both_ the PM_INDOM_SEM
 *	and the PM_INDOM_SEMSET instance domains.
 */

#if BEFORE_IRIX6_5
#define COUNT_SEMSET seminfo.semmni
#define CHECK_SEMSET_VALID(k) if (semds[k] == NULL) continue
#define SEMSET_ID(k) ipc_id(k, seminfo.semmni, semds_instance[k].sem_perm.seq)
#define SEMSET_KEY(k) semds_instance[k].sem_perm.key
#define SEMSET_NUMSEM(k) semds_instance[k].sem_nsems
#else
#define COUNT_SEMSET semset_ctr
#define CHECK_SEMSET_VALID(k)
#define SEMSET_ID(k) semset_list[k].set_id
#define SEMSET_KEY(k) semset_list[k].set_key
#define SEMSET_NUMSEM(k) semset_list[k].set_nsems
#endif

	case PM_INDOM_SEM:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		int	k, count=0, c;
		int	id;
		key_t	key;
		/* find all instance ids for the given indom */
		for (k=0; k < COUNT_SEMSET; k++) {
		    CHECK_SEMSET_VALID(k);
		    id = SEMSET_ID(k);
		    key = SEMSET_KEY(k);
		    for (c=0; c < SEMSET_NUMSEM(k); c++) {
			res->instlist[count] = (id << 16) | c;
			if (key == IPC_PRIVATE)
			    sprintf(namebuf, "ID_%d.%d", id, c);
			else
			    sprintf(namebuf, "KEY_0x%08x.%d", key, c);
			if ((res->namelist[count] = strdup(namebuf)) == NULL) {
			    sts = -errno;
			    break;
			}
			count++;
		    }
		    if (sts != 0)
			break;
                }
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		int	k, id;
		int	c;
		key_t	key;
		id = inst >> 16;
		c = inst & 0xffff;
		sts = PM_ERR_INST;
		for (k=0; k < COUNT_SEMSET; k++) {
		    CHECK_SEMSET_VALID(k);
		    if (id != SEMSET_ID(k))
			continue;
		    key = SEMSET_KEY(k);
		    if (key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d.%d", id, c);
		    else
			sprintf(namebuf, "KEY_0x%08x.%d", key, c);
		    if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		    sts = 0;
		    break;
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: sem(inst=%d): %s\n", inst, pmErrStr(sts));
#endif
	    }
	    else {
		/* find the instance for the given indom/name */
		int	semnum = 0;	/* default number (in set) is zero */
		sts = PM_ERR_INST;
		if (strncmp(name, "ID_", 3) == 0) {
		    /* IPC_PRIVATE, search for matching ID */
		    int		k;
		    int		id;
		    if (sscanf(&name[3], "%d.%d", &id, &semnum) >= 1) {
			for (k=0; k < COUNT_SEMSET; k++) {
			    CHECK_SEMSET_VALID(k);
			    if (id != SEMSET_ID(k))
				continue;
			    res->instlist[0] = (id << 16) | semnum;
			    sts = 0;
			    break;
			}
		    }
		}
		else if (strncmp(name, "KEY_0x", 6) == 0) {
		    /* public key, use this to find matching ID */
		    int		k;
		    key_t	key;
		    if (sscanf(&name[6], "%x.%d", &key, &semnum) >= 1) {
			for (k=0; k < COUNT_SEMSET; k++) {
			    CHECK_SEMSET_VALID(k);
			    if (key != SEMSET_KEY(k))
				continue;
			    res->instlist[0] = (SEMSET_ID(k) << 16) | semnum;
			    sts = 0;
			    break;
			}
		    }
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: sem(name=%s): %s\n", name, pmErrStr(sts));
#endif
	    }
	    break;

	case PM_INDOM_SEMSET:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		int	k, count=0;
		int	id;
		key_t	key;
		/* find all instance ids for the given indom */
		for (k=0; k < COUNT_SEMSET; k++) {
		    CHECK_SEMSET_VALID(k);
		    id = SEMSET_ID(k);
		    key = SEMSET_KEY(k);
		    res->instlist[count] = id;
		    if (key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d", id);
		    else
			sprintf(namebuf, "KEY_0x%08x", key);
		    if ((res->namelist[count] = strdup(namebuf)) == NULL) {
			sts = -errno;
			break;
		    }
		    count++;
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		int	k;
		key_t	key;
		sts = PM_ERR_INST;
		for (k=0; k < COUNT_SEMSET; k++) {
		    CHECK_SEMSET_VALID(k);
		    if (inst != SEMSET_ID(k))
			continue;
		    key = SEMSET_KEY(k);
		    if (key == IPC_PRIVATE)
			sprintf(namebuf, "ID_%d", inst);
		    else
			sprintf(namebuf, "KEY_0x%08x", key);
		    if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		    sts = 0;
		    break;
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: semset(inst=%d): %s\n", inst, pmErrStr(sts));
#endif
	    }
	    else {
		/* find the instance for the given indom/name */
		sts = PM_ERR_INST;
		if (strncmp(name, "ID_", 3) == 0) {
		    /* IPC_PRIVATE, search for matching ID */
		    int		k;
		    int		id;
		    if (sscanf(&name[3], "%d", &id) == 1) {
			for (k=0; k < COUNT_SEMSET; k++) {
			    CHECK_SEMSET_VALID(k);
			    if (id != SEMSET_ID(k))
				continue;
			    res->instlist[0] = id;
			    sts = 0;
			    break;
			}
		    }
		}
		else if (strncmp(name, "KEY_0x", 6) == 0) {
		    /* public key, use this to find matching ID */
		    int		k;
		    key_t	key;
		    if (sscanf(&name[6], "%x", &key) == 1) {
			for (k=0; k < COUNT_SEMSET; k++) {
			    CHECK_SEMSET_VALID(k);
			    if (key != SEMSET_KEY(k))
				continue;
			    res->instlist[0] = SEMSET_ID(k);
			    sts = 0;
			    break;
			}
		    }
		}
#ifdef PCP_DEBUG
		if (sts < 0 && (pmDebug & DBG_TRACE_INDOM))
		    __pmNotifyErr(LOG_ERR, "irix_instance: semset(name=%s): %s\n", name, pmErrStr(sts));
#endif
	    }
	    break;

	case PM_INDOM_MBUF:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = mbuftyp[j].i_id;
		    if ((res->namelist[j] = strdup(mbuftyp[j].i_name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (mbuftyp[j].i_id == inst) {
			if ((res->namelist[0] = strdup(mbuftyp[j].i_name)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, mbuftyp[j].i_name) == 0) {
			res->instlist[0] = mbuftyp[j].i_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

#if !BEFORE_IRIX6_4
	case PM_INDOM_ROUTER:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = routers[j].id;
		    if ((res->namelist[j] = strdup(routers[j].name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (routers[j].id == inst) {
			if ((res->namelist[0] = strdup(routers[j].name)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, routers[j].name) == 0) {
			res->instlist[0] = routers[j].id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_ROUTERPORT:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = routerports[j].id;
		    if ((res->namelist[j] = strdup(routerports[j].name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (routerports[j].id == inst) {
			if ((res->namelist[0] = strdup(routerports[j].name))== NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, routerports[j].name) == 0) {
			res->instlist[0] = routerports[j].id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_NODE:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = NODE_ID(&nodes[j]);
		    if ((res->namelist[j] = strdup(nodes[j].extname)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (NODE_ID(&nodes[j]) == inst) {
			if ((res->namelist[0] = strdup(nodes[j].extname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, nodes[j].extname) == 0) {
			res->instlist[0] = NODE_ID(&nodes[j]);
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_LPAGESIZE:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		res->instlist[0] = 16;
		res->namelist[0] = strdup("16Kb");
		res->instlist[1] = 64;
		res->namelist[1] = strdup("64Kb");
		res->instlist[2] = 256;
		res->namelist[2] = strdup("256Kb");
		res->instlist[3] = 1024;
		res->namelist[3] = strdup("1024Kb");
		res->instlist[4] = 4096;
		res->namelist[4] = strdup("4096Kb");
		res->instlist[5] = 16384;
		res->namelist[5] = strdup("16384Kb");
		if (res->namelist[0] == NULL ||
		    res->namelist[1] == NULL ||
		    res->namelist[2] == NULL ||
		    res->namelist[3] == NULL ||
		    res->namelist[4] == NULL ||
		    res->namelist[5] == NULL) {
		    __pmFreeInResult(res);
			return -errno;
		    }
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		if (inst == 16 || inst == 64 || inst == 256 ||
			inst == 1024 || inst == 4096 || inst == 16384) {
		    sprintf(namebuf, "%dKb", inst);
		    if ((res->namelist[0] = strdup(namebuf)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
		else
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		inst = (int)strtol(name, &end, 10);
		if ((strcmp(end, "Kb") == 0) && (inst == 16 || inst == 64 ||
			inst == 256 || inst == 1024 || inst == 4096 || inst == 16384))
		    res->instlist[0] = inst;
		else
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_XBOW:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = XBOW_ID(&xbows[j]);
		    if ((res->namelist[j] = strdup(xbows[j].extname)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (XBOW_ID(&xbows[j]) == inst) {
			if ((res->namelist[0] = strdup(xbows[j].extname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, xbows[j].extname) == 0) {
			res->instlist[0] = XBOW_ID(&xbows[j]);
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_XBOWPORT:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		int k;
		for (k = 0, j = 0; k < n_xbowportln; k++) {
		    if (XBOWPORT_ACTIVE(&xbowports[k])) {
			res->instlist[j] = XBOWPORT_ID(&xbowports[k]);
			if ((res->namelist[j] = strdup(xbowports[k].extname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			j++;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < n_xbowportln; j++) {
		    if (XBOWPORT_ID(&xbowports[j]) == inst) {
			if ((res->namelist[0] = strdup(xbowports[j].extname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < n_xbowportln; j++) {
		    if (strcmp(name, xbowports[j].extname) == 0) {
			res->instlist[0] = XBOWPORT_ID(&xbowports[j]);
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_XLV:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = xlv_inst[j].id;
		    if ((res->namelist[j] = strdup(xlv_inst[j].extname)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (xlv_inst[j].id == inst) {
			if ((res->namelist[0] = strdup(xlv_inst[j].extname)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, xlv_inst[j].extname) == 0) {
			res->instlist[0] = xlv_inst[j].id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

#endif /* !BEFORE_IRIX6_4 */

#if !BEFORE_IRIX6_5

	case PM_INDOM_SOCKTYPE:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = socktype[j].i_id;
		    if ((res->namelist[j] = strdup(socktype[j].i_name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (socktype[j].i_id == inst) {
			if ((res->namelist[0] = strdup(socktype[j].i_name)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, socktype[j].i_name) == 0) {
			res->instlist[0] = socktype[j].i_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;

	case PM_INDOM_SOCKSTATE:
	    if (name == NULL && inst == PM_IN_NULL) {
		/* find all instance ids for the given indom */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    res->instlist[j] = sockstate[j].i_id;
		    if ((res->namelist[j] = strdup(sockstate[j].i_name)) == NULL) {
			__pmFreeInResult(res);
			return -errno;
		    }
		}
	    }
	    else if (name == NULL) {
		/* find the name for the given indom/instance */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (sockstate[j].i_id == inst) {
			if ((res->namelist[0] = strdup(sockstate[j].i_name)) == NULL) {
			    __pmFreeInResult(res);
			    return -errno;
			}
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    else {
		/* find the instance for the given indom/name */
		for (j=0; j < indomtab[idx].i_numinst; j++) {
		    if (strcmp(name, sockstate[j].i_name) == 0) {
			res->instlist[0] = sockstate[j].i_id;
			break;
		    }
		}
		if (j == indomtab[idx].i_numinst)
		    sts = PM_ERR_INST;
	    }
	    break;


#endif /* !BEFORE_IRIX6_5 */

	default:
	    sts = PM_ERR_INDOM;
    }

    if (sts == 0)
	*result = res;
    else
	__pmFreeInResult(res);

    return sts;
}
