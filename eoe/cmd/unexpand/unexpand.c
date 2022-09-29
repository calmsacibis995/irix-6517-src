/*-
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)unexpand.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * unexpand - put tabs into a file replacing blanks
 */
#include <stdio.h>

#include <locale.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

char	genbuf[BUFSIZ];
char	linebuf[BUFSIZ];
int	all;
int	nstops;
int	tabstops[100];
int	deftab;

main(argc, argv)
	int argc;
	char *argv[];
{
	register char *cp;
        int ret = 0;

	argc--, argv++;
	if (argc > 0 && argv[0][0] == '-') {
		if (strcmp(argv[0], "-a") == 0) {
			all++;
			argc--, argv++;
		}
	}
        if (argc > 0 && argv[0][0] == '-') {
          if (argv[0][1] == 't' && argv[0][2] == '\0') {
            argc--, argv++;
	    all++;
	  }
          else if (argv[0][1] == 't' && argv[0][2] != '\0') {
		 all++;
                 if (!getstops(&argv[0][2]))
                        argc--, argv++;
               }
          while (argc > 0 && (argv[0][0] >= '0' && argv[0][0] <= '9')) {
             if (!getstops(argv[0]))
                argc--, argv++;
             else /* process file arguments */
                break;
          }
	}
        if (argc > 0 && argv[0][0] == '-') {  /* Check for "-a" again */
                if (strcmp(argv[0], "-a") == 0) {
                        all++;
                        argc--, argv++;
                }
        }
        if (argc > 0 && (argv[0][0] == '-' && argv[0][1] =='-')) { 
               /* End of options. Check for "--" */
               argc--, argv++;
        }
	do {
		if (argc > 0) {
			if (freopen(argv[0], "r", stdin) == NULL) {
				perror(argv[0]);
                                ret = 1;
			}
			argc--, argv++;
		}
		while (fgets(genbuf, BUFSIZ, stdin) != NULL) {
			for (cp = linebuf; *cp; cp++)
				continue;
			if (cp > linebuf)
				cp[-1] = 0;
			tabify(all);
			printf("%s", linebuf);
		}
	} while (argc > 0);
	exit(ret);
}


tabify(c)
	char c;
{
	register char *cp, *dp;
	register int dcol;
	register int n;
	int ocol;

	ocol = 0;
	dcol = 0;

	if (nstops == 0 ) deftab = 8;
	else if (nstops == 1) deftab = tabstops[0];

	cp = genbuf, dp = linebuf;
	for (;;) {
		switch (*cp) {

		case ' ':
			dcol++;
			break;

		case '\t':
			dcol += 8;
			dcol &= ~07;
			break;
		/*case '\b':
			dcol--;
			break;*/
		default:
			if (nstops <=1)
				while (((ocol + deftab) - ((ocol + deftab)%deftab)) <= dcol) {
					if (ocol + 1 == dcol)
						break;
					*dp++ = '\t';
					ocol += deftab;
					ocol = ocol - (ocol%deftab);
				}
			else {
				for (n = 0; n < nstops; n++) 
					if (tabstops[n] > ocol)
						break;
				if (n == nstops) {
					c = 0;
				}
                                else
					while ((n < nstops) && (tabstops[n] <= dcol)) {
						*dp++ = '\t';
						ocol = tabstops[n];
						n++;
					}
			}
			while (ocol < dcol) {
				*dp++ = ' ';
				ocol++;
			}
			if (*cp == 0 || c == 0) {
				strcpy(dp, cp);
				return;
			}
			*dp++ = *cp;
			if (*cp == '\b')
				ocol--,dcol--;
			else
				ocol++,dcol++;
		}
		cp++;
	}
}


int getstops(cp)
        register char *cp;
{
        register int i;

        for (;;) {
                i = 0;
                while (*cp >= '0' && *cp <= '9')
                        i = i * 10 + *cp++ - '0';
                if (i <= 0 || i > 2147483647) {
bad:
                        fprintf(stderr, gettxt(_SGI_MMX_expand_bad, "Bad tab stop spec\n"));
                        exit(1);
                }
                if (nstops > 0 && i <= tabstops[nstops-1])
                        goto bad;
                tabstops[nstops++] = i;
                if (*cp == 0)
                        break;
                if ((*cp != ',')&&(*cp != ' ')) {/* 2 possibilities: a) Error or
                                   * (b) Filename starting with a digit. In
                                   * either case, return and let the error
                                   * processing be done later.
                                   */
                        *cp++;
                        return(1);
                } else *cp++;
        }
return(0);
}
