/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 * From: "script.c	5.4 (Berkeley) 11/13/85";
 */

#ident "script/script.c: $Revision: 1.11 $"

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */


/*
 * script
 */
#include <stdio.h>
#include <signal.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stropts.h>


char	*getenv();
char	*shell;
FILE	*fscript;
int	master;
int	slave;
int	child;
int	subchild;
char	*fname = "typescript";
void	finish();
extern char *_getpty(int *fildes,  int oflag, mode_t mode, int nofork);

struct termio term;
struct	winsize win;
char	line[32];
int	aflg;

main(argc, argv)
	int argc;
	char *argv[];
{
	shell = getenv("SHELL");
	if (shell == 0)
		shell = "/bin/sh";
	argc--, argv++;
	while (argc > 0 && argv[0][0] == '-') {
		switch (argv[0][1]) {

		case 'a':
			aflg++;
			break;

		default:
			fprintf(stderr,
			    "usage: script [ -a ] [ typescript ]\n");
			exit(1);
		}
		argc--, argv++;
	}
	if (argc > 0)
		fname = argv[0];
	if ((fscript = fopen(fname, aflg ? "a" : "w")) == NULL) {
		perror(fname);
		fail();
	}
	getmaster();
	printf("Script started, file is %s\n", fname);
	fixtty();

	(void) signal(SIGCHLD, finish);
	child = fork();
	if (child < 0) {
		perror("fork");
		fail();
	}
	if (child == 0) {
		subchild = child = fork();
		if (child < 0) {
			perror("fork");
			fail();
		}
		if (child) {
			(void) close(0);
			dooutput();
		}
		else {
			(void) close(master);
			(void) fclose(fscript);
			doshell();
		}
		/*NOTREACHED*/
	}
	(void) fclose(fscript);
	doinput();
	/*NOTREACHED*/
}

doinput()
{
	char ibuf[BUFSIZ];
	int cc;

	while ((cc = read(0, ibuf, BUFSIZ)) > 0)
		(void) write(master, ibuf, cc);
	done();
}

#include <sys/wait.h>

void
finish()
{
	union wait status;
	register int pid;
	register int die = 0;

	while ((pid = wait3((int *)&status, WNOHANG, (struct rusage *)0)) > 0)
		if (pid == child)
			die = 1;

	if (die)
		done();
}

dooutput()
{
	time_t tvec;
	char obuf[BUFSIZ];
	int cc;

	tvec = time((time_t *)0);
	fprintf(fscript, "Script started on %s", ctime(&tvec));
	for (;;) {
		cc = read(master, obuf, sizeof (obuf));
		if (cc <= 0)
			break;
		(void) write(1, obuf, cc);
		(void) fwrite(obuf, 1, cc, fscript);
	}
	done();
}

doshell()
{
	int t;
	setsid();
	getslave();
	(void) dup2(slave, 0);
	(void) dup2(slave, 1);
	(void) dup2(slave, 2);
	(void) close(slave);
	execl(shell, "sh", "-i", 0);
	perror(shell);
	fail();
}

fixtty()
{
	struct termio nterm;

	nterm = term;
	nterm.c_lflag &= ~(ECHO | ISIG | ICANON);
	nterm.c_iflag &= ~(ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF|BRKINT);
	nterm.c_oflag &= ~OPOST;
	nterm.c_cc[VMIN] = nterm.c_cc[VTIME] = 1;
	(void) ioctl(0, TCSETAW, (char *)&nterm);
}

fail()
{

	(void) kill(0, SIGTERM);
	done();
}

done()
{
	if (subchild) {
	    time_t tvec = time((time_t *)0);
	    if ( ioctl(1, TCSETAF, (char *)&term)<0) 
		perror("ioctl");
	    fprintf(fscript,"\nscript done on %s", ctime(&tvec));
	    fclose(fscript);
	    printf("Script done, file is %s\n", fname);
	}
	exit(0);
}

getmaster()
{
	char *tmp;
	
	if ((tmp = _getpty(&master, O_RDWR, 0666, 0)) == (char*) 0) {
		fprintf(stderr, "Could not open pty\n");
		fail();
	}
	strcpy(line, tmp);

	(void) ioctl(0, TCGETA, (char *)&term);
	(void) ioctl(0, TIOCGWINSZ, (char *)&win);
}

getslave()
{
	slave = open(line, O_RDWR);
	if (slave < 0) {
		perror(line);
		fail();
	}
	(void) ioctl(slave, TCSETAW, (char *)&term);
	(void) ioctl(slave, TIOCSWINSZ, (char *)&win);
}
