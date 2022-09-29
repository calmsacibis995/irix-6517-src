/* #include <sys/syssgi.h> */
#define _KMEMUSER
#include <sys/types.h>
#include <sys/prctl.h>
#undef _KMEMUSER
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <nanothread.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <siginfo.h>
#include <ulocks.h>

extern void single_context (void);
extern upcall_handler_t local_upcall_code;
extern int resume_nid (nid_t, kusharena_t *, nid_t);

int mustrun;

greg_t
return_gp(void)
{
   return get_gp();
}

void
dump_kus (kusharena_t * lkusp)
{
	register int i;

	printf ("kusp %p nr %d na %d\n", lkusp, lkusp->nrequested, lkusp->nallocated);
	printf ("rbits ");
	for (i = 0; ((i < 64) && (i <= lkusp->nrequested)); i++) {
		printf ("%d", ((lkusp->rbits[0] & 1LL << i) > 0));
	}
	printf ("\nfbits ");
	for (i = 0; ((i < 64) && (i <= lkusp->nrequested)); i++) {
		printf ("%d", ((lkusp->fbits[0] & 1LL << i) > 0));
	}
	printf ("\nn2rsa ");
	for (i = 0; ((i < 64) && (i <= lkusp->nrequested)); i++) {
		printf ("%3d", kusp->nid_to_rsaid[i]);
	}
	/*
	   printf("\nn2thd ");
	   for (i=1; ((i<64) && (i<=kusp->nrequested)); i++) {
	   printf("%3d", kusp->cacheline_filler[i]);
	   }
	 */
	printf ("\n");
}

#define rsa_fpregs rsa.rsa_context.fpregs.fp_r.fp_regs
void
dump_rsa(nid_t nid)
{
	padded_rsa_t *rsap = &kusp->rsa[nid];
	printf("zr=%16llx at=%16llx v0=%16llx v1=%16llx\n",
		rsap->rsa_gregs[CTX_R0], rsap->rsa_gregs[CTX_AT],
		rsap->rsa_gregs[CTX_V0], rsap->rsa_gregs[CTX_V1]);
	printf("a0=%16llx a1=%16llx a2=%16llx a3=%16llx\n",
		rsap->rsa_gregs[CTX_A0], rsap->rsa_gregs[CTX_A1],
		rsap->rsa_gregs[CTX_A2], rsap->rsa_gregs[CTX_A3]);
	printf("a4=%16llx a5=%16llx a6=%16llx a7=%16llx\n",
		rsap->rsa_gregs[CTX_A4], rsap->rsa_gregs[CTX_A5],
		rsap->rsa_gregs[CTX_A6], rsap->rsa_gregs[CTX_A7]);
	printf("t0=%16llx t1=%16llx t2=%16llx t3=%16llx\n",
		rsap->rsa_gregs[CTX_T0], rsap->rsa_gregs[CTX_T1],
		rsap->rsa_gregs[CTX_T2], rsap->rsa_gregs[CTX_T3]);
	printf("s0=%16llx s1=%16llx s2=%16llx s3=%16llx\n",
		rsap->rsa_gregs[CTX_S0], rsap->rsa_gregs[CTX_S1],
		rsap->rsa_gregs[CTX_S2], rsap->rsa_gregs[CTX_S3]);
	printf("s4=%16llx s5=%16llx s6=%16llx s7=%16llx\n",
		rsap->rsa_gregs[CTX_S4], rsap->rsa_gregs[CTX_S5],
		rsap->rsa_gregs[CTX_S6], rsap->rsa_gregs[CTX_S7]);
	printf("t8=%16llx jp=%16llx k0=%16llx k1=%16llx\n",
		rsap->rsa_gregs[CTX_T8], rsap->rsa_gregs[CTX_T9],
		rsap->rsa_gregs[CTX_K0], rsap->rsa_gregs[CTX_K1]);
	printf("gp=%16llx sp=%16llx fp=%16llx ra=%16llx\n",
		rsap->rsa_gregs[CTX_GP], rsap->rsa_gregs[CTX_SP],
		rsap->rsa_gregs[CTX_S8], rsap->rsa_gregs[CTX_RA]);
	printf("pc=%16llx now floating point\n", rsap->rsa_gregs[CTX_EPC]);
	printf("v0=%16llx tc=%16llx v1=%16llx td=%16llx\n",
		rsap->rsa_fpregs[CTX_FV0], rsap->rsa_fpregs[CTX_FT12],
		rsap->rsa_fpregs[CTX_FV1], rsap->rsa_fpregs[CTX_FT13]);
	printf("t0=%16llx t1=%16llx t2=%16llx t3=%16llx\n",
		rsap->rsa_fpregs[CTX_FT0], rsap->rsa_fpregs[CTX_FT1],
		rsap->rsa_fpregs[CTX_FT2], rsap->rsa_fpregs[CTX_FT3]);
	printf("t4=%16llx t5=%16llx t6=%16llx t7=%16llx\n",
		rsap->rsa_fpregs[CTX_FT4], rsap->rsa_fpregs[CTX_FT5],
		rsap->rsa_fpregs[CTX_FT6], rsap->rsa_fpregs[CTX_FT7]);
	printf("a0=%16llx a1=%16llx a2=%16llx a3=%16llx\n",
		rsap->rsa_fpregs[CTX_FA0], rsap->rsa_fpregs[CTX_FA1],
		rsap->rsa_fpregs[CTX_FA2], rsap->rsa_fpregs[CTX_FA3]);
	printf("a4=%16llx a5=%16llx a6=%16llx a7=%16llx\n",
		rsap->rsa_fpregs[CTX_FA4], rsap->rsa_fpregs[CTX_FA5],
		rsap->rsa_fpregs[CTX_FA6], rsap->rsa_fpregs[CTX_FA7]);
	printf("t8=%16llx t9=%16llx ta=%16llx tb=%16llx\n",
		rsap->rsa_fpregs[CTX_FT8], rsap->rsa_fpregs[CTX_FT9],
		rsap->rsa_fpregs[CTX_FT10], rsap->rsa_fpregs[CTX_FT11]);
	printf("s0=%16llx s1=%16llx s2=%16llx s3=%16llx\n",
		rsap->rsa_fpregs[CTX_FS0], rsap->rsa_fpregs[CTX_FS1],
		rsap->rsa_fpregs[CTX_FS2], rsap->rsa_fpregs[CTX_FS3]);
	printf("s4=%16llx s5=%16llx s6=%16llx s7=%16llx\n",
		rsap->rsa_fpregs[CTX_FS4], rsap->rsa_fpregs[CTX_FS5],
		rsap->rsa_fpregs[CTX_FS6], rsap->rsa_fpregs[CTX_FS7]);
}

int
yieldattempt (nid_t mynid, kusharena_t * kusp, nid_t resumenid)
{
	printf ("resume_nid(%d %p %d)\n", mynid, kusp, resumenid);
	return resume_nid (mynid, kusp, resumenid);
}

void
exitstatus (void)
{
	int i;

/*   if (PRDA->t_sys.t_nid == 1) { */
/*   } */
	dump_kus (kusp);
}

void
sig_register (int sig, struct sigaction *action_p)
{
	if (sigaction (sig, action_p, NULL) != 0) {
		printf ("Unable to sig_register %s handler.\n", _sys_siglist[sig]);
		exit (0);
	}
}

void
dsiginfo (FILE * fp, siginfo_t * sip)
{
	register const char *c;
	struct siginfolist *listp;
	const char *colon;

	if (sip == NULL)
		return;

	fprintf (fp, "%s ", _sys_siglist[sip->si_signo]);
	if (SI_FROMUSER (sip)) {
		fprintf (fp, " ( from processes %d ) ", sip->si_pid);
	}
	else if ((listp = &_sys_siginfolist[sip->si_signo - 1])
		 && sip->si_code > 0 && sip->si_code <= listp->nsiginfo) {
		fprintf (fp, " %s ", listp->vsiginfo[sip->si_code - 1]);
		switch (sip->si_signo) {
		case SIGILL:
			fprintf (fp, " %x[0x%x] ",
				 *(__uint32_t *) sip->si_addr, sip->si_addr);
			break;
		case SIGSEGV:
		case SIGBUS:
		case SIGFPE:
			fprintf (fp, " [0x%x] ", sip->si_addr);
			break;
		default:
			break;
		}
	}
	fprintf (fp, "\n");
}

void
signal_handle (int signal, siginfo_t * siginfo, void *context)
{
	extern void *get_sp (void);
	register int i;

	dsiginfo (stdout, siginfo);
	printf ("sp %16p(%d) epc %16p sr %16p sigsp %16p\n",
		((ucontext_t *) context)->uc_stack.ss_sp,
		((ucontext_t *) context)->uc_stack.ss_size,
		((ucontext_t *) context)->uc_mcontext.gregs[CTX_EPC],
	    ((ucontext_t *) context)->uc_mcontext.gregs[CTX_SR], get_sp ());
	for (i = 1; i < 32; i += 4) {
		printf ("%2d: %16p %16p %16p %16p\n", i,
			((ucontext_t *) context)->uc_mcontext.gregs[i],
			((ucontext_t *) context)->uc_mcontext.gregs[i + 1],
			((ucontext_t *) context)->uc_mcontext.gregs[i + 2],
			((ucontext_t *) context)->uc_mcontext.gregs[i + 3]);
	}
	exit (-1);
}

void
bump_pc (int signal, siginfo_t * siginfo, void *context)
{
	register mcontext_t *cp = &((ucontext_t *) context)->uc_mcontext;

	printf ("Context bumped from 0x%llx", cp->gregs[CTX_EPC]);
	cp->gregs[CTX_EPC] += 4;
	printf (" to 0x%llx\n", cp->gregs[CTX_EPC]);
	return;
}

void
thread_start (void)
{
/*
   stack_t sigstack;
   sigstack.ss_sp = (char *)stackmem + SIGSTKSZ * (PRDA->t_sys.t_nid - 1);
   sigstack.ss_sp = (char *)sigstack.ss_sp + SIGSTKSZ - 1;
   sigstack.ss_size = SIGSTKSZ;
   sigstack.ss_flags = 0;

   if (sigaltstack(&sigstack, NULL) < 0) {
      printf("%d: sigaltstack - %s\n", PRDA->t_sys.t_nid, strerror(errno));
      exit(0);
   }
 */
	/*
	   {
	   struct sigaction action_s;
	   action_s.sa_flags = SA_SIGINFO | SA_ONSTACK;
	   action_s.sa_sigaction = signal_handle;
	   sig_register(SIGBUS, &action_s);
	   sig_register(SIGSEGV, &action_s);
	   }
	 */
	if (mustrun == 1) {
		if (PRDA->t_sys.t_nid == kusp->nrequested) {
			printf("[%d]: mustrun requested\n", PRDA->t_sys.t_nid);
		}
		if (sysmp(MP_MUSTRUN, PRDA->t_sys.t_nid-1) < 0) {
			perror("sysmp");
		}
	}
	single_context ();
	abort ();		/* NOTREACHED */
}

void
thread_monitor (void)
{
	volatile loop;

	while (1) {
		for (loop = 0; loop < 1000000; loop++);
		/*      dump_kus(kusp); */
	}
}

int
main (int argc, char *argv[])
{
	char *stackmem;
	int parallelism;

	parallelism = 2;
	mustrun = 0;
	parse:
	switch(getopt(argc, argv, "m")) {
		case 'm': mustrun=1; goto parse;
		case 't': parallelism=atoi(); goto parse;
		default: break;
	}
	if (atoi(argv[optind]) > 0) {
		parallelism = atoi(argv[optind]);
	}
	printf("parallelism = %d, mustrun = %d\n", parallelism, mustrun);
	
	/* prctl(PR_SETEXITSIG, SIGKILL); */
	/* __ateachexit(exitstatus); */
	{
		struct sigaction action_s;

		action_s.sa_flags = SA_SIGINFO;
		action_s.sa_handler = bump_pc;
		sig_register (SIGTRAP, &action_s);
	}
	if (usconfig (CONF_STHREADIOOFF) < 0) {
		perror ("while trying usconfig(CONF_STHREADIOOFF)");
		exit (1);
	}
#ifdef MONITOR
	parallelism++;
	printf("Using a monitor thread.\n");
#endif

	if (set_num_processors (parallelism, 1) == -1) {
		printf ("Unable to initalize thread count.\n");
		return -1;
	}
	printf("0x%p 0x%p\n", &kusp->nrequested, &kusp->rsa[0]);
	assert(kusp->nrequested == parallelism);
	setup_upcall((upcall_handler_t)&local_upcall_code, (greg_t)kusp,
		     (greg_t)&resume_nid, (greg_t)&run_nid, NULL);
	if (start_threads ((nanostartf_t) thread_start) == -1) {
		printf ("Unable to start thread.\n");
		return -1;
	}
	assert (PRDA->t_sys.t_nid == NULL_NID+1);
#ifdef MONITOR
	thread_monitor ();
#else
	thread_start ();
#endif

}

void
failtest (short nid, int64_t i, int64_t j, int64_t iteration, int section)
{
	if (j < -3) {
		printf ("failtest: iter %d PRDA->t_sys.t_nid %d register a2 %d does not make sense\n", iteration, PRDA->t_sys.t_nid, j);
	}
	else if (j == -3) {
		printf ("failtest: rbit set for running context nid %d PRDA->t_sys.t_nid %d\n", nid, PRDA->t_sys.t_nid);
	}
	else if (j == -2) {
		printf ("failtest: over thread test limit. nid %d nrequested %d kusp %p\n", nid, i, iteration);
	}
	else if (j == -1) {
		printf ("failtest: iter %d PRDA->t_sys.t_nid %d nid == a0 %d  a1 %d \n", iteration, PRDA->t_sys.t_nid, nid, i);
	}
	else if (j < 32) {
		printf ("failtest: iter %d nid %d section %d difference %d register %d(%x)\n", iteration, nid, section, i - nid, j, i + j * 0x100 + (section - 1));
	}
	else if (j < 64) {
		printf ("failtest: iter %d nid %d section %d difference %d register fp%d(%x)\n", iteration, nid, section, i - nid, j - 32, i + j * 0x100 + (section - 1));
	}
	else {
		printf ("failtest: iter %d (PRDA->t_sys.t_nid=%d) register a2=%d does not make sense\n", iteration, PRDA->t_sys.t_nid, j);
	}
	exit (-1);
}
