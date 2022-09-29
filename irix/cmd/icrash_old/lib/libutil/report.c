#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/report.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/dump.h>

#include "icrash.h"
#include "extern.h"

/*
 * punderline() -- Draw an underline under a buffer.
 */
void
punderline(char *buf, FILE *ofp)
{
	int i;

	for (i = 0; i < strlen(buf); i++) {
		fprintf(ofp, "=");
	}
	fprintf(ofp, "\n");
}

/*
 * prline() -- Outline a string.
 */
void
prline(char *buf, FILE *ofp)
{
	punderline(buf, ofp);
	fprintf(ofp, "%s\n", buf);
	punderline(buf, ofp);
}

/*
 * get_putbuf() -- Get the putbuf into a temporary buffer.
 */
char *
get_putbuf()
{
	int i, j;
	char *np, *pbuf;

	if (K_DUMP_HDR(K)->dmp_version > 2) {
		np = (char *)alloc_block(K_DUMP_HDR(K)->dmp_putbuf_size+500, B_TEMP);
		pbuf = (char *)alloc_block(K_DUMP_HDR(K)->dmp_putbuf_size+1, B_TEMP);
		if (LSEEK(K, K_CORE_FD(K), K_DUMP_HDR(K)->dmp_hdr_sz, SEEK_SET) < 0) {
			return ((char *)NULL);
		}
		read(K_CORE_FD(K), (void *)pbuf, K_DUMP_HDR(K)->dmp_putbuf_size);
		for (i = 0, j = 0; i < K_DUMP_HDR(K)->dmp_putbuf_size; i++) {
			if (pbuf[i] == '\0') {
				/* skip all the NULLs */
			} 
			else {
				np[j++] = pbuf[i];
			}
		}
	} else {
		np = (char *)alloc_block(1500, B_TEMP);
		pbuf = (char *)alloc_block(1001, B_TEMP);
		kl_get_block(K, K_PUTBUF(K), 1000, pbuf, "putbuf");
		for (i = 0, j = 0; i < 1000; i++) {
			if (pbuf[i] == '\0') {
				/* skip all the NULLs */
			} 
			else {
				np[j++] = pbuf[i];
			}
		}
	}
	np[j] = '\0';
	free_block((k_ptr_t)pbuf);
	return(np);
}

/*
 * get_putbuf_eframe() -- 
 *
 *   Grab the EP address from the putbuf, if it exists. Otherwise,
 *   return 0 (not -1).
 */
kaddr_t
get_putbuf_eframe()
{
	int j = 0;
	kaddr_t value;
	char *np, *ptr, addrbuf[20];

	np = get_putbuf();
	if (ptr = strstr(np, "ep: 0x")) {
		ptr += 6;
		while ((((*ptr >= '0') && (*ptr <= '9')) ||
			((*ptr >= 'a') && (*ptr <= 'f'))) && (j < 20)) {
				addrbuf[j++] = *ptr++;
		}
		free_block((k_ptr_t)np);
		GET_VALUE(addrbuf, &value);
		return(value);
	}
	free_block((k_ptr_t)np);
	return(0);
}

/*
 * get_putbuf_pc() -- 
 * 
 *   Grab the PC address from the putbuf, if it exists. Otherwise,
 *   return 0 (not -1).
 */
kaddr_t
get_putbuf_pc()
{
	int j = 0;
	kaddr_t value;
	char *np, *ptr, addrbuf[20];

	np = get_putbuf();
	if (ptr = strstr(np, "PC: 0x")) {
		ptr += 4;
		addrbuf[0] = 0;
		strcat(addrbuf, "0x");
		ptr += 2;
		j = 2;
		while ((((*ptr >= '0') && (*ptr <= '9')) ||
			((*ptr >= 'a') && (*ptr <= 'f'))) && (j < 20)) {
				addrbuf[j++] = *ptr++;
		}
		addrbuf[j] = 0;
		free_block((k_ptr_t)np);
		GET_VALUE(addrbuf, &value);
		return(value);
	}
	free_block((k_ptr_t)np);
	return(0);
}

/*
 * get_putbuf_cpu() -- 
 * 
 *   Get the CPU that crashed from the putbuf, if it exists.
 *   Otherwise, return -1.
 */
int
get_putbuf_cpu()
{
	int i, j = 0;
	char *np, *ptr;

	np = get_putbuf();
	if (ptr = strstr(np, "PANIC: CPU ")) {
		ptr += 11;
		sscanf(ptr, "%d", &i);
		free_block((k_ptr_t)np);
		if ((i < 0) || (i >= K_MAXCPUS(K))) {
			return(-1);
		} 
		else {
			return(i);
		}
	}
	free_block((k_ptr_t)np);
	return(-1);
}

/*
 * get_putbuf_nmi() -- Get the NMI string from the putbuf, if
 *                     it exists.  Otherwise, return -1.
 */
int
get_putbuf_nmi()
{
	int i, j = 0;
	char *np, *ptr;

	np = get_putbuf();
	if (ptr = strstr(np, "NMI")) {
		free_block((k_ptr_t)np);
		return(1);
	}
	free_block((k_ptr_t)np);
	return(-1);
}

/*
 * print_repkthread_info() -- Print out report kthread information.
 */
void
print_repkthread_info(kaddr_t curkthread, k_ptr_t ktp, int i, FILE *ofp)
{
	k_ptr_t cp, kt_name;

	cp = alloc_block(CRED_SIZE(K), B_TEMP);
	if (!curkthread) {
		fprintf(ofp, " CPU %d did not have a kthread running.\n", i);
	} 
#ifdef ANON_ITHREADS
	else if (IS_ITHREAD(K, ktp)) {
		if (kt_name = kl_kthread_name(K, ktp)) {
			fprintf(ofp, " The ithread '%s' was running.\n", kt_name);
		}
		else {
			fprintf(ofp, " An ithread was running.\n");
		}
	}
#endif
	else if (IS_STHREAD(K, ktp)) {
		if (kt_name = kl_kthread_name(K, ktp)) {
			fprintf(ofp, " The sthread '%s' was running.\n", kt_name);
		}
		else {
			fprintf(ofp, " An sthread was running.\n");
		}
	}
	else if (IS_UTHREAD(K, ktp)) {
		fprintf(ofp, " The command '%s' was running.", kl_kthread_name(K, ktp));
		if (kl_get_struct(K, kl_kaddr(K, ktp, "proc", "p_cred"), 	
							 					CRED_SIZE(K), cp, "cred")) {
			fprintf(ofp, " under UID %lld\n", KL_INT(K, cp, "cred", "cr_ruid"));
		} 
		else {
			fprintf(ofp, "\n");
		}
	}
	free_block(cp);
}

/*
 * get_report_pda() -- Get PDA information for a process.
 */
k_ptr_t
get_report_pda(int i, int *stkflg, kaddr_t *curkthread)
{
	k_ptr_t pdap;
	kaddr_t pdaval;

	*stkflg = 0;
	*curkthread = 0;

	pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
	pdaval = kl_get_pda_s(K, (kaddr_t)i, pdap);
	if (KL_ERROR) {
		free_block(pdap);
		return((k_ptr_t)NULL);
	}
	else {
		*stkflg = kl_uint(K, pdap, "pda_s", "p_kstackflag", 0);
		*curkthread = kl_kaddr(K, pdap, "pda_s", "p_curkthread");
	}
	return(pdap);
}

/*
 * get_report_kthread()
 */
k_ptr_t
get_report_kthread(kaddr_t curkthread)
{
	if (!curkthread) {
		return((k_ptr_t)NULL);
	}
	return(kl_get_kthread(K, curkthread, 0));
}

/*
 * free_report_fields() -- 
 *
 *   Free the appropriate fields assigned to each pointer (if any).
 */
void
free_report_fields(k_ptr_t pdap, k_ptr_t kthreadp)
{
	if (pdap) {
		free_block(pdap);
	}

	if (kthreadp) {
		free_block(kthreadp);
	}
}

/*
 * print_cpudata() -- Print out information specific to each CPU  
 *
 *   This gives the caller a summary report of what was going on for
 *   each CPU when the panic/NMI occurred.
 */
void
print_cpudata(FILE *ofp)
{
	int i;

	for (i = 0; i < K_MAXCPUS(K); i++) {
		print_cpu_status(i, ofp);
	}
}

/*
 * regular_report() -- Report CPU information on an regular dump.
 */
void
regular_report(command_t cb, FILE *ofp)
{
	char buf[128];
	int i, stkflg, cpunum, first_run = TRUE;
	k_ptr_t pdap, ktp;
	kaddr_t eaddr, pc, curkthread;

	/* Grab the eframe data (if any).  We call out to a function
	 * for now, because we don't have this information stored
	 * anywhere, except in the putbuf (which means we have to scan
	 * it out by hand!
	 */
	fprintf(cb.ofp, "STACK TRACE:\n\n");
	get_cmd(&cb, "trace");
	checkrun_cmd(cb, ofp);
	fprintf(cb.ofp, "\n");

	if (pc = get_putbuf_pc()) {
		fprintf(cb.ofp, "INSTRUCTIONS AT EXCEPTION:\n\n");
		sprintf(buf, "dis -f 0x%llx-32 8", pc);
		get_cmd(&cb, buf);
		checkrun_cmd(cb, ofp);
		sprintf(buf, "dis -f 0x%llx", pc);
		get_cmd(&cb, buf);
		checkrun_cmd(cb, ofp);
		sprintf(buf, "dis -f 0x%llx+4 8", pc);
		get_cmd(&cb, buf);
		checkrun_cmd(cb, ofp);
		fprintf(cb.ofp, "\n");
	}

	for (i = 0; i < K_MAXCPUS(K); i++) {
		pdap = get_report_pda(i, &stkflg, &curkthread);
		ktp = get_report_kthread(curkthread);
		if (pdap) {
			sprintf(buf, "CRASH SUMMARY FOR CPU %d", i);
			prline(buf, cb.ofp);
			fprintf(cb.ofp, "\n");
			if (KL_ERROR) {
				kl_print_error(K);
			}
			else {
				print_repkthread_info(curkthread, ktp, i, cb.ofp);
				cpu_trace((cpuid_t)i, cb.flags, cb.ofp);
			}
			fprintf(cb.ofp, "\n");
		}
		free_report_fields(pdap, ktp);
	}
}

/*
 * nmi_report() -- Report CPU information on an NMI dump.
 */
void
nmi_report(command_t cb, FILE *ofp)
{
	int i, stkflg;
	char buf[128];
	k_ptr_t pdap = 0, kthreadp = 0;
	kaddr_t spage, epc, sp, errorepc, ra, curkthread, hold_defkthread;

	hold_defkthread = K_DEFKTHREAD(K);
	for (i = 0; i < K_MAXCPUS(K); i++) {

		pdap = get_report_pda(i, &stkflg, &curkthread);
		if (KL_ERROR) {
			if (KL_ERROR != KLE_NO_CPU) {
				kl_print_error(K);
			}
			continue;
		}

		sprintf(buf, "NMI SUMMARY FOR CPU %d", i);
		prline(buf, cb.ofp);
		fprintf(cb.ofp, "\n");

		if (IS_NMI(K)) {

			kl_NMI_saveregs(K, i, &errorepc, &epc, &sp, &ra);

			fprintf(cb.ofp, "REGISTERS FOR CPU %d:\n\n"
				" ERREPC: 0x%llx\n"
				"     SP: 0x%llx\n"
				"     RA: 0x%llx\n\n", i, errorepc, sp, ra);
		}

		if (pdap) {
			fprintf(cb.ofp, "LTICKS FOR CPU %d: %lld\n\n", 
				i, KL_INT(K, pdap, "pda_s", "p_lticks"));
		}

		fprintf(cb.ofp, "STACK TRACE FOR CPU %d:\n\n", i);

		if (curkthread) {
			kthreadp = get_report_kthread(curkthread);
			if (KL_ERROR) {
				fprintf(ofp, "NOTICE: ");
				kl_print_error(K);
			}
			else if (stkflg == 0) {
				fprintf(cb.ofp,
					" No stack trace. CPU %d was running in user mode.\n", i);
				continue;
			}
			else {
				K_DEFKTHREAD(K) = curkthread;
				print_repkthread_info(curkthread, kthreadp, i, cb.ofp);
			}
			fprintf(cb.ofp, "\n");
		}
		if (cpu_trace(i, C_SUPRSERR, cb.ofp)) {
			fprintf(cb.ofp, " Could not find a valid stack trace for "
				"CPU %d\n", i);
		}
		fprintf(cb.ofp, "\n");

		/* Next, print out the disassembly information.  Note that this is
		 * based on the ErrorEPC register value, not the EPC.
		 */
		fprintf(cb.ofp, "INSTRUCTIONS NEAR PC IN NMI FOR CPU %d:\n\n", i);
		if (stkflg == 0) {
			fprintf(cb.ofp,
				" CPU %d was running in user mode and does not "
				"have a PC to disassemble.\n", i);
		} 
		else if (!errorepc) {
			fprintf(cb.ofp,
				" No valid ERROR_EPC found in NMI registers.\n");
		} 
		else {
			sprintf(buf, "dis -f %llx-32 8", errorepc);
			get_cmd(&cb, buf);
			checkrun_cmd(cb, ofp);
			sprintf(buf, "dis -f %llx", errorepc);
			get_cmd(&cb, buf);
			checkrun_cmd(cb, ofp);
			sprintf(buf, "dis -f %llx+4 8", errorepc);
			get_cmd(&cb, buf);
			checkrun_cmd(cb, ofp);
		}
		fprintf(cb.ofp, "\n");
		free_report_fields(pdap, kthreadp);
		kthreadp = (k_ptr_t)NULL;
	}

	K_DEFKTHREAD(K) = hold_defkthread;
	fprintf(cb.ofp, "\n");
	prline("SLEEPING PROCESS STATES", cb.ofp);
	fprintf(cb.ofp, "\n");
	get_cmd(&cb, "slpproc");
	checkrun_cmd(cb, ofp);
}

/*
 * availmon_report() -- Availability information report.
 */
int
availmon_report()
{
	FILE *afp, *tfp;
	command_t cmd;

	/* Don't dump a report if we don't request one!
	 */
	if ((!availmon) || (!availmonfile)) {
		return(1);
	}

	/* Open up the availmon report file, dump the information
	 * requested, close up the file and return.  In this particular
	 * case, we always obliterate any previous availmon report.
	 */
	if ((afp = fopen(availmonfile, "a")) == (FILE *)NULL) {
		return(0);
	}
	fprintf(afp, "    CRASH TIME: %d %s",
		K_DUMP_HDR(K)->dmp_crash_time, ctime(&(K_DUMP_HDR(K)->dmp_crash_time)));
	fprintf(afp, "    PANIC STRING: %s\n", K_DUMP_HDR(K)->dmp_panic_str);

	/* If we can't close the file for some reason, we probably are
	 * going to fail with the analysis report to.  Don't bother, just
	 * exit out here.
	 */
	if (fclose(afp)) {
		return(1);
	}
	return(0);
}

/*
 * dump_header_report() -- 
 *
 *   Create a dump header report when everything else fails. We do
 *   what uncompvm -h would do.
 */
int
dump_header_report(FILE *ofp)
{
	command_t cb;

	if (writeflag) {
		cb.ofp = ofp;
	} 
	else {
		cb.ofp = stdout;
	}

	fprintf(cb.ofp,
		"No dump data found.  Printing dump header and exiting.\n\n");
	expand_header(K_DUMP_HDR(K), cb.ofp);

	if (writeflag) {
		return (fclose(cb.ofp));
	}
	return(0);
}

/*
 * base_report() -- The basic reporting information.
 */
void
base_report(command_t cb, char *p, FILE *ofp)
{
	char *ptr;
	time_t tloc;
	struct stat st;
	struct syment *SBE;

	/* Give out general icrash information.
	 */
	fprintf(cb.ofp, "\n");
	prline("ICRASH CORE FILE REPORT", cb.ofp);
	fprintf(cb.ofp, "\nSYSTEM:\n");
	print_utsname(cb.ofp);

	tloc = time((time_t *)0);
	fprintf(cb.ofp, "\nGENERATED ON:\n    %s", ctime(&tloc));

	fprintf(cb.ofp, "\nTIME OF CRASH:\n    %d %s",
		K_DUMP_HDR(K)->dmp_crash_time, ctime(&(K_DUMP_HDR(K)->dmp_crash_time)));

	fprintf(cb.ofp, "\nPANIC STRING:\n    %s\n", K_DUMP_HDR(K)->dmp_panic_str);

	if (stat(namelist, &st) < 0) {
		fprintf(cb.ofp, "\nNAMELIST:\n    %s\n\n", namelist);
	} 
	else {
		ptr = ctime(&(st.st_ctime));
		ptr[strlen(ptr)-1] = '\0';
		fprintf(cb.ofp, "\nNAMELIST:\n    %s [CREATE TIME: %s]\n\n",
			namelist, ptr);
	}

	if (stat(corefile, &st) < 0) {
		fprintf(cb.ofp, "COREFILE:\n    %s\n", corefile);
	} 
	else {
		ptr = ctime(&(st.st_ctime));
		ptr[strlen(ptr)-1] = '\0';
		fprintf(cb.ofp, "COREFILE:\n    %s [CREATE TIME: %s]\n\n",
			corefile, ptr);
	}

	/* Dump out any summary report we can give on the dump.
	 */
	prline("COREFILE SUMMARY", cb.ofp);
	if (IS_NMI(K) && (get_putbuf_nmi() > 0)) {
		fprintf(cb.ofp, "\n    The system was brought down ");
		fprintf(cb.ofp, "in response to a user initiated NMI.\n\n");
	} 
	else if (ACTIVE(K)) {
		fprintf(cb.ofp, "\n    The system is currently active.\n\n");
	} 
	else {
		fprintf(cb.ofp, "\n    The system was brought down ");
		fprintf(cb.ofp, "due to an internal panic.\n\n");
	}

	/* General putbuf data.
	 */ 
	prline("PUTBUF DUMP", cb.ofp);
	print_putbuf(cb.ofp);
	if (K_ERROR_DUMPBUF(K)) {
		fprintf(cb.ofp, "\n\nERROR DUMPBUF (HARDWARE ERROR STATE):\n\n");
		fprintf(cb.ofp, "0x%llx = \"", K_ERROR_DUMPBUF(K));
		dump_string(K_ERROR_DUMPBUF(K), cb.flags, cb.ofp);
		fprintf(cb.ofp, "\"");
	}
	fprintf(cb.ofp, "\n\n");

	/* Single bit error information, if available.
	 */
	if ((K_IP(K) == 19) || (K_IP(K) == 21) || (K_IP(K) == 25)) {
		SBE = get_sym("sbe_log_errors", B_TEMP); 
		if (!KL_ERROR) {
			if (SBE->n_value != 0) {
				prline("SINGLE BIT ERROR LOGGING INFORMATION", cb.ofp);
				get_cmd(&cb, "sbe");
				checkrun_cmd(cb, ofp);
				fprintf(cb.ofp, "\n");
			}
			free_sym(SBE);
		}
	}

	/* Print out general CPU information.
	 */
	prline("CPU SUMMARY", cb.ofp);
	fprintf(cb.ofp, "\n");
	print_cpudata(cb.ofp);
	fprintf(cb.ofp, "\n");

	/* If we have an NMI, we perform some extra actions.  For the most
	 * part, this will entail grabbing proc information, along with
	 * trying to get specific NMI trace data (if any).
	 */
	if (IS_NMI(K)) {
		nmi_report(cb, ofp);
	} 
	else {
		regular_report(cb, ofp);
	}
}
