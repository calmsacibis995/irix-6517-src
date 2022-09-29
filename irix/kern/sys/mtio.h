#ident	"$Revision: 3.56 $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

#ifndef __SYS_MTIO_H__
#define __SYS_MTIO_H__

/*	mtio.h	6.1	83/07/29	*/

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/types.h>
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/*
 * Structures and definitions for mag tape io control commands
 */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/* structure for MTIOCTOP - mag tape op command */
struct	mtop	{
	short	mt_op;		/* operations defined below */
	uint_t	mt_count;	/* how many of them */
};

/* structure for MIPS ABI equivalent of MTIOCTOP (it was constructed
 * to be a superset of mtop for all the MIPS ABI systems vendors) */
struct abi_mdata {
	short		mt_op;	/* one of the ABI_* tape ops below */
	__int32_t	mt_cnt;	/* count for op */
	unsigned short	mt_sts;	/* returned status on some ABI
		* compliant systems.  IRIX releases currently do not
		* change this field at all */
	__int32_t	reserved1;
	__int32_t	reserved2;
	__int32_t	reserved3;
};
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/* operations (mt_op values for MTIOCTOP) */
#define MTWEOF	0	/* write an end-of-file record */
#define MTFSF	1	/* forward space file */
#define MTBSF	2	/* backward space file */
#define MTFSR	3	/* forward space record */
#define MTBSR	4	/* backward space record */
#define MTREW	5	/* rewind */
#define MTOFFL	6	/* rewind and put the drive offline */
#define MTNOP	7	/* no operation, sets status only */
#define MTRET	8	/* retention operation */
#define MTEOM	10	/* space to end of recorded data */
#define MTAFILE	11	/* space to end of last file before FM (so next data
	written is appended) */
#define MTERASE	12	/* erase tape from current position to EOT */
#define MTUNLOAD 13	/* unload tape from drive */

#define MTSETPART	14	/* skip to partition in mt_count (DAT); for DDS
	* format DAT drives (only ones supported current), partition 1 is the first
	* partition on the tape (nearest BOT), and partition 0 is the remainder of
	* the tape. */
#define MTMKPART	16	/* create partition (DAT), count is multiplied by
	* 1 Mybte (2^20 bytes) to give the size of partition 1; if 0, then changes
	* tape to a single partition tape. */
#define MTSKSM	17	/* skip setmark (DAT); skip backwards if mt_count is
	negative */
#define MTWSM	19	/* write mt_count setmarks (DAT) */
#define MTAUD	20	/* turn audio mode on and off.  If count is 0, we
	* are in data mode (default), if 1, put drive in audio mode.  This is
	* only needed for writing tapes, since when reading the tape type
	* (data or audio) is recognized automaticly. (DAT only for now).
	* Remains set until same ioctl is reused, or until reboot. */
#define MTSEEK	21	/* Seek to the given block number.  Not all drives
	* support this option.  For drives that support audio, the block
	* number actually becomes the program number ( 0 meaning leadin
	* area, 0xfffffffe meaning leadout, and other values (currently
	* 1-799 decimal) the program #).  For seeking to times in audio mode,
	* see MTSETAUDIO. */

/* values for mt_op in abi_mdata */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define ABI_MTIOCTOP	(('t' << 8) | 9)
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#if defined(_LANGUAGE_FORTRAN)
#define ABI_MTIOCTOP	((LSHIFT(ICHAR('t'), 8)) + 9)
#endif  /* defined(_LANGUAGE_FORTRAN) */
#define ABI_MTWEOF	0	/* same as MTWEOF */
#define ABI_MTFSF	1	/* same as MTFSF */
#define ABI_MTBSF	2	/* same as MTBSF */
#define ABI_MTFSR	3	/* same as MTFSR */
#define ABI_MTBSR	4	/* same as MTBSR */
#define ABI_MTREW	5	/* same as MTREW */
#define ABI_MTOFFL	6	/* same as MTOFFL */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/* structure for MTIOCGET - mag tape get status command; note that
 * in audio mode, the blkno is actually the program number; see
 * also the MTGETAUDIO ioctl and structure.  For the 3 special
 * position in audio mode (leadin, leadout, and unknown), the
 * low nibbles are replicated, so they don't match valid program
 * numbers (which are 1-799 decimal), giving values of 0xBBB, 0xEEE,
 * and 0xAAA, respectively.
*/
struct	mtget	{
	short	mt_type;	/* type of magtape device */
	unsigned short	mt_dposn;	/* status of tape position (see below) */
/* the following two registers are device dependent */
	unsigned short	mt_dsreg;	/* ``drive status'' register */
	short	mt_erreg;	/* ``error'' register */

	short	mt_resid;	/* residual count; for SCSI, this is used
		to report the current partition # for DAT tapes that are
		partitioned. */
	/* the following two are not yet fully implemented */
	int	mt_fileno;	/* file number of current position */
	int	mt_blkno;	/* block number of current position */
};

/* old mtget structure , still keep it around for compatibility reason */
/* the librmt and /etc/rmt code uses it */
struct	old_mtget	{
	short	mt_type;	/* type of magtape device */
/* the following two registers are device dependent */
	unsigned short	mt_dsreg;	/* ``drive status'' register */
	short	mt_erreg;	/* ``error'' register */

	short	mt_resid;	/* residual count */
	/* the following two are not yet fully implemented */
	int	mt_fileno;	/* file number of current position */
	int	mt_blkno;	/* block number of current position */
};


/*	structure for MTIOCGETEXT
 *
 *	mt_type		Type of tape device
 *	mt_dposn	Status of tape position. See below.
 *	mt_dsreg	Drive status register.  See below.
 *	mt_erreg	Error registers.  The error register bits for a SCSI
 *			device are defined in tpsc.h, the upper 16 bits of
 *			ct_state in word0, ct_state2 in word 1.
 *	resid		Difference between what was requested and what has
 *			completed, in bytes or blocks depending on the command.
 *	fileno		Not currently supported.
 *	blkno		Block number (from device)
 *	partno		Partition number.
 *	cblkno		Block number calculated by driver, includes filemarks.
 *	lastreq		Last device request made by user.  See MTR_xxx request
 *			codes defined below.
 *	mt_ilimode	1 if ILI reporting is on, otherwise 0.
 *	mt_buffmmode	1 if filemarks are buffered, otherwise 0.
 */
typedef	struct	mtgetext {
	short		mt_type;
	unsigned short	mt_dposn;	/* Status of tape position	*/
	unsigned short	mt_dsreg;	/* Drive status register	*/
	short		mt_erreg[5];	/* Error registers		*/
	int		mt_resid;	/* Residual count		*/
	int		mt_fileno;	/* File number			*/
	int		mt_blkno;	/* Block number (from device)	*/
	int		mt_partno;	/* Partition number		*/
	int		mt_cblkno;	/* Block number	(calculated)	*/
	int		mt_lastreq;	/* Last device request		*/
	int		mt_ilimode;	/* State of ILI reporting mode	*/
	int		mt_buffmmode;	/* State of buffered filemarks	*/
	int		mt_subtype;	/* Device type			*/
	uint_t		mt_capability;	/* Device capabilities (MTCAN*) */
	int		mt_future[6];	/* Future additions		*/
} mtgetext_t;


/*	structure for MTIOCGETBLKINFO - all quantities in bytes.
	A value of 0 indicates no limit
	When using the variable block size device, the lastread
	field is determined by reading one block and then spacing
	backwards, if no i/o has yet been done to this tape.  Unlike 
	MTIOCGETBLKSIZE, all quantities are in bytes.
*/
struct mtblkinfo {
	unsigned minblksz;	/* minimum block size */
	unsigned maxblksz;	/* maximum block size */
	unsigned curblksz;	/* size of block at last i/o.  For fixed
		mode, all requests must be a multiple of this. */
	unsigned recblksz;	/* "recommended" size for i/o.  Mostly
		based on compatibility with earlier releases and/or
		other vendors */
};
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */


/*	structure for MTSCSI_RDLOG
	The fields in this structure reflect the values required in
	the SCSI Log Sense command.  See a product manual for an
	explanation of the fields.
*/

#define	MTSCSI_LOGLEN	4096
typedef	struct	mtscsi_rdlog	{
	uint		mtppc	: 1,	/* Parameter pointer control	*/
			mtsp	: 1,	/* Saving paramters		*/
			mtpc	: 2,	/* Page control			*/
			mtpage	: 6;	/* Page code of requested page	*/
	unsigned short	mtparam;	/* Parameter pointer		*/
	unsigned short	mtlen;		/* Size of buffer receiving log	*/
	caddr_t		mtarg;		/* Buffer pointer		*/
} mtscsi_rdlog_t;

#if _KERNEL
typedef	struct	irix5_mtscsi_rdlog {
	uint		mtppc	: 1,	/* Parameter pointer control	*/
			mtsp	: 1,	/* Saving paramters		*/
			mtpc	: 2,	/* Page control			*/
			mtpage	: 6;	/* Page code of requested page	*/
	unsigned short	mtparam;	/* Parameter pointer		*/
	unsigned short	mtlen;		/* Size of buffer receiving log	*/
	app32_ptr_t	mtarg;		/* Buffer pointer		*/
} irix5_mtscsi_rdlog_t;
#endif /* _KERNEL */	



/* structure and defines for MT[GS]ETAUDIO.
 * When using SETAUDIO to locate to a particular point, the type
 * should be set to one of the MTAUDPOSN_* values, 
 * and only the corresponding fields need be filled in.  See the DAT
 * audio spec for more detail on the definitions of the different times.
 * There are 34 frames per second, legal values are 0 through 33.  The
 * maximum value for the hours field is 99 hours.  Positioning commands
 * return immediately (as does rewind) in audio mode, and requests for
 * current position may be made while the positioning is in progress.
 * Other commands will block while positioning is in progress.
 * When known, all 4 of the positions are returned for GET (that is
 * pnum1-index, and all 3 of the mtaudtime structures).  The program
 * must determine by looking for 0xA in both nibbles whether any particular
 * field is valid.
*/

#define MTAUDPOSN_PROG	0	/* seek to program number */
#define MTAUDPOSN_ABS 	1	/* seek to absolute time */
#define MTAUDPOSN_RUN 	2	/* seek to running time */
#define MTAUDPOSN_PTIME 3	/* seek to program time (within program) */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/*
 * all the times are BCD digits, for both SET and GET.
 * a value of 0xA in BOTH nibbles, (i.e., not a valid BCD number)
 * means that the field isn't valid.
 */
struct mtaudtimecode {
    uchar_t hhi:4, hlo:4;	/* hours */
    uchar_t mhi:4, mlo:4;	/* minutes */
    uchar_t shi:4, slo:4;	/* seconds */
    uchar_t fhi:4, flo:4;	/* frame # (finer grain than seconds) */
};

struct mtaudio {
    uchar_t zero1:4, pno1:4; /* 3 BCD digits giving program number the zero */
    uchar_t zero2:4, pno2:4; /* fields should always be 0, or 0xA, when the */
    uchar_t zero3:4, pno3:4; /* pno field is also 0xA, indicating not valid */
    uchar_t indexhi:4, index:4; /* index number within program; hi is the
		most significant BCD digit, index the least. */
    struct mtaudtimecode ptime;         /* program time */
    struct mtaudtimecode atime;         /* absolute time */
    struct mtaudtimecode rtime;         /* running time */
    uchar_t seektype;            /* one of the MTAUDPOSN_* values */
    uchar_t cfuture[11];  /* future expansion */
};


/* structure for MTCAPABILITY */
struct mt_capablity  {
	int mtsubtype;	/* this is NOT the same as mt_type, this is
		the actual tape type, from invent.h, such as 9track, or DAT;
		otherwise it is difficult to associate a particular device
		with its type from a program. */
	uint_t mtcapablity;	/* the MTCAN* bits */
	uint_t mtfuture[8];	/* for future additions */
};
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */


#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* Constants for mt_type byte */
#define MT_ISSCSI	0x09		/* Western Digital SCSI controller */


/*      values for mt_dposn; values chosen for compatiblity with
        existing programs!  Note that these bits and the gaps between
        are also used in tpsc.h */

/* The expected relative behaviour of MT_EW, MT_EOT and MTANSI can be
   described as follows. 
   - writing from BOT, with MTANSI disabled, when LEOT is encountered,
     MT_EW and MT_EOT are set. If MTANSI is now enabled, MT_EOT will be
     reset and writing will be permitted until PEOT is encountered,
     whereupon MT_EOT will again be set and will remain so until the
     tape is repositioned.
   - reading from BOT, MT_EOT will be set when PEOT is
     encountered. LEOT is not normally reported by the drive, and therefore 
     MT_EW will normally not be set, unless the drive supports the REW mode 
     bit. (few do) 
*/

#define MT_EOT		0x01	/* tape is at end of media */
#define MT_BOT		0x02	/* tape is at beginning of media */
#define MT_WPROT	0x04	/* tape is write-protected */
#define	MT_EW		0x08	/* hit early warning marker	*/
#define MT_ONL		0x40	/* drive is online */
#define MT_EOD		0x4000	/* tape is at end of data */
#define MT_FMK		0x8000	/* tape is at file mark */
#define MT_POSMSK	0xC04F	/* mask for supported bits */

/* bits in mt_dsreg; set only for drives that support QIC150 as well
	as the older formats.  Specific to SCSI tape */
#define MT_QIC24 0x80	/* QIC24 cartridge in drive */
#define MT_QIC120 0x100	/* QIC120 cartridge in drive */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#if defined(_LANGUAGE_FORTRAN)
#define MT_ISSCSI	$09
#define MT_EOT		$01
#define MT_BOT		$02
#define MT_WPROT	$04
#define	MT_EW		$08
#define MT_ONL		$40
#define MT_EOD		$4000
#define MT_FMK		$8000
#define MT_POSMSK	$C04F
#define MT_QIC24 $80
#define MT_QIC120 $100
#endif  /* defined(_LANGUAGE_FORTRAN) */

/* mag tape io control commands */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define MTIOCODE(x)	('t'<<8|(x))
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#if defined(_LANGUAGE_FORTRAN)
#define MTIOCODE(x)	((LSHIFT(ICHAR('t'), 8)) + ICHAR(x))
#endif  /* defined(_LANGUAGE_FORTRAN) */ 
#define MTIOCTOP	MTIOCODE('a')		/* perform tape op */
#define MTIOCGET	MTIOCODE('b')		/* get tape status */
#define MTIOCGETBLKSIZE	MTIOCODE('c')		/* get tape block size
	in multiples of 512 bytes */

#define MTSCSIINQ	MTIOCODE('d')	/* return scsi inquiry info (scsi
	tape drives only!); 3rd arg is a ptr to a buffer of at least
	sizeof(ct_g0inq_data_t) from tpsc.h */
#define MTSCSI_INQUIRY_LEN 82	/* length of inquiry data for scsi tapes */

#define MTSPECOP	MTIOCODE('e')		/* special drive specific ops */
#define MTIOCGETBLKINFO	MTIOCODE('f')		/* get tape block info */
#define MTANSI    MTIOCODE('g')           /* allows i/o past EOT mark to
	provide for ANSI 3.27 labeling.  Doesn't persist across
	opens.  If used, standard SGI EOT handling for tar, bru, etc.
	won't work correctly.  Currently implemented only on SCSI
	tape drives.  An arg of 1 enables, 0 disables.  NOTE: when
	the EOT marker is encountered, the current i/o operation
	returns either a short count (if any i/o completed), or -1
	(if no i/o completed); it is the programmers responsiblity to
	determine if EOT has been encountered by using MTIOCGET or
	logic internal to the program.  This ioctl should be issued
	AFTER encountering EOT, if ANSI 3.27 EOT handling is
	desired.  subsequent i/o's will then be allowed, and will
	return the count actually transferred, or -1 if the drive was
	unable to transfer any data.  The standard calls for writing
	a FM before the label.  If this isn't done, the drive may
	return label info as data.  */
#define MTCAPABILITY    MTIOCODE('h')  /* returns drive tape
	and capablities (MTCAN*).  Currently implemented only
	for SCSI drives.  See mt_capablity struct */
#define MTSETAUDIO    MTIOCODE('i')  /* sets current
		position, etc. in audio mode */
#define MTGETAUDIO    MTIOCODE('j')  /* returns info about
		position, etc. in audio mode */
#define MTABORT    MTIOCODE('k')  /* abort current tape operation,
	if any.  Useful only with commands that run asynchronously
	to the caller, such as seek and rewind when in audio mode. */
#define MTALIAS	   MTIOCODE('l')	/* for use by ioconfig in the
	hardware graph */

#define MTSCSI_SENSE	MTIOCODE('m')	/* returns the data for the most
	recent request sense issued as the result of a scsi tape
	driver command that got a SCSI checkcondition. returns up to
	MTSCSI_SENSE_LEN bytes of data. The buffer supplied as the 3rd
	ioctl argument must be at least that large.  The actual amount
	of data will usually be less. */
#define MTSCSI_SENSE_LEN 256	/* length of sense data returned by
	MTSCSI_SENSE ioctl.  returned data must be examined for for actual
	valid length.  */

#define MTILIMODE	MTIOCODE('n')	/* Set ILI mode */
#define	MTIOCKGET	MTIOCODE('o')	/* CA UniCenter kernel call */
#define	MTSCSI_RDLOG	MTIOCODE('p')	/* Return SCSI device log data	*/
#define	MTIOCGETEXT	MTIOCODE('q')	/* Get extended tape status	*/
#define MTGETATTR	MTIOCODE('r')	/* Get tape device attributes */
#define MTBUFFMMODE	MTIOCODE('s')	/* Set Buffered filemark mode */
#define MTFPMESSAGE	MTIOCODE('t')	/* Write to diagnostic display (if supported) */
#define MTGETPOS	MTIOCODE('u')	/* Get vendor specific position */
#define MTSETPOS	MTIOCODE('v')	/* Set vendor specific position */
#define	MTIOCGETEXTL	MTIOCODE('w')	/* Get last extended tape status */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#if _KERNEL
struct irix5_mt_ioctl_data {
	app32_ptr_t mi_addr;	/* buffer used to pass and return data */
	u_int	 mi_len;	/* The length of the data buffer */
	u_char	 mi_page;	/* used by mode sense */
};

/*
 * Structure for the MTIOCKGET call.
 */
struct	kmtget	{
	uint_t 		mt_state;	/* tape state */
	int		mt_dens;	/* tape density */
	int		mt_block;	/* tape block position */
};

#endif /* _KERNEL */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */



/* MTSPECOP - special drive specific operations.  These use the mtop
	structure. */
#define MTSCSI_CIPHER_SEC_ON	1 /* enable reporting of recovered
				   errors on the Cipher 540S */
#define MTSCSI_CIPHER_SEC_OFF	2	/* disable recovered error report */

/* These are meaningful only for Exabyte.  Default is to return
	errors if read length is less than the block length */
#define MTSCSI_SILI		3	/* suppress illegal length indication */
#define MTSCSI_EILI		4	/* enable illegal length intication */

/* currently implemented only for SCSI tape driver */
#define MTSCSI_SETFIXED		5	/* set size of blocks for fixed block
	device in mt_count.  Lasts till reset same way, or till next boot. */
#define MTSCSI_SPEED		6	/* set speed for multispeed devices in
	mt_count.  Current legal values are 0 (50 ips) and 1 (100 ips)
	on Kennedy drives only.  Default is currently 100. */

#define MTSCSI_SM	7	/* How DAT setmarks are handled.  default
	is to treat similar to FM (also if non-zero in mt_count).  0
	in mt_count causes setmarks to be ignored.  Lasts until
	reboot or reset. */

#define MTSCSI_LOG_OFF		8	/* Disable sense data logging */
#define MTSCSI_LOG_ON		9	/* Enable sense data logging */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/* definitions for tape capablities.  Only those that are not
 * shared by all tape drives are shown. */
#define MTCAN_BSF	0x1	/* can backspace file */
#define MTCAN_BSR	0x2	/* can backspace record (block) */
#define MTCAN_APPEND	0x4	/* can append to existing tape files */
#define MTCAN_SETMK	0x8	/* can do setmarks */
#define MTCAN_PART	0x10	/* can do multiple partitions (DAT) */
#define MTCAN_PREV	0x20	/* can prevent/allow media removal */
#define MTCAN_SYNC	0x40	/* can do synchronous mode SCSI */

/* Note.  If SPEOD are set, or just SPEOD is set,
 * then a simple space to EOD is done; the combination
 * of SPEOD and LEOD is meaningless, and is treated as SPEOD, when
 * spacing, or as LEOD when closing; i.e., do NOT set both, or you
 * will get inconsistent results.
 * Some drives, such as the 8mm drive, have a drive detectible EOD,
 * but don't support the space EOD command.  For these drives,
 * neither SPEOD nor LEOD should be set.  We then fake it by looping
 * on space record and space FM until we get a blank check or other
 * indication of EOD. */
#define MTCAN_LEOD	0x80	/* Use a logical EOD (as opposed to an EOD
	* detectable by the drive).  Currently only 9-track drives.
	* If set, it is assumed EOD is marked by 2 sequential filemarks.
	* Setting this also implies causes two sequential FM's to be written
	* on close, when writing. */
#define MTCAN_SPEOD	0x100	/* can execute the space EOD command. */

#define MTCAN_CHKRDY 0x200	/* can determine if a tape is present; some
	* drives, such as Cipher540S return OK on a test unit ready even if
	* no cartridge is present. */

#define MTCAN_VAR	0x400	/* can do variable block sizes */
#define MTCAN_SETSZ	0x800	/* can set fixed block size */
#define MTCAN_SETSP	0x1000	/* can set tape speed (some 9track) */
#define MTCAN_SETDEN	0x2000	/* can set tape density (some 9track) */
#define MTCAN_SILI	0x4000	/* can set suppress illegal length errors;
	* only on drives that support variable block sizes */
#define MTCANT_RET	0x8000	/* some drives can't retension; the driver
	* will do it's best by spacing to EOD and rewinding. */
#define MTCAN_AUDIO	0x10000	/* drive supports audio over the SCSI bus;
	* only DAT at this time. The data is expected to be read
	* or written in the standard DAT format, including subcodes.
	* collection and gathering of subcodes is handled by the drive.
	* all 7 subcodes follow the data, then the mainID.
	* The drive reports/sets audio or data tapes via the density code
	* in the modesense/select block descriptor.  Actual data layout
	* is as below:
	*   5760 bytes of audio data (16 bits left, 16 bits right, repeat)
	*   7 subcode packs of 8 bytes each
	*   4 bytes of subid (4bits each of ctrlid, dataid, program # high
	*     nibble, # of valid pack items, progno # middle nibble, progno
	*     low nibble, 2 reserved nibbles)
	*   2 bytes main id (2 bits each of fmtid, and id1 - id7)
	*/
#define MTCAN_SEEK	0x20000	/* drive supports seeking to a particular
	* block (or sometimes program # if AUDIO supported) */
#define MTCAN_CHTYPEANY 0x40000	/* drive can change density
	and/or fixed to variable at any point (if it can change at all).
	Otherwise it can change only at BOT */
#define MTCANT_IMM	0x80000 /* drive doesn't work correctly when
	immediate mode rewind, etc. is enabled.  Setting this bit
	will disable immediate mode rewind on the drive, independent
	of the setting of tpsc_immediate_rewind (in master.d/tpsc) */
#define MTCAN_COMPRESS	0x100000 /* drive supports compression */
#define MTCAN_BUFFM	0x200000 /* drive supports writing of
				  * buffered filemarks */
#define MTCAN_FASTSEEK	0x400000 /* Drive needs BT bit set in read position and
				  * locate commands for fast seeking. */
#define MTCANT_LOAD	0x800000 /* Do NOT issue SCSI LOAD command for this drive */
#define MTCAN_LDREW	0x1000000 /* Issue Rewind instead of Load command. Used
				   * in conjunction with MTCANT_LOAD. */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

#if defined(_LANGUAGE_FORTRAN)
/* definitions for tape capablities.  See the comments for the 
   defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) section above. */
#define MTCAN_BSF	$1
#define MTCAN_BSR	$2
#define MTCAN_APPEND	$4
#define MTCAN_SETMK	$8
#define MTCAN_PART	$10
#define MTCAN_PREV	$20
#define MTCAN_SYNC	$40
#define MTCAN_LEOD	$80
#define MTCAN_SPEOD	$100
#define MTCAN_CHKRDY	$200
#define MTCAN_VAR	$400
#define MTCAN_SETSZ	$800
#define MTCAN_SETSP	$1000
#define MTCAN_SETDEN	$2000
#define MTCAN_SILI	$4000
#define MTCAN_AUDIO	$10000
#define MTCAN_SEEK	$20000
#define MTCAN_CHTYPEANY $40000
#define MTCANT_IMM	$80000
#define MTCAN_COMPRESS	$100000
#define MTCAN_BUFFM	$200000
#define MTCAN_FASTSEEK	$400000
#define MTCANT_LOAD	$800000
#define MTCAN_LDREW	$1000000
#endif  /* defined(_LANGUAGE_FORTRAN) */

/* 
 * Definitions for MTGETATTR 
 */

/* Definition of MTGETATTR return struct */
struct mt_attr {
	char		*ma_name;
	char		*ma_value;
	int		ma_vlen;
};

#if _KERNEL
struct irix5_mt_attr {
	app32_ptr_t	ma_name;
	app32_ptr_t	ma_value;
	int		ma_vlen;
};
#endif /* _KERNEL */

/* Definitions of attribute names */
#define MT_ATTR_NAME_REWIND		"rewind"
#define MT_ATTR_NAME_VARIABLE		"varblk"
#define MT_ATTR_NAME_SWAP		"swap"
#define MT_ATTR_NAME_COMPRESS_DENS	"compress"
#define MT_ATTR_NAME_DEVSCSI		"devscsi"

/* (Some of the) Definitions of return values */
#define MT_ATTR_VALUE_TRUE		"true"
#define MT_ATTR_VALUE_FALSE		"false"

#define MT_ATTR_MAX_NAMELEN	32
#define MT_ATTR_MAX_VALLEN	256

/*
 * Definitions for MTFPMESSAGE
 */

#define MT_FPMSG_MAX_MSGLEN	64
#define MT_FPMSG_IBM_MSGLEN	0x18
#define MT_FPMSG_DIANA_MSGLEN	0x11

/* Definition of MTFPMESSAGE struct */
struct mt_fpmsg {
	int		mm_mlen;
	union {
		char	mm_msg[MT_FPMSG_MAX_MSGLEN];	/* Generic format */
		struct {			/* Format for IBM 3590 and 3570 */
			char   display_type;	/* Display Type: Set to 0x80 */
			char   msg_type:3,      /* Message type: 0x00 = General Status message */
		               msg_cntrl:3,     /* Message control: 0 = Display message 0
						 *                  1 = Display message 1
						 *                  2 = Flash message 0
						 *                  3 = Flash message 1
						 *                  4 = Alternate message 0 and 1 */
		               rsvd1:2;
			char   rsvd2[4];
			ushort msg_len;	        /* Message length: Set to 0x10 */
			char   msg1[8];	        /* message 1 */
			char   msg2[8];	        /* message 2 */
		} ibm3590;
		struct {			/* Format for Fujitsu Diana 1,2,3 */
			char   display_mode:3, 
		               display_len:1,
		               flash:1,
		               half_msg:1,
			       :1,
		               data_format:1;
			char   msg1[8];
			char   msg2[8];
		} diana;
	} u;
};

#if _KERNEL
struct irix5_mt_fpmsg {
        int             mm_mlen;
	union {
		char	mm_msg[MT_FPMSG_MAX_MSGLEN];	/* Generic format */
		struct {			/* Format for IBM 3590 and 3570 */
			char   display_type;	/* Display Type: Set to 0x80 */
			char   msg_type:3,      /* Message type: 0x00 = General Status message */
		               msg_cntrl:3,     /* Message control: 0 = Display message 0
						 *                  1 = Display message 1
						 *                  2 = Flash message 0
						 *                  3 = Flash message 1
						 *                  4 = Alternate message 0 and 1 */
		               rsvd1:2;
			char   rsvd2[4];
			ushort msg_len;	        /* Message length: Set to 0x10 */
			char   msg1[8];	        /* message 1 */
			char   msg2[8];	        /* message 2 */
		} ibm3590;
		struct {		 /* Format for Fujitsu Diana 1,2,3 */
			char   display_mode:3, 
		               display_len:1,
		               flash:1,
		               half_msg:1,
			       :1,
		               data_format:1;
			char   msg1[8];
			char   msg2[8];
		} diana;
	} u;
};
#endif /* _KERNEL */

#define default_mm_msg		u.mm_msg
#define ibm3590_display_type	u.ibm3590.display_type
#define ibm3590_msg_type	u.ibm3590.msg_type
#define ibm3590_msg_cntrl	u.ibm3590.msg_cntrl
#define ibm3590_msg_len		u.ibm3590.msg_len
#define ibm3590_msg1		u.ibm3590.msg1
#define ibm3590_msg2		u.ibm3590.msg2
#define diana_display_mode	u.diana.display_mode
#define diana_display_len	u.diana.display_len
#define diana_flash		u.diana.flash
#define diana_half_msg		u.diana.half_msg
#define diana_data_format	u.diana.data_format
#define diana_msg1		u.diana.msg1
#define diana_msg2		u.diana.msg2

/* Definition of MTGETPOS/MTSETPOS structure */
#define MAX_VEND_POS_SIZE	256	/* Maximum vendor specific position data size. */
struct	vendor_specific_pos
{
	u_short	position_type;		/* Identifies which position type is required. */
	short	size;			/* Number of bytes of position data. */
	u_char	pos[MAX_VEND_POS_SIZE];	/* Generic vendor specific position data. */
};

/* Definition for vendor_specific_pos position_type. */
#define	BLKPOSTYPE	0x1001	/* Ampex DST Block Position type. */
#define	FSNPOSTYPE	0x1002	/* Ampex DST File Section Position type. */
#define	DISPOSTYPE	0x1003	/* Ampex DIS Timecode Position type. */

/*
 *   Define user request codes.  These values are returned in field lastreq
 *   of structure mtgetext_t.
 */

#define MTR_READ	1		/* Read				*/
#define	MTR_WRITE	2		/* Write			*/
#define	MTR_WFM		3		/* Write filemark(s)		*/
#define	MTR_SRB		4		/* Skip records backward	*/
#define	MTR_SRF		5		/* Skip records forward		*/
#define	MTR_SFB		6		/* Skip filemarks backwards	*/
#define	MTR_SFF		7		/* Skip filemarks forward	*/
#define	MTR_SEOD	8		/* Space to the end of data	*/
#define	MTR_SEOM	9		/* Space to the end of media	*/
#define	MTR_FORMAT	10		/* Skip filemarkds forward	*/
#define	MTR_PART	11		/* Position to a partition	*/
#define	MTR_SSM		12		/* Skip setmarks		*/
#define	MTR_WSM		13		/* Write setmarks		*/
#define	MTR_MODEAUD	14		/* Enable or disable audio mode	*/
#define	MTR_REW		15		/* Rewind			*/
#define	MTR_ERASE	16		/* Erase from current pos to EOT*/
#define	MTR_RETEN	17		/* Retension			*/
#define	MTR_UNLOAD	18		/* Unload			*/
#define	MTR_PABS	19		/* Position to an absolute pos	*/
#define	MTR_PAUDIO	20		/* Audio position		*/
#define	MTR_GAUDIO	21		/* Get audio position		*/
#define	MTR_RDLOG	22		/* Read log			*/
#define	MTR_ATTR	23		/* Set attribute		*/
#define	MTR_SPOS	24		/* Set vendor specific position	*/
#define	MTR_GPOS	25		/* Get vendor specific position	*/

#endif /* __SYS_MTIO_H__ */
