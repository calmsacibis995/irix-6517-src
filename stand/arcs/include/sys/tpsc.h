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
#ifndef _SYS_TPSC_H_
#define _SYS_TPSC_H_
#ident	"sys/tpsc.h: $Revision: 3.49 $"

/*  This files contains the definitions and data structures specific to
 *  the SCSI tape drives (currently 1/4 inch cartridge, 8mm videotape,
 *  9 track reel, DAT tape).
*/


/* Layout of the INQUIRY data */

/* max inquiry info we accept */
#define	MAX_INQUIRY_DATA	(sizeof (ct_g0inq_data_t))
#define	INQ_PDT_CT	0x01	/* peripheral device type, for tape	*/
#define	MAX_INQ_VID	8	/* # bytes for vendor ID		*/
#define	MAX_INQ_PID	16	/* # bytes for product ID		*/
#define	MAX_INQ_PRL	8	/* # bytes for product revision level	*/

/* NOTE: you can't enlarge this structure for drives that return more
* data without breaking old binaries since some of their data would
* be overwritten by an MTSCSIINQ ioctl.  This structure is used in
* both user programs and in the driver. */
typedef	struct ct_g0inq_data_s {
#ifdef	_MIPSEB
	u_char	id_pqt: 3,	/* peripheral qual type	 */
		id_pdt: 5;	/* peripheral device type	 */
	u_char	id_rmb: 1,	/* removable media bit		 */
		id_dtq: 7;	/* device type qualifier	 */
	u_char	id_iso: 2,	/* ISO version			 */
		id_ecma: 3,	/* ECMA version			 */
		id_ansi: 3;	/* ANSI version			 */
	u_char	id_aenc: 1,	/* supports async event notification */
		id_termiop: 1,	/* supports termiop msg */
		id_rsv0: 2,	/* reserved */
		id_respfmt: 4;	/* response format */
#else	/* _MIPSEL */
	u_char	id_pdt: 5,	/* peripheral device type	 */
	        id_pqt: 3;	/* peripheral qual type	 */
	u_char	id_dtq: 7,	/* device type qualifier	 */
	        id_rmb: 1;	/* removable media bit		 */
	u_char	id_ansi: 3,	/* ANSI version			 */
		id_ecma: 3,	/* ECMA version			 */
		id_iso: 2;	/* ISO version			 */
	u_char	id_respfmt: 4,	/* response format */
		id_rsv0: 2,	/* reserved */
		id_termiop: 1,	/* supports termiop msg */
		id_aenc: 1;	/* supports async event notification */
#endif	/* _MIPSEL */
	u_char	id_ailen;	/* additional inquiry length	 */	
	u_char	id_rsv1;	/* reserved			 */
	u_char	id_rsv2;	/* reserved			 */
		/* next 8 bits valid only if id_respfmt 2 (scsi2) */
#ifdef	_MIPSEB
	u_char	id_reladr: 1,	/* supports relative addressing */
		id_wide32: 1,	/* supports 32 bit wide bus  */
		id_wide16: 1,	/* supports 16 bit wide bus  */
		id_sync: 1,	/* supports sync */
		id_linked: 1,	/* supports linked cmds */
		id_rsv3: 1,	/* reserved	 */
		id_cmdq: 1,	/* supports command queing */
		id_softre: 1;	/* supports soft reset */
#else	/* _MIPSEL */
	u_char	id_softre: 1,	/* supports soft reset */
		id_cmdq: 1,	/* supports command queing */
		id_rsv3: 1,	/* reserved	 */
		id_linked: 1,	/* supports linked cmds */
		id_sync: 1,	/* supports sync */
		id_wide16: 1,	/* supports 16 bit wide bus  */
		id_wide32: 1,	/* supports 32 bit wide bus  */
		id_reladr: 1;	/* supports relative addressing */
#endif	/* _MIPSEL */
	u_char	id_vid[ MAX_INQ_VID ];	/* vendor ID		 */
	u_char	id_pid[ MAX_INQ_PID ];	/* product ID		 */
	u_char	id_prl[ MAX_INQ_PRL ];	/* product revision level*/
} ct_g0inq_data_t;

#ifdef _KERNEL

/*
** Layout of the various areas that receive/xmit info.
**
** Layout of the EXTENDED SENSE data
*/

#define	MAX_SENSE_DATA	(sizeof (ct_g0es_data_t)) /* max sense info	  */

struct exab_as {	/* Exabyte add sense data */
	u_char rs0[4];	/* reserved */
	u_char as;	/* add sense */
	u_char aq;	/* add sense qualifier */
	u_char rs1[2];	/* reserved */
	u_char errs[3];	/* recovered errs */
#ifdef	_MIPSEB
	u_char pf: 1,	/* power fail/reset */
	       bpe: 1,	/* bus parity error */
	       fpe: 1,	/* buffer parity error */
	       me: 1,	/* media error (uncorrectable) */
	       eco: 1,	/* error counter (exerrs) overflow */
	       tme: 1,	/* tape motion error */
	       tnp: 1,	/* no tape in drive */
	       bot: 1;	/* at BOT */
	u_char xfr: 1,	/* disconnect error (while trying to pause transfer) */
	       tmd: 1,	/* error occured during SPACE */
	       wp: 1,	/* write protected */
	       fmke: 1,	/* error while writing FM */
	       ure: 1,	/* data flow underrun from media error */
	       we1: 1,	/* too many retries on write; media error */
	       sse: 1,	/* hardware failure in servo system */
	       fe: 1;	/* hardware failure in data formatter */
	u_char rs2: 6,	/* reserved */
	       wseb: 1,	/* write splice error, blank tape (hardware) */
	       wse0: 1;	/* write splice error, overshoot (hardware) */
#else	/* _MIPSEL */
	u_char bot: 1,	/* at BOT */
	       tnp: 1,	/* no tape in drive */
	       tme: 1,	/* tape motion error */
	       eco: 1,	/* error counter (exerrs) overflow */
	       me: 1,	/* media error (uncorrectable) */
	       fpe: 1,	/* buffer parity error */
	       bpe: 1,	/* bus parity error */
	       pf: 1;	/* power fail/reset */
	u_char fe: 1,	/* hardware failure in data formatter */
	       sse: 1,	/* hardware failure in servo system */
	       we1: 1,	/* too many retries on write; media error */
	       ure: 1,	/* data flow underrun from media error */
	       fmke: 1,	/* error while writing FM */
	       wp: 1,	/* write protected */
	       tmd: 1,	/* error occured during SPACE */
	       xfr: 1;	/* disconnect error (while trying to pause transfer) */
	u_char wse0: 1,	/* write splice error, overshoot (hardware) */
	       wseb: 1,	/* write splice error, blank tape (hardware) */
	       rs2: 6;	/* reserved */
#endif	/* _MIPSEL */
	u_char rs3;	/* reserved */
	u_char left[3];	/* remaining tape in 1K blocks to LEOT */
};

struct ciph_as {	/* Cipher add sense data */
	u_char as;	/* add sense  */
	u_char errs[2];	/* recovered errs */
};

struct viper_as {	/* Viper add sense data */
	u_char copysrc;	/* copy source sense data pointer */
	u_char copydst;	/* copy destination sense data pointer */
	u_char rs0[2];	/* reserved */
	u_char errs[2];	/* recovered errs */
	u_char copyst;	/* copy target status*/
	u_char copysns[8];	/* copy target sense data */
};

struct tand_as {	/* Tandberg 36[246]0 add sense data */
	u_char copysrc;	/* copy source sense data pointer */
	u_char copydst;	/* copy destination sense data pointer */
	u_char rs0[2];	/* reserved */
	u_char errs[2];	/* recovered errs */
#ifdef	_MIPSEB
	u_char ercl: 4, ercd: 4;	/* err class, and code */
#else	/* _MIPSEL */
	u_char ercd: 4, ercl: 4;	/* err class, and code */
#endif	/* _MIPSEL */
	u_char exercd;	/* extended error code */
	u_char blkctr[3];	/* i/o blk cnt since open or space */
	u_char fmctr[2];	/* FM cnt since open or space */
	u_char underrun[2];	/* underrun cnt since open or space */
	u_char margblk;	/* marginal blks cnt since open or space */
	u_char blksbuf;	/* blocks unwritten in buffer after err */
	/* up to 20 copy sense bytes follow, but we don't implement COPY */
};

struct kenn_as {	/* Kennedy add sense data */
	u_char rs0[4];	/* reserved */
	u_char as;	/* add sense */
	u_char aq;	/* add sense qualifier */
	u_char rs1[2];	/* reserved */
};

struct scsi2_as {	/* DAT and other drives in scsi2 mode */
	u_char cmdspecific[4];	/* cmd specific; usually used only
			for COPY cmd */
	u_char as;	/* add sense */
	u_char aq;	/* add sense qualifier */
	u_char fru;	/* field replaceable unit code (0 non-specific) */
#ifdef	_MIPSEB
	u_char sksv: 1,	/* sense key specific valid */
	       cd: 1,	/* err in cmd if set, otherwise in data out */
	       rsv: 2,	/* reserved */
	       bpv: 1,	/* if 1, bitptr is valid */
	       bitp: 3;	/* if 1, bitptr is valid */
#else	/* _MIPSEL */
	u_char bitp: 3,	/* if 1, bitptr is valid */
	       bpv: 1,	/* if 1, bitptr is valid */
	       rsv: 2,	/* reserved */
	       cd: 1,	/* err in cmd if set, otherwise in data out */
	       sksv: 1;	/* sense key specific valid */
#endif	/* _MIPSEL */
	u_char sensespecific[2];	/* if sksv, cmd/err specific */
};

typedef struct
{
#ifdef	_MIPSEB
	u_char	esd_valid: 1,	/* valid bit		*/
		esd_errclass: 3,	/* error class		*/
		esd_mbz0: 3,	/* must be zero		*/
		esd_defer: 1;	/* deferred err from prev cmd */
	u_char	esd_copyseg;		/* seg from COPY cmd*/
	u_char	esd_fm: 1,	/* file mark		*/
		esd_eom: 1,	/* logical EOM (CT_EW)  */
		esd_ili: 1,       /* illegal length indicator */
		esd_reserve: 1,	/* unused for now */
		esd_sensekey: 4;	/* sense key		*/
#else	/* _MIPSEL */
	u_char	esd_defer: 1,	/* deferred err from prev cmd */
		esd_mbz0: 3,	/* must be zero		*/
		esd_errclass: 3,	/* error class		*/
	      	esd_valid: 1;	/* valid bit		*/
	u_char	esd_copyseg;		/* seg from COPY cmd*/
	u_char	esd_sensekey: 4,	/* sense key		*/
		esd_reserve: 1,	/* unused for now */
		esd_ili: 1,       /* illegal length indicator */
		esd_eom: 1,	/* logical EOM (CT_EW)  */
	      	esd_fm: 1;	/* file mark		*/
#endif	/* _MIPSEL */
	u_char	esd_resid[4];		/* residual length	*/
	u_char	esd_aslen;		    /* additional sense length	*/
	union {
		struct exab_as _exab;	/* Exabyte */
		struct ciph_as _ciph;	/* Cipher 540 */
		struct tand_as _tand;	/* Tandberg 36[246]0 */
		struct viper_as _viper;	/* Viper 150,60 */
		struct kenn_as _kenn;	/* Kennedy 96X2 */
		struct scsi2_as _rsscsi2;	/* SCSI 2 DAT, etc. */
	} asd;
} ct_g0es_data_t;

#define exab	asd._exab
#define ciph	asd._ciph
#define tand	asd._tand
#define viper	asd._viper
#define kenn	asd._kenn
#define rsscsi2	asd._rsscsi2

/*
** Layout of the INQUIRY data
*/


/*	used to set tape type from the inquiry cmd and info from master.d/tpsc.
	This allows SGI and users to add new tape drives of known types without
	re-compiling the driver.  The assumption is that the SCSI commands,
	block size, capablities, and request sense info are constant within
	a type.  This is rarely completely true, but is often close enough
	to allow a drive to work.
*/
struct tpsc_types {
	u_char tp_type;	/* a type from the tape types below, used for
		some capability, and some error code decoding */
	u_char tp_hinv;	/* a type from the TP* tape types in invent.h;
		is returned by the MTCAPABILITY ioctl */
	u_char tp_vlen;	/* length of vendor string */
	u_char tp_plen;	/* length of product string */
	u_char tp_vendor[MAX_INQ_VID];	/* vendor ID */
	u_char tp_product[MAX_INQ_PID];	/* product ID */
	u_char tp_msl_pglen;	/* # of extra bytes transferred on modeselect
		(does not include MSD_IND_SIZE).  page 0 is only page done
		currently, except for the partition stuff for DAT, which
		follows SCSI 2 and so should not require any changes. */
	u_char *tp_msl_data;	/* values to ALWAYS put in msld_vend when
		doing a mode select; ptr should be null if tp_msl_pglen is 0 */
	u_char tp_density[4];	/* values for 4 possible densities; used
		only when MTCAN_SETDEN is true; indexed by DENSITY_DEV() */
	ulong  tp_capablity;	/* capablities supported */
	ulong  tp_xfr_divisor;	/* transfer timeout; see XFER_TIMEO
		(units are inverse ticks) */
	ulong  tp_mintimeo;	/* minimum timeout for any cmd; see
		XFER_TIMEO and SHORT_TIMEO (units are seconds) */
	ulong  tp_spacetimeo;	/* space cmd timeout; (units are in seconds),
		see SPACE_TIMEO() */
	ulong  tp_rewtimeo;	/* timeout for very long operations, like
		rewind, retension, erase; (units are in seconds);
		time is doubled for retension. */
	ulong  tp_dfltblksz;	/* default (and often only) block size in
		fixed block mode */
	ulong  tp_recblksz;	/* recommended blocking factor, used only for 
		MTIOCGETBLKINFO ioctl; note that it is given in bytes,
		not as a multipier for the blocksize. */
};


/*
** Layout of the MODE SELECT Parameter List
*/

/* maximum amount of mode select info we send */
#define	MAX_MODE_SEL_DATA	(sizeof (ct_g0msl_data_t))
#define	MSD_IND_SIZE	12	/* # bytes in vendor independent mode select (no
	page data, just header and block descr) */
#define	MSLD_BDLEN	0x08	/* block descriptor cnt		*/
#define	MSLD_BLKCNT_SZ	3	/* number of blocks		*/
#define	MSLD_BLKLEN_SZ	3	/* length of a block		*/
#define MS_VEND_LEN	0x10	/* DAT is now largest */

typedef struct
{
	/* 4 byte header */
	u_char	msld_mbz0;	/* reserved, must be 0			*/
	u_char	msld_mbz1;	/* reserved, must be 0			*/
#ifdef	_MIPSEB
	u_char	msld_mbz2: 3,	/* reserved, must be 0			*/
		msld_bfm: 1,	/* buffered/unbuffered mode		*/
		msld_speed: 4;	/* speed				*/
#else	/* _MIPSEL */
	u_char	msld_speed: 4,	/* speed				*/
		msld_bfm: 1,	/* buffered/unbuffered mode		*/
		msld_mbz2: 3;	/* reserved, must be 0			*/
#endif	/* _MIPSEL */
	u_char	msld_bdlen;	/* block descriptor length		*/
	/* 8 byte block descr if bdlen is non-zero */
	u_char	msld_descode;	/* density code				*/
	u_char	msld_blkcnt[ MSLD_BLKCNT_SZ ];	/* number of blocks	*/
	u_char	msld_mbz3;	/* reserved, must be 0			*/
	u_char	msld_blklen[ MSLD_BLKLEN_SZ ];	/* block size		*/
	/* page data */
	u_char  msld_vend[ MS_VEND_LEN ];
} ct_g0msl_data_t;

/*
** Layout of the MODE SENSE Data
*/

/* maximum amount of mode sense info we accept */
#define	MAX_MODE_SENS_DATA	(sizeof (ct_g0ms_data_t))
#define	MSD_BDLEN	0x08	/* block descriptor length	*/
#define	MSD_BLKCNT_SZ	3	/* number of blocks		*/
#define	MSD_BLKLEN_SZ	3	/* length of a block		*/

typedef struct
{
	/* 4 byte header */
	u_char	msd_len;	/* sense data length		*/
	u_char	msd_mtype;	/* medium type (0?)		*/
#ifdef	_MIPSEB
	u_char	msd_wrp: 1,	/* write protect		*/
		msd_bfm: 3,	/* buffered/nonbuffered mode	*/
		msd_speed: 4;	/* speed			*/
#else	/* _MIPSEL */
	u_char	msd_speed: 4,	/* speed			*/
		msd_bfm: 3,	/* buffered/nonbuffered mode	*/
		msd_wrp: 1;	/* write protect		*/
#endif	/* _MIPSEL */
	u_char	msd_bdlen;	/* block descriptor length		*/
	/* 8 byte block descr if bdlen non-zero */
	u_char	msd_descode;	/* density code				*/
	u_char	msd_blkcnt[ MSD_BLKCNT_SZ ];	/* number of blocks	*/
	u_char	msd_mbz0;	/* reserved, must be 0			*/
	u_char	msd_blklen[ MSD_BLKLEN_SZ ];	/* block size		*/
	/* page data */
	u_char  msd_vend[MS_VEND_LEN];
} ct_g0ms_data_t;

/*
** Command Descriptor Block for Group 0 commands.
*/
typedef struct
{
	u_char	g0_opcode;	/* group code/command code	*/
#ifdef	_MIPSEB
	u_char	g0_lu: 3,	/* logical unit number		*/
		g0_cmd0: 5;	/* command dependent 0		*/
#else	/* _MIPSEL */
	u_char	g0_cmd0: 5,	/* command dependent 0		*/
		g0_lu: 3;	/* logical unit number		*/
#endif	/* _MIPSEL */
	u_char	g0_cmd1;	/* command dependent 1		*/
	u_char	g0_cmd2;	/* command dependent 2		*/
	u_char	g0_cmd3;	/* command dependent 3		*/
#ifdef	_MIPSEB
	u_char	g0_vu67: 2,	/* vendor unique bits 6 and 7	*/
		g0_mbz0: 4,	/* must be zero			*/
		g0_flag: 1,	/* flag				*/
		g0_link: 1;	/* link				*/
#else	/* _MIPSEL */
	u_char	g0_link: 1,	/* link				*/
		g0_flag: 1,	/* flag				*/
		g0_mbz0: 4,	/* must be zero			*/
		g0_vu67: 2;	/* vendor unique bits 6 and 7	*/
#endif	/* _MIPSEL */
} ct_g0cdb_t;

/*
** Command Descriptor Block for Group 2 commands.
*/
typedef struct
{
	u_char	g2_opcode;	/* group code/command code	*/
#ifdef	_MIPSEB
	u_char	g2_lu: 3,	/* logical unit number		*/
		g2_cmd0: 5;	/* command dependent 0		*/
#else	/* _MIPSEL */
	u_char	g2_cmd0: 5,	/* command dependent 0		*/
		g2_lu: 3;	/* logical unit number		*/
#endif	/* _MIPSEL */
	u_char	g2_cmd1;	/* command dependent 1		*/
	u_char	g2_cmd2;	/* command dependent 2		*/
	u_char	g2_cmd3;	/* command dependent 3		*/
	u_char	g2_cmd4;	/* command dependent 4		*/
	u_char	g2_cmd5;	/* command dependent 5		*/
	u_char	g2_cmd6;	/* command dependent 6		*/
	u_char	g2_cmd7;	/* command dependent 7		*/
#ifdef	_MIPSEB
	u_char	g2_vu67: 2,	/* vendor unique bits 6 and 7	*/
		g2_mbz0: 4,	/* must be zero			*/
		g2_flag: 1,	/* flag				*/
		g2_link: 1;	/* link				*/
#else	/* _MIPSEL */
	u_char	g2_link: 1,	/* link				*/
		g2_flag: 1,	/* flag				*/
		g2_mbz0: 4,	/* must be zero			*/
		g2_vu67: 2;	/* vendor unique bits 6 and 7	*/
#endif	/* _MIPSEL */
} ct_g2cdb_t;
/*
** Opcodes for Group 0 commands. These opcodes are composed of the
** group code and opcode.
*/
#define	TST_UNIT_RDY_CMD	0x00
#define	REWIND_CMD		0x01
#define	REQ_BLKADDR		0x02
#define	REQ_SENSE_CMD		0x03
#define	REQ_BLKLIM_CMD		0x05
#define	READ_CMD		0x08
#define	WRITE_CMD		0x0a
#define	SEEKBLK_CMD		0x0c
#define	WFM_CMD			0x10
#define	SPACE_CMD		0x11
#define	INQUIRY_CMD		0x12
#define	MODE_SEL_CMD		0x15
#define	RESERV_UNIT_CMD		0x16
#define	REL_UNIT_CMD		0x17
#define	ERASE_CMD		0x19
#define	MODE_SENS_CMD		0x1a
#define	L_UL_CMD		0x1b
#define	PREV_MED_REMOV_CMD	0x1e	/* not on Kennedy */
#define	LOCATEBLK_CMD		0x2b
#define READ_POS_CMD	0x34	/* SCSI 2 (DAT) read position */
#define LOG_SELECT_CMD	0x4C	/* SCSI 2 (DAT) set logging params */
#define LOG_SENSE_CMD	0x4D	/* SCSI 2 (DAT) get logged info */

/*
** LED controll subfunctions 
*/
#define CT_PREV_REMOV		0x01
#define CT_ALLOW_REMOV		0x00

/*
** Space sub-function codes
*/
#define	SPACE_BLKS_CODE	0	/* space blocks code			*/
#define	SPACE_FM_CODE	1	/* space filemarks code			*/
#define	SPACE_SFM_CODE	2	/* space sequential filemarks code	*/
#define	SPACE_EOM_CODE	3	/* space to end of recorded media code	*/
#define	SPACE_SETM_CODE	4	/* space setmarks code	*/

/*
** Load sub-function bits.
*/
#define	L_UL_RETENSION	0x02	/* bit to set in the load/unload CDB to do a */
				/*   retention				     */
#define	L_UL_LOAD	0x01	/* bit to set if you want a load operation   */

/*
**
**	This structure contains the info for each logical unit.
**	The arrangement of this structure is such that a hex dump is
**	elatively easy to interpret, and minimize gaps.
**	sub structures must follow a long or ptr, OR have a
**	long or ptr as first member OR be part of union that has
**	a long if they will be used for DMA, since compiler won't
**	force quad alignment otherwise. (marked with DMA in comments)
*/
typedef struct
{
	union {
		ct_g0cdb_t	g0cdb;	/* group 0 command descriptor block (DMA) */
		ct_g2cdb_t	g2cdb;	/* group 2 command descriptor block (DMA) */
	} ct_cmd;
	u_char	ct_lastcmd;	/* last command issued, except request sense */
	u_char	ct_openflags;	/* flags from current open call */
	u_char ct_partnum;	/* for partitioned tapes, which partition. */
	u_char ct_scsiv;	/* scsi1 or scsi2, from inq */
	u_char ct_cansync;	/* set if scsiv >=2 and id_sync set */
	dev_t	ct_lastdev;	/* dev on last open; for density/mode changes */
	uint ct_sili: 1,	/* suppress illegal length error (MTCAN_SILI) */
	     ct_cipherrec: 1,	/* cipher 540 report recovered errors */
	     ct_speed: 1,	/* high or low speed if MTCAN_SETSP */
	     ct_density: 8;	/* density value for modesel if MTCAN_SETDEN */
	scsisubchan_t	*ct_subchan;	/* pointer to allocated subchan	*/
	struct tpsc_types ct_typ;	/* a copy of the tpsc_type struct, not
		a pointer, because extra code to de-reference really adds up */
	buf_t	*ct_bp;		/* pointer to allocated buf_t   */
	buf_t	*ct_reqbp;		/* buf_t * for reqsense   */
	ulong	ct_state;	/* state of the device		*/
	ulong	ct_tent_state;	/* state of the device; set while performing
		* immediate mode commands such as rew, unload, etc.  When they
		* complete successfully (drive no longer reports BUSY status;
		* no abort command issued, etc.), then ct_tent_state is OR'ed
		* into state and cleared. */
	ulong	ct_recov_err;	/* # recoverable errors		*/
	ulong	ct_blksz;	/* current blksz in bytes for fixed mode */
	ulong	ct_fixblksz;	/* blksz in bytes for fixed mode, set only
		on first open (from dfltblksz), and by SETFIXED ioctl.
		Used when switching between variable and fixed modes */
	ulong	ct_cur_minsz;	/* min size block drive supports, may be diff
		than blkinfo value */
	ulong	ct_cur_maxsz;	/* max size block drive supports, may be diff
		than blkinfo value */
	ulong	ct_readcnt;	/* reads on this open (not bytes) */
	ulong	ct_writecnt;	/* writes on this open (not bytes) */
	ulong	ct_extrabusy;	/* extra 'delay' because an imm rewind in prog */
	ct_g0es_data_t	ct_es_data; /* extended sense info */
	union ct_bufarea { /* info that isn't needed across commands */
		int resid;	/* for ctgetblklen */
		ct_g0inq_data_t	inq_data;	/* inquiry data */
		ct_g0msl_data_t	msl;	/* mode select */
		ct_g0ms_data_t	ms;	/* mode sense */
		u_char	posninfo[20]; /* tmp read buffer for READPOSN */
		u_char	blklenbuf[4]; /* tmp read buffer for GETBLKLEN */
		u_char	reqblkinfo[6]; /* get block limits */
	} *ct_bufarea;
	struct mtblkinfo blkinfo;	/* from mtio.h */
} sa_ct_target_t;

typedef sa_ct_target_t ctinfo_t;	/* keep old name for back compat,
	but use shorter and more descriptive name */

#define ct_g0cdb ct_cmd.g0cdb
#define ct_g2cdb ct_cmd.g2cdb
#define ct_resid ct_bufarea->resid
#define ct_inq_data ct_bufarea->inq_data
#define ct_msl ct_bufarea->msl
#define ct_ms ct_bufarea->ms
#define ct_blklenbuf ct_bufarea->blklenbuf
#define ct_posninfo ct_bufarea->posninfo
#define ct_reqblkinfo ct_bufarea->reqblkinfo


/*
** Masks for the various bits of the minor device number
*/
#define	SCSIID_MSK	0x07	/* mask for SCSI ID of target	*/
#define	HAID_MSK	0x08	/* mask for Host adaptor number	*/
#define	REWIND_MSK	0x10	/* mask for rewind/norewind bit	*/
#define	SWAP_MSK	0x20	/* mask for swap/noswap bit	*/
#define VAR_MSK		0x40	/* mask for fixed/variable bit  */
#define	DRIVE_MSK	(3<<3)	/* mask for drive specific bits, (in
	'real' minor #, not internal form); currently used only for
	density. */

#define	CT_LUNIT	0	/* only lun 0 supported */

/*
** Shift values to convert various bits in the minor device to a number
*/
#define	HAID_SHIFT	3

/*
** Macros for convience sake (use tpsc internal version of minor #)
*/
#define	SCSIID_DEV(dev)		((dev) & SCSIID_MSK)
/* rewind devices rewind on close ONLY, unless media changed,
 * in which case they rewind on next open also */
#define	ISTPSCREWIND_DEV(dev)	(((dev) & REWIND_MSK ) == 0)	
#define	ISSWAP_DEV(dev)		((dev) & SWAP_MSK)
#define ISVAR_DEV(dev)          (((dev) & VAR_MSK) && (ctinfo->ct_typ.tp_capablity&MTCAN_VAR))
#define	DENSITY_DEV(dev)  (((dev) & DRIVE_MSK)>>3) /* if MTCAN_SETDEN */

/*
** Stuff to map a host adaptor logical number to the SCSI ID.
*/
#define	HA_SCSI_ID		0

/*
** Flag value passed by ctcmd() to the specific function routine
*/
#define	CTCMD_SETUP	0	/* setup to issue cdb	*/
#define	CTCMD_ERROR	1	/* error		*/
#define	CTCMD_NO_ERROR	2	/* no error		*/


/*	The transfer timeout needs to be proportional to the amount of i/o
	being done.  This is fairly easy for the fixed block devices.
	For variable mode, one also needs to take into account the
	size of the i/o, but since the max i/o size is much smaller,
	this really isn't much of a problem.   Loading and rewinding
	tapes is more problematical, since we may not know where we
	are.  Exabyte tapes take a max of 2:15 sec, plus possible load
	time of ~15 seconds.  On Kennedy, rewind of 2400' tape is
	aprox 4 minutes.  Space commands are straight forward for
	space block, but space filemark is something else.  On
	Exabyte, space is ~10 times as fast as i/o, so max would
	be 20 minutes, while on Kennedy, for a 2400 ft tape, it
	would be ~4 minutes.  See tp_xfr_divisor and tp_spacetimeo also.
	All times should be a bit on the generous side; the xfer
	timeout should allow for retries, possible start/stop time,
	and for held off reads/writes like the Kennedy will do.
*/

/* rewind, retension, erase, and load/unload timeouts */
#define REWIND_TIMEO	(ctinfo->ct_typ.tp_rewtimeo)

/*	tpsc_min_delay is for cmds that don't do media i/o,
	and min for any i/o. This long because if tape isn't moving
	some devices take considerably longer than you would expect;
	and we want to be conservative. lbootable so customers can
	more easily add non-standard drives.
*/
#define XFER_TIMEO(cnt) max(cnt/ctinfo->ct_typ.tp_xfr_divisor, \
	ctinfo->ct_typ.tp_mintimeo)

/* this is only for space FM, and space to EOM; space record uses
 * XFER_TIMEO, partly because some QIC drives implement them
 * (particularly back spacing) as read and check, which is
 * very slow. */
#define SPACE_TIMEO	ctinfo->ct_typ.tp_spacetimeo

/* delay time between retries when status BUSY is returned */
#define	BUSY_INTERVAL 	(2*HZ)

#define SHORT_TIMEO	ctinfo->ct_typ.tp_mintimeo


/* macros for determining drive type */
/*
 * The CIPHER 540S drives have a couple of bothers:
 * 1) they don't identify themselves through the INQUIRY command
 * 2) by default, they report recoverable errors, however they
 *	do it incorrectly - if one for example is spacing forward
 *	and encounters a recoverable error, it issues a check-condition
 *	with the RECERR sense key - but the FILE MARK bit is NOT ON
 *	so we lose track of where we are.
 *	To get around this, we set the CIPHER specific SEC bit on
 */
#define IS_CIPHER(ctinfo) (ctinfo->ct_typ.tp_type == CIPHER540)

#define IS_TANDBERG(ctinfo) (ctinfo->ct_typ.tp_type == TANDBERG3660)

/*
 * want to know if drive is a viper 150 because it can read 310 oersted 
 * tapes, but it can't write them.
 */
#define IS_VIPER150(ctinfo) (ctinfo->ct_typ.tp_type == VIPER150)
#define IS_VIPER60(ctinfo) (ctinfo->ct_typ.tp_type == VIPER60)

/* claimed to be 100% compatible with the 150, with some extra
 * capablities (can do tape updates like 9 track), has variable
 * block size. */
#define IS_DATTAPE(ctinfo) (ctinfo->ct_typ.tp_type == DATTAPE)


/*
** Exabyte uniquenesses:
** 1. does not support the RESERVE UNIT or RELEASE UNIT commands.
** 2. uses 1K blocks in fixed mode.
*/
#define IS_EXABYTE(ctinfo) ((ctinfo->ct_typ.tp_type == EXABYTE8200) || \
	(ctinfo->ct_typ.tp_type == EXABYTE8500))

/* Kennedy 9 track drives */
#define IS_KENNEDY(ctinfo) (ctinfo->ct_typ.tp_type == KENNEDY96X2)


#endif /* _KERNEL */



/* The values for ct_typ.tp_type, not same as hinv types */
#define TPUNKNOWN	0	/* type not known */
#define CIPHER540	1	/* cipher QIC24 */
#define TANDBERG3660	2	/* TANDBERG 3660 QIC150 */
#define VIPER150	3	/* VIPER 150 QIC150 */
#define KENNEDY96X2	4	/* Kennedy 96[16]2 1/2" 9 track */
#define EXABYTE8200	5	/* Exabyte EXB-8200 8mm cartridge tape */
#define VIPER60		6	/* VIPER 60 QIC24 */
#define DATTAPE		7	/* Digital Audio Tape */
#define EXABYTE8500	8	/* Exabyte EXB-8500 8mm cartridge tape */

/*	States for the logical unit (ct_state).  states common with
	mtio.h as much as possible.  Note that for MTIOCGET, the position
	bits get put in dposn, the low 16 bits in dsreg, and the upper 16
	bits in erreg.  These bits are all available to users now, so they
	are no longer ifdef _KERNEL.
*/
#define CT_ONL	MT_ONL		/* drive is online */
#define CT_WRP	MT_WPROT	/* drive is write-protected */
#define CT_EOM	MT_EOT		/* drive is at end of media */
#define CT_BOT	MT_BOT		/* drive is at beginning of media */
#define CT_FM	MT_FMK		/* drive is at file mark */
#define	CT_QIC24	MT_QIC24	/* tape is low density (310 oersted); 
	only set for Viper 150 drives */
#define	CT_QIC120	MT_QIC120	/* tape is high density (550 oersted); 
	QIC120 15 tracks. */
#define CT_EOD	MT_EOD	/* at end of data, may also be EOT; sometimes,
	in audio mode, we will set this (along with CT_AUD_MED) when
	the drive is confused due to incorrect subcodes on the tape,
	particularly at the leadin area.  If both these bits are set
	near BOT, then that is probably the problem.  Both may legitimately
	be set near EOD, particularly if audio was recorded on top of a
	data tape, and doesn't extend past the end of the data portion.
	See also CT_INCOMPAT_MEDIA for this case. */

/*	beware the bits defined in mtio.h! */
#define	CT_EW		0x08	/* hit early warning marker	*/
#define CT_GETBLKLEN	0x10	/* in the process of determining block
					length of data on the tape */
#define	CT_MOTION	0x20	/* a command has been performed that
	(potentially) moves the tape since last tape change or reset */
#define	CT_OPEN		0x200	/* device open			*/
#define	CT_READ		0x400	/* opened for reading		*/
#define	CT_WRITE	0x800	/* opened for writing		*/
#define	CT_CHG		0x1000	/* unit attn occurred		*/
#define CT_DIDIO	0x2000	/* i/o has been successfully done on this open.
	Used to handle case where first read after open hits a FM, in which
	case we automatically skip over the FM.  Driver no longer spaces
	to a FM on first tape command after open, since that made mt [bf]sr
	relatively worthless, with no way around it except writing a program
	that does all the ioctls itself.  This gets cleared on any of the
	space, rewind, etc. commands. */
#define	CT_NEEDBOT	0x10000	/* program needs to position at BOT
	before doing any i/o.  Set if opening Kennedy for diff
	density/mode and not at BOT on open */
#define CT_LOADED	0x20000	/* The tape has been loaded, but is not
	necessarily at BOT. */
#define CT_ANSI	0x40000	/* allow i/o after EOT marker for 9 track tapes.
	This allows ANSI labels to be used.  Works for QIC tapes also.
	(see MTANSI in mtio.h) */
#define CT_SMK	0x80000		/* drive is at a setmark.  Note that a skip
	block or skip fm command will stop if a setmark is encountered. */
#define CT_AUDIO	0x100000	/* Drive is in audio mode; persists until
	reset, tape change, or reboot */
#define CT_AUD_MED	0x200000	/* the media in the drive is in audio
	format.  Set if the drive tells us that we have audio media, or
	when we are in audio mode and first write to the tape.  Cleared
	if we are not in audio mode, and we write to the tape. */
#define CT_MULTPART	0x400000	/* the media in the drive is a multi-
	partition tape.  Currently only DAT supports this */
#define CT_SEEKING	0x800000	/* a seek or rewind done with the
	immediate bit set is still in progress; returned in the mt_erreg
	field on MTIOCGET; never set in ct_state itself.  Also set during
	MTIOCGET if the drive is busy loading or unloading media.
	This will currently happen only while in audio mode. */
#define CT_HITFMSHORT	0x1000000	/* in fixed blk mode, we hit a FM,
	and returned a short count (not 0); the next read must return 0
	(assuming no other intervening tape movement) so program knows a
	FM has been reached.  Can't happen in variable block mode */
#define CT_INCOMPAT_MEDIA 0x2000000 /* we have successfully read
	data in audio mode with audio media, and have now gotten status
	from the drive that tells us we have incompatible media.  This
	doesn't get set if we see incompat media near BOT, or if we aren't
	in audio mode.  It gets cleared on any tape motion command after it
	was set. */
#ifdef _KERNEL

/* tape postion flags cleared on tape motion cmds and tape change */
#define CTPOS (CT_BOT|CT_EOM|CT_FM|CT_SMK|CT_EOD|CT_EW|CT_HITFMSHORT|\
	CT_INCOMPAT_MEDIA)

/*	states that persist across close/open. (if tape changes, etc.
	some may be cleared at interrupt time) */
#define CTKEEP (CTPOS|CT_LOADED|CT_MOTION|CT_ONL|CT_QIC24|CT_QIC120|CT_WRP\
	|CT_AUDIO|CT_MULTPART)

/* these states don't persist across a tape change */
#define CHGSTATES (CTKEEP|CT_NEEDBOT|CT_AUD_MED)

/* these states get cleared on a rewind, load, or a space BSF */
#define BACKSTATES (CT_DIDIO|CT_NEEDBOT|CTPOS)

#endif /* _KERNEL */
#endif /* _SYS_TPSC_H_ */
