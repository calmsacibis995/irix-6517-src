#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_symbol.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

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
 * symbol_banner()
 */
void
symbol_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "NAME                  TYPE              ADDR\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "============================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "--------------------------------------------\n");
	}
}

/*
 * print_symbol()
 */
int
print_symbol(struct syment *sp, char *name, int flags, FILE *ofp)
{
	int ifd;
	char *filename;

	if (!sp) {
		fprintf(ofp, "%-20s  ", name);
		fprintf(ofp, "%4d  ", -1);
		fprintf(ofp, "%16s\n", "UNKNOWN!");
		return(0);
	}

	fprintf(ofp, "%-20s  ", name);
	fprintf(ofp, "%4d  ", sp->n_type);
	fprintf(ofp, "%16llx ", sp->n_value);
	if((flags & C_FULL) &&
	   (filename = dw_get_srcfile((kaddr_t)sp->n_value)))
	{
		fprintf(ofp, "\tFile--> %s\n",filename);
		free_block((caddr_t *)filename);
	}
	else
	{
		fprintf(ofp,"\n");
	}

	
	return(1);
}

/*
 * symbol_cmd()
 */
int
symbol_cmd(command_t cmd)
{
	int i, symbol_cnt = 0;
	struct syment *sp;

	symbol_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		sp = get_sym(cmd.args[i], B_TEMP);
		if (KL_ERROR) {
			kl_print_error(K);
		}
		else {
			symbol_cnt += print_symbol(sp, cmd.args[i], cmd.flags, cmd.ofp);
			free_sym(sp);
		}
	}
	symbol_banner(cmd.ofp, SMAJOR);
	PLURAL("symbol", symbol_cnt, cmd.ofp);
	return(0);
}

#define _SYMBOL_USAGE "[-f] [-w outfile] symbol_list"

/*
 * symbol_usage() -- Print the usage string for the 'symbol' command.
 */
void
symbol_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SYMBOL_USAGE);
}

/*
 * symbol_help() -- Print the help information for the 'symbol' command.
 */
void
symbol_help(command_t cmd)
{
	CMD_HELP(cmd, _SYMBOL_USAGE,
		"Displays information about each kernel symbol included in "
		"symbol_list.");
}

/*
 * symbol_parse() -- Parse the command line arguments for 'symbol'.
 */
int
symbol_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
