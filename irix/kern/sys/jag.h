/*
 * Structures used in jag (Interphase 4210 Jaguar) driver.
 */
#define JAG_QDEPTH 1			/* err handling broken if > 1 */
#define JAG_CTLRS 8			/* max # of controllers */
#define JAG_UPC 14			/* max units per controller */
#define JAG_LUPC 16			/* logical units per controller */
#define JAG_NCQE (JAG_QDEPTH*JAG_UPC+1)	/* number command queue entries */
#define JAG_DMABURST 0
#define JAG_SELTIMEOUT 250		/* 250 milliseconds */
#define JAG_CMDTIMEOUT 0		/* no command timeout */
#define JAG_VMETIMEOUT 0		/* 100 milliseconds */

/* Master Control Status Block */
struct jag_mcsb
{
	ushort	msr;		/* master status register */
	ushort	mcr;		/* master control register */
	ushort	iqar;		/* interrupt on CQE available */
	ushort	qhdp;		/* CQE head pointer */
	ushort	thaw;		/* thaw work queue register */
	ushort	reserve1;
	ushort	reserve2;
	ushort	reserve3;
};


/* Controller Initialization Block */
struct jag_cib
{
	ushort	ncqe;		/* number of command queue entries */
	ushort	dmaburst;	/* DMA burst count */
	ushort	normal_vec;	/* normal command completion vector */
	ushort	error_vec;	/* errored command completion vector */
	ushort	scsi0_id;	/* ID of Jaguar controller channel 0 */
	ushort	scsi1_id;	/* ID of Jaguar controller channel 1 */
	ushort	crb_offset;	/* offset of Command Response Block */
	ushort	sel_timeout_h;	/* timeout in milliseconds for select */
	ushort	sel_timeout_l;	/* timeout in milliseconds for select */
	ushort	wq0_timeout_h;	/* timeout in 256ms for scsi commands in WQ 0 */
	ushort	wq0_timeout_l;	/* timeout in 256ms for scsi commands in WQ 0 */
	ushort	vme_timeout_h;	/* timeout in 30ms for VME transactions */
	ushort	vme_timeout_l;	/* timeout in 30ms for VME transactions */
	ushort	reserve1;
	ushort	reserve2;
	ushort	crb_memtype;	/* type, size, addr mod for offboard crb */
	ushort	crb_addr_h;	/* address of offboard crb */
	ushort	crb_addr_l;	/* address of offboard crb */
	ushort	err_recovery;	/* flag to enable work queue freezing */
	ushort	reserve3;
};


/* Command Queue Entry */
struct jag_cqe
{
	ushort	qecreg;		/* queue entry control register */
	ushort	iopbaddr;	/* addr for iopbs in HUS */
				/* mem type, size, addr mod for offboard */
	union
	{
		uint	on;
		struct
		{
			ushort tagh;
			ushort tagl;
		} off;
	} tag;
	u_char	iopblen;	/* iopb length */
	u_char	wq_num;		/* work queue number */
	ushort	reserve1;
};


/* SCSI passthru IOPB structure (works for most other IOPB's) */
struct jag_iopb
{
	ushort	cmd;		/* command to do */
	ushort	options;	/* options on command */
	ushort	status;		/* status byte returned from SCSI bus */
	ushort	reserve1;
	u_char	intvector;	/* normal (good) completion vector */
	u_char	errvector;	/* error command completion vector */
	ushort	intlevel;	/* interrupt level */
	ushort	reserve2;
	ushort	iopb_memtype;	/* xfer type, xfer size, addr mode for data */
	uint	addr;		/* address of data */
	uint	length;		/* max length of data to xfer */
	uint	reserve3;
	ushort	reserve4;
	ushort	unit;		/* unit to send command to */
	u_char	scsi[12];	/* actual SCSI command */
};


/* Initialize Work Queue IOPB structure */
struct jag_wqiopb
{
	ushort	cmd;		/* command to do */
	ushort	options;	/* options on command */
	ushort	status;		/* return status */
	ushort	reserve1;
	u_char	intvector;	/* normal (good) completion vector */
	u_char	errvector;	/* error command completion vector */
	ushort	intlevel;	/* interrupt level */
	ushort	ngroup;		/* number of commands to group */
	ushort	reserve2[7];
	ushort	wqnum;		/* work queue number */
	ushort	wopt;		/* work queue options */
	ushort	nslots;		/* number of slots in work queue */
	ushort	priority;	/* priority of this work queue */
	ushort	timeout_h;	/* command timeout for this queue */
	ushort	timeout_l;	/* command timeout for this queue */
	ushort	reserve3;
};


/* Command Response Block */
struct jag_crb
{
	ushort	crsw;		/* Command Response Status Word */
	ushort	reserve1;
	uint	tag;		/* command tag */
	u_char	iopblen;	/* iopb length */
	u_char	wq_num;		/* work queue number */
	ushort	reserve2;
	struct jag_iopb iopb;	/* returned iopb */
};


/* Configuration Status Block */
struct jag_csb
{
	ushort	cougarid;
	u_char	reserve2;
	u_char	product[3];
	ushort	reserve3;
	u_char	reserve4;
	u_char	variation;
	ushort	reserve5;
	u_char	reserve6;
	u_char	fwrev[3];
	ushort	reserve7;
	u_char	fwdate[8];
	ushort	reserve8;
	ushort	bufsize;
	uint	reserve9;
	u_char	id0;		/* SCSI id of controller on channel 0 */
	u_char	id1;		/* SCSI id of controller on channel 1 */
	u_char	lastsel0;	/* SCSI id of most recent selection channel 0 */
	u_char	lastsel1;	/* SCSI id of most recent selection channel 1 */
	u_char	phase0;		/* Phase sense on channel 0 */
	u_char	phase1;		/* Phase sense on channel 1 */
	u_char	reserveA;
	u_char	daughter;	/* Type of daughter board */
	u_char	reserveB;
	u_char	SW_DIP;		/* Status of switches on switch block SW2 */
	ushort	frozenwq;	/* Frozen Work Queues bitmap */
};



/*****************************************************************************
 *
 * Driver software structures and defines
 *
 *****************************************************************************/

/* defines for scsirequest.sr_ha_flags */
#define JFLAG_ODDBYTEFIX	1
#define JFLAG_TIMEOUT		2


/*
 * Jaguar driver equivalent of scsirequest -- once a slot is
 * free, a jagrequest gets matched with a scsirequest.
 */
struct jagrequest
{
	u_char			 active;
	u_char			 ctlr;
	u_char			 unit;
	toid_t			 timeout_id;
	dmamap_t		*dmamap;
	struct scsi_request	*sreq;
	struct jagrequest	*next;
};


struct jagunitinfo
{
	u_char	  number;	/* unit number of this unit (0-13) */
	u_char	  devnum;	/* device number (0-15) */
	ushort	  sense:1,	/* request sense in progress */
		  u_ca:1,	/* contingent allegiance occurred */
		  abortqueue:1,	/* work queues aborted */
		  aenreq:1,	/* need AEN acknowledge */
		  tostart:1,	/* start timeout running */
		  exclusive:1,	/* currently have exclusive access */
		  refcount:10;	/* number of high level drivers using */

	struct scsi_request *reqhead; /* queue commands here normally */
	struct scsi_request *reqtail;

	struct jagrequest *jrfree; /* pointer to free jagrequest structs */

	void	(*sense_callback)();
	struct jagrequest jreq[JAG_QDEPTH];

	struct scsi_target_info tinfo;
};


struct jagctlrinfo
{
	lock_t	  lock;
	lock_t	  auxlock;

	u_char	  number;	/* number of this controller */
	u_char	  vmebusno;	/* number of VME bus ctlr is installed in */
	u_char	  reset[2];	/* set if resetting scsi busses */

	uint	  present:1,	/* flag indicating if ctlr is installed */
		  pagebusy:1,	/* odd-byte fixup buffer use status */
		  intr:1,	/* currently processing intr on this ctlr */
		  ignorexcept:1;/* ignore transfer count exceptions */

	u_char	 *page;		/* odd-byte fixup buffer */

	struct scsi_request *auxhead; /* queue commands here during intr */
	struct scsi_request *auxtail;

	struct jagunitinfo *unit[JAG_UPC]; /* unit info */

	volatile struct jag_mcsb *mcsb; /* short i/o master command/status */
	volatile struct jag_cqe *mce; /* short i/o master command entry */
	volatile struct jag_cqe *cmdq; /* short i/o command queue pointer */
	volatile short *hus;	/* host usable space pointer */
	volatile struct jag_crb *crb; /* off board command response block */
	volatile struct jag_csb *csb; /* configuration status block pointer */
};


/*
 * Command Queue Entry placed in memory.
 */
struct jagmemcqe
{
	volatile struct jag_cqe *cqe_offboard; /* pointer to offboard cqe */
	struct jag_cqe cqe;	/* command queue entry for the unit */
	struct jag_iopb iopb;	/* iopb for the unit */
};
