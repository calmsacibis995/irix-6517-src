/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)collect.c	5.2 (Berkeley) 6/21/85";
#endif /* !lint */

/*
 * Mail -- a mail program
 *
 * Collect input from standard input, handling
 * ~ escapes.
 */

#include "glob.h"
#include <sys/stat.h>
#include <ctype.h>
#include <termio.h>

/*
 * Read a message from standard output and return a read file to it
 * or NULL on error.
 */

/*
 * The following hokiness with global variables is so that on
 * receipt of an interrupt signal, the partial message can be salted
 * away on dead.letter.  The output file must be available to flush,
 * and the input to read.  Several open files could be saved all through
 * Mail if stdio allowed simultaneous read/write access.
 */

#if defined(SVR3) || defined(_SVR4_SOURCE)
static	void	(*savesig)();		/* Previous SIGINT value */
static	void	(*savehup)();		/* Previous SIGHUP value */
#ifdef VMUNIX
static	void	(*savecont)();		/* Previous SIGCONT value */
#endif /* VMUNIX */
#else
static	int	(*savesig)();		/* Previous SIGINT value */
static	int	(*savehup)();		/* Previous SIGHUP value */
#ifdef VMUNIX
static	int	(*savecont)();		/* Previous SIGCONT value */
#endif /* VMUNIX */
#endif /* SVR3 || _SVR4_SOURCE */
static	FILE	*newi;			/* File for saving away */
static	FILE	*newo;			/* Output side of same */
static	int	hf;			/* Ignore interrups */
static	int	hadintr;		/* Have seen one SIGINT so far */

static	jmp_buf	coljmp;			/* To get back to work */

void collrub(int);
void resetsigs(FILE *, FILE *);

FILE *
collect(hp)
	struct header *hp;
{
	FILE *ibuf, *fbuf, *obuf;
	int lc, cc, escape, eof, frtn;
	register int c, t;
	char linebuf[LINESIZE], *cp;
	extern char tempMail[];
	void collintsig(), collhupsig(), intack(), collcont();
	char getsub;
	struct tcmd *tcom;

	noreset++;
	ibuf = obuf = NULL;
	if (value("ignore") != NOSTR)
		hf = 1;
	else
		hf = 0;
	hadintr = 0;
#ifdef VMUNIX
	if ((savesig = sigset(SIGINT, SIG_IGN)) != SIG_IGN)
		sigset(SIGINT, hf ? intack : collrub), sigblock(sigmask(SIGINT));
	if ((savehup = sigset(SIGHUP, SIG_IGN)) != SIG_IGN)
		sigset(SIGHUP, collrub), sigblock(sigmask(SIGHUP));
	savecont = sigset(SIGCONT, collcont);
#else /* !VMUNIX */
	savesig = signal(SIGINT, SIG_IGN);
	savehup = signal(SIGHUP, SIG_IGN);
#endif /* VMUNIX */
	newi = NULL;
	newo = NULL;
	if ((obuf = fopen(tempMail, "w")) == NULL){
		perror(tempMail);
		resetsigs(ibuf, obuf);
		return(NULL);
	}
	newo = obuf;
	if ((ibuf = fopen(tempMail, "r")) == NULL) {
		perror(tempMail);
		newo = NULL;
		fclose(obuf);
		resetsigs(ibuf, obuf);
		return(NULL);
	}
	newi = ibuf;
	m_remove(tempMail);

	/*
	 * If we are going to prompt for a subject,
	 * refrain from printing the new line after
	 * the headers.
	 */

	t = (GALLHDR|GNL)&~GBCC; 
	getsub = 0;
	if (intty && sflag == NOSTR &&
	    hp->h_subject == NOSTR &&
	    value("ask")) {
		t &= ~GNL;
		getsub++;
	}
	if (hp->h_seq != 0) {
		puthead(hp, stdout, t);
		fflush(stdout);
	}
	escape = ESCAPE;
	if ((cp = value("escape")) != NOSTR)
		escape = *cp;
	eof = 0;
	for (;;) {
#ifdef	VMUNIX
		int omask = sigblock(0) &~ (sigmask(SIGINT)|sigmask(SIGHUP));
#endif

		setjmp(coljmp);
#ifdef VMUNIX
		sigsetmask(omask);
#else /* !VMUNIX */
		if (savesig != SIG_IGN) {
			if(hf)	/* work around compiler bug; can't use ? : . 3/89 */
				signal(SIGINT, intack);
			else
				signal(SIGINT, collintsig);
		}
		if (savehup != SIG_IGN)
			signal(SIGHUP, collhupsig);
#endif /* VMUNIX */
		fflush(stdout);
		if (getsub) {
			grabh(hp, GSUBJECT);
			getsub = 0;
			continue;
		}
		if (readline(stdin, linebuf) <= 0) {
			if (intty && value("ignoreeof") != NOSTR) {
				if (++eof > 35)
					break;
				printf("Use \".\" to terminate letter\n");
				continue;
			}
			break;
		}
		eof = 0;
		hadintr = 0;
		if (intty && equal(".", linebuf) &&
		    (value("dot") != NOSTR || value("ignoreeof") != NOSTR))
			break;
		if (linebuf[0] != escape || rflag != NOSTR) {
			if ((t = putline(obuf, linebuf)) < 0) {
				resetsigs(ibuf, obuf);
				return(NULL);
			}
			continue;
		}
		if ((linebuf[0] == escape) && (linebuf[1] == escape)) {
			if (putline(obuf, &linebuf[1]) < 0) { 
				resetsigs(ibuf, obuf);
				return(NULL);
			}
		}
		else if (linebuf[1] == '.') {
			frtn = 0;
			goto eofl;
		}
		else {
			tcom = tlex(&linebuf[1], &cp);
			if (tcom == TNONE) 
				printf("Unknown command: \"%s\"\n",
				       &linebuf[1]);
			else {
				switch(tcom->tc_argtype) {
				case TNOARG:
					frtn = (*tcom->tc_func)(0);
					break;

				case TSTR:
					frtn = (*tcom->tc_func)(cp);
					break;

				case THDR:
					frtn = (*tcom->tc_func)(hp);
					break;

				case TBUF:
					frtn = (*tcom->tc_func)(&ibuf, &obuf);
					break;

				case THDRSTR:
					frtn = (*tcom->tc_func)(hp, cp);
					break;

				case TBUFSTR:
					frtn = (*tcom->tc_func)(&ibuf, &obuf,
							        cp);
					break;
				
				case TBUFHDR:
					frtn = (*tcom->tc_func)(&ibuf, &obuf,
							        hp);
					break;
				}
				if (frtn)
					return(NULL);
			}
		}
	}

eofl:
{
	char xsigned[PATHSIZE];
	char xsigned2[PATHSIZE];
        struct name *names;
        int remote;


	/*
	 * Signature search:
	 *   1. If no '!' in "to" field try ~/.lsignature
	 *      otherwise try ~/.rsignature.
	 *   2. If no such file, try ~/.signature (also
	 *      used by netnews).
	 */
	strncpy(xsigned, homedir, PATHSIZE);
	xsigned[PATHSIZE - 1] = '\0';
	strncpy(xsigned2,homedir, PATHSIZE);
	xsigned2[PATHSIZE - 1] = '\0';
        remote = 0;
        if (0 != hp->h_to) {
                names = usermap(extract(hp->h_to));
                while (names != NULL) {
                        if (strchr(names->n_name,'!') 
                                || strchr(names->n_name,'@')) {
                                remote++;
                                break;
                        }
                        names = names->n_flink;
                }
		/* make sure the strcat() has enough room before copy */
		if(((PATHSIZE - 1) - strlen(xsigned)) >= strlen("/.rsignature"))
                	strcat(xsigned, ((remote != 0) ?
				"/.rsignature" : "/.lsignature")); 
        }
	/* make sure the strcat() has enough room before copy */
	if(((PATHSIZE - 1) - strlen(xsigned2)) >= strlen("/.signature"))
		strcat(xsigned2,"/.signature");
	if (((fbuf = fopen(xsigned, "r")) != NULL) ||
	    ((fbuf = fopen(xsigned2, "r")) != NULL)) {
		fflush(stdout);
		fflush(stderr);
		lc = 0;
		cc = 0;
		while (readline(fbuf, linebuf) > 0) {
			lc++;
			if ((t = putline(obuf, linebuf)) < 0) {
				fclose(fbuf);
				resetsigs(ibuf, obuf);
				return(NULL);
			}
			cc += t;
		}
		fclose(fbuf);
	}
}
	fclose(obuf);
	rewind(ibuf);
	sigset(SIGINT, savesig);
	sigset(SIGHUP, savehup);
#ifdef VMUNIX
	sigset(SIGCONT, savecont);
	sigsetmask(0);
#endif /* VMUNIX */
	noreset = 0;
	return(ibuf);
}


void
resetsigs(FILE *ibuf, FILE *obuf)
{
	if (ibuf != NULL)
		fclose(ibuf);
	if (obuf != NULL)
		fclose(obuf);
	sigset(SIGINT, savesig);
	sigset(SIGHUP, savehup);
#ifdef VMUNIX
	sigset(SIGCONT, savecont);
	sigsetmask(0);
#endif /* VMUNIX */
	noreset = 0;
	return;
}


esctocmd(str)
	char *str;
{
	execute(str, 1);
	printf("(continue)\n");
	return(0);
}

tintr()
{
	hadintr++;
	collrub(SIGINT);
	safe_exit(1);
}

/*
 * Grab the "usual" headers and any others that are now in use.
 */
grabhdrs(hp)
	struct header *hp;
{
	if (!intty || !outtty)
		printf("~h: no can do!?\n");
	else {
		grabh(hp, GTO|GCC|GBCC|GSUBJECT|hp->h_optusedmask);
		printf("(continue)\n");
	}
	return(0);
}

/*
 * Grab all the headers.
 */
graballhdrs(hp)
	struct header *hp;
{
	if (!intty || !outtty)
		printf("~H: no can do!?\n");
	else {
		grabh(hp, GREGHDR);
		printf("(continue)\n");
	}
	return(0);
}

/*
 * Add to the In-Reply-To list.
 */
addirt(hp, str)
	struct header *hp;
	char *str;
{
	if (notempty(str)) {
		hp->h_inreplyto = addto(hp->h_inreplyto, str);
		hp->h_optusedmask |= GIRT;
	}
	hp->h_seq++;
	return(0);
}


/*
 * Add to the Keywords list.
 */
addkey(hp, str)
	struct header *hp;
	char *str;
{
	if (notempty(str)) {
		hp->h_keywords = addto(hp->h_keywords, str);
		hp->h_optusedmask |= GKEY;
	}
	hp->h_seq++;
	return(0);
}

/*
 * Add to the To list.
 */
addt(hp, str)
	struct header *hp;
	char *str;
{
	hp->h_to = addto(hp->h_to, str);
	hp->h_seq++;
	return(0);
}

/*
 * Set the Subject list.
 */
setsubj(hp, str)
	struct header *hp;
	char *str;
{
	while (isspace(*str))
		str++;
	hp->h_subject = savestr(str);
	hp->h_seq++;
	return(0);
}

/*
 * Add to the cc list.
 */
addcc(hp, str)
	struct header *hp;
	char *str;
{
	hp->h_cc = addto(hp->h_cc, str);
	hp->h_seq++;
	return(0);
}

/*
 * Add to the bcc list.
 */
addbcc(hp, str)
	struct header *hp;
	char *str;
{
	hp->h_bcc = addto(hp->h_bcc, str);
	hp->h_seq++;
	return(0);
}

/*
 * Set the Comments line.
 */
setcomments(hp, str)
	struct header *hp;
	char *str;
{
	while (isspace(*str))
		str++;
	if (*str != '\0')
		hp->h_optusedmask |= GCOM;
	else
		hp->h_optusedmask &= ~GCOM;
	hp->h_comments = savestr(str);
	hp->h_seq++;
	return(0);
}

/*
 * Read deadletter into message.
 */
getdead(ibufp, obufp)
	FILE **ibufp, **obufp;
{
	return(getfile(ibufp, obufp, deadletter));
}

/*
 * Read file into message
 */
getfile(ibufp, obufp, str)
	FILE **ibufp, **obufp;
	char *str;
{
	register int lc, cc, t;
	char *cp, linebuf[LINESIZE];
	FILE *fbuf;


	cp = str;
	while (isspace(*cp))
		cp++;
	if (*cp == '\0') {
		printf("Interpolate what file?\n");
		return(0);
	}
	cp = expand(cp);
	if (cp == NOSTR)
		return(0);
	if (isdir(cp)) {
		printf("%s: directory\n", cp);
		return(0);
	}
	if ((fbuf = fopen(cp, "r")) == NULL) {
		perror(cp);
		return(0);
	}
	printf("\"%s\" ", cp);
	fflush(stdout);
	lc = 0;
	cc = 0;
	while (readline(fbuf, linebuf) > 0) {
		lc++;
		if ((t = putline(*obufp, linebuf)) < 0) {
			fclose(fbuf);
			resetsigs(*ibufp, *obufp);
			return(1);
		}
		cc += t;
	}
	fclose(fbuf);
	printf("%d/%d\n", lc, cc);
	return(0);
}

/*
 * Add to the References list.
 */
addref(hp, str)
	struct header *hp;
	char *str;
{
	if (notempty(str)) {
		hp->h_references = addto(hp->h_references, str);
		hp->h_optusedmask |= GREF;
	}
	hp->h_seq++;
	return(0);
}


/*
 * Add to Reply-to header field.
 */
addreplyto(hp, str)
	struct header *hp;
	char *str;
{
	if (notempty(str)) {
		hp->h_replyto = addto(hp->h_replyto, str);
		hp->h_optusedmask |= GRT;
	}
	hp->h_seq++;
	return(0);
}

/*
 * Set the Return-Receipt-To header field.
 */
addrtnrcpt(hp, str)
	struct header *hp;
	char *str;
{
	if (notempty(str)) {
		hp->h_rtnrcpt = addto(hp->h_rtnrcpt, str);
		hp->h_optusedmask |= GRR;
	}
	else if (hp->h_rtnrcpt == NOSTR) {
		hp->h_rtnrcpt = addto(hp->h_rtnrcpt, myname);
		hp->h_optusedmask |= GRR;
		hp->h_seq++;
		printf("Return receipt to \"%s\" added.\n", myname);
	}
	else {
		hp->h_rtnrcpt = NOSTR;
		hp->h_optusedmask &= ~GRR;
		hp->h_seq++;
		printf("Return receipt removed.\n");
	}
	printf("(continue)\n");
	return(0);
}

/*
 * Write the message on a file.
 */
putfile(ibufp, obufp, str)
	FILE **ibufp, **obufp;
	char *str;
{
	char *cp;
	extern char tempMail[];

	cp = str;
	while (isspace(*cp))
		cp++;
	if (*cp == '\0') {
		fprintf(stderr, "Write what file!?\n");
		return(0);
	}
	if ((cp = expand(cp)) == NOSTR)
		return(0);
	fflush(*obufp);
	if (ferror(*obufp)) {
		perror(tempMail);
		return(0);
	}
	rewind(*ibufp);
	exwrite(cp, *ibufp, 1);
	return(0);
}

/*
 * Interpolate the named messages with shift, if we
 * are in receiving mail mode.  Does the
 * standard list processing garbage.
 */
fgetmessage(ibufp, obufp, str)
	FILE **ibufp, **obufp;
	char *str;
{
	int c;
	char *cp;

	if (!rcvmode) {
		printf("No messages to send from!?!\n");
		return(0);
	}
	cp = str;
	while (isspace(*cp))
		cp++;
	c = 'f';
	if (forward(cp, *obufp, c) < 0) {
		resetsigs(*ibufp, *obufp);
		return(1);
	}
	printf("(continue)\n");
	return(0);
}

/*
 * Interpolate the named messages without shift, if we
 * are in receiving mail mode.  Does the
 * standard list processing garbage.
 */
mgetmessage(ibufp, obufp, str)
	FILE **ibufp, **obufp;
	char *str;
{
	int c;
	char *cp;

	if (!rcvmode) {
		printf("No messages to send from!?!\n");
		return(0);
	}
	cp = str;
	while (isspace(*cp))
		cp++;
	c = 'm';
	if (forward(cp, *obufp, c) < 0) {
		resetsigs(*ibufp, *obufp);
		return(1);
	}
	printf("(continue)\n");
	return(0);
}

/*
 * Print the help file.
 */
helpme()
{
	register int t;
	FILE *fbuf;

	if ((fbuf = fopen(THELPFILE, "r")) == NULL) {
		perror(THELPFILE);
		return(0);
	}
	t = getc(fbuf);
	while (t != -1) {
		putchar(t);
		t = getc(fbuf);
	}
	fclose(fbuf);
	printf("(continue)\n");
	return(0);
}

/*
 * Print out the current state of the
 * message without altering anything.
 */
printmessage(ibufp, obufp, hp)
	FILE **ibufp, **obufp;
	struct header *hp;
{
	register int t;
	extern char tempMail[];

	fflush(*obufp);
	if (ferror(*obufp)) {
		perror(tempMail);
		return(0);
	}
	rewind(*ibufp);
{
	FILE * pbuf;
	extern jmp_buf pipestop;
	extern void brokpipe();
	int nwcols;
	char *cp;

	pbuf = stdout;
	if (setjmp(pipestop)) {
		if (pbuf != stdout)
			pclose(pbuf);
		sigset(SIGPIPE, SIG_DFL);
		return(0);
	}
	if (intty && outtty) {
		nwcols = getwincols();
		if (getheadlen(hp, nwcols)
		    + gettextlen(*ibufp, nwcols) + 2
		    > getwinlines(0)) {
			pbuf = popen(MORE, "w");
			if (pbuf == NULL) {
				perror(MORE);
				pbuf = stdout;
			} else
				sigset(SIGPIPE, brokpipe);
		}
	}

	fprintf(pbuf, "-------\nMessage contains:\n");
	puthead(hp, pbuf, GALLHDR|GNL);
	t = getc(*ibufp);
	while (t != EOF) {
		putc(t, pbuf);
		t = getc(*ibufp);
	}
	if (pbuf != stdout)
		pclose(pbuf);
}
	printf("(continue)\n");
	return(0);
}

/*
 * Pipe message through command.
 * Collect output as new message.
 */

pipemessage(ibufp, obufp, str)
	FILE **ibufp, **obufp;
	char *str;
{
	*obufp = mespipe(*ibufp, *obufp, str);
	newo = *obufp;
	*ibufp = newi;
	newi = *ibufp;
	printf("(continue)\n");
	return(0);
}

/*
 * Edit the current message body only using EDITOR.
 */
eeditmsgbody(ibufp, obufp, hp)
	FILE **ibufp, **obufp;
{
	if ((*obufp = mesedit(*ibufp, *obufp, hp, 'e', 0)) == NULL) {
		resetsigs(*ibufp, *obufp);
		return(1);
	}
	newo = *obufp;
	*ibufp = newi;
	printf("(continue)\n");
	return(0);
}

/*
 * Edit the current message body only using VISUAL.
 */
veditmsgbody(ibufp, obufp, hp)
	FILE **ibufp, **obufp;
{
	if ((*obufp = mesedit(*ibufp, *obufp, hp, 'v', 0)) == NULL) {
		resetsigs(*ibufp, *obufp);
		return(1);
	}
	newo = *obufp;
	*ibufp = newi;
	printf("(continue)\n");
	return(0);
}

/*
 * Edit the complete message including the headers using EDITOR.
 */
eeditmsg(ibufp, obufp, hp)
	FILE **ibufp, **obufp;
	struct header *hp;
{
	if ((*obufp = mesedit(*ibufp, *obufp, hp, 'e', 1)) == NULL) {
		resetsigs(*ibufp, *obufp);
		return(1);
	}
	newo = *obufp;
	*ibufp = newi;
	printf("(continue)\n");
	return(0);
}

/*
 * Edit the complete message including the headers using VISUAL.
 */
veditmsg(ibufp, obufp, hp)
	FILE **ibufp, **obufp;
	struct header *hp;
{
	if ((*obufp = mesedit(*ibufp, *obufp, hp, 'v', 1)) == NULL) {
		resetsigs(*ibufp, *obufp);
		return(1);
	}
	newo = *obufp;
	*ibufp = newi;
	printf("(continue)\n");
	return(0);
}

/*
 * Set the Encrypt field.
 */
setencrypt(hp, str)
	struct header *hp;
	char *str;
{
	while (isspace(*str))
		str++;
	if (*str != '\0')
		hp->h_optusedmask |= GEN;
	else
		hp->h_optusedmask &= ~GEN;
	hp->h_encrypt = savestr(str);
	hp->h_seq++;
	return(0);
}

notempty(str)
	char *str;
{
	while(isspace(*str))
		str++;
	return(*str != '\0');
}

/*
 * Write a file, ex-like if f set.
 */

exwrite(name, ibuf, f)
	char name[];
	FILE *ibuf;
{
	register FILE *of;
	register int c;
	long cc;
	int lc;
	struct stat junk;

	if (f) {
		printf("\"%s\" ", name);
		fflush(stdout);
	}
	if (stat(name, &junk) >= 0 && (junk.st_mode & S_IFMT) == S_IFREG) {
		if (!f)
			fprintf(stderr, "%s: ", name);
		fprintf(stderr, "File exists\n");
		return(-1);
	}
	if ((of = fopen(name, "w")) == NULL) {
		perror("");
		return(-1);
	}
	lc = 0;
	cc = 0;
	while ((c = getc(ibuf)) != EOF) {
		cc++;
		if (c == '\n')
			lc++;
		putc(c, of);
		if (ferror(of)) {
			perror(name);
			fclose(of);
			return(-1);
		}
	}
	fclose(of);
	printf("%d/%ld\n", lc, cc);
	fflush(stdout);
	return(0);
}

/*
 * Edit the message being collected on ibuf and obuf.
 * Write the message out onto some poorly-named temp file
 * and point an editor at it.
 *
 * On return, make the edit file the new temp file.
 */

FILE *
mesedit(ibuf, obuf, hp, cmd, dohdr)
	FILE *ibuf, *obuf;
	struct header *hp;
	int cmd, dohdr;
{
	int pid, s;
	FILE *fbuf;
	register int t;
#if defined(SVR3) || defined(_SVR4_SOURCE)
	void (*sig)(), (*scont)(), signull();
#else
	int (*sig)(), (*scont)(), signull();
#endif
	struct stat sbuf;
	extern char tempEdit[], tempMail[];
	register char *edit;

	sig = sigset(SIGINT, SIG_IGN);
#ifdef VMUNIX
	scont = sigset(SIGCONT, signull);
#endif /* VMUNIX */

	/* create a temp file */
	if (stat(tempEdit, &sbuf) >= 0) {
		printf("%s: file exists\n", tempEdit);
		goto out;
	}
	close(creat(tempEdit, 0600));
	if ((fbuf = fopen(tempEdit, "w")) == NULL) {
		perror(tempEdit);
		goto out;
	}

	if (dohdr) {
		/* write all current headers into the temp file */
		puthead(hp, fbuf, GALLHDR|GSEP);
	}

	/* write out the message body collected so far */
	fflush(obuf);
	if (ferror(obuf)) {
		perror(tempMail);
		fclose(fbuf);
		m_remove(tempEdit);
		goto out;
	}
	rewind(ibuf);
	t = getc(ibuf);
	while (t != EOF) {
		putc(t, fbuf);
		t = getc(ibuf);
	}
	fflush(fbuf);
	if (ferror(fbuf)) {
		perror(tempEdit);
		m_remove(tempEdit);
		goto fix;
	}
	fclose(fbuf);

	/* fork an editor on the message */
	if ((edit = value(cmd == 'e' ? "EDITOR" : "VISUAL")) == NOSTR)
		edit = cmd == 'e' ? EDITOR : VISUAL;
	pid = fork();
	if (pid == 0) {
		/* CHILD - exec the editor. */
		sigchild();
		if (sig != SIG_IGN)
			sigsys(SIGINT, SIG_DFL);
		execl(edit, edit, tempEdit, 0);
		perror(edit);
		_exit(1);
	}
	if (pid == -1) {
		/* ERROR - issue warning and get out. */
		perror("fork");
		m_remove(tempEdit);
		goto out;
	}
	/* PARENT - wait for the child (editor) to complete. */
	while (wait(&s) != pid)
		;

	/* Check the results. */
	if ((s & 0377) != 0) {
		printf("Fatal error in \"%s\"\n", edit);
		m_remove(tempEdit);
		goto out;
	}

	/* If necessary, extract the headers from the resulting file. */
	if (dohdr && extracthead(hp, tempEdit) < 0) {
		perror(tempEdit);
		m_remove(tempEdit);
		goto out;
	}

	/* Switch files */
	if ((fbuf = fopen(tempEdit, "a")) == NULL) {
		perror(tempEdit);
		m_remove(tempEdit);
		goto out;
	}
	if ((ibuf = fopen(tempEdit, "r")) == NULL) {
		perror(tempEdit);
		fclose(fbuf);
		m_remove(tempEdit);
		goto out;
	}
	m_remove(tempEdit);
	fclose(obuf);
	fclose(newi);
	obuf = fbuf;
	goto out;
fix:
	perror(tempEdit);
out:
#ifdef VMUNIX
	sigset(SIGCONT, scont);
#endif /* VMUNIX */
	sigset(SIGINT, sig);
	newi = ibuf;
	return(obuf);
}

/*
 * Pipe the message through the command.
 * Old message is on stdin of command;
 * New message collected from stdout.
 * Sh -c must return 0 to accept the new message.
 */

FILE *
mespipe(ibuf, obuf, cmd)
	FILE *ibuf, *obuf;
	char cmd[];
{
	register FILE *ni, *no;
	int pid, s;
#if defined(SVR3) || defined(_SVR4_SOURCE)
	void (*savesig)();
#else
	int (*savesig)();
#endif
	char *Shell;
	extern char tempEdit[], tempMail[];

	newi = ibuf;
	if ((no = fopen(tempEdit, "w")) == NULL) {
		perror(tempEdit);
		return(obuf);
	}
	if ((ni = fopen(tempEdit, "r")) == NULL) {
		perror(tempEdit);
		fclose(no);
		m_remove(tempEdit);
		return(obuf);
	}
	m_remove(tempEdit);
	savesig = sigset(SIGINT, SIG_IGN);
	fflush(obuf);
	if (ferror(obuf)) {
		perror(tempMail);
		fclose(ni);
		fclose(no);
		m_remove(tempEdit);
		return(obuf);
	}
	rewind(ibuf);
	if (((Shell = value("SHELL")) == NULL) || (Shell[0] == '\0'))
		Shell = "/bin/sh";
	if ((pid = fork()) == -1) {
		perror("fork");
		goto err;
	}
	if (pid == 0) {
		/*
		 * stdin = current message.
		 * stdout = new message.
		 */
		sigchild();
		close(0);
		dup(fileno(ibuf));
		close(1);
		dup(fileno(no));
		for (s = 4; s < 15; s++)
			close(s);
		execl(Shell, Shell, "-c", cmd, 0);
		perror(Shell);
		_exit(1);
	}
	while (wait(&s) != pid)
		;
	if (s != 0 || pid == -1) {
		fprintf(stderr, "\"%s\" failed!?\n", cmd);
		goto err;
	}
	if (fsize(ni) == 0) {
		fprintf(stderr, "No bytes from \"%s\" !?\n", cmd);
		goto err;
	}

	/*
	 * Take new files.
	 */

	newi = ni;
	fclose(ibuf);
	fclose(obuf);
	sigset(SIGINT, savesig);
	return(no);

err:
	fclose(no);
	fclose(ni);
	sigset(SIGINT, savesig);
	return(obuf);
}

/*
 * Interpolate the named messages into the current
 * message, preceding each line with a tab or user-defined
 * string.
 *
 * Return a count of the number of characters now in
 * the message, or -1 if an error is encountered writing
 * the message temporary.  The flag argument is 'm' if we
 * should shift over and 'f' if not.
 */
forward(ms, obuf, f)
	char ms[];
	FILE *obuf;
{
	register int *msgvec, *ip;
	int lcnt;
	long ccnt;
	extern char tempMail[];

	msgvec = (int *) salloc((msgCount+1) * sizeof *msgvec);
	if (msgvec == (int *) NOSTR)
		return(0);
	if (getmsglist(ms, msgvec, 0) < 0)
		return(0);
	if (*msgvec == NULL) {
		*msgvec = first(0, MMNORM);
		if (*msgvec == NULL) {
			printf("No appropriate messages\n");
			return(0);
		}
		msgvec[1] = NULL;
	}
	printf("Interpolating:");
	for (ip = msgvec; *ip != NULL; ip++) {
		touch(*ip);
		printf(" %d", *ip);
		if (f == 'm') {
			if (transmit(&message[*ip-1], obuf) < 0L) {
				perror(tempMail);
				return(-1);
			}
		} else
			if (send_msg(&message[*ip-1], obuf, NO_SUPPRESS,
				     &lcnt, &ccnt) < 0) {
				perror(tempMail);
				return(-1);
			}
	}
	printf("\n");
	return(0);
}

/*
 * Send message described by the passed pointer to the
 * passed output buffer.  Insert a tab or user-defined
 * character in front of each line.  Return a count of
 * the characters sent, or -1 on error.
 */

long
transmit(mailp, obuf)
	struct message *mailp;
	FILE *obuf;
{
	register struct message *mp;
	register int ch;
	char *istr;
	u_long c, n;
	int bol;
	FILE *ibuf;

	if ((istr = value("mprefix")) == NOSTR)
		istr = "\t";
	mp = mailp;
	ibuf = setinput(mp);
	c = mp->m_size;
	n = c;
	bol = 1;
	while (c-- > 0L) {
		if (bol) {
			bol = 0;
			fprintf(obuf, "%s", istr);
			n++;
			if (ferror(obuf)) {
				perror("transmit");
				return(-1L);
			}
		}
		ch = getc(ibuf);
		if (ch == '\n')
			bol++;
		putc(ch, obuf);
		if (ferror(obuf)) {
			perror("transmit");
			return(-1L);
		}
	}
	return(n);
}

/*
 * Print (continue) when continued after ^Z.
 */
void
collcont(s)
{

	printf("(continue)\n");
	fflush(stdout);
}

/*
 * On interrupt, go here to save the partial
 * message on ~/dead.letter.
 * Then restore signals and execute the normal
 * signal routine.  We only come here if signals
 * were previously set anyway.
 */

#ifndef VMUNIX
void
collintsig()
{
	signal(SIGINT, SIG_IGN);
	collrub(SIGINT);
}

void
collhupsig()
{
	signal(SIGHUP, SIG_IGN);
	collrub(SIGHUP);
}
#endif /* !VMUNIX */

void
collrub(s)
{
	register FILE *dbuf;
	register int c;

	if (s == SIGINT && hadintr == 0) {
		hadintr++;
		fflush(stdout);
		fprintf(stderr, "\n(Interrupt -- one more to kill letter)\n");
		longjmp(coljmp, 1);
	}
	fclose(newo);
	rewind(newi);
	if (s == SIGINT && value("nosave") != NOSTR || fsize(newi) == 0)
		goto done;
	if ((dbuf = fopen(deadletter, "w")) == NULL)
		goto done;
	chmod(deadletter, 0600);
	while ((c = getc(newi)) != EOF)
		putc(c, dbuf);
	fclose(dbuf);

done:
	fclose(newi);
	sigset(SIGINT, savesig);
	sigset(SIGHUP, savehup);
#ifdef VMUNIX
	sigset(SIGCONT, savecont);
#endif /* VMUNIX */
	if (rcvmode) {
		if (s == SIGHUP)
			hangup(SIGHUP);
		else
			stop(s);
	}
	else 
		safe_exit(1);
}

/*
 * Acknowledge an interrupt signal from the tty by typing an @
 */

void
intack(s)
{
	
	puts("@");
	fflush(stdout);
	clearerr(stdin);
}

/*
 * Add a string to the end of a header entry field.
 */

char *
addto(hf, news)
	char hf[], news[];
{
	register char *cp, *cp2, *linebuf;

	for (cp = news; isspace(*cp); cp++)
		;
	if (*cp == '\0')
		return(hf);
	if (hf == NOSTR) 
		hf = cp = "";
	linebuf = salloc(strlen(hf) + strlen(news) + 2);
	cp2 = linebuf;
	if (hf != cp) {
		for (cp = hf; isspace(*cp); cp++)
			;
		for (cp2 = linebuf; *cp;)
			*cp2++ = *cp++;
		*cp2++ = ','; 
	}
	for (cp = news; isspace(*cp); cp++)
		;
	while (*cp != '\0') {
		if (isspace(*cp)) {
			for (; isspace(*cp); cp++);
			*cp2++ = ','; 
		}
		*cp2++ = *cp++;
	}
	*cp2 = '\0';
	return(linebuf);
}
