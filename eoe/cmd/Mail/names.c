/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)names.c	5.2 (Berkeley) 6/21/85";
#endif /* !lint */

/*
 * Mail -- a mail program
 *
 * Handle name lists.
 */

#include "glob.h"
#include <ctype.h>

/*
 * Allocate a single element of a name list,
 * initialize its name field to the passed
 * name and return it.
 */

struct name *
nalloc(str)
	char str[];
{
	register struct name *np;

	np = (struct name *) salloc(sizeof *np);
	np->n_flink = NIL;
	np->n_blink = NIL;
	np->n_type = -1;
	np->n_name = savestr(str);
	return(np);
}

/*
 * Find the tail of a list and return it.
 */

struct name *
tailof(name)
	struct name *name;
{
	register struct name *np;

	np = name;
	if (np == NIL)
		return(NIL);
	while (np->n_flink != NIL)
		np = np->n_flink;
	return(np);
}

/*
 * Extract a list of names from a line,
 * and make a list of names from it.
 * Return the list or NIL if none found.
 */

struct name *
extract(line, ntype)
	char line[];
{
	register char *cp;
	register struct name *top, *np, *t;
	char nbuf[BUFSIZ], abuf[BUFSIZ];

	if (line == NOSTR || strlen(line) == 0)
		return(NIL);
	top = NIL;
	np = NIL;
	cp = line;
	while ((cp = yankword(cp, nbuf)) != NOSTR) {
		if (np != NIL && equal(nbuf, "at")) {
			strncpy(abuf, nbuf, BUFSIZ);
			abuf[BUFSIZ - 1] = '\0';
			if ((cp = yankword(cp, nbuf)) == NOSTR) {
				strncpy(nbuf, abuf, BUFSIZ);
				nbuf[BUFSIZ - 1] = '\0';
				goto normal;
			}
			strncpy(abuf, np->n_name, BUFSIZ);
			abuf[BUFSIZ - 1] = '\0';
			if(strlen(abuf) < (BUFSIZ - 1))
				strcat(abuf, "@");
			strncat(abuf, nbuf, BUFSIZ);
			abuf[BUFSIZ - 1] = '\0';
			np->n_name = savestr(abuf);
			continue;
		}
normal:
		t = nalloc(nbuf);
		t->n_type = ntype;
		if (top == NIL)
			top = t;
		else
			np->n_flink = t;
		t->n_blink = np;
		np = t;
	}
	return(top);
}

/*
 * Turn a list of names into a string of the same names.
 */

char *
detract(np, ntype)
	register struct name *np;
{
	register int s;
	register char *cp, *top;
	register struct name *p;
	register int comma;

/*
 *	comma = ntype & GCOMMA;
 */
	comma = 1;
	if (np == NIL)
		return(NOSTR);
	ntype &= ~GCOMMA;
	s = 0;
	if (debug && comma)
		fprintf(stderr, "detract asked to insert commas\n");
	for (p = np; p != NIL; p = p->n_flink) {
		if (ntype && (p->n_type & GMASK) != ntype)
			continue;
		s += strlen(p->n_name) + 1;
		if (comma)
			s++;
	}
	if (s == 0)
		return(NOSTR);
	s += 2;
	top = salloc(s);
	cp = top;
	for (p = np; p != NIL; p = p->n_flink) {
		if (ntype && (p->n_type & GMASK) != ntype)
			continue;
		cp = copy(p->n_name, cp, (top + s) - cp);
		if (comma && p->n_flink != NIL)
			if((cp + 1) < (top + s - 1))
				*cp++ = ',';
		if((cp + 1) < (top + s - 1))
			*cp++ = ' ';
	}
	*--cp = 0;
	if (comma && *--cp == ',')
		*cp = 0;
	return(top);
}

/*
 * Grab a single word (liberal word, address actually)
 * Ignore things between ()'s.
 */

char *
yankword(ap, wbuf)
	char *ap, wbuf[];
{
	register char *cp, *cp2, *cp3;
	register int nestlevel;

	cp = ap;
	/*
	 * Strip leading garbage
	 */
	while (isspace(*cp) || *cp == ',')
		cp++;
	if (*cp == '\0')
		return(NOSTR);
	/*
	 * Scan ahead and see if there's a "<route-addr>" 
	 * or comma further down the line.  If so, the first
         * one we encounter deliniates the end of the current
         * address field.
	 */
	cp3 = cp;
	while (*cp3 != '\0' && *cp3 != '<' && *cp3 != ',') {
		if (*cp3 == '(')
			while (*cp3 != '\0' && *cp3++ != ')')
				;
		else if (*cp3 == '\"') {
			cp3++;
			while (*cp3 != '\0' && *cp3++ != '\"')
				;
		}
		else
			cp3++;
	}
	if (*cp3 == ',')
		cp3--;
	else if (*cp3 == '<') {
		/*
		 * Allow nesting, sendmail seems to!
		 */
		nestlevel = 1;
		while (*++cp3 != '\0') {
			if (*cp3 == '<')
				nestlevel++;
			else if (*cp3 == '>' && --nestlevel <= 0)
				break;
		}
	}
	else if (*cp3 == '\0') {
		/*
		 * O.K.  What we got here is some nasty arbitrary
		 * string.  Send it on to the parser.
		 */

		cp3 = getaddr(cp);

		/*
		 * Strip any trailing spaces.
		 */
		while(isspace(*cp3))
			cp3--;
	}
	/*
	 * cp now points to the first character of the complete address
	 * field, cp3 points to the last.
	 *
	 * Copy the address into wbuf.
	 */
	if (*cp == '\0')
		return(NOSTR);
	for (cp2 = wbuf; (cp <= cp3 && *cp != '\0') ; *cp2++ = *cp++)
		;
	*cp2 = '\0';
	return(cp);
}

/* state definitions */

#define LWSP	0
#define LCOM	1
#define QUOT	2
#define  WSP	3
#define COM1	4
#define COM2	5
#define COM3	6
#define SPEC	7
#define WORD	8

#define DONE	-1

/* input definitions */

#define   ENDL	0
#define  SPACE	1
#define  SPCHR	2
#define OPEREN	3
#define CPEREN	4
#define  QUOTE	5
#define  OTHER	6


static short parsetab[9][7] = {

/*	    ENDL,  SPACE,  SPCHR, OPEREN, CPEREN,  QUOTE, OTHER */
/*LWSP*/{   DONE,   LWSP,   SPEC,   LCOM,   LWSP,   QUOT,  WORD },
/*LCOM*/{   DONE,   LCOM,   LCOM,   LCOM,   LCOM,   LCOM,  LCOM },
/*QUOT*/{   DONE,   QUOT,   QUOT,   QUOT,   QUOT,    WSP,  QUOT },
/* WSP*/{   DONE,    WSP,   SPEC,   COM1,   DONE,   QUOT,  DONE },
/*COM1*/{   DONE,   COM1,   COM1,   COM1,   COM1,   COM1,  COM1 },
/*COM2*/{   DONE,   COM2,   COM2,   COM2,   COM2,   COM2,  COM2 },
/*COM3*/{   DONE,   COM3,   COM3,   COM3,   COM3,   COM3,  COM3 },
/*SPEC*/{   DONE,   SPEC,   SPEC,   COM3,   WORD,   QUOT,  WORD },
/*WORD*/{   DONE,    WSP,   SPEC,   COM2,   WORD,   QUOT,  WORD }
};

char *
getaddr(cp1)
	char *cp1;
{
	register int curstate, curinput, nextstate, nestlevel;
	char *cp;

	cp = cp1;
	curstate = LWSP;
	nestlevel = 0;
	curinput = convert(*cp);
	while ((nextstate = parsetab[curstate][curinput]) != DONE) {
		/*
		 * Some states require special processing.
		 */
		switch(nextstate) {
		case LCOM:
			if (curinput == CPEREN && --nestlevel <= 0) {
				nextstate = LWSP;
				nestlevel = 0;
			}
			else if (curinput == OPEREN)
				nestlevel++;
			break;
		case COM1:
			if (curinput == CPEREN && --nestlevel <= 0) {
				nextstate = WSP;
				nestlevel = 0;
			}
			else if (curinput == OPEREN)
				nestlevel++;
			break;
		case COM2:
			if (curinput == CPEREN && --nestlevel <= 0) {
				nextstate = WORD;
				nestlevel = 0;
			}
			else if (curinput == OPEREN)
				nestlevel++;
			break;
		case COM3:
			if (curinput == CPEREN && --nestlevel <= 0) {
				nextstate = SPEC;
				nestlevel = 0;
			}
			else if (curinput == OPEREN)
				nestlevel++;
			break;
		}
		curstate = nextstate;
		curinput = convert(*++cp);
	}
	return(--cp);
}

convert(chr)
	char chr;
{
	if (isspace(chr))
		return(SPACE);
	else if (chr == '\0')
		return(ENDL);
	else if (chr == '.' || chr == '@')
		return(SPCHR);
	else if (chr == '(')
		return(OPEREN);
	else if (chr == ')')
		return(CPEREN);
	else if (chr == '\"')
		return(QUOTE);
	else
		return(OTHER);
}

tidyaddr(cp1, cp2)
	char *cp1, *cp2;
{
	register int inquote, nestlevel;
	char *cp;

	cp = cp2;
	inquote = nestlevel = 0;
	while(*cp1 != '\0') {
		if (*cp1 == '\"') {
			if (inquote)
				inquote = 0;
			else
				inquote++;
		}
		if (*cp1 == '(') {
			nestlevel = 1;
			while(*++cp1 != '\0') {
				if (*cp1 == '(')
					nestlevel++;
				else if (*cp1 == ')' && --nestlevel <= 0) {
					cp1++;
					break;
				}
			}
			continue;
		}
		if (isspace(*cp1) && !inquote) {
			cp1++;
			continue;
		}
		if (*cp1 == '<') {
			nestlevel = 1;
			cp = cp2;
			do {
				*cp++ = *cp1++;
				if (*cp1 == '<')
					nestlevel++;
				else if (*cp1 == '>' && --nestlevel <= 0) {
					*cp++ = *cp1++;
					break;
				}
			} while(*cp1 != '\0');
			break;
		}
		*cp++ = *cp1++;
	}
	*cp = '\0';
	return;
}

/*
 * Verify that all the users in the list of names are
 * legitimate.  Bitch about and delink those who aren't.
 */

struct name *
verify(names)
	struct name *names;
{
	register struct name *np, *top, *t, *x;
	register char *cp;

#ifdef SENDMAIL
	return(names);
#else
	top = names;
	np = names;
	while (np != NIL) {
		if (np->n_type & GDEL) {
			np = np->n_flink;
			continue;
		}
		for (cp = "!:@^"; *cp; cp++)
			if (any(*cp, np->n_name))
				break;
		if (*cp != 0) {
			np = np->n_flink;
			continue;
		}
		cp = np->n_name;
		while (*cp == '\\')
			cp++;
		if (equal(cp, "msgs") ||
		    getuserid(cp) != -1) {
			np = np->n_flink;
			continue;
		}
		fprintf(stderr, "Can't send to %s\n", np->n_name);
		senderr++;
		if (np == top) {
			top = np->n_flink;
			if (top != NIL)
				top->n_blink = NIL;
			np = top;
			continue;
		}
		x = np->n_blink;
		t = np->n_flink;
		x->n_flink = t;
		if (t != NIL)
			t->n_blink = x;
		np = t;
	}
	return(top);
#endif
}

/*
 * For each recipient in the passed name list with a /
 * in the name, append the message to the end of the named file
 * and remove him from the recipient list.
 *
 * Recipients whose name begins with | are piped through the given
 * program and removed.
 */

struct name *
outof(names, fo, hp)
	struct name *names;
	FILE *fo;
	struct header *hp;
{
	register int c;
	register struct name *np, *top, *t, *x;
	long now;
	char *date, *fname, *shell, *ctime();
	FILE *fout, *fin;
	int ispipe, s, pid;
	extern char tempEdit[];

	top = names;
	np = names;
	time(&now);
	date = ctime(&now);
	while (np != NIL) {
		if (!isfileaddr(np->n_name) && np->n_name[0] != '|') {
			np = np->n_flink;
			continue;
		}
		ispipe = np->n_name[0] == '|';
		if (ispipe)
			fname = np->n_name+1;
		else
			fname = expand(np->n_name);

		/*
		 * See if we have copied the complete message out yet.
		 * If not, do so.
		 */

		if (image < 0) {
			if ((fout = fopen(tempEdit, "a")) == NULL) {
				perror(tempEdit);
				senderr++;
				goto cant;
			}
			image = open(tempEdit, 2);
			unlink(tempEdit);
			if (image < 0) {
				perror(tempEdit);
				senderr++;
				goto cant;
			}
			else {
				rewind(fo);
				fprintf(fout, "From %s %s", myname, date);
				puthead(hp, fout, (GALLHDR|GNL) & ~GBCC);
				while ((c = getc(fo)) != EOF)
					putc(c, fout);
				rewind(fo);
				putc('\n', fout);
				fflush(fout);
				if (ferror(fout))
					perror(tempEdit);
				fclose(fout);
			}
		}

		/*
		 * Now either copy "image" to the desired file
		 * or give it as the standard input to the desired
		 * program as appropriate.
		 */

		if (ispipe) {
			wait(&s);
			switch (pid = fork()) {
			case 0:
				sigchild();
				sigsys(SIGHUP, SIG_IGN);
				sigsys(SIGINT, SIG_IGN);
				sigsys(SIGQUIT, SIG_IGN);
				close(0);
				dup(image);
				close(image);
				if ((shell = value("SHELL")) == NOSTR)
					shell = SHELL;
				execl(shell, shell, "-c", fname, 0);
				perror(shell);
				safe_exit(1);
				break;

			case -1:
				perror("fork");
				senderr++;
				goto cant;
			}
		}
		else {
			if ((fout = fopen(fname, "a")) == NULL) {
				perror(fname);
				senderr++;
				goto cant;
			}
			fin = Fdopen(image, "r");
			if (fin == NULL) {
				fprintf(stderr, "Can't reopen image\n");
				fclose(fout);
				senderr++;
				goto cant;
			}
			rewind(fin);
			while ((c = getc(fin)) != EOF)
				putc(c, fout);
			if (ferror(fout))
				senderr++, perror(fname);
			fclose(fout);
			fclose(fin);
		}

cant:

		/*
		 * In days of old we removed the entry from the
		 * the list; now for sake of header expansion
		 * we leave it in and mark it as deleted.
		 */

#ifdef CRAZYWOW
		if (np == top) {
			top = np->n_flink;
			if (top != NIL)
				top->n_blink = NIL;
			np = top;
			continue;
		}
		x = np->n_blink;
		t = np->n_flink;
		x->n_flink = t;
		if (t != NIL)
			t->n_blink = x;
		np = t;
#endif

		np->n_type |= GDEL;
		np = np->n_flink;
	}
	if (image >= 0) {
		close(image);
		image = -1;
	}
	return(top);
}

#define METANET "!^:%@"

/*
 * Determine if the passed address is a local "send to file" address.
 * If any of the network metacharacters precedes any slashes, it can't
 * be a filename.
 */
isfileaddr(name)
	char *name;
{
	register char *cp;

	if (any('@', name))
		return(0);
	if (*name == '+')
		return(1);
	for (cp = name; *cp; cp++) {
		if (any(*cp, METANET))
			return(0);
		if (*cp == '/')
			return(1);
	}
	return(0);
}

/*
 * Map all of the aliased users in the invoker's mailrc
 * file and insert them into the list.
 * Changed after all these months of service to recursively
 * expand names (2/14/80).
 */

struct name *
usermap(names)
	struct name *names;
{
	register struct name *new, *np, *cp;
	struct name *getto;
	struct grouphead *gh;
	register int metoo;

	new = NIL;
	np = names;
	getto = NIL;
	metoo = (value("metoo") != NOSTR);
	while (np != NIL) {
		if (np->n_name[0] == '\\') {
			cp = np->n_flink;
			new = put(new, np);
			np = cp;
			continue;
		}
		gh = findgroup(np->n_name);
		cp = np->n_flink;
		if (gh != NOGRP)
			new = gexpand(new, gh, metoo, np->n_type);
		else
			new = put(new, np);
		np = cp;
	}
	return(new);
}

/*
 * Recursively expand a group name.  We limit the expansion to some
 * fixed level to keep things from going haywire.
 * Direct recursion is not expanded for convenience.
 */

struct name *
gexpand(nlist, gh, metoo, ntype)
	struct name *nlist;
	struct grouphead *gh;
{
	struct group *gp;
	struct grouphead *ngh;
	struct name *np;
	static int depth;
	char *cp;

	if (depth > MAXEXP) {
		printf("Expanding alias to depth larger than %d\n", MAXEXP);
		return(nlist);
	}
	depth++;
	for (gp = gh->g_list; gp != NOGE; gp = gp->ge_link) {
		cp = gp->ge_name;
		if (*cp == '\\')
			goto quote;
		if (strcmp(cp, gh->g_name) == 0)
			goto quote;
		if ((ngh = findgroup(cp)) != NOGRP) {
			nlist = gexpand(nlist, ngh, metoo, ntype);
			continue;
		}
quote:
		np = nalloc(cp);
		np->n_type = ntype;
		/*
		 * At this point should allow to expand
		 * to self if only person in group
		 */
		if (gp == gh->g_list && gp->ge_link == NOGE)
			goto skip;
		if (!metoo && strcmp(cp, myname) == 0)
			np->n_type |= GDEL;
skip:
		nlist = put(nlist, np);
	}
	depth--;
	return(nlist);
}



/*
 * Compute the length of the passed name list and
 * return it.
 */

lengthof(name)
	struct name *name;
{
	register struct name *np;
	register int c;

	for (c = 0, np = name; np != NIL; c++, np = np->n_flink)
		;
	return(c);
}

/*
 * Concatenate the two passed name lists, return the result.
 */

struct name *
cat(n1, n2)
	struct name *n1, *n2;
{
	register struct name *tail;

	if (n1 == NIL)
		return(n2);
	if (n2 == NIL)
		return(n1);
	tail = tailof(n1);
	tail->n_flink = n2;
	n2->n_blink = tail;
	return(n1);
}

/*
 * Unpack the name list onto a vector of strings.
 * Return an error if the name list won't fit.
 */

char **
unpack(np)
	struct name *np;
{
	register char **ap, **top;
	register struct name *n;
	char *cp;
	char hbuf[10];
	int t, extra, metoo, verbose;

	n = np;
	if ((t = lengthof(n)) == 0)
		panic("No names to unpack");

	/*
	 * Compute the number of extra arguments we will need.
	 * We need at least two extra -- one for "mail" and one for
	 * the terminating 0 pointer.  Additional spots may be needed
	 * to pass along -r and -f to the host mailer.
	 */

	extra = 2;
	if (rflag != NOSTR)
		extra += 2;
#ifdef SENDMAIL
	extra++;
	metoo = value("metoo") != NOSTR;
	if (metoo)
		extra++;
	verbose = value("verbose") != NOSTR;
	if (verbose)
		extra++;
#endif /* SENDMAIL */
	if (hflag)
		extra += 2;
	top = (char **) salloc((t + extra) * sizeof cp);
	ap = top;
	*ap++ = "send-mail";
	if (rflag != NOSTR) {
		*ap++ = "-r";
		*ap++ = rflag;
	}
#ifdef SENDMAIL
	*ap++ = "-i";
	if (metoo)
		*ap++ = "-m";
	if (verbose)
		*ap++ = "-v";
#endif /* SENDMAIL */
	if (hflag) {
		*ap++ = "-h";
		snprintf(hbuf, 10, "%d", hflag);
		*ap++ = savestr(hbuf);
	}
	while (n != NIL) {
		if (n->n_type & GDEL) {
			n = n->n_flink;
			continue;
		}
		tidyaddr(n->n_name, n->n_name);
		*ap++ = n->n_name;
		n = n->n_flink;
	}
	*ap = NOSTR;
	return(top);
}

/*
 * Put another node onto a list of names and return
 * the list.
 */

struct name *
put(list, node)
	struct name *list, *node;
{
	node->n_flink = list;
	node->n_blink = NIL;
	if (list != NIL)
		list->n_blink = node;
	return(node);
}

/*
 * Determine the number of elements in
 * a name list and return it.
 */

count(np)
	register struct name *np;
{
	register int c = 0;

	while (np != NIL) {
		c++;
		np = np->n_flink;
	}
	return(c);
}

/*
 * Delete the given name from a namelist, using the passed
 * function to compare the names.
 */
struct name *
delname(np, name, cmpfun)
	register struct name *np;
	char name[];
	int (* cmpfun)();
{
	register struct name *p;

	for (p = np; p != NIL; p = p->n_flink)
		if ((* cmpfun)(skin(p->n_name), skin(name))) {
			if (p->n_blink == NIL) {
				if (p->n_flink != NIL)
					p->n_flink->n_blink = NIL;
				np = p->n_flink;
				continue;
			}
			if (p->n_flink == NIL) {
				if (p->n_blink != NIL)
					p->n_blink->n_flink = NIL;
				continue;
			}
			p->n_blink->n_flink = p->n_flink;
			p->n_flink->n_blink = p->n_blink;
		}
	return(np);
}
