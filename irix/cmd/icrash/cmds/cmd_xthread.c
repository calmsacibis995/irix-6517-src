#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_xthread.c,v 1.6 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * xthread_cmd() -- Dump out xthread struct information 
 */
int
xthread_cmd(command_t cmd)
{
	int i, mode, first_time = TRUE, xthread_cnt = 0;
	k_ptr_t stp; 
	kaddr_t value, xthread;

	if (!cmd.nargs) {
		xthread_cnt = list_active_xthreads(cmd.flags, cmd.ofp);
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			if ((first_time == TRUE) || (cmd.flags & C_FULL)) {
				if (cmd.flags & C_KTHREAD) {
					kthread_banner(cmd.ofp, BANNER|SMAJOR);
				}
				else {
					xthread_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
				first_time = FALSE;
			}

			kl_get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_XTHREAD_S;
				kl_print_error();
				continue;
			}

			if (mode != 2) {
				KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, value, mode);
				kl_print_error();
			}
			else {
				xthread = value;
				stp = kl_get_xthread_s(xthread, 2, (cmd.flags|C_ALL));
				if (KL_ERROR) {
					KL_SET_ERROR_NVAL(KL_ERROR, value, mode);
					kl_print_error();
				}
				else {
					if (cmd.flags & C_KTHREAD) {
						print_kthread(xthread, stp, cmd.flags, cmd.ofp);
					}
					else {
						print_xthread_s(xthread, stp, cmd.flags, cmd.ofp);
					}
					kl_free_block(stp);
					xthread_cnt++;
				}
			}
		}
	}
	if (cmd.flags & C_KTHREAD) {
		kthread_banner(cmd.ofp, SMAJOR);
	}
	else {
		xthread_s_banner(cmd.ofp, SMAJOR);
	}
	fprintf(cmd.ofp, "%d active xthreads found\n", xthread_cnt);
	return(0);
}

#define _XTHREAD_USAGE "[-f] [-n] [-k] [-w outfile] [xthread_list]"

/*
 * xthread_usage() -- Print the usage string for the 'xthread' command.
 */
void
xthread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _XTHREAD_USAGE);
}

/*
 * xthread_help() -- Print the help information for the 'xthread' command.
 */
void
xthread_help(command_t cmd)
{
	CMD_HELP(cmd, _XTHREAD_USAGE,
		"Display relevant information for each entry in xthread_list. If "
		"no entries are specified, display information for all active "
		"xthreads. Entries in xthread_list can take the form of a "
		"virtual address. When the -k command line option is specified, "
		"display all xthread_s structs as kthread structs.");
}

/*
 * xthread_parse() -- Parse the command line arguments for 'xthread'.
 */
int
xthread_parse(command_t cmd)
{
	return (C_MAYBE|C_FULL|C_NEXT|C_KTHREAD|C_WRITE);
}
