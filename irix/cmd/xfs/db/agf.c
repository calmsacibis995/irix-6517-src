#ident "$Revision: 1.13 $"

#include <sys/param.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include "sim.h"
#include "agf.h"
#include "command.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "io.h"
#include "bit.h"
#include "output.h"
#include "mount.h"

static int agf_f(int argc, char **argv);
static void agf_help(void);

static const cmdinfo_t agf_cmd =
	{ "agf", NULL, agf_f, 0, 1, 1, "[agno]",
	  "set address to agf header", agf_help };

const field_t	agf_hfld[] = {
	{ "", FLDT_AGF, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_agf_t, agf_ ## f))
#define	SZ(f)	bitszof(xfs_agf_t, agf_ ## f)
const field_t	agf_flds[] = {
	{ "magicnum", FLDT_UINT32X, OI(OFF(magicnum)), C1, 0, TYP_NONE },
	{ "versionnum", FLDT_UINT32D, OI(OFF(versionnum)), C1, 0, TYP_NONE },
	{ "seqno", FLDT_AGNUMBER, OI(OFF(seqno)), C1, 0, TYP_NONE },
	{ "length", FLDT_AGBLOCK, OI(OFF(length)), C1, 0, TYP_NONE },
	{ "roots", FLDT_AGBLOCK, OI(OFF(roots)), CI(XFS_BTNUM_AGF),
	  FLD_ARRAY|FLD_SKIPALL, TYP_NONE },
	{ "bnoroot", FLDT_AGBLOCK,
	  OI(OFF(roots) + XFS_BTNUM_BNO * SZ(roots[XFS_BTNUM_BNO])), C1, 0,
	  TYP_BNOBT },
	{ "cntroot", FLDT_AGBLOCK,
	  OI(OFF(roots) + XFS_BTNUM_CNT * SZ(roots[XFS_BTNUM_CNT])), C1, 0,
	  TYP_CNTBT },
	{ "levels", FLDT_UINT32D, OI(OFF(levels)), CI(XFS_BTNUM_AGF),
	  FLD_ARRAY|FLD_SKIPALL, TYP_NONE },
	{ "bnolevel", FLDT_UINT32D,
	  OI(OFF(levels) + XFS_BTNUM_BNO * SZ(levels[XFS_BTNUM_BNO])), C1, 0,
	  TYP_NONE },
	{ "cntlevel", FLDT_UINT32D,
	  OI(OFF(levels) + XFS_BTNUM_CNT * SZ(levels[XFS_BTNUM_CNT])), C1, 0,
	  TYP_NONE },
	{ "flfirst", FLDT_UINT32D, OI(OFF(flfirst)), C1, 0, TYP_NONE },
	{ "fllast", FLDT_UINT32D, OI(OFF(fllast)), C1, 0, TYP_NONE },
	{ "flcount", FLDT_UINT32D, OI(OFF(flcount)), C1, 0, TYP_NONE },
	{ "freeblks", FLDT_EXTLEN, OI(OFF(freeblks)), C1, 0, TYP_NONE },
	{ "longest", FLDT_EXTLEN, OI(OFF(longest)), C1, 0, TYP_NONE },
	{ NULL }
};

static void
agf_help(void)
{
	dbprintf(
"\n"
" set allocation group free block list\n"
"\n"
" Example:\n"
"\n"
" agf 2 - move location to AGF in 2nd filesystem allocation group\n"
"\n"
" Located in the 2nd 512 byte block of each allocation group,\n"
" the AGF contains the root of two different freespace btrees:\n"
" The 'cnt' btree keeps track freespace indexed on section size.\n"
" The 'bno' btree tracks sections of freespace indexed on block number.\n"
);
}

static int
agf_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;
	char		*p;

	if (argc == 1) {
		agno = (xfs_agnumber_t)strtoul(argv[0], &p, 0);
		if (*p != '\0' || agno >= mp->m_sb.sb_agcount) {
			dbprintf("bad allocation group number %s\n", argv[0]);
			return 0;
		}
		cur_agno = agno;
	} else if (cur_agno == NULLAGNUMBER)
		cur_agno = 0;
	assert(typtab[TYP_AGF].typnm == TYP_AGF);
	set_cur(&typtab[TYP_AGF], XFS_AG_DADDR(mp, cur_agno, XFS_AGF_DADDR), 1,
		DB_RING_ADD, NULL);
	return 0;
}

void
agf_init(void)
{
	add_command(&agf_cmd);
}

/*ARGSUSED*/
int
agf_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_sectsize);
}
