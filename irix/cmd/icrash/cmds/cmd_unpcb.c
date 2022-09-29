#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_unpcb.c,v 1.15 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * unpcb_cmd() -- Dump out unpcb structure data, based on socket address.
 */
int
unpcb_cmd(command_t cmd)
{
	int i, un_cnt = 0;
	kaddr_t un;
	k_ptr_t unp;

	unp = kl_alloc_block(UNPCB_SIZE, K_TEMP);
	unpcb_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &un);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_UNPCB;
			kl_print_error();
			continue;
		}
		kl_get_struct(un, UNPCB_SIZE, unp, "unpcb");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_UNPCB;
			kl_print_error();
		}
		else {
			print_unpcb(un, unp, cmd.flags, cmd.ofp);
			un_cnt++;
		}
	}
	unpcb_banner(cmd.ofp, SMAJOR);
	PLURAL("unpcb struct", un_cnt, cmd.ofp);
	kl_free_block(unp);
	return(0);
}

#define _UNPCB_USAGE "[-f] [-w outfile] unpcb_list"

/*
 * unpcb_usage() -- Print the usage string for the 'unpcb' command.
 */
void
unpcb_usage(command_t cmd)
{
	CMD_USAGE(cmd, _UNPCB_USAGE);
}

/*
 * unpcb_help() -- Print the help information for the 'unpcb' command.
 */
void
unpcb_help(command_t cmd)
{
	CMD_HELP(cmd, _UNPCB_USAGE,
		"Display the unpcb structure located at each virtual address included "
		"in unpcb_list.");
}

/*
 * unpcb_parse() -- Parse the command line arguments for 'unpcb'.
 */
int
unpcb_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
