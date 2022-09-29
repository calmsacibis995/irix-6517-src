#ifndef _FS_XFS_ALIST_H
#define	_FS_XFS_ALIST_H

#ident	"$Revision: 1.2 $"

struct alloc
{
	xfs_fsblock_t bno;
	xfs_extlen_t len;
	int rt;
	struct alloc *next;
	struct alloc *prev;	/* used only in aused list */
};

extern int alist_nallocs;
#define ALLOC_INCR 200

void add_alloced(xfs_fsblock_t, xfs_extlen_t, int);
void get_alloced(int, xfs_fsblock_t *, xfs_extlen_t *, int *, int);
void print_alist(void);

#endif	/* !_FS_XFS_ALIST_H */
