#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_addtype.c,v 1.4 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * addtype_cmd()
 */
int
addtype_cmd(command_t cmd)
{
	if (kl_init_nmlist(cmd.args[0])) {
		fprintf(KL_ERRORFP, "ERROR: Could not open namelist %s\n", cmd.args[0]);
		return(1);
	}
	kl_init_types(STP);
	return(0);
}

#define _ADDTYPE_USAGE "namelist <symbol_list>"

/*
 * addtype_usage() -- Add kernel type definitions from another namelist
 */
void
addtype_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ADDTYPE_USAGE);
}

/*
 * addtype_help() -- Print the help information for the 'addtype' command.
 */
void
addtype_help(command_t cmd)
{
	CMD_HELP(cmd, _ADDTYPE_USAGE,
		"Add kernel type definitions from another namelist.");
}

/*
 * addtype_parse() -- Parse the command line arguments for 'addtype'.
 */
int
addtype_parse(command_t cmd)
{
	return (C_TRUE);
}
