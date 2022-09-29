/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)awk:run.c	2.16"

/* SGI BUG FIX: Don't dereference x if it is null */
#define tempfree(x,s)	if ((x) && istemp(x)) tfree(x,s); else

/* #define	execute(p)	(isvalue(p) ? (Cell *)((p)->narg[0]) : r_execute(p)) */
#define	execute(p) r_execute(p)

#define DEBUG
#include	"awk.h"
#include	<math.h>
#include	"y.tab.h"
#include	<stdio.h>
#include	<ctype.h>
#include	<setjmp.h>
#include	<pfmt.h>
#include	<string.h>
#include	<errno.h>

#ifndef	FOPEN_MAX
#define	FOPEN_MAX	15	/* max number of open files, from ANSI std. */
#endif


jmp_buf env;

#define	getfval(p)	(((p)->tval & (ARR|FLD|REC|NUM)) == NUM ? (p)->fval : r_getfval(p))
#define	getsval(p)	(((p)->tval & (ARR|FLD|REC|STR)) == STR ? (p)->sval : r_getsval(p, 0))

extern	Awkfloat r_getfval();
extern	uchar	*r_getsval();
extern	Cell	*r_execute(), *fieldel(), *dopa2(), *gettemp(), *copycell();
extern	FILE	*openfile(), *redirect();
extern	double	errcheck();

#define PA2NUM	29
int	pairstack[PA2NUM], paircnt;
Node	*winner = NULL;
Cell	*tmps;

static Cell	truecell	={ OBOOL, BTRUE, 0, 0, 1.0, NUM };
Cell	*true	= &truecell;
static Cell	falsecell	={ OBOOL, BFALSE, 0, 0, 0.0, NUM };
Cell	*false	= &falsecell;
static Cell	breakcell	={ OJUMP, JBREAK, 0, 0, 0.0, NUM };
Cell	*jbreak	= &breakcell;
static Cell	contcell	={ OJUMP, JCONT, 0, 0, 0.0, NUM };
Cell	*jcont	= &contcell;
static Cell	nextcell	={ OJUMP, JNEXT, 0, 0, 0.0, NUM };
Cell	*jnext	= &nextcell;
static Cell	exitcell	={ OJUMP, JEXIT, 0, 0, 0.0, NUM };
Cell	*jexit	= &exitcell;
static Cell	retcell		={ OJUMP, JRET, 0, 0, 0.0, NUM };
Cell	*jret	= &retcell;
static Cell	tempcell	={ OCELL, CTEMP, 0, 0, 0.0, NUM };

Node	*curnode = NULL;	/* the node being executed, for debugging */

static	const	char
	restoobig[] = ":40:%s() result %.20s too big",
	notarray[] = ":41:%s is not an array",
	ioerror[] = ":42:I/O error occurred on %s";
const char
	illstat[] = ":43:Illegal statement";

extern	const	char	readvofid[], readvof[], badopen[];
void closeall(), tfree();

void
run(a) Node *a;
{
	execute(a);
	closeall();
}

Cell *r_execute(u) Node *u;
{
	register Cell *(*proc)();
	register Cell *x;
	register Node *a;

	if (u == NULL)
		return(true);
	for (a = u; ; a = a->nnext) {
		curnode = a;
		if (isvalue(a)) {
			x = (Cell *) (a->narg[0]);
			if ((x->tval & FLD) && !donefld)
				fldbld();
			else if ((x->tval & REC) && !donerec)
				recbld();
			return(x);
		}
		if (notlegal(a->nobj))	/* probably a Cell* but too risky to print */
			error(MM_ERROR, illstat);
		proc = proctab[a->nobj-FIRSTTOKEN];
		x = (*proc)(a->narg, a->nobj);
		if ((x->tval & FLD) && !donefld)
			fldbld();
		else if ((x->tval & REC) && !donerec)
			recbld();
		if (isexpr(a))
			return(x);
		/* a statement, goto next statement */
		if (isjump(x))
			return(x);
		if (a->nnext == (Node *)NULL)
			return(x);
		tempfree(x, "execute");
	}
}


Cell *program(a, n) register Node **a;
{
	register Cell *x;
	register int   i;

	if (setjmp(env) != 0)
		goto ex;
	if (a[0]) {		/* BEGIN */
		x = execute(a[0]);
		if (isexit(x))
			return(true);
		if (isjump(x))
			error(MM_ERROR,
				":44:Illegal break, continue or next from BEGIN");
		tempfree(x, "");
	}
	/* XPG Initialize command line args now */
	if(_XPG == 1) {
		for(i=0;i<Argc;i++) {
			if (isclvar(Argv[i])) {
				setclvar(Argv[i]);
				free(Argv[i]);
			}
		}
	}
  loop:
	if (a[1] || a[2])
		while (getrec(record) > 0) {
			x = execute(a[1]);
			if (isexit(x))
				break;
			tempfree(x, "");
		}
  ex:
	if (setjmp(env) != 0)
		goto ex1;
	if (a[2]) {		/* END */
		x = execute(a[2]);
		if (iscont(x))	/* read some more */
			goto loop;
		if (isbreak(x) || isnext(x))
			error(MM_ERROR, ":45:Illegal break or next from END");
		tempfree(x, "");
	}
  ex1:
	return(true);
}

struct Frame {
	int nargs;	/* number of arguments in this call */
	Cell *fcncell;	/* pointer to Cell for function */
	Cell **args;	/* pointer to array of arguments after execute */
	Cell *retval;	/* return value */
};

#define	NARGS	30

struct Frame *frame = NULL;	/* base of stack frames; dynamically allocated */
int	nframe = 0;		/* number of frames allocated */
struct Frame *fp = NULL;	/* frame pointer. bottom level unused */

Cell *call(a, n) Node **a;
{
	static Cell newcopycell = { OCELL, CCOPY, 0, (uchar *) "", 0.0, NUM|STR|DONTFREE };
	int i, ncall, ndef;
	Node *x;
	Cell *args[NARGS], *oargs[NARGS], *y, *z, *fcn;
	uchar *s;

	fcn = execute(a[0]);	/* the function itself */
	s = fcn->nval;
	if (!isfunc(fcn))
		error(MM_ERROR, ":46:Calling undefined function %s", s);
	if (frame == NULL) {
		fp = frame = (struct Frame *) calloc(nframe += 100, sizeof(struct Frame));
		if (frame == NULL)
			error(MM_ERROR, ":47:Out of space for stack frames calling %s", s);
	}
	for (ncall = 0, x = a[1]; x != NULL; x = x->nnext)	/* args in call */
		ncall++;
	ndef = (int) fcn->fval;			/* args in defn */
	dprintf( ("calling %s, %d args (%d in defn), fp=%d\n", s,
		ncall, ndef, fp-frame) );
	if (ncall > ndef)
		if (ncall == 1)
			error(MM_WARNING, ":48:Function %s called with 1 arg, uses only %d",
				s, ndef);
		else
			error(MM_WARNING, ":49:Function %s called with %d args, uses only %d",
				s, ncall, ndef);
	if (ncall + ndef > NARGS)
		error(MM_ERROR, ":50:Function %s has %d arguments, limit %d",
			s, ncall+ndef, NARGS);
	for (i = 0, x = a[1]; x != NULL; i++, x = x->nnext) {	/* get call args */
		dprintf( ("evaluate args[%d], fp=%d:\n", i, fp-
			frame) );
		y = execute(x);
		oargs[i] = y;
		dprintf( ("args[%d]: %s %f <%s>, t=%o\n",
			   i, y->nval, y->fval, isarr(y) ?
			   	"(array)" : (char*) y->sval, y->tval) );
		if (isfunc(y))
			error(MM_ERROR, ":51:Cannot use function %s as argument in %s",
				y->nval, s);
		if (isarr(y))
			args[i] = y;	/* arrays by ref */
		else
			args[i] = copycell(y);
		tempfree(y, "callargs");
	}
	for ( ; i < ndef; i++) {	/* add null args for ones not provided */
		args[i] = gettemp("nullargs");
		*args[i] = newcopycell;
	}
	fp++;	/* now ok to up frame */
	if (fp >= frame + nframe) {
		int dfp = fp - frame;	/* old index */
		frame = (struct Frame *)
			realloc(frame, (nframe += 100) * sizeof(struct Frame));
		if (frame == NULL)
			error(MM_ERROR, ":52:Out of space for stack frames in %s", s);
		fp = frame + dfp;
	}
	fp->fcncell = fcn;
	fp->args = args;
	fp->nargs = ndef;	/* number defined with (excess are locals) */
	fp->retval = gettemp("retval");

	dprintf( ("start exec of %s, fp=%d\n", s, fp-frame) );

	/* SGI BUG FIX: Don't try and execute null function bodies */
	if (!fcn->sval)
		y = true;
	else
		y = execute((Node *)(fcn->sval));	/* execute body */

	dprintf( ("finished exec of %s, fp=%d\n", s, fp-frame) );

	for (i = 0; i < ndef; i++) {
		Cell *t = fp->args[i];
		if (isarr(t)) {
			if (t->csub == CCOPY) {
				if (i >= ncall) {
					freesymtab(t);
					t->csub = CTEMP;
				} else {
					oargs[i]->tval = t->tval;
					oargs[i]->tval &= ~(STR|NUM|DONTFREE);
					oargs[i]->sval = t->sval;
					tempfree(t, "oargsarr");
				}
			}
		} else if (t != y) {	/* kludge to prevent freeing twice */
			t->csub = CTEMP;
			tempfree(t, "fp->args");
		}
	}
	tempfree(fcn, "call.fcn");
	if (isexit(y) || isnext(y))
		return y;
	tempfree(y, "fcn ret");		/* this can free twice! */
	z = fp->retval;			/* return value */
	dprintf( ("%s returns %g |%s| %o\n", s, getfval(z), 
		getsval(z), z->tval) );
	fp--;
	return(z);
}

Cell *copycell(x)	/* make a copy of a cell in a temp */
	Cell *x;
{
	Cell *y;

	y = gettemp("copycell");
	y->csub = CCOPY;	/* prevents freeing until call is over */
	y->nval = x->nval;
	y->sval = x->sval ? tostring(x->sval) : NULL;
	y->fval = x->fval;
	y->tval = x->tval & ~(CON|FLD|REC|DONTFREE);	/* copy is not constant or field */
							/* is DONTFREE right? */
	return y;
}

Cell *arg(a) Node **a;
{
	int n;

	n = (int) a[0];	/* argument number, counting from 0 */
	dprintf( ("arg(%d), fp->nargs=%d\n", n, fp->nargs) );
	if (n+1 > fp->nargs)
		error(MM_ERROR, ":53:Argument #%d of function %s was not supplied",
			n+1, fp->fcncell->nval);
	return fp->args[n];
}

Cell *jump(a, n) Node **a;
{
	register Cell *y;

	switch (n) {
	case EXIT:
		if (a[0] != NULL) {
			y = execute(a[0]);
			errorflag = getfval(y);
			tempfree(y, "");
		}
		longjmp(env, 1);
	case RETURN:
		if (a[0] != NULL) {
			y = execute(a[0]);
			if ((y->tval & (STR|NUM)) == (STR|NUM)) {
				setsval(fp->retval, getsval(y));
				fp->retval->fval = getfval(y);
				fp->retval->tval |= NUM;
			}
			else if (y->tval & STR)
				setsval(fp->retval, getsval(y));
			else if (y->tval & NUM)
				setfval(fp->retval, getfval(y));
			tempfree(y, "");
		}
		return(jret);
	case NEXT:
		return(jnext);
	case BREAK:
		return(jbreak);
	case CONTINUE:
		return(jcont);
	default:	/* can't happen */
		error(MM_ERROR, ":54:Illegal jump type %d", n);
	}
	return(false);
}

Cell *getline(a, n) Node **a; int n;
{
	/* a[0] is variable, a[1] is operator, a[2] is filename */
	register Cell *r, *x;
	uchar buf[RECSIZE];
	FILE *fp;

	fflush(stdout);	/* in case someone is waiting for a prompt */
	r = gettemp("");
	if (a[1] != NULL) {		/* getline < file */
		x = execute(a[2]);		/* filename */
		if ((int) a[1] == '|')	/* input pipe */
			a[1] = (Node *) LE;	/* arbitrary flag */
		fp = openfile((int) a[1], getsval(x));
		tempfree(x, "");
		if (fp == NULL)
			n = -1;
		else
			n = readrec(buf, sizeof(buf), fp);
		if (n <= 0) {
			;
		} else if (a[0] != NULL) {	/* getline var <file */
			setsval(execute(a[0]), buf);
		} else {			/* getline <file */
			if (!(recloc->tval & DONTFREE))
				xfree(recloc->sval);
			strcpy((char*)record, (char*)buf);
			recloc->sval = record;
			recloc->tval = REC | STR | DONTFREE;
			donerec = 1; donefld = 0;
		}
	} else {			/* bare getline; use current input */
		if (a[0] == NULL)	/* getline */
			n = getrec(record);
		else {			/* getline var */
			n = getrec(buf);
			setsval(execute(a[0]), buf);
		}
	}
	setfval(r, (Awkfloat) n);
	return r;
}

Cell *getnf(a,n) register Node **a;
{
	if (donefld == 0)
		fldbld();
	return (Cell *) a[0];
}

Cell *array(a,n) register Node **a;
{
	register Cell *x, *y, *z;
	register uchar *s;
	register Node *np;
	uchar buf[RECSIZE];

	x = execute(a[0]);	/* Cell* for symbol table */
	buf[0] = 0;
	for (np = a[1]; np; np = np->nnext) {
		y = execute(np);	/* subscript */
		s = getsval(y);
		strcat((char*)buf, (char*)s);
		if (np->nnext)
			strcat((char*)buf, (char*)*SUBSEP);
		tempfree(y, "");
	}
	if (!isarr(x)) {
		dprintf( ("making %s into an array\n", x->nval) );
		if (freeable(x))
			xfree(x->sval);
		x->tval &= ~(STR|NUM|DONTFREE);
		x->tval |= ARR;
		x->sval = (uchar *) makesymtab(NSYMTAB);
	}
	z = setsymtab(buf, "", 0.0, STR|NUM, (Array *) x->sval);
	z->ctype = OCELL;
	z->csub = CVAR;
	tempfree(x, "");
	return(z);
}

Cell *delete(a, n) Node **a;
{
	Cell *x, *y;
	Node *np;
	uchar buf[RECSIZE], *s;

	x = execute(a[0]);	/* Cell* for symbol table */
	if (!isarr(x))
		return true;
	buf[0] = 0;
	for (np = a[1]; np; np = np->nnext) {
		y = execute(np);	/* subscript */
		s = getsval(y);
		strcat((char*)buf, (char*)s);
		if (np->nnext)
			strcat((char*)buf, (char*)*SUBSEP);
		tempfree(y, "");
	}
	freeelem(x, buf);
	tempfree(x, "");
	return true;
}

Cell *intest(a, n) Node **a;
{
	register Cell *x, *ap, *k;
	Node *p;
	char buf[RECSIZE];
	uchar *s;

	ap = execute(a[1]);	/* array name */
	if (!isarr(ap))
		error(MM_ERROR, notarray, ap->nval);
	buf[0] = 0;
	for (p = a[0]; p; p = p->nnext) {
		x = execute(p);	/* expr */
		s = getsval(x);
		strcat(buf, (char*)s);
		tempfree(x, "");
		if (p->nnext)
			strcat(buf, (char*)*SUBSEP);
	}
	k = lookup(buf, (Array *) ap->sval);
	tempfree(ap, "");
	if (k == NULL)
		return(false);
	else
		return(true);
}


Cell *matchop(a,n) Node **a;
{
	register Cell *x, *y;
	register uchar *s, *t;
	register int i;
	extern int match(), pmatch();
	fa *pfa;
	int (*mf)() = match, mode = 0;

	if (n == MATCHFCN) {
		mf = pmatch;
		mode = 1;
	}
	x = execute(a[1]);
	s = getsval(x);
	if (a[0] == 0)
		i = (*mf)(a[2], s, 1);
	else {
		y = execute(a[2]);
		t = getsval(y);
		pfa = makedfa(t, mode);
		i = (*mf)(pfa, s);
		tempfree(y, "");
	}
	tempfree(x, "");
	if (n == MATCHFCN) {
		int start = patbeg - s + 1;
		if (patlen < 0)
			start = 0;
		setfval(rstartloc, (Awkfloat) start);
		setfval(rlengthloc, (Awkfloat) patlen);
		x = gettemp("");
		x->tval = NUM;
		x->fval = start;
		return x;
	} else if (n == MATCH && i == 1 || n == NOTMATCH && i == 0)
		return(true);
	else
		return(false);
}


Cell *boolop(a,n) Node **a;
{
	register Cell *x, *y;
	register int i;

	x = execute(a[0]);
	i = istrue(x);
	tempfree(x, "");
	switch (n) {
	case BOR:
		if (i) return(true);
		y = execute(a[1]);
		i = istrue(y);
		tempfree(y, "");
		if (i) return(true);
		else return(false);
	case AND:
		if ( !i ) return(false);
		y = execute(a[1]);
		i = istrue(y);
		tempfree(y, "");
		if (i) return(true);
		else return(false);
	case NOT:
		if (i) return(false);
		else return(true);
	default:	/* can't happen */
		error(MM_ERROR, ":55:Unknown boolean operator %d", n);
	}
	/*NOTREACHED*/
}

Cell *relop(a,n) Node **a;
{
	register int i;
	register Cell *x, *y;
	Awkfloat j;

	x = execute(a[0]);
	y = execute(a[1]);
	if (x->tval&NUM && y->tval&NUM) {
		j = x->fval - y->fval;
		i = j<0? -1: (j>0? 1: 0);
	} else {
		i = strcmp((char*)getsval(x), (char*)getsval(y));
	}
	tempfree(x, "");
	tempfree(y, "");
	switch (n) {
	case LT:	if (i<0) return(true);
			else return(false);
	case LE:	if (i<=0) return(true);
			else return(false);
	case NE:	if (i!=0) return(true);
			else return(false);
	case EQ:	if (i == 0) return(true);
			else return(false);
	case GE:	if (i>=0) return(true);
			else return(false);
	case GT:	if (i>0) return(true);
			else return(false);
	default:	/* can't happen */
		error(MM_ERROR, ":56:Unknown relational operator %d", n);
	}
	/*NOTREACHED*/
}

void
tfree(a, s) register Cell *a; char *s;
{
	if (dbg>1) printf("## tfree %.8s %06o %s\n",
		s, a, a->sval ? a->sval : (uchar *)"");
	if (freeable(a))
		xfree(a->sval);
	if (a == tmps)
		error(MM_ERROR, ":57:Tempcell list is curdled");
	a->cnext = tmps;
	tmps = a;
}

Cell *gettemp(s) char *s;
{	int i;
	register Cell *x;

	if (!tmps) {
		tmps = (Cell *) calloc(100, sizeof(Cell));
		if (!tmps)
			error(MM_ERROR, ":58:No space for temporaries");
		for(i = 1; i < 100; i++)
			tmps[i-1].cnext = &tmps[i];
		tmps[i-1].cnext = 0;
	}
	x = tmps;
	tmps = x->cnext;
	*x = tempcell;
	if (dbg>1) printf("## gtemp %.8s %06o\n", s, x);
	return(x);
}

Cell *indirect(a,n) Node **a;
{
	register Cell *x;
	register int m;
	register uchar *s;
	Cell *fieldadr();

	x = execute(a[0]);
	m = getfval(x);
	if (m == 0 && !isnumber(s = getsval(x)))	/* suspicion! */
		error(MM_ERROR, ":59:Illegal field $(%s)", s);
	tempfree(x, "");
	x = fieldadr(m);
	x->ctype = OCELL;
	x->csub = CFLD;
	return(x);
}

Cell *substr(a, nnn) Node **a;
{
	register int k, m, n;
	register uchar *s;
	int temp;
	register Cell *x, *y, *z;

	x = execute(a[0]);
	y = execute(a[1]);
	if (a[2] != 0)
		z = execute(a[2]);
	s = getsval(x);
	k = strlen((char*)s) + 1;
	if (k <= 1) {
		tempfree(x, "");
		tempfree(y, "");
		if (a[2] != 0)
			tempfree(z, "");
		x = gettemp("");
		setsval(x, "");
		return(x);
	}
	m = getfval(y);
	if (m <= 0)
		m = 1;
	else if (m > k)
		m = k;
	tempfree(y, "");
	if (a[2] != 0) {
		n = getfval(z);
		tempfree(z, "");
	} else
		n = k - 1;
	if (n < 0)
		n = 0;
	else if (n > k - m)
		n = k - m;
	dprintf( ("substr: m=%d, n=%d, s=%s\n", m, n, s) );
	y = gettemp("");
	temp = s[n+m-1];	/* with thanks to John Linderman */
	s[n+m-1] = '\0';
	setsval(y, s + m - 1);
	s[n+m-1] = temp;
	tempfree(x, "");
	return(y);
}

Cell *sindex(a, nnn) Node **a;
{
	register Cell *x, *y, *z;
	register uchar *s1, *s2, *p1, *p2, *q;
	Awkfloat v = 0.0;

	x = execute(a[0]);
	s1 = getsval(x);
	y = execute(a[1]);
	s2 = getsval(y);

	z = gettemp("");
	for (p1 = s1; *p1 != '\0'; p1++) {
		for (q=p1, p2=s2; *p2 != '\0' && *q == *p2; q++, p2++)
			;
		if (*p2 == '\0') {
			v = (Awkfloat) (p1 - s1 + 1);	/* origin 1 */
			break;
		}
	}
	tempfree(x, "");
	tempfree(y, "");
	setfval(z, v);
	return(z);
}

int
sformat(buf, bufsize, s, a) uchar *buf, *s; int bufsize; Node *a;
{
	uchar fmt[RECSIZE];
	register uchar *p, *t, *os;
	char *val;
	register Cell *x;
	int flag = 0;

	os = s;
	p = buf;
	while (*s) {
		if (p - buf >= bufsize)
			return -1;
		if (*s != '%') {
			*p++ = *s++;
			continue;
		}
		if (*(s+1) == '%') {
			*p++ = '%';
			s += 2;
			continue;
		}
		for (t=fmt; (*t++ = *s) != '\0'; s++) {
			if (isalpha(*s) && *s != 'l' && *s != 'h' && *s != 'L')
				break;	/* the ansi panoply */
			if (*s == '*') {
				if (!a)
					error(MM_ERROR, ":61:Not enough args in printf(\"%s\")", os);
				x = execute(a);
				a = a->nnext;
				sprintf((char *)t-1, "%d", (int) getfval(x));
				t = fmt + strlen((char*)fmt);
				tempfree(x, "");
			}
		}
		*t = '\0';
		if (t >= fmt + sizeof(fmt))
			error(MM_ERROR, ":60:Format item %.20s... too long",
				os);
		switch (*s) {
		case 'f': case 'e': case 'g': case 'E': case 'G':
			flag = 1;
			break;
		case 'd': case 'i':
			flag = 2;
			if(*(s-1) == 'l') break;
			*(t-1) = 'l';
			*t = 'd';
			*++t = '\0';
			break;
		case 'o': case 'x': case 'X': case 'u':
			flag = *(s-1) == 'l' ? 6 : 3;
			break;
		case 's':
			flag = 4;
			break;
		case 'c':
			flag = 5;
			break;
		default:
			flag = 0;
			break;
		}
		if (flag == 0) {
			sprintf((char *)p, "%s", fmt);
			p += strlen((char*)p);
			continue;
		}
		if (a == NULL)
			error(MM_ERROR, ":61:Not enough args in printf(%s)",
				os);
		x = execute(a);
		a = a->nnext;

		/* We take preventative action to insure we don't overrun.
		 * This isn't guaranteed to work, but it is better than 
	         * nothing.
		 */
		if ((p - buf) >= (bufsize - 32)) {
			*p = '\0';
			return -1;
		}

		switch (flag) {
		case 1:
			sprintf((char *)p, (char *)fmt, getfval(x)); 
			break;
		case 2:		
			sprintf((char *)p, (char *)fmt, (long) getfval(x)); 
			break;
		case 3:	
			sprintf((char *)p, (char *)fmt, (unsigned int) getfval(x)); 
			break;
		case 4:
			val = getsval(x);
			if (strlen(val) >= (bufsize - (p - buf))) {
				*p = '\0';
				return -1;
		    	} else {
				sprintf((char *)p, (char *)fmt, getsval(x)); 
			}
			break;
		case 5: 
			isnum(x) ? 
			    sprintf((char *)p, (char *)fmt, (int) getfval(x))
			  : sprintf((char *)p, (char *)fmt, getsval(x)[0]);
			break;
		case 6:
			sprintf((char *)p, (char *)fmt, (unsigned long) getfval(x));
			break;
		}
		tempfree(x, "");
		p += strlen((char*)p);
		s++;
	}
	*p = '\0';
	for ( ; a; a = a->nnext)	/* evaluate any remaining args */
		execute(a);
	return 0;
}


/*
 * fformat is similar to format in intent, but it writes the output
 * directly to the given FILE rather than using an intermediary buffer.
 * These two were made separate functions since there is a fair amount
 * of overhead in checking the bounds of the buffer in sformat.
 */
 
int
fformat(fp, s, a) FILE *fp; uchar *s; Node *a;
{
	uchar fmt[RECSIZE];
	register uchar *t, *os;
	register Cell *x;
	int flag = 0;

	while (*s) {

		/* Continue printing chars until a '%' of '\0' is found */
		while (*s != '%') {
			putc(*s++, fp);
			if (*s == '\0')
			 	return 0;
		}
		
		if (*(s+1) == '%') {
			putc('%', fp);
			s += 2;
			continue;
		}

		/* We now copy the formatting info into the fmt array. */
		for (t=fmt; (*t++ = *s) != '\0'; s++) {

			if (isalpha(*s) && *s != 'l' && *s != 'h' && *s != 'L')
				break;	/* the ansi panoply */

			/* Handle the case where the field width info is
			 * stored in an argument.  Basically, we just 
			 * move it from the arg into the format string.
			 */
			if (*s == '*') {
				if (!a)
					error(MM_ERROR, ":61:Not enough args in printf(\"%s\")", os);
				x = execute(a);
				a = a->nnext;
				sprintf((char*) t-1, "%d", (int) getfval(x));
				t = fmt + strlen((char*)fmt);
				tempfree(x, "");
			}
		}

		*t = '\0';
		if (t >= fmt + sizeof(fmt))
			error(MM_ERROR, ":60:Format item %.20s... too long",
				os);
		switch (*s) {
		case 'f': case 'e': case 'g': case 'E': case 'G':
			flag = 1;
			break;
		case 'd': case 'i':
			flag = 2;
			if(*(s-1) == 'l') break;
			*(t-1) = 'l';
			*t = 'd';
			*++t = '\0';
			break;
		case 'o': case 'x': case 'X': case 'u':
			flag = *(s-1) == 'l' ? 6 : 3;
			break;
		case 's':
			flag = 4;
			break;
		case 'c':
			flag = 5;
			break;
		default:
			flag = 0;
			break;
		}
		if (flag == 0) {
			fprintf(fp, "%s", fmt);
			continue;
		}
		if (a == NULL)
			error(MM_ERROR, ":61:Not enough args in printf(%s)",
				os);
		x = execute(a);
		a = a->nnext;
		switch (flag) {
		case 1:	fprintf(fp, (char *)fmt, getfval(x)); break;
		case 2:	fprintf(fp, (char *)fmt, (long) getfval(x)); break;
		case 3:	fprintf(fp, (char *)fmt, (unsigned int) getfval(x));
			break;
		case 4:	fprintf(fp, (char *)fmt, getsval(x)); break;
		case 5: isnum(x) ? fprintf(fp, (char *)fmt, (int) getfval(x))
				 : fprintf(fp, (char *)fmt, getsval(x)[0]);
			break;
		case 6:
			fprintf(fp, (char *)fmt, (unsigned long) getfval(x));
			break;
		}
		tempfree(x, "");
		s++;
	}
	for ( ; a; a = a->nnext)	/* evaluate any remaining args */
		execute(a);

	return 0;
}

Cell *asprintf(a,n) Node **a;
{
	register Cell *x;
	register Node *y;
	uchar buf[3*RECSIZE];

	y = a[0]->nnext;
	x = execute(a[0]);
	if (sformat(buf, sizeof buf, getsval(x), y) == -1)
		error(MM_ERROR, ":62:sprintf string %.40s ... too long", buf);
	tempfree(x, "");
	x = gettemp("");
	x->sval = tostring(buf);
	x->tval = STR;
	return(x);
}

Cell *aprintf(a,n) Node **a;
{
	FILE *fp;
	register Cell *x;
	register Node *y;
	uchar buf[4*RECSIZE];

	y = a[0]->nnext;
	x = execute(a[0]);
	
	if (a[1] == NULL)
		fformat(stdout, getsval(x), y);
	else {
		fp = redirect((int)a[1], a[2]);
		fformat(fp, getsval(x), y);
		fflush(fp);
	}
	tempfree(x, "");
	return(true);
}

Cell *arith(a,n) Node **a;
{
	Awkfloat i, j;
	double v, ipow();
	register Cell *x, *y, *z;

	x = execute(a[0]);
	i = getfval(x);
	tempfree(x, "");
	if (n != UMINUS) {
		y = execute(a[1]);
		j = getfval(y);
		tempfree(y, "");
	}
	z = gettemp("");
	switch (n) {
	case ADD:
		i += j;
		break;
	case MINUS:
		i -= j;
		break;
	case MULT:
		i *= j;
		break;
	case DIVIDE:
		if (j == 0)
			error(MM_ERROR, ":64:Division by zero");
		i /= j;
		break;
	case MOD:
		if (j == 0)
			error(MM_ERROR, ":65:Division by zero in mod");
		modf(i/j, &v);
		i = i - j * v;
		break;
	case UMINUS:
		i = -i;
		break;
	case POWER:
		if (j >= 0 && modf(j, &v) == 0.0)	/* pos integer exponent */
			i = ipow(i, (int) j);
		else
			i = errcheck(pow(i, j), "pow");
		break;
	default:	/* can't happen */
		error(MM_ERROR, ":66:Illegal arithmetic operator %d", n);
	}
	setfval(z, i);
	return(z);
}

double ipow(x, n)
	double x;
	int n;
{
	double v;

	if (n <= 0)
		return 1;
	v = ipow(x, n/2);
	if (n % 2 == 0)
		return v * v;
	else
		return x * v * v;
}

Cell *incrdecr(a, n) Node **a;
{
	register Cell *x, *z;
	register int k;
	Awkfloat xf;

	x = execute(a[0]);
	xf = getfval(x);
	k = (n == PREINCR || n == POSTINCR) ? 1 : -1;
	if (n == PREINCR || n == PREDECR) {
		setfval(x, xf + k);
		return(x);
	}
	z = gettemp("");
	setfval(z, xf);
	setfval(x, xf + k);
	tempfree(x, "");
	return(z);
}

Cell *assign(a,n) Node **a;
{
	register Cell *x, *y;
	Awkfloat xf, yf;
	double v, ipow();

	y = execute(a[1]);
	x = execute(a[0]);	/* order reversed from before... */
	if (n == ASSIGN) {	/* ordinary assignment */
		if ((y->tval & (STR|NUM)) == (STR|NUM)) {
			setsval(x, getsval(y));
			x->fval = getfval(y);
			x->tval |= NUM;
		}
		else if (y->tval & STR)
			setsval(x, getsval(y));
		else if (y->tval & NUM)
			setfval(x, getfval(y));
		else
			funnyvar(y, gettxt(readvofid, readvof));
		tempfree(y, "");
		return(x);
	}
	xf = getfval(x);
	yf = getfval(y);
	switch (n) {
	case ADDEQ:
		xf += yf;
		break;
	case SUBEQ:
		xf -= yf;
		break;
	case MULTEQ:
		xf *= yf;
		break;
	case DIVEQ:
		if (yf == 0)
			error(MM_ERROR, ":67:Division by zero in /=");
		xf /= yf;
		break;
	case MODEQ:
		if (yf == 0)
			error(MM_ERROR, ":68:Division by zero in %%=");
		modf(xf/yf, &v);
		xf = xf - yf * v;
		break;
	case POWEQ:
		if (yf >= 0 && modf(yf, &v) == 0.0)	/* pos integer exponent */
			xf = ipow(xf, (int) yf);
		else
			xf = errcheck(pow(xf, yf), "pow");
		break;
	default:
		error(MM_ERROR, ":69:Illegal assignment operator %d", n);
		break;
	}
	tempfree(y, "");
	setfval(x, xf);
	return(x);
}

Cell *cat(a,q) Node **a;
{
	register Cell *x, *y, *z;
	register int n1, n2;
	register uchar *s;

	x = execute(a[0]);
	y = execute(a[1]);
	getsval(x);
	getsval(y);
	n1 = (int)strlen((char*)x->sval);
	n2 = (int)strlen((char*)y->sval);
	s = (uchar *) malloc(n1 + n2 + 1);
	if (s == NULL)
		error(MM_ERROR, ":70:Out of space concatenating %.15s and %.15s",
			x->sval, y->sval);
	strcpy((char*)s, (char*)x->sval);
	strcpy((char*)s+n1, (char*)y->sval);
	tempfree(y, "");
	z = gettemp("");
	z->sval = s;
	z->tval = STR;
	tempfree(x, "");
	return(z);
}

Cell *pastat(a,n) Node **a;
{
	register Cell *x;

	if (a[0] == 0)
		x = execute(a[1]);
	else {
		x = execute(a[0]);
		if (istrue(x)) {
			tempfree(x, "");
			x = execute(a[1]);
		}
	}
	return x;
}

Cell *dopa2(a,n) Node **a;
{
	register Cell *x;
	register int pair;

	pair = (int) a[3];
	if (pairstack[pair] == 0) {
		x = execute(a[0]);
		if (istrue(x))
			pairstack[pair] = 1;
		tempfree(x, "");
	}
	if (pairstack[pair] == 1) {
		x = execute(a[1]);
		if (istrue(x))
			pairstack[pair] = 0;
		tempfree(x, "");
		x = execute(a[2]);
		return(x);
	}
	return(false);
}

Cell *split(a,nnn) Node **a;
{
	Cell *x, *y, *ap;
	register uchar *s;
	register int sep;
	uchar *t, temp, num[5], *fs;
	int n, tempstat;

	y = execute(a[0]);	/* source string */
	s = getsval(y);
	if (a[2] == 0)		/* fs string */
		fs = *FS;
	else if ((int) a[3] == STRING) {	/* split(str,arr,"string") */
		x = execute(a[2]);
		fs = getsval(x);
	} else if ((int) a[3] == REGEXPR)
		fs = (uchar*) "(regexpr)";	/* split(str,arr,/regexpr/) */
	else
		error(MM_ERROR, ":71:Illegal type of split()");
	sep = *fs;
	ap = execute(a[1]);	/* array name */
	freesymtab(ap);
	dprintf( ("split: s=|%s|, a=%s, sep=|%s|\n", s, ap->nval, fs) );
	ap->tval &= ~STR;
	ap->tval |= ARR;
	ap->sval = (uchar *) makesymtab(NSYMTAB);

	n = 0;
	if (*s != '\0' && strlen((char*)fs) > (unsigned)1 ||
		(int) a[3] == REGEXPR) {	/* reg expr */
		fa *pfa;
		if ((int) a[3] == REGEXPR) {	/* it's ready already */
			pfa = (fa *) a[2];
		} else {
			pfa = makedfa(fs, 1);
		}
		if (nematch(pfa,s, 1)) {
#if OLD_REGEXP
			tempstat = pfa->initstat;
			pfa->initstat = 2;
#endif
			do {
				n++;
				sprintf((char *)num, "%d", n);
				temp = *patbeg;
				*patbeg = '\0';
				if (isnumber(s))
					setsymtab(num, s, atof((char *)s), STR|NUM, (Array *) ap->sval);
				else
					setsymtab(num, s, 0.0, STR, (Array *) ap->sval);
				*patbeg = temp;
				s = patbeg + patlen;
				if (*(patbeg+patlen-1) == 0 || *s == 0) {
					n++;
					sprintf((char *)num, "%d", n);
					setsymtab(num, "", 0.0, STR, (Array *) ap->sval);
#if OLD_REGEXP
					pfa->initstat = tempstat;
#endif
					goto spdone;
				}
			} while (nematch(pfa,s, 0));
		}
		n++;
		sprintf((char *)num, "%d", n);
		if (isnumber(s))
			setsymtab(num, s, atof((char *)s), STR|NUM, (Array *) ap->sval);
		else
			setsymtab(num, s, 0.0, STR, (Array *) ap->sval);
  spdone:
		pfa = NULL;
	} else if (sep == ' ') {
		for (n = 0; ; ) {
			while (*s == ' ' || *s == '\t' || *s == '\n')
				s++;
			if (*s == 0)
				break;
			n++;
			t = s;
			do
				s++;
			while (*s!=' ' && *s!='\t' && *s!='\n' && *s!='\0');
			temp = *s;
			*s = '\0';
			sprintf((char *)num, "%d", n);
			if (isnumber(t))
				setsymtab(num, t, atof((char *)t), STR|NUM, (Array *) ap->sval);
			else
				setsymtab(num, t, 0.0, STR, (Array *) ap->sval);
			*s = temp;
			if (*s != 0)
				s++;
		}
	} else if (*s != 0) {
		for (;;) {
			n++;
			t = s;
			while (*s != sep && *s != '\n' && *s != '\0')
				s++;
			temp = *s;
			*s = '\0';
			sprintf((char *)num, "%d", n);
			if (isnumber(t))
				setsymtab(num, t, atof((char *)t), STR|NUM, (Array *) ap->sval);
			else
				setsymtab(num, t, 0.0, STR, (Array *) ap->sval);
			*s = temp;
			if (*s++ == 0)
				break;
		}
	}
	tempfree(ap, "");
	tempfree(y, "");
	if (a[2] != 0 && (int) a[3] == STRING)
		tempfree(x, "");
	x = gettemp("");
	x->tval = NUM;
	x->fval = n;
	return(x);
}

Cell *condexpr(a,n) Node **a;
{
	register Cell *x;

	x = execute(a[0]);
	if (istrue(x)) {
		tempfree(x, "");
		x = execute(a[1]);
	} else {
		tempfree(x, "");
		x = execute(a[2]);
	}
	return(x);
}

Cell *ifstat(a,n) Node **a;
{
	register Cell *x;

	x = execute(a[0]);
	if (istrue(x)) {
		tempfree(x, "");
		x = execute(a[1]);
	} else if (a[2] != 0) {
		tempfree(x, "");
		x = execute(a[2]);
	}
	return(x);
}

Cell *whilestat(a,n) Node **a;
{
	register Cell *x;

	for (;;) {
		x = execute(a[0]);
		if (!istrue(x))
			return(x);
		tempfree(x, "");
		x = execute(a[1]);
		if (isbreak(x)) {
			x = true;
			return(x);
		}
		if (isnext(x) || isexit(x) || isret(x))
			return(x);
		tempfree(x, "");
	}
}

Cell *dostat(a,n) Node **a;
{
	register Cell *x;

	for (;;) {
		x = execute(a[0]);
		if (isbreak(x))
			return true;
		if (isnext(x) || isexit(x) || isret(x))
			return(x);
		tempfree(x, "");
		x = execute(a[1]);
		if (!istrue(x))
			return(x);
		tempfree(x, "");
	}
}

Cell *forstat(a,n) Node **a;
{
	register Cell *x;

	x = execute(a[0]);
	tempfree(x, "");
	for (;;) {
		if (a[1]!=0) {
			x = execute(a[1]);
			if (!istrue(x)) return(x);
			else tempfree(x, "");
		}
		x = execute(a[3]);
		if (isbreak(x))		/* turn off break */
			return true;
		if (isnext(x) || isexit(x) || isret(x))
			return(x);
		tempfree(x, "");
		x = execute(a[2]);
		tempfree(x, "");
	}
}

Cell *instat(a, n) Node **a;
{
	register Cell *x, *vp, *arrayp, *cp, *ncp;
	Array *tp;
	int i;

	vp = execute(a[0]);
	arrayp = execute(a[1]);
	if (!isarr(arrayp))
		error(MM_ERROR, notarray, arrayp->nval);
	tp = (Array *) arrayp->sval;
	tempfree(arrayp, "");
	for (i = 0; i < tp->size; i++) {	/* this routine knows too much */
		for (cp = tp->tab[i]; cp != NULL; cp = ncp) {
			setsval(vp, cp->nval);
			ncp = cp->cnext;
			x = execute(a[2]);
			if (isbreak(x)) {
				tempfree(vp, "");
				return true;
			}
			if (isnext(x) || isexit(x) || isret(x)) {
				tempfree(vp, "");
				return(x);
			}
			tempfree(x, "");
		}
	}
	return true;
}


Cell *bltin(a,n) Node **a;
{
	register Cell *x, *y;
	Awkfloat u;
	Awkfloat v;
	register int t;
	long time();
	uchar *p, buf[RECSIZE];
	Node *nextarg;
	static int srand_seed = 0;

	t = (int) a[0];
	x = execute(a[1]);
	nextarg = a[1]->nnext;
	switch (t) {
	case FLENGTH:
		u = (Awkfloat) (int)strlen((char*)getsval(x)); break;
	case FLOG:
		u = errcheck(log(getfval(x)), "log"); break;
	case FINT:
		modf(getfval(x), &u); break;
	case FEXP:
		u = errcheck(exp(getfval(x)), "exp"); break;
	case FSQRT:
		u = errcheck(sqrt(getfval(x)), "sqrt"); break;
	case FSIN:
		u = sin(getfval(x)); break;
	case FCOS:
		u = cos(getfval(x)); break;
	case FATAN:
		if (nextarg == 0) {
			error(MM_WARNING,
				":72:atan2 requires two arguments; returning 1.0");
			u = 1.0;
		} else {
			y = execute(a[1]->nnext);
			u = atan2(getfval(x), getfval(y));
			tempfree(y, "");
			nextarg = nextarg->nnext;
		}
		break;
	case FSYSTEM:
		fflush(stdout);		/* in case something is buffered already */
		u = (Awkfloat) system((char *)getsval(x)) / 256;   /* 256 is unix-dep */
		break;
	case FRAND:
		u = (Awkfloat) (rand() % 32767) / 32767.0;
		break;
	case FSRAND:
		if (x->tval & REC)	/* no argument provided */
			v = time((long *)0);
		else
			v = getfval(x);
		srand((int) v); 
		u = (Awkfloat) srand_seed;
		srand_seed = (int) v;
		break;
	case FTOUPPER:
	case FTOLOWER:
		strcpy((char*)buf, (char*)getsval(x));
		if (t == FTOUPPER) {
			for (p = buf; *p; p++)
				if (islower(*p))
					*p = toupper(*p);
		} else {
			for (p = buf; *p; p++)
				if (isupper(*p))
					*p = tolower(*p);
		}
		tempfree(x, "");
		x = gettemp("");
		setsval(x, buf);
		return x;
	default:	/* can't happen */
		error(MM_ERROR, ":73:Illegal function type %d", t);
		break;
	}
	tempfree(x, "");
	x = gettemp("");
	setfval(x, u);
	if (nextarg != 0) {
		error(MM_WARNING, ":74:Function has too many arguments");
		for ( ; nextarg; nextarg = nextarg->nnext)
			execute(nextarg);
	}
	return(x);
}

Cell *print(a,n) Node **a;
{
	register Node *x;
	register Cell *y;
	FILE *fp;

	if (a[1] == 0)
		fp = stdout;
	else
		fp = redirect((int)a[1], a[2]);
	for (x = a[0]; x != NULL; x = x->nnext) {
		y = execute(x);
		
		/* We want to use the output format, not the conversion
		 * format, so pass a 1 to r_getsval.
		 */
		fputs((char *)r_getsval(y, 1), fp);
		tempfree(y, "");
		if (x->nnext == NULL)
			fputs((char *)*ORS, fp);
		else
			fputs((char *)*OFS, fp);
	}
	if (a[1] != 0)
		fflush(fp);
	return(true);
}

Cell *nullproc() { return 0; }


struct
{
	FILE	*fp;
	uchar	*fname;
	int	mode;	/* '|', 'a', 'w' */
} files[FOPEN_MAX];

FILE *redirect(a, b)
	Node *b;
{
	FILE *fp;
	Cell *x;
	uchar *fname;

	x = execute(b);
	fname = getsval(x);
	fp = openfile(a, fname);
	if (fp == NULL)
		error(MM_ERROR, badopen, fname, strerror(errno));
	tempfree(x, "");
	return fp;
}

FILE *openfile(a, s)
	uchar *s;
{
	register int i, m;
	register FILE *fp;
	extern FILE *popen();

	if (*s == '\0')
		error(MM_ERROR, ":75:Null file name in print or getline");
	for (i=0; i < FOPEN_MAX; i++)
		if (files[i].fname &&
			strcmp((char*)s, (char*)files[i].fname) == 0)
			if (a == files[i].mode || a==APPEND && files[i].mode==GT)
				return files[i].fp;
	for (i=0; i < FOPEN_MAX; i++)
		if (files[i].fp == 0)
			break;
	if (i >= FOPEN_MAX)
		error(MM_ERROR, ":76:%s makes too many open files", s);
	fflush(stdout);	/* force a semblance of order */
	m = a;
	if (a == GT) {
		fp = fopen((char *)s, "w");
	} else if (a == APPEND) {
		fp = fopen((char *)s, "a");
		m = GT;	/* so can mix > and >> */
	} else if (a == '|') {	/* output pipe */
		fp = popen((char *)s, "w");
	} else if (a == LE) {	/* input pipe */
		fp = popen((char *)s, "r");
	} else if (a == LT) {	/* getline <file */
		fp = strcmp((char *)s, "-") == 0 ? stdin : fopen((char *)s, "r");	/* "-" is stdin */
	} else	/* can't happen */
		error(MM_ERROR, ":77:Illegal redirection");
	if (fp != NULL) {
		files[i].fname = tostring(s);
		files[i].fp = fp;
		files[i].mode = m;
	}
	return fp;
}

Cell *closefile(a) Node **a;
{
	register Cell *x;
	int i, stat;
        Cell *retval;

	retval = false;
	x = execute(a[0]);
	getsval(x);
	for (i = 0; i < FOPEN_MAX; i++) {
		if (files[i].fname &&
			strcmp((char*)x->sval, (char*)files[i].fname) == 0) {
			if (ferror(files[i].fp)) 
				error(MM_WARNING, ioerror, files[i].fname);
			if (files[i].mode == '|' || files[i].mode == LE)
				stat = pclose(files[i].fp);
			else
				stat = fclose(files[i].fp);
			if (stat == EOF) {
				error(MM_WARNING, 
				      ":78:I/O error occurred closing %s",
				      files[i].fname);
				retval = true;
			}
			xfree(files[i].fname);
			files[i].fname = NULL;	/* watch out for ref thru this */
			files[i].fp = NULL;
		}
	}
	tempfree(x, "close");
	return retval;
}

void
closeall()
{
	int i, stat;

	for (i = 0; i < FOPEN_MAX; i++)
		if (files[i].fp) {
			if (ferror(files[i].fp))
				error(MM_WARNING, ioerror, files[i].fname);
			if (files[i].mode == '|' || files[i].mode == LE)
				stat = pclose(files[i].fp);
			else
				stat = fclose(files[i].fp);
			if (stat == EOF)
				error(MM_WARNING, ":79:I/O error occurred while closing %s",
					files[i].fname);
		}
}

Cell *sub(a, nnn) Node **a;
{
	register uchar *sptr, *pb, *q;
	register Cell *x, *y, *result;
	uchar buf[RECSIZE], *t;
	fa *pfa;

	x = execute(a[3]);	/* target string */
	t = getsval(x);
	if (a[0] == 0)
		pfa = (fa *) a[1];	/* regular expression */
	else {
		y = execute(a[1]);
		pfa = makedfa(getsval(y), 1);
		tempfree(y, "");
	}
	y = execute(a[2]);	/* replacement string */
	result = false;
	if (pmatch(pfa, t, 1)) {
		pb = buf;
		sptr = t;
		while (sptr < patbeg)
			*pb++ = *sptr++;
		sptr = getsval(y);
		while (*sptr != 0 && pb < buf + RECSIZE - 1)
			if (*sptr == '\\' && *(sptr+1) == '&') {
				sptr++;		/* skip \, */
				*pb++ = *sptr++; /* add & */
			} else if (*sptr == '&') {
				sptr++;
				for (q = patbeg; q < patbeg+patlen; )
					*pb++ = *q++;
			} else
				*pb++ = *sptr++;
		*pb = '\0';
		if (pb >= buf + RECSIZE)
			error(MM_ERROR, restoobig, "sub", buf);
		sptr = patbeg + patlen;
		if ((patlen == 0 && *patbeg) || (patlen && *(sptr-1)))
			while (*pb++ = *sptr++)
				;
		if (pb >= buf + RECSIZE)
			error(MM_ERROR, restoobig, "sub", buf);
		setsval(x, buf);
		result = true;;
	}
	tempfree(x, "");
	tempfree(y, "");
	return result;
}

Cell *gsub(a, nnn) Node **a;
{
	register Cell *x, *y;
	register uchar *rptr, *sptr, *t, *pb;
	uchar buf[RECSIZE];
	register fa *pfa;
	int mflag, tempstat, num;

	mflag = 0;	/* if mflag == 0, can replace empty string */
	num = 0;
	x = execute(a[3]);	/* target string */
	t = getsval(x);
	if (a[0] == 0)
		pfa = (fa *) a[1];	/* regular expression */
	else {
		y = execute(a[1]);
		pfa = makedfa(getsval(y), 1);
		tempfree(y, "");
	}
	y = execute(a[2]);	/* replacement string */
	if (pmatch(pfa, t, 1)) {
#if OLD_REGEXP
		tempstat = pfa->initstat;
		pfa->initstat = 2;
#endif
		pb = buf;
		rptr = getsval(y);
		do {
			/*
			uchar *p;
			int i;
			printf("target string: %s, *patbeg = %o, patlen = %d\n",
				t, *patbeg, patlen);
			printf("	match found: ");
			p=patbeg;
			for (i=0; i<patlen; i++)
				printf("%c", *p++);
			printf("\n");
			*/
			if (patlen == 0 && *patbeg != 0) {	/* matched empty string */
				if (mflag == 0) {	/* can replace empty */
					num++;
					sptr = rptr;
					while (*sptr != 0 && pb < buf + RECSIZE-1)
						if (*sptr == '\\' && *(sptr+1) == '&') {
							sptr++;
							*pb++ = *sptr++;
						} else if (*sptr == '&') {
							uchar *q;
							sptr++;
							for (q = patbeg; q < patbeg+patlen; )
								*pb++ = *q++;
						} else
							*pb++ = *sptr++;
				}
				if (*t == 0)	/* at end */
					goto done;
				*pb++ = *t++;
				if (pb >= buf + RECSIZE)
					error(MM_ERROR, restoobig, "gsub", buf);
				mflag = 0;
			}
			else {	/* matched nonempty string */
				num++;
				sptr = t;
				while (sptr < patbeg && pb < buf + RECSIZE-1)
					*pb++ = *sptr++;
				sptr = rptr;
				while (*sptr != 0 && pb < buf + RECSIZE-1)
					if (*sptr == '\\' && *(sptr+1) == '&') {
						sptr++;
						*pb++ = *sptr++;
					} else if (*sptr == '&') {
						uchar *q;
						sptr++;
						for (q = patbeg; q < patbeg+patlen; )
							*pb++ = *q++;
					} else
						*pb++ = *sptr++;
				t = patbeg + patlen;
				if ((*(t-1) == 0) || (*t == 0))
					goto done;
				if (pb >= buf + RECSIZE)
					error(MM_ERROR, restoobig, "gsub", buf);
				mflag = 1;
			}
		} while (pmatch(pfa,t,0));
		sptr = t;
		while (*pb++ = *sptr++)
			;
	done:	if (pb >= buf + RECSIZE)
			error(MM_ERROR, restoobig, "gsub", buf);
		*pb = '\0';
		setsval(x, buf);
#if OLD_REGEXP
		pfa->initstat = tempstat;
#endif
	}
	tempfree(x, "");
	tempfree(y, "");
	x = gettemp("");
	x->tval = NUM;
	x->fval = num;
	return(x);
}
