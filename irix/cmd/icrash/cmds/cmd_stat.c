#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_stat.c,v 1.12 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * stat_cmd() -- Print out the putbuf / system information.
 */
int
stat_cmd(command_t cmd)
{
	int lbolt;

	print_utsname(cmd.ofp);
	fprintf(cmd.ofp, "    time of crash:  ");
	print_curtime(cmd.ofp);

	kl_get_block(K_LBOLT, 4, &lbolt, "lbolt");
	fprintf(cmd.ofp, "    system age:     %d day, %d hour, %d min.\n",
		(((lbolt/100)/60)/60)/24,
		(((lbolt/100)/60)/60)%24,
		((lbolt/100)/60)%60);

	fprintf(cmd.ofp, "    putbuf:\n");
	print_putbuf(cmd.ofp);

	/* dump the error dumpbuf (IP19 systems only) 
	 */
	if (K_ERROR_DUMPBUF) {
		fprintf(cmd.ofp, "\n\nERROR DUMPBUF:\n\n");
		fprintf(cmd.ofp, "0x%llx = \"", K_ERROR_DUMPBUF);
		dump_string(K_ERROR_DUMPBUF, cmd.flags, cmd.ofp);
		fprintf(cmd.ofp, "\"\n");
	}
	fprintf(cmd.ofp, "\n");
	return(0);
}

#define _STAT_USAGE "[-w outfile]"

/*
 * stat_usage() -- Print the usage string for the 'stat' command.
 */
void
stat_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STAT_USAGE);
}

/*
 * stat_help() -- Print the help information for the 'stat' command.
 */
void
stat_help(command_t cmd)
{
	CMD_HELP(cmd, _STAT_USAGE,
		"Display system statistics and the putbuf array, which contains "
		"the latest messages printed via the kernel printf/cmn_err "
		"routines.");
}

/*
 * stat_parse() -- Parse the command line arguments for 'stat'.
 */
int
stat_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE);
}
