/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)profil-3b5:prfld.c	1.4" */
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/profiler/RCS/prfld.c,v 1.11 1997/08/21 02:52:44 leedom Exp $"
/*
 *	prfld - load profiler with sorted kernel text addresses
 */

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "prf.h"

static int Dump = 0;			/* if set then dump symbols used */
static char *namelist = "/unix";	/* namelist file */
static int symcnt;			/* number of symbols */

static int symlistsize;			/* sized of malloc'ed memory for list */
static symaddr_t *symlist;		/* list of symbol address */
static symaddr_t *symnext;		/* next free slot */

extern int  rdsymtab(int, void (*)(char *, symaddr_t));
extern int  rdelfsymtab(int, void (*)(char *, symaddr_t));

static int  compar(const void *, const void *);
static void listinit(void);
static void listadd(char *, symaddr_t);

static void fatal(const char *s, ...);

int
main(int argc, char **argv)
{
	int fd, prf;
	int dwarfsyms;

	setlinebuf(stdout);
	setlinebuf(stderr);
	if (argc > 1) {
		if (argv[1][0] == '-') {
			if (argv[1][1] == 'd')
				Dump++;
			else
				fatal("usage: prfld [-d] [/unix]");
			if (argc > 3)
				fatal("usage: prfld [-d] [/unix]");
			namelist = argv[2];
		} else
			namelist = argv[1];
	}
	if ((prf = open("/dev/prf", O_WRONLY)) < 0)
		fatal("cannot open /dev/prf: %s", strerror(errno));
	if ((fd = open(namelist, O_RDONLY)) < 0)
		fatal("cannot open namelist file: %s", strerror(errno));

	listinit();
	dwarfsyms = rdsymtab(fd, listadd);
	if (rdelfsymtab(fd, listadd) && dwarfsyms)
		fatal("unable to load dwarf/mdebug or elf symbol table information");
	qsort(symlist, symcnt, sizeof (symaddr_t), compar);
	if (write(prf, symlist, symcnt*sizeof(symaddr_t)) !=
			        symcnt*sizeof(symaddr_t))
		switch(errno) {
		case ENOSPC:
			fatal("insufficient space in system for addresses");
		case E2BIG:
			fatal("unaligned data or insufficient addresses");
		case EBUSY:
			fatal("profiler is enabled");
		case EINVAL:
			fatal("text addresses not sorted properly");
		default:
			fatal("cannot load profiler addresses");
		}
	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

static void
fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("prfld: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
    /*NOTREACHED*/
}

#define	LISTINCR	6000

static void
listinit(void)
{
	symlist = (symaddr_t *)malloc(LISTINCR * sizeof (symaddr_t));
	if (symlist == NULL)
		fatal("cannot allocate memory");
	symnext = symlist;
	symlistsize = LISTINCR;

	/*
	 * Provide addr=0 bucket for PC addresses less than first legal
	 * text address.  rdelfsymtab() will provide a similar bucket
	 * initialized to the value of "_etext".
	 */
	*symnext++ = 0;
	symcnt = 1;
}

/* ARGSUSED */
static void
listadd(char *name, symaddr_t addr)
{
	if (Dump)
		printf("%s 0x%lx\n", name, addr);

	if (strcmp("tcp_saveti", name) == 0)
		puts("Function symbol filter let data address through!");
	if (symcnt >= symlistsize) {
		symlistsize += LISTINCR;
		symlist = (symaddr_t *)
			realloc(symlist, symlistsize * sizeof(symaddr_t));
		if (symlist == NULL)
			fatal("cannot allocate memory");
		symnext = &symlist[symcnt];
	}
	*symnext++ = addr;
	symcnt++;
}

int
list_repeated(symaddr_t addr)
{
	register symaddr_t *sp;
	int i;

	for (sp = symlist, i = 0 ; i < symcnt ; i++, sp++)
		if( *sp == addr ) 
			return 1;
	return 0;
}

static int
compar(const void *x, const void *y)
{
	if (*(symaddr_t *)x > *(symaddr_t *)y)
		return(1);
	else if (*(symaddr_t *)x == *(symaddr_t *)y)
		return(0);
	return(-1);
}
