#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_pager.c,v 1.10 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * pager_cmd() -- Turns ON/OFF the paging of command output
 */
int
pager_cmd(command_t cmd)
{
	int d;

	if (cmd.nargs == 0) {
		if (pager_flag) {
			fprintf(cmd.ofp, "Pager flag is currently set.\n");
		}
		else {
			fprintf(cmd.ofp, "Pager flag is not set.\n");
		}
		return(1);
	}
	if (!strncasecmp(cmd.args[0], "on", 2)) {
		pager_flag = 1;
		fprintf(cmd.ofp, "Pager flag is now set.\n");
	} 
	else if (!strncasecmp(cmd.args[0], "off", 3)) {
		pager_flag = 0;
		fprintf(cmd.ofp, "Pager flag is now clear.\n");
	}
	return(0);
}

#define _PAGER_USAGE "[-w outfile] [ on | off ]"

/*
 * pager_usage() -- Print the usage string for the 'pager' command.
 */
void
pager_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PAGER_USAGE);
}

/*
 * pager_help() -- Print the help information for the 'pager' command.
 */
void
pager_help(command_t cmd)
{
	CMD_HELP(cmd, _PAGER_USAGE,
		"When pager is on, all command output is piped through pg. When "
		"pager is off, all output is printed continiously without page "
		"breaks. When the pager command is issued without a command line "
		"option, the current state of the pager is displayed (\"on\" or "
		"\"off\"). Note that when the pager is on, command output cannot "
		"be redirected to a pipe.");
}

/*
 * pager_parse() -- Parse the command line arguments for 'pager'.
 */
int
pager_parse(command_t cmd)
{
	return (C_MAYBE);
}
