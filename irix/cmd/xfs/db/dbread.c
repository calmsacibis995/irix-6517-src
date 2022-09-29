#ident "$Revision: 1.4 $"

#include <sys/param.h>
#include <sys/uuid.h>
#include <unistd.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bit.h>
#include "sim.h"
#include "bmap.h"
#include "data.h"
#include "dbread.h"
#include "io.h"
#include "mount.h"

int
dbread(void *buf, int nblocks, xfs_fileoff_t bno, int whichfork)
{
	bmap_ext_t	bm;
	char		*bp;
	xfs_dfiloff_t	eb;
	xfs_dfiloff_t	end;
	int		i;
	int		nex;

	nex = 1;
	end = bno + nblocks;
	bp = buf;
	while (bno < end) {
		bmap(bno, end - bno, whichfork, &nex, &bm);
		if (nex == 0) {
			bm.startoff = end;
			bm.blockcount = 1;
		}
		if (bm.startoff > bno) {
			eb = end < bm.startoff ? end : bm.startoff;
			i = (int)XFS_FSB_TO_B(mp, eb - bno);
			memset(bp, 0, i);
			bp += i;
			bno = eb;
		}
		if (bno == end)
			break;
		if (bno > bm.startoff) {
			bm.blockcount -= bno - bm.startoff;
			bm.startblock += bno - bm.startoff;
			bm.startoff = bno;
		}
		if (bm.startoff + bm.blockcount > end)
			bm.blockcount = end - bm.startoff;
		i = read_bbs(XFS_FSB_TO_DADDR(mp, bm.startblock),
			     (int)XFS_FSB_TO_BB(mp, bm.blockcount),
			     (void **)&bp, NULL);
		if (i)
			return i;
		bp += XFS_FSB_TO_B(mp, bm.blockcount);
		bno += bm.blockcount;
	}
	return 0;
}
