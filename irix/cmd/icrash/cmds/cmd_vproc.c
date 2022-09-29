#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_vproc.c,v 1.5 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * vproc_cmd() -- Display information from the vproc struct.
 */
int
vproc_cmd(command_t cmd)
{
	int i, vproc_cnt = 0, first_time = TRUE;
	kaddr_t value;
	k_ptr_t vp;

	vp = kl_alloc_block(VPROC_SIZE, K_TEMP);
	vproc_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_VPROC;
			kl_print_error();
			continue;
		}
		kl_get_struct(value, VPROC_SIZE, vp, "vproc");
		if (KL_ERROR) {
			kl_print_error();
		}
		else {
			if (first_time) {
				first_time = FALSE;
			}
			else if (cmd.flags & (C_FULL|C_NEXT)) {
				vproc_banner(cmd.ofp, BANNER|SMAJOR);
			}
			print_vproc(value, vp, cmd.flags, cmd.ofp);
			vproc_cnt++;
		}

		if ((DEBUG(DC_GLOBAL, 1) || vp) && (cmd.flags & C_FULL)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	vproc_banner(cmd.ofp, SMAJOR);
	PLURAL("vproc struct", vproc_cnt, cmd.ofp);
	kl_free_block(vp);
	return(0);
}

#define _VPROC_USAGE "[-a] [-f] [-n] [-w outfile] vproc_list"

/*
 * vproc_usage() -- Print the usage string for the 'vproc' command.
 */
void
vproc_usage(command_t cmd)
{
	CMD_USAGE(cmd, _VPROC_USAGE);
}

/*
 * vproc_help() -- Print the help information for the 'vproc' command.
 */
void
vproc_help(command_t cmd)
{
	CMD_HELP(cmd, _VPROC_USAGE,
		"Display the vproc structure for each virtual address included "
		"in vproc_list.");
}

/*
 * vproc_parse() -- Parse the command line arguments for 'vproc'.
 */
int
vproc_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_ALL);
}
