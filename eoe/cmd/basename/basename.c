/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)basename.c	5.1 (Berkeley) 3/9/91";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <regexpr.h>
#include <pfmt.h>


void	usage();


int
main(int argc, char **argv)
{
	extern int optind;
	register char *p;
	int ch;

	(void) setlocale(LC_ALL, "");
	(void) setcat("uxcore");
	(void) setlabel("UX:basename");

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 2)
		usage();

	if (argc == 0) {
		(void) printf(".\n");
		exit (0);
	}

	/*
	 * (1) If string is // it is implementation defined whether steps (2)
	 *     through (5) are skipped or processed.
	 *
	 * (2) If string consists entirely of slash characters, string shall
	 *     be set to a single slash character.  In this case, skip steps
	 *     (3) through (5).
	 */
	for (p = *argv;; ++p) {
		if (!*p) {
			if (p > *argv)
				(void)printf("/\n");
			else
				(void)printf(".\n"); /* Null String */
			exit(0);
		}
		if (*p != '/')
			break;
	}

	/*
	 * (3) If there are any trailing slash characters in string, they
	 *     shall be removed.
	 */
	for (; *p; ++p);
	while (*--p == '/');
	*++p = '\0';

	/*
	 * (4) If there are any slash characters remaining in string, the
	 *     prefix of string up to an including the last slash character
	 *     in string shall be removed.
	 */
	while (--p >= *argv)
		if (*p == '/')
			break;
	++p;

	/*
	 * (5) If the suffix operand is present, is not identical to the
	 *     characters remaining in string, and is identical to a suffix
	 *     of the characters remaining in string, the suffix suffix
	 *     shall be removed from string.
	 */
	if (*++argv && (strcmp(p,*argv) != 0)) {
		char *expr;
		char *suffix;

		if((suffix = malloc(strlen(*argv) + 3)) == NULL) {
			pfmt(stderr, MM_ERROR|MM_NOGET, 
				"Cannot malloc new string\n");
			exit(2);
		}

		*suffix = '\0';
		if (*argv[0] == '.')
			strcpy(suffix,"\\");

		strcat(suffix,*argv);
		strcat(suffix,"$");

		/* Compile the regular expression */
		if ((expr = compile(suffix, NULL, NULL)) == NULL) {
			pfmt(stderr, MM_ERROR|MM_NOGET, 
		     		"Invalid regular expression: '%s'\n",
		     		*argv);
			exit(2);
		}

		if (step(p, expr) && (*loc2 == '\0'))
			*loc1 = '\0';

		free(suffix);
	}
	(void)printf("%s\n", p);
	exit(0);
}

void
usage()
{
	pfmt(stderr, MM_ERROR, "uxcore:1:Incorrect usage\n");
	pfmt(stderr, MM_ACTION, "uxcore:2:Usage: basename [ path [ suffix-pattern ] ]\n");
	exit(1);
}
