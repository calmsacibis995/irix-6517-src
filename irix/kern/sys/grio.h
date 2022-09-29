/*                                                                        *
 *               Copyright (C) 1989, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.50 $"
#ifndef _GRIO_H_
#define _GRIO_H_

/*
 * This include file contains the definitions and structures 
 * used by the GRIO subsystem
 */

/*
 * Global defines that determine the amount of resources used 
 * by the GRIO subsystem.
 */

#include	<sys/uuid.h>	/* for uuid_t in stream_id	*/
#include	<sys/pmo.h>	/* for MLD, MLDSET defs		*/
#include	<sys/param.h>	/* needed for MAXPATHLEN	*/ 

/*
 * maximum number of disks used by the GRIO subsystem
 */
#define	MAX_NUM_DISKS		512	

/*
 * maximum number of disks in a single stripped volume used by GRIO
 */
#define	MAX_ROTOR_SLOTS		256

/*
 * maximum number of reservations allowed in GRIO subsystem
 */
#define MAX_NUM_RESERVATIONS	3000

/*
 * maximum depth of ggd/grio command queue
 */
#define MAX_GRIO_QUEUE_COUNT    200

/* 
 * maximum number of streams returned by a single STAT call
 */
#define	MAX_STREAM_STAT_COUNT	MAX_NUM_RESERVATIONS

/* 
 * maximum size of a GRIO device name (as recorded in the /etc/grio_config file)
 */
#define DEV_NMLEN		MAXPATHLEN

/* 
 * redefine type definitions
 */
typedef	__uint64_t	gr_ino_t;
typedef	uuid_t		stream_id_t;

/* 
 * grio_resv structure is filled in by the user process and is sent to the
 * library routine grio_request() along with the file descriptor of the 
 * resouce for which the I/O rate guarantee is being requested.
 * 
 * The grio_request() call will return 0 if there are no errors. If there
 * is an error the routine will return -1 and the gr_error field will 
 * contain the error number. In addition, if the guarantee request is
 * denied due to lack of device bandwidth, then gr_optime, and gr_opsize
 * will contain values describing the maximum remaining bandwidth.
 *
 *
 * Return errors are:
 * 	The first two errors indicate an error in the library.
 *		EINVAL		- could not communicate to daemon
 *		ESRCH		- invalid procid 
 *
 *	These errors indicate an error in the calling process.
 *		EBADF		- could not stat file or file already
 *				  has a guarantee
 *		EIO		- error in guarantee request structure 
 *				   * start or duration time is incorrect
 *				   * invalid flags in gr_flags field
 *		EPERM		- invalid I/O size for file system
 *		ENOSPC		- bandwidth could not be allocated
 *		ENOENT		- file does not contain any extents.
 *		EACCES		- cannot provide desired level of 
 *				  guarantee (i.e HARD vs SOFT)
 */

typedef struct grio_resv {
	char		gr_action;		/* RESV_UNRESV action    */
	time_t          gr_start;               /* when to start in secs */
	time_t          gr_duration;            /* len of guarantee in secs*/
	time_t          gr_optime;              /* time of one op in usecs */
	int             gr_opsize;              /* size of each op in bytes*/
	stream_id_t     gr_stream_id;           /* stream id             */
	int             gr_flags;               /* flags field           */
	union {
		dev_t	gr_fsdev;		/* FS being reserved     */
		int	gr_fd;			/* fd being reserved     */
	} gr_object_u;
	__uint64_t	gr_memloc;		/* Opaque memory handle  */
	int             gr_error;               /* returned: error code  */
	char            gr_errordev[DEV_NMLEN]; /* device that caused error*/
} grio_resv_t;


#define gr_fsid		gr_object_u.gr_fsdev
#define gr_fid		gr_object_u.gr_fd

/*
 * Action values for the gr_action field
 */
#define GRIO_RESV_ACTION	0x1
#define GRIO_UNRESV_ACTION	0x2

/* Define for gr_duration field. This is assumed to be the default if
 * no duration is specified. (2 yrs in seconds)
 */
#define GRIO_RESV_DURATION_INFINITE	(2*365*24*60*60)

/* 
 * This structure is used to return statistics info to the caller.
 * It is used with GRIO_GET_INFO resv_type.
 *	subcommands are:
 *		GRIO_DEV_RESVS:
 *			return number of reservations on the device 
 *			identified by ( device_name) in grio_blk_t
 *			structure.
 *		GRIO_FILE_RESVS:	
 *			return number of reservations on the file 
 *			identified by ( fs_dev, ino) pair in grio_blk_t
 *			structure.
 *		GRIO_PROC_RESVS:
 *			return number of reservations for the process 
 *			identified by ( procid ) in grio_blk_t structure.
 */
typedef struct grio_stats {
	/*
	 * value dependent on the subcommand and grio_blk_t parameters.
	 */
	u_long	gs_count;		/* current number of reservations
					 * active on the device/file/proc
					 */
	u_long	gs_maxresv;		/* maximum number of reservations
					 * allowed on the device.
					 * for file/proc this is the number
					 * of licensed streams.
					 */
	u_long	gs_optiosize;		/* size of the optimal i/o size
					 * in bytes for this device.
					 * not defined for file/proc.
					 */
	char 	devname[DEV_NMLEN];
} grio_stats_t;

/* info returned to user by libgrio when MLD stuff is done */
typedef	struct grio_mem_locality_s {
	cnodeid_t       grio_cnode;
} grio_mem_locality_t;

/* 
 * Defines for the gr_flags field.
 * Set the user process or by the grio_lib
 */
#define	PER_FILE_GUAR		0x00000008
#define	PER_FILE_SYS_GUAR	0x00000010

#define PROC_PRIVATE_GUAR	0x00000020	
#define PROC_SHARE_GUAR		0x00000040

#define FIXED_ROTOR_GUAR	0x00000100
#define SLIP_ROTOR_GUAR		0x00000200
#define NON_ROTOR_GUAR		0x00000400

#define REALTIME_SCHED_GUAR	0x00002000
#define NON_SCHED_GUAR		0x00004000

#define SYSTEM_GUAR		0x00008000

#define READ_GUAR		0x00010000
#define WRITE_GUAR		0x00020000

#define GUARANTEE_MASK		0x0003FFFF

/*
 * Defines for the types of reservations
 * Set by the ggd daemon
 */
#define RESERVATION_STARTED	0x10000000
#define RESERVATION_TYPE_VOD	0x20000000

/*
 * Defines for stream states. 
 * Set by the grio driver.
 */
#define	STREAM_REMOVE_IN_PROGRESS	0x01000000
#define	STREAM_INITIATE_IN_PROGRESS	0x02000000

#define STREAM_SLIPPED_ONCE		0x04000000

#define STREAM_ASSOCIATED		0x08000000

#define STREAM_STATE_MASK		0xFF000000

/*
 * backwards compatability
 */
#define VOD_LAYOUT		SLIP_ROTOR_GUAR

#define IS_VOD_GUAR( griorp ) \
	( (griorp->gr_flags & FIXED_ROTOR_GUAR)  || \
	  (griorp->gr_flags & SLIP_ROTOR_GUAR)  )

#define IS_FILESYS_GUAR( griorp ) \
	( griorp->gr_flags & PER_FILE_SYS_GUAR )

#define IS_FILE_GUAR( griorp ) \
	( griorp->gr_flags & PER_FILE_GUAR )

/* This structure has information about the end object to which we are
 * allocating bandwidth.
 */
struct end_info {
	char		gr_end_type;
        dev_t           gr_dev;			/* dev_t/file system dev_t */
        gr_ino_t       	gr_ino;			/* inode number            */
};

/*
 * Values for the gr_end_type field
 */
#define END_TYPE_NONE		0x0		/* nothing at this end  */
#define END_TYPE_REG		0x1		/* regular XFS file/fs  */
#define END_TYPE_SPECIAL	0x2		/* device special file */

typedef struct grio_command {
        int             gr_cmd;			/* grio command            */
	int		gr_subcmd;		/* grio subcommand	   */
        pid_t           gr_procid;		/* process id of requestor */
	__uint64_t	gr_fp;			/* file pointer		   */
	struct end_info	one_end;		/* one end info		   */
	struct end_info other_end;		/* other end info. This is
						 * normally END_TYPE_NONE
						 * except for peer-to-peer
						 * cases.
						 */
	__uint64_t	memloc;			/* memory location	   */
	union {
        	grio_resv_t     gr_resv;      	/* grio request info       */
		grio_stats_t	gr_stats;	/* grio stats info	   */
	} cmd_info;
	int		gr_bytes_bw;		/* returned bandwidth	   */
	int		gr_usecs_bw;		/* returned bandwidth	   */
	int		gr_time_to_wait;	/* kernel wait time	   */
} grio_cmd_t;

/*
 * Structure definitions with fields defined as known sizes.
 * This is done to have a layer of abstraction between the kernel and the
 * ggd daemon. The kernel can have 32 bit inumber, file sizes, and file
 * system sizes, but the ggd will always view them as 64 bit quantities.
 */
typedef struct grio_file_id {
	gr_ino_t	ino;
	dev_t		fs_dev;
	pid_t		procid;
	__uint64_t	fp;
} grio_file_id_t;

typedef struct grio_disk_id {
	int		num_iops;
	int		opt_io_size;
	time_t		iops_time;
} grio_disk_id_t;

typedef struct grio_bmbt_irec {
	__uint64_t	br_startoff;
	__uint64_t	br_startblock;
	__uint64_t	br_blockcount;
} grio_bmbt_irec_t;


/*
 * defines for the gr_cmd field.
 */
#define GRIO_UNKNOWN		0
#define GRIO_RESV		1
#define	GRIO_UNRESV		2
#define GRIO_UNRESV_ASYNC	3
#define GRIO_PURGE_VDEV_ASYNC	4
#define	GRIO_GET_STATS		5
#define	GRIO_GET_BW		6
#define	GRIO_GET_FILE_RESVD_BW	7
#define GRIO_ACTIVE_COMMANDS	GRIO_GET_FILE_RESVD_BW + 1
#define GRIO_MAX_COMMANDS	20


#define GRIO_ASYNC_CMD(griocp)	\
	(( (griocp)->gr_cmd == GRIO_UNRESV_ASYNC ) || \
	 ( (griocp)->gr_cmd == GRIO_PURGE_VDEV_ASYNC) )

#define GRIO_STATS_REQ(grioreq)	\
	( (grioreq)->gr_cmd == GRIO_GET_STATS )

#define	GRIO_GET_RESV_DATA( griocp )		(&(griocp->cmd_info.gr_resv))
#define	GRIO_GET_STATS_DATA( griocp )		(&(griocp->cmd_info.gr_stats))

/*
 * syssgi SGI_GRIO commands
 */
#define GRIO_ADD_DISK_INFO   		21
#define GRIO_UPDATE_DISK_INFO   	22
#define GRIO_ADD_STREAM_INFO		23
#define	GRIO_ADD_STREAM_DISK_INFO	24
#define GRIO_ASSOCIATE_FILE_WITH_STREAM	25
#define	GRIO_REMOVE_STREAM_INFO		26
#define	GRIO_REMOVE_ALL_STREAMS_INFO	27
#define GRIO_GET_FILE_EXTENTS   	28
#define GRIO_GET_FS_BLOCK_SIZE  	29
#define GRIO_GET_FILE_RT		30
#define GRIO_WRITE_GRIO_REQ 		31
#define GRIO_READ_GRIO_REQ 		32
#define GRIO_WRITE_GRIO_RESP		33
#define GRIO_GET_STREAM_ID		34
#define GRIO_GET_ALL_STREAMS		35
#define GRIO_GET_VOD_DISK_INFO		36

#define GRIO_MONITOR_START		37
#define GRIO_MONITOR_GET		38
#define GRIO_MONITOR_END		39

#define	GRIO_READ_NUM_CMDS		40
#define	GRIO_GET_HWGRAPH_PATH		41
#define	GRIO_GET_MLD_CNODE		42
#define	GRIO_GET_FP			43
#define GRIO_DISSOCIATE_STREAM		44


#define COPY_STREAM_ID(from, to)     ( bcopy(&(from),&(to), sizeof(uuid_t)) )
#define EQUAL_STREAM_ID(one, two)    ( uuid_equal( &one, &two, &status) )
#define SET_GRIO_IOPRI(bp, iopri)    BUF_GRIO_PRIVATE(bp)->grio_iopri = iopri
#define GET_GRIO_IOPRI(bp, iopri)    iopri = BUF_GRIO_PRIVATE(bp)->grio_iopri

typedef struct grio_add_disk_info_struct {
	dev_t	gdev;
	int	num_iops_rsv;
	int	num_iops_max;
	int	opt_io_size;
	int	rotation_slot;
	int	realtime_disk;
} grio_add_disk_info_struct_t;


typedef struct grio_stream_stat {
	stream_id_t		stream_id;
	gr_ino_t		ino;
	dev_t			fs_dev;
	pid_t			procid;
} grio_stream_stat_t;

typedef struct grio_vod_info {
	int			num_rotor_slots;
	int			rotor_position;
	int			num_rotor_streams;
	int			num_nonrotor_streams;
	int			rotor_slot[MAX_ROTOR_SLOTS];
} grio_vod_info_t;


#ifdef _KERNEL
/*
 * Kernel buffer scheduling structure.
 */
/*
 * grio disk information
 * the kernel allocates and maintains one such structure for 
 * each disk used by the grio subsystem
 */
typedef struct grio_disk_info {
        int             		num_iops_max;
        int             		num_iops_rsv;
        int             		opt_io_size;
	int				active;
	lock_t          		lock;
	struct grio_stream_disk_info	*diskstreams;

	int				ops_issued;
	int				ops_complete;
	int				subops_issued;
	int				subops_complete;
	int				rotate_position;
	int				realtime_disk;
	
	time_t				time_start;
	int				opcount;
	time_t				reset_time;
	time_t				timeout_time;
	toid_t				timeout_id;
	dev_t				diskdev;
} grio_disk_info_t;


/*
 * grio disk information used by grioidbg for global op
 */
typedef struct grio_idbg_disk_info	{
	grio_disk_info_t	*griodp_ptr;
	struct	grio_idbg_disk_info	*next;
} grio_idbg_disk_info_t;


#define GRIO_STREAM_TABLE_SIZE	50
#define GRIO_STREAM_TABLE_INDEX( id )	 ( (id / 8) % GRIO_STREAM_TABLE_SIZE )

/*
 * grio stream information 
 * the kernel allocates and maintains one such structure for
 * each active grio stream
 */
typedef struct grio_stream_info {
	struct grio_stream_info 	*nextstream;
	struct grio_stream_disk_info	*diskstreams;
	lock_t				lock;
	stream_id_t			stream_id;
	__uint64_t			fp;
	pid_t				procid;
	gr_ino_t			ino;
	dev_t				fs_dev;
	int				flags;
	int				total_slots;
	int				max_count_per_slot;
	int				rotate_slot;
	time_t				last_stream_op;
} grio_stream_info_t;



/*
 * Macros to access the per stream flags.
 */
#define MARK_STREAM_AS_BEING_REMOVED(griosp) \
			(griosp->flags |= STREAM_REMOVE_IN_PROGRESS)

#define MARK_STREAM_AS_INITIATING_IO(griosp) \
			(griosp->flags |= STREAM_INITIATE_IN_PROGRESS)

#define CLEAR_STREAM_AS_INITIATING_IO(griosp) \
			(griosp->flags &= ~STREAM_INITIATE_IN_PROGRESS)

#define STREAM_BEING_REMOVED(griosp)	\
			(griosp->flags & STREAM_REMOVE_IN_PROGRESS)

#define STREAM_INITIATE_IO(griosp)	\
			(griosp->flags & STREAM_INITIATE_IN_PROGRESS)


#define PER_FILE_SYS_STREAM(griosp)	\
			(griosp->flags & PER_FILE_SYS_GUAR)

#define PER_FILE_STREAM(griosp)		\
			(griosp->flags & PER_FILE_GUAR)


#define FIXED_ROTOR_STREAM( griosp )	\
			(griosp->flags & FIXED_ROTOR_GUAR)

#define SLIP_ROTOR_STREAM( griosp )	\
			(griosp->flags & SLIP_ROTOR_GUAR)

#define ROTOR_STREAM(griosp)		\
		(FIXED_ROTOR_STREAM( griosp) || SLIP_ROTOR_STREAM(griosp))

#define NON_ROTOR_STREAM(griosp)	 \
			(griosp->flags & NON_ROTOR_GUAR)



#define RT_SCHED_STREAM(griosp)	\
			(griosp->flags & REALTIME_SCHED_GUAR)

#define NS_SCHED_STREAM(griosp)	\
			(griosp->flags & NON_SCHED_GUAR)


#define SLIPPED_ONCE_STREAM(griosp) \
			(griosp->flags & STREAM_SLIPPED_ONCE)

#define MARK_SLIPPED_ONCE_STREAM(griosp) \
			(griosp->flags  |= STREAM_SLIPPED_ONCE)

#define CLEAR_SLIPPED_ONCE_STREAM(griosp) \
			(griosp->flags &= ~STREAM_SLIPPED_ONCE)

#define MARK_STREAM_AS_ASSOCIATED(griosp) \
			(griosp->flags |= STREAM_ASSOCIATED)

#define MARK_STREAM_AS_DISSOCIATED(griosp) \
			(griosp->flags &= ~STREAM_ASSOCIATED)

#define STREAM_IS_ASSOCIATED(griosp) \
			(griosp->flags & STREAM_ASSOCIATED)

/*
 * per disk stream information
 * the kernel allocates and maintaines one such structure for
 * each (stream id, disk used by that stream) pair.
 */
typedef struct grio_stream_disk_info {
	struct grio_stream_disk_info	*nextdiskinstream;
	grio_stream_info_t		*thisstream;
	struct grio_stream_disk_info	*nextstream;
	grio_disk_info_t		*griodp;

	lock_t	lock;				/* lock to protect structure            */
	time_t	period_end;			/* time current period will end         */
	time_t	iops_time;			/* length of period for this stream     */
	buf_t	*realbp;			/* orignal bp from xlv for this request */
	buf_t	*bp_list;			/* list of sub bps for this request     */
	buf_t	*issued_bp_list;		/* list of issued sub bps for this request     */
	buf_t	*queuedbps_front;		/* ptrs to queue of bps using this */ 
	buf_t	*queuedbps_back;		/* stream id                            */
	int	iops_remaining_this_period;	/* num grios left for this req this prd */
	time_t	time_priority_start;
	time_t	last_op_time;			/* time last out of band op */
	int	num_iops;			/* num grios for this req this period   */
	int	opt_io_size;			/* size of grio request                 */
} grio_stream_disk_info_t;


extern int 	grio_strat( buf_t *);
extern int 	grio_config( sysarg_t, sysarg_t, sysarg_t, sysarg_t, sysarg_t );
extern void 	grio_iodone( buf_t *);

/* This structure defines a linked list of rate guaranteed requests which
 * have been issued by clients but not yet satisfied by the daemon.
 */
typedef struct grio_cmd_queue {
        sema_t          	sema;
	int			num_cmds;
        grio_cmd_t      	*griocmd;
        struct grio_cmd_queue 	*forw;
        struct grio_cmd_queue 	*back;
} grio_cmd_queue_t;

#define GRIO_NONGUARANTEED_STREAM(griosp) \
	( EQUAL_STREAM_ID( griosp->stream_id, non_guaranteed_id ) )

/*
 * Private data attached to the buffer structures.
 *	This structure may NOT contain any data unique specific to a 
 *	per disk I/O request. It is common to all the bps generated from
 *	a single user I/O request.
 */
typedef struct grio_buf_data {
	uuid_t	grio_id;		/* stream id of stream associated */
	short	grio_iopri;		/* priority of the io, b_iopri */
#ifdef GRIO_DEBUG			/* with this I/O request */
	time_t	start_time;
	time_t	enter_queue_time;
#endif
} grio_buf_data_t;



#ifdef GRIO_DEBUG
#define	INIT_GRIO_TIMESTAMP( bp  )  {			\
	if ( BUF_IS_GRIO( bp ) ) {			\
		BUF_GRIO_PRIVATE(bp)->start_time = lbolt;	\
	}						\
}
		
#define CHECK_GRIO_TIMESTAMP( bp, maxtime )	{			\
	if ( BUF_IS_GRIO( bp ) ) {					\
		if ((lbolt - BUF_GRIO_PRIVATE(bp)->start_time) > maxtime) {\
			printf("GRIO TIMESTAMP %d TICKS: file %s, line %d\n", \
				lbolt - BUF_GRIO_PRIVATE(bp)->start_time,\
				__FILE__,__LINE__);			\
		}							\
	}								\
}

#else
#define	INIT_GRIO_TIMESTAMP( bp )
#define	CHECK_GRIO_TIMESTAMP( bp, maxtime )
#endif


#define	BUF_GRIO_PRIVATE(bp) 	((grio_buf_data_t *)((bp)->b_grio_private))

/*
 *  Global lock and unlock macros
 */
#ifdef SIM

#define GRIO_GLOB_LOCK()        	1
#define GRIO_GLOB_UNLOCK( s )   	s
#define	GRIO_DISK_LOCK()		1
#define	GRIO_DISK_UNLOCK( s )		s
#define	GRIO_STREAM_LOCK()		1
#define	GRIO_STREAM_UNLOCK( s )		s
#define	GRIO_STREAM_DISK_LOCK()		1
#define	GRIO_STREAM_DISK_UNLOCK( s )	s
#define GRIO_TICKETT_LOCK() 		1
#define GRIO_TICKETT_UNLOCK(s)		s

#else

#define GRIO_GLOB_LOCK()		mutex_spinlock(&grio_global_lock)
#define GRIO_GLOB_UNLOCK(s)		mutex_spinunlock(&grio_global_lock, s)

#define GRIO_STABLE_LOCK()		mutex_spinlock(&grio_stream_table_lock)
#define GRIO_STABLE_UNLOCK(s) \
				mutex_spinunlock(&grio_stream_table_lock, s)

#define GRIO_DISK_LOCK(griodp)		mutex_spinlock(&griodp->lock)
#define GRIO_DISK_UNLOCK(griodp,s)	mutex_spinunlock(&griodp->lock, s)

#define GRIO_STREAM_LOCK(griosp)	mutex_spinlock(&griosp->lock)
#define GRIO_STREAM_UNLOCK(griosp,s)	mutex_spinunlock(&griosp->lock, s)

#define GRIO_STREAM_DISK_LOCK(griosdp)		mutex_spinlock(&griosdp->lock)
#define GRIO_STREAM_DISK_UNLOCK(griosdp,s)    mutex_spinunlock(&griosdp->lock,s)

#endif

/* Function to convert the device number to an pointer to
 * structure containing grio_info
 */
grio_disk_info_t	*grio_disk_info(dev_t gdev);

/*
 * Macros to set or clear the bits in the b_flags field of the buf structure.
 */
#define BUF_IS_GRIO( bp )               (bp->b_flags &   B_GR_BUF)
#define BUF_IS_GRIO_ISSUED( bp )        (bp->b_flags &   B_GR_ISD)
#define CLR_BUF_GRIO_ISSUED(bp)         (bp->b_flags &= ~B_GR_ISD)
#define MARK_BUF_GRIO_ISSUED(bp)        (bp->b_flags |=  B_GR_ISD)


#ifdef DEBUG
#define dbg1printf(_x)		if (grio_dbg_level > 1 ) { printf _x ; }
#define dbg2printf(_x)		if (grio_dbg_level > 2 ) { printf _x ; }
#define dbg3printf(_x)		if (grio_dbg_level > 3 ) { printf _x ; }
#else
#define dbg1printf(_x)
#define dbg2printf(_x)
#define dbg3printf(_x)
#endif



#endif /* _KERNEL */

#define GRIO_MONITOR_COUNT	 100

typedef struct grio_monitor_times {
	time_t		starttime;
	time_t		endtime;
	__int64_t	size;
} grio_monitor_times_t;

typedef struct  grio_monitor {
	grio_monitor_times_t	times[GRIO_MONITOR_COUNT];
	int			start_index;
	int			end_index;
	stream_id_t		stream_id;
	int			monitoring;
} grio_monitor_t;


/*
 * syssgi GRIO_GET_HWGRAPH_PATH returns an array of these structures.
 * The class and type are akin to inventory records. For hardware 
 * components which do not have actual inventory records associated 
 * with the hwgfs vertices, grio determines the class, type and state
 * fields. This structure can be extended to return unit/controller etc.
 */

typedef	struct grio_dev_info {
	int		grio_dev_class;
	int		grio_dev_type;
	int		grio_dev_state;
	dev_t		devnum;
} grio_dev_info_t;

typedef struct grio_ioctl_info {
	vertex_hdl_t		prev_vhdl;
	vertex_hdl_t		next_vhdl;
	unsigned long long	reqbw;
} grio_ioctl_info_t;

#endif /* _GRIO_H_ */
