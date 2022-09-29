#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_findsym.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <nlist.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/*
 * findsym_banner() -- Print out banner information for 'findsym'.
 */
void
findsym_banner(FILE *ofp, int flags)
{
	if (flags & SMAJOR) {
		fprintf(ofp,
			"======================================="
			"=======================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"---------------------------------------"
			"---------------------------------------\n");
	}
}

/*
 * findsym_cmd() -- Find the closest symbol to the address passed in.
 */
int
findsym_cmd(command_t cmd)
{
	int i, symbol_cnt = 0;
	kaddr_t value;
	struct syment *sp;
	symaddr_t *ss;
	char *filename=NULL;

	findsym_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		sp = get_sym(cmd.args[i], B_TEMP);
		if (KL_ERROR) {
			GET_VALUE(cmd.args[i], &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_SYMADDR;
				kl_print_error(K);
				continue;
			}
			if (!value) {
				KL_SET_ERROR_NVAL(KLE_BAD_SYMADDR, value, 2);
				kl_print_error(K);
				continue;
			}
			if (ss = sym_addrtonm(stp, value)) {
				sp = get_sym(ss->s_name, B_TEMP);
			}
			if (KL_ERROR) {
				kl_print_error(K);
				continue;
			}
			fprintf(cmd.ofp, "0x%llx --> %s", value, sp->n_name);
			if (value - sp->n_value) {
				fprintf(cmd.ofp, " + 0x%llx ", (value - sp->n_value));
			}
			if((cmd.flags & C_FULL) &&
			   (filename = dw_get_srcfile((kaddr_t)sp->n_value)))
			  {
			    fprintf(cmd.ofp, " \tFile--> %s\n",filename);
			    free_block((caddr_t *)filename);
			  }
			else
			  fprintf(cmd.ofp,"\n");
			
			symbol_cnt++;
			free_sym(sp);
		}
		else {
			fprintf(cmd.ofp, "%s--> 0x%llx ", 
				cmd.args[i], sp->n_value);
			if((cmd.flags & C_FULL) &&
			   (filename = dw_get_srcfile((kaddr_t)sp->n_value)))
			  {
			    fprintf(cmd.ofp, " \tFile--> %s\n",filename);
			    free_block((caddr_t *)filename);
			  }
			else
			  fprintf(cmd.ofp,"\n");

			symbol_cnt++;
			free_sym(sp);
		}
	}
	findsym_banner(cmd.ofp, SMAJOR);
	PLURAL("symbol", symbol_cnt, cmd.ofp);
	return(0);
}

#define _FINDSYM_USAGE "[-f] [-w outfile] address_list"

/*
 * findsym_usage() -- Print the usage string for the 'findsym' command.
 */
void
findsym_usage(command_t cmd)
{
	CMD_USAGE(cmd, _FINDSYM_USAGE);
}

/*
 * findsym_help() -- Print the help information for the 'findsym' command.
 */
void
findsym_help(command_t cmd)
{
	CMD_HELP(cmd, _FINDSYM_USAGE,
		"Locate the kernel symbol closest to each virtual address "
		"contained in address_list.");
}

/*
 * findsym_parse() -- Parse the command line arguments for 'findsym'.
 */
int
findsym_parse(command_t cmd)
{
	return (C_TRUE|C_FULL|C_WRITE);
}
