/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cut:cut.c	1.11.1.1"
#
/* cut : cut and paste columns of a table (projection of a relation) */
/* Release 1.5; handles single backspaces as produced by nroff    */
# include <stdlib.h>
# include <stdio.h>	/* make: cc cut.c */
# include <ctype.h>
# include <locale.h>
# include <pfmt.h>
# include <errno.h>
# include <string.h>
# include <bstring.h>

# define BUFSIZE 1024	/* max no of fields or resulting line length */
# define BACKSPACE '\b'

#define MLTDUMB ' '
#include <sys/euc.h>	
#include <getwidth.h>

void
usage(int complain)
{
	if (complain)
		pfmt(stderr, MM_ERROR, ":1:Incorrect usage\n");
	pfmt(stderr, MM_ACTION,
		":137:Usage:\n\tcut [-s] [-d<char>] {-c<list> | -f<list>} file ...\n");
	exit(2);
}

void diag(char *s);
int selcheck(void);
void bytesplit(FILE *fp);
void charsplit(FILE *fp);
void fieldsplit(FILE *fp, char *del);
void dump(int nl);

int state;
int maxnum;
int low;
char	*sel;
int split_chars;
int supflag;
eucwidth_t wp;

#define STATE_START 0
#define STATE_HASNUM 1
#define	STATE_INRANGE 2
#define	STATE_HASRANGE 3

#define MID_VALUE 1
#define TERM_VALUE 2

int tty;

static char	cflist[] = ":138:Bad list for b/c/f option\n";
static char	long_line[] = ":999:Line too long, memory exhausted\n";

char *
liststate(char *p)
{
	int i;
	char c = *p;
	int num = 0;
	enum {TERM, RANGE, DIGIT} type;

	if (isdigit(c)) {
		num = atoi(p);
		type = DIGIT;
		while (isdigit(*p))
			p++;
	} else if (c == '\t' || c == ' ' || c == ',' || c == '\0') {
		type = TERM;
		p++;
	} else if (c == '-') {
		type = RANGE;
		p++;
	} else {
		diag(cflist);
		/* NOTREACHED */
	}

	switch (state) {
	case STATE_START:
		if (type == TERM)
			diag(cflist);
		if (type == DIGIT) {
			low = num;
			state = STATE_HASNUM;
		} else {
			low = 1;
			state = STATE_INRANGE;
		}
		break;
	case STATE_HASNUM:
		if (type == DIGIT)
			diag(cflist);
		if (type == TERM) {
			sel[low] = TERM_VALUE;
			state = STATE_START;
			low = 0;
		} else {
			state = STATE_INRANGE;
		}
		break;
	case STATE_INRANGE:
		if (type == RANGE)
			diag(cflist);
		if (type == TERM) {
			for (i = low; i <= maxnum; i++)
				sel[i] = MID_VALUE;
		} else {
			for (i = low; i < num; i++)
				sel[i] = MID_VALUE;
			sel[num] = TERM_VALUE;
		}
		low = 0;
		state = STATE_HASRANGE;
		break;
	case STATE_HASRANGE:
		if (type == RANGE || type == DIGIT)
			diag(cflist);
		state = STATE_START;
		break;
	}
	return p;
}

int
selcheck()
{
	int found = 0;
	char *cur = &sel[1], *last = &sel[maxnum + 1];

	while (cur < last) {
		while (cur < last)
			if (*cur++) {
				found = 1;
				break;
			}
		while (cur < last) {
			if (*cur == MID_VALUE) {
				cur++;
			} else if (*cur == 0) {
				cur++;
			} else if (*(cur + 1) != 0) {
				*cur++ = MID_VALUE;
			} else
				cur++;
		}
	}
	return found;
}

main(int argc, char **argv)
{
	extern int	optind;
	extern char	*optarg;
	register int	c;
	register char	*p, *list;
	/* permits multibyte delimiter */
	char	*del;
	int	bflag, cflag, fflag, filenr;
	FILE	*inptr;
	int delw = 1;
	int exitval = 0;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore");
	(void)setlabel("UX:cut");

	del = "\t";

	getwidth(&wp);
	wp._eucw2++;
	wp._eucw3++;
	supflag = bflag = cflag = fflag = 0;
	split_chars = 1;
	if (isatty(1)) {
		setvbuf(stdout, malloc(BUFSIZE), _IOFBF, BUFSIZE);
		tty = 1;
	}

	while((c = getopt(argc, argv, "b:c:d:f:ns")) != EOF)
		switch(c) {
			case 'b':
				bflag = 1;
				list = optarg;
				break;
			case 'c':
				cflag = 1;
				list = optarg;
				break;
			case 'd':
			/* permits multibyte delimiter 	*/
				if (ISASCII(*optarg))
					delw = 1;
				else
					delw = ISSET2(*optarg) ? wp._eucw2 :
					       ISSET3(*optarg) ? wp._eucw3 :
								 wp._eucw1 ;
				if ((int)strlen(optarg) > delw)
					diag(":139:No delimiter\n");
				else
					del = optarg;
				break;
			case 'f':
				fflag = 1;
				list = optarg;
				break;
			case 'n':
				split_chars = 0;
				break;
			case 's':
				supflag++;
				break;
			case '?':
				usage(0);
		}

	argv = &argv[optind];
	argc -= optind;

	if (bflag + cflag + fflag > 1)
		usage(1);
	if (bflag + cflag + fflag == 0)
		usage(1);
	if (!split_chars && bflag == 0)
		usage(1);
	
	for (p = list, maxnum = 0; *p != '\0'; ) {
		if (isdigit(*p)) {
			int tmp = atoi(p);
			if (tmp > maxnum) maxnum = tmp;
			while (isdigit(*p))
				p++;
		} else
			p++;
	}

	sel = malloc(maxnum + 2);
	bzero(sel, maxnum + 2);

	for (p = list; *p != '\0'; )
		p = liststate(p);
	liststate(p);

	if (!selcheck())
		diag(":140:No fields\n");

	filenr = 0;
	do {	/* for all input files */
		if ( argc == 0 || strcmp(argv[filenr],"-") == 0 )
			inptr = stdin;
		else
			if ((inptr = fopen(argv[filenr], "r")) == NULL) {
				pfmt(stderr, MM_WARNING,
					":92:Cannot open %s: %s\n",
					argv[filenr], strerror(errno));
				exitval = 1;
				continue;
			}

		if (bflag)
			bytesplit(inptr);
		else if (cflag)
			charsplit(inptr);
		else
			fieldsplit(inptr, del);
		fclose(inptr);
	} while (++filenr < argc);

	return exitval;
}

void
bytesplit(FILE *fp)
{
	for (;;) {	/* for all lines of a file */
		int bytecnt = 0;
		int c;

		for (;;) {
			int chwidth;
			int i;
			char *mcp;
			char	mcbuf[100];

			c = getc(fp);
			bytecnt++;

			if (c == EOF) {
				if (bytecnt != 1) {
					putchar('\n');
					if (tty)
						fflush(stdout);
				}
				return;
			} else if (c == '\n') {
				putchar(c);
				if (tty)
					fflush(stdout);
				break;
			}

			if (ISASCII(c)) {
				if (bytecnt <= maxnum ? sel[bytecnt]
					: sel[maxnum] == MID_VALUE)
					putchar(c);
				continue;
			}

			chwidth = ISSET2(c) ? wp._eucw2
				   : ISSET3(c) ? wp._eucw3 : wp._eucw1;
			mcbuf[0] = (char)c;
			for (i = chwidth - 1, mcp = &mcbuf[1]; i-- > 0; )
				*mcp++ = getc(fp);
			*mcp = '\0';

			if (bytecnt <= maxnum) {
				if (sel[bytecnt + chwidth - 1])
					fputs(mcbuf, fp);
			} else if (!split_chars && sel[maxnum] == MID_VALUE) {
				fputs(mcbuf, fp);
			} else {
				for (i = bytecnt; i < bytecnt + chwidth; i++)
					if (sel[i])
						putchar(mcbuf[i - bytecnt]);
			}
			bytecnt += chwidth - 1;
		}
	}
}

static int cnt;
static int cap;
static char *buf;

void
save(int c)
{
	if (c == EOF)
		return;
	if (cnt == cap) {
		if (cap == 0)
			cap = 1024;
		cap *= 2;
		buf = realloc(buf, cap);
		if (!buf)
			diag(long_line);
	}
	buf[cnt++] = (char)c;
}

void
dump(nl)
{
	fwrite(buf, nl ? cnt : cnt - 1, 1, stdout);
	if (nl && cnt > 0 && buf[cnt - 1] != '\n')
		putchar('\n');
	if (tty)
		fflush(stdout);
	cnt = 0;
}

void
newline()
{
	cnt = 0;
}

void
fieldsplit(FILE *fp, char *del)
{
	for (;;) {	/* for all lines of a file */
		int fieldcnt = 1;
		int del_found = 0;
		int no_fields_printed = 1;

		newline();

		for (;;) {
			int chwidth;
			int i;
			char *mcp;
			char	mcbuf[100];
			int c;

			c = getc(fp);
			if (fieldcnt == 1)
				save(c);

			if (c == EOF) {
				if (fieldcnt == 1 && !supflag)
					dump(1);
				return;
			} else if (c == '\n') {
				if (fieldcnt > 1) {
					putchar(c);
					if (tty)
						fflush(stdout);
				} else if (!supflag) {
					dump(1);
				}
				break;
			}

			if (ISASCII(c)) {
				chwidth = 1;
				mcbuf[0] = (char)c;
				mcbuf[1] = '\0';
			} else {
				chwidth = ISSET2(c) ? wp._eucw2
					   : ISSET3(c) ? wp._eucw3 : wp._eucw1;
				mcbuf[0] = (char)c;
				for (i = chwidth-1, mcp = &mcbuf[1]; i-- > 0; )
					*mcp++ = getc(fp);
				*mcp = '\0';
			}
			if (!strcmp(mcbuf, del))
				fieldcnt++;
			if (fieldcnt == 1)
				continue;
			if (!del_found) {
				if (sel[1]) {
					no_fields_printed = 0;
					dump(0);
				}
				del_found = 1;
			}
			if (fieldcnt <= maxnum
				? sel[fieldcnt] : sel[maxnum] == MID_VALUE)
			{
				if (no_fields_printed)
					no_fields_printed = 0;
				else
					fputs(mcbuf, stdout);
			}
		}
	}
}

void
charsplit(FILE *fp)
{
	for (;;) {	/* for all lines of a file */
		int charcnt = 0;
		int c;

		for (;;) {
			int chwidth;
			int i;
			char *mcp;
			char	mcbuf[100];

			c = getc(fp);
			charcnt++;

			if (c == EOF) {
				if (charcnt != 1) {
					putchar('\n');
					if (tty)
						fflush(stdout);
				}
				return;
			} else if (c == '\n') {
				putchar(c);
				if (tty)
					fflush(stdout);
				break;
			}

			if (ISASCII(c)) {
				if (charcnt <= maxnum ? sel[charcnt]
					: sel[maxnum] == MID_VALUE)
					putchar(c);
				continue;
			}

			chwidth = ISSET2(c) ? wp._eucw2
				   : ISSET3(c) ? wp._eucw3 : wp._eucw1;
			mcbuf[0] = (char)c;
			for (i = chwidth - 1, mcp = &mcbuf[1]; i-- > 0; )
				*mcp++ = getc(fp);
			*mcp = '\0';

			if (charcnt <= maxnum && sel[charcnt]
			    || sel[maxnum] == MID_VALUE)
				fputs(mcbuf, fp);
		}
	}
}


void
diag(char *s)
{
	pfmt(stderr, MM_ERROR, s);
	exit(2);
}
