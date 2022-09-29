#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_quit.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * quit_cmd() -- Run the 'quit' command.
 */
int
quit_cmd(command_t cmd)
{
	char str[256];

	if (!strcmp(cmd.com, "q")) {
		fprintf(cmd.ofp, "Do you really want to quit (y to quit) ? ");
		gets(str);
		if (str[0] == 'y' || str[0] == 'Y') {
			exit(0);
		}
	}
	else if (!strcmp(cmd.com, "quit") || !strcmp(cmd.com, "q!")) {
		exit(0);
	}
	return 1;
}

#define _QUIT_USAGE ""

/*
 * quit_usage() -- Print the usage string for the 'quit' command.
 */
void
quit_usage(command_t cmd)
{
	CMD_USAGE(cmd, _QUIT_USAGE);
}

/*
 * quit_help() -- Print the help information for the 'quit' command.
 */
void
quit_help(command_t cmd)
{
	CMD_HELP(cmd, _QUIT_USAGE,
		"Exit icrash.  Note that q will prompt for confirmation unless a "
		"'!' is appended to the command line.");
}

/*
 * quit_parse() -- Parse the command line arguments for 'quit'.
 */
int
quit_parse(command_t cmd)
{
	return (C_FALSE);
}
