/*
 * listvec.h
 *
 * This file comes directly from /proj/banyan/isms/irix/kern/bsd/misc.  It
 * should be kept up-to-date as much as possible from banyan to ficus to
 * nexus, etc., etc., etc.  Unfortunately, these structures aren't defined
 * in the kernel, but we need them to do some of the stuff for listing all
 * mbufs, routes, etc.  I've also included some of the definitions for
 * MBUF_??* from kern_mbuf.c.
 */
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/listvec.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#ifndef __SYS_LISTVEC_H__
#define __SYS_LISTVEC_H__

/*
 * Constants for various node size's for the vectors associated with each cpu.
 */
#define MBUF_CPI_INDEX          0
#define MBUF_CPI_OFFSET         0

#define MBUF_INDEX              1
#define MBUF_OFFSET             sizeof(struct lf_listvec)*MBUF_INDEX

#define MBUF_MISC_INDEX         2
#define MBUF_MISC_OFFSET        sizeof(struct lf_listvec)*MBUF_MISC_INDEX

#define MBUFCL_INDEX            3
#define MBUFCL_OFFSET           (sizeof(struct lf_listvec)*MBUFCL_INDEX)

#define MBUFCL_PAGE_INDEX       4
#define MBUFCL_PAGE_OFFSET      (sizeof(struct lf_listvec)*MBUFCL_PAGE_INDEX)

#define MBUF_INDEXMAX   MBUFCL_PAGE_INDEX + 1

/*
 * Lock Free (lf) structure definitions
 */
struct lf_elt {
	struct	lf_elt *next;
};

/*
 * element structure for global list of bristles, list of lists
 */
struct global_elt {
	struct global_elt *next, *remain;
};

/*
 * element structure for a list vector.
 */
struct lf_listvec {
	char 	*get;		/* Head of get free node list */
	char 	*put;		/* Head of put free node list */
	char	*reclaim_page;	/* base address of page we're scavenging */

	u_int	getcnt;		/* number elts on local get list */
	u_int	putcnt;		/* number elts on local put list */
	u_int	nelts;		/* number of elts of reclaimed page so far */

	short	threshold;	/* threshold for local put to global put xfer*/
	short	size;		/* node size in bytes (Power of 2) */
	short	offset;		/* byte offset from start to this entry */
	short	e_per_page;	/* number of elts per page to reclaim */

	lock_t	lock;		/* lock (Used in MP machines) */

	__psint_t node_mask;	/* address mask for debugging free ops */

	u_int	get_calls;	/* num calls to lf_get() */
	u_int	get_lget;	/* num times local get list immed success */
	u_int	get_lput_to_lget; /* num xfers of local put to local get */
	u_int	get_gget_to_lget; /* num xfers from global get to local get*/
	u_int	get_gput_to_gget; /* num xfers from global put to glob get */
	u_int	get_totalfails;  /* num of all-lists empty failures */
	u_int	get_addpage;     /* # times added new page to get list */
	u_int	get_nopage;      /* # times failed to add new page get list */

	u_int	free_calls;	/* number calls to lf_free */
	u_int	free_nullnode;	/* num times null node address passed in */
	u_int	free_greclaim;	/* num times node reclaimed to global vector */
	u_int	free_lput;	/* num times free directly to local put list */
	u_int	free_lput_to_gput; /* # free's where local put to global put */
};

/*
 * NOTE: The constant below is the number of slots in the lf vectors
 * for the clust page info notes, and for all the various size mbuf's
 * It is also defined in mbuf.h and so any change in either place
 * must be reflect in the other. The only reason for having two symbols
 * is to avoid including the mbuf.h header file to compile this code.
 */
#define LF_NETVECTORMAX 5

/*
 * Kernel network activity statistics for the TCP/IP protocol family
 */
struct knetvec {
	struct lf_listvec *lfvectors[LF_NETVECTORMAX];
};

extern int mbuf_init_slave(struct knetvec *);

/*
 * Debug mbuf procedure call statistics
 */
struct mbuf_procstats {
	u_int m_adj_calls;		/* Number calls */
	u_int m_adj_head;		/* # times adjusted from head */
	u_int m_adj_tail;		/* # times adjusted from tail */

	u_int m_cat_calls;		/* Number calls */
	u_int m_copy_calls;		/* Number calls */
	u_int m_datacopy_calls;		/* Number calls */

	u_int m_get_calls;		/* Number calls */
	u_int m_get_calls_fail;		/* # times returned failure */

	u_int m_getclr_calls;		/* Number calls */

	u_int m_getpage_calls;		/* Number calls to m_getpage_tiled */
	u_int m_getpage_calls_reclaim; /* # times got reclaimed page */
	u_int m_getpage_calls_newpage; /* # times got new page */
	u_int m_getpage_calls_failed;  /* # times totally failed */
	u_int m_getpage_calls_maxwait; /* # times over limit and waited */
	u_int m_getpage_calls_maxnowait; /* # times over limit and no wait */

	u_int m_flip_calls;		/* Number calls */
	u_int m_flip_calls_ok;		/* # calls failed succeeded */
	u_int m_flip_calls_optoff;	/* # calls failed as m_flip disabled */
	u_int m_flip_calls_cluster;	/* # calls failed by NO cluster */
	u_int m_flip_calls_refct;	/* # calls failed with refct > 1 */
	u_int m_flip_calls_pflip;	/* # calls failed as pflip failed */

	u_int m_free_calls;		/* Number calls */
	u_int m_free_mbuf;		/* # times called with regular mbuf */
	u_int m_free_cluster;		/* # times called with cluster mbuf */

	u_int m_freem_calls;		/* Number calls */

	u_int m_dupfunction_calls;	/* Number calls */

	u_int m_freefunction_calls;	/* Number calls */
	u_int m_freefunction_shared;	/* # times shared page */
	u_int m_freefunction_clusters;	/* # times cluster mbuf */
	u_int m_freefunction_pages;	/* # times with pages */
	u_int m_freefunction_dec;	/* # times to decrement ref ct */

	u_int m_freepage_calls;		/* Number calls to m_freepage() */
	u_int m_freepage_release;	/* # times page released to kernel */
	u_int m_freepage_reclaim;	/* # times page added to reclaim list*/
	u_int m_freepage_recycle;	/* # times page recycled for use */

	u_int m_vget_calls;		/* Number calls to m_vget_internal */
	u_int m_vget_mbuf;		/* # times small mbuf returned */
	u_int m_vget_cluster;		/* # times clusters returned */
	u_int m_vget_page;		/* # times pages returned */
	u_int m_vget_nombuf;		/* # times no mbuf header available */
	u_int m_vget_nocpi;		/* # times no cpi ref node available */
	u_int m_vget_nobuf;		/* # times no buffer available */

	u_int m_shget_calls;		/* Number calls */
	u_int m_shget_calls_ok;		/* # calls failed succeeded */
	u_int m_shget_calls_optoff;	/* # calls failed as shget disabled */
	u_int m_shget_calls_nombuf;	/* # calls failed by NO mbuf hdr */
	u_int m_shget_calls_nocpi;	/* # calls failed by NO cpi node */
	u_int m_shget_calls_pattach;	/* # calls failed via pattach fail */

	u_int m_length_calls;		/* Number calls to m_length() */
	u_int m_pullup_calls;		/* Number calls to m_pullup() */

	u_int m_trim_calls;		/* Number calls */
	u_int m_trim_zerolen;		/* # times zero length returned */
	u_int m_trim_newlen;		/* # times simple new length case */

	u_int lf_addpage_calls;		/* Number calls */
	u_int lf_addpage_empty;		/* # times get list head null */
	u_int lf_addpage_occupied;	/* # times get list head non-zero */
};
#endif /* __SYS_LISTVEC_H__ */
