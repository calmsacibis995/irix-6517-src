/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)lex.c	5.3 (Berkeley) 9/15/85";
#endif /* not lint */

#include "glob.h"
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

/*
 * Mail -- a mail program
 *
 * Lexical processing of commands.
 */

static char *prompt = "& ";
static char *mailx_prompt = "? ";
static char efile[PATHSIZE];
static char lfile[PATHSIZE];

/*
 * Set up editing on the given file name.
 * If isedit is true, we are considered to be editing the file,
 * otherwise we are reading our mail which has signficance for
 * mbox and so forth.
 */

setfile(name, isedit, ronly)
	char *name;
	int isedit, ronly;
{
	FILE *ibuf;
	int fd;
	struct stat stb;
	static int shudclob;
	extern char tempMesg[];
	extern int errno;

	rounlock(); /* unlock any previous file */
	if ((ibuf = fopen(name, "r")) == NULL) {
		if (dasheflg)
			exit (1);
		return(-1);
	}

	if (fstat(fileno(ibuf), &stb) < 0) {
		if (dasheflg)
			exit (1);
		fclose(ibuf);
		return (-1);
	}

	switch (stb.st_mode & S_IFMT) {
	case S_IFDIR:
		fclose(ibuf);
		errno = EISDIR;
		if (dasheflg)
			exit (1);
		return (-1);

	case S_IFREG:
		break;

	default:
		fclose(ibuf);
		errno = EINVAL;
		if (dasheflg)
			exit (1);
		return (-1);
	}

	if (!edit && stb.st_size == 0) {
		fclose(ibuf);
		if (dasheflg)
			exit (1);
		return(-1);
	}
	if (dasheflg)
		exit (0);

	/*
	 * Looks like all will be well.  We must now relinquish our
	 * hold on the current set of stuff.  Must hold signals
	 * while we are reading the new file, else we will ruin
	 * the message[] data structure.
	 */

	holdsigs();
	if (shudclob) {
		if (edit)
			edstop();
		else
			quit();
	}

	/*
	 * Copy the messages into temp file 
	 * and set pointers.
	 */

	readonly = NOT_READONLY;
/* REMIND
	if ((fd = open(name, 1)) < 0)
		readonly = NO_ACCESS;
	else
		close(fd);
*/
	if (shudclob) {
		fclose(itf);
		fclose(otf);
	}
	shudclob = 1;
	edit = isedit;
	strncpy(efile, name, PATHSIZE);
	efile[PATHSIZE - 1]='\0';
	editfile = efile;
	if (name != mailname) {
		strncpy(mailname, name, PATHSIZE);
		mailname[PATHSIZE - 1]='\0';
	}
	if (ronly != NOT_READONLY) {
		readonly = ronly;
	}
	else {
		rolockit(name, &readonly);
	}
	mailsize = fsize(ibuf);
	if ((otf = fopen(tempMesg, "w")) == NULL) {
		perror(tempMesg);
		safe_exit(1);
	}
	if ((itf = fopen(tempMesg, "r")) == NULL) {
		perror(tempMesg);
		safe_exit(1);
	}
	m_remove(tempMesg);
	lockit(name);
	setptr(ibuf);
	setmsize(msgCount);
	fclose(ibuf);
	unlock();
	relsesigs();
	sawcom = 0;
	return(0);
}

/*
 * Interpret user commands one by one.  If standard input is not a tty,
 * print no prompt.
 */

int	*msgvec;

commands()
{
	extern int dashHflg;
	int eofloop, shudprompt;
	register int n;
	char linebuf[LINESIZE];
	void hangup(), contin(), stop();
	char *cp;

	if (dashHflg)
		return;
# ifdef VMUNIX
	sigset(SIGCONT, SIG_DFL);
# endif /* VMUNIX */
	if (rcvmode && !sourcing) {
		if (sigset(SIGINT, SIG_IGN) != SIG_IGN)
			sigset(SIGINT, stop);
		if (sigset(SIGHUP, SIG_IGN) != SIG_IGN)
			sigset(SIGHUP, hangup);
	}
	shudprompt = intty && !sourcing;
	for (;;) {
		setexit();

		/*
		 * Print the prompt, if needed.  Clear out
		 * string space, and flush the output.
		 */

		if (!rcvmode && !sourcing)
			return;
		eofloop = 0;
top:
		if (shudprompt) {
			if ((cp = value("prompt")) != NOSTR)
				printf(cp);
			else {
				if (ismailx) {
					if (value("noprompt") != NOSTR) 
						goto noprompt;
					else
						printf(mailx_prompt);
				} else
					printf(prompt);
			}
	noprompt:
			fflush(stdout);
# ifdef VMUNIX
			sigset(SIGCONT, contin);
# endif /* VMUNIX */
		} else
			fflush(stdout);
		sreset();

		/*
		 * Read a line of commands from the current input
		 * and handle end of file specially.
		 */

		n = 0;
		for (;;) {
			if (readline(input, &linebuf[n]) <= 0) {
				if (n != 0)
					break;
				if (loading)
					return;
				if (sourcing) {
					unstack();
					goto more;
				}
				if (value("ignoreeof") != NOSTR && shudprompt) {
					if (++eofloop < 25) {
						printf("Use \"quit\" to quit.\n");
						goto top;
					}
				}
				if (edit)
					edstop();
				return;
			}
			if (sourcing && linebuf[0] == '#')
				continue;
			if ((n = strlen(linebuf)) == 0)
				break;
			n--;
			if (linebuf[n] != '\\')
				break;
			linebuf[n++] = ' ';
		}
# ifdef VMUNIX
		sigset(SIGCONT, SIG_DFL);
# endif /* VMUNIX */
		if (execute(linebuf, 0))
			return;
		if (value("debug") != NOSTR)
			debug++;
more:		;
	}
}

/*
 * Execute a single command.  If the command executed
 * is "quit," then return non-zero so that the caller
 * will know to return back to main, if he cares.
 * Contxt is non-zero if called while composing mail.
 */

#define CMD_RENAME_PREFIX "command:"

execute(linebuf, contxt)
	char linebuf[];
{
	char word[LINESIZE];
	char word1[LINESIZE];
	char origword[LINESIZE];
	char *arglist[MAXARGC];
	struct cmd *com;
	register char *cp, *cp2, *cp3;
	register int c;
	int muvec[2];
	int edstop(), e;

	/*
	 * Strip the white space away from the beginning
	 * of the command, then scan out a word, which
	 * consists of anything except digits and white space.
	 *
	 * Handle ! escapes differently to get the correct
	 * lexical conventions.
	 */

	cp = linebuf;
	while (isspace(*cp))
		cp++;
	if (*cp == '!') {
		if (sourcing) {
			printf("Can't \"!\" while sourcing\n");
			if (loading)
				return(1);
			unstack();
			return(0);
		}
		shell(cp+1);
		return(0);
	}
	cp2 = word;
	if (value("qmarkishelp") == NOSTR)
		strcpy(word1, "?$^.:/-+*'\"");
	else
		strcpy(word1, "$^.:/-+*'\"");
	while (*cp && !isspace(*cp) && !isdigit(*cp) &&
		      !any(*cp, word1))
		*cp2++ = *cp++;
	*cp2 = '\0';

	/*
	 * See if the typed command maps to some other command.
	 */
	if (!strncmp(word, "next", 4))
		sawnextcom = 1;
	else
		sawnextcom = 0;

	strncpy(origword, word, LINESIZE);
	origword[LINESIZE - 1] = '\0';
	for(;;) {
		strcpy(word1, CMD_RENAME_PREFIX);
		strncat(word1, word, LINESIZE - strlen(word1));
		word1[LINESIZE - 1] = '\0';
		if ((cp2 = value(word1)) == NOSTR ||
		    strcmp(origword, cp2) == 0) 
			break;
		strncpy(word, cp2, LINESIZE);
		word[LINESIZE - 1] = '\0';
	}
	if (cp2 != NOSTR) {
		printf("\nCommand rename loop...\n");
		strncpy(word, origword, LINESIZE);
		word[LINESIZE - 1] = '\0';
		printf("%s", origword);
		for(;;) {
			strcpy(word1, CMD_RENAME_PREFIX);
			strncat(word1, word, LINESIZE - strlen(word1));
			word1[LINESIZE - 1] = '\0';
			cp2 = value(word1);
			if (strcmp(origword, cp2) == 0)
				break;
			strncpy(word, cp2, LINESIZE);
			word[LINESIZE - 1] = '\0';
			printf("=>%s", word);
		}
		printf("=>%s\n", origword);
		printf("Using \"%s\"\n", origword);
		strncpy(word, origword, LINESIZE);
		word[LINESIZE - 1] = '\0';
	}

	/*
	 * Look up the command; if not found, bitch.
	 * Normally, a blank command would map to the
	 * first command in the table; while sourcing,
	 * however, we ignore blank lines to eliminate
	 * confusion.
	 */

	if (sourcing && equal(word, ""))
		return(0);
	com = lex(word);
	if (com == NONE) {
		printf("Unknown command: \"%s\"\n", word);
		if (loading) {
			printf("... while reading %s\n", lfile);
			return(1);
		}
		if (sourcing)
			unstack();
		return(0);
	}

	/*
	 * See if we should execute the command -- if a conditional
	 * we always execute it, otherwise, check the state of cond.
	 */

	if ((com->c_argtype & F) == 0)
		if (cond == CRCV && !rcvmode || cond == CSEND && rcvmode)
			return(0);

	/*
	 * Special case so that quit causes a return to
	 * main, who will call the quit code directly.
	 * If we are in a source file, just unstack.
	 */

	if (com->c_func == edstop && sourcing) {
		if (loading)
			return(1);
		unstack();
		return(0);
	}
	if (!edit && com->c_func == edstop) {
		sigset(SIGINT, SIG_IGN);
		return(1);
	}

	/*
	 * Process the arguments to the command, depending
	 * on the type he expects.  Default to an error.
	 * If we are sourcing an interactive command, it's
	 * an error.
	 */

	if (!rcvmode && (com->c_argtype & M) == 0) {
		printf("May not execute \"%s\" while sending\n",
		    com->c_name);
		if (loading)
			return(1);
		if (sourcing)
			unstack();
		return(0);
	}
	if (sourcing && com->c_argtype & I) {
		printf("May not execute \"%s\" while sourcing\n",
		    com->c_name);
		if (loading)
			return(1);
		unstack();
		return(0);
	}
	if (readonly && com->c_argtype & W) {
		if (readonly == RO_LOCKED)
			printf("May not execute \"%s\" -- message file is read only locked.\n", com->c_name);
		else
			printf("May not execute \"%s\" -- message file is read only\n", com->c_name);
		if (loading)
			return(1);
		if (sourcing)
			unstack();
		return(0);
	}
	if (contxt && com->c_argtype & R) {
		printf("Cannot recursively invoke \"%s\"\n", com->c_name);
		return(0);
	}
	e = 1;
	switch (com->c_argtype & ~(F|P|I|M|T|W|R)) {
	case MSGLIST:
		/*
		 * A message list defaulting to nearest forward
		 * legal message.
		 */
		if (msgvec == 0) {
			printf("Illegal use of \"message list\"\n");
			return(-1);
		}
		if ((c = getmsglist(cp, msgvec, com->c_msgflag)) < 0)
			break;
		if (c  == 0) {
			*msgvec = first(com->c_msgflag,
				com->c_msgmask);
			msgvec[1] = NULL;
		}
		if (*msgvec == NULL) {
			printf("No applicable messages\n");
			break;
		}
		cp3 = com->c_name;
		if (((strcmp(cp3,"delete") == 0) ||
		     (strcmp(cp3,"dt") == 0) ||
       	 	     (strcmp(cp3,"dp") == 0 )) && (c != 0)) {
			printf("Deleting: ");
			pmsglist(msgvec);
			printf("\n");
		}
		e = (*com->c_func)(msgvec);
		break;

	case NDMLIST:
		/*
		 * A message list with no defaults, but no error
		 * if none exist.
		 */
		if (msgvec == 0) {
			printf("Illegal use of \"message list\"\n");
			return(-1);
		}
		if (getmsglist(cp, msgvec, com->c_msgflag) < 0)
			break;
		e = (*com->c_func)(msgvec);
		break;

	case STRLIST:
		/*
		 * Just the straight string, with
		 * leading and trailing blanks removed.
		 */
		while (isspace(*cp))
			cp++;
		cp2 = strlen(cp) + cp - 1;
		while (isspace(*cp2) && (cp2 > cp))
			cp2--;
		*++cp2 = '\0';
		e = (*com->c_func)(cp);
		break;

	case RAWLIST:
		/*
		 * A vector of strings, in shell style.
		 */
		if ((c = getrawlist(cp, arglist)) < 0)
			break;
		if (c < com->c_minargs) {
			printf("%s requires at least %d arg(s)\n",
				com->c_name, com->c_minargs);
			break;
		}
		if (c > com->c_maxargs) {
			printf("%s takes no more than %d arg(s)\n",
				com->c_name, com->c_maxargs);
			break;
		}
		e = (*com->c_func)(arglist);
		break;

	case ALIASLIST:
		/*
		 * A vector of strings, in 822 address style.
		 */
		if ((c = getaliaslist(cp, arglist)) < 0)
			break;
		if (c < com->c_minargs) {
			printf("%s requires at least %d arg(s)\n",
				com->c_name, com->c_minargs);
			break;
		}
		if (c > com->c_maxargs) {
			printf("%s takes no more than %d arg(s)\n",
				com->c_name, com->c_maxargs);
			break;
		}
		e = (*com->c_func)(arglist);
		break;

	case NOLIST:
		/*
		 * Just the constant zero, for exiting,
		 * eg.
		 */
		e = (*com->c_func)(0);
		break;

	default:
		panic("Unknown argtype");
	}

	/*
	 * Exit the current source file on
	 * error.
	 */

	if (e && loading)
		return(1);
	if (e && sourcing)
		unstack();
	if (com->c_func == edstop)
		return(1);
	if (value("autoprint") != NOSTR && com->c_argtype & P)
		if ((dot->m_flag & MDELETED) == 0) {
			muvec[0] = dot - &message[0] + 1;
			muvec[1] = 0;
			type(muvec);
		}
	if (!ismailx) {
		if (!sourcing && (com->c_argtype & T) == 0)
			sawcom = 1;
	} else {
		if (!sourcing)
			sawcom = 1;
	}
	return(0);
}

/*
 * When we wake up after ^Z, reprint the prompt.
 */
void
contin(s)
{
	char *cp;

	if (dashHflg)
		return;
	if ((cp = value("prompt")) != NOSTR)
		printf(cp);
	else {
		if (ismailx) {
			if (value("noprompt") != NOSTR)
				goto nopmt;
			else
				printf(mailx_prompt);
		} else
			printf(prompt);
	}
nopmt:
	fflush(stdout);
}

/*
 * Branch here on hangup signal and simulate quit.
 */
void
hangup()
{

	holdsigs();
	if (edit) {
		if (setexit()) {
			rounlock();
			exit(0);
		}
		edstop();
	}
	else
		quit();
	rounlock();
	exit(0);
}

/*
 * Set the size of the message vector used to construct argument
 * lists to message list functions.
 */
 
setmsize(sz)
{

	if (msgvec != (int *) 0)
		cfree(msgvec);
	msgvec = (int *) calloc((unsigned) (sz + 1), sizeof *msgvec);
}

/*
 * Find the correct command in the command table corresponding
 * to the passed command "word"
 */

struct cmd *
lex(word)
	char word[];
{
	register struct cmd *cp;
	extern struct cmd cmdtab[];

	for (cp = &cmdtab[0]; cp->c_name != NOSTR; cp++)
		if (isprefix(word, cp->c_name))
			return(cp);
	return(NONE);
}

/*
 * Find the correct command in the tilde-escape command table corresponding
 * to the passed command "word"
 */

struct tcmd *
tlex(str, cpp)
	char *str, **cpp;
{
	register struct tcmd *cp;
	extern struct tcmd tcmdtab[];

	for (cp = &tcmdtab[0]; cp->tc_name != NOSTR; cp++)
		if (istcmd(cp->tc_name, str, cpp))
			return(cp);
	return(TNONE);
}

/*
 * Determine if as1 is a valid prefix of as2.
 * Return true if yep.
 */

isprefix(as1, as2)
	char *as1, *as2;
{
	register char *s1, *s2;

	s1 = as1;
	s2 = as2;
	while (*s1++ == *s2)
		if (*s2++ == '\0')
			return(1);
	return(*--s1 == '\0');
}

/*
 * Determine if tilde command in cmd appears at the head of ustr.
 * Return true if yep.
 */

istcmd(cmd, ustr, cpp)
	char *cmd, *ustr, **cpp;
{
	register char *cmd1, *ustr1, firstchar;

	cmd1 = cmd;
	ustr1 = ustr;
	firstchar = *ustr;
	while (*cmd1++ == *ustr1) {
		if (*ustr1++ == '\0') {
			*cpp = --ustr1;
			return(1);
		}
	}
	*cpp = ustr1;
	if (isalpha(firstchar))
		return(*--cmd1 == '\0' && !isalpha(*ustr1));
	else
		return(*--cmd1 == '\0');
}

/*
 * The following gets called on receipt of a rubout.  This is
 * to abort printout of a command, mainly.
 * Dispatching here when command() is inactive crashes rcv.
 * Close all open files except 0, 1, 2, and the temporary.
 * The special call to getuserid() is needed so it won't get
 * annoyed about losing its open file.
 * Also, unstack all source files.
 */

extern int	inithdr;		/* am printing startup headers */

#ifdef _NFILE
static
_fwalk(function)
	register int (*function)();
{
	register FILE *iop;

#ifdef _MODERN_C
	for (iop = __iob; iop < __iob + _NFILE; iop++)
#else
	for (iop = _iob; iop < _iob + _NFILE; iop++)
#endif /* _MODERN_C */
		(*function)(iop);
}
#endif

static
xclose(iop)
	register FILE *iop;
{
	if (iop == stdin || iop == stdout ||
	    iop == stderr || iop == itf || iop == otf)
		return;

	if (iop != pipef)
		fclose(iop);
	else {
		pclose(pipef);
		pipef = NULL;
	}
}

void
stop(s)
{
	register FILE *fp;

# ifndef VMUNIX
	s = SIGINT;
# endif
	noreset = 0;
	if (!inithdr)
		sawcom++;
	inithdr = 0;
	while (sourcing)
		unstack();
	getuserid((char *) -1);

	/*
	 * Walk through all the open FILEs, applying xclose() to them
	 */
	_fwalk(xclose);

	if (image >= 0) {
		close(image);
		image = -1;
	}
	fprintf(stderr, "Interrupt\n");

# ifndef VMUNIX
	sigrelse(s);
# endif
	reset(0);
}

/*
 * Announce the presence of the current Mail version,
 * give the message count, and print a header listing.
 */

char	*greeting	= "Mail version %s.  Type ? for help.\n";

announce(pr)
{
	int vec[2], mdot;
	extern char *version;

	if (pr && value("quiet") == NOSTR)
		printf(greeting, version);
	if (dashHflg)
		mdot = 1;
	else
		mdot = newfileinfo();
	vec[0] = mdot;
	vec[1] = 0;
	dot = &message[mdot - 1];
	if (msgCount > 0 && !noheader) {
		inithdr++;
		headers(vec);
		inithdr = 0;
	}
}

/*
 * Announce information about the file we are editing.
 * Return a likely place to set dot.
 */
newfileinfo()
{
	register struct message *mp;
	register int u, n, mdot, d, s;
	char fname[BUFSIZ], zname[BUFSIZ], *ename;

	for (mp = &message[0]; mp < &message[msgCount]; mp++)
		if (mp->m_flag & MNEW)
			break;
	if (mp >= &message[msgCount])
		for (mp = &message[0]; mp < &message[msgCount]; mp++)
			if ((mp->m_flag & MREAD) == 0)
				break;
	if (mp < &message[msgCount]) {
		mdot = mp - &message[0] + 1;
	} else if (value("showlast") != NOSTR) {
		mdot = msgCount;
	} else {
		mdot = 1;
	}
	s = d = 0;
	for (mp = &message[0], n = 0, u = 0; mp < &message[msgCount]; mp++) {
		if (mp->m_flag & MNEW)
			n++;
		if ((mp->m_flag & MREAD) == 0)
			u++;
		if (mp->m_flag & MDELETED)
			d++;
		if (mp->m_flag & MSAVED)
			s++;
	}
	ename = mailname;
	if (getfold(fname, BUFSIZ) >= 0) {
		strcat(fname, "/");
		if (strncmp(fname, mailname, strlen(fname)) == 0) {
			snprintf(zname, BUFSIZ, "+%s", mailname + strlen(fname));
			ename = zname;
		}
	}
	printf("\"%s\": ", ename);
	if (msgCount == 1)
		printf("1 message");
	else
		printf("%d messages", msgCount);
	if (n > 0)
		printf(" %d new", n);
	if (u-n > 0)
		printf(" %d unread", u);
	if (d > 0)
		printf(" %d deleted", d);
	if (s > 0)
		printf(" %d saved", s);
	if (readonly)
		printf(" [Read only]");
	printf("\n");
	return(mdot);
}

strace() {}

/*
 * Print the current version number.
 */

pversion(e)
{
	extern char *version;

	printf("Version %s\n", version);
	return(0);
}

static void
check_mailrc(FILE *infd)
{
	extern struct bcmd badcmdtab[];
	register struct bcmd *bp;
	char linebuf[LINESIZE];

	for (;;) {
		if (readline(infd, &linebuf[0]) <= 0) {
			return;
		}
		if (strlen(linebuf) == 0)
			break;
		for (bp = &badcmdtab[0]; bp->bc_cmdname != NOSTR; bp++) {
			if (!strncmp(linebuf, bp->bc_cmdname, 
					strlen(bp->bc_cmdname)))
			{
				mailrcerr = 1;
				fprintf(stderr, 
					"Invalid mailrc startup command: %s\n",
						linebuf);
			}
		}
	}
	return;
}

/*
 * Load a file of user definitions.
 */
load(name)
	char *name;
{
	register FILE *in, *oldin;

	mailrcerr = 0;
	if ((in = fopen(name, "r")) == NULL)
		return;
	if (name == mailrc) {
		check_mailrc(in);
		fclose(in);
		if ((in = fopen(name, "r")) == NULL)
			return;
	}
	strncpy(lfile, name, PATHSIZE);
	oldin = input;
	input = in;
	loading = 1;
	sourcing = 1;
	commands();
	loading = 0;
	sourcing = 0;
	input = oldin;
	fclose(in);
}

safe_exit(val)
{
	/*
	 * Typically, in case of errors, it would be unwise
	 * to exit without removing lock files. Although these
	 * actions might be redundant, it's better to ensure 
	 * that the lock files are indeed removed before we 
	 * actually exit the program.
	 */
	rounlock();
	unlock();
	exit (val);
	fprintf(stderr, "SAFE EXIT\n");
}
