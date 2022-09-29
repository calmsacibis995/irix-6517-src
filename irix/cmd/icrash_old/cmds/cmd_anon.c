#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_anon.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * anon_cmd() -- Display anon information on some anon adddress.
 */
int anon_cmd(command_t cmd)
{
	int i, anon_cnt = 0, first_time = TRUE;
	kaddr_t KPanon0,KPanon1;
	k_ptr_t Panon0,Panon1;

	Panon0 = alloc_block(VNODE_SIZE(K), B_TEMP);
	anon_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) 
	{
		GET_VALUE(cmd.args[i], &KPanon0);
		if (KL_ERROR) 
		{
			KL_ERROR |= KLE_BAD_ANON;
			kl_print_error(K);
			continue;
		}
		Panon1 = get_anon(KPanon0, Panon0, (cmd.flags|C_ALL));
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
			else if (cmd.flags & (C_FULL|C_NEXT|C_ALL)) 
			{
				anon_banner(cmd.ofp, SMAJOR);
				anon_banner(cmd.ofp, BANNER|SMAJOR);
			}
			print_anon(KPanon0, Panon0, cmd.flags, cmd.ofp);
			anon_cnt++;
		}

		if ((DEBUG(DC_GLOBAL, 1) || Panon1) && (cmd.flags & C_FULL)) 
		{
			fprintf(cmd.ofp, "\n");
		}
	}
	anon_banner(cmd.ofp, SMAJOR);
	PLURAL("anon struct", anon_cnt, cmd.ofp);
	free_block(Panon0);
	return(0);
}

#define _ANON_USAGE "[-a] [-f] [-n] [-w outfile] anon_list"

/*
 * anon_usage() -- Print the usage string for the 'anon' command.
 */
void anon_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ANON_USAGE);
}

/*
 * anon_help() -- Print the help information for the 'anon' command.
 */
void anon_help(command_t cmd)
{
	CMD_HELP(cmd, _ANON_USAGE,
		 "Display the anon structure for each virtual address included "
		 "in anon_list. The -a option displays pfdat's in the "
		 "pcache in ascending order of their page numbers (pf_pageno). "
		 "The -f option on the other hand displays all the pfdat's in the"
		 " pcache on a \"first-find\" basis. Using both the -a and -f "
		 "options will cause icrash to behave as if only "
		 "the -a option were specified.\n\n"
		 "If the anon command hangs, while printing the pfdat's when "
		 "the -a option is used, that usually means icrash is unable "
		 "to print the pfdat's in the pcache in sorted order. "
		 "Then please ^C out and try the same command with the -f "
		 "option instead of the -a option.");
}

/*
 * anon_parse() -- Parse the command line arguments for 'anon'.
 */
int anon_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_ALL);
}
