/* MACRO.C -- MACRO SUPPORT FUNCTIONS                           */

/* history:                                                     */

/*  6/23/87  J. Howe     --  created                            */
/*  6/03/88  Gumby       --  expanded macro arg local labels    */
/*  8/06/87  J. Howe     --  allow for handling local $ labels  */
/*  4/29/88  Gumby       --  rearranged handling of @ signs     */
/*  8/11/88  J. Ellis    --  various revisions                  */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/11/88  RJK         --  rev 0.17                           */
/* 10/21/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/* 12/13/88  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

/* ------------------------------------------------------------ */

#include "asm29.h"

static char macbuf[MAXBODY+2];

/* ------------------------------------------------------------ */

/*
 * Define a macro.
 */

macdef(name)
char *name;
{
	char *cp;
	struct sym *ps;
	struct macro *pm;
	int argc, i, flg;
	char formal[MAXNAME+1];

	ps = insertsym (name, mactab);
	if (ps == NULL)
		return;
	argc = 0;			/* count args */
	cp = --lptr;			/* back up one & mark place in line */
	do {
		lptr++;			/* skip real or imaginary comma */
		deblank();
		if (get_name (formal))
			argc++;
	} while (*lptr == ',');
	lptr = cp;			/* restore place */

	/* note: if argc == 0, the expression below subtracts the dummy arg */
	i = sizeof (struct macro) + ((argc-1) *sizeof (struct arg));
	pm = (struct macro *) e_alloc (1, i);
	ps->se.n_value = (long) pm;
	pm->m_argc = argc;

	for (i = 0; i < argc; i++)
		{
		lptr++;                                         /* skip the same old comma */
		deblank();
		(void)get_name(formal);
		if (!*formal) panic ("macdef");
		pm->m_args[i].arg_name = savestring (formal);
		}
	i = 0;    /* now read in the macro body */
	flg = 0;
	macbuf[0] = '\0';
	for (;;)
		{
		lpc = 0;
		if (!getline())
			s_fatal("End of file inside macro definition");

		deblank();

		if (get_name (formal)) {
			downshift (formal);
			if (!strcmp (formal,".endm")) break;
		}

		if ((i += (strlen (srcline) + 1)) > MAXBODY) {
			if (!flg)
				error (e_bigmac);
			flg = 1;
		} else {
			(void)strcat(macbuf, srcline);
			(void)strcat(macbuf, "\n");
		}
	}
	pm->m_body = savestring(macbuf);
}

/* ------------------------------------------------------------ */

/*
 * Expand a macro.
 */

macexp (ps)
struct sym *ps;
{
struct include *ip;
struct macro *pm;
int i;
char arg[220];

	pm = (struct macro *)ps->se.n_value;
	i = sizeof (struct include) + ((pm->m_argc-1) * sizeof (struct arg));
	ip = (struct include*) e_alloc (1, i);
	ip->if_link = src;

	/* make the body look like a file */
	s_open(pm->m_body, strlen(pm->m_body), &ip->if_fs);
	ip->if_fp = &ip->if_fs;
	ip->if_name = savestring (ps->sy_name);
	ip->if_type = 'M';
#ifdef NOTDEF
	ip->if_argc = pm->m_argc;
	for (i = 0; i < pm->m_argc; i++)
		{
		scanarg(arg);
		ip->if_args[i].arg_name = pm->m_args[i].arg_name;
		ip->if_args[i].arg_val = savestring (arg);
		if (*lptr == ',')
			lptr++;
		}
#else
	/* Do not quit on null macro names.
	 * Define missing args.
	 */
	for (i = 0; i < pm->m_argc; i++) {
		scanarg(arg);
		ip->if_args[i].arg_name = pm->m_args[i].arg_name;
		ip->if_args[i].arg_val = savestring(arg);
		if (*lptr == ',') {
		    ++lptr;
		} else {
			while (++i < pm->m_argc) {
			    ip->if_args[i].arg_name = pm->m_args[i].arg_name;
			    ip->if_args[i].arg_val = savestring ("");
			}
			break;
		}
	}
	ip->if_argc = i;
#endif
	pm->m_rcnt++;
	ip->if_mac = pm;
	if (pm->m_rcnt > MAXMRECUR)
		error (e_recur);
	src = ip;
	error (0);              /* reset errline */
}




static int
chkr_rep(char *mark,			/* pointer to previous position */
	 char *name,			/* symbol name to check         */
	 int in_str,			/* in-string flag               */
	 int x_rsp)			/* stack level to check (1+)    */
{
    char *acp;                  /* argument string pointer      */
    int c;                      /* argument character           */
    char *cp = name;            /* pointer into original string */
    int i;                      /* scratch                      */
    int n;                      /* replacement mode flag        */
    char *v_fmt;                /* replacement format string    */

/* ------------------------------------------------------------ */

/* preliminary setup            */

    if (rep_formal[x_rsp] == NULL) return (FALSE);
    if (strcmp (rep_formal[x_rsp],name)) return (FALSE);

    strdelete (mark,strlen (name));
    i   = rep_num[x_rsp];
    acp = rep_args[x_rsp][i];

/* ------------------------------------------------------------ */

/* what kind of loop is this?   */

    if (rep_type[x_rsp] == PS_IREP)
    {                           /* ".irep"                      */
	strinsert (mark,acp);
	lptr = mark + strlen (acp);
	return (TRUE);          /* done, exit                   */
    }
    else
    {                           /* ".irepc"                     */
	c  = ((int) acp) & BMASK;
    }

/* ------------------------------------------------------------ */

/* choose "irepc" output format */

    switch (c)
    {
case BSLASH: case DQ: case SQ:
	n = FALSE;
	break;
default:
	n = (c > SPACE) && (c < 0xFF);
	break;
    }

    if (in_str) {
	v_fmt = "%c";

	if (n == FALSE)
	{
	    *cp++ = BSLASH;
	    v_fmt = "%03o";
	}
    }
    else
    {
	v_fmt = n ? "'%c'" : "0x%x";
    }

/* ------------------------------------------------------------ */

/* replace "irepc" argument     */

    sprintf (cp,v_fmt,c);
    strinsert (mark,name);
    lptr = mark + strlen (name);
    return (TRUE);              /* done, exit                   */
}

/* ------------------------------------------------------------ */

/* handles   macro  argument substitution  for "chka_sub()".*/
static int
chka_mac(char *mark,			/* pointer to previous position */
	 char *name)			/* symbol name to check         */
{
    char *cp;                   /* scratch                      */
    int i;                      /* argument counter             */

/* ------------------------------------------------------------ */

    if (src->if_type != 'M') return (FALSE);

    for (i = 0; i < src->if_argc; i++)
    {
	cp = src->if_args[i].arg_name;
	if (strcmp (name,cp)) continue;
				/* macro argument               */
	strdelete (mark,strlen (name));
	cp = src->if_args[i].arg_val;
	strinsert (mark,cp);
	lptr = mark + strlen (cp);
	return (TRUE);
    }

    return (FALSE);
}

/* ------------------------------------------------------------ */

/* "chka_rep()"   handles   ".irep"  and/or  ".irepc"  argument */
/* substitution for "chka_sub()".                               */

/* ------------------------------------------------------------ */

static int
chka_rep(char *mark,			/* pointer to previous position */
	 char *name,			/* symbol name to check         */
	 int in_str)			/* in-string flag               */
{
    int x_rsp;                  /* stack level to check         */

    for (x_rsp = rsp; x_rsp > 0; x_rsp--)
    {
	if (chkr_rep (mark,name,in_str,x_rsp)) return (TRUE);
    }

    return (FALSE);
}

/* ------------------------------------------------------------ */

static int
chka_sub(void)
{
    int at1 = FALSE;            /* 1st "at-sign" flag           */
    int at2;                    /* 2nd "at-sign" flag           */
    int clear = TRUE;
    int delim = 0;
    char *mark;
    int match;
    int n;
    int nch;
    int mflag = 0;
    char name[MAXNAME+1];
    int string = FALSE;

/* ------------------------------------------------------------ */

    lptr = srcline;
    if (no_args) return (0);


    while (*lptr &&
	   ((*lptr != SEMIC) || (*lptr != HASH) || string)) {
	    mark = lptr;

	    if (namech (*lptr) &&
		!isdigit (*lptr) && (*lptr != '$') && clear) {
		    n = get_name(name);
		    at2 = (*lptr == '@');

		    if (n && (!string || at2)) {
			    if (rsp) {	/* check for ".irepc" argument  */
				    match = chka_rep (mark,name,string);
			    } else {	/* check for macro argument     */
				    match = chka_mac (mark,name);
			    }

			    if (match && string) {
				    clear = at2;
			    }

			    if (match && at1) {
				    if (mark[-1] != '@') panic ("argcheck");
				    strdelete (mark-1,1);
				    lptr--;

				    if (at2) {
					    if (*lptr != '@')
						    panic("argcheck");
					    strdelete(lptr,1);
				    }
			    }

			    at1 = FALSE;
			    mflag |= match;
			    continue;
		    }
	    }


/* not an argument              */

	    nch = *lptr++;

	    if (nch == '@') {
		    at1 = clear = TRUE;
		    continue;
	    }

	    if (((nch != SQ) && (nch != DQ))
		|| (string && (nch != delim))
		|| (lptr[-1] == BSLASH)) {
		    ;
	    } else {
		    string = !string;
		    if (string)
			    delim = nch;
	    }

	    at1   = FALSE;
	    clear = !namech (nch) && !string;
    }


    return (mflag);
}

/* ------------------------------------------------------------ */

/*
 * Check for macro arguments -- check "srcline" for anything
 * resembling formal parameters.
 */

argcheck()
{
    while (chka_sub()) {}
}

/* ------------------------------------------------------------ */

/*
 * Scan a line for an argument.  This is anything which does not contain
 *   nulls, commas, or semicolons, except that commas or semicolons are
 *   enclosed in paired punctuation:  (), [], {}, "", ''.    <> are not
 *      paired 'cause .LT. looks dumb.
 */

scanarg (d)
char *d;
{
char  pairtype[30];
short pairp;
char  c, oc, *cp, *base;

	base = d;
	*d = '\0';
	deblank();
	pairp = 0;
	pairtype[0] = oc = '\0';

#ifdef OLD
	while ((c = *lptr) && (((c != ',') && (c != ';')) || pairp))
#else
	while ((c = *lptr) &&
	    ((c != COMMA && c != SEMIC && c != HASH) || pairp))
#endif
		{
		*d++ = c;
		lptr++;
		if (c == '(')
			pairtype[++pairp] = ')';
		if (c == '[')
			pairtype[++pairp] = ']';
		if (c == '{')
			pairtype[++pairp] = '}';
		if (c == '"' || c == '\'')
			{
			if (pairtype[pairp] == c && oc != '\\')
				pairp--;
			  else
				pairtype[++pairp] = c;
			}
		  else
			if ((c == '$') && isdigit (*lptr))
				{
				while (isdigit (*lptr))
					*d++ = *lptr++;
				if (!namech(*lptr)) /* expand temp name in arg */
					{
					cp = tempext();
					while (*cp)
						*d++ = *cp++;
					}
				}
			  else
				if (c == pairtype[pairp])
					pairp--;
		oc = c;
		}
	while ((d > base) && isspace (*--d))
		{
		};
	*++d = '\0';
}

/*
 * Enter predefined macros into the macro table.
 */

#ifdef NOTDEF
char *mnames[] =
	   { "CONSTX", "JMPX", "CALLX","JMPTX","JMPFX","NOP" } ;

char *mbodies[]=
	 {  "\tCONST\trn,cnst\n\tCONSTH\trn,cnst\n",
	"\tCONST\trn,tgt\n\tCONSTH\trn,tgt\n\tJMPI\trn\n",
	"\tCONST\trb,tgt\n\tCONSTH\trb,tgt\n\tCALLI\tra,rb\n",
	"\tCONST\trb,tgt\n\tCONSTH\trb,tgt\n\tJMPTI\tra,rb\n",
	"\tCONST\trb,tgt\n\tCONSTH\trb,tgt\n\tJMPFI\tra,rb\n",
	"\tASEQ\t40#h,gr1,gr1\n"
	 };

char *margs[] = { "rn","cnst","rn","tgt","ra","rb","tgt","ra","rb","tgt",
		  "ra","rb","tgt" };

int margnum[] = { 2,2,3,3,3,0 };

#define NUMMACS 6
#endif

#ifdef NOTDEF
installmacs()
{
struct sym *ps;
struct macro *pm;
int i, j, m = 0;

	for (j = 0; j < NUMMACS; j++)
		{
		ps = insertsym (mnames[j], mactab);
		if (ps == NULL)
			return;
		i = sizeof (struct macro) + ((margnum[j]-1) * sizeof (struct arg));
		pm = (struct macro *) e_alloc (1, i);
		ps->se.n_value = (long) pm;
		pm->m_body = mbodies[j];
		pm->m_argc = margnum[j];
		for (i = 0; i < margnum[j]; i++)
			pm->m_args[i].arg_name = margs[m++];
		}
}
#endif
