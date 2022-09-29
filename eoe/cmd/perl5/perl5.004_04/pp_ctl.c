/*    pp_ctl.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * Now far ahead the Road has gone,
 * And I must follow, if I can,
 * Pursuing it with eager feet,
 * Until it joins some larger way
 * Where many paths and errands meet.
 * And whither then?  I cannot say.
 */

#include "EXTERN.h"
#include "perl.h"

#ifndef WORD_ALIGN
#define WORD_ALIGN sizeof(U16)
#endif

#define DOCATCH(o) ((CATCH_GET == TRUE) ? docatch(o) : (o))

static OP *docatch _((OP *o));
static OP *doeval _((int gimme));
static OP *dofindlabel _((OP *op, char *label, OP **opstack, OP **oplimit));
static void doparseform _((SV *sv));
static I32 dopoptoeval _((I32 startingblock));
static I32 dopoptolabel _((char *label));
static I32 dopoptoloop _((I32 startingblock));
static I32 dopoptosub _((I32 startingblock));
static void save_lines _((AV *array, SV *sv));
static int sortcv _((const void *, const void *));
static int sortcmp _((const void *, const void *));
static int sortcmp_locale _((const void *, const void *));

static I32 sortcxix;

PP(pp_wantarray)
{
    dSP;
    I32 cxix;
    EXTEND(SP, 1);

    cxix = dopoptosub(cxstack_ix);
    if (cxix < 0)
	RETPUSHUNDEF;

    switch (cxstack[cxix].blk_gimme) {
    case G_ARRAY:
	RETPUSHYES;
    case G_SCALAR:
	RETPUSHNO;
    default:
	RETPUSHUNDEF;
    }
}

PP(pp_regcmaybe)
{
    return NORMAL;
}

PP(pp_regcomp) {
    dSP;
    register PMOP *pm = (PMOP*)cLOGOP->op_other;
    register char *t;
    SV *tmpstr;
    STRLEN len;

    tmpstr = POPs;
    t = SvPV(tmpstr, len);

    /* JMR: Check against the last compiled regexp */
    if ( ! pm->op_pmregexp  || ! pm->op_pmregexp->precomp
	|| strnNE(pm->op_pmregexp->precomp, t, len) 
	|| pm->op_pmregexp->precomp[len]) {
	if (pm->op_pmregexp) {
	    pregfree(pm->op_pmregexp);
	    pm->op_pmregexp = Null(REGEXP*);	/* crucial if regcomp aborts */
	}

	pm->op_pmflags = pm->op_pmpermflags;	/* reset case sensitivity */
	pm->op_pmregexp = pregcomp(t, t + len, pm);
    }

    if (!pm->op_pmregexp->prelen && curpm)
	pm = curpm;
    else if (strEQ("\\s+", pm->op_pmregexp->precomp))
	pm->op_pmflags |= PMf_WHITE;

    if (pm->op_pmflags & PMf_KEEP) {
	pm->op_private &= ~OPpRUNTIME;	/* no point compiling again */
	hoistmust(pm);
	cLOGOP->op_first->op_next = op->op_next;
    }
    RETURN;
}

PP(pp_substcont)
{
    dSP;
    register PMOP *pm = (PMOP*) cLOGOP->op_other;
    register CONTEXT *cx = &cxstack[cxstack_ix];
    register SV *dstr = cx->sb_dstr;
    register char *s = cx->sb_s;
    register char *m = cx->sb_m;
    char *orig = cx->sb_orig;
    register REGEXP *rx = cx->sb_rx;

    rxres_restore(&cx->sb_rxres, rx);

    if (cx->sb_iters++) {
	if (cx->sb_iters > cx->sb_maxiters)
	    DIE("Substitution loop");

	if (!cx->sb_rxtainted)
	    cx->sb_rxtainted = SvTAINTED(TOPs);
	sv_catsv(dstr, POPs);

	/* Are we done */
	if (cx->sb_once || !pregexec(rx, s, cx->sb_strend, orig,
				s == m, Nullsv, cx->sb_safebase))
	{
	    SV *targ = cx->sb_targ;
	    sv_catpvn(dstr, s, cx->sb_strend - s);

	    TAINT_IF(cx->sb_rxtainted || rx->exec_tainted);

	    (void)SvOOK_off(targ);
	    Safefree(SvPVX(targ));
	    SvPVX(targ) = SvPVX(dstr);
	    SvCUR_set(targ, SvCUR(dstr));
	    SvLEN_set(targ, SvLEN(dstr));
	    SvPVX(dstr) = 0;
	    sv_free(dstr);
	    (void)SvPOK_only(targ);
	    SvSETMAGIC(targ);
	    SvTAINT(targ);

	    PUSHs(sv_2mortal(newSViv((I32)cx->sb_iters - 1)));
	    LEAVE_SCOPE(cx->sb_oldsave);
	    POPSUBST(cx);
	    RETURNOP(pm->op_next);
	}
    }
    if (rx->subbase && rx->subbase != orig) {
	m = s;
	s = orig;
	cx->sb_orig = orig = rx->subbase;
	s = orig + (m - s);
	cx->sb_strend = s + (cx->sb_strend - m);
    }
    cx->sb_m = m = rx->startp[0];
    sv_catpvn(dstr, s, m-s);
    cx->sb_s = rx->endp[0];
    cx->sb_rxtainted |= rx->exec_tainted;
    rxres_save(&cx->sb_rxres, rx);
    RETURNOP(pm->op_pmreplstart);
}

void
rxres_save(rsp, rx)
void **rsp;
REGEXP *rx;
{
    UV *p = (UV*)*rsp;
    U32 i;

    if (!p || p[1] < rx->nparens) {
	i = 6 + rx->nparens * 2;
	if (!p)
	    New(501, p, i, UV);
	else
	    Renew(p, i, UV);
	*rsp = (void*)p;
    }

    *p++ = (UV)rx->subbase;
    rx->subbase = Nullch;

    *p++ = rx->nparens;

    *p++ = (UV)rx->subbeg;
    *p++ = (UV)rx->subend;
    for (i = 0; i <= rx->nparens; ++i) {
	*p++ = (UV)rx->startp[i];
	*p++ = (UV)rx->endp[i];
    }
}

void
rxres_restore(rsp, rx)
void **rsp;
REGEXP *rx;
{
    UV *p = (UV*)*rsp;
    U32 i;

    Safefree(rx->subbase);
    rx->subbase = (char*)(*p);
    *p++ = 0;

    rx->nparens = *p++;

    rx->subbeg = (char*)(*p++);
    rx->subend = (char*)(*p++);
    for (i = 0; i <= rx->nparens; ++i) {
	rx->startp[i] = (char*)(*p++);
	rx->endp[i] = (char*)(*p++);
    }
}

void
rxres_free(rsp)
void **rsp;
{
    UV *p = (UV*)*rsp;

    if (p) {
	Safefree((char*)(*p));
	Safefree(p);
	*rsp = Null(void*);
    }
}

PP(pp_formline)
{
    dSP; dMARK; dORIGMARK;
    register SV *form = *++MARK;
    register U16 *fpc;
    register char *t;
    register char *f;
    register char *s;
    register char *send;
    register I32 arg;
    register SV *sv;
    char *item;
    I32 itemsize;
    I32 fieldsize;
    I32 lines = 0;
    bool chopspace = (strchr(chopset, ' ') != Nullch);
    char *chophere;
    char *linemark;
    double value;
    bool gotsome;
    STRLEN len;

    if (!SvMAGICAL(form) || !SvCOMPILED(form)) {
	SvREADONLY_off(form);
	doparseform(form);
    }

    SvPV_force(formtarget, len);
    t = SvGROW(formtarget, len + SvCUR(form) + 1);  /* XXX SvCUR bad */
    t += len;
    f = SvPV(form, len);
    /* need to jump to the next word */
    s = f + len + WORD_ALIGN - SvCUR(form) % WORD_ALIGN;

    fpc = (U16*)s;

    for (;;) {
	DEBUG_f( {
	    char *name = "???";
	    arg = -1;
	    switch (*fpc) {
	    case FF_LITERAL:	arg = fpc[1]; name = "LITERAL";	break;
	    case FF_BLANK:	arg = fpc[1]; name = "BLANK";	break;
	    case FF_SKIP:	arg = fpc[1]; name = "SKIP";	break;
	    case FF_FETCH:	arg = fpc[1]; name = "FETCH";	break;
	    case FF_DECIMAL:	arg = fpc[1]; name = "DECIMAL";	break;

	    case FF_CHECKNL:	name = "CHECKNL";	break;
	    case FF_CHECKCHOP:	name = "CHECKCHOP";	break;
	    case FF_SPACE:	name = "SPACE";		break;
	    case FF_HALFSPACE:	name = "HALFSPACE";	break;
	    case FF_ITEM:	name = "ITEM";		break;
	    case FF_CHOP:	name = "CHOP";		break;
	    case FF_LINEGLOB:	name = "LINEGLOB";	break;
	    case FF_NEWLINE:	name = "NEWLINE";	break;
	    case FF_MORE:	name = "MORE";		break;
	    case FF_LINEMARK:	name = "LINEMARK";	break;
	    case FF_END:	name = "END";		break;
	    }
	    if (arg >= 0)
		PerlIO_printf(PerlIO_stderr(), "%-16s%ld\n", name, (long) arg);
	    else
		PerlIO_printf(PerlIO_stderr(), "%-16s\n", name);
	} )
	switch (*fpc++) {
	case FF_LINEMARK:
	    linemark = t;
	    lines++;
	    gotsome = FALSE;
	    break;

	case FF_LITERAL:
	    arg = *fpc++;
	    while (arg--)
		*t++ = *f++;
	    break;

	case FF_SKIP:
	    f += *fpc++;
	    break;

	case FF_FETCH:
	    arg = *fpc++;
	    f += arg;
	    fieldsize = arg;

	    if (MARK < SP)
		sv = *++MARK;
	    else {
		sv = &sv_no;
		if (dowarn)
		    warn("Not enough format arguments");
	    }
	    break;

	case FF_CHECKNL:
	    item = s = SvPV(sv, len);
	    itemsize = len;
	    if (itemsize > fieldsize)
		itemsize = fieldsize;
	    send = chophere = s + itemsize;
	    while (s < send) {
		if (*s & ~31)
		    gotsome = TRUE;
		else if (*s == '\n')
		    break;
		s++;
	    }
	    itemsize = s - item;
	    break;

	case FF_CHECKCHOP:
	    item = s = SvPV(sv, len);
	    itemsize = len;
	    if (itemsize <= fieldsize) {
		send = chophere = s + itemsize;
		while (s < send) {
		    if (*s == '\r') {
			itemsize = s - item;
			break;
		    }
		    if (*s++ & ~31)
			gotsome = TRUE;
		}
	    }
	    else {
		itemsize = fieldsize;
		send = chophere = s + itemsize;
		while (s < send || (s == send && isSPACE(*s))) {
		    if (isSPACE(*s)) {
			if (chopspace)
			    chophere = s;
			if (*s == '\r')
			    break;
		    }
		    else {
			if (*s & ~31)
			    gotsome = TRUE;
			if (strchr(chopset, *s))
			    chophere = s + 1;
		    }
		    s++;
		}
		itemsize = chophere - item;
	    }
	    break;

	case FF_SPACE:
	    arg = fieldsize - itemsize;
	    if (arg) {
		fieldsize -= arg;
		while (arg-- > 0)
		    *t++ = ' ';
	    }
	    break;

	case FF_HALFSPACE:
	    arg = fieldsize - itemsize;
	    if (arg) {
		arg /= 2;
		fieldsize -= arg;
		while (arg-- > 0)
		    *t++ = ' ';
	    }
	    break;

	case FF_ITEM:
	    arg = itemsize;
	    s = item;
	    while (arg--) {
#if 'z' - 'a' != 25
		int ch = *t++ = *s++;
		if (!iscntrl(ch))
		    t[-1] = ' ';
#else
		if ( !((*t++ = *s++) & ~31) )
		    t[-1] = ' ';
#endif

	    }
	    break;

	case FF_CHOP:
	    s = chophere;
	    if (chopspace) {
		while (*s && isSPACE(*s))
		    s++;
	    }
	    sv_chop(sv,s);
	    break;

	case FF_LINEGLOB:
	    item = s = SvPV(sv, len);
	    itemsize = len;
	    if (itemsize) {
		gotsome = TRUE;
		send = s + itemsize;
		while (s < send) {
		    if (*s++ == '\n') {
			if (s == send)
			    itemsize--;
			else
			    lines++;
		    }
		}
		SvCUR_set(formtarget, t - SvPVX(formtarget));
		sv_catpvn(formtarget, item, itemsize);
		SvGROW(formtarget, SvCUR(formtarget) + SvCUR(form) + 1);
		t = SvPVX(formtarget) + SvCUR(formtarget);
	    }
	    break;

	case FF_DECIMAL:
	    /* If the field is marked with ^ and the value is undefined,
	       blank it out. */
	    arg = *fpc++;
	    if ((arg & 512) && !SvOK(sv)) {
		arg = fieldsize;
		while (arg--)
		    *t++ = ' ';
		break;
	    }
	    gotsome = TRUE;
	    value = SvNV(sv);
	    /* Formats aren't yet marked for locales, so assume "yes". */
	    SET_NUMERIC_LOCAL();
	    if (arg & 256) {
		sprintf(t, "%#*.*f", (int) fieldsize, (int) arg & 255, value);
	    } else {
		sprintf(t, "%*.0f", (int) fieldsize, value);
	    }
	    t += fieldsize;
	    break;

	case FF_NEWLINE:
	    f++;
	    while (t-- > linemark && *t == ' ') ;
	    t++;
	    *t++ = '\n';
	    break;

	case FF_BLANK:
	    arg = *fpc++;
	    if (gotsome) {
		if (arg) {		/* repeat until fields exhausted? */
		    *t = '\0';
		    SvCUR_set(formtarget, t - SvPVX(formtarget));
		    lines += FmLINES(formtarget);
		    if (lines == 200) {
			arg = t - linemark;
			if (strnEQ(linemark, linemark - arg, arg))
			    DIE("Runaway format");
		    }
		    FmLINES(formtarget) = lines;
		    SP = ORIGMARK;
		    RETURNOP(cLISTOP->op_first);
		}
	    }
	    else {
		t = linemark;
		lines--;
	    }
	    break;

	case FF_MORE:
	    if (itemsize) {
		arg = fieldsize - itemsize;
		if (arg) {
		    fieldsize -= arg;
		    while (arg-- > 0)
			*t++ = ' ';
		}
		s = t - 3;
		if (strnEQ(s,"   ",3)) {
		    while (s > SvPVX(formtarget) && isSPACE(s[-1]))
			s--;
		}
		*s++ = '.';
		*s++ = '.';
		*s++ = '.';
	    }
	    break;

	case FF_END:
	    *t = '\0';
	    SvCUR_set(formtarget, t - SvPVX(formtarget));
	    FmLINES(formtarget) += lines;
	    SP = ORIGMARK;
	    RETPUSHYES;
	}
    }
}

PP(pp_grepstart)
{
    dSP;
    SV *src;

    if (stack_base + *markstack_ptr == sp) {
	(void)POPMARK;
	if (GIMME_V == G_SCALAR)
	    XPUSHs(&sv_no);
	RETURNOP(op->op_next->op_next);
    }
    stack_sp = stack_base + *markstack_ptr + 1;
    pp_pushmark();				/* push dst */
    pp_pushmark();				/* push src */
    ENTER;					/* enter outer scope */

    SAVETMPS;
    SAVESPTR(GvSV(defgv));

    ENTER;					/* enter inner scope */
    SAVESPTR(curpm);

    src = stack_base[*markstack_ptr];
    SvTEMP_off(src);
    GvSV(defgv) = src;

    PUTBACK;
    if (op->op_type == OP_MAPSTART)
	pp_pushmark();				/* push top */
    return ((LOGOP*)op->op_next)->op_other;
}

PP(pp_mapstart)
{
    DIE("panic: mapstart");	/* uses grepstart */
}

PP(pp_mapwhile)
{
    dSP;
    I32 diff = (sp - stack_base) - *markstack_ptr;
    I32 count;
    I32 shift;
    SV** src;
    SV** dst; 

    ++markstack_ptr[-1];
    if (diff) {
	if (diff > markstack_ptr[-1] - markstack_ptr[-2]) {
	    shift = diff - (markstack_ptr[-1] - markstack_ptr[-2]);
	    count = (sp - stack_base) - markstack_ptr[-1] + 2;
	    
	    EXTEND(sp,shift);
	    src = sp;
	    dst = (sp += shift);
	    markstack_ptr[-1] += shift;
	    *markstack_ptr += shift;
	    while (--count)
		*dst-- = *src--;
	}
	dst = stack_base + (markstack_ptr[-2] += diff) - 1; 
	++diff;
	while (--diff)
	    *dst-- = SvTEMP(TOPs) ? POPs : sv_mortalcopy(POPs); 
    }
    LEAVE;					/* exit inner scope */

    /* All done yet? */
    if (markstack_ptr[-1] > *markstack_ptr) {
	I32 items;
	I32 gimme = GIMME_V;

	(void)POPMARK;				/* pop top */
	LEAVE;					/* exit outer scope */
	(void)POPMARK;				/* pop src */
	items = --*markstack_ptr - markstack_ptr[-1];
	(void)POPMARK;				/* pop dst */
	SP = stack_base + POPMARK;		/* pop original mark */
	if (gimme == G_SCALAR) {
	    dTARGET;
	    XPUSHi(items);
	}
	else if (gimme == G_ARRAY)
	    SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER;					/* enter inner scope */
	SAVESPTR(curpm);

	src = stack_base[markstack_ptr[-1]];
	SvTEMP_off(src);
	GvSV(defgv) = src;

	RETURNOP(cLOGOP->op_other);
    }
}


PP(pp_sort)
{
    dSP; dMARK; dORIGMARK;
    register SV **up;
    SV **myorigmark = ORIGMARK;
    register I32 max;
    HV *stash;
    GV *gv;
    CV *cv;
    I32 gimme = GIMME;
    OP* nextop = op->op_next;

    if (gimme != G_ARRAY) {
	SP = MARK;
	RETPUSHUNDEF;
    }

    if (op->op_flags & OPf_STACKED) {
	ENTER;
	if (op->op_flags & OPf_SPECIAL) {
	    OP *kid = cLISTOP->op_first->op_sibling;	/* pass pushmark */
	    kid = kUNOP->op_first;			/* pass rv2gv */
	    kid = kUNOP->op_first;			/* pass leave */
	    sortcop = kid->op_next;
	    stash = curcop->cop_stash;
	}
	else {
	    cv = sv_2cv(*++MARK, &stash, &gv, 0);
	    if (!(cv && CvROOT(cv))) {
		if (gv) {
		    SV *tmpstr = sv_newmortal();
		    gv_efullname3(tmpstr, gv, Nullch);
		    if (cv && CvXSUB(cv))
			DIE("Xsub \"%s\" called in sort", SvPVX(tmpstr));
		    DIE("Undefined sort subroutine \"%s\" called",
			SvPVX(tmpstr));
		}
		if (cv) {
		    if (CvXSUB(cv))
			DIE("Xsub called in sort");
		    DIE("Undefined subroutine in sort");
		}
		DIE("Not a CODE reference in sort");
	    }
	    sortcop = CvSTART(cv);
	    SAVESPTR(CvROOT(cv)->op_ppaddr);
	    CvROOT(cv)->op_ppaddr = ppaddr[OP_NULL];

	    SAVESPTR(curpad);
	    curpad = AvARRAY((AV*)AvARRAY(CvPADLIST(cv))[1]);
	}
    }
    else {
	sortcop = Nullop;
	stash = curcop->cop_stash;
    }

    up = myorigmark + 1;
    while (MARK < SP) {	/* This may or may not shift down one here. */
	/*SUPPRESS 560*/
	if (*up = *++MARK) {			/* Weed out nulls. */
	    SvTEMP_off(*up);
	    if (!sortcop && !SvPOK(*up))
		(void)sv_2pv(*up, &na);
	    up++;
	}
    }
    max = --up - myorigmark;
    if (sortcop) {
	if (max > 1) {
	    AV *oldstack;
	    CONTEXT *cx;
	    SV** newsp;
	    bool oldcatch = CATCH_GET;

	    SAVETMPS;
	    SAVESPTR(op);

	    oldstack = curstack;
	    if (!sortstack) {
		sortstack = newAV();
		AvREAL_off(sortstack);
		av_extend(sortstack, 32);
	    }
	    CATCH_SET(TRUE);
	    SWITCHSTACK(curstack, sortstack);
	    if (sortstash != stash) {
		firstgv = gv_fetchpv("a", TRUE, SVt_PV);
		secondgv = gv_fetchpv("b", TRUE, SVt_PV);
		sortstash = stash;
	    }

	    SAVESPTR(GvSV(firstgv));
	    SAVESPTR(GvSV(secondgv));

	    PUSHBLOCK(cx, CXt_NULL, stack_base);
	    if (!(op->op_flags & OPf_SPECIAL)) {
		bool hasargs = FALSE;
		cx->cx_type = CXt_SUB;
		cx->blk_gimme = G_SCALAR;
		PUSHSUB(cx);
		if (!CvDEPTH(cv))
		    (void)SvREFCNT_inc(cv); /* in preparation for POPSUB */
	    }
	    sortcxix = cxstack_ix;

	    qsort((char*)(myorigmark+1), max, sizeof(SV*), sortcv);

	    POPBLOCK(cx,curpm);
	    SWITCHSTACK(sortstack, oldstack);
	    CATCH_SET(oldcatch);
	}
	LEAVE;
    }
    else {
	if (max > 1) {
	    MEXTEND(SP, 20);	/* Can't afford stack realloc on signal. */
	    qsort((char*)(ORIGMARK+1), max, sizeof(SV*),
		  (op->op_private & OPpLOCALE) ? sortcmp_locale : sortcmp);
	}
    }
    stack_sp = ORIGMARK + max;
    return nextop;
}

/* Range stuff. */

PP(pp_range)
{
    if (GIMME == G_ARRAY)
	return cCONDOP->op_true;
    return SvTRUEx(PAD_SV(op->op_targ)) ? cCONDOP->op_false : cCONDOP->op_true;
}

PP(pp_flip)
{
    dSP;

    if (GIMME == G_ARRAY) {
	RETURNOP(((CONDOP*)cUNOP->op_first)->op_false);
    }
    else {
	dTOPss;
	SV *targ = PAD_SV(op->op_targ);

	if ((op->op_private & OPpFLIP_LINENUM)
	  ? last_in_gv && SvIV(sv) == IoLINES(GvIOp(last_in_gv))
	  : SvTRUE(sv) ) {
	    sv_setiv(PAD_SV(cUNOP->op_first->op_targ), 1);
	    if (op->op_flags & OPf_SPECIAL) {
		sv_setiv(targ, 1);
		SETs(targ);
		RETURN;
	    }
	    else {
		sv_setiv(targ, 0);
		sp--;
		RETURNOP(((CONDOP*)cUNOP->op_first)->op_false);
	    }
	}
	sv_setpv(TARG, "");
	SETs(targ);
	RETURN;
    }
}

PP(pp_flop)
{
    dSP;

    if (GIMME == G_ARRAY) {
	dPOPPOPssrl;
	register I32 i;
	register SV *sv;
	I32 max;

	if (SvNIOKp(left) || !SvPOKp(left) ||
	  (looks_like_number(left) && *SvPVX(left) != '0') )
	{
	    i = SvIV(left);
	    max = SvIV(right);
	    if (max >= i) {
		EXTEND_MORTAL(max - i + 1);
		EXTEND(SP, max - i + 1);
	    }
	    while (i <= max) {
		sv = sv_2mortal(newSViv(i++));
		PUSHs(sv);
	    }
	}
	else {
	    SV *final = sv_mortalcopy(right);
	    STRLEN len;
	    char *tmps = SvPV(final, len);

	    sv = sv_mortalcopy(left);
	    while (!SvNIOKp(sv) && SvCUR(sv) <= len &&
		strNE(SvPVX(sv),tmps) ) {
		XPUSHs(sv);
		sv = sv_2mortal(newSVsv(sv));
		sv_inc(sv);
	    }
	    if (strEQ(SvPVX(sv),tmps))
		XPUSHs(sv);
	}
    }
    else {
	dTOPss;
	SV *targ = PAD_SV(cUNOP->op_first->op_targ);
	sv_inc(targ);
	if ((op->op_private & OPpFLIP_LINENUM)
	  ? last_in_gv && SvIV(sv) == IoLINES(GvIOp(last_in_gv))
	  : SvTRUE(sv) ) {
	    sv_setiv(PAD_SV(((UNOP*)cUNOP->op_first)->op_first->op_targ), 0);
	    sv_catpv(targ, "E0");
	}
	SETs(targ);
    }

    RETURN;
}

/* Control. */

static I32
dopoptolabel(label)
char *label;
{
    register I32 i;
    register CONTEXT *cx;

    for (i = cxstack_ix; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	case CXt_SUBST:
	    if (dowarn)
		warn("Exiting substitution via %s", op_name[op->op_type]);
	    break;
	case CXt_SUB:
	    if (dowarn)
		warn("Exiting subroutine via %s", op_name[op->op_type]);
	    break;
	case CXt_EVAL:
	    if (dowarn)
		warn("Exiting eval via %s", op_name[op->op_type]);
	    break;
	case CXt_NULL:
	    if (dowarn)
		warn("Exiting pseudo-block via %s", op_name[op->op_type]);
	    return -1;
	case CXt_LOOP:
	    if (!cx->blk_loop.label ||
	      strNE(label, cx->blk_loop.label) ) {
		DEBUG_l(deb("(Skipping label #%ld %s)\n",
			(long)i, cx->blk_loop.label));
		continue;
	    }
	    DEBUG_l( deb("(Found label #%ld %s)\n", (long)i, label));
	    return i;
	}
    }
    return i;
}

I32
dowantarray()
{
    I32 gimme = block_gimme();
    return (gimme == G_VOID) ? G_SCALAR : gimme;
}

I32
block_gimme()
{
    I32 cxix;

    cxix = dopoptosub(cxstack_ix);
    if (cxix < 0)
	return G_VOID;

    switch (cxstack[cxix].blk_gimme) {
    case G_VOID:
	return G_VOID;
    case G_SCALAR:
	return G_SCALAR;
    case G_ARRAY:
	return G_ARRAY;
    default:
	croak("panic: bad gimme: %d\n", cxstack[cxix].blk_gimme);
    }
}

static I32
dopoptosub(startingblock)
I32 startingblock;
{
    I32 i;
    register CONTEXT *cx;
    for (i = startingblock; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	default:
	    continue;
	case CXt_EVAL:
	case CXt_SUB:
	    DEBUG_l( deb("(Found sub #%ld)\n", (long)i));
	    return i;
	}
    }
    return i;
}

static I32
dopoptoeval(startingblock)
I32 startingblock;
{
    I32 i;
    register CONTEXT *cx;
    for (i = startingblock; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	default:
	    continue;
	case CXt_EVAL:
	    DEBUG_l( deb("(Found eval #%ld)\n", (long)i));
	    return i;
	}
    }
    return i;
}

static I32
dopoptoloop(startingblock)
I32 startingblock;
{
    I32 i;
    register CONTEXT *cx;
    for (i = startingblock; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	case CXt_SUBST:
	    if (dowarn)
		warn("Exiting substitution via %s", op_name[op->op_type]);
	    break;
	case CXt_SUB:
	    if (dowarn)
		warn("Exiting subroutine via %s", op_name[op->op_type]);
	    break;
	case CXt_EVAL:
	    if (dowarn)
		warn("Exiting eval via %s", op_name[op->op_type]);
	    break;
	case CXt_NULL:
	    if (dowarn)
		warn("Exiting pseudo-block via %s", op_name[op->op_type]);
	    return -1;
	case CXt_LOOP:
	    DEBUG_l( deb("(Found loop #%ld)\n", (long)i));
	    return i;
	}
    }
    return i;
}

void
dounwind(cxix)
I32 cxix;
{
    register CONTEXT *cx;
    SV **newsp;
    I32 optype;

    while (cxstack_ix > cxix) {
	cx = &cxstack[cxstack_ix];
	DEBUG_l(PerlIO_printf(Perl_debug_log, "Unwinding block %ld, type %s\n",
			      (long) cxstack_ix+1, block_type[cx->cx_type]));
	/* Note: we don't need to restore the base context info till the end. */
	switch (cx->cx_type) {
	case CXt_SUBST:
	    POPSUBST(cx);
	    continue;  /* not break */
	case CXt_SUB:
	    POPSUB(cx);
	    break;
	case CXt_EVAL:
	    POPEVAL(cx);
	    break;
	case CXt_LOOP:
	    POPLOOP(cx);
	    break;
	case CXt_NULL:
	    break;
	}
	cxstack_ix--;
    }
}

OP *
die_where(message)
char *message;
{
    if (in_eval) {
	I32 cxix;
	register CONTEXT *cx;
	I32 gimme;
	SV **newsp;

	if (in_eval & 4) {
	    SV **svp;
	    STRLEN klen = strlen(message);
	    
	    svp = hv_fetch(GvHV(errgv), message, klen, TRUE);
	    if (svp) {
		if (!SvIOK(*svp)) {
		    static char prefix[] = "\t(in cleanup) ";
		    sv_upgrade(*svp, SVt_IV);
		    (void)SvIOK_only(*svp);
		    SvGROW(GvSV(errgv), SvCUR(GvSV(errgv))+sizeof(prefix)+klen);
		    sv_catpvn(GvSV(errgv), prefix, sizeof(prefix)-1);
		    sv_catpvn(GvSV(errgv), message, klen);
		}
		sv_inc(*svp);
	    }
	}
	else
	    sv_setpv(GvSV(errgv), message);
	
	cxix = dopoptoeval(cxstack_ix);
	if (cxix >= 0) {
	    I32 optype;

	    if (cxix < cxstack_ix)
		dounwind(cxix);

	    POPBLOCK(cx,curpm);
	    if (cx->cx_type != CXt_EVAL) {
		PerlIO_printf(PerlIO_stderr(), "panic: die %s", message);
		my_exit(1);
	    }
	    POPEVAL(cx);

	    if (gimme == G_SCALAR)
		*++newsp = &sv_undef;
	    stack_sp = newsp;

	    LEAVE;

	    if (optype == OP_REQUIRE) {
		char* msg = SvPVx(GvSV(errgv), na);
		DIE("%s", *msg ? msg : "Compilation failed in require");
	    }
	    return pop_return();
	}
    }
    PerlIO_printf(PerlIO_stderr(), "%s",message);
    PerlIO_flush(PerlIO_stderr());
    my_failure_exit();
    /* NOTREACHED */
    return 0;
}

PP(pp_xor)
{
    dSP; dPOPTOPssrl;
    if (SvTRUE(left) != SvTRUE(right))
	RETSETYES;
    else
	RETSETNO;
}

PP(pp_andassign)
{
    dSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else
	RETURNOP(cLOGOP->op_other);
}

PP(pp_orassign)
{
    dSP;
    if (SvTRUE(TOPs))
	RETURN;
    else
	RETURNOP(cLOGOP->op_other);
}
	
#ifdef DEPRECATED
PP(pp_entersubr)
{
    dSP;
    SV** mark = (stack_base + *markstack_ptr + 1);
    SV* cv = *mark;
    while (mark < sp) {	/* emulate old interface */
	*mark = mark[1];
	mark++;
    }
    *sp = cv;
    return pp_entersub();
}
#endif

PP(pp_caller)
{
    dSP;
    register I32 cxix = dopoptosub(cxstack_ix);
    register CONTEXT *cx;
    I32 dbcxix;
    I32 gimme;
    SV *sv;
    I32 count = 0;

    if (MAXARG)
	count = POPi;
    EXTEND(SP, 6);
    for (;;) {
	if (cxix < 0) {
	    if (GIMME != G_ARRAY)
		RETPUSHUNDEF;
	    RETURN;
	}
	if (DBsub && cxix >= 0 &&
		cxstack[cxix].blk_sub.cv == GvCV(DBsub))
	    count++;
	if (!count--)
	    break;
	cxix = dopoptosub(cxix - 1);
    }
    cx = &cxstack[cxix];
    if (cxstack[cxix].cx_type == CXt_SUB) {
        dbcxix = dopoptosub(cxix - 1);
	/* We expect that cxstack[dbcxix] is CXt_SUB, anyway, the
	   field below is defined for any cx. */
	if (DBsub && dbcxix >= 0 && cxstack[dbcxix].blk_sub.cv == GvCV(DBsub))
	    cx = &cxstack[dbcxix];
    }

    if (GIMME != G_ARRAY) {
	dTARGET;

	sv_setpv(TARG, HvNAME(cx->blk_oldcop->cop_stash));
	PUSHs(TARG);
	RETURN;
    }

    PUSHs(sv_2mortal(newSVpv(HvNAME(cx->blk_oldcop->cop_stash), 0)));
    PUSHs(sv_2mortal(newSVpv(SvPVX(GvSV(cx->blk_oldcop->cop_filegv)), 0)));
    PUSHs(sv_2mortal(newSViv((I32)cx->blk_oldcop->cop_line)));
    if (!MAXARG)
	RETURN;
    if (cx->cx_type == CXt_SUB) { /* So is cxstack[dbcxix]. */
	sv = NEWSV(49, 0);
	gv_efullname3(sv, CvGV(cxstack[cxix].blk_sub.cv), Nullch);
	PUSHs(sv_2mortal(sv));
	PUSHs(sv_2mortal(newSViv((I32)cx->blk_sub.hasargs)));
    }
    else {
	PUSHs(sv_2mortal(newSVpv("(eval)",0)));
	PUSHs(sv_2mortal(newSViv(0)));
    }
    gimme = (I32)cx->blk_gimme;
    if (gimme == G_VOID)
	PUSHs(&sv_undef);
    else
	PUSHs(sv_2mortal(newSViv(gimme & G_ARRAY)));
    if (cx->cx_type == CXt_EVAL) {
	if (cx->blk_eval.old_op_type == OP_ENTEREVAL) {
	    PUSHs(cx->blk_eval.cur_text);
	    PUSHs(&sv_no);
	} 
	else if (cx->blk_eval.old_name) { /* Try blocks have old_name == 0. */
	    /* Require, put the name. */
	    PUSHs(sv_2mortal(newSVpv(cx->blk_eval.old_name, 0)));
	    PUSHs(&sv_yes);
	}
    }
    else if (cx->cx_type == CXt_SUB &&
	    cx->blk_sub.hasargs &&
	    curcop->cop_stash == debstash)
    {
	AV *ary = cx->blk_sub.argarray;
	int off = AvARRAY(ary) - AvALLOC(ary);

	if (!dbargs) {
	    GV* tmpgv;
	    dbargs = GvAV(gv_AVadd(tmpgv = gv_fetchpv("DB::args", TRUE,
				SVt_PVAV)));
	    GvMULTI_on(tmpgv);
	    AvREAL_off(dbargs);		/* XXX Should be REIFY */
	}

	if (AvMAX(dbargs) < AvFILL(ary) + off)
	    av_extend(dbargs, AvFILL(ary) + off);
	Copy(AvALLOC(ary), AvARRAY(dbargs), AvFILL(ary) + 1 + off, SV*);
	AvFILL(dbargs) = AvFILL(ary) + off;
    }
    RETURN;
}

static int
sortcv(a, b)
const void *a;
const void *b;
{
    SV * const *str1 = (SV * const *)a;
    SV * const *str2 = (SV * const *)b;
    I32 oldsaveix = savestack_ix;
    I32 oldscopeix = scopestack_ix;
    I32 result;
    GvSV(firstgv) = *str1;
    GvSV(secondgv) = *str2;
    stack_sp = stack_base;
    op = sortcop;
    runops();
    if (stack_sp != stack_base + 1)
	croak("Sort subroutine didn't return single value");
    if (!SvNIOKp(*stack_sp))
	croak("Sort subroutine didn't return a numeric value");
    result = SvIV(*stack_sp);
    while (scopestack_ix > oldscopeix) {
	LEAVE;
    }
    leave_scope(oldsaveix);
    return result;
}

static int
sortcmp(a, b)
const void *a;
const void *b;
{
    return sv_cmp(*(SV * const *)a, *(SV * const *)b);
}

static int
sortcmp_locale(a, b)
const void *a;
const void *b;
{
    return sv_cmp_locale(*(SV * const *)a, *(SV * const *)b);
}

PP(pp_reset)
{
    dSP;
    char *tmps;

    if (MAXARG < 1)
	tmps = "";
    else
	tmps = POPp;
    sv_reset(tmps, curcop->cop_stash);
    PUSHs(&sv_yes);
    RETURN;
}

PP(pp_lineseq)
{
    return NORMAL;
}

PP(pp_dbstate)
{
    curcop = (COP*)op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    stack_sp = stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;

    if (op->op_private || SvIV(DBsingle) || SvIV(DBsignal) || SvIV(DBtrace))
    {
	SV **sp;
	register CV *cv;
	register CONTEXT *cx;
	I32 gimme = G_ARRAY;
	I32 hasargs;
	GV *gv;

	gv = DBgv;
	cv = GvCV(gv);
	if (!cv)
	    DIE("No DB::DB routine defined");

	if (CvDEPTH(cv) >= 1 && !(debug & (1<<30))) /* don't do recursive DB::DB call */
	    return NORMAL;

	ENTER;
	SAVETMPS;

	SAVEI32(debug);
	SAVESTACK_POS();
	debug = 0;
	hasargs = 0;
	sp = stack_sp;

	push_return(op->op_next);
	PUSHBLOCK(cx, CXt_SUB, sp);
	PUSHSUB(cx);
	CvDEPTH(cv)++;
	(void)SvREFCNT_inc(cv);
	SAVESPTR(curpad);
	curpad = AvARRAY((AV*)*av_fetch(CvPADLIST(cv),1,FALSE));
	RETURNOP(CvSTART(cv));
    }
    else
	return NORMAL;
}

PP(pp_scope)
{
    return NORMAL;
}

PP(pp_enteriter)
{
    dSP; dMARK;
    register CONTEXT *cx;
    I32 gimme = GIMME_V;
    SV **svp;

    ENTER;
    SAVETMPS;

    if (op->op_targ)
	svp = &curpad[op->op_targ];		/* "my" variable */
    else
	svp = &GvSV((GV*)POPs);			/* symbol table variable */

    SAVESPTR(*svp);

    ENTER;

    PUSHBLOCK(cx, CXt_LOOP, SP);
    PUSHLOOP(cx, svp, MARK);
    if (op->op_flags & OPf_STACKED)
	cx->blk_loop.iterary = (AV*)SvREFCNT_inc(POPs);
    else {
	cx->blk_loop.iterary = curstack;
	AvFILL(curstack) = sp - stack_base;
	cx->blk_loop.iterix = MARK - stack_base;
    }

    RETURN;
}

PP(pp_enterloop)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme = GIMME_V;

    ENTER;
    SAVETMPS;
    ENTER;

    PUSHBLOCK(cx, CXt_LOOP, SP);
    PUSHLOOP(cx, 0, SP);

    RETURN;
}

PP(pp_leaveloop)
{
    dSP;
    register CONTEXT *cx;
    struct block_loop cxloop;
    I32 gimme;
    SV **newsp;
    PMOP *newpm;
    SV **mark;

    POPBLOCK(cx,newpm);
    mark = newsp;
    POPLOOP1(cx);	/* Delay POPLOOP2 until stack values are safe */

    TAINT_NOT;
    if (gimme == G_VOID)
	; /* do nothing */
    else if (gimme == G_SCALAR) {
	if (mark < SP)
	    *++newsp = sv_mortalcopy(*SP);
	else
	    *++newsp = &sv_undef;
    }
    else {
	while (mark < SP) {
	    *++newsp = sv_mortalcopy(*++mark);
	    TAINT_NOT;		/* Each item is independent */
	}
    }
    SP = newsp;
    PUTBACK;

    POPLOOP2();		/* Stack values are safe: release loop vars ... */
    curpm = newpm;	/* ... and pop $1 et al */

    LEAVE;
    LEAVE;

    return NORMAL;
}

PP(pp_return)
{
    dSP; dMARK;
    I32 cxix;
    register CONTEXT *cx;
    struct block_sub cxsub;
    bool popsub2 = FALSE;
    I32 gimme;
    SV **newsp;
    PMOP *newpm;
    I32 optype = 0;

    if (curstack == sortstack) {
	if (cxstack_ix == sortcxix || dopoptosub(cxstack_ix) <= sortcxix) {
	    if (cxstack_ix > sortcxix)
		dounwind(sortcxix);
	    AvARRAY(curstack)[1] = *SP;
	    stack_sp = stack_base + 1;
	    return 0;
	}
    }

    cxix = dopoptosub(cxstack_ix);
    if (cxix < 0)
	DIE("Can't return outside a subroutine");
    if (cxix < cxstack_ix)
	dounwind(cxix);

    POPBLOCK(cx,newpm);
    switch (cx->cx_type) {
    case CXt_SUB:
	POPSUB1(cx);	/* Delay POPSUB2 until stack values are safe */
	popsub2 = TRUE;
	break;
    case CXt_EVAL:
	POPEVAL(cx);
	if (optype == OP_REQUIRE &&
	    (MARK == SP || (gimme == G_SCALAR && !SvTRUE(*SP))) )
	{
	    /* Unassume the success we assumed earlier. */
	    char *name = cx->blk_eval.old_name;
	    (void)hv_delete(GvHVn(incgv), name, strlen(name), G_DISCARD);
	    DIE("%s did not return a true value", name);
	}
	break;
    default:
	DIE("panic: return");
    }

    TAINT_NOT;
    if (gimme == G_SCALAR) {
	if (MARK < SP)
	    *++newsp = (popsub2 && SvTEMP(*SP))
			? *SP : sv_mortalcopy(*SP);
	else
	    *++newsp = &sv_undef;
    }
    else if (gimme == G_ARRAY) {
	while (++MARK <= SP) {
	    *++newsp = (popsub2 && SvTEMP(*MARK))
			? *MARK : sv_mortalcopy(*MARK);
	    TAINT_NOT;		/* Each item is independent */
	}
    }
    stack_sp = newsp;

    /* Stack values are safe: */
    if (popsub2) {
	POPSUB2();	/* release CV and @_ ... */
    }
    curpm = newpm;	/* ... and pop $1 et al */

    LEAVE;
    return pop_return();
}

PP(pp_last)
{
    dSP;
    I32 cxix;
    register CONTEXT *cx;
    struct block_loop cxloop;
    struct block_sub cxsub;
    I32 pop2 = 0;
    I32 gimme;
    I32 optype;
    OP *nextop;
    SV **newsp;
    PMOP *newpm;
    SV **mark = stack_base + cxstack[cxstack_ix].blk_oldsp;

    if (op->op_flags & OPf_SPECIAL) {
	cxix = dopoptoloop(cxstack_ix);
	if (cxix < 0)
	    DIE("Can't \"last\" outside a block");
    }
    else {
	cxix = dopoptolabel(cPVOP->op_pv);
	if (cxix < 0)
	    DIE("Label not found for \"last %s\"", cPVOP->op_pv);
    }
    if (cxix < cxstack_ix)
	dounwind(cxix);

    POPBLOCK(cx,newpm);
    switch (cx->cx_type) {
    case CXt_LOOP:
	POPLOOP1(cx);	/* Delay POPLOOP2 until stack values are safe */
	pop2 = CXt_LOOP;
	nextop = cxloop.last_op->op_next;
	break;
    case CXt_SUB:
	POPSUB1(cx);	/* Delay POPSUB2 until stack values are safe */
	pop2 = CXt_SUB;
	nextop = pop_return();
	break;
    case CXt_EVAL:
	POPEVAL(cx);
	nextop = pop_return();
	break;
    default:
	DIE("panic: last");
    }

    TAINT_NOT;
    if (gimme == G_SCALAR) {
	if (MARK < SP)
	    *++newsp = ((pop2 == CXt_SUB) && SvTEMP(*SP))
			? *SP : sv_mortalcopy(*SP);
	else
	    *++newsp = &sv_undef;
    }
    else if (gimme == G_ARRAY) {
	while (++MARK <= SP) {
	    *++newsp = ((pop2 == CXt_SUB) && SvTEMP(*MARK))
			? *MARK : sv_mortalcopy(*MARK);
	    TAINT_NOT;		/* Each item is independent */
	}
    }
    SP = newsp;
    PUTBACK;

    /* Stack values are safe: */
    switch (pop2) {
    case CXt_LOOP:
	POPLOOP2();	/* release loop vars ... */
	LEAVE;
	break;
    case CXt_SUB:
	POPSUB2();	/* release CV and @_ ... */
	break;
    }
    curpm = newpm;	/* ... and pop $1 et al */

    LEAVE;
    return nextop;
}

PP(pp_next)
{
    I32 cxix;
    register CONTEXT *cx;
    I32 oldsave;

    if (op->op_flags & OPf_SPECIAL) {
	cxix = dopoptoloop(cxstack_ix);
	if (cxix < 0)
	    DIE("Can't \"next\" outside a block");
    }
    else {
	cxix = dopoptolabel(cPVOP->op_pv);
	if (cxix < 0)
	    DIE("Label not found for \"next %s\"", cPVOP->op_pv);
    }
    if (cxix < cxstack_ix)
	dounwind(cxix);

    TOPBLOCK(cx);
    oldsave = scopestack[scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return cx->blk_loop.next_op;
}

PP(pp_redo)
{
    I32 cxix;
    register CONTEXT *cx;
    I32 oldsave;

    if (op->op_flags & OPf_SPECIAL) {
	cxix = dopoptoloop(cxstack_ix);
	if (cxix < 0)
	    DIE("Can't \"redo\" outside a block");
    }
    else {
	cxix = dopoptolabel(cPVOP->op_pv);
	if (cxix < 0)
	    DIE("Label not found for \"redo %s\"", cPVOP->op_pv);
    }
    if (cxix < cxstack_ix)
	dounwind(cxix);

    TOPBLOCK(cx);
    oldsave = scopestack[scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return cx->blk_loop.redo_op;
}

static OP* lastgotoprobe;

static OP *
dofindlabel(op,label,opstack,oplimit)
OP *op;
char *label;
OP **opstack;
OP **oplimit;
{
    OP *kid;
    OP **ops = opstack;
    static char too_deep[] = "Target of goto is too deeply nested";

    if (ops >= oplimit)
	croak(too_deep);
    if (op->op_type == OP_LEAVE ||
	op->op_type == OP_SCOPE ||
	op->op_type == OP_LEAVELOOP ||
	op->op_type == OP_LEAVETRY)
    {
	*ops++ = cUNOP->op_first;
	if (ops >= oplimit)
	    croak(too_deep);
    }
    *ops = 0;
    if (op->op_flags & OPf_KIDS) {
	/* First try all the kids at this level, since that's likeliest. */
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling) {
	    if ((kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE) &&
		    kCOP->cop_label && strEQ(kCOP->cop_label, label))
		return kid;
	}
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling) {
	    if (kid == lastgotoprobe)
		continue;
	    if ((kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE) &&
		(ops == opstack ||
		 (ops[-1]->op_type != OP_NEXTSTATE &&
		  ops[-1]->op_type != OP_DBSTATE)))
		*ops++ = kid;
	    if (op = dofindlabel(kid, label, ops, oplimit))
		return op;
	}
    }
    *ops = 0;
    return 0;
}

PP(pp_dump)
{
    return pp_goto(ARGS);
    /*NOTREACHED*/
}

PP(pp_goto)
{
    dSP;
    OP *retop = 0;
    I32 ix;
    register CONTEXT *cx;
#define GOTO_DEPTH 64
    OP *enterops[GOTO_DEPTH];
    char *label;
    int do_dump = (op->op_type == OP_DUMP);

    label = 0;
    if (op->op_flags & OPf_STACKED) {
	SV *sv = POPs;

	/* This egregious kludge implements goto &subroutine */
	if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVCV) {
	    I32 cxix;
	    register CONTEXT *cx;
	    CV* cv = (CV*)SvRV(sv);
	    SV** mark;
	    I32 items = 0;
	    I32 oldsave;

	    if (!CvROOT(cv) && !CvXSUB(cv)) {
		if (CvGV(cv)) {
		    SV *tmpstr = sv_newmortal();
		    gv_efullname3(tmpstr, CvGV(cv), Nullch);
		    DIE("Goto undefined subroutine &%s",SvPVX(tmpstr));
		}
		DIE("Goto undefined subroutine");
	    }

	    /* First do some returnish stuff. */
	    cxix = dopoptosub(cxstack_ix);
	    if (cxix < 0)
		DIE("Can't goto subroutine outside a subroutine");
	    if (cxix < cxstack_ix)
		dounwind(cxix);
	    TOPBLOCK(cx);
	    mark = stack_sp;
	    if (cx->blk_sub.hasargs) {   /* put @_ back onto stack */
		AV* av = cx->blk_sub.argarray;
		
		items = AvFILL(av) + 1;
		stack_sp++;
		EXTEND(stack_sp, items); /* @_ could have been extended. */
		Copy(AvARRAY(av), stack_sp, items, SV*);
		stack_sp += items;
		SvREFCNT_dec(GvAV(defgv));
		GvAV(defgv) = cx->blk_sub.savearray;
		AvREAL_off(av);
		av_clear(av);
	    }
	    if (!(CvDEPTH(cx->blk_sub.cv) = cx->blk_sub.olddepth))
		SvREFCNT_dec(cx->blk_sub.cv);
	    oldsave = scopestack[scopestack_ix - 1];
	    LEAVE_SCOPE(oldsave);

	    /* Now do some callish stuff. */
	    SAVETMPS;
	    if (CvXSUB(cv)) {
		if (CvOLDSTYLE(cv)) {
		    I32 (*fp3)_((int,int,int));
		    while (sp > mark) {
			sp[1] = sp[0];
			sp--;
		    }
		    fp3 = (I32(*)_((int,int,int)))CvXSUB(cv);
		    items = (*fp3)(CvXSUBANY(cv).any_i32,
		                   mark - stack_base + 1,
				   items);
		    sp = stack_base + items;
		}
		else {
		    stack_sp--;		/* There is no cv arg. */
		    (void)(*CvXSUB(cv))(cv);
		}
		LEAVE;
		return pop_return();
	    }
	    else {
		AV* padlist = CvPADLIST(cv);
		SV** svp = AvARRAY(padlist);
		cx->blk_sub.cv = cv;
		cx->blk_sub.olddepth = CvDEPTH(cv);
		CvDEPTH(cv)++;
		if (CvDEPTH(cv) < 2)
		    (void)SvREFCNT_inc(cv);
		else {	/* save temporaries on recursion? */
		    if (CvDEPTH(cv) == 100 && dowarn)
			sub_crush_depth(cv);
		    if (CvDEPTH(cv) > AvFILL(padlist)) {
			AV *newpad = newAV();
			SV **oldpad = AvARRAY(svp[CvDEPTH(cv)-1]);
			I32 ix = AvFILL((AV*)svp[1]);
			svp = AvARRAY(svp[0]);
			for ( ;ix > 0; ix--) {
			    if (svp[ix] != &sv_undef) {
				char *name = SvPVX(svp[ix]);
				if ((SvFLAGS(svp[ix]) & SVf_FAKE)
				    || *name == '&')
				{
				    /* outer lexical or anon code */
				    av_store(newpad, ix,
					SvREFCNT_inc(oldpad[ix]) );
				}
				else {		/* our own lexical */
				    if (*name == '@')
					av_store(newpad, ix, sv = (SV*)newAV());
				    else if (*name == '%')
					av_store(newpad, ix, sv = (SV*)newHV());
				    else
					av_store(newpad, ix, sv = NEWSV(0,0));
				    SvPADMY_on(sv);
				}
			    }
			    else {
				av_store(newpad, ix, sv = NEWSV(0,0));
				SvPADTMP_on(sv);
			    }
			}
			if (cx->blk_sub.hasargs) {
			    AV* av = newAV();
			    av_extend(av, 0);
			    av_store(newpad, 0, (SV*)av);
			    AvFLAGS(av) = AVf_REIFY;
			}
			av_store(padlist, CvDEPTH(cv), (SV*)newpad);
			AvFILL(padlist) = CvDEPTH(cv);
			svp = AvARRAY(padlist);
		    }
		}
		SAVESPTR(curpad);
		curpad = AvARRAY((AV*)svp[CvDEPTH(cv)]);
		if (cx->blk_sub.hasargs) {
		    AV* av = (AV*)curpad[0];
		    SV** ary;

		    cx->blk_sub.savearray = GvAV(defgv);
		    cx->blk_sub.argarray = av;
		    GvAV(defgv) = (AV*)SvREFCNT_inc(av);
		    ++mark;

		    if (items >= AvMAX(av) + 1) {
			ary = AvALLOC(av);
			if (AvARRAY(av) != ary) {
			    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
			    SvPVX(av) = (char*)ary;
			}
			if (items >= AvMAX(av) + 1) {
			    AvMAX(av) = items - 1;
			    Renew(ary,items+1,SV*);
			    AvALLOC(av) = ary;
			    SvPVX(av) = (char*)ary;
			}
		    }
		    Copy(mark,AvARRAY(av),items,SV*);
		    AvFILL(av) = items - 1;
		    
		    while (items--) {
			if (*mark)
			    SvTEMP_off(*mark);
			mark++;
		    }
		}
		if (PERLDB_SUB && curstash != debstash) {
		    /*
		     * We do not care about using sv to call CV;
		     * it's for informational purposes only.
		     */
		    SV *sv = GvSV(DBsub);
		    save_item(sv);
		    gv_efullname3(sv, CvGV(cv), Nullch);
		}
		RETURNOP(CvSTART(cv));
	    }
	}
	else
	    label = SvPV(sv,na);
    }
    else if (op->op_flags & OPf_SPECIAL) {
	if (! do_dump)
	    DIE("goto must have label");
    }
    else
	label = cPVOP->op_pv;

    if (label && *label) {
	OP *gotoprobe = 0;

	/* find label */

	lastgotoprobe = 0;
	*enterops = 0;
	for (ix = cxstack_ix; ix >= 0; ix--) {
	    cx = &cxstack[ix];
	    switch (cx->cx_type) {
	    case CXt_EVAL:
		gotoprobe = eval_root; /* XXX not good for nested eval */
		break;
	    case CXt_LOOP:
		gotoprobe = cx->blk_oldcop->op_sibling;
		break;
	    case CXt_SUBST:
		continue;
	    case CXt_BLOCK:
		if (ix)
		    gotoprobe = cx->blk_oldcop->op_sibling;
		else
		    gotoprobe = main_root;
		break;
	    case CXt_SUB:
		if (CvDEPTH(cx->blk_sub.cv)) {
		    gotoprobe = CvROOT(cx->blk_sub.cv);
		    break;
		}
		/* FALL THROUGH */
	    case CXt_NULL:
		DIE("Can't \"goto\" outside a block");
	    default:
		if (ix)
		    DIE("panic: goto");
		gotoprobe = main_root;
		break;
	    }
	    retop = dofindlabel(gotoprobe, label,
				enterops, enterops + GOTO_DEPTH);
	    if (retop)
		break;
	    lastgotoprobe = gotoprobe;
	}
	if (!retop)
	    DIE("Can't find label %s", label);

	/* pop unwanted frames */

	if (ix < cxstack_ix) {
	    I32 oldsave;

	    if (ix < 0)
		ix = 0;
	    dounwind(ix);
	    TOPBLOCK(cx);
	    oldsave = scopestack[scopestack_ix];
	    LEAVE_SCOPE(oldsave);
	}

	/* push wanted frames */

	if (*enterops && enterops[1]) {
	    OP *oldop = op;
	    for (ix = 1; enterops[ix]; ix++) {
		op = enterops[ix];
		/* Eventually we may want to stack the needed arguments
		 * for each op.  For now, we punt on the hard ones. */
		if (op->op_type == OP_ENTERITER)
		    DIE("Can't \"goto\" into the middle of a foreach loop",
			label);
		(*op->op_ppaddr)();
	    }
	    op = oldop;
	}
    }

    if (do_dump) {
#ifdef VMS
	if (!retop) retop = main_start;
#endif
	restartop = retop;
	do_undump = TRUE;

	my_unexec();

	restartop = 0;		/* hmm, must be GNU unexec().. */
	do_undump = FALSE;
    }

    if (curstack == signalstack) {
        restartop = retop;
        JMPENV_JUMP(3);
    }

    RETURNOP(retop);
}

PP(pp_exit)
{
    dSP;
    I32 anum;

    if (MAXARG < 1)
	anum = 0;
    else {
	anum = SvIVx(POPs);
#ifdef VMSISH_EXIT
	if (anum == 1 && VMSISH_EXIT)
	    anum = 0;
#endif
    }
    my_exit(anum);
    PUSHs(&sv_undef);
    RETURN;
}

#ifdef NOTYET
PP(pp_nswitch)
{
    dSP;
    double value = SvNVx(GvSV(cCOP->cop_gv));
    register I32 match = I_32(value);

    if (value < 0.0) {
	if (((double)match) > value)
	    --match;		/* was fractional--truncate other way */
    }
    match -= cCOP->uop.scop.scop_offset;
    if (match < 0)
	match = 0;
    else if (match > cCOP->uop.scop.scop_max)
	match = cCOP->uop.scop.scop_max;
    op = cCOP->uop.scop.scop_next[match];
    RETURNOP(op);
}

PP(pp_cswitch)
{
    dSP;
    register I32 match;

    if (multiline)
	op = op->op_next;			/* can't assume anything */
    else {
	match = *(SvPVx(GvSV(cCOP->cop_gv), na)) & 255;
	match -= cCOP->uop.scop.scop_offset;
	if (match < 0)
	    match = 0;
	else if (match > cCOP->uop.scop.scop_max)
	    match = cCOP->uop.scop.scop_max;
	op = cCOP->uop.scop.scop_next[match];
    }
    RETURNOP(op);
}
#endif

/* Eval. */

static void
save_lines(array, sv)
AV *array;
SV *sv;
{
    register char *s = SvPVX(sv);
    register char *send = SvPVX(sv) + SvCUR(sv);
    register char *t;
    register I32 line = 1;

    while (s && s < send) {
	SV *tmpstr = NEWSV(85,0);

	sv_upgrade(tmpstr, SVt_PVMG);
	t = strchr(s, '\n');
	if (t)
	    t++;
	else
	    t = send;

	sv_setpvn(tmpstr, s, t - s);
	av_store(array, line++, tmpstr);
	s = t;
    }
}

static OP *
docatch(o)
OP *o;
{
    int ret;
    I32 oldrunlevel = runlevel;
    OP *oldop = op;
    dJMPENV;

    op = o;
#ifdef DEBUGGING
    assert(CATCH_GET == TRUE);
    DEBUG_l(deb("(Setting up local jumplevel, runlevel = %ld)\n", (long)runlevel+1));
#endif
    JMPENV_PUSH(ret);
    switch (ret) {
    default:				/* topmost level handles it */
	JMPENV_POP;
	runlevel = oldrunlevel;
	op = oldop;
	JMPENV_JUMP(ret);
	/* NOTREACHED */
    case 3:
	if (!restartop) {
	    PerlIO_printf(PerlIO_stderr(), "panic: restartop\n");
	    break;
	}
	op = restartop;
	restartop = 0;
	/* FALL THROUGH */
    case 0:
        runops();
	break;
    }
    JMPENV_POP;
    runlevel = oldrunlevel;
    op = oldop;
    return Nullop;
}

static OP *
doeval(gimme)
int gimme;
{
    dSP;
    OP *saveop = op;
    HV *newstash;
    CV *caller;
    AV* comppadlist;

    in_eval = 1;

    PUSHMARK(SP);

    /* set up a scratch pad */

    SAVEI32(padix);
    SAVESPTR(curpad);
    SAVESPTR(comppad);
    SAVESPTR(comppad_name);
    SAVEI32(comppad_name_fill);
    SAVEI32(min_intro_pending);
    SAVEI32(max_intro_pending);

    caller = compcv;
    SAVESPTR(compcv);
    compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)compcv, SVt_PVCV);
    CvUNIQUE_on(compcv);

    comppad = newAV();
    comppad_name = newAV();
    comppad_name_fill = 0;
    min_intro_pending = 0;
    av_push(comppad, Nullsv);
    curpad = AvARRAY(comppad);
    padix = 0;

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)comppad_name);
    av_store(comppadlist, 1, (SV*)comppad);
    CvPADLIST(compcv) = comppadlist;

    if (saveop->op_type != OP_REQUIRE)
	CvOUTSIDE(compcv) = (CV*)SvREFCNT_inc(caller);

    SAVEFREESV(compcv);

    /* make sure we compile in the right package */

    newstash = curcop->cop_stash;
    if (curstash != newstash) {
	SAVESPTR(curstash);
	curstash = newstash;
    }
    SAVESPTR(beginav);
    beginav = newAV();
    SAVEFREESV(beginav);

    /* try to compile it */

    eval_root = Nullop;
    error_count = 0;
    curcop = &compiling;
    curcop->cop_arybase = 0;
    SvREFCNT_dec(rs);
    rs = newSVpv("\n", 1);
    if (saveop->op_flags & OPf_SPECIAL)
	in_eval |= 4;
    else
	sv_setpv(GvSV(errgv),"");
    if (yyparse() || error_count || !eval_root) {
	SV **newsp;
	I32 gimme;
	CONTEXT *cx;
	I32 optype;

	op = saveop;
	if (eval_root) {
	    op_free(eval_root);
	    eval_root = Nullop;
	}
	SP = stack_base + POPMARK;		/* pop original mark */
	POPBLOCK(cx,curpm);
	POPEVAL(cx);
	pop_return();
	lex_end();
	LEAVE;
	if (optype == OP_REQUIRE) {
	    char* msg = SvPVx(GvSV(errgv), na);
	    DIE("%s", *msg ? msg : "Compilation failed in require");
	}
	SvREFCNT_dec(rs);
	rs = SvREFCNT_inc(nrs);
	RETPUSHUNDEF;
    }
    SvREFCNT_dec(rs);
    rs = SvREFCNT_inc(nrs);
    compiling.cop_line = 0;
    SAVEFREEOP(eval_root);
    if (gimme & G_VOID)
	scalarvoid(eval_root);
    else if (gimme & G_ARRAY)
	list(eval_root);
    else
	scalar(eval_root);

    DEBUG_x(dump_eval());

    /* Register with debugger: */
    if (PERLDB_INTER && saveop->op_type == OP_REQUIRE) {
	CV *cv = perl_get_cv("DB::postponed", FALSE);
	if (cv) {
	    dSP;
	    PUSHMARK(sp);
	    XPUSHs((SV*)compiling.cop_filegv);
	    PUTBACK;
	    perl_call_sv((SV*)cv, G_DISCARD);
	}
    }

    /* compiled okay, so do it */

    CvDEPTH(compcv) = 1;

    SP = stack_base + POPMARK;		/* pop original mark */
    op = saveop;					/* The caller may need it. */
    RETURNOP(eval_start);
}

PP(pp_require)
{
    dSP;
    register CONTEXT *cx;
    SV *sv;
    char *name;
    char *tryname;
    SV *namesv = Nullsv;
    SV** svp;
    I32 gimme = G_SCALAR;
    PerlIO *tryrsfp = 0;

    sv = POPs;
    if (SvNIOKp(sv) && !SvPOKp(sv)) {
	SET_NUMERIC_STANDARD();
	if (atof(patchlevel) + 0.00000999 < SvNV(sv))
	    DIE("Perl %s required--this is only version %s, stopped",
		SvPV(sv,na),patchlevel);
	RETPUSHYES;
    }
    name = SvPV(sv, na);
    if (!*name)
	DIE("Null filename used");
    TAINT_PROPER("require");
    if (op->op_type == OP_REQUIRE &&
      (svp = hv_fetch(GvHVn(incgv), name, SvCUR(sv), 0)) &&
      *svp != &sv_undef)
	RETPUSHYES;

    /* prepare to compile file */

    if (*name == '/' ||
	(*name == '.' && 
	    (name[1] == '/' ||
	     (name[1] == '.' && name[2] == '/')))
#ifdef DOSISH
      || (name[0] && name[1] == ':')
#endif
#ifdef WIN32
      || (name[0] == '\\' && name[1] == '\\')	/* UNC path */
#endif
#ifdef VMS
	|| (strchr(name,':')  || ((*name == '[' || *name == '<') &&
	    (isALNUM(name[1]) || strchr("$-_]>",name[1]))))
#endif
    )
    {
	tryname = name;
	tryrsfp = PerlIO_open(name,"r");
    }
    else {
	AV *ar = GvAVn(incgv);
	I32 i;
#ifdef VMS
	char *unixname;
	if ((unixname = tounixspec(name, Nullch)) != Nullch)
#endif
	{
	    namesv = NEWSV(806, 0);
	    for (i = 0; i <= AvFILL(ar); i++) {
		char *dir = SvPVx(*av_fetch(ar, i, TRUE), na);
#ifdef VMS
		char *unixdir;
		if ((unixdir = tounixpath(dir, Nullch)) == Nullch)
		    continue;
		sv_setpv(namesv, unixdir);
		sv_catpv(namesv, unixname);
#else
		sv_setpvf(namesv, "%s/%s", dir, name);
#endif
		tryname = SvPVX(namesv);
		tryrsfp = PerlIO_open(tryname, "r");
		if (tryrsfp) {
		    if (tryname[0] == '.' && tryname[1] == '/')
			tryname += 2;
		    break;
		}
	    }
	}
    }
    SAVESPTR(compiling.cop_filegv);
    compiling.cop_filegv = gv_fetchfile(tryrsfp ? tryname : name);
    SvREFCNT_dec(namesv);
    if (!tryrsfp) {
	if (op->op_type == OP_REQUIRE) {
	    SV *msg = sv_2mortal(newSVpvf("Can't locate %s in @INC", name));
	    SV *dirmsgsv = NEWSV(0, 0);
	    AV *ar = GvAVn(incgv);
	    I32 i;
	    if (instr(SvPVX(msg), ".h "))
		sv_catpv(msg, " (change .h to .ph maybe?)");
	    if (instr(SvPVX(msg), ".ph "))
		sv_catpv(msg, " (did you run h2ph?)");
	    sv_catpv(msg, " (@INC contains:");
	    for (i = 0; i <= AvFILL(ar); i++) {
		char *dir = SvPVx(*av_fetch(ar, i, TRUE), na);
		sv_setpvf(dirmsgsv, " %s", dir);
	        sv_catsv(msg, dirmsgsv);
	    }
	    sv_catpvn(msg, ")", 1);
    	    SvREFCNT_dec(dirmsgsv);
	    DIE("%_", msg);
	}

	RETPUSHUNDEF;
    }

    /* Assume success here to prevent recursive requirement. */
    (void)hv_store(GvHVn(incgv), name, strlen(name),
	newSVsv(GvSV(compiling.cop_filegv)), 0 );

    ENTER;
    SAVETMPS;
    lex_start(sv_2mortal(newSVpv("",0)));
    if (rsfp_filters){
 	save_aptr(&rsfp_filters);
	rsfp_filters = NULL;
    }

    rsfp = tryrsfp;
    name = savepv(name);
    SAVEFREEPV(name);
    SAVEI32(hints);
    hints = 0;
 
    /* switch to eval mode */

    push_return(op->op_next);
    PUSHBLOCK(cx, CXt_EVAL, SP);
    PUSHEVAL(cx, name, compiling.cop_filegv);

    compiling.cop_line = 0;

    PUTBACK;
    return DOCATCH(doeval(G_SCALAR));
}

PP(pp_dofile)
{
    return pp_require(ARGS);
}

PP(pp_entereval)
{
    dSP;
    register CONTEXT *cx;
    dPOPss;
    I32 gimme = GIMME_V, was = sub_generation;
    char tmpbuf[TYPE_DIGITS(long) + 12];
    char *safestr;
    STRLEN len;
    OP *ret;

    if (!SvPV(sv,len) || !len)
	RETPUSHUNDEF;
    TAINT_PROPER("eval");

    ENTER;
    lex_start(sv);
    SAVETMPS;
 
    /* switch to eval mode */

    SAVESPTR(compiling.cop_filegv);
    sprintf(tmpbuf, "_<(eval %lu)", (unsigned long)++evalseq);
    compiling.cop_filegv = gv_fetchfile(tmpbuf+2);
    compiling.cop_line = 1;
    /* XXX For C<eval "...">s within BEGIN {} blocks, this ends up
       deleting the eval's FILEGV from the stash before gv_check() runs
       (i.e. before run-time proper). To work around the coredump that
       ensues, we always turn GvMULTI_on for any globals that were
       introduced within evals. See force_ident(). GSAR 96-10-12 */
    safestr = savepv(tmpbuf);
    SAVEDELETE(defstash, safestr, strlen(safestr));
    SAVEI32(hints);
    hints = op->op_targ;

    push_return(op->op_next);
    PUSHBLOCK(cx, CXt_EVAL, SP);
    PUSHEVAL(cx, 0, compiling.cop_filegv);

    /* prepare to compile string */

    if (PERLDB_LINE && curstash != debstash)
	save_lines(GvAV(compiling.cop_filegv), linestr);
    PUTBACK;
    ret = doeval(gimme);
    if (PERLDB_INTER && was != sub_generation /* Some subs defined here. */
	&& ret != op->op_next) {	/* Successive compilation. */
	strcpy(safestr, "_<(eval )");	/* Anything fake and short. */
    }
    return DOCATCH(ret);
}

PP(pp_leaveeval)
{
    dSP;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register CONTEXT *cx;
    OP *retop;
    U8 save_flags = op -> op_flags;
    I32 optype;

    POPBLOCK(cx,newpm);
    POPEVAL(cx);
    retop = pop_return();

    TAINT_NOT;
    if (gimme == G_VOID)
	MARK = newsp;
    else if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (SvFLAGS(TOPs) & SVs_TEMP)
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	}
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
    }
    else {
	/* in case LEAVE wipes old return values */
	for (mark = newsp + 1; mark <= SP; mark++) {
	    if (!(SvFLAGS(*mark) & SVs_TEMP)) {
		*mark = sv_mortalcopy(*mark);
		TAINT_NOT;	/* Each item is independent */
	    }
	}
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    /*
     * Closures mentioned at top level of eval cannot be referenced
     * again, and their presence indirectly causes a memory leak.
     * (Note that the fact that compcv and friends are still set here
     * is, AFAIK, an accident.)  --Chip
     */
    if (AvFILL(comppad_name) >= 0) {
	SV **svp = AvARRAY(comppad_name);
	I32 ix;
	for (ix = AvFILL(comppad_name); ix >= 0; ix--) {
	    SV *sv = svp[ix];
	    if (sv && sv != &sv_undef && *SvPVX(sv) == '&') {
		SvREFCNT_dec(sv);
		svp[ix] = &sv_undef;

		sv = curpad[ix];
		if (CvCLONE(sv)) {
		    SvREFCNT_dec(CvOUTSIDE(sv));
		    CvOUTSIDE(sv) = Nullcv;
		}
		else {
		    SvREFCNT_dec(sv);
		    sv = NEWSV(0,0);
		    SvPADTMP_on(sv);
		    curpad[ix] = sv;
		}
	    }
	}
    }

#ifdef DEBUGGING
    assert(CvDEPTH(compcv) == 1);
#endif
    CvDEPTH(compcv) = 0;

    if (optype == OP_REQUIRE &&
	!(gimme == G_SCALAR ? SvTRUE(*sp) : sp > newsp))
    {
	/* Unassume the success we assumed earlier. */
	char *name = cx->blk_eval.old_name;
	(void)hv_delete(GvHVn(incgv), name, strlen(name), G_DISCARD);
	retop = die("%s did not return a true value", name);
    }

    lex_end();
    LEAVE;

    if (!(save_flags & OPf_SPECIAL))
	sv_setpv(GvSV(errgv),"");

    RETURNOP(retop);
}

PP(pp_entertry)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme = GIMME_V;

    ENTER;
    SAVETMPS;

    push_return(cLOGOP->op_other->op_next);
    PUSHBLOCK(cx, CXt_EVAL, SP);
    PUSHEVAL(cx, 0, 0);
    eval_root = op;		/* Only needed so that goto works right. */

    in_eval = 1;
    sv_setpv(GvSV(errgv),"");
    PUTBACK;
    return DOCATCH(op->op_next);
}

PP(pp_leavetry)
{
    dSP;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register CONTEXT *cx;
    I32 optype;

    POPBLOCK(cx,newpm);
    POPEVAL(cx);
    pop_return();

    TAINT_NOT;
    if (gimme == G_VOID)
	SP = newsp;
    else if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	}
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
	SP = MARK;
    }
    else {
	/* in case LEAVE wipes old return values */
	for (mark = newsp + 1; mark <= SP; mark++) {
	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP))) {
		*mark = sv_mortalcopy(*mark);
		TAINT_NOT;	/* Each item is independent */
	    }
	}
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;
    sv_setpv(GvSV(errgv),"");
    RETURN;
}

static void
doparseform(sv)
SV *sv;
{
    STRLEN len;
    register char *s = SvPV_force(sv, len);
    register char *send = s + len;
    register char *base;
    register I32 skipspaces = 0;
    bool noblank;
    bool repeat;
    bool postspace = FALSE;
    U16 *fops;
    register U16 *fpc;
    U16 *linepc;
    register I32 arg;
    bool ischop;

    if (len == 0)
	croak("Null picture in formline");
    
    New(804, fops, (send - s)*3+10, U16);    /* Almost certainly too long... */
    fpc = fops;

    if (s < send) {
	linepc = fpc;
	*fpc++ = FF_LINEMARK;
	noblank = repeat = FALSE;
	base = s;
    }

    while (s <= send) {
	switch (*s++) {
	default:
	    skipspaces = 0;
	    continue;

	case '~':
	    if (*s == '~') {
		repeat = TRUE;
		*s = ' ';
	    }
	    noblank = TRUE;
	    s[-1] = ' ';
	    /* FALL THROUGH */
	case ' ': case '\t':
	    skipspaces++;
	    continue;
	    
	case '\n': case 0:
	    arg = s - base;
	    skipspaces++;
	    arg -= skipspaces;
	    if (arg) {
		if (postspace)
		    *fpc++ = FF_SPACE;
		*fpc++ = FF_LITERAL;
		*fpc++ = arg;
	    }
	    postspace = FALSE;
	    if (s <= send)
		skipspaces--;
	    if (skipspaces) {
		*fpc++ = FF_SKIP;
		*fpc++ = skipspaces;
	    }
	    skipspaces = 0;
	    if (s <= send)
		*fpc++ = FF_NEWLINE;
	    if (noblank) {
		*fpc++ = FF_BLANK;
		if (repeat)
		    arg = fpc - linepc + 1;
		else
		    arg = 0;
		*fpc++ = arg;
	    }
	    if (s < send) {
		linepc = fpc;
		*fpc++ = FF_LINEMARK;
		noblank = repeat = FALSE;
		base = s;
	    }
	    else
		s++;
	    continue;

	case '@':
	case '^':
	    ischop = s[-1] == '^';

	    if (postspace) {
		*fpc++ = FF_SPACE;
		postspace = FALSE;
	    }
	    arg = (s - base) - 1;
	    if (arg) {
		*fpc++ = FF_LITERAL;
		*fpc++ = arg;
	    }

	    base = s - 1;
	    *fpc++ = FF_FETCH;
	    if (*s == '*') {
		s++;
		*fpc++ = 0;
		*fpc++ = FF_LINEGLOB;
	    }
	    else if (*s == '#' || (*s == '.' && s[1] == '#')) {
		arg = ischop ? 512 : 0;
		base = s - 1;
		while (*s == '#')
		    s++;
		if (*s == '.') {
		    char *f;
		    s++;
		    f = s;
		    while (*s == '#')
			s++;
		    arg |= 256 + (s - f);
		}
		*fpc++ = s - base;		/* fieldsize for FETCH */
		*fpc++ = FF_DECIMAL;
		*fpc++ = arg;
	    }
	    else {
		I32 prespace = 0;
		bool ismore = FALSE;

		if (*s == '>') {
		    while (*++s == '>') ;
		    prespace = FF_SPACE;
		}
		else if (*s == '|') {
		    while (*++s == '|') ;
		    prespace = FF_HALFSPACE;
		    postspace = TRUE;
		}
		else {
		    if (*s == '<')
			while (*++s == '<') ;
		    postspace = TRUE;
		}
		if (*s == '.' && s[1] == '.' && s[2] == '.') {
		    s += 3;
		    ismore = TRUE;
		}
		*fpc++ = s - base;		/* fieldsize for FETCH */

		*fpc++ = ischop ? FF_CHECKCHOP : FF_CHECKNL;

		if (prespace)
		    *fpc++ = prespace;
		*fpc++ = FF_ITEM;
		if (ismore)
		    *fpc++ = FF_MORE;
		if (ischop)
		    *fpc++ = FF_CHOP;
	    }
	    base = s;
	    skipspaces = 0;
	    continue;
	}
    }
    *fpc++ = FF_END;

    arg = fpc - fops;
    { /* need to jump to the next word */
        int z;
	z = WORD_ALIGN - SvCUR(sv) % WORD_ALIGN;
	SvGROW(sv, SvCUR(sv) + z + arg * sizeof(U16) + 4);
	s = SvPVX(sv) + SvCUR(sv) + z;
    }
    Copy(fops, s, arg, U16);
    Safefree(fops);
    sv_magic(sv, Nullsv, 'f', Nullch, 0);
    SvCOMPILED_on(sv);
}
