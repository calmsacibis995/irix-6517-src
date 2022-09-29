#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_inode.c,v 1.25 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * inode_cmd() -- Dump out inode information.
 */
int
inode_cmd(command_t cmd)
{
	int i, inode_cnt = 0;
	kaddr_t value;
	k_ptr_t ip, ibuf;

	if (!(cmd.flags & C_FULL)) {
		inode_banner(cmd.ofp, BANNER|SMAJOR);
	}
	ibuf = kl_alloc_block(kl_struct_len("inode"), K_TEMP);
	for (i = 0; i < cmd.nargs; i++) {
		if (cmd.flags & C_FULL) {
			inode_banner(cmd.ofp, BANNER|SMAJOR);
		}
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_INODE;
			kl_print_error();
			continue;
		}
		ip = get_inode(value, ibuf, (cmd.flags|C_ALL));
		if (KL_ERROR) {
			kl_print_error();
		}
		else {
			inode_cnt += print_inode(value, ip, cmd.flags, cmd.ofp);
		}

		if ((DEBUG(DC_GLOBAL, 1) || ip) && (cmd.flags & C_FULL)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	inode_banner(cmd.ofp, SMAJOR);
	PLURAL("inode struct", inode_cnt, cmd.ofp);
	kl_free_block(ibuf);
	return(0);
}

#define _INODE_USAGE "[-f] [-w outfile] inode_list"

/*
 * inode_usage() -- Print the usage string for the 'inode' command.
 */
void
inode_usage(command_t cmd)
{
	CMD_USAGE(cmd, _INODE_USAGE);
}

/*
 * inode_help() -- Print the help information for the 'inode' command.
 */
void
inode_help(command_t cmd)
{
	CMD_HELP(cmd, _INODE_USAGE,
		"Display the inode structure located at each virtual address "
		"included in inode_list.");
}

/*
 * inode_parse() -- Parse the command line arguments for 'inode'.
 */
int
inode_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
