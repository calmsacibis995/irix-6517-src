/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)list.c	5.3 (Berkeley) 9/15/85";
#endif /* !lint */

#include "glob.h"
#include <ctype.h>

/*
 * Mail -- a mail program
 *
 * Message list handling.
 */

/*
 * Convert the user string of message numbers and
 * store the numbers into vector.
 *
 * Returns the count of messages picked up or -1 on error.
 */

getmsglist(buf, vector, flags)
	char *buf;
	int *vector;
{
	register int *ip;
	register struct message *mp;

	if (markall(buf, flags) < 0)
		return(-1);
	ip = vector;
	for (mp = &message[0]; mp < &message[msgCount]; mp++)
		if (mp->m_flag & MMARK)
			*ip++ = mp - &message[0] + 1;
	*ip = NULL;
	return(ip - vector);
}

/*
 * Mark all messages that the user wanted from the command
 * line in the message structure.  Return 0 on success, -1
 * on error.
 */

#define BSUBJIND '/'
#define BSENDIND '?'

/*
 * Bit values for colon modifiers.
 */

#define	CMNEW		01		/* New messages */
#define	CMOLD		02		/* Old messages */
#define	CMUNREAD	04		/* Unread messages */
#define	CMDELETED	010		/* Deleted messages */
#define	CMREAD		020		/* Read messages */

/*
 * The following table describes the letters which can follow
 * the colon and gives the corresponding modifier bit.
 */

struct coltab {
	char	co_char;		/* What to find past : */
	int	co_bit;			/* Associated modifier bit */
	int	co_mask;		/* m_status bits to mask */
	int	co_equal;		/* ... must equal this */
} coltab[] = {
	'n',		CMNEW,		MNEW,		MNEW,
	'o',		CMOLD,		MNEW,		0,
	'u',		CMUNREAD,	MREAD,		0,
	'd',		CMDELETED,	MDELETED,	MDELETED,
	'r',		CMREAD,		MREAD,		MREAD,
	0,		0,		0,		0
};

static	int	lastcolmod;

markall(buf, f)
	char buf[];
{
	register char **np;
	register int i;
	register struct message *mp;
	char *namelist[NMLSIZE], *bufp;
	int tok, beg, mc, star, valdot, colmod, colresult,
            bynumber, bystring;

	valdot = dot - &message[0] + 1;
	colmod = 0;
	for (i = 1; i <= msgCount; i++)
		unmark(i);
	bufp = buf;
	mc = 0;
	np = &namelist[0];
	scaninit();
	tok = scan(&bufp);
	star = 0;
	beg = 0;
	bynumber=0;
	bystring=0;
	while (tok != TEOL) {
		switch (tok) {
		case TNUMBER:
number:
			if (bystring != 0) {
				printf("Improper message list\n");
				return(-1);
			}
			bynumber++;
			mc++;
			if (beg != 0) {
				if (check(lexnumber, f))
					return(-1);
				if (beg > lexnumber) {
					printf("Improper message list\n");
					return(-1);
				}
				for (i = beg; i <= lexnumber; i++)
					if ((message[i - 1].m_flag & MDELETED)
                                             == f)
						mark(i);
				beg = 0;
				break;
			}
			beg = lexnumber;
			if (check(beg, f))
				return(-1);
			tok = scan(&bufp);
			regret(tok);
			if (tok != TDASH) {
				mark(beg);
				beg = 0;
			}
			break;

		case TPLUS:
                        if (bystring != 0) {
                                printf("Improper message list\n");
                                return(-1);
                        }
                        bynumber++;
			if (beg != 0) {
				printf("Non-numeric second argument\n");
				return(-1);
			}
			i = valdot;
			do {
				i++;
				if (i > msgCount) {
					printf("Referencing beyond EOF\n");
					return(-1);
				}
			} while ((message[i - 1].m_flag & MDELETED) != f);
			mark(i);
			break;

		case TDASH:
                        if (bystring != 0) {
                                printf("Improper message list\n");
                                return(-1);
                        }
                        bynumber++;
			if (beg == 0) {
				i = valdot;
				do {
					i--;
					if (i <= 0) {
					    printf("Referencing before 1\n");
					    return(-1);
					}
				} while ((message[i - 1].m_flag & MDELETED)
					  != f);
				mark(i);
			}
			break;

		case TSTRING:
			if (beg != 0) {
				printf("Non-numeric second argument\n");
				return(-1);
			}
                        if (bynumber != 0) {
                                printf("Improper message list\n");
                                return(-1);
                        }
                        bystring++;
			if (lexstring[0] == ':') {
				colresult = evalcol(lexstring[1]);
				if (colresult == 0) {
				    printf("Unknown colon modifier \"%s\"\n",
				           lexstring);
				    return(-1);
				}
				colmod |= colresult;
			}
			else
				*np++ = savestr(lexstring);
			break;

		case TDOLLAR:
		case TUP:
		case TDOT:
			lexnumber = metamess(lexstring[0], f);
			if (lexnumber == -1)
				return(-1);
			goto number;

		case TSTAR:
			tok = scan(&bufp);
			regret(tok);
			if (tok != TEOL || bystring || bynumber) {
				printf("Can't mix \"*\" with anything\n");
				return(-1);
			}
			star++;
			break;

		case TERROR:
			return(-1);

		}
		tok = scan(&bufp);
	}
	if (beg != 0) {
		printf("Improper message list\n");
		return(-1);
	}
	lastcolmod = colmod;
	*np = NOSTR;
	mc = 0;
	if (star) {
		for (i = 0; i < msgCount; i++)
			if ((message[i].m_flag & MDELETED) == f) {
				mark(i+1);
				mc++;
			}
		if (mc == 0) {
			printf("No applicable messages.\n");
			return(-1);
		}
		return(0);
	}

	/*
	 * If no numbers were given, mark all of the messages,
	 * so that we can unmark any whose sender was not selected
	 * if any user names were given.
	 */

	if ((np > &namelist[0] || colmod != 0) && mc == 0)
		for (i = 1; i <= msgCount; i++)
			if ((message[i-1].m_flag & (MDELETED)) == f)
				mark(i);

	/*
	 * If any names were given, go through and eliminate any
	 * messages whose senders were not requested.
	 */

	if (np > &namelist[0]) {
		for (i = 1; i <= msgCount; i++) {
			for (mc = 0, np = &namelist[0]; *np != NOSTR; np++)
				if (**np == BSUBJIND) {
					if (matchsubj(*np, i)) {
						mc++;
						break;
					}
				}
				else if (**np == BSENDIND) {
					if (matchsender(*np, i)) {
						mc++;
						break;
					}
				}
				else {
					if (sender(*np, i)) {
						mc++;
						break;
					}
				}
			if (mc == 0)
				unmark(i);
		}

		/*
		 * Make sure we got some decent messages.
		 */

		mc = 0;
		for (i = 1; i <= msgCount; i++)
			if (message[i-1].m_flag & MMARK) {
				mc++;
				break;
			}
		if (mc == 0) {
			printf("No applicable messages from {%s",
				namelist[0]);
			for (np = &namelist[1]; *np != NOSTR; np++)
				printf(", %s", *np);
			printf("}\n");
			return(-1);
		}
	}

	/*
	 * If any colon modifiers were given, go through and
	 * unmark any messages which do not satisfy the modifiers.
	 */

	if (colmod != 0) {
		for (i = 1; i <= msgCount; i++) {
			register struct coltab *colp;

			mp = &message[i - 1];
			for (colp = &coltab[0]; colp->co_char; colp++)
				if (colp->co_bit & colmod)
					if ((mp->m_flag & colp->co_mask)
					    != colp->co_equal)
						unmark(i);
			
		}
		for (mp = &message[0]; mp < &message[msgCount]; mp++)
			if (mp->m_flag & MMARK)
				break;
		if (mp >= &message[msgCount]) {
			register struct coltab *colp;

			printf("No messages satisfy");
			for (colp = &coltab[0]; colp->co_char; colp++)
				if (colp->co_bit & colmod)
					printf(" :%c", colp->co_char);
			printf("\n");
			return(-1);
		}
	}
	return(0);
}

/*
 * Turn the character after a colon modifier into a bit
 * value.
 */
evalcol(col)
{
	register struct coltab *colp;

	if (col == 0)
		return(lastcolmod);
	for (colp = &coltab[0]; colp->co_char; colp++)
		if (colp->co_char == col)
			return(colp->co_bit);
	return(0);
}

/*
 * Check the passed message number for legality and proper flags.
 */

check(mesg, f)
{
	register struct message *mp;

	if (mesg < 1 || mesg > msgCount) {
		printf("%d: Invalid message number\n", mesg);
		return(-1);
	}
	mp = &message[mesg-1];
	if ((mp->m_flag & MDELETED) != f) {
		printf("%d: Inappropriate message\n", mesg);
		return(-1);
	}
	return(0);
}

/*
 * Scan out the list of string arguments, RFC 822-style 
 * for an ALIASLIST.
 */

getaliaslist(line, argv)
	char line[];
	char **argv;
{
	register char **ap, *cp, *cp2;
	char linebuf[BUFSIZ];

	ap = argv;
	cp = line;
	while (isspace(*cp))
		cp++;
	/*
	 * First argument (if present) is group name.
	 */
	if (*cp != '\0') {
		cp2 = linebuf;
		while (!isspace(*cp) && *cp != '\0')
			*cp2++ = *cp++;
		*cp2 = '\0';
		*ap++ = savestr(linebuf);
		while ((cp = yankword(cp, linebuf)) != NOSTR) {
			*ap++ = savestr(linebuf);
		}
	}
	*ap = NOSTR;
	return(ap-argv);
}

/*
 * Scan out the list of string arguments, shell style
 * for a RAWLIST.
 */

getrawlist(line, argv)
	char line[];
	char **argv;
{
	register char **ap, *cp, *cp2;
	char linebuf[BUFSIZ], quotec;
	int done;

	ap = argv;
	cp = line;
	while (*cp != '\0') {
		while (isspace(*cp))
			cp++;
		cp2 = linebuf;
		quotec = 0;
		done = 0;

		do {
			switch(*cp) {
			case '\0':
				done = 1;
				break;
			case '\'':
			case '\"':
				if (quotec == 0)
					quotec = *cp++;
				else if (quotec != *cp)
					*cp2++ = *cp++;
				else {
					cp++;
					quotec = 0;
				}
				break;
			default:
				if (isspace(*cp) && quotec == 0)
					done = 1;
				else
					*cp2++ = *cp++;
			}
		}
		while(!done);

		*cp2 = '\0';
		if (cp2 == linebuf)
			break;
		*ap++ = savestr(linebuf);
	}
	*ap = NOSTR;
	return(ap-argv);
}

/*
 * scan out a single lexical item and return its token number,
 * updating the string pointer passed **p.  Also, store the value
 * of the number or string scanned in lexnumber or lexstring as
 * appropriate.  In any event, store the scanned `thing' in lexstring.
 */

struct lex {
	char	l_char;
	char	l_token;
} singles[] = {
	'$',	TDOLLAR,
	'.',	TDOT,
	'^',	TUP,
	'*',	TSTAR,
	'-',	TDASH,
	'+',	TPLUS,
	'(',	TOPEN,
	')',	TCLOSE,
	0,	0
};

scan(sp)
	char **sp;
{
	register char *cp, *cp2;
	register int c;
	register struct lex *lp;
	int quotec;

	if (regretp >= 0) {
		copy(stringstack[regretp], lexstring, STRINGLEN);
		lexnumber = numberstack[regretp];
		return(regretstack[regretp--]);
	}
	cp = *sp;
	cp2 = lexstring;
	c = *cp++;

	/*
	 * strip away leading white space.
	 */

	while (isspace(c))
		c = *cp++;

	/*
	 * If no characters remain, we are at end of line,
	 * so report that.
	 */

	if (c == '\0') {
		*sp = --cp;
		return(TEOL);
	}

	/*
	 * If the leading character is a digit, scan
	 * the number and convert it on the fly.
	 * Return TNUMBER when done.
	 */

	if (isdigit(c)) {
		lexnumber = 0;
		while (isdigit(c)) {
			if (cp2 - lexstring < STRINGLEN - 1) {
				*cp2++ = c;
				lexnumber = lexnumber*10 + c - '0';
			}
			c = *cp++;
		}
		*cp2 = '\0';
		*sp = --cp;
		return(TNUMBER);
	}

	/*
	 * Check for single character tokens; return such
	 * if found.
	 */

	for (lp = &singles[0]; lp->l_char != 0; lp++)
		if (c == lp->l_char) {
			lexstring[0] = c;
			lexstring[1] = '\0';
			*sp = cp;
			return(lp->l_token);
		}

	/*
	 * We've got a string!  Copy all the characters
	 * of the string into lexstring, until we see
	 * a null, space, or tab.
	 * If the lead character is a " or ', save it
	 * and scan until you get another.
	 */

	quotec = 0;
	/*
	 * If deleting by substring in subject or sender, 
	 * quotes could follow.
	 */
	if ((c == BSUBJIND) || (c == BSENDIND)) {
		*cp2++ = c;
		c = *cp++;
	}
	if (any(c, "'\"")) {
		quotec = c;
		c = *cp++;
	}
	while (c != '\0') {
		if (c == quotec) {
			cp++;
			break;
		}
		if (quotec == 0 && isspace(c))
			break;
		if (cp2 - lexstring < STRINGLEN-1)
			*cp2++ = c;
		c = *cp++;
	}
	if (quotec && c == 0) {
		fprintf(stderr, "Missing %c\n", quotec);
		return(TERROR);
	}
	*sp = --cp;
	*cp2 = '\0';
	return(TSTRING);
}

/*
 * Unscan the named token by pushing it onto the regret stack.
 */

regret(token)
{
	if (++regretp >= REGDEP)
		panic("Too many regrets");
	regretstack[regretp] = token;
	lexstring[STRINGLEN-1] = '\0';
	stringstack[regretp] = savestr(lexstring);
	numberstack[regretp] = lexnumber;
}

/*
 * Reset all the scanner global variables.
 */

scaninit()
{
	regretp = -1;
}

/*
 * Find the first message whose flags & m == f  and return
 * its message number.
 */

first(f, m)
{
	register int mesg;
	register struct message *mp;

	mesg = dot - &message[0] + 1;
	f &= MDELETED;
	m &= MDELETED;
	for (mp = dot; mp < &message[msgCount]; mp++) {
		if ((mp->m_flag & m) == f)
			return(mesg);
		mesg++;
	}
	mesg = dot - &message[0];
	for (mp = dot-1; mp >= &message[0]; mp--) {
		if ((mp->m_flag & m) == f)
			return(mesg);
		mesg--;
	}
	return(NULL);
}

/*
 * See if the passed name sent the passed message number.  Return true
 * if so.  Note that this routine requires an exact name match.
 */

sender(str, mesg)
	char *str;
{
	register struct message *mp;
	register char *cp, *cp2;
	char tidyname[STRINGLEN];

  	if (strlen(str) == 0)
                return(0);
	mp = &message[mesg-1];
	cp2 = nameof(mp, 0);
	if (cp2 == NOSTR)
		return(0);
	tidyaddr(cp2, tidyname);
	cp2 = tidyname;
	cp = str;
	while (*cp2)
		if ((*cp == 0) || (toupper(*cp++) != toupper(*cp2++)))
			return(0);
	return(*cp == 0);
}

/*
 * See if the given string matches inside the sender field of the
 * given message.  For the purpose of the scan, we ignore case differences.
 * If it does, return true.  The string search argument is assumed to
 * have the form "?search-string."
 */

char lastsenderscan[128];
char lastsubjscan[128];

matchsender(str, mesg)
	char *str;
	int mesg;
{
	register struct message *mp;
	register char *cp, *cp2, *backup;
	char tidyname[STRINGLEN];

	str++;
	if (strlen(str) == 0)
		str = lastsenderscan;
	else {
		strncpy(lastsenderscan, str, 128);
		lastsenderscan[127] = '\0';
	}
	mp = &message[mesg-1];
	cp = str;
	cp2 = nameof(mp, 0);
	if (cp2 == NOSTR)
		return(0);
	tidyaddr(cp2, tidyname);
	cp2 = tidyname;
	if ((cp2 == NOSTR) || (strlen(cp) == 0))
		return(0);
	backup = cp2;
	while (*cp2 != '\0') {
		if (*cp == '\0')
			return(1);
		if (toupper(*cp++) != toupper(*cp2++)) {
			cp2 = ++backup;
			cp = str;
		}
	}
	return(*cp == '\0');
}


/*
 * See if the given string matches inside the subject field of the
 * given message.  For the purpose of the scan, we ignore case differences.
 * If it does, return true.  The string search argument is assumed to
 * have the form "/search-string."  If it is of the form "/," we use the
 * previous search string.
 */

matchsubj(str, mesg)
	char *str;
{
	register struct message *mp;
	register char *cp, *cp2, *backup;

	str++;
	if (strlen(str) == 0)
		str = lastsubjscan;
	else {
		strncpy(lastsubjscan, str, 128);
		lastsubjscan[127] = '\0';
	}
	mp = &message[mesg-1];
	
	/*
	 * Now look, ignoring case, for the word in the string.
	 */

	cp = str;
	cp2 = hfield("subject", mp);
	if ((cp2 == NOSTR) || (strlen(cp) == 0))
		return(0);
	backup = cp2;
	while (*cp2) {
		if (*cp == 0)
			return(1);
		if (toupper(*cp++) != toupper(*cp2++)) {
			cp2 = ++backup;
			cp = str;
		}
	}
	return(*cp == 0);
}

/*
 * Mark the named message by setting its mark bit.
 */

mark(mesg)
{
	register int i;

	i = mesg;
	if (i < 1 || i > msgCount)
		panic("Bad message number to mark");
	message[i-1].m_flag |= MMARK;
}

/*
 * Unmark the named message.
 */

unmark(mesg)
{
	register int i;

	i = mesg;
	if (i < 1 || i > msgCount)
		panic("Bad message number to unmark");
	message[i-1].m_flag &= ~MMARK;
}

/*
 * Return the message number corresponding to the passed meta character.
 */

metamess(meta, f)
{
	register int c, m;
	register struct message *mp;

	c = meta;
	switch (c) {
	case '^':
		/*
		 * First 'good' message left.
		 */
		for (mp = &message[0]; mp < &message[msgCount]; mp++)
			if ((mp->m_flag & MDELETED) == f)
				return(mp - &message[0] + 1);
		printf("No applicable messages\n");
		return(-1);

	case '$':
		/*
		 * Last 'good message left.
		 */
		for (mp = &message[msgCount-1]; mp >= &message[0]; mp--)
			if ((mp->m_flag & MDELETED) == f)
				return(mp - &message[0] + 1);
		printf("No applicable messages\n");
		return(-1);

	case '.':
		/* 
		 * Current message.
		 */
		m = dot - &message[0] + 1;
		if ((dot->m_flag & MDELETED) != f) {
			printf("%d: Inappropriate message\n", m);
			return(-1);
		}
		return(m);

	default:
		printf("Unknown metachar (%c)\n", c);
		return(-1);
	}
}
