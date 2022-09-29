#ifndef	_XFS_LOG_RECOVER_H
#define _XFS_LOG_RECOVER_H

#ident	"$Revision: 1.11 $"

/*
 * Macros, structures, prototypes for internal log manager use.
 */

#define XLOG_RHASH_BITS  4
#define XLOG_RHASH_SIZE	16
#define XLOG_RHASH_SHIFT 2
#define XLOG_RHASH(tid)	\
	((((__uint32_t)tid)>>XLOG_RHASH_SHIFT) & (XLOG_RHASH_SIZE-1))

#define XLOG_MAX_REGIONS_IN_ITEM   (XFS_MAX_BLOCKSIZE / XFS_BLI_CHUNK / 2 + 1)


/*
 * item headers are in ri_buf[0].  Additional buffers follow.
 */
typedef struct xlog_recover_item {
	struct xlog_recover_item *ri_next;
	struct xlog_recover_item *ri_prev;
	int			 ri_type;
	int			 ri_cnt;	/* count of regions found */
	int			 ri_total;	/* total regions */
	xfs_log_iovec_t		 *ri_buf;	/* ptr to regions buffer */
} xlog_recover_item_t;

struct xlog_tid;
typedef struct xlog_recover {
	struct xlog_recover *r_next;
	xlog_tid_t	    r_log_tid;		/* log's transaction id */
	xfs_trans_header_t  r_theader;		/* trans header for partial */
	int		    r_state;		/* not needed */
	xfs_lsn_t	    r_lsn;		/* xact lsn */
	xlog_recover_item_t *r_itemq;		/* q for items */
} xlog_recover_t;

#define ITEM_TYPE(i)	(*(ushort *)(i)->ri_buf[0].i_addr)

/*
 * This is the number of entries in the l_buf_cancel_table used during
 * recovery.
 */
#define	XLOG_BC_TABLE_SIZE	64

#define	XLOG_RECOVER_PASS1	1
#define	XLOG_RECOVER_PASS2	2

#endif /* _XFS_LOG_RECOVER_H */
