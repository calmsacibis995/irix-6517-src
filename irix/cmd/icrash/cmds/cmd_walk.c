#ident  "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * walk_cmd() -- walk linked lists of kernel data structures 
 */
int
walk_cmd(command_t cmd)
{
	int count = 0;
	k_uint_t size, byte_size;
	unsigned offset;
	kaddr_t value, start_addr, last_addr, next_addr;

	if (cmd.flags & C_LIST) {
		structlist(cmd.ofp);
		return(0);
	}

	/* Make sure we have the right number of paramaters
	 */
	if (cmd.nargs != 3) {
		walk_usage(cmd);
		return(1);
	}

	/* Determine if the first paramater is an address or the name of
	 * a struct. If it is an address, then do a hex dump of a size block
	 * of memory and walk to the next block using the offset value.
	 */
	if (kl_struct_len(cmd.args[0]) == 0) {
		GET_VALUE(cmd.args[0], &start_addr);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}
		GET_VALUE(cmd.args[1], &value);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}
		offset = value;

		GET_VALUE(cmd.args[2], &value);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}

		/* Set byte_size and size (number of nbpw units)
		 */
		byte_size = value;
		if (byte_size % K_NBPW) {
			byte_size += (K_NBPW - (byte_size % K_NBPW));
		}
		size = (byte_size / K_NBPW);

		next_addr = start_addr;
		while (next_addr) {

			/* Make sure the address is valid
			 */
			kl_is_valid_kaddr(next_addr, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
			if (KL_ERROR) {
				break;
			}

			fprintf(cmd.ofp, "Dumping %lld byte block at 0x%llx:\n\n", 
					byte_size, next_addr);
			dump_memory(next_addr, size, 0, cmd.ofp);
			count++;
			fprintf(cmd.ofp, "\n");

			/* Get the next pointer (if any) at offset.
			 */
			last_addr = next_addr;
			kl_get_kaddr((next_addr + offset), (k_ptr_t)&next_addr, "addr"); 
			if (KL_ERROR) {
				break;
			}

			/* Make sure we haven't looped around to the start of our
			 * list
			 */
			if ((next_addr == start_addr) || (next_addr == last_addr)) {
				break;
			}
		}
		if (count == 1) {
			fprintf(cmd.ofp, "1 block found\n");
		}
		else {
			fprintf(cmd.ofp, "%d blocks in linked list\n", count);
		}
		return(0);
	}

	/* Determine if a struct offset or field name has been entered
	 */
	GET_VALUE(cmd.args[1], &value);
	if (KL_ERROR) {
		offset = -1;
	}
	else {
		offset = value;
	}

	/* Get the start address of the structure
	 */
	GET_VALUE(cmd.args[2], &start_addr);
	if (KL_ERROR) {
		kl_print_error();
		return(1);
	}

	walk_structs(cmd.args[0], cmd.args[1], offset, 
				start_addr, cmd.flags, cmd.ofp);
	return(0);
}

#define _WALK_USAGE "[-s] [-l] [-f] [-w outfile] [struct field|offset addr] [address offset size] [structname]"

/*
 * walk_usage() -- Walk a linked list of structures
 */
void
walk_usage(command_t cmd)
{
	CMD_USAGE(cmd, _WALK_USAGE);
}

/*
 * walk_help() -- Print the help information for the 'walk' command.
 */
void
walk_help(command_t cmd)
{
	CMD_HELP(cmd, _WALK_USAGE, 
		"Walk a linked list of kernel structures or memory blocks. The walk "
		"command has three modes of operation. By default, output from the "
		"walk command consists of a linked list of formatted structure "
		"entries. Each entry contains a single line of output, similar "
		"to the output of other icrash commands. The list of structures "
		"which can be displayed in this manner is limited. To see "
		"a listing, issue the walk (or struct) command with the -l "
		"command line option. Note that when viewing the list of structures, "
		"only those structures marked \"YES\" in the LINKS column "
		"contain links that can be followed. When the walk command is "
		"issued with the -s option, each structure is displayed, in "
		"its entirity, in a C-like format. The only limitation on "
		"which structures can be walked in this manner is that structure "
		"related information must be contained in the kernel's symbol "
		"table. Even if information about a structure is not available, "
		"it is possible to do a hex memory dump of each structure in "
		"the list.\n\n"

		"With the first two options outlined above, the structure name,"
		"field name (or byte offset), and next pointer are required. With "
		"the third option, a start address, byte offset, and struct size "
		"are required.");
}

/*
 * walk_parse() -- Parse the command line arguments for 'walk'.
 */
int
walk_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_LIST|C_STRUCT);
}
