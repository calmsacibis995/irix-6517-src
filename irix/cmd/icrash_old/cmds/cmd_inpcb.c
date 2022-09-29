#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_inpcb.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * inpcb_cmd() -- Dump out inpcb structure data, based on socket address.
 */
int
inpcb_cmd(command_t cmd)
{
	int i, in_cnt = 0;
	kaddr_t in;
	k_ptr_t inp;

	inp = alloc_block(INPCB_SIZE(K), B_TEMP);
	inpcb_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {

		GET_VALUE(cmd.args[i], &in);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_INPCB;
			kl_print_error(K);
			continue;
		}
		kl_get_struct(K, in, INPCB_SIZE(K), inp, "inpcb");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_INPCB;
			kl_print_error(K);
		}
		else {
			print_inpcb(in, inp, cmd.flags, cmd.ofp);
			in_cnt++;
		}
	}
	inpcb_banner(cmd.ofp, SMAJOR);
	PLURAL("inpcb struct", in_cnt, cmd.ofp);
	free_block(inp);
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
