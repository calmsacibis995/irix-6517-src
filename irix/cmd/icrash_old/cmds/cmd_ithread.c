#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_ithread.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

#ifdef ANON_ITHREADS

/*
 * ithread_cmd() -- Dump out ithread struct information 
 */
int
ithread_cmd(command_t cmd)
{
	int i, mode, first_time = TRUE, ithread_cnt = 0;
	k_ptr_t itp; 
	kaddr_t value, ithread;

	if (!cmd.nargs) {
		ithread_cnt = list_active_ithreads(cmd.flags, cmd.ofp);
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			if ((first_time == TRUE) || (cmd.flags & C_FULL)) {
				if (cmd.flags & C_KTHREAD) {
					kthread_banner(cmd.ofp, BANNER|SMAJOR);
				}
				else {
					ithread_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
				first_time = FALSE;
			}

			get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_ITHREAD_S;
				kl_print_error(K);
				continue;
			}

			if (mode != 2) {
				KL_SET_ERROR_NVAL(KLE_BAD_ITHREAD_S, value, mode);
				kl_print_error(K);
			}
			else {
				ithread = value;
				itp = kl_get_ithread_s(K, ithread, 2, (cmd.flags|C_ALL));
				if (KL_ERROR) {
					KL_SET_ERROR_NVAL(KL_ERROR, value, mode);
					kl_print_error(K);
				}
				else {
					if (cmd.flags & C_KTHREAD) {
						print_kthread(ithread, itp, cmd.flags, cmd.ofp);
					}
					else {
						print_ithread_s(ithread, itp, cmd.flags, cmd.ofp);
					}
					free_block(itp);
					ithread_cnt++;
				}
			}
		}
	}
	if (cmd.flags & C_KTHREAD) {
		kthread_banner(cmd.ofp, SMAJOR);
	}
	else {
		ithread_s_banner(cmd.ofp, SMAJOR);
	}
	fprintf(cmd.ofp, "%d active ithreads found\n", ithread_cnt);
	return(0);
}

#define _ITHREAD_USAGE "[-f] [-n] [-k] [-w outfile] [ithread_list]"

/*
 * ithread_usage() -- Print the usage string for the 'ithread' command.
 */
void
ithread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ITHREAD_USAGE);
}

/*
 * ithread_help() -- Print the help information for the 'ithread' command.
 */
void
ithread_help(command_t cmd)
{
	CMD_HELP(cmd, _ITHREAD_USAGE,
		"Display relevant information for each entry in ithread_list. If "
		"no entries are specified, display information for all active "
		"ithreads. Entries in ithread_list can take the form of a "
		"virtual address. When the -k command line option is specified, "
		"display all ithread_s structs as kthread structs.");
}

/*
 * ithread_parse() -- Parse the command line arguments for 'ithread'.
 */
int
ithread_parse(command_t cmd)
{
	return (C_MAYBE|C_FULL|C_NEXT|C_KTHREAD|C_WRITE);
}
#endif
