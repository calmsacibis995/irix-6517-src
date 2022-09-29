/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tabs:tabs.c	1.8"

/*
 *	tabs [tabspec] [+mn] [-Ttype]
 *	set tabs (and margin, if +mn), for terminal type
 */


#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curses.h>
#include <term.h>
#define EQ(a,b)	(strcmp(a, b) == 0)
/*	max # columns used (needed for GSI) */
#define NCOLS	256
#define NTABS	65	/* max # tabs +1 (to be set) */
#define	NTABSCL	21	/* max # tabs + 1 that will be cleared */
#define ESC	033
#define CLEAR	'2'
#define SET	'1'
#define TAB	'\t'
#define CR	'\r'
#define NMG	0	/* no margin setting */
#define GMG	1	/* DTC300s margin */
#define TMG	2	/* TERMINET margin */
#define DMG	3	/* DASI450 margin */
#define FMG	4	/* TTY 43 margin */
#define TRMG	5	/* Trendata 4000a */

#define TCLRLN	0	/* long, repetitive, general tab clear */

int	err;
int 	tmarg, getmarg();
int 	ttp;
char	settab[32], clear_tabs[32];

int	maxtab;		/* max tab for repetitive spec */
int	margin;	
int	margflg;	/* >0 ==> +m option used, 0 ==> not */
char	*terminal = "";
char	*tabspec = "-8";	/* default tab specification */

struct termio ttyold;	/* tty table */
int	ttyisave;	/* save for input modes */
int	ttyosave;	/* save for output modes */
int	istty;		/* 1 ==> is actual tty */

struct	stat	statbuf;
char	*devtty;

void	endup();
char	*getenv();

main(argc, argv)
char **argv;
{
	int tabvect[NTABS];	/* build tab list here */
	int need_term = 0;
	char *ttyname();
	char *scan;	/* scan pointer to next char */

	signal(SIGINT, endup);
	if (ioctl(1, TCGETA, &ttyold) == 0) {
		ttyisave = ttyold.c_iflag;
		ttyosave = ttyold.c_oflag;
		fstat(1, &statbuf);
		devtty = ttyname(1);
		chmod(devtty, 0000);	/* nobody, not even us */
		istty++;
	}
	tabvect[0] = 0;	/* mark as not yet filled in */
	while (--argc > 0) {
		scan = *++argv;
		if (*scan == '+')
			switch (*++scan) {
			case 'm':
				margflg++;
				if (*++scan)
					margin = getnum(&scan);
				else
					margin = 10;
				break;
			}
		else if (*scan == '-' && *(scan+1) == 'T') {
			terminal = scan+2;
			need_term++;
		} else {
			tabspec = scan;		/* save tab specification */
		}
	}
	if (*terminal == '\0') {
	    if (need_term) {
		fprintf(stderr, "tabs: Need to specify a terminal type for the -T option\n");
		exit(1);
	    } else 
		terminal = getenv("TERM");
	}

	if ((terminal == NULL) || (*terminal == '\0')) {
		fprintf(stderr, "tabs: No value for $TERM and no -T specified\n");
		exit(1);
	}

	setupterm(terminal,1,&err);
	if (err <= 0) {
		fprintf(stderr,
			"tabs: Could not find `%s' in terminfo data base\n",
			terminal);
		exit(1);
	}
	if (set_tab == NULL) {
		fprintf(stderr,
		 "tabs: terminal type `%s' does not have the ability to set tabs\n",
			terminal);
		exit(1);
	}
	maxtab = columns;
	strcpy(settab,set_tab);
	strcpy(clear_tabs,clear_all_tabs);

	scantab(tabspec,tabvect,0);
	if (!tabvect[0])
		repetab("8",tabvect);
		settabs(tabvect);
	endup();
	exit(0);
}

/*	scantab: scan 1 tabspec & return tab list for it */

scantab(scan,tabvect,level)
char *scan;
int tabvect[NTABS], level;
{
	register char c;
	if (*scan == '-')
		if ((c = *++scan) == '-')
			filetab(++scan,tabvect,level);
		else if (c >= '0' && c <= '9')
			repetab(scan,tabvect);
		else if (stdtab(scan,tabvect))
			error("unknown tab code");
		else;
	else
		arbitab(scan,tabvect);
}

/*	repetab: scan and set repetitve tabs, 1+n, 1+2*n, etc */

repetab(scan,tabvect)
char *scan;
int tabvect[NTABS];
{
	register incr, i, tabn;
	int limit;
	incr = getnum(&scan);
	tabn = 1;
	limit = (maxtab-1)/(incr?incr:1)-1; /* # last actual tab */
	if (limit>NTABS-2)
		limit = NTABS-2;
	for (i = 0; i<=limit; i++)
		tabvect[i] = tabn += incr;
	tabvect[i] = 0;
}

/*	arbitab: handle list of arbitrary tabs */

arbitab(scan,tabvect)
char *scan;
int tabvect[NTABS];
{
	register i, t, last;
	last = 0;
	for (i = 0; i<NTABS-1;) {
		if (*scan == '+') {
			scan++;		/* +n ==> increment, not absolute */
			if (t = getnum(&scan))
				tabvect[i++] = last += t;
			else error("illegal increment");
		}
		else {
			if ((t = getnum(&scan)) > last)
				tabvect[i++] = last = t;
			else error("illegal tabs");
		}
		if (*scan++ != ',') break;
	}
	if (last > NCOLS)
		error("illegal tabs");
	tabvect[i] = 0;
}

/*	filetab: copy tabspec from existing file */
#define CARDSIZ	132
filetab(scan,tabvect,level)
char *scan;
int tabvect[NTABS];
{
	register length, i;
	register char c;
	int fildes;
	char card[CARDSIZ];	/* buffer area for 1st card in file */
	char state, found;
	char *temp;
	if (level)
		error("file indirection");
	if ((fildes = open(scan,0)) < 0)
		error("can't open");
	length = read(fildes,card,CARDSIZ);
	close(fildes);
	found = state = 0;
	scan = 0;
	for (i = 0; i<length && (c = card[i]) != '\n'; i++) {
		switch (state) {
		case 0:
			state = (c == '<'); break;
		case 1:
			state = (c == ':')?2:0; break;
		case 2:
			if (c == 't')
				state = 3;
			else if (c == ':')
				state = 6;
			else if (c != ' ')
				state = 5;
			break;
		case 3:
			if (c == ' ')
				state = 2;
			else {
				scan = &card[i];
				state = 4;
			}
			break;
		case 4:
			if (c == ' ') {
				card[i] = '\0';
				state = 5;
			}
			else if (c == ':') {
				card[i] = '\0';
				state = 6;
			}
			break;
		case 5:
			if (c == ' ')
				state = 2;
			else if (c == ':')
				state = 6;
			break;
		case 6:
			if (c == '>') {
				found = 1;
				goto done;
			}
			else state = 5;
			break;
		}
	}
done:
	if (found && scan != 0) {
		scantab(scan,tabvect,1);
		temp = scan;
		while (*++temp);
		*temp = '\n';
	}
	else scantab("-8",tabvect,1);
}

int
getmarg (term)
char *term;
{
	if (strncmp(term,"1620",4) == 0 || strncmp(term,"1700",4) == 0 || strncmp(term,"450",3) == 0)
		return(DMG);
	else if (strncmp(term,"300s",4) == 0)
		return(GMG);
	else if (strncmp(term,"4000a",5) == 0)
		return(TRMG);
	else if (strcmp(term,"43") == 0)
		return(FMG);
	else if (strcmp(term,"tn300") == 0 || strcmp(term,"tn1200") == 0)
		return(TMG);
	else return(NMG);
}

char	*cleartabs();
/*	settabs: set actual tabs at terminal */
/*	note: this code caters to necessities of handling GSI and
	other terminals in a consistent way. */

settabs(tabvect)
int tabvect[NTABS];
{
	char setbuf[512];	/* 2+3*NTABS+2+NCOLS+NTABS (+ some extra) */
	register char *p;		/* ptr for assembly in setbuf */
	register *curtab;		/* ptr to tabvect item */
	int i, previous, nblanks;
	if (istty) {
		ttyold.c_iflag &= ~ICRNL ;
		ttyold.c_oflag &= ~(ONLCR|OCRNL|ONOCR|ONLRET);
		ioctl(1, TCSETAW, &ttyold);	/* turn off cr-lf map */
	}
	p = setbuf;
	*p++ = CR;
	p = cleartabs(p, clear_tabs);

	if (margflg) {
		tmarg = getmarg(terminal);
		switch(tmarg) {
		case GMG:	/* GSI300S */
		/* NOTE: the 300S appears somewhat odd, in that there is
		a column 0, but there is no way to do a direct tab to it.
		The sequence ESC 'T' '\0' jumps to column 27 and prints
		a '0', without changing the margin. */
			*p++ = ESC;
			*p++ = 'T';	/* setup for direct tab */
			if (margin &= 0177)	/* normal case */
				*p++ = margin;
			else {			/* +m0 case */
				*p++ = 1;	/* column 1 */
				*p++ = '\b';	/* column 0 */
			}
			*p++ = margin;	/* direct horizontal tab */
			*p++ = ESC;
			*p++ = '0';	/* actual margin set */
			break;
		case TMG:	/* TERMINET 300 & 1200 */
			while (margin--)
				*p++ = ' ';
			break;
		case DMG:	/* DASI450/DIABLO 1620 */
			*p++ = ESC;	/* direct tab ignores margin */
			*p++ = '\t';
			if (margin == 3){
				*p++ = (margin & 0177);
				*p++ = ' ';
			}
			else
				*p++ = (margin & 0177) + 1;
			*p++ = ESC;
			*p++ = '9';
			break;
		case FMG:	/* TTY 43 */
			p--;
			*p++ = ESC;
			*p++ = 'x';
			*p++ = CR;
			while (margin--)
				*p++ = ' ';
			*p++ = ESC;
			*p++ = 'l';
			*p++ = CR;
			write(1, setbuf, p - setbuf);
			return;
		case TRMG:
			p--;
			*p++ = ESC;
			*p++ = 'N';
			while (margin--)
				*p++ = ' ';
			*p++ = ESC;
			*p++ = 'F';
			break;
		}
	}

/*
 *	actual setting: at least terminals do this consistently!
 */
	previous = 1; curtab = tabvect;
	while ((nblanks = *curtab-previous) >= 0 &&
		previous + nblanks <= maxtab) {
		for (i = 1; i <= nblanks; i++) *p++ = ' ';
		previous = *curtab++;
		strcpy(p,settab);
		p += strlen(settab);
	}
	*p++ = CR;
	if (EQ(terminal,"4424"))
		*p++ = '\n';	/* TTY40/2 needs LF, not just CR */
	write(1, setbuf, p - setbuf);
}

/*	cleartabs(pointer to buffer, pointer to clear sequence) */
char *cleartabs(p, qq)
register char *p;
char *qq;
{
	register i;
	register char *q;
	q = qq;
	if (clear_tabs == 0) {	      /* if repetitive sequence */
		*p++ = CR;
		for(i = 0; i < NTABSCL - 1; i++) {
			*p++ = TAB;
			*p++ = ESC;
			*p++ = CLEAR;
		}
		*p++ = CR;
	}
	else {
		while(*p++ = *q++);	/* copy table sequence */
		p--;			/* adjust for null */
		if (EQ(terminal,"4424")) {	/* TTY40 extra delays needed */
			*p++ = '\0';
			*p++ = '\0';
			*p++ = '\0';
			*p++ = '\0';
		}
	}
	return(p);
}
/*	getnum: scan and convert number, return zero if none found */
/*	set scan ptr to addr of ending delimeter */
getnum(scan1)
char **scan1;
{
	register n;
	register char c, *scan;
	n = 0;
	scan = *scan1;
	while ((c = *scan++) >= '0' && c <= '9') n = n * 10 + c -'0';
	*scan1 = --scan;
	return(n);
}

/*	error: terminate processing with message to terminal */
error(arg)
char *arg;
{
	register char *temp;
	temp = arg;
	while (*++temp);	/* get length */
	*temp = '\n';
	endup();
	write(2, arg, temp+1-arg);
	exit(1);
}

/*	endup: make sure tty mode reset & exit */
void
endup()
{
	if (istty) {
		ttyold.c_iflag = ttyisave;
		ttyold.c_oflag = ttyosave;
		ioctl(1, TCSETAW, &ttyold);	/* reset cr-lf to previous */
		chmod(devtty, statbuf.st_mode & 0777);
	}
	if (err > 0)
		resetterm();
}

/*	stdtabs: standard tabs table
	format: option code letter(s), null, tabs, null */
char stdtabs[] = {
'a',	0,1,10,16,36,72,0,			/* IBM 370 Assembler */
'a','2',0,1,10,16,40,72,0,			/* IBM Assembler alternative*/
'c',	0,1,8,12,16,20,55,0,			/* COBOL, normal */
'c','2',0,1,6,10,14,49,0,			/* COBOL, crunched*/
'c','3',0,1,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,67,0,
						/* crunched COBOL, many tabs */
'f',	0,1,7,11,15,19,23,0,			/* FORTRAN */
'p',	0,1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,0, /* PL/I */
's',	0,1,10,55,0,				/* SNOBOL */
'u',	0,1,12,20,44,0,				/* UNIVAC ASM */
0};

/*	stdtab: return tab list for any "canned" tab option.
	entry: option points to null-terminated option string
		tabvect points to vector to be filled in
	exit: return(0) if legal, tabvect filled, ending with zero
		return(-1) if unknown option
*/
stdtab(option,tabvect)
char option[];
int tabvect[];
{
	register char *sp;
	tabvect[0] = 0;
	sp = stdtabs;
	while (*sp) {
		if (EQ(option,sp)) {
			while (*sp++);		/* skip to 1st tab value */
			while (*tabvect++ = *sp++);	/* copy, make int */
			return(0);
		}
		while(*sp++);	/* skip to 1st tab value */
		while(*sp++);		/* skip over tab list */
	}
	return(-1);
}
