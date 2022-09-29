#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_struct.c,v 1.14 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * struct_cmd() -- Dump out structure information.
 */
int
struct_cmd(command_t cmd)
{
	kaddr_t addr;

	if (cmd.flags & C_LIST) {
		structlist(cmd.ofp);
		return(0);
	}
	else if (cmd.nargs == 0) {
		struct_usage(cmd);	
		return(1);
	}
	else if (cmd.nargs != 2) {
		struct_usage(cmd);
		return(1);
	}

	/* Get the address of the struct
	 */
	GET_VALUE(cmd.args[1], &addr);
	if (KL_ERROR) {
		kl_print_error();
		return(1);
	}
	struct_print(cmd.args[0], addr, cmd.flags, cmd.ofp);
	return(0);
}

#define _STRUCT_USAGE "[-f] [-n] [-l] [-w outfile] struct addr"

/*
 * struct_usage() -- Print the usage string for the 'struct' command.
 */
void
struct_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STRUCT_USAGE);
}

/*
 * struct_help() -- Print the help information for the 'struct' command.
 */
void
struct_help(command_t cmd)
{
	CMD_HELP(cmd, _STRUCT_USAGE,
		"Print structure information for struct using the block of memory "
		"pointed to by addr.");
}

/*
 * struct_parse() -- Parse the command line arguments for 'struct'.
 */
int
struct_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_NEXT|C_LIST);
}
