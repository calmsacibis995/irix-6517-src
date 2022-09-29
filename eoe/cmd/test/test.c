/*
 *	test expression
 *	[ expression ]
 *
 *	This is a program residing in /sbin which may be used by either shell.
 *	All System V test facilities are available.
 */

#include <pfmt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#define EQ(a,b)	((tmp=a)==0?0:(strcmp(tmp,b)==0))

int	ap;
int	ac;
char	**av;
char	*tmp;

int	exp(void), e1(void), e2(void), e3(void);
int	fsizep(char *);
int	ftype(char *, int);
int	length(char *);
char	*nxtarg(int);
int	tio(char *, int);

main(int argc, char *argv[])
{

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:test");
	ac = argc; av = argv; ap = 1;
	if(EQ(argv[0],"[")) {
		if(!EQ(argv[--ac],"]"))
			pfmt(stderr, MM_ERROR, ":476:] missing\n");
	}
	argv[ac] = 0;
	if (ac<=1) exit(1);
	exit(exp()?0:1);
}

char *
nxtarg(int mt)
{
	if (ap>=ac) {
		if(mt) {
			ap++;
			return(0);
		}
		pfmt(stderr, MM_ERROR, ":477:Argument expected\n");
	}
	return(av[ap++]);
}

exp()
{
	int p1;

	p1 = e1();
	if (EQ(nxtarg(1), "-o")) return(p1 | exp());
	ap--;
	return(p1);
}

e1()
{
	int p1;

	p1 = e2();
	if (EQ(nxtarg(1), "-a")) return (p1 & e1());
	ap--;
	return(p1);
}

e2()
{
	if (EQ(nxtarg(0), "!"))
		return(!e3());
	ap--;
	return(e3());
}

e3()
{
	int p1;
	register char *a;
	char *p2;
	int64_t int1, int2;

	a=nxtarg(0);
	if(EQ(a, "(")) {
		p1 = exp();
		if(!EQ(nxtarg(0), ")")) 
		    pfmt(stderr, MM_ERROR, ":478:) expected\n");
		return(p1);
	}

	if(EQ(a, "-r"))
		return(tio(nxtarg(0), 4));

	if(EQ(a, "-w"))
		return(tio(nxtarg(0), 2));

	if(EQ(a, "-x"))
		return(tio(nxtarg(0), 1));

	if(EQ(a, "-d"))
		return(ftype(nxtarg(0),S_IFDIR));

	if(EQ(a, "-e"))
		return(exists(nxtarg(0)));

	if(EQ(a, "-f"))
		return(ftype(nxtarg(0),S_IFREG));

	if(EQ(a, "-c"))
		return(ftype(nxtarg(0),S_IFCHR));

	if(EQ(a, "-b"))
		return(ftype(nxtarg(0),S_IFBLK));

	if(EQ(a, "-p"))
		return(ftype(nxtarg(0),S_IFIFO));

	if(EQ(a, "-l") || EQ(a, "-L") || EQ(a, "-h"))
		return(ftype(nxtarg(0),S_IFLNK));

	if(EQ(a, "-u"))
		return(ftype(nxtarg(0),S_ISUID));

	if(EQ(a, "-g"))
		return(ftype(nxtarg(0),S_ISGID));

	if(EQ(a, "-k"))
		return(ftype(nxtarg(0),S_ISVTX));

	if(EQ(a, "-s"))
		return(fsizep(nxtarg(0)));

	if(EQ(a, "-t"))
		if(ap>=ac)
			return(isatty(1));
		else
			return(isatty(atoi(nxtarg(0))));

	if(EQ(a, "-n"))
		return(!EQ(nxtarg(0), ""));
	if(EQ(a, "-z"))
		return(EQ(nxtarg(0), ""));

	p2 = nxtarg(1);
	if (p2==0)
		return(!EQ(a,""));
	if(EQ(p2, "="))
		return(EQ(nxtarg(0), a));

	if(EQ(p2, "!="))
		return(!EQ(nxtarg(0), a));

	int1 = atoll(a);
	if(a = nxtarg(0))
		int2 = atoll(a);
	else	return(0);

	if(EQ(p2, "-eq"))
		return(int1==int2);
	if(EQ(p2, "-ne"))
		return(int1!=int2);
	if(EQ(p2, "-gt"))
		return(int1>int2);
	if(EQ(p2, "-lt"))
		return(int1<int2);
	if(EQ(p2, "-ge"))
		return(int1>=int2);
	if(EQ(p2, "-le"))
		return(int1<=int2);

	pfmt(stderr, MM_ERROR, ":539:Unknown operator: %s\n", p2);
	return(0);
}

tio(char *a, int f)
{
	register int accessible = !access(a, f);
	register int mode;
	struct stat64 stb;

	if (f != 1 || !accessible)
		return accessible;
	/*
	 * f == 1 && accessible
	 */
	if (stat64(a, &stb) < 0)
		return 0;
	mode = stb.st_mode;
	return ((mode & S_IFMT) == S_IFREG
	    && (mode & (S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6))) != 0);
}

ftype(char *f, int field)
{
	struct stat64 statb;

	if (field == S_IFLNK) {
		if(lstat64(f,&statb)<0)
			return(0);
	} else {
		if(stat64(f,&statb)<0)
			return(0);
	}
				/*
				 * Testing for (statb.st_mode & field)
				 * will fail if file is block special and
				 * we test for either dir or char.
				 */
	if (field & S_IFMT)
		return (statb.st_mode & S_IFMT) == field;
	else
				/* Extra code copes with multi-bit weirdness */
		return (statb.st_mode & field) == field;
}

fsizep(char *f)
{
	struct stat64 statb;
	if(stat64(f,&statb)<0)
		return(0);
	return(statb.st_size>0);
}

exists(char *f)
{
	struct stat64 statb;
	if(stat64(f,&statb)<0)
		return(0);
	return(1);
}

length(char *s)
{
	char *es=s;
	while(*es++);
	return(es-s-1);
}
