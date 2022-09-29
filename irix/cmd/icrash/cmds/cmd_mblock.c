#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_mblock.c,v 1.8 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

#ifdef NOTYET
/*
 * print_mblock()
 */
int
print_mblock(kaddr_t mblock, k_ptr_t mblkp, int flags, FILE *ofp)
{
	k_ptr_t dblkp;

	fprintf(ofp, "%8x  %8x  %8x  %8x  %8x  %8x   %3d %4x  %8x\n",
		mblock, mblkp->b_next, mblkp->b_prev, mblkp->b_cont,
		mblkp->b_rptr, mblkp->b_wptr, mblkp->b_band,
		mblkp->b_flag, mblkp->b_datap);
	if (flags & DO_FULL) {
		if (dblkp = GET_DATAB(mblkp->b_datap, &dblkbuf)) {
			fprintf(ofp, "\n    DBLOCK  RCNT  SIZE  TYPE  ISWHAT  ZTYPE      BASE     LIMIT     FREEP\n");
			fprintf(ofp, "  -----------------------------------------------------------------------\n");
			print_dblock(mblkp->b_datap, dblkp, flags, ofp);
		}
	}
}

/*
 * do_msg()
 */
int
do_msg(struct msgb *msgp, int flags, FILE *ofp)
{
	int i, first_time = 1;
	int msgcnt, mblocks, dblocks[10];
	struct msgb *mblkp, *cur_mblk, msgbuf, mblkbuf;
	struct datab dblkbuf, *dblkp;

	mblocks = 0;
	msgcnt = 0;
	for (i = 0; i < 10; i++) {
		dblocks[i] = 0;
	}

	if (debug > 3) {
		fprintf(ofp, "do_msg: msgp=0x%x, flags=0x%x\n", msgp, flags);
	}

	while (msgp) {
		if (!(mblkp = GET_MSGB(msgp, &msgbuf))) {
			if (debug) {
				fprintf(ofp,
					"0x%x does not point to a valid mblock.\n", msgp);
			}
			return(-1);
		}
		msgcnt++;
		cur_mblk = msgp;

mblk_cont:
		mblocks++;
		if (debug > 4) {
			fprintf(ofp, "do_msg: dblkp = 0x%x\n", mblkp->b_datap);
		}
		if (!(dblkp = GET_DATAB(mblkp->b_datap, &dblkbuf))) {
			if (debug) {
				fprintf(ofp,
					"0x%x does not point to a valid dblock.\n",
					mblkp->b_datap);
			}
			return(-1);
		}

		/* 
		 * Even if we get an mblock and dblock, do a little sanity 
		 * checking to ensure that they are valid (even if unused). 
		 * Make sure the read and write pointers in the mblock are 
		 * within the range of the data buffer.
		 */
		if ((mblkp->b_rptr < dblkp->db_base) ||
			(mblkp->b_rptr > dblkp->db_lim) ||
			(mblkp->b_wptr < dblkp->db_base) ||
			(mblkp->b_wptr > dblkp->db_lim)) {
				if (debug) {
					fprintf(ofp,
						"The mblock at 0x%x does not point to "
						"a valid dblock.\n", cur_mblk, mblkp->b_datap);
				}
				return(-1);
		}

		if (flags & DO_FULL) {
			if (!first_time) {
				fprintf(ofp, "  MBLOCK      NEXT      PREV      CONT      RPTR      WPTR  BAND FLAG     DATAB\n");
				fprintf(ofp, "===============================================================================\n");
			} else {
				first_time = 0;
			}
		}
		if (!(flags & DO_TOTALS)) {
			print_mblock((unsigned)cur_mblk, mblkp, flags, ofp);
		}
		if (flags & DO_FULL) {
			fprintf(ofp, "\n");
		}
		if (!(flags & DO_FOLLOW)) {
			goto mblk_done;
		}
		if (mblkp->b_cont) {
			cur_mblk = mblkp->b_cont;
			if (!(mblkp = GET_MSGB(cur_mblk, &mblkbuf))) {
				fprintf(ofp, "do_msg: bad mblock!  ");
				fprintf(ofp, "  mblock = 0x%x\n", cur_mblk); 
				return(-1);
			}
			goto mblk_cont;
		} else {
			msgp = msgbuf.b_next;
			if (msgp && (flags & DO_FULL)) {
				fprintf(ofp,"\nMESSAGE : %d\n\n", msgcnt + 1);
			}
		}
	}

	if (!(flags & DO_TOTALS))
		fprintf(ofp, "===============================================================================\n");
	fprintf(ofp,"message count : %3d\n", msgcnt);
	fprintf(ofp,"      mblocks : %3d\n", mblocks);

mblk_done:
	return(0);
}

/*
 * domblock()
 */
int
domblock(command_t cmd)
{
	int i, msgb_cnt = 0;
	unsigned mblock = 0;

	if ((cmd.flags & (DO_FOLLOW|DO_FULL)) == (DO_FOLLOW|DO_FULL)
		&& cmd.nargs) {
			fprintf(cmd.ofp, "\n  MESSAGE : 1\n");
	}
	if (!(cmd.flags & DO_TOTALS)) {
		fprintf(cmd.ofp, "\n  MBLOCK      NEXT      PREV      CONT      RPTR      WPTR  BAND FLAG     DATAB\n");
		fprintf(cmd.ofp, "===============================================================================\n");
	}
	for (i = 0; i < cmd.nargs; i++) {
		if (GET_VALUE(cmd.args[i], &mblock)) {
			fprintf(cmd.ofp, "%s not a valid msgb pointer!\n", cmd.args[i]);
			continue;
		}
		if (do_msg((struct msgb *)mblock, cmd.flags, cmd.ofp)) {
			if ((cmd.flags & DO_FULL) && i < (cmd.nargs - 1)) {
				fprintf(cmd.ofp, "  MBLOCK      NEXT      PREV      CONT      RPTR      WPTR  BAND FLAG     DATAB\n");
				fprintf(cmd.ofp, "===============================================================================\n");
			}
		}
	}

	if (!(cmd.flags & DO_FOLLOW)) {
		fprintf(cmd.ofp, "===============================================================================\n");
	}
	fprintf(cmd.ofp, "%d msgb struct%s found\n", 
		msgb_cnt, (msgb_cnt != 1) ? "s" : "");
}
#endif /* NOTYET */
