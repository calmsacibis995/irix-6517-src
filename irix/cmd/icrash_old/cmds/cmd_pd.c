#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_pd.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>

#include "icrash.h"
#include "extern.h"
#include "eval.h"

/*
 * printd_cmd() -- Run 'pd' in the print routine.
 */
int
printd_cmd(command_t cmd)
{
	cmd.flags |= C_DECIMAL;
	print_cmd(cmd);
	return(0);
}

#define _PRINTD_USAGE "[-w outfile] expression"

/*
 * printd_usage() -- Print the usage string for the 'printd' command.
 */
void
printd_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PRINTD_USAGE);
}

/*
 * printd_help() -- Print the help information for the 'printd' command.
 */
void
printd_help(command_t cmd)
{
	CMD_HELP(cmd, _PRINTD_USAGE,
		"The printd command is the same as the print command except that "
		"all integers are printed as decimal values.");
}

/*
 * printd_parse() -- Parse the command line arguments for 'printd'.
 */
int
printd_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
