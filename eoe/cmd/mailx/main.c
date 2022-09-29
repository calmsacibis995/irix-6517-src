/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */


#ifndef lint
static char *sccsid = "@(#)main.c	5.3 (Berkeley) 9/15/85";
#endif /* !lint */

#include "glob.h"
#include <sys/stat.h>

int	dasheflg = 0;
int	dashHflg = 0;
int	Fflag    = 0;

/*
 * Mail -- a mail program
 *
 * Startup -- interface with user.
 */

jmp_buf	hdrjmp;

/*
 * Find out who the user is, copy his mail file (if exists) into
 * temp file and set up the message pointers.  Then, print out the
 * message headers and read user commands.
 *
 * Command line syntax:
 *	Mail [ -i ] [ -r address ] [ -h number ] [ -f [ name ] ]
 * or:
 *	Mail [ -i ] [ -r address ] [ -h number ] people ...
 */

main(argc, argv)
	int  argc;
	char **argv;
{
	register char *ef;
	register int i, argp;
	char ef2[PATHSIZE], wsize[8];
	int fopt;
	char *invoked_name; /* this program called? 'Mail' or 'mailx' */
#if defined(SVR3) || defined(_SVR4_SOURCE)
	int mustsend, uflag, f;
	void (*prevint)(), hdrstop();
#else
	int mustsend, uflag, hdrstop(), (*prevint)(), f;
#endif
	extern struct cmd cmdtab[];
	extern struct cmd bsdcompat_entries[];
	struct cmd *cp1;
	struct cmd *cp2;
	FILE *ibuf, *ftat;
#ifndef	USG
	struct sgttyb tbuf;
#else
	struct termio tbuf;
#endif
	savedegid = getegid();
	setgid(getgid());

#ifdef signal
	Siginit();
#endif


	/*
	 * Set up a reasonable environment.  We clobber the last
	 * element of argument list for compatibility with version 6,
	 * figure out whether we are being run interactively, set up
	 * all the temporary files, buffer standard output, and so forth.
	 */

	uflag = 0;
	argv[argc] = (char *) -1;
	mypid = getpid();
	intty = isatty(0);
	outtty = isatty(1);
	if (outtty) {
#ifndef	USG
		gtty(1, &tbuf);
		baud = tbuf.sg_ospeed;
#else
		ioctl(1, TCGETA, &tbuf);
		baud = tbuf.c_cflag & CBAUD;
#endif
	}
	else
		baud = B9600;
	image = -1;

	invoked_name = argv[0];
	if ((invoked_name && invoked_name != (char *)-1) &&
	    (strlen(invoked_name) > 0) &&
	    (*((invoked_name + strlen(invoked_name)) - 1) == 'x'))
	{
		ismailx = 1;
	} else
		ismailx = 0;

	if (!ismailx) {
		/*
		 *  To maintain compatibility for BSD Mail, the below 
		 *  replaces 'cmdtab[]' entries which have conflicts with
		 *  the BSD Mail entries.  The default 'cmdtab[]' entries 
		 *  are for the 'mailx' version.
		 */
		for (cp2 = &bsdcompat_entries[0]; cp2->c_name != NOSTR; cp2++) {
			for (cp1 = &cmdtab[0]; cp1->c_name != NOSTR; cp1++) {
				if (!strcmp(cp2->c_name, cp1->c_name)) {
					cp1->c_func = cp2->c_func;
					cp1->c_argtype = cp2->c_argtype;
					cp1->c_msgflag = cp2->c_msgflag;
					cp1->c_msgmask = cp2->c_msgmask;
				}
			}
		}
	}
	/*
	 * Now, determine how we are being used.
	 * We successively pick off instances of -r, -h, -f, and -i.
	 * If called as "rmail" we note this fact for letter sending.
	 * If there is anything left, it is the base of the list
	 * of users to mail to.  Argp will be set to point to the
	 * first of these users.
	 */

	dasheflg = 0;
	ef = NOSTR;
	argp = -1;
	fopt = 0;
	mustsend = 0;
	readonly = NOT_READONLY;
	if (argc > 0 && **argv == 'r')
		rmail++;
	for (i = 1; i < argc; i++) {

		/*
		 * If current argument is not a flag, then the
		 * rest of the arguments must be recipients.
		 */
		if (*argv[i] != '-') {
			argp = i;
			break;
		}
		switch (argv[i][1]) {
		case 'r':
			/*
			 * Next argument is address to be sent along
			 * to the mailer.
			 */
			if (i >= argc - 1) {
				fprintf(stderr, "Address required after -r\n");
				exit(1);
			}
			mustsend++;
			rflag = argv[i+1];
			i++;
			break;

		case 'R':
			/*
			 * Force read-only mode.
			 */
			readonly = NO_ACCESS;
			break;

		case 'T':
			/*
			 * Next argument is temp file to write which
			 * articles have been read/deleted for netnews.
			 */
			if (i >= argc - 1) {
				fprintf(stderr, "Name required after -T\n");
				exit(1);
			}
			Tflag = argv[i+1];
			if ((f = creat(Tflag, 0600)) < 0) {
				perror(Tflag);
				exit(1);
			}
			close(f);
			i++;
			break;

		case 'u':
			/*
			 * Next argument is person to pretend to be.
			 */
			uflag++;
			if (i >= argc - 1) {
				fprintf(stderr, "Missing user name for -u\n");
				exit(1);
			}
			strncpy(myname, argv[i+1], sizeof(myname)-1);
			i++;
			if (value("MAIL") != NOSTR)
				assign("MAIL", "");
			break;

		case 'i':
			/*
			 * User wants to ignore interrupts.
			 * Set the variable "ignore"
			 */
			assign("ignore", "");
			break;

		case 'd':
			debug++;
			break;

		case 'H':
			dashHflg = 1;
			break;

		case 'h':
			/*
			 * Specified sequence number for network.
			 * This is the number of "hops" made so
			 * far (count of times message has been
			 * forwarded) to help avoid infinite mail loops.
			 */
			if (i >= argc - 1) {
				fprintf(stderr, "Number required for -h\n");
				exit(1);
			}
			mustsend++;
			hflag = atoi(argv[i+1]);
			if (hflag == 0) {
				fprintf(stderr, "-h needs non-zero number\n");
				exit(1);
			}
			i++;
			break;

		case 's':
			/*
			 * Give a subject field for sending from
			 * non terminal
			 */
			if (i >= argc - 1) {
				fprintf(stderr, "Subject req'd for -s\n");
				exit(1);
			}
			mustsend++;
			sflag = argv[i+1];
			i++;
			break;

		case 'l':
			/*
			 * Ignore locks.
			 */
			nolock++;

			/* Fall through... */

		case 'f':
			/*
			 * User is specifying file to "edit" with Mail,
			 * as opposed to reading system mailbox.
			 * If no argument is given after -f, we read his
			 * mbox file in his home directory.
			 */
			if (i >= argc - 1)
				ef = mbox;
			else {
				if (argv[i + 1][0] == '-')
					ef = mbox;
				else {
					fopt++;
					ef = argv[i + 1];
					i++;
				}
			}
			break;

		case 'n':
			/*
			 * User doesn't want to source /usr/lib/Mail.rc
			 */
			nosrc++;
			break;

		case 'N':
			/*
			 * Avoid initial header printing.
			 */
			noheader++;
			break;

		case 'v':
			/*
			 * Send mailer verbose flag
			 */
			assign("verbose", "");
			break;

		case 'I':
			/*
			 * We're interactive
			 */
			intty = 1;
			break;

		case 'e':
			dasheflg = 1;
			break;

		case '-':
			continue;

		case 'F':
			Fflag = 1;
			break;

		case '?':
			fprintf(stderr,"\
Usage: %s [-v] [-i] [-n] [-s subject] [user ...]\n\
       %s [-v] [-i] [-n]-f [name]\n\
       %s [-v] [-i] [-n]-u user\n\
       %s [-s subject] address ...\n\
       %s -e\n\
       %s [-HiNn] [-F] [-u user]\n\
       %s -f[-HiNn] [-F] [file]\n",
		argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
			exit(1);

		default:
			fprintf(stderr, "Unknown flag: %s\n", argv[i]);
			exit(1);
		}
	}

	/*
	 * Check for inconsistent arguments.
	 */

	if (ef != NOSTR && argp != -1) {
		fprintf(stderr, "Cannot give -f and people to send to.\n");
		exit(1);
	}
	if (mustsend && argp == -1) {
		fprintf(stderr, "The flags you gave make no sense since you're not sending mail.\n");
		exit(1);
	}
	input = stdin;
	rcvmode = argp == -1;
	tinit();
	if (argp != -1) {
		mail(&argv[argp]);

		/*
		 * why wait?
		 */

		exit(senderr);
	}

	/*
	 * Ok, we are reading mail.
	 * Decide whether we are editing a mailbox or reading
	 * the system mailbox, and open up the right stuff.
	 */

	if (ef != NOSTR) {
		char *ename;

		edit++;
		ef = expand(ef);
		if (ef == NOSTR)
			exit(1);
		if (fopt && ((*ef == '.') || (*ef != '/'))) {
			if (getwd(ef2) == 0) {
				perror(ef2);
				exit(1);
			}
			if(strlen(ef2) < (PATHSIZE - 1))
				snprintf(ef2 + strlen(ef2),
					PATHSIZE - strlen(ef2) - 1, "/%s", ef);
			ef = ef2;
		}
		editfile = ef;
		strncpy(mailname, ef, PATHSIZE);
		mailname[PATHSIZE - 1] = '\0';
	}
	if (setfile(mailname, edit, readonly) < 0) {
		if (edit)
			perror(mailname);
		else
			fprintf(stderr, "No mail for %s\n", myname);
		exit(1);
	}
	if (msgCount > 0 && !noheader && value("noheader") == NOSTR) {
		if (setjmp(hdrjmp) == 0) {
			if ((prevint = sigset(SIGINT, SIG_IGN)) != SIG_IGN)
				sigset(SIGINT, hdrstop);
			announce(!0);
			fflush(stdout);
			sigset(SIGINT, prevint);
		}
	}
	if (dashHflg || (!edit && msgCount == 0)) {
		if (!dashHflg)
			printf("No mail\n");
		fflush(stdout);
		rounlock();
		exit(0);
	}
	sigset(SIGQUIT, SIG_IGN);
	commands();
	if (!edit) {
		sigset(SIGHUP, SIG_IGN);
		sigset(SIGINT, SIG_IGN);
		quit();
	}
	rounlock();
	exit(0);
}

/*
 * Interrupt printing of the headers.
 */
void
hdrstop()
{

	fflush(stdout);
	fprintf(stderr, "\nInterrupt\n");
	longjmp(hdrjmp, 1);
}
