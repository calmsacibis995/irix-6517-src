#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_socket.c,v 1.17 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * socket_cmd() -- Dump out socket information.
 */
int
socket_cmd(command_t cmd)
{
	int i, soc_cnt = 0;
	kaddr_t soc = 0;
	k_ptr_t sp;

	sp = kl_alloc_block(SOCKET_SIZE, K_TEMP);
	socket_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &soc);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_SOCKET;
			kl_print_error();
			continue;
		}
		get_socket(soc, sp, (cmd.flags|C_ALL));
		if (KL_ERROR) {
			kl_print_error();
		}
		else {
			print_socket(soc, sp, cmd.flags, cmd.ofp);
			soc_cnt++;
			if ((cmd.flags & (C_FULL|C_NEXT)) && (i < (cmd.nargs - 1))) {
				socket_banner(cmd.ofp, BANNER|SMAJOR);
			}
		}
	}
	if (!cmd.nargs) {
		soc_cnt = list_sockets(cmd.flags, cmd.ofp);
	}
	socket_banner(cmd.ofp, SMAJOR);
	PLURAL("socket struct", soc_cnt, cmd.ofp);
	kl_free_block(sp);
	return(0);
}

#define _SOCKET_USAGE "[-f] [-n] [-w outfile] [socket_list]"

/*
 * socket_usage() -- Print the usage string for the 'socket' command.
 */
void
socket_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SOCKET_USAGE);
}

/*
 * socket_help() -- Print the help information for the 'socket' command.
 */
void
socket_help(command_t cmd)
{
	CMD_HELP(cmd, _SOCKET_USAGE,
		"Display the socket structure for each virtual address included "
		"in socket_list.  If no entries are specified, display all "
		"sockets that are currently allocated.  If the next option (-n) "
		"is specified, a linked list of protocol control block "
		"structures associated with each socket will also be displayed.");
}

/*
 * socket_parse() -- Parse the command line arguments for 'socket'.
 */
int
socket_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_NEXT);
}
