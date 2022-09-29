/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/

#ident	"@(#)sh:test.c	1.14.11.1"
/*
 *      test expression
 *      [ expression ]
 */

#include	"defs.h"
#include	"hash.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern	int lstat();
static int	ap, ac;
static unsigned char **av;

static int	e1();
static int	e2();
static int	e3();
static int	exp();
static int	filtyp();
static int	fsizep();
static int	ftype();
static unsigned char	*nxtarg();

int
test(argn, com)
unsigned char *com[];
int	argn;
{
	ac = argn;
	av = com;
	ap = 1;
	if (eq(com[0],"["))
	{
		if (!eq(com[--ac], "]"))
			error(SYSTST, "] missing", ":476");
	}
	com[ac] = 0;
	if (ac <= 1)
		return(1);
	return(exp() ? 0 : 1);
}

static unsigned char *
nxtarg(mt)
{
	if (ap >= ac)
	{
		if (mt)
		{
			ap++;
			return(0);
		}
		error(SYSTST, "Argument expected", ":477");
	}
	return(av[ap++]);
}

static int
exp()
{
	int	p1;
	unsigned char	*p2;

	p1 = e1();
	p2 = nxtarg(1);
	if (p2 != 0)
	{
		if (eq(p2, "-o"))
			return(p1 | exp());

		/* if (!eq(p2, ")"))
			failed("test", synmsg); */
	}
	ap--;
	return(p1);
}

static int
e1()
{
	int	p1;
	unsigned char	*p2;

	p1 = e2();
	p2 = nxtarg(1);

	if ((p2 != 0) && eq(p2, "-a"))
		return(p1 & e1());
	ap--;
	return(p1);
}

static int
e2()
{
	if (eq(nxtarg(0), "!"))
		return(!e3());
	ap--;
	return(e3());
}

static int
e3()
{
	int	p1;
	register unsigned char	*a;
	unsigned char	*p2;
	long	int1, int2;

	a = nxtarg(0);
	if (eq(a, "("))
	{
		p1 = exp();
		if (!eq(nxtarg(0), ")"))
			error(SYSTST, ") expected", ":478");
		return(p1);
	}
	p2 = nxtarg(1);
	ap--;
	if ((p2 == 0) || (!eq(p2, "=") && !eq(p2, "!=")))
	{
		if (eq(a, "-r"))
			return(chk_access(nxtarg(0), R_OK) == 0);
		if (eq(a, "-w"))
			return(chk_access(nxtarg(0), W_OK) == 0);
		if (eq(a, "-x"))
			return(chk_access(nxtarg(0), X_OK) == 0);
		if (eq(a, "-d"))
			return(filtyp(nxtarg(0), S_IFDIR));
		if (eq(a, "-c"))
			return(filtyp(nxtarg(0), S_IFCHR));
		if (eq(a, "-b"))
			return(filtyp(nxtarg(0), S_IFBLK));
		if (eq(a, "-f"))
			return(filtyp(nxtarg(0), S_IFREG));
		if (eq(a, "-u"))
			return(ftype(nxtarg(0), S_ISUID));
		if (eq(a, "-g"))
			return(ftype(nxtarg(0), S_ISGID));
		if (eq(a, "-k"))
			return(ftype(nxtarg(0), S_ISVTX));
		if (eq(a, "-p"))
			return(filtyp(nxtarg(0), S_IFIFO));
		if (eq(a, "-h"))
			return(filtyp(nxtarg(0), S_IFLNK));
		if (eq(a, "-L") || eq(a, "-l"))
			return(filtyp(nxtarg(0), S_IFLNK));
   		if (eq(a, "-s"))
			return(fsizep(nxtarg(0)));
		if (eq(a, "-t"))
		{
			if (ap >= ac)		/* no args */
				return(isatty(1));
			else if (eq((a = nxtarg(0)), "-a") || eq(a, "-o"))
			{
				ap--;
				return(isatty(1));
			}
			else
				return(isatty(atoi((char *)a)));
		}
		if (eq(a, "-n"))
			return(!eq(nxtarg(0), ""));
		if (eq(a, "-z"))
			return(eq(nxtarg(0), ""));
	}

	p2 = nxtarg(1);
	if (p2 == 0)
		return(!eq(a, ""));
	if (eq(p2, "-a") || eq(p2, "-o"))
	{
		ap--;
		return(!eq(a, ""));
	}
	if (eq(p2, "="))
		return(eq(nxtarg(0), a));
	if (eq(p2, "!="))
		return(!eq(nxtarg(0), a));
	int1 = atol((char *)a);
	int2 = atol((char *)nxtarg(0));
	if (eq(p2, "-eq"))
		return(int1 == int2);
	if (eq(p2, "-ne"))
		return(int1 != int2);
	if (eq(p2, "-gt"))
		return(int1 > int2);
	if (eq(p2, "-lt"))
		return(int1 < int2);
	if (eq(p2, "-ge"))
		return(int1 >= int2);
	if (eq(p2, "-le"))
		return(int1 <= int2);

	failed(SYSTST, p2, badop, badopid);
/* NOTREACHED */
}

static int
ftype(f, field)
unsigned char	*f;
int	field;
{
	struct stat statb;

	if (stat((char *)f, &statb) < 0)
		return(0);
	if ((statb.st_mode & field) == field)
		return(1);
	return(0);
}

static int
filtyp(f,field)
unsigned char	*f;
int field;
{
	struct stat statb;
	int (*statf)() = (field == S_IFLNK) ? lstat : stat;

	if ((*statf)(f, &statb) < 0)
		return(0);
	if ((statb.st_mode & S_IFMT) == field)
		return(1);
	else
		return(0);
}

static int
fsizep(f)
unsigned char *f;
{
	struct stat statb;

	if (stat((char *)f, &statb) < 0)
		return(0);
	return(statb.st_size > 0);
}
