/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:hashserv.c	1.10.13.1"

/* 	Portions Copyright(c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/
/*
 *	UNIX shell
 */

#include	"hash.h"
#include	"defs.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<errno.h>
#include	<unistd.h>

#define		EXECUTE		01

static unsigned char	cost;
static int	dotpath;
static int	multrel;
static struct entry	relcmd;

static int	argpath();
int		findpath();
static void	pr_path();
extern uid_t	sveuid;

short
pathlook(com, flg, arg)
	unsigned char	*com;
	int		flg;
	register struct argnod	*arg;
{
	register unsigned char	*name = com;
	register ENTRY	*h;

	ENTRY		hentry;
	int		count = 0;
	int		i;
	int		oldpath = 0;



	hentry.data = 0;

	if (any('/', name))
		return(COMMAND);

	h = hfind(name);


	if (h)
	{
		if (h->data & (BUILTIN | FUNCTION))
		{
			if (flg)
				h->hits++;
			return(h->data);
		}

		if (arg && argpath(arg))
			return(PATH_COMMAND);

		if ((h->data & DOT_COMMAND) == DOT_COMMAND)
		{
			if (multrel == 0 && hashdata(h->data) > dotpath)
				oldpath = hashdata(h->data);
			else
				oldpath = dotpath;

			h->data = 0;
			goto pathsrch;
		}

		if (h->data & (COMMAND | REL_COMMAND))
		{
			if (flg)
				h->hits++;
			return(h->data);
		}

		h->data = 0;
		h->cost = 0;
	}

	if (i = syslook(name, commands, no_commands))
	{
		hentry.data = (BUILTIN | i);
		count = 1;
	}
	else
	{
		if (arg && argpath(arg))
			return(PATH_COMMAND);
pathsrch:
			count = findpath(name, oldpath);
	}

	if (count > 0)
	{
		if (h == 0)
		{
			hentry.cost = 0;
			hentry.key = make(name);
			h = henter(hentry);
		}

		if (h->data == 0)
		{
			if (count < dotpath)
				h->data = COMMAND | count;
			else
			{
				h->data = REL_COMMAND | count;
				h->next = relcmd.next;
				relcmd.next = h;
			}
		}


		h->hits = flg;
		h->cost += cost;
		return((short)h->data);
	}
	if ( count == -3)
	{
		return(1);
	}
	return(-count);
}
			

static void
zapentry(h)
	ENTRY *h;
{
	h->data &= HASHZAP;
}

void
zaphash()
{
	hscan(zapentry);
	relcmd.next = 0;
}

void 
zapcd()
{
	ENTRY *ptr = relcmd.next;
	
	while (ptr)
	{
		ptr->data |= CDMARK;
		ptr = ptr->next;
	}
	relcmd.next = 0;
}


static void
hashout(h)
	ENTRY *h;
{
	sigchk();

	if (hashtype(h->data) == NOTFOUND)
		return;

	if (h->data & (BUILTIN | FUNCTION))
		return;

	prn_buff(h->hits);

	if (h->data & REL_COMMAND)
		prc_buff('*');


	prc_buff(TAB);
	prn_buff(h->cost);
	prc_buff(TAB);

	pr_path(h->key, hashdata(h->data));
	prc_buff(NL);
}

void
hashpr()
{
	prs_buff(gettxt(":480", "hits	cost	command\n"));
	hscan(hashout);
}

void
set_dotpath()
{
	register unsigned char	*path;
	register int	cnt = 1;

	dotpath = 10000;
	path = getpath("");

	while (path && *path)
	{
		if (*path == '/')
			cnt++;
		else
		{
			if (dotpath == 10000)
				dotpath = cnt;
			else
			{
				multrel = 1;
				return;
			}
		}
	
		path = nextpath(path);
	}

	multrel = 0;
}

void
hash_func(name)
	unsigned char *name;
{
	ENTRY	*h;
	ENTRY	hentry;

	h = hfind(name);

	if (h)
		h->data = FUNCTION;
	else
	{
		hentry.data = FUNCTION;
		hentry.key = make(name);
		hentry.cost = 0;
		hentry.hits = 0;
		(void)henter(hentry);
	}
}

void
func_unhash(name)
	unsigned char *name;
{
	ENTRY 	*h;
	int i;

	h = hfind(name);

	if (h && (h->data & FUNCTION)) {
		if(i = syslook(name, commands, no_commands))
			h->data = (BUILTIN|i);
		else
			h->data = NOTFOUND;
	}
}


short
hash_cmd(name)
	unsigned char *name;
{
	ENTRY	*h;

	if (any('/', name))
		return(COMMAND);

	h = hfind(name);

	if (h)
	{
		if (h->data & (BUILTIN | FUNCTION))
			return(h->data);
		else if ((h->data & REL_COMMAND) == REL_COMMAND)
		{ /* unlink h from relative command list */
			ENTRY *ptr = &relcmd;
			while(ptr-> next != h)
				ptr = ptr->next;
			ptr->next = h->next;
		}
		zapentry(h);
	}

	return(pathlook(name, 0, (struct argnod *)0));
}

int
what_is_path(name)
	register unsigned char *name;
{
	register ENTRY	*h;
	int		cnt;
	short	hashval;

	h = hfind(name);

	prs_buff(name);
	if (h)
	{
		hashval = hashdata(h->data);

		switch (hashtype(h->data))
		{
			case BUILTIN:
				prs_buff(gettxt(":481", " is a shell builtin\n"));
				return(0);
	
			case FUNCTION:
			{
				struct namnod *n = lookup(name);

				prs_buff(gettxt(":482", " is a function\n"));
				prs_buff(name);
				prs_buff("() ");
				prf(n->namenv);
				prc_buff(NL);
				return(0);
			}
	
			case REL_COMMAND:
			{
				short hash;

				if ((h->data & DOT_COMMAND) == DOT_COMMAND)
				{
					hash = pathlook(name, 0, (struct argnod *)0);
					if (hashtype(hash) == NOTFOUND)
					{
						prs_buff(gettxt(":483", " not found\n"));
						return(1);
					}
					else
						hashval = hashdata(hash);
				}
			}

			case COMMAND:	/*FALLTHROUGH*/
				prs_buff(gettxt(":484", " is hashed ("));
				pr_path(name, hashval);
				prs_buff(")\n");
				return(0);
		}
	}

	if (syslook(name, commands, no_commands))
	{
		prs_buff(gettxt(":481", " is a shell builtin\n"));
		return(0);
	}

	if ((cnt = findpath(name, 0)) > 0)
	{
		prs_buff(gettxt(":485", " is "));
		pr_path(name, cnt);
		prc_buff(NL);
		return(0);
	}
	else	{
		prs_buff(gettxt(":483", " not found\n"));
		return(1);
	}
}

int
findpath(name, oldpath)
	register unsigned char *name;
	int oldpath;
{
	register unsigned char 	*path;
	register int	count = 1;

	unsigned char	*p;
	int	ok = 1;
	int 	e_code = 1;
	
	cost = 0;
	path = getpath(name);

	if (oldpath)
	{
		count = dotpath;
		while (--count)
			path = nextpath(path);

		if (oldpath > dotpath)
		{
			catpath(path, name);
			p = curstak();
			cost = 1;

			if ((ok = chk_access(p, X_OK)) == 0)
				return(dotpath);
			else
				return(oldpath);
		}
		else 
			count = dotpath;
	}

	while (path)
	{
		path = catpath(path, name);
		cost++;
		p = curstak();

		if ((ok = chk_access(p, X_OK)) == 0)
			break;
		else
			e_code = max(e_code, ok);

		count++;
	}

	return(ok ? -e_code : count);
}

/*
 * Determine if file given by name is accessible with permissions
 * given by mode.
 */
int
chk_access(name, mode)
register unsigned char	*name;
mode_t mode; 
{	
	struct stat statb;

	/* IRIX 4 compatibility */
	if (sveuid == 0 && mode == X_OK) {
		if (stat((char *)name, &statb) == 0) {
			/* root can execute file as long as it has execute 
			   permission for someone */
			if (statb.st_mode & (S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6)))
				return (0);
			else
				return (3);
		}
		return 1;
	}

	if (access((char *)name, EFF_ONLY_OK | mode) == 0) {
		return(0);
	}

	return (errno == EACCES ? 3 : 1);
}

static void
pr_path(name, count)
	register unsigned char	*name;
	int count;
{
	register unsigned char	*path;

	path = getpath(name);

	while (--count && path)
		path = nextpath(path);

	catpath(path, name);
	prs_buff(curstak());
}


static int
argpath(arg)
	register struct argnod	*arg;
{
	register unsigned char 	*s;
	register unsigned char	*start;

	while (arg)
	{
		s = arg->argval;
		start = s;

		if (letter(*s))		
		{
			while (alphanum(*s))
				s++;

			if (*s == '=')
			{
				*s = 0;

				if (eq(start, pathname))
				{
					*s = '=';
					return(1);
				}
				else
					*s = '=';
			}
		}
		arg = arg->argnxt;
	}

	return(0);
}
