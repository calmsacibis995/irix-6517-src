#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_unpcb.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * unpcb_cmd() -- Dump out unpcb structure data, based on socket address.
 */
int
unpcb_cmd(command_t cmd)
{
	int i, un_cnt = 0;
	kaddr_t un;
	k_ptr_t unp;

	unp = alloc_block(UNPCB_SIZE(K), B_TEMP);
	unpcb_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &un);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_UNPCB;
			kl_print_error(K);
			continue;
		}
		kl_get_struct(K, un, UNPCB_SIZE(K), unp, "unpcb");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_UNPCB;
			kl_print_error(K);
		}
		else {
			print_unpcb(un, unp, cmd.flags, cmd.ofp);
			un_cnt++;
		}
	}
	unpcb_banner(cmd.ofp, SMAJOR);
	PLURAL("unpcb struct", un_cnt, cmd.ofp);
	free_block(unp);
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
