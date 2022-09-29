#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_tcpcb.c,v 1.15 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * tcpcb_cmd() -- Dump out tcpcb structure data, based on socket address.
 */
int
tcpcb_cmd(command_t cmd)
{
	int i, tcp_cnt = 0;
	kaddr_t tcp;
	k_ptr_t tcpp;

	/* This is here because the tcpcb struct info cannot currently be
	 * found in the symbol table information (Dwarf). So, if we do not
	 * allocate a block, then the rest of the command shouldn't be run
	 * (or we'll cause a core dump when we try to free the NULL block
	 * pointer.
	 */
	if (!(tcpp = kl_alloc_block(TCPCB_SIZE, K_TEMP))) {
		fprintf(cmd.ofp, "unable to allocate block of %d size\n", TCPCB_SIZE);
		return(1);
	}

	tcpcb_banner(cmd.ofp, BANNER|SMAJOR);
	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &tcp);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_TCPCB;
			kl_print_error();
			continue;
		}
		kl_get_struct(tcp, TCPCB_SIZE, tcpp, "tcpcb");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_TCPCB;
			kl_print_error();
		}
		else {
			print_tcpcb(tcp, tcpp, cmd.flags, cmd.ofp);
			tcp_cnt++;
		}
	}
	tcpcb_banner(cmd.ofp, SMAJOR);
	PLURAL("tcpcb struct", tcp_cnt, cmd.ofp);
	kl_free_block(tcpp);
	return(0);
}

#define _TCPCB_USAGE "[-f] [-w outfile] tcpcb_list"

/*
 * tcpcb_usage() -- Print the usage string for the 'tcpcb' command.
 */
void
tcpcb_usage(command_t cmd)
{
	CMD_USAGE(cmd, _TCPCB_USAGE);
}

/*
 * tcpcb_help() -- Print the help information for the 'tcpcb' command.
 */
void
tcpcb_help(command_t cmd)
{
	CMD_HELP(cmd, _TCPCB_USAGE,
		"Display the tcpcb structure for each virtual address included "
		"in tcpcb_list.");
}

/*
 * tcpcb_parse() -- Parse the command line arguments for 'tcpcb'.
 */
int
tcpcb_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
