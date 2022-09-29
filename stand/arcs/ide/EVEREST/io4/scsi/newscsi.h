#ifndef _NSCSI_
#define _NSCSI_
#ident	"$Revision: 1.4 $"
/*
 * Calls from high level driver to low level driver
 *
 * The following declares arrays of function pointers.  Index into them
 *    with the driver number of the low level driver.  The driver number
 *    can be found out with SCSI_DRIVER_NUMBER (below).  Only scsi_command()
 *    may be called from interrupt.  The others should in general only be
 *    called from device open and close routines.
 *
 * Example call:
 *	(*scsi_command[scsi_driver_number(dev)])(scsi_request)
 *
 * scsi_command() issues a SCSI command to the low level SCSI driver.
 *
 *    If a contingent allegiance condition is entered (check condition
 *    command status), a request sense will be automatically performed.
 *    If the sr_sense field of the request is not null, the sense data
 *    will be copied to the sr_sense address, but not more than
 *    sr_senselen bytes.
 *
 *    In addition, whenever there is a contingent allegiance or SCSI bus
 *    reset, all commands queued in the driver and on the device (except
 *    for the command causing or noticing the error) will be returned
 *    with the sr_status field set to SC_ATTN.  New commands will continue
 *    to be returned with this status until the flag bit SRF_AEN_ACK is
 *    set in sr_flags.
 *
 *    Before any command queueing is done, the device driver should do
 *    a mode sense on mode page 0xA (control mode page) to discover the
 *    QErr bit setting.  Error handling will not work correctly in a
 *    queued environment unless the QErr bit is set.
 *
 * scsi_alloc() initializes a connection between a device level driver and
 *    a host adaptor driver for a given controller, target, and lun.  It
 *    will cause the low level driver to initialize structures needed for
 *    communication if it has not already done so.
 *
 *    The opt argument gives additional information about how the driver
 *    would like to talk to the device.  Currently, two options are
 *    allowed.  SCSIALLOC_EXCLUSIVE causes the device to be allocated
 *    exclusively for the use of the calling driver.  If a device is
 *    allocated for exclusive use, no other driver may access it. Also,
 *    if a device is already in use by another driver, exclusive or not,
 *    and exclusive alloc will fail.  SCSIALLOC_QDEPTH is a mask for
 *    an eight bit value specifying how many commands a device driver
 *    would like to queue.  It is to be considered advice only, and
 *    will not necessarily be followed.
 *
 *    The cb argument is used as a callback if a request sense was done
 *    as a result of a check condition from a command.  If the cb
 *    argument is NULL, no callbacks will be done.  Note that having
 *    a callback set may result in a driver seeing the error twice.
 *    The purpose of the callback is to give make sure that a given
 *    driver gets all sense data from a device, even if it is returned
 *    with a command issued by a different driver.  Only one driver is
 *    allowed to have a callback.  scsi_alloc() will fail if a callback
 *    is requested, and another driver currently has a callback
 *    registered.
 *
 *    A return value of 0 indicates an error.  Non-zero is success.
 *
 * scsi_free() terminates a connection between a device level driver and
 *    the host adaptor driver.  The host adaptor driver will free any
 *    structures that it does not need.
 *
 *    The callback argument should be the same as that given to
 *    scsi_alloc().  If it does not match, the callback will not be
 *    freed up.
 *
 * scsi_info() issues an inquiry to the given controller, target, and lun,
 *    returning a pointer to the scsi_target_info structure.  It should
 *    be used to see if the device is appropriate to the driver making
 *    the call.
 *
 *    The si_maxq field in the scsi_target_info structure (returned by
 *    scsi_info()) will give queueing information.
 *
 * scsi_abort() aborts the request passed as an argument.  If it is
 *    currently active, an abort message will be sent to the device.
 *    A non-zero return indicates that the abort was successful.  Not
 *    all host adaptors support this function.
 *
 * scsi_reset() issues a bus reset to the controller number requested.
 *    A non-zero return indicates a successful reset.  Not all host
 *    adaptors support this reset function.
 *
 * scsi_driver_table is an array indexed by adaptor (or bus) number.
 *    It returns the adaptor type (a constant defined by the
 *    SCSIDRIVER_XXXX #define's below.  The SCSI_XXXSTART #defines
 *    define the starting adaptor number for the various controller
 *    types.  The SCSI_XXXCOUNT are the number of adaptors (or buses)
 *    for a given controller type.  Both SCSIDRIVER_WD93 and
 *    SCSIDRIVER_WD95 are part of SGI-built SCSI controllers, and so
 *    use the SCSI_SGISTART and SCSI_SGICOUNT #define's.  To give an
 *    example of how this works, on an Everest, the second bus on
 *    jaguar controller 4 would be considered adaptor number 137.
 *    SCSI_JAGSTART is 128 + (controller 4 * 2 buses per controller) +
 *    bus 1 (or the second bus on the controller) is adaptor (or bus)
 *    #137 from the point of view of a SCSI device driver.
 */
struct scsi_request;
extern void (*scsi_command[])(struct scsi_request *req);
extern int (*scsi_alloc[])(u_char c, u_char t, u_char l, int opt, void (*cb)());
extern void (*scsi_free[])(u_char c, u_char t, u_char l, void (*cb)());
extern struct scsi_target_info * (*scsi_info[])(u_char c, u_char t, u_char l);

extern int (*scsi_abort[])(struct scsi_request *req);
extern int (*scsi_reset[])(u_char c);
extern int (*scsi_dump[])(u_char c);

extern u_char scsi_driver_table[];

#define SCSIALLOC_EXCLUSIVE	0x100
#define SCSIALLOC_QDEPTH	0x0FF

/*
 * To find out the driver number of a device, index into scsi_driver_table
 * with the adaptor number.
 */
#define SCSI_TYPE_COUNT	4
#define SCSIDRIVER_NULL	0
#define SCSIDRIVER_WD93	1
#define SCSIDRIVER_JAG	2
#define SCSIDRIVER_WD95	3

/*
 * Defines for number of SCSI buses
 */
#if defined(EVEREST)
#define SCSI_SGISTART	  0
#define SCSI_SGICOUNT	128
#define SCSI_JAGSTART	128
#define SCSI_JAGCOUNT	 16
#define SCSI_MAXCTLR	144
#define SCSI_MAXTARG	 16
#else

#if defined(IP22)
#define SCSI_SGISTART	  0
#define SCSI_SGICOUNT	  2
#define SCSI_MAXCTLR	  2
#define SCSI_MAXTARG	 16
#else

#define SCSI_SGISTART	  0
#define SCSI_SGICOUNT	  1
#define SCSI_MAXCTLR	  1
#define SCSI_MAXTARG	  8
#endif
#endif
#define SCSI_MAXLUN	  8


/*
 * SCSI minor number masks and shifts
 */
#define SCSI_UNIT_MASK	 0xF
#define DKSC_UNIT_SHIFT    4
#define TPSC_UNIT_SHIFT    5

#define SCSI_LUN_MASK	 0x7
#define DKSC_LUN_SHIFT    15
#define TPSC_LUN_SHIFT    15

#define DKSC_CTLR_MASK	0x7F
#define TPSC_CTLR_MASK	0x3F
#define DS_CTLR_MASK	0x7F
#define DKSC_CTLR_SHIFT    8
#define TPSC_CTLR_SHIFT    9
#define DS_CTLR_SHIFT      7

#define DKSC_UNIT(dev) ((dev >> DKSC_UNIT_SHIFT) & SCSI_UNIT_MASK)
#define TPSC_UNIT(dev) ((dev >> TPSC_UNIT_SHIFT) & SCSI_UNIT_MASK)

#define DKSC_LUN(dev) ((dev >> DKSC_LUN_SHIFT) & SCSI_LUN_MASK)
#define TPSC_LUN(dev) ((dev >> TPSC_LUN_SHIFT) & SCSI_LUN_MASK)

#define DKSC_CTLR(dev) ((dev >> DKSC_CTLR_SHIFT) & DKSC_CTLR_MASK)
#define TPSC_CTLR(dev) ((dev >> TPSC_CTLR_SHIFT) & TPSC_CTLR_MASK)
#define DS_CTLR(dev)  ((dev >> DS_CTLR_SHIFT) & DS_CTLR_MASK)

/*
 * SCSI sense defines and message external declarations
 */
#define	SC_NUMSENSE	0x10	/* # of messages in scsi_key_msgtab */
#define	SC_NUMADDSENSE	0x4a	/* # of messages in scsi_addit_msgtab */
extern char *scsi_key_msgtab[], *scsi_addit_msgtab[];

/* base sense error codes */
#define SC_NOSENSE		0x0
#define SC_ERR_RECOVERED	0x1
#define SC_NOT_READY		0x2
#define SC_MEDIA_ERR		0x3
#define SC_HARDW_ERR		0x4
#define SC_ILLEGALREQ		0x5
#define SC_UNIT_ATTN		0x6
#define SC_DATA_PROT		0x7
#define SC_BLANKCHK		0x8
#define SC_VENDUNIQ		0x9
#define SC_COPY_ABORT		0xA
#define SC_CMD_ABORT		0xB
#define SC_EQUAL		0xC
#define SC_VOL_OVERFL		0xD
#define SC_MISCMP		0xE
	
/* some common extended sense error codes */
#define SC_NO_ADD	0x0	/* no extended sense code */
#define SC_NOINDEX	0x1
#define SC_NOSEEKCOMP	0x2
#define SC_WRITEFAULT	0x3
#define SC_NOTREADY	0x4
#define SC_NOTSEL	0x5
#define SC_ECC		0x10
#define SC_READERR	0x11
#define SC_NOADDRID	0x12
#define SC_NOADDRDATA	0x13
#define SC_DEFECT_ERR	0x19
#define SC_WRITEPROT	0x27
#define SC_RESET	0x29

/* Possible status bytes returned after a scsi command */
#define ST_GOOD		 0
#define ST_CHECK	 2
#define ST_COND_MET	 4
#define ST_BUSY		 8
#define ST_INT_GOOD	16
#define ST_INT_COND_MET	20
#define ST_RES_CONF	24

/* Size of different scsi command classes */
#define SC_CLASS0_SZ	 6
#define SC_CLASS1_SZ	10
#define SC_CLASS2_SZ	12


/*
 * SCSI host adapter independent target information structure
 * This structure is used to pass information between the host adapter drivers
 * and the device drivers (disk, tape, etc.).
 */
struct scsi_target_info
{
	u_char	 *si_inq;	/* inquiry data */
	u_char	 *si_sense;	/* sense data from last request sense */
	u_char	  si_maxq;	/* maximum queue depth for driver */
	u_char	  si_qdepth;	/* max queue depth so far */
	u_char	  si_qlimit:1;	/* boolean "max queue depth reached"? */
};
typedef struct scsi_target_info      scsi_target_info_t;

#define SCSI_INQUIRY_LEN	64
#define SCSI_SENSE_LEN		64


/*
 * SCSI request structure
 * This structure is used by a SCSI device driver to make a request to a
 * SCSI host adapter.
 */
struct scsi_request
{
	/* values filled in by device driver */
	u_char	 sr_ctlr;
	u_char	 sr_target;
	u_char	 sr_lun;
	u_char	 sr_tag;	/* first byte of tag message */

	u_char	*sr_command;	/* scsi command */
	ushort	 sr_cmdlen;	/* length of scsi command */
	ushort	 sr_flags;	/* direction of data transfer */
	ulong	 sr_timeout;	/* in seconds */

	u_char	*sr_buffer;	/* location of data */
	uint	 sr_buflen;	/* amount of data to transfer */

	u_char	*sr_sense;	/* where to put sense data in case of CC */
	ulong	 sr_senselen;	/* size of buffer allocated for sense data */
	void	(*sr_notify)();	/* callback pointer */
	void	*sr_bp;		/* usually a buf_t pointer */

	/* spare pointer used by device driver */
	void	*sr_dev;

	/* spare fields used by host adapter driver */
	void	*sr_ha;		/* usually used for linked list of req's */
	void	*sr_spare;	/* used as spare pointer, int, etc. */

	/* results filled in by host adapter driver */
	uint	 sr_status;	 /* Status of command */
	u_char	 sr_scsi_status; /* SCSI status byte */
	u_char	 sr_ha_flags;	 /* flags used by host adaptor driver */
	short	 sr_sensegotten; /* number of sense bytes received; -1 == err */
	uint	 sr_resid;	 /* amount of sr_buflen not transferred */
};
typedef struct scsi_request      scsi_request_t;

/*
 * constants for scsirequest.sr_flags
 */
#define SRF_DIR_IN    0x0001	/* data xfer into memory (DMA write) if set */
#define SRF_FLUSH     0x0002	/* data writeback/inval required */
#define SRF_MAP	      0x0008	/* data must be mapped */
#define SRF_MAPBP     0x0010	/* data must be "mapbp"ed */
#define SRF_AEN_ACK   0x0020	/* acknowledge AEN */

/*
 * constants for scsirequest.sr_status
 */
#define SC_GOOD		0	/* No error */
#define SC_TIMEOUT	1	/* Timeout on scsi bus */
#define SC_HARDERR	2	/* Hardware or scsi device error */
#define SC_PARITY	3	/* Parity error on the SCSI bus during xfer */
#define SC_MEMERR	4	/* Parity/ECC error on host memory */
#define SC_CMDTIME	5	/* the command timed out */
#define SC_ALIGN	6	/* i/o address wasn't properly aligned */
#define SC_ATTN		7	/* unit attention received on other command */
#define SC_REQUEST	8	/* error in request; malformed, for non-
				   existent device; no alloc done, etc. */

#endif
