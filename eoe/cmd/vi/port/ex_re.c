/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* Copyright (c) 1981 Regents of the University of California */
#ident	"@(#)vi:port/ex_re.c	1.21.1.2"

#include "ex.h"
#include "ex_re.h"
#include <string.h>

char *vbraslist[NBRA+1];
char *vbraelist[NBRA+1];
char	*vlocs;
char    *vloc1;      /* Where re began to match (in linebuf) */
char    *vloc2;      /* First char after re match (") */


/*
 * Global, substitute and regular expressions.
 * Very similar to ed, with some re extensions and
 * confirmed substitute.
 */
global(k)
	bool k;
{
	register unsigned char *gp;
	register int c;
	register line *a1;
	unsigned char globuf[GBSIZE], *Cwas;
	int nlines = lineDOL();
	int oinglobal = inglobal;
	unsigned char *oglobp = globp;

	Cwas = Command;
	/*
	 * States of inglobal:
	 *  0: ordinary - not in a global command.
	 *  1: text coming from some buffer, not tty.
	 *  2: like 1, but the source of the buffer is a global command.
	 * Hence you're only in a global command if inglobal==2. This
	 * strange sounding convention is historically derived from
	 * everybody simulating a global command.
	 */
	if (inglobal==2)
		error(":129",
			"Global within global|Global within global not allowed");
	markDOT();
	setall();
	nonzero();
	if (skipend())
		error(":130", 
			"Global needs re|Missing regular expression for global");
	c = getchar();
	(void)vi_compile(c, 1);
	savere(scanre);
	gp = globuf;
	while ((c = getchar()) != '\n') {
		switch (c) {

		case EOF:
			c = '\n';
			goto brkwh;

		case '\\':
			c = getchar();
			switch (c) {

			case '\\':
				ungetchar(c);
				break;

			case '\n':
				break;

			default:
				*gp++ = '\\';
				break;
			}
			break;
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE - 2])
			error(":131", "Global command too long");
	}
brkwh:
	ungetchar(c);
out:
	donewline();
	*gp++ = c;
	*gp++ = 0;
	saveall();
	inglobal = 2;
	for (a1 = one; a1 <= dol; a1++) {
		*a1 &= ~01;
		if (a1 >= addr1 && a1 <= addr2 && execute(0, a1) == k)
			*a1 |= 01;
	}
#ifdef notdef
/*
 * This code is commented out for now.  The problem is that we don't
 * fix up the undo area the way we should.  Basically, I think what has
 * to be done is to copy the undo area down (since we shrunk everything)
 * and move the various pointers into it down too.  I will do this later
 * when I have time. (Mark, 10-20-80)
 */
	/*
	 * Special case: g/.../d (avoid n^2 algorithm)
	 */
	if (globuf[0]=='d' && globuf[1]=='\n' && globuf[2]=='\0') {
		gdelete();
		return;
	}
#endif
	if (inopen)
		inopen = -1;
	/*
	 * Now for each marked line, set dot there and do the commands.
	 * Note the n^2 behavior here for lots of lines matching.
	 * This is really needed: in some cases you could delete lines,
	 * causing a marked line to be moved before a1 and missed if
	 * we didn't restart at zero each time.
	 */
	for (a1 = one; a1 <= dol; a1++) {
		if (*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands(1, 1);
			a1 = zero;
		}
	}
	globp = oglobp;
	inglobal = oinglobal;
	endline = 1;
	Command = Cwas;
	netchHAD(nlines);
	setlastchar(EOF);
	if (inopen) {
		ungetchar(EOF);
		inopen = 1;
	}
}

/*
 * gdelete: delete inside a global command. Handles the
 * special case g/r.e./d. All lines to be deleted have
 * already been marked. Squeeze the remaining lines together.
 * Note that other cases such as g/r.e./p, g/r.e./s/r.e.2/rhs/,
 * and g/r.e./.,/r.e.2/d are not treated specially.  There is no
 * good reason for this except the question: where to you draw the line?
 */
gdelete()
{
	register line *a1, *a2, *a3;

	a3 = dol;
	/* find first marked line. can skip all before it */
	for (a1=zero; (*a1&01)==0; a1++)
		if (a1>=a3)
			return;
	/* copy down unmarked lines, compacting as we go. */
	for (a2=a1+1; a2<=a3;) {
		if (*a2&01) {
			a2++;		/* line is marked, skip it */
			dot = a1;	/* dot left after line deletion */
		} else
			*a1++ = *a2++;	/* unmarked, copy it */
	}
	dol = a1-1;
	if (dot>dol)
		dot = dol;
	change();
}

bool	cflag;
int	scount, slines, stotal;

substitute(c)
	int c;
{
	register line *addr;
	register int n;
	int gsubf, hopcount;

	gsubf = compsub(c);
	if(FIXUNDO)
		save12(), undkind = UNDCHANGE;
	stotal = 0;
	slines = 0;
	for (addr = addr1; addr <= addr2; addr++) {
		scount = hopcount = 0;
		if (dosubcon(0, addr) == 0)
			continue;
		if (gsubf) {
			/*
			 * The loop can happen from s/\</&/g
			 * but we don't want to break other, reasonable cases.
			 */
			hopcount = 0;
			while (*vloc2) {
				if (++hopcount > sizeof linebuf)
					error(":132", "substitution loop");
				if (dosubcon(1, addr) == 0)
					break;
			}
		}
		if (scount) {
			stotal += scount;
			slines++;
			putmark(addr);
			n = append(getsub, addr);
			addr += n;
			addr2 += n;
		}
	}
	if (stotal == 0 && !inglobal && !cflag)
		error(":133", "Fail|Substitute pattern match failed");
	snote(stotal, slines);
	return (stotal);
}

compsub(ch)
{
	register int seof, c, uselastre; 
	static int gsubf;
	static unsigned char remem[RHSSIZE];
	static int remflg = -1;

	if (!value(vi_EDCOMPATIBLE))
		gsubf = cflag = 0;
	uselastre = 0;
	switch (ch) {

	case 's':
		(void)skipwh();
		seof = getchar();
		if (endcmd(seof) || any(seof, "gcr")) {
			ungetchar(seof);
			goto redo;
		}
		if (isalpha(seof) || isdigit(seof))
			error(":134", 
				"Substitute needs re|Missing regular expression for substitute");
		seof = vi_compile(seof, 1);
		uselastre = 1;
		comprhs(seof);
		if (!value(vi_EDCOMPATIBLE))
			gsubf = cflag = 0;
		break;

	case '~':
		uselastre = 1;
		/* fall into ... */
	case '&':
	redo:
		if (re.nore)
			error(EXmnopreid, EXmnopre);
		if (subre.nore)
			error(":135", 
				"No previous substitute re|No previous substitute to repeat");
		break;
	}
	for (;;) {
		c = getchar();
		switch (c) {

		case 'g':
			gsubf = !gsubf;
			continue;

		case 'c':
			cflag = !cflag;
			continue;

		case 'r':
			uselastre = 1;
			continue;

		default:
			ungetchar(c);
			setcount();
			donewline();
			if (uselastre)
				savere(subre);
			else
				resre(subre);

			/*
			 * The % by itself on the right hand side means
			 * that the previous value of the right hand side
			 * should be used. A -1 is used to indicate no
			 * previously remembered search string.
			 */

			if (rhsbuf[0] == '%' && rhsbuf[1] == 0)
				if (remflg == -1)
					error(":136", 
						"No previously remembered string");
			        else
					strcpy(rhsbuf, remem);
			else {
				strcpy(remem, rhsbuf);
				remflg = 1;
			}
			return (gsubf);
		}
	}
}

comprhs(seof)
	int seof;
{
	register unsigned char *rp, *orp;
	register int c;
	unsigned char orhsbuf[RHSSIZE];

	rp = rhsbuf;
	CP(orhsbuf, rp);
	for (;;) {
		c = getchar();
		if (c == seof)
			break;
		switch (c) {

		case '\\':
			c = getchar();
			if (c == EOF)
				error(":137", "Replacement string ends with \\");
			if (value(vi_MAGIC)) {
				/*
				 * When "magic", \& turns into a plain &,
				 * and all other chars work fine quoted.
				 */
				if (c != '&') {
					if(rp >= &rhsbuf[RHSSIZE - 1]) {
						*rp=0;
						error(EXmreppatid, EXmreppat);
					}
					*rp++ = '\\';
				}
				break;
			}
magic:
			if (c == '~') {
				for (orp = orhsbuf; *orp; *rp++ = *orp++)
					if (rp >= &rhsbuf[RHSSIZE - 1])
						goto toobig;
				continue;
			}
			if(rp >= &rhsbuf[RHSSIZE - 1]) {
				*rp=0;
				error(EXmreppatid, EXmreppat);
			}
			*rp++ = '\\';
			break;

		case '\n':
		case EOF:
			if (!(globp && globp[0])) {
				ungetchar(c);
				goto endrhs;
			}

		case '~':
		case '&':
			if (value(vi_MAGIC))
				goto magic;
			break;
		}
		if (rp >= &rhsbuf[RHSSIZE - 1]) {
toobig:
			*rp = 0;
			error(EXmreppatid, EXmreppat);
		}
		*rp++ = c;
	}
endrhs:
	*rp++ = 0;
}

getsub()
{
	register unsigned char *p;

	if ((p = linebp) == 0)
		return (EOF);
	strcLIN(p);
	linebp = 0;
	return (0);
}

dosubcon(f, a)
	bool f;
	line *a;
{

	if (execute(f, a) == 0)
		return (0);
	if (confirmed(a)) {
		dosub();
		scount++;
	}
	return (1);
}

confirmed(a)
	line *a;
{
	register int c, cnt, ch;

	if (cflag == 0)
		return (1);
	pofix();
	pline(lineno(a));
	if (inopen)
		putchar('\n' | QUOTE);
	c = lcolumn(vloc1);
	ugo(c - 1 + (inopen ? 1 : 0), ' ');
	ugo(lcolumn(vloc2) - c, '^');
	flush();
	cnt = 0;
bkup:	
	ch = c = getkey();
again:
	if (c == '\b') {
		if ((inopen)
		 && (cnt > 0)) {
			putchar('\b' | QUOTE);
			putchar(' ');
			putchar('\b' | QUOTE), flush();
			cnt --;
		} 
		goto bkup;
	}
	if (c == '\r')
		c = '\n';
	if (inopen && MB_CUR_MAX == 1 || c < 0200) {
		putchar(c);
		flush();
		cnt++;
	}
	if (c != '\n' && c != EOF) {
		c = getkey();
		goto again;
	}
	noteinp();
	return (ch == 'y');
}

ugo(cnt, with)
	int with;
	int cnt;
{

	if (cnt > 0)
		do
			putchar(with);
		while (--cnt > 0);
}

int	casecnt;
bool	destuc;

dosub()
{
	register unsigned char *lp, *sp, *rp;
	int c;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < (unsigned char *)vloc1)
		*sp++ = *lp++;
	casecnt = 0;
	/*
	 * Caution: depending on the hardware, c will be either sign
	 * extended or not if C&QUOTE is set.  Thus, on a VAX, c will
	 * be < 0, but on a 3B, c will be >= 128.
	 */
	while (c = *rp++) {
		/* ^V <return> from vi to split lines */
		if (c == '\r')
			c = '\n';

		if (c == '\\') {
			switch ((c = *rp++)) {

			case '&':
				sp = place(sp, vloc1, vloc2);
				if (sp == 0)
					goto ovflo;
				continue;

			case 'l':
				casecnt = 1;
				destuc = 0;
				continue;

			case 'L':
				casecnt = LBSIZE;
				destuc = 0;
				continue;

			case 'u':
				casecnt = 1;
				destuc = 1;
				continue;

			case 'U':
				casecnt = LBSIZE;
				destuc = 1;
				continue;

			case 'E':
			case 'e':
				casecnt = 0;
				continue;
			}
			if(c >= '1' && c < re.Nbra + '1') {
				sp = place(sp, vbraslist[c - '1'] , vbraelist[c - '1']);
				if (sp == 0)
					goto ovflo;
				continue;
			}
		}
		if (casecnt)
			*sp++ = fixcase(c);
		else
			*sp++ = c;
		if (sp >= &genbuf[LBSIZE])
ovflo:
			error(":138",
				"Line overflow|Line overflow in substitute");
	}
	lp = (unsigned char *)vloc2;
	vloc2 = (char *)(linebuf + (sp - genbuf));
	while (*sp++ = *lp++)
		if (sp >= &genbuf[LBSIZE])
			goto ovflo;
	strcLIN(genbuf);
}

fixcase(c)
	register int c;
{

	if (casecnt == 0)
		return (c);
	casecnt--;
	if (destuc) {
		if (islower(c))
			c = toupper(c);
	} else
		if (isupper(c))
			c = tolower(c);
	return (c);
}

unsigned char *
place(sp, l1, l2)
	register unsigned char *sp, *l1, *l2;
{

	while (l1 < l2) {
		*sp++ = fixcase(*l1++);
		if (sp >= &genbuf[LBSIZE])
			return (0);
	}
	return (sp);
}

snote(total, nlines)
	register int total, nlines;
{

	if (!notable(total))
		return;
	if (total == 1)
		mprintf(":139", "1 sub|1 substitution");
	else
		mprintf(":140", "%d subs|%d substitutions", total);
	if (nlines != 1 && nlines != total)
		gprintf(":141", " on %d lines", nlines);
	noonl();
	flush();
}

vi_compile(eof, oknl)
	int eof;
	int oknl;
{
	register int c;
	register unsigned char *gp, *p1;
	unsigned char *rhsp;
	unsigned char rebuf[LBSIZE];
	int	re_flag = 0;
	int	ret;
	unsigned	char	*bstr;

	gp = genbuf;
	if (isalpha(eof) || isdigit(eof))
error(":142", "Regular expressions cannot be delimited by letters or digits");
	if(eof >= 0200 && MB_CUR_MAX > 1)
error(":143", "Regular expressions cannot be delimited by multibyte characters");
	c = getchar();
	if (eof == '\\')
		switch (c) {

		case '/':
		case '?':
			if (scanre.nore)
error(":144", "No previous scan re|No previous scanning regular expression");
			resre(scanre);
			return (c);

		case '&':
			if (subre.nore)
error(":145", "No previous substitute re|No previous substitute regular expression");
			resre(subre);
			return (c);

		default:
error(":146", "Badly formed re|Regular expression \\ must be followed by / or ?");
		}
	if (c == eof || c == '\n' || c == EOF) {
		if (re.nore)
			error(EXmnopreid, EXmnopre);
		if (c == '\n' && oknl == 0)
error(":147", "Missing closing delimiter|Missing closing delimiter for regular expression");
		if (c != eof)
			ungetchar(c);
		return (eof);
	}
	gp = genbuf;
	if (c == '^') {
		*gp++ = c;
		c = getchar();
	}
	ungetchar(c);
	for (;;) {
		c = getchar();
		if (c == eof || c == EOF) {
			if (c == EOF)
				ungetchar(c);
			goto out;
		}
		if (gp >= &genbuf[LBSIZE - 3])
complex:
cerror(":148", "Re too complex|Regular expression too complicated");
		switch (c) {

		case '\\':
			c = getchar();
			switch (c) {

			case '(':
			case ')':
			case '<':
			case '>':
			case '{':
			case '}':
			case '$':
			case '^':
			case '\\':
				*gp++ = '\\';
				*gp++ = c;
				continue;
			
			case 'n':
				*gp++ = c;
				continue;
			}
			if(c >= '0' && c <= '9') {
				*gp++ = '\\';
				*gp++ = c;
				continue;
			}
			if (value(vi_MAGIC) == 0)
magic:
			switch (c) {

			case '.':
				*gp++ = '.';
				continue;

			case '~':
				rhsp = rhsbuf;
				while (*rhsp) {
					if (*rhsp == '\\') {
						c = *++rhsp;
						if (c == '&')
cerror(":149", "Replacement pattern contains &|Replacement pattern contains & - cannot use in re");
						if (c >= '1' && c <= '9')
cerror(":150", "Replacement pattern contains \\d|Replacement pattern contains \\d - cannot use in re");
						if(any(c, ".\\*[$"))
							*gp++ = '\\';
					}
					if (gp >= &genbuf[LBSIZE-2])
						goto complex;
					c = *rhsp++;
					*gp++ = c;
				}
				continue;

			case '*':
				*gp++ = '*';
				continue;

			case '[':
				*gp++ = '[';
				c = getchar();
				if (c == '^') {
					*gp++ = '^';
					c = getchar();
				}
				do { 
					if (gp >= &genbuf[LBSIZE-4])
						goto complex;
					if(c == '\\' && peekchar() == ']') {
						(void)getchar();
						*gp++ = '\\';
						*gp++ = ']';
					}
					else if (c == '\n' || c == EOF)
						cerror(":151", "Missing ]");
					else
						*gp++ = c;
					c = getchar();
				} while(c != ']');
				*gp++ = ']';
				continue;
			}
			if (c == EOF) {
				ungetchar(EOF);
				*gp++ = '\\';
				*gp++ = '\\';
				continue;
			}
			if (c == '\n')
cerror(":152", "No newlines in re's|Cannot escape newlines into regular expressions");
			*gp++ = '\\';
			*gp++ = c;
			continue;

		case '\n':
			if (oknl) {
				ungetchar(c);
				goto out;
			}
cerror(":153", "Badly formed re|Missing closing delimiter for regular expression");

		case '.':
		case '~':
		case '*':
		case '[':
			if (value(vi_MAGIC))
				goto magic;
			if(c != '~')
				*gp++ = '\\';
defchar:
		default:
			*gp++ = c;
			continue;
		}
	}
out:
	*gp++ = '\0';
	/* word boundary search */
	while(bstr = strstr(genbuf, "\\<")) {
		unsigned char tmpbuf[LBSIZE];
		char    wbow[] = "[[:<:]]"; /* Map to regcomp() syntax */
		register int    i;

		strcpy(tmpbuf,genbuf);
		i = bstr - genbuf;
		strcpy(&tmpbuf[i], wbow);
		strcat(tmpbuf,&genbuf[i+2]);
		strcpy(genbuf,tmpbuf);
	}
	while(bstr = strstr(genbuf, "\\>")) {
		unsigned char tmpbuf[LBSIZE];
		char	weow[] = "[[:>:]]";	/* Map to regcomp() syntax */
		register int    i;

		strcpy(tmpbuf,genbuf);
		i = bstr - genbuf;
		strcpy(&tmpbuf[i], weow);
		strcat(tmpbuf,&genbuf[i+2]);
		strcpy(genbuf,tmpbuf);
	}

	re.nore = 0;
	if (value(vi_IGNORECASE))
		re_flag |= REG_ICASE;
	ret=regcomp(&re.preg, genbuf, re_flag);
	if(ret) {
		char msg[REG_MSGLEN];

		re.nore = 1;
		(void)regerror(ret, &re.preg, msg, REG_MSGLEN);
		cerror(":156", msg);
	}

	re.Nbra = re.preg.re_nsub;
	return(eof);
}

cerror(t, s)
        unsigned char *s, *t;
{
	re.nomatch = 0;
	re.nore = 0;
	error (t, s);
}

execute(gf, addr)
	line *addr;
{
	register unsigned char *p1, *p2;
	char *start;
	register int c, i;
	regmatch_t  pmatch[NBRA+1];
	size_t  nmatch = (size_t)NBRA;
	int	re_flag = 0;
	int ret;

	if (gf) {
		if (re.nomatch)
			return (0);
		p1 = (unsigned char *)vloc2;
		vlocs = vloc2;
		re_flag = REG_NOTBOL;
	} else {
		if (addr == zero)
			return (0);
		p1 = linebuf;
		getline(*addr);
		vlocs = (char *)0;
	}
	re.nomatch = 0;
	ret=regexec(&re.preg, (char *)p1, (size_t)nmatch, pmatch, re_flag);
	if (ret && (ret == REG_NOMATCH))
		re.nomatch = 1;
	if (!ret) {
		vloc1 = p1 + pmatch[0].rm_so;
		vloc2 = p1 + pmatch[0].rm_eo;
		for (i = 0; i < NBRA; i++) {
			if (i < re.Nbra) {
				vbraslist[i] = p1 + pmatch[i+1].rm_so;
				vbraelist[i] = p1 + pmatch[i+1].rm_eo;
			} else {
				vbraslist[i] = NULL;
				vbraelist[i] = NULL;
			}
		}
	}
	return !ret;
}
