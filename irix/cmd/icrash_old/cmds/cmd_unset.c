#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_unset.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <stdio.h>
#include <sys/types.h>
#include "icrash.h"
#include "eval.h"
#include "variable.h"
#include "extern.h"

/*
 * unset_cmd() -- unset eval variables
 */
int
unset_cmd(command_t cmd)
{
	variable_t *vp;

	if (cmd.nargs != 1) {
		unset_usage(cmd);
		return(1);
	}
	if (!(vp = find_variable(vtab, cmd.args[0], 0))) {
		fprintf(KL_ERRORFP, "%s cannot be unset\n", cmd.args[0]);
		return(1);
	}
	unset_variable(vtab, vp);
	return(0);
}

#define _UNSET_USAGE "[-w outfile] var_name"

/*
 * unset_usage() -- Print the usage string for the 'unset' command.
 */
void
unset_usage(command_t cmd)
{
	CMD_USAGE(cmd, _UNSET_USAGE);
}

/*
 * unset_help() -- Print the help information for the 'unset' command.
 */
void
unset_help(command_t cmd)
{
	CMD_HELP(cmd, _UNSET_USAGE,
		"Help for the unset command");
}

/*
 * unset_parse() -- Parse the command line arguments for 'unset'.
 */
int
unset_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
