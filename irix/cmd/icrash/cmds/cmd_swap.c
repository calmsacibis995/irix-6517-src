#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_swap.c,v 1.19 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/* Defining the MAXLSWAP variable here.  Normally this would come from
 * some type of #include file, such as <sys/swap.h>, but the _KERNEL
 * mode causes other headaches.
 */
#define MAXLSWAP 255

/*
 * swap_cmd() -- Run the 'swap' command.
 */
int
swap_cmd(command_t cmd)
{
	int i = 0, swap_cnt = 0;
	kaddr_t value, j, taddr;
	struct syment *sym_lswaptab;
	k_ptr_t swapbuf, sbuf;

	/* Get the symbol.  This will load the offset of the
	 * structure into sym_lswaptab->n_value.
	 */
	if (!(sym_lswaptab = kl_get_sym("lswaptab",K_TEMP))) {
		fprintf(cmd.ofp, "lswaptab not found in symbol table\n");
		return(1);
	}

	/* Now start dumping out swap information.
	 */
	if (!(cmd.flags & C_FULL)) {
		swapinfo_banner(cmd.ofp, BANNER|SMAJOR);
	}
	swapbuf = kl_alloc_block(kl_struct_len("swapinfo"), K_TEMP);
	if (!cmd.nargs) {
		j = sym_lswaptab->n_value;
		kl_free_sym(sym_lswaptab);
		for (i = 0; i < MAXLSWAP; i++) {
			kl_get_kaddr(j, &taddr, "lswaptab index");
			if (!KL_ERROR && (taddr) &&
						(sbuf = get_swapinfo(taddr, swapbuf, cmd.flags))) {
					if (cmd.flags & C_FULL) {
						swapinfo_banner(cmd.ofp, BANNER|SMAJOR);
					}
					swap_cnt += print_swapinfo(taddr, sbuf, cmd.flags, cmd.ofp);
					if (cmd.flags & C_FULL) {
						fprintf(cmd.ofp, "\n");
					}
			}
			if (PTRSZ64) {
				j += 8;
			}
			else {
				j += 4;
			}
		}
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {
			if (cmd.flags & C_FULL) {
				swapinfo_banner(cmd.ofp, BANNER|SMAJOR);
			}
			GET_VALUE(cmd.args[i], &value);
			if (KL_ERROR) {
				kl_print_error();
			}
			else {
				sbuf = get_swapinfo(value, swapbuf, cmd.flags);
				if (KL_ERROR) {
					kl_print_error();
				} 
				else {
					swap_cnt += print_swapinfo(value, sbuf, cmd.flags, cmd.ofp);
				}
			}
			if (cmd.flags & C_FULL) {
				fprintf(cmd.ofp, "\n");
			}
		}
	}
	swapinfo_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d swap device%s found\n", 
		swap_cnt, (swap_cnt == 1) ? "" : "s");
	return(0);
}

#define _SWAP_USAGE "[-f] [-w outfile] [swap_list]"

/*
 * swap_usage() -- Print the usage string for the 'swap' command.
 */
void
swap_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SWAP_USAGE);
}

/*
 * swap_help() -- Print the help information for the 'swap' command.
 */
void
swap_help(command_t cmd)
{
	CMD_HELP(cmd, _SWAP_USAGE,
		"Dump out the list of swap devices, including the vnodes that are "
		"represented.  The number of pages, number of free pages, number "
		"of max pages, priority, and device are listed.  The -f flag will "
		"dump out the name of the swapinfo entry as well.");
}

/*
 * swap_parse() -- Parse the command line arguments for 'swap'.
 */
int
swap_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL);
}
