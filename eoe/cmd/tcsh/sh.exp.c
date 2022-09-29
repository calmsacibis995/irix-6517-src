/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/tcsh/RCS/sh.exp.c,v 1.2 1993/07/15 17:52:27 pdc Exp $ */
/*
 * sh.exp.c: Expression evaluations
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "sh.h"

RCSID("$Id: sh.exp.c,v 1.2 1993/07/15 17:52:27 pdc Exp $")

/*
 * C shell
 */

#define IGNORE	1	/* in ignore, it means to ignore value, just parse */
#define NOGLOB	2	/* in ignore, it means not to globone */

#define	ADDOP	1
#define	MULOP	2
#define	EQOP	4
#define	RELOP	8
#define	RESTOP	16
#define	ANYOP	31

#define	EQEQ	1
#define	GTR	2
#define	LSS	4
#define	NOTEQ	6
#define EQMATCH 7
#define NOTEQMATCH 8

static	int	 sh_access	__P((Char *, int));
static	int	 exp1		__P((Char ***, bool));
static	int	 exp2		__P((Char ***, bool));
static	int	 exp2a		__P((Char ***, bool));
static	int	 exp2b		__P((Char ***, bool));
static	int	 exp2c		__P((Char ***, bool));
static	Char 	*exp3		__P((Char ***, bool));
static	Char 	*exp3a		__P((Char ***, bool));
static	Char 	*exp4		__P((Char ***, bool));
static	Char 	*exp5		__P((Char ***, bool));
static	Char 	*exp6		__P((Char ***, bool));
static	void	 evalav		__P((Char **));
static	int	 isa		__P((Char *, int));
static	int	 egetn		__P((Char *));

#ifdef EDEBUG
static	void	 etracc		__P((char *, Char *, Char ***));
static	void	 etraci		__P((char *, int, Char ***));
#endif


/*
 * shell access function according to POSIX and non POSIX
 * From Beto Appleton (beto@aixwiz.aix.ibm.com)
 */
static int
sh_access(fname, mode)
    Char *fname;
    int mode;
{
#ifdef POSIX
    struct stat     statb;
#endif /* POSIX */
    char *name = short2str(fname);

    if (*name == '\0')
	return 1;

#ifndef POSIX
    return access(name, mode);
#else /* POSIX */

    /*
     * POSIX 1003.2-d11.2 
     *	-r file		True if file exists and is readable. 
     *	-w file		True if file exists and is writable. 
     *			True shall indicate only that the write flag is on. 
     *			The file shall not be writable on a read-only file
     *			system even if this test indicates true.
     *	-x file		True if file exists and is executable. 
     *			True shall indicate only that the execute flag is on. 
     *			If file is a directory, true indicates that the file 
     *			can be searched.
     */
    if (mode != W_OK && mode != X_OK)
	return access(name, mode);

    if (stat(name, &statb) == -1) 
	return 1;

    if (access(name, mode) == 0) {
#ifdef S_ISDIR
	if (S_ISDIR(statb.st_mode) && mode == X_OK)
	    return 0;
#endif /* S_ISDIR */

	/* root needs permission for someone */
	switch (mode) {
	case W_OK:
	    mode = S_IWUSR | S_IWGRP | S_IWOTH;
	    break;
	case X_OK:
	    mode = S_IXUSR | S_IXGRP | S_IXOTH;
	    break;
	default:
	    abort();
	    break;
	}

    } 

    else if (euid == statb.st_uid)
	mode <<= 6;

    else if (egid == statb.st_gid)
	mode <<= 3;

# ifdef NGROUPS_MAX
    else {
#  if defined(__386BSD__) || defined(BSD4_4)
    /*
     * These two decided that setgroup() should take an array of int's
     * and they define _SC_NGROUPS_MAX without having sysconf
     */
#   undef _SC_NGROUPS_MAX	
#   define GID_T int
#  else
#   define GID_T gid_t
#  endif /* __386BSD__ || BSD4_4 */
	/* you can be in several groups */
	int	n;
	GID_T	*groups;

	/*
	 * Try these things to find a positive maximum groups value:
	 *   1) sysconf(_SC_NGROUPS_MAX)
	 *   2) NGROUPS_MAX
	 *   3) getgroups(0, unused)
	 * Then allocate and scan the groups array if one of these worked.
	 */
#  ifdef _SC_NGROUPS_MAX
	if ((n = sysconf(_SC_NGROUPS_MAX)) == -1)
#  endif /* _SC_NGROUPS_MAX */
	    n = NGROUPS_MAX;
	if (n <= 0)
	    n = getgroups(0, (GID_T *) NULL);

	if (n > 0) {
	    groups = (GID_T *) xmalloc((size_t) (n * sizeof(GID_T)));
	    n = getgroups(n, groups);
	    while (--n >= 0)
		if (groups[n] == statb.st_gid) {
		    mode <<= 3;
		    break;
		}
	}
    }
# endif /* NGROUPS_MAX */

    if (statb.st_mode & mode)
	return 0;
    else
	return 1;
#endif /* !POSIX */
}

int
expr(vp)
    register Char ***vp;
{
    return (exp0(vp, 0));
}

int
exp0(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register int p1 = exp1(vp, ignore);

#ifdef EDEBUG
    etraci("exp0 p1", p1, vp);
#endif
    if (**vp && eq(**vp, STRor2)) {
	register int p2;

	(*vp)++;
	p2 = exp0(vp, (ignore & IGNORE) || p1);
#ifdef EDEBUG
	etraci("exp0 p2", p2, vp);
#endif
	return (p1 || p2);
    }
    return (p1);
}

static int
exp1(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register int p1 = exp2(vp, ignore);

#ifdef EDEBUG
    etraci("exp1 p1", p1, vp);
#endif
    if (**vp && eq(**vp, STRand2)) {
	register int p2;

	(*vp)++;
	p2 = exp1(vp, (ignore & IGNORE) || !p1);
#ifdef EDEBUG
	etraci("exp1 p2", p2, vp);
#endif
	return (p1 && p2);
    }
    return (p1);
}

static int
exp2(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register int p1 = exp2a(vp, ignore);

#ifdef EDEBUG
    etraci("exp3 p1", p1, vp);
#endif
    if (**vp && eq(**vp, STRor)) {
	register int p2;

	(*vp)++;
	p2 = exp2(vp, ignore);
#ifdef EDEBUG
	etraci("exp3 p2", p2, vp);
#endif
	return (p1 | p2);
    }
    return (p1);
}

static int
exp2a(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register int p1 = exp2b(vp, ignore);

#ifdef EDEBUG
    etraci("exp2a p1", p1, vp);
#endif
    if (**vp && eq(**vp, STRcaret)) {
	register int p2;

	(*vp)++;
	p2 = exp2a(vp, ignore);
#ifdef EDEBUG
	etraci("exp2a p2", p2, vp);
#endif
	return (p1 ^ p2);
    }
    return (p1);
}

static int
exp2b(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register int p1 = exp2c(vp, ignore);

#ifdef EDEBUG
    etraci("exp2b p1", p1, vp);
#endif
    if (**vp && eq(**vp, STRand)) {
	register int p2;

	(*vp)++;
	p2 = exp2b(vp, ignore);
#ifdef EDEBUG
	etraci("exp2b p2", p2, vp);
#endif
	return (p1 & p2);
    }
    return (p1);
}

static int
exp2c(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register Char *p1 = exp3(vp, ignore);
    register Char *p2;
    register int i;

#ifdef EDEBUG
    etracc("exp2c p1", p1, vp);
#endif
    if ((i = isa(**vp, EQOP)) != 0) {
	(*vp)++;
	if (i == EQMATCH || i == NOTEQMATCH)
	    ignore |= NOGLOB;
	p2 = exp3(vp, ignore);
#ifdef EDEBUG
	etracc("exp2c p2", p2, vp);
#endif
	if (!(ignore & IGNORE))
	    switch (i) {

	    case EQEQ:
		i = eq(p1, p2);
		break;

	    case NOTEQ:
		i = !eq(p1, p2);
		break;

	    case EQMATCH:
		i = Gmatch(p1, p2);
		break;

	    case NOTEQMATCH:
		i = !Gmatch(p1, p2);
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (i);
    }
    i = egetn(p1);
    xfree((ptr_t) p1);
    return (i);
}

static Char *
exp3(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register Char *p1, *p2;
    register int i;

    p1 = exp3a(vp, ignore);
#ifdef EDEBUG
    etracc("exp3 p1", p1, vp);
#endif
    if ((i = isa(**vp, RELOP)) != 0) {
	(*vp)++;
	if (**vp && eq(**vp, STRequal))
	    i |= 1, (*vp)++;
	p2 = exp3(vp, ignore);
#ifdef EDEBUG
	etracc("exp3 p2", p2, vp);
#endif
	if (!(ignore & IGNORE))
	    switch (i) {

	    case GTR:
		i = egetn(p1) > egetn(p2);
		break;

	    case GTR | 1:
		i = egetn(p1) >= egetn(p2);
		break;

	    case LSS:
		i = egetn(p1) < egetn(p2);
		break;

	    case LSS | 1:
		i = egetn(p1) <= egetn(p2);
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp3a(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register Char *p1, *p2, *op;
    register int i;

    p1 = exp4(vp, ignore);
#ifdef EDEBUG
    etracc("exp3a p1", p1, vp);
#endif
    op = **vp;
    if (op && any("<>", op[0]) && op[0] == op[1]) {
	(*vp)++;
	p2 = exp3a(vp, ignore);
#ifdef EDEBUG
	etracc("exp3a p2", p2, vp);
#endif
	if (op[0] == '<')
	    i = egetn(p1) << egetn(p2);
	else
	    i = egetn(p1) >> egetn(p2);
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp4(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register Char *p1, *p2;
    register int i = 0;

    p1 = exp5(vp, ignore);
#ifdef EDEBUG
    etracc("exp4 p1", p1, vp);
#endif
    if (isa(**vp, ADDOP)) {
	register Char *op = *(*vp)++;

	p2 = exp4(vp, ignore);
#ifdef EDEBUG
	etracc("exp4 p2", p2, vp);
#endif
	if (!(ignore & IGNORE))
	    switch (op[0]) {

	    case '+':
		i = egetn(p1) + egetn(p2);
		break;

	    case '-':
		i = egetn(p1) - egetn(p2);
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp5(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    register Char *p1, *p2;
    register int i = 0;

    p1 = exp6(vp, ignore);
#ifdef EDEBUG
    etracc("exp5 p1", p1, vp);
#endif
    if (isa(**vp, MULOP)) {
	register Char *op = *(*vp)++;

	p2 = exp5(vp, ignore);
#ifdef EDEBUG
	etracc("exp5 p2", p2, vp);
#endif
	if (!(ignore & IGNORE))
	    switch (op[0]) {

	    case '*':
		i = egetn(p1) * egetn(p2);
		break;

	    case '/':
		i = egetn(p2);
		if (i == 0)
		    stderror(ERR_DIV0);
		i = egetn(p1) / i;
		break;

	    case '%':
		i = egetn(p2);
		if (i == 0)
		    stderror(ERR_MOD0);
		i = egetn(p1) % i;
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp6(vp, ignore)
    register Char ***vp;
    bool    ignore;
{
    int     ccode, i = 0;
    register Char *cp, *dp, *ep;

    if (**vp == 0)
	stderror(ERR_NAME | ERR_EXPRESSION);
    if (eq(**vp, STRbang)) {
	(*vp)++;
	cp = exp6(vp, ignore);
#ifdef EDEBUG
	etracc("exp6 ! cp", cp, vp);
#endif
	i = egetn(cp);
	xfree((ptr_t) cp);
	return (putn(!i));
    }
    if (eq(**vp, STRtilde)) {
	(*vp)++;
	cp = exp6(vp, ignore);
#ifdef EDEBUG
	etracc("exp6 ~ cp", cp, vp);
#endif
	i = egetn(cp);
	xfree((ptr_t) cp);
	return (putn(~i));
    }
    if (eq(**vp, STRLparen)) {
	(*vp)++;
	ccode = exp0(vp, ignore);
#ifdef EDEBUG
	etraci("exp6 () ccode", ccode, vp);
#endif
	if (*vp == 0 || **vp == 0 || ***vp != ')')
	    stderror(ERR_NAME | ERR_EXPRESSION);
	(*vp)++;
	return (putn(ccode));
    }
    if (eq(**vp, STRLbrace)) {
	register Char **v;
	struct command faket;
	Char   *fakecom[2];

	faket.t_dtyp = NODE_COMMAND;
	faket.t_dflg = F_BACKQ;
	faket.t_dcar = faket.t_dcdr = faket.t_dspr = NULL;
	faket.t_dcom = fakecom;
	fakecom[0] = STRfakecom;
	fakecom[1] = NULL;
	(*vp)++;
	v = *vp;
	for (;;) {
	    if (!**vp)
		stderror(ERR_NAME | ERR_MISSING, '}');
	    if (eq(*(*vp)++, STRRbrace))
		break;
	}
	if (ignore & IGNORE)
	    return (Strsave(STRNULL));
	psavejob();
	if (pfork(&faket, -1) == 0) {
	    *--(*vp) = 0;
	    evalav(v);
	    exitstat();
	}
	pwait();
	prestjob();
#ifdef EDEBUG
	etraci("exp6 {} status", egetn(value(STRstatus)), vp);
#endif
	return (putn(egetn(value(STRstatus)) == 0));
    }
    if (isa(**vp, ANYOP))
	return (Strsave(STRNULL));
    cp = *(*vp)++;
#define FILETESTS "erwxfdzoplstSX"
    if (*cp == '-' && any(FILETESTS, cp[1])) {
	struct stat stb;
	Char *ft;

	for (ft = &cp[2]; *ft; ft++) 
	    if (!any(FILETESTS, *ft))
		stderror(ERR_NAME | ERR_FILEINQ);
	/*
	 * Detect missing file names by checking for operator in the file name
	 * position.  However, if an operator name appears there, we must make
	 * sure that there's no file by that name (e.g., "/") before announcing
	 * an error.  Even this check isn't quite right, since it doesn't take
	 * globbing into account.
	 */
	if (isa(**vp, ANYOP) && stat(short2str(**vp), &stb))
	    stderror(ERR_NAME | ERR_FILENAME);

	dp = *(*vp)++;
	if (ignore & IGNORE)
	    return (Strsave(STRNULL));
	ep = globone(dp, G_ERROR);
	ft = &cp[1];
	do 
	    switch (*ft) {

	    case 'r':
		i = !sh_access(ep, R_OK);
		break;

	    case 'w':
		i = !sh_access(ep, W_OK);
		break;

	    case 'x':
		i = !sh_access(ep, X_OK);
		break;

	    case 'X':	/* tcsh extension, name is an executable in the path
			 * or a tcsh builtin command 
			 */
		i = find_cmd(ep, 0);
		break;

	    case 't':	/* SGI extension, true when file is a tty */
		i = isatty(atoi(short2str(ep)));
		break;

	    default:
		if (
#ifdef S_IFLNK
		    *ft == 'l' ? lstat(short2str(ep), &stb) :
#endif
		    stat(short2str(ep), &stb)) {
		    xfree((ptr_t) ep);
		    return (Strsave(STR0));
		}
		switch (*ft) {

		case 'f':
#ifdef S_ISREG
		    i = S_ISREG(stb.st_mode);
#else
		    i = 0;
#endif
		    break;

		case 'd':
#ifdef S_ISDIR
		    i = S_ISDIR(stb.st_mode);
#else
		    i = 0;
#endif
		    break;

		case 'p':
#ifdef S_ISFIFO
		    i = S_ISFIFO(stb.st_mode);
#else
		    i = 0;
#endif
		    break;

		case 'l':
#ifdef S_ISLNK
		    i = S_ISLNK(stb.st_mode);
#else
		    i = 0;
#endif
		    break;

		case 'S':
# ifdef S_ISSOCK
		    i = S_ISSOCK(stb.st_mode);
# else
		    i = 0;
# endif
		    break;

		case 'b':
#ifdef S_ISBLK
		    i = S_ISBLK(stb.st_mode);
#else
		    i = 0;
#endif
		    break;

		case 'c':
#ifdef S_ISCHR
		    i = S_ISCHR(stb.st_mode);
#else
		    i = 0;
#endif
		    break;

		case 'u':
#ifdef S_ISUID
		    i = (S_ISUID & stb.st_mode) != 0;
#else
		    i = 0;
#endif
		    break;

		case 'g':
#ifdef S_ISGID
		    i = (S_ISGID & stb.st_mode) != 0;
#else
		    i = 0;
#endif
		    break;

		case 'k':
#ifdef S_ISVTX
		    i = (S_ISVTX & stb.st_mode) != 0;
#else
		    i = 0;
#endif
		    break;

		case 'z':
		    i = stb.st_size == 0;
		    break;

		case 's':
		    i = stb.st_size != 0;
		    break;

		case 'e':
		    i = 1;
		    break;

		case 'o':
		    i = stb.st_uid == uid;
		    break;
		}
	    }
	while (*++ft && i);
#ifdef EDEBUG
	etraci("exp6 -? i", i, vp);
#endif
	xfree((ptr_t) ep);
	return (putn(i));
    }
#ifdef EDEBUG
    etracc("exp6 default", cp, vp);
#endif
    return (ignore & NOGLOB ? Strsave(cp) : globone(cp, G_ERROR));
}

static void
evalav(v)
    register Char **v;
{
    struct wordent paraml1;
    register struct wordent *hp = &paraml1;
    struct command *t;
    register struct wordent *wdp = hp;

    set(STRstatus, Strsave(STR0), VAR_READWRITE);
    hp->prev = hp->next = hp;
    hp->word = STRNULL;
    while (*v) {
	register struct wordent *new =
	(struct wordent *) xcalloc(1, sizeof *wdp);

	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	wdp->word = Strsave(*v++);
    }
    hp->prev = wdp;
    alias(&paraml1);
    t = syntax(paraml1.next, &paraml1, 0);
    if (seterr)
	stderror(ERR_OLD);
    execute(t, -1, NULL, NULL);
    freelex(&paraml1), freesyn(t);
}

static int
isa(cp, what)
    register Char *cp;
    register int what;
{
    if (cp == 0)
	return ((what & RESTOP) != 0);
    if (cp[1] == 0) {
	if (what & ADDOP && (*cp == '+' || *cp == '-'))
	    return (1);
	if (what & MULOP && (*cp == '*' || *cp == '/' || *cp == '%'))
	    return (1);
	if (what & RESTOP && (*cp == '(' || *cp == ')' || *cp == '!' ||
			      *cp == '~' || *cp == '^' || *cp == '"'))
	    return (1);
    }
    else if (cp[2] == 0) {
	if (what & RESTOP) {
	    if (cp[0] == '|' && cp[1] == '&')
		return (1);
	    if (cp[0] == '<' && cp[1] == '<')
		return (1);
	    if (cp[0] == '>' && cp[1] == '>')
		return (1);
	}
	if (what & EQOP) {
	    if (cp[0] == '=') {
		if (cp[1] == '=')
		    return (EQEQ);
		if (cp[1] == '~')
		    return (EQMATCH);
	    }
	    else if (cp[0] == '!') {
		if (cp[1] == '=')
		    return (NOTEQ);
		if (cp[1] == '~')
		    return (NOTEQMATCH);
	    }
	}
    }
    if (what & RELOP) {
	if (*cp == '<')
	    return (LSS);
	if (*cp == '>')
	    return (GTR);
    }
    return (0);
}

static int
egetn(cp)
    register Char *cp;
{
    if (*cp && *cp != '-' && !Isdigit(*cp))
	stderror(ERR_NAME | ERR_EXPRESSION);
    return (getn(cp));
}

/* Phew! */

#ifdef EDEBUG
static void
etraci(str, i, vp)
    char   *str;
    int     i;
    Char ***vp;
{
    xprintf("%s=%d\t", str, i);
    blkpr(*vp);
    xputchar('\n');
}
static void
etracc(str, cp, vp)
    char   *str;
    Char   *cp;
    Char ***vp;
{
    xprintf("%s=%s\t", str, cp);
    blkpr(*vp);
    xputchar('\n');
}
#endif
