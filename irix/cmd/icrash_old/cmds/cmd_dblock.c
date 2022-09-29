#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_dblock.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "stream.h"
#include "extern.h"

#ifdef NOTYET
/*
 * print_dblock()
 */
int
print_dblock(unsigned dblock, struct datab *dblkp, int flags, FILE *ofp)
{
	fprintf(ofp, "  %8x   ", dblock);
	fprintf(ofp, "%3d  %4d   %3d     %3d    %3d  %8x  %8x  %8x\n", 
		dblkp->db_ref, dblkp->db_size, dblkp->db_type, 
		dblkp->db_iswhat, dblkp->db_ztype, dblkp->db_base, 
		dblkp->db_lim, dblkp->db_freep);
}

/*
 * dodblock()
 */
int
dodblock(command_t cmd)
{
	int i, datab_cnt = 0;
	unsigned dblock = 0;
	struct datab dblkbuf, *dblkp;

	if (!(cmd.flags & DO_TOTALS)) {
		fprintf(cmd.ofp, "\n    DBLOCK  RCNT  SIZE  TYPE  ISWHAT  ZTYPE      BASE     LIMIT     FREEP\n");
		fprintf(cmd.ofp, "  -----------------------------------------------------------------------\n");
	}

	for (i = 0; i < cmd.nargs; i++) {
		if (GET_VALUE(cmd.args[i], &dblock)) {
			fprintf(cmd.ofp, "%s not a valid datab pointer!\n", cmd.args[i]);
			continue;
		}
		if (dblkp = GET_DATAB(dblock, &dblkbuf)) {
			print_dblock(dblock, dblkp, cmd.flags, cmd.ofp);
		}
	}
	fprintf(cmd.ofp, "  -----------------------------------------------------------------------\n");
	fprintf(cmd.ofp, "%d datab struct%s found\n", 
		datab_cnt, (datab_cnt != 1) ? "s" : "");
}
#endif /* NOTYET */
