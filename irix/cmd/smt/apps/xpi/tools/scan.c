/* SCAN.C -- LOW-LEVEL INPUT SCANNING                           */

/* history:                                                     */

/* n/a       E. Wilner   --  created                            */
/*  6/23/87  J. Howe     --  change order of expression eval    */
/*  4/28/88  Gumby       --  fix to recognize grumpy, gr001     */
/*  4/29/88  Gumby       --  changed '.' from lpc to pc         */
/*  5/02/88  Gumby       --  added $isreg() code                */
/*  6/03/88  Gumby       --  fixed [MAXNAME] in get_name()      */
/*  8/11/88  J. Ellis    --  various revisions                  */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/12/88  RJK         --  rev 0.17                           */
/* 10/25/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/* 12/13/88  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

/* ------------------------------------------------------------ */

#include "asm29.h"



/* "deblank()" skips over white-space characters in the current */
/* input line (but stops on EOS).                               */
void
deblank(void)
{
	register char *cp = lptr;
	register char c = '\0';

	while ((c = *cp) && (c <= SPACE)) cp++;
	lptr = cp;
}



/* "readline(ifp,bp)"  reads one  source line  (terminated by a */
/* newline or by EOF)  from the stream specified by "ifp"  into */
/* the buffer pointed to by "bp".                               */
void
readline(register FILE *ifp,		/* input file pointer           */
	 register char *bp)		/* pointer to line buffer       */
{
	int badchar = FALSE;		/* illegal-character flag       */
	register short c;		/* input characters             */
	int nc = 0;			/* # of characters in line      */

/* main loop                    */

    while (TRUE)
    {
	c = getc(ifp);          /* get next input character     */

#ifdef OLD
	if ((c < SPACE) && (c >= EOS) && !isspace(c))
#else
	if (c == EOS)
#endif
	{                       /* bad input character          */
	    c = '?';
	    badchar = TRUE;
	}

	if ((c == NL) || (c == EOF)) {	/* end of line                  */
		if (c == EOF)
			endafile = TRUE;
		*bp = EOS;		/* terminate the input string   */
		break;			/* done, exit the input loop    */
	}

	if (c == CR) continue;  /* never executed if CR == NL   */

	if (++nc > LLEN)        /* is the input line too long?  */
	{                       /* yes                          */
	    if (nc == LLEN+1)   /* just now overflowed?         */
	    {                   /* yes                          */
		*bp = EOS;      /* terminate the input string   */
		lineno++;       /* adjust  line number          */
		error           /* print error message          */
		    (e_toolong);
		lineno--;       /* restore line number          */
	    }
	    continue;           /* skip rest of input line      */
	}

	*bp++ = c;              /* save the input character     */
    }

/* wrap it up                   */

    if (badchar)                /* was there a bad character?   */
    {                           /* yes                          */
	lineno++;               /* adjust  line number          */
	error (e_badchar);      /* print error message          */
	lineno--;               /* restore line number          */
    }
}

/* ------------------------------------------------------------ */

/*
 * Get & list one source line (lists last line processed, gets a new line).
 */

int
getline(void)
{
	char *cp;
	int lword = 0;
	int lchars;
	int slen = 0;
	char *srcp;
	static int plen[] = { 3,9,5,5 };


	do {
	    /* always list if anything generated */
	    if (!listovr && lwords <= 0 &&
		((srctype == 'M' && !listmac) ||
		 (srctype == 'R' && !listmac) ||
		 (srctype == 'I' && !listinc) ||
		 (ifstk[ifsp] && !(listcond || listuncond)))) {
		lineno = 0;
	    }

	    listuncond = 0;

	    if ((llist && pass == 3 && lineno) || debugon) {
		if (llist < 0)
		    llist = 0;
reprint:
		lchars = 0;
		(void)newpage();

		if (debugon) {
		    printf ("%c%c%4d %08lx ",
			    ifstk[ifsp] ? '*' : ' ',
			    srctype,lineno,lpc);
		} else if (lwords > 0 || listpc) {
		    printf("%0*lx ", pcprec, lpc&pcmask);
		} else if (lword < lwords || slen > 0) {
		    printf("%*s", pcprec+1, "");
		}


#define BCOLEN 9
		while ((lchars+plen[lcode[lword].v_flags&VF_SIZE] <= BCOLEN)
		       && (lword < lwords)) {
		    switch (lcode[lword].v_flags & VF_SIZE) {
		    case VF_BYTE:
			printf ("%02lx",lcode[lword].v_val & 0xFFL);
			lpc += 1;	/* increment on alternate bytes */
			break;

		    case VF_HWORD:
			printf ("%04lx",lcode[lword].v_val & 0xFFFFL);
			lpc += 2;
			break;

		    case VF_WORD:
			printf ("%08lx",
				lcode[lword].v_val & 0xFFFFFFFFL);
			lpc += 4;
		    }

		    lchars += plen[lcode[lword].v_flags & VF_SIZE];

		    if (lcode[lword].v_seg || (pr_reloc == R_SEG)) {
			printf("!");
			lchars++;
		    } else if ((lcode[lword].v_ext != NULL) ||
			       (pr_reloc == R_EXT)) {
			printf ("%%");
			lchars++;
		    }

		    pr_reloc = 0;
		    lword++;
		    printf(" ");
		}

		srcp = srcline;
		slen = strlen (srcline);

		if (slen == 0) {
		    printf("\n");
		    pageline++;
		}

		while (slen > 0) {
		    printf ("%*s\t%.*s\n",
			    lchars <= BCOLEN ? (BCOLEN+1-lchars) : 0, "",
			    pgcolumns-BCOLEN-pcprec-1, srcp);
		    srcp += pgcolumns-BCOLEN-pcprec-1;
		    slen -= pgcolumns-BCOLEN-pcprec-1;
		    pageline++;
		    lchars = (-16);
		}

		if ((lword < lwords) && listalld) {
		    /* overflow data */
		    srcline[0] = EOS;
		    goto reprint;
		}

		if (ttyout)
		    fflush(stdout);
	    }

	    lwords = 0;
	    listpc = 0;
	    listovr = 0;
retry:

#ifdef OLD
	    if (src->if_type == 'M')
		readmacline (src->if_fp,srcline);
	    else
		readline (src->if_fp,srcline);
#else
	    readline (src->if_fp,srcline);
#endif

#ifdef CPP_HASH
	    if (*srcline == HASH) {
		*srcline = EOS;
		lptr = srcline + 1;
		deblank();

		if (isdigit(*lptr)) {
		    lineno = atoi (lptr);

		    if ((lptr = strchr (lptr,DQ)) != NULL) {
			if ((cp = strchr (++lptr,DQ)) != NULL) {
			    *cp = EOS;
			    free (src->if_name);
			    src->if_lineno = lineno-2;
			    src->if_name = savestring (lptr);
			}
		    }
		}
	    }
#endif

	    lineno  = ++src->if_lineno;
	    srctype = src->if_type;

	    if (endafile) {
		endafile = FALSE;
		/* end of main source file      */
		if (!inclose (0)) return (FALSE);
		goto retry;
	    }

	    lptr = srcline;
	    if (isspace(*lptr)) deblank();
	    lpc = 0;			/* blank or comment lines => 0  */
	}
#ifdef OLD
	while (*lptr == ';' || *lptr == '\0');
#else
	while (*lptr == SEMIC || *lptr == HASH || *lptr == EOS);
#endif

/* if expanding a macro or a ".irepc" statement, check for any */
/* arguments on the new line */

#ifdef OLD
	if (src->if_argc)
#else
    if (src->if_argc || rsp)
#endif
    {
	argcheck();
	lptr = srcline;
	if (isspace(*lptr)) deblank();
    }

    return (TRUE);
}

/* ------------------------------------------------------------ */

/* "parseline()" reads and parses an input line.                */

/* ------------------------------------------------------------ */

int parseline()
{
    int hi, mid, lo;
    char *mark;
    char name[MAXNAME+1];
    struct sym *ps;

retry:
    if (getline() == 0) {
	return (FALSE);
    }

    tag = NULL;
    tagname[0] = EOS;
    lpc = pc;               /* listed pc is same as current pc */
				/*   unless changed by a directive */

/* check for a label            */

    mark = lptr;        /* lptr already skipped over whitespace */

#ifdef OLD
    if (namech (*lptr) && !isdigit (*lptr)) {
	    (void)get_name(tagname);
#else
    if (get_name(tagname)) {
#endif
	if (*lptr == ':') {		/* label                        */
	    lptr++;
	    if (*mark != '$') lab_cnt++;
	    if (cs_flags (STYP_TEXT))
		align(4);
	} else {
	    lptr = mark;
	    tagname[0] = EOS;
	}
    }

    deblank();
    inst = NULL;

    if (lptr == srcline)
    {                           /* invalid label                */
	error (e_badlabel);
	return (TRUE);
    }

    mark = lptr;

    if (get_name(name)) {
	if (!ifstk[ifsp]) {		/* check for macro name         */
		ps = findsym (name,mactab);

		if ((ps != NULL) && !(ps->sy_flags & F_UNDEF)) {
			macexp (ps);
			savelabel();
			goto retry;
		}
	}

/* check for instruction or directive */

	downshift (name);
	lo = 0;
	hi = nres-1;

	while (lo <= hi) {
		mid = (lo + hi) >> 1;

		switch (sgn (strncmp (name,restab[mid].rs_name,11))) {
		case 0:
			inst = &restab[mid];
			lo = hi+1;
			break;

		case 1:
			lo = mid+1;
			break;

		case -1:
			hi = mid-1;
		}
	}

	if ((inst == NULL) && !ifstk[ifsp]) {
		/* invalid instruction          */
		error (e_instr);
	}
    }

    savelabel();
    return (TRUE);
}

/* ------------------------------------------------------------ */

int cdigit(c)
char c;
{
    if (isdigit (c)) return (c - '0');
    if (isupper (c)) return (10 + c - 'A');
    if (islower (c)) return (10 + c - 'a');
    return (999);
}

/* ------------------------------------------------------------ */

/*
 * Evaluate & scan past a single-precision floating-point number.
 */

float
getfloat(void)
{
    int n;
    float val;

    if (setjmp (jmp_fltp))
    {
	error (e_range);
	return (1.0);
    }

    n = sscanf (lptr,"%f",&val);
    clrjmp (jmp_fltp);

    if (n != 1)
    {
	error (e_operand);
	return (1.0);
    }

    while (isdigit (*lptr) ||
	(*lptr == '.') ||
	(*lptr == 'e') ||
	(*lptr == 'E') ||
	(*lptr == '-') ||
	(*lptr == '+'))
    {
	lptr++;
    }

    deblank();
    return (val);
}

/* ------------------------------------------------------------ */

/*
 * Evaluate & scan past a double-precision floating-point number.
 */

double
getdouble(void)
{
    int n;
    double val;

    if (setjmp (jmp_fltp))
    {
	error (e_range);
	return (1.0);
    }

    n = sscanf (lptr,"%lf",&val);
    clrjmp (jmp_fltp);

    if (n != 1)
    {
	error (e_operand);
	return (1.0);
    }

    while (isdigit (*lptr) ||
	(*lptr == '.') ||
	(*lptr == 'e') ||
	(*lptr == 'E') ||
	(*lptr == '-') ||
	(*lptr == '+'))
    {
	lptr++;
    }

    deblank();
    return (val);
}

/* ------------------------------------------------------------ */

/*
 * Scan a line for a number beginning at lptr.  Binary numbers are prefixed by
 *   0B; hex numbers are prefixed by 0X; octal numbers are prefixed by a zero;
 *   otherwise the radix is decimal.  This routine does not check that there
 *   is indeed a number available -- a 0 is returned if none is found.
 *   Recommended usage:  if isdigit(*lptr) num = getnum();
 */

long getnum()
{
    char b;
    int d;
    int base = 10;
    unsigned long result = 0;
    unsigned long ulb;

    if (*lptr == '0')           /* check if radix specified */
    {
	lptr++;
	b = islower (*lptr) ? toupper (*lptr) : *lptr;

	switch (b)
	{
    case 'B':
	    base = 2;
	    lptr++;
	    break;
    case 'X':
	    base = 16;
	    lptr++;
	    break;
    default:
	    if (isdigit(b)) base = 8;
	    break;
	}
    }

    ulb = base;

    while ((d = cdigit(*lptr)) < base)
    {
	unsigned long ulx = result * ulb;
	if ((ulx / ulb) != result) error (e_range);
	result = ulx + (unsigned long) d;
	lptr++;
    }

    if ((d < 16) && (d >= base)) error (e_illdigit);

    return (result);
}

/* ------------------------------------------------------------ */

/*
 * Get a "primitive" -- handles parentheses, numbers, symbols, repeat-block
 *   arguments, string constants, and the . operator.
 */

getpri(vp)
struct value *vp;
{
char name[300]; /* name & other-things buffer */
struct sym *ps;
int i, j;
short lno, cnt;
struct xrefer *xp;
union dblflt dbl;
float fl;
char *mark, *cp, *cp2;

	vp->v_seg = 0;
	vp->v_flags = 0;
	vp->v_reloc = 0;
	vp->v_ext = NULL;
	vp->v_val = 0L;
	if (isspace (*lptr))
		deblank();
	if (*lptr == '(')
		{
		lptr++;
		getval(vp);
		if (*lptr == ')')
			lptr++;
		  else
			error (e_cparen);
		return;
		}

    if (isdigit(*lptr))
    {
	vp->v_val = getnum();
	return;
    }

    if (*lptr == '&')
    {                           /* "address" of register        */
	++lptr;
	(void)getreg(vp,1);
	return;
    }

    if ((*lptr == '$') && isalpha (*(lptr+1))) {
	lptr++;			/* skip over '$'                */
	(void)get_name(name);
	downshift (name);

	if (!strcmp (name,"isreg")) {
	    scanarg (name);     /* get one valid macro argument */
	    downshift (name);

	    cp = name;
	    while (*cp) ++cp;
	    --cp;

	    if (*cp == ')') *cp = EOS;
	    else
	    {
		error (e_missing);
	    }

	    cp = name;

	    if (*cp == '(') cp++;
	    else
	    {
		error (e_missing);
	    }

	    cp2 = cp;

	    if ((*cp == '%') && (*(cp+1) == '%'))
	    {
		vp->v_val = 1;
		return;
	    }

	    if ((*(cp+1) == 'r') &&
		((*cp == 'l') || (*cp == 'g')))
	    {
		cp += 2;

		if (*cp != '0' || *(cp+1) == EOS)
		{
		    i = 0;

		    while (isdigit (*cp))
		    {
			i = 10 * i + (*cp++ - '0');
		    }

		    if ((i <= 127) && (*cp == EOS))
		    {
			vp->v_val = 1;
			return;
		    }
		}
	    }

	    ps = findsym (cp2,NULL);

	    if (ps != NULL)
	    {
		if (ps->sy_flags & F_REGSYM)
		{
		    vp->v_val = 1;
		}
	    }
	    return;
	}

	if (!strcmp (name,"float")) {
		checkfor ('(',1);
		fl = getfloat();
		checkfor (')',1);
		ieee ((double) fl,(char *) &vp->v_val);
		return;
	}

	if (!strcmp (name,"double0"))
	{
	    checkfor ('(', 1);
	    dbl.d = getdouble();
	    ieeed (dbl.d,(char *) &dbl.l[0]);
	    vp->v_val = dbl.l[0];
	    checkfor (')',1);
	    return;
	}

	if (!strncmp (name,"double1",8))
	{
	    checkfor ('(',1);
	    dbl.d = getdouble();
	    ieeed (dbl.d,(char *) &dbl.l[0]);
	    vp->v_val = dbl.l[1];
	    checkfor (')',1);
	    return;
	}

	if ((!strncmp (name,"extend",6)) &&
	    (strlen (name) == 7))
	{
	    mark = lptr-1;

	    if ((*mark >= '0') &&
		(*mark <= '3') &&
		(mark[1] == '('))
	    {
		lptr++;
		deblank();
		dbl.d = getdouble();
		ieeex (dbl.d,(char *) &ext_float[0]);
		vp->v_val = ext_float[*mark - '0'];
		checkfor (')',1);
		return;
	    }

	    error (e_operand);
	    return;
	}

	if (!strcmp (name,"narg"))
	{
	    struct include *ip;
	    ip = src;

	    while ((ip != NULL) && (ip->if_type != 'M'))
	    {
		ip = ip->if_link;
	    }

	    vp->v_val = (ip == NULL) ? 0 : ip->if_argc;
	    return;
	}

	error (e_nofunc);
	return;
    }

#ifdef OLD
    if (namech (*lptr)) {
	(void)get_name(name);
#else
    if (get_name(name)) {
#endif
	ps = searchsym (name);

	if (ps->sy_flags & F_VAL)
	{
	    *vp = *((struct value *) ps->se.n_value);
	    return;
	}

	if (ps->se.n_scnum > 0)
	{
	    vp->v_seg = ps->se.n_scnum;
	}

	vp->v_val = ps->se.n_value;

	if (ps->sy_flags & (F_EXTERN | F_RELOC | F_UNDEF))
	{
	    ps->sy_flags |= F_RELOC;
	    vp->v_ext = ps;

	    if (ps->sy_flags & F_EXTERN)
	    {                   /* avoid adding extern # to     */
		vp->v_val = 0;  /* value                        */
	    }
	}

	if (xref && (pass == 3))
	{                       /* handle cross-reference entry */
	    if (ps->sy_xref == NULL)
	    {
		ps->sy_xref = (struct xrefer *)
		    e_alloc (1,sizeof(struct xrefer));
	    }

	    xp = ps->sy_xref;

	    while (xp->xr_link != NULL)
	    {
		xp = xp->xr_link;
	    }

	    if (xp->xr_cnt == XR_PACK)
	    {
		xp->xr_link = (struct xrefer *)
		    e_alloc (1,sizeof(struct xrefer));
		xp = xp->xr_link;
	    }

	    lno = mainsrc->if_lineno;
	    cnt = xp->xr_cnt;

	    if ((cnt == 0) ||
		(xp->xr_list[cnt-1].xr_line != lno))
	    {
		xp->xr_list[cnt].xr_line = lno;
		xp->xr_cnt++;
	    }
	}
	return;
    }

    /* '.' is handled by get_name() and searchsym() */

    if (*lptr == '\'')
    {                           /* character constant           */
	i = 0;
	lptr++;

	while ((j = strchar ('\'')) != -1)
	{
	    vp->v_val = (vp->v_val << 8) + j;
	    i++;
	}

	if (*lptr == '\'') lptr++;

	if (i > 4) error (e_size);
	return;
    }

    error (e_operand);
}

/* ------------------------------------------------------------ */

/*
 * Get a unary operator.
 */
getunary (vp)
struct value *vp;
{
char op;

	op = *lptr;
	if ((op != '-') && (op != '~') && (op != '!') && (op != '+'))
		{
		getpri (vp);
		return;
		}
	lptr++;
	deblank();
	getunary (vp);
	if (( op == '-') || (op == '~') || (op == '!'))
		if (vp->v_ext != NULL || vp->v_seg != 0)
			error(e_reloc);                 /* forbid relocatable items! */
	switch (op)
		{
		case '-':
			vp->v_val = -vp->v_val;
			break;
		case '~':
			vp->v_val = ~vp->v_val;
			break;
		case '!':
			vp->v_val = (vp->v_val == 0L);
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a term -- handle multiplication, division, and modulo
 * operators.
 */

getterm(vp)
struct value *vp;
{
    char op;
    unsigned long ulx,uly,ulz;  /* scratch                      */
    struct value v2;

    getunary (vp);

    for (;;)
    {
	deblank();

	if ((*lptr != '*') && (*lptr != '/') && (*lptr != '%'))
	{
	    break;
	}

	op = *lptr++;
	deblank();
	getunary (&v2);

	if (v2.v_ext != NULL)
	{
	    error (e_reloc);
	}

#ifdef OLD
    if (op == '*')
	vp->v_val *= v2.v_val;
    else
	if (!v2.v_val)
	    error(e_divzero);
	else
	    if (op == '/')
		vp->v_val =
		    (long) (vp->v_val / (unsigned long) v2.v_val);
	    else
		vp->v_val =
		    (long) (vp->v_val % (unsigned long) v2.v_val);
    continue;
#endif

/* handle multiplication        */

	if (op == '*')          /* multiplication?              */
	{                       /* yes                          */
	    ulx = vp->v_val;
	    uly = v2.v_val;
	    ulz = ulx * uly;

	    if (ulx && ((ulz/ulx) != uly)) error (e_exov);
	    if (uly && ((ulz/uly) != ulx)) error (e_exov);

	    vp->v_val = ulz;
	    continue;
	}

/* handle division and modulo   */

	if (!v2.v_val)          /* division by zero?            */
	{                       /* yes                          */
	    error (e_divzero);
	    continue;
	}

	if (op == '/')          /* division?                    */
	{                       /* yes                          */
	    vp->v_val =
		(long) (vp->v_val / (unsigned long) v2.v_val);
	    continue;
	}

/* modulo                       */

	vp->v_val =
	    (long) (vp->v_val % (unsigned long) v2.v_val);
    }
}

/* ------------------------------------------------------------ */

/*
 * Get an additive term -- handles addition & stubraction operators.
 */

getsum(vp)
struct value *vp;
{
    char op;
    long lx,ly;                 /* scratch                      */
    unsigned long ulx,uly,ulz;  /* scratch                      */
    struct value v2;

	getterm (vp);
	for (;;)
		{
		deblank();
		if ((*lptr != '+') && (*lptr != '-'))
			break;
		op = *lptr++;
		deblank();
		getterm (&v2);
		if (op == '+')
			{
#ifdef OLD
		vp->v_val += v2.v_val;
#else
		ulx = lx = vp->v_val;
		uly = ly = v2.v_val;
		ulz = ulx + uly;

		if (ulx != (ulz-uly)) error (e_exov);
		if (uly != (ulz-ulx)) error (e_exov);
		if ((lx < 0) && (ly < 0)) error (e_exov);

		vp->v_val = ulz;
#endif
			if ((v2.v_seg && !vp->v_seg) ||
					((vp->v_ext == NULL) && (v2.v_ext != NULL)))
				{
				vp->v_seg = v2.v_seg;
				vp->v_ext = v2.v_ext;
				}
			}
		  else
			{
		vp->v_val -= v2.v_val;
			if (vp->v_seg == v2.v_seg)
				vp->v_seg = 0;
			}
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a "shift" term -- handles shift operators.
 */

getshift(vp)
struct value *vp;
{
    int left;                   /* left-shift flag              */
    char op;                    /* operator character           */
    long shift;                 /* shift count                  */
    struct value v2;            /* shift count value structure  */

    getsum (vp);

    for (;;) {
	deblank();              /* point to operator (if any)   */
	op = *lptr;             /* get operator character       */

	if ((op != lptr[1]) || ((op != '<') && (op != '>')))
	{                       /* not << or >>                 */
	    break;
	}

	lptr += 2;              /* skip over << or >>           */
	deblank();              /* point to shift value         */
	getsum (&v2);           /* get shift value              */

	if ((vp->v_ext != NULL) || (v2.v_ext != NULL))
	{                       /* invalid shift                */
	    error (e_reloc);
	    break;
	}

	if (v2.v_seg != 0)
	{                       /* invalid shift                */
	    error (e_reloc);
	    break;
	}

	if ((shift = v2.v_val) == 0)
	{                       /* null shift                   */
	    continue;
	}

	left = (op == '<');     /* set left-shift flag          */

	if (shift < 0)
	{                       /* handle negative shift count  */
	    shift = (-shift);
	    left  = !left;
	}
				/* size of a long (in bits)    */
#define BITL    (8 * sizeof(long))

	if (left)
	{
	    vp->v_val = vp->v_val << shift;
	}
	else
#ifdef NOTDEF
	{
	    vp->v_val = (long) ((unsigned long) vp->v_val >> shift);
	}
#else                           /* work around VAX restriction  */
	{
	    unsigned long xx,yy,zz;
	    xx = vp->v_val;
	    yy = shift;
	    zz = (yy < BITL) ? (xx >> yy) : 0;
	    vp->v_val = (long) zz;
	}
#endif
    }
}

/* ------------------------------------------------------------ */

/*
 * Get a relational term -- handle relational operators.
 */

#define LESS    1
#define LEQ             2
#define MORE    3
#define GEQ             4
#define EQUAL   5
#define NEQ             6
#define UCOMP(x, y, op)         (((unsigned long) x) op ((unsigned long) y))

getrel(vp)
struct value *vp;
{
struct value  v2;
int op = 0;

	getshift (vp);
	if (*lptr == '<')
		op = LESS;
	  else
		if (*lptr == '>')
			op = MORE;
	if (op)
		{
		if (*++lptr == '=')
			{
			op++;
			lptr++;
		}
		getshift(&v2);
		if ((vp->v_seg != v2.v_seg) || (vp->v_ext != v2.v_ext))
			vp->v_val = 0;
		  else
			switch (op)
				{
				case LESS:
					vp->v_val = UCOMP (vp->v_val, v2.v_val, < );
					break;
				case LEQ:
					vp->v_val = UCOMP (vp->v_val, v2.v_val, <=);
					break;
				case MORE:
					vp->v_val = UCOMP (vp->v_val, v2.v_val, > );
					break;
				case GEQ:
					vp->v_val = UCOMP (vp->v_val, v2.v_val, >=);
				}
		vp->v_seg = 0;
		vp->v_ext = NULL;
		}
}

/* ------------------------------------------------------------ */

/*
 * Get an equality/inequality term -- handles equal and not-equal operators.
 */
geteq(vp)
struct value *vp;
{
struct value v2;
char op;

	getrel(vp);
	for (;;)
		{
		deblank();
		if (!((*(lptr+1) == '=') && ((*lptr == '=') || (*lptr == '!'))))
			break;
		op = *lptr;
		lptr += 2;
		deblank();
		getrel(&v2);
		if ((vp->v_seg != v2.v_seg) || (vp->v_ext != v2.v_ext))
			vp->v_val = (op == '!');
		  else
			if (op == '=')
				vp->v_val = (vp->v_val == v2.v_val);
			  else
				vp->v_val = (vp->v_val != v2.v_val);
		vp->v_seg = 0;
		vp->v_ext = NULL;
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a bitwise-AND term.
 */
getbitand(vp)
struct value *vp;
{
struct value v2;

	geteq (vp);
	for (;;)
		{
		deblank();
		if (!(*lptr == '&' && *(lptr+1) != '&'))
			break;
		lptr++;
		deblank();
		geteq(&v2);
		if ((v2.v_ext != NULL) || (vp->v_ext != NULL))
			error (e_reloc);
		if ((v2.v_seg != 0) || (vp->v_seg != 0))
			error (e_reloc);
		vp->v_val = vp->v_val & v2.v_val;
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a bitwise-XOR term.
 */
getexor(vp)
struct value *vp;
{
struct value v2;

	getbitand (vp);
	for (;;)
		{
		deblank();
		if (*lptr != '^')
			break;
		lptr++;
		deblank();
		getbitand (&v2);
		if ((v2.v_ext != NULL) || (vp->v_ext != NULL))
			error (e_reloc);
		if ((v2.v_seg != 0) || (vp->v_seg != 0))
			error (e_reloc);
		vp->v_val = vp->v_val ^ v2.v_val;
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a bitwise-OR term.
 */
getincor(vp)
struct value *vp;
{
struct value v2;

	getexor(vp);
	for (;;)
		{
		deblank();
		if (! (*lptr == '|' && *(lptr+1) != '|'))
			break;
		lptr++;
		deblank();
		getexor (&v2);
		if ((v2.v_ext != NULL) || (vp->v_ext != NULL))
			error (e_reloc);
		if ((v2.v_seg != 0) || (vp->v_seg != 0))
			error(e_reloc);
		vp->v_val = vp->v_val | v2.v_val;
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a logical-AND term.
 */
getand(vp)
struct value *vp;
{
struct value v2;

	getincor (vp);
	for (;;)
		{
		deblank();
		if      ( !(*lptr == '&' && *(lptr+1) == '&'))
			break;
		lptr += 2;
		deblank();
		getincor (&v2);
		if ((v2.v_ext != NULL) || (vp->v_ext != NULL))
			error (e_reloc);
		if ((v2.v_seg != 0) || (vp->v_seg != 0))
			error (e_reloc);
		vp->v_val = vp->v_val && v2.v_val;
		}
}

/* ------------------------------------------------------------ */

/*
 * Get a logical-OR term.
 */
getor(vp)
struct value *vp;
{
struct value v2;

	getand (vp);
	for (;;)
		{
		deblank();
		if ((*lptr != '|') || (*(lptr+1) != '|'))
			break;
		lptr += 2;
		deblank();
		getor (&v2);
		if ((v2.v_ext != NULL) || (vp->v_ext != NULL))
			error (e_reloc);
		if ((v2.v_seg != 0)  || (vp->v_seg != 0))
			error(e_reloc);
		vp->v_val = vp->v_val || v2.v_val;
		}
}


/* ------------------------------------------------------------ */

/*
 * Get a value -- beginning of expression parser.
 */
void
getval(struct value *vp)
{
	if (isspace (*lptr))
		deblank();
	getor (vp);
}

/* ------------------------------------------------------------ */

/*
 * Get a non-relocatable integer beginning at "lptr".
 */

long
getint(void)
{
    struct value v;

    getval (&v);
    if (v.v_ext != NULL) error (e_reloc);
    return (v.v_val);
}

/* ------------------------------------------------------------ */

/*
 * Get an integer within bounds --
 * similar to "getintb()", but allows externals.
 */

void
getintval(struct value *vp,
	  long lo,
	  long hi)
{
    getval(vp);
#ifdef NOT_NOW
    if (vp->v_ext && !(vp->v_ext->sy_flags & F_EXTERN))
	error (e_operand);
#endif
    vp->v_val = bcheck(vp->v_val,lo,hi);
}

/* ------------------------------------------------------------ */

/*
 * Get a non-relocatable integer that must be within
 * a low/high limit.
 */

long
getintb(long lo,long hi)
{
#ifdef OLD
    long i = getint();
    (void)bcheck (i,lo,hi);
    return (i);
#else
    return (bcheck (getint(),lo,hi));
#endif
}

/* ------------------------------------------------------------ */

/*
 * Get a valid name identifier and chack for local symbols.
 */

int
get_name(char *dest)
{
	char cp[MAXNAME+1];
	int i;

	cp[0] = '\0';
	*dest = '\0';
	if (!namech (*lptr) || (*lptr == '?') || isdigit (*lptr))
		return 0;                                       /* no name */
	if (*lptr == '$') {
		if (isdigit (*(lptr + 1))) {
			i = 0;
			*dest++ = *lptr++;
			while (isdigit (*lptr)) {
				*dest++ = *lptr++;
				i++;
				if (i > 7) {
					error (e_local);
					break;
				}
			}
			if (!namech (*lptr))
				strcpy (cp, tempext());
		} else {
			return(0);
		}
	}

	for (i = 0; namech (*lptr) && i < MAXNAME-1; i++) {
		if ((*lptr == '\\') && isprint (*(lptr+1)))
			lptr++;
		*dest++ = *lptr++;
	}
	if (namech (*lptr))
		error (e_longname);
	while (namech (*lptr))
		lptr++;			/* Skip extra characters in name */
	*dest = '\0';
	strcat (dest, cp);
	return 1;
}



/* "get_rname(name,errcode)" is equivalent to "get_name(name)", */
/* with the single  distinction that  "get_rname()"  prints the */
/* specified error message if a name is not obtained.           */

int
get_rname(char *name,			/* name buffer                  */
	  int errcode)			/* error-message code           */
{
    int result = get_name (name);
    if (result == FALSE) error (errcode);
    return (result);
}

/* ------------------------------------------------------------ */

/* "skip_eol()" skips to the end of the current input line.     */

/* ------------------------------------------------------------ */

void
skip_eol(void)
{
    while (*lptr) lptr++;
}

/* ------------------------------------------------------------ */

int skip_char(c,required)
char c;
int required;
{
int ret;

	while (isspace (*lptr))
		lptr++;
	if (ret = (*lptr == c))
		lptr++;
	  else
		if (required)
			if (*lptr != '\0')
#ifdef OLD
				if (*lptr != ';')
#else
				if (*lptr != SEMIC && *lptr != HASH)
#endif
					error (e_missing);
	while (isspace (*lptr))
		lptr++;
	return ret;
}

/* ------------------------------------------------------------ */

/*
 * Skip a certain number of commas.
 */

int
comma(int required)
{
	return skip_char (',', required);
}


/*
 * Check for end-of-line.
 */
int
eol(void)
{
	register int c;
	while (isspace (c = *lptr))
		lptr++;
#ifdef OLD
	return ((*lptr == '\0') || (*lptr == ';'));
#else
	return ((c == EOS) || (c == SEMIC) || (c == HASH));
#endif
}


/* ------------------------------------------------------------ */

/*
 * Get one character of a string.
 */
int
strchar(int term)
{
	int bflag = TRUE;		/* backslash-enable flag        */
	int i;
	int ret;

	if ((term & ~BMASK) == NO_BSL) {
		bflag = FALSE;
		term &= BMASK;
	}

	if (*lptr == term)
		return -1;

	if (!*lptr) {
		error(e_quote);
		return -1;
	}

	if ((*lptr == BSLASH) && bflag) {
		i = ret = 0;
		if (*++lptr == 'x') {
			lptr++;
			while (isxdigit(*lptr) && (i++ < 3)) {
				ret = 16 * ret + cdigit (*lptr);
				lptr++;
			}
		} else if (isodigit (*lptr)) {
			while (isodigit(*lptr) && (i++ < 3)) {
				ret = 8*ret + *lptr - '0';
				lptr++;
			}
			/* NOTE: newline not allowed in this implementation */

		} else {
			switch (*lptr++) {
			case 'b':	/* backspace */
				ret = 0x08;
				break;
			case '0':	/* escape */
				ret = 0x00;
				break;
			case 'f':	/* formfeed */
				ret = 0x0C;
				break;
			case 'r':	/* return */
				ret = 0x0D;
				break;
			case 't':	/* tab */
				ret = 0x09;
				break;
			case 'n':	/* newline */
				ret = 0x0A;
				break;
			default:
				ret = *(lptr-1);
			}
		}
	} else {
		ret = *lptr++;
	}
	if (ret > 0x0ff) {
		error(e_num);
		return -1;
	}
	return ret;
}



/*
 * Get the next register-name or -number starting at "lptr".  Check that it is
 *   a valid register name and return a number 0-255 denoting the register.
 *   Predefined register names have the forms "lrxxx" and "grxxx" where xxx is
 *   a number 0-127.  An expression is also     allowed if preceded by %%.  If the
 *   value is relocatable, write relocatable record     to the output file.
 */
int
getreg(struct value *vp,
       int required)
{
	long reg = 0;
	char name[40], *mark, *cp;
	struct sym *ps;

	zap(vp);
	(void)comma (0);
	if ((*lptr == '%') && (*(lptr+1) == '%'))
		{
		lptr += 2;
		getval (vp);
#ifdef OLD
		(void)bcheck(vp->v_val,0L,255L);
#else                           /* CHANGE 8/22/88               */
		vp->v_val = bcheck(vp->v_val,0L,255L);
#endif
		return 0;
		}
	mark = lptr;

#ifdef OLD
	if (!namech (*lptr)) return (-1);
#else
	if (!namech (*lptr))
	{
	    if (required) error (e_operand);
	    return (-1);
	}
#endif

	if (!get_name (name)) strcpy (name,"XYZZY");
	ps = findsym (name, NULL);
	if ((ps != NULL) && (ps->sy_flags & F_VAL))
		{
		*vp = *(struct value *)ps->se.n_value;
		return 0;
		}
	if ((ps != NULL) && (ps->sy_flags & F_REGSYM))
		{
		vp->v_val = ps->se.n_value;
		return 0;
		}
	lptr = mark;
	downshift (name);
	cp = name;
	if ((((*cp == 'l') && (*(cp+1) == 'r'))  ||
			((*cp == 'g') && (*(cp+1) == 'r')))
			&& isdigit(*(cp+2)) )
		{
		lptr += 2;
		if (*lptr == '0')
			{                                               /* error if leading zeros */
			if (isdigit (*++lptr))
				{
				if (required)
					{
					error (e_illreg);
					return 0;
					}
				  else
					return -1;
				}
			}
		reg = 0;
		while (isdigit (*lptr))
			{
			reg = reg * 10 + ((*lptr) - '0');
			lptr++;
			}
#ifdef OLD
		(void)bcheck(reg,0L,127L);
#else                           /* CHANGE 8/22/88               */
		reg = bcheck(reg,0L,127L);
#endif
		}
	  else
		if (!required)
			return -1;
		  else
			error (e_illreg);
	if ( *cp == 'l')
		reg |= 0x80;  /* hi order bit is 1 for local regs */
	vp->v_val = reg;
	return 0;
}

/* ------------------------------------------------------------ */

/*
 * Get the next special-purpose register starting at "lptr".
 * Check that it is a valid special-purpose register name and
 * return a number denoting the register in value structure.
 * If the value is relocatable, write a relocatable
 * record to the output file.
 */

getspreg(vp)
struct value *vp;
{
    char name[40];
    struct sym *ps;

    zap (vp);
    (void)comma(0);

    if ((lptr[0] == '%') && (lptr[1] == '%'))
    {
	lptr += 2;
	getval (vp);

	if ((vp->v_val > 14) && (vp->v_val < 128))
	{
	    zap (vp);
	    error (e_spreg);
	}
	return;
    }

    if (!get_rname (name,e_spreg)) return;
    downshift (name);
    ps = findsym (name,NULL);

    if ((ps == NULL) || !(ps->sy_flags & F_SPREG))
    {
	error (e_spreg);
	return;
    }
    ps->sy_flags |= F_REF;  /* mark symbol as referenced */
    vp->v_val = ps->se.n_value;
}
