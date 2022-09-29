/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)profil-3b5:prfpr.c	1.6" */
#ident "$Revision: 1.12 $"
/*
 *	prfpr - print profiler log files
 *
 * Format of profiler file:
 *	prfmax		(int) # of symbols
 *	nprocs		(int) max # of cpus
 *	symbols		(prfmax symaddr_t) symbol addresses
 *	timestamp	(time_t) time stamp
 *	cpuid		(int)
 *	ctrs		(prfmax ints) - actual counters
 * 	cpuid		(int)
 *	.
 *	.
 *	timestamp
 */

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "prf.h"

static struct	profile	{
	time_t	p_date;		/* time stamp of record */
	int	*p_cpuid;	/* cpuid */
	int	*p_ctr;		/* profiler counter values */
} p[2];

static int prfmax;		/* actual number of text symbols */
static int nprocs;		/* # of processors worth of info */
static double cutoff = 1.0;	/* percentage cutoff */
static double pc;
static double spc = 0.0;

static symaddr_t *t;

static char *namelist = "/unix";
static char *logfile;

static int *sum, *osum;

typedef struct {
	char		*name;
	symaddr_t	addr;
} syment_t;

static int symcnt;			/* number of symbols */
static int symlistsize;			/* size of malloc'ed memory for list */
static syment_t *symlist;		/* list of symbols */
static syment_t *symnext;		/* next free slot */

static void shtime(time_t *);

static void listinit(void);
static void listadd(char *, symaddr_t);
static void printname(syment_t *);
static syment_t *search(symaddr_t);

extern int  rdsymtab(int, void (*)(char *, symaddr_t));
extern int  rdelfsymtab(int, void (*)(char *, symaddr_t));

int
main(int argc, char **argv)
{
	register int ff, log, i, j;
	register int nsamp;
	register int icutoff;
	register int *nctr, *octr;
	int filesampsz;
	int fd;
	syment_t *sp;
	int dwarfsyms;

	switch(argc) {
		case 4:
			namelist = argv[3];
		case 3:
			cutoff = atof(argv[2]);
		case 2:
			logfile = argv[1];
			break;
		default:
			error("usage: prfpr file [ cutoff [ namelist ] ]");
	}
	if ((log = open(logfile, O_RDONLY)) < 0)
		error("cannot open data file");
	if((fd = open(namelist, O_RDONLY)) < 0)
		error("cannot open namelist file");
	if (cutoff > 100.0 || cutoff < 0.0)
		error("invalid cutoff percentage");
	if (read(log, &prfmax, sizeof prfmax) != sizeof prfmax || prfmax == 0)
		error("bad data file (no prfmax)");
	if (read(log, &nprocs, sizeof nprocs) != sizeof nprocs || nprocs == 0)
		error("bad data file (no nprocs)");

	/* alloc space for addresses */
	if ((t = (symaddr_t *) malloc(prfmax * sizeof(symaddr_t))) == 0)
		error("cannot allocate space for addresses");

	filesampsz = prfmax * (int) sizeof(int);

	/* alloc space for counters - 2 sets */
	if ((p[0].p_ctr = (int *) malloc(nprocs * filesampsz)) == 0)
		error("cannot allocate space for counters");
	if ((p[1].p_ctr = (int *) malloc(nprocs * filesampsz)) == 0)
		error("cannot allocate space for counters");

	/* alloc space for cpuids - 2 sets */
	if ((p[0].p_cpuid = (int *) malloc(nprocs * sizeof(int))) == 0)
		error("cannot allocate space for counters");
	if ((p[1].p_cpuid = (int *) malloc(nprocs * sizeof(int))) == 0)
		error("cannot allocate space for counters");

	/* alloc sum counters */
	if ((osum = (int *) malloc(nprocs * sizeof(int))) == 0)
		error("cannot allocate space for sums");
	if ((sum = (int *) malloc(nprocs * sizeof(int))) == 0)
		error("cannot allocate space for sums");

	if (read(log, t, prfmax * sizeof (symaddr_t))
	     != prfmax * sizeof (symaddr_t))
		error("cannot read profile addresses");

	ff = 0;
	for (j = 0; j < nprocs; j++)
		osum[j] = sum[j] = 0;

	if (read(log, &p[!ff].p_date, sizeof(time_t)) != sizeof(time_t))
		error("error reading time stamp");

	/* calc total # of samples */
	for (j = 0; j < nprocs; j++) {
		nctr = &(p[!ff].p_ctr[j * prfmax]);
		/* read in first buffer full */
		if (read(log, &p[!ff].p_cpuid[j], sizeof(int)) != sizeof(int))
			error("error reading cpuid");
		if (read(log, nctr, filesampsz) != filesampsz)
			error("error reading counters");

		for (i = 0; i < prfmax; i++)
			osum[j] += *nctr++;
	}

	system("uname -a");

	listinit();
	dwarfsyms = rdsymtab(fd, listadd);
	if( rdelfsymtab(fd, listadd) && dwarfsyms ) {
		printf("%s: unable to load dwarf/mdebug or elf symbol \
table information\n", argv[0]);
		exit(1);
	}

	/* integerize cutoff - we keep 100* percentage
	 * 0 -> ALL samples
	 */
	icutoff = (int)(100 * cutoff);

	for(;;) {
		for (j = 0; j < nprocs; j++)
			sum[j] = 0;
		if (read(log, &p[ff].p_date, sizeof(time_t)) != sizeof(time_t)) {
			break;
		}

		printf("\n");
		shtime(&p[!ff].p_date);
		shtime(&p[ff].p_date);
		for (j = 0; j < nprocs; j++) {
		    nctr = &(p[ff].p_ctr[j * prfmax]);
		    if (read(log, &p[ff].p_cpuid[j], sizeof(int)) != sizeof(int))
			    error("error reading cpuid");
		    if (read(log, nctr, filesampsz) != filesampsz)
			    break;
		    for (i = 0; i < prfmax; i++)
			    sum[j] += *nctr++;
		    if (sum[j] == osum[j]) {
			    printf("CPU %d no samples\n\n", p[ff].p_cpuid[j]);
			    continue;
		    }
		    if (p[ff].p_cpuid[j] != p[!ff].p_cpuid[j])
			error("CPU #'s changed!");
		    printf("\nCPU %d - %d total samples; cutoff %f\n",
			    p[ff].p_cpuid[j], sum[j] - osum[j], cutoff);
		    nctr = &(p[ff].p_ctr[j * prfmax]);
		    octr = &(p[!ff].p_ctr[j * prfmax]);
		    for (i = 0; i < prfmax; i++) {
			nsamp = *nctr++ - *octr++;
			if (nsamp == 0)
				continue;
			pc = 10000.0 * nsamp;
			pc = pc / (sum[j] - osum[j]);
			if(pc > icutoff || icutoff == 0) {
				sp = search(t[i]);
				if(sp == 0)
					printf("unknown  ");
				else {
					printname(sp);
					printf(" ");
				}
				printf("%.4f\n", pc/100);
				spc += pc;
			}
		    }
		}
		ff = !ff;
		for (j = 0; j < nprocs; j++)
			osum[j] = sum[j];
		printf("Total                    %.2f\n", spc/100);
		spc = 0.0;
	}

	exit(0);
	/* NOTREACHED */
}

void
error(char *s)
{
	printf("error: %s\n", s);
	exit(1);
}

static void
shtime(time_t *l)
{
	register  struct  tm  *t;

	if(*l == (time_t) 0) {
		printf("initialization\n");
		return;
	}
	t = localtime(l);
	printf("%02.2d/%02.2d/%02.2d %02.2d:%02.2d\n", t->tm_mon + 1,
		t->tm_mday, t->tm_year%100, t->tm_hour, t->tm_min);
}

#define	LISTINCR	6000

static void
listinit(void)
{
	symlist = (syment_t *)malloc(LISTINCR * sizeof (syment_t));
	if (symlist == NULL)
		error("cannot allocate memory");
	symnext = symlist;
	symcnt = 0;
	symlistsize = LISTINCR;
}

/* ARGSUSED */
static void
listadd(char *name, symaddr_t addr)
{
	if (symcnt >= symlistsize) {
		symlistsize += LISTINCR;
		symlist = (syment_t *)
			realloc(symlist, symlistsize * sizeof(syment_t));
		if (symlist == NULL)
			error("cannot allocate memory");
		symnext = &symlist[symcnt];
	}
	symnext->name = name;
	symnext->addr = addr;
	symnext++;
	symcnt++;
}

static syment_t *
search(symaddr_t addr)
{
	register syment_t *sp;
	register syment_t *save;
	symaddr_t value;

	value = 0;
	save = 0;
	for(sp = symlist; sp < &symlist[symcnt]; sp++) {
		if(sp->addr <= addr && sp->addr > value) {
			value = sp->addr;
			save = sp;
		}
	}
	return(save);
}

int
list_repeated(symaddr_t addr)
{
	register syment_t *sp;

	for(sp = symlist; sp < &symlist[symcnt]; sp++)
		if(sp->addr == addr ) 
			return 1;
	return 0;
}

static void
printname(syment_t *ent)
{
	printf("%-24.24s ", ent->name);
}
