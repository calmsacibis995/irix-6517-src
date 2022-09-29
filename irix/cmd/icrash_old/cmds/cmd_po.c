#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_po.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

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
 * printo_cmd() -- Run the 'po' command via print_cmd().
 */
int
printo_cmd(command_t cmd)
{
	cmd.flags |= C_OCTAL;
	print_cmd(cmd);
	return(0);
}

#define _PRINTO_USAGE "[-w outfile] expression"

/*
 * printo_usage() -- Print the usage string for the 'printo' command.
 */
void
printo_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PRINTO_USAGE);
}

/*
 * printo_help() -- Print the help information for the 'printo' command.
 */
void
printo_help(command_t cmd)
{
	CMD_HELP(cmd, _PRINTO_USAGE,
		"The printo command is the same as the print command except that "
		"all integers are printed as hexadecimal values.");
}

/*
 * printo_parse() -- Parse the command line arguments for 'printo'.
 */
int
printo_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
