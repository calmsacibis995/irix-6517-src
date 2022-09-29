#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_pmap.c,v 1.13 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * pmap_cmd() -- Dump out page map data.
 */
int
pmap_cmd(command_t cmd)
{
	int pmap_cnt = 0, i, firsttime = 1;
	kaddr_t value = 0;
	k_ptr_t pmp;

	pmap_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			kl_print_error();
		}
		else {
			pmp = kl_alloc_block(PMAP_SIZE, K_TEMP);
			kl_get_struct(value, PMAP_SIZE, pmp, "pmap");
			if (KL_ERROR) {
				kl_print_error();
			}
			else {
				if (klib_debug > 1 || (cmd.flags & C_FULL)) {
					if (!firsttime) {
						pmap_banner(cmd.ofp, BANNER|SMAJOR);
					} 
					else {
						firsttime = 0;
					}
				}
				print_pmap(value, pmp, cmd.flags, cmd.ofp);
				pmap_cnt++;
			}
		}
	}
	pmap_banner(cmd.ofp, SMAJOR);
	PLURAL("pmap struct", pmap_cnt, cmd.ofp);
	kl_free_block(pmp);
	return(0);
}

#define _PMAP_USAGE "[-f] [-w outfile] pmap_list"

/*
 * pmap_usage() -- Print the usage string for the 'pmap' command.
 */
void
pmap_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PMAP_USAGE);
}

/*
 * pmap_help() -- Print the help information for the 'pmap' command.
 */
void
pmap_help(command_t cmd)
{
	CMD_HELP(cmd, _PMAP_USAGE,
		"Display the pmap structure located at each virtual address "
		"included in pmap_list.");
}

/*
 * pmap_parse() -- Parse the command line arguments for 'pmap'.
 */
int
pmap_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
