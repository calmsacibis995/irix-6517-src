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

#ident	"$Revision: 1.10 $"

/*
**	run multiple scrands to force swapping and swapouts
*/

#define	_KMEMUSER	/* to wake up some kernel typedefs for swap.h */

#include	<stdio.h>
#include	<ctype.h>
#include	<fcntl.h>
#include	<string.h>
#include	<limits.h>
#include	<stdlib.h>
#include	"sys/types.h"
#include	"sys/errno.h"
#include	"sys/immu.h"
#include	"sys/sysmp.h"
#include	<sys/param.h>
#include	<dirent.h>
#include	<sys/swap.h>
#include	<sys/stat.h>
# include	<sys/syssgi.h>

static struct pageconst pageconst;
#undef _PAGESZ
#undef PNUMSHFT
#define _PAGESZ		pageconst.p_pagesz
#define PNUMSHFT	pageconst.p_pnumshft

int rminfosz;
long physmem;
int nprocs;
#define	MAXMMAP	(2*1024*1024*1024LL)
char	*argbuf[16];
char	buf[20];
char	*Cmd;

main(argc, argv)
int argc;
char *argv[];
{
	extern	errno;
	extern	char *sys_errlist[];
	struct rminfo *rminfo;
	register int i;
	int	pid, domapped = 0;

	if (syssgi(SGI_CONST, SGICONST_PAGESZ, &pageconst,
						sizeof(pageconst), 0) == -1) {
		perror("syssgi: pageconst");
		exit(1);
	}
	if ((Cmd = strrchr(argv[0], '/')) == NULL)
		Cmd = argv[0];
	else
		Cmd++;
	if (strcmp(Cmd, "nmwalk") == 0)
		domapped++;
	if ((rminfosz = sysmp(MP_SASZ, MPSA_RMINFO)) == -1) {
		fprintf(stderr, "%s: cannot get meminfo sz\n", argv[0]);
		exit(2);
	}
	if ((rminfo = (struct rminfo *)malloc(rminfosz)) == NULL) {
		fprintf(stderr, "can't malloc space for rminfo struct\n");
		exit(2);
	}
	sysmp(MP_SAGET, MPSA_RMINFO, (char *)rminfo, rminfosz);
	physmem = rminfo->physmem;
	/* grow up into swap */
	physmem += swaptot()/3;
	if (argc < 2) {
		/* ICK XXX - check for over 2gb, won't currently work */
		if ((long long)physmem / 4 * getpagesize() >= MAXMMAP)
			nprocs = (physmem * getpagesize() + MAXMMAP - 1) / MAXMMAP;
		else
			nprocs = 4;
	} else {
		nprocs = atoi(argv[1]);
	}
	fprintf(stderr, "%s: using %d processes, %ld total memory (pages) %s using mapped files\n",
		argv[0], nprocs, physmem, domapped ? "" : "not");
	physmem /= nprocs;
	physmem *= getpagesize();
	sprintf(buf, "%ld", physmem);
	argbuf[0] = "./mwalk";
	argbuf[1] = "-s";
	argbuf[2] = buf;
	argbuf[3] = "-l";
	argbuf[4] = "1.8";
	argbuf[5] = "-c";
	argbuf[6] = "20000";
	argbuf[7] = "-n";
	argbuf[8] = "1";
	argbuf[9] = "-S";
	argbuf[10] = "4";
	argbuf[11] = "-w";
	argbuf[12] = "1000";
	if (domapped) {
		argbuf[13] = "-m";
		argbuf[14] = (char *)0;
	} else
		argbuf[13] = (char *)0;

	fprintf(stderr, "%s: execing %d processes\n", argv[0], nprocs);
	for (i = 0; i < nprocs; i++) {
		if (i == 0)
			argbuf[8] = "3";
		else
			argbuf[8] = "1";
		pid = fork();
		switch (pid) {

		case 0:
			execv(argbuf[0], argbuf);
			fprintf(stderr, "%s\n", sys_errlist[errno]);
			exit(2);
			break;
		case -1:
			fprintf(stderr, "%s: fork failed\n", argv[0]);
			break;
		default:
			break;
		}

		sleep(2);
	}
	while ((wait((int *)0) != -1) && errno != ECHILD)
		;

	errno = 0;
}

/*
 * Routine to find total swap space.
 */
swaptot()
{
	int nswap;

	if (swapctl(SC_GETSWAPMAX, &nswap) != 0) {
		perror("swapctl(GETSWAPMAX)");
		return(0);
	}
	return(dtop(nswap));
}
