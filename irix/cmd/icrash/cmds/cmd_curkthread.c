#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_curkthread.c,v 1.18 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * curkthread_cmd() -- Display the current kthread running on the processor(s).
 */
int
curkthread_cmd(command_t cmd)
{
	int i, slot;
	kaddr_t curkthread, proc;
	k_ptr_t procp, ktp;

	for (i = 0; i < K_MAXCPUS; i++) {

		if (K_MAXCPUS > 1) {
			fprintf(cmd.ofp, "CPU%d: ", i);
		}
		if (curkthread = kl_get_curkthread(i)) {
			ktp = kl_get_kthread(curkthread, cmd.flags);
			if (KL_ERROR) {
				fprintf(cmd.ofp, "CURKTHREAD = ERROR (0x%llx)\n\n", KL_ERROR);
				continue;
			}
		}
		fprintf(cmd.ofp, "CURKTHREAD = 0x%llx", curkthread);
		if (curkthread) {
			if (IS_STHREAD(ktp)) {
				fprintf(cmd.ofp, " (STHREAD)\n\n");
			}
			else if (IS_UTHREAD(ktp)) {
				fprintf(cmd.ofp, " (UTHREAD)\n\n");
			}
			else if (IS_XTHREAD(ktp)) {
				fprintf(cmd.ofp, " (XTHREAD)\n\n");
			}

			/* Now print out the kthread. Note that the uthread_s/proc,
			 * sthread_s, and xthread_s information will also be 
			 * displayed when the C_NEXT flag is set.
			 */
			kthread_banner(cmd.ofp, BANNER|SMAJOR);
			print_kthread(curkthread, ktp, cmd.flags, cmd.ofp);
			if (cmd.flags & C_FULL) {
				kthread_banner(cmd.ofp, SMAJOR);
			}
			kl_free_block(ktp);
		}
		else {
			fprintf(cmd.ofp, "\n");
		}
		if (i < K_MAXCPUS - 1) {
			fprintf(cmd.ofp, "\n");
		}
	}
	return(0);
}

#define _CURPROC_USAGE "[-f] [-n] [-w outfile]"

/*
 * curkthread_usage() -- Print the usage string for the 'curkthread' command.
 */
void
curkthread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _CURPROC_USAGE);
}

/*
 * curkthread_help() -- Print help information for the 'curkthread' command.
 */
void
curkthread_help(command_t cmd)
{
	CMD_HELP(cmd, _CURPROC_USAGE,
		"Display the proc table entry for the currently running process "
		"(or processes when there are multiple CPUs).");
}

/*
 * curkthread_parse() -- Parse the command line arguments for 'curkthread'.
 */
int
curkthread_parse(command_t cmd)
{
	return (C_FALSE|C_FULL|C_NEXT|C_WRITE);
}
