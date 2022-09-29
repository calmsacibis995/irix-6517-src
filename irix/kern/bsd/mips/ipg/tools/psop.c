/* PSOP.C -- PSEUDO-OP SUPPORT FUNCTIONS                        */

/*
 * history:
 *
 *  6/23/87  J. Howe     --  created
 *  4/28/88  Gumby       --  re-arranged use of comma() and eol()
 *                           to avoid infinite looping on invalid
 *                           operands like .WORD et al
 *  8/11/88  J. Ellis    --  various revisions
 */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/12/88  RJK         --  rev 0.17                           */
/* 10/25/88  RJK         --  rev 0.18                           */
/* 11/21/88  RJK         --  rev 0.19                           */
/* 12/13/88  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */
/*  5/22/89  RJK         --  rev 1.0d0                          */

/* ------------------------------------------------------------ */

#include "asm29.h"

#define STYP_FOO    (STYP_DATA | STYP_TEXT | STYP_BSS | STYP_LIT)

extern char listsubttl[],listtitle[];

extern struct seglist defsegs[];

#define BSS_SECT    3

/* ------------------------------------------------------------ */

#define SA_ENDEF    0
#define SA_DIM      1
#define SA_VAL      2
#define SA_SIZE     3
#define SA_LINE     4
#define SA_TAG      5
#define SA_SCL      7
#define SA_TYPE     6
#define MAXSA       8

/* ------------------------------------------------------------ */

char sanames[][8] =
{
    ".endef",
    ".dim",
    ".val",
    ".size",
    ".line",
    ".tag",
    ".type",
    ".scl"
};

/* ------------------------------------------------------------ */

int sectnum[] =
{
     0, -1, -3, -3, -1, -3, -3,  0,
    -1, -1, -2, -1, -2, -2, -3, -2,
    -1, -1, -1,  0, -3, -3, -1, -2,
    -2, -2
};

#define NSECTN      ((sizeof(sectnum))/(sizeof(sectnum[0])))

/* ------------------------------------------------------------ */

/* "chk_eqs()"  is a  support  function  for the  ".ifeqs"  and */
/* ".ifnes" directives.                                         */

/* ------------------------------------------------------------ */

chk_eqs()
{
    char ch;

    deblank();
    ch = *lptr;

    if ((ch <= SPACE) || (ch == COMMA) || (ch == SEMIC))
    {
	error (e_nooperand);
    }
}

/* ------------------------------------------------------------ */

use_ps(ps)
struct sym *ps;                 /* segment symbol pointer       */
{
    long pos;                   /* file position                */
    long ss;                    /* section size in bytes        */

    if ((ps == NULL) || (ps->sy_segx != SEGX_MARK))
    {                           /* bad segment symbol           */
	panic ("use_ps");
    }

    curseg  = ps->se.n_scnum;
    cursegp = (struct seglist *) ps->se.n_value;
    ss      = cursegp->sg_head.s_size;

    pc  = cursegp->sg_head.s_paddr + ss;
    lpc = pc;

    if (pass != 3) return;
#ifdef OLD
    pos = cursegp->sg_head.s_scnptr + pc;
#else
    pos = cursegp->sg_head.s_scnptr + ss;
#endif
    s_seek (outf,pos,0);
}

/* ------------------------------------------------------------ */

void
useseg(char *name)
{
    struct sym *ps = NULL;
    static struct sym star;

    if (!strcmp (name,"*"))
    {
	if (prevsegp == NULL)
	{
	    error (e_nsseg);
	    return;
	}
	ps = &star;
	star.sy_segx = SEGX_MARK;
	star.se.n_scnum = prevseg;
	star.se.n_value = (long) prevsegp;
    }

    if (!strcmp (name,"$$"))
    {
	ps = dummy;
	dsection->sg_head.s_size = 0;
    }

    if (ps == NULL)
    {
	ps = findsym (name,segtab);
    }

    if (ps == NULL)
    {
	error (e_nsseg);
	return;
    }

    prevseg  = curseg;
    prevsegp = cursegp;
    endseg (1);
    use_ps (ps);
}

/* ------------------------------------------------------------ */

/*
 * Handle assembler pseudo-ops.
 */

do_psop()
{
    long bool;
    char buf[120];
    int c;
    char *cp;
    union dblflt dbl_flt;
    float fl;
    int found;
    int i,j,k;
    long lx;
    char name[120];
    char name2[120];
    int nr;
    long num;
    int opc;
    ulong uval;

    struct lineno   ln;
    struct sym      *ps;
    struct sym      *res;
    struct scnhdr   sh;
#ifdef NOTDEF
    struct symlist  *sl;
#endif
    struct value    v;
    struct value    *vp;



    if ((saptr != NULL) &&
	((inst->rs_opcode < PS_VAL)
	 || (inst->rs_opcode > PS_ENDEF))) {
	    error (e_symatt);
	    return;
    }
    deblank();

    switch (opc = inst->rs_opcode) {
default:
	    panic ("do_psop");
	break;                  /* superfluous                  */

case PS_SECT:
	bzero ((char *) &sh,SCNHSZ);

	if (!get_name (name) || (strlen (name) > S_LEN))
	{
	    error (e_operand);
	    return;
	}
	(void)strncpy(sh.s_name,name,S_LEN);
	(void)comma(1);

	do
	{
	    if (!get_rname (name,e_operand)) return;
	    downshift (name);

	    switch (TRUE)
	    {                   /* REWRITTEN 8/21/88            */
	default:
		if (!strcmp (name,"data"))
		{
		    if (sh.s_flags & STYP_FOO) error (e_2type);
		    sh.s_flags = STYP_DATA;
		    break;
		}
		if (!strcmp (name,"text"))
		{
		    if (sh.s_flags & STYP_FOO) error (e_2type);
		    sh.s_flags = STYP_TEXT;
		    break;
		}
		if (!strcmp (name,"bss"))
		{
		    if (sh.s_flags & STYP_FOO) error (e_2type);
		    sh.s_flags = STYP_BSS;
		    break;
		}
		if (!strcmp (name,"lit"))
		{
		    if (sh.s_flags & STYP_FOO) error (e_2type);
		    sh.s_flags = STYP_LIT;
		    break;
		}
		if (!strcmp (name,"absolute")) {
			(void)comma(0);
			sh.s_flags |= STYP_ABS;
			sh.s_vaddr = sh.s_paddr = getint();
			break;
		}
		error (e_badopt);
		return;
	    }
	    (void)comma(1);
	}
	    while (!eol());

	if ((sh.s_flags & STYP_FOO) == 0)
	{
	    error (e_notype);
	    return;
	}
	addseg (&sh);
	lpc = 0;
	break;

case PS_LCOMM:                  /* alloc space in bss sect */
	if (!get_rname (name,e_notag))
		return;
	ps = insertsym(name,NULL);
	if (ps == NULL)
		return;

	(void)comma(1);
	v.v_val = getintb(0L,0x7FFFFL);

	if (curseg == BSS_SECT) {
		ps->se.n_value = lpc = pc;
		pc += v.v_val;
	} else {
		lpc = defsegs[BSS_SECT-1].sg_head.s_size;
		ps->se.n_value = lpc;
		defsegs[BSS_SECT-1].sg_head.s_size += v.v_val;
	}
	ps->se.n_sclass = C_EXT;
	ps->se.n_scnum  = BSS_SECT;
	break;

case PS_COMM:
	if (!get_rname (name,e_notag)) return;
	ps = findsym (name,NULL);

#ifdef OLD
	if ((ps == NULL) || !(ps->sy_flags & F_EXTERN)) {
		ps = insertsym (name,NULL);
	}
#else
	if (ps != NULL) {
		lx = ps->sy_flags & (F_EXTERN | F_UNDEF);

		if (lx == 0) {
			skip_eol();
			return;
		}

		if (lx == F_UNDEF)
			ps = NULL;
	}

	if (ps == NULL) ps = insertsym (name,NULL);
#endif

	if (ps == NULL)
		return;
	(void)comma(1);
	ps->sy_flags |= F_EXTERN;
	ps->se.n_sclass = C_EXT;

#ifdef OLD
	num = getintb(0L,0x7FFFFL);	/* number of bytes */
#else
	deblank();
	c = *lptr;
	uval = (ulong) getint();	/* number of bytes */

	if (c == '-') {
		error (e_range);
		uval = 0;
	}
#endif

	if (uval > (ulong) ps->se.n_value) {
		ps->se.n_value = uval;
	}

#ifdef NOTDEF
	if (pass != 1)
		break;

	if ((sl = extlist) == NULL) {
		sl = extlist = (struct symlist *) e_alloc (1,sizeof(*sl));
	} else {
		while (sl->sl_link != NULL)
			sl = sl->sl_link;
		sl = (sl->sl_link = (struct symlist *)e_alloc(1,sizeof(*sl)));
	}
	sl->sl_sym = ps;
#endif
	break;

case PS_BLOCK:
	/* listing the PC is interesting */
	if (curseg != DSECT)
		listpc = 1;
	v.v_val = getintb(0L,0x7FFFFFL);

#ifdef OLD
	if (!(cursegp->sg_head.s_flags & STYP_BSS)
	    && (curseg != DSECT))
#else
		if ((curseg != DSECT) && !cs_flags (STYP_BSS))
#endif
			{
				for (i = 0; i < v.v_val; i++)
					genbyte(0);
	} else {
		pc += v.v_val;
	}
	break;

case PS_DSECT:
	useseg ("$$");
	break;

case PS_TEXT:
	useseg (".text");
	break;

case PS_DATA:
	useseg (".data");
	break;

case PS_USE:
	if (*lptr == '*')
	{
	    lptr++;
	    useseg ("*");
	    break;
	}
	if (!get_rname (name,e_notag)) break;
	useseg (name);
	break;

case PS_BYTE:
case PS_WORD:
case PS_HWORD:
	do {
		num = 1;
		if (*lptr == '[') {
			lptr++;
			num = getintb(0L,4092L);

			if (*lptr++ != ']') {
				error (e_mult);
				return;
			}
		}
		getval(&v);

		switch (inst->rs_opcode) {
		case PS_BYTE:		/* bounds are 0 - 0xff */
			v.v_val = bcheck(v.v_val,-128L,255L);
			v.v_reloc = R_BYTE;
			for (i = 0; i < num; i++)
				genvbyte(&v);
			break;

		case PS_WORD:
			ck_align (4);
			v.v_reloc = R_WORD;
			for (i = 0; i < num; i++)
				genvword(&v);
			break;

		case PS_HWORD:		/* bounds are 0 - 0xffff */
			v.v_val = bcheck(v.v_val,-32768L,65535L);
			ck_align (2);
			v.v_reloc = R_HWORD;
			for (i = 0; i < num; i++)
				genvhword(&v);
			break;
		}
	} while (comma(0));
	break;

case PS_FLOAT:
	do {
		num = 1;
		if (*lptr == '[') {
			lptr++;
			num = getintb (0L,4092L);

			if (*lptr++ != ']') {
				error (e_mult);
				return;
			}
		}
		zap (&v);
		deblank();
		fl = getfloat();
		ieee((double) fl, (char *) &v.v_val);
		align (4);
		for (i = 0; i < num; i++)
			genvword(&v);
	} while (comma (0));
	break;

case PS_DOUBLE:
	do {
		num = 1;
		if (*lptr == '[') {
			lptr++;
			num = getintb (0L,4092L);

			if (*lptr++ != ']') {
				error (e_mult);
				return;
			}
		}
		zap(&v);
		deblank();
		dbl_flt.d = getdouble();
		ieeed(dbl_flt.d, (char*)&dbl_flt.l[0]);
		align(4);

		for (i = 0; i < num; i++) {
			v.v_val = dbl_flt.l[0]; /* MSB first */
			genvword(&v);
			v.v_val = dbl_flt.l[1];
			genvword(&v);
		}
	} while (comma (0));
	break;

case PS_EXTEND:         /* >>><<< for now, get a double and pad with 0's */
	do {
		num = 1;
		if (*lptr == '[') {
			lptr++;
			num = getintb (0L,4092L);

			if (*lptr++ != ']') {
				error (e_mult);
				return;
			}
		}
		zap (&v);
		deblank();
		dbl_flt.d = getdouble();
		ieeex (dbl_flt.d,(char *) &ext_float[0]);
		align (4);

		for (i = 0; i < num; i++) {
			v.v_val = ext_float[0];
			genvword (&v);
			v.v_val = ext_float[1];
			genvword (&v);
			v.v_val = ext_float[2];
			genvword (&v);
			v.v_val = ext_float[3];
			genvword (&v);
		}
	} while (comma(0));
	break;

case PS_EXTERN:
	do {
	    if (!get_rname (name,e_notag)) break;
	    ps = findsym (name,NULL);
	    if (ps != NULL) continue;
	    ps = insertsym (name,NULL);
	    if (ps == NULL) continue;

	    ps->sy_flags |= F_EXTERN;
	    ps->se.n_sclass = C_EXT;

#ifdef NOTDEF
	    if (pass == 1)
	    {
		if ((sl = extlist) == NULL)
		{
		    sl = extlist =
			(struct symlist *) e_alloc (1,sizeof(*sl));
		}
		else
		{
		    while (sl->sl_link != NULL) sl = sl->sl_link;
		    sl = (sl->sl_link =
			(struct symlist *) e_alloc (1,sizeof(*sl)));
		}
		sl->sl_sym = ps;
	    }
#endif
	} while (comma(0));

	lpc = 0;
	break;

case PS_GLOBAL:
	do {
		if (!get_rname (name,e_notag))
			break;

		if ((ps = findsym (name,NULL)) == NULL) {
			ps = insertsym (name,NULL);
			if (ps == NULL) panic ("PS_GLOBAL");
			ps->sy_flags = F_UNDEF;
		} else {
			if (ps->sy_flags & F_EXTERN) {
				/* previously COMMON or EXTERN  */
				ps->sy_flags   = F_UNDEF;
				ps->se.n_value = 0;
			}
		}

		ps->sy_flags |= F_PUBLIC;
		ps->se.n_sclass = C_EXT;
#ifdef NOTDEF
		if (pass == 1)
			ndefs++;
#endif
	} while (comma(0));

	lpc = 0;
	break;

case PS_ASCII:
	if (*lptr++ != DQ)
	{
	    error (e_text);
	    break;
	}
	zap (&v);

	while ((v.v_val = (long) strchar (DQ)) != -1)
	{
	    genvbyte (&v);
	}
	if (*lptr == DQ) lptr++;
	break;

case PS_EQU:
	if (!get_rname (tagname,e_notag))
		break;
	(void)comma(1);
	getval (&v);
	tag = insertsym (tagname,NULL);
	if (tag == NULL) break;

	if (tag->sy_flags & F_VAL) {
		free ((char *) (struct value *) tag->se.n_value);
		tag->sy_flags &= ~F_VAL;
	}

	if (v.v_ext != NULL || v.v_seg > 0) {
		vp  = (struct value *) e_alloc (1,sizeof(*vp));
		*vp = v;
		lpc = (long) vp;
		tag->se.n_value = lpc;
		tag->sy_flags |= F_VAL;
	} else {
		lpc = v.v_val;
		tag->se.n_value = lpc;
		tag->se.n_scnum = N_ABS;
	}
	break;

case PS_SET:
	if (!get_rname (tagname,e_notag)) break;
	tag = findsym (tagname,NULL);

	if (tag == NULL)
	{
	    tag = insertsym (tagname,NULL);
	    if (tag == NULL) panic ("PS_SET");
	    tag->sy_flags = F_SETSYM;
	}

	if ((tag->sy_flags & F_SETSYM) == 0)
	{
	    error (e_muldef);
	    tag->sy_flags |= F_MULDEF;
	}

	tag->sy_flags &= ~F_UNDEF;
	(void)comma(1);
	getval (&v);

	if (tag->sy_flags & F_VAL)
	{
	    free ((char *) (struct value *) tag->se.n_value);
	    tag->sy_flags &= ~F_VAL;
	}

	if ((v.v_ext != NULL) || (v.v_seg > 0))
	{
	    vp  = (struct value *) e_alloc (1,sizeof(*vp));
	    *vp = v;
	    lpc = (long) vp;
	    tag->se.n_value = lpc;
	    tag->sy_flags |= F_VAL;
	}
	else
	{
	    lpc = v.v_val;
	    tag->se.n_value = lpc;
	}
	break;

case PS_IDENT:
	deblank();

	if ((*lptr++ != DQ) || (strchar (DQ) < 0)) {
		error (e_operand);
		break;
	}

	lptr--;				/* point to first character     */
	bzero ((char *) &sh,SCNHSZ);
	(void)strncpy (sh.s_name,".comment",S_LEN);
	sh.s_flags = STYP_INFO;
	addseg (&sh);
	useseg (nam_str (sh.s_name));
	lpc = 0;

	zap(&v);
	while ((c = strchar (DQ)) >= 0) {
		v.v_val	= c & BMASK;
		v.v_reloc = R_BYTE;
		genvbyte(&v);
	}

	useseg ("*");
	while (*lptr) lptr++;
	break;

case PS_REG:
	if (!get_rname (tagname,e_notag)) break;
	(void)comma(1);
	(void)getreg(&v,1);
	tag = insertsym (tagname,NULL);
	if (tag == NULL) break;

	if (tag->sy_flags & F_VAL)
	{
	    free ((char *) (struct value *) tag->se.n_value);
	    tag->sy_flags &= ~F_VAL;
	}
	tag->sy_flags |= F_REGSYM;

	if ((v.v_ext != NULL) || (v.v_seg > 0)) {
		vp  = (struct value *) e_alloc (1,sizeof(*vp));
		*vp = v;
		lpc = (long) vp;
		tag->se.n_value = lpc;
		tag->sy_flags |= F_VAL;
	} else {
		lpc = v.v_val;
		tag->se.n_value = lpc;
		tag->se.n_scnum = N_ABS;
	}
	break;

case PS_ALIGN:
	if (eol())
		num = 4;
	else
		num = getintb (0L,16L);

	if ((num == 1) || ((num & 1) == 0))
		align (num);
	else
		error (e_operand);
	break;

case PS_MACRO:
	if (get_rname (tagname,e_notag)) {
		/* successful */
		(void)comma(1);
		macdef(tagname);
	}
	lpc = 0;
	break;

case PS_ENDM:                   /* end a macro                  */
	error (e_endm);         /* we're not in one             */
	break;

case PS_EXITM:
	if (srctype != 'M') {		/* not in a macro               */
		error (e_notinmac);
		break;
	}
	ifsp = m_ifsp;
	(void)inclose(0);
	break;

case PS_PURGEM:
	while (namech (*lptr) && !eol()) {
		if (!get_rname (name,e_notag)) break;
		ps = mactab;
		found = FALSE;

		while ((ps != NULL) && !found) {
			switch (cmpstr (name,ps->sy_name)) {
			case -1:
				ps = ps->sy_llink;
				break;
			case 1:
				ps = ps->sy_rlink;
				break;
			case 0:
				found = TRUE;
				break;
			}
		}

		if (found) ps->sy_flags |= F_UNDEF;
		(void)comma(1);
	}

	lpc = 0;
	break;


/* ------------------------------------------------------------ */

case PS_REP:
	if (eol()) {
		error (e_operand);
		break;
	}

	num = getint();
	if (num <= 0) num = 0;
	nr = rsp+1;

	rep_cnt[nr]    = num;
	rep_formal[nr] = NULL;
	rep_num[nr]    = num;
	rep_type[nr]   = PS_REP;
	goto rep_code;

/* ------------------------------------------------------------ */

case PS_IREP:
	if (eol()) {
		error (e_operand);
		break;
	}

	if (!get_rname (name,e_operand)) break;
	nr = rsp+1;
	rep_type[nr] = PS_IREP;
	rep_formal[nr] = savestring (name);

/* ------------------------------------------------------------ */

#ifdef OLD
	cp = lptr;
	j = 0;

	while (*cp) {
		if (*cp++ == COMMA)
			j++;
	}

	for (i = 0; (i < j) && *lptr && (i < MAXRARGS); i++) {
		(void)comma(1);
		getval (&v);

		rep_args[rsp+1][j-i-1] =
		(struct value *) e_alloc (1,sizeof(struct value));

		*rep_args[rsp+1][j-i-1] = v;
	}
#endif

/* ------------------------------------------------------------ */

	for (cp = lptr, i = 0; i < MAXRARGS; i++)
	{
	    deblank();
	    if (*lptr != COMMA) break;
	    lptr++;
	    scanarg (buf);
	}

	for (lptr = cp, j = i, i = 0; i < j; i++)
	{
	    deblank();
	    if (*lptr != COMMA) break;
	    lptr++;
	    scanarg (buf);
	    rep_args[nr][j-i-1] = savestring (buf);
	}

	rep_args[nr][i] = rep_formal[nr];
	rep_cnt[nr]     = i;
	rep_num[nr]     = i;
	goto rep_code;

/* ------------------------------------------------------------ */

case PS_IREPC:
	if (eol()) {
		error (e_operand);
		break;
	}

	if (!get_rname (name,e_operand)) break;
	nr = rsp+1;
	rep_type[nr] = PS_IREPC;
	rep_formal[nr] = savestring (name);
	i = 0;
	(void)comma(1);

	if (*lptr != DQ)
	{
	    error (e_quote);
	    break;
	}

	lptr++;                 /* skip over quote              */
	j = 0;                  /* number of arguments          */

#ifdef OLD
	cp = lptr;
	while (*cp && *cp++ != DQ) j++;
#else                           /* BUG FIX 10/15/88             */
	cp = lptr;
	while (strchar (DQ) >= 0) j++;
	lptr = cp;
#endif

	if (j > MAXRARGS)
	{
	    fatal (e_operand);
	}

	while ((v.v_val = (long) strchar (DQ)) >= 0)
	{
#ifdef OLD
	    rep_args[rsp+1][j-i-1] =
		(struct value *) e_alloc (1,sizeof(struct value));
	    *rep_args[rsp+1][j-i-1] = v;
	    i++;
#else
	    k = (int) (v.v_val & BMASK);
	    rep_args[nr][j-i-1] = (char *) k;
	    i++;
#endif
	}

	if (*lptr == DQ) lptr++;
	rep_args[nr][i] = rep_formal[nr];
	rep_cnt[nr]     = i;
	rep_num[nr]     = i;

/* ------------------------------------------------------------ */

/*
 * If this is the outermost loop of a repeat block, then the
 * following code reads source lines until the final ".endr" is
 * reached.  Then the file pointer is repositioned to the first
 * executable line of the repeat.
 */

/* ------------------------------------------------------------ */

rep_code:
	lab_cnt++;              /* handle local labels */

	if (rsp == 0)           /* outer repeat */
	{
	    rsp++;
	    rep_fpos[rsp]    = s_tell (src->if_fp);
	    rep_lastype[rsp] = src->if_type;
	    i = 1;

	    for (;;) {
		no_args = TRUE;

		if (!getline()) {
			x_error ("EOF during repeat block");
		}

		no_args = FALSE;
		deblank();
#ifdef OLD
		get_name (name);
#else
		if (!get_name (name)) { lpc = 0; continue; }
#endif
		downshift (name);

		switch (TRUE)
		{
	    default:
		    i++;
		    if (!strcmp (name,".rep"   )) break;
		    if (!strcmp (name,".irep"  )) break;
		    if (!strcmp (name,".irepc" )) break;
		    i--;
		    break;
		}

		if (!strcmp (name,".endr"))
		{
		    if (--i == 0) break;
		}
		lpc = 0;
	    }

	    rep_lineno[rsp] = src->if_lineno;
	    src->if_type    = 'R';
	    src->if_lineno  = 0;
	}
	else
	{
	    if (rsp >= REPNEST-1)
	    {
		fatal (e_repnest);
	    }
	    rsp++;
	    rep_fpos[rsp] = s_tell (src->if_fp);
	    lineno = 0;     /* suppress listing of .rep  */
	    src->if_lineno--;
	    if (rep_num[rsp]) rep_num[rsp]--;
	    break;
	}

/* NO BREAK here, drop to .endr routine if first .rep */

/* ------------------------------------------------------------ */

case PS_ENDR:
	if (rsp < 0) fatal (e_repnest);

	if (rsp == 0)
	{
	    error (e_repnest);
	    break;
	}

	if (src->if_lineno > 0) /* suppress listing of .endr */
	{
	    lineno = 0;
	    src->if_lineno--;
	}

	if (rep_num[rsp] <= 0)
	{
	    if (rsp == 1)
	    {
		src->if_type = rep_lastype[1];
		src->if_lineno = rep_lineno[1];
		lab_cnt -= (rep_cnt[rsp] + 1); /* -1 for extra cnt */
	    }
	    rsp--;              /* BUG FIX 11/21/88             */
	}
	else
	{
	    s_seek (src->if_fp,rep_fpos[rsp],0);
	    rep_num[rsp]--;
	    lab_cnt++;
	}
	break;

/* ------------------------------------------------------------ */

case PS_INCLUDE:
	if (*lptr++ != DQ) {
		error (e_text);
		break;
	}
	i = 0;
	while ((j = strchar (DQ | NO_BSL)) != -1)
		name[i++] = j;
	lptr++;
	name[i] = EOS;
	if (infopen (name,'I',0) < 0)
		error(e_nofile);
	lab_cnt++;
	break;

/* ------------------------------------------------------------ */

case PS_TITLE:
case PS_SUBTTL:
	if (*lptr++ != DQ)
	{
	    error (e_text);
	    break;
	}
	i = 0;
	cp = (opc == PS_TITLE) ? listtitle : listsubttl;
	while ((j = strchar (DQ)) != -1) cp[i++] = j;
	lptr++;
	cp[i] = EOS;
	break;

case PS_IF:
	if (++ifsp > IFNEST-1) fatal (e_condnest);
	/* do not evaluate the arg if already skipping */
	if (ifstk[ifsp-1]) {
		ifstk[ifsp] = 1;
		skip_eol();
	} else {
		ifstk[ifsp] = !(v.v_val = getint());
	}
	listuncond = !ifstk[ifsp-1];
	break;

case PS_IFDEF:
case PS_IFNDEF:
	if (++ifsp > IFNEST-1) fatal (e_condnest);
	if (!get_rname (name,e_notag)) strcpy (name,"XYZZY");
	bool = 0L;
	res  = findsym (name,NULL);

	if (res == NULL)
	{
	    res = findsym (name,mactab);
	}

	if (res != NULL) {
	    bool = !(res->sy_flags & F_UNDEF);
#ifdef CS_IF
	    if (!(res->sy_f2 & (F2_DEFP | F2_DDEF)))
		bool = FALSE;
#endif
	}

	if (inst->rs_opcode == PS_IFDEF) {
	    ifstk[ifsp] = ifstk[ifsp-1] || !bool;
	} else {
	    ifstk[ifsp] = ifstk[ifsp-1] || bool;
	}
	listuncond = !ifstk[ifsp-1];
	break;

case PS_IFEQS:
case PS_IFNES:
	if (++ifsp > IFNEST-1) fatal (e_condnest);
	chk_eqs();
	scanarg (name);
	if (*lptr == COMMA) lptr++;

	chk_eqs();
	scanarg (name2);
	i = strcmp (name,name2);

	if (inst->rs_opcode == PS_IFEQS)
	{
	    ifstk[ifsp] = ifstk[ifsp-1] || (i != 0);
	}
	else
	{
	    ifstk[ifsp] = ifstk[ifsp-1] || (i == 0);
	}
	listuncond = !ifstk[ifsp-1];
	break;

case PS_ELSE:
	if (ifsp < 1)
	{
	    error (e_endcond);
	    break;
	}
	ifstk[ifsp] = ifstk[ifsp-1] || !ifstk[ifsp];
	listuncond = !ifstk[ifsp-1];
	break;

case PS_ENDIF:
	if (ifsp < 1)
	{
	    error (e_endcond);
	    break;
	}
	ifsp--;
	break;

case PS_ERR:
	error(e_user);
	break;

case PS_LFLAGS:
	lflags (lptr);
	while (*lptr) lptr++;
	break;

case PS_LIST:
	if (list) llist = 1;
	break;

case PS_NOLIST:
#ifdef OLD
	if (list) llist = 0;
#else
	if (list) llist = (-llist);
#endif
	break;

case PS_PRINT:
	if (*lptr != DQ)
	{
	    error (e_text);
	    break;
	}
	cp = name;
	lptr++;
	while ((i = strchar (DQ)) >= 0) *cp++ = i;
	lptr++;
	*cp = EOS;
	if (pass == 3) printf ("%s\n",name);
	break;

case PS_EJECT:
	if (pageline > 1) pageline = pglength;
	break;

case PS_SPACE:
	if (pass == 3)
	{
	    i = getintb (0L,(long) pglength);
	    if (i < (j = pglength - pageline)) j = i;
	    for (i = 0; i < j; i++) printf ("\n");
	}
	lineno = 0;
	break;

case PS_END:
	(void)getline();

	if (pass == 3) {
		while (!endafile) {
			if (srcline[0] != EOS) {
				printf ("\n text encountered after .end\n\n");
				break;
			}
			readline (src->if_fp,srcline);
			lineno++;
		}

		lptr  = srcline;
		*lptr = EOS;
	}

	endafile = TRUE;
	break;

case PS_ENDEF:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	idbugsym();
	saptr = NULL;
	break;

case PS_SCL:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	v.v_val = getint();
	saptr->db_cf.n_sclass = (char) v.v_val;

/* the scnum field depends on storage class */

	if ((v.v_val < 0) || (saptr->db_cf.n_scnum != -3))
	{
	    v.v_val = (-3);
	}
	else
	{
	    i = (int) v.v_val;
	    j = (i < C_BLOCK) ? i : i-80;

	    if ((j < 0) || (j > NSECTN-1))
	    {
		error (e_range);
		j = 0;
	    }
	    v.v_val = sectnum[j];
	}

	if (v.v_val != -3)
	{
	    saptr->db_cf.n_scnum = v.v_val;
	}
	break;  /* >>><<< may have to do more checks here */

case PS_TYPE:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	v.v_val = getint();
	saptr->db_cf.n_type = (unsigned short) v.v_val;

	if (((v.v_val & 0x030) == (DT_FCN << 4)) &&
	    ((saptr->db_cf.n_sclass == C_EXT) ||
		(saptr->db_cf.n_sclass == C_STAT)))
	{
	    ln.l_lnno = 0;
	    ln.ll_symndx = ndbsyms;
	    auxsym.xx_lnnoptr = cursegp->sg_head.s_lnnoptr +
		cursegp->sg_head.s_nlnno * LINESZ;
	    auxsym.xx_endndx = ndbsyms+2;

#ifdef SWAP_COFF
	    if (target_order != host_order)
	    {
	       putswap (auxsym.xx_lnnoptr,
		   (char *) &auxsym.xx_lnnoptr,
		       sizeof(long));
	       putswap (auxsym.xx_endndx,
		   (char *) &auxsym.xx_endndx,
		       sizeof(long));
	    }
#endif
	    wrlineno (cursegp,&ln);
	    saptr->db_cf.n_numaux = 1;
	}

	if ((v.v_val == 0) && (saptr->db_cf.n_sclass == C_STAT))
	{
	   saptr->db_cf.n_numaux = 1;      /* section entry */
	}
	break;

case PS_TAG:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	deblank();
	if (!get_rname (name,e_notag)) return;
	auxsym.xx_tagndx = dbfindtag (name);

#ifdef SWAP_COFF
	if (target_order != host_order)
	{
	    putswap (auxsym.xx_tagndx,
		(char *) &auxsym.xx_tagndx,
		    sizeof(long));
	}
#endif
	saptr->db_cf.n_numaux = 1;
	break;

case PS_LINE:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	getval (&v);
	saptr->db_cf.n_numaux = 1;
	auxsym.xx_lnno = (unsigned short) v.v_val;

#ifdef SWAP_COFF
	if (target_order != host_order)
	{
	    putswap ((long) auxsym.xx_lnno,
		(char *) &auxsym.xx_lnno,
		    sizeof(short));
	}
#endif
	break;

case PS_SIZE:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	getval (&v);

	if (((saptr->db_cf.n_type & D1MASK) >> 4) == DT_FCN)
	{
	    auxsym.xx_fsize = v.v_val;

#ifdef SWAP_COFF
	    if (target_order != host_order)
	    {
		putswap (auxsym.xx_fsize,
		    (char *) &auxsym.xx_fsize,
			sizeof(long));
	    }
#endif
	}
	else
	{
	    auxsym.xx_size = (unsigned short) v.v_val;

#ifdef SWAP_COFF
	    if (target_order != host_order)
	    {
		putswap ((long) auxsym.xx_size,
		    (char *) &auxsym.xx_size,
			sizeof(short));
	    }
#endif
	}
	saptr->db_cf.n_numaux = 1;
	break;

case PS_DIM:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	i = 0;

	do {
		if (i >= DIMNUM) {
			error (e_too_op);
			break;
		}

		getval (&v);
		auxsym.xx_dimen[i] = (unsigned short) v.v_val;

#ifdef SWAP_COFF
		if (target_order != host_order) {
			putswap ((long) auxsym.xx_dimen[i],
				 (char *) &auxsym.xx_dimen[i],
				 sizeof(short));
		}
#endif
		i++;
	} while (comma(0));

	saptr->db_cf.n_numaux = 1;
	break;

case PS_VAL:
	if (saptr == NULL)
	{
	    error (e_symatt);
	    return;
	}
	getval (&v);
	saptr->db_cf.n_value = v.v_val;

	if (v.v_seg || v.v_reloc || v.v_ext)
	{
	    saptr->db_cf.n_scnum = v.v_seg;
	}
	break;

/* PS_DEF:                           */
/* symbol and aux symbol entries are */
/* filled in with directives until a */
/* .endef is seen.  At that time the */
/* entries are  saved in a  table if */
/* during pass 1                     */

case PS_DEF:
	if (saptr != NULL)
	{
	    error (e_symatt);
	    break;
	}
	if (!get_rname (name,e_operand)) break;
	zap (&dbugsym);
	zap (&auxsym);

	saptr = &dbugsym;
	strcpy (saptr->db_name,name);
	saptr->db_cf.n_scnum = (-3); /* not yet filled-in flag */
	if (*lptr != SEMIC) break;

	do
	{
/* special temporary option ; list all on 1 line */
	    (void)skip_char(SEMIC,REQUIRED);
	    (void)get_rname (name,e_symatt);

	    for (k = 0; k < MAXSA; k++)
	    {
		if (!strcmp (name,sanames[k])) break;
	    }

	    switch (k)
	    {
	case SA_ENDEF:
		idbugsym();
		saptr = NULL;
		break;

	case SA_SCL:
		v.v_val = getint();
		saptr->db_cf.n_sclass = (char) v.v_val;

/* the scnum field depends on storage class */

		if ((v.v_val < 0) || (saptr->db_cf.n_scnum != -3))
		    v.v_val = (-3);
		else
		{
		    i = (int) v.v_val;
		    j = (i < C_BLOCK) ? i : i-80;

		    if ((j < 0) || (j > NSECTN-1))
		    {
			error (e_range);
			j = 0;
		    }
		    v.v_val = sectnum[j];
		}

		if (v.v_val != -3)
		    saptr->db_cf.n_scnum = v.v_val;
		break;   /* >>><<< may have to do more checks here */

	case SA_TYPE:
		v.v_val = getint();
		saptr->db_cf.n_type = (unsigned short) v.v_val;

		if (((v.v_val & 0x030) == (DT_FCN << 4)) &&
		    ((saptr->db_cf.n_sclass == C_EXT) ||
			(saptr->db_cf.n_sclass == C_STAT)))
		{
		    ln.l_lnno = 0;
		    ln.ll_symndx = ndbsyms;
		    auxsym.xx_lnnoptr = cursegp->sg_head.s_lnnoptr +
			cursegp->sg_head.s_nlnno * LINESZ;
		    auxsym.xx_endndx = ndbsyms+2;

#ifdef SWAP_COFF
		    if (target_order != host_order)
		    {
			putswap (auxsym.xx_lnnoptr,
			    (char *) &auxsym.xx_lnnoptr,
				sizeof(long));
			putswap (auxsym.xx_endndx,
			    (char *) &auxsym.xx_endndx,
				sizeof(long));
		    }
#endif
		    wrlineno (cursegp,&ln);
		    saptr->db_cf.n_numaux = 1;
		}

		if ((v.v_val == 0) &&
		    (saptr->db_cf.n_sclass == C_STAT))
		{
		    saptr->db_cf.n_numaux = 1;      /* section entry */
		}
		break;

	case SA_TAG:
		deblank();
		if (!get_rname (name,e_operand)) break;
		auxsym.xx_tagndx = dbfindtag (name);
#ifdef SWAP_COFF
		if (target_order != host_order)
		{
		    putswap (auxsym.xx_tagndx,
			(char *) &auxsym.xx_tagndx,
			    sizeof(long));
		}
#endif
		saptr->db_cf.n_numaux = 1;
		break;

	case SA_LINE:
		getval (&v);
		saptr->db_cf.n_numaux = 1;
		auxsym.xx_lnno = (unsigned short) v.v_val;
#ifdef SWAP_COFF
		if (target_order != host_order)
		{
		    putswap ((long) auxsym.xx_lnno,
			(char *) &auxsym.xx_lnno,
			    sizeof(short));
		}
#endif
		break;

	case SA_SIZE:
		getval (&v);
		if (((saptr->db_cf.n_type & D1MASK) >> 4) == DT_FCN)
		{
		    auxsym.xx_fsize = v.v_val;
#ifdef SWAP_COFF
		    if (target_order != host_order)
		    {
			putswap (auxsym.xx_fsize,
			    (char *) &auxsym.xx_fsize,
				sizeof(long));
		    }
#endif
		}
		else
		{
		    auxsym.xx_size = (unsigned short) v.v_val;
#ifdef SWAP_COFF
		    if (target_order != host_order)
		    {
			putswap ((long) auxsym.xx_size,
			    (char *) &auxsym.xx_size,
				sizeof(short));
		    }
#endif
		}
		saptr->db_cf.n_numaux = 1;
		break;

	case SA_DIM:
		i = 0;
		do {
			getval (&v);
			auxsym.xx_dimen[i] = (unsigned short) v.v_val;
#ifdef SWAP_COFF
			if (target_order != host_order) {
				putswap ((long) auxsym.xx_dimen[i],
					 (char *) &auxsym.xx_dimen[i],
					 sizeof(short));
			}
#endif
			i++;
		} while (comma(0));

		saptr->db_cf.n_numaux = 1;
		break;

	case SA_VAL:
		getval (&v);
		saptr->db_cf.n_value = v.v_val;

		if (v.v_seg || v.v_reloc || v.v_ext)
		{
		    saptr->db_cf.n_scnum = v.v_seg;
		}
		break;

	default:
		error (e_symatt);
		while (*lptr && !isspace (*lptr++)) {}
		break;
	    }
	}
	    while (*lptr && (saptr != NULL));

	if (saptr != NULL) fatal (e_symatt);
	break;

case PS_LN:
       getval (&v);
       ln.l_lnno = (unsigned short) bcheck (v.v_val,0L,0x0FFFFL);
       ln.ll_paddr = lpc;
       wrlineno (cursegp,&ln);
       break;

/* ------------------------------------------------------------ */

case PS_FILE:
	if (*lptr++ != DQ)
	{
	    error (e_text);
	    break;
	}

/* ------------------------------------------------------------ */

#ifdef OLD
#ifdef SKIP_PATH
	cp = lptr;

	while ((c = strchar (DQ | NO_BSL)) != -1)
	{
	    if (dir_sep (c)) cp = lptr;
	}
	lptr = cp;
#endif
	name[0] = EOS;
	if (namech (*lptr)) get_name (name);

	if (name[0] == EOS)
	{
	    error (e_operand);
	    break;
	}
#endif

/* ------------------------------------------------------------ */

	i = j = 0;

	while ((c = strchar (DQ)) != -1)
	{
	    if (i < FILNMLEN)
	    {
		name[i++] = c;
	    }
	    else
	    {                   /* already have FILNMLEN chars  */
		if (j) continue;
		j = TRUE;
		error (w_fsize);
	    }
	}

	if (i == 0)
	{
	    error (e_operand);
	    break;
	}

	name[i] = EOS;

/* ------------------------------------------------------------ */

	zap (&dbugsym);
	strcpy (dbugsym.db_name,".file");

	dbugsym.db_cf.n_sclass = C_FILE;
	dbugsym.db_cf.n_scnum  = N_DEBUG;
	dbugsym.db_cf.n_type   = T_NULL;
	dbugsym.db_cf.n_value  = dbfiles[ndbfils+1];
	dbugsym.db_cf.n_numaux = 1;

	dbfiles[ndbfils] = ndbsyms;
	ndbfils++;
	zap (&auxsym);
#ifdef OLD
	strcpy (auxsym.xx_fname,name);
#else
	(void)strncpy(auxsym.xx_fname,name,FILNMLEN);
#endif
	idbugsym();

	if (*lptr != DQ)
	    error (e_quote);
	else
	    lptr++;
	break;
    }
}

/* ------------------------------------------------------------ */

fixup_undef(ps)
struct sym *ps;
{
#ifdef NOTDEF
	struct symlist *sl;
#endif

	if (ps != NULL) {
		fixup_undef (ps->sy_llink);

		if (ps->sy_flags & F_UNDEF) {
			ps->sy_flags |= F_EXTERN;
			ps->se.n_sclass = C_EXT;
#ifdef NOTDEF
			sl = extlist;
			if (!sl) {
				sl = (struct symlist*)e_alloc(1,sizeof(*sl));
				extlist = sl;
			} else {
				while (sl->sl_link != NULL)
					sl = sl->sl_link;
				sl = (sl->sl_link =
				      (struct symlist*)e_alloc(1,
							       sizeof(*sl)));
			}
			sl->sl_sym = ps;
#endif
		}

		fixup_undef (ps->sy_rlink);
	}
}
