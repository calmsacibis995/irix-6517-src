/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright(c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/

#ident	"@(#)sh:name.c	1.14.9.1"
/*
 * UNIX shell
 */

#include	<pfmt.h>
#include	"hash.h"
#include	"defs.h"

extern unsigned char	*simple();
extern long lseek();

extern int	mailchk;

void	setlist();
void	replace();
void	dfault();
void	assign();
void	assnum();
void	setup_env();
void	unset_name();
void	namscan();
void	setup_tfm();
static void	namwalk();
static void	set_builtins_path();
static void	exname();
static void	pushnam();
static void	setname();
static BOOL	chkid();
static int	patheq();

struct namnod ps2nod =
{
	(struct namnod *)0,
	(struct namnod *)&acctnod,
	(unsigned char *)ps2name
};
struct namnod cdpnod = 
{
	(struct namnod *)0,
	(struct namnod *)0,
	(unsigned char *)cdpname
};
struct namnod pathnod =
{
	(struct namnod *)0,
	(struct namnod *)0,
	(unsigned char *)pathname
};
struct namnod ifsnod =
{
	&homenod,
	&mailnod,
	(unsigned char *)ifsname
};
struct namnod ps1nod =
{
	&pathnod,
	&ps2nod,
	(unsigned char *)ps1name
};
struct namnod homenod =
{
	&cdpnod,
	(struct namnod *)0,
	(unsigned char *)homename
};
struct namnod mailnod =
{
	(struct namnod *)0,
	&mailpnod,
	(unsigned char *)mailname
};
struct namnod mchknod =
{
	&ifsnod,
	&ps1nod,
	(unsigned char *)mchkname
};
struct namnod acctnod =
{
	(struct namnod *)0,
#ifdef SECURITY_FEATURES
	&tfadminnod,
#else
	(struct namnod *)0,
#endif
	(unsigned char *)acctname
};
struct namnod mailpnod =
{
	(struct namnod *)0,
	(struct namnod *)0,
	(unsigned char *)mailpname
};
#ifdef SECURITY_FEATURES
struct namnod tfadminnod =
{
	(struct namnod *)0,
	&tmoutnod,
	(unsigned char *)tfadminname
};
struct namnod tmoutnod =
{
	(struct namnod *)0,
	(struct namnod *)0,
	(unsigned char *)timeoutname
};
#endif /* SECURITY_FEATURES */

static struct namnod *namep = &mchknod;

/* ========	variable and string handling	======== */
int
syslook(w, syswds, n)
	register unsigned char *w;
	register struct sysnod syswds[];
	int n;
{
	int	low;
	int	high;
	int	mid;
	register int cond;

	if (w == 0 || *w == 0)
		return(0);

	low = 0;
	high = n - 1;

	while (low <= high)
	{
		mid = (low + high) / 2;

		if ((cond = cf(w, syswds[mid].sysnam)) < 0)
			high = mid - 1;
		else if (cond > 0)
			low = mid + 1;
		else
			return(syswds[mid].sysval);
	}
	return(0);
}

void
setlist(arg, xp)
register struct argnod *arg;
int	xp;
{
	if (flags & exportflg)
		xp |= N_EXPORT;

	while (arg)
	{
		register unsigned char *s = mactrim(arg->argval);
		setname(s, xp);
		arg = arg->argnxt;
		if (flags & execpr)
		{
			prs(s);
			if (arg)
				blank();
			else
				newline();
		}
	}
}

static void
setname(argi, xp)	/* does parameter assignments */
unsigned char	*argi;
int	xp;
{
	register unsigned char *argscan = argi;
	register struct namnod *n;

	if (letter(*argscan))
	{
		while (alphanum(*argscan))
			argscan++;

		if (*argscan == '=')
		{
			*argscan = 0;	/* make name a cohesive string */

			n = lookup(argi);
			*argscan++ = '=';
#ifdef sgi
			/*
			 * If someone tries to pass IFS in the
			 * environment, and they are running with
			 * a setuid/gid or as root, then don't
			 * let them do it.  This closes a
			 * security hole in popen() and system()
			 * when called from a setuid/gid
			 * process.  This fix is styled after a
			 * similar fix in the BSD /bin/sh.
			 */
			if (xp & N_ENVNAM)
				if (n == &ifsnod) {
					int uid;
			    		if ((uid=getuid()) != geteuid() ||
					         getgid()  != getegid() || !uid)
						return;
				}
#endif
			attrib(n, xp);
			if (xp & N_ENVNAM)
			{
				n->namenv = n->namval = argscan;
				if (n == &pathnod)
					set_builtins_path();
			}
			else
				assign(n, argscan);
		}
	}
}

void
replace(a, v)
register unsigned char	**a;
unsigned char	*v;
{
	free(*a);
	*a = make(v);
}

void
dfault(n, v)
struct namnod *n;
unsigned char	*v;
{
	if (n->namval == NULL || *n->namval == '\0') 
		assign(n, v);
	/*
         * to fix bug id 11318
         * if MAILCHECK is defined but not given a value make it NULL
         */

}

void
assign(n, v)
struct namnod *n;
unsigned char	*v;
{
	if (n->namflg & N_RDONLY)	{
		failure(0, n->namid, wtfailed, wtfailedid);
		return;
	}

#ifndef RES	
	else if (flags & rshflg)
	{
		if (n == &pathnod || eq(n->namid,"SHELL"))
			failed(0, n->namid, restricted, restrictedid);
	}
#endif

	else if (n->namflg & N_FUNCTN)
	{
		func_unhash(n->namid);
		freefunc(n);

		n->namenv = 0;
		n->namflg = N_DEFAULT;
	}

	if (n == &mchknod)
	{
		mailchk = stoi(v);
	}
		
	replace(&n->namval, v);
	attrib(n, N_ENVCHG);

	/* Change internal locale if necessary */
	if (eq(n->namid, "LANG"))
		(void)setlocale(LC_ALL, (char *)v);
	else if (eq(n->namid, "LC_MESSAGES"))
		(void)setlocale(LC_MESSAGES, (char *)v);

	if (n == &pathnod)
	{
		zaphash();
		set_dotpath();
		set_builtins_path();
		return;
	}
	
	if (flags & prompt)
	{
		if ((n == &mailpnod) || (n == &mailnod && mailpnod.namflg == N_DEFAULT))
			setmail(n->namval);
	}
}

static void
set_builtins_path()
{
        register unsigned char *path;

        ucb_builtins = 0;
#ifdef sgi
	return;
#else
        path = getpath("");
        while (path && *path)
        {
                if (patheq(path, "/usr/ucb"))
                {
                        ucb_builtins++;
                        break;
                }
                else if (patheq(path, "/usr/bin"))
                        break;
                path = nextpath(path);
        }
#endif
}
 
static int
patheq(component, dir)
register unsigned char   *component;
register char   *dir;
{
        register unsigned char   c;
 
        for (;;)
        {
                c = *component++;
                if (c == COLON)
                        c = '\0';       /* end of component of path */
                if (c != *dir++)
                        return(0);
                if (c == '\0')
                        return(1);
        }
}

int 
readvar(names)
unsigned char	**names;
{
	struct fileblk	fb;
	register struct fileblk *f = &fb;
	unsigned char	c[MULTI_BYTE_MAX+1];
	register int	rc = 0;
	struct namnod *n = lookup(*names++);	/* done now to avoid storage mess */
	unsigned char	*rel = (unsigned char *)relstak();
	unsigned char *oldstak;
	register unsigned char *pc, *rest, d;

	push(f);
	initf(dup(0));

	if (lseek(0, 0L, 1) == -1)
		f->fsiz = 1;

	/*
	 * strip leading IFS characters
	 */
	for (;;) 
	{
		d = nextc();
		if(eolchar(d))
			break;
		rest = readw(d);
		pc = c;
		while(*pc++ = *rest++);
		if(!anys(c, ifsnod.namval))
			break;
	}
	
	oldstak = curstak();
	for (;;)
	{
		if ((*names && anys(c, ifsnod.namval)) || eolchar(d))
		{
			zerostak();
			assign(n, absstak(rel));
			setstak(rel);

			if(flags & exportflg)	{
				attrib(n, N_EXPORT);
			}

			if (*names)
				n = lookup(*names++);
			else
				n = 0;
			if (eolchar(d))
			{
				break;
			}
			else		/* strip imbedded IFS characters */
				for(;;) {
					d = nextc();
					if(eolchar(d))
						break;
					rest = readw(d);
					pc = c;
					while(*pc++ = *rest++);
					if(!anys(c, ifsnod.namval))
						break;
				}
		}
		else
		{
			if(d == '\\') {
				d = readc();
				rest = readw(d);
				while(d = *rest++)
					pushstak(d);
				oldstak = staktop;
			}
			else
			{
				pc = c;
				while(d = *pc++) 
					pushstak(d);
				if(!anys(c, ifsnod.namval))
					oldstak = staktop;
			}
			d = nextc();

			if (eolchar(d))
				staktop = oldstak;
			else 
			{
				rest = readw(d);
				pc = c;
				while(*pc++ = *rest++);
			}
		}
	}
	while (n)
	{
		assign(n, (char *)nullstr);

		if(flags & exportflg)	{
			attrib(n, N_EXPORT);
		}

		if (*names)
			n = lookup(*names++);
		else
			n = 0;
	}

	if (eof)
		rc = 1;
	lseek(0, (long)(f->fnxt - f->fend), 1);
	pop();
	return(rc);
}

void
assnum(p, i)
unsigned char	**p;
long	i;
{
	int j = ltos(i);
	replace(p, &numbuf[j]);
}

unsigned char *
make(v)
unsigned char	*v;
{
	register unsigned char	*p;

	if (v)
	{
		movstr(v, p = (unsigned char *)alloc(length(v)));
		return(p);
	}
	else
		return(0);
}


struct namnod *
lookup(nam)
	register unsigned char	*nam;
{
	register struct namnod *nscan = namep;
	register struct namnod **prev;
	int		LR;

	if (!chkid(nam))
		failed(0, nam, notid, notidid);
	
	while (nscan)
	{
		if ((LR = cf(nam, nscan->namid)) == 0)
			return(nscan);

		else if (LR < 0)
			prev = &(nscan->namlft);
		else
			prev = &(nscan->namrgt);
		nscan = *prev;
	}
	/*
	 * add name node
	 */
	nscan = (struct namnod *)alloc(sizeof *nscan);
	nscan->namlft = nscan->namrgt = (struct namnod *)0;
	nscan->namid = make(nam);
	nscan->namval = 0;
	nscan->namflg = N_DEFAULT;
	nscan->namenv = 0;

	return(*prev = nscan);
}

static BOOL
chkid(nam)
unsigned char	*nam;
{
	register unsigned char *cp = nam;

	if (!letter(*cp))
		return(FALSE);
	else
	{
		while (*++cp)
		{
			if (!alphanum(*cp))
				return(FALSE);
		}
	}
	return(TRUE);
}

static void (*namfn)();

void
namscan(fn)
	void	(*fn)();
{
	namfn = fn;
	namwalk(namep);
}

static void
namwalk(np)
register struct namnod *np;
{
	if (np)
	{
		namwalk(np->namlft);
		(*namfn)(np);
		namwalk(np->namrgt);
	}
}

void
printnam(n)
struct namnod *n;
{
	register unsigned char	*s;

	sigchk();

	if (n->namflg & N_FUNCTN)
	{
		prs_buff(n->namid);
		prs_buff("() ");
		prf(n->namenv);
		prc_buff(NL);
	}
	else if (s = n->namval)
	{
		prs_buff(n->namid);
		prc_buff('=');
		prs_buff(s);
		prc_buff(NL);
	}
}

static unsigned char *
staknam(n)
register struct namnod *n;
{
	register unsigned char	*p;

	p = movstr(n->namid, staktop);
	p = movstr("=", p);
	p = movstr(n->namval, p);
	return((unsigned char *)getstak(p + 1 - stakbot));
}

static int namec;

static void
exname(n)
	register struct namnod *n;
{
	register int 	flg = n->namflg;

	if (flg & N_ENVCHG)
	{

		if (flg & N_EXPORT)
		{
			free(n->namenv);
			n->namenv = make(n->namval);
		}
		else
		{
			free(n->namval);
			n->namval = make(n->namenv);
		}
	}

	
	if (!(flg & N_FUNCTN))
		n->namflg = N_DEFAULT;

	if (n->namval)
		namec++;

}

void
printro(n)
register struct namnod *n;
{
	if (n->namflg & N_RDONLY)
	{
		prs_buff(gettxt(readonlyid, readonly));
		prc_buff(SP);
		prs_buff(n->namid);
		prc_buff(NL);
	}
}

void
printexp(n)
register struct namnod *n;
{
	if (n->namflg & N_EXPORT)
	{
		prs_buff(gettxt(exportid, export));
		prc_buff(SP);
		prs_buff(n->namid);
		prc_buff(NL);
	}
}

void
setup_env()
{
	register unsigned char **e = environ;

	while (*e)
		setname(*e++, N_ENVNAM);
}


static unsigned char **argnam;

static void
pushnam(n)
struct namnod *n;
{
	if (n->namval)
		*argnam++ = staknam(n);
}

unsigned char **
setenv()
{
	register unsigned char	**er;

	namec = 0;
	namscan(exname);

	argnam = er = (unsigned char **)getstak(namec * BYTESPERWORD + BYTESPERWORD);
	namscan(pushnam);
	*argnam++ = 0;
	return(er);
}

struct namnod *
findnam(nam)
	register unsigned char	*nam;
{
	register struct namnod *nscan = namep;
	int		LR;

	if (!chkid(nam))
		return(0);
	while (nscan)
	{
		if ((LR = cf(nam, nscan->namid)) == 0)
			return(nscan);
		else if (LR < 0)
			nscan = nscan->namlft;
		else
			nscan = nscan->namrgt;
	}
	return(0); 
}


void
unset_name(name)
	register unsigned char 	*name;
{
	register struct namnod	*n;

	if (n = findnam(name))
	{
		if (n->namflg & N_RDONLY)
			failed(0, name, wtfailed, wtfailedid);

		if (n == &pathnod ||
		    n == &ifsnod ||
		    n == &ps1nod ||
		    n == &ps2nod ||
		    n == &mchknod)
		{
			failed(0, name, badunset, badunsetid);
		}

#ifndef RES

		if ((flags & rshflg) && eq(name, "SHELL"))
			failed(0, name, restricted, restrictedid);

#endif

		if (n->namflg & N_FUNCTN)
		{
			func_unhash(name);
			freefunc(n);
		}
		else
		{
			free(n->namval);
			free(n->namenv);
		}

		n->namval = n->namenv = 0;
		n->namflg = N_DEFAULT;

		if (flags & prompt)
		{
			if (n == &mailpnod)
				setmail(mailnod.namval);
			else if (n == &mailnod && mailpnod.namflg == N_DEFAULT)
				setmail(0);
		}
	}
}

#ifdef SECURITY_FEATURES
/*
** Routine to set up the $TFADMIN environment variable for shell scripts
** and other shell sessions that need it.
*/
void
setup_tfm()
{
	struct	namnod	*n;

	n = lookup("TFADMIN");
	assign(n, "/sbin/tfadmin ");
	n->namflg = N_EXPORT | N_ENVNAM;
}
#endif /* SECURITY_FEATURES */
