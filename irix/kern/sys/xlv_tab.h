#ifndef __SYS_XLV_TAB_H__
#define __SYS_XLV_TAB_H__

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
#ident "$Revision: 1.57 $"

/*
 * The geometry of all XLV volumes on the system is described by
 * 2 tables:
 *
 *   xlv_tab is an array of all subvolumes.  It is indexed by minor
 *     device number.
 *   xlv_tab_vol is an array of all the volumes.  Each entry contains
 *     pointers to the xlv_tab entries that describe all of its
 *     subvolumes.
 *  
 * Each entry of xlv_tab is protected by two multiple-reader/single-writer
 * locks.  Because the xlv_tab structure is used by both kernel and
 * user mode code, the locks will be kept in separate arrays -
 *   xlv_io_lock and xlv_config_lock.
 */

#include <sys/xlv_ioctl.h>

/*
 * xlv_tab_vol and xlv_tab are allocated from kernel heap.
 */
extern struct xlv_tab_vol_s 	*xlv_tab_vol;
extern struct xlv_tab_s 	*xlv_tab;


/*
 * Current version of the in-core structures.
 */

#define	XLV_TAB_VOL_ENTRY_VERS_1 1
#define	XLV_TAB_VOL_ENTRY_VERS	 2      /* make 6.2/5.3 tools incompatable */
#define	XLV_TAB_SUBVOL_VERS_1	 1      /* Failover support */
#define	XLV_TAB_SUBVOL_VERS_2	 2      /* 64 bits subvol_size */
#define XLV_TAB_SUBVOL_VERS      3      /* make 6.2/5.3 tools incompatable */

/*
 * Array of volumes on the system.
 * Entries are not sorted.
 */

#define XLV_VOL_AUTOMOUNT		0x1
#define XLV_VOL_READONLY		0x2
#define XLV_VOL_MOUNT_COMPLETE_ONLY	0x4
#define XLV_VOL_MOUNT_MISSING_OKAY	0x8

typedef struct xlv_tab_vol_entry_s {
	__uint32_t		version;
        uuid_t          	uuid;
        xlv_name_t      	name;
	__uint32_t		flags;
	unsigned char		state;
	unsigned char		rsvd;			/* padding */
        short           	sector_size;		/* Read only */
        struct xlv_tab_subvol_s *log_subvol;
        struct xlv_tab_subvol_s *data_subvol;
        struct xlv_tab_subvol_s *rt_subvol;
	void			*nodename;		/* owner */ 
} xlv_tab_vol_entry_t;

#if defined(_KERNEL)
typedef struct irix5_xlv_tab_vol_entry_s {
	app32_uint_t 		version;
        uuid_t          	uuid;
        xlv_name_t      	name;
	app32_uint_t		flags;
	unsigned char		state;
	unsigned char		rsvd;			/* padding */
        short           	sector_size;		/* Read only */
        app32_ptr_t 		log_subvol;		/* 32 bit ptr */
        app32_ptr_t 		data_subvol;		/* 32 bit ptr */
        app32_ptr_t 		rt_subvol;		/* 32 bit ptr */
	app32_ptr_t		nodename;		/* 32 bit ptr */ 
} irix5_xlv_tab_vol_entry_t;
#endif

typedef struct xlv_tab_vol_s {
	__uint32_t		num_vols;
	__uint32_t		max_vols;
	xlv_tab_vol_entry_t	vol[1];	/* dynamically allocated */
} xlv_tab_vol_t;

#if defined(_KERNEL)
typedef struct irix5_xlv_tab_vol_s {
	app32_uint_t		num_vols;
	app32_uint_t		max_vols;
	irix5_xlv_tab_vol_entry_t	vol[1];	/* dynamically allocated */
} irix5_xlv_tab_vol_t;
#endif


/*
 * Array of all subvolumes on the system.
 * Indexed by minor device number.
 */

#define N_PATHS		4

typedef struct {
        uuid_t  	uuid;
	unsigned char	class;		/* which class of disk */
	unsigned char	n_paths;	/* number of alternate i/o paths */
	unsigned char	active_path;	/* active i/o path: must be < n_paths */
	unsigned char	retry_path;	/* keeps track of retry paths */
	/* each devno represents an alt i/o path to the disk partition */
	dev_t   	dev[N_PATHS];
	__uint32_t	part_size;	/* in blocks */
} xlv_tab_disk_part_t;

/* Note: (a, b) evaluates both a and b, and returns the value of b. */
#define DP_DEV(dp) \
  ((ASSERT((dp).n_paths <= N_PATHS \
	   && (0 == (dp).n_paths || (dp).active_path < (dp).n_paths) \
	   && (dp).active_path == (dp).retry_path), \
    (dp).dev[(dp).active_path]))

#ifdef DEBUG_FAILOVER
#define FO_DEBUG(x) do { x } while (0)
#else
#define FO_DEBUG(x) 
#endif /* DEBUG_FAILOVER */

#ifndef _KERNEL
extern void xlv_fill_dp(xlv_tab_disk_part_t *, dev_t);
#endif

/* classes supported */
#define NULL_CLASS	0
#define DGC_CLASS	1


/* 
   Definitions:

   A stripe unit is the unit of data interleaving.  It is the amount
   of data that is placed on a disk before placing data on a different
   disk.  The default stripe unit size will be one track.

   A stripe group is the collection of disks over which the data is
   striped.

   A stripe is the collection of stripe units that span a stripe
   group.  In the case of RAID, a stripe is the collection of
   stripe units over which parity is computed.
   There will usually be multiple stripes in a stripe group.
 */

#define XLV_VE_REVIVING	0x01			/* vol elmnt being revived. */
#define XLV_VE_OPEN	0x02			/* vol elmnt is open. */
	

typedef struct {
        uuid_t          uuid;
	union {
        	xlv_name_t      name;
		struct {
			char	name[8];
			time_t	timestamp;
		} ve_us;
	} ve_u;
	unsigned char	type;			/* stripe or concatenate */
        unsigned char   grp_size;		/* number of disks involved */ 
        unsigned short  stripe_unit_size;	/* size of a stripe unit,
						   in 512 byte blocks;
						   track size if non-striped. */
	unsigned char	state;
	unsigned char	flags;
        __int64_t	start_block_no;
        __int64_t	end_block_no;
       	xlv_tab_disk_part_t disk_parts[XLV_MAX_DISK_PARTS_PER_VE];
} xlv_tab_vol_elmnt_t;
#define	veu_name	ve_u.name
#define	veu_uname	ve_u.ve_us.name
#define	veu_timestamp	ve_u.ve_us.timestamp


typedef struct xlv_tab_plex_s {
	__uint32_t		flags;
	uuid_t          	uuid;
        xlv_name_t      	name;
        __uint32_t		num_vol_elmnts;
        __uint32_t		max_vol_elmnts;
	xlv_tab_vol_elmnt_t	vol_elmnts[1];
} xlv_tab_plex_t;

#if defined(_KERNEL) && _MIPS_SIM == _ABI64
typedef struct irix5_xlv_tab_plex_s {
	app32_uint_t		flags;
	uuid_t          	uuid;
        xlv_name_t      	name;
        app32_uint_t           	num_vol_elmnts;
        app32_uint_t            max_vol_elmnts;
	xlv_tab_vol_elmnt_t	vol_elmnts[1];
} irix5_xlv_tab_plex_t;
#endif

/*
 * A critical region. Used to temporarily exclude a section from
 * I/O.
 * All requests, except for those whose bp->dma_addr matches that
 * of excluded_rqst, will be subject to the critical region.
 *
 * This structure is really only used in the kernel. We make it
 * part of the user mode code also so that we can easily pass
 * data structures back and forth.
 */
typedef struct {
	caddr_t		excluded_rqst;
        daddr_t		start_blkno;
        daddr_t		end_blkno;
        struct buf	*pending_requests;
} xlv_critical_region_t;

#if defined(_KERNEL)
typedef struct {
	app32_ptr_t	excluded_rqst;
        app32_long_t	start_blkno;
        app32_long_t	end_blkno;
        app32_ptr_t	pending_requests;	/* 32 bit pointer */
} irix5_o32_xlv_critical_region_t;

typedef struct {
	app32_ptr_t		excluded_rqst;
        app32_long_long_t	start_blkno;
        app32_long_long_t	end_blkno;
        app32_ptr_t		pending_requests;	/* 32 bit pointer */
} irix5_n32_xlv_critical_region_t;
#endif


/*
 * Array describing the plexes that contain each block of disk addresses.
 * Since each subvolume starts at address 0, all we need to do is to
 * store the ending block numbers for these extents.
 *
 * We use separate read/write plex vectors because we only read from
 * online plexes but may write to stale plexes during recovery.
 */
typedef struct {
	__int64_t	end_blkno;
	unsigned char	read_plex_vector;
	unsigned char	write_plex_vector;
} xlv_block_map_entry_t;

typedef struct {
	__int64_t	start_blkno;
	__int64_t	end_blkno;
	unsigned	read_plex_vector;
	unsigned	write_plex_vector;
} ve_entry_t;

#if 0
#if defined(_KERNEL)
typedef struct {
	app32_long_t	end_blkno;
	unsigned char	read_plex_vector;
	unsigned char	write_plex_vector;
} irix5_o32_xlv_block_map_entry_t;

typedef struct {
	app32_long_long_t	end_blkno;
	unsigned char		read_plex_vector;
	unsigned char		write_plex_vector;
} irix5_n32_xlv_block_map_entry_t;
#endif
#endif

typedef struct {
	__int32_t		entries;
	xlv_block_map_entry_t	map[1];		/* dynamically allocated */
} xlv_block_map_t;


typedef struct xlv_tab_subvol_s {
	__uint32_t		version;
	__uint32_t		flags;
	__uint32_t		open_flag;	/* flag passed to xlv 
						   when subvol was opened. */
	struct xlv_tab_vol_entry_s *vol_p;
        uuid_t          	uuid;
        short           	subvol_type;
        short           	subvol_depth;
#ifndef _USE_XLV_TAB_SUBVOL_VERS_1
	__int64_t		subvol_size;	/* maximum of the sizes of
						   the plexes. */
#else
	daddr_t			subvol_size;	/* maximum of the sizes of
						   the plexes. */
#endif
        dev_t           	dev;

	/*
	 * The following fields are only referenced if this subvolume
	 * is plexed.
	 */
	xlv_critical_region_t	critical_region;
	struct buf		*err_pending_rqst;
	daddr_t			rd_wr_back_start;
	daddr_t			rd_wr_back_end;
	unsigned short		initial_read_slice;
        unsigned short         	num_plexes;
	xlv_tab_plex_t		*plex[XLV_MAX_PLEXES];
	xlv_block_map_t		*block_map;
} xlv_tab_subvol_t;

#if defined(_KERNEL)
typedef struct irix5_o32_xlv_tab_subvol_s {
	app32_uint_t		version;
	app32_uint_t		flags;
	app32_uint_t		open_flag;	/* flag passed to xlv 
						   when subvol was opened. */
	app32_ptr_t 		vol_p;		/* 32 bit pointer */

        uuid_t          	uuid;
        short           	subvol_type;
        short           	subvol_depth;
#ifndef _USE_XLV_TAB_SUBVOL_VERS_1
	app32_long_long_t	subvol_size;	/* maximum of the sizes of
						   the plexes. */
#else
	app32_long_t		subvol_size;	/* maximum of the sizes of
						   the plexes. */
#endif
        app32_ulong_t          	dev;

	/*
	 * The following fields are only referenced if this subvolume
	 * is plexed.
	 */
	irix5_o32_xlv_critical_region_t	critical_region;
	app32_ptr_t 		err_pending_rqst;	/* 32 bit pointer */
	app32_long_t		rd_wr_back_start;
	app32_long_t		rd_wr_back_end;
	unsigned short		initial_read_slice;
        unsigned short         	num_plexes;
	app32_ptr_t		plex[XLV_MAX_PLEXES];	/* 32 bit pointer */
	app32_ptr_t		block_map;		/* 32 bit pointer */
} irix5_o32_xlv_tab_subvol_t;

typedef struct irix5_n32_xlv_tab_subvol_s {
	app32_uint_t		version;
	app32_uint_t		flags;
	app32_uint_t		open_flag;	/* flag passed to xlv 
						   when subvol was opened. */
	app32_ptr_t 		vol_p;		/* 32 bit pointer */

        uuid_t          	uuid;
        short           	subvol_type;
        short           	subvol_depth;
	app32_long_long_t	subvol_size;	/* maximum of the sizes of
						   the plexes. */
        app32_ulong_t          	dev;

	/*
	 * The following fields are only referenced if this subvolume
	 * is plexed.
	 */
	irix5_n32_xlv_critical_region_t	critical_region;
	app32_ptr_t 		err_pending_rqst;	/* 32 bit pointer */
	app32_long_long_t	rd_wr_back_start;
	app32_long_long_t	rd_wr_back_end;
	unsigned short		initial_read_slice;
        unsigned short         	num_plexes;
	app32_ptr_t		plex[XLV_MAX_PLEXES];	/* 32 bit pointer */
	app32_ptr_t		block_map;		/* 32 bit pointer */
} irix5_n32_xlv_tab_subvol_t;
#endif

typedef struct xlv_tab_s {
	__uint32_t		num_subvols;
	__uint32_t		max_subvols;
	xlv_tab_subvol_t	subvolume[1];	/* dynamically allocated */
} xlv_tab_t;

#if defined(_KERNEL)
typedef struct irix5_o32_xlv_tab_s {
	app32_uint_t			num_subvols;
	app32_uint_t			max_subvols;
	irix5_o32_xlv_tab_subvol_t	subvolume[1];
} irix5_o32_xlv_tab_t;

typedef struct irix5_n32_xlv_tab_s {
	app32_uint_t			num_subvols;
	app32_uint_t			max_subvols;
	irix5_n32_xlv_tab_subvol_t	subvolume[1];
} irix5_n32_xlv_tab_t;
#endif


typedef unsigned char xlv_slice_t;

#define XLV_MAX_FAILURES 5	/* max number of vol elmnt failures that
				   we'll store per request before declaring
				   entire request failed. */

#ifdef _KERNEL

/*
 * Memory block of buffers used by xlv. Then number of entries in buf[]
 * is computed dynamically.
 */
typedef struct xlvmem {
        struct xlvmem    *next;
	int		mapped;
        unsigned        bufs;
        unsigned        privmem;	/* 1 if part of reserved pool. Always
					   retained in case system runs
					   low on memory. */
        struct buf      buf[1];
} xlvmem_t;

/*
 * pdev_list is the array that we use to sort the devices.  It has
 * to be large enough to hold 2 maximum logical volume configurations.
 * To simplify the code, we also add an extra element at the end.
 *
 * At current sizes, this is 32K*2+1 ~ 64K
 */

typedef struct {
	dev_t		dev;
	boolean_t	in_current_config;	/* whether this device
					is part of the current/old
					logical volume. */
} xlv_pdev_node_t;

typedef xlv_pdev_node_t *xlv_pdev_list_t;

#else

typedef void		xlvmem_t;

#endif /* _KERNEL */


/*
 * The failure record stores the external device number and the range 
 * of blocks within it that failed.
 */
typedef struct {
	dev_t			dev;		/* external dev number */
	int			error;		/* reason for failure */
} xlv_failure_record_t;


/*
 * This structure describes the actual slices that were involved in
 * an I/O.
 */
/*
 * A single request can only span 3 rows (or 3 volume elements). And this
 * can only happen in the case where you have ridiculously small paritions.
 */
#define XLV_MAX_ROWS_PER_IO 3

typedef struct {
	__uint32_t		first_block_map_index;
	__uint32_t		num_entries;
	xlv_slice_t		slices_used[XLV_MAX_ROWS_PER_IO];
} xlv_blk_map_history_t;



/*
 * Structure passed as input to the DIOCXLVPLEXCOPY ioctl to 
 * initiate a "plex" copy.  This is designed so that you can
 * specify only a subset of the address space to be copied.
 */
#define	XLV_PLEX_COPY_PARAM_VERS	2	/* blkno == 64 bits */
typedef struct xlv_plex_copy_param_s {
	__uint32_t		version;
	__int64_t		start_blkno;
	__int64_t		end_blkno;
	__uint32_t		chunksize;	/* in blocks */
	__uint32_t		sleep_intvl_msec;
} xlv_plex_copy_param_t;

#if defined(_KERNEL)
typedef struct irix5_o32_xlv_plex_copy_param_s {
	app32_uint_t		version;
	app32_long_long_t	start_blkno;
	app32_long_long_t	end_blkno;
	app32_uint_t		chunksize;	/* in blocks */
	app32_uint_t		sleep_intvl_msec;
} irix5_o32_xlv_plex_copy_param_t;

typedef struct irix5_n32_xlv_plex_copy_param_s {
	app32_uint_t		version;
	app32_long_long_t	start_blkno;
	app32_long_long_t	end_blkno;
	app32_uint_t		chunksize;	/* in blocks */
	app32_uint_t		sleep_intvl_msec;
} irix5_n32_xlv_plex_copy_param_t;
#endif


/*
 * Subvolume flags
 * The following flags are only written by the kernel code. They are
 * read by user-mode code. xlv_shutdown, for example, checks the
 * open flags.
 */
#define XLV_OPEN_CHR 0x01 /* xlv device is open for char I/O. */
#define XLV_OPEN_MNT 0x02 /* xlv device is open for fs I/O. */
#define XLV_OPEN_BLK 0x04 /* xlv device is open for blk I/O. */
#define XLV_OPEN_SWP 0x08 /* xlv device is open for swap I/O. (unlikely!) */
#define XLV_OPEN_LYR 0x10 /* xlv device open from higher driver. (unlikely!) */
#define XLV_OPENMASK 0x1f
#define XLV_ISOPEN(x)  (x & XLV_OPENMASK)

#define XLV_SUBVOL_OFFLINE		0x20
#define XLV_SUBVOL_PLEX_CPY_IN_PROGRESS	0x40
#define XLV_SUBVOL_IN_RETRY		0x80

/*
 * Subvolume flags which are saved across restarts.
 */
#define XLV_SUBVOL_NO_ERR_RETRY		0x80000000
#define	XLV_SUBVOL_FLAGS_SAVEMASK	0xFFFF0000	/* bits in label */
#define	XLV_SUBVOL_FLAGS_SAVE(x)	((x) & XLV_SUBVOL_FLAGS_SAVEMASK)


#define COPY_UUID(uuid1,uuid2) ( bcopy(&(uuid1),&(uuid2),sizeof(uuid_t)) )

#define COPY_NAME(name1,name2) ( bcopy((name1),(name2),sizeof(xlv_name_t)) )

/* We assume that the subvol entry is empty if the owning volume is
   non-NULL.  This check is required since xlv_tab is sparse. */
#define XLV_SUBVOL_EXISTS(sv) ((sv)->vol_p != NULL)

#endif /* __SYS_XLV_TAB_H__ */
