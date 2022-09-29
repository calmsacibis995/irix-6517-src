#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_memory.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * memory_cmd() -- Run the 'memory' command.  
 */
int
memory_cmd(command_t cmd)
{
	int i, mode, pages = 0, first_time = 1;
	kaddr_t value;

	if ((cmd.flags & C_LIST) && (ACTIVE(K) || UNCOMPRESSED(K))) {
		fprintf(cmd.ofp, "The list option is only valid with a compressed "
			"vmcore image.\n");
		return(1);
	}

	if (check_node_memory(K)) {
		fprintf(cmd.ofp, "System memory information is not available.\n");
		if (cmd.flags & C_LIST) {
			fprintf(cmd.ofp, "Doing a brute force memory search...\n\n");
			pages = list_dump_memory(cmd.ofp);
			fprintf(cmd.ofp, "\n");
			goto end;
		}
		return(1);
	}
	if (cmd.nargs) {
		for (i = 0; i < cmd.nargs; i++) {
			get_value(cmd.args[i], &mode, K_NPROCS(K), &value);
			if (KL_ERROR) {
				kl_print_error(K);
				continue;
			}
			if (first_time) {
				node_memory_banner(cmd.ofp, BANNER|SMAJOR);
				first_time = 0;
			}
			else {
				if (DEBUG(DC_GLOBAL, 1) || cmd.flags & (C_FULL|C_LIST)) {
					node_memory_banner(cmd.ofp, BANNER|SMAJOR);
				}
			}
			if (!kl_valid_nasid(K, value)) {
				fprintf(cmd.ofp, "%lld: invalid nasid\n", value);
				continue;
			}
			pages += print_node_memory(cmd.ofp, value, cmd.flags);
		}
		node_memory_banner(cmd.ofp, SMAJOR);
	}
	else {
		pages = list_system_memory(cmd.ofp, cmd.flags);
	}

end:
	if (cmd.flags & C_LIST) {
		if (pages == 1) {
			fprintf(cmd.ofp, "1 page found in vmdump.\n");
		}
		else {
			fprintf(cmd.ofp, "%d pages found in vmdump.\n", pages);
		}
	}
	return(0);
}

#define _MEMORY_USAGE "[-f] [-l] [-w outfile] [nasid_list]"

/*
 * memory_usage() -- Print the usage string for the 'memory' command.
 */
void
memory_usage(command_t cmd)
{
    CMD_USAGE(cmd, _MEMORY_USAGE);
}

/*
 * memory_help() -- Print the help information for the 'memory' command.
 */
void
memory_help(command_t cmd)
{
    CMD_HELP(cmd, _MEMORY_USAGE,
        "Display information about node memory for each nasid included "
		"in nasid_list. If no nasids are specified, display memory "
		"information for all nasids (nodes). If the memory command is "
		"issued with the -f option, display information for each node "
		"memory slot that actually contains memory. If the -l option is "
		"specified, then display a listing of all memory contained in "
		"the vmcore image.");
}

/*
 * memory_parse() -- Parse the command line arguments for 'memory'.
 */
int
memory_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_LIST);
}
