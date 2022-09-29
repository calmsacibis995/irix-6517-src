#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_string.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * string_cmd() -- Run the 'string' command.
 */
int
string_cmd(command_t cmd)
{
	int c;
	k_uint_t count;
	kaddr_t addr;
	struct syment *sp;

	sp = get_sym(cmd.args[0], B_TEMP);
	if (!KL_ERROR) {
		addr = sp->n_value;
		free_sym(sp);
	} 
	else {
		GET_VALUE(cmd.args[0], &addr);
		if (KL_ERROR) {
			kl_print_error(K);
			return(1);
		}
	}

	if (cmd.nargs > 1) {
		GET_VALUE(cmd.args[1], &count);
		if (KL_ERROR) {
			kl_print_error(K);
			return(1);
		}
	} 
	else {
		count = 1;
	}

	if (!addr) {
		KL_SET_ERROR_CVAL(KLE_BAD_SYMNAME, cmd.args[0]);
		kl_print_error(K);
		return(1);
	}

	kl_is_valid_kaddr(K, addr, (k_ptr_t)NULL, 0);
	if (KL_ERROR) {
		kl_print_error(K);
		return(1);
	}

	c = 0;
	fprintf(cmd.ofp, "0x%llx = \"", addr);
	while (c++ < count) {
		addr = dump_string(addr, cmd.flags, cmd.ofp);
		if (KL_ERROR) {
			kl_print_error(K);
			break;
		}
	}
	fprintf(cmd.ofp, "\"\n");
	return(0);
}

#define _STRING_USAGE "[-w outfile] start_address | symbol [count]"

/*
 * string_usage() -- Print the usage string for the 'string' command.
 */
void
string_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STRING_USAGE);
}

/*
 * string_help() -- Print the help information for the 'string' command.
 */
void
string_help(command_t cmd)
{
	CMD_HELP(cmd, _STRING_USAGE,
		"Display count strings of ASCII characters starting at "
		"start_address (or address for symbol).");
}

/*
 * string_parse() -- Parse the command line arguments for 'string'.
 */
int
string_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
