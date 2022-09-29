#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_etrace.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/* 
 * etrace_cmd() -- Tries to generate a stack trace using the contents of an
 *                 exception frame.
 */
int
etrace_cmd(command_t cmd)
{
	trace_t *trace;
	kaddr_t ef;

	trace = alloc_trace_rec();

	/* get the eframe pointer and make sure it's a valid kernel 
	 * address 
	 */
	GET_VALUE(cmd.args[0], &ef);
	if (!KL_ERROR) {
		kl_virtop(K, ef, (k_ptr_t)NULL);
	}
	if (KL_ERROR && (KL_ERROR != KLE_NO_DEFKTHREAD)) {
		KL_ERROR |= KLE_BAD_EFRAME_S;
		print_trace_error(trace, cmd.ofp);
		free_trace_rec(trace);
		return(1);
	}

	/* Get the trace and print it out. Note that print_trace_error()
	 * will be initiated from within eframe_trace() if an error occurs.
	 */
	eframe_trace(ef, trace, cmd.flags, cmd.ofp);
	free_trace_rec(trace);
	return(0);
}

#define _ETRACE_USAGE "[-f] [-w outfile] eframe_addr"

/*
 * etrace_usage() -- Print the usage string for the 'etrace' command.
 */
void
etrace_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ETRACE_USAGE);
}

/*
 * etrace_help() -- Print the help information for the 'etrace' command.
 */
void
etrace_help(command_t cmd)
{
	CMD_HELP(cmd, _ETRACE_USAGE,
		"Display a stack trace using the PC and SP found in the exception "
		"frame pointed to by eframe_addr. The stack address is determined "
		"using the stack pointer from the exception frame. Note that "
		"defproc must be set equal to the proper proc slot when stack_addr "
		"is the address of the kernel stack or when stack_addr is the CPU "
		"interrupt stack and there is a process running on that CPU.");
}

/*
 * etrace_parse() -- Parse the command line arguments for 'etrace'.
 */
int
etrace_parse(command_t cmd)
{
	return (C_TRUE|C_FULL|C_WRITE);
}
