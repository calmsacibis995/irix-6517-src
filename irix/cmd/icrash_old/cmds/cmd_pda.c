#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_pda.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <stdio.h>
#include <sys/types.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/*
 * pda_cmd() -- Dump out the pda structures in the kernel (pdaindr).
 */
int
pda_cmd(command_t cmd)
{
	int i, pda_cnt = 0, first_time = TRUE;
	k_ptr_t pdap;
	kaddr_t value, pdaval;

	if (!(cmd.flags & C_FULL)) {
		pda_s_banner(cmd.ofp, BANNER|SMAJOR);
	}
	pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
	if (!cmd.nargs) {
		for (i = 0; i < K_MAXCPUS(K); i++) {
			pdaval = kl_get_pda_s(K, (kaddr_t)i, pdap);
			if (KL_ERROR) {
				continue;
			}
			else {
				if (cmd.flags & C_FULL) {
					pda_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
				print_pda_s(pdaval, pdap, cmd.flags, cmd.ofp);
				pda_cnt++;
				if (cmd.flags & C_FULL) {
					fprintf(cmd.ofp, "\n");
				}
			}
		}
	}
	else {
		for (i = 0; i < cmd.nargs; i++) {
			get_value(cmd.args[i], NULL, K_MAXCPUS(K), &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PDA;
				kl_print_error(K);
			}
			else {
				pdaval = kl_get_pda_s(K, value, pdap);
				if (KL_ERROR) {
					kl_print_error(K);
				}
				else {
					if (cmd.flags & C_FULL) {
						pda_s_banner(cmd.ofp, BANNER|SMAJOR);
					}
					print_pda_s(pdaval, pdap, cmd.flags, cmd.ofp);
					pda_cnt++;
					if (cmd.flags & C_FULL) {
						fprintf(cmd.ofp, "\n");
					}
				}
			}
		}
	}
	pda_s_banner(cmd.ofp, SMAJOR);
	PLURAL("pda_s struct", pda_cnt, cmd.ofp);
	free_block(pdap);
	return(0);
}

#define _PDA_USAGE "[-f] [-w outfile] [cpu_list]"

/*
 * pda_usage() -- Print the usage string for the 'pda' command.
 */
void
pda_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PDA_USAGE);
}

/*
 * pda_help() -- Print the help information for the 'pda' command.
 */
void
pda_help(command_t cmd)
{
	CMD_HELP(cmd, _PDA_USAGE,
		"Display the pda_s structure associated with each item in cpu_list. "
		"Entries in pda_list can take the form of a CPU ID or a virtual "
		"address pointing to a pda_s struct in memory. If cpu_list is "
		"empty, the pda_s structure for all CPUs will be displayed.");
}

/*
 * pda_parse() -- Parse the command line arguments for 'pda'.
 */
int
pda_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL);
}
