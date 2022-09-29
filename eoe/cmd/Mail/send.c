/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)send.c	5.2 (Berkeley) 6/21/85";
#endif /* lint */

#include "glob.h"
#ifdef VMUNIX
#include <sys/wait.h>
#endif
#include <ctype.h>
#include <sys/stat.h>
#include <termio.h>

/*
 * Mail -- a mail program
 *
 * Mail to others.
 */

/*
 * Send message described by the passed pointer to the
 * passed output buffer.  Return -1 on error, but normally
 * the number of lines written.  Adjust the status: field
 * if need be.  If suppress ignored header fields as indicated
 * by suppress value.
 */
send_msg(mailp, obuf, suppress, lcnt, ccnt)
	struct message *mailp;
	FILE *obuf;
	int suppress, *lcnt;
	long *ccnt;
{
	register struct message *mp;
	register int t;
	u_long c;
	FILE *ibuf;
	char line[LINESIZE], field[BUFSIZ];
	int cc, lc, ishead, infld, fline, dostat;
	char *cp, *cp2;

	mp = mailp;
	ibuf = setinput(mp);
	c = mp->m_size;
	ishead = 1;
	dostat = 1;
	infld = 0;
	fline = 1;
	lc = 0;
	*lcnt = 0;
	*ccnt = 0L;
	while (c > 0L) {
		fgets(line, LINESIZE, ibuf);
		cc = strlen(line);
		c -= (long) cc;
		lc++;
		*ccnt += cc;
		if (ishead) {
			/* 
			 * First line is the From line, so no headers
			 * there to worry about. 
			 */
			if (fline) {
				fline = 0;
				if (suppress == SUPPRESS_ALL) {
					lc--;
					*ccnt -= cc;
					continue;
				}
				goto writeit;
			}
			/*
			 * If line is blank, we've reached end of
			 * headers, so force out status: field
			 * and note that we are no longer in header
			 * fields
			 */
			if (line[0] == '\n') {
				if (dostat) {
					statusput(mailp, obuf, suppress);
					dostat = 0;
				}
				ishead = 0;
				goto writeit;
			}
			/*
			 * If this line is a continuation (via space or tab)
			 * of a previous header field, just echo it
			 * (unless the field should be ignored).
			 */
			if (infld && (isspace(line[0]) || line[0] == '\t')) {
				if (suppress == SUPPRESS_ALL ||
				    (suppress == SUPPRESS_IGN 
                                     && isign(field))) {
					lc--;
					*ccnt -= cc;
					continue;
				}
				goto writeit;
			}
			infld = 0;
			/*
			 * If we are no longer looking at real
			 * header lines, force out status:
			 * This happens in uucp style mail where
			 * there are no headers at all.
			 */
			if (!headerp(line)) {
				if (dostat) {
					statusput(mailp, obuf, suppress);
					dostat = 0;
				}
				putc('\n', obuf);
				ishead = 0;
				goto writeit;
			}
			infld++;
			if (suppress == SUPPRESS_ALL) {
				lc--;
				*ccnt -= cc;
				continue;
			}
			/*
			 * Pick up the header field.
			 * If it is an ignored field and
			 * we care about such things, skip it.
			 */
			cp = line;
			cp2 = field;
			while (*cp && *cp != ':' && !isspace(*cp))
				*cp2++ = *cp++;
			*cp2 = 0;
			if (suppress == SUPPRESS_IGN && isign(field)) {
				lc--;
				*ccnt -= cc;
				continue;
			}
			/*
			 * If the field is "status," go compute and print the
			 * real Status: field
			 */
			if (icequal(field, "status")) {
				if (dostat) {
					statusput(mailp, obuf, suppress);
					dostat = 0;
				}
				if (suppress == SUPPRESS_ALL ||
				    (suppress == SUPPRESS_IGN &&
				     isign(field))) {
					lc--; 
					*ccnt -= cc;
				}
				continue;
			}
		}
writeit:
		fputs(line, obuf);
		if (ferror(obuf))
			return(-1);
	}
	if (ferror(obuf))
		return(-1);
	if (ishead && (mailp->m_flag & MSTATUS))
		printf("failed to fix up status field\n");
	*lcnt = lc;
	return(lc);
}

/*
 * Test if the passed line is a header line, RFC 733 style.
 */
headerp(line)
	register char *line;
{
	register char *cp = line;

	while (*cp && !isspace(*cp) && *cp != ':')
		cp++;
	while (*cp && isspace(*cp))
		cp++;
	return(*cp == ':');
}

/*
 * Output a reasonable looking status field.
 * If suppressing all headers, forget it.
 * If suppressing ignored headers and "status" is
 * ignored, forget it as well.
 */
statusput(mp, obuf, suppress)
	register struct message *mp;
	register FILE *obuf;
{
	char statout[3];

	if (suppress == SUPPRESS_ALL ||
            (suppress == SUPPRESS_IGN && isign("status")))
		return;
	if ((mp->m_flag & (MNEW|MREAD)) == MNEW)
		return;
	if (mp->m_flag & MREAD)
		strcpy(statout, "R");
	else
		strcpy(statout, "");
	if ((mp->m_flag & MNEW) == 0)
		strcat(statout, "O");
	fprintf(obuf, "Status: %s\n", statout);
}


/*
 * Interface between the argument list and the mail1 routine
 * which does all the dirty work.
 */

mail(people)
	char **people;
{
	register char *cp2;
	register int s;
	char *buf, **ap;
	struct header head;

	for (s = 0, ap = people; *ap != (char *) -1; ap++)
		s += strlen(*ap) + 2;
	buf = salloc(s+1);
	*(buf + s) = '\0';
	*buf = '\0';
	cp2 = buf;
	for (ap = people; *ap != (char *) -1; ap++) {
		cp2 = copy(*ap, cp2, (s+1) - strlen(buf));
		cp2 = copy(", ", cp2, (s+1) - strlen(buf));
	}
	while (s = strlen(buf)) {
		if (isspace(buf[s-1]) || buf[s-1] == ',')
			buf[s-1] = '\0';
		else
			break;
	}
	initheader(&head);
	head.h_to = buf;
	if ((cp2 = value("replyto")) != NOSTR) {
		head.h_replyto = savestr(cp2);
		head.h_optusedmask |= GRT;
	}
	mail1(&head);
	return(0);
}


/*
 * Send mail to a bunch of user names.  The interface is through
 * the mail routine below.
 */

sendmail(str)
	char *str;
{
	register char **ap;
	char *bufp, *cp;
	register int t;
	struct header head;

	initheader(&head);
	if (!blankline(str))
		head.h_to = str;
	if ((cp = value("replyto")) != NOSTR) {
		head.h_replyto = savestr(cp);
		head.h_optusedmask |= GRT;
	}
	mail1(&head);
	return(0);
}

/*
 * Mail a message on standard input to the people indicated
 * in the passed header.  (Internal interface).
 */

mail1(hp)
	struct header *hp;
{
	register char *cp;
	int pid, i, s, p, gotcha;
	char **namelist, *deliver;
	struct name *to, *np;
	struct stat sbuf;
	FILE *mtf, *postage;
	int remote = rflag != NOSTR || rmail;
	char **t;

	/*
	 * Collect user's mail from standard input.
	 * Get the result as mtf.
	 */

	pid = -1;
	if ((mtf = collect(hp)) == NULL)
		return(-1);
	hp->h_seq = 1;
	if (hp->h_subject == NOSTR)
		hp->h_subject = sflag;
	if (intty && value("askcc") != NOSTR)
		grabh(hp, GCC|GTOOLATE);
	else if (intty) {
		printf("EOT\n");
		fflush(stdout);
	}

	/*
	 * Now, take the user names from the combined
	 * to and cc lists and do all the alias
	 * processing.
	 */

	senderr = 0;
	to = usermap(cat(extract(hp->h_bcc, GBCC),
	    cat(extract(hp->h_to, GTO), extract(hp->h_cc, GCC))));
	if (to == NIL) {
		printf("No recipients specified\n");
		goto topdog;
	}

	/*
	 * Look through the recipient list for names with /'s
	 * in them which we write to as files directly.
	 */

	to = outof(to, mtf, hp);
	rewind(mtf);
	to = verify(to);
	if (senderr && !remote) {
topdog:

		if (fsize(mtf) != 0) {
			m_remove(deadletter);
			exwrite(deadletter, mtf, 1);
			rewind(mtf);
		}
	}
	for (gotcha = 0, np = to; np != NIL; np = np->n_flink)
		if ((np->n_type & GDEL) == 0) {
			gotcha++;
			break;
		}
	if (!gotcha)
		goto out;
	if (count(to) > 1)
		hp->h_seq++;
	if (hp->h_seq > 0 && !remote) {
		fixhead(hp, to);
		if (fsize(mtf) == 0)
		    if (hp->h_subject == NOSTR)
			printf("No message, no subject; hope that's ok\n");
		    else
			printf("Null message body; hope that's ok\n");
		if ((mtf = infix(hp, mtf)) == NULL) {
			fprintf(stderr, ". . . message lost, sorry.\n");
			return(-1);
		}
	}
	namelist = unpack(to);
	if (debug) {
		printf("Recipients of message:\n");
		for (t = namelist; *t != NOSTR; t++)
			printf(" \"%s\"", *t);
		printf("\n");
		fflush(stdout);
		return;
	}
	if ((cp = value("record")) != NOSTR)
		savemail(expand(cp), hp, mtf);

	/*
	 * Wait, to absorb a potential zombie, then
	 * fork, set up the temporary mail file as standard
	 * input for "mail" and exec with the user list we generated
	 * far above. Return the process id to caller in case he
	 * wants to await the completion of mail.
	 */

#ifdef VMUNIX
#ifdef	pdp11
	while (wait2(&s, WNOHANG) > 0)
#endif
#if defined(vax) || defined(sun)
	while (wait3(&s, WNOHANG, 0) > 0)
#endif
		;
#else
	wait(&s);
#endif
	rewind(mtf);
	pid = fork();
	if (pid == -1) {
		perror("fork");
		m_remove(deadletter);
		exwrite(deadletter, mtf, 1);
		goto out;
	}
	if (pid == 0) {
		sigchild();
#ifdef SIGTSTP
		if (remote == 0) {
			sigset(SIGTSTP, SIG_IGN);
			sigset(SIGTTIN, SIG_IGN);
			sigset(SIGTTOU, SIG_IGN);
		}
#endif
		for (i = SIGHUP; i <= SIGQUIT; i++)
			sigset(i, SIG_IGN);
		if (!stat(POSTAGE, &sbuf))
			if ((postage = fopen(POSTAGE, "a")) != NULL) {
				fprintf(postage, "%s %d %lld\n", myname,
				    count(to), fsize(mtf));
				fclose(postage);
			}
		s = fileno(mtf);
		for (i = 3; i < 15; i++)
			if (i != s)
				close(i);
		close(0);
		dup(s);
		close(s);
#ifdef CC
		submit(getpid());
#endif /* CC */
#ifdef SENDMAIL
		if ((deliver = value("sendmail")) == NOSTR)
			deliver = SENDMAIL;
		execv(deliver, namelist);
#endif /* SENDMAIL */
		execv(MAIL, namelist);
		perror(MAIL);
		safe_exit(1);
	}

out:
	if (remote || (value("verbose") != NOSTR)) {
		while ((p = wait(&s)) != pid && p != -1)
			;
		if (s != 0)
			senderr++;
		pid = 0;
	}
	fclose(mtf);
	return(pid);
}

/*
 * Fix the header by glopping all of the expanded names from
 * the distribution list into the appropriate fields.
 * If there are any ARPA net recipients in the message,
 * we must insert commas, alas.
 */

fixhead(hp, tolist)
	struct header *hp;
	struct name *tolist;
{
	register struct name *nlist;
	register int f;
	register struct name *np;

	for (f = 0, np = tolist; np != NIL; np = np->n_flink)
		if (any('@', np->n_name)) {
			f |= GCOMMA;
			break;
		}

	if (debug && f & GCOMMA)
		fprintf(stderr, "Should be inserting commas in recip lists\n");
	hp->h_to = detract(tolist, GTO|f);
	hp->h_cc = detract(tolist, GCC|f);
}

/*
 * Prepend a header in front of the collected stuff
 * and return the new file.
 */

FILE *
infix(hp, fi)
	struct header *hp;
	FILE *fi;
{
	extern char tempMail[];
	register FILE *nfo, *nfi;
	register int c;

	rewind(fi);
	if ((nfo = fopen(tempMail, "w")) == NULL) {
		perror(tempMail);
		return(fi);
	}
	if ((nfi = fopen(tempMail, "r")) == NULL) {
		perror(tempMail);
		fclose(nfo);
		return(fi);
	}
	m_remove(tempMail);
	puthead(hp, nfo, (GALLHDR|GNL)&~GBCC);
	c = getc(fi);
	while (c != EOF) {
		putc(c, nfo);
		c = getc(fi);
	}
	if (ferror(fi)) {
		perror("read");
		return(fi);
	}
	fflush(nfo);
	if (ferror(nfo)) {
		perror(tempMail);
		fclose(nfo);
		fclose(nfi);
		return(fi);
	}
	fclose(nfo);
	fclose(fi);
	rewind(nfi);
	return(nfi);
}

/*
 * Save the outgoing mail on the passed file.
 */

savemail(name, hp, fi)
	char name[];
	struct header *hp;
	FILE *fi;
{
	register FILE *fo;
	register int c;
	long now;
	char *n;

	if ((fo = fopen(name, "a")) == NULL) {
		perror(name);
		return(-1);
	}
	time(&now);
	n = rflag;
	if (n == NOSTR)
		n = myname;
	fprintf(fo, "From %s %s", n, ctime(&now));
	rewind(fi);
	for (c = getc(fi); c != EOF; c = getc(fi))
		putc(c, fo);
	fprintf(fo, "\n");
	fflush(fo);
	if (ferror(fo))
		perror(name);
	fclose(fo);
	return(0);
}
