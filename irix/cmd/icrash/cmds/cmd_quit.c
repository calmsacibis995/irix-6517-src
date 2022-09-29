#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_quit.c,v 1.8 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

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
			kl_free_klib(KLP);
			exit(0);
		}
	}
	else if (!strcmp(cmd.com, "quit") || !strcmp(cmd.com, "q!")) {
		kl_free_klib(KLP);
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
