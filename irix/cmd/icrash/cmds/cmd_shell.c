#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_shell.c,v 1.12 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * shell_cmd() -- Perform some shell command.
 */
int
shell_cmd(command_t cmd)
{
	int i;
	char cmdstr[300];
	char *shellptr;

	if ((shellptr = getenv("SHELL")) == (char *)NULL) {
		fprintf(cmd.ofp, "SHELL environment variable not set\n");
		return(1);
	} 
	else {
		sprintf(cmdstr, "%s ", shellptr);
		for (i = 0; i < cmd.nargs; i++) {
			strcat(cmdstr, cmd.args[i]);
			strcat(cmdstr, " ");
		}
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(cmd.ofp, "shell_cmd: %s\n", cmdstr);
		}
		system(cmdstr);
	}
	return(0);
}

#define _SHELL_USAGE "[command]"

/*
 * shell_usage() -- Print the usage string for the 'shell' command.
 */
void
shell_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SHELL_USAGE);
}

/*
 * shell_help() -- Print the help information for the 'shell' command.
 */
void
shell_help(command_t cmd)
{
	CMD_HELP(cmd, _SHELL_USAGE,
		"Escapes to the shell with no arguments, or will execute the "
		"command entered.  Note that it will use your SHELL environment "
		"variable to determine which shell to use.");
}

/*
 * shell_parse() -- Parse the command line arguments for 'shell'.
 */
int
shell_parse(command_t cmd)
{
	return (C_MAYBE);
}
