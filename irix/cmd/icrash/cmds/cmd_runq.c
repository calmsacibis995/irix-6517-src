#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_runq.c,v 1.19 1999/05/25 19:21:38 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * These macros make it easier for code which is the same for both 32-bit 
 * and 64-bit.. 
 * This macro prints out any field in the pda structure.
 * It assumes the existence of variables:
 *      offset, Ppda, KPkthread, Pkthread, flags & ofp
 * The ##  is a pre-processor directive to concatenate stuff together.
 */
#define PRINT_FIELD_KTHREAD_PDA_BRIEF(PRINTFNAME, FIELDNAME, PRINTFORMAT)  \
{ \
	if(kl_is_member("pda_s",FIELDNAME)) { \
		KPkthread = kl_kaddr(Ppda,"pda_s",FIELDNAME); \
		Pkthread = kl_get_kthread(KPkthread,flags); \
		fprintf(ofp," " ## PRINTFORMAT ## "  %4lld",\
			KPkthread,Pkthread ? KL_INT(Pkthread,"kthread", "k_pri") : 0); \
		if(Pkthread) { \
			kl_free_block(Pkthread); \
		} \
	} \
}

#define PRINT_FIELD_KTHREAD_PDA(PRINTFNAME, FIELDNAME, PRINTFORMAT) \
{ \
	if(kl_is_member("pda_s",FIELDNAME)) { \
		KPkthread = kl_kaddr(Ppda, "pda_s", FIELDNAME); \
		Pkthread = kl_get_kthread(KPkthread, flags); \
		if(Pkthread && KPkthread && (flags & C_NEXT)) { \
			fprintf(ofp,"\n"); \
			indent_it(flags,ofp); \
			fprintf(ofp, "%s=" ## PRINTFORMAT ## ", K_PRI=%4lld",\
				PRINTFNAME,KPkthread,\
				Pkthread ? KL_INT(Pkthread, "kthread", "k_pri") : 0); \
			fprintf(ofp,"\n"); \
			kthread_banner(ofp,C_INDENT|SMINOR|BANNER); \
			print_kthread(KPkthread,Pkthread, (flags|C_INDENT) & ~C_NEXT,ofp);\
			kthread_banner(ofp, C_INDENT|SMINOR); \
		} \
		if(Pkthread) { \
			kl_free_block(Pkthread); \
		} \
	} \
}

/*
 * cpu_runq_banner()
 */
void
cpu_runq_banner(FILE *ofp, int flags)
{
	fprintf(ofp,"\n");
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64) {
			fprintf(ofp,
				"\n CPU CURPRI       CURKTHREAD K_PRI       NEXTTHREAD "
				"K_PRI  RUNQ\n");
		}
		else {
			fprintf(ofp,
				"\n CPU CURPRI       CURKTHREAD K_PRI       NEXTTHREAD "
				"K_PRI  RUNQ\n");
		}
	}
		
	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"================================================="
			"============================\n");
	}
	
	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------------"
			"----------------------------\n");
	}
}


/*
 *   print_runq_pda_full() 
 *
 *   Print out the scheduler specific info from the pda. Be very verbose.
 *   Call out print_runq_kthread if C_NEXT...
 */
int 
print_runq_pda_full(kaddr_t KPpda, k_ptr_t Ppda, int flags, FILE *ofp)
{
	kaddr_t KPkthread = NULL, KPrflink = NULL, KPc_threads;
	k_ptr_t Pcpu_s, Pkthread;
	int i = 0, offset;

	if (!Ppda || !KPpda || !ofp) {
		return(-1);
	}

	Pcpu_s = (k_ptr_t)((__psunsigned_t)Ppda + 
			   (__psunsigned_t)kl_member_offset("pda_s","p_cpu"));

	fprintf(ofp,"\n");

	indent_it(flags,ofp);
	if(kl_is_member("cpu_s","c_onq")) {
		fprintf(ofp,"C_ONQ=%lld, ", KL_UINT(Pcpu_s,"cpu_s","c_onq"));
	}
		
	if(kl_is_member("cpu_s","c_restricted")) {
		fprintf(ofp,"C_RESTRICTED=%lld, ", 
			KL_UINT(Pcpu_s,"cpu_s","c_restricted"));
	}

	if(kl_is_member("cpu_s","c_peekrotor")) {
		fprintf(ofp,"C_PEEKROTOR=0x%llx, ",
			KL_INT(Pcpu_s,"cpu_s","c_peekrotor"));
	}
	
	fprintf(ofp,"\n");
		
	/* Now print out the threads on each cpu's runq.
	 */
	PRINT_FIELD_KTHREAD_PDA("CURKTHREAD","p_curkthread", "%llx");
	PRINT_FIELD_KTHREAD_PDA("NEXTTHREAD","p_nextthread", "%llx");
			 
	if(!kl_is_member("cpu_s","c_threads")) {
		goto error;
	}

	KPc_threads = KPkthread = kl_kaddr(Pcpu_s, "cpu_s", "c_threads");
		
	if(KPkthread) {
		fprintf(ofp,"\n");
		indent_it(flags,ofp);
		fprintf(ofp,"C_THREADS=0x%llx, ", KPkthread);
		Pkthread = kl_get_kthread(KPkthread, flags);
		fprintf(ofp," K_PRI=%-4lld  ",
			Pkthread ? KL_INT(Pkthread, "kthread", "k_pri") : 0);
		if(Pkthread) {
			if(flags & C_NEXT) {       
				fprintf(ofp,"\n");                     
				kthread_banner(ofp, SMINOR|BANNER|C_INDENT);
				print_kthread(KPkthread, Pkthread, 
					(flags|C_INDENT) & ~C_NEXT,ofp); 
				kthread_banner(ofp, SMINOR|C_INDENT); 
			}
			KPrflink = kl_kaddr(Pkthread,"kthread", "k_rflink");
			kl_free_block(Pkthread);
		}
			
		/* On a live system, since we don't have the luxury
		 * of locking these data structures down.. we may
		 * go into an infinite loop. Check for those cases.
		 * 
		 * We warn folks about that.
		 */
		i = 0;
		while (KPrflink && KPrflink != KPkthread && KPrflink != KPc_threads) {
			if(flags & C_NEXT || (i++%2 == 0)) {
				fprintf(ofp,"\n");
				indent_it(flags,ofp);
			}
			fprintf(ofp,"THRD=0x%llx,",KPrflink);
			KPkthread = KPrflink;
			Pkthread = kl_get_kthread(KPkthread, flags);
			fprintf(ofp," K_PRI=%-4lld  ",
				Pkthread ? KL_INT(Pkthread, "kthread", "k_pri") : 0);
			if(Pkthread) {
				if(flags & C_NEXT) {                                 
					fprintf(ofp,"\n");
					kthread_banner(ofp, SMINOR|BANNER| C_INDENT);
					print_kthread(KPkthread, Pkthread, 
						(flags|C_INDENT)& ~C_NEXT, ofp);
					kthread_banner(ofp,SMINOR| C_INDENT);    
				}
				KPrflink = kl_kaddr(Pkthread, "kthread", "k_rflink");
				kl_free_block(Pkthread);
			}
			else {
				break;
			}
		}
		if(KPrflink) {
			fprintf(ofp,"\n");
		}
	}
		
	return(0);
error:
	return(-1);
}

/*
 *   print_runq_pda() 
 *
 *   Print out the scheduler specific info from the pda. Be brief.
 */
int 
print_runq_pda_brief(kaddr_t KPpda, k_ptr_t Ppda, int flags, FILE *ofp)
{
	kaddr_t KPkthread=NULL,KPrflink=NULL,KPc_threads;
	k_ptr_t Pcpu_s,Pkthread;
	int num_threads=0,offset=-1;

	if (!Ppda || !KPpda || !ofp) {
		fprintf(ofp,"\n");
		return(-1);
	}

	Pcpu_s = (k_ptr_t)((__psunsigned_t)Ppda + 
			   (__psunsigned_t)kl_member_offset("pda_s","p_cpu"));
	
	/* Current processor priority... = priority of current kthread.
	 * Higher the value ... the higher the priority.
	 */
	fprintf(ofp," % 4lld", KL_INT(Ppda,"pda_s","p_curpri"));
		
	PRINT_FIELD_KTHREAD_PDA_BRIEF("","p_curkthread","%16llx");
	PRINT_FIELD_KTHREAD_PDA_BRIEF("","p_nextthread","%16llx");
		
	/* Now count the threads on each cpu's runq.
	 */
	KPc_threads = KPkthread = kl_kaddr(Pcpu_s,"cpu_s", "c_threads");
		
	if(KPkthread && (Pkthread = kl_get_kthread(KPkthread, flags))) {
		KPrflink = kl_kaddr(Pkthread,"kthread", "k_rflink");

		kl_free_block(Pkthread);
			
		/* On a live system, since we don't have the luxury
		 * of locking these data structures down.. we may
		 * go into an infinite loop. Check for those cases.
		 * 
		 * We warn folks about that.
		 */
		num_threads = 1;
		while (KPrflink && KPrflink != KPkthread && KPrflink != KPc_threads) {
			KPkthread = KPrflink;
			if(Pkthread = kl_get_kthread(KPkthread, flags)) {
				KPrflink = kl_kaddr(Pkthread, "kthread", "k_rflink");
				kl_free_block(Pkthread);
			}
			else {
				break;
			}
			num_threads++;
		}
	}

	/* RUNQ
	 */
	fprintf(ofp,"   %3d", num_threads);
	fprintf(ofp,"\n");
	return(0);
}

/*
 *   print_runq_pda() 
 *
 *   Print out the scheduler specific info from the pda.
 */
int 
print_runq_pda(kaddr_t KPpda, k_ptr_t Ppda, int flags, FILE *ofp)
{
	if(flags & (C_FULL|C_NEXT)) {
		if(print_runq_pda_brief(KPpda, Ppda, flags|C_INDENT,ofp) < 0) {
			return(-1);
		}
		return(print_runq_pda_full(KPpda,Ppda,flags|C_INDENT,ofp));
	}
	else {
		return(print_runq_pda_brief(KPpda,Ppda,flags,ofp));
	}
}

/*
 *   print_runq_rt() 
 *
 *   Print out the scheduler info for real-time. This code is the same as
 *   some code in idbg_rt
 */
int
print_runq_rt_gq(int flags,FILE *ofp)
{
	int num_threads = 0,offset;
	kaddr_t KPvalue, KPkthread,KPrt_gq;
	k_ptr_t Pkthread=NULL;
	

	fprintf(ofp,"\nOVERFLOW QUEUE REAL-TIME SCHEDULER INFORMATION:\n");

	KPrt_gq = KPkthread = kl_kaddr_to_ptr(kl_sym_addr("rt_gq"));

	if (!(flags & C_FULL) && KPrt_gq) {
		kthread_banner(ofp,SMAJOR|BANNER);
	}
	while(KPkthread && (!num_threads || (KPkthread != KPrt_gq))) {
		Pkthread  = kl_get_kthread(KPkthread, flags);

		if(Pkthread) {
			if (flags & C_FULL) {
				fprintf(ofp,"\n");
				kthread_banner(ofp, SMAJOR|BANNER);
			}
			print_kthread(KPkthread, Pkthread, flags & ~C_NEXT, ofp);
			if (flags & C_FULL) {
				kthread_banner(ofp, SMAJOR);
			}
			KPkthread = kl_kaddr(Pkthread, "kthread", "k_rflink");
			kl_free_block(Pkthread);
			num_threads ++;
		}
		else {
			break;
		}
	}
	
	if (!(flags & C_FULL) && KPrt_gq) {
		kthread_banner(ofp,SMAJOR);
	}
	fprintf(ofp,"%d real-time threads found on overflow queue.\n", num_threads);
	return(num_threads);
}

/*
 * runq_cmd() -- Show process(es) on the runq.
 */
int
runq_cmd(command_t cmd)
{
	int runq_cnt = 0, i, firsttime = TRUE, cpu = 0;
	kaddr_t KPvalue, KPkthread,KPpda;
	k_ptr_t Ppda=NULL,Pkthread=NULL,Puthread;
	
	if (!cmd.ofp) {
		return(1);
	}

	if(ACTIVE) {
		fprintf(cmd.ofp,
			"\nSome of the information here will NOT be "
			"accurate, as on a live system, the\n"
			"scheduler information changes very quickly.\n\n");
	}

	if (cmd.nargs == 0) {
		/*
		 * Do the idbg stuff for the cpu and then do the rt,
		 * gangq and batch (?) stuff.
		 *
		 * At present the gangq stuff cannot be done since it is a
		 * static defined variable in the kernel.
		 */
	
		Ppda = kl_alloc_block(PDA_S_SIZE, K_TEMP);

		fprintf(cmd.ofp,"\nCPU SCHEDULER INFORMATION:");
		for(cpu=0;cpu<K_NUMCPUS;cpu++) {
			KPpda = kl_get_pda_s((kaddr_t)cpu, Ppda);
			if (KL_ERROR) {
				continue;
			}
			if(cmd.flags & (C_FULL|C_NEXT) || cpu == 0) {
				cpu_runq_banner(cmd.ofp,BANNER|SMAJOR);
			}

			if(PTRSZ64) {
				fprintf(cmd.ofp,
					"%4d  ",
					(int)KL_UINT(Ppda,"pda_s",
							 "p_cpuid"));
			} 
			else {
				fprintf(cmd.ofp, "%4d  ", 
					(int)KL_UINT(Ppda,"pda_s", "p_cpuid"));
			}

			/* All the above was the brief, single line stuff..
			 * The rest is if the -f and -n flags are specfied.
			 */ 
			print_runq_pda(KPpda,Ppda,cmd.flags,cmd.ofp);
			if(cmd.flags & (C_FULL|C_NEXT)) {
				cpu_runq_banner(cmd.ofp,SMAJOR);
				fprintf(cmd.ofp,"\n");
			}
		}

		if(!(cmd.flags & (C_FULL|C_NEXT))) {
			cpu_runq_banner(cmd.ofp, SMAJOR);
			fprintf(cmd.ofp,"\n");
		}

		kl_free_block(Ppda);

		print_runq_rt_gq(cmd.flags, cmd.ofp);
		fprintf(cmd.ofp,"\n");
	} 
	else if (cmd.flags & C_FULL || cmd.nargs > 0) {
		/*
		 * Just print out the cpu specific stuff. This option *SHOULD*
		 * be last conditional in the if .. else statements above.
		 */
		if(cmd.nargs == 0) {
			runq_usage(cmd);
			goto error;
		}

		fprintf(cmd.ofp,"\nCPU SCHEDULER INFORMATION:");
		for (i=0;i<cmd.nargs;i++) {
			GET_VALUE(cmd.args[i], &KPvalue);
			cpu = (int)KPvalue;
			if (cpu > K_NUMCPUS) {
				goto error;
			}
			if (KL_ERROR) {
				goto error;
			}
			Ppda = kl_alloc_block(PDA_S_SIZE, K_TEMP);

			KPpda = kl_get_pda_s((kaddr_t)cpu, Ppda);
			if (KL_ERROR) {
				kl_free_block(Ppda);

				/* Early versions of the kernel did not have 
				 * the cpu_set_s structure in the symbol table.
				 */
				goto error;
			}

			cpu_runq_banner(cmd.ofp,BANNER|SMAJOR);
			if(PTRSZ64) {
				fprintf(cmd.ofp, "%4d  ",
					(int)KL_UINT(Ppda,"pda_s", "p_cpuid"));
			} 
			else {
				fprintf(cmd.ofp, "%4d  ",
					(int)KL_UINT(Ppda,"pda_s", "p_cpuid"));
			}

			/* All the above was the brief, single line stuff..
			 * The rest is if the -f and -n flags are specfied.
			 */ 
			print_runq_pda(KPpda,Ppda,cmd.flags,cmd.ofp);
			cpu_runq_banner(cmd.ofp,SMAJOR);

			kl_free_block(Ppda);
		}
	}

	return(0);
error:  
	return(1);	
}

#define _RUNQ_USAGE "[-f] [-n] [-w outfile] [cpu_list]"

/*
 * runq_usage() -- Print the usage string for the 'runq' command.
 */
void
runq_usage(command_t cmd)
{
	CMD_USAGE(cmd, _RUNQ_USAGE);
}

/*
 * runq_help() -- Print the help information for the 'runq' command.
 */
void
runq_help(command_t cmd)
{
	CMD_HELP(cmd, _RUNQ_USAGE,
		 "Display scheduler information for each CPU in cpu_list. "
		 "If cpu_list is empty, then scheduler information for all "
		 "CPUs will be displayed. When the -f option is specified, "
		 "more scheduler information about the cpu and local run queue "
		 "(if any) is displayed.");
}

/*
 * runq_parse() -- Parse the command line arguments for 'runq'.
 */
int
runq_parse(command_t cmd)
{
	return (C_FULL|C_NEXT|C_WRITE|C_MAYBE);
}
