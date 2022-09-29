#ident "$Revision: 1.270 $"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/pfdat.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/grio.h>
#include <sys/pda.h>
#include <sys/dmi_kern.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h> 
#include <sys/fcntl.h>
#include <sys/var.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/uthread.h>
#include <ksys/as.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/dmi.h>
#include <sys/dmi_kern.h>
#include <sys/schedctl.h>
#include <sys/atomic_ops.h>
#include <sys/ktrace.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <ksys/sthread.h>
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_itable.h"
#include "xfs_btree.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_ialloc.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_error.h"
#include "xfs_bit.h"
#include "xfs_rw.h"
#include "xfs_quota.h"
#include "xfs_trans_space.h"
#include "xfs_dmapi.h"
#include <limits.h>

/*
 * turning on UIOSZ_DEBUG in a DEBUG kernel causes each xfs_write/xfs_read
 * to set the write/read i/o size to a random valid value and instruments
 * the distribution.
 *
#define UIOSZ_DEBUG
 */

#ifdef UIOSZ_DEBUG
int uiodbg = 0;
int uiodbg_readiolog[XFS_UIO_MAX_READIO_LOG - XFS_UIO_MIN_READIO_LOG + 1] =
		{0, 0, 0, 0};
int uiodbg_writeiolog[XFS_UIO_MAX_WRITEIO_LOG - XFS_UIO_MIN_WRITEIO_LOG + 1] =
		{0, 0, 0, 0};
int uiodbg_switch = 0;
#endif

#define XFS_NUMVNMAPS 10	    /* number of uacc maps to pass to VM */

extern int xfs_nfs_io_units;

/*
 * This lock is used by xfs_strat_write().
 * The xfs_strat_lock is initialized in xfs_init().
 */
lock_t	xfs_strat_lock;

/*
 * Variables for coordination with the xfs_strat daemon.
 * The xfsc_lock and xfsc_wait variables are initialized
 * in xfs_init();
 */
static int	xfsc_count;
static buf_t	*xfsc_list;
static int	xfsc_bufcount;
lock_t		xfsc_lock;
sv_t		xfsc_wait;

/*
 * Variables for coordination with the xfsd daemons.
 * The xfsd_lock and xfsd_wait variables are initialized
 * in xfs_init();
 */
static int	xfsd_count;
static buf_t	*xfsd_list;
static int	xfsd_bufcount;
lock_t		xfsd_lock;
sv_t		xfsd_wait;

/*
 * Zone allocator for xfs_gap_t structures.
 */
zone_t		*xfs_gap_zone;

#ifdef DEBUG
/*
 * Global trace buffer for xfs_strat_write() tracing.
 */
ktrace_t	*xfs_strat_trace_buf;
#endif

#if !defined(XFS_STRAT_TRACE)
#define	xfs_strat_write_bp_trace(tag, ip, bp)
#define	xfs_strat_write_subbp_trace(tag, ip, bp, rbp, loff, lcnt, lblk)
#endif	/* !XFS_STRAT_TRACE */

STATIC int
xfs_zero_last_block(
	xfs_inode_t	*ip,
	off_t		offset,
	xfs_fsize_t	isize,
	cred_t		*credp,
	struct pm	*pmp);

STATIC void
xfs_zero_bp(
	buf_t	*bp,
	int	data_offset,
	int	data_len);

STATIC int
xfs_retrieved(
	uint		available,
	off_t		offset,
	size_t		count,
	uint		*total_retrieved,
	xfs_fsize_t	isize);

#ifndef DEBUG

#define	xfs_strat_write_check(ip,off,count,imap,nimap)
#define	xfs_check_rbp(ip,bp,rbp,locked)
#define	xfs_check_bp(ip,bp)
#define	xfs_check_gap_list(ip)

#else /* DEBUG */

STATIC void
xfs_strat_write_check(
	xfs_inode_t	*ip,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	buf_fsb,
	xfs_bmbt_irec_t	*imap,
	int		imap_count);

STATIC void
xfs_check_rbp(
	xfs_inode_t	*ip,
	buf_t		*bp,
	buf_t		*rbp,
	int		locked);

STATIC void
xfs_check_bp(
	xfs_inode_t	*ip,
	buf_t		*bp);

STATIC void
xfs_check_gap_list(
	xfs_inode_t	*ip);

#endif /* DEBUG */		      

STATIC int
xfs_build_gap_list(
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count);

STATIC void
xfs_free_gap_list(
	xfs_inode_t	*ip);

STATIC void
xfs_cmp_gap_list_and_zero(
	xfs_inode_t	*ip,
	buf_t		*bp);

STATIC void
xfs_delete_gap_list(
	xfs_inode_t	*ip,
	xfs_fileoff_t	offset_fsb,
	xfs_extlen_t	count_fsb);

STATIC int
xfs_strat_write_unwritten(
	bhv_desc_t	*bdp,
	buf_t		*bp);

STATIC void
xfs_strat_write_iodone(
	buf_t		*bp);

STATIC void
xfs_strat_comp(void);

STATIC int
xfsd(void);

int
xfs_diordwr(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	uint64_t	rw);

STATIC int
xfs_dio_write_zero_rtarea(
	xfs_inode_t	*ip,
	struct buf	*bp,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	count_fsb);

extern int
grio_io_is_guaranteed(
	vfile_t *,
	stream_id_t	*);

extern void
griostrategy(
	buf_t	*);

extern int
grio_monitor_io_start( 
	stream_id_t *, 
	__int64_t);

extern int
grio_monitor_io_end(
	stream_id_t *,
	int);
	

extern void xfs_error(
	xfs_mount_t *,
	int);

STATIC void
xfs_delalloc_cleanup(
	xfs_inode_t	*ip,
	xfs_fileoff_t	start_fsb,
	xfs_filblks_t	count_fsb);

extern void xfs_buf_iodone_callbacks(struct buf *);
extern void xlog_iodone(struct buf *);
int    xfs_bioerror_relse(buf_t *bp);

/*
 * Round the given file offset down to the nearest read/write
 * size boundary.
 */
#define	XFS_READIO_ALIGN(ip,off)	(((off) >> ip->i_readio_log) \
					        << ip->i_readio_log)
#define	XFS_WRITEIO_ALIGN(ip,off)	(((off) >> ip->i_writeio_log) \
					        << ip->i_writeio_log)

#if !defined(XFS_RW_TRACE)
#define	xfs_rw_enter_trace(tag, ip, uiop, ioflags)
#define	xfs_iomap_enter_trace(tag, ip, offset, count);
#define	xfs_iomap_map_trace(tag, ip, offset, count, bmapp, imapp)
#define xfs_inval_cached_trace(ip, offset, len, first, last)
#else
/*
 * Trace routine for the read/write path.  This is the routine entry trace.
 */
static void
xfs_rw_enter_trace(
	int		tag,	     
	xfs_inode_t	*ip,
	uio_t		*uiop,
	int		ioflags)
{
	if (ip->i_rwtrace == NULL) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((unsigned long)tag),
		     (void*)ip, 
		     (void*)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)(((__uint64_t)uiop->uio_offset >> 32) &
			     0xffffffff),
		     (void*)(uiop->uio_offset & 0xffffffff),
		     (void*)uiop->uio_resid,
		     (void*)((unsigned long)ioflags),
		     (void*)((ip->i_next_offset >> 32) & 0xffffffff),
		     (void*)(ip->i_next_offset & 0xffffffff),
		     (void*)((unsigned long)((ip->i_io_offset >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_io_offset & 0xffffffff),
		     (void*)((unsigned long)(ip->i_io_size)),
		     (void*)((unsigned long)(ip->i_last_req_sz)),
		     (void*)((unsigned long)((ip->i_new_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_new_size & 0xffffffff));
}

static void
xfs_iomap_enter_trace(
	int		tag,
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count)
{
	if (ip->i_rwtrace == NULL) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((unsigned long)tag),
		     (void*)ip, 
		     (void*)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)(((__uint64_t)offset >> 32) & 0xffffffff),
		     (void*)(offset & 0xffffffff),
		     (void*)((unsigned long)count),
		     (void*)((ip->i_next_offset >> 32) & 0xffffffff),
		     (void*)(ip->i_next_offset & 0xffffffff),
		     (void*)((ip->i_io_offset >> 32) & 0xffffffff),
		     (void*)(ip->i_io_offset & 0xffffffff),
		     (void*)((unsigned long)(ip->i_io_size)),
		     (void*)((unsigned long)(ip->i_last_req_sz)),
		     (void*)((ip->i_new_size >> 32) & 0xffffffff),
		     (void*)(ip->i_new_size & 0xffffffff),
		     (void*)0);
}

void
xfs_iomap_map_trace(
	int		tag,	     
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count,
	struct bmapval	*bmapp,
	xfs_bmbt_irec_t	*imapp)    
{
	if (ip->i_rwtrace == NULL) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((unsigned long)tag),
		     (void*)ip, 
		     (void*)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)(((__uint64_t)offset >> 32) & 0xffffffff),
		     (void*)(offset & 0xffffffff),
		     (void*)((unsigned long)count),
		     (void*)((bmapp->offset >> 32) & 0xffffffff),
		     (void*)(bmapp->offset & 0xffffffff),
		     (void*)((unsigned long)(bmapp->length)),
		     (void*)((unsigned long)(bmapp->pboff)),
		     (void*)((unsigned long)(bmapp->pbsize)),
		     (void*)(bmapp->bn),
		     (void*)(__psint_t)(imapp->br_startoff),
		     (void*)((unsigned long)(imapp->br_blockcount)),
		     (void*)(__psint_t)(imapp->br_startblock));
}

static void
xfs_inval_cached_trace(
	xfs_inode_t	*ip,
	off_t		offset,
	off_t		len,
	off_t		first,
	off_t		last)
{
	if (ip->i_rwtrace == NULL)
		return;
	ktrace_enter(ip->i_rwtrace,
		(void *)(__psint_t)XFS_INVAL_CACHED,
		(void *)ip,
		(void *)(((__uint64_t)offset >> 32) & 0xffffffff),
		(void *)(offset & 0xffffffff),
		(void *)(((__uint64_t)len >> 32) & 0xffffffff),
		(void *)(len & 0xffffffff),
		(void *)(((__uint64_t)first >> 32) & 0xffffffff),
		(void *)(first & 0xffffffff),
		(void *)(((__uint64_t)last >> 32) & 0xffffffff),
		(void *)(last & 0xffffffff),
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0);
}
#endif	/* XFS_RW_TRACE */

#ifdef DEBUG
/* ARGSUSED */
void
debug_print_vnmaps(vnmap_t *vnmap, int numvnmaps, int vnmap_flags)
{
	int i;

	for (i = 0; i < numvnmaps; i++, vnmap++) {
		cmn_err(CE_DEBUG,
"vaddr = 0x%llx, len = 0x%llx, flags = 0x%x\n  ovvaddr = 0x%llx len = 0x%llx offset = %lld\n",
			(uint64_t)vnmap->vnmap_vaddr,
			(uint64_t)vnmap->vnmap_len,
			vnmap->vnmap_flags,
			(uint64_t)vnmap->vnmap_ovvaddr,
			(uint64_t)vnmap->vnmap_ovlen,
			vnmap->vnmap_ovoffset);
	}
}
#endif

/*
 * Fill in the bmap structure to indicate how the next bp
 * should fit over the given extent.
 *
 * Everything here is in terms of file system blocks, not BBs.
 */
void
xfs_next_bmap(
	xfs_mount_t	*mp,
	xfs_bmbt_irec_t	*imapp,
	struct bmapval	*bmapp,
	int		iosize,
	int		last_iosize,
	xfs_fileoff_t	ioalign,
	xfs_fileoff_t	last_offset,
	xfs_fileoff_t	req_offset,
	xfs_fsize_t	isize)
{
	__int64_t	extra_blocks;
	xfs_fileoff_t	size_diff;
	xfs_fileoff_t	ext_offset;
	xfs_fileoff_t	last_file_fsb;
	xfs_fsblock_t	start_block;

	/*
	 * Make sure that the request offset lies in the extent given.
	 */
	ASSERT(req_offset >= imapp->br_startoff);
	ASSERT(req_offset < (imapp->br_startoff + imapp->br_blockcount));

	if (last_offset == -1) {
		ASSERT(ioalign != -1);
		if (ioalign < imapp->br_startoff) {
			/*
			 * The alignment we guessed at can't
			 * happen on this extent, so align
			 * to the beginning of this extent.
			 * Subtract whatever we drop from the
			 * iosize so that we stay aligned on
			 * our iosize boundaries.
			 */
			size_diff = imapp->br_startoff - ioalign;
			iosize -= size_diff;
			ASSERT(iosize > 0);
			ext_offset = 0;
			bmapp->offset = imapp->br_startoff;
			ASSERT(bmapp->offset <= req_offset);
		} else {
			/*
			 * The alignment requested fits on this
			 * extent, so use it.
			 */
			ext_offset = ioalign - imapp->br_startoff;
			bmapp->offset = ioalign;
			ASSERT(bmapp->offset <= req_offset);
		}
	} else {
		/*
		 * This is one of a series of sequential access to the
		 * file.  Make sure to line up the buffer we specify
		 * so that it doesn't overlap the last one.  It should
		 * either be the same as the last one (if we need data
		 * from it) or follow immediately after the last one.
		 */
		ASSERT(ioalign == -1);
		if (last_offset >= imapp->br_startoff) {
			/*
			 * The last I/O was from the same extent
			 * that this one will at least start on.
			 * This assumes that we're going sequentially.
			 */
			if (req_offset < (last_offset + last_iosize)) {
				/*
				 * This request overlaps the buffer
				 * we used for the last request.  Just
				 * get that buffer again.
				 */
				ext_offset = last_offset -
					     imapp->br_startoff;
				bmapp->offset = last_offset;
				iosize = last_iosize;
			} else {
				/*
				 * This request does not overlap the buffer
				 * used for the last one.  Get it its own.
				 */
				ext_offset = req_offset - imapp->br_startoff;
				bmapp->offset = req_offset;
			}
		} else {
			/*
			 * The last I/O was on a different extent than
			 * this one.  We start at the beginning of this one.
			 * This assumes that we're going sequentially.
			 */
			ext_offset = req_offset - imapp->br_startoff;
			bmapp->offset = req_offset;
		}

	}
	start_block = imapp->br_startblock;
	if (start_block == HOLESTARTBLOCK) {
		bmapp->bn = -1;
		bmapp->eof = BMAP_HOLE;
	} else if (start_block == DELAYSTARTBLOCK) {
		bmapp->bn = -1;
		bmapp->eof = BMAP_DELAY;
	} else {
		bmapp->bn = start_block + ext_offset;
		bmapp->eof = 0;
		if (imapp->br_state == XFS_EXT_UNWRITTEN)
			bmapp->eof |= BMAP_UNWRITTEN;
	}
	bmapp->length = iosize;
	
	/*
	 * If the iosize from our offset extends beyond the
	 * end of the extent, then trim down the length
	 * to match that of the extent.
	 */
	 extra_blocks = (off_t)(bmapp->offset + bmapp->length) -
	                (__uint64_t)(imapp->br_startoff +
				     imapp->br_blockcount);   
	 if (extra_blocks > 0) {
	    	bmapp->length -= extra_blocks;
		ASSERT(bmapp->length > 0);
	}

	/*
	 * If the iosize from our offset extends beyond the end
	 * of the file and the current extent is simply a hole,
	 * then trim down the length to match the
	 * size of the file.  This keeps us from going out too
	 * far into hole at the EOF that extends to infinity.
	 */
	if (start_block == HOLESTARTBLOCK) {
		last_file_fsb = XFS_B_TO_FSB(mp, isize);
		extra_blocks = (off_t)(bmapp->offset + bmapp->length) -
			(__uint64_t)last_file_fsb;
		if (extra_blocks > 0) {
			bmapp->length -= extra_blocks;
			ASSERT(bmapp->length > 0);
		}
		ASSERT((bmapp->offset + bmapp->length) <= last_file_fsb);
	}

	bmapp->bsize = XFS_FSB_TO_B(mp, bmapp->length);
}

/*
 * xfs_retrieved() is a utility function used to calculate the
 * value of bmap.pbsize.
 *
 * Available is the number of bytes mapped by the current bmap.
 * Offset is the file offset of the current request by the user.
 * Count is the size of the current request by the user.
 * Total_retrieved is a running total of the number of bytes
 *  which have been setup for the user in this call so far.
 * Isize is the current size of the file being read.
 */
STATIC int
xfs_retrieved(
	uint		available,
	off_t		offset,
	size_t		count,
	uint		*total_retrieved,
	xfs_fsize_t	isize)
{
	uint		retrieved;
	xfs_fsize_t	file_bytes_left;
	

	if ((available + *total_retrieved) > count) {
		/*
		 * This buffer will return more bytes
		 * than we asked for.  Trim retrieved
		 * so we can set bmapp->pbsize correctly.
		 */
		retrieved = count - *total_retrieved;
	} else {
		retrieved = available;
	}

	file_bytes_left = isize - (offset + *total_retrieved);
	if (file_bytes_left < retrieved) {
		/*
		 * The user has requested more bytes
		 * than there are in the file.  Trim
		 * down the number to those left in
		 * the file.
		 */
		retrieved = file_bytes_left;
	}

	*total_retrieved += retrieved;
	return retrieved;
}

/*
 * xfs_iomap_extra()
 *
 * This is called when the VM/chunk cache is trying to create a buffer
 * for a page which is beyond the end of the file.  If we're at the
 * start of the page we give it as much of a mapping as we can, but
 * if it comes back for the rest of the page we say there is nothing there.
 * This behavior is tied to the code in the VM/chunk cache (do_pdflush())
 * that will call here.
 */
STATIC int					/* error */
xfs_iomap_extra(
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count,
	struct bmapval	*bmapp,
	int		*nbmaps,
	struct pm	*pmp)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	end_fsb;
	xfs_fsize_t	nisize;
	xfs_mount_t	*mp;
	int		nimaps;
	xfs_fsblock_t	firstblock;
	int		error;
	xfs_bmbt_irec_t	imap;

	nisize = ip->i_new_size;
	if (nisize < ip->i_d.di_size) {
		nisize = ip->i_d.di_size;
	}
	mp = ip->i_mount;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);

	if (poff(offset) != 0) {
		/*
		 * This is the 'remainder' of a page being mapped out.
		 * Since it is already beyond the EOF, there is no reason
		 * to bother.
		 */
		ASSERT(count < NBPP);
		*nbmaps = 1;
		bmapp->eof = BMAP_EOF;
		bmapp->bn = -1;
		bmapp->offset = XFS_FSB_TO_BB(mp, offset_fsb);
		bmapp->length = 0;
		bmapp->bsize = 0;
		bmapp->pboff = 0;
		bmapp->pbsize = 0;
		bmapp->pmp = pmp;
		if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
			bmapp->pbdev = mp->m_rtdev;
		} else {
			bmapp->pbdev = mp->m_dev;
		}
	} else {
		/*
		 * A page is being mapped out so that it can be flushed.
		 * The page is beyond the EOF, but we need to return
		 * something to keep the chunk cache happy.
		 */
		ASSERT(count <= NBPP);
		end_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
		nimaps = 1;
		firstblock = NULLFSBLOCK;
		error = xfs_bmapi(NULL, ip, offset_fsb,
				  (xfs_filblks_t)(end_fsb - offset_fsb),
				  0, &firstblock, 0, &imap,
				  &nimaps, NULL);
		if (error) {
			return error;
		}
		ASSERT(nimaps == 1);
		*nbmaps = 1;
		bmapp->eof = BMAP_EOF;
		if (imap.br_startblock == HOLESTARTBLOCK) {
			bmapp->eof |= BMAP_HOLE;
			bmapp->bn = -1;
		} else if (imap.br_startblock == DELAYSTARTBLOCK) {
			bmapp->eof |= BMAP_DELAY;
			bmapp->bn = -1;
		} else {
			bmapp->bn = XFS_FSB_TO_DB(ip, imap.br_startblock);
			if (imap.br_state == XFS_EXT_UNWRITTEN)
				bmapp->eof |= BMAP_UNWRITTEN;
		}
		bmapp->offset = XFS_FSB_TO_BB(mp, offset_fsb);
		bmapp->length =	XFS_FSB_TO_BB(mp, imap.br_blockcount);
		ASSERT(bmapp->length > 0);
		bmapp->bsize = BBTOB(bmapp->length);
		bmapp->pboff = offset - BBTOOFF(bmapp->offset);
		ASSERT(bmapp->pboff >= 0);
		bmapp->pbsize = bmapp->bsize - bmapp->pboff;
		ASSERT(bmapp->pbsize > 0);
		bmapp->pmp = pmp;
		if (bmapp->pbsize > count) {
			bmapp->pbsize = count;
		}
		if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
			bmapp->pbdev = mp->m_rtdev;
		} else {
			bmapp->pbdev = mp->m_dev;
		}
	}
	return 0;
}

/*
 * xfs_iomap_read()
 *
 * This is the main I/O policy routine for reads.  It fills in
 * the given bmapval structures to indicate what I/O requests
 * should be used to read in the portion of the file for the given
 * offset and count.
 *
 * The inode's I/O lock may be held SHARED here, but the inode lock
 * must be held EXCL because it protects the read ahead state variables
 * in the inode.
 * Bug 516806: The readahead state is now maintained by i_rlock therefore,
 * the inode lock can be held in SHARED mode. The only time we need it 
 * in EXCL mode is when it is being read in the first time.  
 */
int					/* error */
xfs_iomap_read(
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count,
	struct bmapval	*bmapp,
	int		*nbmaps,
	struct pm	*pmp,
	int		*unlocked,
	unsigned int	lockmode)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	ioalign;
	xfs_fileoff_t	last_offset;
	xfs_fileoff_t	last_required_offset;
	xfs_fileoff_t	next_offset;
	xfs_fileoff_t	last_fsb;
	xfs_fileoff_t	max_fsb;
	xfs_fsize_t	nisize;
	off_t		offset_page;
	off_t		aligned_offset;
	xfs_fsblock_t	firstblock;
	int		nimaps;
	int		error;
	unsigned int	iosize;
	unsigned int	last_iosize;
	unsigned int	retrieved_bytes;
	unsigned int	total_retrieved_bytes;
	short		filled_bmaps;
	short		read_aheads;
	int		x;
	xfs_mount_t	*mp;
	struct bmapval	*curr_bmapp;
	struct bmapval	*next_bmapp;
	struct bmapval	*last_bmapp;
	struct bmapval	*first_read_ahead_bmapp;
	struct bmapval	*next_read_ahead_bmapp;
	xfs_bmbt_irec_t	*curr_imapp;
	xfs_bmbt_irec_t	*last_imapp;
	xfs_bmbt_irec_t	imap[XFS_MAX_RW_NBMAPS];

	ASSERT(ismrlocked(&ip->i_lock, MR_UPDATE | MR_ACCESS) != 0);
	ASSERT(ismrlocked(&ip->i_iolock, MR_UPDATE | MR_ACCESS) != 0);
	xfs_iomap_enter_trace(XFS_IOMAP_READ_ENTER, ip, offset, count);

	mp = ip->i_mount;
	nisize = ip->i_new_size;
	if (nisize < ip->i_d.di_size) {
		nisize = ip->i_d.di_size;
	}
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	nimaps = sizeof(imap) / sizeof(imap[0]);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)nisize));
	if (offset >= nisize) {
		/*
		 * The VM/chunk code is trying to map a page or part
		 * of a page to be pushed out that is beyond the end
		 * of the file.  We handle these cases separately so
		 * that they do not interfere with the normal path
		 * code.
		 */
		error = xfs_iomap_extra(ip, offset, count, bmapp, nbmaps, pmp);
		return error;
	}
	/*
	 * Map out to the maximum possible file size.  This will return
	 * an extra hole we don't really care about at the end, but we
	 * won't do any read-ahead beyond the EOF anyway.  We do this
	 * so that the buffers we create here line up well with those
	 * created in xfs_iomap_write() which extend beyond the end of
	 * the file.
	 */
	max_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)XFS_MAX_FILE_OFFSET);
	firstblock = NULLFSBLOCK;
	error = xfs_bmapi(NULL, ip, offset_fsb,
			  (xfs_filblks_t)(max_fsb - offset_fsb),
			  XFS_BMAPI_ENTIRE, &firstblock, 0, imap,
			  &nimaps, NULL);
	if (error) {
		return error;
	}

	xfs_iunlock_map_shared(ip, lockmode);
	*unlocked = 1;
	mutex_lock(&ip->i_rlock, PINOD);
	if ((offset == ip->i_next_offset) &&
	    (count <= ip->i_last_req_sz)) {
		/*
		 * Sequential I/O of same size as last time.
	 	 */
		ASSERT(ip->i_io_size > 0);
		iosize = ip->i_io_size;
		ASSERT(iosize <= XFS_BB_TO_FSBT(mp, XFS_MAX_BMAP_LEN_BB));
		last_offset = ip->i_io_offset;
		ioalign = -1;
	} else {
		/*
		 * The I/O size for the file has not yet been
		 * determined, so figure it out.
		 */
		if (XFS_B_TO_FSB(mp, count) <= ip->i_readio_blocks) {
			/*
			 * The request is smaller than our
			 * minimum I/O size, so default to
			 * the minimum.  For these size requests
			 * we always want to align the requests
			 * to XFS_READ_SIZE boundaries as well.
			 */
			iosize = ip->i_readio_blocks;
			ASSERT(iosize <=
			       XFS_BB_TO_FSBT(mp, XFS_MAX_BMAP_LEN_BB));
			aligned_offset = XFS_READIO_ALIGN(ip, offset);
			ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		} else {
			/*
			 * The request is bigger than our
			 * minimum I/O size and it's the
			 * first one in this sequence, so
			 * set the I/O size for the file
			 * now.
			 *
			 * In calculating the offset rounded down
			 * to a page, make sure to round down the
			 * fs block offset rather than the byte
			 * offset for the case where our block size
			 * is greater than the page size.  This way
			 * offset_page will always align to a fs block
			 * as well as a page.
			 *
			 * For the end of the I/O we need to round
			 * offset + count up to a page boundary and
			 * then round that up to a file system block
			 * boundary.
			 */
			offset_page = ctooff(offtoct(XFS_FSB_TO_B(mp,
							 offset_fsb)));
			last_fsb = XFS_B_TO_FSB(mp,
					ctooff(offtoc(offset + count)));
			iosize = last_fsb - XFS_B_TO_FSBT(mp, offset_page);
			if (iosize >
			    XFS_BB_TO_FSBT(mp, XFS_MAX_BMAP_LEN_BB)) {
				iosize = XFS_BB_TO_FSBT(mp,
							XFS_MAX_BMAP_LEN_BB);
			}
			ioalign = XFS_B_TO_FSB(mp, offset_page);
		}
		last_offset = -1;
	}

	/*
	 * Now we've got the I/O size and the last offset,
	 * so start figuring out how to align our
	 * buffers.
	 */
	xfs_next_bmap(mp, imap, bmapp, iosize, iosize, ioalign,
		      last_offset, offset_fsb, nisize);
	ASSERT((bmapp->length > 0) &&
	       (offset >= XFS_FSB_TO_B(mp, bmapp->offset)));
	
	if (XFS_FSB_TO_B(mp, bmapp->offset + bmapp->length) >= nisize) {
		bmapp->eof |= BMAP_EOF;
	}

	bmapp->pboff = offset - XFS_FSB_TO_B(mp, bmapp->offset);
	retrieved_bytes = bmapp->bsize - bmapp->pboff;
	total_retrieved_bytes = 0;
	bmapp->pbsize = xfs_retrieved(retrieved_bytes, offset, count,
				      &total_retrieved_bytes, nisize);
	xfs_iomap_map_trace(XFS_IOMAP_READ_MAP,
			    ip, offset, count, bmapp, imap);

	/*
	 * Only map additional buffers if they've been asked for
	 * and the I/O being done is sequential and has reached the
	 * point where we need to initiate more read ahead or we didn't get
	 * the whole request in the first bmap.
	 */
	last_fsb = XFS_B_TO_FSB(mp, nisize);
	filled_bmaps = 1;
	last_required_offset = bmapp[0].offset;
	first_read_ahead_bmapp = NULL;
	if ((*nbmaps > 1) &&
	    (((offset == ip->i_next_offset) &&
	      (offset != 0) &&
	      (offset_fsb >= ip->i_reada_blkno)) ||
	     retrieved_bytes < count)) {
		curr_bmapp = &bmapp[0];
		next_bmapp = &bmapp[1];
		last_bmapp = &bmapp[*nbmaps - 1];
		curr_imapp = &imap[0];
		last_imapp = &imap[nimaps - 1];

		/*
		 * curr_bmap is always the last one we filled
		 * in, and next_bmapp is always the next one
		 * to be filled in.
		 */
		while (next_bmapp <= last_bmapp) {
			next_offset = curr_bmapp->offset +
				      curr_bmapp->length;
			if (next_offset >= last_fsb) {
				/*
				 * We've mapped all the way to the EOF.
				 * Everything beyond there is inaccessible,
				 * so get out now.
				 */
				break;
			}

			last_iosize = curr_bmapp->length;
			if (next_offset <
			    (curr_imapp->br_startoff +
			     curr_imapp->br_blockcount)) {
				xfs_next_bmap(mp, curr_imapp,
					 next_bmapp, iosize, last_iosize, -1,
					 curr_bmapp->offset, next_offset,
					 nisize);
			} else {
				curr_imapp++;
				if (curr_imapp <= last_imapp) {
					xfs_next_bmap(mp,
					    curr_imapp, next_bmapp,
					    iosize, last_iosize, -1,
					    curr_bmapp->offset, next_offset,
					    nisize);	      
				} else {
					/*
					 * We're out of imaps.  We
					 * either hit the end of
					 * file or just didn't give
					 * enough of them to bmapi.
					 * The caller will just come
					 * back if we haven't done
					 * enough yet.
					 */
					break;
				}
			}
			
			filled_bmaps++;
			curr_bmapp = next_bmapp;
			next_bmapp++;
			ASSERT(curr_bmapp->length > 0);
		       
			/*
			 * Make sure to fill in the pboff and pbsize
			 * fields as long as the bmaps are required for
			 * the request (as opposed to strictly read-ahead).
			 */
			if (total_retrieved_bytes < count) {
				curr_bmapp->pboff = 0;
				curr_bmapp->pbsize =
					xfs_retrieved(curr_bmapp->bsize,
						      offset, count,
						      &total_retrieved_bytes,
						      nisize);
			}
			
			if (XFS_FSB_TO_B(mp, curr_bmapp->offset +
					 curr_bmapp->length) >= nisize) {
				curr_bmapp->eof |= BMAP_EOF;
			}
			xfs_iomap_map_trace(XFS_IOMAP_READ_MAP, ip, offset,
					    count, curr_bmapp, curr_imapp);

 			/*
			 * Keep track of the offset of the last buffer
			 * needed to satisfy the I/O request.  This will
			 * be used for i_io_offset later.  Also record
			 * the first bmapp used to track a read ahead.
			 */
			if (XFS_FSB_TO_B(mp, curr_bmapp->offset) <
			    (offset + count)) {
				last_required_offset = curr_bmapp->offset;
			} else if (first_read_ahead_bmapp == NULL) {
				first_read_ahead_bmapp = curr_bmapp;
			}

		}

		/*
		 * If we're describing any read-ahead here, then move
		 * the read-ahead blkno up to the point where we've
		 * gone through half the read-ahead described here.
		 * This way we don't issue more read-ahead until we
		 * are half-way through the last read-ahead.
		 * 
		 * If we're not describing any read-ahead because the
		 * request is just fragmented, then move up the
		 * read-ahead blkno to just past what we're returning
		 * so that we can maybe start it on the next request.
		 */
		if (first_read_ahead_bmapp != NULL) {
			read_aheads = curr_bmapp - first_read_ahead_bmapp +1;
			next_read_ahead_bmapp = first_read_ahead_bmapp +
						(read_aheads / 2);
			ip->i_reada_blkno = next_read_ahead_bmapp->offset;
		} else {
			ip->i_reada_blkno = curr_bmapp->offset +
					    curr_bmapp->length;
		}
	} else if ((*nbmaps > 1) && (offset != ip->i_io_offset)) {
		/*
		 * In this case the caller is not yet accessing the
		 * file sequentially, but set the read-ahead blkno
		 * so that we can tell if they start doing so.
		 */
		ip->i_reada_blkno = bmapp[0].offset + bmapp[0].length;
	}

	ASSERT(iosize <= XFS_BB_TO_FSBT(mp, XFS_MAX_BMAP_LEN_BB));
	ip->i_io_size = iosize;
	ip->i_io_offset = last_required_offset;
	if (count > ip->i_last_req_sz) {
		/*
		 * Record the "last request size" for the file.
		 * We don't let it shrink so that big requests
		 * that are not satisfied in one call here still
		 * record the full request size (not the smaller
		 * one that comes in to finish mapping the request).
		 */
		ip->i_last_req_sz = count;
	}
	if (total_retrieved_bytes >= count) {
		/*
		 * We've mapped all of the caller's request, so
		 * the next request in a sequential read will
		 * come in the block this one ended on or the
		 * next if we consumed up to the very end of
		 * a block.
		 */
		ip->i_next_offset = offset + count;
	} else {
		/*
		 * We didn't satisfy the entire request, so we
		 * can expect xfs_read_file() to come back with
		 * what is left of the request.
		 */
		ip->i_next_offset = offset + total_retrieved_bytes;
	}
	mutex_unlock(&ip->i_rlock);

	*nbmaps = filled_bmaps;
	for (x = 0; x < filled_bmaps; x++) {
		curr_bmapp = &bmapp[x];
		if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
			curr_bmapp->pbdev = mp->m_rtdev;
		} else {
			curr_bmapp->pbdev = mp->m_dev;
		}
		ASSERT(curr_bmapp->offset <= XFS_B_TO_FSB(mp, nisize));
		curr_bmapp->offset = XFS_FSB_TO_BB(mp, curr_bmapp->offset);
		curr_bmapp->length = XFS_FSB_TO_BB(mp, curr_bmapp->length);
		ASSERT(curr_bmapp->length > 0);
		ASSERT((x == 0) ||
		       ((bmapp[x - 1].offset + bmapp[x - 1].length) ==
			curr_bmapp->offset));
		if (curr_bmapp->bn != -1) {
			curr_bmapp->bn = XFS_FSB_TO_DB(ip, curr_bmapp->bn);
		}
		curr_bmapp->pmp = pmp;
	}
	return 0;
}				

/* ARGSUSED */
int
xfs_vop_readbuf(bhv_desc_t 	*bdp,
		off_t		offset,
		ssize_t		len,
		int		ioflags,
		struct cred	*creds,
		flid_t		*fl,
		buf_t		**rbuf,
		int		*pboff,
		int		*pbsize)
{
	vnode_t		*vp;
	xfs_inode_t	*ip;
	int		error;
	struct bmapval	bmaps[2];
	int		nmaps;
	buf_t		*bp;
	extern void	chunkrelse(buf_t *bp);
	int		unlocked;
	int		lockmode;

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	*rbuf = NULL;
	*pboff = *pbsize = -1;
	error = 0;

	if (!(ioflags & IO_ISLOCKED))
		xfs_rwlockf(bdp, VRWLOCK_READ, 0);

	if (XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		error = XFS_ERROR(EIO);
		goto out;
	}

	/*
	 * blow this off if mandatory locking or DMI are involved
	 */
	if ((vp->v_flag & (VENF_LOCKING|VFRLOCKS)) == (VENF_LOCKING|VFRLOCKS))
		goto out;

	if (DM_EVENT_ENABLED(vp->v_vfsp, ip, DM_EVENT_READ) && !(ioflags & IO_INVIS))
		goto out;

	unlocked = 0;
	lockmode = xfs_ilock_map_shared(ip);

	if (offset >= ip->i_d.di_size) {
		xfs_iunlock_map_shared(ip, lockmode);
		goto out;
	}

	/*
	 * prohibit iomap read from giving us back our data in
	 * two buffers but let it set up read-ahead.  Turn off
	 * read-ahead for NFSv2.  It's I/O sizes are too small
	 * to be of any real benefit (8K reads, 32K read buffers).
	 */
	nmaps = (ioflags & IO_NFS) ? 1 : 2;

	error = xfs_iomap_read(ip, offset, len, bmaps, &nmaps, NULL, &unlocked,
				lockmode);
	if (!unlocked)
		xfs_iunlock_map_shared(ip, lockmode);

	/*
	 * if the first bmap doesn't match the I/O request, forget it.
	 * This means that we can't fit the request into one buffer.
	 */
	if (error ||
	    ((bmaps[0].pbsize != len) &&
	     (bmaps[0].eof & BMAP_EOF) == 0))
		goto out;

	/*
	 * if the caller has specified that the I/O fit into
	 * one page and it doesn't, forget it.  The caller won't
	 * be able to use it.
	 */
	if ((ioflags & IO_ONEPAGE)
	    && pnum(offset) != pnum(offset + bmaps[0].pbsize-1)) {
		goto out;
	}

	bp = chunkread(vp, bmaps, nmaps, creds);

	if (bp->b_flags & B_ERROR) {
		error = geterror(bp);
		ASSERT(error != EINVAL);
		/*
		 * b_relse functions like chunkhold
		 * expect B_DONE to be there.
		 */
		bp->b_flags |= B_DONE|B_STALE;
		brelse(bp);
		goto out;
	}

	if ((bmaps[0].pboff + bmaps[0].pbsize) == bmaps[0].bsize)
		bp->b_relse = chunkrelse;

	*rbuf = bp;
	*pboff = bmaps[0].pboff;
	*pbsize = bmaps[0].pbsize;

	xfs_ichgtime(ip, XFS_ICHGTIME_ACC);
out:
	if (!(ioflags & IO_ISLOCKED))
		xfs_rwunlockf(bdp, VRWLOCK_READ, 0);
	return XFS_ERROR(error);
}

/*
 * set of routines (xfs_lockdown_iopages, xfs_unlock_iopages,
 * xfs_mapped_biomove) to deal with i/o to or from mmapped files where
 * some or all of the user buffer is mmapped to the file passed in
 * as the target of the read/write system call.
 * 
 * there are 6 sets of deadlocks and possible problems.
 *
 * 1)	the i/o is a read, the user buffer lies in a region
 *	that is mapped to the file and not paged in.  the fault
 *	code calls VOP_READ.  But we deadlock if another thread
 *	has tried to write the file in the meantime because he's
 *	now waiting on the lock and the i/o lock has to be a barrier
 *	lock to prevent writer starvation.  this is addressed by calling
 *	the new VASOP, verifyvnmap that returns a set of maps indicating
 *	which ranges of the biomove'd virtual addresses are mmapped to
 *	the file.  the i/o path then breaks up the biomove (using
 *	xfs_mapped_biomove) into pieces and enables nested locking
 *	on the i/o lock during the biomove calls that could result
 *	in page faults that need to read data from the file.
 *
 * 2)   like above only the i/o is a write.  the page fault deadlocks
 *	instantly since we already hold the i/o lock in write mode and
 *	the xfs_read called by the page fault needs it in read mode.
 *	this one is handled as above by enabling nested locking around
 *	the appropriate biomove calls.
 *
 * 3)	the i/o is a read, the user buffer lies in a region
 *	that is mapped autogrow,shared, and the biomove filling
 *	the user buffer causes the file to grow.  the page fault
 *	triggered by the biomove needs to run xfs_write() and take
 *	the i/o lock in write mode.  this is addressed by making the
 *	read path smart enough to detect this condition using the
 *	information returned by the verifyvnmap VASOP and take the
 *	i/o lock in update mode at the beginning.  we then enable
 *	recursive locking (see xfs_ilock) to allow the fault to obtain
 *	the i/o lock regardless of whose waiting on it.
 *
 * 4)	deadlock in pagewait().  if the biomove faults and needs a
 *	page that exists but that page was created by the chunkread
 *	issued by xfs_read/xfs_write, the page won't be marked P_DONE
 *	(and usable by the fault code) until the i/o finishes and releases
 *	the buffer.  so the fault code will find the page and wait forever
 *	waiting for it to be marked P_DONE.  this case is handled by
 *	useracc'ing the dangerous pages before the chunkread so that
 *	the pages exist prior to the chunkread.  this case only happens
 *	if the range of file offsets touched by the i/o overlap with
 *	the range of file offsets associated with the mmapped virtual
 *	addresses touched by the biomove.
 *
 * 5)	buffer deadlock.  in the autogrow case, it's possible that
 *	that there can be no page overlap but that the file blocks
 *	that need to be initialized for the autogrow by xfs_zeroeof_blocks()
 *	and the file blocks in the i/o will both wind up living in the same
 *	buffer set up by the i/o path.  this will deadlock because the
 *	the buffer will be held by the i/o path when the biomove faults
 *	causing the autogrow process to deadlock on the buffer semaphore.
 *	this case is handled like case #2.  we create the pages and cause
 *	the autogrow to happen before the chunkread so that the fault/autogrow
 *	code and the io path code don't have to use the same buffer at the
 *	same time.
 *
 * 6)	a write i/o that passes in a data buffer that is mmapped beyond
 *	the current EOF.  Even if nested locking is enabled, the write path
 *	assumes that because buffered writes are single-threaded only one
 *	buffered writer can use the gap list and other inode fields at once.
 *	this is addressed by faulting in the user virtual address associated
 *	mapped to the largest file offset mapped by the buffer.  the fault
 *	occurs after the i/o lock is taken in write mode but before any real
 *	work is done.  this allows the VM system to issue a VOP_WRITE to
 *	grow the file to the appropriate point.  then the real write call
 *	executes after the file has been grown.  the fault is issued with
 *	nested locking enabled so the write-path inode fields are used
 *	serially while still holding onto the i/o lock to prevent a race
 *	with truncate().
 *
 * Situations this code does NOT attempt to deal with:
 *
 *	- the above situations only we're doing direct I/O instead of
 *	  buffered I/O.
 *
 *	- situations arising from the mappings changing while our i/o
 *	  is in progress.  it's possible that someone could remap part
 *	  of the buffer to make it dangerous after we've called to
 *	  verifyvnmap but before we've done all our biomoves.  fixing
 *	  this would require serializing i/o's and mmaps/munmaps
 *	  and I'd rather not do that.
 */

/*
 * routines to enable/disable/query nested iolock locking and isolate
 * the curuthread references to make the Cellular Irix merge easier
 */
void
xfs_enable_nested_locking(void)
{
	ASSERT_ALWAYS(curuthread->ut_vnlock == 0);
	curuthread->ut_vnlock = UT_FSNESTED;
}

void
xfs_disable_nested_locking(void)
{
	ASSERT_ALWAYS(curuthread->ut_vnlock == UT_FSNESTED);
	curuthread->ut_vnlock = 0;
}

int
xfs_is_nested_locking_enabled(void)
{
	return curuthread && curuthread->ut_vnlock & UT_FSNESTED;
}

/*
 * xfs_lockdown_iopages() - lock down any mmapped user pages required for
 *	this i/o.  this is either
 *
 *	1) pages which will be referenced by the biomove and whose backing
 *		file blocks will be directly read/written by the i/o
 *	2) pages which will be referenced by the biomove, whose backing
 *		file blocks will reside in the same buffer used by the i/o,
 *		and whose file blocks are beyond the current EOF causing
 *		the file to be grown as part of the biomove
 *
 *	if any pages are locked down, *useracced is set to 1 and a set
 *	of xfs_uaccmap_t's are returned indicating the set of address ranges
 *	were locked down.  these pages should be unlocked as soon as
 *	possible after they are biomoved.
 *
 * returns ENOMEM if the number of pages being locked down exceeds
 * maxdmasz.  the pages should be unlocked ASAP after the biomove
 * using xfs_unlock_iopages().
 */
/* ARGSUSED */
int
xfs_lockdown_iopages(
	struct bmapval	*bmapp,
	xfs_fsize_t	isize,		/* in - current inode size/eof */
	int		vnmapflags,	/* in - map flags */
	vnmap_t		**vnmapp,	/* in/out - vmaps array */
	int		*nvnmapp,	/* in/out - number of valid maps left */
	xfs_uaccmap_t	*uaccmap,	/* in - caller supplied useracc maps */
	int		*nuaccmapp,	/* out - number of filled in uaccmaps */
	int		*useracced)	/* out - did we useracc anything */
{
	int		nuaccmaps;
	vnmap_t		*vnmap = *vnmapp;
	uvaddr_t	useracc_startvaddr;
	uvaddr_t	useracc_endvaddr;
	size_t		useracc_len;	
	__psunsigned_t	uacc_startshift;	
	__psunsigned_t	uacc_endshift;	
	uvaddr_t	agac_vaddr_start;
	uvaddr_t	agac_vaddr_end;
	__psunsigned_t	start_trim;	
	off_t		bmap_end;
	off_t		bmap_start;
	off_t		vnmap_end;
	off_t		overlap_off_start;
	off_t		overlap_off_end;
	off_t		iomove_off_start;
	off_t		iomove_off_end;
	off_t		curr_offset;
	int		error;
	int		numpages;

	/*
	 * do we have overlapping pages we need to
	 * useracc?  don't have to worry about readahead
	 * buffers since those pages will be marked
	 * P_DONE by the buffer release function out of
	 * biodone when the i/o to the buffer finishes.
	 */
	ASSERT(*nuaccmapp >= *nvnmapp);
	ASSERT((vnmapflags & (AS_VNMAP_OVERLAP|AS_VNMAP_AUTOGROW)));

	error = numpages = *useracced = *nuaccmapp = nuaccmaps = 0;
	curr_offset = 0;
	bmap_start = BBTOOFF(bmapp[0].offset);
	bmap_end = bmap_start + BBTOOFF(bmapp[0].length);

	/*
	 * process all vnmaps up through the end of the current
	 * buffer
	 */
	while (vnmap && curr_offset < bmap_end &&
	       vnmap->vnmap_ovoffset < bmap_end) {
		/*
		 * skip over maps that aren't marked as overlap or
		 * autogrow
		 */
		if (!(vnmap->vnmap_flags &
		      (AS_VNMAP_OVERLAP|AS_VNMAP_AUTOGROW))) {
			(*nvnmapp)--;
			if (*nvnmapp >= 0)
				vnmap++;
			else
				vnmap = NULL;
			continue;
		}

		/*
		 * if we haven an autogrow region, if the iomove
		 * grows the file, we need to calculate if any part
		 * of the iomove that is beyond the EOF will land
		 * in this buffer.  if so, we need to lock those
		 * pages down now otherwise the xfs_write called by
		 * the fault code will need to zero the area of the
		 * file between the current and new EOF but it has to
		 * to get the buffer to do that.  the problem is
		 * the buffer will be held (already locked) by the
		 * i/o so we'll deadlock.
		 */
		if (isize >= 0 && vnmap->vnmap_flags & AS_VNMAP_AUTOGROW) {
			/*
			 * calculate file offsets touched by the iomove
			 * indicated in this vnmap
			 */
			start_trim = vnmap->vnmap_ovvaddr - vnmap->vnmap_vaddr;
			iomove_off_start = vnmap->vnmap_ovoffset - start_trim;
			iomove_off_end = iomove_off_start + vnmap->vnmap_len;

			/*
			 * determine if any of the uimoved pages are in
			 * the buffer that will be set up by this bmap.
			 * we have to lock down any pages in the buffer
			 * between eof and the end of the uiomove to
			 * grow the file out to the end of the uiomove.
			 */
			if (isize < bmap_start) {
				overlap_off_start = MAX(bmap_start,
							iomove_off_start);
			} else {
				overlap_off_start = MIN(isize,
							iomove_off_start);
			}

			overlap_off_end = MIN(iomove_off_end, bmap_end);

			/*
			 * if so, set up the useracc range to span those
			 * pages.  the useracc range can only grow larger
			 * than that range.  it can't get smaller.
			 */
			if (overlap_off_end - overlap_off_start > 0) {
				agac_vaddr_start = vnmap->vnmap_vaddr +
					(iomove_off_start > overlap_off_start ?
					 iomove_off_start - overlap_off_start :
					 0);
				agac_vaddr_end = vnmap->vnmap_vaddr +
					vnmap->vnmap_len -
					 (iomove_off_end > overlap_off_end ?
					  iomove_off_end - overlap_off_end :
					  0);
			} else {
				agac_vaddr_start = (uvaddr_t) -1LL;
				agac_vaddr_end = (uvaddr_t) 0LL;
			}
		} else {
			agac_vaddr_start = (uvaddr_t) -1LL;
			agac_vaddr_end = (uvaddr_t) 0LL;
		}

		/*
		 * useracc the smallest possible range.  don't bother
		 * unless the overlap specified in the vnmap overlaps
		 * the file offsets specified for the buffer by the bmap.
		 */
		vnmap_end = vnmap->vnmap_ovoffset + vnmap->vnmap_ovlen;
		overlap_off_start = MAX(vnmap->vnmap_ovoffset, bmap_start);
		overlap_off_end = MIN(vnmap_end, bmap_end);

		if (vnmap->vnmap_flags & AS_VNMAP_OVERLAP &&
		    overlap_off_end - overlap_off_start > 0) {
			uacc_startshift = overlap_off_start -
						vnmap->vnmap_ovoffset;
			uacc_endshift = vnmap_end - overlap_off_end;
			ASSERT(overlap_off_start - vnmap->vnmap_ovoffset >= 0);
			ASSERT(vnmap_end - overlap_off_end >= 0);
			useracc_startvaddr = vnmap->vnmap_ovvaddr +
						uacc_startshift;
			useracc_endvaddr = vnmap->vnmap_ovvaddr +
						 vnmap->vnmap_ovlen -
						 uacc_endshift;
		} else {
			useracc_startvaddr = (uvaddr_t) -1LL;
			useracc_endvaddr = (uvaddr_t) 0LL;
		}

		/*
		 * enlarge range if necessary to include
		 * pages that have to be pinned because they
		 * would cause a zero-eof autogrow scenario
		 */
		useracc_startvaddr = MIN(useracc_startvaddr, agac_vaddr_start);
		useracc_endvaddr = MAX(useracc_endvaddr, agac_vaddr_end);

		if (useracc_startvaddr != (uvaddr_t) -1LL &&
		    useracc_endvaddr != (uvaddr_t) 0LL) {
			/*
			 * enable recursive locking so the fault handler won't
			 * block on the i/o lock when setting up the pages.
			 * round the lockdown range to page boundaries.
			 * make sure we don't pin more than maxdmasz pages
			 * per i/o.  that's not allowed.
			 */
			ASSERT(useracc_endvaddr > useracc_startvaddr);
			useracc_startvaddr = (uvaddr_t)
						ctob(btoct(useracc_startvaddr));
			useracc_endvaddr = (uvaddr_t)
						ctob(btoc(useracc_endvaddr));
			useracc_len = useracc_endvaddr - useracc_startvaddr;
			numpages += btoc(useracc_len);

			if (numpages > maxdmasz) {
				cmn_err(CE_WARN,
"xfs_lockdown_iopages needed to pin %d pages, maxdmasz = %d\nPlease increase maxdmasz and try again.\n",
					numpages, maxdmasz);
				error = XFS_ERROR(ENOMEM);
				break;
			}

			xfs_enable_nested_locking();
			error = useracc((void *)useracc_startvaddr, useracc_len,
					B_READ|B_PHYS, NULL);
			xfs_disable_nested_locking();
			if (!error) {
				*useracced = 1;
				uaccmap->xfs_uacstart = useracc_startvaddr;
				uaccmap->xfs_uaclen = useracc_len;
				uaccmap++;
				nuaccmaps++;
			}
		}

		/*
		 * have to check next vmap if this vmap ends
		 * before the buffer does
		 */
		if (bmap_end >= vnmap_end) {
			(*nvnmapp)--;
			if (*nvnmapp >= 0)
				vnmap++;
			else
				vnmap = NULL;
			curr_offset = vnmap_end;
		} else
			curr_offset = bmap_end;
	}

	*nuaccmapp = nuaccmaps;
	*vnmapp = vnmap;

	return error;
}

/*
 * xfs_unlock_iopages() - unlock the set of pages specified in the
 *	xfs_uaccmap_t's set up by xfs_lockdown_iopages().
 */
void
xfs_unlock_iopages(
	xfs_uaccmap_t	*uaccmap,	/* useracc maps */
	int		nuaccmaps)	/* number of filled in uaccmaps */

{
	int	i;

	for (i = 0; i < nuaccmaps; i++, uaccmap++) {
		ASSERT(uaccmap->xfs_uaclen <= ((size_t)-1));
		unuseracc((void *)uaccmap->xfs_uacstart,
			  (size_t)uaccmap->xfs_uaclen,
			  B_PHYS|B_READ);
	}

	return;
}

/*
 * handles biomoves of data where some of the user addresses are mapped to
 * the file being read/written.  each vnmap_t represents a range of addresses
 * mapped to a file.  that range needs to be biomoved with recursive locking
 * enabled so we don't deadlock on the i/o lock when faulting in the biomove.
 * however, we don't want recursive locking enabled on any other page since
 * we could screw up the locking on other inodes if we try and take the
 * i/o lock on a different inode where we don't hold the lock and we have
 * recursive locking enabled.
 *
 * note -- we could do one biomove if we were willing to add an
 * inode pointer in the uthread along with the vnlocks field.  this
 * code trades off increased complexity and slower execution speed
 * (the extra biomoves plus the cycles required in pas_verifyvnmap
 * to set up the additional vnmap's that we might not need if we
 * weren't breaking up our biomoves) suffered only in the danger cases
 * against memory bloat in the uthread structure that would be suffered by
 * all uthreads.
 *
 * vnmapp and nvnmapp are set to the first map that hasn't completely been
 * moved and the count of remaining valid maps respectively.
 */
int
xfs_mapped_biomove(
	struct buf	*bp,
	u_int		pboff,
	size_t		io_len,
	enum uio_rw	rw,
	struct uio	*uiop,
	vnmap_t		**vnmapp,
	int		*nvnmapp)
{
	uvaddr_t	io_end;
	uvaddr_t	current_vaddr;
	int		numvnmaps = *nvnmapp;
	vnmap_t		*vnmap = *vnmapp;
	size_t		partial_io_len;
	size_t		vmap_io_len;
	int		error = 0;

	ASSERT(uiop->uio_iovcnt == 1);

	/*
	 * do nothing if the requested biomove is entirely before
	 * the first vnmap address range
	 */
	current_vaddr = uiop->uio_iov->iov_base;
	io_end = current_vaddr + io_len;

	if (io_end < vnmap->vnmap_vaddr)
		return XFS_ERROR(biomove(bp, pboff, io_len, rw, uiop));

	/*
	 * move as much as we can (up to the first vnmap)
	 */
	if (current_vaddr < vnmap->vnmap_vaddr) {
		partial_io_len = MIN(io_len,
				 (__psunsigned_t) vnmap->vnmap_vaddr -
				 (__psunsigned_t) uiop->uio_iov->iov_base);
		if (error = biomove(bp, pboff, partial_io_len, rw, uiop))
			return XFS_ERROR(error);
		pboff += partial_io_len;
		io_len -= partial_io_len;
		current_vaddr += partial_io_len;
	}

	while (vnmap && current_vaddr < io_end) {
		/*
		 * move what we can of the first vnmap.  allow recursive
		 * io-lock locking in the biomove.
		 */
		ASSERT(uiop->uio_iov->iov_base >= vnmap->vnmap_vaddr);

		ASSERT(current_vaddr >= vnmap->vnmap_vaddr);
		vmap_io_len = vnmap->vnmap_len -
				((__psunsigned_t) uiop->uio_iov->iov_base -
				 (__psunsigned_t) vnmap->vnmap_vaddr);
		partial_io_len = MIN(vmap_io_len, io_len);

		xfs_enable_nested_locking();
		if (error = biomove(bp, pboff, partial_io_len, rw, uiop)) {
			xfs_disable_nested_locking();
			error = XFS_ERROR(error);
			break;
		}
		xfs_disable_nested_locking();
		pboff += partial_io_len;
		io_len -= partial_io_len;
		current_vaddr += partial_io_len;

		/*
		 * did we move the entire vnmap?  if so, look at the next map
		 * and move the data up to the next vnmap if there is one
		 * (or finish up the I/O if we're out of maps)
		 */
		ASSERT(current_vaddr <= vnmap->vnmap_vaddr + vnmap->vnmap_len);
		if (current_vaddr == vnmap->vnmap_vaddr + vnmap->vnmap_len) {
			numvnmaps--;
			if (numvnmaps > 0) {
				vnmap++;
				partial_io_len = MIN(
					(__psunsigned_t)vnmap->vnmap_vaddr -
					(__psunsigned_t)uiop->uio_iov->iov_base,
						     io_len);
			} else {
				vnmap = NULL;
				partial_io_len = io_len;
			}
			if (partial_io_len > 0) {
				if (error = biomove(bp, pboff, partial_io_len,
							rw, uiop)) {
					error = XFS_ERROR(error);
					break;
				}
				pboff += partial_io_len;
				io_len -= partial_io_len;
				current_vaddr += partial_io_len;
			}
		}
	}

	*nvnmapp = numvnmaps;
	*vnmapp = vnmap;

	return error;
}

/* ARGSUSED */		
int
xfs_read_file(
	bhv_desc_t	*bdp,	      
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	vnmap_t		*vnmaps,
	int		numvnmaps,
	const uint	vnmapflags,
	xfs_uaccmap_t	*uaccmaps,
	xfs_fsize_t	isize)
{
	struct bmapval	bmaps[XFS_MAX_RW_NBMAPS];
	struct bmapval	*bmapp;
	int		nbmaps;
	vnode_t		*vp;
	buf_t		*bp;
	int		read_bmaps;
	int		buffer_bytes_ok;
	xfs_inode_t	*ip;
	int		error;
	int		unlocked;
	unsigned int	lockmode;
	int		useracced = 0;
	vnmap_t		*cur_ldvnmap = vnmaps;
	int		num_ldvnmaps = numvnmaps;
	int		num_biovnmaps = numvnmaps;
	int		nuaccmaps;
	int		do_lockdown = vnmapflags & (AS_VNMAP_OVERLAP |
						    AS_VNMAP_AUTOGROW);
	vnmap_t		*cur_biovnmap = vnmaps;

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	if (XFS_FORCED_SHUTDOWN(ip->i_mount))
		return XFS_ERROR(EIO);
	error = 0;
	buffer_bytes_ok = 0;
	XFSSTATS.xs_read_calls++;
	XFSSTATS.xs_read_bytes += uiop->uio_resid;

	/*
	 * Loop until uio->uio_resid, which is the number of bytes the
	 * caller has requested, goes to 0 or we get an error.  Each
	 * call to xfs_iomap_read tries to map as much of the request
	 * plus read-ahead as it can.  We must hold the inode lock
	 * exclusively when calling xfs_iomap_read.
	 * Bug 516806: Introduced i_rlock to protect the readahead state
	 * therefore do not need to hold the inode lock in exclusive
	 * mode except when we first read in the file and the extents
	 * are in btree format - xfs_ilock_map_shared takes care of it.
	 */
	do {
		lockmode = xfs_ilock_map_shared(ip);
		xfs_rw_enter_trace(XFS_READ_ENTER, ip, uiop, ioflag);

		/*
		 * We've fallen off the end of the file, so
		 * just return with what we've done so far.
		 */
		if (uiop->uio_offset >= ip->i_d.di_size) {
			xfs_iunlock_map_shared(ip, lockmode);
			break;
		}
 
		unlocked = 0;
		nbmaps = ip->i_mount->m_nreadaheads ;
		ASSERT(nbmaps <= sizeof(bmaps) / sizeof(bmaps[0]));

		/*
		 * XXX - rcc - we could make sure that if an overlap
		 * exists, that we don't set up bmaps that are > maxdmasz
		 * pages
		 */
		error = xfs_iomap_read(ip, uiop->uio_offset, uiop->uio_resid,
			bmaps, &nbmaps, uiop->uio_pmp, &unlocked, lockmode);

		if (!unlocked)
			xfs_iunlock_map_shared(ip, lockmode);

		if (error || (bmaps[0].pbsize == 0)) {
			break;
		}

		bmapp = &bmaps[0];
		read_bmaps = nbmaps;
		ASSERT(BBTOOFF(bmapp->offset) <= uiop->uio_offset);
		/*
		 * The first time through this loop we kick off I/O on
		 * all the bmaps described by the iomap_read call.
		 * Subsequent passes just wait for each buffer needed
		 * to satisfy this request to complete.  Buffers which
		 * are started in the first pass but are actually just
		 * read ahead buffers are never waited for, since uio_resid
		 * will go to 0 before we get to them.
		 *
		 * This works OK because iomap_read always tries to
		 * describe all the buffers we need to satisfy this
		 * read call plus the necessary read-ahead in the
		 * first call to it.
		 */
		while ((uiop->uio_resid != 0) && (nbmaps > 0)) {
			/*
			 * do we have overlapping pages we need to
			 * useracc?  don't have to worry about readahead
			 * buffers since those pages will be marked
			 * P_DONE by the buffer release function out of
			 * biodone when the i/o to the buffer finishes.
			 */
			if (cur_ldvnmap && do_lockdown) {
				nuaccmaps = numvnmaps;
				if (xfs_lockdown_iopages(bmapp, isize,
							vnmapflags,
							&cur_ldvnmap,
							&num_ldvnmaps,
							uaccmaps, &nuaccmaps,
							&useracced)) {
					if (useracced)
						xfs_unlock_iopages(uaccmaps,
								    nuaccmaps);
					error = XFS_ERROR(ENOMEM);
					useracced = 0;
					break;
				}
			}

			bp = chunkread(vp, bmapp, read_bmaps, credp);

			if (bp->b_flags & B_ERROR) {
				error = geterror(bp);
				ASSERT(error != EINVAL);
				/*
				 * b_relse functions like chunkhold
				 * expect B_DONE to be there.
				 */
				bp->b_flags |= B_DONE|B_STALE;
				brelse(bp);
				break;
			} else if (bp->b_resid != 0) {
				buffer_bytes_ok = 0;
				bp->b_flags |= B_DONE|B_STALE;
				brelse(bp);
				break;
			} else {
				buffer_bytes_ok = 1;
				ASSERT((BBTOOFF(bmapp->offset) + bmapp->pboff)
				       == uiop->uio_offset);
				if (!cur_biovnmap) {
					error = biomove(bp, bmapp->pboff,
							bmapp->pbsize, UIO_READ,
							uiop);
				} else {
#pragma mips_frequency_hint NEVER
					/*
					 * break up the biomoves so that
					 * we never biomove across a region
					 * that might fault on more than
					 * one inode
					 */
					error = xfs_mapped_biomove(bp,
							bmapp->pboff,
							bmapp->pbsize,
							UIO_READ, uiop,
							&cur_biovnmap,
							&num_biovnmaps);
				}
				if (error) {
					bp->b_flags |= B_DONE|B_STALE;
					brelse(bp);
					break;
				}
			}

			brelse(bp);

			if (useracced) {
				xfs_unlock_iopages(uaccmaps, nuaccmaps);
				useracced = 0;
			}

			XFSSTATS.xs_read_bufs++;
			read_bmaps = 1;
			nbmaps--;
			bmapp++;
		}

		if (useracced) {
			xfs_unlock_iopages(uaccmaps, nuaccmaps);
			useracced = 0;
		}
	} while (!error && (uiop->uio_resid != 0) && buffer_bytes_ok);

	return error;
}

/*
 * xfs_read
 *
 * This is the XFS VOP_READ entry point.  It does some minimal
 * error checking and then switches out based on the file type.
 */
int
xfs_read(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	xfs_inode_t	*ip;
	int		type;
	off_t		offset;
	off_t		n;
	size_t		count;
	int		error;
	int		lflag;
	vrwlock_t	lmode;
	xfs_mount_t	*mp;
	size_t		resid;
	vnode_t 	*vp;
	vasid_t			vasid;
	as_verifyvnmap_t	vnmap_args;
	as_verifyvnmapres_t	vnmap_res;
	vnmap_t			vnmaps[XFS_NUMVNMAPS];
	vnmap_t			*rvnmaps;
	int			num_rvnmaps;
	int			rvnmap_flags;
	int			rvnmap_size = 0;
	xfs_uaccmap_t		uaccmap_array[XFS_NUMVNMAPS];
	xfs_uaccmap_t		*uaccmaps;
	xfs_fsize_t		isize;

#if defined(DEBUG) && defined(UIOSZ_DEBUG)
	/*
	 * Randomly set io size
	 */
	extern ulong_t	random(void);
	extern int	srandom(int);
	timespec_t	now;		/* current time */
	static int	seed = 0;	/* randomizing seed value */

	if (!seed) {
		nanotime(&now);
		seed = (int)now.tv_sec ^ (int)now.tv_nsec;
		srandom(seed);
	}
	ioflag |= IO_UIOSZ;
	uiop->uio_readiolog = (random() & 0x3) + XFS_UIO_MIN_READIO_LOG;
#endif

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	lmode = VRWLOCK_READ;
	lflag = 0;

	/*
	 * need to protect against deadlocks that can occur if the
	 * biomove touches a virtual address in user space that is
	 * mapped to the file being read.  This only works for
	 * read/write and pread/pwrite.  readv/writev lose.
	 * direct i/o loses too for now.
	 *
	 * note that if someone remaps the user buffer to this file
	 * while the I/O in progess, we lose, too.  instant deadlock.
	 */
	rvnmaps = NULL;
	num_rvnmaps = 0;
	rvnmap_flags = 0;
	uaccmaps = NULL;

	if (uiop->uio_segflg == UIO_USERSPACE && uiop->uio_iovcnt == 1 &&
	    !(ioflag & IO_DIRECT) && VN_MAPPED(vp)) {
#pragma mips_frequency_hint NEVER
		as_lookup_current(&vasid);
		VAS_LOCK(vasid, AS_SHARED);

		vnmap_args.as_vp = vp;
		vnmap_args.as_vaddr = (uvaddr_t)
					ctob(btoct(uiop->uio_iov->iov_base));
		vnmap_args.as_len = uiop->uio_iov->iov_len +
				((__psunsigned_t) uiop->uio_iov->iov_base -
				 (__psunsigned_t) vnmap_args.as_vaddr);
		vnmap_args.as_offstart = uiop->uio_offset;
		vnmap_args.as_offend = uiop->uio_offset + uiop->uio_resid;
		vnmap_args.as_vnmap = vnmaps;
		vnmap_args.as_nmaps = XFS_NUMVNMAPS;

		if (error = VAS_VERIFYVNMAP(vasid, &vnmap_args, &vnmap_res)) {
			VAS_UNLOCK(vasid);
			return error;
		}

		VAS_UNLOCK(vasid);

		if (vnmap_res.as_rescodes) {
			rvnmaps = (vnmap_res.as_multimaps == NULL)
					? vnmaps 
					: vnmap_res.as_multimaps;
			rvnmap_flags = vnmap_res.as_rescodes;
			num_rvnmaps = vnmap_res.as_nmaps;
			rvnmap_size = vnmap_res.as_mapsize;
			if (num_rvnmaps <= XFS_NUMVNMAPS)
				uaccmaps = uaccmap_array;
			else if ((uaccmaps = kmem_alloc(num_rvnmaps *
						sizeof(xfs_uaccmap_t),
						KM_SLEEP)) == NULL) {
				error = XFS_ERROR(ENOMEM);
				goto out;
			}
		}
	}

	/*
	 * check if we're in recursive lock mode (a read inside a biomove
	 * to a page that is mapped to ip that faulted)
	 */
	lflag = xfs_is_nested_locking_enabled()
		 ? XFS_IOLOCK_NESTED
		 : 0;
	if (!(ioflag & IO_ISLOCKED)) {
		/* For calls from the paging system where the faulting
		 * process is multithreaded, try to grab the I/O lock,
		 * if it is already held, then we ask the paging system
		 * to try again by returning EAGAIN.  It's safe to return
		 * directly since the UIO_NOSPACE i/o never takes the
		 * aspacelock (VAS_LOCK) above.
		 */
		if ((uiop->uio_segflg == UIO_NOSPACE) &&
		    (ioflag & IO_MTTHREAD) && VN_MAPPED(vp)) {
			if (!xfs_ilock_nowait(ip, XFS_IOLOCK_SHARED|lflag)) {
				return XFS_ERROR(EAGAIN);
			}
		} else {
			xfs_rwlockf(bdp, VRWLOCK_READ, lflag);
		}
	}

	/*
	 * if we're in a possible mmap autogrow case, check to
	 * see if a biomove is going to have to grow the file.
	 * if so, drop the iolock and re-obtain it in write mode.
	 * it's possible that someone might have grown the file
	 * while we were re-acquiring the lock.  if so, then we
	 * demote the iolock from exclusive back to shared and
	 * proceed onwards.
	 */
	isize = -1;

	if (rvnmap_flags & AS_VNMAP_AUTOGROW) {
		xfs_ilock(ip, XFS_ILOCK_SHARED);

		if (vnmap_res.as_maxoffset <= ip->i_d.di_size)
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
		else {
			/*
			 * note, we don't have to worry about the
			 * multi-threaded ilock_nowait case above
			 * because the fault path will never biomove
			 * a page and cause an autogrow fault
			 */
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			xfs_rwunlockf(bdp, VRWLOCK_READ, lflag);
			xfs_rwlockf(bdp, VRWLOCK_WRITE, lflag);
			xfs_ilock(ip, XFS_ILOCK_SHARED);
			if (vnmap_res.as_maxoffset > ip->i_d.di_size) {
				isize = ip->i_d.di_size;
				xfs_iunlock(ip, XFS_ILOCK_SHARED);
				lmode = VRWLOCK_WRITE;
			} else {
				xfs_iunlock(ip, XFS_ILOCK_SHARED);
				xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
			}
		}
	}


	type = ip->i_d.di_mode & IFMT;
	ASSERT(type == IFDIR ||
	       ismrlocked(&ip->i_iolock, MR_ACCESS | MR_UPDATE) != 0);

	ASSERT(type == IFREG || type == IFDIR ||
	       type == IFLNK || type == IFSOCK);

	offset = uiop->uio_offset;
	count = uiop->uio_resid;

	/* check for locks if some exist and mandatory locking is enabled */
	if ((vp->v_flag & (VENF_LOCKING|VFRLOCKS)) == 
	    (VENF_LOCKING|VFRLOCKS)) {
		error = fs_checklock(vp, FREAD, offset, count, uiop->uio_fmode,
				     credp, fl, VRWLOCK_READ);
		if (error)
			goto out;
	}

	if (offset < 0) {
		error = XFS_ERROR(EINVAL);
		goto out;
	}
	if ((ssize_t)count <= 0) {
		error = (ssize_t)count < 0 ? XFS_ERROR(EINVAL) : 0;
		goto out;
	}
	if (ioflag & IO_RSYNC) {
		/* First we sync the data */
		if ((ioflag & IO_SYNC) || (ioflag & IO_DSYNC)) {
			chunkpush(vp, offset, offset + count - 1, 0);
		}
		/* Now we sync the timestamps */
		if (ioflag & IO_SYNC) {
			xfs_ilock(ip, XFS_ILOCK_SHARED);
			xfs_iflock(ip);
			error = xfs_iflush(ip, XFS_IFLUSH_SYNC);
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			if (error == EFSCORRUPTED)
				goto out;
		} else {
			if (ioflag & IO_DSYNC) {
				mp = ip->i_mount;
				xfs_log_force(mp, (xfs_lsn_t)0,
					      XFS_LOG_FORCE | XFS_LOG_SYNC );
			}
		}
	}
	switch (type) {
	case IFREG:
		/*
		 * Don't allow reads to pass down counts which could
		 * overflow.  Be careful to record the part that we
		 * refuse so that we can add it back into uio_resid
		 * so that the caller will see a short read.
		 */
		n = XFS_MAX_FILE_OFFSET - offset;
		if (n <= 0) {
			error = 0;
			goto out;
		}
		if (n < uiop->uio_resid) {
			resid = uiop->uio_resid - n;
			uiop->uio_resid = n;
		} else {
			resid = 0;
		}
			    
		/*
		 * Not ready for in-line files yet.
		 */
		ASSERT((ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS) ||
		       (ip->i_d.di_format == XFS_DINODE_FMT_BTREE));

		if (DM_EVENT_ENABLED(vp->v_vfsp, ip, DM_EVENT_READ) &&
		    !(ioflag & IO_INVIS)) {
			vrwlock_t	locktype = VRWLOCK_READ;

			error = xfs_dm_send_data_event(DM_EVENT_READ, bdp,
					offset, count,
					UIO_DELAY_FLAG(uiop), &locktype);
			if (error)
				goto out;
		}

		/*
		 * Respect preferred read size if indicated in uio structure.
		 * But if the read size has already been set, go with the
		 * smallest value.  Silently ignore requests that aren't
		 * within valid I/O size limits.
		 */
		mp = ip->i_mount;

		if ((ioflag & IO_UIOSZ) &&
		    uiop->uio_readiolog != ip->i_readio_log &&
		    uiop->uio_readiolog >= mp->m_sb.sb_blocklog &&
		    uiop->uio_readiolog >= XFS_UIO_MIN_READIO_LOG &&
		    uiop->uio_readiolog <= XFS_UIO_MAX_READIO_LOG) {
			xfs_ilock(ip, XFS_ILOCK_EXCL);
#if !(defined(DEBUG) && defined(UIOSZ_DEBUG))
			if (!(ip->i_flags & XFS_IUIOSZ) ||
			    uiop->uio_readiolog < ip->i_readio_log) {
#endif /* ! (DEBUG && UIOSZ_DEBUG) */
				ip->i_readio_log =  uiop->uio_readiolog;
				ip->i_readio_blocks = 1 <<
						(int) (ip->i_readio_log -
							mp->m_sb.sb_blocklog);
				/*
				 * set inode max io field to largest
				 * possible value that could have been
				 * applied to the inode
				 */
				if (!(ip->i_flags & XFS_IUIOSZ))  {
					ip->i_max_io_log = MAX(ip->i_max_io_log,
							MAX(mp->m_readio_log,
							    ip->i_readio_log));
					ip->i_flags |= XFS_IUIOSZ;
				}
#if defined(DEBUG) && defined(UIOSZ_DEBUG)
				atomicAddInt(&uiodbg_switch, 1);
				atomicAddInt(
					&(uiodbg_readiolog[ip->i_readio_log -
						XFS_UIO_MIN_READIO_LOG]),
					1);
#endif
#if !(defined(DEBUG) && defined(UIOSZ_DEBUG))
			}
#endif /* ! (DEBUG && UIOSZ_DEBUG) */
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		}

		if (ioflag & IO_DIRECT) {
			error = xfs_diordwr(bdp, uiop, ioflag, credp, B_READ);
		} else {
			error = xfs_read_file(bdp, uiop, ioflag, credp,
						rvnmaps, num_rvnmaps,
						rvnmap_flags, uaccmaps, isize);
		}

		ASSERT(ismrlocked(&ip->i_iolock, MR_ACCESS | MR_UPDATE) != 0);
		/* don't update timestamps if doing invisible I/O */
		if (!(ioflag & IO_INVIS)) {
			xfs_ichgtime(ip, XFS_ICHGTIME_ACC);
		}

		/*
		 * Add back whatever we refused to do because of file
		 * size limitations.
		 */
		uiop->uio_resid += resid;

		break;

	case IFDIR:
		error = XFS_ERROR(EISDIR);
		break;

	case IFLNK:
		error = XFS_ERROR(EINVAL);
		break;
	      
	case IFSOCK:
		error = XFS_ERROR(ENODEV);
		break;

	default:
		ASSERT(0);
		error = XFS_ERROR(EINVAL);
		break;
	}

out:
	if (rvnmap_size > 0)
		kmem_free(rvnmaps, rvnmap_size);

	if (num_rvnmaps > XFS_NUMVNMAPS)
		kmem_free(uaccmaps, num_rvnmaps * sizeof(xfs_uaccmap_t));

	if (!(ioflag & IO_ISLOCKED))
		xfs_rwunlockf(bdp, lmode, lflag);

	return error;
}


/*
 * Map the given I/O size and I/O alignment over the given extent.
 * If we're at the end of the file and the underlying extent is
 * delayed alloc, make sure we extend out to the
 * next i_writeio_blocks boundary.  Otherwise make sure that we
 * are confined to the given extent.
 */
/*ARGSUSED*/
STATIC void
xfs_write_bmap(
	xfs_mount_t	*mp,
	xfs_bmbt_irec_t	*imapp,
	struct bmapval	*bmapp,
	int		iosize,
	xfs_fileoff_t	ioalign,
	xfs_fsize_t	isize)
{
	__int64_t	extra_blocks;
	xfs_fileoff_t	size_diff;
	xfs_fileoff_t	ext_offset;
	xfs_fsblock_t	start_block;
	
	if (ioalign < imapp->br_startoff) {
		/*
		 * The desired alignment doesn't end up on this
		 * extent.  Move up to the beginning of the extent.
		 * Subtract whatever we drop from the iosize so that
		 * we stay aligned on iosize boundaries.
		 */
		size_diff = imapp->br_startoff - ioalign;
		iosize -= (int)size_diff;
		ASSERT(iosize > 0);
		ext_offset = 0;
		bmapp->offset = imapp->br_startoff;
	} else {
		/*
		 * The alignment requested fits on this extent,
		 * so use it.
		 */
		ext_offset = ioalign - imapp->br_startoff;
		bmapp->offset = ioalign;
	}
	start_block = imapp->br_startblock;
	ASSERT(start_block != HOLESTARTBLOCK);
	if (start_block != DELAYSTARTBLOCK) {
		bmapp->bn = start_block + ext_offset;
		bmapp->eof = (imapp->br_state != XFS_EXT_UNWRITTEN) ?
					0 : BMAP_UNWRITTEN;
	} else {
		bmapp->bn = -1;
		bmapp->eof = BMAP_DELAY;
	}
	bmapp->length = iosize;

	/*
	 * If the iosize from our offset extends beyond the end of
	 * the extent, then trim down length to match that of the extent.
	 */
	extra_blocks = (off_t)(bmapp->offset + bmapp->length) -
		       (__uint64_t)(imapp->br_startoff +
				    imapp->br_blockcount);
	if (extra_blocks > 0) {
		bmapp->length -= extra_blocks;
		ASSERT(bmapp->length > 0);
	}

	bmapp->bsize = XFS_FSB_TO_B(mp, bmapp->length);
}

/*
 * This routine is called to handle zeroing any space in the last
 * block of the file that is beyond the EOF.  We do this since the
 * size is being increased without writing anything to that block
 * and we don't want anyone to read the garbage on the disk.
 */
/* ARGSUSED */
STATIC int				/* error */
xfs_zero_last_block(
	xfs_inode_t	*ip,
	off_t		offset,
	xfs_fsize_t	isize,
	cred_t		*credp,
	struct pm	*pmp)
{
	xfs_fileoff_t	last_fsb;
	xfs_fileoff_t	next_fsb;
	xfs_fileoff_t	end_fsb;
	xfs_fsblock_t	firstblock;
	xfs_mount_t	*mp;
	buf_t		*bp;
	vnode_t		*vp;
	int		nimaps;
	int		zero_offset;
	int		zero_len;
	int		isize_fsb_offset;
	int		i;
	int		error;
	int		hole;
	pfd_t		*pfdp;
	xfs_bmbt_irec_t	imap;
	struct bmapval	bmap;

	ASSERT(ismrlocked(&ip->i_lock, MR_UPDATE) != 0);
	ASSERT(offset > isize);

	mp = ip->i_mount;
	vp = XFS_ITOV(ip);

	/*
	 * If the file system block size is less than the page size,
	 * then there could be bytes in the last page after the last
	 * fsblock containing isize which have not been initialized.
	 * Since if such a page is in memory it will be marked P_DONE
	 * and may now be fully accessible, we need to zero any part of
	 * it which is beyond the old file size.  We don't need to send
	 * this out to disk, we're just initializing it to zeroes like
	 * we would have done in xfs_strat_read() had the size been bigger.
	 */
	if ((mp->m_sb.sb_blocksize < NBPP) && ((i = poff(isize)) != 0)) {
		pfdp = pfind(vp, offtoct(isize), VM_ATTACH);
		if (pfdp != NULL) {
			page_zero(pfdp, NOCOLOR, i, (NBPP - i));

			/*
			 * Now we check to see if there are any holes in the
			 * page over the end of the file that are beyond the
			 * end of the file.  If so, we want to set the P_HOLE
			 * flag in the page and blow away any active mappings
			 * to it so that future faults on the page will cause
			 * the space where the holes are to be allocated.
			 * This keeps us from losing updates that are beyond
			 * the current end of file when the page is already
			 * in memory.
			 */
			next_fsb = XFS_B_TO_FSBT(mp, isize);
			end_fsb = XFS_B_TO_FSB(mp, ctooff(offtoc(isize)));
			hole = 0;
			while (next_fsb < end_fsb) {
				nimaps = 1;
				firstblock = NULLFSBLOCK;
				error = xfs_bmapi(NULL, ip, next_fsb, 1, 0,
						  &firstblock, 0, &imap,
						  &nimaps, NULL);
				if (error) {
					pagefree(pfdp);
					return error;
				}
				ASSERT(nimaps > 0);
				if (imap.br_startblock == HOLESTARTBLOCK) {
					hole = 1;
					break;
				}
				next_fsb++;
			}
			if (hole) {
				/*
				 * In order to make processes notice the
				 * newly set P_HOLE flag, blow away any
				 * mappings to the file.  We have to drop
				 * the inode lock while doing this to avoid
				 * deadlocks with the chunk cache.
				 */
				if (VN_MAPPED(vp)) {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					VOP_PAGES_SETHOLE(vp, pfdp, 1, 1,
						ctooff(offtoct(isize)));
					xfs_ilock(ip, XFS_ILOCK_EXCL);
				}
			}
			pagefree(pfdp);
		}
	}

	isize_fsb_offset = XFS_B_FSB_OFFSET(mp, isize);
	if (isize_fsb_offset == 0) {
		/*
		 * There are not extra bytes in the last block to
		 * zero, so return.
		 */
		return 0;
	}

	last_fsb = XFS_B_TO_FSBT(mp, isize);
	nimaps = 1;
	firstblock = NULLFSBLOCK;
	error = xfs_bmapi(NULL, ip, last_fsb, 1, 0, &firstblock, 0, &imap,
			  &nimaps, NULL);
	if (error) {
		return error;
	}
	ASSERT(nimaps > 0);
	/*
	 * If the block underlying isize is just a hole, then there
	 * is nothing to zero.
	 */
	if (imap.br_startblock == HOLESTARTBLOCK) {
		return 0;
	}
	/*
	 * Get a buffer for the last block, zero the part beyond the
	 * EOF, and write it out sync.  We need to drop the ilock
	 * while we do this so we don't deadlock when the buffer cache
	 * calls back to us.
	 */
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	bmap.offset = XFS_FSB_TO_BB(mp, last_fsb);
	bmap.length = XFS_FSB_TO_BB(mp, 1);
	bmap.bsize = BBTOB(bmap.length);
	bmap.pboff = 0;
	bmap.pbsize = bmap.bsize;
	if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
		bmap.pbdev = mp->m_rtdev;
	} else {
		bmap.pbdev = mp->m_dev;
	}
	bmap.eof = BMAP_EOF;
	bmap.pmp = pmp;
	if (imap.br_startblock != DELAYSTARTBLOCK) {
		bmap.bn = XFS_FSB_TO_DB(ip, imap.br_startblock);
		if (imap.br_state == XFS_EXT_UNWRITTEN)
			bmap.eof |= BMAP_UNWRITTEN;
	} else {
		bmap.bn = -1;
		bmap.eof |= BMAP_DELAY;
	}
	bp = chunkread(vp, &bmap, 1, credp);
	if (bp->b_flags & B_ERROR) {
		error = geterror(bp);
		bp->b_flags |= B_DONE|B_STALE;
		brelse(bp);
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		return error;
	}
	zero_offset = isize_fsb_offset;
	zero_len = mp->m_sb.sb_blocksize - isize_fsb_offset;
	xfs_zero_bp(bp, zero_offset, zero_len);
	/*
	 * We don't want to start a transaction here, so don't
	 * push out a buffer over a delayed allocation extent.
	 * Also, we can get away with it since the space isn't
	 * allocated so it's faster anyway.
	 *
	 * We don't bother to call xfs_b*write here since this is
	 * just userdata, and we don't want to bring the filesystem
	 * down if they hit an error. Since these will go through
	 * xfsstrategy anyway, we have control over whether to let the
	 * buffer go thru or not, in case of a forced shutdown.
	 */
	ASSERT(bp->b_vp);
	if (imap.br_startblock == DELAYSTARTBLOCK ||
	    imap.br_state == XFS_EXT_UNWRITTEN) {
		bdwrite(bp);
	} else {
		error = bwrite(bp);
	}

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	return error;
}

/*
 * Zero any on disk space between the current EOF and the new,
 * larger EOF.  This handles the normal case of zeroing the remainder
 * of the last block in the file and the unusual case of zeroing blocks
 * out beyond the size of the file.  This second case only happens
 * with fixed size extents and when the system crashes before the inode
 * size was updated but after blocks were allocated.  If fill is set,
 * then any holes in the range are filled and zeroed.  If not, the holes
 * are left alone as holes.
 */
int					/* error */
xfs_zero_eof(
	xfs_inode_t	*ip,
	off_t		offset,
	xfs_fsize_t	isize,
	cred_t		*credp,
	struct pm	*pmp)
{
	xfs_fileoff_t	start_zero_fsb;
	xfs_fileoff_t	end_zero_fsb;
	xfs_fileoff_t	prev_zero_fsb;
	xfs_fileoff_t	zero_count_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_fsblock_t	firstblock;
	xfs_extlen_t	buf_len_fsb;
	xfs_extlen_t	prev_zero_count;
	xfs_mount_t	*mp;
	buf_t		*bp;
	int		nimaps;
	int		error;
	xfs_bmbt_irec_t	imap;
	struct bmapval	bmap;
	pfd_t		*pfdp;
	int		i;
	vnode_t		*vp;
	int		length;

	ASSERT(ismrlocked(&(ip->i_lock), MR_UPDATE));
	ASSERT(ismrlocked(&(ip->i_iolock), MR_UPDATE));

	mp = ip->i_mount;
	vp = XFS_ITOV(ip);

	/*
	 * First handle zeroing the block on which isize resides.
	 * We only zero a part of that block so it is handled specially.
	 */
	error = xfs_zero_last_block(ip, offset, isize, credp, pmp);
	if (error) {
		ASSERT(ismrlocked(&(ip->i_lock), MR_UPDATE));
		ASSERT(ismrlocked(&(ip->i_iolock), MR_UPDATE));
		return error;
	}

	/*
	 * Calculate the range between the new size and the old
	 * where blocks needing to be zeroed may exist.  To get the
	 * block where the last byte in the file currently resides,
	 * we need to subtract one from the size and truncate back
	 * to a block boundary.  We subtract 1 in case the size is
	 * exactly on a block boundary.
	 */
	last_fsb = isize ? XFS_B_TO_FSBT(mp, isize - 1) : (xfs_fileoff_t)-1;
	start_zero_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)isize);
	end_zero_fsb = XFS_B_TO_FSBT(mp, offset - 1);
	ASSERT((xfs_sfiloff_t)last_fsb < (xfs_sfiloff_t)start_zero_fsb);
	if (last_fsb == end_zero_fsb) {
		/*
		 * The size was only incremented on its last block.
		 * We took care of that above, so just return.
		 */
		return 0;
	}

	ASSERT(start_zero_fsb <= end_zero_fsb);
	prev_zero_fsb = NULLFILEOFF;
	prev_zero_count = 0;
	while (start_zero_fsb <= end_zero_fsb) {
		nimaps = 1;
		zero_count_fsb = end_zero_fsb - start_zero_fsb + 1;
		firstblock = NULLFSBLOCK;
		error = xfs_bmapi(NULL, ip, start_zero_fsb, zero_count_fsb,
				  0, &firstblock, 0, &imap, &nimaps, NULL);
		if (error) {
			ASSERT(ismrlocked(&(ip->i_lock), MR_UPDATE));
			ASSERT(ismrlocked(&(ip->i_iolock), MR_UPDATE));
			return error;
		}
		ASSERT(nimaps > 0);

		if (imap.br_startblock == HOLESTARTBLOCK) {
			/* 
			 * This loop handles initializing pages that were
			 * partially initialized by the code below this 
			 * loop. It basically zeroes the part of the page
			 * that sits on a hole and sets the page as P_HOLE
			 * and calls remapf if it is a mapped file.
			 */	
			if ((prev_zero_fsb != NULLFILEOFF) && 
			    (dtopt(XFS_FSB_TO_BB(mp, prev_zero_fsb)) ==
			     dtopt(XFS_FSB_TO_BB(mp, imap.br_startoff)) ||
			     dtopt(XFS_FSB_TO_BB(mp, prev_zero_fsb + 
						     prev_zero_count)) ==
			     dtopt(XFS_FSB_TO_BB(mp, imap.br_startoff)))) {

				pfdp = pfind(vp, dtopt(XFS_FSB_TO_BB(mp, 
						imap.br_startoff)), VM_ATTACH);

				if (pfdp != NULL) {
					i = poff(XFS_FSB_TO_B(mp, 
							imap.br_startoff));
					length = MIN(NBPP - i, XFS_FSB_TO_B(mp, 
							 imap.br_blockcount));

					page_zero(pfdp, NOCOLOR, i, length);

					if (VN_MAPPED(vp))
					    VOP_PAGES_SETHOLE(vp, pfdp, 1, 1, 
					    ctooff(offtoct(XFS_FSB_TO_B(mp, 
					    imap.br_startoff))));
                                        pagefree(pfdp);
                                }
			}
		   	prev_zero_fsb = NULLFILEOFF;
			prev_zero_count = 0;
		   	start_zero_fsb = imap.br_startoff +
					 imap.br_blockcount;
			ASSERT(start_zero_fsb <= (end_zero_fsb + 1));
			continue;
		}

		/*
		 * There are blocks in the range requested.
		 * Zero them a single write at a time.  We actually
		 * don't zero the entire range returned if it is
		 * too big and simply loop around to get the rest.
		 * That is not the most efficient thing to do, but it
		 * is simple and this path should not be exercised often.
		 */
		buf_len_fsb = XFS_FILBLKS_MIN(imap.br_blockcount,
					      ip->i_writeio_blocks);

		/*
		 * Drop the inode lock while we're doing the I/O.
		 * We'll still have the iolock to protect us.
		 */
		xfs_iunlock(ip, XFS_ILOCK_EXCL);

		bmap.offset = XFS_FSB_TO_BB(mp, imap.br_startoff);
		bmap.length = XFS_FSB_TO_BB(mp, buf_len_fsb);
		bmap.bsize = BBTOB(bmap.length);
		bmap.eof = BMAP_EOF;
		bmap.pmp = pmp;
		if (imap.br_startblock == DELAYSTARTBLOCK) {
			bmap.eof |= BMAP_DELAY;
			bmap.bn = -1;
		} else {
			bmap.bn = XFS_FSB_TO_DB(ip, imap.br_startblock);
			if (imap.br_state == XFS_EXT_UNWRITTEN)
				bmap.eof |= BMAP_UNWRITTEN;
		}
		if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
			bmap.pbdev = mp->m_rtdev;
		} else {
			bmap.pbdev = mp->m_dev;
		}
		bp = getchunk(XFS_ITOV(ip), &bmap, credp);

#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			extern void biozero(struct buf *, u_int, int);
			biozero(bp, 0, bmap.bsize);
		} else
#endif
		{	
			bp_mapin(bp);
			bzero(bp->b_un.b_addr, bp->b_bcount);
	        }
		ASSERT(bp->b_vp);
		buftrace("XFS ZERO EOF", bp);
		if (imap.br_startblock == DELAYSTARTBLOCK ||
		    imap.br_state == XFS_EXT_UNWRITTEN) {
			bdwrite(bp);
		} else {
			error = bwrite(bp);
			if (error) {
				xfs_ilock(ip, XFS_ILOCK_EXCL);
				return error;
			}
		}

		prev_zero_fsb = start_zero_fsb;
		prev_zero_count = buf_len_fsb;
		start_zero_fsb = imap.br_startoff + buf_len_fsb;
		ASSERT(start_zero_fsb <= (end_zero_fsb + 1));

		xfs_ilock(ip, XFS_ILOCK_EXCL);
	}

	return 0;
}

STATIC int
xfs_iomap_write(
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count,
	struct bmapval	*bmapp,
	int		*nbmaps,
	int		ioflag,
	struct pm	*pmp)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	ioalign;
	xfs_fileoff_t	next_offset_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_fileoff_t	bmap_end_fsb;
	xfs_fileoff_t	last_file_fsb;
	xfs_fileoff_t	start_fsb;
	xfs_filblks_t	count_fsb;
	off_t		aligned_offset;
	xfs_fsize_t	isize;
	xfs_fsblock_t	firstblock;
	__uint64_t	last_page_offset;
	int		nimaps;
	int		error;
	int		n;
	unsigned int	iosize;
	unsigned int	writing_bytes;
	short		filled_bmaps;
	short		x;
	short		small_write;
	size_t		count_remaining;
	xfs_mount_t	*mp;
	struct bmapval	*curr_bmapp;
	struct bmapval	*next_bmapp;
	struct bmapval	*last_bmapp;
	xfs_bmbt_irec_t	*curr_imapp;
	xfs_bmbt_irec_t	*last_imapp;
#define	XFS_WRITE_IMAPS	XFS_BMAP_MAX_NMAP
	xfs_bmbt_irec_t	imap[XFS_WRITE_IMAPS];
	int		aeof;

	ASSERT(ismrlocked(&ip->i_lock, MR_UPDATE) != 0);

	xfs_iomap_enter_trace(XFS_IOMAP_WRITE_ENTER, ip, offset, count);

	mp = ip->i_mount;
	ASSERT(! XFS_NOT_DQATTACHED(mp, ip));

	if (ip->i_new_size > ip->i_d.di_size) {
		isize = ip->i_new_size;
	} else {
		isize = ip->i_d.di_size;
	}

	aeof = 0;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	/*
	 * If the caller is doing a write at the end of the file,
	 * then extend the allocation (and the buffer used for the write)
	 * out to the file system's write iosize.  We clean up any extra
	 * space left over when the file is closed in xfs_inactive().
	 * We can only do this if we are sure that we will create buffers
	 * over all of the space we allocate beyond the end of the file.
	 * Not doing so would allow us to create delalloc blocks with
	 * no pages in memory covering them.  So, we need to check that
	 * there are not any real blocks in the area beyond the end of
	 * the file which we are optimistically going to preallocate. If
	 * there are then our buffers will stop when they encounter them
	 * and we may accidentally create delalloc blocks beyond them
	 * that we never cover with a buffer.  All of this is because
	 * we are not actually going to write the extra blocks preallocated
	 * at this point.
	 *
	 * We don't bother with this for sync writes, because we need
	 * to minimize the amount we write for good performance.
	 */
	if (!(ioflag & IO_SYNC) && ((offset + count) > ip->i_d.di_size)) {
		start_fsb = XFS_B_TO_FSBT(mp,
				  ((xfs_ufsize_t)(offset + count - 1)));
		count_fsb = ip->i_writeio_blocks;
		while (count_fsb > 0) {
			nimaps = XFS_WRITE_IMAPS;
			firstblock = NULLFSBLOCK;
			error = xfs_bmapi(NULL, ip, start_fsb, count_fsb,
					  0, &firstblock, 0, imap, &nimaps,
					  NULL);
			if (error) {
				return error;
			}
			for (n = 0; n < nimaps; n++) {
				if ((imap[n].br_startblock != HOLESTARTBLOCK) &&
				    (imap[n].br_startblock != DELAYSTARTBLOCK)) {
					goto write_map;
				}
				start_fsb += imap[n].br_blockcount;
				count_fsb -= imap[n].br_blockcount;
				ASSERT(count_fsb < 0xffff000);
			}
		}
		iosize = ip->i_writeio_blocks;
		aligned_offset = XFS_WRITEIO_ALIGN(ip, (offset + count - 1));
		ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		last_fsb = ioalign + iosize;
		aeof = 1;
	}
 write_map:
	nimaps = XFS_WRITE_IMAPS;
	firstblock = NULLFSBLOCK;

	/*
	 * roundup the allocation request to m_dalign boundary if file size
	 * is greater that 512K and we are allocating past the allocation eof
	 */
	if (mp->m_dalign && (ip->i_d.di_size >= 524288) && aeof) {
		int eof;
		xfs_fileoff_t new_last_fsb;
		new_last_fsb = roundup(last_fsb, mp->m_dalign);
		error = xfs_bmap_eof(ip, new_last_fsb, XFS_DATA_FORK, &eof);
		if (error) {
			return error;
		}
		if (eof)
			last_fsb = new_last_fsb;
	}

	error = xfs_bmapi(NULL, ip, offset_fsb,
			  (xfs_filblks_t)(last_fsb - offset_fsb),
			  XFS_BMAPI_DELAY | XFS_BMAPI_WRITE |
			  XFS_BMAPI_ENTIRE, &firstblock, 1, imap,
			  &nimaps, NULL);
	/* 
	 * This can be EDQUOT, if nimaps == 0
	 */
	if (error) {
		return error;
	}
	/*
	 * If bmapi returned us nothing, and if we didn't get back EDQUOT,
	 * then we must have run out of space.
	 */
	if (nimaps == 0) {
		xfs_iomap_enter_trace(XFS_IOMAP_WRITE_NOSPACE,
				      ip, offset, count);
		return XFS_ERROR(ENOSPC);
	}

	if (!(ioflag & IO_SYNC) ||
	    ((last_fsb - offset_fsb) >= ip->i_writeio_blocks)) {
		/*
		 * For normal or large sync writes, align everything
		 * into i_writeio_blocks sized chunks.
		 */
		iosize = ip->i_writeio_blocks;
		aligned_offset = XFS_WRITEIO_ALIGN(ip, offset);
		ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		small_write = 0;
	} else {
		/*
		 * For small sync writes try to minimize the amount
		 * of I/O we do.  Round down and up to the larger of
		 * page or block boundaries.  Set the small_write
		 * variable to 1 to indicate to the code below that
		 * we are not using the normal buffer alignment scheme.
		 */
		if (NBPP > mp->m_sb.sb_blocksize) {
			aligned_offset = ctooff(offtoct(offset));
			ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
			last_page_offset = ctob64(btoc64(offset + count));
			iosize = XFS_B_TO_FSBT(mp, last_page_offset -
					       aligned_offset);
		} else {
			ioalign = offset_fsb;
			iosize = last_fsb - offset_fsb;
		}
		small_write = 1;
	}

	/*
	 * Now map our desired I/O size and alignment over the
	 * extents returned by xfs_bmapi().
	 */
	xfs_write_bmap(mp, imap, bmapp, iosize, ioalign, isize);
	ASSERT((bmapp->length > 0) &&
	       (offset >= XFS_FSB_TO_B(mp, bmapp->offset)));

	/*
	 * A bmap is the EOF bmap when it reaches to or beyond the new
	 * inode size.
	 */
	bmap_end_fsb = bmapp->offset + bmapp->length;
	if (XFS_FSB_TO_B(mp, bmap_end_fsb) >= isize) {
		bmapp->eof |= BMAP_EOF;
	}
	bmapp->pboff = offset - XFS_FSB_TO_B(mp, bmapp->offset);
	writing_bytes = bmapp->bsize - bmapp->pboff;
	if (writing_bytes > count) {
		/*
		 * The mapping is for more bytes than we're actually
		 * going to write, so trim writing_bytes so we can
		 * get bmapp->pbsize right.
		 */
		writing_bytes = count;
	}
	bmapp->pbsize = writing_bytes;

	xfs_iomap_map_trace(XFS_IOMAP_WRITE_MAP,
			    ip, offset, count, bmapp, imap);

	/*
	 * Map more buffers if the first does not map the entire
	 * request.  We do this until we run out of bmaps, imaps,
	 * or bytes to write.
	 */
	last_file_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)isize));
	filled_bmaps = 1;
	if ((*nbmaps > 1) &&
	    ((nimaps > 1) || (bmapp->offset + bmapp->length <
	     imap[0].br_startoff + imap[0].br_blockcount)) &&
	    (writing_bytes < count)) {
		curr_bmapp = &bmapp[0];
		next_bmapp = &bmapp[1];
		last_bmapp = &bmapp[*nbmaps - 1];
		curr_imapp = &imap[0];
		last_imapp = &imap[nimaps - 1];
		count_remaining = count - writing_bytes;

		/*
		 * curr_bmapp is always the last one we filled
		 * in, and next_bmapp is always the next one to
		 * be filled in.
		 */
		while (next_bmapp <= last_bmapp) {
			next_offset_fsb = curr_bmapp->offset +
					  curr_bmapp->length;
			if (next_offset_fsb >= last_file_fsb) {
				/*
				 * We've gone beyond the region asked for
				 * by the caller, so we're done.
				 */
				break;
			}
			if (small_write) {
				iosize -= curr_bmapp->length;
				ASSERT((iosize > 0) ||
				       (curr_imapp == last_imapp));
				/*
				 * We have nothing more to write, so
				 * we're done.
				 */
				if (iosize == 0) {
					break;
				}
			}
			if (next_offset_fsb <
			    (curr_imapp->br_startoff +
			     curr_imapp->br_blockcount)) {
				/*
				 * I'm still on the same extent, so
				 * the last bmap must have ended on
				 * a writeio_blocks boundary.  Thus,
				 * we just start where the last one
				 * left off.
				 */
				ASSERT((XFS_FSB_TO_B(mp, next_offset_fsb) &
					((1 << (int) ip->i_writeio_log) - 1))
					==0);
				xfs_write_bmap(mp, curr_imapp, next_bmapp,
					       iosize, next_offset_fsb,
					       isize);
			} else {
				curr_imapp++;
				if (curr_imapp <= last_imapp) {
					/*
					 * We're moving on to the next
					 * extent.  Since we try to end
					 * all buffers on writeio_blocks
					 * boundaries, round next_offset
					 * down to a writeio_blocks boundary
					 * before calling xfs_write_bmap().
					 *
					 * For small, sync writes we don't
					 * bother with the alignment stuff.
					 *
					 * XXXajs
					 * Adding a macro to writeio align
					 * fsblocks would be good to reduce
					 * the bit shifting here.
					 */
					if (small_write) {
						ioalign = next_offset_fsb;
					} else {
						aligned_offset =
							XFS_FSB_TO_B(mp,
							    next_offset_fsb);
						aligned_offset =
							XFS_WRITEIO_ALIGN(ip,
							    aligned_offset);
						ioalign = XFS_B_TO_FSBT(mp,
							    aligned_offset);
					}
					xfs_write_bmap(mp, curr_imapp,
						       next_bmapp, iosize,
						       ioalign, isize);
				} else {
					/*
					 * We're out of imaps.  The caller
					 * will have to call again to map
					 * the rest of the write request.
					 */
					break;
				}
			}
			/*
			 * The write must start at offset 0 in this bmap
			 * since we're just continuing from the last
			 * buffer.  Thus the request offset in the buffer
			 * indicated by pboff must be 0.
			 */
			next_bmapp->pboff = 0;

			/*
			 * The request size within this buffer is the
			 * entire buffer unless the count of bytes to
			 * write runs out.
			 */
			writing_bytes = next_bmapp->bsize;
			if (writing_bytes > count_remaining) {
				writing_bytes = count_remaining;
			}
			next_bmapp->pbsize = writing_bytes;
			count_remaining -= writing_bytes;
			ASSERT(((long)count_remaining) >= 0);

			filled_bmaps++;
			curr_bmapp++;
			next_bmapp++;
			/*
			 * A bmap is the EOF bmap when it reaches to
			 * or beyond the new inode size.
			 */
			bmap_end_fsb = curr_bmapp->offset +
				       curr_bmapp->length;
			if (((xfs_ufsize_t)XFS_FSB_TO_B(mp, bmap_end_fsb)) >=
			    (xfs_ufsize_t)isize) {
				curr_bmapp->eof |= BMAP_EOF;
			}
			xfs_iomap_map_trace(XFS_IOMAP_WRITE_MAP, ip, offset,
					    count, curr_bmapp, curr_imapp);
		}
	}
	*nbmaps = filled_bmaps;
	for (x = 0; x < filled_bmaps; x++) {
		curr_bmapp = &bmapp[x];
		if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
			curr_bmapp->pbdev = mp->m_rtdev;
		} else {
			curr_bmapp->pbdev = mp->m_dev;
		}
		curr_bmapp->offset = XFS_FSB_TO_BB(mp, curr_bmapp->offset);
		curr_bmapp->length = XFS_FSB_TO_BB(mp, curr_bmapp->length);
		ASSERT((x == 0) ||
		       ((bmapp[x - 1].offset + bmapp[x - 1].length) ==
			curr_bmapp->offset));
		if (curr_bmapp->bn != -1) {
			curr_bmapp->bn = XFS_FSB_TO_DB(ip, curr_bmapp->bn);
		}
		curr_bmapp->pmp = pmp;
	}

	return 0;
}

int
xfs_write_file(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	xfs_lsn_t	*commit_lsn_p,
	vnmap_t		*vnmaps,
	int		numvnmaps,
	const uint	vnmapflags,
	xfs_uaccmap_t	*uaccmaps)
{
	struct bmapval	bmaps[XFS_MAX_RW_NBMAPS];
	struct bmapval	*bmapp;
	int		nbmaps;
	vnode_t		*vp;
	buf_t		*bp;
	xfs_inode_t	*ip;
	int		error;
	int		eof_zeroed;
	int		fillhole;
	int		gaps_mapped;
	off_t		offset;
	size_t		count;
	int		read;
	xfs_fsize_t	isize;
	xfs_fsize_t	new_size;
	xfs_mount_t	*mp;
	int		fsynced;
	extern void	chunkrelse(buf_t*);
	int		useracced = 0;
	vnmap_t		*cur_ldvnmap = vnmaps;
	int		num_ldvnmaps = numvnmaps;
	int		num_biovnmaps = numvnmaps;
	int		nuaccmaps;
	vnmap_t		*cur_biovnmap = vnmaps;

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;
	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	/*
	 * If the file has fixed size extents or is a real time file, 
	 * buffered I/O cannot be performed.
	 * This check will be removed in the future.
	 */
	if ((ip->i_d.di_extsize) || 
	    (ip->i_d.di_flags & XFS_DIFLAG_REALTIME))  {
		return (XFS_ERROR(EINVAL));
	}
		
	/* 
	 * Make sure that the dquots are there
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0))
				return (error);
		}
	}

	error = 0;
	eof_zeroed = 0;
	gaps_mapped = 0;
	XFSSTATS.xs_write_calls++;
	XFSSTATS.xs_write_bytes += uiop->uio_resid;

	/*
	 * i_new_size is used by xfs_iomap_read() when the chunk
	 * cache code calls back into the file system through
	 * xfs_bmap().  This way we can tell where the end of
	 * file is going to be even though we haven't yet updated
	 * ip->i_d.di_size.  This is guarded by the iolock and the
	 * inode lock.  Either is sufficient for reading the value.
	 */
	new_size = uiop->uio_offset + uiop->uio_resid;

	/*
	 * i_write_offset is used by xfs_strat_read() when the chunk
	 * cache code calls back into the file system through
	 * xfs_strategy() to initialize a buffer.  We use it there
	 * to know how much of the buffer needs to be zeroed and how
	 * much will be initialize here by the write or not need to
	 * be initialized because it will be beyond the inode size.
	 * This is protected by the io lock.
	 */
	ip->i_write_offset = uiop->uio_offset;

	/*
	 * Loop until uiop->uio_resid, which is the number of bytes the
	 * caller has requested to write, goes to 0 or we get an error.
	 * Each call to xfs_iomap_write() tries to map as much of the
	 * request as it can in ip->i_writeio_blocks sized chunks.
	 */
	if (!((ioflag & (IO_NFS3|IO_NFS)) &&
	    uiop->uio_offset > ip->i_d.di_size &&
	    uiop->uio_offset - ip->i_d.di_size <= (xfs_nfs_io_units *
				 (1 << (int) MAX(ip->i_writeio_log,
				   uiop->uio_writeiolog))))) {
		fillhole = 0;
		offset = uiop->uio_offset;
		count = uiop->uio_resid;
	} else  {
		/*
		 * Cope with NFS out-of-order writes.  If we're
		 * extending eof to a point within the indicated
		 * window, fill any holes between old and new eof.
		 * Set up offset/count so we deal with all the bytes
		 * between current eof and end of the new write.
		 */
		fillhole = 1;
		offset = ip->i_d.di_size;
		count = uiop->uio_offset + uiop->uio_resid - offset;
	}
	fsynced = 0;

	do {
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		isize = ip->i_d.di_size;
		if (new_size > isize) {
			ip->i_new_size = new_size;
		}

		xfs_rw_enter_trace(XFS_WRITE_ENTER, ip, uiop, ioflag);

		/*
		 * If this is the first pass through the loop, then map
		 * out all of the holes we might fill in with this write
		 * and list them in the inode's gap list.  This is for
		 * use by xfs_strat_read() in determining if the real
		 * blocks underlying a delalloc buffer have been initialized
		 * or not.  Since writes are single threaded, if the blocks
		 * were holes when we started and xfs_strat_read() is asked
		 * to read one in while we're still here in xfs_write_file(),
		 * then the block is not initialized.  Only we can
		 * initialize it and once we write out a buffer we remove
		 * any entries in the gap list which overlap that buffer.
		 */
		if (!gaps_mapped) {
			error = xfs_build_gap_list(ip, offset, count);
			if (error) {
				goto error0;
			}
			gaps_mapped = 1;
		}

		/*
		 * If we've seeked passed the EOF to do this write,
		 * then we need to make sure that any buffer overlapping
		 * the EOF is zeroed beyond the EOF.
		 */
		if (!eof_zeroed && uiop->uio_offset > isize && isize != 0) {
			error = xfs_zero_eof(ip, uiop->uio_offset, isize,
						credp, uiop->uio_pmp);
			if (error) {
				goto error0;
			}
			eof_zeroed = 1;
		}

		nbmaps = sizeof(bmaps) / sizeof(bmaps[0]);
		error = xfs_iomap_write(ip, offset, count, bmaps, &nbmaps,
					ioflag, uiop->uio_pmp);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);

		/*
	 	 * Clear out any read-ahead info since the write may
	 	 * have made it invalid.
	 	 */
		if (!error)
			XFS_INODE_CLEAR_READ_AHEAD(ip);

		if (error == ENOSPC) {
			switch (fsynced) {
			case 0:
				VOP_FLUSH_PAGES(vp, 0,
					(off_t)xfs_file_last_byte(ip) - 1, 0,
					FI_NONE, error);
				error = 0;
				fsynced = 1;
				continue;
			case 1:
				fsynced = 2;
				if (!(ioflag & IO_SYNC)) {
					ioflag |= IO_SYNC;
					error = 0;
					continue;
				}
				/* FALLTHROUGH */
			case 2:
			case 3:
				VFS_SYNC(vp->v_vfsp,
					SYNC_NOWAIT|SYNC_BDFLUSH|SYNC_FSDATA,
					get_current_cred(), error);
				error = 0;
				delay(HZ);
				fsynced++;
				continue;
			}
		}
		if (error || (bmaps[0].pbsize == 0)) {
			break;
		}

		fsynced = 0;
		bmapp = &bmaps[0];
		/*
		 * Each pass through the loop writes another buffer
		 * to the file.  For big requests, iomap_write will
		 * have given up multiple bmaps to use so we make fewer
		 * calls to it on big requests than if it only gave
		 * us one at a time.
		 *
		 * Error handling is a bit tricky because of delayed
		 * allocation.  We need to make sure that we create
		 * dirty buffers over all the delayed allocation
		 * extents created in xfs_iomap_write().  Thus, when
		 * we get an error we continue to process each of
		 * the bmaps returned by xfs_iomap_write().  Each is
		 * read in so that it is fully initialized and then
		 * written out without actually copying in the user's
		 * data.
		 */
		while (((uiop->uio_resid != 0) || (error != 0)) &&
		       (nbmaps > 0)) {
			/*
			 * do we have overlapping pages we need to
			 * useracc?  don't have to worry about autogrow
			 * case here as those have been failed earlier
			 */
			if (cur_ldvnmap && vnmapflags & AS_VNMAP_OVERLAP) {
				nuaccmaps = numvnmaps;
				/*
				 * tell xfs_lockdown_iopages to skip
				 * the autogrow checking since any
				 * necessary file growing was handled
				 * at the beginning of xfs_write()
				 */
				if (error = xfs_lockdown_iopages(bmapp, -1,
							vnmapflags,
							&cur_ldvnmap,
							&num_ldvnmaps,
							uaccmaps, &nuaccmaps,
							&useracced)) {
					if (useracced)
						xfs_unlock_iopages(uaccmaps,
								    nuaccmaps);
					error = XFS_ERROR(ENOMEM);
					useracced = 0;
				}
			}

			/*
			 * If the write doesn't completely overwrite
			 * the buffer and we're not writing from
			 * the beginning of the buffer to the end
			 * of the file then we need to read the
			 * buffer.  We also always want to read the
			 * buffer if we've encountered an error and
			 * we're just cleaning up.
			 *
			 * Reading the buffer will send it to xfs_strategy
			 * which will take care of zeroing the holey
			 * parts of the buffer and coordinating with
			 * other, simultaneous writers.
			 */
			if ((error != 0) ||
			    ((bmapp->pbsize != bmapp->bsize) &&
			    !((bmapp->pboff == 0) &&
			      (uiop->uio_offset >= isize)))) {
				bp = chunkread(vp, bmapp, 1, credp);
				read = 1;
			} else {
				bp = getchunk(vp, bmapp, credp);
				read = 0;
			}

			/*
			 * There is not much we can do with buffer errors.
			 * The assumption here is that the space underlying
			 * the buffer must now be allocated (even if it
			 * wasn't when we mapped the buffer) and we got an
			 * error reading from it.  In this case the blocks
			 * will remain unreadable, so we just toss the buffer
			 * and its associated pages.
			 */
			if (bp->b_flags & B_ERROR) {
				error = geterror(bp);
				ASSERT(error != EINVAL);
				bp->b_flags |= B_DONE|B_STALE;
				bp->b_flags &= ~(B_DELWRI);
				brelse(bp);
				bmapp++;
				nbmaps--;
				continue;
			}

			/*
			 * If we've already encountered an error, then
			 * write the buffers out without copying the user's
			 * data into them.  This way we get dirty buffers
			 * over our delayed allocation extents which
			 * have been initialized by xfs_strategy() since
			 * we forced the chunkread() above.
			 * We write the data out synchronously here so that
			 * we don't have to worry about having buffers
			 * possibly out beyond the EOF when we later flush
			 * or truncate the file.  We set the B_STALE bit so
			 * that the buffer will be decommissioned after it
			 * is synced out.
			 */
			if (error != 0) {
				bp->b_flags |= B_STALE;
				(void) bwrite(bp);
				bmapp++;
				nbmaps--;
				continue;
			}

			ASSERT(fillhole || fillhole == 0 &&
					BBTOOFF(bmapp->offset) + bmapp->pboff
						== uiop->uio_offset);

			/*
			 * zero the bytes up to the data being written
			 * but don't overwrite data we read in.  This
			 * zero-fills the buffers we set up for the NFS
			 * case to fill the holes between EOF and the new
			 * write.
			 */
			if (!read && BBTOOFF(bmapp->offset) + bmapp->pboff
					<  uiop->uio_offset)  {
				xfs_zero_bp(bp, bmapp->pboff,
					MIN(bmapp->pbsize, uiop->uio_offset -
					    (BBTOOFF(bmapp->offset) +
					     bmapp->pboff)));
			}

			/*
			 * biomove the data in the region to be written.
			 * In the NFS hole-filling case, don't fill
			 * anything until we hit the first buffer with
			 * data that we have to write.
			 */
			if (!fillhole) {
				if (!cur_biovnmap) {
					error = biomove(bp, bmapp->pboff,
							bmapp->pbsize,
							UIO_WRITE,
							uiop);
				} else {
#pragma mips_frequency_hint NEVER
					/*
					 * break up the biomoves so that
					 * we never biomove across a region
					 * that might fault on more than
					 * one inode
					 */
					error = xfs_mapped_biomove(bp,
							bmapp->pboff,
							bmapp->pbsize,
							UIO_WRITE, uiop,
							&cur_biovnmap,
							&num_biovnmaps);
				}
			} else if (BBTOOFF(bmapp->offset) + bmapp->pboff +
					bmapp->pbsize >= uiop->uio_offset)  {
				/*
				 * NFS - first buffer to be written.  biomove
				 * into the portion of the buffer that the
				 * user originally asked to write to.
				 */
				ASSERT(BBTOOFF(bmapp->offset) + bmapp->pboff
							<= uiop->uio_offset);
				if (!cur_biovnmap) {
					error = biomove(bp,
						  uiop->uio_offset -
						   BBTOOFF(bmapp->offset),
						  bmapp->pbsize -
						    (int)(uiop->uio_offset -
						      (BBTOOFF(bmapp->offset) +
						       bmapp->pboff)),
						  UIO_WRITE,
						  uiop);
				} else {
#pragma mips_frequency_hint NEVER
					/*
					 * break up the biomoves so that
					 * we never biomove across a region
					 * that might fault on more than
					 * one inode
					 */
					error = xfs_mapped_biomove(bp,
						  uiop->uio_offset -
						   BBTOOFF(bmapp->offset),
						  bmapp->pbsize -
						    (int)(uiop->uio_offset -
						      (BBTOOFF(bmapp->offset) +
						       bmapp->pboff)),
						  UIO_WRITE,
						  uiop,
						  &cur_biovnmap,
						  &num_biovnmaps);
				}

				/*
				 * turn off hole-filling code.  The rest
				 * of the buffers can be handled as per
				 * the usual write path.
				 */
				fillhole = 0;
			}

			/*
			 * reset offset/count to reflect the biomove
			 */
			if (!fillhole)  {
				offset = uiop->uio_offset;
				count = uiop->uio_resid;
			} else  {
				/*
				 * NFS - set offset to the beginning
				 * of the next area in the file to be
				 * copied or zero-filled.  Drop count
				 * by the the amount we just zero-filled.
				 */
				offset = BBTOOFF(bmapp->offset) +
					 bmapp->pboff + bmapp->pbsize;
				count -= bmapp->pbsize;
			}

			/*
			 * Make sure that any gap list entries overlapping
			 * the buffer being written are removed now that
			 * we know that the blocks underlying the buffer
			 * will be initialized.  We don't need the inode
			 * lock to manipulate the gap list here, because
			 * we have the io lock held exclusively so noone
			 * else can get to xfs_strat_read() where we look
			 * at the list.
			 */
			xfs_delete_gap_list(ip,
					    XFS_BB_TO_FSBT(mp, bp->b_offset),
					    XFS_B_TO_FSBT(mp, bp->b_bcount));
			ASSERT(bp->b_vp);

			if (error)  {
				/*
				 * If the buffer is already done then just
				 * mark it dirty without copying any more
				 * data into it.  It is already fully
				 * initialized.
				 * Otherwise, we must have getchunk()'d
				 * the buffer above.  Use chunkreread()
				 * to get it initialized by xfs_strategy()
				 * and then write it out.
				 * We write the data out synchronously here
				 * so that we don't have to worry about
				 * having buffers possibly out beyond the
				 * EOF when we later flush or truncate
				 * the file.  We set the B_STALE bit so
				 * that the buffer will be decommissioned
				 * after it is synced out.
				 */

				if (!(bp->b_flags & B_DONE)) {
					chunkreread(bp);
				}

				bp->b_flags |= B_STALE;
				(void) bwrite(bp);
			} else {

				if ((ioflag & IO_SYNC) || (ioflag & IO_DSYNC)) {
					if ((bmapp->pboff + bmapp->pbsize) ==
					    bmapp->bsize) {
						bp->b_relse = chunkrelse;
					}

					/* save the commit lsn for dsync
					 * writes so xfs_write can force the
					 * log up to the appropriate point.
					 * don't bother doing this for sync
					 * writes since xfs_write will have to
					 * kick off a sync xaction to log the
					 * timestamp updates anyway.
					 */

					if (ioflag & IO_DSYNC) {
						bp->b_fsprivate3 = commit_lsn_p;
						bp->b_flags |= B_HOLD;
					}
					error = bwrite(bp);
					if (ioflag & IO_DSYNC) {
						bp->b_fsprivate3 = NULL;
						bp->b_flags &= ~B_HOLD;
						brelse(bp);
					}
				} else {
					bdwrite(bp);
				}
			}

			if (useracced) {
				xfs_unlock_iopages(uaccmaps, nuaccmaps);
				useracced = 0;
			}

			/*
			 * If we've grown the file, get back the
			 * inode lock and move di_size up to the
			 * new size.  It may be that someone else
			 * made it even bigger, so be careful not
			 * to shrink it.
			 *
			 * No one could have shrunk the file, because
			 * we are holding the iolock exclusive.
			 *
			 * Have to update di_size after brelsing the buffer
			 * because if we are running low on buffers and 
			 * xfsd is trying to push out a delalloc buffer for
			 * our inode, then it grabs the ilock in exclusive 
			 * mode to do an allocation, and calls get_buf to 
			 * get to read in a metabuffer (agf, agfl). If the
			 * metabuffer is in the buffer cache, but it gets 
			 * reused before we can grab the cpsema(), then
			 * we will sleep in get_buf waiting for it to be
			 * released whilst holding the ilock.
			 * If it so happens that the buffer was reused by
			 * the above code path, then we end up holding this 
			 * buffer locked whilst we try to get the ilock so we 
			 * end up deadlocking. (bug 504578).
			 * For the IO_SYNC writes, the di_size now gets logged
			 * and synced to disk in the transaction in xfs_write().
			 */  
			if (offset > isize) {
				xfs_ilock(ip, XFS_ILOCK_EXCL);
				if (offset > ip->i_d.di_size) {
					ip->i_d.di_size = offset;
					ip->i_update_core = 1;
					ip->i_update_size = 1;
					isize = offset;
				}
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
			}

			XFSSTATS.xs_write_bufs++;
			bmapp++;
			nbmaps--;
		}
	} while ((uiop->uio_resid > 0) && !error);

	/*
	 * Free up any remaining entries in the gap list, because the 
	 * list only applies to this write call.  Also clear the new_size
	 * field of the inode while we've go it locked.
	 */
	xfs_ilock(ip, XFS_ILOCK_EXCL);
error0:
	xfs_free_gap_list(ip);
	ip->i_new_size = 0;
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	ip->i_write_offset = 0;

	return error;
}

/*
 * This is a subroutine for xfs_write() and other writers
 * (xfs_fcntl) which clears the setuid and setgid bits when a file is written.
 */
int
xfs_write_clear_setuid(
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;
	xfs_trans_t	*tp;
	int		error;

	mp = ip->i_mount;
	tp = xfs_trans_alloc(mp, XFS_TRANS_WRITEID);
	if (error = xfs_trans_reserve(tp, 0,
				      XFS_WRITEID_LOG_RES(mp),
				      0, 0, 0)) {
		xfs_trans_cancel(tp, 0);
		return error;
	}
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	ip->i_d.di_mode &= ~ISUID;

	/*
	 * Note that we don't have to worry about mandatory
	 * file locking being disabled here because we only
	 * clear the ISGID bit if the Group execute bit is
	 * on, but if it was on then mandatory locking wouldn't
	 * have been enabled.
	 */
	if (ip->i_d.di_mode & (IEXEC >> 3)) {
		ip->i_d.di_mode &= ~ISGID;
	}
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_set_sync(tp);
	error = xfs_trans_commit(tp, 0, NULL);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;
}

/*
 * xfs_write
 *
 * This is the XFS VOP_WRITE entry point.  It does some minimal error
 * checking and then switches out based on the file type.
 */
int
xfs_write(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	xfs_inode_t	*ip;
	xfs_mount_t	*mp;
	xfs_trans_t	*tp;
	int		type;
	off_t		offset;
	size_t		count;
	int		error, transerror;
	int		lflag;
	off_t		n;
	int		resid;
	off_t		savedsize;
	xfs_fsize_t	limit;
	int		eventsent;
	vnode_t 	*vp;
	xfs_lsn_t	commit_lsn;
	vasid_t		vasid;
	vnmap_t		vnmaps[XFS_NUMVNMAPS];
	vnmap_t		*rvnmaps;
	vnmap_t		*map;
	int		num_rvnmaps;
	int		rvnmap_flags;
	int		rvnmap_size = 0;
	int		i;
	xfs_uaccmap_t		uaccmap_array[XFS_NUMVNMAPS];
	xfs_uaccmap_t		*uaccmaps;
	as_verifyvnmap_t	vnmap_args;
	as_verifyvnmapres_t	vnmap_res;

#if defined(DEBUG) && defined(UIOSZ_DEBUG)
	/*
	 * Randomly set io size
	 */
	extern ulong_t	random(void);
	extern int	srandom(int);
	timespec_t	now;		/* current time */
	static int	seed = 0;	/* randomizing seed value */

	if (!seed) {
		nanotime(&now);
		seed = (int)now.tv_sec ^ (int)now.tv_nsec;
		srandom(seed);
	}
	ioflag |= IO_UIOSZ;
	uiop->uio_writeiolog = (random() & 0x3) + XFS_UIO_MIN_WRITEIO_LOG;
#endif

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	eventsent = 0;
	commit_lsn = -1;
	if (XFS_FORCED_SHUTDOWN(ip->i_mount))
		return (EIO);

	/*
	 * need to protect against deadlocks that can occur if the
	 * biomove touches a virtual address in user space that is
	 * mapped to the file being read.  This only works for
	 * read/write and pread/pwrite.  readv/writev lose.
	 * direct i/o loses too for now.
	 *
	 * note that if someone remaps the user buffer to this file
	 * while the I/O in progess, we lose too.  instant deadlock.
	 */
	rvnmaps = NULL;
	num_rvnmaps = 0;
	rvnmap_flags = 0;
	uaccmaps = NULL;

	if (uiop->uio_segflg == UIO_USERSPACE && uiop->uio_iovcnt == 1 &&
	    !(ioflag & IO_DIRECT) && VN_MAPPED(vp)) {
#pragma mips_frequency_hint NEVER
		as_lookup_current(&vasid);
		VAS_LOCK(vasid, AS_SHARED);

		vnmap_args.as_vp = vp;
		vnmap_args.as_vaddr = (uvaddr_t)
					ctob(btoct(uiop->uio_iov->iov_base));
		vnmap_args.as_len = uiop->uio_iov->iov_len +
				((__psunsigned_t) uiop->uio_iov->iov_base -
				 (__psunsigned_t) vnmap_args.as_vaddr);
		vnmap_args.as_offstart = uiop->uio_offset;
		vnmap_args.as_offend = uiop->uio_offset + uiop->uio_resid;
		vnmap_args.as_vnmap = vnmaps;
		vnmap_args.as_nmaps = XFS_NUMVNMAPS;

		if (error = VAS_VERIFYVNMAP(vasid, &vnmap_args, &vnmap_res)) {
			VAS_UNLOCK(vasid);
			return error;
		}

		VAS_UNLOCK(vasid);

		if (vnmap_res.as_rescodes) {
			rvnmaps = (vnmap_res.as_multimaps == NULL)
					? vnmaps 
					: vnmap_res.as_multimaps;
			rvnmap_flags = vnmap_res.as_rescodes;
			num_rvnmaps = vnmap_res.as_nmaps;
			rvnmap_size = vnmap_res.as_mapsize;

			if (num_rvnmaps <= XFS_NUMVNMAPS)
				uaccmaps = uaccmap_array;
			else if ((uaccmaps = kmem_alloc(num_rvnmaps *
						sizeof(xfs_uaccmap_t),
						KM_SLEEP)) == NULL) {
				error = XFS_ERROR(ENOMEM);
				goto out;
			}
		}
	}

	/*
	 * check if we're in recursive lock mode (a read inside a biomove
	 * to a page that is mapped to ip that faulted)
	 */
	lflag = xfs_is_nested_locking_enabled()
		 ? XFS_IOLOCK_NESTED
		 : 0;

	if (!(ioflag & IO_ISLOCKED))
		xfs_rwlockf(bdp, (ioflag & IO_DIRECT) ?
			   VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE,
			   lflag);

	/*
	 * if the write will grow the file beyond the current
	 * eof, grow the file now by faulting in the page with
	 * the largest file offset.  That will zero-fill all
	 * pages in the buffer containing eof and will enable
	 * faults between the current eof and the new eof to
	 * read in zero'ed pages.
	 */
	if (rvnmap_flags & AS_VNMAP_AUTOGROW) {
#pragma mips_frequency_hint NEVER
		xfs_ilock(ip, XFS_ILOCK_SHARED);
		if (vnmap_res.as_maxoffset <= ip->i_d.di_size) {
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
		} else {
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			map = rvnmaps;
			for (i = 0; i < num_rvnmaps; i++) {
				if (!(map->vnmap_flags & AS_VNMAP_AUTOGROW))
					continue;
				xfs_enable_nested_locking();
				error = fubyte((char *) map->vnmap_ovvaddr +
						map->vnmap_ovlen - 1);
				xfs_disable_nested_locking();
				if (error == -1) {
					error = XFS_ERROR(EFAULT);
					goto out;
				}
			}
		}
	}

	type = ip->i_d.di_mode & IFMT;
	ASSERT(!(vp->v_vfsp->vfs_flag & VFS_RDONLY));
	ASSERT(type == IFDIR ||
	       ismrlocked(&ip->i_iolock, MR_UPDATE) ||
	       (ismrlocked(&ip->i_iolock, MR_ACCESS) &&
		(ioflag & IO_DIRECT)));

	ASSERT(type == IFREG || type == IFDIR ||
	       type == IFLNK || type == IFSOCK);

start:
	if (ioflag & IO_APPEND) {
		/*
		 * In append mode, start at the end of the file.
		 * Since I've got the iolock exclusive I can look
		 * at di_size.
		 */
		uiop->uio_offset = savedsize = ip->i_d.di_size;
	}

	offset = uiop->uio_offset;
	count = uiop->uio_resid;

	/* check for locks if some exist and mandatory locking is enabled */
	if ((vp->v_flag & (VENF_LOCKING|VFRLOCKS)) == 
	    (VENF_LOCKING|VFRLOCKS)) {
		error = fs_checklock(vp, FWRITE, offset, count, 
				     uiop->uio_fmode, credp, fl, 
				     VRWLOCK_WRITE);	
		if (error)
			goto out;
	}

	if (offset < 0) {
		error = XFS_ERROR(EINVAL);
		goto out;
	}
	if ((ssize_t)count <= 0) {
		error = (ssize_t)count < 0 ? XFS_ERROR(EINVAL) : 0;
		goto out;
	}

	switch (type) {
	case IFREG:
		limit = ((uiop->uio_limit < XFS_MAX_FILE_OFFSET) ?
			 uiop->uio_limit : XFS_MAX_FILE_OFFSET);
		n = limit - uiop->uio_offset;
		if (n <= 0) {
			error = XFS_ERROR(EFBIG);
			goto out;
		}
		if (n < uiop->uio_resid) {
			resid = uiop->uio_resid - n;
			uiop->uio_resid = n;
		} else {
			resid = 0;
		}

		if (DM_EVENT_ENABLED(vp->v_vfsp, ip, DM_EVENT_WRITE) &&
		    !(ioflag & IO_INVIS) && !eventsent) {
			vrwlock_t	locktype;

			locktype = (ioflag & IO_DIRECT) ?
				VRWLOCK_WRITE_DIRECT:VRWLOCK_WRITE;

			error = xfs_dm_send_data_event(DM_EVENT_WRITE, bdp,
					offset, count,
					UIO_DELAY_FLAG(uiop), &locktype);
			if (error)
				goto out;
			eventsent = 1;
		}
		/*
		 *  The iolock was dropped and reaquired in
		 *  xfs_dm_send_data_event so we have to recheck the size
		 *  when appending.  We will only "goto start;" once,
		 *  since having sent the event prevents another call
		 *  to xfs_dm_send_data_event, which is what
		 *  allows the size to change in the first place.
		 */
		if ((ioflag & IO_APPEND) && savedsize != ip->i_d.di_size)
			goto start;
		/*
		 * implement osync == dsync option
		 */
		if (ioflag & IO_SYNC && mp->m_flags & XFS_MOUNT_OSYNCISDSYNC) {
			ioflag &= ~IO_SYNC;
			ioflag |= IO_DSYNC;
		}

		/*
		 * If we're writing the file then make sure to clear the
		 * setuid and setgid bits if the process is not being run
		 * by root.  This keeps people from modifying setuid and
		 * setgid binaries.  Don't allow this to happen if this
		 * file is a swap file (I know, weird).
		 */
		if (((ip->i_d.di_mode & ISUID) ||
		    ((ip->i_d.di_mode & (ISGID | (IEXEC >> 3))) ==
			(ISGID | (IEXEC >> 3)))) &&
		    !(vp->v_flag & VISSWAP) &&
		    !cap_able_cred(credp, CAP_FSETID)) {
			error = xfs_write_clear_setuid(ip);
			if (error) {
				goto out;
			}
		}

		/*
		 * Respect preferred write size if indicated in uio structure.
		 * But if the write size has already been set, go with the
		 * smallest value.  Silently ignore requests that aren't
		 * within valid I/O size limits.
		 */
		if ((ioflag & IO_UIOSZ) &&
		    uiop->uio_writeiolog != ip->i_writeio_log &&
		    uiop->uio_writeiolog >= mp->m_sb.sb_blocklog &&
		    uiop->uio_writeiolog >= XFS_UIO_MIN_WRITEIO_LOG &&
		    uiop->uio_writeiolog <= XFS_UIO_MAX_WRITEIO_LOG) {
			xfs_ilock(ip, XFS_ILOCK_EXCL);
#if !(defined(DEBUG) && defined(UIOSZ_DEBUG))
			if (!(ip->i_flags & XFS_IUIOSZ) ||
			    uiop->uio_writeiolog < ip->i_writeio_log) {
#endif /* ! (DEBUG && UIOSZ_DEBUG) */
				ip->i_writeio_log = uiop->uio_writeiolog;
				ip->i_writeio_blocks = 1 <<
					(int) (ip->i_writeio_log -
						mp->m_sb.sb_blocklog);
				/*
				 * set inode max io size to largest value
				 * that the inode could ever have had
				 */
				if (!(ip->i_flags & XFS_IUIOSZ)) {
					ip->i_max_io_log = MAX(ip->i_max_io_log,
							MAX(mp->m_writeio_log,
							    ip->i_writeio_log));
					ip->i_flags |= XFS_IUIOSZ;
				}
#if defined(DEBUG) && defined(UIOSZ_DEBUG)
				atomicAddInt(&uiodbg_switch, 1);
				atomicAddInt(
					&(uiodbg_writeiolog[ip->i_writeio_log -
						XFS_UIO_MIN_WRITEIO_LOG]),
					1);
#endif
#if !(defined(DEBUG) && defined(UIOSZ_DEBUG))
			}
#endif /* ! (DEBUG && UIOSZ_DEBUG) */
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		}

retry:
		if (ioflag & IO_DIRECT) {
			error = xfs_diordwr(bdp, uiop, ioflag, credp, B_WRITE);
		} else {
			error = xfs_write_file(bdp, uiop, ioflag, credp,
						&commit_lsn,
						rvnmaps, num_rvnmaps,
						rvnmap_flags, uaccmaps);
		}

		if (error == ENOSPC &&
		    DM_EVENT_ENABLED(vp->v_vfsp, ip, DM_EVENT_NOSPACE) &&
		    !(ioflag & IO_INVIS)) {
			vrwlock_t	locktype;

			locktype = (ioflag & IO_DIRECT) ?
				VRWLOCK_WRITE_DIRECT:VRWLOCK_WRITE;

			VOP_RWUNLOCK(vp, locktype);
			error = dm_send_namesp_event(DM_EVENT_NOSPACE, bdp,
					DM_RIGHT_NULL, bdp, DM_RIGHT_NULL, NULL, NULL,
					0, 0, 0); /* Delay flag intentionally unused */
			VOP_RWLOCK(vp, locktype);
			if (error)
				goto out;

			offset = uiop->uio_offset;
			goto retry;
		} else if (error == ENOSPC) {
			if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME)  {
				xfs_error(mp, 2);
			} else {
				xfs_error(mp, 1);
			}
		}

		/*
		 * Add back whatever we refused to do because of
		 * uio_limit.
		 */
		uiop->uio_resid += resid;

		/*
		 * We've done at least a partial write, so don't
		 * return an error on this call.  Also update the
		 * timestamps since we changed the file.
		 */
		if (count != uiop->uio_resid) {
			error = 0;
			/* don't update timestamps if doing invisible I/O */
			if (!(ioflag & IO_INVIS))
				xfs_ichgtime(ip,
					XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
		}

		/*
		 * If the write was synchronous then we need to make
		 * sure that the inode modification time is permanent.
		 * We'll have update the timestamp above, so here
		 * we use a synchronous transaction to log the inode.
		 * It's not fast, but it's necessary.
		 *
		 * If this a dsync write and the size got changed
		 * non-transactionally, then we need to ensure that
		 * the size change gets logged in a synchronous
		 * transaction.  If an allocation transaction occurred
		 * without extending the size, then we have to force
		 * the log up the proper point to ensure that the
		 * allocation is permanent.  We can't count on
		 * the fact that buffered writes lock out direct I/O
		 * writes because the direct I/O write could have extended
		 * the size non-transactionally and then finished just before
		 * we started.  xfs_write_file will think that the file
		 * didn't grow but the update isn't safe unless the
		 * size change is logged.
		 *
		 * If the vnode is a swap vnode, then don't do anything
		 * which could require allocating memory.
		 */
		if ((ioflag & IO_SYNC ||
		     (ioflag & IO_DSYNC && ip->i_update_size)) &&
		    !(vp->v_flag & VISSWAP)) {
			tp = xfs_trans_alloc(mp, XFS_TRANS_WRITE_SYNC);
			if (transerror = xfs_trans_reserve(tp, 0,
						      XFS_SWRITE_LOG_RES(mp),
						      0, 0, 0)) {
				xfs_trans_cancel(tp, 0);
				error = transerror;
				break;
			}
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);
			xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
			xfs_trans_set_sync(tp);
			transerror = xfs_trans_commit(tp, 0, &commit_lsn);
			if ( transerror )
				error = transerror;
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		} else if ((ioflag & IO_DSYNC) && !(vp->v_flag & VISSWAP)) {
			/*
			 * force the log if we've committed a transaction
			 * against the inode or if someone else has and
			 * the commit record hasn't gone to disk (e.g.
			 * the inode is pinned).  This guarantees that
			 * all changes affecting the inode are permanent
			 * when we return.
			 */
			if (commit_lsn != -1)
				xfs_log_force(mp, (xfs_lsn_t)commit_lsn,
					      XFS_LOG_FORCE | XFS_LOG_SYNC );
			else if (xfs_ipincount(ip) > 0)
				xfs_log_force(mp, (xfs_lsn_t)0,
					      XFS_LOG_FORCE | XFS_LOG_SYNC );
		}
		if (ioflag & (IO_NFS|IO_NFS3)) {
			xfs_refcache_insert(ip);
		}
		break;

	case IFDIR:
		error = XFS_ERROR(EISDIR);
		break;

	case IFLNK:
		error = XFS_ERROR(EINVAL);
		break;

	case IFSOCK:
		error = XFS_ERROR(ENODEV);
		break;

	default:
		ASSERT(0);
		error = XFS_ERROR(EINVAL);
		break;
	}

out:
	if (rvnmap_size > 0)
		kmem_free(rvnmaps, rvnmap_size);

	if (num_rvnmaps > XFS_NUMVNMAPS)
		kmem_free(uaccmaps, num_rvnmaps * sizeof(xfs_uaccmap_t));

	if (!(ioflag & IO_ISLOCKED))
		xfs_rwunlockf(bdp, (ioflag & IO_DIRECT) ?
			     VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE,
			     lflag);

	return error;
}


/*
 * This is the XFS entry point for VOP_BMAP().
 * It simply switches based on the given flags
 * to either xfs_iomap_read() or xfs_iomap_write().
 * This cannot be used to grow a file or to read
 * beyond the end of the file.
 *
 * The caller is required to be holding the inode's
 * iolock in at least shared mode for a read mapping
 * and exclusively for a write mapping.
 */
/*ARGSUSED*/
int
xfs_bmap(
	bhv_desc_t	*bdp,
	off_t		offset,
	ssize_t		count,
	int		flags,
	cred_t		*credp,
	struct bmapval	*bmapp,
	int		*nbmaps)
{
	xfs_inode_t	*ip;
	int		error;
	int		unlocked;
	int		lockmode;

	ip = XFS_BHVTOI(bdp);
	ASSERT((ip->i_d.di_mode & IFMT) == IFREG);
	ASSERT((flags == B_READ) || (flags == B_WRITE));
	if (XFS_FORCED_SHUTDOWN(ip->i_mount))
		return (EIO);

	if (flags == B_READ) {
		ASSERT(ismrlocked(&ip->i_iolock, MR_ACCESS | MR_UPDATE) != 0);
		unlocked = 0;
		lockmode = xfs_ilock_map_shared(ip);
		error = xfs_iomap_read(ip, offset, count, bmapp,
				 nbmaps, NULL, &unlocked, lockmode);
		if (!unlocked)
			xfs_iunlock_map_shared(ip, lockmode);
	} else {
		ASSERT(ismrlocked(&ip->i_iolock, MR_UPDATE) != 0);
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		ASSERT(ip->i_d.di_size >= (offset + count));

		/* 
		 * Make sure that the dquots are there. This doesn't hold 
		 * the ilock across a disk read.
		 */
		if (XFS_IS_QUOTA_ON(ip->i_mount)) {
			if (XFS_NOT_DQATTACHED(ip->i_mount, ip)) {
				if (error = xfs_qm_dqattach(ip,
						    XFS_QMOPT_ILOCKED)){
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					goto error0;
				}
			}
		}

		error = xfs_iomap_write(ip, offset, count, bmapp,
					nbmaps, 0, NULL);

		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		if (!error)
			XFS_INODE_CLEAR_READ_AHEAD(ip);
	}
 error0:
	return error;
}

/*
 * Set up rbp so that it points to the memory attached to bp
 * from rbp_offset from the start of bp for rbp_len bytes.
 */
STATIC void
xfs_overlap_bp(
	buf_t	*bp,
	buf_t	*rbp,
	uint	rbp_offset,
	uint	rbp_len)
{
	int	pgbboff;
	int	bytes_off;
	pfd_t	*pfdp;

	
	if (BP_ISMAPPED(bp)) {
		/*
		 * The real buffer is already mapped, so just use
		 * its virtual memory for ourselves.
		 */
		rbp->b_un.b_addr = bp->b_un.b_addr + rbp_offset;
		rbp->b_bcount = rbp_len;
		rbp->b_bufsize = rbp_len;
	} else {
		/*
		 * The real buffer is not yet mapped to virtual memory.
		 * Just get the subordinate buffer's page pointers
		 * set up and make it a PAGEIO buffer like the real one.
		 *
		 * First find the first page of rbp.  We do this by
		 * walking the list of pages in bp until we find the
		 * one containing the start of rbp.  Note that neither
		 * bp nor rbp are required to start on page boundaries.
		 */
		bytes_off = rbp_offset + BBTOOFF(dpoff(bp->b_offset));
		pfdp = NULL;
		pfdp = getnextpg(bp, pfdp);
		ASSERT(pfdp != NULL);
		while (bytes_off >= NBPP) {
			pfdp = getnextpg(bp, pfdp);
			ASSERT(pfdp != NULL);
			bytes_off -= NBPP;
		}
		rbp->b_pages = pfdp;

		rbp->b_bcount = rbp_len;
		rbp->b_offset = bp->b_offset + BTOBB(rbp_offset);
		pgbboff = dpoff(rbp->b_offset);
		rbp->b_bufsize = ctob(dtop(pgbboff + BTOBB(rbp_len)));

		rbp->b_flags |= B_PAGEIO;

		if (pgbboff != 0) {
			bp_mapin(rbp);
		}
	}
	rbp->b_blkno = bp->b_blkno + BTOBB(rbp_offset);
	rbp->b_remain = 0;
	rbp->b_vp = bp->b_vp;
	rbp->b_edev = bp->b_edev;
	rbp->b_flags |= (bp->b_flags & (B_UNCACHED | B_ASYNC));
}


/*
 * Zero the given bp from data_offset from the start of it for data_len
 * bytes.
 */
STATIC void
xfs_zero_bp(
	buf_t	*bp,
	int	data_offset,
	int	data_len)
{
	pfd_t	*pfdp;
	caddr_t	page_addr;
	int	len;

	if (BP_ISMAPPED(bp)) {
		bzero(bp->b_un.b_addr + data_offset, data_len);
		return;
	}

	data_offset += BBTOOFF(dpoff(bp->b_offset));
	pfdp = NULL;
	pfdp = getnextpg(bp, pfdp);
	ASSERT(pfdp != NULL);
	while (data_offset >= NBPP) {
		pfdp = getnextpg(bp, pfdp);
		ASSERT(pfdp != NULL);
		data_offset -= NBPP;
	}
	ASSERT(data_offset >= 0);
	while (data_len > 0) {
		page_addr = page_mapin(pfdp, (bp->b_flags & B_UNCACHED ?
					      VM_UNCACHED : 0), 0);
		len = MIN(data_len, NBPP - data_offset);
		bzero(page_addr + data_offset, len);
		data_len -= len;
		data_offset = 0;
		page_mapout(page_addr);
		pfdp = getnextpg(bp, pfdp);
	}
}

/*
 * Verify that the gap list is properly sorted and that no entries
 * overlap.
 */
#ifdef DEBUG
STATIC void
xfs_check_gap_list(
	xfs_inode_t	*ip)
{
	xfs_gap_t	*last_gap;
	xfs_gap_t	*curr_gap;
	int		loops;

	last_gap = NULL;
	curr_gap = ip->i_gap_list;
	loops = 0;
	while (curr_gap != NULL) {
		ASSERT(curr_gap->xg_count_fsb > 0);
		if (last_gap != NULL) {
			ASSERT((last_gap->xg_offset_fsb +
				last_gap->xg_count_fsb) <
			       curr_gap->xg_offset_fsb);
		}
		last_gap = curr_gap;
		curr_gap = curr_gap->xg_next;
		ASSERT(loops++ < 1000);
	}
}
#endif

/*
 * For the given inode, offset, and count of bytes, build a list
 * of xfs_gap_t structures in the inode's gap list describing the
 * holes in the file in the range described by the offset and count.
 *
 * The list must be empty when we start, and the inode lock must
 * be held exclusively.
 */
STATIC int				/* error */
xfs_build_gap_list(
	xfs_inode_t	*ip,
	off_t		offset,
	size_t		count)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_filblks_t	count_fsb;
	xfs_fsblock_t	firstblock;
	xfs_gap_t	*new_gap;
	xfs_gap_t	*last_gap;
	xfs_mount_t	*mp;
	int		i;
	int		error;
	int		nimaps;
#define	XFS_BGL_NIMAPS	8
	xfs_bmbt_irec_t	imaps[XFS_BGL_NIMAPS];
	xfs_bmbt_irec_t	*imapp;

	ASSERT(ismrlocked(&(ip->i_lock), MR_UPDATE) != 0);
	ASSERT(ip->i_gap_list == NULL);

	mp = ip->i_mount;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	count_fsb = (xfs_filblks_t)(last_fsb - offset_fsb);
	ASSERT(count_fsb > 0);

	last_gap = NULL;
	while (count_fsb > 0) {
		nimaps = XFS_BGL_NIMAPS;
		firstblock = NULLFSBLOCK;
		error = xfs_bmapi(NULL, ip, offset_fsb, count_fsb,
				  0, &firstblock, 0, imaps, &nimaps, NULL);
		if (error) {
			return error;
		}
		ASSERT(nimaps != 0);

		/*
		 * Look for the holes in the mappings returned by bmapi.
		 * Decrement count_fsb and increment offset_fsb as we go.
		 */
		for (i = 0; i < nimaps; i++) {
			imapp = &imaps[i];
			count_fsb -= imapp->br_blockcount;
			ASSERT(((long)count_fsb) >= 0);
			ASSERT(offset_fsb == imapp->br_startoff);
			offset_fsb += imapp->br_blockcount;
			ASSERT(offset_fsb <= last_fsb);
			ASSERT((offset_fsb < last_fsb) || (count_fsb == 0));

			/*
			 * Skip anything that is not a hole or
			 * unwritten.
			 */
			if (imapp->br_startblock != HOLESTARTBLOCK ||
			    imapp->br_state == XFS_EXT_UNWRITTEN) {
				continue;
			}

			/*
			 * We found a hole.  Now add an entry to the inode's
			 * gap list corresponding to it.  The list is
			 * a singly linked, NULL terminated list.  We add
			 * each entry to the end of the list so that it is
			 * sorted by file offset.
			 */
			new_gap = kmem_zone_alloc(xfs_gap_zone, KM_SLEEP);
			new_gap->xg_offset_fsb = imapp->br_startoff;
			new_gap->xg_count_fsb = imapp->br_blockcount;
			new_gap->xg_next = NULL;

			if (last_gap == NULL) {
				ip->i_gap_list = new_gap;
			} else {
				last_gap->xg_next = new_gap;
			}
			last_gap = new_gap;
		}
	}
	xfs_check_gap_list(ip);
	return 0;
}

/*
 * Remove or trim any entries in the inode's gap list which overlap
 * the given range.  I'm going to assume for now that we never give
 * a range which is actually in the middle of an entry (i.e. we'd need
 * to split it in two).  This is a valid assumption for now given the
 * use of this in xfs_write_file() where we start at the front and
 * move sequentially forward.
 */
STATIC void
xfs_delete_gap_list(
	xfs_inode_t	*ip,
	xfs_fileoff_t	offset_fsb,
	xfs_extlen_t	count_fsb)
{
	xfs_gap_t	*curr_gap;
	xfs_gap_t	*last_gap;
	xfs_gap_t	*next_gap;
	xfs_fileoff_t	gap_offset_fsb;
	xfs_extlen_t	gap_count_fsb;
	xfs_fileoff_t	gap_end_fsb;
	xfs_fileoff_t	end_fsb;

	last_gap = NULL;
	curr_gap = ip->i_gap_list;
	while (curr_gap != NULL) {
		gap_offset_fsb = curr_gap->xg_offset_fsb;
		gap_count_fsb = curr_gap->xg_count_fsb;

		/*
		 * The entries are sorted by offset, so if we see
		 * one beyond our range we're done.
		 */
		end_fsb = offset_fsb + count_fsb;
		if (gap_offset_fsb >= end_fsb) {
			return;
		}

		gap_end_fsb = gap_offset_fsb + gap_count_fsb;
		if (gap_end_fsb <= offset_fsb) {
			/*
			 * This shouldn't be able to happen for now.
			 */
			ASSERT(0);
			last_gap = curr_gap;
			curr_gap = curr_gap->xg_next;
			continue;
		}

		/*
		 * We've go an overlap.  If the gap is entirely contained
		 * in the region then remove it.  If not, then shrink it
		 * by the amount overlapped.
		 */
		if (gap_end_fsb > end_fsb) {
			/*
			 * The region does not extend to the end of the gap.
			 * Shorten the gap by the amount in the region,
			 * and then we're done since we've reached the
			 * end of the region.
			 */
			ASSERT(gap_offset_fsb >= offset_fsb);
			curr_gap->xg_offset_fsb = end_fsb;
			curr_gap->xg_count_fsb = gap_end_fsb - end_fsb;
			return;
		}

		next_gap = curr_gap->xg_next;
		if (last_gap == NULL) {
			ip->i_gap_list = next_gap;
		} else {
			ASSERT(0);
			ASSERT(last_gap->xg_next == curr_gap);
			last_gap->xg_next = next_gap;
		}
		kmem_zone_free(xfs_gap_zone, curr_gap);
		curr_gap = next_gap;
	}
}		    
/*
 * Free up all of the entries in the inode's gap list.  This requires
 * the inode lock to be held exclusively.
 */
STATIC void
xfs_free_gap_list(
	xfs_inode_t	*ip)
{
	xfs_gap_t	*curr_gap;
	xfs_gap_t	*next_gap;

	ASSERT(ismrlocked(&(ip->i_lock), MR_UPDATE) != 0);
	xfs_check_gap_list(ip);

	curr_gap = ip->i_gap_list;
	while (curr_gap != NULL) {
		next_gap = curr_gap->xg_next;
		kmem_zone_free(xfs_gap_zone, curr_gap);
		curr_gap = next_gap;
	}
	ip->i_gap_list = NULL;
}

/*
 * Zero the parts of the buffer which overlap gaps in the inode's gap list.
 * Deal with everything in BBs since the buffer is not guaranteed to be block
 * aligned.
 */
STATIC void
xfs_cmp_gap_list_and_zero(
	xfs_inode_t	*ip,
	buf_t		*bp)
{
	off_t		bp_offset_bb;
	int		bp_len_bb;
	off_t		gap_offset_bb;
	int		gap_len_bb;
	int		zero_offset_bb;
	int		zero_len_bb;
	xfs_gap_t	*curr_gap;
	xfs_mount_t	*mp;

	ASSERT(ismrlocked(&(ip->i_lock), MR_UPDATE | MR_ACCESS) != 0);
	xfs_check_gap_list(ip);

	bp_offset_bb = bp->b_offset;
	bp_len_bb = BTOBB(bp->b_bcount);
	mp = ip->i_mount;
	curr_gap = ip->i_gap_list;
	while (curr_gap != NULL) {
		gap_offset_bb = XFS_FSB_TO_BB(mp, curr_gap->xg_offset_fsb);
		gap_len_bb = XFS_FSB_TO_BB(mp, curr_gap->xg_count_fsb);

		/*
		 * Check to see if this gap is before the buffer starts.
		 */
		if ((gap_offset_bb + gap_len_bb) <= bp_offset_bb) {
			curr_gap = curr_gap->xg_next;
			continue;
		}

		/*
		 * Check to see if this gap is after th buffer ends.
		 * If it is then we're done since the list is sorted
		 * by gap offset.
		 */
		if (gap_offset_bb >= (bp_offset_bb + bp_len_bb)) {
			break;
		}

		/*
		 * We found a gap which overlaps the buffer.  Zero
		 * the portion of the buffer overlapping the gap.
		 */
		if (gap_offset_bb < bp_offset_bb) {
			/*
			 * The gap starts before the buffer, so we start
			 * zeroing from the start of the buffer.
			 */
			zero_offset_bb = 0;
			/*
			 * To calculate the amount of overlap.  First
			 * subtract the portion of the gap which is before
			 * the buffer.  If the length is still longer than
			 * the buffer, then just zero the entire buffer.
			 */
			zero_len_bb = gap_len_bb -
				      (bp_offset_bb - gap_offset_bb);
			if (zero_len_bb > bp_len_bb) {
				zero_len_bb = bp_len_bb;
			}
			ASSERT(zero_len_bb > 0);
		} else {
			/*
			 * The gap starts at the beginning or in the middle
			 * of the buffer.  The offset into the buffer is
			 * the difference between the two offsets.
			 */
			zero_offset_bb = gap_offset_bb - bp_offset_bb;
			/*
			 * Figure out the length of the overlap.  If the
			 * gap extends beyond the end of the buffer, then
			 * just zero to the end of the buffer.  Otherwise
			 * just zero the length of the gap.
			 */
			if ((gap_offset_bb + gap_len_bb) >
			    (bp_offset_bb + bp_len_bb)) {
				zero_len_bb = bp_len_bb - zero_offset_bb;
			} else {
				zero_len_bb = gap_len_bb;
			}
		}

		/*
		 * Now that we've calculated the range of the buffer to
		 * zero, do it.
		 */
		xfs_zero_bp(bp, BBTOB(zero_offset_bb), BBTOB(zero_len_bb));

		curr_gap = curr_gap->xg_next;
	}
}


/*
 * "Read" in a buffer whose b_blkno is -1 or uninitialized.
 * If b_blkno is -1, this means that at the time the buffer was created
 * there was no underlying backing store for the range of the file covered
 * by the bp. An uninitialized buffer (with B_UNINITIAL set) indicates
 * that there is allocated storage, but all or some portions of the
 * underlying blocks have never been written.
 *
 * To figure out the current state of things, we lock the inode
 * and call xfs_bmapi() to look at the current extents format.
 * If we're over a hole, delayed allocation or uninitialized space we
 * simply zero the corresponding portions of the buffer.  For parts
 * over real disk space we need to read in the stuff from disk.
 *
 * We know that we can just use xfs_ilock(SHARED) rather than
 * xfs_ilock_map_shared() here, because the extents had to be
 * read in in order to create the buffer we're trying to write out.
 */
STATIC int
xfs_strat_read(
	bhv_desc_t	*bdp,
	buf_t		*bp)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t   map_start_fsb;
	xfs_fileoff_t	imap_offset;
	xfs_fsblock_t	last_bp_bb;
	xfs_fsblock_t	last_map_bb;
	xfs_fsblock_t	firstblock;
	xfs_filblks_t	count_fsb;
	xfs_extlen_t	imap_blocks;
	xfs_fsize_t	isize;
	off_t		offset;
	off_t		end_offset;
	off_t		init_limit;
	int		x;
	caddr_t		datap;
	buf_t		*rbp;
	xfs_mount_t	*mp;
	xfs_inode_t	*ip;
	int		count;
	int		block_off;
	int		data_bytes;
	int		data_offset;
	int		nimaps;
	int		error;
#define	XFS_STRAT_READ_IMAPS	XFS_BMAP_MAX_NMAP
	xfs_bmbt_irec_t	imap[XFS_STRAT_READ_IMAPS];
	
	ASSERT((bp->b_blkno == -1) || (bp->b_flags & B_UNINITIAL));
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;
	offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
	/*
	 * Only read up to the EOF or the current write offset.
	 * The idea here is to avoid initializing pages which are
	 * going to be immediately overwritten in xfs_write_file().
	 * The most important case is the sequential write case, where
	 * the new pages at the end of the file are sent here by
	 * chunk_patch().  We don't want to zero them since they
	 * are about to be overwritten.
	 *
	 * The ip->i_write_off tells us the offset of any write in
	 * progress.  If it is 0 then we assume that no write is
	 * in progress.  If the write offset is within the file size,
	 * the the file size is the upper limit.  If the write offset
	 * is beyond the file size, then we only want to initialize the
	 * buffer up to the write offset.  Beyond that will either be
	 * overwritten or be beyond the new EOF.
	 */
	isize = ip->i_d.di_size;
	offset = BBTOOFF(bp->b_offset);
	end_offset = offset + bp->b_bcount;

	if (ip->i_write_offset == 0) {
		init_limit = isize;
	} else if (ip->i_write_offset <= isize) {
		init_limit = isize;
	} else {
		init_limit = ip->i_write_offset;
	}

	if (end_offset <= init_limit) {
		count = bp->b_bcount;
	} else {
		count = init_limit - offset;
	}

	if (count <= 0) {
		iodone(bp);
		return 0;
	}

	/*
	 * Since the buffer may not be file system block aligned, we
	 * can't do a simple shift to find the number of blocks underlying
	 * it.  Instead we subtract the last block it sits on from the first.
	 */
	count_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count))) -
		    XFS_B_TO_FSBT(mp, offset);
	map_start_fsb = offset_fsb;
	xfs_ilock(ip, XFS_ILOCK_SHARED);
	while (count_fsb != 0) {
		nimaps = XFS_STRAT_READ_IMAPS;
		firstblock = NULLFSBLOCK;
		error = xfs_bmapi(NULL, ip, map_start_fsb, count_fsb, 0,
				  &firstblock, 0, imap, &nimaps, NULL);
		if (error) {
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			xfs_bioerror_relse(bp);
			return error;
		}
		ASSERT(nimaps >= 1);
		
		for (x = 0; x < nimaps; x++) {
			imap_offset = imap[x].br_startoff;
			ASSERT(imap_offset == map_start_fsb);
			imap_blocks = imap[x].br_blockcount;
			ASSERT(imap_blocks <= count_fsb);
			/*
			 * Calculate the offset of this mapping in the
			 * buffer and the number of bytes of this mapping
			 * that are in the buffer.  If the block size is
			 * greater than the page size, then the buffer may
			 * not line up on file system block boundaries
			 * (e.g. pages being read in from chunk_patch()).
			 * In that case we need to account for the space
			 * in the file system blocks underlying the buffer
			 * that is not actually a part of the buffer.  This
			 * space is the space in the first block before the
			 * start of the buffer and the space in the last
			 * block after the end of the buffer.
			 */
			data_offset = XFS_FSB_TO_B(mp,
						   imap_offset - offset_fsb);
			data_bytes = XFS_FSB_TO_B(mp, imap_blocks);
			block_off = 0;

			if (mp->m_sb.sb_blocksize > NBPP) {
				/*
				 * If the buffer is actually fsb
				 * aligned then this will simply
				 * subtract 0 and do no harm.  If the
				 * current mapping is for the start of
				 * the buffer, then data offset will be
				 * zero so we don't need to subtract out
				 * any space at the beginning.
				 */
				if (data_offset > 0) {
					data_offset -= BBTOB(
							XFS_BB_FSB_OFFSET(mp,
							      bp->b_offset));
				}

				if (map_start_fsb == offset_fsb) {
					ASSERT(data_offset == 0);
					/*
					 * This is on the first block
					 * mapped, so it must be the start
					 * of the buffer.  Subtract out from
					 * the number of bytes the bytes
					 * between the start of the block
					 * and the start of the buffer.
					 */
					data_bytes -=
						BBTOB(XFS_BB_FSB_OFFSET(
							mp, bp->b_offset));

					/*
					 * Set block_off to the number of
					 * BBs that the buffer is offset
					 * from the start of this mapping.
					 */
					block_off = XFS_BB_FSB_OFFSET(mp,
							    bp->b_offset);
					ASSERT(block_off >= 0);
				}

				if (imap_blocks == count_fsb) {
					/*
					 * This mapping includes the last
					 * block to be mapped.  Subtract out
					 * from the number of bytes the bytes
					 * between the end of the buffer and
					 * the end of the block.  It may
					 * be the case that the buffer
					 * extends beyond the mapping (if
					 * it is beyond the end of the file),
					 * in which case no adjustment
					 * is necessary.
					 */
					last_bp_bb = bp->b_offset +
						BTOBB(bp->b_bcount);
					last_map_bb =
						XFS_FSB_TO_BB(mp,
							      (imap_offset +
							       imap_blocks));

					if (last_map_bb > last_bp_bb) {
						data_bytes -=
							BBTOB(last_map_bb -
							      last_bp_bb);
					}

				}
			}
			ASSERT(data_bytes > 0);
			ASSERT(data_offset >= 0);
			if ((imap[x].br_startblock == DELAYSTARTBLOCK) ||
			    (imap[x].br_startblock == HOLESTARTBLOCK) ||
			    (imap[x].br_state == XFS_EXT_UNWRITTEN)) {
				/*
				 * This is either a hole,a delayed alloc
				 * extent or uninitialized allocated space.
				 * Either way, just fill it with zeroes.
				 */
				datap = bp_mapin(bp);
				datap += data_offset;
				bzero(datap, data_bytes);
				if (!dpoff(bp->b_offset)) {
					bp_mapout(bp);
				}

			} else {
				/*
				 * The extent really exists on disk, so
				 * read it in.
				 */
				rbp = getrbuf(KM_SLEEP);
				xfs_overlap_bp(bp, rbp, data_offset,
					       data_bytes);
				rbp->b_blkno = XFS_FSB_TO_DB(ip,
						   imap[x].br_startblock) +
					       block_off;
				rbp->b_offset = XFS_FSB_TO_BB(mp,
							      imap_offset) +
						block_off;
				rbp->b_flags |= B_READ;
				rbp->b_flags &= ~B_ASYNC;
				rbp->b_target = bp->b_target;

				xfs_check_rbp(ip, bp, rbp, 1);
				(void) xfsbdstrat(mp, rbp);
				error = iowait(rbp);
				if (error) {
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
					ASSERT(bp->b_error != EINVAL);
				}

				/*
				 * Check to see if the block extent (or parts
				 * of it) have not yet been initialized and
				 * should therefore be zeroed.
				 */
				xfs_cmp_gap_list_and_zero(ip, rbp);

				if (BP_ISMAPPED(rbp)) {
					bp_mapout(rbp);
				}

				freerbuf(rbp);
			}
			count_fsb -= imap_blocks;
			map_start_fsb += imap_blocks;
		}
	}
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	iodone(bp);
	return error;
}


#if defined(XFS_STRAT_TRACE)

void
xfs_strat_write_bp_trace(
	int		tag,
	xfs_inode_t	*ip,
	buf_t		*bp)
{
	if (ip->i_strat_trace == NULL) {
		return;
	}

	ktrace_enter(ip->i_strat_trace,
		     (void*)((__psunsigned_t)tag),
		     (void*)ip,
		     (void*)((__psunsigned_t)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)((__psunsigned_t)((bp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(bp->b_offset & 0xffffffff),
		     (void*)((__psunsigned_t)(bp->b_bcount)),
		     (void*)((__psunsigned_t)(bp->b_bufsize)),
		     (void*)(bp->b_blkno),
		     (void*)(__psunsigned_t)((bp->b_flags >> 32) & 0xffffffff),
		     (void*)(bp->b_flags & 0xffffffff),
		     (void*)(bp->b_pages),
		     (void*)(bp->b_pages->pf_pageno),
		     (void*)0,
		     (void*)0);

	ktrace_enter(xfs_strat_trace_buf,
		     (void*)((__psunsigned_t)tag),
		     (void*)ip,
		     (void*)((__psunsigned_t)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)((__psunsigned_t)((bp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(bp->b_offset & 0xffffffff),
		     (void*)((__psunsigned_t)(bp->b_bcount)),
		     (void*)((__psunsigned_t)(bp->b_bufsize)),
		     (void*)(bp->b_blkno),
		     (void*)(__psunsigned_t)((bp->b_flags >> 32) & 0xffffffff),
		     (void*)(bp->b_flags & 0xffffffff),
		     (void*)(bp->b_pages),
		     (void*)(bp->b_pages->pf_pageno),
		     (void*)0,
		     (void*)0);
}


void
xfs_strat_write_subbp_trace(
	int		tag,
	xfs_inode_t	*ip,
	buf_t		*bp,
	buf_t		*rbp,
	off_t		last_off,
	int		last_bcount,
	daddr_t		last_blkno)			    
{
	if (ip->i_strat_trace == NULL) {
		return;
	}

	ktrace_enter(ip->i_strat_trace,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((unsigned long)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)rbp,
		     (void*)((unsigned long)((rbp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(rbp->b_offset & 0xffffffff),
		     (void*)((unsigned long)(rbp->b_bcount)),
		     (void*)(rbp->b_blkno),
		     (void*)((__psunsigned_t)(rbp->b_flags)), /* lower 32 flags only */
		     (void*)(rbp->b_un.b_addr),
		     (void*)(bp->b_pages),
		     (void*)(last_off),
		     (void*)((unsigned long)(last_bcount)),
		     (void*)(last_blkno));

	ktrace_enter(xfs_strat_trace_buf,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((unsigned long)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)rbp,
		     (void*)((unsigned long)((rbp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(rbp->b_offset & 0xffffffff),
		     (void*)((unsigned long)(rbp->b_bcount)),
		     (void*)(rbp->b_blkno),
		     (void*)((__psunsigned_t)(rbp->b_flags)), /* lower 32 flags only */
		     (void*)(rbp->b_un.b_addr),
		     (void*)(bp->b_pages),
		     (void*)(last_off),
		     (void*)((unsigned long)(last_bcount)),
		     (void*)(last_blkno));
}
#endif /* XFS_STRAT_TRACE */

#ifdef DEBUG
/*
 * xfs_strat_write_check
 *
 * Make sure that there are blocks or delayed allocation blocks
 * underlying the entire area given.  The imap parameter is simply
 * given as a scratch area in order to reduce stack space.  No
 * values are returned within it.
 */
STATIC void
xfs_strat_write_check(
	xfs_inode_t	*ip,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	buf_fsb,
	xfs_bmbt_irec_t	*imap,
	int		imap_count)
{
	xfs_filblks_t	count_fsb;
	xfs_fsblock_t	firstblock;
	int		nimaps;
	int		n;
	int		error;

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	count_fsb = 0;
	while (count_fsb < buf_fsb) {
		nimaps = imap_count;
		firstblock = NULLFSBLOCK;
		error = xfs_bmapi(NULL, ip, (offset_fsb + count_fsb),
				  (buf_fsb - count_fsb), 0, &firstblock, 0,
				  imap, &nimaps, NULL);
		if (error) {
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			return;
		}
		ASSERT(nimaps > 0);
		n = 0;
		while (n < nimaps) {
			ASSERT(imap[n].br_startblock != HOLESTARTBLOCK);
			count_fsb += imap[n].br_blockcount;
			ASSERT(count_fsb <= buf_fsb);
			n++;
		}
	}
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
		
	return;
}
#endif /* DEBUG */

/*
 * xfs_strat_write_iodone -
 *	I/O completion for the first write of unwritten buffered data.
 *	Since this occurs in an interrupt thread, massage some bp info
 *	and queue to the xfs_strat daemon.
 */
STATIC void
xfs_strat_write_iodone(
	buf_t	*bp)
{
	bhv_desc_t 	*bdp;
	/* REFERENCED */
	xfs_inode_t	*ip;
	int		s;

	ASSERT(bp->b_flags & B_UNINITIAL);
	ASSERT(bp->b_vp);
	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(bp->b_vp), &xfs_vnodeops);
	ASSERT(bdp);
	ASSERT(xfsc_count > 0);
	ip = XFS_BHVTOI(bdp);
	/*
	 * Delay I/O done work until the transaction is completed.
	 */
	bp->b_iodone = NULL;
	ASSERT(bp->b_flags & B_BUSY);
	ASSERT(valusema(&bp->b_lock) <= 0);
	ASSERT(!(bp->b_flags & B_DONE));

	/*
	 * Queue to the xfsc_list.
	 */
	s = mp_mutex_spinlock(&xfsc_lock);
	/*
	 * Queue the buffer at the end of the list.
	 * Bump the inode count of the number of queued buffers.
	 */
	if (xfsc_list == NULL) {
		bp->av_forw = bp;
		bp->av_back = bp;
		xfsc_list = bp;
	} else {
		bp->av_back = xfsc_list->av_back;
		xfsc_list->av_back->av_forw = bp;
		xfsc_list->av_back = bp;
		bp->av_forw = xfsc_list;
	}
	buftrace("STRAT_WRITE_IODONE", bp);
	xfsc_bufcount++;
	xfs_strat_write_bp_trace(XFS_STRAT_ENTER, ip, bp);
	(void)sv_signal(&xfsc_wait);
	mp_mutex_spinunlock(&xfsc_lock, s);
	return;
}


STATIC void
xfs_strat_comp(void)
{
	bhv_desc_t 	*bdp;
	xfs_inode_t	*ip;
	xfs_trans_t	*tp;
	xfs_mount_t	*mp;
	buf_t		*bp;
	buf_t		*forw;
	buf_t		*back;
	xfs_fileoff_t	offset_fsb;
	xfs_filblks_t	count_fsb;
	xfs_filblks_t	numblks_fsb;
	xfs_bmbt_irec_t	imap;
	int		nimaps;
	int		nres;
	int		s;
	int		error;
	int		committed;
	xfs_fsblock_t	firstfsb;
	xfs_bmap_free_t	free_list;

	s = mp_mutex_spinlock(&xfsc_lock);
	xfsc_count++;

	while (1) {
		while (xfsc_list == NULL) {
			mp_sv_wait(&xfsc_wait, PRIBIO, &xfsc_lock, s);
			s = mp_mutex_spinlock(&xfsc_lock);
		}

		/*
		 * Pull a buffer off of the list.
		 */
		bp = xfsc_list;
		ASSERT(bp);
		forw = bp->av_forw;
		back = bp->av_back;
		forw->av_back = back;
		back->av_forw = forw;
		if (forw == bp) {
			xfsc_list = NULL;
		} else {
			xfsc_list = forw;
		}
		xfsc_bufcount--;;
		ASSERT(xfsc_bufcount >= 0);

		mp_mutex_spinunlock(&xfsc_lock, s);
		bp->av_forw = bp;
		bp->av_back = bp;
		ASSERT((bp->b_flags & B_UNINITIAL) == B_UNINITIAL);
		ASSERT(bp->b_vp);
		buftrace("STRAT_WRITE_CMPL", bp);
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(bp->b_vp),
			&xfs_vnodeops);
		ASSERT(bdp);
		ip = XFS_BHVTOI(bdp);
		mp = ip->i_mount;
		offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
		count_fsb = XFS_B_TO_FSB(mp, bp->b_bcount);
		do {
			nres = XFS_DIOSTRAT_SPACE_RES(mp, 0);
			xfs_strat_write_bp_trace(XFS_STRAT_UNINT_CMPL, ip, bp);
			/*
			 * Set up a transaction with which to allocate the
			 * backing store for the file.  Do allocations in a
			 * loop until we get some space in the range we are
			 * interested in.  The other space that might be
			 * allocated is in the delayed allocation extent
			 * on which we sit but before our buffer starts.
			 */
			tp = xfs_trans_alloc(mp, XFS_TRANS_STRAT_WRITE);
			error = xfs_trans_reserve(tp, nres,
					XFS_WRITE_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_WRITE_LOG_COUNT);
			if (error) {
				xfs_trans_cancel(tp, 0);
				goto error0;
			}

			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);
			xfs_strat_write_bp_trace(XFS_STRAT_ENTER, ip, bp);

			/*
			 * Modify the unwritten extent state of the buffer.
			 */
			XFS_BMAP_INIT(&free_list, &firstfsb);
			nimaps = 1;
			error = xfs_bmapi(tp, ip, offset_fsb, count_fsb,
					  XFS_BMAPI_WRITE, &firstfsb,
					  nres, &imap, &nimaps, &free_list);
			if (error)
				goto error_on_bmapi_transaction;

			error = xfs_bmap_finish(&(tp), &(free_list),
					firstfsb, &committed);
			if (error)
				goto error_on_bmapi_transaction;

			error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES,
							NULL);
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			if (error)
				goto error0;

			if ((numblks_fsb = imap.br_blockcount) == 0) {
				/*
				 * The extent size should alway get bigger
				 * otherwise the loop is stuck.
				 */
				ASSERT(imap.br_blockcount);
				break;
			}
			offset_fsb += numblks_fsb;
			count_fsb -= numblks_fsb;
		} while (count_fsb > 0);

		bp->b_flags &= ~(B_UNINITIAL);
		xfs_strat_write_bp_trace(XFS_STRAT_UNINT_CMPL, ip, bp);
		buftrace("STRAT_WRITE_CMPL", bp);
		/* fall through on normal completion */

error0:
		if (error) {
			bp->b_flags |= B_ERROR;
			bp->b_error = error;
		}
		biodone(bp);
		error = 0;
		s = mp_mutex_spinlock(&xfsc_lock);
	}

 error_on_bmapi_transaction:
	xfs_bmap_cancel(&free_list);
	xfs_trans_cancel(tp, (XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT));
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	goto error0;
}

/*
 * This is the completion routine for the heap-allocated buffers
 * used to write out a buffer which becomes fragmented during
 * xfs_strat_write().  It must coordinate with xfs_strat_write()
 * to properly mark the lead buffer as done when necessary and
 * to free the subordinate buffer.
 */
STATIC void
xfs_strat_write_relse(
	buf_t	*rbp)
{
	int	s;
	buf_t	*leader;
	buf_t	*forw;
	buf_t	*back;
	

	s = mutex_spinlock(&xfs_strat_lock);
	ASSERT(rbp->b_flags & B_DONE);

	forw = (buf_t*)rbp->b_fsprivate2;
	back = (buf_t*)rbp->b_fsprivate;
	ASSERT(back != NULL);
	ASSERT(((buf_t *)back->b_fsprivate2) == rbp);
	ASSERT((forw == NULL) || (((buf_t *)forw->b_fsprivate) == rbp));

	/*
	 * Pull ourselves from the list.
	 */
	back->b_fsprivate2 = forw;
	if (forw != NULL) {
		forw->b_fsprivate = back;
	}

	if ((forw == NULL) &&
	    (back->b_flags & B_LEADER) &&
	    !(back->b_flags & B_PARTIAL)) {
		/*
		 * We are the only buffer in the list and the lead buffer
		 * has cleared the B_PARTIAL bit to indicate that all
		 * subordinate buffers have been issued.  That means it
		 * is time to finish off the lead buffer.
		 */
		leader = back;
		if (rbp->b_flags & B_ERROR) {
			leader->b_flags |= B_ERROR;
			leader->b_error = XFS_ERROR(rbp->b_error);
			ASSERT(leader->b_error != EINVAL);
		}
		leader->b_flags &= ~B_LEADER;
		mutex_spinunlock(&xfs_strat_lock, s);

		iodone(leader);
	} else {
		/*
		 * Either there are still other buffers in the list or
		 * not all of the subordinate buffers have yet been issued.
		 * In this case just pass any errors on to the lead buffer.
		 */
		while (!(back->b_flags & B_LEADER)) {
			back = (buf_t*)back->b_fsprivate;
		}
		ASSERT(back != NULL);
		ASSERT(back->b_flags & B_LEADER);
		leader = back;
		if (rbp->b_flags & B_ERROR) {
			leader->b_flags |= B_ERROR;
			leader->b_error = XFS_ERROR(rbp->b_error);
			ASSERT(leader->b_error != EINVAL);
		}
		mutex_spinunlock(&xfs_strat_lock, s);
	}

	rbp->b_fsprivate = NULL;
	rbp->b_fsprivate2 = NULL;
	rbp->b_relse = NULL;

	if (BP_ISMAPPED(rbp)) {
		bp_mapout(rbp);
	}

	freerbuf(rbp);
}

#ifdef DEBUG
/*ARGSUSED*/
void
xfs_check_rbp(
	xfs_inode_t	*ip,
	buf_t		*bp,
	buf_t		*rbp,
	int		locked)
{
	xfs_mount_t	*mp;
	int		nimaps;
	xfs_bmbt_irec_t	imap;
	xfs_fileoff_t	rbp_offset_fsb;
	xfs_filblks_t	rbp_len_fsb;
	pfd_t		*pfdp;
	xfs_fsblock_t	firstblock;
	int		error;

	mp = ip->i_mount;
	rbp_offset_fsb = XFS_BB_TO_FSBT(mp, rbp->b_offset);
	rbp_len_fsb = XFS_BB_TO_FSB(mp, rbp->b_offset+BTOBB(rbp->b_bcount)) -
		      XFS_BB_TO_FSBT(mp, rbp->b_offset);
	nimaps = 1;
	if (!locked) {
		xfs_ilock(ip, XFS_ILOCK_SHARED);
	}
	firstblock = NULLFSBLOCK;
	error = xfs_bmapi(NULL, ip, rbp_offset_fsb, rbp_len_fsb, 0,
			  &firstblock, 0, &imap, &nimaps, NULL);
	if (!locked) {
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
	}
	if (error) {
		return;
	}

	ASSERT(imap.br_startoff == rbp_offset_fsb);
	ASSERT(imap.br_blockcount == rbp_len_fsb);
	ASSERT((XFS_FSB_TO_DB(ip, imap.br_startblock) +
		XFS_BB_FSB_OFFSET(mp, rbp->b_offset)) ==
	       rbp->b_blkno);

	if (rbp->b_flags & B_PAGEIO) {
		pfdp = NULL;
		pfdp = getnextpg(rbp, pfdp);
		ASSERT(pfdp != NULL);
		ASSERT(dtopt(rbp->b_offset) == pfdp->pf_pageno);
	}

	if (rbp->b_flags & B_MAPPED) {
		ASSERT(BTOBB(poff(rbp->b_un.b_addr)) ==
		       dpoff(rbp->b_offset));
	}
}

/*
 * Verify that the given buffer is going to the right place in its
 * file.  Also check that it is properly mapped and points to the
 * right page.  We can only do a trylock from here in order to prevent
 * deadlocks, since this is called from the strategy routine.
 */
void
xfs_check_bp(
	xfs_inode_t	*ip,
	buf_t		*bp)
{
	xfs_mount_t	*mp;
	int		nimaps;
	xfs_bmbt_irec_t	imap[2];
	xfs_fileoff_t	bp_offset_fsb;
	xfs_filblks_t	bp_len_fsb;
	pfd_t		*pfdp;
	int		locked;
	xfs_fsblock_t	firstblock;
	int		error;
	int		bmapi_flags;

	mp = ip->i_mount;

	if (bp->b_flags & B_PAGEIO) {
		pfdp = NULL;
		pfdp = getnextpg(bp, pfdp);
		ASSERT(pfdp != NULL);
		ASSERT(dtopt(bp->b_offset) == pfdp->pf_pageno);
		if (dpoff(bp->b_offset)) {
			ASSERT(bp->b_flags & B_MAPPED);
		}
	}

	if (bp->b_flags & B_MAPPED) {
		ASSERT(BTOBB(poff(bp->b_un.b_addr)) ==
		       dpoff(bp->b_offset));
	}

	bp_offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
	bp_len_fsb = XFS_BB_TO_FSB(mp, bp->b_offset + BTOBB(bp->b_bcount)) -
		     XFS_BB_TO_FSBT(mp, bp->b_offset);
	ASSERT(bp_len_fsb > 0);
	if (bp->b_flags & B_UNINITIAL) {
		nimaps = 2;
		bmapi_flags = XFS_BMAPI_WRITE|XFS_BMAPI_IGSTATE;
	} else {
		nimaps = 1;
		bmapi_flags = 0;
	}

	locked = xfs_ilock_nowait(ip, XFS_ILOCK_SHARED);
	if (!locked) {
		return;
	}
	firstblock = NULLFSBLOCK;
	error = xfs_bmapi(NULL, ip, bp_offset_fsb, bp_len_fsb, bmapi_flags,
			  &firstblock, 0, imap, &nimaps, NULL);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	if (error) {
		return;
	}

	ASSERT(nimaps == 1);
	ASSERT(imap->br_startoff == bp_offset_fsb);
	ASSERT(imap->br_blockcount == bp_len_fsb);
	ASSERT((XFS_FSB_TO_DB(ip, imap->br_startblock) +
		XFS_BB_FSB_OFFSET(mp, bp->b_offset)) ==
	       bp->b_blkno);
}
#endif /* DEBUG */


/*
 *	xfs_strat_write_unwritten is called for buffered
 *	writes of preallocated but unwritten extents. These
 *	require a transaction to indicate the extent has been
 *	written.
 *	The write is set up by a call to xfs_bmapi with
 *	an "ignore state" flag. After the I/O has completed,
 *	xfs_strat_write_iodone is called to queue the completed
 *	buffer to xfs_strat_write_complete. That routine 
 *	calls xfs_bmapi() with a write flag, and issues the 
 *	required transaction.
 */

STATIC int
xfs_strat_write_unwritten(
	bhv_desc_t	*bdp,
	buf_t		*bp)
{
	xfs_fileoff_t	offset_fsb;
	off_t		offset_fsb_bb;
	xfs_filblks_t	count_fsb;
	/* REFERENCED */
	xfs_mount_t	*mp;
	xfs_inode_t	*ip;
	xfs_bmap_free_t	free_list;
	xfs_fsblock_t	first_block;
	int		error;
	int		nimaps;
	xfs_bmbt_irec_t	imap[XFS_BMAP_MAX_NMAP];
#define	XFS_STRAT_WRITE_IMAPS	2

	/*
	 * If XFS_STRAT_WRITE_IMAPS is changed then the definition
	 * of XFS_STRATW_LOG_RES in xfs_trans.h must be changed to
	 * reflect the new number of extents that can actually be
	 * allocated in a single transaction.
	 */
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;
	error = 0;

	/*
	 * Drop the count of queued buffers. We need to do
	 * this before the bdstrat because callers of
	 * VOP_FLUSHINVAL_PAGES(), for example, may expect the queued_buf
	 * count to be down when it rturns. See xfs_itruncate_start.
	 */
	atomicAddInt(&(ip->i_queued_bufs), -1);
	ASSERT(ip->i_queued_bufs >= 0);

	/*
	 * Don't proceed if we're forcing a shutdown.
	 */
	if (XFS_FORCED_SHUTDOWN(mp)) {
		return xfs_bioerror_relse(bp);
	}

	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0)) {
				return xfs_bioerror_relse(bp);
			}
		}
	}

	/*
	 * 
	 */

	offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
	count_fsb = XFS_B_TO_FSB(mp, bp->b_bcount);
	offset_fsb_bb = XFS_FSB_TO_BB(mp, offset_fsb);
	ASSERT((offset_fsb_bb == bp->b_offset) || (count_fsb == 1));

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_strat_write_bp_trace(XFS_STRAT_ENTER, ip, bp);

	/*
	 * Modify the unwritten extent state of the buffer.
	 */
	XFS_BMAP_INIT(&(free_list), &(first_block));
	nimaps = 2;
	error = xfs_bmapi(NULL, ip, offset_fsb, count_fsb,
			  XFS_BMAPI_WRITE|XFS_BMAPI_IGSTATE, &first_block,
			  0, imap, &nimaps, &free_list);
	if (error) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		goto error0;
	}

	ASSERT(nimaps == 1);
	if (bp->b_blkno < 0) {
		bp->b_blkno = XFS_FSB_TO_DB(ip, imap->br_startblock) +
				(bp->b_offset - offset_fsb_bb);
	}
	
	/*
	 * Before dropping the inode lock, clear any
	 * read-ahead state since in allocating space
	 * here we may have made it invalid.
	 */
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	XFS_INODE_CLEAR_READ_AHEAD(ip);

	/*
	 * For an unwritten buffer, do a write of the buffer,
	 * and commit the transaction which modified the
	 * extent state.
	 */
	ASSERT((imap[0].br_startoff <= offset_fsb) &&
	       (imap[0].br_blockcount >=
	       (offset_fsb + count_fsb - imap[0].br_startoff)));
	ASSERT(bp->b_iodone == NULL);
	bp->b_iodone = xfs_strat_write_iodone;
	xfs_strat_write_bp_trace(XFS_STRAT_UNINT, ip, bp);
	xfs_check_bp(ip, bp);
	xfsbdstrat(mp, bp);
	return 0;

 error0:
	bp->b_flags |= B_ERROR;
	bp->b_error = error;
	biodone(bp);
	return error;
}


/*
 * This is called to convert all delayed allocation blocks in the given
 * range back to 'holes' in the file.  It is used when a buffer will not
 * be able to be written out due to disk errors in the allocation calls.
 */
STATIC void
xfs_delalloc_cleanup(
	xfs_inode_t	*ip,
	xfs_fileoff_t	start_fsb,
	xfs_filblks_t	count_fsb)
{
	xfs_fsblock_t	first_block;
	int		nimaps;
	int		done;
	int		error;
	int		n;
#define	XFS_CLEANUP_MAPS	4
	xfs_bmbt_irec_t	imap[XFS_CLEANUP_MAPS];

	ASSERT(count_fsb < 0xffff000);
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	while (count_fsb != 0) {
		first_block = NULLFSBLOCK;
		nimaps = XFS_CLEANUP_MAPS;
		error = xfs_bmapi(NULL, ip, start_fsb, count_fsb, 0,
				  &first_block, 1, imap, &nimaps, NULL);
		if (error) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			return;
		}

		ASSERT(nimaps > 0);
		n = 0;
		while (n < nimaps) {
			if (imap[n].br_startblock == DELAYSTARTBLOCK) {
				if (!XFS_FORCED_SHUTDOWN(ip->i_mount))
					xfs_force_shutdown(ip->i_mount,
						XFS_METADATA_IO_ERROR);
				error = xfs_bunmapi(NULL, ip,
						    imap[n].br_startoff,
						    imap[n].br_blockcount,
						    0, 1, &first_block, NULL,
						    &done);
				if (error) {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					return;
				}
				ASSERT(done);
			}
			start_fsb += imap[n].br_blockcount;
			count_fsb -= imap[n].br_blockcount;
			ASSERT(count_fsb < 0xffff000);
			n++;
		}
	}
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
}

/*
 *	xfs_strat_write is called for buffered writes which
 *	require a transaction. These cases are:
 *	- Delayed allocation (since allocation now takes place).
 *	- Writing a previously unwritten extent.
 */

STATIC int
xfs_strat_write(
	bhv_desc_t	*bdp,
	buf_t		*bp)
{
	xfs_fileoff_t	offset_fsb;
	off_t		offset_fsb_bb;
	xfs_fileoff_t   map_start_fsb;
	xfs_fileoff_t	imap_offset;
	xfs_fsblock_t	first_block;
	xfs_filblks_t	count_fsb;
	xfs_extlen_t	imap_blocks;
#ifdef DEBUG
	off_t		last_rbp_offset;
	xfs_extlen_t	last_rbp_bcount;
	daddr_t		last_rbp_blkno;
#endif
	/* REFERENCED */
	int		rbp_count;
	buf_t		*rbp;
	xfs_mount_t	*mp;
	xfs_inode_t	*ip;
	xfs_trans_t	*tp;
	int		error;
	xfs_bmap_free_t	free_list;
	xfs_bmbt_irec_t	*imapp;
	int		rbp_offset;
	int		rbp_len;
	int		set_lead;
	int		s;
	/* REFERENCED */
	int		loops;
	int		imap_index;
	int		nimaps;
	int		committed;
	xfs_lsn_t	commit_lsn;
	xfs_bmbt_irec_t	imap[XFS_BMAP_MAX_NMAP];
#define	XFS_STRAT_WRITE_IMAPS	2

	/*
	 * If XFS_STRAT_WRITE_IMAPS is changed then the definition
	 * of XFS_STRATW_LOG_RES in xfs_trans.h must be changed to
	 * reflect the new number of extents that can actually be
	 * allocated in a single transaction.
	 */
	 
	XFSSTATS.xs_xstrat_bytes += bp->b_bcount;
	if ((bp->b_flags & B_UNINITIAL) == B_UNINITIAL)
		return xfs_strat_write_unwritten(bdp, bp);

	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;
	set_lead = 0;
	rbp_count = 0;
	error = 0;
	bp->b_flags |= B_STALE;

	/*
	 * Drop the count of queued buffers. We need to do
	 * this before the bdstrat(s) because callers of
	 * VOP_FLUSHINVAL_PAGES(), for example, may expect the queued_buf
	 * count to be down when it rturns. See xfs_itruncate_start.
	 */
	atomicAddInt(&(ip->i_queued_bufs), -1);
	ASSERT(ip->i_queued_bufs >= 0);

	/*
	 * Don't proceed if we're forcing a shutdown.
	 * We may not have bmap'd all the blocks needed.
	 */
	if (XFS_FORCED_SHUTDOWN(mp)) {
		return xfs_bioerror_relse(bp);
	}

	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0)) {
				return xfs_bioerror_relse(bp);
			}
		}
	}

	/*
	 * It is possible that the buffer does not start on a block
	 * boundary in the case where the system page size is less
	 * than the file system block size.  In this case, the buffer
	 * is guaranteed to be only a single page long, so we know
	 * that we will allocate the block for it in a single extent.
	 * Thus, the looping code below does not have to worry about
	 * this case.  It is only handled in the fast path code.
	 */

	ASSERT(bp->b_blkno == -1);
	offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
	count_fsb = XFS_B_TO_FSB(mp, bp->b_bcount);
	offset_fsb_bb = XFS_FSB_TO_BB(mp, offset_fsb);
	ASSERT((offset_fsb_bb == bp->b_offset) || (count_fsb == 1));
	xfs_strat_write_check(ip, offset_fsb,
			      count_fsb, imap,
			      XFS_STRAT_WRITE_IMAPS);
	map_start_fsb = offset_fsb;
	while (count_fsb != 0) {
		/*
		 * Set up a transaction with which to allocate the
		 * backing store for the file.  Do allocations in a
		 * loop until we get some space in the range we are
		 * interested in.  The other space that might be allocated
		 * is in the delayed allocation extent on which we sit
		 * but before our buffer starts.
		 */
		nimaps = 0;
		loops = 0;
		while (nimaps == 0) {
			tp = xfs_trans_alloc(mp,
					     XFS_TRANS_STRAT_WRITE);
			error = xfs_trans_reserve(tp, 0,
					XFS_WRITE_LOG_RES(mp),
					0, XFS_TRANS_PERM_LOG_RES,
					XFS_WRITE_LOG_COUNT);
			if (error) {
				xfs_trans_cancel(tp, 0);
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
				goto error0;
			}

			ASSERT(error == 0);
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ip,
					XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);
			xfs_strat_write_bp_trace(XFS_STRAT_ENTER,
						 ip, bp);

			/*
			 * Allocate the backing store for the file.
			 */
			XFS_BMAP_INIT(&(free_list),
				      &(first_block));
			nimaps = XFS_STRAT_WRITE_IMAPS;
			error = xfs_bmapi(tp, ip, map_start_fsb, count_fsb,
					  XFS_BMAPI_WRITE, &first_block, 1,
					  imap, &nimaps, &free_list);
			if (error) {
				xfs_bmap_cancel(&free_list);
				xfs_trans_cancel(tp,
						 (XFS_TRANS_RELEASE_LOG_RES |
						  XFS_TRANS_ABORT));
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
				goto error0;
			}
			ASSERT(loops++ <=
			       (offset_fsb +
				XFS_B_TO_FSB(mp, bp->b_bcount)));
			error = xfs_bmap_finish(&(tp), &(free_list),
						first_block, &committed);
			if (error) {
				xfs_bmap_cancel(&free_list);
				xfs_trans_cancel(tp,
						 (XFS_TRANS_RELEASE_LOG_RES |
						  XFS_TRANS_ABORT));
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
				goto error0;
			}

			error = xfs_trans_commit(tp,
						 XFS_TRANS_RELEASE_LOG_RES,
						 &commit_lsn);
			if (error) {
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
				goto error0;
			}

			/*
			 * write the commit lsn if requested into the
			 * place pointed at by the buffer.  This is
			 * used by IO_DSYNC writes and b_fsprivate3
			 * should be a pointer to a stack (automatic)
			 * variable.  So be *very* careful if you muck
			 * with b_fsprivate3.
			 */
			if (bp->b_fsprivate3)
				*(xfs_lsn_t *)bp->b_fsprivate3 = commit_lsn;

			/*
			 * Before dropping the lock, clear any read-ahead
			 * state since in allocating space here we may have
			 * made it invalid.
			 */
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			XFS_INODE_CLEAR_READ_AHEAD(ip);
		}

		/*
		 * This is a quick check to see if the first time through
		 * was able to allocate a single extent over which to
		 * write.
		 */
		if ((map_start_fsb == offset_fsb) &&
		    (imap[0].br_blockcount == count_fsb)) {
			ASSERT(nimaps == 1);
			/*
			 * Set the buffer's block number to match
			 * what we allocated.  If the buffer does
			 * not start on a block boundary (can only
			 * happen if the block size is larger than
			 * the page size), then make sure to add in
			 * the offset of the buffer into the file system
			 * block to the disk block number to write.
			 */
			bp->b_blkno =
				XFS_FSB_TO_DB(ip, imap[0].br_startblock) +
				(bp->b_offset - offset_fsb_bb);
			xfs_strat_write_bp_trace(XFS_STRAT_FAST,
						 ip, bp);
			xfs_check_bp(ip, bp);
#ifdef XFSRACEDEBUG
			delay_for_intr();
			delay(100);
#endif
			xfsbdstrat(mp, bp);

			XFSSTATS.xs_xstrat_quick++;
			return 0;
		}

		/*
		 * Bmap couldn't manage to lay the buffer out as
		 * one extent, so we need to do multiple writes
		 * to push the data to the multiple extents.
		 * Write out the subordinate bps asynchronously
		 * and have their completion functions coordinate
		 * with the code at the end of this function to
		 * deal with marking our bp as done when they have
		 * ALL completed.
		 */
		XFSSTATS.xs_xstrat_split++;
		imap_index = 0;
		if (!set_lead) {
			bp->b_flags |= B_LEADER | B_PARTIAL;
			set_lead = 1;
		}
		while (imap_index < nimaps) {
			rbp = getrbuf(KM_SLEEP);

			imapp = &(imap[imap_index]);
			ASSERT((imapp->br_startblock !=
				DELAYSTARTBLOCK) &&
			       (imapp->br_startblock !=
				HOLESTARTBLOCK));
			imap_offset = imapp->br_startoff;
			rbp_offset = XFS_FSB_TO_B(mp,
						  imap_offset -
						  offset_fsb);
			imap_blocks = imapp->br_blockcount;
			ASSERT((imap_offset + imap_blocks) <=
			       (offset_fsb +
				XFS_B_TO_FSB(mp, bp->b_bcount)));
			rbp_len = XFS_FSB_TO_B(mp,
					       imap_blocks);
			xfs_overlap_bp(bp, rbp, rbp_offset,
				       rbp_len);
			rbp->b_blkno =
				XFS_FSB_TO_DB(ip, imapp->br_startblock);
			rbp->b_offset = XFS_FSB_TO_BB(mp,
						      imap_offset);
			rbp->b_target = bp->b_target;
			xfs_strat_write_subbp_trace(XFS_STRAT_SUB,
						    ip, bp,
						    rbp,
						    last_rbp_offset,
						    last_rbp_bcount,
						    last_rbp_blkno);
#ifdef DEBUG
			xfs_check_rbp(ip, bp, rbp, 0);
			if (rbp_count > 0) {
				ASSERT((last_rbp_offset +
					BTOBB(last_rbp_bcount)) ==
				       rbp->b_offset);
				ASSERT((rbp->b_blkno <
					last_rbp_blkno) ||
				       (rbp->b_blkno >=
					(last_rbp_blkno +
					 BTOBB(last_rbp_bcount))));
				if (rbp->b_blkno <
				    last_rbp_blkno) {
					ASSERT((rbp->b_blkno +
					      BTOBB(rbp->b_bcount)) <
					       last_rbp_blkno);
				}
			}
			last_rbp_offset = rbp->b_offset;
			last_rbp_bcount = rbp->b_bcount;
			last_rbp_blkno = rbp->b_blkno;
#endif
					       
			
			/*
			 * Link the buffer into the list of subordinate
			 * buffers started at bp->b_fsprivate2.  The
			 * subordinate buffers use b_fsprivate and
			 * b_fsprivate2 for back and forw pointers, but
			 * the lead buffer cannot use b_fsprivate.
			 * A subordinate buffer can always find the lead
			 * buffer by searching back through the fsprivate
			 * fields until it finds the buffer marked with
			 * B_LEADER.
			 */
			s = mutex_spinlock(&xfs_strat_lock);
			rbp->b_fsprivate = bp;
			rbp->b_fsprivate2 = bp->b_fsprivate2;
			if (bp->b_fsprivate2 != NULL) {
				((buf_t*)(bp->b_fsprivate2))->b_fsprivate =
								rbp;
			}
			bp->b_fsprivate2 = rbp;
			mutex_spinunlock(&xfs_strat_lock, s);

			rbp->b_relse = xfs_strat_write_relse;
			rbp->b_flags |= B_ASYNC;

#ifdef XFSRACEDEBUG
			delay_for_intr();
			delay(100);
#endif
			xfsbdstrat(mp, rbp);
			map_start_fsb +=
				imapp->br_blockcount;
			count_fsb -= imapp->br_blockcount;
			ASSERT(count_fsb < 0xffff000);

			imap_index++;
		}
	}

	/*
	 * Now that we've issued all the partial I/Os, check to see
	 * if they've all completed.  If they have then mark the buffer
	 * as done, otherwise clear the B_PARTIAL flag in the buffer to
	 * indicate that the last subordinate buffer to complete should
	 * mark the buffer done.  Also, drop the count of queued buffers
	 * now that we know that all the space underlying the buffer has
	 * been allocated and it has really been sent out to disk.
	 *
	 * Use set_lead to tell whether we kicked off any partial I/Os
	 * or whether we jumped here after an error before issuing any.
	 */
 error0:
	if (error) {
		ASSERT(count_fsb != 0);
		/*
		 * Since we're never going to convert the remaining
		 * delalloc blocks beneath this buffer into real block,
		 * get rid of them now.
		 */
		xfs_delalloc_cleanup(ip, map_start_fsb, count_fsb);
	}
	if (set_lead) {
		s = mutex_spinlock(&xfs_strat_lock);
		ASSERT((bp->b_flags & (B_DONE | B_PARTIAL)) == B_PARTIAL);
		ASSERT(bp->b_flags & B_LEADER);
		
		if (bp->b_fsprivate2 == NULL) {
			/*
			 * All of the subordinate buffers have completed.
			 * Call iodone() to note that the I/O has completed.
			 */
			bp->b_flags &= ~(B_PARTIAL | B_LEADER);
			mutex_spinunlock(&xfs_strat_lock, s);

			biodone(bp);
			return error;
		}

		bp->b_flags &= ~B_PARTIAL;
		mutex_spinunlock(&xfs_strat_lock, s);
	} else {
		biodone(bp);
	}
	return error;
}

/*
 * Force a shutdown of the filesystem instantly while keeping
 * the filesystem consistent. We don't do an unmount here; just shutdown
 * the shop, make sure that absolutely nothing persistent happens to
 * this filesystem after this point. 
 */
void
xfs_force_shutdown(
	xfs_mount_t	*mp,
	int		flags)
{
	int ntries;
	int logerror;
	extern dev_t rootdev;		/* from sys/systm.h */

#define XFS_MAX_DRELSE_RETRIES	10
	logerror = flags & XFS_LOG_IO_ERROR;

	/*
	 * No need to duplicate efforts.
	 */
	if (XFS_FORCED_SHUTDOWN(mp) && !logerror)
		return;

	if (XFS_MTOVFS(mp)->vfs_dev == rootdev)
		cmn_err(CE_PANIC, "Fatal error on root filesystem");

	/*
	 * This flags XFS_MOUNT_FS_SHUTDOWN, makes sure that we don't
	 * queue up anybody new on the log reservations, and wakes up
	 * everybody who's sleeping on log reservations and tells
	 * them the bad news.
	 */
	if (xfs_log_force_umount(mp, logerror))
		return;

	if (flags & XFS_CORRUPT_INCORE)
		cmn_err(CE_ALERT,
    "Corruption of in-memory data detected.  Shutting down filesystem: %s",
			mp->m_fsname);
	else
		cmn_err(CE_ALERT,
			"I/O Error Detected.  Shutting down filesystem: %s",
			mp->m_fsname);

	cmn_err(CE_ALERT,
		"Please umount the filesystem, and rectify the problem(s)");

	/*
	 * Release all delayed write buffers for this device.
	 * It wouldn't be a fatal error if we couldn't release all
	 * delwri bufs; in general they all get unpinned eventually.
	 */
	ntries = 0;
#ifdef XFSERRORDEBUG
	{
		int nbufs;
		while (nbufs = incore_relse(mp->m_dev, 1, 0)) {
			printf("XFS: released 0x%x bufs\n", nbufs);
			if (ntries >= XFS_MAX_DRELSE_RETRIES) {
				printf("XFS: ntries 0x%x\n", ntries);
				debug("ntries");
				break;
			}
			delay(++ntries * 5);
		}
	}
#else
	while (incore_relse(mp->m_dev, 1, 0)) {
		if (ntries >= XFS_MAX_DRELSE_RETRIES)
			break;
		delay(++ntries * 5);
	}

#endif
}


/*
 * Called when we want to stop a buffer from getting written or read.
 * We attach the EIO error, muck with its flags, and call biodone
 * so that the proper iodone callbacks get called.
 */
int
xfs_bioerror(
	buf_t *bp)
{

#ifdef XFSERRORDEBUG
	ASSERT(bp->b_flags & B_READ || bp->b_iodone);
#endif

	/*
	 * No need to wait until the buffer is unpinned.
	 * We aren't flushing it.
	 */
	buftrace("XFS IOERROR", bp);
	bioerror(bp, EIO);
	/*
	 * We're calling biodone, so delete B_DONE flag. Either way
	 * we have to call the iodone callback, and calling biodone
	 * probably is the best way since it takes care of
	 * GRIO as well.
	 */
	bp->b_flags &= ~(B_READ|B_DELWRI|B_DONE);
	bp->b_flags |= B_STALE;

	bp->b_bdstrat = NULL;
	biodone(bp);
	
	return (EIO);
}

/*
 * Same as xfs_bioerror, except that we are releasing the buffer
 * here ourselves, and avoiding the biodone call.
 * This is meant for userdata errors; metadata bufs come with
 * iodone functions attached, so that we can track down errors.
 */
int
xfs_bioerror_relse(
	buf_t *bp)
{
	int64_t fl;

	ASSERT(bp->b_iodone != xfs_buf_iodone_callbacks);
	ASSERT(bp->b_iodone != xlog_iodone);

	buftrace("XFS IOERRELSE", bp);
	fl = bp->b_flags;
	/*
	 * No need to wait until the buffer is unpinned.
	 * We aren't flushing it.
	 *
	 * chunkhold expects B_DONE to be set, whether
	 * we actually finish the I/O or not. We don't want to
	 * change that interface.
	 */
	bp->b_flags &= ~(B_READ|B_DELWRI);
	bp->b_flags |= B_DONE|B_STALE;
	bp->b_iodone = NULL;
	bp->b_bdstrat = NULL;
	if (!(fl & B_ASYNC)) {
		/*
		 * Mark b_error and B_ERROR _both_.
		 * Lot's of chunkcache code assumes that.
		 * There's no reason to mark error for
		 * ASYNC buffers.
		 */
		bioerror(bp, EIO);
		vsema(&bp->b_iodonesema);
	} else {
		brelse(bp);
	}
	return (EIO);
}

/*
 * Prints out an ALERT message about I/O error. 
 */
void
xfs_ioerror_alert(
	char 			*func,
	struct xfs_mount	*mp,
	dev_t			dev,
	daddr_t			blkno)
{
	cmn_err(CE_ALERT,
 "I/O error in filesystem (\"%s\") meta-data dev 0x%x block 0x%x (\"%s\")",
		mp->m_fsname, 
		dev,
		blkno,
		func);
}

/*
 * This isn't an absolute requirement, but it is
 * just a good idea to call xfs_read_buf instead of
 * directly doing a read_buf call. For one, we shouldn't
 * be doing this disk read if we are in SHUTDOWN state anyway,
 * so this stops that from happening. Secondly, this does all
 * the error checking stuff and the brelse if appropriate for
 * the caller, so the code can be a little leaner.
 */
int
xfs_read_buf(
	struct xfs_mount *mp,
	buftarg_t	 *target,
        daddr_t 	 blkno,
        int              len,
        uint             flags,
	buf_t		 **bpp)
{
	buf_t		 *bp;
	int 		 error;
	
	bp = read_buf_targ(target, blkno, len, flags);
	error = geterror(bp);
	if (bp && !error && !XFS_FORCED_SHUTDOWN(mp)) {
		*bpp = bp;
	} else {
		*bpp = NULL;
		if (!error)
			error = XFS_ERROR(EIO);
		if (bp) {	
			bp->b_flags &= ~(B_DONE|B_DELWRI);
			bp->b_flags |= B_STALE;
			/* 
			 * brelse clears B_ERROR and b_error
			 */
			brelse(bp);
		}
	}
	return (error);
}
	
/*
 * Wrapper around bwrite() so that we can trap 
 * write errors, and act accordingly.
 */
int
xfs_bwrite(
	struct xfs_mount *mp,
	struct buf	 *bp)
{
	int	error;

	/*
	 * XXXsup how does this work for quotas.
	 */
	ASSERT(bp->b_target);
	ASSERT(bp->b_vp == NULL);
	bp->b_bdstrat = xfs_bdstrat_cb;
	bp->b_fsprivate3 = mp;

	if (error = bwrite(bp)) {
		ASSERT(mp);
		/* 
		 * Cannot put a buftrace here since if the buffer is not 
		 * B_HOLD then we will brelse() the buffer before returning 
		 * from bwrite and we could be tracing a buffer that has 
		 * been reused.
		 */
		xfs_force_shutdown(mp, XFS_METADATA_IO_ERROR);
	}
	return (error);
}

/*
 * All xfs metadata buffers except log state machine buffers
 * get this attached as their b_bdstrat callback function. 
 * This is so that we can catch a buffer
 * after prematurely unpinning it to forcibly shutdown the filesystem.
 */
int
xfs_bdstrat_cb(struct buf *bp)
{

	xfs_mount_t	*mp;

	mp = bp->b_fsprivate3;

	ASSERT(bp->b_target);
	if (!XFS_FORCED_SHUTDOWN(mp)) {
#if CELL_IRIX
		vnode_t	*vp;
		vp = bp->b_target->specvp;
		ASSERT(vp);
		bp->b_bdstrat = NULL;
		VOP_STRATEGY(vp, bp);
#else
		struct bdevsw *my_bdevsw;
		my_bdevsw =  bp->b_target->bdevsw;
		ASSERT(my_bdevsw != NULL);
		bp->b_bdstrat = NULL;
		bdstrat(my_bdevsw, bp);
#endif /* CELL_IRIX */
		return 0;
	} else { 
		buftrace("XFS__BDSTRAT IOERROR", bp);
		/*
		 * Metadata write that didn't get logged but 
		 * written delayed anyway. These aren't associated
		 * with a transaction, and can be ignored.
		 */
		if (bp->b_iodone == NULL &&
		    (bp->b_flags & B_READ) == 0)
			return (xfs_bioerror_relse(bp));
		else
			return (xfs_bioerror(bp));
	}
}

/*
 * Wrapper around bdstrat so that we can stop data
 * from going to disk in case we are shutting down the filesystem.
 * Typically user data goes thru this path; one of the exceptions
 * is the superblock.
 */
int
xfsbdstrat(
	struct xfs_mount 	*mp,
	struct buf		*bp)
{
	int		dev_major = emajor(bp->b_edev);

	ASSERT(mp);
	ASSERT(bp->b_target);
	if (!XFS_FORCED_SHUTDOWN(mp)) {
		/*
		 * We want priority I/Os to non-XLV disks to go thru'
		 * griostrategy(). The rest of the I/Os follow the normal
		 * path, and are uncontrolled. If we want to rectify
		 * that, use griostrategy2.
		 */
		if ( (BUF_IS_GRIO(bp)) &&
				(dev_major != XLV_MAJOR) ) {
			griostrategy(bp);
		} else {
#if CELL_IRIX
			vnode_t	*vp;

			vp = bp->b_target->specvp;
			ASSERT(vp);
			VOP_STRATEGY(vp, bp);
#else
			struct bdevsw	*my_bdevsw;

			my_bdevsw = bp->b_target->bdevsw;
			ASSERT(my_bdevsw != NULL);
			bdstrat(my_bdevsw, bp);
#endif
		}
		return 0;
	}

	buftrace("XFSBDSTRAT IOERROR", bp);
	return (xfs_bioerror_relse(bp));
}


/*
 * xfs_strategy
 *
 * This is where all the I/O and all the REAL allocations take
 * place.  For buffers with -1 for their b_blkno field, we need
 * to do a bmap to figure out what to do with them.  If it's a
 * write we may need to do an allocation, while if it's a read
 * we may either need to read from disk or do some block zeroing.
 * If b_blkno specifies a real but prevously unwritten block,
 * a transaction needs to be initiated to mark the block as
 * initialized. If b_blkno specifies a real block, then all
 * we need to do is pass the buffer on to the underlying driver.
 */
void
xfs_strategy(
	bhv_desc_t	*bdp,
	buf_t		*bp)
{
	int		s;
	xfs_inode_t	*ip;
	xfs_mount_t	*mp;

	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;
	bp->b_target = (bp->b_edev == mp->m_dev) ? mp->m_ddev_targp :
						   &mp->m_rtdev_targ;
	/*
	 * If this is just a buffer whose underlying disk space
	 * is already allocated, then just do the requested I/O.
	 */
	buftrace("XFS_STRATEGY", bp);
	if (bp->b_blkno >= 0 && !(bp->b_flags & B_UNINITIAL)) {
		xfs_check_bp(ip, bp);
		/*
		 * XXXsup We should probably ignore FORCED_SHUTDOWN
		 * in this case. The disk space is already allocated,
		 * and this seems like data loss that can be avoided.
		 * But I need to test it first.
		 */
		(void) xfsbdstrat(ip->i_mount, bp);
		return;
	}

	/*
	 * If we're reading, then we need to find out how the
	 * portion of the file required for this buffer is layed
	 * out and zero/read in the appropriate data.
	 */
	if (bp->b_flags & B_READ) {
		xfs_strat_read(bdp, bp);
		return;
	}

	if (XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		xfs_bioerror_relse(bp);
		return;
	}
	/*
	 * Here we're writing the file and probably need to allocate
	 * some underlying disk space or to mark it as initialized.
	 * If the buffer is being written asynchronously by bdflush()
	 * then we queue if for the xfsds so that we won't put
	 * bdflush() to sleep.
	 */
	if ((bp->b_flags & (B_ASYNC | B_BDFLUSH)) == (B_ASYNC | B_BDFLUSH) &&
	    (xfsd_count > 0)) {
		s = mp_mutex_spinlock(&xfsd_lock);
		/*
		 * Queue the buffer at the end of the list.
		 * Bump the inode count of the number of queued buffers.
		 */
		if (xfsd_list == NULL) {
			bp->av_forw = bp;
			bp->av_back = bp;
			xfsd_list = bp;
		} else {
			bp->av_back = xfsd_list->av_back;
			xfsd_list->av_back->av_forw = bp;
			xfsd_list->av_back = bp;
			bp->av_forw = xfsd_list;
		}
		/*
		 * Store the behavior pointer where the xfsds can find
		 * it so that we don't have to lookup the XFS behavior
		 * from the vnode in the buffer again.
		 */
		bp->b_private = bdp;
		xfsd_bufcount++;
		ASSERT(ip->i_queued_bufs >= 0);
		atomicAddInt(&(ip->i_queued_bufs), 1);
		(void)sv_signal(&xfsd_wait);
		mp_mutex_spinunlock(&xfsd_lock, s);
	} else {
		/*
		 * We're not going to queue it for the xfsds, but bump the
		 * inode's count anyway so that we can tell that this
		 * buffer is still on its way out.
		 */
		ASSERT(ip->i_queued_bufs >= 0);
		atomicAddInt(&(ip->i_queued_bufs), 1);
		xfs_strat_write(bdp, bp);
	}
}

/*
 * This is called from xfs_init() to start the xfs daemons.
 * We'll start with a minimum of 4 of them, and add 1
 * for each 128 MB of memory up to 1 GB.  That should
 * be enough.
 */
void
xfs_start_daemons(void)
{
	int	num_daemons;
	int	i;
	int	num_pages;
	extern int xfsd_pri;

	num_daemons = 4;
#if _MIPS_SIM != _ABI64
	/*
	 * For small memory systems we reduce the number of daemons
	 * to conserve memory.  For systems with less than 32 MB of
	 * memory we go with 2 daemons, and for systems with less
	 * than 48 MB of memory we go with 3.
	 */
	if (physmem < 8192) {
		num_daemons = 2;
	} else if (physmem < 12288) {
		num_daemons = 3;
	}
#endif
	num_pages = (int)physmem - 32768;
	while ((num_pages > 0) && (num_daemons < 13)) {
		num_pages -= 32768;
		num_daemons++;
	}
	ASSERT(num_daemons <= 13);

#define XFSD_SSIZE	(2*KTHREAD_DEF_STACKSZ)
	/*
	 * Start the xfs_strat_completion daemon, and the 
	 * xfsds. For now, use the same priority.
	 */
	sthread_create("xfsc", 0, XFSD_SSIZE, 0, xfsd_pri, KT_PS,
			(st_func_t *)xfs_strat_comp, 0, 0, 0, 0);
	for (i = 0; i < num_daemons; i++) {
		sthread_create("xfsd", 0, XFSD_SSIZE, 0, xfsd_pri, KT_PS,
				(st_func_t *)xfsd, 0, 0, 0, 0);
	}
#undef XFSD_SSIZE
	return;
}


#define MAX_BUF_EXAMINED 10

/*
 * This function purges the xfsd list of all bufs belonging to the
 * specified vnode.  This will allow a file about to be deleted to 
 * remove its buffers from the xfsd_list so it doesn't have to wait
 * for them to be pushed out to disk
 */
void xfs_xfsd_list_evict(bhv_desc_t * bdp)
{
	vnode_t		*vp;
	xfs_inode_t	*ip;
	
	int	s;
	int	cur_count;	/* 
				 * Count of buffers that have been processed
				 * since aquiring the spin lock 
				 */
	int countdown;		/* 
				 * Count of buffers that have been processed
				 * since we started.  This will prevent non-
				 * termination if buffers are being added to
				 * the head of the list 
				 */
	buf_t	*bp;
	buf_t	*forw;
	buf_t	*back;
	buf_t	*next_bp;
	
	/* List and count of the saved buffers */
	buf_t	*bufs;
	unsigned int bufcount;

	/* Marker Buffers */
	buf_t	*cur_marker;
	buf_t	*end_marker;
	
	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	
	/* Initialize bufcount and bufs */
	bufs = NULL;
	bufcount = 0;

	/* Allocate both markers at once... it's a little nicer. */
	cur_marker = (buf_t *)kmem_alloc(sizeof(buf_t)*2, KM_SLEEP);
	
	/* A little sketchy pointer-math, but should be ok. */
	end_marker = cur_marker + 1;

	s = mp_mutex_spinlock(&xfsd_lock);
	
	/* Make sure there are buffers to check */
	if (xfsd_list == NULL) {
		mp_mutex_spinunlock(&xfsd_lock, s);
		
		kmem_free(cur_marker, sizeof(buf_t)*2);
		return;
	}

	/* 
	 * Ok.  We know we're going to use the markers now, so let's 
	 * actually initialize them.  At Ray's suggestion, we'll make
	 * the b_vp == -1 as the signifier that this is a marker.  We 
	 * know a marker is unlinked if it's av_forw and av_back pointers
	 * point to itself.
	 */

	cur_marker->b_vp = (vnode_t *)-1L;
	end_marker->b_vp = (vnode_t *)-1L;

	/* Now link end_marker onto the end of the list */

	end_marker->av_back = xfsd_list->av_back;
	xfsd_list->av_back->av_forw = end_marker;
	end_marker->av_forw = xfsd_list;
	xfsd_list->av_back = end_marker;

	xfsd_bufcount++;

	/* 
	 * Set the countdown to it's initial value.  This will be a snapshot
	 * of the size of the list.  If we process this many buffers without
	 * finding the end_marker, then someone is putting buffers onto the 
	 * head of the list
	 */
	countdown = xfsd_bufcount;
	
	/* Zero the initial count */
	cur_count = 0;

	bp = xfsd_list;
 
	/* 
	 * Loop: Assumptions: the end_marker has been set, bp is set to the 
	 * current buf being examined, the xfsd_lock is held, cur_marker is
	 * <not> linked into the list.
	 */
	while (1) {
		/* We are processing a buffer.  Take note of that fact. */
		cur_count++;
		countdown--;
		
		if (bp == end_marker) {
			/* Unlink it from the list */
			
			/* 
			 * If it's the only thing on the list, NULL the 
			 * xfsd_list. Otherwise, unlink normally 
			 */
			if (bp->av_forw == bp) {
				xfsd_list = NULL;
			} else {
				forw = bp->av_forw;
				back = bp->av_back;
				forw->av_back = back;
				back->av_forw = forw;
			}
			
			xfsd_bufcount--;

			/* Move the head of the list forward if necessary */
			if (bp == xfsd_list)
				xfsd_list = bp->av_forw;
			
			break;
		}

		/* Check to see if this buffer should be removed */
		if (bp->b_vp == vp) {
			next_bp = bp->av_forw;
	    
			/* Remove the buffer from the xfsd_list */
			forw = bp->av_forw;
			back = bp->av_back;
			forw->av_back = back;
			back->av_forw = forw;

			/* 
			 * If we removed the head of the xfsd_list, move 
			 * it forward. 
			 */
			if (xfsd_list == bp) xfsd_list = next_bp;

			xfsd_bufcount--;
			
			/* 
			 * We can't remove all of the list, since we know 
			 * we have yet to see the end_marker.. that's the
			 * only buffer we'll see that MIGHT be the sole
			 * occupant of the list.
			 */
			   
			/* Now add the buffer to the list of buffers to free */
			if (bufcount > 0) {			
				bufs->av_back->av_forw = bp;
				bp->av_back = bufs->av_back;
				bufs->av_back = bp;
				bp->av_forw = bufs;
				
				bufcount++;
			} else {
				bufs = bp;
				bp->av_forw = bp;
				bp->av_back = bp;
				
				bufcount = 1;
			}
			
			bp = next_bp;
		} else {
			bp = bp->av_forw;
		}
		
		/* Now, bp has been advanced. */
		
		/* Now before we iterate, make sure we haven't run too long */
		if (cur_count > MAX_BUF_EXAMINED) {
			/* 
			 * Stick the cur_marker into the current pos in the 
			 * list.  The only special case is if the current bp
			 * is the head of the list, in which case we have to
			 * point the head of the list.
			 */
			
			/* First, link the cur_marker before the new bp */
			cur_marker->av_forw = bp;
			cur_marker->av_back = bp->av_back;
			bp->av_back->av_forw = cur_marker;
			bp->av_back = cur_marker;
			
			xfsd_bufcount++;
			
			if (bp == xfsd_list)
				xfsd_list = cur_marker;
			
			/* Now, it's safe to release the lock... */
			mp_mutex_spinunlock(&xfsd_lock, s);
			
			/*
			 * Kill me! I'm here! KILL ME!!! (If an interrupt
			 * needs too happen, it can. Now we won't blow
			 * realtime ;-).
			 */
			
			s = mp_mutex_spinlock(&xfsd_lock);
			
			/* Zero the current count */
			cur_count = 0;
			
			/* 
			 * Figure out if we SHOULD continue (if the end_marker
			 * has been removed, give up, unless it's the head of
			 * the otherwise empty list, since it's about to be 
			 * dequed and then we'll stop.
			 */
			if ((end_marker->av_forw == end_marker) && 
				(xfsd_list != end_marker)) {
				break;
			}
			
			/*
			 * Now determine if we should start at the marker or
			 * from the beginning of the list.  It can't be the
			 * only thing on the list, since the end_marker should
			 * be there too.
			 */
			if (cur_marker->av_forw == cur_marker) {
				bp = xfsd_list;
			} else {
				bp = cur_marker->av_forw;
				
				/* 
				 * Now dequeue the marker.  It might be the 
				 * head of the list, so we might have to move
				 * the list head... 
				 */
				
				forw = cur_marker->av_forw;
				back = cur_marker->av_back;
				forw->av_back = back;
				back->av_forw = forw;
				
				if (cur_marker == xfsd_list)
					xfsd_list = bp;
				
				xfsd_bufcount--;
				
				/* 
				 * We know we can't be the only buffer on the 
				 * list, as previously stated... so we 
				 * continue. 
				 */
			}
		}

		/*
		 * If countdown reaches zero without breaking by dequeuing the
		 * end_marker, someone is putting things on the front of the 
		 * list, so we'll quit to thwart their cunning attempt to keep
		 * us tied up in the list.  So dequeue the end_marker now and
		 * break.
		 */
		   
		/*
		 * Invarients at this point:  The end_marker is on the list.
		 * It may or may not be the head of the list.  cur_marker is
		 * NOT on the list.
		 */
		if (countdown == 0) {
			/* Unlink the end_marker from the list */
			
			if (end_marker->av_forw == end_marker) {
				xfsd_list = NULL;
			} else {
				forw = end_marker->av_forw;
				back = end_marker->av_back;
				forw->av_back = back;
				back->av_forw = forw;
			}
			
			xfsd_bufcount--;

			/* Move the head of the list forward if necessary */
			if (end_marker == xfsd_list) xfsd_list = end_marker->av_forw;
			
			break;
		}
	}
	
	mp_mutex_spinunlock(&xfsd_lock, s);
	kmem_free(cur_marker, sizeof(buf_t)*2);	
	
	/*
	 * At this point, bufs contains the list of buffers that would have
	 * been written to disk, if we hadn't swiped them (which we did
	 * because they are part of a file being deleted, so they obviously
	 * shouldn't go to the disk.  At this point, we need to make them as
	 * done.
	 */
	
	bp = bufs;
	
	/* We use s as a counter.  It's handy, so there. */
	for (s = 0; s < bufcount; s++) {
		next_bp = bp->av_forw;
		
		bp->av_forw = bp;
		bp->av_back = bp;
		
		bp->b_flags |= B_STALE;
		atomicAddInt(&(ip->i_queued_bufs), -1);
		
		/* Now call biodone. */
		biodone(bp);
		
		bp = next_bp;
	}
}

/*
 * This is the main loop for the xfs daemons.
 * From here they wait in a loop for buffers which will
 * require transactions to write out and process them as they come.
 * This way we never force bdflush() to wait on one of our transactions,
 * thereby keeping the system happier and preventing buffer deadlocks.
 */
STATIC int
xfsd(void)
{
	int		s;
	buf_t		*bp;
	buf_t		*forw;
	buf_t		*back;
	bhv_desc_t	*bdp;
	xfs_inode_t	*ip;

	s = mp_mutex_spinlock(&xfsd_lock);
	xfsd_count++;

	while (1) {
		while (xfsd_list == NULL) {
			mp_sv_wait(&xfsd_wait, PRIBIO, &xfsd_lock, s);
			s = mp_mutex_spinlock(&xfsd_lock);
		}

		/*
		 * Pull a buffer off of the list.
		 */
		bp = xfsd_list;
		forw = bp->av_forw;
		back = bp->av_back;
		forw->av_back = back;
		back->av_forw = forw;
		if (forw == bp) {
			xfsd_list = NULL;
		} else {
			xfsd_list = forw;
		}
		xfsd_bufcount--;;
		ASSERT(xfsd_bufcount >= 0);

		bp->av_forw = bp;
		bp->av_back = bp;

		/* Now make sure we didn't just process a marker */
		if (bp->b_vp == (vnode_t *)-1L) {
			continue;
		}
		/*
		 * Don't give up the xfsd_lock until we have set the
		 * av_forw and av_back pointers, because otherwise 
		 * xfs_xfsd_list_evict() might be racing with us on
		 * a marker that it placed and we just removed.
		 */
		mp_mutex_spinunlock(&xfsd_lock, s);

		ASSERT((bp->b_flags & (B_BUSY | B_ASYNC | B_READ)) ==
		       (B_BUSY | B_ASYNC));
		XFSSTATS.xs_xfsd_bufs++;
		bdp = bp->b_private;
		bp->b_private = NULL;

		/* Now make sure we still want to write out this buffer */
		ip = XFS_BHVTOI(bdp);
		if (!((ip->i_d.di_nlink == 0) && (bp->b_vp->v_flag & VINACT))) {
			xfs_strat_write(bdp, bp);
		} else {
			bp->b_flags |= B_STALE;
			atomicAddInt(&(ip->i_queued_bufs), -1);
			biodone(bp);
		}

		s = mp_mutex_spinlock(&xfsd_lock);
	}
}

struct dio_s {
	bhv_desc_t	*bdp;
	cred_t		*cr;
	int		ioflag;
	struct pm	*uio_pmp;
};

/*
 * xfs_inval_cached_pages()
 * This routine is responsible for keeping direct I/O and buffered I/O
 * somewhat coherent.  From here we make sure that we're at least
 * temporarily holding the inode I/O lock exclusively and then call
 * the page cache to flush and invalidate any cached pages.  If there
 * are no cached pages this routine will be very quick.
 */
void
xfs_inval_cached_pages(
	xfs_inode_t	*ip,
	off_t		offset,
	off_t		len,
	void		*dio)		    
{
	struct dio_s	*diop = (struct dio_s *)dio;
	vnode_t		*vp;
	int		relock;
	__uint64_t	flush_end;

	vp = XFS_ITOV(ip);
	if (!VN_CACHED(vp)) {
		return;
	}
	/*
	 * We need to get the I/O lock exclusively in order
	 * to safely invalidate pages and mappings.
	 */
	relock = ismrlocked(&(ip->i_iolock), MR_ACCESS);
	if (relock) {
		xfs_iunlock(ip, XFS_IOLOCK_SHARED);
		xfs_ilock(ip, XFS_IOLOCK_EXCL);
	}

	/* Writing beyond EOF creates a hole that must be zeroed */
	if (diop && (offset > ip->i_d.di_size)) {
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		if (offset > ip->i_d.di_size) {
			xfs_zero_eof(ip, offset, ip->i_d.di_size, diop->cr,
					diop->uio_pmp);
		}
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
	}

	/*
	 * Round up to the next page boundary and then back
	 * off by one byte.  We back off by one because this
	 * is a first byte/last byte interface rather than
	 * a start/len interface.  We round up to a page
	 * boundary because the page/chunk cache code is
	 * slightly broken and won't invalidate all the right
	 * buffers otherwise.
	 *
	 * We also have to watch out for overflow, so if we
	 * go over the maximum off_t value we just pull back
	 * to that max.
	 */
	flush_end = (__uint64_t)ctooff(offtoc(offset + len)) - 1;
	if (flush_end > (__uint64_t)LONGLONG_MAX) {
		flush_end = LONGLONG_MAX;
	}
	xfs_inval_cached_trace(ip, offset, len, ctooff(offtoct(offset)),
		flush_end);
	VOP_FLUSHINVAL_PAGES(vp, ctooff(offtoct(offset)), (off_t)flush_end,
			FI_REMAPF_LOCKED);
	if (relock) {
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		xfs_ilock(ip, XFS_IOLOCK_SHARED);
	}
}

/*
 * A user has written some portion of a realtime extent.  We need to zero
 * what remains, so the caller can mark the entire realtime extent as
 * written.  This is only used for filesystems that don't support unwritten
 * extents.
 */
STATIC int
xfs_dio_write_zero_rtarea(
	xfs_inode_t	*ip,
	struct buf	*bp,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	count_fsb)
{
	char		*buf;
	long		bufsize, remain_count;
	int		error;
	xfs_mount_t	*mp;
	struct bdevsw	*my_bdevsw;
	xfs_bmbt_irec_t	imaps[XFS_BMAP_MAX_NMAP], *imapp;
	buf_t		*nbp;
	int		reccount, sbrtextsize;
	xfs_fsblock_t	firstfsb;
	xfs_fileoff_t	zero_offset_fsb, limit_offset_fsb;
	xfs_fileoff_t	orig_zero_offset_fsb;
	xfs_filblks_t	zero_count_fsb;

	ASSERT(ip->i_d.di_flags & XFS_DIFLAG_REALTIME);
	mp = ip->i_mount;
	sbrtextsize = mp->m_sb.sb_rextsize;
	/* Arbitrarily limit the buffer size to 32 FS blocks or less. */
	if (sbrtextsize <= 32)
		bufsize = XFS_FSB_TO_B(mp, sbrtextsize);
	else
		bufsize = mp->m_sb.sb_blocksize * 32;
	ASSERT(sbrtextsize > 0 && bufsize > 0);
	limit_offset_fsb = (((offset_fsb + count_fsb + sbrtextsize - 1)
				/ sbrtextsize ) * sbrtextsize );
	zero_offset_fsb = offset_fsb - (offset_fsb % sbrtextsize);
	orig_zero_offset_fsb = zero_offset_fsb;
	zero_count_fsb = limit_offset_fsb - zero_offset_fsb;
	reccount = 1;

	/* Discover the full realtime extent affected */

	error = xfs_bmapi(NULL, ip, zero_offset_fsb, 
			  zero_count_fsb, 0, &firstfsb, 0, imaps, 
			  &reccount, 0);
	imapp = &imaps[0];
	if (error)
		return error;

	buf = (char *)kmem_alloc(bufsize, KM_SLEEP|KM_CACHEALIGN);
	bzero(buf, bufsize);
	nbp = getphysbuf(bp->b_edev);
	nbp->b_grio_private = bp->b_grio_private;
						/* b_iopri */
     	nbp->b_error     = 0;
	nbp->b_edev	 = bp->b_edev;
	nbp->b_un.b_addr = buf;
	my_bdevsw	 = get_bdevsw(nbp->b_edev);
	ASSERT(my_bdevsw != NULL);

	/* Loop while there are blocks that need to be zero'ed */

	while (zero_offset_fsb < limit_offset_fsb) {
		remain_count = 0;
		if (zero_offset_fsb < offset_fsb)
			remain_count = offset_fsb - zero_offset_fsb;
		else if (zero_offset_fsb >= (offset_fsb + count_fsb))
			remain_count = limit_offset_fsb - zero_offset_fsb;
		else {
			zero_offset_fsb += count_fsb;
			continue;
		}
		remain_count = XFS_FSB_TO_B(mp, remain_count);
		nbp->b_flags     = bp->b_flags;
		nbp->b_bcount    = (bufsize < remain_count) ? bufsize :
						remain_count;
 	    	nbp->b_error     = 0;
		nbp->b_blkno     = XFS_FSB_TO_BB(mp, imapp->br_startblock +
				    (zero_offset_fsb - orig_zero_offset_fsb));
		(void) bdstrat(my_bdevsw, nbp);
		if ((error = geterror(nbp)) != 0)
			break;
		biowait(nbp);
		/* Stolen directly from xfs_dio_write */
		nbp->b_flags &= ~B_GR_BUF;	/* Why? B_PRV_BUF? */
		if ((error = geterror(nbp)) != 0)
			break;
		else if (nbp->b_resid)
			nbp->b_bcount -= nbp->b_resid;
			
		zero_offset_fsb += XFS_B_TO_FSB(mp, nbp->b_bcount);
	}
	/* Clean up for the exit */
	nbp->b_flags		= 0;
	nbp->b_bcount		= 0;
	nbp->b_un.b_addr	= 0;
	nbp->b_grio_private	= 0;	/* b_iopri */
 	putphysbuf( nbp );
	kmem_free(buf, bufsize);

	return error;
}

/*
 * xfs_dio_read()
 *	This routine issues the calls to the disk device strategy routine
 *	for file system read made using direct I/O from user space.
 *	The I/Os for each extent involved are issued at once.
 *
 * RETURNS:
 *	error 
 */
STATIC int
xfs_dio_read(
	xfs_dio_t *diop)
{
	buf_t		*bp;
	xfs_inode_t 	*ip;
	xfs_trans_t	*tp;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	imaps[XFS_BMAP_MAX_NMAP], *imapp;
	buf_t		*bps[XFS_BMAP_MAX_NMAP], *nbp;
	xfs_fileoff_t	offset_fsb;
	xfs_fsblock_t	firstfsb;
	xfs_filblks_t	count_fsb;
	xfs_bmap_free_t free_list;
	caddr_t		base;
	ssize_t		resid, count, totxfer;
	off_t		offset, offset_this_req, bytes_this_req, trail = 0;
	int		i, j, error, reccount;
	int		end_of_file, bufsissued, totresid;
	int		blk_algn, rt;
	int		unwritten;
	uint		lock_mode;
	xfs_fsize_t	new_size;

	CHECK_GRIO_TIMESTAMP(bp, 40);

	bp = diop->xd_bp;
	ip = diop->xd_ip;
	mp = diop->xd_mp;
	blk_algn = diop->xd_blkalgn;
	base = bp->b_un.b_addr;
	
	error = resid = totxfer = end_of_file = 0;
	offset = BBTOOFF((off_t)bp->b_blkno);
	totresid = count = bp->b_bcount;

	/*
 	 * Determine if this file is using the realtime volume.
	 */
	rt = (ip->i_d.di_flags & XFS_DIFLAG_REALTIME);

	/*
	 * Process the request until:
	 * 1) an I/O error occurs
	 * 2) end of file is reached.
	 * 3) end of device (driver error) occurs
	 * 4) request is completed.
	 */
	while (!error && !end_of_file && !resid && count) {
		offset_fsb = XFS_B_TO_FSBT(mp, offset);
		count_fsb  = XFS_B_TO_FSB(mp, count);

		tp = NULL;
		unwritten = 0;
retry:
		XFS_BMAP_INIT(&free_list, &firstfsb);
		/*
		 * Read requests will be issued 
		 * up to XFS_BMAP_MAX_MAP at a time.
		 */
		reccount = XFS_BMAP_MAX_NMAP;
		imapp = &imaps[0];
		CHECK_GRIO_TIMESTAMP(bp, 40);

		lock_mode = xfs_ilock_map_shared(ip);

		CHECK_GRIO_TIMESTAMP(bp, 40);

		/*
 		 * Issue the bmapi() call to get the extent info.
		 */
		CHECK_GRIO_TIMESTAMP(bp, 40);
		error = xfs_bmapi(tp, ip, offset_fsb, count_fsb, 
				  0, &firstfsb, 0, imapp,
				  &reccount, &free_list);
		CHECK_GRIO_TIMESTAMP(bp, 40);

		xfs_iunlock_map_shared(ip, lock_mode);
		if (error)
			break;

                /*
                 * xfs_bmapi() did not return an error but the 
 		 * reccount was zero. This means that a delayed write is
		 * in progress and it is necessary to call xfs_bmapi() again
		 * to map the correct portion of the file.
                 */
                if ((!error) && (reccount == 0)) {
			goto retry;
                }

		/*
   		 * Run through each extent.
		 */
		bufsissued = 0;
		for (i = 0; (i < reccount) && (!end_of_file) && (count);
		     i++) {
			imapp = &imaps[i];
			unwritten = !(imapp->br_state == XFS_EXT_NORM);

			bytes_this_req =
				XFS_FSB_TO_B(mp, imapp->br_blockcount) -
				BBTOB(blk_algn);

			ASSERT(bytes_this_req);

			offset_this_req =
				XFS_FSB_TO_B(mp, imapp->br_startoff) +
				BBTOB(blk_algn); 

			/*
			 * Reduce request size, if it
			 * is longer than user buffer.
			 */
			if (bytes_this_req > count) {
				 bytes_this_req = count;
			}

			/*
			 * Check if this is the end of the file.
			 */
			new_size = offset_this_req + bytes_this_req;
			if (new_size >ip->i_d.di_size) {
				/*
 			 	 * If trying to read past end of
 			 	 * file, shorten the request size.
				 */
				xfs_ilock(ip, XFS_ILOCK_SHARED);
				if (new_size > ip->i_d.di_size) {
				   if (ip->i_d.di_size > offset_this_req) {
					trail = ip->i_d.di_size - 
							offset_this_req;
					bytes_this_req = trail;
					bytes_this_req &= ~BBMASK;
					bytes_this_req += BBSIZE;
				   } else {
					bytes_this_req =  0;
				   }

				   end_of_file = 1;

				   if (!bytes_this_req) {
					xfs_iunlock(ip, XFS_ILOCK_SHARED);
					break;
				   }
				}
				xfs_iunlock(ip, XFS_ILOCK_SHARED);
			}

			/*
 			 * Do not do I/O if there is a hole in the file.
			 * Do not read if the blocks are unwritten.
			 */
			if ((imapp->br_startblock == HOLESTARTBLOCK) ||
			    unwritten) {
				/*
 				 * Physio() has already mapped user address.
				 */
				bzero(base, bytes_this_req);

				/*
				 * Bump the transfer count.
				 */
				if (trail) 
					totxfer += trail;
				else
					totxfer += bytes_this_req;
			} else {
				/*
 				 * Setup I/O request for this extent.
				 */
				CHECK_GRIO_TIMESTAMP(bp, 40);
	     			bps[bufsissued++]= nbp = getphysbuf(bp->b_edev);
				CHECK_GRIO_TIMESTAMP(bp, 40);

	     			nbp->b_flags     = bp->b_flags;
				nbp->b_grio_private = bp->b_grio_private;
								/* b_iopri */

	     			nbp->b_error     = 0;
				nbp->b_target    = bp->b_target;
				if (rt) {
	     				nbp->b_blkno = XFS_FSB_TO_BB(mp,
						imapp->br_startblock);
				} else {
	     				nbp->b_blkno = XFS_FSB_TO_DADDR(mp,
						imapp->br_startblock) + 
						blk_algn;
				}
				ASSERT(bytes_this_req);
	     			nbp->b_bcount    = bytes_this_req;
	     			nbp->b_un.b_addr = base;
				/*
 				 * Issue I/O request.
				 */
				CHECK_GRIO_TIMESTAMP(nbp, 40);
				(void) xfsbdstrat(mp, nbp);
				
		    		if (error = geterror(nbp)) {
					biowait(nbp);
					nbp->b_flags = 0;
		     			nbp->b_un.b_addr = 0;
					nbp->b_grio_private = 0; /* b_iopri */
					putphysbuf( nbp );
					bps[bufsissued--] = 0;
					break;
		     		}
			}

			/*
			 * update pointers for next round.
			 */

	     		base   += bytes_this_req;
	     		offset += bytes_this_req;
	     		count  -= bytes_this_req;
			blk_algn= 0;

		} /* end of for loop */

		/*
		 * Wait for I/O completion and recover buffers.
		 */
		for (j = 0; j < bufsissued ; j++) {
	  		nbp = bps[j];
	    		biowait(nbp);
			nbp->b_flags &= ~B_GR_BUF;	/* Why? B_PRV_BUF? */

	     		if (!error)
				error = geterror(nbp);

	     		if (!error && !resid) {
				resid = nbp->b_resid;

				/*
				 * prevent adding up partial xfers
				 */
				if (trail && (j == (bufsissued -1 ))) {
					if (resid <= (nbp->b_bcount - trail) )
						totxfer += trail;
				} else {
					totxfer += (nbp->b_bcount - resid);
				}
			} 
	    	 	nbp->b_flags		= 0;
	     		nbp->b_bcount		= 0;
	     		nbp->b_un.b_addr	= 0;
	     		nbp->b_grio_private	= 0; /* b_iopri */
	    	 	putphysbuf( nbp );
	     	}
	} /* end of while loop */

	/*
 	 * Fill in resid count for original buffer.
	 * if any of the io's fail, the whole thing fails
	 */
	if (error) {
		totxfer = 0;
	}

	bp->b_resid = totresid - totxfer;

	return (error);
}


/*
 * xfs_dio_write()
 *	This routine issues the calls to the disk device strategy routine
 *	for file system writes made using direct I/O from user space. The
 *	I/Os are issued one extent at a time.
 *
 * RETURNS:
 *	error
 */
STATIC int
xfs_dio_write(
	xfs_dio_t *diop)
{
	buf_t		*bp;
	struct dio_s	*dp;
	xfs_inode_t 	*ip;
	xfs_trans_t	*tp;
			/* REFERENCED */
	vnode_t		*vp;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	imaps[XFS_BMAP_MAX_NMAP], *imapp;
	buf_t		*nbp;
	xfs_fileoff_t	offset_fsb;
	xfs_fsblock_t	firstfsb;
	xfs_filblks_t	count_fsb, datablocks;
	xfs_bmap_free_t free_list;
	caddr_t		base;
	ssize_t		resid, count, totxfer;
	off_t		offset, offset_this_req, bytes_this_req;
	int		error, reccount, bmapi_flag, ioexcl;
	int		end_of_file, totresid, exist;
	int		blk_algn, rt, numrtextents, sbrtextsize, iprtextsize;
	int		committed, unwritten, using_quotas, nounwritten;
	xfs_fsize_t	new_size;
	int		nres;

	bp = diop->xd_bp;
	dp = diop->xd_dp;
	vp = diop->xd_vp;
	ip = diop->xd_ip;
	mp = diop->xd_mp;
	blk_algn = diop->xd_blkalgn;
	base = bp->b_un.b_addr;
	error = resid = totxfer = end_of_file = ioexcl = 0;
	offset = BBTOOFF((off_t)bp->b_blkno);
	numrtextents = iprtextsize = sbrtextsize = 0;
	totresid = count = bp->b_bcount;

	/*
 	 * Determine if this file is using the realtime volume.
	 */
	if ((rt = ip->i_d.di_flags & XFS_DIFLAG_REALTIME)) {
		sbrtextsize = mp->m_sb.sb_rextsize;
		iprtextsize =
			ip->i_d.di_extsize ? ip->i_d.di_extsize : sbrtextsize;
	}
	if (using_quotas = XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0)) 
				goto error0;
		}
	}
	nounwritten = XFS_SB_VERSION_HASEXTFLGBIT(&mp->m_sb) == 0;

	/*
	 * Process the request until:
	 * 1) an I/O error occurs
	 * 2) end of file is reached.
	 * 3) end of device (driver error) occurs
	 * 4) request is completed.
	 */
	while (!error && !end_of_file && !resid && count) {
		offset_fsb = XFS_B_TO_FSBT(mp, offset);
		count_fsb  = XFS_B_TO_FSB(mp, count);

		tp = NULL;
retry:
		XFS_BMAP_INIT(&free_list, &firstfsb);

		/*
 		 * We need to call bmapi() with the read flag set first to
		 * determine the existing 
		 * extents. This is done so that the correct amount
		 * of space can be reserved in the transaction 
		 * structure. Also, a check is needed to see if the
		 * extents are for valid blocks but also unwritten.
		 * If so, again a transaction needs to be reserved.
		 */
		reccount = 1;

		xfs_ilock(ip, XFS_ILOCK_EXCL);

		error = xfs_bmapi(NULL, ip, offset_fsb, 
				  count_fsb, 0, &firstfsb, 0, imaps, 
				  &reccount, 0);

		if (error) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			break;
		}

		/*
 		 * Get a pointer to the current extent map.
		 */
		imapp = &imaps[0];

		/*
		 * Check if the file extents already exist
		 */
		exist = imapp->br_startblock != DELAYSTARTBLOCK &&
			imapp->br_startblock != HOLESTARTBLOCK;

		reccount = 1;
		count_fsb = imapp->br_blockcount;

		/*
		 * If blocks are not yet allocated for this part of
		 * the file, allocate space for the transactions.
		 */
		if (!exist) {
			bmapi_flag = XFS_BMAPI_WRITE;
			if (rt) {
				/*
				 * Round up to even number of extents.
				 * Need the worst case, aligning the start
				 * offset down and the end offset up.
				 */
				xfs_fileoff_t	s, e;

				s = offset_fsb / iprtextsize;
				s *= iprtextsize;
				e = roundup(offset_fsb + count_fsb,
					    iprtextsize);
				numrtextents = (e - s) / sbrtextsize;
				datablocks = 0;
			} else {
				/*
				 * If this is a write to the data
				 * partition, reserve the space.
				 */
				datablocks = count_fsb;
			}

			/*
 			 * Setup transaction.
 			 */
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			if (rt && nounwritten && !ioexcl) {
				xfs_iunlock(ip, XFS_IOLOCK_SHARED);
				xfs_ilock(ip, XFS_IOLOCK_EXCL);
				ioexcl = 1;
				goto retry;
			}
			tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);

			nres = XFS_DIOSTRAT_SPACE_RES(mp, datablocks);
			error = xfs_trans_reserve(tp, nres,
				   XFS_WRITE_LOG_RES(mp), numrtextents,
				   XFS_TRANS_PERM_LOG_RES,
				   XFS_WRITE_LOG_COUNT );
			xfs_ilock(ip, XFS_ILOCK_EXCL);

			if (error) {
				/*
				 * Ran out of file system space.
				 * Free the transaction structure.
				 */
				ASSERT(error == ENOSPC || 
				       XFS_FORCED_SHUTDOWN(mp));
				xfs_trans_cancel(tp, 0);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				break;
			} 
			/* 
			 * quota reservations
			 */
			if (using_quotas &&
			    xfs_trans_reserve_blkquota(tp, ip, nres)) {
				error = XFS_ERROR(EDQUOT);
				xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				break;
			}
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);

			if (offset < ip->i_d.di_size || rt)
				bmapi_flag |= XFS_BMAPI_PREALLOC;

			/*
 			 * Issue the bmapi() call to do actual file
			 * space allocation.
			 */
			CHECK_GRIO_TIMESTAMP(bp, 40);
			error = xfs_bmapi(tp, ip, offset_fsb, count_fsb, 
				  bmapi_flag, &firstfsb, nres, imapp, &reccount,
				  &free_list);
			CHECK_GRIO_TIMESTAMP(bp, 40);

			if (error) 
				goto error_on_bmapi_transaction;

			/*
	 		 * Complete the bmapi() allocations transactions.
			 * The bmapi() unwritten to written changes will
			 * be committed after the writes are completed.
			 */
		    	error = xfs_bmap_finish(&tp, &free_list,
					    firstfsb, &committed);
			if (error) 
				goto error_on_bmapi_transaction;

			xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES,
				     NULL);
		} else if (ioexcl) {
			xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
			ioexcl = 0;
		}
		xfs_iunlock(ip, XFS_ILOCK_EXCL);

                /*
                 * xfs_bmapi() did not return an error but the 
 		 * reccount was zero. This means that a delayed write is
		 * in progress and it is necessary to call xfs_bmapi() again
		 * to map the correct portion of the file.
                 */
                if ((!error) && (reccount == 0)) {
			if (ioexcl) {
				xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
				ioexcl = 0;
			}
			goto retry;
                }

		imapp = &imaps[0];
		unwritten = imapp->br_state != XFS_EXT_NORM;

		bytes_this_req = XFS_FSB_TO_B(mp, imapp->br_blockcount) -
				BBTOB(blk_algn);

		ASSERT(bytes_this_req);

		offset_this_req = XFS_FSB_TO_B(mp, imapp->br_startoff) +
				BBTOB(blk_algn); 

		/*
		 * Reduce request size, if it
		 * is longer than user buffer.
		 */
		if (bytes_this_req > count) {
			 bytes_this_req = count;
		}

		/*
		 * Check if this is the end of the file.
		 */
		new_size = offset_this_req + bytes_this_req;
		if (new_size >ip->i_d.di_size) {
			/*
			 * File is being extended on a
			 * write, update the file size if
			 * someone else didn't make it even
			 * bigger.
			 */
	         	ASSERT((vp->v_flag & VISSWAP) == 0);
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			if (new_size > ip->i_d.di_size) {
		 		ip->i_d.di_size = offset_this_req + 
							bytes_this_req;
				ip->i_update_core = 1;
				ip->i_update_size = 1;
			}
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		}

		/*
		 * For realtime extents in filesystems that don't support
		 * unwritten extents we need to zero the part of the
		 * extent we're not writing.  If unwritten extents are
		 * supported the transaction after the write will leave
		 * the unwritten piece of the extent marked as such.
		 */
		if (ioexcl) {
			ASSERT(!unwritten);
			offset_fsb = XFS_B_TO_FSBT(mp, offset_this_req);
			count_fsb = XFS_B_TO_FSB(mp, bytes_this_req);
			error = xfs_dio_write_zero_rtarea(ip, bp, offset_fsb,
					count_fsb);
			xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
			ioexcl = 0;
			if (error)
				goto error0;
		}

		/*
 		 * Setup I/O request for this extent.
		 */
		CHECK_GRIO_TIMESTAMP(bp, 40);
		nbp = getphysbuf(bp->b_edev);
		CHECK_GRIO_TIMESTAMP(bp, 40);

	     	nbp->b_flags     = bp->b_flags;
		nbp->b_grio_private = bp->b_grio_private;
						/* b_iopri */

	     	nbp->b_error     = 0;
		nbp->b_target    = bp->b_target;
		nbp->b_blkno	 = XFS_FSB_TO_DB(ip, imapp->br_startblock) +
				   blk_algn;
		ASSERT(bytes_this_req);
	     	nbp->b_bcount    = bytes_this_req;
	     	nbp->b_un.b_addr = base;
		/*
 		 * Issue I/O request.
		 */
		CHECK_GRIO_TIMESTAMP(nbp, 40);
		(void) xfsbdstrat(mp, nbp);

    		if ((error = geterror(nbp)) == 0) {

			/*
			 * update pointers for next round.
			 */

     			base   += bytes_this_req;
     			offset += bytes_this_req;
     			count  -= bytes_this_req;
			blk_algn = 0;
     		}

		/*
		 * Wait for I/O completion and recover buffer.
		 */
		biowait(nbp);
		nbp->b_flags &= ~B_GR_BUF;	/* Why? B_PRV_BUF? */

		if (!error)
			error = geterror(nbp);

		if (!error && !resid) {
			resid = nbp->b_resid;

			/*
			 * prevent adding up partial xfers
			 */
			totxfer += (nbp->b_bcount - resid);
		} 
 		nbp->b_flags		= 0;
		nbp->b_bcount		= 0;
		nbp->b_un.b_addr	= 0;
		nbp->b_grio_private	= 0;	/* b_iopri */
 		putphysbuf( nbp );
		if (error)
			break;
		
		if (unwritten) {
			offset_fsb = XFS_B_TO_FSBT(mp, offset_this_req);
			count_fsb = XFS_B_TO_FSB(mp, bytes_this_req);
			/*
			 * Set up the xfs_bmapi() call to change the 
			 * extent from unwritten to written.
			 */
			tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);

			nres = XFS_DIOSTRAT_SPACE_RES(mp, 0);
			error = xfs_trans_reserve(tp, nres,
				   XFS_WRITE_LOG_RES(mp), 0,
				   XFS_TRANS_PERM_LOG_RES,
				   XFS_WRITE_LOG_COUNT );
			xfs_ilock(ip, XFS_ILOCK_EXCL);

			if (error) {
				/*
				 * Ran out of file system space.
				 * Free the transaction structure.
				 */
				ASSERT(error == ENOSPC || 
				       XFS_FORCED_SHUTDOWN(mp));
				xfs_trans_cancel(tp, 0);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				break;
			} 
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);

			/*
 			 * Issue the bmapi() call to change the extents
			 * to written.
			 */
			reccount = 1;
			CHECK_GRIO_TIMESTAMP(bp, 40);
			error = xfs_bmapi(tp, ip, offset_fsb, count_fsb, 
				  XFS_BMAPI_WRITE, &firstfsb, nres, imapp,
				  &reccount, &free_list);
			CHECK_GRIO_TIMESTAMP(bp, 40);

			if (error) 
				goto error_on_bmapi_transaction;

			/*
	 		 * Complete the bmapi() allocations transactions.
			 * The bmapi() unwritten to written changes will
			 * be committed after the writes are completed.
			 */
		    	error = xfs_bmap_finish(&tp, &free_list,
					    firstfsb, &committed);
			if (error) 
				goto error_on_bmapi_transaction;

			xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES,
				     NULL);
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		}
	} /* end of while loop */
	
	/*
 	 * Fill in resid count for original buffer.
	 * if any of the io's fail, the whole thing fails
	 */
	if (error) {
		totxfer = 0;
	}

	bp->b_resid = totresid - totxfer;

	/*
 	 *  Update the inode timestamp.
 	 */
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if ((ip->i_d.di_mode & (ISUID|ISGID)) &&
	    !cap_able_cred(dp->cr, CAP_FSETID)) {
		ip->i_d.di_mode &= ~ISUID;
		/*
		 * Note that we don't have to worry about mandatory
		 * file locking being disabled here because we only
		 * clear the ISGID bit if the Group execute bit is
		 * on, but if it was on then mandatory locking wouldn't
		 * have been enabled.
		 */
		if (ip->i_d.di_mode & (IEXEC >> 3))
			ip->i_d.di_mode &= ~ISGID;
	}
	xfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

 error0:
	if (ioexcl)
		xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
	return (error);

 error_on_bmapi_transaction:
	xfs_bmap_cancel(&free_list);
	xfs_trans_cancel(tp, (XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT));
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	goto error0;
}


/*
 * xfs_diostrat()
 *	This routine issues the calls to the disk device strategy routine
 *	for file system reads and writes made using direct I/O from user
 *	space. In the case of a write request the I/Os are issued one 
 *	extent at a time. In the case of a read request I/Os for each extent
 *	involved are issued at once.
 *
 * RETURNS:
 *	none
 */
int
xfs_diostrat(
	buf_t	*bp)
{
	xfs_dio_t	dio, *diop = &dio;
	struct dio_s	*dp;
	xfs_inode_t 	*ip;
	vnode_t		*vp;
	bhv_desc_t	*bdp;
	xfs_mount_t	*mp;
	off_t		offset;
	int		error;

	CHECK_GRIO_TIMESTAMP(bp, 40);

	dio.xd_bp = bp;
	dio.xd_dp = dp = (struct dio_s *)bp->b_private;
	bdp = dp->bdp;
	dio.xd_vp = vp = BHV_TO_VNODE(bdp);
	dio.xd_ip = ip = XFS_BHVTOI(bdp);
	dio.xd_mp = mp = ip->i_mount;
	
	offset = BBTOOFF((off_t)bp->b_blkno);
	ASSERT(!(bp->b_flags & B_DONE));
        ASSERT(ismrlocked(&ip->i_iolock, MR_ACCESS| MR_UPDATE) != 0);

	/*
 	 * Check if the request is on a file system block boundary.
	 */
	dio.xd_blkalgn = ((offset & mp->m_blockmask) != 0) ? 
		 		OFFTOBB(offset & mp->m_blockmask) : 0;

	/*
	 * We're going to access the disk directly.
	 * Blow anything in the range of the request out of the
	 * buffer cache first.  This isn't perfect because we allow
	 * simultaneous direct I/O writers and buffered readers, but
	 * it should be good enough.
	 */
	if (!(dp->ioflag & IO_IGNCACHE) && VN_CACHED(vp)) {
		xfs_inval_cached_pages(ip, offset, bp->b_bcount, dp);
	}

	/*
	 * Alignment checks are done in xfs_diordwr().
	 * Determine if the operation is a read or a write.
	 */
	if (bp->b_flags & B_READ) {
		error = xfs_dio_read(diop);
	} else {
		error = xfs_dio_write(diop);
	}

	/*
	 * Issue completion on the original buffer.
	 */
	bioerror(bp, error);
	biodone(bp);

        ASSERT(ismrlocked(&ip->i_iolock, MR_ACCESS| MR_UPDATE) != 0);

	return (0);
}

/*
 * xfs_diordwr()
 *	This routine sets up a buf structure to be used to perform 
 * 	direct I/O operations to user space. The user specified 
 *	parameters are checked for alignment and size limitations. A buf
 *	structure is allocated an biophysio() is called.
 *
 * RETURNS:
 * 	 0 on success
 * 	errno on error
 */
int
xfs_diordwr(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	uint64_t	rw)
{
	extern 		zone_t	*grio_buf_data_zone;

	xfs_inode_t	*ip;
	vnode_t		*vp;
	struct dio_s	dp;
	xfs_mount_t	*mp;
	uuid_t		stream_id;
	buf_t		*bp;
	int		error, index;
	__int64_t	iosize;
	extern int	scache_linemask;
	int		guartype = -1;

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;
	xfs_rw_enter_trace(rw & B_READ ? XFS_DIORD_ENTER : XFS_DIOWR_ENTER,
		ip, uiop, ioflag);

	/*
 	 * Check that the user buffer address is on a secondary cache
	 * line offset, while file offset and
 	 * request size are both multiples of file system block size. 
	 * This prevents the need for read/modify/write operations.
	 *
	 * This enforces the alignment restrictions indicated by 
 	 * the F_DIOINFO fcntl call.
	 *
	 * We make an exception for swap I/O and trusted clients like
	 * cachefs.  Swap I/O will always be page aligned and all the
	 * blocks will already be allocated, so we don't need to worry
	 * about read/modify/write stuff.  Cachefs ensures that it only
	 * reads back data which it has written, so we don't need to
	 * worry about block zeroing and such.
 	 */
	if (!(vp->v_flag & VISSWAP) && !(ioflag & IO_TRUSTEDDIO) &&
	    ((((long)(uiop->uio_iov->iov_base)) & scache_linemask) ||
	     (uiop->uio_offset & mp->m_blockmask) ||
	     (uiop->uio_resid & mp->m_blockmask))) {

		/*
		 * if the user tries to start reading at the
		 * end of the file, just return 0.
		 */
		if ((rw & B_READ) &&
		    (uiop->uio_offset == ip->i_d.di_size)) {
			return (0);
		}
		return XFS_ERROR(EINVAL);
	}
	/*
	 * This ASSERT should catch bad addresses being passed in by
	 * trusted callers.
	 */
	ASSERT(!(((long)(uiop->uio_iov->iov_base)) & scache_linemask));

	/*
 	 * Do maxio check.
 	 */
	if (uiop->uio_resid > ctooff(v.v_maxdmasz - 1)) {
		return XFS_ERROR(EINVAL);
	}

	/*
 	 * Use dio_s structure to pass file/credential
	 * information to file system strategy routine.
	 */
	dp.bdp = bdp;
	dp.cr = credp;
	dp.ioflag = ioflag;
	dp.uio_pmp = uiop->uio_pmp;

	/*
 	 * Allocate local buf structure.
	 */
	if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
		bp = getphysbuf(mp->m_rtdev);
		bp->b_target = &mp->m_rtdev_targ;
	} else {
		bp = getphysbuf(mp->m_dev);
		bp->b_target = mp->m_ddev_targp;
	}
	bp->b_private = &dp;

	bp->b_grio_private = NULL;		/* b_iopri = 0 */
	bp->b_flags &= ~(B_GR_BUF|B_PRV_BUF);	/* lo pri queue */

	/*
	 * Check if this is a guaranteed rate I/O
	 */
	if (ioflag & IO_PRIORITY) {

		guartype = grio_io_is_guaranteed(uiop->uio_fp, &stream_id);

		/*
		 * Get priority level if this is a multilevel request.
		 * The level is stored in b_iopri, except if the request
		 * is controlled by griostrategy.
		 */
		if (uiop->uio_fp->vf_flag & FPRIO) {
			bp->b_flags |= B_PRV_BUF;
			VFILE_GETPRI(uiop->uio_fp, bp->b_iopri);
			/*
			 * Take care of some other thread racing
			 * and clearing FPRIO.
			 */
			if (bp->b_iopri == 0) 
				bp->b_flags &= ~B_PRV_BUF;
		}

		if (guartype == -1) {
			/*
			 * grio is not configed into kernel, but FPRIO
			 * is set.
			 */
		} else if (guartype) {

			short prval = bp->b_iopri;

			bp->b_flags |= B_GR_BUF;
			ASSERT((bp->b_grio_private == NULL) || 
						(bp->b_flags & B_PRV_BUF));
			bp->b_grio_private = 
				kmem_zone_alloc(grio_buf_data_zone, KM_SLEEP);
			ASSERT(BUF_GRIO_PRIVATE(bp));
			COPY_STREAM_ID(stream_id,BUF_GRIO_PRIVATE(bp)->grio_id);
			SET_GRIO_IOPRI(bp, prval);
			iosize =  uiop->uio_iov[0].iov_len;
			index = grio_monitor_io_start(&stream_id, iosize);
			INIT_GRIO_TIMESTAMP(bp);
		} else {
			/*
			 * FPRIORITY|FPRIO was set when we looked,
			 * but FPRIORITY is not set anymore.
			 */
		}
	}

	/*
 	 * Perform I/O operation.
	 */
	error = biophysio(xfs_diostrat, bp, bp->b_edev, rw, 
		(daddr_t)OFFTOBB(uiop->uio_offset), uiop);

	/*
 	 * Free local buf structure.
 	 */
	if (ioflag & IO_PRIORITY) {
		bp->b_flags &= ~(B_PRV_BUF|B_GR_BUF);
		if (guartype > 0) {
			grio_monitor_io_end(&stream_id, index);
#ifdef GRIO_DEBUG
			CHECK_GRIO_TIMESTAMP(bp, 400);
#endif
			ASSERT(BUF_GRIO_PRIVATE(bp));
			kmem_zone_free(grio_buf_data_zone, BUF_GRIO_PRIVATE(bp));
		}
		bp->b_grio_private = NULL;
	}

	ASSERT((bp->b_flags & B_MAPPED) == 0);
	bp->b_flags = 0;
	bp->b_un.b_addr = 0;
	putphysbuf(bp);

	return (error);
}




lock_t		xfs_refcache_lock;
xfs_inode_t	**xfs_refcache;
int		xfs_refcache_size;
int		xfs_refcache_index;
int		xfs_refcache_busy;
int		xfs_refcache_count;

/*
 * Insert the given inode into the reference cache.
 */
void
xfs_refcache_insert(
	xfs_inode_t	*ip)
{
	int		s;
	vnode_t		*vp;
	xfs_inode_t	*release_ip;
	xfs_inode_t	**refcache;

	ASSERT(ismrlocked(&(ip->i_iolock), MR_UPDATE));

	/*
	 * If an unmount is busy blowing entries out of the cache,
	 * then don't bother.
	 */
	if (xfs_refcache_busy) {
		return;
	}

	/*
	 * The inode is already in the refcache, so don't bother
	 * with it.
	 */
	if (ip->i_refcache != NULL) {
		return;
	}

	vp = XFS_ITOV(ip);
	ASSERT(vp->v_count > 0);
	VN_HOLD(vp);

	/*
	 * We allocate the reference cache on use so that we don't
	 * waste the memory on systems not being used as NFS servers.
	 */
	if (xfs_refcache == NULL) {
		refcache = (xfs_inode_t **)kmem_zalloc(xfs_refcache_size *
						       sizeof(xfs_inode_t *),
						       KM_SLEEP);
	} else {
		refcache = NULL;
	}

	s = mp_mutex_spinlock(&xfs_refcache_lock);

	/*
	 * If we allocated memory for the refcache above and it still
	 * needs it, then use the memory we allocated.  Otherwise we'll
	 * free the memory below.
	 */
	if (refcache != NULL) {
		if (xfs_refcache == NULL) {
			xfs_refcache = refcache;
			refcache = NULL;
		}
	}

	/*
	 * If an unmount is busy clearing out the cache, don't add new
	 * entries to it.
	 */
	if ((xfs_refcache_busy) || (vp->v_vfsp->vfs_flag & VFS_OFFLINE)) {
		mp_mutex_spinunlock(&xfs_refcache_lock, s);
		VN_RELE(vp);
		/*
		 * If we allocated memory for the refcache above but someone
		 * else beat us to using it, then free the memory now.
		 */
		if (refcache != NULL) {
			kmem_free(refcache,
				  xfs_refcache_size * sizeof(xfs_inode_t *));
		}
		return;
	}
	release_ip = xfs_refcache[xfs_refcache_index];
	if (release_ip != NULL) {
		release_ip->i_refcache = NULL;
		xfs_refcache_count--;
		ASSERT(xfs_refcache_count >= 0);
	}
	xfs_refcache[xfs_refcache_index] = ip;
	ASSERT(ip->i_refcache == NULL);
	ip->i_refcache = &(xfs_refcache[xfs_refcache_index]);
	xfs_refcache_count++;
	ASSERT(xfs_refcache_count <= xfs_refcache_size);
	xfs_refcache_index++;
	if (xfs_refcache_index == xfs_refcache_size) {
		xfs_refcache_index = 0;
	}
	mp_mutex_spinunlock(&xfs_refcache_lock, s);

	/*
	 * Save the pointer to the inode to be released so that we can
	 * VN_RELE it once we've dropped our inode locks in xfs_rwunlock().
	 * The pointer may be NULL, but that's OK.
	 */
	ip->i_release = release_ip;

	/*
	 * If we allocated memory for the refcache above but someone
	 * else beat us to using it, then free the memory now.
	 */
	if (refcache != NULL) {
		kmem_free(refcache,
			  xfs_refcache_size * sizeof(xfs_inode_t *));
	}
	return;
}


/*
 * If the given inode is in the reference cache, purge its entry and
 * release the reference on the vnode.
 */
void
xfs_refcache_purge_ip(
	xfs_inode_t	*ip)
{
	int	s;
	vnode_t	*vp;

	/*
	 * If we're not pointing to our entry in the cache, then
	 * we must not be in the cache.
	 */
	if (ip->i_refcache == NULL) {
		return;
	}

	s = mp_mutex_spinlock(&xfs_refcache_lock);
	if (ip->i_refcache == NULL) {
		mp_mutex_spinunlock(&xfs_refcache_lock, s);
		return;
	}

	/*
	 * Clear both our pointer to the cache entry and its pointer
	 * back to us.
	 */
	ASSERT(*(ip->i_refcache) == ip);
	*(ip->i_refcache) = NULL;
	ip->i_refcache = NULL;
	xfs_refcache_count--;
	ASSERT(xfs_refcache_count >= 0);
	mp_mutex_spinunlock(&xfs_refcache_lock, s);

	vp = XFS_ITOV(ip);
	ASSERT(vp->v_count > 1);
	VN_RELE(vp);

	return;
}


/*
 * This is called from the XFS unmount code to purge all entries for the
 * given mount from the cache.  It uses the refcache busy counter to
 * make sure that new entries are not added to the cache as we purge them.
 */
void
xfs_refcache_purge_mp(
	xfs_mount_t	*mp)
{
	int		s;
	vnode_t		*vp;
	int		i;
	xfs_inode_t	*ip;

	if (xfs_refcache == NULL) {
		return;
	}

	s = mp_mutex_spinlock(&xfs_refcache_lock);
	/*
	 * Bumping the busy counter keeps new entries from being added
	 * to the cache.  We use a counter since multiple unmounts could
	 * be in here simultaneously.
	 */
	xfs_refcache_busy++;

	for (i = 0; i < xfs_refcache_size; i++) {
		ip = xfs_refcache[i];
		if ((ip != NULL) && (ip->i_mount == mp)) {
			xfs_refcache[i] = NULL;
			ip->i_refcache = NULL;
			xfs_refcache_count--;
			ASSERT(xfs_refcache_count >= 0);
			mp_mutex_spinunlock(&xfs_refcache_lock, s);
			vp = XFS_ITOV(ip);
			VN_RELE(vp);

			s = mp_mutex_spinlock(&xfs_refcache_lock);
		} else {
			/*
			 * Make sure the don't hold the lock for too long.
			 */
			if ((i & 15) == 0) {
				mp_mutex_spinunlock(&xfs_refcache_lock, s);
				s = mp_mutex_spinlock(&xfs_refcache_lock);
			}
		}
	}

	xfs_refcache_busy--;
	ASSERT(xfs_refcache_busy >= 0);
	mp_mutex_spinunlock(&xfs_refcache_lock, s);
}


/*
 * This is called from the XFS sync code to ensure that the refcache
 * is emptied out over time.  We purge a small number of entries with
 * each call.
 */
void
xfs_refcache_purge_some(void)
{
	int		s;
	int		i;
	xfs_inode_t	*ip;
	int		iplist_index;
#define	XFS_REFCACHE_PURGE_COUNT	10
	xfs_inode_t	*iplist[XFS_REFCACHE_PURGE_COUNT];

	if ((xfs_refcache == NULL) || (xfs_refcache_count == 0)) {
		return;
	}

	iplist_index = 0;
	s = mp_mutex_spinlock(&xfs_refcache_lock);

	/*
	 * Store any inodes we find in the next several entries
	 * into the iplist array to be released after dropping
	 * the spinlock.  We always start looking from the currently
	 * oldest place in the cache.  We move the refcache index
	 * forward as we go so that we are sure to eventually clear
	 * out the entire cache when the system goes idle.
	 */
	for (i = 0; i < XFS_REFCACHE_PURGE_COUNT; i++) {
		ip = xfs_refcache[xfs_refcache_index];
		if (ip != NULL) {
			xfs_refcache[xfs_refcache_index] = NULL;
			ip->i_refcache = NULL;
			xfs_refcache_count--;
			ASSERT(xfs_refcache_count >= 0);
			iplist[iplist_index] = ip;
			iplist_index++;
		}
		xfs_refcache_index++;
		if (xfs_refcache_index == xfs_refcache_size) {
			xfs_refcache_index = 0;
		}
	}

	mp_mutex_spinunlock(&xfs_refcache_lock, s);

	/*
	 * Now drop the inodes we collected.
	 */
	for (i = 0; i < iplist_index; i++) {
		VN_RELE(XFS_ITOV(iplist[i]));
	}
}
