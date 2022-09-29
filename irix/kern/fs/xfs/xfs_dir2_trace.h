#ifndef _FS_XFS_DIR2_TRACE_H
#define _FS_XFS_DIR2_TRACE_H

#ident "$Revision: 1.1 $"

/*
 * Tracing for xfs v2 directories.
 */

struct ktrace;
struct xfs_dabuf;
struct xfs_da_args;

#ifdef XFS_ALL_TRACE
#define XFS_DIR2_TRACE
#endif	/* XFS_ALL_TRACE */

#if !defined(DEBUG) || defined(SIM)
#undef XFS_DIR2_TRACE
#endif	/* !DEBUG || SIM */

#define	XFS_DIR2_GTRACE_SIZE		4096	/* global buffer */
#define	XFS_DIR2_KTRACE_SIZE		32	/* per-inode buffer */

#define	XFS_DIR2_KTRACE_ARGS		1	/* args only */
#define	XFS_DIR2_KTRACE_ARGS_B		2	/* args + buffer */
#define	XFS_DIR2_KTRACE_ARGS_BB		3	/* args + 2 buffers */ 
#define	XFS_DIR2_KTRACE_ARGS_DB		4	/* args, db, buffer */
#define	XFS_DIR2_KTRACE_ARGS_I		5	/* args, inum */
#define	XFS_DIR2_KTRACE_ARGS_S		6	/* args, int */
#define	XFS_DIR2_KTRACE_ARGS_SB		7	/* args, int, buffer */
#define	XFS_DIR2_KTRACE_ARGS_BIBII	8	/* args, buf/int/buf/int/int */

#ifdef XFS_DIR2_TRACE

void xfs_dir2_trace_args(char *where, struct xfs_da_args *args);
void xfs_dir2_trace_args_b(char *where, struct xfs_da_args *args,
			   struct xfs_dabuf *bp);
void xfs_dir2_trace_args_bb(char *where, struct xfs_da_args *args,
			    struct xfs_dabuf *lbp, struct xfs_dabuf *dbp);
void xfs_dir2_trace_args_bibii(char *where, struct xfs_da_args *args,
			       struct xfs_dabuf *bs, int ss,
			       struct xfs_dabuf *bd, int sd, int c);
void xfs_dir2_trace_args_db(char *where, struct xfs_da_args *args,
			    xfs_dir2_db_t db, struct xfs_dabuf *bp);
void xfs_dir2_trace_args_i(char *where, struct xfs_da_args *args, xfs_ino_t i);
void xfs_dir2_trace_args_s(char *where, struct xfs_da_args *args, int s);
void xfs_dir2_trace_args_sb(char *where, struct xfs_da_args *args, int s,
			    struct xfs_dabuf *bp);

#else	/* XFS_DIR2_TRACE */

#define	xfs_dir2_trace_args(where, args)
#define	xfs_dir2_trace_args_b(where, args, bp)
#define	xfs_dir2_trace_args_bb(where, args, lbp, dbp)
#define	xfs_dir2_trace_args_bibii(where, args, bs, ss, bd, sd, c)
#define	xfs_dir2_trace_args_db(where, args, db, bp)
#define	xfs_dir2_trace_args_i(where, args, i)
#define	xfs_dir2_trace_args_s(where, args, s)
#define	xfs_dir2_trace_args_sb(where, args, s, bp)

#endif	/* XFS_DIR2_TRACE */

extern struct ktrace *xfs_dir2_trace_buf;

#endif	/* !_FS_XFS_DIR2_TRACE_H */
