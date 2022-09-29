#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_page.c,v 1.8 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include <klib/alloc_private.h>
#include "icrash.h"

#ifdef ICRASH_DEBUG
/*
 * page_banner()
 */
void
page_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, "    PAGE  STATE   BLKLIST    BLKSZ  NBLOCKS  NFREE  "
			"INDEX      NEXT      PREV\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "===================================================="
			"==========================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "----------------------------------------------------"
			"--------------------------\n");
	}
}

/*
 * page_print()
 */
page_print(page_t *p, int flags, FILE *ofp)
{
	int pcount = 1;
	page_t *np;

	fprintf(ofp, "%8x  %5d  %8x  %7d  %7d  %5d  %5d  %8x  %8x\n", 
		p, p->state, p->blklist, p->blksz, p->nblocks, 
		p->nfree, p->index, p->next, p->prev);

	if (flags & C_NEXT) {
		np = p->next;
		while (np != p) {
			fprintf(ofp, "%8x  %5d  %8x  %7d  %7d  %5d  %5d  %8x  %8x\n", 
				np, np->state, np->blklist, np->blksz, p->nblocks, 
				np->nfree, np->index, np->next, np->prev);
			np = np->next;
			pcount++;
		}
	}
	return (pcount);
}

/*
 * page_cmd() -- Dump out page_t information, properly formatted.
 */
int
page_cmd(command_t cmd)
{
	int i, page_cnt = 0, bcount = 0, first_time = 1;
	page_t *page;

	/* If there aren't any pages then just return.
	 */
	if (cmd.nargs == 0) {
		return(1);
	}

	page_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		page = (page_t *)strtoul(cmd.args[i], (char**)NULL, 16);
		page_cnt = page_print(page, cmd.flags, cmd.ofp);
	}
	page_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d page%s found\n", page_cnt, (page_cnt != 1) ? "s" : "");
	return(0);
}

/*
 * page_parse() -- Parse command line options for 'page' command.
 */
int
page_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_NEXT);
}

#define _PAGE_USAGE "[-n] [-w outfile] page_addr"

/*
 * page_help() -- Print help information for the 'page' command.
 */
void
page_help(command_t cmd)
{
	CMD_HELP(cmd, _PAGE_USAGE,
		"Print out memory page information related to the internal memory "
		"manager in 'icrash'.");
}

/*
 * page_usage() -- Print usage information for the 'page' command.
 */
void
page_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PAGE_USAGE);
}
#endif
