/* SYM.C -- ASSEMBLER SYMBOL TABLE SUPPORT FUNCTIONS            */

/* history:                                                     */

/*  n/a      E. Wilner   --  created                            */
/*  9/22/87  J. Howe     --  added cross-reference listing      */
/*  8/11/88  J. Ellis    --  various revisions                  */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/09/88  RJK         --  rev 0.17                           */
/* 10/15/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

/* ------------------------------------------------------------ */

#include "asm29.h"

/* ------------------------------------------------------------ */

char symheader[] =
"SYMBOL                                VALUE       SECTION     LINE\n";

char symunder[] =
"------                                -----       -------     ----\n";

/* ------------------------------------------------------------ */

/*
 * Insert a symbol into a symbol table.
 * Searches specified symbol table (if unspecified, search the main symbol
 * table) for a specified name.  If not found, allocate space for new symbol.
 * Otherwise, post flags for future errors (i.e. multiply-defined and
 * undefined symbols).  Returns pointer to symbol-table entry or NULL if
 * symbol is already defined.
 */

/* ------------------------------------------------------------ */

struct sym*
insertsym(char *s,			/* pointer to new symbol */
	  struct sym *t)		/* pointer to symbol table */
{
    struct sym *p, *q;
    int which, i;

    if (t != NULL)
	p = q = t;
    else
	p = q = symtab;

    if ((s == NULL) || !*s) panic ("insertsym");

    while (p != NULL)
		{
		q = p;
		switch (which = cmpstr (s, p->sy_name))
			{
			case 0:
#ifdef CS_IF
				p->sy_f2 |= F2_DEFP;
#endif

				if ((pass == 3) && (p->sy_flags & F_MULDEF))
					{
					error (e_muldef);
					return p;
					}
				if (p->sy_flags & F_UNDEF )
					{
					p->sy_flags &= ~(F_UNDEF | F_RELOC);
					return p;
					}
				if (pass == 1)
					{
					p->sy_flags |= F_MULDEF;
					return NULL;
					}
				return p;
			case -1:
				p = p->sy_llink;
				break;
			case 1:
				p = p->sy_rlink;
			}
		}
	if ((i = strlen(s)) > (S_LEN-1))
		{
		p = (struct sym *) e_alloc (1, (sizeof(*p)) + i + 1);
		p->sy_name = (char *)(p+1);
		if (i == S_LEN)
			strcpy (p->se._n._n_name, s);
		}
	else
		{
		p = (struct sym *) e_alloc (1, (sizeof(*p)));
		p->sy_name = p->se._n._n_name;
		}

	p->sy_flags = 0;
#ifdef CS_IF
	p->sy_f2 |= F2_DEFP;
#endif

	if (which > 0)
		q->sy_rlink = p;
	  else
		q->sy_llink = p;
	strcpy (p->sy_name,s);
	return p;
}

/* ------------------------------------------------------------ */

/*
 * Find a symbol in a specified symbol table.  Returns pointer to symbol-table
 *   entry if found, otherwise returns NULL ptr.
 */

/* ------------------------------------------------------------ */

struct sym*
findsym(char *s,			/* pointer to desired symbol */
	struct sym *t)			/* pointer to symbol table */
{
struct sym *p;

	if (t != NULL)
		p = t;
	  else
		p = symtab;

	if ((s == NULL) || !*s) panic ("findsym");

	while (p != NULL)
		switch (cmpstr (s, p->sy_name))
			{
			case 0:
				return p;
			case -1:
				p = p->sy_llink;
				break;
			case 1:
				p = p->sy_rlink;
			}
	return NULL;
}

/* ------------------------------------------------------------ */

/*
 * Search the main symbol table for a symbol, which is assumed to be present.
 *   If not, or if it is undefined, an error is generated and the symbol is
 *   inserted if necessary.  A pointer to the symbol is returned.
 */

/* ------------------------------------------------------------ */

struct sym *
searchsym(char *name)
{
	struct sym *ps;
	static struct sym dot;

	ps = NULL;
	if (ps == NULL)
		ps = findsym (name,NULL);
	if ((ps == NULL) && (!strcmp (name, ".")))
		{
		/* make '.' absolute if the current segment is */
		dot.se.n_value = lpc;
		if (cursegp->sg_head.s_flags & STYP_ABS) {
		    dot.se.n_scnum = 0;
		} else {
		    dot.se.n_scnum = curseg;
		}
		ps = &dot;
		}
	if (ps == NULL)
		{
		ps = insertsym(name,NULL);
		if (ps == NULL) panic ("searchsym");
		ps->sy_flags = F_UNDEF;
		}
	if (ps->sy_flags & F_UNDEF)
		error(e_undef);
	return ps;
}

/* ------------------------------------------------------------ */

/*
 * Print symbols to list device.
 */

/* ------------------------------------------------------------ */

prsyms(struct sym *st)
{
	char *cp;
	int i, pflag = 0;
	static unsigned int prsyno = 0;
	struct xrefer *xp;
	struct seglist *sl;
	char typname[20], *syname;

	if (st == NULL)
		return;
	prsyms (st->sy_llink);
	strcpy (typname," ");
	if (st->sy_flags & (F_EXTERN | F_PUBLIC)) {
		syname = st->sy_name;
		if (st->sy_flags & F_EXTERN)
			strcpy (typname, "EXTERNAL");
		else
			if (st->sy_flags & F_PUBLIC)
				strcpy (typname, "GLOBAL");
		if (st->sy_flags & F_UNDEF)
			strcpy (typname, "UNDEFINED");
		pflag = 1;

	} else if (!(st->sy_flags & F_ASMGEN) && (st->sy_name[0] != 'L')) {
		if (psyms) {
			if ((st->sy_flags ^ (F_SPREG | F_REF)) == 0) {
				pflag = 1;
				syname = st->sy_name;
				strcpy (typname, "SPECIAL REGISTER");
			} else if (!(st->sy_flags & F_SPREG)
				   && (st->sy_name[0] != 'L')) {
				syname = st->sy_name;
				if (st->sy_flags & F_UNDEF)
					strcpy (typname, "UNDEFINED");
				else if (st->sy_flags & F_RELOC)
					strcpy (typname, "RELOC ");
				else if (st->sy_flags & F_REGSYM) {
					if (st->se.n_value < 128)
						sprintf(typname, "GR%ld",
							st->se.n_value);
					else
						sprintf(typname, "LR%ld",
							st->se.n_value - 128);
				}
				pflag = 1;
			}
		}

	} else if (pasyms) {
		syname = st->sy_name;
		strcpy (typname, "AS29 SYMBOL");
		pflag = 1;
	}

	if (pflag) {
#ifdef OLD
		if (newpage()) printf ("\n%s%s",symheader,symunder);
#else
		if (newpage() || !prsyno) {
			printf ("\n%s%s",symheader,symunder);
			prsyno++;
		}
#endif
		if (typname[0] != ' ') {
			printf ("%-19s", syname);
			printf ("%-16s", typname);
		} else {
			printf ("%-35s", syname);
		}
		printf ("%9lx", st->se.n_value);
		if (st->se.n_scnum > 0) {
			sl = sectlist;
			for (i = 1; i < st->se.n_scnum; i++)
				sl = sl->sg_link;
			cp = nam_str (sl->sg_head.s_name);
			if (st->sy_defline)
				printf("       %-12s%d", cp, st->sy_defline);
			else
				printf("       %-12s",cp);
		} else if (st->sy_defline) {
			printf("%19s%d", "", st->sy_defline);
		}
		printf ("\n");
		pageline++;
	}

	if (xref && pflag) {
		xp = st->sy_xref;
		while (xp != NULL) {
			if (newpage())
				printf ("\n\n%s%s", symheader, symunder);
			printf ("     referenced:");
			for (i = 0; i < xp->xr_cnt; i++)
				printf (" %5d", xp->xr_list[i].xr_line);
			printf ("\n");
			pageline++;
			xp = xp->xr_link;
		}
	}
	prsyms (st->sy_rlink);
}
