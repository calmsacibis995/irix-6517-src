/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)crypt:crypt.c	1.8"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/crypt/RCS/crypt.c,v 1.6 1992/12/15 17:36:52 frank Exp $"
/*
 *	A one-rotor machine designed along the lines of Enigma
 *	but considerably trivialized.
 */

#define ECHO 010
#include <stdio.h>
#include <stdlib.h>
#define ROTORSZ 256
#define MASK 0377

#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

char	t1[ROTORSZ];
char	t2[ROTORSZ];
char	t3[ROTORSZ];

char	cmd_label[] = "UX:crypt";

/*
 * some msg prints
 */
static void
usage()
{
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_crypt_usage, "crypt [ -k ] [ key]"));
}

static void
err_envvar(s)
char *s;
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_envvar_err,
		"The '%s' environment variable is not defined"), s);
}

setup(pw)
char *pw;
{
	int ic, i, k, temp;
	unsigned random;
	char buf[13];
	long seed;

	strncpy(buf, pw, 8);
	buf[8] = buf[0];
	buf[9] = buf[1];
	strncpy(buf, crypt(buf, &buf[8]), 13);
	seed = 123;
	for (i=0; i<13; i++)
		seed = seed*buf[i] + i;
	for(i=0;i<ROTORSZ;i++) {
		t1[i] = i;
		t3[i] = 0;
	}
	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		random = seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (random&MASK)%(k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0) continue;
		ic = (random&MASK) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0;i<ROTORSZ;i++)
		t2[t1[i]&MASK] = i;
}

/*
 * main entry
 */
int
main(argc, argv)
char *argv[];
{
	register char *keyfmt;
	extern int optind;
	register char *p1;
	register i, n1, n2, nchar;
	int c;
	struct { 
		long offset;
		unsigned int count;
	} header;
	int pflag = 0;
	int kflag = 0;
	char *buf;
	char *keyp;
	char key[8];
	char *keyvar = "CrYpTkEy=XXXXXXXX";

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	keyfmt = gettxt(_SGI_DMMX_crypt_keyask, "Enter key:");
	if (argc < 2){
		if((buf = (char *)getpass(keyfmt)) == NULL) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"),
			    "/dev/tty");
			exit(1);
		}
		setup(buf);
	}
	else {
		while ((c = getopt(argc, argv, "pk")) != EOF)
			switch (c) {
			case 'p':
			/* notify editor that exec has succeeded */
				if(write(1, "y", 1) != 1)
					exit(1);
				if(read(0, key, 8) != 8)
					exit(1);
				setup(key);
				pflag = 1;
				break;
			case 'k':
				if ( !(keyp = getenv("CrYpTkEy")) ) {
					err_envvar("CrYpTkEy");
					(void)exit(1);
				}
				strncpy(key, keyp, 8);
				setup(key);
				kflag = 1;
				break;
			case '?':
				usage();
				exit(2);
			}
		if(pflag == 0 && kflag == 0) {
			strncpy(keyvar+9,argv[optind],8);
			putenv(keyvar);
			execlp("crypt","crypt","-k",(char *)NULL);
		}
	}	
	if(pflag)
		while(1) {
			if((nchar = read(0, (char *)&header, sizeof(header))) != sizeof(header))
				exit(nchar);
			n1 = (int) (header.offset&MASK);
			n2 = (int) ((header.offset >> 8) &MASK);
			nchar = header.count;
			buf = (char *)malloc(nchar);
			p1 = buf;
			if(read(0, buf, nchar) != nchar)
				exit(1);
			while(nchar--) {
				*p1 = t2[(t3[(t1[(*p1+n1)&MASK]+n2)&MASK]-n2)&MASK]-n1;
				n1++;
				if(n1 == ROTORSZ) {
					n1 = 0;
					n2++;
					if(n2 == ROTORSZ) n2 = 0;
				}
				p1++;
			}
			nchar = header.count;
			if(write(1, buf, nchar) != nchar)
				exit(1);
			free(buf);
		}
		
	n1 = 0;
	n2 = 0;

	while((i=getchar()) >=0) {
		i = t2[(t3[(t1[(i+n1)&MASK]+n2)&MASK]-n2)&MASK]-n1;
		putchar(i);
		n1++;
		if(n1==ROTORSZ) {
			n1 = 0;
			n2++;
			if(n2==ROTORSZ) n2 = 0;
		}
	}
	exit(0);
}
