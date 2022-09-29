/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sum:sum.c	1.6"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sum/RCS/sum.c,v 1.8 1997/07/19 05:27:02 csm Exp $"
/*
 * Sum bytes in file mod 2^16
 */


#define WDSHFT 16
#define WDMSK 0xffffUL

#define BLKSIZE 512
#define BUFSIZE 16384

#include <stdio.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

main(argc,argv)
char **argv;
{
	char buf[BUFSIZE];
	unsigned long sum;
	int fd, i, alg, errflg = 0;
	register int n, j;
	long long nbytes;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel("UX:sum");

	alg = 0;
	i = 1;
	if((argc > 1) && (argv[1][0]=='-' && argv[1][1]=='r')) {
		alg = 1;
		i = 2;
	}

	do {
		if (i < argc) {
			if((fd = open(argv[i], O_RDONLY)) < 0) {
				(void) pfmt(stderr, MM_ERROR, 
					":3:Cannot open %s: %s\n",
					argv[i], strerror(errno));
				errflg += 10;
				continue;
			}
		} else
			fd = 0;
		sum = 0;
		nbytes = 0;
		while ((n = read(fd, buf, sizeof (buf))) > 0) {
			if(alg == 1)
				for (j = 0; j < n; j++)
					sum = ((sum >> 1) +
					       ((sum & 1) << 15) +
					       buf[j]) & WDMSK;
			else
				for (j = 0; j < n; j++)
					sum += buf[j];
			nbytes += n;
		}
		if (n < 0) {
			errflg++;
			if (argc > 1)
				(void) pfmt(stderr, MM_ERROR, 
					":59:Read error on %s: %s\n",
					argv[i], strerror(errno));
			else
				(void) pfmt(stderr, MM_ERROR,
					":60:Read error on stdin: %s\n",
					strerror(errno));
		}
		if (alg == 1)
			(void) printf("%.5u %6lld",
				      sum, (nbytes+BLKSIZE-1)/BLKSIZE);
		else {
			sum = (sum & WDMSK) + (sum >> WDSHFT & WDMSK);
			sum = (sum & WDMSK) + (sum >> WDSHFT & WDMSK);

			(void) printf("%u %lld",
				      sum, (nbytes+BLKSIZE-1)/BLKSIZE);
		}
		if(argc > 1)
			(void) printf(" %s", argv[i]==(char *)0?"":argv[i]);
		(void) printf("\n");
		(void) close(fd);
	} while(++i < argc);
	exit(errflg);
}
