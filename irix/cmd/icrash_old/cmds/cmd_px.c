#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_px.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

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
 * printx_cmd() -- Run the 'printx' command.
 */
int
printx_cmd(command_t cmd)
{
	cmd.flags |= C_HEX;
	print_cmd(cmd);
	return(0);
}

#define _PRINTX_USAGE "[-w outfile] expression"

/*
 * printx_usage() -- Print the usage string for the 'printx' command.
 */
void
printx_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PRINTX_USAGE);
}

/*
 * printx_help() -- Print the help information for the 'printx' command.
 */
void
printx_help(command_t cmd)
{
	CMD_HELP(cmd, _PRINTX_USAGE,
		"The printx command is the same as the print command except that "
		"all integers are printed as hexadecimal values.");
}

/*
 * printx_parse() -- Parse the command line arguments for 'printx'.
 */
int
printx_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
