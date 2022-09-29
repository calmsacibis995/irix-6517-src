#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/include/RCS/ggd.h,v 1.38 1997/04/28 18:47:10 kanoj Exp $"

/*
 * Definitions of well known file locations.
 */
#define GRIODISKS	"/etc/grio_disks"
#define GRIOCONFIG	"/etc/grio_config"

/*
 * Temporary device nodes created by ggd.
 */
#define GRIO_LOCK_FILE  "/tmp/grio.lock"
#define TMPVDEV		"/tmp/grio_vdev"

/* 
 * system reservation duration ( 2 years in seconds )
 */
#define SYSTEM_DURATION		(2*365*24*60*60)

typedef __uint64_t inode_num_t;

/*
 * Debug macros 
 * 
 * dbglevel go from 0 to 10 
 * 	when 10 most verbose, and 1 least verbose, O means no debugging
 * 
 */
extern int	dbglevel;
#ifdef DEBUG
#define	dbgprintf( level, _x)		if (level < dbglevel) { printf _x ;  }
#else
#define	dbgprintf( level, _x)		
#endif

/* 
 * Define global data structure types
 */

/*
 *  One of these structures is allocated for each device.
 *  It contains the information about the reservations
 *  on that device.
 *
 *  An additional such structure is used to keep track
 *  of all the reservations (file_reservation_list).
 */
typedef struct device_reserv_info{
	int			maxioops;    /* max num optiosize ops which can
					      * be guaranteed on this device  
					      */
	int			optiosize;   /* optimal I/O size in bytes     */
	int			num_ios_rsv; /* number of i/ops reserved      */
        int                     numresvs;    /* number of resvs in resvs list */
	dev_t			dev;	     /* devt of reserved device	      */
        struct reservations	*resvs;      /* linked list of resvs for dev  */
} device_reserv_info_t;


/*
 * reservation information unique
 * to device level reservations
 */
typedef	struct dev_resv_info {
	int			num_iops;      	/* num of opt_io_size reqs    */
	int			iosize;        	/* opt_io_size in bytes       */
	int			resv_type;	/* START or END reservation   */
	dev_t			diskdev;	/* pair of (stream_id, diskdev)
			 			 * ids unique reservation
			  	 		 * in device tree
				 		 */
	time_t			io_time;       	/* time to complete in usecs  */
} dev_resv_info_t;

/*
 * reservation information unique
 * to file level reservations
 */
typedef	struct glob_resv_info {
	char			file_type;	/* regular/special file	      */
	dev_t			fs_dev;		/* dev of file system         */
	pid_t			procid;		/* id of process making resv  */
	ino_t			inum;		/* ino num (if any)           */
	__uint64_t		fp;		/* file pointer		      */
} glob_resv_info_t;


/*
 * Reservation structures. There is one such structure on the
 * reservation list of each device affected by a given reservation.
 *
 * These are linked into lists sorted by the sort_time
 * to hold reservations for file, ctlrs, disks, etc.
 */
typedef struct reservations {
	/*
 	 * common fields
 	 */
	int			flags;         	/* type of reservation        */
        time_t			start_time;    	/* start time in seconds      */
        time_t			end_time;      	/* duration time in seconds   */
	time_t			sort_time;     	/* sort time in seconds       */
	stream_id_t		stream_id;     	/* stream id of this stream   */
        struct reservations	*nextresv;     	/* pointer to next reservation*/
	union  {
		dev_resv_info_t		dev_resv_info;
		glob_resv_info_t	glob_resv_info;
	} resv_info;
	dev_t			fs_dev;
	int			prdevprresvarg1;
	int			prdevprresvarg2;
} reservations_t;

/* disks use this definition */
#define		total_slots		prdevprresvarg1
#define		max_count_per_slot	prdevprresvarg2

/* pci bridges use this definition */
#define		pcitarget		prdevprresvarg1

/* xbows use this definition */
#define		pcibridge		prdevprresvarg1
#define		hubdevice		prdevprresvarg2

/*
 * A device reservation structure must be marked as 
 * either the beginning or the end of a reservation 
 * period. These values are set in the resv_type field 
 * of the dev_resv_info_t structure.
 */
#define	RESV_START	1
#define	RESV_END	2

/*
 * Macros to access device/global parts of reservations_t structure.
 */
#define DEV_RESV_INFO( resvp )		(&(resvp->resv_info.dev_resv_info))
#define GLOB_RESV_INFO( resvp )		(&(resvp->resv_info.glob_resv_info))

/* 
 * Macros to determine device reservation type
 */
#define SET_START_RESV(resvp)	((DEV_RESV_INFO(resvp))->resv_type = RESV_START)
#define SET_END_RESV(resvp)	((DEV_RESV_INFO(resvp))->resv_type = RESV_END)
#define IS_START_RESV(resvp)	((DEV_RESV_INFO(resvp))->resv_type ==RESV_START)
#define IS_END_RESV(resvp)	((DEV_RESV_INFO(resvp))->resv_type ==RESV_END)


/* 
 * Macros to determine reservation flags 
 */
#define RESV_STARTED( resvp )		(resvp->flags & RESERVATION_STARTED)
#define RESV_MARK_STARTED(resvp)	resvp->flags |= RESERVATION_STARTED
#define RESV_IS_VOD( resvp )			\
	( (resvp->flags & RESERVATION_TYPE_VOD) || \
	  (resvp->flags & FIXED_ROTOR_GUAR) 	|| \
	  (resvp->flags & SLIP_ROTOR_GUAR) )

#define RESV_MARK_VOD( resvp )		resvp->flags |= RESERVATION_TYPE_VOD


/*
 * Data structure which contains info about file reservations.
 */
typedef struct file_reservation {
	device_reserv_info_t	info;
} file_reservation_t;

extern file_reservation_t file_reservation_list;

#define FILE_RSV_LIST()		file_reservation_list.info.resvs
#define FILE_RSV_NUM()		file_reservation_list.info.unmresvs
#define FILE_RSV_INFO()		file_reservation_list.info

/*
 * Macro returns true if the the request with  start time s1 and
 * duration s1, overlaps the request with start time s2 and duration s2.
 */
#define TIME_OVERLAP( s1, d1, s2, d2)                           \
                (!( ( (s1 + d1) < s2) || ( (s2 + d2) < s1) ))

/*
 * Macro returns true if the the request rsvp is active at time t.
 */
#define ACTIVE_RESERVATION( rsvp, t)                            \
                (( rsvp->start <= t) &&                         \
                 (t < ( rsvp->start + rsvp->duration)) )

/* 
 * Macro returns the number of "reqsize" requests in "total".
 */
#define NUM_REQS_OF_SIZE( total, reqsize ) \
         ((total / reqsize) + (total % reqsize ? 1 : 0 ))


#define VOD_RESERVATION( flags)		\
	( (flags & FIXED_ROTOR_GUAR) || (flags & SLIP_ROTOR_GUAR) )

/*
 * Structure used by the ggd to return information to a process
 * which issues a GRIO_GET_INFO command.
 */
#define GRIO_LIST_NUM		30
#define GRIO_INFO_STR_SIZE	120
#define GRIO_INFO_VOD_SECS	20
#define GRIO_NUM_FILE_RESVS	100

typedef union grio_return {
	int	size[GRIO_LIST_NUM];
	struct {
		int	current;
		int	total;
		int	opsize;
		int	nonvodcount;
		int	vodgroup;
		int	vodsecs[GRIO_INFO_VOD_SECS];
	} resv;
	char	string[GRIO_INFO_STR_SIZE];
	struct {
		int pid;
		int num;
		time_t	start;
		time_t	duration;
	} fresv[GRIO_NUM_FILE_RESVS];
} grio_return_t;

#define RESV_INFO(dnp)	(&(dnp->resv_info))

typedef struct grio_disk_list {
        dev_t           physdev;        /* physical disk device */
	dev_t		diskdev;	/* grio disk id */
        int             reqsize;        /* size of request */
        time_t          optime;         /* number of usecs */
	int		numbuckets;	/* number of VOD buckets for disk */
} grio_disk_list_t;


#define LINE_LENGTH	520
#define CFG_STR_LEN	LINE_LENGTH


typedef struct dev_cached_info {
	dev_t		devnum;		/* device number */
	void		*info;		/* info inserted by kernel */
} dev_cached_info_t;

#define	MAX_DEVINFOS_CACHED	512

#define	GGD_CACHE_SUCCESS		0
#define	GGD_DEVNUM_NOT_PRESENT		-1
#define	GGD_CACHE_FULL			-2

/*
 * Maximum number of hardware components on any DMA path.
 */

#define	MAXCOMPS		10

#define GET_VOL_STRIPE_SIZE( volep ) \
		(volep->grp_size * volep->stripe_unit_size * NBPSCTR)
#define ROUND_UP( amount, alignment ) \
		(((amount + alignment - 1)/alignment) *alignment)
