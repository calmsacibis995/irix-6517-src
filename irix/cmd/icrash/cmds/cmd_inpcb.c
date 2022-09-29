#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_inpcb.c,v 1.14 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * inpcb_cmd() -- Dump out inpcb structure data, based on socket address.
 */
int
inpcb_cmd(command_t cmd)
{
	int i, in_cnt = 0;
	kaddr_t in;
	k_ptr_t inp;

	inp = kl_alloc_block(INPCB_SIZE, K_TEMP);
	inpcb_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {

		GET_VALUE(cmd.args[i], &in);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_INPCB;
			kl_print_error();
			continue;
		}
		kl_get_struct(in, INPCB_SIZE, inp, "inpcb");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_INPCB;
			kl_print_error();
			continue;
		}
		else {
			print_inpcb(in, inp, cmd.flags, cmd.ofp);
			in_cnt++;
		}
	}
	inpcb_banner(cmd.ofp, SMAJOR);
	PLURAL("inpcb struct", in_cnt, cmd.ofp);
	kl_free_block(inp);
	return(0);
}

#define _INPCB_USAGE "[-f] [-w outfile] inpcb_list"

/*
 * inpcb_usage() -- Print the usage string for the 'inpcb' command.
 */
void
inpcb_usage(command_t cmd)
{
	CMD_USAGE(cmd, _INPCB_USAGE);
}

/*
 * inpcb_help() -- Print the help information for the 'inpcb' command.
 */
void
inpcb_help(command_t cmd)
{
	CMD_HELP(cmd, _INPCB_USAGE,
		"Display the inpcb structure located at each virtual address "
		"included in inpcb_list.");
}

/*
 * inpcb_parse() -- Parse the command line arguments for 'inpcb'.
 */
int
inpcb_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
