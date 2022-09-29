#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_dump.c,v 1.15 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * dump_cmd() -- Run the 'dump' command.
 */
int
dump_cmd(command_t cmd)
{
	int mode; 
	k_uint_t size;
	kaddr_t start_addr;
	struct syment *sp;

	sp = kl_get_sym(cmd.args[0], K_TEMP);
	if (!KL_ERROR) {
		start_addr = sp->n_value;
		kl_free_sym(sp);
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(cmd.ofp, "start_addr=0x%llx\n", start_addr);
		}
	}
	else {
		kl_get_value(cmd.args[0], &mode, 0, &start_addr);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}

		/* If entered as a PFN, convert to physical address  
		 */
		if (mode == 1) {
			start_addr = Ctob(start_addr);
		}
	}

	if (cmd.nargs > 1) {
		GET_VALUE(cmd.args[1], &size);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}
	} 
	else {
		size = 1;
	}

	if ((start_addr == 0) && 
		((cmd.args[0][0] != '0') && (cmd.args[0][0] != '#'))) {
			fprintf(cmd.ofp, "%s not found in symbol table.\n", cmd.args[0]);
	} 
	dump_memory(start_addr, size, cmd.flags, cmd.ofp);
	if (KL_ERROR) {
		kl_print_error();
		return(1);
	}
	return(0);
}

#define _DUMP_USAGE \
	"[-d] [-o] [-x] [-B] [-D] [-H] [-W] [-w outfile] addr [count]"
/*
 * dump_usage() -- Print the usage string for the 'dump' command.
 */
void
dump_usage(command_t cmd)
{
	CMD_USAGE(cmd, _DUMP_USAGE);
}

/*
 * dump_help() -- Print the help information for the 'dump' command.
 */
void
dump_help(command_t cmd)
{
	CMD_HELP(cmd, _DUMP_USAGE,
		"Display count values starting at kernel virtual address "
		"addr in one of the following formats: decimal (-d), "
		"octal (-o), or hexadecimal (-x).  The default format is hexidecimal, "
		"and the default count is 1.");
}

/*
 * dump_parse() -- Parse the command line arguments for the 'dump' command.
 */
int
dump_parse(command_t cmd)
{
	int i;

	i = C_TRUE|C_DECIMAL|C_OCTAL|C_HEX|C_BYTE|C_WORD|C_DWORD|C_HWORD|C_WRITE;
	return (i);
}
