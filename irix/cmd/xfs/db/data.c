#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include "sim.h"
#include "data.h"

int			blkbb;
xfs_agnumber_t		cur_agno = NULLAGNUMBER;
int			exitcode;
int                     flag_expert_mode = 0;
int                     flag_readonly = 0;
sim_init_t		simargs;
