#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_anontree.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * anon_cmd() -- Display anon information on some anon adddress.
 */
int
anontree_cmd(command_t cmd)
{
	int i, anon_cnt = 0, first_time = TRUE;
	kaddr_t KPanon0;

	if(cmd.flags & (C_FULL|C_ALL|C_NEXT))
	{
		anon_banner(cmd.ofp, SMAJOR);
	}
	else
	{
		anon_banner(cmd.ofp, BANNER|SMAJOR);
	}
	for (i = 0; i < cmd.nargs; i++) 
	{
		GET_VALUE(cmd.args[i], &KPanon0);
		if (KL_ERROR) 
		{
			KL_ERROR |= KLE_BAD_ANON;
			kl_print_error(K);
			continue;
		}
		if (KL_ERROR) 
		{
			kl_print_error(K);
		}
		else 
		{
			if (first_time) 
			{
				first_time = FALSE;
			}
			else if (cmd.flags & (C_FULL|C_NEXT)) 
			{
				anon_banner(cmd.ofp, SMAJOR);
				PLURAL("anon struct", anon_cnt, cmd.ofp);
				anon_cnt=0;
			}
			print_anontree(find_anonroot(KPanon0), 
				       &anon_cnt, cmd.flags, cmd.ofp);
		}

		if (DEBUG(DC_GLOBAL, 1) && (cmd.flags & C_FULL)) 
		{
			fprintf(cmd.ofp, "\n");
		}
	}
	anon_banner(cmd.ofp, SMAJOR);
	PLURAL("anon struct", anon_cnt, cmd.ofp);

	return(0);
}

#define _ANONTREE_USAGE "[-a] [-f] [-n] [-w outfile] anon_list"

/*
 * anontree_usage() -- Print the usage string for the 'anon' command.
 */
void
anontree_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ANONTREE_USAGE);
}

/*
 * anontree_help() -- Print the help information for the 'anontree' command.
 */
void
anontree_help(command_t cmd)
{
	CMD_HELP(cmd, _ANONTREE_USAGE,
		"Display the anon tree structure for each virtual address "
		 "included in anon_list.");
}

/*
 * anontree_parse() -- Parse the command line arguments for 'anon'.
 */
int
anontree_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_ALL);
}
