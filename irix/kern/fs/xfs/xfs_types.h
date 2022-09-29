#ifndef _FS_XFS_TYPES_H
#define	_FS_XFS_TYPES_H

#ident	"$Revision: 1.34 $"

/*
 * XFS types
 */

/*
 * Some types are conditional based on the selected configuration.
 * Set XFS_BIG_FILES=1 or 0 and XFS_BIG_FILESYSTEMS=1 or 0 depending
 * on the desired configuration.
 * XFS_BIG_FILES needs pgno_t to be 64 bits (64-bit kernels).
 * XFS_BIG_FILESYSTEMS needs daddr_t to be 64 bits (N32 and 64-bit kernels).
 *
 * Expect these to be set from klocaldefs, or from the machine-type
 * defs files for the normal case.
 */

#ifndef XFS_BIG_FILES
#if _MIPS_SIM == _ABI64
#define	XFS_BIG_FILES		1
#else
#define	XFS_BIG_FILES		0
#endif
#endif

#ifndef XFS_BIG_FILESYSTEMS
#if _MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32
#define	XFS_BIG_FILESYSTEMS	1
#else
#define	XFS_BIG_FILESYSTEMS	0
#endif
#endif

typedef int8_t		__int8_t;
typedef	u_int8_t	__uint8_t;

typedef	int16_t		__int16_t;
typedef	u_int16_t	__uint16_t;

typedef __uint32_t	xfs_agblock_t;	/* blockno in alloc. group */
typedef	__uint32_t	xfs_extlen_t;	/* extent length in blocks */
typedef	__uint32_t	xfs_agnumber_t;	/* allocation group number */
typedef __int32_t	xfs_extnum_t;	/* # of extents in a file */
typedef __int16_t	xfs_aextnum_t;	/* # extents in an attribute fork */
typedef	__int64_t	xfs_fsize_t;	/* bytes in a file */
typedef __uint64_t	xfs_ufsize_t;	/* unsigned bytes in a file */

typedef	__int32_t	xfs_suminfo_t;	/* type of bitmap summary info */
typedef	__int32_t	xfs_rtword_t;	/* word type for bitmap manipulations */

typedef	__int64_t	xfs_lsn_t;	/* log sequence number */
typedef	__int32_t	xfs_tid_t;	/* transaction identifier */

typedef	__uint32_t	xfs_dablk_t;	/* dir/attr block number (in file) */
typedef	__uint32_t	xfs_dahash_t;	/* dir/attr hash value */

typedef __uint16_t	xfs_prid_t;	/* prid_t truncated to 16bits in XFS */

/*
 * These types are 64 bits on disk but are either 32 or 64 bits in memory.
 * Disk based types:
 */
typedef __uint64_t	xfs_dfsbno_t;	/* blockno in filesystem (agno|agbno) */
typedef __uint64_t	xfs_drfsbno_t;	/* blockno in filesystem (raw) */
typedef	__uint64_t	xfs_drtbno_t;	/* extent (block) in realtime area */
typedef	__uint64_t	xfs_dfiloff_t;	/* block number in a file */
typedef	__uint64_t	xfs_dfilblks_t;	/* number of blocks in a file */

/*
 * Memory based types are conditional.
 */
#if XFS_BIG_FILESYSTEMS
typedef	__uint64_t	xfs_fsblock_t;	/* blockno in filesystem (agno|agbno) */
typedef __uint64_t	xfs_rfsblock_t;	/* blockno in filesystem (raw) */
typedef __uint64_t	xfs_rtblock_t;	/* extent (block) in realtime area */
typedef	__int64_t	xfs_srtblock_t;	/* signed version of xfs_rtblock_t */
#else
typedef	__uint32_t	xfs_fsblock_t;	/* blockno in filesystem (agno|agbno) */
typedef __uint32_t	xfs_rfsblock_t;	/* blockno in filesystem (raw) */
typedef __uint32_t	xfs_rtblock_t;	/* extent (block) in realtime area */
typedef	__int32_t	xfs_srtblock_t;	/* signed version of xfs_rtblock_t */
#endif
#if XFS_BIG_FILES
typedef	__uint64_t	xfs_fileoff_t;	/* block number in a file */
typedef	__int64_t	xfs_sfiloff_t;	/* signed block number in a file */
typedef	__uint64_t	xfs_filblks_t;	/* number of blocks in a file */
#else
typedef	__uint32_t	xfs_fileoff_t;	/* block number in a file */
typedef	__int32_t	xfs_sfiloff_t;	/* signed block number in a file */
typedef	__uint32_t	xfs_filblks_t;	/* number of blocks in a file */
#endif

/*
 * Null values for the types.
 */
#define	NULLDFSBNO	((xfs_dfsbno_t)-1)
#define	NULLDRFSBNO	((xfs_drfsbno_t)-1)
#define	NULLDRTBNO	((xfs_drtbno_t)-1)
#define	NULLDFILOFF	((xfs_dfiloff_t)-1)

#define	NULLFSBLOCK	((xfs_fsblock_t)-1)
#define	NULLRFSBLOCK	((xfs_rfsblock_t)-1)
#define	NULLRTBLOCK	((xfs_rtblock_t)-1)
#define	NULLFILEOFF	((xfs_fileoff_t)-1)

#define	NULLAGBLOCK	((xfs_agblock_t)-1)
#define	NULLAGNUMBER	((xfs_agnumber_t)-1)
#define	NULLEXTNUM	((xfs_extnum_t)-1)

/*
 * Max values for extlen, extnum, aextnum.
 */
#define	MAXEXTLEN	((xfs_extlen_t)0x001fffff)	/* 21 bits */
#define	MAXEXTNUM	((xfs_extnum_t)0x7fffffff)	/* signed int */
#define	MAXAEXTNUM	((xfs_aextnum_t)0x7fff)		/* signed short */

typedef enum {
	XFS_LOOKUP_EQi, XFS_LOOKUP_LEi, XFS_LOOKUP_GEi
} xfs_lookup_t;

typedef enum {
	XFS_BTNUM_BNOi, XFS_BTNUM_CNTi, XFS_BTNUM_BMAPi, XFS_BTNUM_INOi,
	XFS_BTNUM_MAX
} xfs_btnum_t;

#endif	/* !_FS_XFS_TYPES_H */
