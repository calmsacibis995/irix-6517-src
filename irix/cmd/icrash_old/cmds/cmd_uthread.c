#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_uthread.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * uthread_cmd() -- Dump out uthread struct information 
 */
int
uthread_cmd(command_t cmd)
{
	int i, j, utcnt = 0, mode, first_time = TRUE, uthread_cnt = 0;
	k_ptr_t procp, utp; 
	kaddr_t value, proc, uthread;

	if (!cmd.nargs) {
		uthread_cnt = list_active_uthreads(cmd.flags, cmd.ofp);
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			if ((first_time == TRUE) || (cmd.flags & C_FULL) ||
					(cmd.flags & C_NEXT)) {
				if (cmd.flags & C_KTHREAD) {
					kthread_banner(cmd.ofp, BANNER|SMAJOR);
				}
				else {
					uthread_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
				first_time = FALSE;
			}

			get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_UTHREAD_S;
				kl_print_error(K);
				continue;
			}

			if (mode == 0) {
				KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, value, mode);
				kl_print_error(K);
			}
			else if (mode == 1) {

				/* Treat value as a PID. This will allow us to print
				 * out all uthreads associated with a particular PID.
				 */
				proc = kl_pid_to_proc(K, value);
				procp = kl_get_proc(K, proc, 2, 0);
				uthread = kl_proc_to_kthread(K, procp, &utcnt);
				free_block(procp);
			}
			else {
				if (cmd.flags & C_SIBLING) {

					/* Print out all siblings for the specified uthread
					 */
					utp = kl_get_uthread_s(K, value, 2, (cmd.flags|C_ALL));
					proc = kl_kaddr(K, utp, "uthread_s", "ut_proc");
					free_block(utp);
					procp = kl_get_proc(K, proc, 2, 0);
					uthread = kl_proc_to_kthread(K, procp, &utcnt);
					free_block(procp);
				}
				else {
					uthread = value;
					utcnt = 1;
				}
			}

			for (j = 0; j < utcnt; j++) {
				utp = kl_get_uthread_s(K, uthread, 2, (cmd.flags|C_ALL));
				if (KL_ERROR) {
					KL_SET_ERROR_NVAL(KL_ERROR, value, mode);
					kl_print_error(K);
					break;
				}
				if (cmd.flags & C_KTHREAD) {
					print_kthread(uthread, utp, cmd.flags, cmd.ofp);
				}
				else {
					print_uthread_s(uthread, utp, cmd.flags, cmd.ofp);
				}
				uthread_cnt++;
				uthread = kl_kaddr(K, utp, "uthread_s", "ut_next");
				free_block(utp);
			}
		}
	}
	if (cmd.flags & C_KTHREAD) {
		kthread_banner(cmd.ofp, SMAJOR);
	}
	else {
		uthread_s_banner(cmd.ofp, SMAJOR);
	}
	fprintf(cmd.ofp, "%d active uthreads found\n", uthread_cnt);
	return(0);
}

#define _UTHREAD_USAGE "[-f] [-n] [-k] [-S] [-w outfile] [uthread_list]"

/*
 * uthread_usage() -- Print the usage string for the 'uthread' command.
 */
void
uthread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _UTHREAD_USAGE);
}

/*
 * uthread_help() -- Print the help information for the 'uthread' command.
 */
void
uthread_help(command_t cmd)
{
	CMD_HELP(cmd, _UTHREAD_USAGE,
		"Display relevant information for each entry in uthread_list. If "
		"no entries are specified, display information for all active "
		"uthreads. When the -S command line option is specified, displays "
		"a list of all siblings for the specified uthread. Entries in "
		"uthread_list can take the form of a process PID (following a '#'), "
		"or virtual address. Note that with the PID option, all uthreads "
		"that share a specified PID will be displayed. When the -k command "
		"line option is specified, display all uthread_s structs as"
		"kthread structs.\n");
}

/*
 * uthread_parse() -- Parse the command line arguments for 'uthread'.
 */
int
uthread_parse(command_t cmd)
{
	return (C_MAYBE|C_FULL|C_NEXT|C_KTHREAD|C_SIBLING|C_WRITE);
}
