#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <libexc.h>
#include <klib/klib.h>

jmp_buf klib_jbuf;
static struct sigaction klib_sigact;

/* Global flag which forces the klib_sig_handler() function to generate 
 * a core file (off by default).
 */
int generate_core = 0;

/* Global flag to control the printing of KLIB ERROR TRAP message
 * (on by default).
 */
int klib_trap_msg = 1;

/*
 * klib_sigon()
 */
void
klib_sigon()
{
	if (sigaction(SIGINT, &klib_sigact, NULL) < 0) {
		KL_SET_ERROR(KLE_SIG_ERROR);
		return;
	}
	if (sigaction(SIGPIPE, &klib_sigact, NULL) < 0) {
		KL_SET_ERROR(KLE_SIG_ERROR);
		return;
	}	
	if (sigaction(SIGABRT, &klib_sigact, NULL) < 0) {
		KL_SET_ERROR(KLE_SIG_ERROR);
		return;              
	}
	if (sigaction(SIGSEGV, &klib_sigact, NULL) < 0) {
		KL_SET_ERROR(KLE_SIG_ERROR);
		return;              
	}
	if (sigaction(SIGBUS, &klib_sigact, NULL) < 0) {
		KL_SET_ERROR(KLE_SIG_ERROR);
		return;
	}
}

/*
 * klib_sig_handler() -- Handle signals
 */
void
klib_sig_handler(int sig, siginfo_t *sip, void *p)
{
	ucontext_t *up = p;       /* .h has wrong type in struct sigaction */
	greg_t *gregs = up->uc_mcontext.gregs;
	greg_t epc, sp, ra, badaddr;

	epc = gregs[CTX_EPC];
	ra = gregs[CTX_RA];
	sp = gregs[CTX_SP];
	if (sip) {
		badaddr = (greg_t)sip->si_addr;
	}

	if (klib_trap_msg) {
		switch (sig) {

			case SIGABRT:           
				/* abort called (assert() failed)
				 */
				fprintf(KL_ERRORFP, "KLIB ERROR TRAP: ABORT\n");
				fprintf(KL_ERRORFP, "  EPC=0x%llx, RA=0x%llx, SP=0x%llx, "
					"BADADDR=0x%llx\n", epc, ra, sp, badaddr);
				trace_back_stack_and_print();
				break;

			case SIGSEGV:           
				/* out-of-range access 
				 */
				fprintf(KL_ERRORFP, "KLIB ERROR TRAP: SEGV ");
				switch (sip->si_code) {

					case SEGV_MAPERR:
						fprintf(KL_ERRORFP, "(address not mapped to object)\n");
						break;

					case SEGV_ACCERR:
						fprintf(KL_ERRORFP, "(invalid permissions for mapped "
							"object)\n");
						break;

					case SI_USER:
						fprintf(KL_ERRORFP, "(sent by kill or sigsend, pid %d, "
							"uid %d)\n", sip->si_pid, sip->si_uid);
						break;

					case SI_QUEUE:
						fprintf(KL_ERRORFP, "(sent by sigqueue, pid %d, "
							"uid %d)\n", sip->si_pid, sip->si_uid);
						break;

					default:
						fprintf(KL_ERRORFP, "(unknown si_code = %d)\n", 
							sip->si_code);
				}
				fprintf(KL_ERRORFP, "  EPC=0x%llx, RA=0x%llx, SP=0x%llx, "
					"BADADDR=0x%llx\n", epc, ra, sp, badaddr);
				trace_back_stack_and_print();
				break;

			case SIGBUS:
				fprintf(KL_ERRORFP, "KLIB ERROR TRAP: BUSERR ");
				switch (sip->si_code) {
					case BUS_ADRALN:
						fprintf(KL_ERRORFP, "(invalid address alignment)\n");
						break;

					case BUS_ADRERR:
						fprintf(KL_ERRORFP, "(non-existent physical "
								"address)\n");
						break;

					case BUS_OBJERR:
						fprintf(KL_ERRORFP, "(object specific hardware "
								"error)\n");
						break;

					case SI_USER:
						fprintf(KL_ERRORFP, "(sent by kill or sigsend, pid %d, "
								"uid %d)\n", sip->si_pid, sip->si_uid);
						break;

					case SI_QUEUE:
						fprintf(KL_ERRORFP, "(sent by sigqueue, pid %d, "
								"uid %d)\n", sip->si_pid, sip->si_uid);
						break;

					default:
						fprintf(KL_ERRORFP, 
							"(unknown si_code = %d)\n", sip->si_code);
				}
				fprintf(KL_ERRORFP, "  EPC=0x%llx, RA=0x%llx, SP=0x%llx, "
					"BADADDR=0x%llx\n", epc, ra, sp, badaddr);
				trace_back_stack_and_print();
				break;

			case SIGINT:
				fprintf(KL_ERRORFP, "Received SIGINT signal\n");
				break;

			case SIGPIPE:
				break;
			default:
				fprintf(KL_ERRORFP, "Error:  unexpectedly got signal %d\n",sig);
				break;
		}
	}

	if (generate_core) {
		switch (sig) {
			case SIGABRT:
			case SIGSEGV:
			case SIGBUS:
				klib_sigact.sa_flags = 0;
				klib_sigact.sa_sigaction = SIG_DFL;
				if (sigaction(SIGABRT, &klib_sigact, NULL) < 0) {
					fprintf(KL_ERRORFP, "Could not set SIGABRT disposition to "
								"SIG_DFL!\n");
				}
				abort();
		}
	}

	/* Make sure we get ready to trap signals again
	 */
	klib_sigon();

	/* Go back to where we left off...
	 */
	longjmp(klib_jbuf, 0);
}

/*
 * kl_sig_setup() -- initialize the sigactions struct
 */
int
kl_sig_setup() 
{
	/* Don't block the signal, return verbose info, and reset the
	 * signal disposition to default.
	 */
	klib_sigact.sa_flags = (SA_NODEFER | SA_SIGINFO | SA_RESETHAND);
	klib_sigact.sa_sigaction = klib_sig_handler; 
	if (sigemptyset(&klib_sigact.sa_mask) < 0) {
		return(1);
	}
	klib_sigon();
	return(0);
}

/* 
 * kl_hold_signals() -- Hold signals in critical blocks of code
 */
void
kl_hold_signals()
{
	sighold((int)SIGINT);
	sighold((int)SIGPIPE);
}

/* 
 * kl_release_signals() -- Allow signals again
 */
void
kl_release_signals()
{
	sigrelse((int)SIGINT);
	sigrelse((int)SIGPIPE);
}
