/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * ftimer.c - handle fast clock and fast itimers
 */
#ident "$Revision: 1.10 $"

#include <stdio.h>
#include <errno.h>
#include "sys/types.h"
#include "sys/pda.h"
#include "sys/syssgi.h"
#include "sys/sysmp.h"

main(argc, argv)
int argc;
char **argv;
{
	extern char *optarg;
	extern int optind;
	extern int errno;
	int c;
	int option = 0;

	if (argc == 1) {
		printstatus();
		exit(0);
	}
	Usage();
	/* NOT REACHED */
}

Usage()
{
	fprintf(stderr, "Usage: ftimer\n");
	exit(-1);
}


printstatus()
{
	register int	i;
	int		nprocs;
	register struct pda_stat *pstatus, *p;

	nprocs = sysmp(MP_NPROCS);
	if (nprocs < 0) 
		goto ftimer_err;
	pstatus = (struct pda_stat *)
		calloc(nprocs, sizeof(struct pda_stat));
	if (sysmp(MP_STAT, pstatus) < 0) 
		goto ftimer_err;
	for (i = 0, p = pstatus; i < nprocs; i++) {
		if (p->p_flags & PDAF_FASTCLOCK) {
			printf("Fast clock is on processor %d\n", p->p_cpuid);
			break;
		}
		p++;
	}
	if (i >= nprocs)
		printf("Fast clock is off\n");
	if ((i = syssgi(SGI_QUERY_FTIMER)) < 0)
		goto ftimer_err;
	if (i == 0)
		printf("There are currently no outstanding fast timeouts\n");
	else
		printf("There are outstanding fast timeouts\n");
	return;
ftimer_err:
	perror("ftimer: status request failed");
	exit(-1);
}
