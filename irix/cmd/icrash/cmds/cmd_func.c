#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_func.c,v 1.12 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * func_banner()
 */
void
func_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "NAME                              ADDR      SIZE  "
			"FRAMESIZE  RA_OFFSET\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "=================================================="
			"====================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "--------------------------------------------------"
			"--------------------\n");
	}
}

/*
 * func_cmd() -- Run the 'func' command.
 */
int
func_cmd(command_t cmd)
{
	int i, size, frame_size, ra_offset, func_cnt = 0;
	kaddr_t inst, func_addr;
	char *funcname, *symname;

	func_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &inst);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_FUNCADDR;
			kl_print_error();
			continue;
		}
		if (funcname = kl_funcname(inst)) {
			func_addr = kl_funcaddr(inst);
			size = kl_funcsize(func_addr);
			frame_size = get_frame_size(func_addr, 1);
			ra_offset = get_ra_offset(func_addr, 1);
			fprintf(cmd.ofp, "%-20s  ", funcname);
			fprintf(cmd.ofp, "%16llx  ", func_addr);
			fprintf(cmd.ofp, "%8d  %9d  %9d\n", size, frame_size, ra_offset);
			func_cnt++;
		} 
		else {
			KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, inst, 2);
			kl_print_error();
		}
	}
	func_banner(cmd.ofp, SMAJOR);
	PLURAL("function", func_cnt, cmd.ofp);
	return(0);
}

#define _FUNC_USAGE "[-w outfile] func_addr"

/*
 * func_usage() -- Print the usage string for the 'func' command.
 */
void
func_usage(command_t cmd)
{
	CMD_USAGE(cmd, _FUNC_USAGE);
}

/*
 * func_help() -- Print the help information for the 'func' command.
 */
void
func_help(command_t cmd)
{
	CMD_HELP(cmd, _FUNC_USAGE,
		"Print out information about to a function at func_addr.");
}

/*
 * func_parse() -- Parse the command line arguments for 'func'.
 */
int
func_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
