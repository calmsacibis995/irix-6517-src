#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_vsocket.c,v 1.5 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * vsocket_cmd() -- Display information from the vsocket struct.
 */
int
vsocket_cmd(command_t cmd)
{
	int i, vsocket_cnt = 0, first_time = TRUE;
	kaddr_t value;
	k_ptr_t vp;

	vp = kl_alloc_block(VSOCKET_SIZE, K_TEMP);
	vsocket_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_VSOCKET;
			kl_print_error();
			continue;
		}
		kl_get_struct(value, VSOCKET_SIZE, vp, "vsocket");
		if (KL_ERROR) {
			kl_print_error();
		}
		else {
			if (first_time) {
				first_time = FALSE;
			}
			else if (cmd.flags & (C_FULL|C_NEXT)) {
				vsocket_banner(cmd.ofp, BANNER|SMAJOR);
			}
			print_vsocket(value, vp, cmd.flags, cmd.ofp);
			vsocket_cnt++;
		}

		if ((DEBUG(DC_GLOBAL, 1) || vp) && (cmd.flags & C_FULL)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	vsocket_banner(cmd.ofp, SMAJOR);
	PLURAL("vsocket struct", vsocket_cnt, cmd.ofp);
	kl_free_block(vp);
	return(0);
}

#define _VSOCKET_USAGE "[-a] [-f] [-n] [-w outfile] vsocket_list"

/*
 * vsocket_usage() -- Print the usage string for the 'vsocket' command.
 */
void
vsocket_usage(command_t cmd)
{
	CMD_USAGE(cmd, _VSOCKET_USAGE);
}

/*
 * vsocket_help() -- Print the help information for the 'vsocket' command.
 */
void
vsocket_help(command_t cmd)
{
	CMD_HELP(cmd, _VSOCKET_USAGE,
		"Display the vsocket structure for each virtual address included "
		"in vsocket_list.");
}

/*
 * vsocket_parse() -- Parse the command line arguments for 'vsocket'.
 */
int
vsocket_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_ALL);
}
