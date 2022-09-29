/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)m4:m4.c	1.8"

#include	<stdio.h>
#include	<signal.h>
#include	"m4.h"

#define match(c,s)	(c==*s && (!s[1] || inpmatch(s+1)))
#define MAXINCLS 10
int nincls;
char *incls[MAXINCLS];


main(argc,argv)
char 	**argv;
{
	register t;
	FILE *fp;
	int couldnt_open_file = 0;
	int final_exit_status = OK;

#ifdef gcos
	tempfile = "m4*tempa";
#endif

#ifdef unix
	{
	static	sigs[] = {SIGHUP, SIGINT, SIGPIPE, 0};
	for (t=0; sigs[t]; ++t)
		if (signal(sigs[t], SIG_IGN) != SIG_IGN)
			(void) signal(sigs[t],catchsig);
	}

	tempfile = mktemp("/tmp/m4aXXXXX");
	close(creat(tempfile,0));
#endif

	procnam = argv[0];
	getflags(&argc,&argv);
	initalloc();

	setfname("-");
	if (argc>1) {
		--argc;
		++argv;
		if (strcmp(argv[0],"-")) {
		    if ((fp=fopen(argv[0],"r"))==NULL) {
		       fprintf(stderr,"Can't open file %s\n",argv[0]);
		       couldnt_open_file = 1;
		       final_exit_status = NOT_OK;
                    } else {
			ifile[ifx] = fp;
			couldnt_open_file = 0;
			setfname(argv[0]);
                    }
		}
	}

	for (;;) {
		if (couldnt_open_file == 0) {
	          token[0] = t = getchr();
		  token[1] = EOS;
                }

		if (t==EOF || couldnt_open_file) {
			if (ifx > 0) {
				fclose(ifile[ifx]);
				ipflr = ipstk[--ifx];
				continue;
			}

			getflags(&argc,&argv);

			if (argc<=1) 
			  if (Wrapstr) {
				while (Wrapstr) {
				  pbstr(Wrapstr->str);
				  Wrapstr = Wrapstr->previous;
                                }
				continue;
                           } else {
			     break;
                           }


			--argc;
			++argv;

			if (ifile[ifx]!=stdin && couldnt_open_file == 0)
				fclose(ifile[ifx]);

			if (*argv[0]=='-')
				ifile[ifx] = stdin;
			else {
				if ((fp=fopen(argv[0],"r"))==NULL) {
		                  fprintf(stderr,"Can't open file %s\n",argv[0]);
		                  couldnt_open_file = 1;
				  final_exit_status = NOT_OK;
                                } else {
				   ifile[ifx] = fp;
				   couldnt_open_file = 0;
                                }
                        }
                        if (couldnt_open_file == 0)
			  setfname(argv[0]);
			continue;
		}

		if (isalpha(t) || (t == '_') ) {
			register char	*tp = token+1;
			register	tlim = toksize;
			struct nlist	*macadd;  /* temp variable */

			while ( ( isalnum(*tp = getchr()) || *tp == '_' ) )
			{
			    tp ++;
			    if (--tlim<=0)
				error2("more than %d chars in word",
				       toksize);
			}

			putbak(*tp);
			*tp = EOS;

			macadd = lookup(token);
			*Ap = (char *) macadd;
			if (macadd->def) {
				if ((char *) (++Ap) >= astklm) {
					--Ap;
					error2(astkof,stksize);
				}

				if (Cp++==NULL)
					Cp = callst;

				Cp->argp = Ap;
				*Ap++ = op;
				puttok(token);
				stkchr(EOS);
				t = getchr();
				putbak(t);

				if (t!='(')
					pbstr("()");
				else	/* try to fix arg count */
					*Ap++ = op;

				Cp->plev = 0;
			} else {
				puttok(token);
			}
		} else if (match(t,lquote)) {
			register	qlev = 1;

			for (;;) {
				token[0] = t = getchr();
				token[1] = EOS;

				if (match(t,rquote)) {
					if (--qlev > 0)
						puttok(token);
					else
						break;
				} else if (match(t,lquote)) {
					++qlev;
					puttok(token);
				} else {
					if (t==EOF)
						error("EOF in quote", NULL);

					putchr(t);
				}
			}
		} else if (match(t,lcom)) {
			puttok(token);

			for (;;) {
				token[0] = t = getchr();
				token[1] = EOS;

				if (match(t,rcom)) {
					puttok(token);
					break;
				} else {
					if (t==EOF)
						error("EOF in comment", NULL);
					putchr(t);
				}
			}
		} else if (Cp==NULL) {
			putchr(t);
		} else if (t=='(') {
			if (Cp->plev)
				stkchr(t);
			else {
				/* skip white before arg */
				while (isspace(t=getchr()))
					;

				putbak(t);
			}

			++Cp->plev;
		} else if (t==')') {
			--Cp->plev;

			if (Cp->plev==0) {
				stkchr(EOS);
				expand(Cp->argp,Ap-Cp->argp-1);
				op = *Cp->argp;
				Ap = Cp->argp-1;

				if (--Cp < callst)
					Cp = NULL;
			} else
				stkchr(t);
		} else if (t==',' && Cp->plev<=1) {
			stkchr(EOS);
			*Ap = op;

			if ((char *) (++Ap) >= astklm) {
				--Ap;
				error2(astkof,stksize);
			}

			while (isspace(t=getchr()))
				;

			putbak(t);
		} else
			stkchr(t);
	}

	if (Cp!=NULL)
		error("EOF in argument list", NULL);

	delexit(final_exit_status);
}

char	*inpmatch(s)
register char	*s;
{
	register char	*tp = token+1;

	while (*s) {
		*tp = getchr();

		if (*tp++ != *s++) {
			*tp = EOS;
			pbstr(token+1);
			return 0;
		}
	}

	*tp = EOS;
	return token;
}

getflags(xargc,xargv)
register int	*xargc;
register char 	***xargv;
{
        int delimiter = 1;
	while (*xargc > 1 && delimiter) {
		register char	*arg = (*xargv)[1];

		if (arg[0]!='-' || arg[1]==EOS)
			break;

		switch (arg[1]) {
		case 'B':
			bufsize = atoi(&arg[2]);
			break;
		case 'D':
			{
			register char *t;
			char *s[2];

			initalloc();

			t = s[0] = &arg[2];
			if (*t == NULL) {
			   (*xargv)++;
			   --(*xargc);
			   if (*xargc > 1) {
			     arg = (*xargv)[1];
			     t = s[0] = &arg[0];
                           } else {
			     fprintf(stderr,"Argument for -D is missing\n");
			     delexit(NOT_OK);
                           }
                         }
                        while (*t) {
				if (*t=='=') {
					*t++ = EOS;
					break;
				} else {
				   t++;
                                }
                        }
			s[1] = t;
			dodef(&s[-1],2);
			break;
                        }
		case 'H':
			hshsize = atoi(&arg[2]);
			break;
		case 'S':
			stksize = atoi(&arg[2]);
			break;
		case 'T':
			toksize = atoi(&arg[2]);
			break;
		case 'U':
			{
			char *s[1];

			initalloc();
			s[0] = &arg[2];
			if (*s[0] == NULL) {
			  (*xargv)++;
			  --(*xargc);
			  if (*xargc > 1) {
			    arg = (*xargv)[1];
			    s[0] = &arg[0];
                          } else {
			     fprintf(stderr,"Argument for -U is missing\n");
			     delexit(NOT_OK);
                          }

                        }
			doundef(&s[-1],1);
			break;
			}
		case 'e':
#ifdef unix
			setbuf(stdout,(char *) NULL);
			signal(SIGINT,SIG_IGN);
#endif
			break;
		case 's':
			/* turn on line sync */
			sflag = 1;
			break;

                case '-':
			delimiter = 0;
			break;
		case 'I':
			if (nincls >= MAXINCLS) {
			     fprintf(stderr,"Too many -I arguments\n");
			     delexit(NOT_OK);
			}
			incls[nincls++] = &arg[2];
			break;
		default:
			fprintf(stderr,"%s: bad option: %s\n",
				procnam,arg);
			delexit(NOT_OK);
		}

		(*xargv)++;
		--(*xargc);
	}

	return;
}

initalloc()
{
	static	done = 0;
	register	t;

	if (done++)
		return;

	hshtab = (struct nlist **) xcalloc(hshsize,sizeof(struct nlist *));
	callst = (struct call *) xcalloc(stksize/3+1,sizeof(struct call));
	Ap = argstk = (char **) xcalloc(stksize+3,sizeof(char *));
	ipstk[0] = ipflr = ip = ibuf = xcalloc(bufsize+1,sizeof(char));
	op = obuf = xcalloc(bufsize+1,sizeof(char));
	token = xcalloc(toksize+1,sizeof(char));

	astklm = (char *) (&argstk[stksize]);
	ibuflm = &ibuf[bufsize];
	obuflm = &obuf[bufsize];
	toklm = &token[toksize];

	for (t=0; barray[t].bname; ++t) {
		static char	p[2] = {0, EOS};

		p[0] = t|~LOW7;
		install(barray[t].bname,p,NOPUSH);
	}

#ifdef unix
	install("unix",nullstr,NOPUSH);
#endif

#ifdef gcos
	install("gcos",nullstr,NOPUSH);
#endif
}

struct nlist	*
install(nam,val,mode)
char	*nam;
register char	*val;
{
	register struct nlist *np;
	register char	*cp;
	int		l;

	if (mode==PUSH)
		lookup(nam);	/* lookup sets hshval */
	else
		while (undef(nam))	/* undef calls lookup */
			;

	np = (struct nlist *) xcalloc(1,sizeof(*np));
	np->name = copy(nam);
	np->next = hshtab[hshval];
	hshtab[hshval] = np;

	cp = xcalloc((l=strlen(val))+1,sizeof(*val));
	np->def = cp;
	cp = &cp[l];

	while (*val)
		*--cp = *val++;
}

struct nlist	*
lookup(str)
char 	*str;
{
	register char		*s1;
	register struct nlist	*np;
	static struct nlist	nodef;

	s1 = str;

	for (hshval = 0; *s1; )
		hshval += *s1++;

	hshval %= hshsize;

	for (np = hshtab[hshval]; np!=NULL; np = np->next) {
		if (!strcmp(str, np->name))
			return(np);
	}

	return(&nodef);
}

expand(a1,c)
char	**a1;
{
	register char	*dp;
	register struct nlist	*sp;

	sp = (struct nlist *) a1[-1];

	if (sp->tflag || trace) {
		int	i;

		fprintf(stderr,"Trace(%d): %s",Cp-callst,a1[0]);

		if (c > 0) {
			fprintf(stderr,"(%s",chkbltin(a1[1]));
			for (i=2; i<=c; ++i)
				fprintf(stderr,",%s",chkbltin(a1[i]));
			fprintf(stderr,")");
		}

		fprintf(stderr,"\n");
	}

	dp = sp->def;

	for (; *dp; ++dp) {
		if (*dp&~LOW7) {
			(*barray[*dp&LOW7].bfunc)(a1,c);
		} else if (dp[1]=='$') {
			if (isdigit(*dp)) {
				register	n;
				if ((n = *dp-'0') <= c)
					pbstr(a1[n]);
				++dp;
			} else if (*dp=='#') {
				pbnum((long) c);
				++dp;
			} else if (*dp=='*' || *dp=='@') {
				register i = c;
				char **a = a1;

				if (i > 0)
					for (;;) {
						if (*dp=='@')
							pbstr(rquote);

						pbstr(a[i--]);

						if (*dp=='@')
							pbstr(lquote);

						if (i <= 0)
							break;

						pbstr(",");
					}
				++dp;
			} else 
				putbak(*dp);
		} else 
			putbak(*dp);
	}
}

setfname(s)
register char 	*s;
{
	strcpy(fname[ifx],s);
	fname[ifx+1] = fname[ifx]+strlen(s)+1;
	fline[ifx] = 1;
	nflag = 1;
	lnsync(stdout);
}

lnsync(iop)
register FILE	*iop;
{
	static int cline = 0;
	static int cfile = 0;

	if (!sflag || iop!=stdout)
		return;

	if (nflag || ifx!=cfile) {
		nflag = 0;
		cfile = ifx;
		fprintf(iop,"#line %d \"",cline = fline[ifx]);
		fpath(iop);
		fprintf(iop,"\"\n");
	} else if (++cline != fline[ifx])
		fprintf(iop,"#line %d\n",cline = fline[ifx]);
}

fpath(iop)
register FILE	*iop;
{
	register	i;

	fprintf(iop,"%s",fname[0]);

	for (i=1; i<=ifx; ++i)
		fprintf(iop,":%s",fname[i]);
}

/* ARGUSED */
void catchsig(i)
int i;
{
	i = 0;
#ifdef unix
	(void) signal(SIGHUP,SIG_IGN);
	(void) signal(SIGINT,SIG_IGN);
#endif

	delexit(NOT_OK);
}

delexit(code)
{
	register i;

	cf = stdout;

/*	if (ofx != 0) {	/* quitting in middle of diversion */
/*		ofx = 0;
/*		code = NOT_OK;
/*	}
*/
	ofx = 0;	/* ensure that everything comes out */
	for (i=1; i<10; i++)
		undiv(i,code);

	tempfile[7] = 'a';
	unlink(tempfile);

	/* if (code==OK) */
		exit(code);

	/* _exit(code); */
}

puttok(tp)
register char *tp;
{
	if (Cp) {
		while (*tp)
			stkchr(*tp++);
	} else if (cf)
		while (*tp)
			sputchr(*tp++,cf);
        }

pbstr(str)
register char *str;
{
	register char *p;

	for (p = str + strlen(str); --p >= str; )
		putbak(*p);
}

undiv(i,code)
register	i;
{
	register FILE *fp;
	register	c;

	if (i<1 || i>9 || i==ofx || !ofile[i])
		return;

	fclose(ofile[i]);
	tempfile[7] = 'a'+i;

	if (code==OK && cf) {
		fp = xfopen(tempfile,"r");

		while ((c=getc(fp)) != EOF)
			sputchr(c,cf);

		fclose(fp);
	}

	unlink(tempfile);
	ofile[i] = NULL;
}

char 	*copy(s)
register char *s;
{
	register char *p;

	p = xcalloc(strlen(s)+1,sizeof(char));
	strcpy(p, s);
	return(p);
}

pbnum(num)
long num;
{
	pbnbr(num,10,1);
}

pbnbr(nbr,base,len)
long	nbr;
register	base, len;
{
	register	neg = 0;

	if (base<=0)
		return;

	if (nbr<0)
		neg = 1;
	else
		nbr = -nbr;

	while (nbr<0) {
		register int	i;
		if (base>1) {
			i = nbr%base;
			nbr /= base;
#		if (-3 % 2) != -1
			while (i > 0) {
				i -= base;
				++nbr;
			}
#		endif
			i = -i;
		} else {
			i = 1;
			++nbr;
		}
		putbak(itochr(i));
		--len;
	}

	while (--len >= 0)
		putbak('0');

	if (neg)
		putbak('-');
}

itochr(i)
register	i;
{
	if (i>9)
		return i-10+'A';
	else
		return i+'0';
}

long ctol(str)
register char *str;
{
	register sign;
	long num;

	while (isspace(*str))
		++str;
	num = 0;
	if (*str=='-') {
		sign = -1;
		++str;
	}
	else
		sign = 1;
	while (isdigit(*str))
		num = num*10 + *str++ - '0';
	return(sign * num);
}

min(a,b)
{
	if (a>b)
		return(b);
	return(a);
}

FILE	*
xfopen(name,mode)
char	*name,
	*mode;
{
	FILE	*fp;

	if ((fp=fopen(name,mode))==NULL)
		error(badfile, name);

	return fp;
}

char	*xcalloc(nbr,size)
{
	register char	*ptr;

	if ((ptr=calloc((unsigned) nbr,(unsigned) size)) == NULL)
		error(nocore, NULL);

	return ptr;
}

error2(str,num)
	char *str;
	int num;
{
	char buf[500];

	sprintf(buf,str,num);
	error(buf, NULL);
	return 0;
}

error(str, str2)
	char *str, *str2;
{
	fprintf(stderr,"\n%s:",procnam);
	fpath(stderr);
	fprintf(stderr,":%d %s",fline[ifx],str);
	if (str2)
		fprintf(stderr, ":%s", str2);
	fprintf(stderr, "\n");
	if (Cp) {
		register struct call	*mptr;

		/* fix limit */
		*op = EOS;
		(Cp+1)->argp = Ap+1;

		for (mptr=callst; mptr<=Cp; ++mptr) {
			register char	**aptr, **lim;

			aptr = mptr->argp;
			lim = (mptr+1)->argp-1;
			if (mptr==callst)
				fputs(*aptr,stderr);
			++aptr;
			fputs("(",stderr);
			if (aptr < lim)
				for (;;) {
					fputs(*aptr++,stderr);
					if (aptr >= lim)
						break;
					fputs(",",stderr);
				}
		}
		while (--mptr >= callst)
			fputs(")",stderr);

		fputs("\n",stderr);
	}
	delexit(NOT_OK);
}

char	*chkbltin(s)
char	*s;
{
	static char	buf[24];

	if (*s&~LOW7){
		sprintf(buf,"<%s>",barray[*s&LOW7].bname);
		return buf;
	}

	return s;
}

int	getchr()
{
	if (ip > ipflr)
		return (*--ip);
	C = feof(ifile[ifx]) ? EOF : getc(ifile[ifx]);
	if (C =='\n')
		fline[ifx]++;
	return (C);
}
