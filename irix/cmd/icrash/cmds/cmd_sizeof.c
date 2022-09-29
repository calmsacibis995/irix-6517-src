#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_sizeof.c,v 1.13 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * sizeof_cmd() -- Find an item in the symbol table that is either
 *                 a struct, field, etc.
 */
int
sizeof_cmd(command_t cmd)
{
	int i, size;
	char *ptr0, *ptr1, *ptr2;

	for (i = 0; i < cmd.nargs; i++) {
		ptr0 = strdup(cmd.args[i]);
		if (ptr1 = strtok(cmd.args[i], ".")) {
			if (ptr2 = strtok(NULL, ".")) {
				size = kl_member_size(ptr1, ptr2);
			} 
			else {
				size = kl_struct_len(ptr1);
			}
		} 
		else {
			size = kl_struct_len(ptr1);
		}
		if (size) {
			fprintf(cmd.ofp, "Size of \"%s\": %d bytes\n", ptr0, size);
		} 
		else {
			fprintf(cmd.ofp, "%s: structure/field not found.\n", ptr0);
		}
		free(ptr0);
	}
	return(0);
}

#define _SIZEOF_USAGE "[-w outfile] structure[.field]"

/*
 * sizeof_usage() -- Print the usage string for the 'sizeof' command.
 */
void
sizeof_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SIZEOF_USAGE);
}

/*
 * sizeof_help() -- Print the help information for the 'sizeof' command.
 */
void
sizeof_help(command_t cmd)
{
	CMD_HELP(cmd, _SIZEOF_USAGE,
		"This command will dump out the size of a structure entered on "
		"the command line.  The value returned will be in bytes.  The user "
		"can also use a structure.field notation in order to find out what "
		"the size of a field in a structure is.  If the sizeof command is "
		"unable to find the structure or field, then it is not located in "
		"the symbol table.");
}

/*
 * sizeof_parse() -- Parse the command line arguments for 'sizeof'.
 */
int
sizeof_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
