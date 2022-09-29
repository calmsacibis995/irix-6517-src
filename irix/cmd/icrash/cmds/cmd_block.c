#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_block.c,v 1.8 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include <klib/alloc_private.h>
#include "icrash.h"

#ifdef ICRASH_DEBUG
/*
 * block_banner()
 */
void
block_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, "   BLOCK      ADDR  FLAG      PAGE      NEXT      "
			"PREV  ALLOC_PC\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "=================================================="
			"==============\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "--------------------------------------------------"
			"--------------\n");
	}
}

/* 
 * block_print()
 */
block_print(block_t *b, int flags, FILE *ofp)
{
	int bcount = 1;
	block_t *nb;

	fprintf(ofp, "%8x  %8x  %4d  %8x  %8x  %8x  %8x\n", 
		b, b->addr, b->flag, b->page, b->next, b->prev, b->alloc_pc);

	if (flags & C_NEXT) {
		nb = b->next;
		while (nb != b) {
			fprintf(ofp, "%8x  %8x  %4d  %8x  %8x  %8x  %8x\n", 
				nb, nb->addr, nb->flag, nb->page, 
				nb->next, nb->prev, nb->alloc_pc);
			nb = nb->next;
			bcount++;
		}
	}
	return (bcount);
}

/*
 * block_cmd() -- Dump out block_t information, properly formatted.
 */
int
block_cmd(command_t cmd)
{
	int i, block_cnt = 0, first_time = 1;
	block_t *block;

	/* If there aren't any blocks then just return.
	 */
	if (cmd.nargs == 0) {
		return(0);
	}

	block_banner(cmd.ofp, BANNER|SMAJOR);

	for (i = 0; i < cmd.nargs; i++) {
		block = (block_t *)strtoul(cmd.args[i], (char**)NULL, 16);
		block_cnt += block_print(block, cmd.flags, cmd.ofp);
	}
	block_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d block%s found\n", 
		block_cnt, (block_cnt != 1) ? "s" : "");
	return(0);
}

/*
 * block_parse() -- Parse command line options for 'block' command.
 */
int
block_parse(command_t cmd)
{
	return (C_TRUE|C_NEXT|C_WRITE);
}

#define _BLOCK_USAGE "[-n] [-w outfile] block_addr"

/*
 * block_help() -- Print help information for the 'block' command.
 */
void
block_help(command_t cmd)
{
	CMD_HELP(cmd, _BLOCK_USAGE,
		"Print out memory block information related to the internal memory "
		"manager in 'icrash'.");
}

/*
 * block_usage() -- Print usage information for the 'block' command.
 */
void
block_usage(command_t cmd)
{
	CMD_USAGE(cmd, _BLOCK_USAGE);
}
#endif
