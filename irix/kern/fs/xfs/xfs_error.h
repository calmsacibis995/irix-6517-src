#ifndef	_XFS_ERROR_H
#define	_XFS_ERROR_H

#ident "$Revision: 1.10 $"

#define XFS_ERECOVER	1	/* Failure to recover log */
#define XFS_ELOGSTAT	2	/* Failure to stat log in user space */
#define XFS_ENOLOGSPACE	3	/* Reservation too large */
#define XFS_ENOTSUP	4	/* Operation not supported */
#define	XFS_ENOLSN	5	/* Can't find the lsn you asked for */
#define XFS_ENOTFOUND	6
#define XFS_ENOTXFS	7	/* Not XFS filesystem */

#ifdef DEBUG
#define	XFS_ERROR_NTRAP	10
extern int	xfs_etrap[XFS_ERROR_NTRAP];
extern int	xfs_error_trap(int);
#define	XFS_ERROR(e)	xfs_error_trap(e)
#else
#define	XFS_ERROR(e)	(e)
#endif

#endif	/* _XFS_ERROR_H */


/*
 * error injection tags - the labels can be anything you want
 * but each tag should have its own unique number
 */

#define XFS_ERRTAG_NOERROR				0
#define XFS_ERRTAG_IFLUSH_1				1
#define XFS_ERRTAG_IFLUSH_2				2
#define XFS_ERRTAG_IFLUSH_3				3
#define XFS_ERRTAG_IFLUSH_4				4
#define XFS_ERRTAG_IFLUSH_5				5
#define XFS_ERRTAG_IFLUSH_6				6
#define	XFS_ERRTAG_DA_READ_BUF				7
#define	XFS_ERRTAG_BTREE_CHECK_LBLOCK			8
#define	XFS_ERRTAG_BTREE_CHECK_SBLOCK			9
#define	XFS_ERRTAG_ALLOC_READ_AGF			10
#define	XFS_ERRTAG_IALLOC_READ_AGI			11
#define	XFS_ERRTAG_ITOBP_INOTOBP			12
#define	XFS_ERRTAG_IUNLINK				13
#define	XFS_ERRTAG_IUNLINK_REMOVE			14
#define	XFS_ERRTAG_DIR_INO_VALIDATE			15
#define XFS_ERRTAG_BULKSTAT_READ_CHUNK			16
#define XFS_ERRTAG_MAX					17

/*
 * Random factors for above tags, 1 means always, 2 means 1/2 time, etc.
 */
#define	XFS_RANDOM_DEFAULT				100
#define	XFS_RANDOM_IFLUSH_1				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IFLUSH_2				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IFLUSH_3				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IFLUSH_4				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IFLUSH_5				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IFLUSH_6				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_DA_READ_BUF				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_BTREE_CHECK_LBLOCK			(XFS_RANDOM_DEFAULT/4)
#define	XFS_RANDOM_BTREE_CHECK_SBLOCK			XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_ALLOC_READ_AGF			XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IALLOC_READ_AGI			XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_ITOBP_INOTOBP			XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IUNLINK				XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_IUNLINK_REMOVE			XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_DIR_INO_VALIDATE			XFS_RANDOM_DEFAULT
#define	XFS_RANDOM_BULKSTAT_READ_CHUNK			XFS_RANDOM_DEFAULT

#if !defined(SIM) && (defined(DEBUG) || defined(INDUCE_IO_ERROR))
extern int	xfs_error_test(int, int *, char *, int, char *, unsigned long);

#define	XFS_NUM_INJECT_ERROR				10

#ifdef __ANSI_CPP__
#define XFS_TEST_ERROR(expr, mp, tag, rf)		\
	((expr) || \
	 xfs_error_test((tag), (mp)->m_fixedfsid, #expr, __LINE__, __FILE__, \
			 (rf)))
#else
#define XFS_TEST_ERROR(expr, mp, tag, rf)		\
	((expr) || \
	 xfs_error_test((tag), (mp)->m_fixedfsid, "expr", __LINE__, __FILE__, \
			(rf)))
#endif /* __ANSI_CPP__ */

int		xfs_errortag_add(int error_tag, int fd);
int		xfs_errortag_clear(int error_tag, int fd);

int		xfs_errortag_clearall(int fd);
int		xfs_errortag_clearall_umount(int64_t fsid, char *fsname,
						int loud);
#else
#define XFS_TEST_ERROR(expr, mp, tag, rf)	(expr)
#endif /* !SIM && (DEBUG || INDUCE_IO_ERROR) */

/*
 * XFS panic tags -- allow a call to xfs_cmn_err() be turned into
 *			a panic by setting xfs_panic_mask in the
 *			stune file.
 */
#define		XFS_NO_PTAG			0LL
#define		XFS_PTAG_IFLUSH			0x0000000000000001LL
#define 	XFS_PTAG_LOGRES			0x0000000000000002LL
#define 	XFS_PTAG_AILDELETE		0x0000000000000004LL

struct xfs_mount;
extern uint64_t	xfs_panic_mask;
/* PRINTFLIKE4 */
void		xfs_cmn_err(uint64_t panic_tag, int level, struct xfs_mount *mp,
			    char *fmt, ...);
/* PRINTFLIKE3 */
void		xfs_fs_cmn_err(int level, struct xfs_mount *mp, char *fmt, ...);
