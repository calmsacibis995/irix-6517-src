#ident "$Header: "

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * lsnode_cmd() -- Dump out the special node information.
 */
int
lsnode_cmd(command_t cmd)
{
	int i, lsnode_cnt = 0, first_time = TRUE;
	kaddr_t value;
	k_ptr_t sp, sbuf;

	sbuf = alloc_block(STRUCT("lsnode"), B_TEMP);
	lsnode_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		if (cmd.flags & C_FULL) {
			if (first_time) {
				first_time = FALSE;
			}
			else {
				lsnode_banner(cmd.ofp, BANNER|SMAJOR);
			}
		}
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_LSNODE;
			kl_print_error(K);
		}
		else {
			sp = get_lsnode(value, sbuf, (cmd.flags|C_ALL));
			if (KL_ERROR) {
				kl_print_error(K);
			}
			else {
				print_lsnode(value, sp, cmd.flags, cmd.ofp);
				lsnode_cnt++;
			}
		}
		if ((DEBUG(DC_GLOBAL, 1) || sp) && (cmd.flags & C_FULL)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	lsnode_banner(cmd.ofp, SMAJOR);
	PLURAL("lsnode struct", lsnode_cnt, cmd.ofp);
	free_block(sbuf);
	return(0);
}

#define _LSNODE_USAGE "[-f] [-w outfile] lsnode_list"

/*
 * lsnode_usage() -- Print the usage string for the 'lsnode' command.
 */
void
lsnode_usage(command_t cmd)
{
	CMD_USAGE(cmd, _LSNODE_USAGE);
}

/*
 * lsnode_help() -- Print the help information for the 'lsnode' command.
 */
void
lsnode_help(command_t cmd)
{
	CMD_HELP(cmd, _LSNODE_USAGE,
		"Display the lsnode structure located at each virtual address "
		"included in lsnode_list.");
}

/*
 * lsnode_parse() -- Parse the command line arguments for 'lsnode'.
 */
int
lsnode_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
