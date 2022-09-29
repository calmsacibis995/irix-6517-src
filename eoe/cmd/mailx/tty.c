/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)tty.c	5.2 (Berkeley) 6/21/85";
#endif /* !lint */

/*
 * Mail -- a mail program
 *
 * Generally useful tty stuff.
 */

#include "glob.h"

static	int	c_erase;		/* Current erase char */
static	int	c_kill;			/* Current kill char */
static	int	hadcont;		/* Saw continue signal */
static	jmp_buf	rewrite;		/* Place to go when continued */
#ifndef TIOCSTI
static	int	ttyset;			/* We must now do erase/kill */
#endif

/*
 * Read all relevant header fields.
 */

grabh(hp, gflags)
	struct header *hp;
{
#ifndef	USG
	struct sgttyb ttybuf;
#else
	struct termio ttybuf;
#endif
#if defined(SVR3) || defined(_SVR4_SOURCE)
	void ttycont(), signull();
#else
	int ttycont(), signull();
#endif
#ifndef TIOCSTI
	int (*savesigs[2])();
#endif
	int (*savecont)();
	register int s;
	int errs;
	int tl = gflags & GTOOLATE;

#ifdef VMUNIX
	savecont = sigset(SIGCONT, signull);
#endif /* VMUNIX */
	errs = 0;
#ifndef TIOCSTI
	ttyset = 0;
#endif
#ifndef	USG
	if (gtty(fileno(stdin), &ttybuf) < 0) {
		perror("gtty");
		return(-1);
	}
	c_erase = ttybuf.sg_erase;
	c_kill = ttybuf.sg_kill;
#else
	if (ioctl(fileno(stdin), TCGETA, &ttybuf) < 0) {
		perror("gtty");
		return(-1);
	}
	c_erase = ttybuf.c_cc[VERASE];
	c_kill = ttybuf.c_cc[VKILL];
#endif
#ifndef TIOCSTI
#ifndef	USG
	ttybuf.sg_erase = 0;
	ttybuf.sg_kill = 0;
#else
	ttybuf.c_cc[VERASE] = 0;
	ttybuf.c_cc[VKILL] = 0;
#endif
	for (s = SIGINT; s <= SIGQUIT; s++)
		if ((savesigs[s-SIGINT] = sigset(s, SIG_IGN)) == SIG_DFL)
			sigset(s, SIG_DFL);
#endif
	if (gflags & GTO) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_to != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_to = readtty("To: ", hp->h_to, tl);
		if (hp->h_to != NOSTR)
			hp->h_seq++;
	}
	if (gflags & GSUBJECT) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_subject != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_subject = readtty("Subject: ", hp->h_subject, tl);
		if (hp->h_subject != NOSTR)
			hp->h_seq++;
	}
	if (gflags & GCC) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_cc != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_cc = readtty("Cc: ", hp->h_cc, tl);
		if (hp->h_cc != NOSTR)
			hp->h_seq++;
	}
	if (gflags & GBCC) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_bcc != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_bcc = readtty("Bcc: ", hp->h_bcc, tl);
		if (hp->h_bcc != NOSTR)
			hp->h_seq++;
	}
	if (gflags & GRT) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_replyto != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_replyto = readtty("Reply-To: ", hp->h_replyto, tl);
		if (hp->h_replyto != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GRT;
	}
	if (gflags & GRR) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_rtnrcpt != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_rtnrcpt = readtty("Return-Receipt-To: ", hp->h_rtnrcpt, tl);
		if (hp->h_rtnrcpt != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GRR;
	}
	if (gflags & GIRT) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_inreplyto != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_inreplyto = readtty("In-Reply-To: ", hp->h_inreplyto, tl);
		if (hp->h_inreplyto != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GIRT;
	}

	if (gflags & GREF) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_references != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_references = readtty("References: ", hp->h_references, tl);
		if (hp->h_references != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GREF;
	}
	if (gflags & GKEY) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_keywords != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_keywords = readtty("Keywords: ", hp->h_keywords, tl);
		if (hp->h_keywords != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GKEY;
	}
	if (gflags & GCOM) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_comments != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_comments = readtty("Comments: ", hp->h_comments, tl);
		if (hp->h_comments != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GCOM;
	}
	if (gflags & GEN) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_encrypt != NOSTR) {
			ttyset++;
#ifndef	USG
			stty(fileno(stdin), &ttybuf);
#else
			ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
		}
#endif
		hp->h_encrypt = readtty("Encrypted: ", hp->h_encrypt, tl);
		if (hp->h_encrypt != NOSTR)
			hp->h_seq++;
		else
			hp->h_optusedmask &= ~GEN;
	}
#ifdef VMUNIX
	sigset(SIGCONT, savecont);
#endif /* VMUNIX */
#ifndef TIOCSTI
#ifndef	USG
	ttybuf.sg_erase = c_erase;
	ttybuf.sg_kill = c_kill;
	if (ttyset)
		stty(fileno(stdin), &ttybuf);
#else
	ttybuf.c_cc[VERASE] = c_erase;
	ttybuf.c_cc[VKILL] = c_kill;
	if (ttyset)
		ioctl(fileno(stdin), TCSETAW, &ttybuf);
#endif
	for (s = SIGINT; s <= SIGQUIT; s++)
		sigset(s, savesigs[s-SIGINT]);
#endif
	return(errs);
}

/*
 * Read up a header from standard input.
 * The source string has the preliminary contents to
 * be read.
 *
 */

char *
readtty(pr, src, toolate)
	char pr[], src[];
	int toolate;
{
	char ch, canonb[MAX_CANON];
#if defined(SVR3) || defined(_SVR4_SOURCE)
	int c;
#else
	int c, signull();
#endif
	register char *cp, *cp2;

	fputs(pr, stdout);
	fflush(stdout);
	if (src != NOSTR && strlen(src) > MAX_CANON - 2) {
		printf("[Line is too long to edit.  %s]\n",
			toolate ? "Sorry." : "Use ~eh or ~vh.");
		return(src);
	}
#ifndef TIOCSTI
	if (src != NOSTR)
		cp = copy(src, canonb, MAX_CANON);
	else
		cp = copy("", canonb, MAX_CANON);
	fputs(canonb, stdout);
	fflush(stdout);
#else
	cp = src == NOSTR ? "" : src;
	while (c = *cp++) {
		if (c == c_erase || c == c_kill) {
			ch = '\\';
			ioctl(0, TIOCSTI, &ch);
		}
		ch = c;
		ioctl(0, TIOCSTI, &ch);
	}
	cp = canonb;
	*cp = 0;
#endif
	cp2 = cp;
	while (cp2 < canonb + MAX_CANON)
		*cp2++ = 0;
	cp2 = cp;
	if (setjmp(rewrite))
		goto redo;
#ifdef VMUNIX
	sigset(SIGCONT, ttycont);
#endif
	clearerr(stdin);
	while (cp2 < canonb + MAX_CANON) {
		c = getc(stdin);
		if (c == EOF || c == '\n')
			break;
		*cp2++ = c;
	}
	*cp2 = 0;
#ifdef VMUNIX
	sigset(SIGCONT, signull);
#endif 
	if (c == EOF && ferror(stdin) && hadcont) {
redo:
		hadcont = 0;
		cp = strlen(canonb) > 0 ? canonb : NOSTR;
		clearerr(stdin);
		return(readtty(pr, cp, toolate));
	}
#ifndef TIOCSTI
	if (cp == NOSTR || *cp == '\0')
		return(src);
	cp2 = cp;
	if (!ttyset)
		return(strlen(canonb) > 0 ? savestr(canonb) : NOSTR);
	while (*cp != '\0') {
		c = *cp++;
		if (c == c_erase) {
			if (cp2 == canonb)
				continue;
			if (cp2[-1] == '\\') {
				cp2[-1] = c;
				continue;
			}
			cp2--;
			continue;
		}
		if (c == c_kill) {
			if (cp2 == canonb)
				continue;
			if (cp2[-1] == '\\') {
				cp2[-1] = c;
				continue;
			}
			cp2 = canonb;
			continue;
		}
		*cp2++ = c;
	}
	*cp2 = '\0';
#endif
	if (equal("", canonb))
		return(NOSTR);
	return(savestr(canonb));
}

#ifdef VMUNIX
/*
 * Receipt continuation.
 */
#if defined(SVR3) || defined(_SVR4_SOURCE)
void
#endif
ttycont(s)
{

	hadcont++;
	longjmp(rewrite, 1);
}
#endif /* VMUNIX */

/*
 * Null routine to satisfy
 * silly system bug that denies us holding SIGCONT
 */
#if defined(SVR3) || defined(_SVR4_SOURCE)
void
#endif
signull()
{
}
