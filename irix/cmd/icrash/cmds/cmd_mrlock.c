#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_mrlock.c,v 1.3 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * mrlock_cmd() -- Dump information about a multi-reader lock
 */
int
mrlock_cmd(command_t cmd)
{
	int i, mrlock_cnt = 0, first_time = 1;
	kaddr_t mrlock;
	k_ptr_t mrlockp;

	mrlockp = kl_alloc_block(MRLOCK_S_SIZE, K_TEMP);
	if (!(cmd.flags & C_FULL)) {
		mrlock_s_banner(cmd.ofp, BANNER|SMAJOR);
	}
	for (i = 0; i < cmd.nargs; i++) {
		if (cmd.flags & C_FULL) {
			mrlock_s_banner(cmd.ofp, BANNER|SMAJOR);
		}
		GET_VALUE(cmd.args[i], &mrlock);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_MRLOCK_S;
			kl_print_error();
		}
		else {
			kl_get_struct(mrlock, MRLOCK_S_SIZE, mrlockp, "mrlock_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_MRLOCK_S;
				kl_print_error();
			}
			else {
				print_mrlock_s(mrlock, mrlockp, cmd.flags, cmd.ofp);
				mrlock_cnt++;
			}
		}
		if (cmd.flags & C_FULL) {
			fprintf(cmd.ofp, "\n");
		}
	}
	kl_free_block(mrlockp);
	mrlock_s_banner(cmd.ofp, SMAJOR);
	PLURAL("mrlock_s struct", mrlock_cnt, cmd.ofp);
	return(0);
}

#define _MRLOCK_USAGE "[-f] [-n] [-w outfile] mrlock_list"

/*
 * mrlock_usage() -- Print the usage string for the 'mrlock' command.
 */
void
mrlock_usage(command_t cmd)
{
	CMD_USAGE(cmd, _MRLOCK_USAGE);
}

/*
 * mrlock_help() -- Print the help information for the 'mrlock' command.
 */
void
mrlock_help(command_t cmd)
{
	CMD_HELP(cmd, _MRLOCK_USAGE,
		"Display the mrlock_s structure located at each virtual address "
		"included in mrlock_list. If the -f command line option is specified, "
		"display all queues of WAITERS and HOLDERS. If the -n command line "
		"option is specified (in conjunction with the -f option), then walk "
		"each queue displaying information on associated kthreads.");
}

/*
 * mrlock_parse() -- Parse the command line arguments for 'mrlock'.
 */
int
mrlock_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT);
}
