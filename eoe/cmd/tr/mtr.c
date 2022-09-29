/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tr:mtr.c	1.2"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <widec.h>
#include <wctype.h>


#include "extern.h"

extern int dflag, sflag, cflag;
extern char cmd_label[];
extern FILE *input;

#define MAXC	256
struct string {
	wchar_t ch[MAXC], rng[MAXC];
	int rep[MAXC], nch;
	} s1, s2;

#define CH(ss,i)	ss->ch[i]
#define RNG(ss,i)	ss->rng[i]
#define REP(ss,i)	ss->rep[i]
#define NCH(ss)		ss->nch

static void sbuild(struct string *, char *);

typedef struct {
	char *name;
	int (*func) (wchar_t);
} CLASS;

static CLASS classes[] = {
	{ "alnum",  iswalnum  },
	{ "alpha",  iswalpha  },
	{ "blank", __iswblank },
	{ "cntrl",  iswcntrl  },
	{ "digit",  iswdigit  },
	{ "graph",  iswgraph  },
	{ "lower",  iswlower  },
	{ "print",  iswprint  },
	{ "punct",  iswpunct  },
	{ "space",  iswspace  },
	{ "upper",  iswupper  },
	{ "xdigit", iswxdigit },
};
CLASS *pClass;
int   nClassFlag = 0;

#define  MBUF_SIZE  BUFSIZ

void mtr(char *string1, char *string2)
{
	register int 	c,i, (*func)(wchar_t);
	wchar_t		save=0;
	register long   int j, k;
	wchar_t		wc;
	int		len=0;
	char 		mbuf[MBUF_SIZE];
	int mb_limit, mb_len, numb, nxt, cont;


	sbuild(&s1, string1);
	sbuild(&s2, string2);

	input = stdin;
	mb_limit = MBUF_SIZE - MB_CUR_MAX - 1;
        numb = 0;
        cont = 1;


	do {
		if ( (c = getc(input)) != EOF ) {
		 	mbuf[ numb ] = c;
                 numb++;

                 if ( numb < MBUF_SIZE-1 )
                     continue;
             	}else
                 cont = 0;

             mbuf[ numb ] = '\0';
	     for ( nxt=0; !(mb_limit<nxt) && nxt<numb; ) 
		if ( (len = mbtowc( &wc, mbuf + nxt, MB_CUR_MAX )) == -1 ) {
			/* invalid mb byte, output it */
		      putchar(mbuf[nxt]);
		      nxt++;
		 }
	        else {	
		if(nClassFlag && (pClass->func(wc)) && (len>1))
			i = s1.nch;   
		else  
			for(i=s1.nch; i-->0 && ((wc<s1.ch[i]) || 
					(wc-s1.ch[i]>s1.rng[i])); );
		if((i>=0 && !cflag) || (i<0 && cflag)) { 
			if(dflag) goto next;
			if (i<0 && cflag)
				j = s1.nch;
			else	{
				j = wc-s1.ch[i]+s1.rep[i];
				while(i-->0) j += s1.rep[i]+s1.rng[i];
			}
			/* j is the character position of wc in string1 */

			if ( s2.nch == 0 && wc!=save && 
				((i<0 && cflag) || (++i>=0 && !cflag)) )
					goto put;

			for(i=k=0; i<s2.nch; i++) {
				if((k += s2.rep[i]+s2.rng[i]) >= j) {
					wc = s2.ch[i]+s2.rng[i];
					if(s2.rng[i]) wc -= k-j;
					if(!sflag || wc!=save) goto put;
					else  	 goto next;
				}
				else if(i==s2.nch-1)   {
					wc = s2.ch[i]+s2.rng[i];
					if(!sflag || wc!=save) goto put;
					else     goto next;	
				}
			}
			goto next;
		}

		for(i=s2.nch; i-->0 && ((wc<s2.ch[i]) || (wc-s2.ch[i]>s2.rng[i])); );

		if(i<0 || !sflag || wc!=save) {
		put:
			save = wc;
			putwc(wc,stdout);
		}
		next: ;
		nxt += (len == 0) ? 1 : len;
	} /* else */
		for ( i=0; i<numb-nxt; i++ )
                  mbuf[i] = mbuf[ nxt + i ];
             	mbuf[i] = '\0';
             	numb -= nxt;

	 } while(cont);
}

static int
c_class(const void *a, const void *b)
{
	return (strcmp(((CLASS *)a)->name, ((CLASS *)b)->name));
}


static void sbuild(struct string *s, char *t)
{
	register int i, nCount, (*func) (wchar_t);
	long int     c;
	int 	     base, n;
	char         mbChar[MB_LEN_MAX], *pTemp;
	int          nCharLen1,nCharLen2;
	wchar_t      wTemp;
	CLASS	     tmp;
	int	     bracketed=0;
			
		/* Function Declaration */
	int GetNextChar(char **,char *);

	#define	PLACE(i,mbChar,nLen) { mbtowc((wchar_t *)&(CH(s,i)),mbChar,nLen+1); REP(s,i)=1; RNG(s,i)=0; }
	#define GENRANGE	{	\
				t++;	\
				nCharLen2 = GetNextChar(&t, mbChar);	\
				if((nCharLen1 == nCharLen2) || (nCharLen1!=1 && nCharLen2 != 1))  {	\
					mbtowc(&wTemp, mbChar, MB_CUR_MAX);	\
					RNG(s,i) = wTemp - s->ch[i];	\
					if (RNG(s,i) < 0)	\
						goto error;	\
				}	\
				else {	\
				  nCharLen2 = GetNextChar(&t, mbChar);	\
				  i++;	\
				  PLACE(i,mbChar,nCharLen2)	\
				}	\
		}
	#define GENSEQ		{	\
				t++;	\
				base = (*t=='0')?8:10;	\
				n = 0;	\
				while((c = *t)>='0' && c<'0'+base) {	\
					n = base*n + c - '0';	\
					t++;	\
				}	\
				if(*t++!=']') goto error;	\
				if(n==0) n = 10000;	\
				REP(s,i) = n;	\
		}

	for(i=0; *t; i++) {
		if(i>MAXC) goto error;

		if(*t == '[') {
			bracketed = 1;
			t++;
			if(*t ==  ':')	{
				t++;
				if(pTemp = strstr(t, ":]"))	{
					*pTemp = '\0';
					tmp.name = t;
					if((pClass = (CLASS *) bsearch(&tmp, classes, sizeof(classes) /
						sizeof(CLASS), sizeof(CLASS), c_class)) == NULL)  {
						_sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
						  gettxt(_SGI_DMMX_tr_unkclass, 
						   "tr: unknown class %s"), tmp.name);
						exit(1);
					}
					mbChar[1] = 0;
					for (nCount = 0, func = pClass->func; nCount < NCHARS; ++nCount) {
						mbChar[0] = nCount;
						if ((func)(mbChar[0]))	{
							PLACE(i, mbChar, 1)
							i++;
						}
					}
					t = pTemp + 2;
					nClassFlag = 1;
					continue;
				}
				else 	t--;	/* not '[:class:]' form */
			}
			else if(*t == '=')  {
				t++;
				if(pTemp = strstr(t, "=]"))	{
					nCharLen1 = GetNextChar(&t, mbChar);
					i++;
					PLACE(i, mbChar, nCharLen1)
					if(*t != '=')	{
						_sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
						   gettxt(_SGI_DMMX_tr_badequals,
						    "tr: misplaced equivalence equals sign"));
						exit(1);
					}
					t = pTemp+2;
					continue;
				}
				else 	t--;	/* not '[=equiv=]' form */
			}

			nCharLen1 = GetNextChar(&t,mbChar);
			PLACE(i,mbChar,nCharLen1)

			switch(*t) {
			case '-':
				GENRANGE
				if(*t++!=']') goto error;
				break;

			case '*':
				if (s == &s1)	{
					_sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
					   gettxt(_SGI_DMMX_tr_badseq, 
					    "tr: sequences only valid in string2"));
					exit(1);
					/*NOTREACHED*/
				}
				GENSEQ
				break;
			case ']':
				t++;
				continue;
			default:
				nCharLen1 = GetNextChar(&t,mbChar);
				i++;
				PLACE(i,mbChar,nCharLen1)
				break;
			error:
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				   gettxt(_SGI_DMMX_tr_badstring, "tr: Bad string"));
				exit(1);
			}
		} 
		else if ( bracketed && ( *t == '*' ) && (i > 1)) { 
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
                        		gettxt(_SGI_DMMX_tr_badseqaster,
                                	"tr: misplaced sequence asterisk"));
                		exit(1);
							}
 
		else    {
			nCharLen1 = GetNextChar(&t,mbChar);
			PLACE(i,mbChar,nCharLen1)

			if(*t == '-' && (*(t+1) != NULL))	{
				GENRANGE
				continue;
			}
		}
	}
	NCH(s) = i;
}


int GetNextChar(char **t,char *mbChar)
{
	char *ptr;
	int nLen=1, i, n;
	char c;

	ptr = *t;
	mbChar[0] = *ptr++;
	if(mbChar[0] =='\\') {
		i = n = 0;
		while(i<3 && isdigit(c = *ptr))  {
			n = n*8 + c - '0';
			i++;
			ptr++;
		}
		if(i>0)
			mbChar[0] = (char) n;
		else  {
			switch (c)  {
			  case 'a' : mbChar[0] = '\7';
				     break;
			  case 'b' : mbChar[0] = '\b';
				     break;
			  case 'f' : mbChar[0] = '\f';
				     break;
			  case 'n' : mbChar[0] = '\n';
				     break;
			  case 'r' : mbChar[0] = '\r';
				     break;
			  case 't' : mbChar[0] = '\t';
				     break;
			  case 'v' : mbChar[0] = '\13';
				     break;
			  default  : mbChar[0] = *ptr;
			}
			ptr++;
		}
	}
	mbChar[nLen] = 0;
	if (mblen(mbChar, nLen+1) < 0)  {
		mbChar[nLen++] = *ptr++;
		mbChar[nLen] = 0;
	}
	*t = ptr;
	return(nLen+1);		
}
