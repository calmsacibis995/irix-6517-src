#ident "$Revision: 1.2 $"

#ifndef _XR_ATTRREPAIR_H
#define _XR_ATTRREPAIR_H

struct blkmap;

int
process_attributes(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	struct blkmap	*blkmap,
	int		*repair);


#endif /* _XR_ATTRREPAIR_H */
