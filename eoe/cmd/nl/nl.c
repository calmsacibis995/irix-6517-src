/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)nl:nl.c	1.28.1.3"
/*	NLSID
*/

#include <stdio.h>	/* Include Standard Header File */
#include <regexpr.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>


#define EXPSIZ		512
#define	MAXWIDTH	100	/* max value used with '-w' option */


#ifdef u370
	int nbra, sed;	/* u370 - not used in nl.c, but extern in regexp.h */
#endif
	int width = 6;	/* Declare default width of number */
	char nbuf[MAXWIDTH + 1];	/* Declare bufsize used in convert/pad/cnt routines */
	char *bexpbuf;	/* Declare the regexp buf */
	char *hexpbuf;	/* Declare the regexp buf */
	char *fexpbuf;	/* Declare the regexp buf */
	char delim1 = '\\'; char delim2 = ':';	/* Default delimiters. */
	char pad = ' ';	/* Declare the default pad for numbers */
	char *s;	/* Declare the temp array for args */
	char s1[EXPSIZ];	/* Declare the conversion array */
	char format = 'n';	/* Declare the format of numbers to be rt just */
	int q = 2;	/* Initialize arg pointer to drop 1st 2 chars */
	int k;	/* Declare var for return of convert */
	int r;	/* Declare the arg array ptr for string args */

main(argc,argv)
int argc;
char *argv[];
{
	register int j;
	register int i = 0;
	register char *p;
	register char header = 'n';
	register char body = 't';
	register char footer = 'n';
	char line[BUFSIZ];
	char tempchr;	/* Temporary holding variable. */
	char swtch = 'n';
	char cntck = 'n';
	char type;
	int cnt;	/* line counter */
	int pass1 = 1;	/* First pass flag. 1=pass1, 0=additional passes. */
	char sep[EXPSIZ];
	char pat[EXPSIZ];
	char *string;
	register char *ptr ;
	int startcnt=1;
	int increment=1;
	int blank=1;
	int blankctr = 0;
	int c,lnt;
	char last;
	FILE *iptr=stdin;
	FILE *optr=stdout;
	sep[0] = '\t';
	sep[1] = '\0';

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel("UX:nl");

/*		DO WHILE THERE IS AN ARGUMENT
		CHECK ARG, SET IF GOOD, ERR IF BAD	*/
	
	while((c = getopt(argc, argv, "b:f:d:h:i:l:n:ps:v:w:")) != EOF)
	  switch(c){
		case 'h':
		  switch(*optarg){
			case 'n':
			  header = 'n';
			  break;
			case 't':
			  header = 't';
			  break;
			case 'a':
			  header = 'a';
			  break;
			  case 'p':
			  s=optarg;
			  q=1;
			    r=0;
			  while (s[q] != '\0'){
				  pat[r] = s[q];
				  r++;
				  q++;
			  }
			    pat[r] = '\0';
			  header = 'h';
			  ptr = pat;
			  hexpbuf = compile(pat, (char *)0, (char *)0);
			    if (regerrno)
			      regerr(regerrno);
			  break;
			default:
			  optmsg(*optarg, gettxt(":22","Header: "));
		  }
		  break;
		  
		case 'b':
		  switch(*optarg){
			case 't':
			  body = 't';
			  break;
			case 'a':
			  body = 'a';
			  break;
			case 'n':
			  body = 'n';
			  break;
			case 'p':
			  s=optarg;
			  q=1;
			  r=0;
			  while (s[q] != '\0'){
				  pat[r] = s[q];
				  r++;
				  q++;
			  }
			  pat[r] = '\0';
			  body = 'b';
			  ptr = pat;
			  bexpbuf = compile(pat, (char *)0, (char *)0);
			  if (regerrno)
			    regerr(regerrno);
			  break;
			default:
			  optmsg(*optarg,
				 gettxt(":23",
					"Body: "));
		  }
		  break;

		case 'f':
		  switch(*optarg){
			case 'n':
			  footer = 'n';
			  break;
			case 't':
			  footer = 't';
			  break;
			case 'a':
			  footer = 'a';
			  break;
			case 'p':
			  s=optarg;
			  q=1;
			  r=0;
			  while (s[q] != '\0'){
				  pat[r] = s[q];
				  r++;
				  q++;
			  }
			  pat[r] = '\0';
			  footer = 'f';
			  ptr = pat;
			  fexpbuf = compile(pat, (char *)0, (char *)0);
			  if (regerrno)
			    regerr(regerrno);
			  break;
			default:
			  optmsg(*optarg, gettxt(":24","Footer: "));
		  }
		  break;
		  
		case 'p':
		  cntck = 'y';
		  break;
		case 'v':
		  startcnt = convert(optarg);
		  break;
		case 'i':
		  increment = convert(optarg);
		  break;
		case 'w':
		  width = convert(optarg);
		  if (width > MAXWIDTH)
		    width = MAXWIDTH;
		  break;
		case 'l':
		  blank = convert(optarg);
		  break;
		case 'n':
		  switch (*optarg) {
			case 'l':
			  if (optarg[1] == 'n')
			    format = 'l';
			  else
			    {
				    optmsg(optarg[1], "");
			    }
			  break;
			case 'r':
			  if ( optarg[1] == 'n' || optarg[1] == 'z')
			    format = optarg[1];
			  else
			    {
				    optmsg(optarg[1], "");
			    }
			  break;
			default:
			  optmsg(optarg[1], "");
			  break;
		  }
		  break;
		case 's':
		  s = optarg;
		  q = 0;
		  r = 0;
		  if(s != NULL)
		    while (s[q] != '\0') {

			    sep[r] = s[q];
			    r++;
			    q++;
		    }
		  sep[r] = '\0';
		  break;
		case 'd':
		  tempchr = *optarg;
		  delim1 = tempchr;
		  tempchr = optarg[1];
		  if(tempchr == '\0')break;
		  delim2 = tempchr;
		  if(optarg[2] != '\0')optmsg(optarg[2],"");
		  break;
		default:
		  optmsg(c, "");
	  }

	argc -= optind;
	argv = &argv[optind];
	
	/* only one file may be named */
	if(argc > 1){
		pfmt(stderr, MM_ERROR, ":26:Only one file may be named\n");
		exit(1);
	}

	/* Use stdin as input if filename not specified. */
	if(argc < 1 || !strcmp(argv[0], "-"))
	  iptr = stdin;
	else if ((iptr = fopen(argv[0],"r")) == NULL)  {
		pfmt(stderr, MM_ERROR, ":3:Cannot open %s: %s\n",
		     argv[0], strerror(errno));
		exit(1);
	}

	/* ON FIRST PASS ONLY, SET LINE COUNTER (cnt) = startcnt &
		SET DEFAULT BODY TYPE TO NUMBER ALL LINES.	*/
	if(pass1){
		cnt = startcnt; type = body; last = 'b'; pass1 = 0;
	}

/*		DO WHILE THERE IS INPUT
		CHECK TO SEE IF LINE IS NUMBERED,
		IF SO, CALCULATE NUM, PRINT NUM,
		THEN OUTPUT SEPERATOR CHAR AND LINE	*/

	while (( p = fgets(line,sizeof(line),iptr)) != NULL) {
		if (p[0] == delim1 && p[1] == delim2) {
			if (p[2] == delim1 && p[3] == delim2 && p[4]==delim1 
			    && p[5]==delim2 && p[6] == '\n') {
				if ( cntck != 'y')
				  cnt = startcnt;
				type = header;
				last = 'h';
				swtch = 'y';
			}
			else {
				if (p[2] == delim1 && p[3] == delim2 && 
				    p[4] == '\n') {
					if ( cntck != 'y' && last != 'h')
					  cnt = startcnt;
					type = body;
					last = 'b';
					swtch = 'y';
				}
				else {
					if (p[0] == delim1 && p[1] == delim2 
					    && p[2] == '\n') {
						if ( cntck != 'y' && last == 'f')
						  cnt = startcnt;
						type = footer;
						last = 'f';
						swtch = 'y';
					}
				}
			}
		}
		if (p[0] != '\n'){
			lnt = strlen(p);
			if(p[lnt-1] == '\n')
			  p[lnt-1] = NULL;
		}
		
		if (swtch == 'y') {
			swtch = 'n';
			fprintf(optr,"\n");
		}
		else {
			switch(type) {
			      case 'n':
				npad(width,sep);
				break;
			      case 't':
				if (p[0] != '\n' && printable(line)) {
					pnum(cnt,sep);
					cnt+=increment;
				}
				else {
					npad(width,sep);
				}
				break;
			      case 'a':
				if (p[0] == '\n') {
					blankctr++;
					if (blank == blankctr) {
						blankctr = 0;
						pnum(cnt,sep);
						cnt+=increment;
					}
					else npad(width,sep);
				}
				else {
					blankctr = 0;
					pnum(cnt,sep);
					cnt+=increment;
				}
				break;
			      case 'b':
				if (step(p,bexpbuf)) {
					pnum(cnt,sep);
					cnt+=increment;
				}
				else {
					npad(width,sep);
				}
				break;
			      case 'h':
				if (step(p,hexpbuf)) {
					pnum(cnt,sep);
					cnt+=increment;
				}
				else {
					npad(width,sep);
				}
				break;
			      case 'f':
				if (step(p,fexpbuf)) {
					pnum(cnt,sep);
					cnt+=increment;
				}
				else {
					npad(width,sep);
				}
				break;
			}
			if (p[0] != '\n')
			  p[lnt-1] = '\n';
			fprintf(optr,"%s",line);
		}	/* Closing brace of "else" (~ line 307). */
	}	/* Closing brace of "while". */
	fclose(iptr);
}

/* check if there is any printable character in this line */
printable(s)
char *s;
{
	for (;*s != '\0';s++)
	  if(isprint(*s))
	    return(1);
	return(0);
}

/*		REGEXP ERR ROUTINE		*/

regerr(c)
int c;
{
pfmt(stderr, MM_ERROR, ":25:Regular Expression error %d\n",c);
exit(1);
}

/*		CALCULATE NUMBER ROUTINE	*/

pnum(n,sep)
int	n;
char *	sep;
{
	register int	i;

	if (format == 'z') {
		pad = '0';
	}
	for ( i = 0; i < width; i++)
	  nbuf[i] = pad;
	num(n,width - 1);
	if (format == 'l') {
		while (nbuf[0]==' ') {
			for ( i = 0; i < width; i++)
			  nbuf[i] = nbuf[i+1];
			nbuf[width-1] = ' ';
		}
	}
	printf("%s%s",nbuf,sep);
}

/*		IF NUM > 10, THEN USE THIS CALCULATE ROUTINE		*/

num(v,p)
int v,p;
{
	if (v < 10)
		nbuf[p] = v + '0' ;
	else {
		nbuf[p] = (v % 10) + '0' ;
		if (p>0) num(v / 10,p - 1);
	}
}

/*		CONVERT ARG STRINGS TO STRING ARRAYS	*/

convert(argv)
char **argv;
{
	s = (char*)argv;
	q=0;
	r=0;
	while (s[q] != '\0') {
		if (s[q] >= '0' && s[q] <= '9')
		{
		s1[r] = s[q];
		r++;
		q++;
		}
		else
				{
				optmsg(s[q], "");
				}
	}
	s1[r] = '\0';
	k = atoi(s1);
	return(k);
}

/*		CALCULATE NUM/TEXT SEPRATOR		*/

npad(width,sep)
	int	width;
	char *	sep;
{
	register int i;

	pad = ' ';
	for ( i = 0; i < width; i++)
		nbuf[i] = pad;
	printf("%s",nbuf);

	for(i=0; i < (int) strlen(sep); i++)
		printf(" ");
}
/* ------------------------------------------------------------- */
optmsg(option, whence)
char option;
char *whence;
{
	pfmt(stderr, MM_ERROR,
		":26:%sIllegal option -- %c\n", whence, option);
	exit(1);
}
