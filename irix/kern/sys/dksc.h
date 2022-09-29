/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.80 $"
#ifndef OTYPCNT
#	include <sys/open.h>	/* for OTYP stuff */
#endif

/* SCSI disk minor # breakdown; different for different low level
 * SCSI chip drivers.
 *
 * For wd93 and wd95 controllers:
 *	partition is 4 bits (0-3)
 *	target is 4 bits (4-7)
 *	controller is 7 bits (8-14)
 *	lun is 3 bits (15-17) (only lun 0 supported at this time)
 *
 * For VME SCSI controllers (jaguar)
 *	controller is derived from major #
 *	partition is 4 bits (0-3)
 *	target is 3 bits (4-6)
 *  SCSI channel on controller is 1 bit (7)
 *	lun's are not supported on this controller (only LUN 0 allowed)
 */

#define dev_to_softc(adap, targ, lun)	(dkdrv[adap][targ][lun])

/*	these allow the dksc driver to work with different logical block size
	devices.  So all raw accesses will be OK; if/when block code is fixed,
	block devices will just start working.
*/
#define dkCHKBB(bytes) ((bytes)%dk->dk_blksz)
#define dkBTOBB(bytes) ((bytes)/dk->dk_blksz)	/* truncates! used where caller
	should be checking for short count on return anyway. */
#define dkBBTOB(blks) ((blks)*dk->dk_blksz)	/* truncates! used where caller
	should be checking for short count on return anyway. */


/* partitions per drive */
#define DK_MAX_PART	16

/* values for dk_flags */
#define DK_WRTPROT   0x0001	/* drive is write protected */
#define DK_QUEUE     0x0002	/* queueing allowed to drive */
#define DK_WONT_START 0x0004	/* device fails attempts to start */
#define DK_MAPUSER   0x0010	/* can map user addresses */
#define DK_NEEDSTART 0x0020 	/* needs a startunit cmd */
#define DK_NO_RETRY  0x0040 	/* do not retry cmd on error or timeout */
#define DK_NO_SORT   0x0100 	/* do not sort i/o request */
#define DK_RECURSE   0x0200 	/* If set have recursion thru dkrelsubchan */
#define DK_ALENLIST  0x0400	/* Alenlist can be delivered with scsireq */
#define DK_NEED_BLKSZ 0x0800	/* Need to set blocksize */
#define DK_HOLD      0x1000	/* Hold commands to device -- backend callback in progress */
#define DK_NEED_CB   (DK_NEEDSTART | DK_NEED_BLKSZ)
#define DK_FAILOVER  0X2000	/* Path failover -- biodone the dk_queue
				   Reject incoming requests for this device.
				   Set/cleared by failover_switch. */


/* by powers of 2, allows tracking request sizes up to 256 blocks;
 * larger requests are included in last bucket.  This is multiples
 * of blocks for that drive, it is not in terms of any particular
 * block size.  Currently implemented only for dksc scsi disk driver.
 * Returned per drive, not per partition. Only ifdef DEBUG
*/
#define NUM_BLK_BUCKETS 9
struct	disk_blkstats	{
	uint bk_reads[NUM_BLK_BUCKETS];
	uint bk_writes[NUM_BLK_BUCKETS];
};


/*
 * Software state per disk drive.
 */
struct	dksoftc {
	struct volume_header dk_vh;	/* Volume header info */
	struct buf dk_buf;		/* Local buffer */
	sv_t	 dk_wait;		/* Synchronous use waiters */
	struct disk_blkstats *dk_blkstats;	/* i/o stats */
	scsi_request_t *dk_subchan; /* struct scsi_request structures */
	scsi_request_t *dk_freereq; /* list of free scsi_requests */
	u_char	 dk_maxq;	/* max depth q has reached; debug; LATER */
	u_char	 dk_curq;	/* current number of queued commands */
	ushort	 dk_driver;	/* host adapter driver number */
	time_t	 dk_iostart;	/* 'start' time for sar measurements */
	uint	 *dk_drivecap;	/* drive size as returned by readcapacity
		(2nd word is block size at end of media) */
	uint	 dk_blksz;	/* logical (from drive) blocksize of device */
	__uint64_t dk_bytecap;	/* drive capacity in bytes */
	ushort	 dk_openparts[OTYPCNT];	/* map of open partitions */
				/* (counts on DK_MAX_PART being 16 or less), */
				/* one entry for CHR, LYR, BLK, SWP, and MNT */
	short	 dk_lyrcount[DK_MAX_PART]; /* number of layered opens */
	ushort	 dk_flags;
	mutex_t	 dk_lock;	/* Lock to protect structure fields */
	u_char	 dk_selflags;	/* flags for DIOCSELECT; see DIOCSELFLAGS */
	u_char	 dk_qdepth;	/* command queue depth */
	u_char	 dk_ioreq;	/* # of I/O request blocks allocated */
	u_char	*dk_inqtyp;	/* devtype from first byte of inquiry data */
	mutex_t	 dk_sema;	/* controls access to dk_done sema */
	sema_t	 dk_done;	/* for synchronous I/O */
	struct iobuf dk_queue;	/* wait queue */
	struct buf *dk_active;	/* active queue */
	struct buf *dk_retry;	/* deferred retry queue */
	vertex_hdl_t  dk_lun_vhdl;	/* vertex handle of the lun vertex */
	vertex_hdl_t  dk_disk_vhdl;	/* vertex handle of the disk vertex */
	void     (*dk_cb)();	/* Sense callback function */
	u_char	 msel_bd[12]; /* placeholder for blocksize modeselect */
	sv_t	 dk_hold;	/* waiter used when commands are to be held off */
};

/* used for dk_inqtype and dk_drivecap, respectively */
#define DK_INQTYP_SIZE 4
#define DK_DRIVECAP_SIZE 8

/*
 * For getting config info to user programs that use nlist (sar, rpc.rstatd)
 */
struct dkscinfo {
	int	maxctlr;
	int	maxtarg;
	int	jagstart;
};


/*
 * This structure is used by the mode select/get ioctls to determine
 * where the data is to be sent to/recieved from.
 */
struct dk_ioctl_data {
	caddr_t	i_addr;
	u_int	i_len;
	u_char	i_page;		/* Page code is used only on mode select */
};

#if _KERNEL
struct irix5_dk_ioctl_data {
	app32_ptr_t	i_addr;
	u_int	i_len;
	u_char	i_page;		/* Page code is used only on mode select */
};
#endif /* _KERNEL */

/* Maximum amount of data allowed by an ioctl cmd */
#define MAX_IOCTL_DATA	4096

/* These are the possible page codes used by mode sense/get */
#define	ERR_RECOV	0x1
#define	CONN_PARMS	0x2
#define DEV_FORMAT	0x3
#define DEV_GEOMETRY	0x4
#define DEV_FDGEOMETRY	0x5	/* geometry for SMS flexible disks */
#define BLOCK_DESCRIP	0x7	/* TOSHIBA MK156FB only */
#define VERIFY_ERR	0x7	/* SCSI 2 Verify error recovery page */
#define CACHE_SCSI2	0x8	/* SCSI 2 cache control */
#define QUEUE_PARAM	0xA	/* SCSI 2 queue parameters */
#define NOTCH_SCSI2	0xC	/* SCSI 2 notch page */
#define CACHE_CONTR	0x38	/* CDC Wren IV/V/VI drives */
#define ALL		0x3f

/* These are the page control field values recognized by the mode sense cmd */
#define CURRENT		0
#define	CHANGEABLE	(0x1 << 6)
#define DEFAULT		(0x2 << 6)
#define SAVED		(0x3 << 6)

/*
 * These are the pages of the drive parameter set/get commands.
 * Note that all of the struct should be allocated to the full
 * size of the union, since we determine their sizes at run time,
 * due to the possiblity of different page sizes on different
 * mfg drives.  This is normally done by using ptrs to the structs,
 * and setting them to point to the dk_pages member of a struct mode_sense_data.
 * Any 'extra' bytes won't be changed by programs like fx, but most
 * drives require that the pages being set by a mode select be
 * the correct size and have valid data for bytes that aren't
 * changed (usually retrieved via a mode sense into the same buffer).
 */
union dk_pages {
	struct common {
		u_char	pg_code;
		u_char	pg_len;
		u_char	pg_maxlen[0x100-2];	/* make room for max possible page len */
	} common;
	struct err_recov {
		u_char 	e_pgcode;
		u_char	e_pglen;
		u_char	e_err_bits;
		u_char	e_retry_count;
		/* rest are spec'ed in scsi2, not supported by most drives yet */
		u_char	e_rdretry;
		u_char	e_corrspan;
		u_char	e_headoffset;
		u_char	e_strobeoffset;
		u_char	e_rsv0;
		u_char	e_wrretry;
		u_char	e_rsv1;
		u_char	e_recovtime[2];
	} err_recov;
	struct connparms {
		u_char	c_pgcode;
		u_char	c_pglen;
		u_char	c_bfull;
		u_char	c_bempty;
		u_char	c_binacthi;
		u_char	c_binactlo;
		u_char	c_disconhi;
		u_char	c_disconlo;
		u_char	c_reserv[2];
	} cparms;
	struct dev_format {
		u_char	f_pgcode;
		u_char	f_pglen;
		u_char	f_trk_zone[2];
		u_char	f_altsec[2];
		u_char	f_alttrk_zone[2];
		u_char	f_alttrk_vol[2];
		u_char	f_sec_trac[2];
		u_char	f_bytes_sec[2];
		u_char	f_interleave[2];
		u_char	f_trkskew[2];
		u_char	f_cylskew[2];
		u_char	f_form_bits;
		u_char	f_reserved4[3];
	} dev_format;
	struct dev_geometry {	/* page 4 for winchesters */
		u_char	g_pgcode;
		u_char	g_pglen;
		u_char	g_ncyl[3];
		u_char	g_nhead;
		u_char	g_wrprecomp[3];
		u_char	g_reducewrcur[3];
		u_char	g_steprate[2];
		u_char	g_landing[3];
		u_char	g_reserved1:7;
		u_char	g_spindlesync:1;
		u_char	g_rotatoff;
		u_char	g_reserved2;
		u_char	g_rotatrate[2];
		u_char	g_reserved3[2];
	} dev_geometry;
	struct dev_fdgeometry {	/* page 5 for floppies */
		u_char	g_pgcode;
		u_char	g_pglen;
		u_char	g_trate[2];
		u_char	g_nhead;
		u_char	g_spt;
		u_char	g_bytes_sec[2];
		u_char	g_ncyl[2];
		u_char	g_wprcomp[2];
		u_char	g_wrcurr[2];
		u_char	g_steprate[2];
		u_char	g_steppulsewidth;
		u_char	g_headset[2];
		u_char	g_moton;
		u_char	g_motoff;
		u_char	g_trdy:1;
		u_char	g_ssn:1;
		u_char	g_mo:1;
		u_char	g_reserv0:5;
		u_char	g_reserv1:4;
		u_char	g_stpcyl:4;
		u_char	g_wrprecomp;
		u_char	g_headld;
		u_char	g_headunld;
		u_char	g_pin34:4;
		u_char	g_pin2:4;
		u_char	g_pin4:4;
		u_char	g_pin1:4;	/* pin 1 TEAC, reserved on NCR */
		u_char	g_reserv2[4];
	} dev_fdgeometry;
	struct block_descrip {	/* Toshiba 156 FB only */
		u_char	b_pgcode;
		u_char	b_pglen;
		u_char	b_reserved0;
		u_char	b_bdlen;
		u_char	b_density;
		u_char	b_reserved1[3];
		u_int	b_blen;		/* This is actually a 3 byte val; high
			byte reserved */
	} block_descrip;
	struct verify_err {	/* scsi 2 error recovery params */
		u_char v_pgcode;
		u_char v_pglen;
		u_char rsv0:4,
		       eer:1,
		       per:1,
		       dte:1,
		       dcr:1;
		u_char v_retry;
		u_char v_corrspan;
		u_char v_rsv1[5];
		u_char v_recovtime[2];
	} verify_err;
	struct cachescsi2 {	/* scsi 2 cache ctrl page */
		u_char c_pgcode;
		u_char c_pglen;
		u_char c_rsrv:5;
		u_char c_wce:1;
		u_char c_mf:1;
		u_char c_rcd:1;
		u_char c_rdpri:4;
		u_char c_wrpri:4;
		u_char c_predislen[2];
		u_char c_minpre[2];
		u_char c_maxpre[2];
		u_char c_maxpreceil[2];
		u_char c_fsw:1;	/* force seq write; not yet in rev 10H */
		u_char c_rsrv2:1;	/* not yet in rev 10H */
		u_char c_dra:1;	/* disable readahead; not yet in rev 10H */
		u_char c_rsrv3:5;	/* not yet in rev 10H */
		u_char c_numseg;
		u_char c_cachsize[2];	/* cache seg size; not yet in rev 10H */
		u_char c_rsv5;	/* not yet in rev 10H */
		u_char c_ncachsize[3];	/* non-cache seg size; not yet in rev 10H */
	} cachescsi2;
	struct queueparam {	/* scsi 2 queue parameter page */
		u_char q_pgcode;
		u_char q_pglen;
		u_char q_rsrv1:7,
		       q_rlec:1;
		u_char q_algorthm_mod:4,
		       q_rsrv2:2,
		       q_err:1,
		       q_disable:1;
		u_char q_eeca:1,
		       q_rsrv3:4,
		       q_raenp:1,
		       q_uaaenp:1,
		       q_eaenp:1;
		u_char q_rsrv4;
		u_char q_ready_aen_holdoff[2];
	} queueparam;
	struct notch {	/* info for 'zone bit recorded' devices */
		u_char n_pgcode;
		u_char n_pglen;
		u_char n_nd:1;
		u_char n_lpn:1;
		u_char n_rsv0:6;
		u_char n_rsv1;
		u_char n_maxnotch[2];
		u_char n_actnotch[2];
		u_char n_startbound[2];
		u_char n_endbound[2];
		u_char n_pagesnotched[2];
	} notch;
	struct cachectrl {	/* CDC Wren IV/V/VI cache control */
		u_char c_pgcode;
		u_char c_pglen;
		u_char c_ccen:1, c_wie:1, c_ssm:1, c_ce:1, c_ctsize:4;
		u_char c_prefetch;
		u_char c_maxpre;
		u_char c_maxpremult;
		u_char c_minpre;
		u_char c_minpremult;
		u_char c_reserv[8];
	} cachctrl;
};

/* Error recovery flag byte bits for e_err_bits */
#define E_DCR	0x1
#define E_DTE	0x2
#define E_PER	0x4
#define E_RC	0x10
#define E_TB	0x20
#define E_ARRE	0x40
#define E_AWRE	0x80

/* The data structure of the mode sense command */
struct mode_sense_data {
	u_char	sense_len;
	u_char	mediatype;
	u_char	wprot:1, reserv0:2, dpofua:1, reserv1:4;
	u_char	bd_len;
	u_char	block_descrip[8];	/* note that this field will NOT be supplied
		by some drives, so that dk_pages data may actually start at this
		offset;  check the bd_len value! */
	union	dk_pages dk_pages;
};

/* The structure of the mode select command */
struct mode_select_data {
	u_char	reserv0;
	u_char	mediatype;
	u_char	wprot:1, reserv1:7;
	u_char	blk_len;		/* This will normally be set to 0 */
	union	dk_pages dk_pages;
};

struct defect_header {
	u_char	reserved0;
	u_char	format_bits;
	u_char	defect_listlen[2];
};

struct defect_entry {
	u_char	def_cyl[3];
	u_char	def_head;
	u_char	def_sector[4];	/* bytes from index, or sector */
};

/* Prototypes */
extern void	dk_unalloc_softc(struct dksoftc *);
extern int	dk_clr_flags (vertex_hdl_t lun_vhdl, int dk_flags);
extern int	dk_set_flags (vertex_hdl_t lun_vhdl, int dk_flags);
