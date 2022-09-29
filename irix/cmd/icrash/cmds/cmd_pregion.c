#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_pregion.c,v 1.21 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * pregion_cmd() -- Display the set of pregions for a process
 */
int
pregion_cmd(command_t cmd)
{
	int i, firsttime = 1, mode, preg_cnt = 0;
	kaddr_t npr, value;
	kaddr_t alvp;
	k_ptr_t prp, setbuf;
	k_ptr_t p, avl;

	if (cmd.flags & C_UTHREAD || cmd.flags & C_PROC) {
		kl_get_value(pfile, &mode, K_NPROCS, &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PREGION;
			kl_print_error();
			return(1);
		}
		if(p = kl_get_proc(value, 2, (cmd.flags & ~C_ALL))) {
			value = kl_kaddr(p,"proc_proxy_s","prxy_threads");
			kl_free_block(p);

			for(;value;) {
				list_uthread_pregions(value, 2, cmd.flags, cmd.ofp);
				p = kl_get_uthread_s(value, 2, cmd.flags);
				value = kl_kaddr(p, "uthread_s", "ut_next");
				kl_free_block(p);
			}
		}
		else if (p = kl_get_uthread_s(value, 2, (cmd.flags & ~C_ALL))) {
			kl_free_block(p);
			list_uthread_pregions(value, mode, cmd.flags, cmd.ofp);
		}
		return(0);
	} 
	else if (cmd.nargs == 0) {
		pregion_usage(cmd);
		return(1);
	}
	else {
		pregion_banner(cmd.ofp, BANNER|SMAJOR);
		prp = kl_alloc_block(PREGION_SIZE, K_TEMP);
		for (i = 0; i < cmd.nargs; i++) {
			GET_VALUE(cmd.args[i], &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PREGION;
				kl_print_error();
				continue;
			}
			kl_get_struct(value, PREGION_SIZE, prp, "pregion");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PREGION;
				kl_print_error();
				continue;
			}
			else {
				if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & C_FULL)) {
					if (!firsttime) {
						pregion_banner(cmd.ofp, BANNER|SMAJOR);
					} 
					else {
						firsttime = 0;
					}
				}
				print_pregion(value, prp, cmd.flags, cmd.ofp);
				preg_cnt++;
			} 
		}
		kl_free_block(prp);
	}
	pregion_banner(cmd.ofp, SMAJOR);
	PLURAL("proc pregion", preg_cnt, cmd.ofp);
	return(0);
}

#define _PREGION_USAGE "[-a] [-f] [-n] [-w outfile] -u uthread|pregion_list"

/*
 * pregion_usage() -- Print the usage string for the 'pregion' command.
 */
void
pregion_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PREGION_USAGE);
}

/*
 * pregion_help() -- Print the help information for the 'pregion' command.
 */
void
pregion_help(command_t cmd)
{
	CMD_HELP(cmd, _PREGION_USAGE,
		"Display the pregion structure located at each virtual address "
		"included in pregion_list.  If the -u option is used, display "
		"all pregions allocated to uthread. With the -f option only the valid "
		"pde's are printed out. Use the -a option to print all the pde's.");
}

/*
 * pregion_parse() -- Parse the command line arguments for 'pregion'.
 */
int
pregion_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_ALL|C_UTHREAD);
}
