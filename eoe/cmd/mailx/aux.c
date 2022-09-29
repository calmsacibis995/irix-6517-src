/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)aux.c	5.3 (Berkeley) 9/15/85";
#endif /* !lint */

#include "glob.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <termio.h>
#include <values.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>

/*
 * Mail -- a mail program
 *
 * Auxiliary functions.
 */

/*
 * Announce a fatal error and die.
 */

panic(str)
	char *str;
{
	prs("panic: ");
	prs(str);
	prs("\n");
	safe_exit(1);
}

/*
 * Print a string on diagnostic output.
 */

prs(str)
	char *str;
{
	register char *s;

	for (s = str; *s; s++)
		;
	write(2, str, s-str);
}

/*
 * Touch the named message by setting its MTOUCH flag.
 * Touched messages have the effect of not being sent
 * back to the system mailbox on exit.
 */

touch(mesg)
{
	register struct message *mp;

	if (mesg < 1 || mesg > msgCount)
		return;
	mp = &message[mesg-1];
	mp->m_flag |= MTOUCH;
	if ((mp->m_flag & MREAD) == 0)
		mp->m_flag |= MREAD|MSTATUS;
}

/*
 * Test to see if the passed file name is a directory.
 * Return true if it is.
 */

isdir(name)
	char name[];
{
	struct stat sbuf;

	if (stat(name, &sbuf) < 0)
		return(0);
	return((sbuf.st_mode & S_IFMT) == S_IFDIR);
}

/*
 * Count the number of arguments in the given string raw list.
 */

argcount(argv)
	char **argv;
{
	register char **ap;

	for (ap = argv; *ap != NOSTR; ap++)
		;	
	return(ap-argv);
}

/*
 * Given a file address, determine the
 * block number it represents.
 */

local_blockof(off)
	off_t off;
{
	off_t a;

	a = off >> 12;
	return((int) a);
}

/*
 * Take a file address, and determine
 * its offset in the current block.
 */

local_offsetof(off)
	off_t off;
{
	off_t a;

	a = off & 07777;
	return((int) a);
}

/*
 * Format the given text to not exceed the screen width.
 */

fmt(str, txt, fo, addrlist)
	register char *str, *txt;
	register FILE *fo;
	int addrlist;
{
	register int col;
	register char *bg, *bl, *pt, ch;
	int ncol, incomment, inquote;
	struct name *np;

	if (*txt == '\0')
		return;
	ncol = getwincols() - 1;
	col = strlen(str);
	if (col)
		fprintf(fo, "%s", str);
	if (addrlist) {
		np = extract(txt, 0);
		txt = detract(np, 0);
	}
	pt = bg = txt;
	bl = 0;
	while (*bg) {
		pt++;
		if (++col > ncol) {
			if (!bl && !addrlist) {
				bl = bg;
				while (*bl && !isspace(*bl))
					bl++;
			}
			if (!bl && addrlist) {
				bl = pt;
				do {
					bl--;
					if (*bl == ')')
						while (*bl-- != '(' &&
						       bl > bg)
							;
					else if (*bl == '\"')
						while (*bl-- != '\"' &&
						       bl > bg)
							;
				} while (bl > bg && !isspace(*bl));
			}
			if (!*bl || bl <= bg)
				goto finish;
			ch = *bl;
			*bl = '\0';
			fprintf(fo, "%s\n    ", bg);
			col = 4;
			*bl = ch;
			pt = bg = ++bl;
			bl = 0;
		}
		if (!*pt) {
finish:
			fprintf(fo, "%s\n", bg);
			return;
		}
		if (isspace(*pt) && !addrlist)
			bl = pt;
		if (addrlist) {
			switch(*pt) {
			case '(':
				incomment = 1;
				break;
			case ')':
				incomment = 0;
				break;
			case '\"':
				if (inquote)
					inquote = 0;
				else
					inquote = 1;
				break;
			case ',':
				if (!incomment && !inquote) {
					bl = pt;
					bl++;
				}
			}
		}
	}
}

/*
 * Return the desired header line from the passed message
 * pointer (or NOSTR if the desired header field is not available).
 */

char *
hfield(field, mp)
	char field[];
	struct message *mp;
{
	register FILE *ibuf;
	char linebuf[LINESIZE];
	register u_int lc;

	ibuf = setinput(mp);
	if ((lc = mp->m_lines) <= 0)
		return(NOSTR);
	if (readline(ibuf, linebuf) < 0)
		return(NOSTR);
	lc--;
	do {
		lc = gethfield(ibuf, linebuf, lc);
		if (lc == -1)
			return(NOSTR);
		if (ishfield(linebuf, field))
			return(savestr(hcontents(linebuf)));
	} while (lc > 0);
	return(NOSTR);
}

/*
 * Return the next header field found in the given message.
 * Return > 0 if something found, <= 0 elsewise.
 * Must deal with \ continuations & other such fraud.
 */

gethfield(f, linebuf, rem)
	register FILE *f;
	char linebuf[];
	register int rem;
{
	char line2[LINESIZE];
	long loc;
	register char *cp, *cp2;
	register int c;


	for (;;) {
		if (rem <= 0)
			return(-1);
		if (readline(f, linebuf) < 0)
			return(-1);
		rem--;
		if (strlen(linebuf) == 0)
			return(-1);
		if (isspace(linebuf[0]))
			continue;
		if (linebuf[0] == '>')
			continue;
		cp = strchr(linebuf, ':');
		if (cp == NULL)
			continue;
		for (cp2 = linebuf; cp2 < cp; cp2++)
			if (isdigit(*cp2))
				continue;
		
		/*
		 * I guess we got a headline.
		 * Handle wraparounding
		 */
		
		for (;;) {
			if (rem <= 0)
				break;
#ifdef CANTELL
			loc = ftell(f);
			if (readline(f, line2) < 0)
				break;
			rem--;
			if (!isspace(line2[0])) {
				fseek(f, loc, 0);
				rem++;
				break;
			}
#else /* !CANTELL */
			c = getc(f);
			ungetc(c, f);
			if (!isspace(c) || c == '\n')
				break;
			if (readline(f, line2) < 0)
				break;
			rem--;
#endif /* CANTELL */
			cp2 = line2;
			for (cp2 = line2; *cp2 != 0 && isspace(*cp2); cp2++)
				;
			if (strlen(linebuf) + strlen(cp2) >= LINESIZE-2)
				break;
			cp = &linebuf[strlen(linebuf)];
			while (cp > linebuf &&
			    (isspace(cp[-1]) || cp[-1] == '\\'))
				cp--;
			*cp++ = ' ';
			for (cp2 = line2; *cp2 != 0 && isspace(*cp2); cp2++)
				;
			strncpy(cp, cp2, LINESIZE - (cp - linebuf));
			linebuf[LINESIZE - 1] = '\0';
		}
		if ((c = strlen(linebuf)) > 0) {
			cp = &linebuf[c-1];
			while (cp > linebuf && isspace(*cp))
				cp--;
			*++cp = 0;
		}
		return(rem);
	}
	/* NOTREACHED */
}

/*
 * Check whether the passed line is a header line of
 * the desired breed.
 */

ishfield(linebuf, field)
	char linebuf[], field[];
{
	register char *cp;
	register int c;

	if ((cp = strchr(linebuf, ':')) == NULL)
		return(0);
	if (cp == linebuf)
		return(0);
	cp--;
	while (cp > linebuf && isspace(*cp))
		cp--;
	c = *++cp;
	*cp = 0;
	if (icequal(linebuf ,field)) {
		*cp = c;
		return(1);
	}
	*cp = c;
	return(0);
}

/*
 * Extract the non label information from the given header field
 * and return it.
 */

char *
hcontents(hfield)
	char hfield[];
{
	register char *cp;

	if ((cp = strchr(hfield, ':')) == NULL)
		return(NOSTR);
	cp++;
	while (*cp && isspace(*cp))
		cp++;
	return(cp);
}

/*
 * Put the requested headers into the given buffer.
 */

puthead(hp, fo, w)
	struct header *hp;
	FILE *fo;
{
	register int gotcha, idx;

	gotcha = 0;
	if (hp->h_to != NOSTR && w & GTO)
		fmt("To: ", hp->h_to, fo, 1), gotcha++;
	if (hp->h_subject != NOSTR && w & GSUBJECT)
		fmt("Subject: ", hp->h_subject, fo, 0), gotcha++;
	if (hp->h_cc != NOSTR && w & GCC)
		fmt("Cc: ", hp->h_cc, fo, 1), gotcha++;
	if (hp->h_bcc != NOSTR && w & GBCC)
		fmt("Bcc: ", hp->h_bcc, fo, 1), gotcha++;
	if (hp->h_replyto != NOSTR && w & GRT)
		fmt("Reply-To: ", hp->h_replyto, fo, 1), gotcha++;
	if (hp->h_rtnrcpt != NOSTR && w & GRR)
		fmt("Return-Receipt-To: ", hp->h_rtnrcpt, fo, 1), gotcha++;
	if (hp->h_inreplyto != NOSTR && w & GIRT)
		fmt("In-Reply-To: ", hp->h_inreplyto, fo, 0), gotcha++;
	if (hp->h_references != NOSTR && w & GREF)
		fmt("References: ", hp->h_references, fo, 0), gotcha++;
	if (hp->h_keywords != NOSTR && w & GKEY)
		fmt("Keywords: ", hp->h_keywords, fo, 0), gotcha++;
	if (hp->h_comments != NOSTR && w & GCOM)
		fmt("Comments: ", hp->h_comments, fo, 0), gotcha++;
	if (hp->h_encrypt != NOSTR && w & GEN)
		fmt("Encrypted: ", hp->h_encrypt, fo, 0), gotcha++;
	if (w & GEX) {
		for (idx = 0; idx < MAX_EX_HDRS; idx++) {
			if (hp->h_ex_hdrs[idx] == NOSTR)
				continue;
			fprintf(fo, "%s\n", hp->h_ex_hdrs[idx]);
			gotcha++;
		}
	}
	/* GNL and GSEP are mutually exclusive. */
	if (gotcha && w & GNL)
		putc('\n', fo);
	else if (gotcha && w & GSEP)
		fprintf(fo, END_HDRS);
	return(0);
}


/*
 * Extract all headers from the given file.  Place
 * them correctly into the given header structure.
 * Return the file with the headers part and any separator
 * removed.
 */
int
extracthead(hp, fname)
	struct header *hp;
	char *fname;
{
	register int lc, idx;
	char linebuf[LINESIZE];
	char tfname[PATH_MAX];
	FILE *fp, *tfp;

	if ((fp = fopen(fname, "r")) == NULL)
		return(-1);
	if ((tfp = mkntmpfil(tfname)) == NULL) {
		fclose(fp);
		return(-1);
	}

	/* Count the header lines */
	lc = 0;
	while (fgets(linebuf, LINESIZE, fp) != NULL && linebuf[0] != '\n'
	       && strncmp(linebuf, END_HDRS, strlen(END_HDRS)/2))
		lc++;
	rewind(fp);
	initheader(hp);
	do {
		if ((lc = gethfield(fp, linebuf, lc)) < 0)
			break;
		if (linebuf == NOSTR)
			continue;
		if (ishfield(linebuf, "to"))
			hp->h_to = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "subject"))
			hp->h_subject = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "cc"))
			hp->h_cc = savestr(hcontents(linebuf));
		else if (ishfield(linebuf,"bcc"))
			hp->h_bcc = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "reply-to"))
			hp->h_replyto = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "return-receipt-to"))
			hp->h_rtnrcpt = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "in-reply-to"))
			hp->h_inreplyto = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "keywords"))
			hp->h_keywords = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "references"))
			hp->h_references = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "comments"))
			hp->h_comments = savestr(hcontents(linebuf));
		else if (ishfield(linebuf, "encrypted"))
			hp->h_encrypt = savestr(hcontents(linebuf));
		else {
			for (idx = 0; idx < MAX_EX_HDRS; idx++) {
				if (hp->h_ex_hdrs[idx] != NOSTR)
					continue;
				hp->h_ex_hdrs[idx] = savestr(linebuf);
				break;
			}
		}
	} while (lc > 0);

	/*
	 * fp now points to the start of the message body.
	 * Copy the remainder into the temp file, and then rename.
	 */
	lc = 0;
	while (fgets(linebuf, LINESIZE, fp) != NULL) {
		if (!lc++ && (linebuf[0] == '\n' ||
		              !strncmp(linebuf, END_HDRS, strlen(END_HDRS)/2)))
			continue;
		fprintf(tfp, "%s", linebuf);
	}
	if (ferror(tfp)) {
		fclose(fp);
		fclose(tfp);
		return(-1);
	}
	fclose(fp);
	if (rename(tfname, fname) < 0)
		return(-1);
	fclose(tfp);
	return(0);
}

/*
 * Compare two strings, ignoring case.
 */

icequal(s1, s2)
	register char *s1, *s2;
{

	while (toupper(*s1++) == toupper(*s2))
		if (*s2++ == 0)
			return(1);
	return(0);
}

/*
 * Copy a string, lowercasing it as we go.
 */
istrcpy(dest, src, size)
	char *dest, *src;
	int size;
{
	register char *cp, *cp2, *max;

	cp2 = dest;
	cp = src;
	max = dest + size - 1;
	while (cp2 <= max && *cp != '\0') {
		if (*cp >= 'A' && *cp <= 'Z')
			*cp2++ = *cp + 'a' - 'A';
		else
			*cp2++ = *cp;
		cp++;
	}
}

/*
 * The following code deals with input stacking to do source
 * commands.  All but the current file pointer are saved on
 * the stack.
 */

static	int	ssp = -1;		/* Top of file stack */
struct sstack {
	FILE	*s_file;		/* File we were in. */
	int	s_cond;			/* Saved state of conditionals */
	int	s_loading;		/* Loading .mailrc, etc. */
} sstack[NOFILE];

/*
 * Pushdown current input file and switch to a new one.
 * Set the global flag "sourcing" so that others will realize
 * that they are no longer reading from a tty (in all probability).
 */

source(name)
	char name[];
{
	register FILE *fi;
	register char *cp;

	if ((cp = expand(name)) == NOSTR)
		return(1);
	if ((fi = fopen(cp, "r")) == NULL) {
		perror(cp);
		return(1);
	}
	if (ssp >= NOFILE - 2) {
		printf("Too much \"sourcing\" going on.\n");
		fclose(fi);
		return(1);
	}
	sstack[++ssp].s_file = input;
	sstack[ssp].s_cond = cond;
	sstack[ssp].s_loading = loading;
	loading = 0;
	cond = CANY;
	input = fi;
	sourcing++;
	return(0);
}

/*
 * Pop the current input back to the previous level.
 * Update the "sourcing" flag as appropriate.
 */

unstack()
{
	if (ssp < 0) {
		printf("\"Source\" stack over-pop.\n");
		sourcing = 0;
		return(1);
	}
	fclose(input);
	if (cond != CANY)
		printf("Unmatched \"if\"\n");
	cond = sstack[ssp].s_cond;
	loading = sstack[ssp].s_loading;
	input = sstack[ssp--].s_file;
	if (ssp < 0)
		sourcing = loading;
	return(0);
}

/*
 * Touch the indicated file.
 * This is nifty for the shell.
 * If we have the utime() system call, this is better served
 * by using that, since it will work for empty files.
 * On non-utime systems, we must sleep a second, then read.
 */

alter(name)
	char name[];
{
#ifdef UTIME
	struct stat statb;
	long time();
	time_t time_p[2];
#else /* !UTIME */
	register int pid, f;
	char w;
#endif /* UTIME */

#ifdef UTIME
	if (stat(name, &statb) < 0)
		return;
	time_p[0] = time((long *) 0) + 1;
	time_p[1] = statb.st_mtime;
	utime(name, time_p);
#else /* !UTIME */
	sleep(1);
	if ((f = open(name, 0)) < 0)
		return;
	read(f, &w, 1);
	safe_exit(0);
#endif /* UTIME */
}

/*
 * Examine the passed line buffer and
 * return true if it is all blanks and tabs.
 */

blankline(linebuf)
	char linebuf[];
{
	register char *cp;

	for (cp = linebuf; *cp; cp++)
		if (!isspace(*cp))
			return(0);
	return(1);
}

/*
 * Get sender's name from this message.
 * Reptype can be
 *	0 -- get sender's name for display purposes
 *	non-zero -- get the envelope "from" sender
 */

char *
nameof(mp, reptype)
	register struct message *mp;
{
	register char *cp, *cp2;

	cp = name1(mp, reptype);
	if (cp == NOSTR)
		return(cp);
	if (reptype != 0 || charcount(cp, '!') < 2)
		return(cp);
	cp2 = strrchr(cp, '!');
	cp2--;
	while (cp2 > cp && *cp2 != '!')
		cp2--;
	if (*cp2 == '!')
		return(cp2 + 1);
	return(cp);
}

/*
 * Remove all comment fields leaving only a raw address.
 */
char *
skin(name)
	char *name;
{
	register int c;
	register char *cp, *cp2, *lastdlmpos;
	int gotlt, lastsp;
	char nbuf[BUFSIZ];
	int nesting;

	if (name == NOSTR)
		return(NOSTR);
	if (strchr(name, '(') == NULL && strchr(name, '<') == NULL
	&& strchr(name, ' ') == NULL)
		return(name);
	gotlt = 0;
	lastsp = 0;
	for (cp = name, cp2 = nbuf, lastdlmpos = nbuf; c = *cp++; ) {
		switch (c) {
		case '\"':
			while (*cp != '\0') {
				switch (*cp++) {
				case '\"':
				case '<':
					c = *cp;
					goto endquote;
				}
			}
			c = *cp;
endquote:
			break;

		case '(':
			nesting = 1;
			while (*cp != '\0') {
				switch (*cp++) {
				case '(':
					nesting++;
					break;

				case ')':
					--nesting;
					break;
				}

				if (nesting <= 0)
					break;
			}
			lastsp = 0;
			break;

		case ' ':
			if (cp[0] == 'a' && cp[1] == 't' && cp[2] == ' ')
				cp += 3, *cp2++ = '@';
			else
			if (cp[0] == '@' && cp[1] == ' ')
				cp += 2, *cp2++ = '@';
			else 
				lastsp = 1;
			break;

		case ',':
			*cp2++ = c;
			lastdlmpos = cp2;
			break;

		case '<':
			cp2 = lastdlmpos;
			gotlt++;
			lastsp = 0;
			break;

		case '>':
			if (gotlt)
				gotlt = 0;
			break;

		default:
			if (lastsp) {
				lastsp = 0;
				*cp2++ = ' ';
			}
			*cp2++ = c;
			break;
		}
	}
	*cp2 = 0;

	return(savestr(nbuf));
}

int
ispure822(address)
	char *address;
{
	int ats;

	if (address == NOSTR
	    || (ats = charcount(address, '@')) > 1
	    || ats < 1
	    || strchr(address, '!') != NULL)
		return(0);
	return(1);
}

extern int parse();

/*
 * Fetch the sender's name from the passed message.
 * Reptype can be
 *	0 -- get sender's name for display purposes
 *	non-zero -- get the envelope "from" sender
 */

char *
name1(mp, reptype)
	register struct message *mp;
{
	struct headline hl;
	char namebuf[LINESIZE];
	char linebuf[LINESIZE];
	register char *cp, *cp2, *from, *sender;
	register FILE *ibuf;
	int first = 1;

	if ((from = hfield("from", mp)) != NOSTR && reptype == 0)
		return(from);
	if ((sender = hfield("sender", mp)) != NOSTR && reptype == 0)
		return(sender);

	/*
	 * Now determine the envelope "from".
	 */
	ibuf = setinput(mp);
	copy("", namebuf, LINESIZE);
	if (readline(ibuf, linebuf) <= 0)
		return(NOSTR);
#ifdef sgi
	if (parse(linebuf, &hl, namebuf)) {
		if (hl.l_from != NOSTR) {
			strncpy(namebuf, hl.l_from, LINESIZE);
			namebuf[LINESIZE - 1] = '\0';
		} else
			strcpy(namebuf, "");
	}
	else
		strcpy(namebuf, "");
#else /* !sgi */
newname:
	for (cp = linebuf; *cp != ' '; cp++)
		;
	while (isspace(*cp))
		cp++;
	for (cp2 = &namebuf[strlen(namebuf)]; *cp && !isspace(*cp) &&
	    cp2-namebuf < LINESIZE-1; *cp2++ = *cp++)
		;
	*cp2 = '\0';
	if (readline(ibuf, linebuf) <= 0)
		return(savestr(namebuf));
	if ((cp = strchr(linebuf, 'F')) == NULL)
		return(savestr(namebuf));
	if (strncmp(cp, "From", 4) != 0)
		return(savestr(namebuf));
	while ((cp = strchr(cp, 'r')) != NULL) {
		if (strncmp(cp, "remote", 6) == 0) {
			if ((cp = strchr(cp, 'f')) == NULL)
				break;
			if (strncmp(cp, "from", 4) != 0)
				break;
			if ((cp = strchr(cp, ' ')) == NULL)
				break;
			cp++;
			if (first) {
				copy(cp, namebuf, LINESIZE);
				first = 0;
			} else {
				strncpy(strrchr(namebuf, '!')+1, cp, LINESIZE);
				namebuf[LINESIZE - 1] = '\0';
			}
			if(strlen(linebuf) <= LINESIZE - 2)
				strcat(namebuf, "!");
			goto newname;
		}
		cp++;
	}
#endif /* sgi */

	/*
	 * namebuf holds the envelope "from" address
	 */

	/*
	 * sender == address in the RFC 822 "Sender" header.
	 * from == address in the RFC 822 "From:" header.
	 * namebuf == address in the envelope "From" line.
	 *
	 * Now we try to be "clever" about which address is the most
	 * reliable to use.
	 *
	 * If the "From:" header is a "pure" RFC 822-style domain address
	 * (contains 1 '@' and no '!'s) we will use it.  (See ispure822();)
	 * Otherwise, we will use the envelope "From" address since it is
	 * considered more reliable.
	 *
	 * Note: This behaviour is debatable.  Being "clever" usually causes
	 * trouble. 
	 * 
	 * REMIND:  Might want to make various behaviours configurable!
	 */

	if (ispure822(from))
		return(from);
	if (strlen(namebuf) != 0)
		return(savestr(namebuf));
	if (from != NOSTR && strlen(from) != 0)
		return(from);
	return(NOSTR);
}

/*
 * Count the occurances of c in str
 */
charcount(str, c)
	char *str;
{
	register char *cp;
	register int i;

	for (i = 0, cp = str; *cp; cp++)
		if (*cp == c)
			i++;
	return(i);
}

/*
 * Are any of the characters in the two strings the same?
 */

anyof(s1, s2)
	register char *s1, *s2;
{
	register int c;

	while (c = *s1++)
		if (any(c, s2))
			return(1);
	return(0);
}

/*
 * See if the given header field is supposed to be ignored.
 */
isign(field)
	char *field;
{
	char realfld[BUFSIZ];
	char *cp;
	int i;

	if ((cp = strchr(field, ':')))
		*cp = '\0';
	/*
	 * Lower-case the string, so that "Status" and "status"
	 * will hash to the same place.
	 */
	for(i=0; i<BUFSIZ; i++)
		realfld[i] = 0;
	istrcpy(realfld, field, BUFSIZ);
	realfld[BUFSIZ-1]='\0';
	if (cp)
		*cp = ':';
	if (nretained > 0)
		return (!member(realfld, retain));
	else
		return (member(realfld, ignore));
}

member(realfield, table)
	register char *realfield;
	register struct ignore **table;
{
	register struct ignore *igp;

	for (igp = table[hash(realfield)]; igp != 0; igp = igp->i_link)
		if (equal(igp->i_field, realfield))
			return (1);

	return (0);
}

#define constrlen(s) (sizeof(s) - 1)

getheadlen(hp, ncols)
	struct header *hp;
	int ncols;
{
	int nlines, ncols2, gotcha;

	nlines = gotcha = 0;
	if (hp->h_to != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("To: ");
		nlines = (strlen(hp->h_to) + ncols2 - 1) / ncols2;
	}
	if (hp->h_subject != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Subject: ");
		nlines = (strlen(hp->h_subject) + ncols2 - 1) / ncols2;
	}
	if (hp->h_cc != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Cc: ");
		nlines = (strlen(hp->h_cc) + ncols2 - 1) / ncols2;
	}
	if (hp->h_bcc != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Bcc: ");
		nlines = (strlen(hp->h_bcc) + ncols2 - 1) / ncols2;
	}
	if (hp->h_replyto != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Reply-To: ");
		nlines = (strlen(hp->h_replyto) + ncols2 - 1) / ncols2;
	}
	if (hp->h_rtnrcpt != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Return-Receipt-To: ");
		nlines = (strlen(hp->h_rtnrcpt) + ncols2 - 1) / ncols2;
	}
	if (hp->h_inreplyto != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("In-Reply-To: ");
		nlines = (strlen(hp->h_inreplyto) + ncols2 - 1) / ncols2;
	}
	if (hp->h_references != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("References: ");
		nlines = (strlen(hp->h_references) + ncols2 - 1) / ncols2;
	}
	if (hp->h_keywords != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Keywords: ");
		nlines = (strlen(hp->h_keywords) + ncols2 - 1) / ncols2;
	}
	if (hp->h_comments != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Comments: ");
		nlines = (strlen(hp->h_comments) + ncols2 - 1) / ncols2;
	}
	if (hp->h_encrypt != NOSTR) {
		gotcha++;
		ncols2 = ncols - constrlen("Encrypted: ");
		nlines = (strlen(hp->h_encrypt) + ncols2 - 1) / ncols2;
	}
	if (gotcha)
		nlines++;
	return(nlines);
}

initheader(hp)
	struct header *hp;
{
	register int idx;

	hp->h_to = NOSTR;
	hp->h_subject = NOSTR;
	hp->h_cc = NOSTR;
	hp->h_bcc = NOSTR;
	hp->h_replyto = NOSTR;
	hp->h_rtnrcpt = NOSTR;
	hp->h_inreplyto = NOSTR;
	hp->h_keywords = NOSTR;
	hp->h_references = NOSTR;
	hp->h_comments = NOSTR;
	hp->h_encrypt = NOSTR;
	for (idx = 0; idx < MAX_EX_HDRS; idx++)
		hp->h_ex_hdrs[idx] = NOSTR;
        hp->h_seq = hp->h_optusedmask = 0;
}

gettextlen(fp, ncols)
	FILE *fp;
	int ncols;
{
	int c, ccnt, nlines;

	nlines = 0;
	ccnt = 0;
	while ((c = getc(fp)) != EOF) {
		if (c == '\n') {
			nlines++;
			ccnt = 0;
		}
		else {
			ccnt++;
			if (ccnt > ncols) {
				nlines++;
				ccnt = 0;
			}
		}
	}
	rewind(fp);
	return(nlines);
}

pmsglist(msgvec)
	int *msgvec;
{
	while (*msgvec != 0) {
		printf("%d ", *msgvec);
		msgvec++;
	}
}


int
getwinsize(ws, page) 
	struct winsize *ws;
	int page;

{
	if (ioctl(fileno(stdout), TIOCGWINSZ, ws) < 0) {
		if (!page)
			ws->ws_row = DEFAULTROWS;
		else
			ws->ws_row = 0;
		ws->ws_col = DEFAULTCOLS;
	}
	else {
		if (ws->ws_row == 0 && !page) 
			ws->ws_row = DEFAULTROWS;
		if (ws->ws_col == 0)
			ws->ws_col = DEFAULTCOLS;
	}
}

int
getwinlines(page)
	int page;
{
	char *cp;
	int nwlines;
	struct winsize ws;

	if (!page) {
		cp = value("crt");
		if (cp == NOSTR)
			return (MAXINT);
		if (sscanf(cp, "%d", &nwlines) != EOF)
			return (nwlines);
	}
	getwinsize(&ws, page);
	return (ws.ws_row);
}

int
getwincols()
{
	struct winsize ws;

	getwinsize(&ws, 0);
	return (ws.ws_col);
}

#define TMPBUFNAME ".MailTempXXXXXX"

/*
 * mktmpfil (make temporary file) works similarly to mkstemp(3C) but
 * returns a pointer to an open-unlinked FILE.  The FILE is open for
 * read and write.
 */

FILE *
mktmpfil()
{
	FILE *fp;
	int fd;
	char name[PATH_MAX];
	extern char tempRoot[];

	snprintf(name, PATH_MAX, "%s/%s", tempRoot, TMPBUFNAME);
	if ((fd = mkstemp(name)) < 0) {
		printf("Cannot make temp file\n");
		perror("mktmpfil");
		fflush(stdout);
		return(NULL);
	}
	if ((fp = fdopen(fd, "w+")) == NULL)
		return(NULL);
	if (unlink(name) < 0)
		return(NULL);
	return(fp);
}

/*
 * mkntmpfil (make named temporary file)  works similarly to mktmpfil
 * above but the returned FILE is not unlinked, and the name is returned
 * to the caller.
 */

FILE *
mkntmpfil(name)
	char *name;
{
	FILE *fp;
	int fd;
	extern char tempRoot[];

	snprintf(name, PATH_MAX, "%s/%s", tempRoot, TMPBUFNAME);
	if ((fd = mkstemp(name)) < 0) {
		printf("Cannot make temp file\n");
		perror("mkntmpfil");
		fflush(stdout);
		return(NULL);
	}
	if ((fp = fdopen(fd, "w+")) == NULL)
		return(NULL);
	return(fp);
}
