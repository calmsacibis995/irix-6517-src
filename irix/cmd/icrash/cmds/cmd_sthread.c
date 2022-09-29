#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_sthread.c,v 1.7 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * sthread_cmd() -- Dump out sthread struct information 
 */
int
sthread_cmd(command_t cmd)
{
	int i, mode, first_time = TRUE, sthread_cnt = 0;
	k_ptr_t stp; 
	kaddr_t value, sthread;

	if (!cmd.nargs) {
		sthread_cnt = list_active_sthreads(cmd.flags, cmd.ofp);
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			if ((first_time == TRUE) || (cmd.flags & C_FULL)) {
				if (cmd.flags & C_KTHREAD) {
					kthread_banner(cmd.ofp, BANNER|SMAJOR);
				}
				else {
					sthread_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
				first_time = FALSE;
			}

			kl_get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_STHREAD_S;
				kl_print_error();
				continue;
			}

			if (mode != 2) {
				KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, value, mode);
				kl_print_error();
			}
			else {
				sthread = value;
				stp = kl_get_sthread_s(sthread, 2, (cmd.flags|C_ALL));
				if (KL_ERROR) {
					KL_SET_ERROR_NVAL(KL_ERROR, value, mode);
					kl_print_error();
				}
				else {
					if (cmd.flags & C_KTHREAD) {
						print_kthread(sthread, stp, cmd.flags, cmd.ofp);
					}
					else {
						print_sthread_s(sthread, stp, cmd.flags, cmd.ofp);
					}
					kl_free_block(stp);
					sthread_cnt++;
				}
			}
		}
	}
	if (cmd.flags & C_KTHREAD) {
		kthread_banner(cmd.ofp, SMAJOR);
	}
	else {
		sthread_s_banner(cmd.ofp, SMAJOR);
	}
	fprintf(cmd.ofp, "%d active sthreads found\n", sthread_cnt);
	return(0);
}

#define _STHREAD_USAGE "[-f] [-n] [-k] [-w outfile] [sthread_list]"

/*
 * sthread_usage() -- Print the usage string for the 'sthread' command.
 */
void
sthread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STHREAD_USAGE);
}

/*
 * sthread_help() -- Print the help information for the 'sthread' command.
 */
void
sthread_help(command_t cmd)
{
	CMD_HELP(cmd, _STHREAD_USAGE,
		"Display relevant information for each entry in sthread_list. If "
		"no entries are specified, display information for all active "
		"sthreads. Entries in sthread_list can take the form of a "
		"virtual address. When the -k command line option is specified, "
		"display all sthread_s structs as kthread structs.");
}

/*
 * sthread_parse() -- Parse the command line arguments for 'sthread'.
 */
int
sthread_parse(command_t cmd)
{
	return (C_MAYBE|C_FULL|C_NEXT|C_KTHREAD|C_WRITE);
}
