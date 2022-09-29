#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_mrlock.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * mrlock_cmd() -- Dump information about a multi-reader lock
 */
int
mrlock_cmd(command_t cmd)
{
	int i, mrlock_cnt = 0, first_time = 1;
	kaddr_t mrlock;
	k_ptr_t mrlockp;

	mrlockp = alloc_block(MRLOCK_S_SIZE(K), B_TEMP);
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
			kl_print_error(K);
		}
		else {
			kl_get_struct(K, mrlock, MRLOCK_S_SIZE(K), mrlockp, "mrlock_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_MRLOCK_S;
				kl_print_error(K);
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
	free_block(mrlockp);
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
