/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mkfifo:mkfifo.c	1.1"

#include <sys/types.h>
#include <sys/stat.h>

#include    <stdio.h>
#include    <errno.h>
#include    <string.h>

static mode_t newmode(), abs(), who();
static int what();

#ifndef sgi
extern int mkfifo();
#endif
extern int errno;
extern char	*sys_errlist[];

extern int opterr, optind;
extern char *optarg;

char *ms;
char *msp;
int mflag = 0;
int nowho;

mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
mode_t expmode;

#define	USER	05700	/* user's bits */
#define	GROUP	02070	/* group's bits */
#define	OTHER	00007	/* other's bits */
#define	ALL	07777	/* all */

#define	READ	00444	/* read permit */
#define	WRITE	00222	/* write permit */
#define	EXEC	00111	/* exec permit */
#define	SETID	06000	/* set[ug]id */

main( argc, argv )
int   argc;
char       *argv[];
{
	char *path;
	int exitval = 0;
	int retval, ch_retval=0;
	int i, c, mask;
	
    	if (argc < 2)
		usage();

	opterr = 0;
	while ((c=getopt(argc, argv, ":m:")) != EOF) {
                switch (c) {
                case 'm':
			mflag++;
                        ms = optarg;
			if(strcmp(ms,"--")==0) {
				fprintf(stderr,"mkfifo: Missing option argument\n");
				exit(1);
			}
			mode_check(ms);
                        break;
		case '?':
			fprintf(stderr,"mkfifo: Bad option (%c)\n",optopt);
			exit(1);
		case ':':
			fprintf(stderr,"mkfifo: Missing option argument\n");
			exit(1);
		}
	}
	if(mflag && optind >= argc) {
		fprintf(stderr,"mkfifo: Missing arguments\n");
		exit(1);
	}
    	for (;optind<argc;optind++) {

		path = argv[optind];
		if(mflag)
			mode = newmode(mode);
		if(mkfifo (path,mode)) {
			fprintf(stderr, "mkfifo: %s\n", sys_errlist[errno]);
			exitval = 1;
		}
		if(mflag && nowho == 0) {
			struct stat64 statbuf;
			if(stat64(path,&statbuf)) {
				fprintf(stderr,"mkfifo: Bad stat of %s\n",path);
				exitval = 1;
			} else if(chmod(path,newmode(statbuf.st_mode))) {
				fprintf(stderr,"mkfifo: Bad chmod of %s\n",path);
				exitval = 1;
			}
		}
    	}
    	exit(exitval);
}


usage()
{
	(void)fprintf(stderr, "usage: mkfifo [-m mode] path ...\n");
	exit(1);
}

static mode_t
newmode(nm)
mode_t  nm;
{
	/* m contains USER|GROUP|OTHER information
	   o contains +|-|= information
	   b contains rwx(slt) information  */
	mode_t m, b;
	register int o;
	register int lcheck, scheck, xcheck, goon;
	mode_t om = nm;	/* save mode for file mode incompatibility return */

	msp = ms;
	if (*msp >= '0' && *msp <= '9')
		return(abs(om));
	do {
		m = who();
		while (o = what()) {
/*
	this section processes permissions
*/
			b = 0;
			goon = 0;
			lcheck = scheck = xcheck = 0;
			switch (*msp) {
			case 'u':
				b = (nm & USER) >> 6;
				goto dup;
			case 'g':
				b = (nm & GROUP) >> 3;
				goto dup;
			case 'o':
				b = (nm & OTHER);
		    dup:
				b &= (READ|WRITE|EXEC);
				b |= (b << 3) | (b << 6);
				msp++;
				goon = 1;
			}
			while (goon == 0) switch (*msp++) {
			case 'r':
				b |= READ;
				continue;
			case 'w':
				b |= WRITE;
				continue;
			case 'x':
				b |= EXEC;
				xcheck = 1;
				continue;
			case 's':
				b |= SETID;
				scheck = 1;
				continue;
			default:
				msp--;
				goon = 1;
			}

			b &= m;

			switch (o) {
			case '+':

				/* if no "who" was specified, don't enable anything that's masked by the umask */
				if (nowho) {
				    int um = umask(0);
				    umask(um);
				    b &= ~um;
				}

				/* create new mode */
				nm |= b;
				break;
			case '-':
				if ( xcheck == 1 && scheck == 0
				     && ((m & GROUP) == GROUP || (m & USER) == USER)
				     && (nm & m & (SETID | EXEC)) == (m & (SETID | EXEC)) ) {
					if ( (b & USER & SETID) != (USER & SETID)
					     && (nm & USER & (SETID | EXEC)) == (m & USER & (SETID | EXEC)) )
						b |= USER & SETID;
					if ( (b & GROUP & SETID) != (GROUP & SETID)
					     && (nm & GROUP & (SETID | EXEC)) == (m & GROUP & (SETID | EXEC)) )
						b |= GROUP & SETID;
				}

				/* create new mode */
				nm &= ~b;
				break;
			case '=':

				/* create new mode */
				nm &= ~m;
				nm |= b;
				break;
			}
		}
	} while (*msp++ == ',');
	if (*--msp) {
		fprintf(stderr,"mkfifo: Bad mode\n");
		exit(1);
	}
	return(nm);
}

static mode_t
abs(mode)
	mode_t mode;
{
	register c;
	mode_t i;

	for ( i = 0; (c = *msp) >= '0' && c <= '7'; msp++)
		i = (mode_t)((i << 3) + (c - '0'));
	if (*msp) {
		fprintf(stderr,"mkfifo: Bad mode\n");
		exit(1);
	}

	return(i);
}

static mode_t
who()
{
	register mode_t m;

	m = 0;
	nowho = 0;
	for (;; msp++) switch (*msp) {
	case 'u':
		m |= USER;
		continue;
	case 'g':
		m |= GROUP;
		continue;
	case 'o':
		m |= OTHER;
		continue;
	case 'a':
		m |= ALL;
		continue;
	default:
		if (m == 0) {
			m = ALL;
			nowho = 1;
		}
		return m;
	}
}

static int
what()
{
	switch (*msp) {
	case '+':
	case '-':
	case '=':
		return *msp++;
	}
	return(0);
}
int mode_check(modestr)
char *modestr;
{
	char *allow = "01234567,ugoarwxs+-=";
	char *mstr = modestr;
	for(;*mstr;++mstr) {
		if(strchr(allow,*mstr) == NULL) {
			fprintf(stderr,"mkfifo: Bad mode char(%c)\n",*mstr);
			exit(1);
		}
	}
	return(0);
}
