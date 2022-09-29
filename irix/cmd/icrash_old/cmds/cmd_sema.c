#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_sema.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * sema_cmd() -- Print out information on a semaphore address.
 */
int
sema_cmd(command_t cmd)
{
	int i, sema_cnt = 0, first_time = 1;
	kaddr_t sema;
	k_ptr_t semap;

	semap = alloc_block(SEMA_SIZE(K), B_TEMP);
	if (!(cmd.flags & C_FULL)) {
		sema_s_banner(cmd.ofp, BANNER|SMAJOR);
	}
	for (i = 0; i < cmd.nargs; i++) {
		if (cmd.flags & C_FULL) {
			sema_s_banner(cmd.ofp, BANNER|SMAJOR);
		}
		GET_VALUE(cmd.args[i], &sema);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_SEMA_S;
			kl_print_error(K);
		}
		else {
			kl_get_struct(K, sema, SEMA_SIZE(K), semap, "sema");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_SEMA_S;
				kl_print_error(K);
			}
			else {
				print_sema_s(sema, semap, cmd.flags, cmd.ofp);
				sema_cnt++;
			}
		}
		if (cmd.flags & C_FULL) {
			fprintf(cmd.ofp, "\n");
		}
	}
	free_block(semap);
	sema_s_banner(cmd.ofp, SMAJOR);
	PLURAL("sema_s struct", sema_cnt, cmd.ofp);
	return(0);
}

#define _SEMA_USAGE "[-f] [-w outfile] sema_list"

/*
 * sema_usage() -- Print the usage string for the 'sema' command.
 */
void
sema_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SEMA_USAGE);
}

/*
 * sema_help() -- Print the help information for the 'sema' command.
 */
void
sema_help(command_t cmd)
{
	CMD_HELP(cmd, _SEMA_USAGE,
		"Display the sema_s structure located at each virtual address "
		"included in sema_list.");
}

/*
 * sema_parse() -- Parse the command line arguments for 'sema'.
 */
int
sema_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
