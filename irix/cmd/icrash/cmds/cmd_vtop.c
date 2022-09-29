#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_vtop.c,v 1.15 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * vaddr_banner()
 */
void
vtop_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);

		fprintf(ofp, "           PADDR      PFN            "
			"K0ADDR            K1ADDR            K2ADDR\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);

		fprintf(ofp, "====================================="
			"==========================================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);

		fprintf(ofp, "-------------------------------------"
			"------------------------------------------\n");
	}
}

/*
 * print_addr()
 */
void
print_addr(kaddr_t paddr, int pfn, kaddr_t k0addr, kaddr_t k1addr,
	kaddr_t k2addr, FILE *ofp)
{
	fprintf(ofp, "%16llx %8d  ", paddr, pfn);

	if (k0addr) {
		fprintf(ofp, "%16llx  ", k0addr);
	} 
	else {
		fprintf(ofp, "              --  ");
	}

	if (k1addr) {
		fprintf(ofp, "%16llx  ", k1addr);
	} 
	else {
		fprintf(ofp, "              --  ");
	}

	if (k2addr) {
		fprintf(ofp, "%16llx\n", k2addr);
	} 
	else {
		fprintf(ofp, "              --\n");
	}
}

/*
 * vtop_cmd() -- Run the 'vtop' command.
 */
int
vtop_cmd(command_t cmd)
{
	int i, ret;
	uint pfn = 0;
	kaddr_t vaddr, paddr = 0, K0vaddr = 0, K1vaddr = 0, K2vaddr = 0;
		
	vtop_banner(cmd.ofp, BANNER|SMAJOR);

	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &vaddr);
		if (KL_ERROR) {
			kl_print_error();
			continue;
		}

		ret = kl_addr_convert(vaddr, VADDR_FLAG, &paddr, 
							&pfn, &K0vaddr, &K1vaddr, &K2vaddr);

		if (ret && (ret != KLE_AFTER_PHYSMEM) && (ret != KLE_BEFORE_RAM_OFFSET)
				&& (ret != KLE_PHYSMEM_NOT_INSTALLED)) {
			kl_print_error();
			continue;
		}

		print_addr(paddr, pfn, K0vaddr, K1vaddr, K2vaddr, cmd.ofp);

		/* There are several errors that are not fatal to the vtop command 
		 * (for example, it is possible for a K0 address to be valid and 
		 * still not map to physical memory for a particular dump). Printing
		 * a NOTICE message makes the user aware of this.
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

#define _VTOP_USAGE "[-w outfile] addr_list"

/*
 * vtop_usage() -- Print the usage string for the 'vtop' command.
 */
void
vtop_usage(command_t cmd)
{
	CMD_USAGE(cmd, _VTOP_USAGE);
}

/*
 * vtop_help() -- Print the help information for the 'vtop' command.
 */
void
vtop_help(command_t cmd)
{
	CMD_HELP(cmd, _VTOP_USAGE,
		"Display all possible virtual address mappings (K0, K1, and K2) "
		"for each virtual address in addr_list.");
}

/*
 * vtop_parse() -- Parse the command line arguments for 'vtop'.
 */
int
vtop_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
