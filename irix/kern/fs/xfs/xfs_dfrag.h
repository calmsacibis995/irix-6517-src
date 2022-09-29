#ifndef _FS_XFS_DFRAG_H
#define	_FS_XFS_DFRAG_H

#ident "$Id: xfs_dfrag.h,v 1.1 1999/03/05 23:44:18 cwf Exp $"

/*
 * Structure passed to xfs_swapext
 */

typedef struct xfs_swapext
{
	__int64_t	sx_version;	/* version */	
	__int64_t	sx_fdtarget;	/* fd of target file */
	__int64_t	sx_fdtmp;	/* fd of tmp file */
	off64_t		sx_offset; 	/* offset into file */
	off64_t		sx_length; 	/* leng from offset */
	char		sx_pad[16];	/* pad space, unused */
	xfs_bstat_t	sx_stat;	/* stat of target b4 copy */
} xfs_swapext_t;

/* 
 * Version flag
 */
#define XFS_SX_VERSION		0


#ifdef _KERNEL
/*
 * Prototypes for visible xfs_dfrag.c routines.
 */

/*
 * Syssgi interface for xfs_swapext
 */
int	xfs_swapext(struct xfs_swapext *sx);

#endif	/* _KERNEL */

#endif	/* !_FS_XFS_DFRAG_H */
