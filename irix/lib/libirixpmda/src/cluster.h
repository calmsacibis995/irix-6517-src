/*
 * Copyright (c) 1994 Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * Use, duplication or disclosure by the Government is subject to
 * restrictions as set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software clause
 * at DFARS 252.227-7013 and/or similar or successor clauses in the FAR,
 * or the DOD or NASA FAR Supplement.  Contractor/manufacturer is Silicon
 * Graphics, Inc., 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * THIS SOFTWARE CONTAINS CONFIDENTIAL AND PROPRIETARY INFORMATION OF
 * SILICON GRAPHICS, INC.  ANY DUPLICATION, MODIFICATION, DISTRIBUTION, OR
 * DISCLOSURE IS STRICTLY PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN
 * PERMISSION OF SILICON GRAPHICS, INC.
 */

/*
 * $Id: cluster.h,v 1.46 1999/04/28 07:02:32 tes Exp $
 */

#include <stddef.h>

/*
 * Simple malloc audit
 */

#ifdef PCP_DEBUG
#include <malloc.h>
#define malloc(x)	__pmMalloc(__FILE__, __LINE__, x)
#define realloc(x, y)	__pmRealloc(__FILE__, __LINE__, x, y)
#define calloc(x, y)	__pmCalloc(__FILE__, __LINE__, x, y)
#define valloc(x)	__pmValloc(__FILE__, __LINE__, x)
#define free(x)		__pmFree(__FILE__, __LINE__, x)
#define strdup(x)	__pmStrdup(__FILE__, __LINE__, x)

extern void *__pmMalloc(const char *, int, size_t);
extern void *__pmRealloc(const char *, int, void *, size_t);
extern void *__pmCalloc(const char *, int, size_t, size_t);
extern void *__pmValloc(const char *, int, size_t);
extern void __pmFree(const char *, int, void *);
extern char *__pmStrdup(const char *, int, const char *);

#endif

typedef struct {
    char 	*m_offset;
    pmDesc	m_desc;
} pmMeta;

/*
 * pmValue Pool Control Block
 */
typedef struct {
    int		p_valfmt;	/* set by foo_fetch() */
    int		p_nval;		/* ditto */
    int		p_maxnval;	/* size of the pmValue pool */
    pmValue	*p_vp;		/* the pmValue pool */
} pmVPCB;

extern int sizepool(pmVPCB *);
extern char *__hwg2inst(const pmInDom, char *);
extern int avcpy(pmAtomValue *, void *, int);

extern int		errno;

#define PMID(x,y,z) ((x<<22)|(y<<10)|z)

/*
 * InDom definitions
 */
typedef struct {
    int		i_indom;
    int		i_numinst;
} pmInDomDef;

typedef struct {
    int i_id;		/* instance id */
    char *i_name;	/* instance name */
} imap_t;

extern imap_t mbuftyp[]; /* defined in indom.c */

/*
 * these must be unique, must match what's in the forms and *.meta files,
 * and indomtab[] must be sized correctly
 */
#define PM_INDOM_DISK		0
#define PM_INDOM_CPU		1
#define PM_INDOM_NFSREQ		2
#define PM_INDOM_SWAP		3
#define PM_INDOM_LOADAV		4
#define PM_INDOM_NETIF		5
#define PM_INDOM_FILESYS	6
#define PM_INDOM_CNTRL		7
#define PM_INDOM_SHM		8
#define PM_INDOM_MSG		9
#define PM_INDOM_SEM		10
#define PM_INDOM_MBUF		11
#define PM_INDOM_NFS3REQ	12
#define PM_INDOM_SEMSET		13
#define PM_INDOM_ROUTER		14
#define PM_INDOM_ROUTERPORT	15
#define PM_INDOM_NODE		16
#define PM_INDOM_LPAGESIZE	17
#define PM_INDOM_XBOW		18
#define PM_INDOM_XBOWPORT	19
#define PM_INDOM_SOCKTYPE	20
#define PM_INDOM_SOCKSTATE	21
#define PM_INDOM_XLV		22

/*
 * update PM_INDOM_NEXT so table is sized correctly in indom.c
 */ 
#define PM_INDOM_NEXT	23
extern pmInDomDef	indomtab[];
extern int		numindom;

/* platform is SN0-based with nodes? */
extern int		_pm_has_nodes;

/* libirixpmda debug control */
extern int		pmIrixDebug;

/* platform has cells? */
extern int		_pm_numcells;

/*
 * add bit field macros as required ... use DBG_IRIX_DEBUG
 * for development debugging
 */
#define DBG_IRIX_DEBUG		1
#define DBG_IRIX_INDOM		2
#define DBG_IRIX_CPU		4
#define DBG_IRIX_DISK		8
#define DBG_IRIX_NODE		16
#define DBG_IRIX_FETCH		32
#define DBG_IRIX_XPC		64
#define DBG_IRIX_XBOW		128
#define DBG_IRIX_XLV		256
#define DBG_IRIX_FILESYS	512
#define DBG_IRIX_IF		1024
#define DBG_IRIX_MALLOC		2048

/*
 * add new prototypes in here
 */
extern void null_init(int reset);
extern int null_desc(pmID, pmDesc *);
extern void null_fetch_setup(void);
extern int null_fetch(pmID, __pmProfile *, pmVPCB *);

extern void minfo_init(int reset);
extern int minfo_desc(pmID, pmDesc *);
extern void minfo_fetch_setup(void);
extern int minfo_fetch(pmID, __pmProfile *, pmVPCB *);

extern void sysinfo_init(int reset);
extern int sysinfo_desc(pmID, pmDesc *);
extern void sysinfo_fetch_setup(void);
extern int sysinfo_fetch(pmID, __pmProfile *, pmVPCB *);

extern void disk_init(int reset);
extern int disk_desc(pmID, pmDesc *);
extern void disk_fetch_setup(void);
extern int disk_fetch(pmID, __pmProfile *, pmVPCB *);

extern void rpc_client_init(int reset);
extern int rpc_client_desc(pmID, pmDesc *);
extern void rpc_client_fetch_setup(void);
extern int rpc_client_fetch(pmID, __pmProfile *, pmVPCB *);

extern void rpc_server_init(int reset);
extern int rpc_server_desc(pmID, pmDesc *);
extern void rpc_server_fetch_setup(void);
extern int rpc_server_fetch(pmID, __pmProfile *, pmVPCB *);

extern void nfs_server_init(int reset);
extern int nfs_server_desc(pmID, pmDesc *);
extern void nfs_server_fetch_setup(void);
extern int nfs_server_fetch(pmID, __pmProfile *, pmVPCB *);

extern void nfs_client_init(int reset);
extern int nfs_client_desc(pmID, pmDesc *);
extern void nfs_client_fetch_setup(void);
extern int nfs_client_fetch(pmID, __pmProfile *, pmVPCB *);

extern void net_tcp_init(int reset);
extern int net_tcp_desc(pmID, pmDesc *);
extern void net_tcp_fetch_setup(void);
extern int net_tcp_fetch(pmID, __pmProfile *, pmVPCB *);

extern void kna_init(int reset);
extern int kna_desc(pmID, pmDesc *);
extern void kna_fetch_setup(void);
extern int kna_fetch(pmID, __pmProfile *, pmVPCB *);

extern void swap_init(int reset);
extern int swap_desc(pmID, pmDesc *);
extern void swap_fetch_setup(void);
extern int swap_fetch(pmID, __pmProfile *, pmVPCB *);

extern void sysmp_init(int reset);
extern int sysmp_desc(pmID, pmDesc *);
extern void sysmp_fetch_setup(void);
extern int sysmp_fetch(pmID, __pmProfile *, pmVPCB *);

extern void var_init(int reset);
extern int var_desc(pmID, pmDesc *);
extern void var_fetch_setup(void);
extern int var_fetch(pmID, __pmProfile *, pmVPCB *);

extern void syserr_init(int reset);
extern int syserr_desc(pmID, pmDesc *);
extern void syserr_fetch_setup(void);
extern int syserr_fetch(pmID, __pmProfile *, pmVPCB *);

extern void ncstats_init(int reset);
extern int ncstats_desc(pmID, pmDesc *);
extern void ncstats_fetch_setup(void);
extern int ncstats_fetch(pmID, __pmProfile *, pmVPCB *);

extern void getblkstats_init(int reset);
extern int getblkstats_desc(pmID, pmDesc *);
extern void getblkstats_fetch_setup(void);
extern int getblkstats_fetch(pmID, __pmProfile *, pmVPCB *);

extern void vnodestats_init(int reset);
extern int vnodestats_desc(pmID, pmDesc *);
extern void vnodestats_fetch_setup(void);
extern int vnodestats_fetch(pmID, __pmProfile *, pmVPCB *);

extern void igetstats_init(int reset);
extern int igetstats_desc(pmID, pmDesc *);
extern void igetstats_fetch_setup(void);
extern int igetstats_fetch(pmID, __pmProfile *, pmVPCB *);

extern void if_init(int reset);
extern int if_desc(pmID, pmDesc *);
extern void if_fetch_setup(void);
extern int if_fetch(pmID, __pmProfile *, pmVPCB *);

extern void hinv_init(int reset);
extern int hinv_desc(pmID, pmDesc *);
extern void hinv_fetch_setup(void);
extern int hinv_fetch(pmID, __pmProfile *, pmVPCB *);

extern void filesys_init(int reset);
extern int filesys_desc(pmID, pmDesc *);
extern void filesys_fetch_setup(void);
extern int filesys_fetch(pmID, __pmProfile *, pmVPCB *);

extern void cntrl_init(int reset);
extern int cntrl_desc(pmID, pmDesc *);
extern void cntrl_fetch_setup(void);
extern int cntrl_fetch(pmID, __pmProfile *, pmVPCB *);

extern void alldk_init(int reset);
extern int alldk_desc(pmID, pmDesc *);
extern void alldk_fetch_setup(void);
extern int alldk_fetch(pmID, __pmProfile *, pmVPCB *);

extern void percpu_init(int reset);
extern int percpu_desc(pmID, pmDesc *);
extern void percpu_fetch_setup(void);
extern int percpu_fetch(pmID, __pmProfile *, pmVPCB *);

extern void shm_init(int reset);
extern int shm_desc(pmID, pmDesc *);
extern void shm_fetch_setup(void);
extern int shm_fetch(pmID, __pmProfile *, pmVPCB *);

extern void msg_init(int reset);
extern int msg_desc(pmID, pmDesc *);
extern void msg_fetch_setup(void);
extern int msg_fetch(pmID, __pmProfile *, pmVPCB *);

extern void irixpmda_sem_init(int reset);
extern int irixpmda_sem_desc(pmID, pmDesc *);
extern void irixpmda_sem_fetch_setup(void);
extern int irixpmda_sem_fetch(pmID, __pmProfile *, pmVPCB *);

extern void xfs_init(int reset);
extern int xfs_desc(pmID, pmDesc *);
extern void xfs_fetch_setup(void);
extern int xfs_fetch(pmID, __pmProfile *, pmVPCB *);

extern void nfs3_server_init(int reset);
extern int nfs3_server_desc(pmID, pmDesc *);
extern void nfs3_server_fetch_setup(void);
extern int nfs3_server_fetch(pmID, __pmProfile *, pmVPCB *);

extern void nfs3_client_init(int reset);
extern int nfs3_client_desc(pmID, pmDesc *);
extern void nfs3_client_fetch_setup(void);
extern int nfs3_client_fetch(pmID, __pmProfile *, pmVPCB *);

extern void engr_init(int reset);
extern int engr_desc(pmID, pmDesc *);
extern void engr_fetch_setup(void);
extern int engr_fetch(pmID, __pmProfile *, pmVPCB *);

extern void evctr_init(int reset);
extern int evctr_desc(pmID, pmDesc *);
extern void evctr_fetch_setup(void);
extern int evctr_fetch(pmID, __pmProfile *, pmVPCB *);

extern void irixpmda_aio_init(int reset);
extern int irixpmda_aio_desc(pmID, pmDesc *);
extern void irixpmda_aio_fetch_setup(void);
extern int irixpmda_aio_fetch(pmID, __pmProfile *, pmVPCB *);

extern void hwrouter_init(int reset);
extern int hwrouter_desc(pmID, pmDesc *);
extern void hwrouter_fetch_setup(void);
extern int hwrouter_fetch(pmID, __pmProfile *, pmVPCB *);

extern void numa_init(int reset);
extern int numa_desc(pmID, pmDesc *);
extern void numa_fetch_setup(void);
extern int numa_fetch(pmID, __pmProfile *, pmVPCB *);

extern void node_init(int reset);
extern int node_desc(pmID, pmDesc *);
extern void node_fetch_setup(void);
extern int node_fetch(pmID, __pmProfile *, pmVPCB *);

extern void irixpmda_lpage_init(int reset);
extern int irixpmda_lpage_desc(pmID, pmDesc *);
extern void irixpmda_lpage_fetch_setup(void);
extern int irixpmda_lpage_fetch(pmID, __pmProfile *, pmVPCB *);

extern void pmda_init(int reset);
extern int pmda_desc(pmID, pmDesc *);
extern void pmda_fetch_setup(void);
extern int pmda_fetch(pmID, __pmProfile *, pmVPCB *);

extern void xbow_init(int reset);
extern int xbow_desc(pmID, pmDesc *);
extern void xbow_fetch_setup(void);
extern int xbow_fetch(pmID, __pmProfile *, pmVPCB *);
extern void xbow_start(int x);
extern void xbow_stop(int x);

extern void socket_init(int reset);
extern int socket_desc(pmID, pmDesc *);
extern void socket_fetch_setup(void);
extern int socket_fetch(pmID, __pmProfile *, pmVPCB *);

extern void hub_init(int reset);
extern int hub_desc(pmID, pmDesc *);
extern void hub_fetch_setup(void);
extern int hub_fetch(pmID, __pmProfile *, pmVPCB *);

extern void xlv_init(int reset);
extern int xlv_desc(pmID, pmDesc *);
extern void xlv_fetch_setup(void);
extern int xlv_fetch(pmID, __pmProfile *, pmVPCB *);

extern void stream_init(int reset);
extern int stream_desc(pmID, pmDesc *);
extern void stream_fetch_setup(void);
extern int stream_fetch(pmID, __pmProfile *, pmVPCB *);
