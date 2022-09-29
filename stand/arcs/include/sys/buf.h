/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
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

#ident	"$Revision: 3.29 $"
#include "sys/sema.h"

struct bdevsw;

/*
 *      Structure used to get to the device driver, which can either be
 *      via a spec vnode, or the bdevsw entry. We cache the bdevsw entry
 *      in order to avoid using the hardware graph to find it in the
 *      get_bdevsw macro. The spec vp is used in the cellular case where
 *      we have to use VOP_STRATEGY in place of bdstrat.
 */

typedef struct buftarg {
        struct vnode    *specvp;
        struct bdevsw   *bdevsw;
        dev_t           dev;
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

typedef struct	buf
{
	sema_t	b_lock;			/* lock for buffer usage */
	int	b_flags;		/* see defines below */
	struct	buf *b_forw;		/* headed by d_tab of conf.c */
	struct	buf *b_back;		/*  "  */
	struct	buf *av_forw;		/* position on free list, */
	struct	buf *av_back;		/*     if not BUSY*/
	dev_t	b_dev;			/* major+minor device name */
	char	b_error;		/* returned after I/O */
	ushort	b_length;		/* length of buffer memory in BBs*/
	unsigned b_bcount;		/* transfer count */
	union {
	    caddr_t b_addr;		/* low order core address */
	    int	*b_words;		/* words for clearing */
	    struct pfdat *b_pfdat;	/* pointer into b_pages list */
	    daddr_t *b_daddr;		/* disk blocks */
	} b_un;
	daddr_t	b_blkno;		/* block # on device */
	unsigned b_resid;		/* words not transferred after error */
	time_t	b_start;		/* request start time */
	struct  proc *b_proc;		/* process doing physical or swap I/O */
	struct  pfdat *b_pages;		/* page list for PAGEIO */
	unsigned b_remain;		/* virt b_bcount for PAGEIO use only */
	unsigned b_sort;		/* key with which to sort on queue */
	void	(*b_relse)();		/* 'special action' release function */
	sema_t	b_iodone;		/* lock indicating I/O done */
	void	(*b_iodonefunc)();	/* 'special action' iodone function */
} buf_t;

typedef int	opaque_t;

#define paddr(X)	(paddr_t)(X->b_un.b_addr)
#define	b_dmaaddr	b_un.b_addr
#define	b_page		b_un.b_pfdat

extern	struct	buf*	buf;		/* The buffer pool itself */
extern	struct	buf	bfreelist;	/* head of available list */
extern	int		bfslpcnt;	/* # procs waiting for free buffers */
extern	sema_t		bfreeslp;	/* sleep semaphore for free list */
extern	lock_t		bfreelock;	/* lock on buffer free list */

struct	pfree	{
	int	b_flags;
	struct	buf	*av_forw;
	struct	buf	*av_back;
};

/*
 *	These flags are kept in b_flags.
 */
#define B_WRITE    0x000000	/* non-read pseudo-flag */
#define B_READ     0x000001	/* read when I/O occurs */
#define B_DONE     0x000002	/* transaction finished */
#define B_ERROR    0x000004	/* transaction aborted */
#define B_BUSY     0x000008	/* not on av_forw/back list */
#define B_PHYS     0x000010	/* Physical IO */
#define B_MAP      0x000020	/* mappable data */
#define B_WANTED   0x000040	/* issue wakeup when BUSY goes off */
#define B_AGE      0x000080	/* delayed write for correct aging */
#define B_ASYNC    0x000100	/* don't wait for I/O completion */
#define B_DELWRI   0x000200	/* delayed write - wait until buffer needed */
#define B_OPEN     0x000400	/* open routine called */
#define B_STALE    0x000800
#define B_VERIFY   0x001000
#define B_FORMAT   0x002000
#define	B_PAGEIO   0x004000	/* use b_pages for I/O */
#define B_MAPPED   0x008000	/* buffer has been mapped to kernel va */
#define B_SWAP	   0x010000	/* block is on swap, paddr is physical addr */
#define	B_BDFLUSH  0x020000	/* block being written by bdflush() */
#define	B_FRAG     0x040000	/* fragmented buffer */
#define	B_BMAP     0x080000	/* FS_BMAP flag -- just want block mappings */
#define	B_PDELWRI  0x100000	/* pseudo-flag for prelse */
#define	B_UNCACHED 0x200000	/* uncached mapping requested for memcopy */
#define	B_LAST 	   0x400000	/* must put this buffer at the end of driver's 
									queue */
#define	B_BFLUSH   0x800000	/* delayed write buffer is being pushed out */

#define BP_ISMAPPED(bp)	((bp->b_flags & (B_PAGEIO|B_MAPPED)) != B_PAGEIO)

/*
 *	Fast access to buffers in cache by hashing.
 */

#define bhash(d,b)	((struct buf *)&hbuf[((int)(d)+(int)(b))&v.v_hmask])

struct	hbuf
{
	sema_t	b_lock;			/* lock for hash queue */
	int	b_flags;
	struct	buf	*b_forw;
	struct	buf	*b_back;
};

extern	struct	hbuf	*hbuf;

/*
 * Pick up the device's error number and pass it to the user;
 * if there is an error but the number is 0 set a generalized code
 */
#define geterror(bp) \
{\
\
	if (bp->b_flags&B_ERROR)\
		if ((u.u_error = bp->b_error)==0)\
			u.u_error = EIO;\
}

#ifdef _KERNEL
#define	BIO_MAXBSIZE_LOG2	(BPCSHIFT+8)
/* Limit for maximum offsets, in chars and BBs that can be reached via lseek */

#define	BIO_MAXBSIZE		(1<<BIO_MAXBSIZE_LOG2)
#define	BIO_MAXBBS		(1<<(BIO_MAXBSIZE_LOG2-BBSHIFT))
/*
 * Hashing parameters.  Each disk block is scaled into a larger block,
 * whose length is twice the maximun length of a buffer block.
 * Maximum overlap of buckets is thus reduced to 2.
 */
#define	BIO_BBSPERBUCKETSHIFT	(5)
#define	BIO_BBSPERBUCKETMINUSONE	((1 << BIO_BBSPERBUCKETSHIFT) - 1)
#define	BIO_BBSPERLBMINUSONE	(BTOBBT(BIO_MAXBSIZE) - 1)

struct buf	*getblk(dev_t, daddr_t, int);
struct buf	*geteblk(int);
struct buf	*bread(dev_t, daddr_t, int);
struct buf	*breada(dev_t, daddr_t, int, daddr_t, int);
char		*bp_mapin(buf_t *);
void		bp_mapout(buf_t *);
struct buf	*getphysbuf(void);
void		putphysbuf(buf_t *);
void		putphysbuf(buf_t *);
struct buf	*incore(dev_t, daddr_t, int, int);
void		prelse(buf_t *);
paddr_t		bptophys(buf_t *);

#endif	/* _KERNEL */
#endif	/* __SYS_BUF_H__ */
