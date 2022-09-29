/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.7 $"
#ifndef OTYPCNT
#include <sys/open.h>	/* for OTYP stuff */
#endif

#define dev_to_usrsoftc(adap, targ, lu)	(usraid_softc[adap][targ][lu])
#define scsi_lu(dev)			0
#define	partition(minor)		(minor & (USRAID_MAX_PART - 1))

/*
 * Basic blocks are 512 bytes on the UltraStor U144F RAID.
 */
#define usrCHKBB(bytes)	((bytes) & 0x1ff)
#define usrBTOBB(bytes)	((bytes) >> 9)		/* truncates */
#define usrBBTOB(blks)	((blks) << 9)		/* truncates */

#define USRAID_MAXLU	1	/* 1 logical units per phys drive */
#define USRAID_MAX_PART	16	/* partitions per drive */
#define USRAID_MAXQ	32	/* max elements in CTQ to controller */


/*
 * Macros for combining SCSI MSB arrays into integers,
 * and for splitting SCSI MSB arrays into binary.
 */
#define USR_MSB_COMB1(a) (                                                  (a)[0])
#define USR_MSB_COMB2(a) (                                  ((a)[0] << 8) | (a)[1])
#define USR_MSB_COMB3(a) (                 ((a)[0] << 16) | ((a)[1] << 8) | (a)[2])
#define USR_MSB_COMB4(a) (((a)[0] << 24) | ((a)[1] << 16) | ((a)[2] << 8) | (a)[3])

#define B(v,s) ((uchar_t)((v) >> s))
#define USR_MSB_SPLIT1(v,a)                                                (a)[0]=B(v,0)
#define USR_MSB_SPLIT2(v,a)                                 (a)[0]=B(v,8), (a)[1]=B(v,0)
#define USR_MSB_SPLIT3(v,a)                 (a)[0]=B(v,16), (a)[1]=B(v,8), (a)[2]=B(v,0)
#define USR_MSB_SPLIT4(v,a) (a)[0]=B(v,24), (a)[1]=B(v,16), (a)[2]=B(v,8), (a)[3]=B(v,0)


/*
 * Accessed by powers of 2, allows tracking sizes up to 8192 blocks
 */
#define USR_NUM_BLK_STATS 13
struct	usr_blkstats	{
	uint bk_reads[USR_NUM_BLK_STATS];
	uint bk_writes[USR_NUM_BLK_STATS];
};

/*
 * Software state per raid drive.
 */
struct	usraid_softc {
	lock_t		usr_lock;	/* MUTEX to protect structure fields */
	mutex_t		usr_sema;	/* controls access to usr_done sema */
	sema_t		usr_done;	/* for synchronous I/O */
	sv_t		usr_wait;	/* synchronous use waiters */

	struct iobuf	usr_queue;	/* wait queue */
	struct buf	*usr_active;	/* active queue */

	u_char		usr_q_max;	/* max possible depth (= 1 if no CTQ) */
	u_char		usr_q_high;	/* high water mark on queue depth */
	u_char		usr_q_current;	/* current depth of queue */

	struct buf	usr_buf;	/* local buffer */

	scsi_request_t	*usr_subchan;	/* active scsi_requests */
	scsi_request_t	*usr_freereq;	/* list of free scsi_requests */
	ushort		usr_driver;	/* host adapter driver number */

	u_char		*usr_inqtyp;	/* devtype from inquiry data */

	ushort		usr_flags;
#define USR_CHANALLOC	0x01		/* scsi channel allocated */
#define USR_WRTPROT	0x02		/* drive is pseudo write-protected */

	u_char		usr_selflags;	/* for DIOCSELECT; see DIOCSELFLAGS */

	sema_t		usr_maint_act;	/* other maint cmd is active, wait */
	sema_t		usr_maint_wait;	/* wait for this maint cmd to complete */

	scsi_request_t		  *usr_reset_sp;	/* use to clean up after RESET */
	struct usr_log_incwrt_pg  *usr_reset_info;	/* what needs to be cleaned up */
	int			  usr_reset_count;	/* number stripes to be fixed */
	int			  usr_reset_current;	/* current index in array */

	scsi_request_t		  *usr_diags_sp;	/* use to send diags after hdw failure */
	int			  usr_diags_doreset;	/* T/F: process reset when diags done */
	sv_t			  usr_io_wait;		/* IO must wait until cleanup done */
	int			  usr_io_blocking;	/* T/F: all IO must psema(io_wait) */

#ifdef GROT
	scsi_request_t		  *usr_fail_sp;		/* use after disk failure */
	struct usr_units_down_data *usr_fail_info;	/* which disk failed on unit */
#endif /* GROT */

	u_char		usr_sentinel_abort;/* T/F: sentinel being aborted */
	scsi_request_t	*usr_sentinel_sp;/* which is sentinel cmd (NULL==none)*/

	ushort	usr_openparts[USRAID_MAX_PART][OTYPCNT]; /* counts of opens */

	struct volume_header usr_vh;	/* volume header info */
	int		usr_drivesize;	/* number of sectors in drive */

	time_t		usr_iostart;	/* 'start' time for sar measurements */
	struct usr_blkstats usr_blkstats;	/* i/o stats */
};
typedef struct usraid_softc *usraid_usp;

/*
 * For getting config info to user programs that use nlist (sar, rpc.rstatd)
 */
struct usraid_info {
	int	maxctlr;
	int	maxtarg;
};

/*
 * This structure is used by the mode select/sense ioctls to determine
 * where the data is to be sent to/received from.
 */
struct usr_ioctl_data {
	caddr_t	i_addr;
	u_int	i_len;
	u_char	i_page;		/* Page code is used only on mode select */
};

#if _KERNEL  
struct irix5_usr_ioctl_data {
	app32_ptr_t	i_addr;
	u_int	i_len;
	u_char	i_page;		/* Page code is used only on mode select */
};
#endif /* _KERNEL */

/* 
 * Maximum amount of data allowed by an ioctl cmd
 */
#define USRAID_MAX_IOCTL_DATA	4096



/*============================================================================
 * Convenience definitions for standard SCSI commands
 *============================================================================*/

/*
 * Read Command - opcode 0x28
 * Write Command - opcode 0x2a
 * Read/write the specified number of blocks from/to the specified LBA.
 */
struct usr_rw_cmd {
	u_char	opcode;
	u_char	rsvd1;
	u_char	lba[4];
	u_char	rsvd2;
	u_char	length[2];
	u_char	rsvd3;
};

/*
 * Read Capacity Command - opcode 0x25
 * Read the last addressable LBA on the device.
 */
struct usr_readcapacity {
	u_char	opcode;
	u_char	rsvd1[9];
};

/*
 * Test Unit Ready Command - opcode 0x00
 * Ask the device if it is ready to do it's chores.
 */
struct usr_tst_unit_rdy {
	u_char	opcode;
	u_char	rsvd1[5];
};

/*
 * Send Diagnostic Command - opcode 0x1d
 * Ask the device to run diagnostics.
 */
struct usr_send_diag {
	u_char	opcode;
	u_char	rsvd1:5, pageformat:1, rsvd2:1, selftest:1, devoffl:1, unitoffl:1;
	u_char	rsvd3[5];
};

/*
 * Receive Diagnostic Results Command - opcode 0x1c
 * Ask the device to return results of the diagnostic.
 */
struct usr_recv_diag {
	u_char	opcode;
	u_char	rsvd1[5];
};

/*
 * Format Command - opcode 0x04
 * Format the device (aliased to Initialize Drive in the controller).
 */
struct usr_format {
	u_char	opcode;
	u_char	rsvd1[5];
};

/*
 * Start Unit Command - opcode 0x1b
 * Spin up or spin down the device.
 */
struct usr_startunit {
	u_char	opcode;
	u_char	rsvd1:7, immediate:1;
	u_char	rsvd2[2];
	u_char	rsvd3:7, startflag:1;
	u_char	rsvd4;
};

/*
 * Inquiry command - opcode 0x12
 * Inquire of the device its basic nature.
 */
struct usr_inquiry {
	u_char	opcode;
	u_char	lun:3, rsvd1:4, evpd:1;
	u_char	page_code;
	u_char	rsvd2;
	u_char	alloc_len;
	u_char	rsvd3;
};

/*
 * Data returned by the inquiry command (250 bytes).
 */
struct usr_inquiry_data {
	u_char	devqual:3, devtype:5;
	u_char	RMB:1, devtype_mod:7;
	u_char	ISO_vers:2, ECMA_vers:3, ANSI_vers:3;
	u_char	AENC:1, TrmIOP:1, rsvd1:2, response_fmt:4;
	u_char	add_len;
	u_char	rsvd2[2];
	u_char	RelAdr:1, WBus32:1, WBus16:1, Sync:1, Linked:1, rsvd3:1, CadQue:1, SftRe:1;
	u_char	vendor[8];
	u_char	product[16];
	u_char	revision[4];
	u_char	vend_spec1[20];		/* vendor stuff */
	u_char	rsvd4[40];
	u_char	copyright1[64];		/* UltraStor's copyright message */
	u_char	copyright2[70];		/* SGI's copyright message */
	u_char	skeleton_product[16];	/* product ID for skeleton firmware */
	u_char	skeleton_revision[4];	/* revision of skeleton firmware */
};

#define	USR_MAXINQDATA	sizeof(struct usr_inquiry_data)


/*============================================================================
 * Definitions for standard SCSI sense/select commands and data sections
 *============================================================================*/

/*
 * Mode Sense Command - opcode 0x1a
 * Read mode pages from the device.
 */
struct usr_mode_sense {
	u_char	opcode;
	u_char	rsvd1;
	u_char	pagecontrol:2, pagecode:6;
	u_char	rsvd2;
	u_char	alloclen;
	u_char	rsvd3;
};

/*
 * Possible page control field values.
 */
#define CURRENT		0
#define	CHANGEABLE	(0x1 << 6)
#define DEFAULT		(0x2 << 6)
#define SAVED		(0x3 << 6)

/*
 * The data structure returned by the Mode Sense command.
 */
struct usr_mode_sense_data {
	u_char	sense_len;
	u_char	mediatype;
	u_char	wprot:1, reserv0:2, dpofua:1, reserv1:4;
	u_char	bd_len;
	u_char	usr_pages[0x100];	/* make room for max possible page len */
};

/*
 * Mode Select Command - opcode 0x15
 * Write new mode pages to the device.
 */
struct usr_mode_select {
	u_char	opcode;
	u_char	rsvd1:3, pageflag:1, rsvd2:3, savepage:1;
	u_char	rsvd3[2];
	u_char	paramlen;
	u_char	rsvd4;
};

/*
 * The data structure accepted by the Mode Select command.
 */
struct usr_mode_select_data {
	u_char	reserv0;
	u_char	mediatype;
	u_char	wprot:1, reserv1:7;
	u_char	blk_len;		/* This will normally be set to 0 */
	u_char	usr_pages[0x100];	/* make room for max possible page len */
};

/*
 * Log Sense Command - opcode 0x4d
 * Read a log status page from the controller.
 */
struct usr_log_sense {
	u_char	opcode;
	u_char	lun:3, rsvd1:3, ppctl:1, savep:1;
	u_char	pagecontrol:2, pagecode:6;
	u_char	rsvd2[2];
	u_char	paramptr[2];
	u_char	alloclen[2];
	u_char	rsvd3;
};

/*
 * Request Sense Command - opcode 0x03
 * Ask the device what its state is, report reasons for any errors.
 */
struct usr_req_sense {
	u_char	opcode;
	u_char	rsvd1[3];
	u_char	alloclen;
	u_char	rsvd2;
};

/*
 * The basic data structure returned by the Request Sense command.
 */
struct usr_std_sense_data {
	u_char	valid:1, errorcode:7;
	u_char	segment;
	u_char	rsvd1:2, incorrlen:1, rsvd2:1, sensekey:4;
	u_char	info[4];
	u_char	add_senselen;
	u_char	cmd_specific[4];
	u_char	add_sensecode;
	u_char	add_sensequal;
	u_char	fru_code;
	u_char	sksvalid:1, key_spec1:7;
	u_char	key_spec2[2];
};

/*
 * The data returned by the UltraStor U144F RAID controller in response
 * to a hard error on a drive.  The first sense data is for the controller
 * itself, while the second is the sense data obtained from the drive.
 * Generally, when a drive gets a hard error, the controller will do
 * recovery actions, then will return an error to the host so that we
 * can pick up one of these double-lenght blocks telling us what happened
 * and why.
 * NOTE: this must be 48 bytes long. See Logged Sense Log Page.
 */
struct usr_req_sense_data {
	struct usr_std_sense_data	ctlr;
	u_char				extravalid:1, flags:7;
	u_char				channel;	/* channel of disk */
	u_char				target;		/* target ID of disk */
	u_char				rsvd1;
	u_char				phys_lba[4];	/* lba used on phys disk */
	u_char				adap_stat;	/* adapter status */
	u_char				targ_stat;	/* target status */
	struct usr_std_sense_data	drive;
	u_char				rsvd[2];
};

#define	USR_ADAP_STAT_SELTIME	0x91	/* selection timeout: drive not present */
#define	USR_ADAP_STAT_OVERRRUN	0x92	/* data over/under-run: drive xfering wrong amount */

/*
 * Sense codes, additional sense codes, and additional sense
 * code qualifiers as used by the UltraStor U144.
 * The following routine assumes that these are the only combinations
 * that are returned by the controller.
 */
#define USR_NOSENSE			0x00	/* no specific sense info available */
#define   USR_NOADDSENSE		0x00	/* no additional sense info avail */
#define     USR_NOADDQUAL		0x00	/* no additional sense qualifier */

#define USR_ERR_RECOVERED		0x01	/* recovered data */

#define USR_NOT_READY			0x02	/* not ready */
#define   USR_NRDY_LUN			0x04	/* LUN not ready */
#define     USR_NRDY_LUN_INIT		0x02	/* initializing cmd req'd */

#define USR_HARDW_ERR			0x04	/* hardware error */
#define   USR_HDW_UNRECOV		0x11	/* unrecovered error */
#define     USR_HDW_UNRCV_STEPS		0x80	/* recovery steps skipped, retry command */
#define     USR_HDW_UNRCV_2NDFLT	0x81	/* second fault while recovering, fix-integ then retry */
#define     USR_HDW_UNRCV_STREAM	0xfc	/* cannot identify which stream had error */
#define     USR_HDW_UNRCV_UNKNOWN	0xfd	/* cannot identify cause of error */
#define     USR_HDW_UNRCV_TASKPTR	0xfe	/* cannot find recovery task ptr */
#define   USR_HDW_DIAGFAIL		0x40	/* diagnostics failed on part "add_qual" */
#define     USR_HDW_DIAG_SCSICHAN	0x80	/* SCSI chip of chan (info LSB-1) failed at ID (info LSB) */
#define     USR_HDW_DIAG_BUFTEST	0x81	/* buffer test failed at channel (info LSB-1) */
#define   USR_HDW_INTTARGFAIL		0x44	/* internal target failure */
#define     USR_HDW_TARG_FLASHSECT	0x80	/* FLASH sector protection error at page in info */
#define     USR_HDW_TARG_FLASHPROG	0x81	/* FLASH programming error */
#define     USR_HDW_TARG_DOWNLOAD	0x82	/* microcode download failed */
#define     USR_HDW_TARG_PARITY		0x83	/* internal buffer had parity error */

#define USR_ILLEGALREQ			0x05	/* illegal request */
#define   USR_ILL_OPCODE		0x20	/* illegal command opcode */
#define   USR_ILL_LBARANGE		0x21	/* LBA out of range */
#define   USR_ILL_FUNC			0x22	/* illegal function */
#define   USR_ILL_CDBFIELD		0x24	/* invalid field in CDB */
#define   USR_ILL_PARAM			0x26	/* invalid field in param list */
#define     USR_ILL_PRM_VALUE		0x02	/* parameter value invalid */
#define   USR_ILL_HOSTCANT		0x2b	/* cannot execute because host doesn't... */
#define     USR_ILL_HCNT_CTQ		0x80	/* support Command Tag Queueing */
#define   USR_ILL_SEQERR		0x2c	/* command sequence error */
#define     USR_ILL_SEQ_LUN		0x80	/* LUN status mismatch */
#define     USR_ILL_SEQ_NOCMD		0x81	/* no target command to abort */
#define     USR_ILL_SEQ_CMD		0x82	/* already has one in queue */
#define   USR_ILL_OVERLAP		0x4e	/* duplicate TAD ID's found in controller */
#define   USR_ILL_TARGCANT		0x83	/* cannot execute because... */
#define     USR_ILL_TCNT_MICSIG		0x00	/* downloaded microcode has invalid signature */
#define     USR_ILL_TCNT_MICCHK		0x01	/* downloaded microcode has invalid checksum */

#define USR_UNIT_ATTN			0x06	/* unit attention */
#define   USR_ATTN_RESET		0x29	/* device was reset or powered on */
#define   USR_ATTN_PARAMCHG		0x2a	/* parameters changed */
#define     USR_ATTN_PRMCHG_MODE	0x01	/* mode parameters changed */
#define     USR_ATTN_PRMCHG_LOG		0x02	/* log parameters changed */
#define   USR_ATTN_CMDCLEARED		0x2f	/* commands clean by another initiator */
#define   USR_ATTN_DOWNLD		0x3f	/* downloaded microcode */
#define     USR_ATTN_DOWNLD_CHG		0x01	/* new microcode is now in effect */
#define   USR_ATTN_ASYNC_EVENT		0x82	/* asycnhronous event occurred */
#define     USR_ATTN_ASYNC_AUX1		0x00	/* AUX1 input asserted */
#define     USR_ATTN_ASYNC_AUX2		0x01	/* AUX2 input asserted */
#define     USR_ATTN_ASYNC_DISKFAIL	0x02	/* disk failure detected */
#define     USR_ATTN_ASYNC_DISKPFA	0x03	/* disk returned a PFA warning */
#define   USR_ATTN_CANNOT		0x83	/* cannot execute because... */
#define     USR_ATTN_CANT_RESETREQ	0x03	/* RESET required after diagnostic failure */
#define   USR_ATTN_UNFINISHED		0x84	/* unfinished task */
#define     USR_ATTN_UNFIN_STRIPE	0x00	/* stripe integrity munged by RESET/power */

#define USR_VENDUNIQ			0x09	/* vendor unique error (none defined) */

#define USR_CMD_ABORT			0x0b	/* command was aborted by target */
#define   USR_ABRT_CLEARED		0x2f	/* tagged cmd cleared by another initiator */
#define     USR_ABRT_CLRD_BITSET	0x81	/* abort bit set */
#define   USR_ABRT_MAINT		0x80	/* maintenance command failed */
#define   USR_ABRT_BYREQ		0x81	/* abort by request */
#define     USR_ABRT_BYREQ_REPL		0x80	/* replacement received */
#define     USR_ABRT_BYREQ_ABORT	0x81	/* abort bit set */

#define USR_MISCOMPARE			0x0e	/* maintenance command failed */
#define   USR_MISCMP_MAINT		0x80	/* maintenance command failed */
#define     USR_MISCMP_MNT_LBA		0x01	/* failed at LBA in info field */

/*
 * Sense codes, additional sense codes, and additional sense
 * code qualifiers that may be returned by the disk drives in
 * the RAID brick.
 */
#define   USR_ATTN_SPINDLE		0x5c	/* change in spindle sync status */
#define     USR_ATTN_SPINDLE_SYNC	0x01	/* spindles synchronized */
#define     USR_ATTN_SPINDLE_NOTSYNC	0x02	/* spindles not synchronized */


/*
 * Action codes returned by usraid_interpret_sense() to tell
 * the caller what response to take for the error.
 */
#define	USR_DO_NOTHING		0x01	/* error was innocuous, no caller action */
#define	USR_DO_RETRY		0x02	/* error was innocuous, retry command unchanged */
#define	USR_DO_DELAY		0x03	/* at intr level, delay before retrying the cmd */
#define	USR_DO_FAIL		0x04	/* error was fatal to cmd, return error to user */
#define	USR_DO_FATAL		0x05	/* error was immediately fatal, don't even retry it */
#define	USR_DO_RESTART		0x06	/* restart the (maint) cmd with LBA from info field */
#define	USR_DO_RESET		0x07	/* ctlr reset, stripes in transition, do fix integ's */
#define	USR_DO_DIAGS		0x08	/* controller had a hardware fault, run diags */
#define	USR_DO_ABORT		0x09	/* got ^C, abort maint cmd */



/*============================================================================
 * UltraStor U144F Specific Commands
 *============================================================================*/

/*
 * Isolate Channel - opcode 0xc4
 * UnIsolate Channel - opcode 0xc5
 *
 * Isolate channel -
 *    This command maked the specified channel, with all units attached
 *    to it, inactive.  The purpose is to allow the user to replace a
 *    failed unit with a new one.  The effect is exactly the same as if
 *    the disk side SCSI channel had failed.
 *
 * Unisolate channel -
 *    This command maked the specified channel, with all units attached
 *    to it, inactive.  The purpose is to allow the user to replace a
 *    failed unit with a new one.  The effect is exactly the same as if
 *    the disk side SCSI channel had failed.
 */
struct usr_isolate_chan {
	u_char	opcode;
	u_char	rsvd1:3, channel:5;
	u_char	rsvd2[4];
};

/*
 * Return Physical Connection - opcode 0xc7
 *
 * This command returns the following information about the disk connected
 * to the indicated channel and target ID.  A 512 byte block of data is
 * returned to the host in the following format:
 *
 * Target ID - 1 byte
 * Vendor ID (in ASCII) - 8 bytes
 * Product ID (in ASCII) - 16 bytes
 * Revision Level (in ASCII) - 4 bytes
 * Number of Sectors - 4 bytes
 * Block Size - 4 bytes
 * Terminator - 1 byte (0xff)
 *
 * If there is no disk at the given channel and target ID, then an 0xff
 * will be returned as the first byte in the 512 byte block.
 */
struct usr_ret_phys_conn {
	u_char	opcode;
	u_char	target:3, channel:5;
	u_char	rsvd[4];
};
struct usr_phys_conn_data {
	u_char	target;
	u_char	vendor[8];
	u_char	product[16];
	u_char	revision[4];
	u_char	sectors[4];
	u_char	blocksize[4];
	u_char	terminator;
	u_char	rsvd1[512-38];
};

/*
 * Put NVRAM - opcode 0xcd
 * Get NVRAM - opcode 0xce
 * Clear NVRAM - opcode 0xd1
 *
 * Put NVRAM - 
 *    The controller will transfer 4 bytes of data from the host.
 *    They will contain 4 bytes destined for offset 4*set into the
 *    controller's NVRAM.  This command is normally used to write
 *    configuration state into the NVRAM.
 *
 * Get NVRAM - 
 *    The controller will transfer 4 bytes of data to the host.
 *    They will contain the 4 bytes of NVRAM at offset 4*set into the
 *    controller's NVRAM.  This command is normally used to read
 *    saved configuration state in the NVRAM.
 *
 * Clear NVRAM - 
 *    This command clears the "key" byte in the NVRAM so that when
 *    the controller is next power cycled, the entire NVRAM will be
 *    re-initialized to the factory ship state.  All settings or
 *    status stored in the NVRAM will be lost.
 *    NOTE: this command has no data phase, no arguments are required.
 *
 * The NVRAM is accessed in 32 bit segments.  If the NVRAM
 * were an array of 32 bit quantities, the "set" would be the
 * array index of the desired entry.  An "set" of 6 will access
 * bytes 24, 25, 26, and 27 from the NVRAM.
 */
struct usr_xfer_nvram {
	u_char	opcode;
	u_char	addr;
	u_char	rsvd[4];
};
struct usr_nvram_data {
	u_char	bytes[4];
};

/*
 * Return Units Down - opcode 0xcf
 * Clear Units Down - opcode 0xd0
 * Set Units Down - opcode 0xd2
 *
 * Return Units Down - 
 *    The controller will transfer 6 bytes of data to the host.
 *    The data will contain a bitmap showing the units that are
 *    marked as being down.
 *
 * Clear Units Down -
 *    The controller will transfer 6 bytes of data from the host.
 *    The data will contain a bitmap showing the units that are to
 *    be marked as being up again.  This command is normally used
 *    after the host has completed its drive replacement procedure
 *    in order to declare that the drive is now usable.
 *
 * Set Units Down - 
 *    The controller will transfer 6 bytes of data from the host.
 *    The data will contain a bitmap showing the units that are to
 *    be marked as being down.  This command is used to force the
 *    controller to think a physical drive has failed.  The drive
 *    replacement procedure (ie: rebuild) will have to be invoked
 *    in order to declare that the drive is usable again.
 *
 * The first byte of the table shows the number of valid entries
 * in the table, while the rest of the bytes are used as a bitmap
 * covering all the SCSI IDs on all the channels connected to the
 * controller.  A bit that is set marks the corresponding target ID
 * on the corresponding SCSI channel as being down.  For example if
 * channel 1 ID 6 is down, then byte 2 bit 6 will be set.
 *
 * The table is exactly 6 bytes long.
 */
struct usr_units_down {
	u_char	opcode;
	u_char	rsvd[5];
};
struct usr_units_down_data {
	u_char	length;
	u_char	bitmap[5];
	u_char	rsvd[2];
};

/*
 * Sentinel Command - opcode 0xfd
 *
 * This command is used to report asynchronous errors back to the
 * host.  Status for this command will be returned either if there
 * is a pre-defined event or at the end of the specified time value.
 * The timeout is specified in seconds, but 0 is a reserved number
 * indicating that the command should never time out.
 *
 * It is bascially a handle for the controller to pass back error
 * conditions that don't correspond to any outstanding I/O operation,
 * or when it doesn't have any outstanding I/O operations.
 */
struct usr_sentinel {
	u_char	opcode;
	u_char	rsvd1:6, abort:1, rsvd2:1;
	u_char	rsvd3[5];
	u_char	timeout[2];
	u_char	rsvd4;
};

/*
 * Set Pass Through Parameters - opcode 0xfe
 *
 * This command sets up parameters for the Execute Pass Through command
 * that will follow.  It transfers the CDB of the command to be passed
 * through, and related information, to the controller as data.
 */
struct usr_set_passthru {
	u_char	opcode;
	u_char	rsvd[9];
};

/*
 * This is the format of the data passed to the controller as part of
 * the Set Pass Through Parameters command.
 *
 * The "fromdisk" and "todisk" flags are mutually exclusive, and tell the
 * controller whether to expect data transfer from controller to disk
 * ("todisk"), or from disk to controller ("fromdisk"), or none at all.
 *
 * The "cdblen" field tells how long the passed in CDB is.
 *
 * The "channel", "target", and "lun" specify the SCSI device that the
 * CDB is to be passed through to.
 *
 * The "xfer_count" specifies the amount of data, in bytes, to be
 * transferred by this CDB when it is executed.
 *
 * The "passedthrucdb" is the image of the CDB that is to be passed
 * to the channel/target/lun specified above.
 */
struct usr_passthru_cdb {
	u_char	todisk:1, fromdisk:1, rsvd:2, cdblen:4;
	u_char	channel;
	u_char	target;
	u_char	lun;
	u_char	xfer_count[4];
	u_char	passthrucdb[12];
};

/*
 * Execute Pass Through Command - opcode 0xff
 *
 * This command issues the CDB that was set up by the Set Pass Through
 * Parameters command previously.  This command is excecuted in a 
 * single threaded mode, and no other commands will be accepted while
 * this command is executing.
 *
 * If the passed through command needs data as input, it will be
 * transferred from the host in the data phase of this command.
 * If the passed through command generates data as output, it will
 * be transferred to the host in the data phase of this commmand.
 */
struct usr_exec_passthru {
	u_char	opcode;
	u_char	rsvd1[5];
	u_char	timeout;
	u_char	rsvd2[3];
};

/*
 * Download firmware - opcode 0x3b	 (version of "write buffer")
 *
 * This command is used to download new firmware into the FLASH EPROM
 * on the controller.  It is a special case of the "write buffer" cmd.
 *
 * To download firmware, the LUN must be 0, mode 0x05, buffer ID 0,
 * buffer offset 0, and the param list len must be the number of bytes
 * to be downloaded.  After this completes successfully, the new firmware
 * will take effect the next time the controller is power cycled.
 */
struct usr_download {
	u_char	opcode;
	u_char	lun:3, rsvd1:2, mode:3;
	u_char	bufferid;
	u_char	offset[3];
	u_char	param_len[3];
	u_char	rsvd2;
};

/*============================================================================
 * UltraStor U144F Specific "Maintenance" Commands
 *============================================================================*/

/*
 * There can be only one "maintenance" command in process on the
 * controller at any one time.  A log page tells if there is a
 * maintenance command executing on the controller, what type of
 * command it is (eg: rebuild, integrity check, etc), and the first
 * LBA of the stripe just completed by that command.  It is intended
 * that that information be used by the host to manage the operation
 * of maintenance commands on the controller.
 *
 * The maintenance commands all support a particular method for
 * asynchronous operation.  That method uses two bits in the command,
 * the "immediate" bit and the "abort" bit.
 *
 * The "immediate" bit is used to initiate a maintenance operation
 * and have the command return good status immediately.  The
 * maintenance command continues operating in the controller, but
 * the host is not waiting for a response.  The command operates in
 * the normal block-and-wait-for-completion mode if the "immediate"
 * bit is not set.
 *
 * If an error occurs and the "immediate" bit is not set, traditional
 * error processing happens.  If the "immediate" bit is set, a check
 * condition is returned to the host via either the sentinel command
 * or the next available command.  The returned sense information will
 * then indicate that an "immediate" maintenance command has failed,
 * and the LBA of the error.  If no error occurs and the maintenance
 * command finishes successfully, then no direct indication of that
 * completion is given to the host.  In either case, the log page will
 * be updated to show that there are no active maintenance commands.
 *
 * The "abort" bit is used to interrupt a maintenance operation that
 * is currently running on the controller.  If a maintenance command
 * of type "X" is running, sending another "X" command to the controller,
 * this one with the "abort" bit set, will cause the first "X" command
 * to gracefully stop.
 *
 * As an example, if an "integrity check" command is active, sending
 * another "integrity check" command with with the "abort" bit set
 * will cause the controller to stop checking integrity.  
 *
 * If the active maintenance command did not have the "immediate" bit
 * set, it will return with an error (that it was aborted).  If it
 * was started with the "immediate" bit, there will be no indication
 * to the host.
 *
 * If the active maintenance command is aborted, the second command
 * (the one with the "abort" bit set) will return a good completion.
 * If there are no active maintenance commands to abort, or there is
 * some other problem, the second command will return with an error.
 *
 * SGI plans not to use the "immediate" bit, but will use "abort".
 * That should avoid most of the complexity of the above description.
 */


/*
 * Initialize Drive - opcode 0xe0
 * Scan Drive - opcode 0xe1
 * Auto-Rebuild Drive - opcode 0xe2
 * Integrity Check Drive - opcode 0xe3
 * Fix Integrity Drive - opcode 0xe4
 *
 * Initialize Drive -
 *    This command initializes the logical drive to a pattern of 0x00.
 *    It starts from the sector specified in the LBA field and runs
 *    through the end of the drive.  If the initialize fails for some
 *    reason, the LBA of the failure will be returned.
 * 
 *    The SCSI "format" command is an alias (with the LBA defaulted
 *    to 0) for this command.
 *
 * Scan Drive -
 *    This command reads the contents of the logical drive starting at
 *    the specified LBA.  It will run until an error occurs or the
 *    the end of the drive is reached.  If an error occurs, the LBA of
 *    the failure will be returned.
 *
 *    Note that this command does simple reads, basically checking for
 *    unreadable sectors, it does not compare the data against the parity.
 *    See the "check integrity" command.
 *
 * Auto-Rebuild Drive -
 *    This command rebuilds the data of the specified logical drive.
 *    The controller will determine from internal data structures the
 *    physical target-ID/channel of the failed drive, and then will
 *    start reconstructing that drive using the existing data sectors
 *    and the parity sectors.
 *
 *    This command will not complete until either reconstruction is
 *    complete, or an unrecoverable error is encountered on the
 *    non-failed drives.  If the specified LUN is not an array type
 *    or a mirrored drive, this command will return with an error.
 *
 *    NOTE: THIS IS NOT THE NORMAL WAY THAT REBUILDS ARE DONE.
 *    The "forced rebuild" command (below) is the normal way.
 *    This command is really intended as an automatic operation
 *    invoked via a push-button on the controller board, done
 *    without host intervention.  Note that the push-button
 *    operation is not supported.
 *
 * Integrity Check Drive - 
 *    This command initiates an integrity check of the parity sectors
 *    of the indicated logical drive (LUN).  The integrity check
 *    starts at the indicated LBA, and continues through the end of
 *    the drive or until an error occurs.  It will either return a
 *    command complete (the whole drive's integrity is good), or an
 *    error (indicating the address that the failure occurred).
 * 
 *    On a RAID4/RAID5 drive, each stripe is checked to see if the
 *    data and parity correspond.  On a mirrored drive, each sector
 *    is compared to its mirrored sector.
 *
 *    If the specified LUN is not an array type or a mirrored drive,
 *    this command will terminate with an error.
 *
 * Fix Integrity Drive -
 *    This command is used if there was a failure with the Integrity
 *    Check Drive command.
 *
 *    In a RAID4/RAID5 drive, this command will update the parity
 *    block of the stripe specified by the LBA so that the parity
 *    block is correct for the existing data in that stripe.  It will
 *    only operate on one stripe.  In a mirrored drive, it will copy the
 *    specified primary sector(s) contents into the mirrored sector(s).
 *
 *    If the specified LUN is not an array type or a mirrored drive,
 *    this command will terminate with an error.  It will return a
 *    command complete with no error (the stripe has been restored or
 *    the mirrored copies have been made), or with an error (indicating
 *    the address that the failure occured).
 */
struct usr_maint_cmd {
	u_char	opcode;
	u_char	rsvd1:6, abort:1, immediate:1;
	u_char	lba[4];
	u_char	rsvd2[4];
};

/*
 * Forced Rebuild Drive - opcode 0xe5
 *
 * This command rebuilds the data of the specified logical drive.
 * The failed physical unit is not determined by internal data
 * structures but from the channel number and target ID specified
 * in the command.  The command will start reconstructing that
 * drive using the existing data sectors and the parity sectors.
 *
 * This command will not complete until either reconstruction is
 * complete, or an unrecoverable error is encountered on the
 * non-failed drives.  If the specified LUN is not an array type or
 * a mirrored drive, this command will return with an error.
 *
 * NOTE: THIS IS THE NORMAL WAY THAT REBUILDS ARE DONE.
 * The "auto-rebuild" command (above) is NOT the normal way.
 * The other command is really intended as an automatic operation
 * invoked via a push-button on the controller board, done without
 * host intervention.  Note that the push-button operation is not
 * supported.
 */
struct usr_rebuild {
	u_char	opcode;
	u_char	rsvd1:6, abort:1, immediate:1;
	u_char	lba[4];
	u_char	target:3, rsvd2:2, channel:3;
	u_char	rsvd3[3];
};



/*============================================================================
 * UltraStor U144F Specific Log and Mode Pages
 *============================================================================*/

/*
 * Miscellaneous Control Mode Page - mode page 0x3a
 *
 * The "page_len" field tells how many bytes FOLLOW this one in this page.
 * The "utilization" field shows the maximum percentage utilization
 *     of the controller bandwidth to be used by maintenance commands.
 *     The values (somewhat illogically) range from 0 thru 0xff.
 */
#define	USR_SENSE_MISC_PG	0x3a
struct usr_misc_page {
	u_char	xferbytes;
	u_char	rsvd1[3];
	u_char	pagestate:1, rsvd2:1, pagecode:6;
	u_char	page_len;
	u_char	utilization;
	u_char	rsvd3[5];
};

/*
 * Drive Group Definition Mode Page - mode page 0x38
 *
 * The "xferbytes" field tells how many bytes in the whole structure.
 * The "page_len" field tells how many bytes FOLLOW this one in this page.
 * The "raid_width" field tells how many drives are in the RAID.
 * The "stripe_depth" field tells how many logically contiguous sectors
 *     are physically contiguous on a drive before moving to the next
 *     physical drive.
 * The "drive size" field tells how large the resulting logical drive is,
 *     should be a multiple of the stripe_depth.
 * The "parity_type" field tells if parity will be used (yes=1, no=0).
 * The "mirror_type" field tells if mirroring will be used (yes=1, no=0).
 * The "parity_diagram" field shows the layout of the parity information.
 *     It the only thing that distinguishes between RAID4 and RAID5.
 *     For RAID4 on a 4+1 config:  {0, 0, 0, 0, 0}
 *     For RAID5 on a 4+1 config:  {0, 1, 2, 3, 4}
 */
#define	USR_SENSE_DEFN_PG	0x38
struct usr_defn_page {
	u_char	xferbytes;
	u_char	rsvd1[3];
	u_char	pagestate:1, rsvd2:1, pagecode:6;
	u_char	page_len;
	u_char	raid_width;
	u_char	stripe_depth;
	u_char	drive_size[4];
	u_char	parity_type;
	u_char	mirror_type;
	u_char	parity_diagram[5];
	u_char	rsvd3[5];
};

/*
 * Drive Partition Definition Mode Page - mode page 0x39
 *
 * The "xferbytes" field tells how many bytes in the whole structure.
 * The "page_len" field tells how many bytes FOLLOW this one in this page.
 * The "channel*" fields give the channel number of each drive.
 * The "target*" fields give the target ID number of each drive.
 * The "beginning*" fields give the first LBA on that drive to be used
 *     by the partition.
 * The "size*" fields give the number of blocks on that drive to be
 *     used by the partition.
 */
#define	USR_SENSE_PART_PG	0x39
struct usr_part_page {
	u_char	xferbytes;
	u_char	rsvd1[3];
	u_char	pagestate:1, rsvd2:1, pagecode:6;
	u_char	page_len;
	struct {
		u_char	channel;
		u_char	target;
		u_char	base[4];
		u_char	size[4];
		u_char	separator;
	} usr_part_drive[5];
	u_char	rsvd3[3];
};

/*
 * Unit Status Log Page - log page 0x30
 *
 * This page records the status of physical and logical units attached
 * to the controller.
 *
 * The "downs" field records a bit for each physical device attached
 *     to the controller.  A "1" bit says the disk has failed.
 * The "attached" field records a bit for each physical device attached
 *     to the controller.  A "1" bit says the disk is physically present.
 */
#define	USR_LOG_UNITS_PG	0x31
#define	USR_LOG_UNITS_MAX	512
#define USR_LOG_UNITS_SIZE	sizeof(struct usr_log_units_pg)
struct usr_log_units_pg {
	u_char	pagecode;
	u_char	rsvd1[2];
	u_char	rsvd2;
	u_char	downs[8];
	u_char	attached[8];
	u_char	rsvd3[12];
};

/*
 * Incomplete Write Log Page - log page 0x31
 *
 * This page records those stripes that were left in transition because
 * of a bus reset, power cycle, controller fault, or some other reason.
 * The intent is that a "fix integrity" command be run against each of
 * these stripes.
 *
 * The "length" field tells how many bytes FOLLOW this one in this page.
 * The "beg_lba" and "end_lba" fields show the beginning and ending
 *     LBA of the (up to several contiguous) stripes that were left in
 *     transition.  They will always be on stripe boundaries, but may
 *     span more than one stripe.
 * The "incwrt_entry" array continues until "length" is used up.
 */
#define	USR_LOG_INCWRT_PG	0x31
#define	USR_LOG_INCWRT_MAX	512
#define USR_LOG_INCWRT_SIZE	sizeof(struct usr_log_incwrt_pg)
struct usr_log_incwrt_pg {
	u_char	pagecode;
	u_char	rsvd1;
	u_char	length[2];
	struct {
		u_char	beg_lba[4];
		u_char	end_lba[4];
	} stripe[USR_LOG_INCWRT_MAX];
};

/*
 * Maintenance Command Status Log Page - log page 0x32
 *
 * This page shows the status of the last maintenance command submitted
 * to the controller.  It will show good completion (error == 0), or
 * the error that occurred and point to the corresponding sense block
 * in the Logged Sense Log Page.
 *
 * The "length" field tells how many bytes FOLLOW this one in this page.
 * The "*_run" fields show if a particular maint cmd is currently active.
 * The "*_err" fields show is a particular maint cmd had an error.
 * The "lba" field is the LBA of the beginning of the last completed stripe.
 * The "sensetag" field is a tag that must be found in the list of sense
 *     structures in the Logged Sense Log Page.
 */
#define	USR_LOG_MNTCMD_PG	0x32
#define USR_LOG_MNTCMD_SIZE	sizeof(struct usr_log_mntcmd_pg)
struct usr_log_mntcmd_maint_stat {
	u_char	run;		/* cmd is running */
	u_char	err;		/* cmd stopping with an error */
	u_char	lba[4];		/* last good sector */
	u_char	sensetag;	/* tag for associated sense block */
	u_char	rsvd;
};
struct usr_log_mntcmd_pg {
	u_char	pagecode;
	u_char	rsvd1[2];
	u_char	length;
	struct usr_log_mntcmd_maint_stat init_stat;	/* last initialize drive cmd */
	struct usr_log_mntcmd_maint_stat integ_stat;	/* last integrity check cmd */
	struct usr_log_mntcmd_maint_stat freb_stat;	/* last forced rebuild cmd */
	struct usr_log_mntcmd_maint_stat scan_stat;	/* last scan drive cmd */
};

/*
 * Logged Maintenance Sense Log Page - log page 0x33
 *
 * This page stores the request sense information from the various errors
 * recorded in the Maintenance Command Log Page.
 *
 * The "length" field tells how many bytes FOLLOW this one in this page.
 * Each entry in the "sens_block" field must be 48 bytes long.
 */
#define	USR_LOG_MNTSENS_PG	0x33
#define	USR_LOG_MNTSENS_MAX	8
#define USR_LOG_MNTSENS_SIZE	sizeof(struct usr_log_mntsens_pg)
struct usr_log_mntsens_pg {
	u_char	pagecode;
	u_char	rsvd1;
	u_char	length[2];
	struct {
		u_char sensetag;	/* tag to match against page 0x32 */
		u_char rsvd2[3];
		struct usr_req_sense_data sense;
	} queue[USR_LOG_MNTSENS_MAX];
};
