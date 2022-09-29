/*	dkio.h	6.1	83/07/29	*/
/*
 * Structures and definitions for disk io control commands
 */

#ident	"$Revision: 3.29 $"

/* disk io control commands */
#define _DIOC_(x)	(('d'<<8) | x)
#define DIOCFMTTRK	_DIOC_(1)	/* Format track */
#define DIOCVFYSEC	_DIOC_(2)	/* Verify sectors */
#define DIOCGETCTLR	_DIOC_(3)	/* Get ctlr info */
#define DIOCDIAG	_DIOC_(4)	/* Perform diag */
#define DIOCSETDP	_DIOC_(5)	/* Set devparams */
#define DIOCGETVH	_DIOC_(6)	/* Get volume header */
#define DIOCSETVH	_DIOC_(7)	/* Set volume header */
					/* Can be or'd with DIOCFORCE */
	/* _DIOC_(8)	 (obsolete) */
#define DIOCTEST	_DIOC_(9)	/* Perform selftest on drive */
#define DIOCFORMAT	_DIOC_(10)	/* Format the drive */
#define DIOCSENSE	_DIOC_(11)	/* Get current drive parameters */
#define DIOCSELECT	_DIOC_(12)	/* Set current drive parameters */
#define DIOCREADCAPACITY _DIOC_(13)	/* Read disk drive capacity */
#define DIOCRDEFECTS	_DIOC_(14)	/* Read bad block info */
#define DIOCADDBB	_DIOC_(15)	/* Add bad block to BB list */

/* added for runtime esdi/smd fx */
#define DIOCFMTMAP	_DIOC_(16)	/* Format or Map track. */
#define DIOCRAWREAD	_DIOC_(17)	/* 'raw read'.. actually just */
					/* for getting flawmaps */
#define DIOCCDCRAW	_DIOC_(18)	/* alternate version of above, */
					/* used for CDC drives which have */
					/* a slightly different format */
#define DIOCGETERROR	_DIOC_(19)	/* report controller status */
#define DIOCHANDSHK	_DIOC_(20)	/* obtain controller information */
					/* (product code etc.) */
#define DIOCREADIDS	_DIOC_(21)	/* read id fields from sectors */

/* Logical volume driver commands. */
#define DIOCGETLV	_DIOC_(22)	/* get logical volume description */
#define DIOCSETLV	_DIOC_(23)	/* set logical volume description */
#define DIOCDRIVETYPE	_DIOC_(24)	/* Return drive type  */

#define DIOCIOPBPASS	_DIOC_(25)	/* passthru iopb (from user prog) */
#define DIOCSELFLAGS	_DIOC_(26)	/* scsi only; set flags to be
	used when doing a mode select (DIOCSELECT);  defaults to 0x11
	(PF and SP bits are set).  Argument is the new flags value.
	Lasts until last close on the drive, or is reset. */

/* IPI commands */
#define DIOCREADCONFIG	_DIOC_(27)	/* read configuration parameters */
#define DIOCREADFORMAT	_DIOC_(28)	/* read format parameters */

#define DIOCSETBLKSZ	_DIOC_(29)	/* intended mainly for standalone
	for use with CD-ROM.  Changes the current params only, doesn't
	save the change on the device.  new blksize is in 3rd argument to
	ioctl (&int).  returns previous blksize in 3rd arg on success.  */

	/* _DIOC_(30-35)	 (obsolete) */

#define SCSI_DEVICE_NAME_SIZE 28	/* size for DIOCDRIVETYPE */

#define IPI_FORMAT_SPEC_SIZE 0x20	/* size of IPI format spec table */
#define IPI_CONFIG_SIZE 0x4A		/* size of IPI configuration table */
#define IPI_DEFECT_SECTOR_SIZE 1044	/* size of IPI defect sector from SV */

/* xlv logical volume manager driver commands. */
#define DIOCGETXLVDEV	_DIOC_(36)	/* get subvolume device nodes */
#define DIOCGETSUBVOLINFO _DIOC_(37)	/* get xlv subvolume info */

/* Added for foreign volume support, i.e., Adobe formatted disks */
#define DIOCALLOWRAWIO	_DIOC_(38)
/* added support of inquiry command for Adobe Input SCSI protocol */
#define DIOCINQUIRY	_DIOC_(39)
#define DIOCINQUIRYPAGE	_DIOC_(40)
#define DIOCNORETRY	_DIOC_(41)	/* turn on/off no retry flag */

#define DIOCXLVPLEXCOPY _DIOC_(42)	/* make the plexes in an
					   xlv subvolume consistent. */

#define DIOCNOSORT      _DIOC_(44)      /* turn on/off no sort flag */

#define DIOCGETXLV      _DIOC_(45)      /* get XLV logical volume description */
#define DIOCMKALIAS	_DIOC_(46)	/* make aliases /hw/disk/dks#d#l#s#  &&
					 *             	/hw/rdisk/dks#d#l#s# 
					 */
#define DIOCFOUPDATE	_DIOC_(48)	/* update failover info for generic dev */
#define DIOCFORCE	_DIOC_(128)	/* "FORCE" is a flag that can be or'd
					 * in with DIOCSETVH to bypass the
					 * specfs "in-use" test that would
					 * fail the SETVH with an EBUSY if
					 * a partition was already "in-use".
					 * Note: Using this bit as a "flag"
					 *	 limits the # of DIOC ioctl's
					 *	 to 127 in number.
					 */

/* structures used by fx */

struct rawread_info {
	daddr_t rr_lbn;			/* block number to read */
	char	*rr_buf;		/* buf to read into */
	int	rr_error;
};

struct read_trackid {
	int	cyl;
	int	head;
	int	buflen;
	caddr_t buf;
};

/*
 * Added from System 5.3:
 *	Ioctls to disk drivers will pass the address to this
 *	structure which the driver handle as appropriate.
 *
 */
struct io_arg {
	int retval;
	unsigned int sectst;
	unsigned int memaddr;
	unsigned int datasz;
};

/*
 * driver ioctl() commands not supported
 */
#define VIOC			('V'<<8)
#define V_PREAD			(VIOC|1)	/* Physical Read */
#define V_PWRITE		(VIOC|2)	/* Physical Write */
#define V_PDREAD		(VIOC|3)	/* Read of Physical 
						 * Description Area */
#define V_PDWRITE		(VIOC|4)	/* Write of Physical 
						 * Description Area */
#define V_GETSSZ		(VIOC|5)	/* Get the sector size of media */
