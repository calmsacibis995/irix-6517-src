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
#include "agi.h"
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

static int agi_f(int argc, char **argv);
static void agi_help(void);

static const cmdinfo_t agi_cmd =
	{ "agi", NULL, agi_f, 0, 1, 1, "[agno]",
	  "set address to agi header", agi_help };

const field_t	agi_hfld[] = {
	{ "", FLDT_AGI, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_agi_t, agi_ ## f))
const field_t	agi_flds[] = {
	{ "magicnum", FLDT_UINT32X, OI(OFF(magicnum)), C1, 0, TYP_NONE },
	{ "versionnum", FLDT_UINT32D, OI(OFF(versionnum)), C1, 0, TYP_NONE },
	{ "seqno", FLDT_AGNUMBER, OI(OFF(seqno)), C1, 0, TYP_NONE },
	{ "length", FLDT_AGBLOCK, OI(OFF(length)), C1, 0, TYP_NONE },
	{ "count", FLDT_AGINO, OI(OFF(count)), C1, 0, TYP_NONE },
	{ "root", FLDT_AGBLOCK, OI(OFF(root)), C1, 0, TYP_INOBT },
	{ "level", FLDT_UINT32D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "freecount", FLDT_AGINO, OI(OFF(freecount)), C1, 0, TYP_NONE },
	{ "newino", FLDT_AGINO, OI(OFF(newino)), C1, 0, TYP_INODE },
	{ "dirino", FLDT_AGINO, OI(OFF(dirino)), C1, 0, TYP_INODE },
	{ "unlinked", FLDT_AGINONN, OI(OFF(unlinked)),
	  CI(XFS_AGI_UNLINKED_BUCKETS), FLD_ARRAY, TYP_NONE },
	{ NULL }
};

static void
agi_help(void)
{
	dbprintf(
"\n"
" set allocation group inode btree\n"
"\n"
" Example:\n"
"\n"
" agi 3 (set location to 3rd allocation group inode btree and type to 'agi')\n"
"\n"
" Located in the 3rd 512 byte block of each allocation group,\n"
" the agi inode btree tracks all used/free inodes in the allocation group.\n"
" Inodes are allocated in 16k 'chunks', each btree entry tracks a 'chunk'.\n"
"\n"
);
}

static int
agi_f(
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
	assert(typtab[TYP_AGI].typnm == TYP_AGI);
	set_cur(&typtab[TYP_AGI], XFS_AG_DADDR(mp, cur_agno, XFS_AGI_DADDR), 1,
		DB_RING_ADD, NULL);
	return 0;
}

void
agi_init(void)
{
	add_command(&agi_cmd);
}

/*ARGSUSED*/
int
agi_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_sectsize);
}
