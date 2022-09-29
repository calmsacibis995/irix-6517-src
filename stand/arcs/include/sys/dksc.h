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

#ident "$Revision: 3.42 $"
#ifndef OTYPCNT
#	include <sys/open.h>	/* for OTYP stuff */
#endif

/* SCSI disk minor # breakdown.
 *	  7     6    5          4              3     2     1     0
 *	++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	+               +                  +                       +
 *	+  Target  ID   +      LUN #       +      Partition        +
 *	+               +                  +                       +
 *	++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#define dev_to_softc(adap, targ, lu)	(dksoftc[adap][targ][lu])
#define scsi_lu(dev)		0

/*	these allow the dksc driver to work with different logical block size
	devices.  So all raw accesses will be OK; if/when block code is fixed,
	block devices will just start working.
*/
#define dkCHKBB(bytes) ((bytes)%dk->dk_blksz)
#define dkBTOBB(bytes) ((bytes)/dk->dk_blksz)	/* truncates! used where caller
	should be checking for short count on return anyway. */
#define dkBBTOB(blks) ((blks)*dk->dk_blksz)

/* Only 1 LU supported per physical disk drive (LUN == 0) */
#define DK_MAXLU	1

/* partitions per drive */
#define DK_MAX_PART	16

/* values for dk_flags */
#define DK_WRTPROT 1	/* drive is write protected */
#define DK_SILENT_NOTREADY 2 /* set when doing test unit ready */

/* by powers of 2, allows tracking sizes up to 128 blocks */
#define NUM_BLK_STATS 8
struct	blkstats	{
	uint bk_reads[NUM_BLK_STATS];
	uint bk_writes[NUM_BLK_STATS];
};

/*
 * Software state per disk drive.
 */
struct	dksoftc {
	struct	volume_header	 dk_vh;		/* Volume header info */
	struct	buf		 dk_buf;	/* Local buffer */
	sema_t			 dk_wait;	/* Synchronous use waiters */
	struct	iobuf		 dk_tab;	/* Strat routine queue header */
	struct	blkstats	 dk_blkstats;	/* i/o stats */
	scsisubchan_t		*dk_subchan;	/* SCSI Subchannel structure */
	unsigned		*dk_drivecap;	/* drive size as returned by
						   readcapacity (2nd word is block
						   size at end of media) */
#define DK_DRIVECAP_SIZE 8
	unsigned dk_blksz;	/* logical (from drive) blocksize of device */
	u_char *dk_sense_data;	/* private buffer for sense data */
#define DK_SENSE_DATA_SIZE 24
	ushort dk_openparts[OTYPCNT];	/* map of open partitions (counts
		on DK_MAX_PART being 16 or less), one entry for
		CHR, LYR, BLK, SWP, and MNT */
	ushort dk_lyrcnt[DK_MAX_PART];	/* lyr open/closes are matched
		pairs, so need a counter for it (ugh) */
	ushort	dk_flags;
	lock_t	dk_lock;		/* Lock to protect structure fields */
	u_char	dk_cmd[SC_CLASS1_SZ];	/* the command packet */
	u_char	dk_selflags;	/* flags for DIOCSELECT; see DIOCSELFLAGS */
	u_char	dk_retry; /* Retry count for cmds */
	u_char	*dk_inqtyp; /* devtype from first byte of inquiry data */
};

/* 
 * We only really need the first byte of inquiry data, but when we do SCSI
 * inquiries on little-endian devices, the significant byte comes in as
 * byte 3.  This needs to be big enough to hold the significant data.
 */
#define DK_INQTYP_SIZE 4

#define	dk_part(minor)	(minor & (DK_MAX_PART - 1))

/*
 * This structure is used by the mode select/get ioctls to determine
 * where the data is to be sent to/recieved from.
 */
struct dk_ioctl_data {
	caddr_t	i_addr;
	u_int	i_len;
	u_char	i_page;		/* Page code is used only on mode select */
};

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
#define CACHE_SCSI2	0x8	/* SCSI 2 cache control; Maxtor 8760, Hitachi 515C */
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
