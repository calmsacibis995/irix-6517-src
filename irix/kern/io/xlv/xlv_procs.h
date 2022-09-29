#ifndef __XLV_PROCS_H__
#define __XLV_PROCS_H__

/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.22 $"

/*
 * Declarations for externally callable xlv functions.
 */

/*
 * The io_lock macros are temporarily defined in <sys/xlv_tab.h>
 * because xfs_vfsops needs it. This will be fixed when we support
 * the ioctl() that does what vfsops currently does inline.
 */

void xlv_io_lock_init(int sv);
int  xlv_acquire_io_lock(int sv, buf_t *bp);
void xlv_release_io_lock(int sv);
int  xlv_block_io(int sv, buf_t *bp);
void xlv_unblock_io(int sv);
void xlv_acquire_cfg_lock(int sv);
void xlv_release_cfg_lock(int sv);

#ifdef DEBUG
#define XLV_IO_LOCKED(sv) (xlv_io_lock[sv].io_count)
#define XLV_IO_LOCKED_OR_RECOVERING(sv) \
	(xlv_io_lock[sv].io_count + xlv_io_lock[sv].error_count)
#define XLV_RECOVERING(sv) \
	(xlv_io_lock[sv].io_count == 0 && xlv_io_lock[sv].error_count)
#define XLV_CFG_LOCKED(sv) \
	(xlv_io_lock[sv].io_count == 0 && xlv_io_lock[sv].error_count == 0 \
	 && xlv_io_lock[sv].wait_count)
#endif

typedef int     (*XLV_VE_FP)(xlv_tab_vol_elmnt_t *, void *);

extern void xlv_for_each_ve (xlv_tab_subvol_t *sv, XLV_VE_FP operation,
        void *arg);
extern int xlvstrategy(struct buf *bp);
extern int xlv_split_among_plexes (xlv_tab_subvol_t *xlv_p,
	struct buf *original_bp, xlv_slice_t slices_to_use[]);
extern xlv_tab_vol_elmnt_t * xlv_vol_elmnt_from_disk_part (
        xlv_tab_subvol_t  *sv, dev_t dev);
extern void xlvd_queue_request (struct buf *bp, xlv_io_context_t *io_context);
extern void xlvd_complete_mmap_write_request (xlv_io_context_t *io_context);

#ifdef _KERNEL
extern xlv_block_map_t *xlv_tab_create_block_map (xlv_tab_subvol_t *sv,
	xlv_block_map_t *block_map, ve_entry_t *ve_array);
extern int xlv_recalculate_blockmap(xlv_tab_subvol_t *svp, 
	xlv_block_map_t **block_map_p, ve_entry_t **ve_array_p);
#else
extern xlv_block_map_t *xlv_tab_create_block_map (xlv_tab_subvol_t *sv);
#endif

extern unsigned xlv_tab_subvol_trksize (xlv_tab_subvol_t *subvol_entry);
extern int xlv_tab_merge (xlv_tab_t *new_xlv_tab, xlv_tab_t *old_xlv_tab,
               int num_subvols, xlv_pdev_list_t);
extern int xlv_tab_close_devs (xlv_tab_t *old_xlv_tab, int num_subvols);
extern void xlv_tab_free (xlv_tab_vol_t *vtab, xlv_tab_t *svtab);


extern daddr_t xlv_tab_subvol_size (xlv_tab_subvol_t *subvol_entry);
extern unsigned xlv_tab_set (xlv_tab_vol_t *u_vtab, xlv_tab_t *u_svtab,
	int from_user_space);
extern int xlv_tab_alloc_reserved_buffers (
        unsigned max_top_bufs_needed, unsigned max_low_bufs_needed, 
	const boolean_t can_shrink);
extern unsigned xlv_tab_low_bufs_for_plex (xlv_tab_plex_t *plexp);
extern unsigned xlv_tab_max_low_bufs (xlv_tab_subvol_t *subvol_entry);
extern unsigned xlv_tab_max_top_bufs (xlv_tab_subvol_t *subvol_entry);

extern unsigned xlv_tab_copyin (xlv_tab_vol_t *u_vtab, xlv_tab_t *u_svtab,
             xlv_tab_vol_t **O_k_vtab, xlv_tab_t **O_k_svtab,
             unsigned *O_max_top_bufs_needed, unsigned *O_max_low_bufs_needed,
	     int from_user_space);

extern int xlv_copy_plexes (dev_t dev, daddr_t start_blkno, daddr_t end_blkno,
	unsigned chunksize, unsigned sleep_intvl_msec);
extern void xlv_setup_plex_rd_wr_back_region (xlv_tab_subvol_t *sv);

extern int xlv_mmap_write_init (buf_t *original_bp, buf_t **new_bp);
extern int xlv_mmap_write_next (buf_t *bp);
extern void xlv_mmap_write_done (buf_t *bp);

extern unsigned xlv_first_bit_set_in[];
#define FIRST_BIT_SET(c) (xlv_first_bit_set_in[(unsigned)c])

#ifdef DEBUG
#define XLV_TRACE_USER_BUF      0x0
#define XLV_TRACE_LOW_BUF       0x1     /* internal to xlv_lower_strat */
#define XLV_TRACE_TOP_BUF       0x2     /* internal to xlv_top_strat */
#define XLV_TRACE_ADMIN		0x3
#define XLV_TRACE_DISK_BUF	0x4	/* disk buffer during failover */
extern void xlv_trace (char *id, int bp_type, buf_t *bp);
extern void xlv_lock_trace (char *id, int bp_type, buf_t *bp);

#else
#define xlv_trace(id, bp_type, bp)
#define xlv_lock_trace(id, bp_type, bp)
#endif /* DEBUG */

#endif /* __XLV_PROCS_H__ */
