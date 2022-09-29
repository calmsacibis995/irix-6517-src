#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_rnode.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * rnode_cmd() -- Dump out data related to an rnode.
 */
int
rnode_cmd(command_t cmd)
{
	int first_time = TRUE;
	int i, rnode_cnt=0;
	kaddr_t value;
	k_ptr_t rp, rbuf;

	if (!(cmd.flags & C_FULL)) {
		rnode_banner(cmd.ofp, BANNER|SMAJOR);
	}
	rbuf = alloc_block(STRUCT("rnode"), B_TEMP);
	for (i = 0; i < cmd.nargs; i++) {
		if (cmd.flags & C_FULL) {
			rnode_banner(cmd.ofp, BANNER|SMAJOR);
		}

		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_RNODE;
			kl_print_error(K);
			continue;
		}
		rp = get_rnode(value, rbuf, (cmd.flags|C_ALL));
		if (KL_ERROR) {
			kl_print_error(K);
		}
		else {
			if (first_time) {
                                first_time = FALSE;
                        }
			else if (cmd.flags & (C_FULL|C_NEXT)) {
				rnode_banner(cmd.ofp, BANNER|SMAJOR);
			}
			rnode_cnt = print_rnode(value, rp, cmd.flags, cmd.ofp);
		}
		if (cmd.flags & C_FULL) {
			fprintf(cmd.ofp, "\n");
		}
	}
	rnode_banner(cmd.ofp, SMAJOR);
	PLURAL("rnode struct", rnode_cnt, cmd.ofp);
	free_block(rbuf);
	return(0);
}

#define _RNODE_USAGE "[-f] [-n] [-w outfile] rnode_list"

/*
 * rnode_usage() -- Print the usage string for the 'rnode' command.
 */
void
rnode_usage(command_t cmd)
{
	CMD_USAGE(cmd, _RNODE_USAGE);
}

/*
 * rnode_help() -- Print the help information for the 'rnode' command.
 */
void
rnode_help(command_t cmd)
{
	CMD_HELP(cmd, _RNODE_USAGE,
		"Display the rnode structure for each virtual address included "
		"in rnode_list.  The rnode contains information about a virtual "
		"node that acts as an interface between two NFS systems.");
}

/*
 * rnode_parse() -- Parse the command line arguments for 'rnode'.
 */
int
rnode_parse(command_t cmd)
{
	return (C_WRITE|C_FULL|C_NEXT|C_ALL|C_TRUE);
}
