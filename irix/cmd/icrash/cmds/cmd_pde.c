#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_pde.c,v 1.13 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * pde_cmd() -- Dump out pde information, with headers and formatting.
 */
int
pde_cmd(command_t cmd)
{
	int i, pde_cnt = 0, firsttime = 1;
	kaddr_t value = 0;
	k_ptr_t pdp;

	pdp = kl_alloc_block(PDE_SIZE, K_TEMP);

	pde_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PDE;
			kl_print_error();
			continue;
		}
		kl_get_pde(value, pdp);
		if (KL_ERROR) {
			kl_print_error();
		}
		else {
			if (DEBUG(DC_PAGE, 1) || (cmd.flags & C_FULL)) {
				if (!firsttime) {
					pde_banner(cmd.ofp, BANNER|SMAJOR);
				} 
				else {
					firsttime = 0;
				}
			}
			print_pde(value, pdp, cmd.ofp);
			pde_cnt++;
		} 
	}
	kl_free_block(pdp);
	pde_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d pde struct%s found\n", 
		pde_cnt, (pde_cnt != 1) ? "s" : "");
	return(0);
}

#define _PDE_USAGE "[-w outfile] pde_list"

/*
 * pde_usage() -- Print the usage string for the 'pde' command.
 */
void
pde_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PDE_USAGE);
}

/*
 * pde_help() -- Print the help information for the 'pde' command.
 */
void
pde_help(command_t cmd)
{
	CMD_HELP(cmd, _PDE_USAGE,
		"Print out a pde (page descriptor entry) that stores the pte "
		"(page table entry) and page index information for each address "
		"in pde_list.");
}

/*
 * pde_parse() -- Parse the command line arguments for 'pde'.
 */
int
pde_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
