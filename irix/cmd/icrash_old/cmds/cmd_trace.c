#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_trace.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include "icrash.h"
#include "trace.h"
#include "extern.h"

/*
 * trace_banner()
 */
void
trace_banner(FILE *ofp, int flags)
{
	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "========================================"
					     "========================\n");
	}
	else if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "----------------------------------------"
					     "------------------------\n");
	}
}

/*
 * trace_cmd() -- This is the basic trace command.
 */
int
trace_cmd(command_t cmd)
{
	int i, mode, uthread_cnt, trace_cnt;
#ifdef ANON_ITHREADS
	kaddr_t addr, value, proc, uthread, ithread, sthread, xthread;
#else
	kaddr_t addr, value, proc, uthread, ithread, sthread, xthread;
#endif
	kaddr_t saved_kthread = 0;
	k_ptr_t p, ktp, procp, itp, stp, xtp;
	kstruct_t *ksp;
	trace_t *trace;

	trace = alloc_trace_rec();
	ksp = (kstruct_t*)alloc_block(sizeof(kstruct_t), B_TEMP);

	for (i = 0; i < cmd.nargs; i++) {

		trace_banner(cmd.ofp, SMAJOR);

		get_value(cmd.args[i], &mode, 0, &value);
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
			addr = kl_pid_to_proc(K, value);
		}
		else {
			addr = value;
		}

		ktp = kl_get_kthread(K, addr, 0);
		if (KL_ERROR) {
			/* It's possible that addr is a proc struct. The proc struct 
			 * no longer contains a kthread struct. Instead, it contains 
			 * a proc_proxy_s struct (located first in proc) which
			 * contains a pointer to the uthread_s/kthread sturct(s). Lets
			 * try out the address as a proc struct pointer and see if it
			 * works.
			 */
			kl_reset_error();
			p = kl_get_proc(K, addr, 2, 0);
			if (!KL_ERROR) {

				uthread = kl_proc_to_kthread(K, p, &uthread_cnt);

				/* Check to see if uthread is NULL. If it is then check
				 * to see if the process is defunct (in a zombie state).
				 * We can't generate a stack trace for a zombie process.
				 */
				if (!uthread) {
					if (KL_UINT(K, p, "proc", "p_stat") == 2) {
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
					fprintf(cmd.ofp, "It has, however, more than one uthread "
									 "associated\n");
					fprintf(cmd.ofp, "with it. Please re-issue the trace "
									 "command using a\n");
					fprintf(cmd.ofp, "unique uthread (kthread) pointer.\n\n");

					/* XXX - Need to output a list of associated uthreads
					 * here.
					 */

					clean_trace_rec(trace);
					continue;
				}
				ktp = kl_get_kthread(K, uthread, 0);
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

		/* Make sure that defkthread is set to our kthread (we have to
		 * save the current defkthread value so that we can restore it 
		 * when we are done). We shouldn't have to check to see if
		 * there was an error setting defkthread since we already
		 * verified that the kthread pointer was good when we setup
		 * the trace rec.
		 */
		if (K_DEFKTHREAD(K) != trace->kthread) {
			saved_kthread = K_DEFKTHREAD(K);
			kl_set_defkthread(K, trace->kthread);
		}

		/* Generate the trace and then clean up...
		 */
		kthread_trace(trace, cmd.flags, cmd.ofp);
		if (KL_ERROR && !trace->nframes) {
			print_trace_error(trace, cmd.ofp);
		}
		clean_trace_rec(trace);

		/* Now restore defkthread (if there is one to restore).
		 */
		if (saved_kthread) {
			kl_set_defkthread(K, saved_kthread);
		}
	}

	if (cmd.nargs == 0) {
		if (cmd.flags & C_ALL) {

			int cpuid;
			kaddr_t pid, vproc, proc, bhv;
			k_ptr_t pidp, vprocp;

			fprintf(cmd.ofp, "\nUTHREAD STACK TRACES:\n\n");
			pidp = alloc_block(PID_ENTRY_SIZE(K), B_TEMP);
			vprocp = alloc_block(VPROC_SIZE(K), B_TEMP);
			pid = K_PIDACTIVE(K);

			/* We have to step over the first entry as it is just
			 * the list head.
			 */
			kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");

			trace_cnt = 0;
			do {
				kl_get_struct(K, pid, PID_ENTRY_SIZE(K), pidp, "pid_entry");
				vproc = kl_kaddr(K, pidp, "pid_entry", "pe_vproc");
				kl_get_struct(K, vproc, VPROC_SIZE(K), vprocp, "vproc");
				bhv = kl_kaddr(K, vprocp, "vproc", "vp_bhvh");
				proc = kl_bhv_pdata(K, bhv);
				if (KL_ERROR) {
					free_block(pidp);
					free_block(vprocp);
					kl_print_error(K);
					return(0);
				}
				procp = kl_get_proc(K, proc, 2, cmd.flags);

				/* We have to convert the proc to a uthread_s/kthread.
				 */
				uthread = kl_proc_to_kthread(K, procp, &uthread_cnt);

				/* Check to see if uthread is NULL. If it is then check
				 * to see if the process is defunct (in a zombie state).
				 * We can't generate a stack trace for a zombie process.
				 */
				if (!uthread) {
					if (KL_UINT(K, procp, "proc", "p_stat") == 2) {
						KL_SET_ERROR_NVAL(KLE_DEFUNCT_PROCESS, proc, 2);
					}
					else {
						KL_SET_ERROR_NVAL(KLE_UNKNOWN_ERROR, proc, 2);
					}
					free_block(procp);
					trace_banner(cmd.ofp, SMAJOR);
					print_trace_error(trace, cmd.ofp);
					trace_cnt++;
					clean_trace_rec(trace);
					kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
					continue;
				}
				free_block(procp);

				/* Generate a stack trace for each uthread associated with
				 * this proc/PID.
				 */
				for (i = 0; i < uthread_cnt; i++) {
					ktp = kl_get_uthread_s(K, uthread, 2, 0);
					if (KL_ERROR) {
						trace_banner(cmd.ofp, SMAJOR);
						kl_print_error(K);
						break;
					}
					ksp->addr = uthread;
					ksp->ptr = ktp;
					setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
					if (KL_ERROR) {
						trace_banner(cmd.ofp, SMAJOR);
						print_trace_error(trace, cmd.ofp);
						clean_trace_rec(trace);
						break;
					}
					trace_banner(cmd.ofp, SMAJOR);
					kthread_trace(trace, cmd.flags, cmd.ofp);
					trace_cnt++;
					clean_trace_rec(trace);
					uthread = kl_kaddr(K, ktp, "uthread_s", "ut_next");
				}
				kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
			} while (pid != K_PIDACTIVE(K));
			free_block(pidp);
			free_block(vprocp);
			if (trace_cnt) {
				trace_banner(cmd.ofp, SMAJOR);
				PLURAL("uthread trace", trace_cnt, cmd.ofp);
				fprintf(cmd.ofp, "\n");
			}
			else {
				fprintf(cmd.ofp, 
					"NOTICE: NO VALID UTHREAD TRACES FOUND\n\n");
			}

			fprintf(cmd.ofp, "\nSTHREAD STACK TRACES:\n\n");
			stp = alloc_block(STHREAD_S_SIZE(K), B_TEMP);
			kl_get_struct(K, K_STHREADLIST(K), 
				STHREAD_S_SIZE(K), stp, "sthread_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_STHREAD_S;
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug(K, "trace_cmd");
				}
			}
			else {
				sthread = kl_kaddr(K, stp, "sthread_s", "st_next");
				free_block(stp);
				trace_cnt = 0;
				while(sthread != K_STHREADLIST(K)) {
					stp = kl_get_sthread_s(K, sthread, 2, 0);
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_STHREAD_S;
						kl_print_error(K);
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
						trace_banner(cmd.ofp, SMAJOR);
						kthread_trace(trace, cmd.flags, cmd.ofp);
						trace_cnt++;
						if (KL_ERROR && !trace->nframes) {
							print_trace_error(trace, cmd.ofp);
						}
						sthread = kl_kaddr(K, stp, "sthread_s", "st_next");
						clean_trace_rec(trace);
					}
				}
				if (trace_cnt) {
					trace_banner(cmd.ofp, SMAJOR);
					PLURAL("sthread trace", trace_cnt, cmd.ofp);
					fprintf(cmd.ofp, "\n");
				}
				else {
					fprintf(cmd.ofp, 
						"NOTICE: NO VALID STHREAD TRACES FOUND\n\n");
				}
			}

#ifdef ANON_ITHREADS
			fprintf(cmd.ofp, "\nITHREAD STACK TRACES:\n\n");
			itp = alloc_block(ITHREAD_S_SIZE(K), B_TEMP);
			kl_get_struct(K, K_ITHREADLIST(K), 
				ITHREAD_S_SIZE(K), itp, "ithread_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_ITHREAD_S;
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug(K, "trace_cmd");
				}
			}
			else {
				ithread = kl_kaddr(K, itp, "ithread_s", "it_next");
				free_block(itp);
				trace_cnt = 0;
				while(ithread != K_ITHREADLIST(K)) {
					itp = kl_get_ithread_s(K, ithread, 2, 0);
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_ITHREAD_S;
						kl_print_error(K);
						break;
					}

					/* We have to make the determination here weather
					 * or not this ithread is active or inactive. If
					 * it's inactive, we can just skip it (there are
					 * no valid traces anyway).
					 */
					cpuid = KL_INT(K, itp, "kthread", "k_sonproc");
					if (cpuid != -1) {
						if (ithread != kl_get_curkthread(K, cpuid)) {
							ithread = kl_kaddr(K, itp, "ithread_s", "it_next");
							free_block(itp);
							continue;
						}
					}
					ksp->addr = ithread;
					ksp->ptr = itp;
					setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
					if (KL_ERROR) {
						if (DEBUG(DC_TRACE, 1)) {
							print_trace_error(trace, cmd.ofp);
						}
						clean_trace_rec(trace);
						continue;
					}
					trace_banner(cmd.ofp, SMAJOR);
					kthread_trace(trace, cmd.flags, cmd.ofp);
					trace_cnt++;
					ithread = kl_kaddr(K, itp, "ithread_s", "it_next");
					clean_trace_rec(trace);
				}
				if (trace_cnt) {
					trace_banner(cmd.ofp, SMAJOR);
					PLURAL("ithread trace", trace_cnt, cmd.ofp);
					fprintf(cmd.ofp, "\n");
				}
				else {
					fprintf(cmd.ofp, 
						"NOTICE: NO VALID ITHREAD TRACES FOUND\n\n");
				}
			}
#endif

			fprintf(cmd.ofp, "\nXTHREAD STACK TRACES:\n\n");
			xtp = alloc_block(XTHREAD_S_SIZE(K), B_TEMP);
			kl_get_struct(K, K_XTHREADLIST(K), 
				XTHREAD_S_SIZE(K), xtp, "xthread_s");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_XTHREAD_S;
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug(K, "trace_cmd");
				}
			}
			else {
				xthread = kl_kaddr(K, xtp, "xthread_s", "xt_next");
				free_block(xtp);
				trace_cnt = 0;
				while(xthread != K_XTHREADLIST(K)) {
					xtp = kl_get_xthread_s(K, xthread, 2, 0);
					if (KL_UINT(K, xtp, "kthread", "k_flags") & 0x400) {
						/* If the xthread is waiting for the next interrupt,
						 * there is no valid trace.
						 */
						xthread = kl_kaddr(K, xtp, "xthread_s", "xt_next");
						free_block(xtp);
						continue;
					}
					if (KL_ERROR) {
						KL_ERROR |= KLE_BAD_XTHREAD_S;
						kl_print_error(K);
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
						trace_banner(cmd.ofp, SMAJOR);
						kthread_trace(trace, cmd.flags, cmd.ofp);
						if (KL_ERROR && !trace->nframes) {
							print_trace_error(trace, cmd.ofp);
						}
						trace_cnt++;
						xthread = kl_kaddr(K, xtp, "xthread_s", "xt_next");
						clean_trace_rec(trace);
					}
				}
				if (trace_cnt) {
					trace_banner(cmd.ofp, SMAJOR);
					PLURAL("xthread trace", trace_cnt, cmd.ofp);
					fprintf(cmd.ofp, "\n");
				}
				else {
					fprintf(cmd.ofp, 
						"NOTICE: NO VALID XTHREAD TRACES FOUND\n\n");
				}
			}
			free_block((k_ptr_t)ksp);
			free_trace_rec(trace);
			return(0);
		} 

		if (K_DEFKTHREAD(K)) {
			ktp = kl_get_kthread(K, K_DEFKTHREAD(K), 0);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				free_block((k_ptr_t)ksp);
				free_trace_rec(trace);
				return(1);
			}
			ksp->addr = K_DEFKTHREAD(K);
			ksp->ptr = ktp;
			setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				free_block((k_ptr_t)ksp);
				free_trace_rec(trace);
				return(1);
			}
			trace_banner(cmd.ofp, SMAJOR);
			kthread_trace(trace, cmd.flags, cmd.ofp);
			if (KL_ERROR && !trace->nframes) {
				print_trace_error(trace, cmd.ofp);
			}
		}
		else if (ACTIVE(K)) {

			/* It's a live system, there isn't a defkthread, and dumpproc
			 * and dumpregs are undefined -- there's nothing much that
			 * can be done but print an error and return.
			 */
			KL_SET_ERROR(E_NO_KTHREAD);
			print_trace_error(trace, cmd.ofp);
			free_block((k_ptr_t)ksp);
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
			pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
			cpuid = kl_get_curcpu(K, pdap);
			if (KL_ERROR) {
				print_trace_error(trace, cmd.ofp);
				free_block(pdap);
			}
			else {
				trace_banner(cmd.ofp, SMAJOR);
				ksp->addr = kl_pda_s_addr(K, cpuid);
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
	trace_banner(cmd.ofp, SMAJOR);
	free_block((k_ptr_t)ksp);
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
