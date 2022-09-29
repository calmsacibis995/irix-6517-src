/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)col:col.c	1.7.2.1"
/*	col - filter reverse carraige motions
 *
 *
 */


# include <stdio.h>
# include <ctype.h>
# include <widec.h>
# include <wctype.h>
# include <locale.h>
# include <pfmt.h>

# define PL 256
# define ESC '\033'
# define RLF '\013'
# define SI '\017'
# define SO '\016'
# define LINELN 4096

/* WARNING: The 31st bit MSB of a wchar_t which is a long here is assumed
   to be free as per every encoding. So that bit is used to mark places
   were a SI/SO are begin encountered. [A trick learnt from 'vi' */
/* # define GREEK 0200 */
#define GREEK   020000000000
#define Japanese 1	/* it need to be turn off when incident #141204 is closed */

wchar_t *page[PL];
wchar_t lbuff [ LINELN ] , *line;
char esc_chars, underline, temp_off, smart;
int bflag, xflag, fflag, pflag;
int half;
int cp, lp;
int ll, llh, mustwr;
int pcp = 0;
char *pgmname;
char *strcpy();

void
usage()
{
	pfmt(stderr, MM_ACTION, ":1:Usage: %s [-bfpx]\n", pgmname);
	exit(2);
}

main (argc, argv)
	int argc; char **argv;
{
	int lastwasspec;
	int i;
	wchar_t greek;
	wchar_t c;
	static char fbuff[BUFSIZ];
	int prev_bs=0, prev_db=0;

	setlocale(LC_ALL, "");
	setbuf (stdout, fbuff);
	pgmname = argv[0];
	(void)setcat("uxdfm");
	(void)setlabel("UX:col");
	for (i = 1; i < argc; i++) {
/*		register char *p; */
		char *p;
		if (*argv[i] != '-') {
			pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
			usage ();
		}
		for (p = argv[i]+1; *p; p++) {
			switch (*p) {
			case 'b':
				bflag++;
				break;

			case 'x':
				xflag++;
				break;

			case 'f':
				fflag++;
				break;

			case 'p':
				pflag++;
				break;

			default:
				pfmt(stderr, MM_ERROR,
					"uxlibc:1:Illegal option -- %c\n", *p);
				usage ();
			}
		}
	}

	for (ll=0; ll<PL; ll++)
		page[ll] = 0;

	smart = temp_off = underline = esc_chars = '\0';
	cp = 0;
	ll = 0;
	greek = 0;
	mustwr = PL;
	line = lbuff;
	/*
	 * by keeping track of special characters that do not print
	 * such as SI/SO - we can be sure that we don't
	 * back up over them. This permits sequences such as:
	 * SI^HSI^H - to work properly
	 */
	lastwasspec = 0;

	while ((c = getwchar()) != EOF) {
				if (underline && temp_off && c > ' ') {
					outc(ESC);
					if (*line ) line++;
					*line++ = 'X';
					*line = temp_off =  '\0';
				}
		if ( c != '\b' ) {
			if ( esc_chars ) esc_chars = '\0';
			if ( wcwidth(c) > 1) 
				prev_db = 1;
			else
				prev_db = 0;
			prev_bs = 0;
			}
		switch (c) {
		case '\n':
			lastwasspec = 0;
			if (underline && !temp_off ) {
				if (*line) line++;
				*line++ = ESC;
				*line++ = 'Y';
				*line = '\0';
				temp_off = '1';
			}
			incr();
			incr();
			cp = 0;
			continue;

		case '\0':
			continue;

		case ESC:
			c = getwchar();
			switch (c) {
			case '7':	/* reverse full line feed */
				lastwasspec = 0;
				decr();
				decr();
				break;

			case '8':	/* reverse half line feed */
				lastwasspec = 0;
				if (fflag)
					decr();
				else {
					if (--half < -1) {
						decr();
						decr();
						half += 2;
					}
				}
				break;

			case '9':	/* forward half line feed */
				lastwasspec = 0;
				if (fflag)
					incr();
				else {
					if (++half > 0) {
						incr();
						incr();
						half -= 2;
					}
				}
				break;

			default:
				if (pflag)	{	/* pass through esc */
					lastwasspec = 0;
					outc(ESC);
					line++;
					*line = c ;
					line++;
					*line='\0';
					esc_chars = 1;
					if ( c == 'X')  underline = 1;
					if ( c == 'Y' && underline ) underline = temp_off = '\0';
					if ( c ==']') smart = 1;
					if ( c == '[') smart = '\0';
				} else
					lastwasspec++;
				break;
			}
			continue;

		case SO:
			lastwasspec++;
			greek = GREEK;
			continue;

		case SI:
			lastwasspec++;
			greek = 0;
			continue;

		case RLF:
			decr();
			decr();
			continue;

		case '\r':
			lastwasspec = 0;
			cp = 0;
			continue;

		case '\t':
			lastwasspec = 0;
			cp = (cp + 8) & -8;
			continue;

		case '\b':
			if ( esc_chars ) {
				*line++ = '\b';
				*line = '\0';
			}
			else if (cp > 0 && !lastwasspec) 
			 if (prev_bs && prev_db)
				prev_bs = prev_db = 0;
			 else {
				prev_bs = 1;
				cp--;
			      }
			lastwasspec = 0;
			continue;

		case ' ':
			lastwasspec = 0;
			cp++;
			continue;

		default:
			lastwasspec = 0;
			if (iswprint(c)) {	/* if printable */
				outc(c | greek);
				cp++;
			}
			continue;
		}
	}

	for (i=0; i<PL; i++)
		if (page[(mustwr+i)%PL] != 0)
			emit (page[(mustwr+i) % PL], mustwr+i-PL);
	/* Flush the output buffer. Pass a blank wide character string
	   to emit */
	lbuff[0] = ' ';
	lbuff[1] = 0;
	emit (lbuff, (llh + 1) & -2);
	return 0;
}

outc (c)
	wchar_t c;
{
	char esc_chars = '\0';
	if (lp > cp) {
		line = lbuff;
		lp = 0;
	}

	while (lp < cp) {
		if ( *line != '\b') if ( esc_chars ) esc_chars = '\0';
		switch (*line)	{ 
		case ESC:
			line++;
			esc_chars = 1;
			break;
		case '\0':
			*line = ' ';
			lp++;
			break;

		case '\b':
/*			if ( ! esc_chars ) */
				lp--;
			break;

		default:
			lp++;
		}
		line++;
	}
	while (*line == '\b') {
		line += 2;
	}
	while (*line == ESC) line += 6;
	if (bflag || *line == '\0' || *line == ' ')
		*line = c;
	else {
		/* overstrike case */
		if (smart) {
			register wchar_t c1, c2, c3, c4, c5, c6, c7;
			c1 = *++line ;
			*line++ = ESC;
			c2 = *line ;
			*line++ = '[';
			c3 = *line ;
			*line++ = '\b';
			c4 = *line ;
			*line++ = ESC;
			c5 = *line ;
			*line++ = ']';
			c6 = *line ;
			*line++ = c;
			while (c1) {
				c7 = *line;
				*line++ = c1;
				c1 = c2;
				c2 = c3;
				c3 = c4;
				c4 = c5;
				c5 = c6;
				c6 = c7;
			}
		}
		else	{
			/* insert a backspace followed by character 
			 * we would like to maintain ordering
			 */
			register wchar_t c1, c2, c3;
			while (*(line+1) == '\b')
				line += 2;
			c1 = *++line;
			*line++ = '\b';
			c2 = *line;
			*line++ = c;
			while (c1) {
				c3 = *line;
				*line++ = c1;
				c1 = c2;
				c2 = c3;
			}
		}
		lp = 0;
		line = lbuff;
	}
}

store (lno)
{
	char *malloc();

	lno %= PL;
	if (page[lno] != 0)
		free (page[lno]);
	page[lno] = (wchar_t *) malloc((wslen(lbuff) + 2)*sizeof(wchar_t));
	if (page[lno] == 0) {
/*		fprintf (stderr, "%s: no storage\n", pgmname);*/
		exit (2);
	}
	wscpy (page[lno],lbuff);
}

fetch(lno)
{
	register wchar_t *p;

	lno %= PL;
	p = lbuff;
	while (*p)
		*p++ = '\0';
	line = lbuff;
	lp = 0;
	if (page[lno])
		wscpy (line, page[lno]);
}
emit (s, lineno)
	wchar_t *s;
	int lineno;
{
	char esc_chars = '\0';
	static int cline = 0;
	register int ncp;
	register wchar_t *p;
	static int gflag = 0;

	if (*s) {
		if (gflag) {
			putchar (SI);
			gflag = 0;
		}
		while (cline < lineno - 1) {
			putchar ('\n');
			pcp = 0;
			cline += 2;
		}
		if (cline != lineno) {
			putchar (ESC);
			putchar ('9');
			cline++;
		}
		if (pcp)
			putchar ('\r');
		pcp = 0;
		p = s;
		while (*p) {
			ncp = pcp;
			while (*p++ == ' ') {
				/* Assumes that screen width of blank
				   is 1 column */
				if ((++ncp & 7) == 0 && !xflag) {
					pcp = ncp;
					putchar ('\t');
				}
			}
			if (!*--p)
				break;
			while (pcp < ncp) {
				putchar (' ');
				/* Assumes that screen width of blank
				   is 1 column */
				pcp ++;
			}
			if (gflag != (*p & GREEK) && *p != '\b') {
				if (gflag)
					putchar (SI);
				else
					putchar (SO);
				gflag ^= GREEK;
			}
			putwchar (*p & ~GREEK);
			if (*p == '\b')	{
				if ( *(p-2) && *(p-2) == ESC)
				{
					pcp++;
				}
				else {
#ifdef Japanese
					/* in japanese, kanji has two columns, we need to output another \b */
					int col;

					if ((col = scrwidth(*(p - 1) & ~GREEK)) > 1) {
					   while (col-- > 1)
					      putwchar (*p & ~GREEK);
					}
#endif /* japanese */
					pcp--;
				}
			}
			else {
				ncp = scrwidth(*p & ~GREEK);
				if (ncp <= 0)
					pcp++;
				else
					pcp += ncp;
			}
			p++;
		}
	}
}

incr()
{
	store (ll++);
	if (ll > llh)
		llh = ll;
	if (ll >= mustwr && page[ll%PL]) {
		emit (page[ll%PL], ll - PL);
		mustwr++;
		free (page[ll%PL]);
		page[ll%PL] = 0;
	}
	fetch (ll);
}

decr()
{
	if (ll > mustwr - PL) {
		store (ll--);
		fetch (ll);
	}
}
