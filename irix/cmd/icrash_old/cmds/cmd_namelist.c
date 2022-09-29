#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_namelist.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <nlist.h>
#include <stdio.h>
#include <errno.h>
#include <filehdr.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/*
 * namelist_cmd()
 */
int
namelist_cmd(command_t cmd)
{
	int i;

	if (cmd.nargs == 0) {
		fprintf(cmd.ofp, "INDEX  NAMELIST\n");
		fprintf(cmd.ofp, "=================================================\n");
		for (i = 0; i < numnmlist; i++) {
			fprintf(cmd.ofp, "%5d  %s\n", i, nmlist[i].namelist); 
		}
		fprintf(cmd.ofp, "=================================================\n");
		fprintf(cmd.ofp, "\n");
	}
	else {
		set_curnmlist(atoi(cmd.args[0]));
	}
	fprintf(cmd.ofp, "The current namelist is %s (%d)\n", 
		nmlist[curnmlist].namelist, curnmlist);
	return(0);
}

#define _NAMELIST_USAGE "<index_number>"

/*
 * namelist_usage() -- Print the usage string for the 'namelist' command.
 */
void
namelist_usage(command_t cmd)
{
	CMD_USAGE(cmd, _NAMELIST_USAGE);
}

/*
 * namelist_help() -- Print the help information for the 'namelist' command.
 */
void
namelist_help(command_t cmd)
{
	CMD_HELP(cmd, _NAMELIST_USAGE,
		"Set/display the current namelist. If index_number is not specified, "
		"display all currently opened namelists.");
}

/*
 * namelist_parse() -- Parse the command line arguments for 'namelist'.
 */
int
namelist_parse(command_t cmd)
{
	return (C_MAYBE);
}
