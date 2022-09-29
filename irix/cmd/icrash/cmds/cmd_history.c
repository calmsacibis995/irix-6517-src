#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_history.c,v 1.6 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <readline/history.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * history_cmd() -- Dump out the history data.
 */
int
history_cmd(command_t cmd)
{
	int i;
	register HIST_ENTRY **the_list = history_list();

	if (the_list) {
		for (i = 0; the_list[i]; i++) {
			fprintf(cmd.ofp,
				"%3d: %s\n", i + history_base, the_list[i]->line);
		}
	}
	return (0);
}

#define _HISTORY_USAGE "[-w outfile]"

/*
 * history_usage() -- Print the usage string for the 'history' command.
 */
void
history_usage(command_t cmd)
{
	CMD_USAGE(cmd, _HISTORY_USAGE);
}

/*
 * history_help() -- Print the help information for the 'history' command.
 */
void
history_help(command_t cmd)
{
	CMD_HELP(cmd, _HISTORY_USAGE,
		"Dump out the last 20 history commands.  You can also use '!' to "
		"access the old commands (including !!, !-N, etc.)");
}

/*
 * history_parse() -- Parse the command line arguments for 'history'.
 */
int
history_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE);
}
