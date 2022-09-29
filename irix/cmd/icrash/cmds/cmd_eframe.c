#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_eframe.c,v 1.14 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

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
	efp = kl_alloc_block(EFRAME_S_SIZE, K_TEMP);
	for (i = 0; i < cmd.nargs; i++) {

		GET_VALUE(cmd.args[i], &eframe);
		if (KL_ERROR) {
			kl_free_block(efp);
			KL_ERROR |= KLE_BAD_EFRAME_S;
			kl_print_error();
			eframe_s_banner(cmd.ofp, SMAJOR);
			return(1);
		}

		kl_get_struct(eframe, EFRAME_S_SIZE, efp, "eframe_s");
		if (KL_ERROR) {
			kl_free_block(efp);
			KL_ERROR |= KLE_BAD_EFRAME_S;
			kl_print_error();
			eframe_s_banner(cmd.ofp, SMAJOR);
			return(1);
		}

		fprintf(cmd.ofp, "EFRAME AT 0x%llx:\n\n", eframe);
		print_eframe_s((kaddr_t)NULL, efp, cmd.flags, cmd.ofp);
		eframe_cnt++;
	}
	eframe_s_banner(cmd.ofp, SMAJOR);
	PLURAL("eframe", eframe_cnt, cmd.ofp);
	kl_free_block(efp);
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
