/*
 *  RMAIL -- UUCP mail server.
 *
 *	This program reads the >From ... remote from ... lines that
 *	UUCP is so fond of and turns them into something reasonable.
 *	It calls sendmail giving it a -f option built from these
 *	lines.
 */

#include <stdio.h>
#include <sysexits.h>
#include <syslog.h>
#include <pwd.h>
#include <sys/types.h>
#include <utmp.h>
#include <string.h>

#ifdef sgi
#include <signal.h>

void (*saveint)();
#endif /* sgi */

extern char* getenv();
extern char* getlogin();


#ifdef DEBUG
char Debug = 1;
#else
char Debug = 0;
#endif

#define MAILER	"/usr/lib/sendmail"

#define BUFLEN 512
char lbuf[BUFLEN];			/* one line of the message */
char from[BUFLEN] = "";			/* accumulated path of sender */
char ufrom[BUFLEN] = "/dev/null";	/* user on remote system */
char sys_buf[BUFLEN];
char cmd[2000];

main(argc, argv)
	char **argv;
{
	FILE *out;			/* output to sendmail */
	register char *uf;		/* ptr into ufrom */
	register int i;
	register char *cp;
	register char *sys = 0;

	/* find out who we are in case there is no 'from' line
	 *	This is because some SGI versions of RCS (ab)use rmail
	 *	to send the lock-breaking notification.
	 */
	putenv("IFS= "); /* for security */
	uf = getenv("LOGNAME");
	if (!uf || !strlen(uf))
		uf = getlogin();
	if (!uf || !strlen(uf))
	    uf = getpwuid(geteuid())->pw_name;

	if (argc > 1 && strcmp(argv[1], "-T") == 0) {
		Debug = 1;
		argc--;
		argv++;
	}
	openlog("rmail", LOG_PID | LOG_NOWAIT, LOG_DAEMON);

	if (argc < 2) {
		syslog(LOG_ERR, "argc=%d",argc);
		exit(EX_USAGE);
	}

	for (;;) {
		if (!fgets(lbuf, sizeof(lbuf), stdin))
			exit(1);	/* quit if file is empty */

		if (strncmp(lbuf, "From ", 5) != 0
		    && strncmp(lbuf, ">From ", 6) != 0)
			break;
		(void)sscanf(lbuf, "%*s %s", ufrom);
		uf = ufrom;
		cp = lbuf;
		for (;;) {
			cp = strchr(cp+1, 'r');
			if (cp == NULL) {
				register char *p = strrchr(uf, '!');

				if (p != NULL) {
					*p = '\0';
					sys = uf;
					uf = p + 1;
				} else if (NULL != (p = strrchr(uf, '@'))) {
					*p = '\0';
					sys = p+1;
					/* take only the end of the route */
					p = strrchr(uf, ':');
					if (NULL != p)
						uf = p+1;
				} else {
					sys = "somewhere";
				}
				break;
			}

			if (1 == sscanf(cp, "remote from %s", sys=sys_buf))
				break;
		}

		if ((strlen(from) + strlen(sys)) <= sizeof(from)) {
			(void)strcat(from, sys);
			(void)strcat(from, "!");
		}
		if (Debug)
			syslog(LOG_DEBUG,"ufrom='%s' sys='%s' from='%s'",
			       uf, sys, from);
	}
	(void)strcat(from, uf);

	if (0 > setregid(getegid(), getegid())) {
		syslog(LOG_DEBUG, "setreguid(%d, %d): %m",
			      getegid(), getegid());
	}

	(void)sprintf(cmd, (sys == 0
			    ? "%s -ee -f%s -oMrUUCP -i"
			    : "%s -ee -f%s -oMs%s -oMrUUCP -i"),
		      MAILER, from, sys);
	while (*++argv != NULL) {
		(void)strcat(cmd, " '");
		if (**argv == '(')
			(void)strncat(cmd, *argv+1, strlen(*argv)-2);
		else
			(void)strcat(cmd, *argv);
		(void)strcat(cmd, "'");
	}
	if (Debug) {
		syslog(LOG_DEBUG, "cmd='%s'", cmd);
	}
#ifdef sgi
	/* ignore SIGINT */
	saveint = sigset(SIGINT, SIG_IGN);
#endif /* sgi */
	out = popen(cmd, "w");
	fputs(lbuf, out);
	while (fgets(lbuf, sizeof lbuf, stdin))
		fputs(lbuf, out);
	i = pclose(out);
#ifdef sgi
	/* restore SIGINT */
	sigset(SIGINT, saveint);
#endif /* sgi */
	sync();				/* make things good at this end */
	if ((i & 0377) != 0)
	{
		syslog(LOG_ERR, "pclose: status=0%o", i);
		exit(EX_OSERR);
	}

	exit(0);
}
