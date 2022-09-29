#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_bucket.c,v 1.10 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include <klib/alloc_private.h>
#include "icrash.h"

#ifdef ICRASH_DEBUG
/*
 * bucket_banner()
 */
void
bucket_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "BUCKET  SIZE  NPAGES  NBLKS  FREEBLKS   ALLOC    "
			"FREE    HIGH  PAGELIST\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "================================================="
			"======================\n");
	}
}

/*
 * bucket_print() -- Print out specific bucket information.
 */
void
bucket_print(int index, bucket_t *bucket, int flags, FILE *ofp)
{
	page_t *p;

	fprintf(ofp, "%6d  %4d  %6d  %5d  %8d  %6d  %6d  %6d  %8x\n", 
		index, bucket->blksize, bucket->npages, bucket->nblks, 
		bucket->freeblks, bucket->s_alloc, bucket->s_free, 
		bucket->s_high, bucket->pagelist);
	
	if (flags & C_NEXT) {
		if (p = bucket->pagelist) {
			fprintf(ofp, "\n");
			page_banner(ofp, SMINOR|BANNER);
			do {
				page_print(p, 0, ofp);
				p = p->next;
			} while (p != bucket->pagelist);
		}
	}
}

/*
 * bucket_cmd() -- Dump out bucket_t information, properly formatted.
 */
int
bucket_cmd(command_t cmd)
{
	int i, bucket_cnt = 0, bpages = 0, epages = 0, first_time = 1;
	k_uint_t index;

	fprintf(cmd.ofp, "\n");
	bucket_banner(cmd.ofp, SMAJOR|BANNER);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &index);
		if (KL_ERROR) {
			kl_print_error();
			continue;
		}
		if (index >= NBUCKETS) {
			fprintf(cmd.ofp, "%s: invalid bucket index\n", cmd.args[i]);
			continue;
		}
		if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & C_NEXT)) {
			if (!first_time) {
				fprintf(cmd.ofp, "\n");
				bucket_banner(cmd.ofp, SMAJOR|BANNER);
			} 
			else {
				first_time = 0;
			}
		}
		bucket_print((int)index, &bucket[index], cmd.flags, cmd.ofp);
		bucket_cnt++;
		if (index == 9) {
			epages += (bucket[index].npages);
		}
		else {
			bpages += (bucket[index].npages);
		}
	}
	if (cmd.nargs == 0) {
		for (i = 0; i < NBUCKETS; i++) {
			if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & C_NEXT)) {
				if (!first_time) {
					fprintf(cmd.ofp, "\n");
					bucket_banner(cmd.ofp, SMAJOR|BANNER);
				} 
				else {
					first_time = 0;
				}
			}
			bucket_print(i, &bucket[i], cmd.flags, cmd.ofp);
			bucket_cnt++;
			if (i == OVRSZBKT) {
				epages += (bucket[i].npages);
			}
			else {
				bpages += (bucket[i].npages);
			}
		}
	}
	if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & C_NEXT)) {
		fprintf(cmd.ofp, "\n");
	}
	bucket_banner(cmd.ofp, SMAJOR);
	if (bpages) {
		fprintf(cmd.ofp, "%4d bucket page%s in use\n", 
			bpages, (bpages != 1) ? "s" : "");
	}
	if (epages) {
		fprintf(cmd.ofp, "%4d extra page%s in use\n", 
			epages, (epages != 1) ? "s" : "");
	}
	return(0);
}

/*
 * bucket_parse() -- Parse command line options for 'bucket' command.
 */
int
bucket_parse(command_t cmd)
{
	return (C_WRITE|C_NEXT|C_MAYBE);
}

#define _BUCKET_USAGE "[-n] [-w outfile] [bucket_addr]"

/*
 * bucket_help() -- Print help information for the 'bucket' command.
 */
void
bucket_help(command_t cmd)
{
	CMD_HELP(cmd, _BUCKET_USAGE,
		"Print out memory bucket information related to the internal "
		"memory manager in 'icrash'.");
}

/*
 * bucket_usage() -- Print usage information for the 'bucket' command.
 */
void
bucket_usage(command_t cmd)
{
	CMD_USAGE(cmd, _BUCKET_USAGE);
}
#endif
