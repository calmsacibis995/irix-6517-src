#ident "$Revision: 1.4 $"

#define _SGI_MP_SOURCE

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL

#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_mount.h>

#include "sim.h"
#include "globals.h"
#include "incore.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"


void
init_bm_cursor(bmap_cursor_t *cursor, int num_levels)
{
	int i;

	bzero(cursor, sizeof(bmap_cursor_t));

	cursor->ino = NULLFSINO;
	cursor->num_levels = num_levels;

	for (i = 0; i < XR_MAX_BMLEVELS; i++)  {
		cursor->level[i].fsbno = NULLDFSBNO;
		cursor->level[i].right_fsbno = NULLDFSBNO;
		cursor->level[i].left_fsbno = NULLDFSBNO;
		cursor->level[i].first_key = NULLDFILOFF;
		cursor->level[i].last_key = NULLDFILOFF;
	}

	return;
}
