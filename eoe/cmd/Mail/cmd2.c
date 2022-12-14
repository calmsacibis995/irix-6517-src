/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)cmd2.c	5.3 (Berkeley) 9/10/85";
#endif /* not lint */

#include "glob.h"
#include <sys/stat.h>
#include <ctype.h>

/*
 * Mail -- a mail program
 *
 * More user commands.
 */

/*
 * If any arguments were given, go to the next applicable argument
 * following dot, otherwise, go to the next applicable message.
 * If given as first command with no arguments, print first message.
 */

next(msgvec)
	int *msgvec;
{
	register struct message *mp;
	register int *ip, *ip2;
	int list[2], mdot;

	if (*msgvec != NULL) {

		/*
		 * If some messages were supplied, find the 
		 * first applicable one following dot using
		 * wrap around.
		 */

		mdot = dot - &message[0] + 1;

		/*
		 * Find the first message in the supplied
		 * message list which follows dot.
		 */

		for (ip = msgvec; *ip != NULL; ip++)
			if (*ip > mdot)
				break;
		if (*ip == NULL)
			ip = msgvec;
		ip2 = ip;
		do {
			mp = &message[*ip2 - 1];
			if ((mp->m_flag & MDELETED) == 0) {
				dot = mp;
				goto hitit;
			}
			if (*ip2 != NULL)
				ip2++;
			if (*ip2 == NULL)
				ip2 = msgvec;
		} while (ip2 != ip);
		printf("No messages applicable\n");
		return(1);
	}

	/*
	 * If this is the first command, select message 1.
	 * Note that this must exist for us to get here at all.
	 */

	if (!sawcom)
		goto hitit;

	/*
	 * Just find the next good message after dot, no
	 * wraparound.
	 */

	for (mp = dot+1; mp < &message[msgCount]; mp++)
		if ((mp->m_flag & (MDELETED)) == 0)
			break;
	if (mp >= &message[msgCount]) {
		printf("At EOF\n");
		return(0);
	}
	dot = mp;
hitit:
	/*
	 * Print dot.
	 */

	list[0] = dot - &message[0] + 1;
	list[1] = NULL;
	return(type(list));
}

/*
 * Save a message in a file.  Mark the message as saved
 * so we can discard when the user quits.
 */
save_msg(str)
	char str[];
{

	return(save1(str, 1, NO_SUPPRESS));
}

/*
 * Copy a message to a file without affected its saved-ness
 */
copycmd(str)
	char str[];
{

	return(save1(str, 0, NO_SUPPRESS));
}

/*
 * Save/copy the indicated messages at the end of the passed file name.
 * If mark is true, mark the message "saved."
 */
save1(str, mark, suppress)
	char str[];
	int mark, suppress;
{
	register int *ip, mesg;
	register struct message *mp;
	char *file, *disp, *cmd;
	int f, *msgvec, lc, lcnt, t;
	long cc, ccnt;
	FILE *obuf;
	struct stat statb;

	/*
	 * Infer command from arguments (KLUDGE)
	 */
	if (suppress == NO_SUPPRESS)
		cmd = mark ? "save" : "copy";
	else
		cmd = "write";
	msgvec = (int *) salloc((msgCount + 2) * sizeof *msgvec);
	if ((file = snarf(str, &f)) == NOSTR)
		return(1);
	if (!f) {
		*msgvec = first(0, MMNORM);
		if (*msgvec == NULL) {
			printf("No messages to %s.\n", cmd);
			return(1);
		}
		msgvec[1] = NULL;
	}
	if (f && getmsglist(str, msgvec, 0) < 0)
		return(1);
	if ((file = expand(file)) == NOSTR)
		return(1);
	printf("\"%s\" ", file);
	fflush(stdout);
	if (file[0] == '|' || file[0] == '!') {
		mark = 0;
		disp = "[Piped]";
		if (getuid() != geteuid() || getgid() != getegid()) {
			fprintf(stderr, "Can't popen from Set-UID programs\n");
			return(1);
		}
		if ((obuf = popen(file+1, "w")) == NULL) {
			perror("");
			return(1);
		}
		sigset(SIGPIPE,SIG_IGN);
	} else {
		if (stat(file, &statb) >= 0)
			disp = "[Appended]";
		else
			disp = "[New file]";
		if ((obuf = fopen(file, "a")) == NULL) {
			perror("");
			return(1);
		}
	}
	cc = 0L;
	lc = 0;
	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		if ((t = send_msg(mp, obuf, suppress, &lcnt, &ccnt)) < 0) {
			perror(file);
			if (file[0] == '|' || file[0] == '!')
				pclose(obuf);
			else
				fclose(obuf);
			sigset(SIGPIPE, SIG_DFL);
			return(1);
		}
		lc += lcnt;
		cc += ccnt;
		if (mark)
			mp->m_flag |= MSAVED;
	}
	fflush(obuf);
	if (ferror(obuf))
		perror(file);
	if (file[0] == '|' || file[0] == '!')
		pclose(obuf);
	else
		fclose(obuf);
	sigset(SIGPIPE, SIG_DFL);
	printf("%s %d/%ld\n", disp, lc, cc);
	return(0);
}

/*
 * Write the indicated messages at the end of the passed
 * file name, minus header and trailing blank line.
 */

swrite(str)
	char str[];
{
	return(save1(str, 1, SUPPRESS_ALL));
}

/*
 * Snarf the file from the end of the command line and
 * return a pointer to it.  If there is no file attached,
 * just return NOSTR.  Put a null in front of the file
 * name so that the message list processing won't see it,
 * unless the file name is the only thing on the line, in
 * which case, return 0 in the reference flag variable.
 */

char *
snarf(linebuf, flag)
	char linebuf[];
	int *flag;
{
	register char *cp;

	*flag = 1;
	cp = strlen(linebuf) + linebuf - 1;

	/*
	 * Strip away trailing blanks.
	 */

	while (isspace(*cp) && cp > linebuf)
		cp--;
	*++cp = 0;

	/*
	 * Now search for the beginning of the file name or piped command.
	 */
{
	char *cp1;
	char *cp2;

	cp1 = strchr(linebuf, '|');
	cp2 = strchr(linebuf, '!');
	if (cp2 && cp2 < cp1)
		cp1 = cp2;
	if (cp1) {
		cp = cp1;
		if (cp > linebuf) {
			if (!isspace(cp[-1])) {
				printf("No space or tab before '|' or '!'.\n");
				return(NOSTR);
			}
			cp[-1] = 0;
		} else
			*flag = 0;
		return(cp);
	}
}

	/*
	 * Now search for the beginning of the file name.
	 */

	while (cp > linebuf && !isspace(*cp))
		cp--;
	if (*cp == '\0') {
		printf("No file specified.\n");
		return(NOSTR);
	}
	if (isspace(*cp))
		*cp++ = 0;
	else
		*flag = 0;
	return(cp);
}

/*
 * Delete messages.
 */

delete_msg(msgvec)
	int msgvec[];
{
	return(delm(msgvec));
}

/*
 * Delete messages, then type the new dot.
 */

deltype(msgvec)
	int msgvec[];
{
	int list[2];
	int lastdot;

	lastdot = dot - &message[0] + 1;
	if (delm(msgvec) >= 0) {
		list[0] = dot - &message[0];
		list[0]++;
		if (list[0] > lastdot) {
			touch(list[0]);
			list[1] = NULL;
			return(type(list));
		}
		printf("At EOF\n");
		return(0);
	}
	else {
		printf("No more messages\n");
		return(0);
	}
}

/*
 * Delete the indicated messages.
 * Set dot to some nice place afterwards.
 * Internal interface.
 */

delm(msgvec)
	int *msgvec;
{
	register struct message *mp;
	register *ip, mesg;
	int last;

	last = NULL;
	for (ip = msgvec; *ip != NULL; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		mp->m_flag |= MDELETED|MTOUCH;
		mp->m_flag &= ~(MPRESERVE|MBOX);
		last = mesg;
	}
	if (last != NULL) {
		dot = &message[last-1];
		last = first(0, MDELETED);
		if (last != NULL) {
			dot = &message[last-1];
			return(0);
		}
		else {
			dot = &message[0];
			return(-1);
		}
	}

	/*
	 * Following can't happen -- it keeps lint happy
	 */

	return(-1);
}

/*
 * Undelete the indicated messages.
 */

undelete_msg(msgvec)
	int *msgvec;
{
	register struct message *mp;
	register *ip, mesg;

	for (ip = msgvec; ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		if (mesg == 0)
			return;
		touch(mesg);
		mp = &message[mesg-1];
		dot = mp;
		mp->m_flag &= ~MDELETED;
	}
}

/*
 * Clobber as many bytes of stack as the user requests.
 */
clobber(argv)
	char **argv;
{
	register int times;

	if (argv[0] == 0)
		times = 1;
	else
		times = (atoi(argv[0]) + 511) / 512;
	clob1(times);
}

/*
 * Clobber the stack.
 */
clob1(n)
{
	char buf[512];
	register char *cp;

	if (n <= 0)
		return;
	for (cp = buf; cp < &buf[512]; *cp++ = 0xFF)
		;
	clob1(n - 1);
}

/*
 * Add the given header fields to the retained list.
 * If no arguments, print the current list of retained fields.
 */
retfield(list)
	char *list[];
{
	char field[BUFSIZ];
	register int h;
	register struct ignore *igp;
	char **ap;
	int i;

	if (argcount(list) == 0)
		return(retshow());
	for (ap = list; *ap != 0; ap++) {
		for(i=0; i<BUFSIZ; i++)
			field[i] = 0;
		istrcpy(field, *ap, BUFSIZ);
		field[BUFSIZ-1]='\0';

		if (member(field, retain))
			continue;

		h = hash(field);
		igp = (struct ignore *) calloc(1, sizeof (struct ignore));
		igp->i_field = calloc(strlen(field) + 1, sizeof (char));
		strcpy(igp->i_field, field);
		igp->i_link = retain[h];
		retain[h] = igp;
		nretained++;
	}
	return(0);
}

/*
 * Print out all currently retained fields.
 */
retshow()
{
	register int h, count;
	struct ignore *igp;
	char **ap, **ring;
	int igcomp();

	count = 0;
	for (h = 0; h < HSHSIZE; h++)
		for (igp = retain[h]; igp != 0; igp = igp->i_link)
			count++;
	if (count == 0) {
		printf("No fields currently being retained.\n");
		return(0);
	}
	ring = (char **) salloc((count + 1) * sizeof (char *));
	ap = ring;
	for (h = 0; h < HSHSIZE; h++)
		for (igp = retain[h]; igp != 0; igp = igp->i_link)
			*ap++ = igp->i_field;
	*ap = 0;
	qsort(ring, count, sizeof (char *), igcomp);
	for (ap = ring; *ap != 0; ap++)
		printf("%s\n", *ap);
	return(0);
}

/*
 * Add the given header fields to the ignored list.
 * If no arguments, print the current list of ignored fields.
 */
igfield(list)
	char *list[];
{
	char field[BUFSIZ];
	register int h;
	register struct ignore *igp;
	char **ap;
	int i;

	if (argcount(list) == 0)
		return(igshow());
	for (ap = list; *ap != 0; ap++) {
		if (isign(*ap))
			continue;
		for(i=0; i<BUFSIZ; i++)
			field[i] = 0;
		istrcpy(field, *ap, BUFSIZ);
		field[BUFSIZ-1]='\0';
		h = hash(field);
		igp = (struct ignore *) calloc(1, sizeof (struct ignore));
		igp->i_field = calloc(strlen(field) + 1, sizeof (char));
		strcpy(igp->i_field, field);
		igp->i_link = ignore[h];
		ignore[h] = igp;
	}
	return(0);
}

/*
 * Print out all currently ignored fields.
 */
igshow()
{
	register int h, count;
	struct ignore *igp;
	char **ap, **ring;
	int igcomp();

	count = 0;
	for (h = 0; h < HSHSIZE; h++)
		for (igp = ignore[h]; igp != 0; igp = igp->i_link)
			count++;
	if (count == 0) {
		printf("No fields currently being ignored.\n");
		return(0);
	}
	ring = (char **) salloc((count + 1) * sizeof (char *));
	ap = ring;
	for (h = 0; h < HSHSIZE; h++)
		for (igp = ignore[h]; igp != 0; igp = igp->i_link)
			*ap++ = igp->i_field;
	*ap = 0;
	qsort(ring, count, sizeof (char *), igcomp);
	for (ap = ring; *ap != 0; ap++)
		printf("%s\n", *ap);
	return(0);
}

/*
 * Compare two names for sorting ignored field list.
 */
igcomp(l, r)
	char **l, **r;
{

	return(strcmp(*l, *r));
}
