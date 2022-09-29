/*    pp.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "It's a big house this, and very peculiar.  Always a bit more to discover,
 * and no knowing what you'll find around a corner.  And Elves, sir!" --Samwise
 */

#include "EXTERN.h"
#include "perl.h"

/*
 * The compiler on Concurrent CX/UX systems has a subtle bug which only
 * seems to show up when compiling pp.c - it generates the wrong double
 * precision constant value for (double)UV_MAX when used inline in the body
 * of the code below, so this makes a static variable up front (which the
 * compiler seems to get correct) and uses it in place of UV_MAX below.
 */
#ifdef CXUX_BROKEN_CONSTANT_CONVERT
static double UV_MAX_cxux = ((double)UV_MAX);
#endif

/*
 * Types used in bitwise operations.
 *
 * Normally we'd just use IV and UV.  However, some hardware and
 * software combinations (e.g. Alpha and current OSF/1) don't have a
 * floating-point type to use for NV that has adequate bits to fully
 * hold an IV/UV.  (In other words, sizeof(long) == sizeof(double).)
 *
 * It just so happens that "int" is the right size almost everywhere.
 */
typedef int IBW;
typedef unsigned UBW;

/*
 * Mask used after bitwise operations.
 *
 * There is at least one realm (Cray word machines) that doesn't
 * have an integral type (except char) small enough to be represented
 * in a double without loss; that is, it has no 32-bit type.
 */
#if BYTEORDER > 0xFFFF && defined(_CRAY) && !defined(_CRAYMPP)
#  define BW_BITS  32
#  define BW_MASK  ((1 << BW_BITS) - 1)
#  define BW_SIGN  (1 << (BW_BITS - 1))
#  define BWi(i)  (((i) & BW_SIGN) ? ((i) | ~BW_MASK) : ((i) & BW_MASK))
#  define BWu(u)  ((u) & BW_MASK)
#else
#  define BWi(i)  (i)
#  define BWu(u)  (u)
#endif

/*
 * Offset for integer pack/unpack.
 *
 * On architectures where I16 and I32 aren't really 16 and 32 bits,
 * which for now are all Crays, pack and unpack have to play games.
 */

/*
 * These values are required for portability of pack() output.
 * If they're not right on your machine, then pack() and unpack()
 * wouldn't work right anyway; you'll need to apply the Cray hack.
 * (I'd like to check them with #if, but you can't use sizeof() in
 * the preprocessor.)
 */
#define SIZE16 2
#define SIZE32 4

#if BYTEORDER > 0xFFFF && defined(_CRAY) && !defined(_CRAYMPP)
#  if BYTEORDER == 0x12345678
#    define OFF16(p)	(char*)(p)
#    define OFF32(p)	(char*)(p)
#  else
#    if BYTEORDER == 0x87654321
#      define OFF16(p)	((char*)(p) + (sizeof(U16) - SIZE16))
#      define OFF32(p)	((char*)(p) + (sizeof(U32) - SIZE32))
#    else
       }}}} bad cray byte order
#    endif
#  endif
#  define COPY16(s,p)  (*(p) = 0, Copy(s, OFF16(p), SIZE16, char))
#  define COPY32(s,p)  (*(p) = 0, Copy(s, OFF32(p), SIZE32, char))
#  define CAT16(sv,p)  sv_catpvn(sv, OFF16(p), SIZE16)
#  define CAT32(sv,p)  sv_catpvn(sv, OFF32(p), SIZE32)
#else
#  define COPY16(s,p)  Copy(s, p, SIZE16, char)
#  define COPY32(s,p)  Copy(s, p, SIZE32, char)
#  define CAT16(sv,p)  sv_catpvn(sv, (char*)(p), SIZE16)
#  define CAT32(sv,p)  sv_catpvn(sv, (char*)(p), SIZE32)
#endif

static void doencodes _((SV* sv, char* s, I32 len));
static SV* refto _((SV* sv));
static U32 seed _((void));

static bool srand_called = FALSE;

/* variations on pp_null */

PP(pp_stub)
{
    dSP;
    if (GIMME_V == G_SCALAR)
	XPUSHs(&sv_undef);
    RETURN;
}

PP(pp_scalar)
{
    return NORMAL;
}

/* Pushy stuff. */

PP(pp_padav)
{
    dSP; dTARGET;
    if (op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(curpad[op->op_targ]);
    EXTEND(SP, 1);
    if (op->op_flags & OPf_REF) {
	PUSHs(TARG);
	RETURN;
    }
    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	EXTEND(SP, maxarg);
	Copy(AvARRAY((AV*)TARG), SP+1, maxarg, SV*);
	SP += maxarg;
    }
    else {
	SV* sv = sv_newmortal();
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	sv_setiv(sv, maxarg);
	PUSHs(sv);
    }
    RETURN;
}

PP(pp_padhv)
{
    dSP; dTARGET;
    I32 gimme;

    XPUSHs(TARG);
    if (op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(curpad[op->op_targ]);
    if (op->op_flags & OPf_REF)
	RETURN;
    gimme = GIMME_V;
    if (gimme == G_ARRAY) {
	RETURNOP(do_kv(ARGS));
    }
    else if (gimme == G_SCALAR) {
	SV* sv = sv_newmortal();
	if (HvFILL((HV*)TARG))
	    sv_setpvf(sv, "%ld/%ld",
		      (long)HvFILL((HV*)TARG), (long)HvMAX((HV*)TARG) + 1);
	else
	    sv_setiv(sv, 0);
	SETs(sv);
    }
    RETURN;
}

PP(pp_padany)
{
    DIE("NOT IMPL LINE %d",__LINE__);
}

/* Translations. */

PP(pp_rv2gv)
{
    dSP; dTOPss;
    
    if (SvROK(sv)) {
      wasref:
	sv = SvRV(sv);
	if (SvTYPE(sv) == SVt_PVIO) {
	    GV *gv = (GV*) sv_newmortal();
	    gv_init(gv, 0, "", 0, 0);
	    GvIOp(gv) = (IO *)sv;
	    (void)SvREFCNT_inc(sv);
	    sv = (SV*) gv;
	} else if (SvTYPE(sv) != SVt_PVGV)
	    DIE("Not a GLOB reference");
    }
    else {
	if (SvTYPE(sv) != SVt_PVGV) {
	    char *sym;

	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv)) {
		if (op->op_flags & OPf_REF ||
		    op->op_private & HINT_STRICT_REFS)
		    DIE(no_usym, "a symbol");
		if (dowarn)
		    warn(warn_uninit);
		RETSETUNDEF;
	    }
	    sym = SvPV(sv, na);
	    if (op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, sym, "a symbol");
	    sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVGV);
	}
    }
    if (op->op_private & OPpLVAL_INTRO)
	save_gp((GV*)sv, !(op->op_flags & OPf_SPECIAL));
    SETs(sv);
    RETURN;
}

PP(pp_rv2sv)
{
    dSP; dTOPss;

    if (SvROK(sv)) {
      wasref:
	sv = SvRV(sv);
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	case SVt_PVHV:
	case SVt_PVCV:
	    DIE("Not a SCALAR reference");
	}
    }
    else {
	GV *gv = (GV*)sv;
	char *sym;

	if (SvTYPE(gv) != SVt_PVGV) {
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv)) {
		if (op->op_flags & OPf_REF ||
		    op->op_private & HINT_STRICT_REFS)
		    DIE(no_usym, "a SCALAR");
		if (dowarn)
		    warn(warn_uninit);
		RETSETUNDEF;
	    }
	    sym = SvPV(sv, na);
	    if (op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, sym, "a SCALAR");
	    gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PV);
	}
	sv = GvSV(gv);
    }
    if (op->op_flags & OPf_MOD) {
	if (op->op_private & OPpLVAL_INTRO)
	    sv = save_scalar((GV*)TOPs);
	else if (op->op_private & OPpDEREF)
	    vivify_ref(sv, op->op_private & OPpDEREF);
    }
    SETs(sv);
    RETURN;
}

PP(pp_av2arylen)
{
    dSP;
    AV *av = (AV*)TOPs;
    SV *sv = AvARYLEN(av);
    if (!sv) {
	AvARYLEN(av) = sv = NEWSV(0,0);
	sv_upgrade(sv, SVt_IV);
	sv_magic(sv, (SV*)av, '#', Nullch, 0);
    }
    SETs(sv);
    RETURN;
}

PP(pp_pos)
{
    dSP; dTARGET; dPOPss;
    
    if (op->op_flags & OPf_MOD) {
	if (SvTYPE(TARG) < SVt_PVLV) {
	    sv_upgrade(TARG, SVt_PVLV);
	    sv_magic(TARG, Nullsv, '.', Nullch, 0);
	}

	LvTYPE(TARG) = '.';
	LvTARG(TARG) = sv;
	PUSHs(TARG);	/* no SvSETMAGIC */
	RETURN;
    }
    else {
	MAGIC* mg; 

	if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	    mg = mg_find(sv, 'g');
	    if (mg && mg->mg_len >= 0) {
		PUSHi(mg->mg_len + curcop->cop_arybase);
		RETURN;
	    }
	}
	RETPUSHUNDEF;
    }
}

PP(pp_rv2cv)
{
    dSP;
    GV *gv;
    HV *stash;

    /* We usually try to add a non-existent subroutine in case of AUTOLOAD. */
    /* (But not in defined().) */
    CV *cv = sv_2cv(TOPs, &stash, &gv, !(op->op_flags & OPf_SPECIAL));
    if (cv) {
	if (CvCLONE(cv))
	    cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
    }
    else
	cv = (CV*)&sv_undef;
    SETs((SV*)cv);
    RETURN;
}

PP(pp_prototype)
{
    dSP;
    CV *cv;
    HV *stash;
    GV *gv;
    SV *ret;

    ret = &sv_undef;
    cv = sv_2cv(TOPs, &stash, &gv, FALSE);
    if (cv && SvPOK(cv))
	ret = sv_2mortal(newSVpv(SvPVX(cv), SvCUR(cv)));
    SETs(ret);
    RETURN;
}

PP(pp_anoncode)
{
    dSP;
    CV* cv = (CV*)curpad[op->op_targ];
    if (CvCLONE(cv))
	cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
    EXTEND(SP,1);
    PUSHs((SV*)cv);
    RETURN;
}

PP(pp_srefgen)
{
    dSP;
    *SP = refto(*SP);
    RETURN;
} 

PP(pp_refgen)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	MARK[1] = *SP;
	SP = MARK + 1;
    }
    EXTEND_MORTAL(SP - MARK);
    while (++MARK <= SP)
	*MARK = refto(*MARK);
    RETURN;
}

static SV*
refto(sv)
SV* sv;
{
    SV* rv;

    if (SvTYPE(sv) == SVt_PVLV && LvTYPE(sv) == 'y') {
	if (LvTARGLEN(sv))
	    vivify_defelem(sv);
	if (!(sv = LvTARG(sv)))
	    sv = &sv_undef;
    }
    else if (SvPADTMP(sv))
	sv = newSVsv(sv);
    else {
	SvTEMP_off(sv);
	(void)SvREFCNT_inc(sv);
    }
    rv = sv_newmortal();
    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = sv;
    SvROK_on(rv);
    return rv;
}

PP(pp_ref)
{
    dSP; dTARGET;
    SV *sv;
    char *pv;

    sv = POPs;

    if (sv && SvGMAGICAL(sv))
	mg_get(sv);     

    if (!sv || !SvROK(sv))
	RETPUSHNO;

    sv = SvRV(sv);
    pv = sv_reftype(sv,TRUE);
    PUSHp(pv, strlen(pv));
    RETURN;
}

PP(pp_bless)
{
    dSP;
    HV *stash;

    if (MAXARG == 1)
	stash = curcop->cop_stash;
    else
	stash = gv_stashsv(POPs, TRUE);

    (void)sv_bless(TOPs, stash);
    RETURN;
}

PP(pp_gelem)
{
    GV *gv;
    SV *sv;
    SV *ref;
    char *elem;
    dSP;

    sv = POPs;
    elem = SvPV(sv, na);
    gv = (GV*)POPs;
    ref = Nullsv;
    sv = Nullsv;
    switch (elem ? *elem : '\0')
    {
    case 'A':
	if (strEQ(elem, "ARRAY"))
	    ref = (SV*)GvAV(gv);
	break;
    case 'C':
	if (strEQ(elem, "CODE"))
	    ref = (SV*)GvCVu(gv);
	break;
    case 'F':
	if (strEQ(elem, "FILEHANDLE")) /* XXX deprecate in 5.005 */
	    ref = (SV*)GvIOp(gv);
	break;
    case 'G':
	if (strEQ(elem, "GLOB"))
	    ref = (SV*)gv;
	break;
    case 'H':
	if (strEQ(elem, "HASH"))
	    ref = (SV*)GvHV(gv);
	break;
    case 'I':
	if (strEQ(elem, "IO"))
	    ref = (SV*)GvIOp(gv);
	break;
    case 'N':
	if (strEQ(elem, "NAME"))
	    sv = newSVpv(GvNAME(gv), GvNAMELEN(gv));
	break;
    case 'P':
	if (strEQ(elem, "PACKAGE"))
	    sv = newSVpv(HvNAME(GvSTASH(gv)), 0);
	break;
    case 'S':
	if (strEQ(elem, "SCALAR"))
	    ref = GvSV(gv);
	break;
    }
    if (ref)
	sv = newRV(ref);
    if (sv)
	sv_2mortal(sv);
    else
	sv = &sv_undef;
    XPUSHs(sv);
    RETURN;
}

/* Pattern matching */

PP(pp_study)
{
    dSP; dPOPss;
    register unsigned char *s;
    register I32 pos;
    register I32 ch;
    register I32 *sfirst;
    register I32 *snext;
    STRLEN len;

    if (sv == lastscream) {
	if (SvSCREAM(sv))
	    RETPUSHYES;
    }
    else {
	if (lastscream) {
	    SvSCREAM_off(lastscream);
	    SvREFCNT_dec(lastscream);
	}
	lastscream = SvREFCNT_inc(sv);
    }

    s = (unsigned char*)(SvPV(sv, len));
    pos = len;
    if (pos <= 0)
	RETPUSHNO;
    if (pos > maxscream) {
	if (maxscream < 0) {
	    maxscream = pos + 80;
	    New(301, screamfirst, 256, I32);
	    New(302, screamnext, maxscream, I32);
	}
	else {
	    maxscream = pos + pos / 4;
	    Renew(screamnext, maxscream, I32);
	}
    }

    sfirst = screamfirst;
    snext = screamnext;

    if (!sfirst || !snext)
	DIE("do_study: out of memory");

    for (ch = 256; ch; --ch)
	*sfirst++ = -1;
    sfirst -= 256;

    while (--pos >= 0) {
	ch = s[pos];
	if (sfirst[ch] >= 0)
	    snext[pos] = sfirst[ch] - pos;
	else
	    snext[pos] = -pos;
	sfirst[ch] = pos;
    }

    SvSCREAM_on(sv);
    sv_magic(sv, Nullsv, 'g', Nullch, 0);	/* piggyback on m//g magic */
    RETPUSHYES;
}

PP(pp_trans)
{
    dSP; dTARG;
    SV *sv;

    if (op->op_flags & OPf_STACKED)
	sv = POPs;
    else {
	sv = GvSV(defgv);
	EXTEND(SP,1);
    }
    TARG = sv_newmortal();
    PUSHi(do_trans(sv, op));
    RETURN;
}

/* Lvalue operators. */

PP(pp_schop)
{
    dSP; dTARGET;
    do_chop(TARG, TOPs);
    SETTARG;
    RETURN;
}

PP(pp_chop)
{
    dSP; dMARK; dTARGET;
    while (SP > MARK)
	do_chop(TARG, POPs);
    PUSHTARG;
    RETURN;
}

PP(pp_schomp)
{
    dSP; dTARGET;
    SETi(do_chomp(TOPs));
    RETURN;
}

PP(pp_chomp)
{
    dSP; dMARK; dTARGET;
    register I32 count = 0;
    
    while (SP > MARK)
	count += do_chomp(POPs);
    PUSHi(count);
    RETURN;
}

PP(pp_defined)
{
    dSP;
    register SV* sv;

    sv = POPs;
    if (!sv || !SvANY(sv))
	RETPUSHNO;
    switch (SvTYPE(sv)) {
    case SVt_PVAV:
	if (AvMAX(sv) >= 0 || SvGMAGICAL(sv))
	    RETPUSHYES;
	break;
    case SVt_PVHV:
	if (HvARRAY(sv) || SvGMAGICAL(sv))
	    RETPUSHYES;
	break;
    case SVt_PVCV:
	if (CvROOT(sv) || CvXSUB(sv))
	    RETPUSHYES;
	break;
    default:
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvOK(sv))
	    RETPUSHYES;
    }
    RETPUSHNO;
}

PP(pp_undef)
{
    dSP;
    SV *sv;

    if (!op->op_private) {
	EXTEND(SP, 1);
	RETPUSHUNDEF;
    }

    sv = POPs;
    if (!sv)
	RETPUSHUNDEF;

    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv))
	    RETPUSHUNDEF;
	if (SvROK(sv))
	    sv_unref(sv);
    }

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_PVAV:
	av_undef((AV*)sv);
	break;
    case SVt_PVHV:
	hv_undef((HV*)sv);
	break;
    case SVt_PVCV:
	if (cv_const_sv((CV*)sv))
	    warn("Constant subroutine %s undefined",
		 CvANON((CV*)sv) ? "(anonymous)" : GvENAME(CvGV((CV*)sv)));
	/* FALL THROUGH */
    case SVt_PVFM:
	{ GV* gv = (GV*)SvREFCNT_inc(CvGV((CV*)sv));
	  cv_undef((CV*)sv);
	  CvGV((CV*)sv) = gv; }   /* let user-undef'd sub keep its identity */
	break;
    case SVt_PVGV:
	if (SvFAKE(sv))
	    sv_setsv(sv, &sv_undef);
	break;
    default:
	if (SvTYPE(sv) >= SVt_PV && SvPVX(sv) && SvLEN(sv)) {
	    (void)SvOOK_off(sv);
	    Safefree(SvPVX(sv));
	    SvPV_set(sv, Nullch);
	    SvLEN_set(sv, 0);
	}
	(void)SvOK_off(sv);
	SvSETMAGIC(sv);
    }

    RETPUSHUNDEF;
}

PP(pp_predec)
{
    dSP;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	croak(no_modify);
    if (SvIOK(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MIN)
    {
	--SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_postinc)
{
    dSP; dTARGET;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	croak(no_modify);
    sv_setsv(TARG, TOPs);
    if (SvIOK(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MAX)
    {
	++SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    if (!SvOK(TARG))
	sv_setiv(TARG, 0);
    SETs(TARG);
    return NORMAL;
}

PP(pp_postdec)
{
    dSP; dTARGET;
    if(SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	croak(no_modify);
    sv_setsv(TARG, TOPs);
    if (SvIOK(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MIN)
    {
	--SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    SETs(TARG);
    return NORMAL;
}

/* Ordinary operators. */

PP(pp_pow)
{
    dSP; dATARGET; tryAMAGICbin(pow,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( pow( left, right) );
      RETURN;
    }
}

PP(pp_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( left * right );
      RETURN;
    }
}

PP(pp_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN); 
    {
      dPOPPOPnnrl;
      double value;
      if (right == 0.0)
	DIE("Illegal division by zero");
#ifdef SLOPPYDIVIDE
      /* insure that 20./5. == 4. */
      {
	IV k;
	if ((double)I_V(left)  == left &&
	    (double)I_V(right) == right &&
	    (k = I_V(left)/I_V(right))*I_V(right) == I_V(left)) {
	    value = k;
	} else {
	    value = left / right;
	}
      }
#else
      value = left / right;
#endif
      PUSHn( value );
      RETURN;
    }
}

PP(pp_modulo)
{
    dSP; dATARGET; tryAMAGICbin(mod,opASSIGN);
    {
      UV left;
      UV right;
      bool left_neg;
      bool right_neg;
      UV ans;

      if (SvIOK(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)) {
	IV i = SvIVX(POPs);
	right = (right_neg = (i < 0)) ? -i : i;
      }
      else {
	double n = POPn;
	right = U_V((right_neg = (n < 0)) ? -n : n);
      }

      if (SvIOK(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)) {
	IV i = SvIVX(POPs);
	left = (left_neg = (i < 0)) ? -i : i;
      }
      else {
	double n = POPn;
	left = U_V((left_neg = (n < 0)) ? -n : n);
      }

      if (!right)
	DIE("Illegal modulus zero");

      ans = left % right;
      if ((left_neg != right_neg) && ans)
	ans = right - ans;
      if (right_neg) {
	/* XXX may warn: unary minus operator applied to unsigned type */
	/* could change -foo to be (~foo)+1 instead	*/
	if (ans <= -(UV)IV_MAX)
	  sv_setiv(TARG, (IV) -ans);
	else
	  sv_setnv(TARG, -(double)ans);
      }
      else
	sv_setuv(TARG, ans);
      PUSHTARG;
      RETURN;
    }
}

PP(pp_repeat)
{
  dSP; dATARGET; tryAMAGICbin(repeat,opASSIGN);
  {
    register I32 count = POPi;
    if (GIMME == G_ARRAY && op->op_private & OPpREPEAT_DOLIST) {
	dMARK;
	I32 items = SP - MARK;
	I32 max;

	max = items * count;
	MEXTEND(MARK, max);
	if (count > 1) {
	    while (SP > MARK) {
		if (*SP)
		    SvTEMP_off((*SP));
		SP--;
	    }
	    MARK++;
	    repeatcpy((char*)(MARK + items), (char*)MARK,
		items * sizeof(SV*), count - 1);
	    SP += max;
	}
	else if (count <= 0)
	    SP -= items;
    }
    else {	/* Note: mark already snarfed by pp_list */
	SV *tmpstr;
	STRLEN len;

	tmpstr = POPs;
	if (TARG == tmpstr && SvTHINKFIRST(tmpstr)) {
	    if (SvREADONLY(tmpstr) && curcop != &compiling)
		DIE("Can't x= to readonly value");
	    if (SvROK(tmpstr))
		sv_unref(tmpstr);
	}
	SvSetSV(TARG, tmpstr);
	SvPV_force(TARG, len);
	if (count != 1) {
	    if (count < 1)
		SvCUR_set(TARG, 0);
	    else {
		SvGROW(TARG, (count * len) + 1);
		repeatcpy(SvPVX(TARG) + len, SvPVX(TARG), len, count - 1);
		SvCUR(TARG) *= count;
	    }
	    *SvEND(TARG) = '\0';
	}
	(void)SvPOK_only(TARG);
	PUSHTARG;
    }
    RETURN;
  }
}

PP(pp_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN); 
    {
      dPOPTOPnnrl_ul;
      SETn( left - right );
      RETURN;
    }
}

PP(pp_left_shift)
{
    dSP; dATARGET; tryAMAGICbin(lshift,opASSIGN); 
    {
      IBW shift = POPi;
      if (op->op_private & HINT_INTEGER) {
	IBW i = TOPi;
	i = BWi(i) << shift;
	SETi(BWi(i));
      }
      else {
	UBW u = TOPu;
	u <<= shift;
	SETu(BWu(u));
      }
      RETURN;
    }
}

PP(pp_right_shift)
{
    dSP; dATARGET; tryAMAGICbin(rshift,opASSIGN); 
    {
      IBW shift = POPi;
      if (op->op_private & HINT_INTEGER) {
	IBW i = TOPi;
	i = BWi(i) >> shift;
	SETi(BWi(i));
      }
      else {
	UBW u = TOPu;
	u >>= shift;
	SETu(BWu(u));
      }
      RETURN;
    }
}

PP(pp_lt)
{
    dSP; tryAMAGICbinSET(lt,0); 
    {
      dPOPnv;
      SETs(boolSV(TOPn < value));
      RETURN;
    }
}

PP(pp_gt)
{
    dSP; tryAMAGICbinSET(gt,0); 
    {
      dPOPnv;
      SETs(boolSV(TOPn > value));
      RETURN;
    }
}

PP(pp_le)
{
    dSP; tryAMAGICbinSET(le,0); 
    {
      dPOPnv;
      SETs(boolSV(TOPn <= value));
      RETURN;
    }
}

PP(pp_ge)
{
    dSP; tryAMAGICbinSET(ge,0); 
    {
      dPOPnv;
      SETs(boolSV(TOPn >= value));
      RETURN;
    }
}

PP(pp_ne)
{
    dSP; tryAMAGICbinSET(ne,0); 
    {
      dPOPnv;
      SETs(boolSV(TOPn != value));
      RETURN;
    }
}

PP(pp_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0); 
    {
      dPOPTOPnnrl;
      I32 value;

      if (left == right)
	value = 0;
      else if (left < right)
	value = -1;
      else if (left > right)
	value = 1;
      else {
	SETs(&sv_undef);
	RETURN;
      }
      SETi(value);
      RETURN;
    }
}

PP(pp_slt)
{
    dSP; tryAMAGICbinSET(slt,0); 
    {
      dPOPTOPssrl;
      int cmp = ((op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp < 0));
      RETURN;
    }
}

PP(pp_sgt)
{
    dSP; tryAMAGICbinSET(sgt,0); 
    {
      dPOPTOPssrl;
      int cmp = ((op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp > 0));
      RETURN;
    }
}

PP(pp_sle)
{
    dSP; tryAMAGICbinSET(sle,0); 
    {
      dPOPTOPssrl;
      int cmp = ((op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp <= 0));
      RETURN;
    }
}

PP(pp_sge)
{
    dSP; tryAMAGICbinSET(sge,0); 
    {
      dPOPTOPssrl;
      int cmp = ((op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp >= 0));
      RETURN;
    }
}

PP(pp_seq)
{
    dSP; tryAMAGICbinSET(seq,0); 
    {
      dPOPTOPssrl;
      SETs(boolSV(sv_eq(left, right)));
      RETURN;
    }
}

PP(pp_sne)
{
    dSP; tryAMAGICbinSET(sne,0); 
    {
      dPOPTOPssrl;
      SETs(boolSV(!sv_eq(left, right)));
      RETURN;
    }
}

PP(pp_scmp)
{
    dSP; dTARGET;  tryAMAGICbin(scmp,0);
    {
      dPOPTOPssrl;
      int cmp = ((op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETi( cmp );
      RETURN;
    }
}

PP(pp_bit_and)
{
    dSP; dATARGET; tryAMAGICbin(band,opASSIGN); 
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (op->op_private & HINT_INTEGER) {
	  IBW value = SvIV(left) & SvIV(right); 
	  SETi(BWi(value));
	}
	else {
	  UBW value = SvUV(left) & SvUV(right); 
	  SETu(BWu(value));
	}
      }
      else {
	do_vop(op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_xor)
{
    dSP; dATARGET; tryAMAGICbin(bxor,opASSIGN); 
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (op->op_private & HINT_INTEGER) {
	  IBW value = (USE_LEFT(left) ? SvIV(left) : 0) ^ SvIV(right); 
	  SETi(BWi(value));
	}
	else {
	  UBW value = (USE_LEFT(left) ? SvUV(left) : 0) ^ SvUV(right); 
	  SETu(BWu(value));
	}
      }
      else {
	do_vop(op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_or)
{
    dSP; dATARGET; tryAMAGICbin(bor,opASSIGN); 
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (op->op_private & HINT_INTEGER) {
	  IBW value = (USE_LEFT(left) ? SvIV(left) : 0) | SvIV(right); 
	  SETi(BWi(value));
	}
	else {
	  UBW value = (USE_LEFT(left) ? SvUV(left) : 0) | SvUV(right); 
	  SETu(BWu(value));
	}
      }
      else {
	do_vop(op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    {
	dTOPss;
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvIOKp(sv) && !SvNOKp(sv) && !SvPOKp(sv) && SvIVX(sv) != IV_MIN)
	    SETi(-SvIVX(sv));
	else if (SvNIOKp(sv))
	    SETn(-SvNV(sv));
	else if (SvPOKp(sv)) {
	    STRLEN len;
	    char *s = SvPV(sv, len);
	    if (isIDFIRST(*s)) {
		sv_setpvn(TARG, "-", 1);
		sv_catsv(TARG, sv);
	    }
	    else if (*s == '+' || *s == '-') {
		sv_setsv(TARG, sv);
		*SvPV_force(TARG, len) = *s == '-' ? '+' : '-';
	    }
	    else
		sv_setnv(TARG, -SvNV(sv));
	    SETTARG;
	}
	else
	    SETn(-SvNV(sv));
    }
    RETURN;
}

PP(pp_not)
{
#ifdef OVERLOAD
    dSP; tryAMAGICunSET(not);
#endif /* OVERLOAD */
    *stack_sp = boolSV(!SvTRUE(*stack_sp));
    return NORMAL;
}

PP(pp_complement)
{
    dSP; dTARGET; tryAMAGICun(compl); 
    {
      dTOPss;
      if (SvNIOKp(sv)) {
	if (op->op_private & HINT_INTEGER) {
	  IBW value = ~SvIV(sv);
	  SETi(BWi(value));
	}
	else {
	  UBW value = ~SvUV(sv);
	  SETu(BWu(value));
	}
      }
      else {
	register char *tmps;
	register long *tmpl;
	register I32 anum;
	STRLEN len;

	SvSetSV(TARG, sv);
	tmps = SvPV_force(TARG, len);
	anum = len;
#ifdef LIBERAL
	for ( ; anum && (unsigned long)tmps % sizeof(long); anum--, tmps++)
	    *tmps = ~*tmps;
	tmpl = (long*)tmps;
	for ( ; anum >= sizeof(long); anum -= sizeof(long), tmpl++)
	    *tmpl = ~*tmpl;
	tmps = (char*)tmpl;
#endif
	for ( ; anum > 0; anum--, tmps++)
	    *tmps = ~*tmps;

	SETs(TARG);
      }
      RETURN;
    }
}

/* integer versions of some of the above */

PP(pp_i_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left * right );
      RETURN;
    }
}

PP(pp_i_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN); 
    {
      dPOPiv;
      if (value == 0)
	DIE("Illegal division by zero");
      value = POPi / value;
      PUSHi( value );
      RETURN;
    }
}

PP(pp_i_modulo)
{
    dSP; dATARGET; tryAMAGICbin(mod,opASSIGN); 
    {
      dPOPTOPiirl;
      if (!right)
	DIE("Illegal modulus zero");
      SETi( left % right );
      RETURN;
    }
}

PP(pp_i_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left + right );
      RETURN;
    }
}

PP(pp_i_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left - right );
      RETURN;
    }
}

PP(pp_i_lt)
{
    dSP; tryAMAGICbinSET(lt,0); 
    {
      dPOPTOPiirl;
      SETs(boolSV(left < right));
      RETURN;
    }
}

PP(pp_i_gt)
{
    dSP; tryAMAGICbinSET(gt,0); 
    {
      dPOPTOPiirl;
      SETs(boolSV(left > right));
      RETURN;
    }
}

PP(pp_i_le)
{
    dSP; tryAMAGICbinSET(le,0); 
    {
      dPOPTOPiirl;
      SETs(boolSV(left <= right));
      RETURN;
    }
}

PP(pp_i_ge)
{
    dSP; tryAMAGICbinSET(ge,0); 
    {
      dPOPTOPiirl;
      SETs(boolSV(left >= right));
      RETURN;
    }
}

PP(pp_i_eq)
{
    dSP; tryAMAGICbinSET(eq,0); 
    {
      dPOPTOPiirl;
      SETs(boolSV(left == right));
      RETURN;
    }
}

PP(pp_i_ne)
{
    dSP; tryAMAGICbinSET(ne,0); 
    {
      dPOPTOPiirl;
      SETs(boolSV(left != right));
      RETURN;
    }
}

PP(pp_i_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0); 
    {
      dPOPTOPiirl;
      I32 value;

      if (left > right)
	value = 1;
      else if (left < right)
	value = -1;
      else
	value = 0;
      SETi(value);
      RETURN;
    }
}

PP(pp_i_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    SETi(-TOPi);
    RETURN;
}

/* High falutin' math. */

PP(pp_atan2)
{
    dSP; dTARGET; tryAMAGICbin(atan2,0); 
    {
      dPOPTOPnnrl;
      SETn(atan2(left, right));
      RETURN;
    }
}

PP(pp_sin)
{
    dSP; dTARGET; tryAMAGICun(sin);
    {
      double value;
      value = POPn;
      value = sin(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_cos)
{
    dSP; dTARGET; tryAMAGICun(cos);
    {
      double value;
      value = POPn;
      value = cos(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_rand)
{
    dSP; dTARGET;
    double value;
    if (MAXARG < 1)
	value = 1.0;
    else
	value = POPn;
    if (value == 0.0)
	value = 1.0;
    if (!srand_called) {
#if RANDBITS == 48
	(void)srand48((long)seed());
#else
	(void)srand((unsigned)seed());
#endif
	srand_called = TRUE;
    }
#if RANDBITS == 48
    value = drand48() * value;
#else
#if RANDBITS == 31
    value = rand() * value / 2147483648.0;
#else
#if RANDBITS == 16
    value = rand() * value / 65536.0;
#else
#if RANDBITS == 15
    value = rand() * value / 32768.0;
#else
    value = rand() * value / (double)(((unsigned long)1) << RANDBITS);
#endif
#endif
#endif
#endif
    XPUSHn(value);
    RETURN;
}

PP(pp_srand)
{
    dSP;
    UV anum;
    if (MAXARG < 1)
	anum = seed();
    else
	anum = POPu;
#if RANDBITS == 48
    (void)srand48((long)anum);
#else
    (void)srand((unsigned)anum);
#endif
    srand_called = TRUE;
    EXTEND(SP, 1);
    RETPUSHYES;
}

static U32
seed()
{
    /*
     * This is really just a quick hack which grabs various garbage
     * values.  It really should be a real hash algorithm which
     * spreads the effect of every input bit onto every output bit,
     * if someone who knows about such tings would bother to write it.
     * Might be a good idea to add that function to CORE as well.
     * No numbers below come from careful analysis or anyting here,
     * except they are primes and SEED_C1 > 1E6 to get a full-width
     * value from (tv_sec * SEED_C1 + tv_usec).  The multipliers should
     * probably be bigger too.
     */
#if RANDBITS > 16
#  define SEED_C1	1000003
#define   SEED_C4	73819
#else
#  define SEED_C1	25747
#define   SEED_C4	20639
#endif
#define   SEED_C2	3
#define   SEED_C3	269
#define   SEED_C5	26107

    U32 u;
#ifdef VMS
#  include <starlet.h>
    /* when[] = (low 32 bits, high 32 bits) of time since epoch
     * in 100-ns units, typically incremented ever 10 ms.        */
    unsigned int when[2];
    _ckvmssts(sys$gettim(when));
    u = (U32)SEED_C1 * when[0] + (U32)SEED_C2 * when[1];
#else
#  ifdef HAS_GETTIMEOFDAY
    struct timeval when;
    gettimeofday(&when,(struct timezone *) 0);
    u = (U32)SEED_C1 * when.tv_sec + (U32)SEED_C2 * when.tv_usec;
#  else
    Time_t when;
    (void)time(&when);
    u = (U32)SEED_C1 * when;
#  endif
#endif
    u += SEED_C3 * (U32)getpid();
    u += SEED_C4 * (U32)(UV)stack_sp;
#ifndef PLAN9           /* XXX Plan9 assembler chokes on this; fix needed  */
    u += SEED_C5 * (U32)(UV)&when;
#endif
    return u;
}

PP(pp_exp)
{
    dSP; dTARGET; tryAMAGICun(exp);
    {
      double value;
      value = POPn;
      value = exp(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_log)
{
    dSP; dTARGET; tryAMAGICun(log);
    {
      double value;
      value = POPn;
      if (value <= 0.0) {
	SET_NUMERIC_STANDARD();
	DIE("Can't take log of %g", value);
      }
      value = log(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_sqrt)
{
    dSP; dTARGET; tryAMAGICun(sqrt);
    {
      double value;
      value = POPn;
      if (value < 0.0) {
	SET_NUMERIC_STANDARD();
	DIE("Can't take sqrt of %g", value);
      }
      value = sqrt(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_int)
{
    dSP; dTARGET;
    {
      double value = TOPn;
      IV iv;

      if (SvIOKp(TOPs) && !SvNOKp(TOPs) && !SvPOKp(TOPs)) {
	iv = SvIVX(TOPs);
	SETi(iv);
      }
      else {
	if (value >= 0.0)
	  (void)modf(value, &value);
	else {
	  (void)modf(-value, &value);
	  value = -value;
	}
	iv = I_V(value);
	if (iv == value)
	  SETi(iv);
	else
	  SETn(value);
      }
    }
    RETURN;
}

PP(pp_abs)
{
    dSP; dTARGET; tryAMAGICun(abs);
    {
      double value = TOPn;
      IV iv;

      if (SvIOKp(TOPs) && !SvNOKp(TOPs) && !SvPOKp(TOPs) &&
	  (iv = SvIVX(TOPs)) != IV_MIN) {
	if (iv < 0)
	  iv = -iv;
	SETi(iv);
      }
      else {
	if (value < 0.0)
	    value = -value;
	SETn(value);
      }
    }
    RETURN;
}

PP(pp_hex)
{
    dSP; dTARGET;
    char *tmps;
    I32 argtype;

    tmps = POPp;
    XPUSHu(scan_hex(tmps, 99, &argtype));
    RETURN;
}

PP(pp_oct)
{
    dSP; dTARGET;
    UV value;
    I32 argtype;
    char *tmps;

    tmps = POPp;
    while (*tmps && isSPACE(*tmps))
	tmps++;
    if (*tmps == '0')
	tmps++;
    if (*tmps == 'x')
	value = scan_hex(++tmps, 99, &argtype);
    else
	value = scan_oct(tmps, 99, &argtype);
    XPUSHu(value);
    RETURN;
}

/* String stuff. */

PP(pp_length)
{
    dSP; dTARGET;
    SETi( sv_len(TOPs) );
    RETURN;
}

PP(pp_substr)
{
    dSP; dTARGET;
    SV *sv;
    I32 len;
    STRLEN curlen;
    I32 pos;
    I32 rem;
    I32 fail;
    I32 lvalue = op->op_flags & OPf_MOD;
    char *tmps;
    I32 arybase = curcop->cop_arybase;

    if (MAXARG > 2)
	len = POPi;
    pos = POPi;
    sv = POPs;
    tmps = SvPV(sv, curlen);
    if (pos >= arybase) {
	pos -= arybase;
	rem = curlen-pos;
	fail = rem;
        if (MAXARG > 2) {
            if (len < 0) {
	        rem += len;
                if (rem < 0)
                    rem = 0;
            }
            else if (rem > len)
                     rem = len;
        }
    }
    else {
        pos += curlen;
        if (MAXARG < 3)
            rem = curlen;
        else if (len >= 0) {
            rem = pos+len;
            if (rem > (I32)curlen)
                rem = curlen;
        }
        else {
            rem = curlen+len;
            if (rem < pos)
                rem = pos;
        }
        if (pos < 0)
            pos = 0;
        fail = rem;
        rem -= pos;
    }
    if (fail < 0) {
	if (dowarn || lvalue) 
	    warn("substr outside of string");
	RETPUSHUNDEF;
    }
    else {
	tmps += pos;
	sv_setpvn(TARG, tmps, rem);
	if (lvalue) {			/* it's an lvalue! */
	    if (!SvGMAGICAL(sv)) {
		if (SvROK(sv)) {
		    SvPV_force(sv,na);
		    if (dowarn)
			warn("Attempt to use reference as lvalue in substr");
		}
		if (SvOK(sv))		/* is it defined ? */
		    (void)SvPOK_only(sv);
		else
		    sv_setpvn(sv,"",0);	/* avoid lexical reincarnation */
	    }

	    if (SvTYPE(TARG) < SVt_PVLV) {
		sv_upgrade(TARG, SVt_PVLV);
		sv_magic(TARG, Nullsv, 'x', Nullch, 0);
	    }

	    LvTYPE(TARG) = 'x';
	    LvTARG(TARG) = sv;
	    LvTARGOFF(TARG) = pos;
	    LvTARGLEN(TARG) = rem; 
	}
    }
    PUSHs(TARG);		/* avoid SvSETMAGIC here */
    RETURN;
}

PP(pp_vec)
{
    dSP; dTARGET;
    register I32 size = POPi;
    register I32 offset = POPi;
    register SV *src = POPs;
    I32 lvalue = op->op_flags & OPf_MOD;
    STRLEN srclen;
    unsigned char *s = (unsigned char*)SvPV(src, srclen);
    unsigned long retnum;
    I32 len;

    offset *= size;		/* turn into bit offset */
    len = (offset + size + 7) / 8;
    if (offset < 0 || size < 1)
	retnum = 0;
    else {
	if (lvalue) {                      /* it's an lvalue! */
	    if (SvTYPE(TARG) < SVt_PVLV) {
		sv_upgrade(TARG, SVt_PVLV);
		sv_magic(TARG, Nullsv, 'v', Nullch, 0);
	    }

	    LvTYPE(TARG) = 'v';
	    LvTARG(TARG) = src;
	    LvTARGOFF(TARG) = offset; 
	    LvTARGLEN(TARG) = size; 
	}
	if (len > srclen) {
	    if (size <= 8)
		retnum = 0;
	    else {
		offset >>= 3;
		if (size == 16) {
		    if (offset >= srclen)
			retnum = 0;
		    else
			retnum = (unsigned long) s[offset] << 8;
		}
		else if (size == 32) {
		    if (offset >= srclen)
			retnum = 0;
		    else if (offset + 1 >= srclen)
			retnum = (unsigned long) s[offset] << 24;
		    else if (offset + 2 >= srclen)
			retnum = ((unsigned long) s[offset] << 24) +
			    ((unsigned long) s[offset + 1] << 16);
		    else
			retnum = ((unsigned long) s[offset] << 24) +
			    ((unsigned long) s[offset + 1] << 16) +
			    (s[offset + 2] << 8);
		}
	    }
	}
	else if (size < 8)
	    retnum = (s[offset >> 3] >> (offset & 7)) & ((1 << size) - 1);
	else {
	    offset >>= 3;
	    if (size == 8)
		retnum = s[offset];
	    else if (size == 16)
		retnum = ((unsigned long) s[offset] << 8) + s[offset+1];
	    else if (size == 32)
		retnum = ((unsigned long) s[offset] << 24) +
			((unsigned long) s[offset + 1] << 16) +
			(s[offset + 2] << 8) + s[offset+3];
	}
    }

    sv_setiv(TARG, (IV)retnum);
    PUSHs(TARG);
    RETURN;
}

PP(pp_index)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    STRLEN biglen;
    I32 arybase = curcop->cop_arybase;

    if (MAXARG < 3)
	offset = 0;
    else
	offset = POPi - arybase;
    little = POPs;
    big = POPs;
    tmps = SvPV(big, biglen);
    if (offset < 0)
	offset = 0;
    else if (offset > biglen)
	offset = biglen;
    if (!(tmps2 = fbm_instr((unsigned char*)tmps + offset,
      (unsigned char*)tmps + biglen, little)))
	retval = -1 + arybase;
    else
	retval = tmps2 - tmps + arybase;
    PUSHi(retval);
    RETURN;
}

PP(pp_rindex)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    STRLEN blen;
    STRLEN llen;
    SV *offstr;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    I32 arybase = curcop->cop_arybase;

    if (MAXARG >= 3)
	offstr = POPs;
    little = POPs;
    big = POPs;
    tmps2 = SvPV(little, llen);
    tmps = SvPV(big, blen);
    if (MAXARG < 3)
	offset = blen;
    else
	offset = SvIV(offstr) - arybase + llen;
    if (offset < 0)
	offset = 0;
    else if (offset > blen)
	offset = blen;
    if (!(tmps2 = rninstr(tmps,  tmps  + offset,
			  tmps2, tmps2 + llen)))
	retval = -1 + arybase;
    else
	retval = tmps2 - tmps + arybase;
    PUSHi(retval);
    RETURN;
}

PP(pp_sprintf)
{
    dSP; dMARK; dORIGMARK; dTARGET;
#ifdef USE_LOCALE_NUMERIC
    if (op->op_private & OPpLOCALE)
	SET_NUMERIC_LOCAL();
    else
	SET_NUMERIC_STANDARD();
#endif
    do_sprintf(TARG, SP-MARK, MARK+1);
    TAINT_IF(SvTAINTED(TARG));
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

PP(pp_ord)
{
    dSP; dTARGET;
    I32 value;
    char *tmps;

#ifndef I286
    tmps = POPp;
    value = (I32) (*tmps & 255);
#else
    I32 anum;
    tmps = POPp;
    anum = (I32) *tmps;
    value = (I32) (anum & 255);
#endif
    XPUSHi(value);
    RETURN;
}

PP(pp_chr)
{
    dSP; dTARGET;
    char *tmps;

    (void)SvUPGRADE(TARG,SVt_PV);
    SvGROW(TARG,2);
    SvCUR_set(TARG, 1);
    tmps = SvPVX(TARG);
    *tmps++ = POPi;
    *tmps = '\0';
    (void)SvPOK_only(TARG);
    XPUSHs(TARG);
    RETURN;
}

PP(pp_crypt)
{
    dSP; dTARGET; dPOPTOPssrl;
#ifdef HAS_CRYPT
    char *tmps = SvPV(left, na);
#ifdef FCRYPT
    sv_setpv(TARG, fcrypt(tmps, SvPV(right, na)));
#else
    sv_setpv(TARG, crypt(tmps, SvPV(right, na)));
#endif
#else
    DIE(
      "The crypt() function is unimplemented due to excessive paranoia.");
#endif
    SETs(TARG);
    RETURN;
}

PP(pp_ucfirst)
{
    dSP;
    SV *sv = TOPs;
    register char *s;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }
    s = SvPV_force(sv, na);
    if (*s) {
	if (op->op_private & OPpLOCALE) {
	    TAINT;
	    SvTAINTED_on(sv);
	    *s = toUPPER_LC(*s);
	}
	else
	    *s = toUPPER(*s);
    }

    RETURN;
}

PP(pp_lcfirst)
{
    dSP;
    SV *sv = TOPs;
    register char *s;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }
    s = SvPV_force(sv, na);
    if (*s) {
	if (op->op_private & OPpLOCALE) {
	    TAINT;
	    SvTAINTED_on(sv);
	    *s = toLOWER_LC(*s);
	}
	else
	    *s = toLOWER(*s);
    }

    SETs(sv);
    RETURN;
}

PP(pp_uc)
{
    dSP;
    SV *sv = TOPs;
    register char *s;
    STRLEN len;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }

    s = SvPV_force(sv, len);
    if (len) {
	register char *send = s + len;

	if (op->op_private & OPpLOCALE) {
	    TAINT;
	    SvTAINTED_on(sv);
	    for (; s < send; s++)
		*s = toUPPER_LC(*s);
	}
	else {
	    for (; s < send; s++)
		*s = toUPPER(*s);
	}
    }
    RETURN;
}

PP(pp_lc)
{
    dSP;
    SV *sv = TOPs;
    register char *s;
    STRLEN len;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }

    s = SvPV_force(sv, len);
    if (len) {
	register char *send = s + len;

	if (op->op_private & OPpLOCALE) {
	    TAINT;
	    SvTAINTED_on(sv);
	    for (; s < send; s++)
		*s = toLOWER_LC(*s);
	}
	else {
	    for (; s < send; s++)
		*s = toLOWER(*s);
	}
    }
    RETURN;
}

PP(pp_quotemeta)
{
    dSP; dTARGET;
    SV *sv = TOPs;
    STRLEN len;
    register char *s = SvPV(sv,len);
    register char *d;

    if (len) {
	(void)SvUPGRADE(TARG, SVt_PV);
	SvGROW(TARG, (len * 2) + 1);
	d = SvPVX(TARG);
	while (len--) {
	    if (!isALNUM(*s))
		*d++ = '\\';
	    *d++ = *s++;
	}
	*d = '\0';
	SvCUR_set(TARG, d - SvPVX(TARG));
	(void)SvPOK_only(TARG);
    }
    else
	sv_setpvn(TARG, s, len);
    SETs(TARG);
    RETURN;
}

/* Arrays. */

PP(pp_aslice)
{
    dSP; dMARK; dORIGMARK;
    register SV** svp;
    register AV* av = (AV*)POPs;
    register I32 lval = op->op_flags & OPf_MOD;
    I32 arybase = curcop->cop_arybase;
    I32 elem;

    if (SvTYPE(av) == SVt_PVAV) {
	if (lval && op->op_private & OPpLVAL_INTRO) {
	    I32 max = -1;
	    for (svp = mark + 1; svp <= sp; svp++) {
		elem = SvIVx(*svp);
		if (elem > max)
		    max = elem;
	    }
	    if (max > AvMAX(av))
		av_extend(av, max);
	}
	while (++MARK <= SP) {
	    elem = SvIVx(*MARK);

	    if (elem > 0)
		elem -= arybase;
	    svp = av_fetch(av, elem, lval);
	    if (lval) {
		if (!svp || *svp == &sv_undef)
		    DIE(no_aelem, elem);
		if (op->op_private & OPpLVAL_INTRO)
		    save_svref(svp);
	    }
	    *MARK = svp ? *svp : &sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* Associative arrays. */

PP(pp_each)
{
    dSP; dTARGET;
    HV *hash = (HV*)POPs;
    HE *entry;
    I32 gimme = GIMME_V;
    
    PUTBACK;
    entry = hv_iternext(hash);		/* might clobber stack_sp */
    SPAGAIN;

    EXTEND(SP, 2);
    if (entry) {
	PUSHs(hv_iterkeysv(entry));	/* won't clobber stack_sp */
	if (gimme == G_ARRAY) {
	    PUTBACK;
	    sv_setsv(TARG, hv_iterval(hash, entry));  /* might hit stack_sp */
	    SPAGAIN;
	    PUSHs(TARG);
	}
    }
    else if (gimme == G_SCALAR)
	RETPUSHUNDEF;

    RETURN;
}

PP(pp_values)
{
    return do_kv(ARGS);
}

PP(pp_keys)
{
    return do_kv(ARGS);
}

PP(pp_delete)
{
    dSP;
    I32 gimme = GIMME_V;
    I32 discard = (gimme == G_VOID) ? G_DISCARD : 0;
    SV *sv;
    HV *hv;

    if (op->op_private & OPpSLICE) {
	dMARK; dORIGMARK;
	hv = (HV*)POPs;
	if (SvTYPE(hv) != SVt_PVHV)
	    DIE("Not a HASH reference");
	while (++MARK <= SP) {
	    sv = hv_delete_ent(hv, *MARK, discard, 0);
	    *MARK = sv ? sv : &sv_undef;
	}
	if (discard)
	    SP = ORIGMARK;
	else if (gimme == G_SCALAR) {
	    MARK = ORIGMARK;
	    *++MARK = *SP;
	    SP = MARK;
	}
    }
    else {
	SV *keysv = POPs;
	hv = (HV*)POPs;
	if (SvTYPE(hv) != SVt_PVHV)
	    DIE("Not a HASH reference");
	sv = hv_delete_ent(hv, keysv, discard, 0);
	if (!sv)
	    sv = &sv_undef;
	if (!discard)
	    PUSHs(sv);
    }
    RETURN;
}

PP(pp_exists)
{
    dSP;
    SV *tmpsv = POPs;
    HV *hv = (HV*)POPs;
    STRLEN len;
    if (SvTYPE(hv) != SVt_PVHV) {
	DIE("Not a HASH reference");
    }
    if (hv_exists_ent(hv, tmpsv, 0))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_hslice)
{
    dSP; dMARK; dORIGMARK;
    register HE *he;
    register HV *hv = (HV*)POPs;
    register I32 lval = op->op_flags & OPf_MOD;

    if (SvTYPE(hv) == SVt_PVHV) {
	while (++MARK <= SP) {
	    SV *keysv = *MARK;

	    he = hv_fetch_ent(hv, keysv, lval, 0);
	    if (lval) {
		if (!he || HeVAL(he) == &sv_undef)
		    DIE(no_helem, SvPV(keysv, na));
		if (op->op_private & OPpLVAL_INTRO)
		    save_svref(&HeVAL(he));
	    }
	    *MARK = he ? HeVAL(he) : &sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* List operators. */

PP(pp_list)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;		/* unwanted list, return last item */
	else
	    *MARK = &sv_undef;
	SP = MARK;
    }
    RETURN;
}

PP(pp_lslice)
{
    dSP;
    SV **lastrelem = stack_sp;
    SV **lastlelem = stack_base + POPMARK;
    SV **firstlelem = stack_base + POPMARK + 1;
    register SV **firstrelem = lastlelem + 1;
    I32 arybase = curcop->cop_arybase;
    I32 lval = op->op_flags & OPf_MOD;
    I32 is_something_there = lval;

    register I32 max = lastrelem - lastlelem;
    register SV **lelem;
    register I32 ix;

    if (GIMME != G_ARRAY) {
	ix = SvIVx(*lastlelem);
	if (ix < 0)
	    ix += max;
	else
	    ix -= arybase;
	if (ix < 0 || ix >= max)
	    *firstlelem = &sv_undef;
	else
	    *firstlelem = firstrelem[ix];
	SP = firstlelem;
	RETURN;
    }

    if (max == 0) {
	SP = firstlelem - 1;
	RETURN;
    }

    for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
	ix = SvIVx(*lelem);
	if (ix < 0) {
	    ix += max;
	    if (ix < 0)
		*lelem = &sv_undef;
	    else if (!(*lelem = firstrelem[ix]))
		*lelem = &sv_undef;
	}
	else {
	    ix -= arybase;
	    if (ix >= max || !(*lelem = firstrelem[ix]))
		*lelem = &sv_undef;
	}
	if (!is_something_there && (SvOK(*lelem) || SvGMAGICAL(*lelem)))
	    is_something_there = TRUE;
    }
    if (is_something_there)
	SP = lastlelem;
    else
	SP = firstlelem - 1;
    RETURN;
}

PP(pp_anonlist)
{
    dSP; dMARK; dORIGMARK;
    I32 items = SP - MARK;
    SV *av = sv_2mortal((SV*)av_make(items, MARK+1));
    SP = ORIGMARK;		/* av_make() might realloc stack_sp */
    XPUSHs(av);
    RETURN;
}

PP(pp_anonhash)
{
    dSP; dMARK; dORIGMARK;
    HV* hv = (HV*)sv_2mortal((SV*)newHV());

    while (MARK < SP) {
	SV* key = *++MARK;
	SV *val = NEWSV(46, 0);
	if (MARK < SP)
	    sv_setsv(val, *++MARK);
	else if (dowarn)
	    warn("Odd number of elements in hash list");
	(void)hv_store_ent(hv,key,val,0);
    }
    SP = ORIGMARK;
    XPUSHs((SV*)hv);
    RETURN;
}

PP(pp_splice)
{
    dSP; dMARK; dORIGMARK;
    register AV *ary = (AV*)*++MARK;
    register SV **src;
    register SV **dst;
    register I32 i;
    register I32 offset;
    register I32 length;
    I32 newlen;
    I32 after;
    I32 diff;
    SV **tmparyval = 0;

    SP++;

    if (++MARK < SP) {
	offset = i = SvIVx(*MARK);
	if (offset < 0)
	    offset += AvFILL(ary) + 1;
	else
	    offset -= curcop->cop_arybase;
	if (offset < 0)
	    DIE(no_aelem, i);
	if (++MARK < SP) {
	    length = SvIVx(*MARK++);
	    if (length < 0)
		length = 0;
	}
	else
	    length = AvMAX(ary) + 1;		/* close enough to infinity */
    }
    else {
	offset = 0;
	length = AvMAX(ary) + 1;
    }
    if (offset > AvFILL(ary) + 1)
	offset = AvFILL(ary) + 1;
    after = AvFILL(ary) + 1 - (offset + length);
    if (after < 0) {				/* not that much array */
	length += after;			/* offset+length now in array */
	after = 0;
	if (!AvALLOC(ary))
	    av_extend(ary, 0);
    }

    /* At this point, MARK .. SP-1 is our new LIST */

    newlen = SP - MARK;
    diff = newlen - length;
    if (newlen && !AvREAL(ary)) {
	if (AvREIFY(ary))
	    av_reify(ary);
	else
	    assert(AvREAL(ary));		/* would leak, so croak */
    }

    if (diff < 0) {				/* shrinking the area */
	if (newlen) {
	    New(451, tmparyval, newlen, SV*);	/* so remember insertion */
	    Copy(MARK, tmparyval, newlen, SV*);
	}

	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    MEXTEND(MARK, length);
	    Copy(AvARRAY(ary)+offset, MARK, length, SV*);
	    if (AvREAL(ary)) {
		EXTEND_MORTAL(length);
		for (i = length, dst = MARK; i; i--) {
		    if (!SvIMMORTAL(*dst))
			sv_2mortal(*dst);	/* free them eventualy */
		    dst++;
		}
	    }
	    MARK += length - 1;
	}
	else {
	    *MARK = AvARRAY(ary)[offset+length-1];
	    if (AvREAL(ary)) {
		if (!SvIMMORTAL(*MARK))
		    sv_2mortal(*MARK);
		for (i = length - 1, dst = &AvARRAY(ary)[offset]; i > 0; i--)
		    SvREFCNT_dec(*dst++);	/* free them now */
	    }
	}
	AvFILL(ary) += diff;

	/* pull up or down? */

	if (offset < after) {			/* easier to pull up */
	    if (offset) {			/* esp. if nothing to pull */
		src = &AvARRAY(ary)[offset-1];
		dst = src - diff;		/* diff is negative */
		for (i = offset; i > 0; i--)	/* can't trust Copy */
		    *dst-- = *src--;
	    }
	    dst = AvARRAY(ary);
	    SvPVX(ary) = (char*)(AvARRAY(ary) - diff); /* diff is negative */
	    AvMAX(ary) += diff;
	}
	else {
	    if (after) {			/* anything to pull down? */
		src = AvARRAY(ary) + offset + length;
		dst = src + diff;		/* diff is negative */
		Move(src, dst, after, SV*);
	    }
	    dst = &AvARRAY(ary)[AvFILL(ary)+1];
						/* avoid later double free */
	}
	i = -diff;
	while (i)
	    dst[--i] = &sv_undef;
	
	if (newlen) {
	    for (src = tmparyval, dst = AvARRAY(ary) + offset;
	      newlen; newlen--) {
		*dst = NEWSV(46, 0);
		sv_setsv(*dst++, *src++);
	    }
	    Safefree(tmparyval);
	}
    }
    else {					/* no, expanding (or same) */
	if (length) {
	    New(452, tmparyval, length, SV*);	/* so remember deletion */
	    Copy(AvARRAY(ary)+offset, tmparyval, length, SV*);
	}

	if (diff > 0) {				/* expanding */

	    /* push up or down? */

	    if (offset < after && diff <= AvARRAY(ary) - AvALLOC(ary)) {
		if (offset) {
		    src = AvARRAY(ary);
		    dst = src - diff;
		    Move(src, dst, offset, SV*);
		}
		SvPVX(ary) = (char*)(AvARRAY(ary) - diff);/* diff is positive */
		AvMAX(ary) += diff;
		AvFILL(ary) += diff;
	    }
	    else {
		if (AvFILL(ary) + diff >= AvMAX(ary))	/* oh, well */
		    av_extend(ary, AvFILL(ary) + diff);
		AvFILL(ary) += diff;

		if (after) {
		    dst = AvARRAY(ary) + AvFILL(ary);
		    src = dst - diff;
		    for (i = after; i; i--) {
			*dst-- = *src--;
		    }
		}
	    }
	}

	for (src = MARK, dst = AvARRAY(ary) + offset; newlen; newlen--) {
	    *dst = NEWSV(46, 0);
	    sv_setsv(*dst++, *src++);
	}
	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    if (length) {
		Copy(tmparyval, MARK, length, SV*);
		if (AvREAL(ary)) {
		    EXTEND_MORTAL(length);
		    for (i = length, dst = MARK; i; i--) {
			if (!SvIMMORTAL(*dst))
			    sv_2mortal(*dst);	/* free them eventualy */
			dst++;
		    }
		}
		Safefree(tmparyval);
	    }
	    MARK += length - 1;
	}
	else if (length--) {
	    *MARK = tmparyval[length];
	    if (AvREAL(ary)) {
		if (!SvIMMORTAL(*MARK))
		    sv_2mortal(*MARK);
		while (length-- > 0)
		    SvREFCNT_dec(tmparyval[length]);
	    }
	    Safefree(tmparyval);
	}
	else
	    *MARK = &sv_undef;
    }
    SP = MARK;
    RETURN;
}

PP(pp_push)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv = &sv_undef;

    for (++MARK; MARK <= SP; MARK++) {
	sv = NEWSV(51, 0);
	if (*MARK)
	    sv_setsv(sv, *MARK);
	av_push(ary, sv);
    }
    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_pop)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_pop(av);
    if (!SvIMMORTAL(sv) && AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_shift)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_shift(av);
    EXTEND(SP, 1);
    if (!sv)
	RETPUSHUNDEF;
    if (!SvIMMORTAL(sv) && AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_unshift)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv;
    register I32 i = 0;

    av_unshift(ary, SP - MARK);
    while (MARK < SP) {
	sv = NEWSV(27, 0);
	sv_setsv(sv, *++MARK);
	(void)av_store(ary, i++, sv);
    }

    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_reverse)
{
    dSP; dMARK;
    register SV *tmp;
    SV **oldsp = SP;

    if (GIMME == G_ARRAY) {
	MARK++;
	while (MARK < SP) {
	    tmp = *MARK;
	    *MARK++ = *SP;
	    *SP-- = tmp;
	}
	SP = oldsp;
    }
    else {
	register char *up;
	register char *down;
	register I32 tmp;
	dTARGET;
	STRLEN len;

	if (SP - MARK > 1)
	    do_join(TARG, &sv_no, MARK, SP);
	else
	    sv_setsv(TARG, (SP > MARK) ? *SP : GvSV(defgv));
	up = SvPV_force(TARG, len);
	if (len > 1) {
	    down = SvPVX(TARG) + len - 1;
	    while (down > up) {
		tmp = *up;
		*up++ = *down;
		*down-- = tmp;
	    }
	    (void)SvPOK_only(TARG);
	}
	SP = MARK + 1;
	SETTARG;
    }
    RETURN;
}

static SV      *
mul128(sv, m)
     SV             *sv;
     U8              m;
{
  STRLEN          len;
  char           *s = SvPV(sv, len);
  char           *t;
  U32             i = 0;

  if (!strnEQ(s, "0000", 4)) {  /* need to grow sv */
    SV             *new = newSVpv("0000000000", 10);

    sv_catsv(new, sv);
    SvREFCNT_dec(sv);		/* free old sv */
    sv = new;
    s = SvPV(sv, len);
  }
  t = s + len - 1;
  while (!*t)                   /* trailing '\0'? */
    t--;
  while (t > s) {
    i = ((*t - '0') << 7) + m;
    *(t--) = '0' + (i % 10);
    m = i / 10;
  }
  return (sv);
}

/* Explosives and implosives. */

PP(pp_unpack)
{
    dSP;
    dPOPPOPssrl;
    SV **oldsp = sp;
    I32 gimme = GIMME_V;
    SV *sv;
    STRLEN llen;
    STRLEN rlen;
    register char *pat = SvPV(left, llen);
    register char *s = SvPV(right, rlen);
    char *strend = s + rlen;
    char *strbeg = s;
    register char *patend = pat + llen;
    I32 datumtype;
    register I32 len;
    register I32 bits;

    /* These must not be in registers: */
    I16 ashort;
    int aint;
    I32 along;
#ifdef HAS_QUAD
    Quad_t aquad;
#endif
    U16 aushort;
    unsigned int auint;
    U32 aulong;
#ifdef HAS_QUAD
    unsigned Quad_t auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    I32 checksum = 0;
    register U32 culong;
    double cdouble;
    static char* bitcount = 0;
    int commas = 0;

    if (gimme != G_ARRAY) {		/* arrange to do first one only */
	/*SUPPRESS 530*/
	for (patend = pat; !isALPHA(*patend) || *patend == 'x'; patend++) ;
	if (strchr("aAbBhHP", *patend) || *pat == '%') {
	    patend++;
	    while (isDIGIT(*patend) || *patend == '*')
		patend++;
	}
	else
	    patend++;
    }
    while (pat < patend) {
      reparse:
	datumtype = *pat++ & 0xFF;
	if (isSPACE(datumtype))
	    continue;
	if (pat >= patend)
	    len = 1;
	else if (*pat == '*') {
	    len = strend - strbeg;	/* long enough */
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat))
		len = (len * 10) + (*pat++ - '0');
	}
	else
	    len = (datumtype != '@');
	switch(datumtype) {
	default:
	    croak("Invalid type in unpack: '%c'", (int)datumtype);
	case ',': /* grandfather in commas but with a warning */
	    if (commas++ == 0 && dowarn)
		warn("Invalid type in unpack: '%c'", (int)datumtype);
	    break;
	case '%':
	    if (len == 1 && pat[-1] != '1')
		len = 16;
	    checksum = len;
	    culong = 0;
	    cdouble = 0;
	    if (pat < patend)
		goto reparse;
	    break;
	case '@':
	    if (len > strend - strbeg)
		DIE("@ outside of string");
	    s = strbeg + len;
	    break;
	case 'X':
	    if (len > s - strbeg)
		DIE("X outside of string");
	    s -= len;
	    break;
	case 'x':
	    if (len > strend - s)
		DIE("x outside of string");
	    s += len;
	    break;
	case 'A':
	case 'a':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum)
		goto uchar_checksum;
	    sv = NEWSV(35, len);
	    sv_setpvn(sv, s, len);
	    s += len;
	    if (datumtype == 'A') {
		aptr = s;	/* borrow register */
		s = SvPVX(sv) + len - 1;
		while (s >= SvPVX(sv) && (!*s || isSPACE(*s)))
		    s--;
		*++s = '\0';
		SvCUR_set(sv, s - SvPVX(sv));
		s = aptr;	/* unborrow register */
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'B':
	case 'b':
	    if (pat[-1] == '*' || len > (strend - s) * 8)
		len = (strend - s) * 8;
	    if (checksum) {
		if (!bitcount) {
		    Newz(601, bitcount, 256, char);
		    for (bits = 1; bits < 256; bits++) {
			if (bits & 1)	bitcount[bits]++;
			if (bits & 2)	bitcount[bits]++;
			if (bits & 4)	bitcount[bits]++;
			if (bits & 8)	bitcount[bits]++;
			if (bits & 16)	bitcount[bits]++;
			if (bits & 32)	bitcount[bits]++;
			if (bits & 64)	bitcount[bits]++;
			if (bits & 128)	bitcount[bits]++;
		    }
		}
		while (len >= 8) {
		    culong += bitcount[*(unsigned char*)s++];
		    len -= 8;
		}
		if (len) {
		    bits = *s;
		    if (datumtype == 'b') {
			while (len-- > 0) {
			    if (bits & 1) culong++;
			    bits >>= 1;
			}
		    }
		    else {
			while (len-- > 0) {
			    if (bits & 128) culong++;
			    bits <<= 1;
			}
		    }
		}
		break;
	    }
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    aptr = pat;			/* borrow register */
	    pat = SvPVX(sv);
	    if (datumtype == 'b') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)		/*SUPPRESS 595*/
			bits >>= 1;
		    else
			bits = *s++;
		    *pat++ = '0' + (bits & 1);
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)
			bits <<= 1;
		    else
			bits = *s++;
		    *pat++ = '0' + ((bits & 128) != 0);
		}
	    }
	    *pat = '\0';
	    pat = aptr;			/* unborrow register */
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'H':
	case 'h':
	    if (pat[-1] == '*' || len > (strend - s) * 2)
		len = (strend - s) * 2;
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    aptr = pat;			/* borrow register */
	    pat = SvPVX(sv);
	    if (datumtype == 'h') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits >>= 4;
		    else
			bits = *s++;
		    *pat++ = hexdigit[bits & 15];
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits <<= 4;
		    else
			bits = *s++;
		    *pat++ = hexdigit[(bits >> 4) & 15];
		}
	    }
	    *pat = '\0';
	    pat = aptr;			/* unborrow register */
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'c':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    culong += aint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    sv = NEWSV(36, 0);
		    sv_setiv(sv, (IV)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'C':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
	      uchar_checksum:
		while (len-- > 0) {
		    auint = *s++ & 255;
		    culong += auint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    auint = *s++ & 255;
		    sv = NEWSV(37, 0);
		    sv_setiv(sv, (IV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 's':
	    along = (strend - s) / SIZE16;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPY16(s, &ashort);
		    s += SIZE16;
		    culong += ashort;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPY16(s, &ashort);
		    s += SIZE16;
		    sv = NEWSV(38, 0);
		    sv_setiv(sv, (IV)ashort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'v':
	case 'n':
	case 'S':
	    along = (strend - s) / SIZE16;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPY16(s, &aushort);
		    s += SIZE16;
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    culong += aushort;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPY16(s, &aushort);
		    s += SIZE16;
		    sv = NEWSV(39, 0);
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    sv_setiv(sv, (IV)aushort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'i':
	    along = (strend - s) / sizeof(int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    if (checksum > 32)
			cdouble += (double)aint;
		    else
			culong += aint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    sv = NEWSV(40, 0);
		    sv_setiv(sv, (IV)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'I':
	    along = (strend - s) / sizeof(unsigned int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    if (checksum > 32)
			cdouble += (double)auint;
		    else
			culong += auint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    sv = NEWSV(41, 0);
		    sv_setuv(sv, (UV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'l':
	    along = (strend - s) / SIZE32;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPY32(s, &along);
		    s += SIZE32;
		    if (checksum > 32)
			cdouble += (double)along;
		    else
			culong += along;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPY32(s, &along);
		    s += SIZE32;
		    sv = NEWSV(42, 0);
		    sv_setiv(sv, (IV)along);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'V':
	case 'N':
	case 'L':
	    along = (strend - s) / SIZE32;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPY32(s, &aulong);
		    s += SIZE32;
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    if (checksum > 32)
			cdouble += (double)aulong;
		    else
			culong += aulong;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPY32(s, &aulong);
		    s += SIZE32;
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    sv = NEWSV(43, 0);
		    sv_setuv(sv, (UV)aulong);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'p':
	    along = (strend - s) / sizeof(char*);
	    if (len > along)
		len = along;
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (sizeof(char*) > strend - s)
		    break;
		else {
		    Copy(s, &aptr, 1, char*);
		    s += sizeof(char*);
		}
		sv = NEWSV(44, 0);
		if (aptr)
		    sv_setpv(sv, aptr);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'w':
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    { 
		UV auv = 0;
		U32 bytes = 0;
		
		while ((len > 0) && (s < strend)) {
		    auv = (auv << 7) | (*s & 0x7f);
		    if (!(*s++ & 0x80)) {
			bytes = 0;
			sv = NEWSV(40, 0);
			sv_setuv(sv, auv);
			PUSHs(sv_2mortal(sv));
			len--;
			auv = 0;
		    }
		    else if (++bytes >= sizeof(UV)) {	/* promote to string */
			char *t;

			sv = newSVpvf("%.*Vu", (int)TYPE_DIGITS(UV), auv);
			while (s < strend) {
			    sv = mul128(sv, *s & 0x7f);
			    if (!(*s++ & 0x80)) {
				bytes = 0;
				break;
			    }
			}
			t = SvPV(sv, na);
			while (*t == '0')
			    t++;
			sv_chop(sv, t);
			PUSHs(sv_2mortal(sv));
			len--;
			auv = 0;
		    }
		}
		if ((s >= strend) && bytes)
		    croak("Unterminated compressed integer");
	    }
	    break;
	case 'P':
	    EXTEND(SP, 1);
	    if (sizeof(char*) > strend - s)
		break;
	    else {
		Copy(s, &aptr, 1, char*);
		s += sizeof(char*);
	    }
	    sv = NEWSV(44, 0);
	    if (aptr)
		sv_setpvn(sv, aptr, len);
	    PUSHs(sv_2mortal(sv));
	    break;
#ifdef HAS_QUAD
	case 'q':
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (s + sizeof(Quad_t) > strend)
		    aquad = 0;
		else {
		    Copy(s, &aquad, 1, Quad_t);
		    s += sizeof(Quad_t);
		}
		sv = NEWSV(42, 0);
		if (aquad >= IV_MIN && aquad <= IV_MAX)
		    sv_setiv(sv, (IV)aquad);
		else
		    sv_setnv(sv, (double)aquad);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'Q':
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (s + sizeof(unsigned Quad_t) > strend)
		    auquad = 0;
		else {
		    Copy(s, &auquad, 1, unsigned Quad_t);
		    s += sizeof(unsigned Quad_t);
		}
		sv = NEWSV(43, 0);
		if (aquad <= UV_MAX)
		    sv_setuv(sv, (UV)auquad);
		else
		    sv_setnv(sv, (double)auquad);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
#endif
	/* float and double added gnb@melba.bby.oz.au 22/11/89 */
	case 'f':
	case 'F':
	    along = (strend - s) / sizeof(float);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    cdouble += afloat;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    sv = NEWSV(47, 0);
		    sv_setnv(sv, (double)afloat);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'd':
	case 'D':
	    along = (strend - s) / sizeof(double);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    cdouble += adouble;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    sv = NEWSV(48, 0);
		    sv_setnv(sv, (double)adouble);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'u':
	    along = (strend - s) * 3 / 4;
	    sv = NEWSV(42, along);
	    if (along)
		SvPOK_on(sv);
	    while (s < strend && *s > ' ' && *s < 'a') {
		I32 a, b, c, d;
		char hunk[4];

		hunk[3] = '\0';
		len = (*s++ - ' ') & 077;
		while (len > 0) {
		    if (s < strend && *s >= ' ')
			a = (*s++ - ' ') & 077;
		    else
			a = 0;
		    if (s < strend && *s >= ' ')
			b = (*s++ - ' ') & 077;
		    else
			b = 0;
		    if (s < strend && *s >= ' ')
			c = (*s++ - ' ') & 077;
		    else
			c = 0;
		    if (s < strend && *s >= ' ')
			d = (*s++ - ' ') & 077;
		    else
			d = 0;
		    hunk[0] = a << 2 | b >> 4;
		    hunk[1] = b << 4 | c >> 2;
		    hunk[2] = c << 6 | d;
		    sv_catpvn(sv, hunk, len > 3 ? 3 : len);
		    len -= 3;
		}
		if (*s == '\n')
		    s++;
		else if (s[1] == '\n')		/* possible checksum byte */
		    s += 2;
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	}
	if (checksum) {
	    sv = NEWSV(42, 0);
	    if (strchr("fFdD", datumtype) ||
	      (checksum > 32 && strchr("iIlLN", datumtype)) ) {
		double trouble;

		adouble = 1.0;
		while (checksum >= 16) {
		    checksum -= 16;
		    adouble *= 65536.0;
		}
		while (checksum >= 4) {
		    checksum -= 4;
		    adouble *= 16.0;
		}
		while (checksum--)
		    adouble *= 2.0;
		along = (1 << checksum) - 1;
		while (cdouble < 0.0)
		    cdouble += adouble;
		cdouble = modf(cdouble / adouble, &trouble) * adouble;
		sv_setnv(sv, cdouble);
	    }
	    else {
		if (checksum < 32) {
		    aulong = (1 << checksum) - 1;
		    culong &= aulong;
		}
		sv_setuv(sv, (UV)culong);
	    }
	    XPUSHs(sv_2mortal(sv));
	    checksum = 0;
	}
    }
    if (sp == oldsp && gimme == G_SCALAR)
	PUSHs(&sv_undef);
    RETURN;
}

static void
doencodes(sv, s, len)
register SV *sv;
register char *s;
register I32 len;
{
    char hunk[5];

    *hunk = len + ' ';
    sv_catpvn(sv, hunk, 1);
    hunk[4] = '\0';
    while (len > 0) {
	hunk[0] = ' ' + (077 & (*s >> 2));
	hunk[1] = ' ' + (077 & ((*s << 4) & 060 | (s[1] >> 4) & 017));
	hunk[2] = ' ' + (077 & ((s[1] << 2) & 074 | (s[2] >> 6) & 03));
	hunk[3] = ' ' + (077 & (s[2] & 077));
	sv_catpvn(sv, hunk, 4);
	s += 3;
	len -= 3;
    }
    for (s = SvPVX(sv); *s; s++) {
	if (*s == ' ')
	    *s = '`';
    }
    sv_catpvn(sv, "\n", 1);
}

static SV      *
is_an_int(s, l)
     char           *s;
     STRLEN          l;
{
  SV             *result = newSVpv("", l);
  char           *result_c = SvPV(result, na);	/* convenience */
  char           *out = result_c;
  bool            skip = 1;
  bool            ignore = 0;

  while (*s) {
    switch (*s) {
    case ' ':
      break;
    case '+':
      if (!skip) {
	SvREFCNT_dec(result);
	return (NULL);
      }
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      skip = 0;
      if (!ignore) {
	*(out++) = *s;
      }
      break;
    case '.':
      ignore = 1;
      break;
    default:
      SvREFCNT_dec(result);
      return (NULL);
    }
    s++;
  }
  *(out++) = '\0';
  SvCUR_set(result, out - result_c);
  return (result);
}

static int
div128(pnum, done)
     SV             *pnum;		    /* must be '\0' terminated */
     bool           *done;
{
  STRLEN          len;
  char           *s = SvPV(pnum, len);
  int             m = 0;
  int             r = 0;
  char           *t = s;

  *done = 1;
  while (*t) {
    int             i;

    i = m * 10 + (*t - '0');
    m = i & 0x7F;
    r = (i >> 7);		/* r < 10 */
    if (r) {
      *done = 0;
    }
    *(t++) = '0' + r;
  }
  *(t++) = '\0';
  SvCUR_set(pnum, (STRLEN) (t - s));
  return (m);
}


PP(pp_pack)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register SV *cat = TARG;
    register I32 items;
    STRLEN fromlen;
    register char *pat = SvPVx(*++MARK, fromlen);
    register char *patend = pat + fromlen;
    register I32 len;
    I32 datumtype;
    SV *fromstr;
    /*SUPPRESS 442*/
    static char null10[] = {0,0,0,0,0,0,0,0,0,0};
    static char *space10 = "          ";

    /* These must not be in registers: */
    char achar;
    I16 ashort;
    int aint;
    unsigned int auint;
    I32 along;
    U32 aulong;
#ifdef HAS_QUAD
    Quad_t aquad;
    unsigned Quad_t auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    int commas = 0;

    items = SP - MARK;
    MARK++;
    sv_setpvn(cat, "", 0);
    while (pat < patend) {
#define NEXTFROM (items-- > 0 ? *MARK++ : &sv_no)
	datumtype = *pat++ & 0xFF;
	if (isSPACE(datumtype))
	    continue;
	if (*pat == '*') {
	    len = strchr("@Xxu", datumtype) ? 0 : items;
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat))
		len = (len * 10) + (*pat++ - '0');
	}
	else
	    len = 1;
	switch(datumtype) {
	default:
	    croak("Invalid type in pack: '%c'", (int)datumtype);
	case ',': /* grandfather in commas but with a warning */
	    if (commas++ == 0 && dowarn)
		warn("Invalid type in pack: '%c'", (int)datumtype);
	    break;
	case '%':
	    DIE("%% may only be used in unpack");
	case '@':
	    len -= SvCUR(cat);
	    if (len > 0)
		goto grow;
	    len = -len;
	    if (len > 0)
		goto shrink;
	    break;
	case 'X':
	  shrink:
	    if (SvCUR(cat) < len)
		DIE("X outside of string");
	    SvCUR(cat) -= len;
	    *SvEND(cat) = '\0';
	    break;
	case 'x':
	  grow:
	    while (len >= 10) {
		sv_catpvn(cat, null10, 10);
		len -= 10;
	    }
	    sv_catpvn(cat, null10, len);
	    break;
	case 'A':
	case 'a':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    if (pat[-1] == '*')
		len = fromlen;
	    if (fromlen > len)
		sv_catpvn(cat, aptr, len);
	    else {
		sv_catpvn(cat, aptr, fromlen);
		len -= fromlen;
		if (datumtype == 'A') {
		    while (len >= 10) {
			sv_catpvn(cat, space10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, space10, len);
		}
		else {
		    while (len >= 10) {
			sv_catpvn(cat, null10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, null10, len);
		}
	    }
	    break;
	case 'B':
	case 'b':
	    {
		char *savepat = pat;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		aptr = SvPV(fromstr, fromlen);
		if (pat[-1] == '*')
		    len = fromlen;
		pat = aptr;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+7)/8;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'B') {
		    for (len = 0; len++ < aint;) {
			items |= *pat++ & 1;
			if (len & 7)
			    items <<= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (*pat++ & 1)
			    items |= 128;
			if (len & 7)
			    items >>= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 7) {
		    if (datumtype == 'B')
			items <<= 7 - (aint & 7);
		    else
			items >>= 7 - (aint & 7);
		    *aptr++ = items & 0xff;
		}
		pat = SvPVX(cat) + SvCUR(cat);
		while (aptr <= pat)
		    *aptr++ = '\0';

		pat = savepat;
		items = saveitems;
	    }
	    break;
	case 'H':
	case 'h':
	    {
		char *savepat = pat;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		aptr = SvPV(fromstr, fromlen);
		if (pat[-1] == '*')
		    len = fromlen;
		pat = aptr;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+1)/2;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'H') {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*pat))
			    items |= ((*pat++ & 15) + 9) & 15;
			else
			    items |= *pat++ & 15;
			if (len & 1)
			    items <<= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*pat))
			    items |= (((*pat++ & 15) + 9) & 15) << 4;
			else
			    items |= (*pat++ & 15) << 4;
			if (len & 1)
			    items >>= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 1)
		    *aptr++ = items & 0xff;
		pat = SvPVX(cat) + SvCUR(cat);
		while (aptr <= pat)
		    *aptr++ = '\0';

		pat = savepat;
		items = saveitems;
	    }
	    break;
	case 'C':
	case 'c':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		achar = aint;
		sv_catpvn(cat, &achar, sizeof(char));
	    }
	    break;
	/* Float and double added by gnb@melba.bby.oz.au  22/11/89 */
	case 'f':
	case 'F':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		afloat = (float)SvNV(fromstr);
		sv_catpvn(cat, (char *)&afloat, sizeof (float));
	    }
	    break;
	case 'd':
	case 'D':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		adouble = (double)SvNV(fromstr);
		sv_catpvn(cat, (char *)&adouble, sizeof (double));
	    }
	    break;
	case 'n':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTONS
		ashort = htons(ashort);
#endif
		CAT16(cat, &ashort);
	    }
	    break;
	case 'v':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTOVS
		ashort = htovs(ashort);
#endif
		CAT16(cat, &ashort);
	    }
	    break;
	case 'S':
	case 's':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
		CAT16(cat, &ashort);
	    }
	    break;
	case 'I':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = SvUV(fromstr);
		sv_catpvn(cat, (char*)&auint, sizeof(unsigned int));
	    }
	    break;
	case 'w':
            while (len-- > 0) {
		fromstr = NEXTFROM;
		adouble = floor(SvNV(fromstr));

		if (adouble < 0)
		    croak("Cannot compress negative numbers");

		if (
#ifdef BW_BITS
		    adouble <= BW_MASK
#else
#ifdef CXUX_BROKEN_CONSTANT_CONVERT
		    adouble <= UV_MAX_cxux
#else
		    adouble <= UV_MAX
#endif
#endif
		    )
		{
		    char   buf[1 + sizeof(UV)];
		    char  *in = buf + sizeof(buf);
		    UV     auv = U_V(adouble);;

		    do {
			*--in = (auv & 0x7f) | 0x80;
			auv >>= 7;
		    } while (auv);
		    buf[sizeof(buf) - 1] &= 0x7f; /* clear continue bit */
		    sv_catpvn(cat, in, (buf + sizeof(buf)) - in);
		}
		else if (SvPOKp(fromstr)) {  /* decimal string arithmetics */
		    char           *from, *result, *in;
		    SV             *norm;
		    STRLEN          len;
		    bool            done;
            
		    /* Copy string and check for compliance */
		    from = SvPV(fromstr, len);
		    if ((norm = is_an_int(from, len)) == NULL)
			croak("can compress only unsigned integer");

		    New('w', result, len, char);
		    in = result + len;
		    done = FALSE;
		    while (!done)
			*--in = div128(norm, &done) | 0x80;
		    result[len - 1] &= 0x7F; /* clear continue bit */
		    sv_catpvn(cat, in, (result + len) - in);
		    Safefree(result);
		    SvREFCNT_dec(norm);	/* free norm */
                }
		else if (SvNOKp(fromstr)) {
		    char   buf[sizeof(double) * 2];	/* 8/7 <= 2 */
		    char  *in = buf + sizeof(buf);

		    do {
			double next = floor(adouble / 128);
			*--in = (unsigned char)(adouble - (next * 128)) | 0x80;
			if (--in < buf)  /* this cannot happen ;-) */
			    croak ("Cannot compress integer");
			adouble = next;
		    } while (adouble > 0);
		    buf[sizeof(buf) - 1] &= 0x7f; /* clear continue bit */
		    sv_catpvn(cat, in, (buf + sizeof(buf)) - in);
		}
		else
		    croak("Cannot compress non integer");
	    }
            break;
	case 'i':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		sv_catpvn(cat, (char*)&aint, sizeof(int));
	    }
	    break;
	case 'N':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
#ifdef HAS_HTONL
		aulong = htonl(aulong);
#endif
		CAT32(cat, &aulong);
	    }
	    break;
	case 'V':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
#ifdef HAS_HTOVL
		aulong = htovl(aulong);
#endif
		CAT32(cat, &aulong);
	    }
	    break;
	case 'L':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
		CAT32(cat, &aulong);
	    }
	    break;
	case 'l':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		along = SvIV(fromstr);
		CAT32(cat, &along);
	    }
	    break;
#ifdef HAS_QUAD
	case 'Q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auquad = (unsigned Quad_t)SvIV(fromstr);
		sv_catpvn(cat, (char*)&auquad, sizeof(unsigned Quad_t));
	    }
	    break;
	case 'q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aquad = (Quad_t)SvIV(fromstr);
		sv_catpvn(cat, (char*)&aquad, sizeof(Quad_t));
	    }
	    break;
#endif /* HAS_QUAD */
	case 'P':
	    len = 1;		/* assume SV is correct length */
	    /* FALL THROUGH */
	case 'p':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		if (fromstr == &sv_undef)
		    aptr = NULL;
		else {
		    /* XXX better yet, could spirit away the string to
		     * a safe spot and hang on to it until the result
		     * of pack() (and all copies of the result) are
		     * gone.
		     */
		    if (dowarn && (SvTEMP(fromstr) || SvPADTMP(fromstr)))
			warn("Attempt to pack pointer to temporary value");
		    if (SvPOK(fromstr) || SvNIOK(fromstr))
			aptr = SvPV(fromstr,na);
		    else
			aptr = SvPV_force(fromstr,na);
		}
		sv_catpvn(cat, (char*)&aptr, sizeof(char*));
	    }
	    break;
	case 'u':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    SvGROW(cat, fromlen * 4 / 3);
	    if (len <= 1)
		len = 45;
	    else
		len = len / 3 * 3;
	    while (fromlen > 0) {
		I32 todo;

		if (fromlen > len)
		    todo = len;
		else
		    todo = fromlen;
		doencodes(cat, aptr, todo);
		fromlen -= todo;
		aptr += todo;
	    }
	    break;
	}
    }
    SvSETMAGIC(cat);
    SP = ORIGMARK;
    PUSHs(cat);
    RETURN;
}
#undef NEXTFROM

PP(pp_split)
{
    dSP; dTARG;
    AV *ary;
    register I32 limit = POPi;			/* note, negative is forever */
    SV *sv = POPs;
    STRLEN len;
    register char *s = SvPV(sv, len);
    char *strend = s + len;
    register PMOP *pm;
    register REGEXP *rx;
    register SV *dstr;
    register char *m;
    I32 iters = 0;
    I32 maxiters = (strend - s) + 10;
    I32 i;
    char *orig;
    I32 origlimit = limit;
    I32 realarray = 0;
    I32 base;
    AV *oldstack = curstack;
    I32 gimme = GIMME_V;
    I32 oldsave = savestack_ix;

#ifdef DEBUGGING
    Copy(&LvTARGOFF(POPs), &pm, 1, PMOP*);
#else
    pm = (PMOP*)POPs;
#endif
    if (!pm || !s)
	DIE("panic: do_split");
    rx = pm->op_pmregexp;

    TAINT_IF((pm->op_pmflags & PMf_LOCALE) &&
	     (pm->op_pmflags & (PMf_WHITE | PMf_SKIPWHITE)));

    if (pm->op_pmreplroot)
	ary = GvAVn((GV*)pm->op_pmreplroot);
    else if (gimme != G_ARRAY)
	ary = GvAVn(defgv);
    else
	ary = Nullav;
    if (ary && (gimme != G_ARRAY || (pm->op_pmflags & PMf_ONCE))) {
	realarray = 1;
	if (!AvREAL(ary)) {
	    AvREAL_on(ary);
	    for (i = AvFILL(ary); i >= 0; i--)
		AvARRAY(ary)[i] = &sv_undef;	/* don't free mere refs */
	}
	av_extend(ary,0);
	av_clear(ary);
	/* temporarily switch stacks */
	SWITCHSTACK(curstack, ary);
    }
    base = SP - stack_base;
    orig = s;
    if (pm->op_pmflags & PMf_SKIPWHITE) {
	if (pm->op_pmflags & PMf_LOCALE) {
	    while (isSPACE_LC(*s))
		s++;
	}
	else {
	    while (isSPACE(*s))
		s++;
	}
    }
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(multiline);
	multiline = pm->op_pmflags & PMf_MULTILINE;
    }

    if (!limit)
	limit = maxiters + 2;
    if (pm->op_pmflags & PMf_WHITE) {
	while (--limit) {
	    m = s;
	    while (m < strend &&
		   !((pm->op_pmflags & PMf_LOCALE)
		     ? isSPACE_LC(*m) : isSPACE(*m)))
		++m;
	    if (m >= strend)
		break;

	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (!realarray)
		sv_2mortal(dstr);
	    XPUSHs(dstr);

	    s = m + 1;
	    while (s < strend &&
		   ((pm->op_pmflags & PMf_LOCALE)
		    ? isSPACE_LC(*s) : isSPACE(*s)))
		++s;
	}
    }
    else if (strEQ("^", rx->precomp)) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && *m != '\n'; m++) ;
	    m++;
	    if (m >= strend)
		break;
	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (!realarray)
		sv_2mortal(dstr);
	    XPUSHs(dstr);
	    s = m;
	}
    }
    else if (pm->op_pmshort && !rx->nparens) {
	i = SvCUR(pm->op_pmshort);
	if (i == 1) {
	    i = *SvPVX(pm->op_pmshort);
	    while (--limit) {
		/*SUPPRESS 530*/
		for (m = s; m < strend && *m != i; m++) ;
		if (m >= strend)
		    break;
		dstr = NEWSV(30, m-s);
		sv_setpvn(dstr, s, m-s);
		if (!realarray)
		    sv_2mortal(dstr);
		XPUSHs(dstr);
		s = m + 1;
	    }
	}
	else {
#ifndef lint
	    while (s < strend && --limit &&
	      (m=fbm_instr((unsigned char*)s, (unsigned char*)strend,
		    pm->op_pmshort)) )
#endif
	    {
		dstr = NEWSV(31, m-s);
		sv_setpvn(dstr, s, m-s);
		if (!realarray)
		    sv_2mortal(dstr);
		XPUSHs(dstr);
		s = m + i;
	    }
	}
    }
    else {
	maxiters += (strend - s) * rx->nparens;
	while (s < strend && --limit &&
	       pregexec(rx, s, strend, orig, 1, Nullsv, TRUE))
	{
	    TAINT_IF(rx->exec_tainted);
	    if (rx->subbase
	      && rx->subbase != orig) {
		m = s;
		s = orig;
		orig = rx->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0];
	    dstr = NEWSV(32, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (!realarray)
		sv_2mortal(dstr);
	    XPUSHs(dstr);
	    if (rx->nparens) {
		for (i = 1; i <= rx->nparens; i++) {
		    s = rx->startp[i];
		    m = rx->endp[i];
		    if (m && s) {
			dstr = NEWSV(33, m-s);
			sv_setpvn(dstr, s, m-s);
		    }
		    else
			dstr = NEWSV(33, 0);
		    if (!realarray)
			sv_2mortal(dstr);
		    XPUSHs(dstr);
		}
	    }
	    s = rx->endp[0];
	}
    }
    LEAVE_SCOPE(oldsave);
    iters = (SP - stack_base) - base;
    if (iters > maxiters)
	DIE("Split loop");
    
    /* keep field after final delim? */
    if (s < strend || (iters && origlimit)) {
	dstr = NEWSV(34, strend-s);
	sv_setpvn(dstr, s, strend-s);
	if (!realarray)
	    sv_2mortal(dstr);
	XPUSHs(dstr);
	iters++;
    }
    else if (!origlimit) {
	while (iters > 0 && (!TOPs || !SvANY(TOPs) || SvCUR(TOPs) == 0))
	    iters--, SP--;
    }
    if (realarray) {
	SWITCHSTACK(ary, oldstack);
	if (SvSMAGICAL(ary)) {
	    PUTBACK;
	    mg_set((SV*)ary);
	    SPAGAIN;
	}
	if (gimme == G_ARRAY) {
	    EXTEND(SP, iters);
	    Copy(AvARRAY(ary), SP + 1, iters, SV*);
	    SP += iters;
	    RETURN;
	}
    }
    else {
	if (gimme == G_ARRAY)
	    RETURN;
    }
    if (iters || !pm->op_pmreplroot) {
	GETTARGET;
	PUSHi(iters);
	RETURN;
    }
    RETPUSHUNDEF;
}

