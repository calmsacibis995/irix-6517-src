/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)cmd3.c	5.3 (Berkeley) 9/15/85";
#endif /* !lint */

#include "glob.h"
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

/*
 * Mail -- a mail program
 *
 * Still more user commands.
 */

/*
 * Process a shell escape by saving signals, ignoring signals,
 * and forking a sh -c
 */

shell(str)
	char *str;
{
#if defined(SVR3) || defined(_SVR4_SOURCE)
	void (*sig[2])();
	int stat[1];
#else
	int (*sig[2])(), stat[1];
#endif
	register int t;
	char *Shell;
	char *cp;
	int i;
	char cmd[BUFSIZ];

	if (str) {
		strncpy(cmd, str, BUFSIZ);
		cmd[BUFSIZ - 1] = '\0';
		if ((value("nobang") == NOSTR) && (value("bang") != NOSTR)) {
			if (bangexp(cmd, BUFSIZ) < 0)
				return(-1);
		} else {
			if (ismailx) {
				if ((strlen(str) > 1) && cmd[0] == '!') {
					cp = (str + 1);
					t = (strlen(str) - 1);
					for (i=0; i<t; i++)
						cmd[i] = *cp++;
					cp = (&cmd[0] + t + 1);
					*cp = '\0';
				}
			}
		}
	}
	if ((Shell = value("SHELL")) == NOSTR || *Shell == '\0')
		if (ismailx)
			Shell = SHELL_XPG4;
		else
			Shell = SHELL;
	for (t = 2; t < 4; t++)
		sig[t-2] = sigset(t, SIG_IGN);
	t = vfork();
	if (t == 0) {
		sigchild();
		for (t = 2; t < 4; t++)
			if (sig[t-2] != SIG_IGN)
				sigsys(t, SIG_DFL);
		if (str)
			execl(Shell, Shell, "-c", cmd, (char *)0);
		else
			execl(Shell, Shell, (char *)0);
		perror(Shell);
		_exit(1);
	}
	while (wait(stat) != t)
		;
	if (t == -1)
		perror("fork");
	for (t = 2; t < 4; t++)
		sigset(t, sig[t-2]);
	printf("!\n");
	return(0);
}

/*
 * Fork an interactive shell.
 */

dosh(str)
	char *str;
{
#if defined(SVR3) || defined(_SVR4_SOURCE)
	void (*sig[2])();
	int stat[1];
#else
	int (*sig[2])(), stat[1];
#endif
	register int t;
	char *Shell;
	if ((Shell = value("SHELL")) == NOSTR)
		if (ismailx)
			Shell = SHELL_XPG4;
		else
			Shell = SHELL;
	for (t = 2; t < 4; t++)
		sig[t-2] = sigset(t, SIG_IGN);
	t = vfork();
	if (t == 0) {
		sigchild();
		for (t = 2; t < 4; t++)
			if (sig[t-2] != SIG_IGN)
				sigsys(t, SIG_DFL);
		execl(Shell, Shell, (char *)0);
		perror(Shell);
		_exit(1);
	}
	while (wait(stat) != t)
		;
	if (t == -1)
		perror("fork");
	for (t = 2; t < 4; t++)
		sigsys(t, sig[t-2]);
	putchar('\n');
	return(0);
}

/*
 * Expand the shell escape by expanding unescaped !'s into the
 * last issued command where possible.
 */

char	lastbang[128];

bangexp(str, size)
	char *str;
	int size;
{
	char bangbuf[BUFSIZ];
	register char *cp, *cp2;
	register int n;
	int changed = 0;

	cp = str;
	cp2 = bangbuf;
	n = BUFSIZ;
	while (*cp) {
		if (*cp == '!') {
			if (n < strlen(lastbang)) {
overf:
				printf("Command buffer overflow\n");
				return(-1);
			}
			changed++;
			strncpy(cp2, lastbang, BUFSIZ - strlen(cp2));
			cp2 += strlen(lastbang);
			n -= strlen(lastbang);
			cp++;
			continue;
		}
		if (*cp == '\\' && cp[1] == '!') {
			if (--n <= 1)
				goto overf;
			*cp2++ = '!';
			cp += 2;
			changed++;
		}
		if (--n <= 1)
			goto overf;
		*cp2++ = *cp++;
	}
	*cp2 = 0;
	if (changed) {
		printf("!%s\n", bangbuf);
		fflush(stdout);
	}
	strncpy(str, bangbuf, size);
	str[size -1] = '\0';
	strncpy(lastbang, bangbuf, 128);
	lastbang[127] = 0;
	return(0);
}

/*
 * Print out a nice help message from some file or another.
 */

help()
{
	register c;
	register FILE *f;
	char *cp;

	if (ismailx)
		cp = XHELPFILE;
	else
		cp = HELPFILE;
	if ((f = fopen(cp, "r")) == NULL) {
		perror(cp);
		return(1);
	}
	while ((c = getc(f)) != EOF)
		putchar(c);
	fclose(f);
	return(0);
}

/*
 * Change user's working directory.
 */

schdir(str)
	char *str;
{
	register char *cp;

	for (cp = str; *cp == ' '; cp++)
		;
	if (*cp == '\0')
		cp = homedir;
	else
		if ((cp = expand(cp)) == NOSTR)
			return(1);
	if (chdir(cp) < 0) {
		perror(cp);
		return(1);
	}
	return(0);
}

respond(msgvec)
	int *msgvec;
{
	if (value("Replyall") == NOSTR)
		return (_respond(msgvec));
	else
		return (_Respond(msgvec));
}

/*
 * Reply to a list of messages.  Extract each name from the
 * message header and send them off to mail1()
 */

_respond(msgvec)
	int *msgvec;
{
	struct message *mp;
	char *cp, *from, *replyto;
	char buf[2 * LINESIZE], **ap;
	struct name *np;
	struct header head;
	int needcomma = 0;

	if (msgvec[1] != 0) {
		printf("Sorry, can't reply to multiple messages at once\n");
		return(1);
	}
	mp = &message[msgvec[0] - 1];
	dot = mp;

	cp = nameof(mp, 1);
	np = extract(cp, 0);
	from = detract(np, 0);

	replyto = hfield("reply-to", mp);
	np = extract(replyto, 0);
	replyto = detract(np, 0);

	cp = hfield("to", mp);
	if (cp != NOSTR) {
		np = extract(cp, 0);
		np = delname(np, myname, icequal);
		if (altnames)
			for (ap = altnames; *ap; ap++)
				np = delname(np, *ap, icequal);
		cp = detract(np, 0);
	}
	strcpy(buf, "");
	if (replyto != NOSTR) {
		strcat(buf, replyto);
		needcomma++;
	}
	else if (from != NOSTR) {
		strcat(buf, from);
		needcomma++;
	}
	if (cp != NOSTR) {
		if (needcomma)
			strcat(buf, ", ");
		strcat(buf, cp);
		needcomma++;
	}
	initheader(&head);
	head.h_seq = 1;
	if (needcomma)
		head.h_to = savestr(buf);
	else
		head.h_to = NOSTR;
	cp = hfield("cc", mp);
	if (cp != NOSTR) { 
		np = extract(cp, 0);
		np = delname(np, myname, icequal);
		if (altnames != 0)
			for (ap = altnames; *ap; ap++)
				np = delname(np, *ap, icequal);
		head.h_cc = detract(np, 0);
	}
	if (head.h_to == NOSTR) {
	    if (head.h_cc != NOSTR) {
		printf("No sender or To: list, replying To: the Cc: list:\n\n");
		head.h_to = head.h_cc;
		head.h_cc = NOSTR;
	    } else {
		printf("No one to reply to!\n");
		return(0);
	    }
	}
	head.h_subject = hfield("subject", mp);
	if (head.h_subject == NOSTR)
		head.h_subject = hfield("subj", mp);
	head.h_subject = reedit(head.h_subject);
	if ((cp = value("replyto")) != NOSTR) {
		head.h_replyto = savestr(cp);
		head.h_optusedmask |= GRT;
	}
	head.h_references = hfield("references", mp);
	head.h_keywords = hfield("keywords", mp);
	mail1(&head);
	return(0);
}

/*
 * Reply to a message, recording the response in a file whose name is derived
 * from the author of the message.  Overrides the 'record' variable.
 */
int followflag = 0;

followup(msgvec)
	int *msgvec;
{
	struct message *mp;
	char *cp, *from;
	char buf[2 * LINESIZE];
	struct name *np;
	struct header head;

	if (msgvec[1] != 0) {
		printf("followup can't reply to multiple messages at once\n");
		return(1);
	}
	mp = &message[msgvec[0] - 1];
	dot = mp;

	cp = nameof(mp, 1);
	np = extract(cp, 0);
	from = detract(np, 0);

	strcpy(buf, "");
	if (from != NOSTR) {
		strcat(buf, from);
	} else {
		printf("No one to reply to!\n");
		return(0);
	}
	initheader(&head);
	head.h_seq = 1;
	head.h_to = savestr(buf);
	head.h_subject = hfield("subject", mp);
	if (head.h_subject == NOSTR)
		head.h_subject = hfield("subj", mp);
	head.h_subject = reedit(head.h_subject);
	head.h_references = hfield("references", mp);
	head.h_keywords = hfield("keywords", mp);

	followflag++;	/* used in 'mail1()' */

	mail1(&head);
	followflag = 0;
	return(0);
}

/*
 * Reply to the first message in the msglist, sending the message to the
 * author of each message in the msglist.  The subject line is taken from
 * the first message and the response is recorded in a file whose name is
 * derived from the author of the first message.
 */

Followup(msglst)
	int *msglst;
{
	followflag++;	/* used in 'mail1()' */
	_Respond(msglst);
	followflag = 0;
	return(0);
}

/*
 * Modify the subject we are replying to to begin with Re: if
 * it does not already.
 */

char *
reedit(subj)
	char *subj;
{
	char sbuf[10];
	register char *newsubj;

	if (subj == NOSTR)
		return(NOSTR);
	strncpy(sbuf, subj, 3);
	sbuf[3] = 0;
	if (icequal(sbuf, "re:"))
		return(subj);
	newsubj = salloc(strlen(subj) + 6);
	snprintf(newsubj, strlen(subj) + 6, "Re:  %s", subj);
	return(newsubj);
}

/*
 * Preserve the named messages, so that they will be sent
 * back to the system mailbox.
 */

preserve(msgvec)
	int *msgvec;
{
	register struct message *mp;
	register int *ip, mesg;

	if (edit) {
		printf("Cannot \"preserve\" in edit mode\n");
		return(1);
	}
	for (ip = msgvec; *ip != NULL; ip++) {
		mesg = *ip;
		mp = &message[mesg-1];
		mp->m_flag |= MPRESERVE;
		mp->m_flag &= ~MBOX;
		dot = mp;
	}
	return(0);
}

/*
 * Mark all given messages as unread.
 */
unread(msgvec)
	int	msgvec[];
{
	register int *ip;

	for (ip = msgvec; *ip != NULL; ip++) {
		dot = &message[*ip-1];
		dot->m_flag &= ~(MREAD|MTOUCH);
		dot->m_flag |= MSTATUS;
	}
	return(0);
}

/*
 * Print the size of each message.
 */

messize(msgvec)
	int *msgvec;
{
	register struct message *mp;
	register int *ip, mesg;

	for (ip = msgvec; *ip != NULL; ip++) {
		mesg = *ip;
		mp = &message[mesg-1];
		printf("%d: %d/%ld\n", mesg, mp->m_lines, mp->m_size);
	}
	return(0);
}

/*
 * Quit quickly.  If we are sourcing, just pop the input level
 * by returning an error.
 */

rexit(e)
{
	if (sourcing)
		return(1);
	if (Tflag != NOSTR)
		close(creat(Tflag, 0600));
	rounlock();
	exit(mailrcerr?mailrcerr:e);
}

/*
 * Set or display a variable value.  Syntax is similar to that
 * of csh.
 */

set(arglist)
	char **arglist;
{
	register struct var *vp;
	register char *cp, *cp2;
	char varbuf[BUFSIZ], **ap, **p;
	int errs, h, s;

	if (argcount(arglist) == 0) {
		for (h = 0, s = 1; h < HSHSIZE; h++)
			for (vp = variables[h]; vp != NOVAR; vp = vp->v_link)
				s++;
		ap = (char **) salloc(s * sizeof *ap);
		for (h = 0, p = ap; h < HSHSIZE; h++)
			for (vp = variables[h]; vp != NOVAR; vp = vp->v_link)
				*p++ = vp->v_name;
		*p = NOSTR;
		sort(ap);
		for (p = ap; *p != NOSTR; p++) {
			cp = *p;
			if (!strcmp("ask", cp))
				cp = "asksub";
			if (!strcmp("noask", cp))
				cp = "noasksub";
			printf("%s\t%s\n", cp, value(*p));
		}
		return(0);
	}
	errs = 0;
	for (ap = arglist; *ap != NOSTR; ap++) {
		cp = *ap;
		cp2 = varbuf;
		while (*cp != '=' && *cp != '\0')
			*cp2++ = *cp++;
		*cp2 = '\0';
		if (*cp == '\0')
			cp = "";
		else
			cp++;
		if (equal(varbuf, "")) {
			printf("Non-null variable name required\n");
			errs++;
			continue;
		}
		assign(varbuf, cp);
	}
	return(errs);
}

/*
 * Unset a bunch of variable values.
 */

unset(arglist)
	char **arglist;
{
	register struct var *vp, *vp2;
	register char *cp;
	int errs, h;
	char **ap;

	errs = 0;
	for (ap = arglist; *ap != NOSTR; ap++) {
		if ((vp2 = lookup(*ap)) == NOVAR) {
			if (!sourcing) {
				printf("\"%s\": undefined variable\n", *ap);
				errs++;
			}
			continue;
		}
		h = hash(*ap);
		if (vp2 == variables[h]) {
			variables[h] = variables[h]->v_link;
			vfree(vp2->v_name);
			vfree(vp2->v_value);
			cfree(vp2);
			continue;
		}
		for (vp = variables[h]; vp->v_link != vp2; vp = vp->v_link)
			;
		vp->v_link = vp2->v_link;
		vfree(vp2->v_name);
		vfree(vp2->v_value);
		cfree(vp2);
	}
	return(errs);
}

/*
 * Put add users to a group.
 */

group(argv)
	char **argv;
{
	register struct grouphead *gh;
	register struct group *gp;
	register int h;
	int s;
	char **ap, *gname, **p;

	if (argcount(argv) == 0) {
		for (h = 0, s = 1; h < HSHSIZE; h++)
			for (gh = groups[h]; gh != NOGRP; gh = gh->g_link)
				s++;
		ap = (char **) salloc(s * sizeof *ap);
		for (h = 0, p = ap; h < HSHSIZE; h++)
			for (gh = groups[h]; gh != NOGRP; gh = gh->g_link)
				*p++ = gh->g_name;
		*p = NOSTR;
		sort(ap);
		for (p = ap; *p != NOSTR; p++)
			printgroup(*p);
		return(0);
	}
	if (argcount(argv) == 1) {
		printgroup(*argv);
		return(0);
	}
	gname = *argv;
	h = hash(gname);
	if ((gh = findgroup(gname)) == NOGRP) {
		gh = (struct grouphead *) calloc(sizeof *gh, 1);
		gh->g_name = vcopy(gname);
		gh->g_list = NOGE;
		gh->g_link = groups[h];
		groups[h] = gh;
	}

	/*
	 * Insert names from the command list into the group.
	 * Who cares if there are duplicates?  They get tossed
	 * later anyway.
	 */

	for (ap = argv+1; *ap != NOSTR; ap++) {
		gp = (struct group *) calloc(sizeof *gp, 1);
		gp->ge_name = vcopy(*ap);
		gp->ge_link = gh->g_list;
		gh->g_list = gp;
	}
	return(0);
}

/*
 * Sort the passed string vecotor into ascending dictionary
 * order.
 */

sort(list)
	char **list;
{
	register char **ap;
	int diction();

	for (ap = list; *ap != NOSTR; ap++)
		;
	if (ap-list < 2)
		return;
	qsort(list, ap-list, sizeof *list, diction);
}

/*
 * Do a dictionary order comparison of the arguments from
 * qsort.
 */

diction(a, b)
	register char **a, **b;
{
	return(strcmp(*a, *b));
}

/*
 * The do nothing command for comments.
 */

null(e)
{
	return(0);
}

/*
 * Print out the current edit file, if we are editing.
 * Otherwise, print the name of the person who's mail
 * we are reading.
 */

file(argv)
	char **argv;
{
	register char *cp;
	char fname[BUFSIZ];
	int edit, ronly;

	if (argv[0] == NOSTR) {
		newfileinfo();
		return(0);
	}

	/*
	 * Acker's!  Must switch to the new file.
	 * We use a funny interpretation --
	 *	# -- gets the previous file
	 *	% -- gets the invoker's post office box
	 *	%user -- gets someone else's post office box
	 *	& -- gets invoker's mbox file
	 *	@ -- gets the current file in readonly mode
	 *	$ -- gets the current file in read/write mode if possible
	 *	string -- reads the given file
	 */

	cp = getfilename(argv[0], &edit, &ronly);
	if (cp == NOSTR)
		return(-1);
	if (setfile(cp, edit, ronly)) {
		perror(cp);
		return(-1);
	}
	announce(0);
}

/*
 * Evaluate the string given as a new mailbox name.
 * Ultimately, we want this to support a number of meta characters.
 * Possibly:
 *	% -- for my system mail box
 *	%user -- for user's system mail box
 *	# -- for previous file
 *	& -- get's invoker's mbox file
 *	@ -- for the current file
 *	file name -- for any other file
 */

static char prevfile[PATHSIZE];
static int prevedit;

char *
getfilename(name, aedit, ronly)
	char *name;
	int *aedit, *ronly;
{
	register char *cp;
	char savename[BUFSIZ];
	char oldmailname[BUFSIZ];

	/*
	 * Assume we will be in "edit file" mode, until
	 * proven wrong.
	 */
	*aedit = 1;

	/*
	 * Don't assume read-only mode.
	 */
	*ronly = NOT_READONLY;

	switch (*name) {
	case '%':
		*aedit = 0;
		strncpy(prevfile, mailname, PATHSIZE);
		prevfile[PATHSIZE - 1] = '\0';
		prevedit = edit;
		if (name[1] != 0) {
			strncpy(savename, myname, BUFSIZ);
			savename[BUFSIZ - 1] = '\0';
			strncpy(oldmailname, mailname, BUFSIZ);
			oldmailname[BUFSIZ - 1] = '\0';
			strncpy(myname, name+1, PATHSIZE-1);
			myname[PATHSIZE-1] = 0;
			findmail();
			cp = savestr(mailname);
			strncpy(myname, savename, PATHSIZE);
			myname[PATHSIZE-1] = 0;
			strncpy(mailname, oldmailname, PATHSIZE);
			mailname[PATHSIZE - 1]='\0';
			return(cp);
		}
		strncpy(oldmailname, mailname, BUFSIZ);
		oldmailname[BUFSIZ - 1] = '\0';
		findmail();
		cp = savestr(mailname);
		strncpy(mailname, oldmailname, PATHSIZE);
		mailname[PATHSIZE - 1]='\0';
		return(cp);

	case '#':
		if (name[1] != 0)
			goto regular;
		if (prevfile[0] == 0) {
			printf("No previous file\n");
			return(NOSTR);
		}
		cp = savestr(prevfile);
		*aedit = prevedit;
		strncpy(prevfile, mailname, PATHSIZE);
		prevfile[PATHSIZE - 1] = '\0';
		prevedit = edit;
		return(cp);

	case '@':
		/* Force the read-only mode. */
		*ronly = NO_ACCESS;
		/* Fall into . . . */

	case '$':
		cp = savestr(mailname);
		return(cp);

	case '&':
		strncpy(prevfile, mailname, PATHSIZE);
		prevfile[PATHSIZE - 1] = '\0';
		prevedit = edit;
		if (name[1] == 0)
			return(mbox);
		/* Fall into . . . */
		
	default:
regular:
		strncpy(prevfile, mailname, PATHSIZE);
		prevfile[PATHSIZE - 1] = '\0';
		prevedit = edit;
		cp = expand(name);
		return(cp);
	}
}

/*
 * Expand file names like echo
 */

echo(argv)
	char **argv;
{
	register char **ap;
	register char *cp;

	for (ap = argv; *ap != NOSTR; ap++) {
		cp = *ap;
		if ((cp = expand(cp)) != NOSTR)
			printf("%s ", cp);
	}
	return(0);
}

Reply(msgvec)
	int *msgvec;
{
	if (!ismailx)
		return (Respond(msgvec));

	if ((value("flipr") != NOSTR) && (value("noflipr") == NOSTR))
		return (_respond(msgvec));
	else
		return (_Respond(msgvec));
}

reply(msgvec)
	int *msgvec;
{
	if (!ismailx)
		return (Respond(msgvec));

	if ((value("flipr") != NOSTR) && (value("noflipr") == NOSTR))
		return (_Respond(msgvec));
	else
		return (_respond(msgvec));
}

Respond(msgvec)
	int *msgvec;
{
	if (value("Replyall") == NOSTR)
		return (_Respond(msgvec));
	else
		return (_respond(msgvec));
}

/*
 * Reply to a series of messages by simply mailing to the senders
 * and not messing around with the To: and Cc: lists as in normal
 * reply.
 */

_Respond(msgvec)
	int msgvec[];
{
	struct header head;
	struct message *mp;
	struct name *np;
	register int s, *ap;
	register char *cp, *cp2, *subject;

	s = 0;

	if (!ismailx) {
		ap = msgvec;
		mp = &message[*ap - 1];
		dot = mp;
		if (!followflag && ((cp2 = hfield("reply-to", mp)) != NOSTR)) {
			s += strlen(cp2) + 1;
		}
		else {
			if((cp2 = nameof(mp, 1)) != NOSTR) 
				s += strlen(cp2) + 1;
		}
	} else {
		for (s = 0, ap = msgvec; *ap != 0; ap++) {
			mp = &message[*ap - 1];
			dot = mp;
			s += strlen(nameof(mp)) + 2;
		}
	}
	if (s == 0) {
		printf("No one to reply to!\n");
		return(0);
	}
	cp = salloc(s + 2);

	initheader(&head);

	if (!ismailx) {
		head.h_to = cp;
		cp = copy(cp2, cp, s + 2);
		if(cp < (head.h_to + s + 1)) {
			strcat(cp, ", ");
			*cp = '\0';
		}
	} else {
		head.h_to = cp;
		*cp = '\0';
		for (ap = msgvec; *ap != 0; ap++) {
			mp = &message[*ap - 1];
			cp = copy(nameof(mp), cp, (s + 2) - strlen(head.h_to));
			if(cp < (cp + s + 1))
				*cp++ = ',';
			if(cp < (cp + s + 1))
				*cp++ = ' ';
		}
		*cp = 0;
	}
	np = extract(head.h_to, 0);
	if ((head.h_to = detract(np, 0)) == NOSTR) {
		printf("No one to reply to!\n");
		return(0);
	}
	mp = &message[msgvec[0] - 1];
	subject = hfield("subject", mp);

	if (subject == NOSTR)
		subject = hfield("subj", mp);
	head.h_subject = reedit(subject);
	if (subject != NOSTR)
		head.h_seq++;
	if ((cp = value("replyto"))!= NOSTR) {
		head.h_replyto = savestr(cp);
		head.h_optusedmask |= GRT;
	}
	head.h_references = hfield("references", mp);
	head.h_keywords = hfield("keywords", mp);
	mail1(&head);
	return(0);
}

/*
 * Conditional commands.  These allow one to parameterize one's
 * .mailrc and do some things if sending, others if receiving.
 */

ifcmd(argv)
	char **argv;
{
	register char *cp;

	if (cond != CANY) {
		printf("Illegal nested \"if\"\n");
		return(1);
	}
	cond = CANY;
	cp = argv[0];
	switch (*cp) {
	case 'r': case 'R':
		cond = CRCV;
		break;

	case 's': case 'S':
		cond = CSEND;
		break;

	default:
		printf("Unrecognized if-keyword: \"%s\"\n", cp);
		return(1);
	}
	return(0);
}

/*
 * Implement 'else'.  This is pretty simple -- we just
 * flip over the conditional flag.
 */

elsecmd()
{

	switch (cond) {
	case CANY:
		printf("\"Else\" without matching \"if\"\n");
		return(1);

	case CSEND:
		cond = CRCV;
		break;

	case CRCV:
		cond = CSEND;
		break;

	default:
		printf("Mail's idea of conditions is screwed up\n");
		cond = CANY;
		break;
	}
	return(0);
}

/*
 * End of if statement.  Just set cond back to anything.
 */

endifcmd()
{

	if (cond == CANY) {
		printf("\"Endif\" without matching \"if\"\n");
		return(1);
	}
	cond = CANY;
	return(0);
}

/*
 * Set the list of alternate names.
 */
alternates(namelist)
	char **namelist;
{
	register int c;
	register char **ap, **ap2, *cp;

	c = argcount(namelist) + 1;
	if (c == 1) {
		if (altnames == 0)
			return(0);
		for (ap = altnames; *ap; ap++)
			printf("%s ", *ap);
		printf("\n");
		return(0);
	}
	if (altnames != 0)
		cfree((char *) altnames);
	altnames = (char **) calloc(c, sizeof (char *));
	for (ap = namelist, ap2 = altnames; *ap; ap++, ap2++) {
		cp = (char *) calloc(strlen(*ap) + 1, sizeof (char));
		strcpy(cp, *ap);
		*ap2 = cp;
	}
	*ap2 = 0;
	return(0);
}

/*
 * Split a command line into two parts: the message lists
 * and the remainder. Return a pointer to the latter (or NOSTR
 * if there is none). Indicate the presence of message lists
 * by returning 1 in the reference flag variable.
 * Put a null in front of the latter part so that the message
 * list processing won't see it, unless the latter part is the
 * only thing on the line, in which case, return 0 in the
 * reference flag variable.
 */

char *
sep_cmd_msg(linebuf, flag)
	char linebuf[];
	int *flag;
{
	register char *cp, *cp2;
	char *svcp;

	*flag = 1;
	/*
	 * Strip away beginning blanks
	 */
	cp = linebuf;
	while (*cp && isspace(*cp))
		cp++;
	svcp = cp;

	/* is there any message list? */

	/* search for something which is not a message list */
	while (isdigit(*cp) || any(*cp, ":&^$*/.+-")) {
		while (*cp && !isspace(*cp))
			cp++;
		while (*cp && isspace(*cp))
			cp++;
	}

	/* no message list? */
	if (cp == svcp)
		*flag = 0;

	/* separate the two parts */
	if (*cp && cp > linebuf)
		cp[-1] = '\0';

	/* check for quoted string */
	if (*cp == '"' || *cp == '\'') {
		/* zap any trailing quote */
		cp++;
		cp2 = cp + strlen(cp) - 1;
		while ((cp2 > cp) && isspace(*cp2))
			cp2--;
		if ((cp2 > cp) && (*cp2 == cp[-1]))
			*cp2 = '\0';
		else {
			fprintf(stdout, "Syntax error: missing %c.\n", cp[-1]);
			*flag = -1;
			return(NOSTR);
		}
	}
	return *cp ? cp : NOSTR;
}

int
pipecmd(msgandcmd)
	char msgandcmd[];
{
	register int *ip, mesg;
	register struct message *mp;
	char *cp;
	int f, nowait=0;
	void (*sigint)(), (*sigpipe)();
	int lc, cc, t;
	register pid_t pid;
	int page, s, pivec[2];
	char *Shell;
	FILE *pio;
	int *msgvec = (int *) salloc((msgCount + 2) * sizeof *msgvec);
	char *cmd;

	cmd = sep_cmd_msg(msgandcmd, &f);
	if (cmd == NOSTR) {
		if (f == -1) {
			fprintf(stdout, "pipe command error\n");
			return(1);
		}
		if ( (cmd = value("cmd")) == NOSTR) {
			fprintf(stdout, "\"cmd\" not set, ignored.\n");
			return(1);
		}
	}
	if (!f) {
		*msgvec = first(0, MMNORM);
		if (*msgvec == NULL) {
			fprintf(stdout, "No messages to pipe.\n");
			return(1);
		}
		msgvec[1] = NULL;
	}
	if (f && getmsglist(msgandcmd, msgvec, 0) < 0)
		return(1);
	if (*(cp=cmd+strlen(cmd)-1)=='&') {
		*cp=0;
		nowait++;
	}
	fprintf(stdout, "Pipe to: \"%s\"\n", cmd);
	fflush(stdout);
					/*  setup pipe */
	if (pipe(pivec) < 0) {
		fprintf(stderr, "pipe failed: %s", strerror(errno));
		return(0);
	}

	if ((pid = vfork()) == 0) {

		close(pivec[1]);	/* child */
		fclose(stdin);
		dup(pivec[0]);
		close(pivec[0]);
		if ((Shell = value("SHELL")) == NOSTR || *Shell=='\0')
			Shell = SHELL_XPG4;
		execl(Shell, Shell, "-c", cmd, (char *)0);
		fprintf(stderr, "cmd=%s failed: %s\n", 
			cmd, strerror(errno));
		fflush(stderr);
		_exit(1);
	}
	if (pid == (pid_t)-1) {		/* error */
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		close(pivec[0]);
		close(pivec[1]);
		return(0);
	}

	close(pivec[0]);		/* parent */
	pio=fdopen(pivec[1],"w");
	sigint = sigset(SIGINT, SIG_IGN);
	sigpipe = sigset(SIGPIPE, SIG_IGN);

					/* send all messages to cmd */
	page = (value("page")!=NOSTR);
	lc = cc = 0;
	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		if ((t = send_msg(mp, pio, NO_SUPPRESS, &lc, &cc, 0)) < 0) {
			fprintf(stderr, "cmd=%s failed: %s\n", 
					cmd, strerror(errno));
			sigset(SIGPIPE, sigpipe);
			sigset(SIGINT, sigint);
			return(1);
		}
		lc += t;
		cc += mp->m_size;
		if (page) putc('\f', pio);
	}

	fflush(pio);

/* XXX - revisit this code after branding
 *	 The 'ferror()' call "sometimes" produces an error 
 *	 (3) CPU on a 150 MHz machine.
 *
	if (ferror(pio))
	      fprintf(stderr, "cmd=%s failed: %s\n", cmd, strerror(errno));
*/
	fclose(pio);

					/* wait */
	if (!nowait) {
		while (wait(&s) != pid);
		if (s != 0)
			fprintf(stdout, "Pipe to \"%s\" failed\n", cmd);
	}
	if (nowait || s == 0)
		printf("\"%s\" %ld/%ld\n", cmd, lc, cc);
	sigset(SIGPIPE, sigpipe);
	sigset(SIGINT, sigint);
	return(0);
}
