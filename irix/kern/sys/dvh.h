#ident	"$Revision: 3.24 $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * dvh.h -- disk volume header
 */

/*
 * Format for volume header information
 *
 * The volume header is a block located at the beginning of all disk
 * media (sector 0).  It contains information pertaining to physical
 * device parameters and logical partition information.
 *
 * The volume header is manipulated by disk formatters/verifiers, 
 * partition builders (e.g. fx, dvhtool, and mkfs), and disk drivers.
 *
 * Previous versions of IRIX wrote a copy of the volume header is
 * located at sector 0 of each track of cylinder 0.  These copies were
 * never used, and reduced the capacity of the volume header to hold large
 * files, so this practice was discontinued.
 * The volume header is constrained to be less than or equal to 512
 * bytes long.  A particular copy is assumed valid if no drive errors
 * are detected, the magic number is correct, and the 32 bit 2's complement
 * of the volume header is correct.  The checksum is calculated by initially
 * zeroing vh_csum, summing the entire structure and then storing the
 * 2's complement of the sum.  Thus a checksum to verify the volume header
 * should be 0.
 *
 * The error summary table, bad sector replacement table, and boot blocks are
 * located by searching the volume directory within the volume header.
 *
 * Tables are sized simply by the integral number of table records that
 * will fit in the space indicated by the directory entry.
 *
 * The amount of space allocated to the volume header, replacement blocks,
 * and other tables is user defined when the device is formatted.
 */

/*
 * device parameters are in the volume header to determine mapping
 * from logical block numbers to physical device addresses
 * alignment of fields has to remain as it used to be, so old drive
 * headers still match.
 */
struct device_parameters {
	unsigned char	dp_unused1[4];
	unsigned short  _dp_cylinders;	/* backwards compat only, so older
		prtvtoc, fx, etc. don't have problems when drives moved around.
		Don't count it being filled in in the future. It and _heads, and
		_sect are deliberately named differently than the old fields in
		their positions */
	unsigned short  dp_unused2;
	unsigned short  _dp_heads;	/* backwards compat only */
	unsigned char	dp_ctq_depth;	/* Depth of CTQ queue */
	unsigned char	dp_unused3[3];
	unsigned short  _dp_sect;	/* backwards compat only */
	unsigned short  dp_secbytes;    /* length of sector in bytes */
	unsigned char	dp_unused4[2];
	int dp_flags;	/* flags used by disk driver */
	unsigned char	dp_unused5[20];
	unsigned int	dp_drivecap;    /* drive capacity in blocks;
		this is in a field that was never used for scsi drives,
		prior to irix 6.3, so it will often be zero.
		When found to be zero, or whenever drive capacity changes,
		this is reset by fx; programs should not rely on this being
		non-zero, since older drives might well never have had this
		newer fx fx run on them. */
};

/*
 * Device characterization flags
 * (dp_flags)
 */
#define	DP_CTQ_EN	0x00000040	/* enable command tag queueing */

/*
 * Boot blocks, bad sector tables, and the error summary table, are located
 * via the volume_directory.
 */
#define VDNAMESIZE	8

struct volume_directory {
	char	vd_name[VDNAMESIZE];	/* name */
	int	vd_lbn;			/* logical block number */
	int	vd_nbytes;		/* file length in bytes */
};

/*
 * partition table describes logical device partitions
 * (device drivers examine this to determine mapping from logical units
 * to cylinder groups, device formatters/verifiers examine this to determine
 * location of replacement tracks/sectors, etc)
 *
 * NOTE: pt_firstlbn SHOULD BE CYLINDER ALIGNED
 */
struct partition_table {		/* one per logical partition */
	int	pt_nblks;		/* # of logical blks in partition */
	int	pt_firstlbn;		/* first lbn of partition */
	int	pt_type;		/* use of partition */
};

#define PTYPE_VOLHDR_DFLTSZ 4096	/* default volhdr size is this many
	blocks.  Used by sash "disksetup=true" and fx "standard" setup.
	Defined here to be sure it remains the same in both places (issue
	with fx -s mode in miniroot, primarily). */

#define	PTYPE_VOLHDR	0		/* partition is volume header */
/* 1 and 2 were used for drive types no longer supported */
#define	PTYPE_RAW	3		/* partition is used for data */
/* 4 and 5 were for filesystem types we haven't ever supported on MIPS cpus */
#define	PTYPE_VOLUME	6		/* partition is entire volume */
#define	PTYPE_EFS	7		/* partition is sgi EFS */
#define	PTYPE_LVOL	8		/* partition is part of a logical vol */
#define	PTYPE_RLVOL	9		/* part of a "raw" logical vol */
#define	PTYPE_XFS	10		/* partition is sgi XFS */
#define	PTYPE_XFSLOG	11		/* partition is sgi XFS log */
#define	PTYPE_XLV	12		/* partition is part of an XLV vol*/
#define NPTYPES		16

#define	VHMAGIC		0xbe5a941	/* randomly chosen value */
#define	NPARTAB		16		/* 16 unix partitions */
#define	NVDIR		15		/* max of 15 directory entries */
#define BFNAMESIZE	16		/* max 16 chars in boot file name */

/* Partition types for ARCS */
#define NOT_USED        0       /* Not used 				*/
#define FAT_SHORT       1       /* FAT filesystem, 12-bit FAT entries 	*/
#define FAT_LONG        4       /* FAT filesystem, 16-bit FAT entries 	*/
#define EXTENDED        5       /* extended partition 			*/
#define HUGE            6       /* huge partition- MS/DOS 4.0 and later */

/* Active flags for ARCS */
#define BOOTABLE        0x00;
#define NOT_BOOTABLE    0x80;

struct volume_header {
	int vh_magic;				/* identifies volume header */
	short vh_rootpt;			/* root partition number */
	short vh_swappt;			/* swap partition number */
	char vh_bootfile[BFNAMESIZE];		/* name of file to boot */
	struct device_parameters vh_dp;		/* device parameters */
	struct volume_directory vh_vd[NVDIR];	/* other vol hdr contents */
	struct partition_table vh_pt[NPARTAB];	/* device partition layout */
	int vh_csum;				/* volume header checksum */
	int vh_fill;	/* fill out to 512 bytes */
};

#if _KERNEL
extern int is_vh(register struct volume_header *);
extern int vh_checksum(struct volume_header *);
#endif /* _KERNEL */
