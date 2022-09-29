#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_ptov.c,v 1.13 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * ptov_cmd()
 */
int
ptov_cmd(command_t cmd)
{
	int i, ret, mode;
	uint pfn;
	kaddr_t value, paddr = 0, K0vaddr = 0, K1vaddr = 0, K2vaddr = 0;

	vtop_banner(cmd.ofp, BANNER|SMAJOR);

	for(i = 0; i < cmd.nargs; i++) {

		kl_get_value(cmd.args[i], &mode, 0, &value);
		if (KL_ERROR) {
			kl_print_error();
			continue;
		}

		/* If mode is one it means the first character of the
		 * paramater was a pound sign ('#'). When this is the case,
		 * paddr contains a PFN instead of a physical address...
		 * So, we load pfn with the value contained in paddr and
		 * use this to generate a physical address (after making
		 * sure it is valid).
		 */
		if (mode == 1) { 
			ret = kl_addr_convert(value, PFN_FLAG, &paddr, &pfn, 
				&K0vaddr, &K1vaddr, &K2vaddr);
		} 
		else {
			ret = kl_addr_convert(value, PADDR_FLAG, &paddr, &pfn, 
				&K0vaddr, &K1vaddr, &K2vaddr);
		}
		if (ret && (ret != KLE_AFTER_PHYSMEM) && (ret != KLE_BEFORE_RAM_OFFSET) 
							&& (ret != KLE_PHYSMEM_NOT_INSTALLED)) {
			KL_SET_ERROR_NVAL(ret, value, mode);
			kl_print_error();
			continue;
		}

		print_addr(paddr, pfn, K0vaddr, K1vaddr, K2vaddr, cmd.ofp);

		/* There are several errors that are not fatal to the ptov command 
		 * (for example, it is possible for a physical address to be below
		 * the start of RAM.  Printing a NOTICE message makes the user aware 
		 * of this.
		 */
		if (ret == KLE_AFTER_PHYSMEM) {
			fprintf(cmd.ofp, "  NOTICE: 0x%llx exceeds installed physical "
				"memory (%u pages)\n", paddr, K_PHYSMEM);
			continue;
		}
		else if (ret == KLE_BEFORE_RAM_OFFSET) {
			fprintf(cmd.ofp, "  NOTICE: 0x%llx proceeds start of physical "
				"memory (0x%llx)\n", paddr, K_RAM_OFFSET);
		}
		else if (ret == KLE_PHYSMEM_NOT_INSTALLED) {
			fprintf(cmd.ofp, "  NOTICE: physical address 0x%llx not "
				"installed\n", paddr);
		}
	}
	vtop_banner(cmd.ofp, SMAJOR);
	return(0);
}

#define _PTOV_USAGE "[-w outfile] address_list"

/*
 * ptov_usage() -- Print the usage string for the 'ptov' command.
 */
void
ptov_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PTOV_USAGE);
}

/*
 * ptov_help() -- Print the help information for the 'ptov' command.
 */
void
ptov_help(command_t cmd)
{
	CMD_HELP(cmd, _PTOV_USAGE,
		"Display all possible virtual address mappings (K0, K1, and K2) "
		"for each entry in address_list.  Entries in address_list can be "
		"a hexadecimal physical address or a PFN (following a '#').");
}

/*
 * ptov_parse() -- Parse the command line arguments for 'ptov'.
 */
int
ptov_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
