/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.17 $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/sysmp.h>
#include <sys/pda.h>
#include <sys/capability.h>

/*
 * mpadmin - allow access to sysmp system calls
 */
char	*Cmd;
int	flags;
int	nflag;	/* # of processors */
int	uflag;	/* unrestrict(ed) processor(s) */
int	rflag;	/* restrict(ed) processor(s) */
int	Iflag;	/* isolate(ed) processor(s) */
int	Uflag;	/* unisolate(ed) processor(s) */
int	cflag;	/* clock processor */
int	fflag;	/* fast clock processor */
int	sflag;	/* summary */
int	Cflag;	/* clock on */
int	Dflag;	/* clock off */

#define NOPROCN	(0xBADBEEF)
int	procn = NOPROCN;

static const cap_value_t cap_all[] = {CAP_SCHED_MGT, CAP_TIME_MGT};

static void Usage(int);
static int getprocn(char *);

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	register int	i;
	int		retval;
	int		nprocs;
	register struct pda_stat *pstatus, *p;
	cap_t ocap;

	Cmd = argv[0];

	while (++argv, --argc >= 1) {
		if ((*argv)[0] != '-')
			Usage(0);
		switch((*argv)[1]) {
		case 's':
			sflag++;
			flags++;
			break;
		case 'n':
			nflag++;
			flags++;
			break;
		case 'u':
			uflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'I':
			Iflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'U':
			Uflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'C':
			Cflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'D':
			Dflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'r':
			rflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'c':
			cflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		case 'f':
			fflag++;
			flags++;
			if ((*argv)[2] != '\0')
				procn = getprocn(&(*argv)[2]);
			break;
		default:
			Usage(0);
		}
	}

	if ((flags == 0) || (flags > 1))
		Usage(flags);

	/*
	 * If no processor was specified, we'll need status info
	 * on all the physically enabled processors.
	 */
	if (procn == NOPROCN) {
		nprocs = sysmp(MP_NPROCS);
		if (nprocs < 0) {
			perror("Status request failed");
			exit(-1);
		}
		pstatus = (struct pda_stat *)
			calloc(nprocs, sizeof(struct pda_stat));
		if (sysmp(MP_STAT, pstatus) < 0) {
			perror("Status request failed");
			exit(-1);
		}
	}

	if (nflag) {

		for (i = 0, p = pstatus; i < nprocs; i++) {
			printf("%d\n", p->p_cpuid);
			p++;
		}
		retval = nprocs;

	} else if (uflag) {

		/*
		 * Print out unrestricted processors if no procn arg given.
		 * Else enable processor procn.
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if (p->p_flags & PDAF_ENABLED) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(1, cap_all);
			retval = sysmp(MP_EMPOWER, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(EMPOWER) failed");
			}
		}
	} else if (rflag) {

		/*
		 * Print out restricted processors if no procn arg given.
		 * Else restrict processor procn.
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if (!(p->p_flags & PDAF_ENABLED)) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(1, cap_all);
			retval = sysmp(MP_RESTRICT, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(RESTRICT) failed");
			}
		}
	} else if (Cflag) {

		/*
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if ((p->p_flags & PDAF_NONPREEMPTIVE) == 0) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(1, cap_all);
			retval = sysmp(MP_PREEMPTIVE, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(PREEMPTIVE) failed");
			}
		}

	} else if (Dflag) {

		/*
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if ((p->p_flags & PDAF_NONPREEMPTIVE)) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(1, cap_all);
			retval = sysmp(MP_NONPREEMPTIVE, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(NONPREEMPTIVE) failed");
			}
		}



	} else if (Iflag) {

		/*
		 * Print out isolated processors if no procn arg given.
		 * Else isolate processor procn.
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if ((p->p_flags & PDAF_ISOLATED)) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(1, cap_all);
			retval = sysmp(MP_ISOLATE, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(ISOLATE) failed");
			}
		}

	} else if (Uflag) {

		/*
		 * Print out unisolated processors if no procn arg given.
		 * Else unisolate processor procn.
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if (p->p_flags & PDAF_ENABLED) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(1, cap_all);
			retval = sysmp(MP_UNISOLATE, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(UNISOLATE) failed");
			}
		}


	} else if (fflag) {

		/*
		 * Print out fast clock processor if no procn arg given.
		 * Else make processor procn the fast clock handler.
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if (p->p_flags & PDAF_FASTCLOCK) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(2, cap_all);
			retval = sysmp(MP_FASTCLOCK, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(FASTCLOCK) failed");
			}
		}
	} else if (cflag) {

		/*
		 * Print out clock processor if no procn arg given.
		 * Else make processor procn the clock handler.
		 */
		if (procn == NOPROCN) {
			retval = 0;
			for (i = 0, p = pstatus; i < nprocs; i++) {
				if (p->p_flags & PDAF_CLOCK) {
					printf("%d\n", p->p_cpuid);
					retval++;
				}
				p++;
			}
		} else {
			ocap = cap_acquire(2, cap_all);
			retval = sysmp(MP_CLOCK, procn);
			cap_surrender(ocap);
			if (retval < 0) {
				perror("sysmp(CLOCK) failed");
			}
		}

	} else if (sflag) {
		int notyet = 1;

		printf("processors:");
		for (i = 0, p = pstatus; i < nprocs; i++) {
			printf(" %d", p->p_cpuid);
			p++;
		}
		printf("\nunrestricted:");
		for (i = 0, p = pstatus; i < nprocs; i++) {
			if (p->p_flags & PDAF_ENABLED) {
				printf(" %d", p->p_cpuid);
			}
			p++;
		}
		printf("\nisolated:");
		for (i = 0, p = pstatus; i < nprocs; i++) {
			if ((p->p_flags & PDAF_ISOLATED)) {
				printf(" %d", p->p_cpuid);
			}
			p++;
		}
		printf("\nrestricted:");
		for (i = 0, p = pstatus; i < nprocs; i++) {
			if (!(p->p_flags & PDAF_ENABLED)) {
				printf(" %d", p->p_cpuid);
			}
			p++;
		}
		printf("\npreemptive:");
		for (i = 0, p = pstatus; i < nprocs; i++) {
			if (!(p->p_flags & PDAF_NONPREEMPTIVE)) {
				printf(" %d", p->p_cpuid);
			}
			p++;
		}
		printf("\nclock:");
		for (i = 0, p = pstatus; i < nprocs; i++) {
			if (p->p_flags & PDAF_CLOCK) {
				printf(" %d", p->p_cpuid);
			}
			p++;
		}
		for (i = 0, p = pstatus; i < nprocs; i++) {
			if (p->p_flags & PDAF_FASTCLOCK) {
				if (notyet) {
					printf("\nfast clock:");
					notyet = 0;
				}
				printf(" %d", p->p_cpuid);
			}
			p++;
		}
		if (notyet)
			printf("\nNo fast clock");
		printf("\n");
		retval = nprocs;
	}

	return(retval);
}

static void
Usage(int flags)
{
	fprintf(stderr,
		"Usage: %s: -s | -n | -u[proc] | -r[proc] | -c[proc] | -f[proc] | -I[proc] -U[proc] -D[proc] -C[proc]\n",
		Cmd);
	if (flags)
		fprintf(stderr, "	Only one argument allowed.\n");
	exit(-1);
}

static int
getprocn(char *s)
{
	int	n = atoi(s);

	while (isspace(*s))
		s++;
	n = atoi(s);
	return(n);
}
