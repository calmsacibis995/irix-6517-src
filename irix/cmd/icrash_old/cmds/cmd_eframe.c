#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_eframe.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/*
 * eframe_cmd() -- Run the 'eframe' command.
 */
int
eframe_cmd(command_t cmd)
{
	int i, eframe_cnt = 0;
	kaddr_t eframe;
	k_ptr_t efp;

	eframe_s_banner(cmd.ofp, SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {

		GET_VALUE(cmd.args[i], &eframe);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_EFRAME_S;
			kl_print_error(K);
			eframe_s_banner(cmd.ofp, SMAJOR);
			return(1);
		}

		efp = kl_get_struct(K, eframe, EFRAME_S_SIZE(K), 0, "eframe_s");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_EFRAME_S;
			kl_print_error(K);
			eframe_s_banner(cmd.ofp, SMAJOR);
			return(1);
		}

		fprintf(cmd.ofp, "EFRAME AT 0x%llx:\n\n", eframe);
		print_eframe_s((kaddr_t)NULL, efp, cmd.flags, cmd.ofp);
		eframe_cnt++;
	}
	eframe_s_banner(cmd.ofp, SMAJOR);
	PLURAL("eframe", eframe_cnt, cmd.ofp);
	free_block(efp);
	return(0);
}

#define _EFRAME_USAGE "[-w outfile] eframe_addr"

/*
 * eframe_usage() -- Print the usage string for the 'eframe' command.
 */
void
eframe_usage(command_t cmd)
{
	CMD_USAGE(cmd, _EFRAME_USAGE);
}

/*
 * eframe_help() -- Print the help information for the 'eframe' command.
 */
void
eframe_help(command_t cmd)
{
	CMD_HELP(cmd, _EFRAME_USAGE,
		"Display the exception frame (containing a register dump, EPC, "
		"cause register, and status register) located at eframe_addr.");
}

/*
 * eframe_parse() -- Parse the command line arguments for 'eframe'.
 */
int
eframe_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
