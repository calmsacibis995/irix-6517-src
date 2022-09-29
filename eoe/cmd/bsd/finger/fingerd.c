/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)fingerd.c	5.3 (Berkeley) 11/3/88";
#endif /* not lint */

/*
 * Finger server.
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <netdb.h>

#define LINELEN	512

extern struct hostent * __getverfhostent(struct in_addr, int);

static void fatal(char *);
static void output(FILE *);

static char *
printable(register const char *cp)
{
	static char line[LINELEN];
	register char *l = line;
	register char c;

	while (c = *cp++) {
		if (isprint(c))
			*l++ = c;
	}
	*l = '\0';
	return line;
}

main(argc, argv)
	int argc;
	char *argv[];
{
	register char *sp;
	char line[LINELEN];
	struct sockaddr_in sin;
	int i, p[2], pid, status;
	FILE *fp;
#define	AVLEN	4
	char *av[AVLEN+1];
	int c;
	char *file = NULL;
	extern int optind;
	extern char *optarg;

	openlog(argv[0], LOG_PID, LOG_DAEMON);
	i = sizeof (sin);
	if (getpeername(0, &sin, &i) < 0)
		fatal("getpeername");
	if (fgets(line, sizeof(line), stdin) == NULL)
		exit(1);
	sp = line;
	av[0] = "finger";
	i = 1;
	while ((c = getopt(argc, argv, "f:lLS")) != EOF) {
		switch(c) {
		case 'l':
			syslog(LOG_INFO, "%s: '%.80s'",
				__getverfhostent(sin.sin_addr, 1)->h_name,
				printable(line));
			break;
		case 'f':
			file = optarg;
			break;
		case 'L':
			av[i++] = "-L";
			break;
		case 'S':
			av[i++] = "-S";
			break;
		default:
			syslog(LOG_ERR, "usage: %s [-l] [-L] [-S | -f file]",
				argv[0]);
			exit(2);
		}
	}
	if (file) {
		if ((fp = fopen(file, "r")) == NULL)
			fatal(file);
		output(fp);
		fclose(fp);
		return(0);
	}

	while (i < AVLEN) {
		while (isspace(*sp))
			sp++;
		if (!*sp)
			break;
		if (*sp == '/' && (sp[1] == 'W' || sp[1] == 'w')) {
			sp += 2;
			av[i++] = "-l";
		}
		if (*sp && !isspace(*sp)) {
			av[i++] = sp;
			while (*sp && !isspace(*sp))
				sp++;
			*sp = '\0';
		}
	}
	av[i] = 0;
	if (pipe(p) < 0)
		fatal("pipe");
	if ((pid = fork()) == 0) {
		close(p[0]);
		if (p[1] != 1) {
			dup2(p[1], 1);
			close(p[1]);
		}
		execv("/usr/bsd/finger", av);
		_exit(1);
	}
	if (pid == -1)
		fatal("fork");
	close(p[1]);
	if ((fp = fdopen(p[0], "r")) == NULL)
		fatal("fdopen");
	output(fp);
	fclose(fp);
	while ((i = wait(&status)) != pid && i != -1)
		;
	return(0);
}

static void
fatal(char *s)
{
	syslog(LOG_ERR, "%s: %m", s);
	exit(1);
}

static void
output(FILE *fp)
{
	int i;
	while ((i = getc(fp)) != EOF) {
		if (i == '\n')
			putchar('\r');
		putchar(i);
	}
}
