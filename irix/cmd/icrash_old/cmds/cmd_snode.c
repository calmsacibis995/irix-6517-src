#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_snode.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * snode_cmd() -- Dump out the special node information.
 */
int
snode_cmd(command_t cmd)
{
	int i, snode_cnt = 0, first_time = TRUE;
	kaddr_t value;
	k_ptr_t sp, sbuf;

	sbuf = alloc_block(STRUCT("snode"), B_TEMP);
	snode_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		if (cmd.flags & C_FULL) {
			if (first_time) {
				first_time = FALSE;
			}
			else {
				snode_banner(cmd.ofp, BANNER|SMAJOR);
			}
		}
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_SNODE;
			kl_print_error(K);
		}
		else {
			sp = get_snode(value, sbuf, (cmd.flags|C_ALL));
			if (KL_ERROR) {
				kl_print_error(K);
			}
			else {
				print_snode(value, sp, cmd.flags, cmd.ofp);
				snode_cnt++;
			}
		}
		if ((DEBUG(DC_GLOBAL, 1) || sp) && (cmd.flags & C_FULL)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	snode_banner(cmd.ofp, SMAJOR);
	PLURAL("snode struct", snode_cnt, cmd.ofp);
	free_block(sbuf);
	return(0);
}

#define _SNODE_USAGE "[-f] [-w outfile] snode_list"

/*
 * snode_usage() -- Print the usage string for the 'snode' command.
 */
void
snode_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SNODE_USAGE);
}

/*
 * snode_help() -- Print the help information for the 'snode' command.
 */
void
snode_help(command_t cmd)
{
	CMD_HELP(cmd, _SNODE_USAGE,
		"Display the snode structure located at each virtual address "
		"included in snode_list.");
}

/*
 * snode_parse() -- Parse the command line arguments for 'snode'.
 */
int
snode_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
