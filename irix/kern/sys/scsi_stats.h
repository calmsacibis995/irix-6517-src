/*
 *  Structures for SCSI statistics.
 *
 *  $Id: scsi_stats.h,v 1.2 1995/12/27 23:23:36 robertn Exp $
 */

/*
 * Request flags for the user to pass down to the scsi side door module.
 */
#define REQ_ADAP_STATS		0x01
#define REQ_DISK_STATS		0x02

typedef struct stats_request {
	uint		request;
	uint		adap;		/* bit field of adap nums requested */
	uint		disk;		/* bit field of disk nums requested */
	caddr_t		buf;		/* user buf for return data */
	uint		buf_size;	/* user buf size */
} stats_request_t;



/*
 * Common disk flags are between 1 and 0xffff
 * Adapter specific disk flags start at 0x10000
 */

#define DISK_FLAG_ADP78                 0x00000001
#define DISK_FLAG_QL                    0x00000002


/*
 * Per disk statistics
 */
typedef struct disk_stats {
        uint            disk_flags;
        uint            cmds;           /* cmds issured */
	uint  		cmds_completed; /* commands completed */
        uint            blocks;         /* blocks either read or write */

        struct timeval  busy;           /* total busy time */
        struct timeval  idle;           /* total idle time */
        struct timeval  last_busy;
        struct timeval  last_idle;

        uint            pad[16];        /* future expansion */

        /*
         * Begin host adapter specific disk stats
         */
        union {
                uint    all[16];
                struct {
                        uint    last_block;     /* block addr + block cnt of last cmd */
                        uint    total_seek;     /* abs(last_block - block for curr cmd) */

                } adp78;
                struct {
                        uint    errors;
                } ql;
        } adap_spec;

} disk_stats_t;

/*
 * Common adapter flags are between 1 and 0xffff
 * Adapter specific flags start at 0x10000
 */
 
#define ADAP_FLAG_ADP78			0x00000001
#define ADAP_FLAG_QL			0x00000002

/*
 * Per Adapter Statistics
 */
typedef struct adapter_stats {

	uint		flags;
        uint            cmds;           /* cmds issued */
        uint            cmds_completed; /* commands completed */

	uint		intrs;		/* intrs received */
	uint		intrs_spur;	/* spurious interrupts received */
	uint		blocks;		/* blocks either read or write */

	struct timeval	busy;		/* total busy time */
	struct timeval	idle;		/* total idle time */
	struct timeval	last_busy;	/* if the adapter is about to go
					   busy, or if it is busy, see
					   how long its been busy and bump
					   busy by that much.  Update
					   this field with current time. */
	struct timeval	last_idle;	/* if adapter is about to go idle,
					   set this field with the time.
					   When it is no longer idle,
					   see how long its been idle and
					   bump idle by that much.  When
					   UI app asks for this, may have
					   to update so idle changes. */
	uint		pad[16];	/* future expansion */

	disk_stats_t disk[SCSI_MAXTARG];
					/* ptrs to disk stats structures */

	/*
	 * Begin host adapter specific stats
	 */

	union {
		uint	all[16];	/* future expansion */
		struct {
			uint q_scb;	/* cmd queued waiting for free scb */
			uint q_mem;	/* cmd queued waiting for memory */
			uint intrs_abn;	/* abnormal interrupts, check length,
					   overrun, negotiate, etc. */
		} adp78;
		struct {
			uint markers;		/* markers issued */
		} ql;
	} adap_spec;

} adapter_stats_t;

