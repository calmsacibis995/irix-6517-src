#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_vnode.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * vnode_cmd() -- Display vnode information on some vnode adddress.
 */
int
vnode_cmd(command_t cmd)
{
	int i, vnode_cnt = 0, first_time = TRUE;
	kaddr_t value;
	k_ptr_t vp, vbuf;

	vbuf = alloc_block(VNODE_SIZE(K), B_TEMP);
	vnode_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_VNODE;
			kl_print_error(K);
			continue;
		}
		vp = get_vnode(value, vbuf, (cmd.flags|C_ALL));
		if (KL_ERROR) {
			kl_print_error(K);
		}
		else {
			if (first_time) {
				first_time = FALSE;
			}
			else if (cmd.flags & (C_FULL|C_NEXT)) {
				vnode_banner(cmd.ofp, BANNER|SMAJOR);
			}
			print_vnode(value, vp, cmd.flags, cmd.ofp);
			vnode_cnt++;
		}

		if ((DEBUG(DC_GLOBAL, 1) || vp) && (cmd.flags & C_FULL)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	vnode_banner(cmd.ofp, SMAJOR);
	PLURAL("vnode struct", vnode_cnt, cmd.ofp);
	free_block(vbuf);
	return(0);
}

#define _VNODE_USAGE "[-a] [-f] [-n] [-w outfile] vnode_list"

/*
 * vnode_usage() -- Print the usage string for the 'vnode' command.
 */
void
vnode_usage(command_t cmd)
{
	CMD_USAGE(cmd, _VNODE_USAGE);
}

/*
 * vnode_help() -- Print the help information for the 'vnode' command.
 */
void
vnode_help(command_t cmd)
{
	CMD_HELP(cmd, _VNODE_USAGE,
		"Display the vnode structure for each virtual address included "
		"in vnode_list.");
}

/*
 * vnode_parse() -- Parse the command line arguments for 'vnode'.
 */
int
vnode_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_ALL);
}
