#ifndef _NSCSI_
#define _NSCSI_
#ident	"$Revision: 3.131 $"
/*
 * Calls from high level driver to low level driver
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
 *    would like to talk to the device.  Currently, three options are
 *    allowed.
 *    SCSIALLOC_EXCLUSIVE causes the device to be allocated
 *    exclusively for the use of the calling driver.  If a device is
 *    allocated for exclusive use, no other driver may access it. Also,
 *    if a device is already in use by another driver, exclusive or not,
 *    an exclusive alloc will fail.
 *    SCSIALLOC_NOSYNC will prevent future synchronous negotiations from
 *    being attempted on this device (where possible; not all adapters
 *    support this capability); only applies when no other driver already
 *    has the device open.  Also see the SRF_NEG_SYNC flag below.
 *    SCSIALLOC_QDEPTH is a mask for an eight bit value
 *    specifying how many commands a device driver would like to
 *    queue (that is, the low bits of opt are used to pass the max
 *    depth the upper layer driver will use).  It is to be
 *    considered advice only; the depth used may be less, but will
 *    never be greater.
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
 *    A return value of SCSIALLOCOK indicates success; any other value
 *    indicates an error.  0 indicates a generic error such as bad
 *    values for the adapter or target; values > 1 typically are an
 *    errno value from errno.h, indicating the cause of the error
 *    (such as EBUSY if SCSIALLOC_EXCLUSIVE is used, and the device
 *    is already allocated), or EINVAL, if a non-null callback is
 *    passed, but a callback is already registered for the device.
 *    Note that this is a change introduced in IRIX 5.1.1; previously
 *    any non-0 return value was success.  The change was made to 
 *    allow the upper level drivers to better handle errors.
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
 *
 * The way this is implemented using the hwgraph is as follows.
 * The controller vertex in the hwgraph has the scsi function pointers
 * stored as  a part of information associated with the vertex.
 * Eg:- /hw/scsi_ctlr/#1/target/#2/lun/#3/...
 *
 *	Let the controller vertex handle be cvhdl pointed to by  #1 &
 *	let the pointer to info hanging off the	controller vertex be cinfo.
 *	Now the scsi functions can be accessed as follows.
 *	cinfo = scsi_ctlr_info_get(cvhdl);
 *
 *	scsi_alloc(..)		SCI_ALLOC(cinfo)(..)
 *	scsi_command(..)	SCI_COMMAND(cinfo)(..)
 * 	scsi_free(..)		SCI_FREE(cinfo)(..)
 * 	scsi_dump(..)		SCI_DUMP(cinfo)(..)
 *	scsi_info(..)		SCI_INQ(cinfo)(..)
 *	scsi_ioctl(..)		SCI_IOCTL(cinfo)(..)
 *	scsi_abort(..)		SCI_ABORT(cinfo)(..)
 *
 *	There are backward pointers which finally get to the controller
 * 	info from the vertices below the controller.
 *	For example scsi_command function can be called from the lun vertex
 * 	handle pointed to by #3 above as follows.
 * 		lun_info = scsi_lun_info_get(lun_vhdl);
 *		SLI_COMMAND(lun_info)(...)
 *
 * 	Sample code for initializing the controller info with these
 * 	function pointers follows:
 *
 *	Allocate the controller info structure. This hangs off the
 *	controller vertex.

 * 	scsi_ctlr_info_t	*ctlr_info = scsi_ctlr_info_init();
 *		.
 *		.
 *		.
 *	Initialize the function pointers in addition to other initializations
 *
 *	SCI_ALLOC(ctlr_info)	= 	your_alloc;
 *	SCI_COMMAND(ctlr_info)	=	your_command;
 *	SCI_FREE(ctlr_info)	= 	your_free;
 *	SCI_DUMP(ctlr_info)	= 	your_dump;	
 *	SCI_INQ(ctlr_info)	= 	your_info;
 *	SCI_IOCTL(ctlr_info)	= 	your_ioctl;
 *		.
 *		.
 *		.
 *	Hang the information off the controller vertex
 *
 *	scsi_ctlr_info_put(ctlr_vhdl,ctlr_info);
 */

extern char   *scsi_adaperrs_tab[];

#define SCSIALLOC_NOSYNC	0x200
#define SCSIALLOC_EXCLUSIVE	0x100
#define SCSIALLOC_QDEPTH	0x0FF

#define SCSIALLOCOK 1	/* succesful return value from scsi_alloc function */

#define SCSIDRIVER_NULL 0

/*
 * Defines for number of SCSI buses
 */
#if defined(EVEREST)
#define SCSI_MAXCTLR		120
#define SCSI_MAXTARG	 	127
#else

#if defined(IP22) || defined(IP26) || defined(IP28)
#define SCSI_MAXCTLR	  	6  
/* why 6? */
/* INDY (0 = base board, 2,3 = GIO/SCSI adapter)                            */
/* INDIGO2 (0,1 base board)  						    */
/* CHAL. S (0 = base board, 2,3 GIO/SCSI adapter#1, 4,5 GIO/SCSI adapter#2) */
#define SCSI_MAXTARG	 	16	/* wide on Indigo2, narrow for Indy */
#else

#if defined(IP20)
#define SCSI_MAXTARG	  	8
#else

#if defined(IP30) || defined(SN)
#define SCSI_MAXTARG	  	127
#else

#if defined(IP32)
#define SCSI_MAXTARG	  	16
#endif /* IP32 */

#endif /* IP30 || SN0 */
#endif /* IP20 */
#endif /* IP22 || IP26 || IP28 */
#endif /* EVEREST */

#define SCSI_MAXLUN	 64

#if defined(EVEREST)
/* used to convert internal ctlr number to external */
#define SCSI_CTLR_SLOTSHIFT	3
#define SCSI_EXT_CTLR(si) ((((si)>>SCSI_CTLR_SLOTSHIFT) * 10) + ((si) & 7))
#define SCSI_INT_CTLR(se) (((se) % 10) + (((se) / 10) << SCSI_CTLR_SLOTSHIFT))
#else
#define SCSI_EXT_CTLR(si) (si)
#define SCSI_INT_CTLR(se) (se)
#endif

/*
 * SCSI sense defines and message external declarations
 */
#define	SC_NUMSENSE	0x10	/* # of messages in scsi_key_msgtab */
#define	SC_NUMADDSENSE	0x71 	/* # of messages in scsi_addit_msgtab */
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
#define SC_MEDIA_ABSENT	0x3a
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
	uint	  si_ha_status; /* SRH_* status bits, if supported */
};
typedef struct scsi_target_info      scsi_target_info_t;

#define SCSI_INQUIRY_LEN	64
#define SCSI_SENSE_LEN		64
#define SCSI_MODESEL_PARAMLIST_LEN	12

/*
 * Definitions for scsi_target_info.si_ha_status -- host adapter status bits
 * Not all host adapter drivers will set or use these bits.
 */
#define SRH_CANTSYNC    0x01	/* sync xfer negotiations attempted, but 
	* device doesn't support sync; BADSYNC will be set if the negotation
	* itself also failed */
#define SRH_SYNCXFR     0x02	/* sync xfer mode in effect; async if not set;
	* async mode is the default for all devscsi devices. */
#define SRH_TRIEDSYNC   0x04	/* did sync negotations at some point.  If
	* SRH_CANTSYNC is not also set, the device supports sync mode. */
#define SRH_BADSYNC     0x08	/* device doesn't handle sync negotations
	* correctly; SRH_CANTSYNC will normally be set whenever this bit
	* is set */
#define SRH_NOADAPSYNC  0x10	/* scsi adapter  doesn't handle sync
	* negotations or system has been configured to not allow sync xfers
	* on this target */
#define SRH_WIDE  0x20	/* scsi adapter supports wide SCSI */
#define SRH_DISC  0x40	/* scsi adapter supports disconnect and is configured
	* for it */
#define SRH_TAGQ  0x80	/* scsi adapter supports tagged queuing and is
	* configured for it */
#define SRH_MAPUSER    0x100	/* host adapter driver can map user addrs */
#define SRH_QERR0      0x200	/* host adapter supports QERR==0 */
#define SRH_QERR1      0x400	/* host adapter supports QERR==1 */
#define SRH_ALENLIST   0x800	/* host adapter understands alenlists */


/*
 * SCSI request structure
 * This structure is used by a SCSI device driver to make a request to a
 * SCSI host adapter.
 *
 * Return values from the host adapter driver are in the fields sr_status,
 * sr_scsi_status, sr_sensegotten, and sr_resid.  The sr_status field
 * indicates the success or failure in circumstances that are outside the
 * traditional SCSI protocol (like HW errors, cable errors, DMA errors, etc.).
 * The sr_scsi_status field contains the SCSI status byte.  If the command
 * generated a check condition, a request sense will be attempted.  If it
 * fails, the sr_status and sr_scsi_status fields will apply to the request
 * sense command, and the sr_sensegotten field will be -1.  If it succeeds,
 * the sr_status and sr_scsi_status fields will be 0, and the sr_sensegotten
 * will contain the number of bytes of sense data received.  The sr_resid
 * field will be the difference between the number of bytes of data expected
 * (in the sr_buflen field) and the number actually transferred in executing
 * the command.
 */
struct scsi_request
{
	/*
	 * values filled in by device driver
	 */
	u_char	 sr_ctlr;
	u_char	 sr_target;
	u_char	 sr_lun;
	u_char	 sr_tag;	/* first byte of tag message */
	vertex_hdl_t sr_lun_vhdl;

	u_char	*sr_command;	/* scsi command */
	ushort	 sr_cmdlen;	/* length of scsi command */
	ushort	 sr_flags;	/* direction of data transfer */
	uint	 sr_timeout;	/* in HZ */

	u_char	*sr_buffer;	/* location of data */
	u_char	*sr_sense;	/* where to put sense data in case of CC */

	uint	 sr_buflen;	/* amount of data to transfer */
	uint	 sr_senselen;	/* size of buffer allocated for sense data */

	void	(*sr_notify)(struct scsi_request *); /* callback pointer */
	void	*sr_bp;		/* usually a buf_t pointer */

	/*
	 * fields used by device driver to reference structures at
	 * notify time or to keep track of retry counts, etc.
	 */
	vertex_hdl_t sr_dev_vhdl;
	void	*sr_dev;

	/*
	 * spare fields used by host adapter driver
	 */
	void	*sr_ha;		/* usually used for linked list of req's */
	void	*sr_spare;	/* used as spare pointer, int, etc. */

	/*
	 * results filled in by host adapter driver
	 */
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
#define SRF_MAPUSER   0x0004	/* data must be "mapuser"ed */
#define SRF_MAP	      0x0008	/* data must be mapped */
#define SRF_MAPBP     0x0010	/* data must be "mapbp"ed */
#define SRF_CONTINGENT_ALLEGIANCE_CLEAR \
		      0x0020 /* allow new command to be issued */
#define SRF_NEG_SYNC  0x0040	/* attempt to negotiate sync xfer mode on
	* this command (if currently async; may be ignored by some
	* adapter drivers, either always, or if the driver has previously
	* failed to negotiate sync xfer mode with this target); this will
	* override the SCSIALLOC_NOSYNC flag to scsi_alloc, if it was
	* specified.  */
#define SRF_NEG_ASYNC 0x0080	/* attempt to negotiate async xfer mode on
	* this command (if currently using sync xfers). */

#define SRF_AEN_ACK   SRF_CONTINGENT_ALLEGIANCE_CLEAR /* compatibility */
#define SRF_PRIORITY_REQUEST \
		      0x0100	/* "priority" scsi request */
#define SRF_ALENLIST  0x0200    /* Use alenlist provided in b_private */

/*
 * constants for scsirequest.sr_status; corresponding text msgs in scsi.c
 */
#define SC_GOOD		0	/* No error */
#define SC_TIMEOUT	1	/* Timeout on scsi bus (no response to selection) */
#define SC_HARDERR	2	/* Hardware or scsi device error */
#define SC_PARITY	3	/* Parity error on the SCSI bus during xfer */
#define SC_MEMERR	4	/* Parity/ECC error on host memory */
#define SC_CMDTIME	5	/* the command timed out (did not complete) */
#define SC_ALIGN	6	/* i/o address wasn't properly aligned */
#define SC_ATTN		7	/* unit attention received on other command
				 * or new command received after a unit attn
				 * without SRF_AEN_ACK set in sr_flags. */
#define SC_REQUEST	8	/* error in request; malformed, for
				 * non-existent device; no alloc done,
				 * sr_notify not set, etc. */
#define NUM_ADAP_ERRS (SC_REQUEST+1)


/*
 * constants for scsirequest.sr_tag
 */
#define	SC_TAG_SIMPLE	0x20	/* Simple tag message */
#define	SC_TAG_HEAD	0x21	/* Head of queue tag message */
#define	SC_TAG_ORDERED	0x22	/* Ordered tag message */


/*
 * Definitions for operations on a SCSI bus or FC loop
 * These structures and #defines are used with the scsi_ioctl interface.
 */
struct scsi_ha_op
{
	uint		sb_opt;    /* command option */
	uint		sb_arg;    /* usually data count */
	uintptr_t	sb_addr;   /* usually user address */
};
typedef struct scsi_ha_op scsi_ha_op_t;

/*
 * Definition of structure filled in by SOP_GET_TARGET_PARMS
 */
struct scsi_parms
{
  uint   sp_selection_timeout;	/* Bus selection timeout (in uS) */
  ushort sp_scsi_host_id;	/* Bus SCSI host ID */
  ushort sp_is_diff:8,		/* Bus is differential, low voltage diff, single ended */
         :8;	
  struct scsi_target_parms {
    ushort stp_is_present:1,	/* Device present at this address */
           :7,
           stp_is_sync:1,	/* Device has negotiated synchronous */
           stp_is_wide:1;	/* Device has negotiated wide */
    ushort stp_sync_period;	/* Synchronous period (in nS) if stp_is_sync */
    ushort stp_sync_offset;	/* Synchronous offset (in nS) if stp_is_sync */
  } sp_target_parms[16];
};
typedef struct scsi_parms scsi_parms_t;

#define SCIO 's'
#define _SCIO(X) ((SCIO << 8) | (X))
/*
 * SCSI bus and Fibrechannel loop commands:
 *   To download firmware, one would use the SOP_SENDDATA operation.
 *   To get operational statistics, use the SOP_GETDATA operation.
 */
#define SOP_RESET	 _SCIO(1)   /* SCSI bus reset */
#define SOP_SCAN	 _SCIO(2)   /* scan for devices (inquiry) */
#define SOP_QUIESCE      	_SCIO(3)   /* queiece bus                   */
#define SOP_UN_QUIESCE   	_SCIO(4)   /* release bus from quesce state */
#define SOP_QUIESCE_STATE   	_SCIO(5)   /* get queisce state             */
#define SOP_MAKE_CTL_ALIAS	_SCIO(6)   /* make controler HWG aliases */
#define SOP_GET_SCSI_PARMS      _SCIO(7)   /* get SCSI bus/target parameters */
#define SOP_SET_BEHAVE          _SCIO(8)   /* set various controller behaviours */

	/* For DEBUGLEVEL, level is sb_arg */
#define SOP_DEBUGLEVEL	 _SCIO(11)	   /* set debug level in host adapter driver */

#define SOP_LIP		 _SCIO(21)	   /* initiate FC lip */
#define SOP_ENABLE_PBC	 _SCIO(22)	   /* Bypass a device -- turn on LRC */
#define SOP_LPB		 SOP_ENABLE_PBC	   /* Bypass a device -- turn on LRC */
#define SOP_DISABLE_PBC	 _SCIO(23)	   /* Unbypass a device -- turn off LRC */
#define SOP_LPE		 SOP_DISABLE_PBC   /* Unbypass a device -- turn off LRC */
#define SOP_LPEALL	 _SCIO(24)         /* Unbypass all devices on loop */
#define SOP_LIPRST	 _SCIO(25)         /* LIP with selective device RESET */

/* SOP_QUIESCE_STATE ioctl write one of the following states to sb_addr */
#define NO_QUIESCE_IN_PROGRESS 	0x1
#define QUIESCE_IN_PROGRESS	0x2
#define QUIESCE_IS_COMPLETE     0x4

/* SOP_SET_BEHAVE behaviour subcodes in sb_opt */
#define LUN_QERR                0x1

/*
 * Encoding of sb_arg when sb_opt == LUN_QERR
 *   00ttllqq   tt = target number
 *              ll = lun number
 *              qq = 0/1 - QERR setting
 */
#define SOP_QERR_TARGET_SHFT   16
#define SOP_QERR_LUN_SHFT      8
#define SOP_QERR_VAL_SHFT      0

	/*
	 * For GETDATA and SENDDATA:
	 *  sb_opt is sub operation,
	 *  sb_arg is count, and
	 *  sb_addr is user address
	 */
#define	SOP_GETDATA	101	/* get data from host adapter driver */
#define SOP_SENDDATA	102	/* send data to host adapter driver */

#define SCSI_DISK_VOL_PARTITION		10   /* volume partition */
#define SCSI_DISK_VH_PARTITION        	8    /* volume header partition */
#define SCSI_DISK_UNDEFINED             '\0'
#define SMFD_DEFAULT			"48"

#if _KERNEL && !_STANDALONE

extern void	subchaninit(vertex_hdl_t, scsi_request_t *, uint);
extern int	cdrom_inquiry_test(u_char *);


/* dev scsi driver prefix */

#define DS_PREFIX			"ds"


/*
 * some useful sprintf formats for getting locator strings in the hardware graph 
 */

#define TARG_LUN_FORMAT			"target/%d/lun/%d"
#define PART_FORMAT			"%s/%d"

#define TARG_FORMAT			"target"
#define TARG_NUM_FORMAT			"target/%d"
#define NODE_PORT_NUM_FORMAT		"node/%llx/port/%llx"
#define LUN_FORMAT			"lun"
#define LUN_NUM_FORMAT			"lun/%d"

#define SPRINTF_PART(part,loc_str)	if (part == SCSI_DISK_VH_PARTITION)        	\
                                                sprintf(loc_str, EDGE_LBL_VOLUME_HEADER); 	\
                                        else if (part == SCSI_DISK_VOL_PARTITION)	\
                                                sprintf(loc_str, EDGE_LBL_VOLUME);		\
					else						\
                                                sprintf(loc_str,PART_FORMAT, EDGE_LBL_PARTITION, part);



/*
 * useful for creating the existing partition
 * vertices for scsi disk devices
 */

#define DKSC_PREFIX			"dksc"

/* max. length of the locator string */

#define LOC_STR_MAX_LEN			50 	



#define	VERTEX_FOUND			1
#define VERTEX_NOT_FOUND		0	

typedef int	ctlr_t;	/* type for the adapter  */
typedef int	targ_t;	/* type for the target */
typedef int	lun_t;	/* type for the lun */




typedef struct scsi_ctlr_info {

	vertex_hdl_t			sci_ctlr_vhdl;	/* vertex handle of the controller */

	/* 
	 * scsi functional interface
	 */

	int				(*sci_alloc)(vertex_hdl_t lun_vhdl,int,void (*cb)());

	void				(*sci_free)(vertex_hdl_t lun_vhdl,void (*cb)());	

	struct scsi_target_info *	(*sci_inq)(vertex_hdl_t lun_vhdl);
 
	int				(*sci_dump)(vertex_hdl_t ctlr_vhdl);
	int				(*sci_ioctl)(vertex_hdl_t 	ctlr_vhdl,
						     unsigned int 	cmd,
						     struct scsi_ha_op	*op);

	void				(*sci_command)(scsi_request_t *);
	
	int				(*sci_abort)(scsi_request_t *);



	__uint64_t			sci_initiator_id; 
	uchar_t				sci_adap;	/* adapter number */	
	void				*sci_info;	/* info maintained for the controller */

} scsi_ctlr_info_t;

/*
 * macros associated with the controller info 
 */
#define SCI_ADAP(p)			(p->sci_adap)  		/* adapter number */
#define SCI_CTLR_VHDL(p)		(p->sci_ctlr_vhdl)	/* controller vertex handle */


#define SCI_ALLOC(p)			(p->sci_alloc)
#define SCI_FREE(p)			(p->sci_free)
#define SCI_INQ(p)			(p->sci_inq)
#define SCI_DUMP(p)			(p->sci_dump)
#define SCI_IOCTL(p)			(p->sci_ioctl)
#define SCI_COMMAND(p)			(p->sci_command)
#define SCI_ABORT(p)			(p->sci_abort)

#define SCI_INFO(p)			(p->sci_info)
/*
 * information maintained for the scsi target vertex
 */
typedef struct scsi_targ_info {
	vertex_hdl_t		sti_targ_vhdl;
	scsi_ctlr_info_t	*sti_ctlr_info;	/* scsi controller info */
	targ_t			sti_targ;	/* unit number */
	void			*sti_info;	/* info maintained for the target */
	uint64_t		sti_node;	/* FC node name */
	uint64_t		sti_port;	/* FC port name */
} scsi_targ_info_t;

/*
 * macros associated with the target info 
 */

#define STI_TARG_VHDL(p)		(p->sti_targ_vhdl)
#define STI_CTLR_INFO(p)		(p->sti_ctlr_info) 	/* controller info */
#define STI_TARG(p)			(p->sti_targ)		/* unit number */	
#define STI_INFO(p)			(p->sti_info)	/* driver specific target info */
#define STI_NODE(p)			(p->sti_node)
#define STI_PORT(p)			(p->sti_port)

/*
 * information associated with a scsi lun vertex
 */
typedef struct scsi_lun_info {
	vertex_hdl_t		sli_lun_vhdl;	/* lun vertex handle */
	scsi_targ_info_t	*sli_targ_info; /* scsi target info */
	lun_t			sli_lun;	/* lun */
	void    		*sli_info;	/* info maintained for the lun */
	void                    *sli_fo_info;   /* pointer to failover info */
} scsi_lun_info_t; 

/*
 * macros associated with the lun info 
 */
#define SLI_LUN_VHDL(p)		(p->sli_lun_vhdl)		/* lun vhdl */
#define SLI_TARG_INFO(p)	(p->sli_targ_info)		/* target info */
#define SLI_LUN(p)		(p->sli_lun)			/* lun */
#define SLI_INFO(p)		(p->sli_info)
#define SLI_FO_INFO(p)		(p->sli_fo_info)

#define SLI_TARG(p)		(STI_TARG(SLI_TARG_INFO(p)))	/* unit number */
#define SLI_NODE(p)		(STI_NODE(SLI_TARG_INFO(p)))
#define SLI_PORT(p)		(STI_PORT(SLI_TARG_INFO(p)))
#define SLI_CTLR_INFO(p)	(STI_CTLR_INFO(SLI_TARG_INFO(p))) 	/* controller info from
									 * lun info pointer
 									 */

#define SLI_CTLR_VHDL(p)	(SCI_CTLR_VHDL(STI_CTLR_INFO(SLI_TARG_INFO(p))))
#define SLI_ADAP(p)		(SCI_ADAP(STI_CTLR_INFO(SLI_TARG_INFO(p))))
                                                                        /* controller number
									 * from lun info ptr
									 */
#define SLI_INQ(p)		(*(SCI_INQ(STI_CTLR_INFO(SLI_TARG_INFO(p)))))
#define SLI_ALLOC(p)		(*(SCI_ALLOC(STI_CTLR_INFO(SLI_TARG_INFO(p)))))
#define SLI_FREE(p)		(*(SCI_FREE(STI_CTLR_INFO(SLI_TARG_INFO(p)))))
#define SLI_COMMAND(p) 		(*(SCI_COMMAND(STI_CTLR_INFO(SLI_TARG_INFO(p)))))
#define SLI_IOCTL(p)		(*(SCI_IOCTL(STI_CTLR_INFO(SLI_TARG_INFO(p)))))

/*
 * information associated with an unit vertex
 * Unit vertices are used by tpsc.
 */
typedef struct scsi_unit_info {
	vertex_hdl_t		sui_unit_vhdl;
	int			sui_sm_created;	/* does the state machine exist for this tape */
	u_char			*sui_inv;	/* inventory info */
	void			*sui_ctinfo;	/* unit specific info */
	mutex_t			sui_opensema;	/* semaphore to single thread opens on a unit */
	scsi_lun_info_t		*sui_lun_info;	/* pointer to lun info */
} scsi_unit_info_t;

/* macros associated with the scsi unit info structure */
#define SUI_UNIT_VHDL(p)	(p->sui_unit_vhdl)
#define SUI_SM_CREATED(p)	(p->sui_sm_created)
#define SUI_INV(p)		(p->sui_inv)
#define SUI_TYPE(p)		(SUI_INV(p)[0] & 0x1f)
#define SUI_CTINFO(p)		(p->sui_ctinfo)
#define SUI_OPENSEMA(p)		(p->sui_opensema)
#define SUI_LUN_INFO(p)		(p->sui_lun_info)
#define SUI_CTLR_VHDL(p)	(SLI_CTLR_VHDL(SUI_LUN_INFO(p)))
#define SUI_ADAP(p)		(SLI_ADAP(SUI_LUN_INFO(p)))
#define SUI_TARG(p)		(SLI_TARG(SUI_LUN_INFO(p)))
#define SUI_LUN_VHDL(p)		(SLI_LUN_VHDL(SUI_LUN_INFO(p)))
#define SUI_LUN(p)		(SLI_LUN(SUI_LUN_INFO(p)))
#define	SUI_NODE(p)		(SLI_NODE(SUI_LUN_INFO(p)))
#define SUI_PORT(p)		(SLI_PORT(SUI_LUN_INFO(p)))

#define SUI_ALLOC(p)		(*(SCI_ALLOC(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(p))))))
                                                        /* alloc function pointer from unit info */
#define SUI_FREE(p)		(*(SCI_FREE(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(p))))))
                                                        /* free function pointer from unit info */
#define SUI_DUMP(p)		(*(SCI_DUMP(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(p))))))
                                                        /* dump function pointer from unit info */
#define SUI_INQ(p)		(*(SCI_INQ(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(p))))))
                                                        /* info function pointer from unit info */
#define SUI_IOCTL(p)		(*(SCI_IOCTL(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(p))))))
                                                        /* ioctl function pointer from unit info */
#define SUI_COMMAND(p)		(*(SCI_COMMAND(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(p))))))

/*
 * information associated with an devscsi vertex
 */
typedef struct scsi_dev_info {
	scsi_lun_info_t	*sdevi_lun_info;
	void		*sdevi_tds;
	vertex_hdl_t	sdevi_dev_vhdl;
	mutex_t		sdevi_tdslock; 
} scsi_dev_info_t;

#define SDEVI_DEV_VHDL(p)	(p->sdevi_dev_vhdl)
#define SDEVI_LUN_INFO(p)	(p->sdevi_lun_info)
#define SDEVI_TDSLOCK(p)	(p->sdevi_tdslock)
#define SDEVI_ADAP(p)		(SLI_ADAP(SDEVI_LUN_INFO(p)))
#define SDEVI_TARG(p)		(SLI_TARG(SDEVI_LUN_INFO(p)))
#define SDEVI_LUN(p)   		(SLI_LUN(SDEVI_LUN_INFO(p)))
#define SDEVI_LUN_VHDL(p)	(SLI_LUN_VHDL(SDEVI_LUN_INFO(p)))
#define SDEVI_NODE(p)		(SLI_NODE(SDEVI_LUN_INFO(p)))
#define SDEVI_PORT(p)		(SLI_PORT(SDEVI_LUN_INFO(p)))

#define SDEVI_ALLOC(dev)	(*(SCI_ALLOC(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))
                                                        /* alloc function pointer from scsi info */
#define SDEVI_FREE(dev)		(*(SCI_FREE(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))
                                                        /* free function pointer from scsi info */
#define SDEVI_DUMP(dev)		(*(SCI_DUMP(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))
                                                        /* dump function pointer from scsi info */
#define SDEVI_INQ(dev)		(*(SCI_INQ(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))
                                                        /* info function pointer from scsi info */
#define SDEVI_IOCTL(dev)	(*(SCI_IOCTL(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))
                                                        /* ioctl function pointer from scsi info */
#define SDEVI_COMMAND(dev)	(*(SCI_COMMAND(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))
                                                        /* command function ptr from scsi info*/

#define SDEVI_ABORT(dev)	(*(SCI_ABORT(STI_CTLR_INFO(SLI_TARG_INFO(SDEVI_LUN_INFO(scsi_dev_info_get(dev)))))))

#define SDEVI_TDS(dev)		(scsi_dev_info_get(dev)->sdevi_tds)

/*
 * information associated with a disk vertex
 */

typedef struct scsi_disk_info {
	vertex_hdl_t		sdi_disk_vhdl;	/* disk vertex handle */
	scsi_lun_info_t		*sdi_lun_info;	/* scsi lun info */
	mutex_t			sdi_dksema;	/* general dksc semaphore */
	void			*sdi_dksoftc;	/* driver specific info */
	void			*sdi_dkiotime;	/* driver specific iotime */
} scsi_disk_info_t;

/*
 * macros associated with this structure
 */

#define SDI_DISK_VHDL(p)	(p->sdi_disk_vhdl)	/* disk vertex handle */	
#define SDI_LUN_INFO(p)		(p->sdi_lun_info)	/* lun info pointer from disk info */
#define SDI_DKSEMA(p)		(p->sdi_dksema)		/* semaphore from disk info */
#define SDI_DKSOFTC(p)		(p->sdi_dksoftc)	/* driver specific info */	
#define SDI_DKIOTIME(p)		(p->sdi_dkiotime)	/* driver specific info */	
#define SDI_LUN_VHDL(p)		(SLI_LUN_VHDL(SDI_LUN_INFO(p)))
                                                        /* lun vertex handle from
							 * disk info 
							 */
#define SDI_CTLR_VHDL(p)	(SCI_CTLR_VHDL(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p)))))
                                                        /* controller vertex handle from
							 * disk info 
							 */

#define SDI_ADAP(p)		(SCI_ADAP(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p)))))
                                                        /* adapter from the disk info 
							 */
#define SDI_TARG(p)		(SLI_TARG(SDI_LUN_INFO(p)))
                                                        /* target number from disk info */
#define SDI_NODE(p)		(SLI_NODE(SDI_LUN_INFO(p)))
#define SDI_PORT(p)		(SLI_PORT(SDI_LUN_INFO(p)))
#define SDI_LUN(p)   		(SLI_LUN(SDI_LUN_INFO(p)))
                                                        /* lun from disk info */
#define SDI_ALLOC(p)		(*(SCI_ALLOC(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p))))))
                                                        /* alloc function pointer from disk info */
#define SDI_FREE(p)		(*(SCI_FREE(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p))))))
                                                        /* free function pointer from disk info */
#define SDI_DUMP(p)		(*(SCI_DUMP(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p))))))
                                                        /* dump function pointer from disk info */
#define SDI_INQ(p)		(*(SCI_INQ(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p))))))
                                                        /* info function pointer from disk info */
#define SDI_IOCTL(p)		(*(SCI_IOCTL(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p))))))
                                                        /* ioctl function pointer from disk info */
#define SDI_COMMAND(p)		(*(SCI_COMMAND(STI_CTLR_INFO(SLI_TARG_INFO(SDI_LUN_INFO(p))))))
                                                        /* command function ptr from disk info*/


/*
 * information associated with a disk partition vertex
 */

typedef struct scsi_part_info {
	vertex_hdl_t		spi_part_vhdl;
	scsi_disk_info_t	*spi_disk_info;	/* scsi disk info */
	uchar_t			spi_part;	/* partition number */
} scsi_part_info_t;

/*
 * macros associated with fields in the part info
 */

#define SPI_PART_VHDL(p)		(p->spi_part_vhdl)
#define SPI_DISK_INFO(p)		(p->spi_disk_info)	/* pointer to disk info */
#define SPI_PART(p)			(p->spi_part)		/* partition number */
#define SPI_DISK_VHDL(p)		(SDI_DISK_VHDL(SPI_DISK_INFO(p)))	

extern void			scsi_device_update(u_char *, vertex_hdl_t);

extern scsi_ctlr_info_t		*scsi_ctlr_info_init(void);
extern void			scsi_ctlr_info_put(vertex_hdl_t,scsi_ctlr_info_t *);
extern scsi_ctlr_info_t		*scsi_ctlr_info_get(vertex_hdl_t);

extern scsi_targ_info_t		*scsi_targ_info_init(void);
extern void			scsi_targ_info_put(vertex_hdl_t,scsi_targ_info_t *);
extern scsi_targ_info_t		*scsi_targ_info_get(vertex_hdl_t);

extern scsi_lun_info_t		*scsi_lun_info_init(void);
extern void			scsi_lun_info_put(vertex_hdl_t,scsi_lun_info_t *);
extern scsi_lun_info_t		*scsi_lun_info_get(vertex_hdl_t);


extern void			scsi_unit_info_put(vertex_hdl_t,scsi_unit_info_t *);
extern scsi_unit_info_t		*scsi_unit_info_get(vertex_hdl_t);

extern void			scsi_dev_info_put(vertex_hdl_t,scsi_dev_info_t *);
extern scsi_dev_info_t		*scsi_dev_info_get(vertex_hdl_t);
extern void			scsi_dev_info_free(vertex_hdl_t);


extern scsi_disk_info_t		*scsi_disk_info_init(void);
extern void			scsi_disk_info_put(vertex_hdl_t,scsi_disk_info_t *);
extern scsi_disk_info_t		*scsi_disk_info_get(vertex_hdl_t);

extern scsi_part_info_t		*scsi_part_info_init(void);
extern void			scsi_part_info_put(vertex_hdl_t,scsi_part_info_t *);
extern scsi_part_info_t		*scsi_part_info_get(vertex_hdl_t);

extern void			dk_namespace_remove(vertex_hdl_t);
extern void			dk_base_namespace_add(vertex_hdl_t, boolean_t);

 
extern int			scsi_targ_vertex_exist(vertex_hdl_t,targ_t,
						       vertex_hdl_t *);
extern int			scsi_node_port_vertex_exist(vertex_hdl_t,
					uint64_t, uint64_t, vertex_hdl_t *);
extern vertex_hdl_t		scsi_targ_vertex_add(vertex_hdl_t,targ_t);
extern vertex_hdl_t		scsi_node_port_vertex_add(vertex_hdl_t,
					uint64_t, uint64_t);
extern vertex_hdl_t		scsi_targ_vertex_remove(vertex_hdl_t,targ_t);
extern vertex_hdl_t		scsi_node_port_vertex_remove(vertex_hdl_t,
					uint64_t, uint64_t);

extern int			scsi_lun_vertex_exist(vertex_hdl_t,lun_t,
						      vertex_hdl_t *);
extern vertex_hdl_t		scsi_lun_vertex_add(vertex_hdl_t,lun_t);
extern vertex_hdl_t		scsi_lun_vertex_remove(vertex_hdl_t,lun_t);


extern int			scsi_disk_vertex_exist(vertex_hdl_t,
						       vertex_hdl_t *);
extern vertex_hdl_t		scsi_disk_vertex_add(vertex_hdl_t,int);
extern void 			scsi_disk_vertex_remove(vertex_hdl_t);

extern int			scsi_part_vertex_exist(vertex_hdl_t,uchar_t,
						       vertex_hdl_t *);
extern vertex_hdl_t		scsi_part_vertex_add(vertex_hdl_t,uchar_t,boolean_t);
extern void 			scsi_part_vertex_remove(vertex_hdl_t,uchar_t );

extern void			scsi_bus_create(vertex_hdl_t);
extern vertex_hdl_t		scsi_lun_vhdl_get(vertex_hdl_t,targ_t,lun_t);
extern vertex_hdl_t		scsi_fabric_lun_vhdl_get(vertex_hdl_t, uint64_t,
					uint64_t, lun_t);

extern uint64_t			fc_short_portname(uint64_t, uint64_t);
extern void			scsi_alias_edge_remove(vertex_hdl_t, char *, char *, char *);

extern int			scsi_driver_register(int,char *);
extern void			scsi_driver_unregister(char *);

struct scsi_driver_reg {
	char *sd_vendor_id;
	char *sd_product_id;
	char *sd_driver_prefix;
	char *sd_pathname_prefix;
};
typedef struct scsi_driver_reg  scsi_driver_reg_s;
typedef struct scsi_driver_reg	*scsi_driver_reg_t;

struct scsi_type_reg {
	char			st_last;
	scsi_driver_reg_t	st_sdr;
};
typedef struct scsi_type_reg	scsi_type_reg_s;
typedef struct scsi_type_reg	*scsi_type_reg_t;

extern scsi_type_reg_s		scsi_drivers[];

extern scsi_driver_reg_t	scsi_device_driver_info_get(u_char *);

#define PREFIX_TO_KEY(_str_)	(((uint_t)(_str_)[0] << 24) | \
				 ((uint_t)(_str_)[1] << 16) | \
				 ((uint_t)(_str_)[2] <<  8) | \
				 ((uint_t)(_str_)[0] <<  0))

extern vertex_hdl_t		scsi_device_add(vertex_hdl_t 	ctlr_vhdl,
						targ_t		targ,
						lun_t		lun);
extern void			scsi_device_remove(vertex_hdl_t	ctlr_vhdl,
						targ_t		targ,
						lun_t		lun);

typedef struct {
	char		*str;
	u_char		offset;
	__psint_t	arg;
} single_inq_str_s;
typedef single_inq_str_s *single_inq_str_t;

typedef struct {
	char		*str1;
	u_char		offset1;
	char		*str2;
	u_char		offset2;
	__psint_t	arg;
} double_inq_str_s;
typedef double_inq_str_s *double_inq_str_t;

/* Locking & Unlocking defines for the scsi_alias_mutex which controls
 * mutual exclusion for all the scsi alias driectories under /hw.
 */
extern	mutex_t			scsi_alias_mutex;
#define	SCSI_ALIAS_LOCK()	mutex_lock(&scsi_alias_mutex, PRIBIO)
#define SCSI_ALIAS_UNLOCK()	mutex_unlock(&scsi_alias_mutex)

#endif /* _KERNEL && !_STANDALONE */

#endif /* _NSCSI_ */
