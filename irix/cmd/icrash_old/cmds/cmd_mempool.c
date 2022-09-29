#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_mempool.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <assert.h>
#include "icrash.h"
#include "alloc.h"
#include "extern.h"

#ifdef ICRASH_DEBUG
/*
 * mempool_cmd() -- Dump out contents of mempool struct
 */
int
mempool_cmd(command_t cmd)
{
	fprintf(cmd.ofp, "\n");
	fprintf(cmd.ofp, "PAGES\n");
	fprintf(cmd.ofp, "===============\n");
	fprintf(cmd.ofp, "         PGS : 0x%x\n", mempool.pgs);
	fprintf(cmd.ofp, "        NPGS : %d\n", mempool.npgs);
	fprintf(cmd.ofp, "    FREE_PGS : %d\n", mempool.free_pgs);
	fprintf(cmd.ofp, "       ALLOC : %d\n", mempool.m_alloc);
	fprintf(cmd.ofp, "        FREE : %d\n", mempool.m_free);
	fprintf(cmd.ofp, "        HIGH : %d\n", mempool.m_high);

	fprintf(cmd.ofp, "\n");
	fprintf(cmd.ofp, "PAGE HEADERS\n");
	fprintf(cmd.ofp, "===============\n");
	fprintf(cmd.ofp, "      PGHDRS : 0x%x\n", mempool.pghdrs);
	fprintf(cmd.ofp, "     NPGHDRS : %d\n", mempool.npghdrs);
	fprintf(cmd.ofp, " FREE_PGHDRS : %d\n", mempool.free_pghdrs);

	fprintf(cmd.ofp, "\n");
	fprintf(cmd.ofp, "BLOCK HEADERS\n");
	fprintf(cmd.ofp, "===============\n");
	fprintf(cmd.ofp, "     BLKHDRS : 0x%x\n", mempool.blkhdrs);
	fprintf(cmd.ofp, "    NBLKHDRS : %d\n", mempool.nblkhdrs);
	fprintf(cmd.ofp, "FREE_BLKHDRS : %d\n", mempool.free_blkhdrs);
	fprintf(cmd.ofp, "\n");

	fprintf(cmd.ofp, " OVRSZPGS -> 0x%x\n", mempool.ovrszpgs);
	fprintf(cmd.ofp, "TEMP_BLKS -> 0x%x\n", temp_blks);
#ifdef ICRASH_DEBUG
	fprintf(cmd.ofp, "PERM_BLKS -> 0x%x\n", perm_blks);
#endif
	fprintf(cmd.ofp, "\n");
	return(0);
}

/*
 * mempool_parse() -- Parse command line options for 'mempool' command.
 */
int
mempool_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE);
}

#define _MEMPOOL_USAGE "[-w outfile]"

/*
 * mempool_help() -- Print help information for the 'mempool' command.
 */
void
mempool_help(command_t cmd)
{
	CMD_HELP(cmd, _MEMPOOL_USAGE,
		"Print out memory pool information related to the internal memory "
		"manager in 'icrash'.");
}

/*
 * mempool_usage() -- Print usage information for the 'mempool' command.
 */
void
mempool_usage(command_t cmd)
{
	CMD_USAGE(cmd, _MEMPOOL_USAGE);
}
#endif
