#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_ctrace.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/*
 * ctrace_banner()
 */
void
ctrace_banner(FILE *ofp, int flags)
{
	if (flags & SMAJOR) {
		fprintf(ofp, "============================================"
			"========================\n");
	}
}

/* 
 * ctrace_cmd() -- Generates a stack trace for a given CPU.
 */
int 
ctrace_cmd(command_t cmd)
{
	int i, mode;
	cpuid_t cpuid;
	kaddr_t eframe;
	k_ptr_t pdap, eframep;
	k_uint_t value;
	kstruct_t *ksp;
	trace_t *trace;

	/* If icrash is being run on a live system, we cannot run
	 * the ctrace command (too fast of a moving target).
	 */
	if (ACTIVE(K)) {
		fprintf(cmd.ofp, "The ctrace command will not work on a live system\n");
		return(1);
	}

	trace = alloc_trace_rec();
	ksp = (kstruct_t*)alloc_block(sizeof(kstruct_t), B_TEMP);

	for (i = 0; i < cmd.nargs; i++) {

		ctrace_banner(cmd.ofp, SMAJOR);

		get_value(cmd.args[i], &mode, K_MAXCPUS(K), &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_CPUID;
			kl_print_error(K);
			clean_trace_rec(trace);
			continue;
		}
		else {
			cpuid = (cpuid_t)value;
			if ((cpuid < 0) || (cpuid >= K_MAXCPUS(K))) {
				KL_SET_ERROR_NVAL(KLE_BAD_CPUID, cpuid, mode);
				print_trace_error(trace, cmd.ofp);
				clean_trace_rec(trace);
				continue;
			}
		}

		/* Allocate block for pda_s sturct here. Once we succeed in getting
		 * a pda_s struct loaded, we don't have to worry about freeing the
		 * memory block -- since clean_trace_rec() and free_trace_rec() take 
		 * care of this for us.
		 */
		pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
		if (!kl_get_pda_s(K, (kaddr_t)cpuid, pdap)) {
			print_trace_error(trace, cmd.ofp);
			free_block(pdap);
			clean_trace_rec(trace);
			continue;
		}

		ksp->addr = kl_pda_s_addr(K, cpuid);
		ksp->ptr = pdap;
		setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
		if (KL_ERROR) {
			print_trace_error(trace, cmd.ofp);
			clean_trace_rec(trace);
			continue;
		}

		fprintf (cmd.ofp, "STACK TRACE FOR CPU %d\n\n", cpuid);
		get_cpu_trace(cpuid, trace);
		if ((K_IP(K) == 27) && IS_NMI(K)) {
			eframep = kl_NMI_eframe(K, cpuid, &eframe);
			fprintf(cmd.ofp, "NMI EXCEPTION FRAME at 0x%llx:\n\n", eframe);
			print_eframe_s((kaddr_t)NULL, eframep, cmd.flags, cmd.ofp);
			free_block(eframep);
		}
		if (KL_ERROR && !trace->nframes) {
			print_trace_error(trace, cmd.ofp);
			clean_trace_rec(trace);
			continue;
		}
		print_trace(trace, cmd.flags, cmd.ofp);
		clean_trace_rec(trace);
	}

	if (cmd.nargs == 0) {
		if (cmd.flags & C_ALL) {
			for (i = 0; i < K_MAXCPUS(K); i++) {

				/* Allocate block for pda_s sturct here. Once we succeed 
				 * in getting a pda_s struct loaded, we don't have to worry 
				 * about freeing the memory block -- since clean_trace_rec() 
				 * and free_trace_rec() take care of this for us.
				 */
				pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
				if (!kl_get_pda_s(K, (kaddr_t)i, pdap)) {
					free_block(pdap);
					clean_trace_rec(trace);
					continue;
				}

				ctrace_banner(cmd.ofp, SMAJOR);

				ksp->addr = kl_pda_s_addr(K, i);
				ksp->ptr = pdap;
				setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					clean_trace_rec(trace);
					continue;
				}
				fprintf (cmd.ofp, "STACK TRACE FOR CPU %d\n\n", i);
				get_cpu_trace(i, trace);
				if ((K_IP(K) == 27) && IS_NMI(K)) {
					eframep = kl_NMI_eframe(K, i, &eframe);
					fprintf(cmd.ofp, "NMI EXCEPTION FRAME at 0x%llx:\n\n", 
						eframe);
					print_eframe_s((kaddr_t)NULL, eframep, cmd.flags, cmd.ofp);
					free_block(eframep);
				}
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					clean_trace_rec(trace);
					continue;
				}
				print_trace(trace, cmd.flags, cmd.ofp);
				clean_trace_rec(trace);
			}
		}
		else {
			/* Allocate block for pda_s sturct here. Once we succeed 
			 * in getting a pda_s struct loaded, we don't have to worry 
			 * about freeing the memory block -- since clean_trace_rec() 
			 * and free_trace_rec() take care of this for us.
			 */
			pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
			cpuid = kl_get_curcpu(K, pdap);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				free_block((k_ptr_t)ksp);
				free_block(pdap);
				free_trace_rec(trace);
				return(0);
			} 
			else {
				ksp->addr = kl_pda_s_addr(K, cpuid);
				ksp->ptr = pdap;
				setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					free_block((k_ptr_t)ksp);
					free_trace_rec(trace);
					return(0);
				} 
				else {
					ctrace_banner(cmd.ofp, SMAJOR);
					fprintf (cmd.ofp, "STACK TRACE FOR CPU %d\n\n", cpuid);
					get_cpu_trace(cpuid, trace);
					if (IS_NMI(K)) {
						eframep = kl_NMI_eframe(K, cpuid, &eframe);
						fprintf(cmd.ofp, "NMI EXCEPTION FRAME at 0x%llx:\n\n", 
							eframe);
						print_eframe_s((kaddr_t)NULL, eframep, 
							cmd.flags, cmd.ofp);
						free_block(eframep);
					}
					if (KL_ERROR && !trace->frame) {
						print_trace_error(trace, cmd.ofp);
					} 
					else {
						print_trace(trace, cmd.flags, cmd.ofp);
					}
				}
			}
		}
	}
	ctrace_banner(cmd.ofp, SMAJOR);
	free_block((k_ptr_t)ksp);
	free_trace_rec(trace);
	return(0);
}

/*
 * ctrace_parse() -- Parse command line options for 'ctrace' command.
 */
int
ctrace_parse(command_t cmd)
{
	return (C_MAYBE|C_ALL|C_FULL|C_WRITE);
}

#define _CTRACE_USAGE "[-a] [-f] [-w outfile] [cpu_list]"

/*
 * ctrace_help() -- Print help information for the 'ctrace' command.
 */
void
ctrace_help(command_t cmd)
{
	CMD_HELP(cmd, _CTRACE_USAGE,
		"Displays a stack trace for each CPU included in cpu_list. If "
		"cpu_list is empty and defslot is set, ctrace displays a stack "
		"trace for the cpu that defslot was running on. If defslot is not "
		"set, ctrace displays a trace for the cpu dumpproc was running on. "
		"If there isn't any dumpproc, ctrace displays a stack trace for the "
		"cpu that initiated the dump (based on the SP saved in dumpregs). "
		"Or, if the -a option is included in the command line, ctrace "
		"displays a stack trace for each cpu in the system.");
}

/*
 * ctrace_usage() -- Print usage information for the 'ctrace' command.
 */
void
ctrace_usage(command_t cmd)
{
	CMD_USAGE(cmd, _CTRACE_USAGE);
}
