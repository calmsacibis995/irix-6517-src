#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_strace.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/* 
 * strace_cmd() -- Command for building stack traces. It either builds
 *                 all possible stack traces (with or without errors)
 *                 contained in a given a stack.  Or, it builds a specific
 *                 backtrace -- using PC, SP and STACK addresses.
 */
int
strace_cmd(command_t cmd)
{
	int i; 
	cpuid_t cpuid;
	k_uint_t level = 5;
	kaddr_t addr, pc, sp, saddr, s;
	k_ptr_t ktp;
	kstruct_t *ksp;
	trace_t *trace;

	trace = alloc_trace_rec();
	ksp = (kstruct_t*)alloc_block(sizeof(kstruct_t), B_TEMP);

	if (cmd.nargs != 3) {

		GET_VALUE(cmd.args[0], &saddr);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_SADDR;
			kl_print_error(K);
			free_trace_rec(trace);
			free_block((k_ptr_t)ksp);
			return(1);
		}

		if (IS_KERNELSTACK(K, saddr)) {
			if (K_DEFKTHREAD(K)) {
				setup_kthread_trace_rec(K_DEFKTHREAD(K), 2, trace);
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					free_block((k_ptr_t)ksp);
					return(1);
				}
			}
			else {
				KL_SET_ERROR(KLE_NO_DEFKTHREAD);
				print_trace_error(trace, cmd.ofp);
				free_trace_rec(trace);
				free_block((k_ptr_t)ksp);
				return(1);
			}
		}
		else {

			k_ptr_t pdap;

			/* Check to see if SP is from the intstack/bootstack of one 
			 * of the cpus.
			 */
			pdap = kl_sp_to_pdap(K, saddr, (k_ptr_t)NULL);
			if (KL_ERROR) {
				KL_ERROR = (KL_ERROR & (k_uint_t)0xffffffff) | KLE_BAD_SADDR;
				print_trace_error(trace, cmd.ofp);
				return(1);
			}

			if (pdap) {
				ksp->ptr = pdap;
				cpuid = KL_INT(K, pdap, "pda_s", "p_cpuid");
				ksp->addr = kl_pda_s_addr(K, cpuid);
				setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
				if (saddr != stack_addr(saddr, trace)) {
					KL_SET_ERROR_NVAL(KLE_BAD_SADDR, saddr, 2);
					print_trace_error(trace, cmd.ofp);
					return(1);
				}
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					free_block((k_ptr_t)ksp);
					return(1);
				}
			}
			else {

				/* Check to see if SP is from the kthread stack for 
				 * defkthread. If it's not, then check to see if it is 
				 * a kthread stack (XXX -- this is a quick hack! It
				 * really needs to be reworked.)
				 */
				if (K_DEFKTHREAD(K)) {
					setup_kthread_trace_rec(K_DEFKTHREAD(K), 2, trace);
					if (KL_ERROR) {
						print_trace_error(trace, cmd.ofp);
						free_trace_rec(trace);
						free_block((k_ptr_t)ksp);
						return(1);
					}
				}
				else {
					/* XXX -- TODO
					 *
					 * We don't know kind of stack this is. We can check
					 * to see if it is a kthread stack for one of the active
					 * kthreads. Also, we can check to see if saddr mapps
					 * to one of the running processes kernel stacks. As a last
					 * resort, we can setup a RAW_STACK trace record and try
					 * a brute force search for stack traces. For now, we will
					 * just return an error.
					 */	
					KL_SET_ERROR(KLE_NOT_IMPLEMENTED);
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					free_block((k_ptr_t)ksp);
					return(1);
				}
			}
		}
		
		/* Make sure the stack address is valid. It's possible, with
		 * 32-bit systems, for the stack address to be valid and NOT
		 * be equal to saddr. This would be the case when the saddr
		 * is equal to the kextstack, but the kextstack is not currently
		 * mapped in. In such a case, the stack address is actually equal
		 * to the kernelstack.
		 */
		s = stack_addr(saddr, trace);
		if ((s != saddr) && (s != K_KERNELSTACK(K))) {
			KL_SET_ERROR_NVAL(KLE_BAD_SADDR, saddr, 2);
			print_trace_error(trace, cmd.ofp);
			return(1);
		}
		if (cmd.flags & C_LIST) {
			do_list(s, trace, cmd.ofp);
		}
		else {
			if (cmd.nargs == 2) {
				GET_VALUE(cmd.args[1], &level);
				if (KL_ERROR) {
					kl_print_error(K);
					free_trace_rec(trace);
					free_block((k_ptr_t)ksp);
					return(1);
				}
			}
			print_valid_traces(s, trace, level, cmd.flags, cmd.ofp);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
			}
		}
		free_trace_rec(trace);
		free_block((k_ptr_t)ksp);
		return(0);
	}

	GET_VALUE(cmd.args[0], &pc);
	if (KL_ERROR) {
		KL_ERROR = KLE_BAD_PC;
		kl_print_error(K);
		free_trace_rec(trace);
		free_block((k_ptr_t)ksp);
		return(1);
	}
	GET_VALUE(cmd.args[1], &sp);
	if (KL_ERROR) {
		KL_ERROR = KLE_BAD_SP;
		kl_print_error(K);
		free_trace_rec(trace);
		free_block((k_ptr_t)ksp);
		return(1);
	}
	GET_VALUE(cmd.args[2], &saddr);
	if (KL_ERROR) {
		KL_ERROR = KLE_BAD_SADDR;
		kl_print_error(K);
		free_trace_rec(trace);
		free_block((k_ptr_t)ksp);
		return(1);
	}

	if (IS_KERNELSTACK(K, saddr)) {
		if (!K_DEFKTHREAD(K)) {
			KL_SET_ERROR(KLE_NO_DEFKTHREAD);
			kl_print_error(K);
			free_trace_rec(trace);
			free_block((k_ptr_t)ksp);
			return(1);
		}
		else {
			setup_kthread_trace_rec(K_DEFKTHREAD(K), 2, trace);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				free_trace_rec(trace);
				free_block((k_ptr_t)ksp);
				return(1);
			}
		}
	}
	else {

		/* Check to see if SP is from the INTSTACK or BOOTSTACK from 
		 * one of the CPUs. If it's not, then check to see if it is 
	 	 * a kthread stack (XXX -- this is a quick hack! It
		 * really needs to be reworked.)
		 */

		k_ptr_t pdap;

		pdap = kl_sp_to_pdap(K, saddr, (k_ptr_t)NULL);
		if (KL_ERROR) {
			print_trace_error(trace, cmd.ofp);
			free_trace_rec(trace);
			return(1);
		}

		if (pdap) {
			cpuid = KL_INT(K, pdap, "pda_s", "p_cpuid");
			ksp->addr = kl_pda_s_addr(K, cpuid);
			ksp->ptr = pdap;
			setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				free_trace_rec(trace);
				free_block((k_ptr_t)ksp);
				return(1);
			}
		}
		else {
			if (K_DEFKTHREAD(K)) {
				setup_kthread_trace_rec(K_DEFKTHREAD(K), 2, trace);
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					free_block((k_ptr_t)ksp);
					return(1);
				}
			}
			else {
				KL_SET_ERROR_NVAL(KLE_BAD_SP, saddr, 2);
				kl_print_error(K);
				free_trace_rec(trace);
				free_block((k_ptr_t)ksp);
				return(1);
			}
		}
	}

	/* Make sure the stack address is valid
	 */
	if (saddr != stack_addr(saddr, trace)) {
		KL_SET_ERROR_NVAL(KLE_BAD_SADDR, saddr, 2);
		print_trace_error(trace, cmd.ofp);
		return(1);
	}

	if (find_trace(pc, sp, (kaddr_t)0, (kaddr_t)0, 
			trace, (cmd.flags | C_ALL))) {
		fprintf(cmd.ofp, "================================================="
				"===============\n");
		print_trace(trace, cmd.flags, cmd.ofp);
		fprintf(cmd.ofp, "================================================="
				"===============\n");
	}
	else {
		if (!KL_ERROR) {
			KL_SET_ERROR(E_NO_TRACE);
		}
		print_trace_error(trace, cmd.ofp);
	}
	free_trace_rec(trace);
	free_block((k_ptr_t)ksp);
	return(0);
}

#define _STRACE_USAGE "[-a] [-l] [-f] [-w outfile] [pc] [sp] stack_addr [level]"

/*
 * strace_usage() -- Print the usage string for the 'strace' command.
 */
void
strace_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STRACE_USAGE);
}

/*
 * strace_help() -- Print the help information for the 'strace' command.
 */
void
strace_help(command_t cmd)
{
	CMD_HELP(cmd, _STRACE_USAGE,
		"Displays all complete and unique stack traces (containing level or "
		"more stack frames) from the stack starting at stack_addr. If a "
		"level isn't specified, then each stack trace must have at least "
		"five frames to be considered valid. Alternately, use a specific PC "
		"and SP to generate a stack trace from the stack starting at "
		"stack_addr. Or, when the -l command line option is specified, "
		"displays a list of all valid kernel code addresses contained in "
		"the stack starting at stack_addr, along with their location in the "
		"stack, source file and line number. Or, if the -a option is "
		"specified, display ALL traces of level or more frames, including "
		"invalid traces and duplicate (sub) traces. Note that defkthread must "
		"be set equal to the process being analyzed when stack_addr is "
		"the kernel stack address or the CPU interrupt stack "
		"and there is a process running on that CPU. This is necessary so "
		"that the trace can automatically \"jump\" from the first stack to "
		"the kernel stack of the process being analyzed.");
}

/*
 * strace_parse() -- Parse the command line arguments for 'strace'.
 */
int
strace_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_LIST|C_ALL);
}
