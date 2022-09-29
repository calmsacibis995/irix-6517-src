#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_strstat.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "stream.h"
#include "extern.h"

/*
 * print_strdzone()
 */
int
print_strdzone(int flags, FILE *ofp)
{
	int i, first_time = 1;
	kaddr_t z;
	k_ptr_t zp;

	zp = alloc_block(ZONE_SIZE(K), B_TEMP);

	zone_banner(ofp, BANNER|SMAJOR);
	for (i = 0; i < STR_NUM_DZONE; i++) {
		kl_get_kaddr(K, S_strdzone + 
			(((i + 1) * K_NBPW(K)) + (i * K_NBPW(K))), &z, "strdzone");
		if (KL_ERROR) {
			kl_print_error(K);
			return(1);
		}
		kl_get_struct(K, z, ZONE_SIZE(K), zp, "zone");
		if (KL_ERROR) {
			kl_print_error(K);
			return(1);
		}
		if (!first_time) {
			if (flags & C_FULL) {
				zone_banner(ofp, BANNER|SMAJOR);
			}
		} 
		else {
			first_time = 0;
		}
		print_zone(z, zp, flags, ofp);
	}
	fprintf(ofp, "\n");
	return(0);
}

/*
 * strstat_cmd() -- Run the 'strstat' command.
 */
int
strstat_cmd(command_t cmd)
{
	int i, blkcnt = 0;
	alcdat *a;

	/* If this is a live system, get the contents of strst (otherwise,
	 * we got it at startup).
	 */
	if (ACTIVE(K)) {
		kl_get_struct(K, K_STRST(K), STRSTAT_SIZE(K), strstp, "strst");
	}

	if (!strstp) {
		fprintf(cmd.ofp, "Could not retrieve stream statistics data!\n\n");
		return (0);
	}

	fprintf(cmd.ofp, "\nITEM              ALLOCATED      MAX      "
		"TOTAL     FAILED\n");
	fprintf(cmd.ofp, "============================================"
		"==============\n");

	if (a = (alcdat *)((unsigned)strstp + FIELD("strstat", "stream"))) {
		fprintf(cmd.ofp, "streams                %4d     %4d   %8d    %7d\n",
			a->use, a->max, a->total, a->fail);
	}

	if (a = (alcdat *)((unsigned)strstp + FIELD("strstat", "queue"))) {
		fprintf(cmd.ofp, "queues                 %4d     %4d   %8d    %7d\n",
			a->use, a->max, a->total, a->fail);
	}

	if (a = (alcdat *)((unsigned)strstp + FIELD("strstat", "msgblock"))) {
		fprintf(cmd.ofp, "message blocks         %4d     %4d   %8d    %7d\n",
			a->use, a->max, a->total, a->fail);
	}

	if (a = (alcdat *)((unsigned)strstp + FIELD("strstat", "mdbblock"))) {
		fprintf(cmd.ofp, "data blocks            %4d     %4d   %8d    %7d\n",
			a->use, a->max, a->total, a->fail);
	}

	if (a = (alcdat *)((unsigned)strstp + FIELD("strstat", "linkblk"))) {
		fprintf(cmd.ofp, "link blocks            %4d     %4d   %8d    %7d\n",
			a->use, a->max, a->total, a->fail);
	}

	if (a = (alcdat *)((unsigned)strstp + FIELD("strstat", "strevent"))) {
		fprintf(cmd.ofp, "strevents              %4d     %4d   %8d    %7d\n",
			a->use, a->max, a->total, a->fail);
	}

	fprintf(cmd.ofp, "\n");
	print_strdzone(cmd.flags, cmd.ofp);
	return(0);
}

#define _STRSTAT_USAGE "[-f] [-w outfile]"

/*
 * strstat_usage() -- Print the usage string for the 'strstat' command.
 */
void
strstat_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STRSTAT_USAGE);
}

/*
 * strstat_help() -- Print the help information for the 'strstat' command.
 */
void
strstat_help(command_t cmd)
{
	CMD_HELP(cmd, _STRSTAT_USAGE,
		"Display information about streams related resources (streams, "
		"queues, message blocks, data blocks, and zones).");
}

/*
 * strstat_parse() -- Parse the command line arguments for 'strstat'.
 */
int
strstat_parse(command_t cmd)
{
	return (C_FALSE|C_FULL|C_WRITE);
}
