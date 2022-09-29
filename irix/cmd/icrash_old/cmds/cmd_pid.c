#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_pid.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * pid_cmd() -- Dump info on all active pid_entry structs
 */
int
pid_cmd(command_t cmd)
{
	int i, mode, first_time = TRUE, pid_cnt = 0;
	kaddr_t value, pid_slot, pid_entry;
	k_ptr_t psp, pep;

	if (!cmd.nargs) {
		pid_entry_banner(cmd.ofp, BANNER|SMAJOR);
		pid_cnt = list_pid_entries(cmd.flags, cmd.ofp);
	}
	else {

		psp = alloc_block(PID_SLOT_SIZE(K), B_TEMP);
		pep = alloc_block(PID_ENTRY_SIZE(K), B_TEMP);

		for (i = 0; i < cmd.nargs; i++) {

			if (first_time || (cmd.flags & (C_FULL|C_NEXT))) {
				pid_entry_banner(cmd.ofp, BANNER|SMAJOR);
				first_time = FALSE;
			}

			get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PID_ENTRY;
				kl_print_error(K);
				continue;
			}

			if (mode == 0) {
				KL_SET_ERROR_NVAL(KLE_BAD_PID_ENTRY, value, 0);
				kl_print_error(K);
				continue;
			}
			else if (mode == 1) {

				pid_slot = K_PID_SLOT(K, value);
				while (pid_slot) {

					kl_get_struct(K, pid_slot, 
							PID_SLOT_SIZE(K), psp, "pid_slot");
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_PID_SLOT;
						break;
					}

					/* Get the pointer to the pid_entry struct
					 */
					pid_entry = kl_kaddr(K, psp, "pid_slot", "ps_chain");
					if (!pid_entry) {
						KL_SET_ERROR_NVAL(KLE_BAD_PID_SLOT, value, 0);
						break;
					}

					if (value == KL_INT(K, psp, "pid_slot", "ps_pid")) {
						
						/* We found it!
						 */
						break;
					}

					/* Now get the pid_entry struct
					 */
					kl_get_struct(K, pid_entry, 
							PID_ENTRY_SIZE(K), pep, "pid_entry");
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_PID_ENTRY;
						break;
					}
					pid_slot = kl_kaddr(K, pep, "pid_entry", "pe_next");
				}
			}
			else if (mode == 2) {
				pid_entry = value;
			}

			if (KL_ERROR) {
				kl_print_error(K);
				continue;
			}

			kl_get_struct(K, pid_entry, PID_ENTRY_SIZE(K), pep, "pid_entry");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PID_ENTRY;
				kl_print_error(K);
				continue;
			}

			print_pid_entry(pid_entry, pep, cmd.flags, cmd.ofp);
			pid_cnt++;
		}
		free_block(psp);
		free_block(pep);
	}
	pid_entry_banner(cmd.ofp, SMAJOR);
	PLURAL("pid_entry struct", pid_cnt, cmd.ofp);
	return(0);
}

#define _PID_USAGE "[-a] [-f] [-w outfile] [pid_list]"

/*
 * pid_usage() -- Print the usage string for the 'pid' command.
 */
void
pid_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PID_USAGE);
}

/*
 * pid_help() -- Print the help information for the 'pid' command.
 */
void
pid_help(command_t cmd)
{
	CMD_HELP(cmd, _PID_USAGE,
		"Display information from the pid_entry structure for each entry "
		"in pid_list.  If no entries are specified, display all active "
		"(in use) pid entries.  Entries in pid_list can take the form "
		"of a process PID (following a '#'), or a virtual address.");
}

/*
 * pid_parse() -- Parse the command line arguments for 'pid'.
 */
int
pid_parse(command_t cmd)
{
	return (C_MAYBE|C_ALL|C_FULL|C_NEXT|C_WRITE);
}
