/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994 Silicon Graphics, Inc.  	          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __XLV_IOCTX_H__
#define __XLV_IOCTX_H__

#ident  "$Revision: 1.11 $"

/*
 * State machine declaration for xlv driver.
 */

/*
 * States
 */
#define XLV_SM_INVALID			0
#define XLV_SM_INIT			1

#define XLV_SM_READ			2
#define XLV_SM_REREAD			3
#define XLV_SM_REWRITE			4
#define XLV_SM_WAIT_FOR_OFFLINE		5

#define XLV_SM_WRITE			6
#define XLV_SM_DISK_OFFLINE_DONE	7

#define XLV_SM_READ_WRITEBACK		8
#define XLV_SM_WRITEBACK		9
#define XLV_SM_REREAD_WRITEBACK		10

#define XLV_SM_NOTIFY_VES_ACTIVE	11

#if 0	/* PLEX_MMAP_COPY: obsolete code */
#define XLV_SM_MMAP_WRITE		12
#define XLV_SM_MMAP_WRITE_CONT		13
#define XLV_SM_MMAP_WRITE_CONT_ERROR	14
#endif /* PLEX_MMAP_COPY*/

#define XLV_SM_UNBLOCK_DRIVER		15

#define XLV_SM_NOTIFY_VES_OFFLINE	16

#define XLV_SM_NOTIFY_CONFIG_CHANGE     17

#define XLV_SM_END			18

/*
 * Values for io_context->flags.
 */
#define XLV_IOCTX_FLAG_ERR_PROCESS	0x01	/* this request needs to
					      clear xlvd_in_error_processing
					      when done. */

/*
 * Plex I/O context structure. This is hung off the headbuffer (via b_sort)
 * to describe the state of the I/O when there are plexes.
 */
typedef struct {
        struct buf              *original_buf;  /* original request issued
                                                   to xlv from above. */
        struct buf               retry_buf;     /* buf used to perform error
                                                   retries. */
        xlvmem_t                *buffers;       /* chunk of memory used for
                                                   buffers created by upper
                                                   xlv driver. */
        unsigned char           state;
	unsigned char		flags;
	unsigned char		rsvd;

        unsigned char           num_failures;
        xlv_failure_record_t    failures[XLV_MAX_FAILURES];

        xlv_blk_map_history_t   current_slices; /* slices used for this I/O */
        xlv_blk_map_history_t   failed_slices;  /* slices on which we
                                        experienced failures during this I/O */
} xlv_io_context_t;


/*
 * NOTE: 1) These defines are only for bp's headed for the disk!
 *	 2) b_fsprivate2 is used in two ways:
 *	    for bp's actively failing over, it holds the failover handle;
 *	    for bp's deferred, it chains the deferred bp list.
 *	    Since a bp can only be in one of these states, it works.
 */
#define FO_DISK_PART(bp) ((xlv_tab_disk_part_t *) (bp)->b_fsprivate)
#define FO_DISK_PART_L(bp) (bp)->b_fsprivate
#define FO_NEXT_DBP(bp) ((buf_t *) (bp)->b_fsprivate2) /* next deferred bp */
#define FO_NEXT_DBP_L(bp) (bp)->b_fsprivate2 /* next deferred bp */
#define FO_HANDLE(bp) ((bp)->b_fsprivate2) /* failover handle */
#define FO_NEXT_PATH(dp) ((dp)->retry_path == (dp)->n_paths - 1 ? \
			  0 : (dp)->retry_path + 1)
#define FO_NO_MORE_PATHS(dp) (FO_NEXT_PATH(dp) == (dp)->active_path)

/* failover states */
#define XLVD_FAILING_OVER(bp) ((bp)->b_flags & B_XLVD_FAILOVER)
#define XLVD_SET_FAILING_OVER(bp) ((bp)->b_flags |= B_XLVD_FAILOVER)
#define XLVD_UNSET_FAILING_OVER(bp) ((bp)->b_flags &= ~B_XLVD_FAILOVER)

#define FO_STATE(bp) ((long) (bp)->b_private)
#define FO_SET_STATE(bp, st) ((bp)->b_private = (void *) (st))
/* NORMAL: nothing special happening, failover-wise.  state = 0 */
#define FO_NORMAL 0L
/* SWITCH_FAILED: switch failed */
#define FO_SWITCH_FAILED 1L
/* TRY_NEXT: try next path */
#define FO_TRY_NEXT 2L
/* SWITCH: i/o failed -- need to switch paths */
#define FO_SWITCH 3L
/* REISSUE: switch succeeded --  reissue i/o */
#define FO_REISSUE 4L
/* ABORT: abort failover */
#define FO_ABORT 5L
/* ISSUE DEFERRED: switch was successful -- issue deferred bps */
#define FO_ISSUE_DEFERRED 6L
/* ABORT_ALL: switch failed -- "iodone" deferred bps with error */
#define FO_ABORT_ALL 7L


extern const int xlvd_n_classes; 	/* number of classes of disks */

int    xlv_devopen(xlv_tab_disk_part_t *, int, int, struct cred *);
int    xlv_devclose(xlv_tab_disk_part_t *, int, int, struct cred *);
int    xlvd_queue_failover(buf_t *);	/* queue a buffer for failover */
buf_t *fo_get_deferred(xlv_tab_disk_part_t *);	/* get dp's deferred bp's */
char   *devtopath(dev_t);

#endif	/* __XLV_IOCTX_H__ */
