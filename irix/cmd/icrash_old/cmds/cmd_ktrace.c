#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_ktrace.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/*
 * ktrace_cmd() -- Generates a stack trace using a kthread as a starting point
 */
int
ktrace_cmd(command_t cmd)
{
	int i, mode;
	kaddr_t value;
	k_ptr_t ktp;
	kstruct_t ksp;
	trace_t *trace;

	trace = alloc_trace_rec();

	for (i = 0; i < cmd.nargs; i++) {

		fprintf(cmd.ofp, "=========================================="
			"======================\n");

		get_value(cmd.args[i], &mode, 0, &value);
		if (KL_ERROR) {
			print_trace_error(trace, cmd.ofp);
			clean_trace_rec(trace);
			continue;
		}

		ktp = kl_get_kthread(K, value, 0);
		if (KL_ERROR) {
			print_trace_error(trace, cmd.ofp);
			clean_trace_rec(trace);
			continue;
		}

		ksp.ptr = ktp;
		ksp.addr = value;

		setup_trace_rec((kaddr_t)0, &ksp, KTHREAD_FLAG, trace);
		if (KL_ERROR) {
			print_trace_error(trace, cmd.ofp);
			clean_trace_rec(trace);
			continue;
		}

		kthread_trace(trace, cmd.flags, cmd.ofp);
		if (KL_ERROR) {
			print_trace_error(trace, cmd.ofp);
		}
		clean_trace_rec(trace);
	}
	free_trace_rec(trace);
	fprintf(cmd.ofp, "================================================="
		"===============\n");
	return(0);
}

#define _KTRACE_USAGE "[-a] [-f] [-w outfile] [kthread_list]"

/*
 * ktrace_usage() -- Print the usage string for the 'ktrace' command.
 */
void
ktrace_usage(command_t cmd)
{
	CMD_USAGE(cmd, _KTRACE_USAGE);
}

/*
 * ktrace_help() -- Print the help information for the 'ktrace' command.
 */
void
ktrace_help(command_t cmd)
{
	CMD_HELP(cmd, _KTRACE_USAGE,
        "Displays a stack trace for each kthread pointer included in "
		"kthread_list.");
}

/*
 * ktrace_parse() -- Parse the command line arguments for 'ktrace'.
 */
int
ktrace_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
