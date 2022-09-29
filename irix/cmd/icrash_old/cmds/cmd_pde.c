#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_pde.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "icrash.h"
#include "extern.h"

/*
 * pde_cmd() -- Dump out pde information, with headers and formatting.
 */
int
pde_cmd(command_t cmd)
{
	int i, pde_cnt = 0, firsttime = 1;
	kaddr_t value = 0;
	k_ptr_t pdp;

	pdp = alloc_block(PDE_SIZE(K), B_TEMP);

	pde_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PDE;
			kl_print_error(K);
			continue;
		}
		kl_get_pde(K, value, pdp);
		if (KL_ERROR) {
			kl_print_error(K);
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
	free_block(pdp);
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
