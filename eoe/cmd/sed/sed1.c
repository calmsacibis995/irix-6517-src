/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sed:sed1.c	1.7"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sed/RCS/sed1.c,v 1.14 1996/07/28 04:15:28 olson Exp $"

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include "sed.h"
#include <regex.h>

union reptr     *abuf[ABUFSIZE];
union reptr **aptr;
char    ibuf[512];
char    *cbp;
char    *ebp;
char    genbuf[LBSIZE];
char    *lbend;
char	*lcomend;
int     dolflag;
int     sflag;
int     jflag;
int     delflag;
int		empty_file;
#ifdef sgi
int	printflag;
#endif
long    lnum;
char    holdsp[LBSIZE+1];
char    *spend;
char    *hspend;
int     nflag;
long    tlno[NLINES];
int     f;
int	numpass;
int     eolflg = 0;
union reptr     *pending;
extern	int xpg4command;
char	*trans[040]  = {
	"\\000",
	"\\001",
	"\\002",
	"\\003",
	"\\004",
	"\\005",
	"\\006",
	"\\a", /* 007 - BEL */
	"\\b", /* 010 - BS */
	"\\t", /* 011 - TAB */
	"\\n", /* 012 - NL */
	"\\v", /* 013 - VT */
	"\\f", /* 014 - FF */
	"\\r", /* 015 - CR */
	"\\016",
	"\\017",
	"\\020",
	"\\021",
	"\\022",
	"\\023",
	"\\024",
	"\\025",
	"\\026",
	"\\027",
	"\\030",
	"\\031",
	"\\032",
	"\\033",
	"\\034",
	"\\035",
	"\\036",
	"\\037"
};
char	rub[] = {"\\177"};

static int match(regex_t *);

execute(file)
char *file;
{
	register regex_t *p1, *p2;
	int	p1lno, p2lno;
	register union reptr	*ipc;
	int	c;
	char	*execp;
	char	*p1c;

	empty_file = 0;
	eolflg = 0;
	if (file) {
		if ((f = open(file, 0)) < 0) {
			fprintf(stderr, "Can't open %s\n", file);
			return(1);
		}
		if (!eolflg)
			eolflg = 1;
	} else
		f = 0;

	ebp = ibuf;
	cbp = ibuf;

	if(pending) {
		ipc = pending;
		pending = 0;
		goto yes;
	}

	for(;;) {
		if((execp = gline(linebuf)) == badp) {
			close(f);
			return(0);
		}
		spend = execp;

		for(ipc = ptrspace; ipc->r1.command; ) {

			p1 = &ipc->r1.ad1;
			p1lno = ipc->r1.ad1lno;
			p2 = &ipc->r1.ad2;
			p2lno = ipc->r1.ad2lno;

			if(p1lno != LNO_NONE) {
				if(ipc->r1.inar) {
					if(p2lno == LNO_LAST) {
						p1lno = LNO_NONE;
					} else if(p2lno >= 0) {
						c = (unsigned char)p2lno;
						if(lnum > tlno[c]) {
							ipc->r1.inar = 0;
							if(ipc->r1.negfl)
								goto yes;
							ipc++;
							continue;
						}
						if(lnum == tlno[c]) {
							ipc->r1.inar = 0;
						}
					} else if(p2lno == LNO_RE &&
						  match(p2)) {
						ipc->r1.inar = 0;
					}
				} else if(p1lno == LNO_LAST) {
					if(!dolflag) {
						if(ipc->r1.negfl)
							goto yes;
						ipc++;
						continue;
					}

				} else if(p1lno >= 0) {
					c = p1lno;
					if(lnum != tlno[c]) {
						if(ipc->r1.negfl)
							goto yes;
						ipc++;
						continue;
					}
					if(p2lno != LNO_NONE)
						ipc->r1.inar = 1;
				} else if(p1lno == LNO_RE &&
					  match(p1)) {
					if(p2lno != LNO_NONE)
						ipc->r1.inar = 1;
				} else {
					if(ipc->r1.negfl)
						goto yes;
					ipc++;
					continue;
				}
			}

			if(ipc->r1.negfl) {
				ipc++;
				continue;
			}
	yes:
			command(ipc);

			if(delflag)
				break;

			if(jflag) {
				jflag = 0;
				if((ipc = ipc->r2.lb1) == 0) {
					ipc = ptrspace;
					break;
				}
			} else
				ipc++;

		}
#ifdef sgi
		if(!nflag && (printflag || !delflag)) {
			if (printflag < 2)	/* 'p' flag or default*/
				for(p1c = linebuf; p1c < spend; p1c++)
					putc(*p1c, stdout);
			else				/* 'P' flag */
				for(p1c = linebuf; *p1c != '\n' && *p1c != '\0'; )
					putc(*p1c++, stdout);
			if (eolflg != 2)
				putc('\n', stdout);
			else
				eolflg = 1;
		}
#else
		if(!nflag && !delflag) {
			for(p1c = linebuf; p1c < spend; p1c++)
				putc(*p1c, stdout);
			putc('\n', stdout);
		}
#endif

		if(aptr > abuf) {
			arout();
		}

		delflag = 0;
#ifdef sgi
		printflag = 0;
#endif

	}
}
#define MATCHBUF_MAX 100

regmatch_t matchbuf[MATCHBUF_MAX];

static void check_matchbuf(regex_t *expbuf)
{
	if (expbuf->re_nsub >= MATCHBUF_MAX) {
		fprintf(stderr,"Error: too many subexpressions in regular expression\n");
		exit(1);
	}
}

static int match(regex_t *expbuf)
{
	check_matchbuf(expbuf);

	return(regexec(expbuf,
		       linebuf,
		       MATCHBUF_MAX,
		       matchbuf,
		       0) == 0);
}

substitute(union reptr	*ipc)
{
	char	*rhsbuf;
	int	n;
	register char *lp, *sp, *rp, *ep, *cp;
	int	lastmatch = 0;
	int c;
	int	len;
	
	numpass = 0;
	sflag = 0;		/* Flags if any substitution was made */
	rhsbuf = ipc->r1.rhs;
	n = ipc->r1.gfl;
	lp = linebuf;
	sp = genbuf;
	ep = lp + strlen(lp);

	if(ipc->r1.re1lno != LNO_RE)
		return(0);

	check_matchbuf(&ipc->r1.re1);

	if (regexec(&ipc->r1.re1,lp,MATCHBUF_MAX,matchbuf,0))
		return(0);

	do {
		cp = lp;
		lp += matchbuf[0].rm_eo;
		numpass++;
		if(n > 0 && n < 999) {
			if(n != numpass) {
				bcopy(cp,sp,matchbuf[0].rm_eo);
				sp += matchbuf[0].rm_eo;
				continue;
			}
		}
		rp = rhsbuf;
		sflag++;
		if (matchbuf[0].rm_so > 0) {
			bcopy(cp,sp,matchbuf[0].rm_so);
			sp += matchbuf[0].rm_so;
		}
		while(c = *rp++) {
			if (c == '&') {
				sp = place(sp,cp,0);
				continue;
			} 
			if(c == '\\') {
				c = *rp++;
				if (isdigit(c) &&
				    c != '0' &&
				    (c - '0') <= ipc->r1.re1.re_nsub) {
					sp = place(sp, cp, (c - '0'));
					continue;
				}
			}
			if (matchbuf[0].rm_eo || !lastmatch)
				*sp++ = c;
			if (sp >= &genbuf[LBSIZE]) {
				/* output line too long, get message below */
				sp--; 
				break;
			}
		}
		if(memccpy(sp, lp, '\0', &genbuf[LBSIZE-1] - sp) == NULL) {
			fprintf(stderr, "Output line too long.\n");
toolong:
			memcpy(linebuf, genbuf, LBSIZE);
			linebuf[LBSIZE-1] = '\0';
			spend = &linebuf[LBSIZE-2];
		} else {
			cp = memccpy(linebuf, genbuf, '\0', LBSIZE);
			if (cp == NULL) {
				/* "can't happen", but be paranoid */
				goto toolong;
			}
			lp = linebuf + (sp - genbuf);
			spend = cp-1;
		}

		if (*lp == 0)
			break;
		if (matchbuf[0].rm_eo)
			lastmatch = 1;
		else {
			lp++;
			sp++;
			lastmatch = 0;
		}
		
	} while ( ((! sflag) || (n >= 999 && matchbuf[0].rm_eo >= 0)) &&
		 ! regexec(&ipc->r1.re1,lp,MATCHBUF_MAX,matchbuf,REG_NOTBOL));
		 
	return 1;
}

char	*place(char *asp, char *al1, int mi)
{
	register char *sp, *l1, *l2;
	int 	len;

	len = matchbuf[mi].rm_eo - matchbuf[mi].rm_so;
	sp = asp;
	l1 = al1 + matchbuf[mi].rm_so;
	l2 = l1 + len;
	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LBSIZE]) {
			fprintf(stderr, "Output line too long.\n");
			sp--;
			break;
		}
	}
	return(sp);
}

static char *check_genbuf(char *,int);
static char *append_genbuf(char *,char *);


command(ipc)
union reptr	*ipc;
{
	register int	i;
	register char   *p1, *p2, *p3;
	char	*execp;


	switch(ipc->r1.command) {

		case ACOM:
			*aptr++ = ipc;
			if(aptr >= &abuf[ABUFSIZE]) {
				fprintf(stderr, "Too many appends after line %ld\n",
					lnum);
			}
			*aptr = 0;
			break;

		case CCOM:
			delflag = 1;
			if(!ipc->r1.inar || dolflag) {
				for(p1 = ipc->r3.p; *p1; )
					putc(*p1++, stdout);
				putc('\n', stdout);
			}
			break;
		case DCOM:
			delflag++;
			break;
		case CDCOM:
			p1 = p2 = linebuf;

			while(*p1 != '\n') {
				if(*p1++ == 0) {
					delflag++;
					return;
				}
			}

			p1++;
			while(*p2++ = *p1++);
			spend = p2-1;
			jflag++;
			break;

		case EQCOM:
			fprintf(stdout, "%ld\n", lnum);
			break;

		case GCOM:
			p1 = linebuf;
			p2 = holdsp;
			while(*p1++ = *p2++);
			spend = p1-1;
			break;

		case CGCOM:
			*spend++ = '\n';
			p1 = spend;
			p2 = holdsp;
			while(*p1++ = *p2++);
			spend = p1-1;
			break;

		case HCOM:
			p1 = holdsp;
			p2 = linebuf;
			while(*p1++ = *p2++);
			hspend = p1-1;
			break;

		case CHCOM:
			*hspend++ = '\n';
			p1 = hspend;
			p2 = linebuf;
			while(*p1++ = *p2++);
			hspend = p1-1;
			break;

		case ICOM:
			for(p1 = ipc->r3.p; *p1; )
				putc(*p1++, stdout);
			putc('\n', stdout);
			break;

		case BCOM:
			jflag = 1;
			break;


		case LCOM:
			p1 = linebuf;
			p2 = genbuf;
			while (*p1) {
				if((unsigned char)*p1 < 040) {
					p3 = trans[(unsigned char)*p1];
					if (xpg4command)
						p2 = append_genbuf(p2,p3);
					else {
						*p2++ = *p3++;
						p2 = check_genbuf(p2,1);
						if (*p3 == '0')
							p3++;
						p2 = append_genbuf(p2,p3);
					}
					p1++;
				} else if (*p1 == 0177) {
					p2 = append_genbuf(p2,rub);
					p1++;
					continue;
				} else if (!isprint(*p1 & 0377)) {
					p2 = append_genbuf(p2,"\\");
					*p2++ = (*p1 >> 6) + '0';
					p2 = check_genbuf(p2,1);
					*p2++ = ((*p1 >> 3) & 07) + '0';
					p2 = check_genbuf(p2,1);
					*p2++ = (*p1++ & 07) + '0';
					p2 = check_genbuf(p2,1);
				} else {
					*p2++ = *p1++;
					p2 = check_genbuf(p2,1);
				}
			}
			if (xpg4command)
				*p2++ = '$';
			(void) check_genbuf(p2,0);
			break;

		case NCOM:
			if(!nflag) {
				for(p1 = linebuf; p1 < spend; p1++)
					putc(*p1, stdout);
				putc('\n', stdout);
			}

			if(aptr > abuf)
				arout();
			if((execp = gline(linebuf)) == badp) {
				pending = ipc;
				delflag = 1;
				break;
			}
			spend = execp;

			break;
		case CNCOM:
			if(aptr > abuf)
				arout();
			*spend++ = '\n';
			if((execp = gline(spend)) == badp) {
				pending = ipc;
				delflag = 1;
				break;
			}
			spend = execp;
			break;

		case PCOM:
			for(p1 = linebuf; p1 < spend; p1++)
				putc(*p1, stdout);
			putc('\n', stdout);
			break;
		case CPCOM:
	cpcom:
			for(p1 = linebuf; *p1 != '\n' && *p1 != '\0'; )
				putc(*p1++, stdout);
			putc('\n', stdout);
			break;

		case QCOM:
			if(!nflag) {
				for(p1 = linebuf; p1 < spend; p1++)
					putc(*p1, stdout);
				putc('\n', stdout);
			}
			if(aptr > abuf) arout();
			fclose(stdout);
			if (cbp < ebp) {
				off_t cpos;

				cpos = lseek(f,(off_t) 0,SEEK_CUR);
				if (cpos != ((off_t) -1)) {
					cpos -= (ebp - cbp);
					if (cpos < 0)
						cpos = 0;
					(void) lseek(f,cpos,SEEK_SET);
				}
			}
			exit(0);
		case RCOM:

			*aptr++ = ipc;
			if(aptr >= &abuf[ABUFSIZE])
				fprintf(stderr, "Too many reads after line%ld\n",
					lnum);

			*aptr = 0;

			break;

		case SCOM:
			i = substitute(ipc);
#ifdef sgi
			/*
			 * if 'p' was specified, and we don't print
			 * below, remember so we still print if the
			 * line was deleted ('p' overrides 'd')
			 */
			if (ipc->r1.pfl && i && !(nflag || xpg4command))
				printflag = ipc->r1.pfl;
#endif
			if(ipc->r1.pfl && (nflag || xpg4command) && i) {
				if(ipc->r1.pfl == 1) {	/* 'p' flag */
					for(p1 = linebuf; p1 < spend; p1++)
						putc(*p1, stdout);
					putc('\n', stdout);
				}
				else			/* 'P' flag */
					goto cpcom;
			}
			if(i && ipc->r1.fcode)
				goto wcom;
			break;

		case TCOM:
			if(sflag == 0)  break;
			sflag = 0;
			jflag = 1;
			break;

		wcom:
		case WCOM:
			fprintf(ipc->r1.fcode, "%s\n", linebuf);
			break;
		case XCOM:
			p1 = linebuf;
			p2 = genbuf;
			while(*p2++ = *p1++);
			p1 = holdsp;
			p2 = linebuf;
			while(*p2++ = *p1++);
			spend = p2 - 1;
			p1 = genbuf;
			p2 = holdsp;
			while(*p2++ = *p1++);
			hspend = p2 - 1;
			break;

		case YCOM: 
			p1 = linebuf;
			p2 = ipc->r3.p;
			while(*p1 = p2[(unsigned char)*p1])	p1++;
			break;
	}

}


static char *check_genbuf(char *p2,int do_escape)
{
	if (! do_escape ||
	    p2 >= lcomend) {
		if (do_escape)
			*p2++ = '\\';
		*p2 = 0;
		fprintf(stdout, "%s\n", genbuf);
		p2 = genbuf;
	}
	return(p2);
}


static char *append_genbuf(char *p2,char *p3)
{
	while(*p2++ = *p3++)
		p2 = check_genbuf(p2,1);
	p2--;
}


char	*
gline(addr)
char	*addr;
{
	register char   *p1, *p2;
	register	c;
	p1 = addr;
	p2 = cbp;
	for (;;) {
		if (p2 >= ebp) {
			if ((c = read(f, ibuf, 512)) <= 0) {
				if ((p2 == ebp) && !*p2 && empty_file) {
					close(f);
					if(eargc == 0)
							dolflag = 1;
					p2 = ibuf;
					ebp = ibuf+c;
					if (eolflg == 1)
						eolflg = 2;
					break;
				} else
					return(badp);
			}
			empty_file = 1;
			p2 = ibuf;
			ebp = ibuf+c;
		}
		if ((c = *p2++) == '\n') {
			if(p2 >=  ebp) {
				if((c = read(f, ibuf, 512)) <= 0) {
					close(f);
					if(eargc == 0)
							dolflag = 1;
				}

				p2 = ibuf;
				ebp = ibuf + c;
			}
			break;
		}
		if(c)
		if(p1 < lbend)
			*p1++ = c;
	}
	lnum++;
	*p1 = 0;
	cbp = p2;

	return(p1);
}

arout()
{
	register char   *p1;
	FILE	*fi;
	char	c;
	int	t;

	aptr = abuf - 1;
	while(*++aptr) {
		if((*aptr)->r1.command == ACOM) {
			for(p1 = (*aptr)->r3.p; *p1; )
				putc(*p1++, stdout);
			putc('\n', stdout);
		} else {
			if((fi = fopen((*aptr)->r3.p, "r")) == NULL)
				continue;
			while((t = getc(fi)) != EOF) {
				c = t;
				putc(c, stdout);
			}
			fclose(fi);
		}
	}
	aptr = abuf;
	*aptr = 0;
}

