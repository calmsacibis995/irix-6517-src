/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_BUF_H__
#define __SYS_BUF_H__

#ident	"$Revision: 3.110 $"
#include <sys/sema.h>
#include <sys/uio.h>

struct ktrace;
struct dhbuf;
struct bdevsw;

/*
 *	Structure used to get to the device driver, which can either be
 *	via a spec vnode, or the bdevsw entry. We cache the bdevsw entry
 *	in order to avoid using the hardware graph to find it in the
 *	get_bdevsw macro. The spec vp is used in the cellular case where
 *	we have to use VOP_STRATEGY in place of bdstrat.
 */

typedef struct buftarg {
	struct vnode	*specvp;
	struct bdevsw	*bdevsw;
	dev_t		dev;
} buftarg_t;

/*
 *	Each buffer in the pool is usually doubly linked into 2 lists:
 *	the device with which it is currently associated (always)
 *	and also on a list of blocks available for allocation
 *	for other use (usually).
 *	The latter list is kept in last-used order, and the two
 *	lists are doubly linked to make it easy to remove
 *	a buffer from one list when it was found by
 *	looking through the other.
 *	A buffer is on the available list, and is liable
 *	to be reassigned to another disk block, if and only
 *	if it is not marked BUSY.  When a buffer is busy, the
 *	available-list pointers can be used for other purposes.
 *	Most drivers use the forward ptr as a link in their I/O active queue.
 *	A buffer header contains all the information required to perform I/O.
 *	Most of the routines which manipulate these things are in fs_bio.c.
 */

typedef struct	buf {
	/*
	 * These first 4 fields must match the hbuf definition
	 * below.  DO NOT CHANGE THEM OR REARRANGE THEM.
	 */
	sema_t	b_lock;			/* lock for buffer usage */
	uint64_t b_flags;		/* see defines below */
	struct	buf *b_forw;		/* headed by d_tab of conf.c */
	struct	buf *b_back;		/*  "  */
	struct  buf *bd_forw;		/* device based hash chain */
	struct  buf *bd_back;		/*  "  */
	struct  dhbuf *bd_hash;		/* hash bucket head */
	struct	buf *av_forw;		/* position on free list, */
	struct	buf *av_back;		/*     if not BUSY */
	dev_t	b_edev;			/* major+minor device name */
	int	b_error;		/* returned after I/O */
	off_t	b_offset;		/* vnode offset (in basic blocks) */
	buftarg_t *b_target;		/* route to I/O device */
	unsigned b_bcount;		/* transfer count */
	unsigned b_resid;		/* words not transferred after error */
	unsigned b_remain;		/* virt b_bcount for PAGEIO use only */
	unsigned b_bufsize;		/* size in bytes of allocated buffer */
	__psunsigned_t b_sort;		/* key with which to sort on queue */
	union {
	    caddr_t b_addr;		/* low order core address */
	    int	*b_words;		/* words for clearing */
	    struct pfdat *b_pfdat;	/* pointer into b_pages list */
	    daddr_t *b_daddr;		/* disk blocks */
	} b_un;
	struct vnode *b_vp;		/* object associated with bp */
	daddr_t	b_blkno;		/* block # on device */
	clock_t	b_start;		/* request start time */
	struct  pfdat *b_pages;		/* page list for PAGEIO */
	void	*b_alenlist;		/* address, length lists */
	void (*b_relse)(struct buf *);	/* function called by brelse */
	sema_t	b_iodonesema;		/* lock for waiting on I/O done */
	void (*b_iodone)(struct buf *);	/* function called by iodone */
	int (*b_bdstrat)(struct buf *); /* function called by bwrite */
	void	*b_private;		/* for driver's use */
	void	*b_fsprivate;		/* private ptr for file systems */
	void	*b_fsprivate2;		/* private ptr for file systems */
	void	*b_fsprivate3;		/* private ptr for file systems */
	short	b_pincount;		/* count of times buf is pinned */
	ushort	b_pin_waiter;		/* someone waiting for unpin? */
	char	b_ref;			/* # of free trips through freelist */
	char	b_balance;		/* tree balance */
	char	b_listid;		/* free list number */
	char	b_bvtype;		/* bufview flags */
	struct 	buf *b_dforw;		/* vnode delwri chain */
	struct 	buf *b_dback;		/* vnode delwri chain */
	struct	buf *b_parent;		/* hash parent pointer */
	union {
		void	*b_gr_prv;	/* private data for grio */
		short	b_pri;		/* io priority level */
	} b_gr;
	struct  buf *b_grio_list;	/* list of bps used in grio */
#ifdef DEBUG_BUFTRACE
	struct ktrace	*b_trace;	/* per buffer trace buffer */
#endif	
} buf_t;

typedef int     opaque_t;

#define paddr(X)	(paddr_t)(X->b_un.b_addr)
#define	b_dmaaddr	b_un.b_addr
#define	b_page		b_un.b_pfdat
#define b_grio_private	b_gr.b_gr_prv
#define b_iopri		b_gr.b_pri

extern buf_t		*global_buf_table;	/* The buffer pool itself */
extern buf_t		*bfreelist;		/* head of available lists */
extern int		bfreelistmask;		/* mask for freelist nums */


struct	pfree	{
	uint64_t	b_flags;
	struct	buf	*av_forw;
	struct	buf	*av_back;
};

/*
 * Flags for buffer usage statistics (bufview).
 * Stored in b_bvtype.
 */
typedef enum {
	B_FS_NONE,
	B_FS_INO,
	B_FS_INOMAP,
	B_FS_DIR_BTREE,
	B_FS_MAP,
	B_FS_ATTR_BTREE,
	B_FS_AGI,
	B_FS_AGF,
	B_FS_AGFL,
	B_FS_DQUOT
} bvtype_t;

#define BINFO_NAME_SIZE 32

typedef struct binfo {
	uint64_t	binfo_flags;
	int		binfo_size;
	dev_t		binfo_dev;
	clock_t		binfo_start;
	int		binfo_fstype;
	char		binfo_fpass;
	char		binfo_bvtype;
	short		binfo_futs;	/* pad */
	int		binfo_futi;	/* pad */
	union	{
		off64_t	bun_off;
		daddr_t	bun_blk;
	} binfo_union2;
	uint64_t	binfo_vp;
	uint64_t	binfo_vnumber;
	char		binfo_name[BINFO_NAME_SIZE];
} binfo_t;

#define binfo_offset	binfo_union2.bun_off
#define binfo_blkno	binfo_union2.bun_blk

/*
 *	These flags are kept in b_flags.
 */
#define B_WRITE    0x0000000000000000LL	/* non-read pseudo-flag */
#define B_READ     0x0000000000000001LL	/* read when I/O occurs */
#define B_DONE     0x0000000000000002LL	/* transaction finished */
#define B_ERROR    0x0000000000000004LL	/* transaction aborted */
#define B_BUSY     0x0000000000000008LL	/* not on av_forw/back list */
#define B_PHYS     0x0000000000000010LL	/* Physical IO */
#define	B_MAP      0x0000000000000020LL	/* mappable data */
#define B_WANTED   0x0000000000000040LL	/* issue wakeup when BUSY goes off */
#define B_AGE      0x0000000000000080LL	/* delayed write for correct aging */
#define B_ASYNC    0x0000000000000100LL	/* don't wait for I/O completion */
#define B_DELWRI   0x0000000000000200LL	/* delayed write - wait until */
					/* buffer is needed */
#define B_OPEN     0x0000000000000400LL	/* open routine called */
#define B_STALE    0x0000000000000800LL
#define B_LEADER   0x0000000000001000LL	/* lead buffer in a list of them */
#define B_FORMAT   0x0000000000002000LL
#define	B_PAGEIO   0x0000000000004000LL	/* use b_pages for I/O */
#define B_MAPPED   0x0000000000008000LL	/* buffer is mapped to a kernel */
					/* virtual address */
#define B_SWAP	   0x0000000000010000LL	/* block is on swap, paddr is */
					/* physical addr */
#define	B_BDFLUSH  0x0000000000020000LL	/* block being written by bdflush() */
#define B_RELSE	   0x0000000000040000LL	/* ignore b_relse field on brelse() */
#define	B_ALENLIST 0x0000000000080000LL	/* buffer data is in an alen list */
#define	B_PARTIAL  0x0000000000100000LL	/* buffer partially done, partially */
					/* undone */
#define	B_UNCACHED 0x0000000000200000LL	/* uncached mapping requested */
					/* for memcopy */
#define	B_INACTIVE 0x0000000000400000LL	/* buffer header taken out of cache */
					/* to reduce buffer cache memory use */
#define	B_BFLUSH   0x0000000000800000LL	/* delayed write buffer is being */
					/* pushed out */
#define B_FOUND    0x0000000001000000LL	/* bp was found in cache (for stats) */
#define B_WAIT	   0x0000000002000000LL	/* bp will be waited for after */
					/* async push */
#define B_WAKE	   0x0000000004000000LL	/* wake up page-waiters on b_relse */
#define	B_HOLD	   0x0000000008000000LL	/* tell bwrite() not to unlock buffer */
#define	B_DELALLOC 0x0000000010000000LL	/* backing store reserved but not */
					/* allocated */
#define B_MAPUSER  0x0000000020000000LL	/* buf address is user address */
#define	B_DACCT	   0x0000000040000000LL	/* buf accounted for in dchunkxxx */
					/* counters */
#define B_VDBUF	   0x0000000080000000LL	/* buffer accounted for in vnode's */
					/* v_dbuf cnt */
#define B_GR_BUF   0x0000000100000000LL	/* bp is a guaranteed rate I/O */
#define B_NOMORE   0x0000000200000000LL	/* FREE to good home was B_GR_Q */
#define B_GR_ISD   0x0000000400000000LL	/* guraranteed rate I/O bp was issued */
#define B_NONEED   0x0000000800000000LL	/* FREE to good home was B_GR_RSTRT */
#define B_XLV_HBUF 0x0000001000000000LL	/* initial request issued to xlv */
#define	B_XLVD_BUF 0x0000002000000000LL	/* buffer was allocated by xlvd */
#define	B_XLVD_FAILOVER \
		   0x0000004000000000LL	/* buffer is failing over */
#define	B_XLV_IOC  0x0000008000000000LL	/* io context has been allocated */
#define	B_XLV_ACK  0x0000010000000000LL	/* buffer requires ack */
#define B_SHUFFLED 0x0000020000000000LL	/* disk sched fairness already applied */
#define B_PRV_BUF  0x0000040000000000LL	/* buffer has associated io pri */
#define B_NOT_USED 0x0000080000000000LL	/* FREE to good home was B_XFS_INO */
#define B_XFS_SHUT 0x0000100000000000LL	/* XFS filesystem is being shutdown */
#define B_NFS_ASYNC \
		   0x0000200000000000LL	/* NFS bawrite buffer */
#define B_NFS_UNSTABLE \
		   0x0000400000000000LL	/* NFS unstable dirty buffer */
#define B_NFS_RETRY \
		   0x0000800000000000LL	/* NFS dirty buffer retried */
#define B_UNINITIAL \
		   0x0001000000000000LL	/* XFS backing store allocated */
					/* but not initialized */
#define B_PRIVATE_B_ADDR \
		   0x0002000000000000LL /* b_addr is private to caller, and not
					   necessarily from kvpalloc */

#define BP_ISMAPPED(bp)	((bp->b_flags & (B_PAGEIO|B_MAPPED)) != B_PAGEIO)
#define BUF_IS_IOSPL(bp) (bp->b_flags & (B_GR_BUF|B_PRV_BUF))

/*
 * Flags for incore_match() and findchunk_match().
 */
#define	BUF_FSPRIV	0x1
#define	BUF_FSPRIV2	0x2

#ifdef _KERNEL

#define bfree_lock(listid) \
		mutex_spinlock((lock_t*)&bfreelist[(listid)].b_private)
#define bfree_unlock(listid, s) \
		mutex_spinunlock((lock_t*)&bfreelist[(listid)].b_private, s)
#define nested_bfree_lock(listid) \
		nested_spinlock((lock_t*)&bfreelist[(listid)].b_private)
#define nested_bfree_unlock(listid) \
		nested_spinunlock((lock_t*)&bfreelist[(listid)].b_private)

#ifdef DEBUG_BUFTRACE
void	bfreelist_check(buf_t *);
#else
#define	bfreelist_check(b)
#endif

/*
 *	Fast access to buffers in cache by hashing.
 */

#define bhash(d,b)  ((buf_t *)&global_buf_hash[_bhash(d,b)])
#define bdhash(d)  (&global_dev_hash[((int)(d))&v.v_hmask])

typedef struct hbuf {
	sema_t		b_lock;			/* lock for hash queue */
	uint64_t	b_flags;
	struct	buf	*b_forw;
	struct	buf	*b_back;
} hbuf_t;

/* Device hash head, needs to look like a buffer up until bd_forw/bd_back
 * to make the code simpler, the first four fields are not actually used.
 */

typedef struct dhbuf {
	sema_t		b_lock;
	uint64_t	b_flags;
	struct	buf	*b_forw;
	struct	buf	*b_back;
	struct  buf	*bd_forw;
	struct  buf	*bd_back;
	lock_t	bd_lock;
} dhbuf_t;

extern	hbuf_t		*global_buf_hash;
extern  dhbuf_t		*global_dev_hash;

#define	BIO_MAXBSIZE_LOG2	(BPCSHIFT+8)
/* Limit for maximum offsets, in chars and BBs that can be reached via lseek */

#define	BIO_MAXBSIZE		(1<<BIO_MAXBSIZE_LOG2)
#define	BIO_MAXBBS		(1<<(BIO_MAXBSIZE_LOG2-BBSHIFT))

/*
 * The bwait_pin structure is used to wait for a buffer to be unpinned.
 */
typedef struct bwait_pin {
	sv_t	bwp_wait;	/* buffer wait variable */
	int	bwp_count;	/* wait counter */
} bwait_pin_t;	

#define	NUM_BWAIT_PIN	32
#define	BWAIT_PIN_MASK	(NUM_BWAIT_PIN - 1)
/*
 * This gets ugly in order to handle buffer allocated from the heap.
 */
#define	BPTOBWP(bp)	((((bp) < &global_buf_table[0]) || \
			  ((bp) >= &global_buf_table[v.v_buf])) ? \
			 &bwait_pin[0] : \
			 &bwait_pin[((bp) - global_buf_table) & BWAIT_PIN_MASK])

extern bwait_pin_t	bwait_pin[];

/*
 * Flags for get_buf() and read_buf().
 */
#define	BUF_TRYLOCK	0x00000001
#define	BUF_BUSY	0x00000002

/*
 * Flags for incore().
 */
#define	INCORE_LOCK	0x00000001
#define	INCORE_TRYLOCK	0x00000002

/*
 * Flags for chunk_decommission().
 */
#define	CD_FLUSH	0x1

/*
 * Flags for chunktoss().
 */
#define	C_PUSH	0x01
#define	C_EOF	0x02

/*
 * Chunk tracing stuff.
 */
struct ktrace;
extern struct ktrace	*buf_trace_buf;
/*
 * This is the number of entries in an individual buffer trace buffer.
 */
#define BUF_TRACE_SIZE  32


#ifndef DEBUG_BUFTRACE

#define	buftrace(id, bp)

#else /* DEBUG_BUFTRACE */

#define buftrace(id, bp) \
		buftrace_enter(id, bp, (inst_t *)__return_address)
extern void	buftrace_enter(char *, buf_t *, inst_t *);

#endif /* DEBUG_BUFTRACE */

enum uio_rw;
struct bmapval;
struct cred;
struct pfdat;
struct uio;
struct vnode;
struct alenlist_s;
struct iovec;

extern void	binit(void);
extern buf_t	*incore_match(dev_t, daddr_t, int, int, void *);
extern int	incore_relse(dev_t, int, int);
extern void	notavail(buf_t *);
extern buf_t	*getblk(dev_t, daddr_t, int);
extern buf_t	*get_buf(dev_t, daddr_t, int, uint);
extern int	geterror(buf_t *);
extern buf_t	*getrbuf(int);
extern buf_t	*ngetrbuf(size_t);
extern buf_t	*ngeteblk(size_t);
extern buf_t	*ngeteblkdev(dev_t, size_t);
extern buf_t	*bread(dev_t, daddr_t, int);
extern buf_t	*breada(dev_t, daddr_t, int, daddr_t, int);
extern buf_t	*read_buf(dev_t, daddr_t, int, uint);
extern buf_t	*read_buf_targ(buftarg_t *, daddr_t, int, uint);
extern void	baread(buftarg_t *, daddr_t, int);
extern int	bwrite(buf_t *);
extern void	bdwrite(buf_t *);
extern void	bawrite(buf_t *);
extern void	brelse(buf_t *);
extern void	binval(dev_t);
extern void	bflush(dev_t);
extern void	bpin(buf_t *);
extern void	bunpin(buf_t *);
extern void	bwait_unpin(buf_t *);
extern int	iowait(buf_t *);
extern void	iodone(buf_t *);
extern int	biowait(buf_t *);
extern void	biodone(buf_t *);
extern void 	bioerror(struct buf *, int);
extern void	clrbuf(buf_t *);
extern void	freerbuf(buf_t *);
extern void	nfreerbuf(buf_t *);
extern char	*bp_mapin(buf_t *);
extern void	bp_mapout(buf_t *);
extern struct pfdat	*getnextpg(buf_t *, struct pfdat *);
extern caddr_t	maputokv(caddr_t, size_t, int);
extern void	unmaputokv(caddr_t, size_t);
extern int	iomap(buf_t *);
extern int	iomap_vector(buf_t *, struct iovec *, int);
extern void	iounmap(buf_t *);
extern void	unuseracc(void *, size_t, int);
extern void	fast_unuseracc(void *, size_t, int, opaque_t *);
extern int	useracc(void *, size_t, int, void *);
extern int	fast_useracc(void *, size_t, int, opaque_t *);
extern int	userdma(void *, size_t, int, void *);
extern int	fast_userdma(void *, size_t, int, opaque_t *);
extern void	undma(void *, size_t, int);
extern void	fast_undma(void *, size_t, int, opaque_t *);
extern buf_t	*getphysbuf(dev_t);
extern int	uiophysio(int (*)(buf_t *), buf_t *, dev_t, uint64_t,
			  struct uio *);
extern int	biophysio(int (*)(buf_t *), buf_t *, dev_t, uint64_t, daddr_t,
			  struct uio *);
extern void	putphysbuf(buf_t *);
extern buf_t	*incore(dev_t, daddr_t, int, int);
extern int	biomove(buf_t *, u_int, size_t, enum uio_rw, struct uio *);
extern buf_t	*chunkread(struct vnode *, struct bmapval *,
			   int, struct cred *);
extern buf_t	*getchunk(struct vnode *, struct bmapval *, struct cred *);
extern buf_t	*chunkreread(buf_t *);
extern void	dchunkunchain(buf_t *);
extern void	delalloc_free(buf_t *);
extern void	clusterwrite(buf_t *, clock_t);
extern void	bp_dcache_wbinval(buf_t *);
extern void	bp_dcache_flush(buf_t *, uint);
extern void	chunktoss(struct vnode *, off_t, off_t);
extern int	chunkpush(struct vnode *, off_t, off_t, uint64_t);
extern void	chunkstabilize(struct vnode *, off_t, off_t);
extern void	chunkinvalfree(struct vnode *);
#ifdef TILE_DATA
extern int	chunkpfind(struct pfdat *, buf_t **);
extern int	chunkpreplace(buf_t *, struct pfdat *, struct pfdat *);
#endif /* TILE_DATA */

#ifdef _VCE_AVOIDANCE
extern int	chunk_vrelse(struct vnode *,off_t,off_t);
#endif /* _VCE_AVOIDANCE */

#ifdef DEBUG
extern void	bflushed(dev_t);
#else
#define	bflushed(dev)
#endif

extern int	_bhash(dev_t, daddr_t);
#endif	/* _KERNEL */
#endif	/* __SYS_BUF_H__ */
