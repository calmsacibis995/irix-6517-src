#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_proc.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * proc_cmd() -- Dump out proc table information.
 */
int
proc_cmd(command_t cmd)
{
	int i, mode, first_time = TRUE, proc_cnt = 0;
	kaddr_t value, proc;
	k_ptr_t procp;

	if (!cmd.nargs) {
		proc_cnt = list_active_procs(cmd.flags, cmd.ofp);
	}
	else {
		for (i = 0; i < cmd.nargs; i++) {
			if ((first_time == TRUE) || (cmd.flags & C_FULL)) {
				proc_banner(cmd.ofp, BANNER|SMAJOR);
				first_time = FALSE;
			}
			get_value(cmd.args[i], &mode, K_NPROCS(K), &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PROC;
				kl_print_error(K);
			}
			else {
				procp = kl_get_proc(K, value, mode, (cmd.flags|C_ALL));
				if (KL_ERROR) {
					kl_print_error(K);
				}
				else {
					if (mode == 1) {
						proc = kl_pid_to_proc(K, (int)value);
					}
					else {
						proc = value;
					}
					print_proc(proc, procp, cmd.flags, cmd.ofp);
					proc_cnt++;
					free_block(procp);
				} 
			}
		}
	}
	proc_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d active processes found\n", proc_cnt);
	return(0);
}

#define _PROC_USAGE "[-a] [-f] [-w outfile] [proc_list]"

/*
 * proc_usage() -- Print the usage string for the 'proc' command.
 */
void
proc_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PROC_USAGE);
}

/*
 * proc_help() -- Print the help information for the 'proc' command.
 */
void
proc_help(command_t cmd)
{
	CMD_HELP(cmd, _PROC_USAGE,
		"Display the proc structure for each entry in proc_list.  If no "
		"entries are specified, display the entire proc table.  Entries "
		"in proc_list can take the form of a process PID (following a '#'), "
		"or by a virtual address.");
}

/*
 * proc_parse() -- Parse the command line arguments for 'proc'.
 */
int
proc_parse(command_t cmd)
{
	return (C_MAYBE|C_ALL|C_FULL|C_WRITE);
}
