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
#include "agfl.h"
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

static int agfl_f(int argc, char **argv);
static void agfl_help(void);

static const cmdinfo_t agfl_cmd =
	{ "agfl", NULL, agfl_f, 0, 1, 1, "[agno]", 
	  "set address to agfl block", agfl_help };

const field_t	agfl_hfld[] = {
	{ "", FLDT_AGFL, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_agfl_t, agfl_ ## f))
const field_t	agfl_flds[] = {
	{ "bno", FLDT_AGBLOCKNZ, OI(OFF(bno)), CI(XFS_AGFL_SIZE), FLD_ARRAY,
	  TYP_DATA },
	{ NULL }
};

static void
agfl_help(void)
{
	dbprintf(
"\n"
" set allocation group freelist\n"
"\n"
" Example:\n"
"\n"
" agfl 5"
"\n"
" Located in the 4th 512 byte block of each allocation group,\n"
" the agfl freelist for internal btree space allocation is maintained\n"
" for each allocation group.  This acts as a reserved pool of space\n" 
" separate from the general filesystem freespace (not used for user data).\n"
"\n"
);

}

static int
agfl_f(
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
	assert(typtab[TYP_AGFL].typnm == TYP_AGFL);
	set_cur(&typtab[TYP_AGFL], XFS_AG_DADDR(mp, cur_agno, XFS_AGFL_DADDR),
		1, DB_RING_ADD, NULL);
	return 0;
}

void
agfl_init(void)
{
	add_command(&agfl_cmd);
}

/*ARGSUSED*/
int
agfl_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_sectsize);
}
