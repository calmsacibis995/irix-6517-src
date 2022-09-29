/* mbuf.h - mbuf header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      @(#)mbuf.h      7.8.1.3 (Berkeley) 2/14/89
 */

/*
modification history
--------------------
02t,12aug94,dzb  cleared cfreeRtn ptr in MCLFREE (SPR #3177).
02s,15mar94,dzb  Fixed bcopy_from_mbufs() to allign odd data lengths (SPR #3094)
02r,31aug93,jag  Fixed M_FREE macro to account for multiple tasks (SPR #2497)
02q,22sep92,rrr  added support for c++
02p,26may92,rrr  the tree shuffle
02o,01mar92,elh  added declaration for m_freem.
02n,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
02m,10jun91.del  added pragma for gnu960 alignment.
02l,18may91,gae  removed declarations of bcopy,etc., included string.h.
02k,01may91,elh  added MC_EI for 596 buffer loaning.
02j,08mar91,elh  added mbufConfig and clusterConfig to make the mbuf
		 parameters user configurable.  Also added
		 NUM_CLUSTERS_TO_EXPAND and MAX_CLUSTERS.
02i,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02h,26jun90,hjb  modified copy_from_mbufs() and bcopy_from_mbufs()
		 to call m_freem ().
02g,07may90,hjb  deleted cl_index and changed cl_refcnt to be char * in
		 struct mbuf, and made corresponding mods to macros.
02f,19apr90,hjb  mbuf structure has changed significantly...hacked to almost
		 beyond recognition, new cluster implementation, cluster
		 typing, driver level callback support, added copy_from_mbufs
		 and bcopy_from_mbufs.
02e,18mar90,hjb  added cluster support.
02d,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  moved mfree and mbstat definitions to uipc_mbuf.c.
02c,28aug87,dnw  removed unnecessary defines.
		 added include of types.h
02b,03apr87,ecs  added header and copyright.
02a,02feb87,jlf  removed ifdef CLEAN
*/

#ifndef __INCmbufh
#define __INCmbufh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "sys/types.h"
#include "net/unixlib.h"
#include "semlib.h"
#include "string.h"

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */


/* imported stuff */

IMPORT struct mbstat 	mbstat;
IMPORT struct mbuf 	*mfree;
IMPORT struct mbuf 	*mclfree;
IMPORT int 		m_want;
IMPORT SEM_ID 		mbufSem;
IMPORT u_char 		mclrefcnt[];

IMPORT struct mbuf 	*m_get();
IMPORT struct mbuf 	*m_getclr();
IMPORT struct mbuf 	*m_free();
IMPORT struct mbuf 	*m_more();
IMPORT struct mbuf 	*m_copy();
IMPORT struct mbuf 	*m_pullup();
IMPORT struct mbuf 	*m_clget();
IMPORT caddr_t 		m_clalloc();
IMPORT void 		m_freem ();

IMPORT struct mbuf 	*bcopy_to_mbufs ();
IMPORT struct mbuf	*build_cluster ();

/* Constants */

#define	MSIZE			128 	/* size of an mbuf */
#define MCLBYTES        	1024	/* size of a cluster mbuf */
#define NUM_INIT_CLUSTERS       4 	/* number of clusters to initialize */
#define NUM_INIT_MBUFS		40	/* number of mbufs to initialize */
#define NUM_MBUFS_TO_EXPAND	10	/* number of mbufs to add each time */
#define NUM_CLUSTERS_TO_EXPAND	1	/* number of clusters to add */
#define MAX_MBUFS		1500
#define MAX_CLUSTERS		256	/* max number of clusters */
#define MAX_MBUF_TYPES		256	/* max number of mbuf types */

#define	MMINOFF		12			/* mbuf header length */
#define	MTAIL		4			/* len of data after m_dat */
#define	MMAXOFF		(MSIZE-MTAIL)		/* offset where data ends */
#define	MLEN		(MSIZE-MMINOFF-MTAIL)	/* mbuf data length */

#define USE_CLUSTER(size)	((size) >= 512 ? TRUE : FALSE)

/* Ubiquitous mbuf */

struct mbuf
    {
    struct	mbuf *m_next;		/* next buffer in chain */
    u_long	m_off;			/* offset of data */
    short	m_len;			/* amount of data in this mbuf */
    u_char	m_type;			/* mbuf type (0 == free) */
    u_char	m_ctype;       		/* cluster type */

    union
	{
	u_char	m_space[MLEN]; 		/* data storage */

	struct				/* cluster description */
	    {
	    struct mbuf	*cl_buf;	/* the cluster buffer */
	    u_char 	*cl_refcnt;	/* reference count */
	    FUNCPTR	cl_freeRtn;	/* driver level free routine */
	    int		cl_arg1;
	    int		cl_arg2;
	    int		cl_arg3;
            short	cl_len;		/* original length of mbuf */

	    } m_cspace;
	} m_union;

    struct	mbuf *m_act;		/* link in higher-level mbuf list */
    };

#define m_dat		m_union.m_space
#define m_cluster	m_union.m_cspace

#define m_cfreeRtn	m_cluster.cl_freeRtn
#define m_cbuf		m_cluster.cl_buf
#define m_refcnt	m_cluster.cl_refcnt
#define m_arg1		m_cluster.cl_arg1
#define m_arg2		m_cluster.cl_arg2
#define m_arg3		m_cluster.cl_arg3
#define m_olen		m_cluster.cl_len

/* Mbuf statistics. */

struct mbstat
    {
    u_long  m_mbufs;        /* mbufs obtained from page pool */
    u_long  m_clusters;     /* clusters obtained from page pool */
    u_long  m_space;        /* interface pages obtained from page pool */
    u_long  m_clfree;       /* free clusters */
    u_long  m_drops;        /* times failed to find space */
    u_long  m_wait;         /* times waited for space */
    u_long  m_drain;        /* times drained protocols for space */
    u_short m_mtypes[MAX_MBUF_TYPES];  /* type specific mbuf allocations */
    };

/*
 *  This structure defines the way the networking code manipulates
 *  mbufs and mbuf clusters.  The defaults are defined above, but the
 *  structure is defined and filled in usrConfig.c.
 */

typedef struct
    {
    int initialAlloc;		/* initial number of mbufs allocated */
    int incrementAlloc;		/* mbuf increment */
    int maxAlloc;		/* maximum number of mbufs allocated */
    int memPartition;		/* memory partition to allocate mbufs */
    int memPartitionSize;	/* memory partition size */
    }  MBUF_CONFIG;

IMPORT MBUF_CONFIG	 mbufConfig;		 /* mbuf configuartion */
IMPORT MBUF_CONFIG	 clusterConfig; 	 /* cluster configuration */

/* address in mbuf to mbuf head */

#define dtom(x)         ((struct mbuf *)((int)x & ~(MSIZE-1)))

/* mbuf head, to typed data */

#define mtod(x,t)       ((t)((int)(x) + (x)->m_off))

/* mbuf types */

#define	MT_FREE		((u_char) 0)	/* should be on free list */
#define	MT_DATA		((u_char) 1)	/* dynamic (data) allocation */
#define	MT_HEADER	((u_char) 2)	/* packet header */
#define	MT_SOCKET	((u_char) 3)	/* socket structure */
#define	MT_PCB		((u_char) 4)	/* protocol control block */
#define	MT_RTABLE	((u_char) 5)	/* routing tables */
#define	MT_HTABLE	((u_char) 6)	/* IMP host tables */
#define	MT_ATABLE	((u_char) 7)	/* address resolution tables */
#define	MT_SONAME	((u_char) 8)	/* socket name */
#define	MT_ZOMBIE	((u_char) 9)	/* zombie proc status */
#define	MT_SOOPTS	((u_char) 10)	/* socket options */
#define	MT_FTABLE	((u_char) 11)	/* fragment reassembly header */
#define	MT_RIGHTS	((u_char) 12)	/* access rights */
#define	MT_IFADDR	((u_char) 13)	/* interface address */

#define NUM_MBUF_TYPES	14		/* number of mbuf types defined */

/* cluster types (m_ctype) */

#define MC_NO_CLUSTER	((u_char) 0)
#define MC_CLUSTER	((u_char) 1)
#define MC_LANCE	((u_char) 2)	/* 0x02 - 0x80 for netif */
#define MC_BACKPLANE	((u_char) 3)
#define MC_EI		((u_char) 4)
#define MC_UCLUSTER	((u_char) 0x80)	/* 0x80 - 0xff for network code */

#define M_HASCL(m)	((m)->m_ctype != MC_NO_CLUSTER)

/* flags to m_get */

#define	M_DONTWAIT	0
#define	M_WAIT		1

/* flags to m_clalloc */

#define	MPG_MBUFS	0		/* put new mbufs on free list */
#define	MPG_CLUSTERS	1		/* put new clusters on free list */
#define	MPG_SPACE	2		/* don't free; caller wants space */

/* length to m_copy to copy all */

#define	M_COPYALL	1000000000

/* m_pullup will pull up additional length if convenient;
 * should be enough to hold headers of second-level and higher protocols.
 */

#define	MPULL_EXTRA	32

/* MGET -- used get a fresh mbuf */

#define MGET(m, i, t) 							\
    { 									\
    int ms = splimp(); 							\
    if ((m)=mfree) 							\
	{ 								\
	if ((m)->m_type != MT_FREE) 					\
	    panic("mget"); 						\
	(m)->m_type = t; 						\
	(m)->m_ctype = MC_NO_CLUSTER; 					\
	mbstat.m_mtypes[MT_FREE]--; 					\
	mbstat.m_mtypes[t]++; 						\
        mfree = (m)->m_next; 						\
	(m)->m_next = 0; 						\
        (m)->m_off = MMINOFF; 						\
	} 								\
    else 								\
	(m) = m_more((int) (i), (int) (t));				\
    splx(ms); 								\
    }

/* Mbuf cluster macros. */

/* MCLALLOC allocates mbuf page clusters. */

#define MCLALLOC(m, i) 							\
    { 									\
    int ms = splimp(); 							\
    if (mclfree == 0) 							\
	(void)m_clalloc((i), MPG_CLUSTERS, M_DONTWAIT); 		\
    if ((m)=mclfree) 							\
	{ 								\
	++(*(m)->m_refcnt);		 				\
	mbstat.m_clfree--; 						\
	mclfree = (m)->m_next; 						\
	} 								\
    splx(ms); 								\
    }

/* MCLGET adds such clusters to a normal mbuf.
 * m->m_len is set to MCLBYTES upon success, and to MLEN on failure.
 */

#define MCLGET(m) 							\
    { 									\
    struct mbuf *p; 							\
    MCLALLOC(p, clusterConfig.incrementAlloc); 				\
    if (p) 								\
	{ 								\
	(m)->m_off = (int) p - (int) (m); 				\
	(m)->m_len = MCLBYTES; 						\
	(m)->m_ctype = MC_CLUSTER; 					\
	(m)->m_cluster = p->m_cluster; 					\
	(m)->m_cfreeRtn = NULL;	 					\
	(m)->m_olen = MCLBYTES;	 					\
	} 								\
    else 								\
	(m)->m_len = MLEN; 						\
    }

/* MCLFREE frees clusters allocated by MCLALLOC. */

#define MCLFREE(m) 							\
    { 									\
    if (--(*(m)->m_refcnt) == 0)		 			\
	{ 								\
	if ((m)->m_cfreeRtn != NULL) 					\
	    (*(m)->m_cfreeRtn) ((m)->m_arg1, (m)->m_arg2, (m)->m_arg3);	\
	else 								\
	    { 								\
	    struct mbuf *p = (m)->m_cbuf; 				\
	    p->m_cluster = (m)->m_cluster; 				\
	    p->m_next = mclfree; 					\
	    mclfree = p; 						\
	    mbstat.m_clfree++; 						\
	    } 								\
	} 								\
    }

/* MFREE free a given mbuf and returns it to the free list.
 * If a cluster is contained inside the mbuf, MFREE will call MCLFREE
 * to free that cluster as well.
 *
 * N.B.: If this MFREE macro changes you must also make corresponding changes
 * to m_freem () routine inside uipc_mbuf.c.
 */

#define MFREE(m, n) 							\
    { 									\
    int ms = splimp(); 							\
    if ((m)->m_type == MT_FREE) 					\
	panic("mfree"); 						\
    mbstat.m_mtypes[(m)->m_type]--; 					\
    mbstat.m_mtypes[MT_FREE]++; 					\
    (m)->m_type = MT_FREE; 						\
    if (M_HASCL(m)) 							\
	MCLFREE(m); 							\
    (n) = (m)->m_next; 							\
    (m)->m_next = mfree; 						\
    (m)->m_off = 0; 							\
    (m)->m_act = 0; 							\
    mfree = (m); 							\
    splx(ms); 								\
    if (m_want>0) 							\
	{ 								\
	m_want--; 							\
	wakeup(mbufSem); 						\
	} 								\
    }

/* SunOS compatible macros to be used in network interface drivers */

/* copy_from_mbufs copies data from mbuf chain to user supplied buffer area.
 *
 * N.B.: This macro takes an extra argument 'len' which is used to return
 * the number of bytes actually transferred.  SunOS routine copy_from_mbufs
 * returns this value; VxWorks macro copy_from_mbufs "returns" the same
 * value in 'len'.
 */

#define copy_from_mbufs(buf0, m, len) 					\
    { 									\
    FAST char *p = (char *) (buf0); 					\
    FAST struct mbuf *m0 = m;						\
    for (; (m) != NULL; p += (m)->m_len, (m) = (m)->m_next) 		\
        bcopy (mtod ((m), char *), p, (m)->m_len); 			\
    (len) = (int) p - (int) (buf0); 					\
    m_freem (m0);							\
    }

/* bcopy_from_mbufs copies dat from mbuf chain to user supplied buffer area
 * by transferring bytes by unit 'width' indicated by the user.
 * This macro is similiar to copy_from_mbufs; the only difference is the
 * extra argument 'width' which is used to accommodate certain hardware
 * restrictions that require copying of data to only occur at certain byte size
 * boundaries.  Calling this macro with 'width' value set to NONE is equivalent
 * to calling copy_from_mbufs macro.
 */

#define bcopy_from_mbufs(buf0, m, len, width) 				\
    { 									\
    FAST char *p = (char *) (buf0); 					\
    FAST struct mbuf *m0 = m;						\
    FAST char *adjBuf;							\
    FAST int adjLen;							\
    FAST int odd = 0;							\
    FAST int ix;							\
    char temp[4];							\
    if ((width) == NONE) 						\
        for (; (m) != NULL; p += (m)->m_len, (m) = (m)->m_next) 	\
            bcopy (mtod ((m), char *), p, (m)->m_len); 			\
    else if ((width) == 1) 						\
	for (; (m) != NULL; p += (m)->m_len, (m) = m->m_next) 		\
	    bcopyBytes (mtod ((m), char *), p, (m)->m_len); 		\
    else if ((width) == 2) 						\
	for (; (m) != NULL; (m) = m->m_next)		 		\
	    {								\
	    adjLen = (m)->m_len;					\
	    adjBuf = mtod((m), char *);					\
	    if (odd > 0)						\
		{							\
		--p;							\
		*((UINT16 *) temp) = *((UINT16 *) p);			\
		temp [1] = *adjBuf++;					\
		--adjLen;						\
		*((UINT16 *) p) = *((UINT16 *) temp);			\
		p += 2;							\
		}							\
	    bcopyWords (adjBuf, p, (adjLen + 1) >> 1);			\
	    p += adjLen;						\
            odd = adjLen & 0x1;						\
	    }								\
    else if ((width) == 4) 						\
	for (; (m) != NULL; (m) = m->m_next)		 		\
	    {								\
	    adjLen = (m)->m_len;					\
	    adjBuf = mtod((m), char *);					\
	    if (odd > 0)						\
		{							\
		p -= odd;						\
		*((UINT32 *) temp) = *((UINT32 *) p);			\
		for (ix = odd; ix < 4; ix++, adjBuf++, --adjLen)	\
		    temp [ix] = *adjBuf;				\
		*((UINT32 *) p) = *((UINT32 *) temp);			\
		p += 4;							\
		}							\
	    bcopyLongs (adjBuf, p, (adjLen + 3) >> 2);			\
	    p += adjLen;						\
            odd = adjLen & 0x3;						\
	    }								\
    else 								\
        panic ("bcopy_from_mbufs");					\
    (len) = (int) p - (int) (buf0); 					\
    m_freem (m0);							\
    }

/* copy_to_mbufs copies data into mbuf chain and returns the pointer to
 * the first mbuf.
 *
 * N.B.: This macro calls a routine bcopy_to_mbufs with width value set
 * to NONE.  VxWorks routine bcopy_to_mbufs is provided to accommodate
 * hardware restrictions that require copying of data to only
 * occur at certain byte size boundaries.
 */

#define copy_to_mbufs(buf, totlen, off, ifp) 				\
    bcopy_to_mbufs (buf, totlen, off, ifp, NONE)

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCmbufh */
