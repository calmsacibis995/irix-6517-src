#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_trace.c,v 1.28 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * trace_cmd() -- This is the basic trace command.
 */
int
trace_cmd(command_t cmd)
{
	int i, mode, uthread_cnt, trace_cnt;
	kaddr_t addr, value, proc, uthread, sthread, xthread;
	kaddr_t saved_kthread = 0;
	k_ptr_t p, ktp, procp, itp, stp, xtp;
	kstruct_t *ksp;
	trace_t *trace;

	trace = alloc_trace_rec();
	ksp = (kstruct_t*)kl_alloc_block(sizeof(kstruct_t), K_TEMP);

	/* Make sure we save the current defkthread (even if it is NULL)
	 * so that we can restore it when we are done (if we modify
	 * it at any time).
	 */
	saved_kthread = K_DEFKTHREAD;

	if (cmd.nargs == 0) {
		if (cmd.flags & C_ALL) {

			int cpuid;
			kaddr_t pid, vproc, proc, bhv;
			k_ptr_t pidp, vprocp;

			fprintf(cmd.ofp, "\nUTHREAD STACK TRACES:\n\n");
			pidp = kl_alloc_block(PID_ENTRY_SIZE, K_TEMP);
			vprocp = kl_alloc_block(VPROC_SIZE, K_TEMP);
			pid = K_PID_ACTIVE_LIST;

			/* We have to step over the first entry as it is just
			 * the list head.
			 */
			kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");

			trace_cnt = 0;
			do {
				kl_get_struct(pid, PID_ENTRY_SIZE, pidp, "pid_entry");
				vproc = kl_kaddr(pidp, "pid_entry", "pe_vproc");
				kl_get_struct(vproc, VPROC_SIZE, vprocp, "vproc");
				bhv = kl_kaddr(vprocp, "vproc", "vp_bhvh");
				proc = kl_bhv_pdata(bhv);
				if (KL_ERROR || (!proc)) {
					kl_print_error();
					clean_trace_rec(trace);
					kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
					continue;
				}
				procp = kl_get_proc(proc, 2, cmd.flags);

				/* We have to convert the proc to a uthread_s/kthread.
				 */
				uthread = kl_proc_to_kthread(procp, &uthread_cnt);

				/* Check to see if uthread is NULL. If it is then check
				 * to see if the process is defunct (in a zombie state).
				 * We can't generate a stack trace for a zombie process.
				 */
				if (!uthread) {
					if (KL_UINT(procp, "proc", "p_stat") == 2) {
						KL_SET_ERROR_NVAL(KLE_DEFUNCT_PROCESS, proc, 2);
					}
					else {
						KL_SET_ERROR_NVAL(KLE_UNKNOWN_ERROR, proc, 2);
					}
					kl_free_block(procp);
					trace_banner(cmd.ofp);
					print_trace_error(trace, cmd.ofp);
					trace_cnt++;
					clean_trace_rec(trace);
					kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
					continue;
				}
				kl_free_block(procp);

				/* Generate a stack trace for each uthread associated with
				 * this proc/PID.
				 */
				for (i = 0; i < uthread_cnt; i++) {
					ktp = kl_get_uthread_s(uthread, 2, 0);
					if (KL_ERROR) {
						trace_banner(cmd.ofp);
						kl_print_error();
						break;
					}
					ksp->addr = uthread;
					ksp->ptr = ktp;
					setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
					if (KL_ERROR) {
						trace_banner(cmd.ofp);
						print_trace_error(trace, cmd.ofp);
						clean_trace_rec(trace);
						break;
					}
					trace_banner(cmd.ofp);
					kthread_trace(trace, cmd.flags, cmd.ofp);
					trace_cnt++;
					clean_trace_rec(trace);
					uthread = kl_kaddr(ktp, "uthread_s", "ut_next");
				}
				kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
			} while (pid != K_PID_ACTIVE_LIST);
			kl_free_block(pidp);
			kl_free_block(vprocp);
			if (trace_cnt) {
				trace_banner(cmd.ofp);
				PLURAL("uthread trace", trace_cnt, cmd.ofp);
				fprintf(cmd.ofp, "\n");
			}
			else {
				fprintf(cmd.ofp, 
					"NOTICE: NO VALID UTHREAD TRACES FOUND\n\n");
			}

			fprintf(cmd.ofp, "\nSTHREAD STACK TRACES:\n\n");
			stp = kl_alloc_block(STHREAD_S_SIZE, K_TEMP);
			kl_get_struct(K_STHREADLIST, 
				STHREAD_S_SIZE, stp, "sthread_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_STHREAD_S;
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug("trace_cmd");
				}
			}
			else {
				sthread = kl_kaddr(stp, "sthread_s", "st_next");
				kl_free_block(stp);
				trace_cnt = 0;
				while(sthread != K_STHREADLIST) {
					stp = kl_get_sthread_s(sthread, 2, 0);
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_STHREAD_S;
						kl_print_error();
						break;
					}
					else {
						ksp->addr = sthread;
						ksp->ptr = stp;
						setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
						if (KL_ERROR) {
							if (DEBUG(DC_TRACE, 1)) {
								print_trace_error(trace, cmd.ofp);
							}
							clean_trace_rec(trace);
							continue;
						}
						trace_banner(cmd.ofp);
						kthread_trace(trace, cmd.flags, cmd.ofp);
						trace_cnt++;
						if (KL_ERROR && !trace->nframes) {
							print_trace_error(trace, cmd.ofp);
						}
						sthread = kl_kaddr(stp, "sthread_s", "st_next");
						clean_trace_rec(trace);
					}
				}
				if (trace_cnt) {
					trace_banner(cmd.ofp);
					PLURAL("sthread trace", trace_cnt, cmd.ofp);
					fprintf(cmd.ofp, "\n");
				}
				else {
					fprintf(cmd.ofp, 
						"NOTICE: NO VALID STHREAD TRACES FOUND\n\n");
				}
			}

			fprintf(cmd.ofp, "\nXTHREAD STACK TRACES:\n\n");
			xtp = kl_alloc_block(XTHREAD_S_SIZE, K_TEMP);
			kl_get_struct(K_XTHREADLIST, 
				XTHREAD_S_SIZE, xtp, "xthread_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_XTHREAD_S;
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug("trace_cmd");
				}
			}
			else {
				xthread = kl_kaddr(xtp, "xthread_s", "xt_next");
				kl_free_block(xtp);
				trace_cnt = 0;
				while(xthread != K_XTHREADLIST) {
					xtp = kl_get_xthread_s(xthread, 2, 0);
					if (KL_UINT(xtp, "kthread", "k_flags") & 0x400) {
						/* If the xthread is waiting for the next interrupt,
						 * there is no valid trace.
						 */
						xthread = kl_kaddr(xtp, "xthread_s", "xt_next");
						kl_free_block(xtp);
						continue;
					}
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_XTHREAD_S;
						kl_print_error();
						break;
					}
					else {
						ksp->addr = xthread;
						ksp->ptr = xtp;
						setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
						if (KL_ERROR) {
							if (DEBUG(DC_TRACE, 1)) {
								print_trace_error(trace, cmd.ofp);
							}
							clean_trace_rec(trace);
							continue;
						}
						trace_banner(cmd.ofp);
						kthread_trace(trace, cmd.flags, cmd.ofp);
						if (KL_ERROR && !trace->nframes) {
							print_trace_error(trace, cmd.ofp);
						}
						trace_cnt++;
						xthread = kl_kaddr(xtp, "xthread_s", "xt_next");
						clean_trace_rec(trace);
					}
				}
				if (trace_cnt) {
					trace_banner(cmd.ofp);
					PLURAL("xthread trace", trace_cnt, cmd.ofp);
					fprintf(cmd.ofp, "\n");
				}
				else {
					fprintf(cmd.ofp, 
						"NOTICE: NO VALID XTHREAD TRACES FOUND\n\n");
				}
			}
			kl_free_block((k_ptr_t)ksp);
			free_trace_rec(trace);
			kl_set_defkthread(saved_kthread);
			return(0);
		} 
		else if (K_DEFKTHREAD) {
			ktp = kl_get_kthread(K_DEFKTHREAD, 0);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				kl_free_block((k_ptr_t)ksp);
				free_trace_rec(trace);
				return(1);
			}
			ksp->addr = K_DEFKTHREAD;
			ksp->ptr = ktp;
			setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				kl_free_block((k_ptr_t)ksp);
				free_trace_rec(trace);
				return(1);
			}
			trace_banner(cmd.ofp);
			kthread_trace(trace, cmd.flags, cmd.ofp);
			if (KL_ERROR && !trace->nframes) {
				print_trace_error(trace, cmd.ofp);
			}
		}
		else if (ACTIVE) {

			/* It's a live system, there isn't a defkthread, and dumpproc
			 * and dumpregs are undefined -- there's nothing much that
			 * can be done but print an error and return.
			 */
			KL_SET_ERROR(KLE_NO_KTHREAD);
			print_trace_error(trace, cmd.ofp);
			kl_free_block((k_ptr_t)ksp);
			free_trace_rec(trace);
			return(1);
		}
		else {	

			/* Generate a the trace for the cpu that generated the 
			 * core dump.
			 */

			int cpuid, size;
			k_ptr_t pdap;

			/* Allocate block for pda_s sturct here. Once we succeed
			 * in getting a pda_s struct loaded, we don't have to worry
			 * about freeing the memory block -- since clean_trace_rec()
			 * and free_trace_rec() take care of this for us.
			 */
			pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
			cpuid = kl_get_curcpu(pdap);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				kl_free_block(pdap);
			}
			else {
				trace_banner(cmd.ofp);
				ksp->addr = kl_pda_s_addr(cpuid);
				ksp->ptr = pdap;
				setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
				}
				else {
					fprintf (cmd.ofp, "STACK TRACE FOR CPU %d\n\n", cpuid);
					get_cpu_trace(cpuid, trace);
					if (KL_ERROR && !trace->nframes) {
						print_trace_error(trace, cmd.ofp);
					}
					else {
						print_trace(trace, cmd.flags, cmd.ofp);
					}
				}
			}
		}
	}
	else {
		for (i = 0; i < cmd.nargs; i++) {

			trace_banner(cmd.ofp);

			kl_get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				clean_trace_rec(trace);
				continue;
			}
			if (mode == 0) {
				KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, value, mode);
				print_trace_error(trace, cmd.ofp);
				clean_trace_rec(trace);
				continue;
			}

			if (mode == 1) {
				addr = kl_pid_to_proc(value);
			}
			else {
				addr = value;
			}

			ktp = kl_get_kthread(addr, 0);
			if (KL_ERROR) {
				/* It's possible that addr is a proc struct. The proc struct 
				 * no longer contains a kthread struct. Instead, it contains 
				 * a proc_proxy_s struct (located first in proc) which
				 * contains a pointer to the uthread_s/kthread sturct(s). Lets
				 * try out the address as a proc struct pointer and see if it
				 * works.
				 */
				kl_reset_error();
				p = kl_get_proc(addr, 2, 0);
				if (!KL_ERROR) {

					uthread = kl_proc_to_kthread(p, &uthread_cnt);

					/* Check to see if uthread is NULL. If it is then check
					 * to see if the process is defunct (in a zombie state).
					 * We can't generate a stack trace for a zombie process.
					 */
					if (!uthread) {
						if (KL_UINT(p, "proc", "p_stat") == 2) {
							KL_SET_ERROR_NVAL(KLE_DEFUNCT_PROCESS, proc, 2);
						}
						else {
							KL_SET_ERROR_NVAL(KLE_UNKNOWN_ERROR, proc, 2);
						}
						print_trace_error(trace, cmd.ofp);
						clean_trace_rec(trace);
						continue;
					}

					/* Check to see if more than one uthread is associated 
					 * with this proc. If there is, we have to generate an
					 * error message to use a specific kthread/uthread address 
					 * instead (after printing out a list of associated 
					 * uthreads).
					 */
					if (uthread_cnt > 1) {
						fprintf(cmd.ofp, "NOTICE: 0x%llx is a valid proc "
							"pointer.\n", addr);
						fprintf(cmd.ofp, "It has, however, more than one "
							"uthread associated\n");
						fprintf(cmd.ofp, "with it. Please re-issue the trace "
										 "command using a\n");
						fprintf(cmd.ofp, "unique uthread (kthread) "
							"pointer.\n\n");

						/* XXX - Need to output a list of associated uthreads
						 * here.
						 */

						clean_trace_rec(trace);
						continue;
					}
					ktp = kl_get_kthread(uthread, 0);
					addr = uthread;
				}
				if (KL_ERROR) {
					print_trace_error(trace, cmd.ofp);
					clean_trace_rec(trace);
					continue;
				}
			}

			ksp->addr = addr;
			ksp->ptr = ktp;
			setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				clean_trace_rec(trace);
				continue;
			}

			/* Set defkthread to the one contained in the trace record. We 
			 * shouldn't have to check to see if there was an error setting 
			 * defkthread since we already verified that the kthread pointer 
			 * was good when we set the trace rec up.
			 */
			kl_set_defkthread(trace->kthread);

			/* Generate the trace and then clean up...
			 */
			kthread_trace(trace, cmd.flags, cmd.ofp);
			if (KL_ERROR && !trace->nframes) {
				print_trace_error(trace, cmd.ofp);
			}
			clean_trace_rec(trace);
		}

		/* Now restore the saved defkthread 
		 */
		kl_set_defkthread(saved_kthread);
	}

	trace_banner(cmd.ofp);
	kl_free_block((k_ptr_t)ksp);
	free_trace_rec(trace);
	return(0);
}

#define _TRACE_USAGE "[-a] [-f] [-w outfile] [kthread_list]"

/*
 * trace_usage() -- Print the usage string for the 'trace' command.
 */
void
trace_usage(command_t cmd)
{
	CMD_USAGE(cmd, _TRACE_USAGE);
}

/*
 * trace_help() -- Print the help information for the 'trace' command.
 */
void
trace_help(command_t cmd)
{
	CMD_HELP(cmd, _TRACE_USAGE,
		"Displays a stack trace for each process included in kthread_list. If "
		"kthread_list is empty and defkthread is set, then a stack trace "
		"for the default kthread is displayed. If defkthread is not set, "
		"then a trace will be displayed for the process stored in dumpproc. "
		"If there isn't any dumpproc, then trace displays a trace for the "
		"cpu that generated the core dump.");
}

/*
 * trace_parse() -- Parse the command line arguments for 'trace'.
 */
int
trace_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_ALL);
}
