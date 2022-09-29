#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_stack.c,v 1.15 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * stack_cmd() -- Run the 'stack' command 
 */
int
stack_cmd(command_t cmd)
{
	int cpuid = -1, stype = -1, ssize = 0, sindex;
	kaddr_t sp, saddr = 0;
	k_ptr_t pdap;
	kstruct_t *ksp;
	trace_t *trace;

	GET_VALUE(cmd.args[0], &sp);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_SADDR;
		kl_print_error();
		return(1);
	}
	if (PTRSZ32) {
		sp &= 0xffffffff;
	}

	trace = alloc_trace_rec();
	if (IS_KERNELSTACK(sp)) {
		if (K_EXTUSIZE && ((sp < K_KERNELSTACK) && (sp >= K_KEXTSTACK))) {
			saddr = K_KEXTSTACK;
			stype = S_KEXTSTACK;
			ssize = 2 * K_PAGESZ;
		}
		else {
			saddr = K_KERNELSTACK;
			stype = S_KERNELSTACK;
			ssize = K_PAGESZ;
		}
	}
	else {
		ksp = (kstruct_t *)kl_alloc_block(sizeof(kstruct_t), K_TEMP);
		for (cpuid = 0; cpuid < K_MAXCPUS; cpuid++) {
			pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
			kl_get_pda_s((kaddr_t)cpuid, pdap);
			ksp->addr = kl_pda_s_addr(cpuid);
			ksp->ptr = pdap;
			if (kl_is_cpu_stack(sp, pdap)) {
				setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
				if (KL_ERROR) {
					KL_ERROR |= KLE_BAD_SADDR;
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					return(0);
				}
				sindex = stack_index(sp, trace);
				saddr = trace->stack[sindex].addr;
				stype = trace->stack[sindex].type;
				ssize = trace->stack[sindex].size;
				break;
			}
		}
		if (cpuid == K_MAXCPUS) { 
			/* If we fell through to here, then the stack is not associated
			 * with one of the CPUs (either as a kthread stack running on
			 * a CPU or as a S_BOOTSTACK or S_INTSTACK). Check to see if 
			 * there is a defkthread set and if there is, check to see if 
			 * SP is from 
			 * that stack. 
			 */
			clean_trace_rec(trace);
			if (K_DEFKTHREAD) {
				setup_kthread_trace_rec(K_DEFKTHREAD, 2, trace);
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					return(0);
				}
				saddr = kthread_stack(trace, &ssize);
				if (KL_ERROR) {
					KL_ERROR |= KLE_BAD_SADDR;
					print_trace_error(trace, cmd.ofp);
					free_trace_rec(trace);
					return(0);
				}
				if ((sp >= saddr) && (sp < (saddr + ssize))) {

					if (IS_STHREAD(trace->ktp)) {
						stype = S_STHREADSTACK;
					}
				}
				else {
					saddr = 0;
				}
			}
		}
	}

	if (!saddr) {
		kl_virtop(sp, trace->ktp);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_SADDR;
		}
		else {
			KL_SET_ERROR_NVAL(KLE_BAD_SADDR, sp, 2);
		}
		print_trace_error(trace, cmd.ofp);
		return(1);
	}
	fprintf(cmd.ofp, "\n");
	fprintf(cmd.ofp, "0x%llx is from the ", sp);
	switch (stype) {
		case S_KERNELSTACK :
			fprintf(cmd.ofp, "KERNELSTACK.\n");
			break;

		case S_INTSTACK :
			fprintf(cmd.ofp, "INTSTACK for CPU %d.\n", cpuid);
			break;

		case S_BOOTSTACK :
			fprintf(cmd.ofp, "BOOTSTACK for CPU %d.\n", cpuid);
			break;

		case S_KEXTSTACK :
			fprintf(cmd.ofp, "KEXTSTACK.\n");
			break;

		case S_STHREADSTACK :
			fprintf(cmd.ofp, "stack for sthread 0x%llx.\n", trace->kthread);
			break;

		default :
			fprintf(cmd.ofp, "UNKNOWN STACK TYPE.\n");
			break;
	}
	fprintf(cmd.ofp, "\n");
	fprintf(cmd.ofp, "    STACK=0x%llx, SIZE=%d\n", saddr, ssize);
	free_trace_rec(trace);
	return(0);
}

#define _STACK_USAGE "[-w outfile] stack_pointer"
/*
 * stack_usage() -- Print the usage string for the 'stack' command.
 */
void
stack_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STACK_USAGE);
}

/*
 * stack_help() -- Print the help information for the 'stack' command.
 */
void
stack_help(command_t cmd)
{
	CMD_HELP(cmd, _STACK_USAGE,
		"Print out the stack type, stack address, and stack size "
		"for a given stack_pointer. If the stack address is from a "
		"kthread stack, then defslot must be set equal to the "
		"kthread slot number.");
}

/*
 * stack_parse() -- Parse the command line arguments for 'stack'.
 */
int
stack_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
