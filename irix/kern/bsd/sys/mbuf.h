#ifndef __SYS_MBUF_H__
#define __SYS_MBUF_H__
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (C) 1994 Silicon Graphics, Inc. All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)mbuf.h	7.10 (Berkeley) 2/8/88
 */
#ident	"$Revision: 3.32 $"

#include <sys/immu.h>

/*
 * Constants related to memory allocator.
 */
#if NBPP == 4096
#if _MIPS_SZLONG == 32
#define	MSIZE	    128			/* size of an mbuf */
#else
#define	MSIZE	    256			/* size of an mbuf */
#endif /* _MIPS_SZLONG == 32 */
#elif NBPP == 16384
#define	MSIZE	    256			/* size of an mbuf */
#elif !defined(_KERNEL)
#define MSIZE	128
#else
? ? ?	The size of a mbuf is dependent on the page size.
#endif
#define	MBUFCL_SIZE	2048		/* Size in bytes of cluster mbuf */

#define VCL_MAX	    CLBYTES		/* largest variable cluster */
#define	MCLBYTES    CLBYTES

#if _MIPS_SZPTR == 32
#define	MMINOFF	    16			/* mbuf header length */
#elif _MIPS_SZPTR == 64
#define	MMINOFF	    32			/* mbuf header length */
#else
? ? ?   We must know the size of pointers to compute the header size.
#endif

#define	MTAIL	    0
#define	MMAXOFF	    (MSIZE-MTAIL)	/* offset where data ends */
#define	MLEN	    (MSIZE-MMINOFF-MTAIL)   /* mbuf data length */

/*
 * This structure is used to define the sizes of various constants that are
 * shared between kernel and administrative utilities.
 */
struct mbufconst {
	int		m_msize;	/* MSIZE */
	int		m_mbufclsize;	/* MBUFCL_SIZE */
	int		m_mclbytes;	/* MCLBYTES */
	int		m_mminoff;	/* MMINOFF */
	int		m_mtail;	/* MTAIL */
	int		m_maxoff;	/* MMAXOFF */
	int		m_mlen;		/* MLEN */
};

/*
 * Macros for type conversion
 */

/* address in mbuf to mbuf head */
#define	datatom(x)	((struct mbuf *)((__psint_t)(x)-1 & ~(MSIZE-1)))
#define sotom(x)	datatom(x)
#define rttom(x)	datatom(x)
#define rcbtom(x)	datatom(x)
#define inpcbtom(x)	datatom(x)
#define tcptmplttom(x)	datatom(x)
#define unptom(x)	datatom(x)
#define mholdtom(x)	datatom(x)

/* mbuf head, to typed data */
#define	mtod(x,t)	((t)((__psint_t)(x) + (x)->m_off))

struct mbuf {
	struct	mbuf *m_next;		/* next buffer in chain */
	__psint_t m_off;		/* offset of data */
	struct	mbuf *m_act;		/* link in higher-level mbuf list */
	u_short	m_len;			/* amount of data in this mbuf */
	u_char	m_flags;		/* attributes */
	u_char	m_type;			/* mbuf type (MT_FREE == free) */
#if (_MIPS_SZPTR == 64)
	__int32_t m_index;		/* private/keep out */
#endif /* _MIPS_SZPTR == 64 */
	union {
		struct {		/* private/keep out */
#if defined(DEBUG) || defined(DEBUG_ALLOC)
#define	_FUNCPARAM	struct mbuf *, inst_t *
#else
#define	_FUNCPARAM	struct mbuf *
#endif
			void	(*mu_freefunc)(_FUNCPARAM);
			long	mu_farg;
			void	(*mu_dupfunc)(_FUNCPARAM);
			long	mu_darg;
#undef _FUNCPARAM
			caddr_t	mu_p;
			u_int	*mu_refp;
			u_int	mu_ref;
			u_int	mu_size;
		} m_us;
		u_char	mu_dat[MLEN];	/* data storage */
	} m_u;
};
#define	m_dat		m_u.mu_dat		/* compatability */
#define	m_freefunc	m_u.m_us.mu_freefunc
#define	m_farg		m_u.m_us.mu_farg
#define	m_dupfunc	m_u.m_us.mu_dupfunc
#define	m_darg		m_u.m_us.mu_darg
#define	m_cluster	m_flags			/* compatibility */

/*
 * mbuf types
 */
#define	MT_FREE		0	/* should be on free list */
#define	MT_DATA		1	/* dynamic (data) allocation */
#define	MT_HEADER	2	/* packet header */
#define	MT_SOCKET	3	/* socket structure */
#define	MT_PCB		4	/* protocol control block */
#define	MT_RTABLE	5	/* routing tables */
#define	MT_HTABLE	6	/* IMP host tables */
#define	MT_ATABLE	7	/* address resolution tables */
#define	MT_SONAME	8	/* socket name */
#define	MT_SAT		9	/* security audit trail */
#define	MT_SOOPTS	10	/* socket options */
#define	MT_FTABLE	11	/* fragment reassembly header */
#define	MT_RIGHTS	12	/* access rights */
#define	MT_IFADDR	13	/* ipv4 interface address */

#define	MT_DN_DRBUF	14	/* 4DDN driver buffer allocation (OBSOLETE) */
#define	MT_DN_BLK	15	/* 4DDN block allocation (OBSOLETE) */
#define	MT_DN_BD	16	/* 4DDN buffer to mbuf (OBSOLETE) */

#define MT_MCAST_RINFO	16	/* number multicast route info struct in use */
#define MT_IPMOPTS	17	/* internet multicast options */
#define MT_MRTABLE	18	/* multicast routing tables */

#define MT_SESMGR	19	/* session manager security attributes */
#define MT_IFADDR6	20	/* ipv6 interface address */

#define MT_MAX		21	/* 'largest' type + 1 */

/*
 * mbuf flags attributes (MUST be bit assigned, 8-bit wide field)
 */
#define MCL_CLUSTER	0x01	/* undistinguished cluster (compatability) */
#define M_CLUSTER	0x01	/* undistinguished cluster mbuf */

#define M_CKSUMMED	0x02	/* link-level handles checksums */
#define M_BCAST		0x04	/* send/received as link-level broadcast */
#define M_MCAST		0x08	/* send/received as link-level multicast */
#define M_SHARED	0x10	/* shared page (COW) cluster mbuf */
#define M_LOANED	0x20	/* foreign cluster buffer */
#define M_COPYFLAGS	(M_CKSUMMED|M_MCAST|M_BCAST)

/*
 * wait flags
 */
#define	M_DONTWAIT	0
#define	M_WAIT		1

/*
 * Length to m_copy to copy all
 */
#define	M_COPYALL	1000000000

/*
 * m_pullup() will pull up additional length if convenient;
 * should be enough to hold headers of second-level and higher protocols.
 */
#define	MPULL_EXTRA	32

/*
 * 4.3BSD compatiblity macros
 */
#define	MFREE(m, n)	((n) = m_free(m))
#define	MGET(m, i, t)	((m) = m_get(i, t))
#define	MGETHDR(m, i, t) ((m) = m_get(i, t))

#define MH_ALIGN(m, len) {(m)->m_off = (MMAXOFF - (len)) &~ (sizeof(long) - 1);}
#define	M_HASCL(m)	((m)->m_flags & M_CLUSTER)
#define M_TRAILINGSPACE(m) (M_HASCL(m) ? 0 : MLEN - ((m)->m_off + (m)->m_len))

#if defined(DEBUG) || defined(DEBUG_ALLOC)
#define	GOODMT(t) ASSERT(((t) > MT_FREE) && ((t) < MT_MAX))
#define GOODMP(m) ASSERT((IS_KSEG0(m) || IS_KSEG2(m)) && ((__psint_t)(m) % MSIZE) == 0)
#else
#define GOODMT(t)
#define GOODMP(m)
#endif

#ifdef	_KERNEL
extern void m_adj(struct mbuf *m, int len);
extern void m_cat(struct mbuf *m, struct mbuf *n);
extern struct mbuf *m_copy(struct mbuf *m, int off, int len);
extern int m_datacopy(struct mbuf *m, int off, int maxlen, caddr_t cp);
extern int m_flip(struct mbuf *m, caddr_t ubase, int do_sync);
extern struct mbuf *m_shget(int canwait, int type, caddr_t vaddr, int size,
			    int do_sync);
extern void m_sync(struct mbuf *m);
extern struct mbuf *m_free(struct mbuf *m);
extern int m_stealcl(struct mbuf *m);
extern void m_freem(struct mbuf *m);
extern struct mbuf *m_get(int canwait, int type);
extern struct mbuf *m_getclr(int canwait, int type);
extern struct mbuf *m_vget(int canwait, int size, int type);
extern int m_length(struct mbuf *m);
extern struct mbuf *m_pullup(struct mbuf *n, int len);
extern void *mcb_get(int canwait, int size, int type);
extern void mcb_free(void *mcb, int size, int type);
extern void m_trim(struct mbuf *m, int len);
extern struct mbuf *mclgetx(void (*dfun)(), long darg, void (*ffun)(), long farg,
	caddr_t addr, int len, int wait);
extern int m_hasroom(struct mbuf *m, int len);
#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_MBUF_H__ */
