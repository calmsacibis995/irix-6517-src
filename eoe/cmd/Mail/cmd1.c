/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)cmd1.c	5.3 (Berkeley) 9/15/85";
#endif /* not lint */

#include "glob.h"
#include <ctype.h>
#include <termio.h>
#include <sys/stat.h>

/*
 * Mail -- a mail program
 *
 * User commands.
 */

/*
 * Print the current active headings.
 * Don't change dot if invoker didn't give an argument.
 */

static int firstmsg;
static int lastmsg;
static int lastlength;

headers(msgvec)
	int *msgvec;
{
	register int n, mesg, flag;
	register struct message *mp;
	int length, width;

	length = getwinlines(0) - 1;
	width = getwincols();
	n = *msgvec;
	if (inithdr) {
		lastlength = 0;
		lastmsg = 0;
		/*
		 * Allow for greeting lines on initial screenful
       		 * of headers.
	         */
		length -= 2;
	}
	if (length >= msgCount)
		firstmsg = 1;
	else if (length > 1 && inithdr) {
		if (n < length)
			firstmsg = 1;
		else
			if (msgCount - n >= length)
				firstmsg = n;
			else
				firstmsg = msgCount - length + 1;
	}
	else if (n < firstmsg
		 || (length != lastlength && n - firstmsg > length - 1)
                 || n > lastmsg) {
		if (n != 0)
			firstmsg = n;
		if (firstmsg <= 0)
			firstmsg = 1;
	}
	mp = &message[firstmsg - 1];
	flag = 0;
	mesg = (mp - &message[0]) + 1;
	if (dot != &message[n-1])
		dot = mp;
	for (; mp < &message[msgCount]; mp++, mesg++) {
		if (mp->m_flag & MDELETED)
			continue;
		if (flag++ >= length) 
			break;
		printhead(mesg, width);
		sreset();
	}
	lastmsg = mesg - 1;
	if (flag == 0) {
		printf("No more mail.\n");
		return(1);
	}
	lastlength = length;
	return(0);
}

/*
 * Print headers of all messages matching flags.
 */
flgdheaders(short matchflags)
{
	int mesg, width;
	register struct message *mp;

	width = getwincols();

	for (mp = &message[0], mesg = 1;
	     mp < &message[msgCount];
	     mp++, mesg++) {
		if (!((mp->m_flag & matchflags) == matchflags))
			continue;
		printhead(mesg, width);
		sreset();
	}
}

/*
 * Print headers of all deleted messages.
 */
prtdeleted()
{
	flgdheaders(MDELETED);
}


/*
 * Set the list of alternate names for out host.
 */
local(namelist)
	char **namelist;
{
	register int c;
	register char **ap, **ap2, *cp;

	c = argcount(namelist) + 1;
	if (c == 1) {
		if (localnames == 0)
			return(0);
		for (ap = localnames; *ap; ap++)
			printf("%s ", *ap);
		printf("\n");
		return(0);
	}
	if (localnames != 0)
		cfree((char *) localnames);
	localnames = (char **) calloc(c, sizeof (char *));
	for (ap = namelist, ap2 = localnames; *ap; ap++, ap2++) {
		cp = (char *) calloc(strlen(*ap) + 1, sizeof (char));
		strcpy(cp, *ap);
		*ap2 = cp;
	}
	*ap2 = 0;
	return(0);
}

/*
 * Scroll to the next/previous screen
 */

scroll(arg)
	char arg[];
{
	int cur[1], length;

	cur[0] = 0;
	length = getwinlines(0) - 1;
	switch (*arg) {
	case 0:
	case '+':
		if (lastmsg >= msgCount) {
			printf("On last screenful of messages\n");
			return(0);
		}
		firstmsg = lastmsg + 1;
		break;

	case '-':
		if (firstmsg <= 1) {
			printf("On first screenful of messages\n");
			return(0);
		}
		firstmsg = firstmsg - length;
		if (firstmsg <= 0)
			firstmsg = 1;
		break;

	default:
		printf("Unrecognized scrolling command \"%s\"\n", arg);
		return(1);
	}
	return(headers(cur));
}

/*
 * Print out the headlines for each message
 * in the passed message list.
 */

from(msgvec)
	int *msgvec;
{
	register int *ip;
	int width;

	width = getwincols();
	if (*msgvec != NULL)
		dot = &message[*msgvec - 1];
	for (ip = msgvec; *ip != NULL; ip++) {
		printhead(*ip, width);
		sreset();
	}
	return(0);
}

/*
 * Print out the header of a specific message.
 * This is a slight improvement to the standard one.
 */

printhead(mesg, width)
{
	struct message *mp;
	FILE *ibuf;
	char headline[LINESIZE], wcount[LINESIZE], *subjline, dispc, curind;
	char *toline, *fromline;
	char tidyline[LINESIZE];
	char obuf[LINESIZE];
	char pbuf[LINESIZE];
	int s, showto;
	struct headline hl;

	mp = &message[mesg-1];
	ibuf = setinput(mp);
	readline(ibuf, headline);
	subjline = hfield("subject", mp);
	if (subjline == NOSTR)
		subjline = hfield("subj", mp);

	curind = dot == mp ? '>' : ' ';
	dispc = ' ';
	showto = 0;
	if (mp->m_flag & MSAVED)
		dispc = '*';
	if (mp->m_flag & MDELETED)
		dispc = 'D';
	if (mp->m_flag & MPRESERVE)
		dispc = 'P';
	if ((mp->m_flag & (MREAD|MNEW)) == MNEW)
		dispc = 'N';
	if ((mp->m_flag & (MREAD|MNEW)) == 0)
		dispc = 'U';
	if (mp->m_flag & MBOX)
		dispc = 'M';
	if (!parse(headline, &hl, pbuf)) {
		snprintf(obuf, LINESIZE, "%c%c%3d Unparsable message headers!",
			curind, dispc, mesg);
		obuf[width-1] = '\0';
		printf("%s\n", obuf);
		return;
	}

	fromline = nameof(mp, 0);
	if (fromline)
		tidyaddr(fromline, tidyline);
	else
		tidyline[0] = '\0';
	toline = hfield("to", mp);
	if (toline && value("showto")) {
		if (strcmp(tidyline, myname) == 0) {
			showto = 1;
			tidyaddr(toline, tidyline);
		}
	}
	if (value("showmsize"))
		snprintf(wcount, LINESIZE, "%3d/%-5ld ", mp->m_lines, mp->m_size);
	else {
		wcount[0] = ' ';
		wcount[1] = '\0';
	}
	if (showto) {
		if (subjline != NOSTR) {
			snprintf(obuf, LINESIZE, "%c%c%3d To %-15.15s  %-16.16s %s%s", 
				curind, dispc, mesg, tidyline, hl.l_date,
				wcount, subjline);
		} else {
			snprintf(obuf, LINESIZE, "%c%c%3d To %-15.15s  %-16.16s %s",
				curind, dispc, mesg, tidyline, hl.l_date,
				wcount);
		}
	}
	else {
		if (subjline != NOSTR) {
			snprintf(obuf, LINESIZE, "%c%c%3d %-18.18s  %-16.16s %s%s", 
				curind, dispc, mesg, tidyline, hl.l_date,
				wcount, subjline);
		} else {
			snprintf(obuf, LINESIZE, "%c%c%3d %-18.18s  %-16.16s %s",
				curind, dispc, mesg, tidyline, hl.l_date,
				wcount);
		}
	}
	obuf[width-1] = '\0';
	printf("%s\n", obuf);
}

/*
 * Print out the value of dot.
 */

pdot()
{
	printf("%d\n", dot - &message[0] + 1);
	return(0);
}

/*
 * Print out all the possible commands.
 */

pcmdlist()
{
	register struct cmd *cp;
	register int cc;
	extern struct cmd cmdtab[];
	int ncol;

	ncol = getwincols();
	printf("Commands are:\n");
	for (cc = 0, cp = cmdtab; cp->c_name != NULL; cp++) {
		cc += strlen(cp->c_name) + 2;
		if (cc > ncol) {
			printf("\n");
			cc = strlen(cp->c_name) + 2;
		}
		if ((cp+1)->c_name != NOSTR)
			printf("%s, ", cp->c_name);
		else
			printf("%s\n", cp->c_name);
	}
	return(0);
}

/*
 * Paginate messages, honor ignored fields.
 */
more(msgvec)
	int *msgvec;
{
	return (type1(msgvec, SUPPRESS_IGN, 1));
}

/*
 * Paginate messages, even printing ignored fields.
 */
More(msgvec)
	int *msgvec;
{

	return (type1(msgvec, NO_SUPPRESS, 1));
}

/*
 * Type out messages, honor ignored fields.
 */
type(msgvec)
	int *msgvec;
{

	return(type1(msgvec, SUPPRESS_IGN, 0));
}

/*
 * Type out messages, even printing ignored fields.
 */
Type(msgvec)
	int *msgvec;
{

	return(type1(msgvec, NO_SUPPRESS, 0));
}

/*
 * Type out the messages requested.
 */
jmp_buf	pipestop;

type1(msgvec, suppress, page)
	int *msgvec;
{
	register *ip;
	register struct message *mp;
	register int mesg;
	register char *cp;
	u_int nmlines;
	void brokpipe();
	FILE *ibuf, *obuf;

	obuf = stdout;
	if (setjmp(pipestop)) {
		if (obuf != stdout) {
			pipef = NULL;
			pclose(obuf);
		}
		sigset(SIGPIPE, SIG_DFL);
		return(0);
	}
	if (intty && outtty) {
		nmlines = 0;
		for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++)
			if (suppress == NO_SUPPRESS)
				nmlines += message[*ip - 1].m_lines;
			else
				nmlines += message[*ip - 1].m_dlines;

		/* XXXrs the hardcoded 3 below is a fudge factor
		 * to account for the "Message:" line, the newline before the
		 * prompt line and the prompt line itself that will be
		 * printed if the pager is not invoked.
		 */
		if (getwinlines(page) < nmlines+3) {
			cp = value("PAGER");
			if (cp == NULL || *cp == '\0')
				cp = MORE;
			obuf = popen(cp, "w");
			if (obuf == NULL) {
				perror(cp);
				obuf = stdout;
			}
			else {
				pipef = obuf;
				sigset(SIGPIPE, brokpipe);
			}
		}
	}
	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		dot = mp;
		print(mp, obuf, suppress);
	}
	if (obuf != stdout) {
		pipef = NULL;
		pclose(obuf);
	}
	sigset(SIGPIPE, SIG_DFL);
	return(0);
}

/*
 * Respond to a broken pipe signal --
 * probably caused by using quitting more.
 */

void
brokpipe()
{
# ifndef VMUNIX
	signal(SIGPIPE, brokpipe);
# endif
	longjmp(pipestop, 1);
}

/*
 * Print the indicated message on standard output.
 */

print(mp, obuf, suppress)
	register struct message *mp;
	FILE *obuf;
{
	int lcnt;
	long ccnt;

	if (value("quiet") == NOSTR)
		fprintf(obuf, "Message %2d:\n", mp - &message[0] + 1);
	touch(mp - &message[0] + 1);
	send_msg(mp, obuf, suppress, &lcnt, &ccnt);
}

/*
 * Print the top so many lines of each desired message.
 * The number of lines is taken from the variable "toplines"
 * and defaults to 5.
 */

top(msgvec)
	int *msgvec;
{
	register int *ip;
	register struct message *mp;
	register int mesg;
	u_int c, topl, lines, lineb;
	char *valtop, linebuf[LINESIZE];
	FILE *ibuf;

	topl = 5;
	valtop = value("toplines");
	if (valtop != NOSTR) {
		topl = atoi(valtop);
		if (topl > 10000)
			topl = 5;
	}
	lineb = 1;
	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		dot = mp;
		if (value("quiet") == NOSTR)
			printf("Message %2d:\n", mesg);
		ibuf = setinput(mp);
		c = mp->m_lines;
		if (!lineb)
			printf("\n");
		for (lines = 0; lines < c && lines <= topl; lines++) {
			if (readline(ibuf, linebuf) <= 0)
				break;
			puts(linebuf);
			lineb = blankline(linebuf);
		}
	}
	return(0);
}

/*
 * Touch all the given messages so that they will
 * get mboxed.
 */

stouch(msgvec)
	int msgvec[];
{
	register int *ip;

	for (ip = msgvec; *ip != 0; ip++) {
		dot = &message[*ip-1];
		dot->m_flag |= MTOUCH;
		dot->m_flag &= ~MPRESERVE;
	}
	return(0);
}

/*
 * Make sure all passed messages get mboxed.
 */

mboxit(msgvec)
	int msgvec[];
{
	register int *ip;

	for (ip = msgvec; *ip != 0; ip++) {
		dot = &message[*ip-1];
		dot->m_flag |= MTOUCH|MBOX;
		dot->m_flag &= ~MPRESERVE;
	}
	return(0);
}

/*
 * List the folders the user currently has.
 */
folders()
{
	char dirname[BUFSIZ], cmd[BUFSIZ];
	int pid, s, e;

	if (getfold(dirname, BUFSIZ) < 0) {
		printf("No value set for \"folder\"\n");
		return(-1);
	}
	switch ((pid = fork())) {
	case 0:
		sigchild();
		execlp("ls", "ls", dirname, 0);
		_exit(1);

	case -1:
		perror("fork");
		return(-1);

	default:
		while ((e = wait(&s)) != -1 && e != pid)
			;
	}
	return(0);
}
