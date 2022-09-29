/*    op.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "You see: Mr. Drogo, he married poor Miss Primula Brandybuck.  She was
 * our Mr. Bilbo's first cousin on the mother's side (her mother being the
 * youngest of the Old Took's daughters); and Mr. Drogo was his second
 * cousin.  So Mr. Frodo is his first *and* second cousin, once removed
 * either way, as the saying is, if you follow me."  --the Gaffer
 */

#include "EXTERN.h"
#include "perl.h"

#define USE_OP_MASK  /* Turned on by default in 5.002beta1h */

#ifdef USE_OP_MASK
/*
 * In the following definition, the ", (OP *) op" is just to make the compiler
 * think the expression is of the right type: croak actually does a Siglongjmp.
 */
#define CHECKOP(type,op) \
    ((op_mask && op_mask[type])					\
     ? ( op_free((OP*)op),					\
	 croak("%s trapped by operation mask", op_desc[type]),	\
	 Nullop )						\
     : (*check[type])((OP*)op))
#else
#define CHECKOP(type,op) (*check[type])(op)
#endif /* USE_OP_MASK */

static I32 list_assignment _((OP *op));
static OP *bad_type _((I32 n, char *t, char *name, OP *kid));
static OP *modkids _((OP *op, I32 type));
static OP *no_fh_allowed _((OP *op));
static bool scalar_mod_type _((OP *op, I32 type));
static OP *scalarboolean _((OP *op));
static OP *too_few_arguments _((OP *op, char* name));
static OP *too_many_arguments _((OP *op, char* name));
static void null _((OP* op));
static PADOFFSET pad_findlex _((char* name, PADOFFSET newoff, U32 seq,
	CV* startcv, I32 cx_ix));

static char*
gv_ename(gv)
GV* gv;
{
    SV* tmpsv = sv_newmortal();
    gv_efullname3(tmpsv, gv, Nullch);
    return SvPV(tmpsv,na);
}

static OP *
no_fh_allowed(op)
OP *op;
{
    yyerror(form("Missing comma after first argument to %s function",
		 op_desc[op->op_type]));
    return op;
}

static OP *
too_few_arguments(op, name)
OP* op;
char* name;
{
    yyerror(form("Not enough arguments for %s", name));
    return op;
}

static OP *
too_many_arguments(op, name)
OP *op;
char* name;
{
    yyerror(form("Too many arguments for %s", name));
    return op;
}

static OP *
bad_type(n, t, name, kid)
I32 n;
char *t;
char *name;
OP *kid;
{
    yyerror(form("Type of arg %d to %s must be %s (not %s)",
		 (int)n, name, t, op_desc[kid->op_type]));
    return op;
}

void
assertref(op)
OP *op;
{
    int type = op->op_type;
    if (type != OP_AELEM && type != OP_HELEM) {
	yyerror(form("Can't use subscript on %s", op_desc[type]));
	if (type == OP_ENTERSUB || type == OP_RV2HV || type == OP_PADHV)
	    warn("(Did you mean $ or @ instead of %c?)\n",
		 type == OP_ENTERSUB ? '&' : '%');
    }
}

/* "register" allocation */

PADOFFSET
pad_allocmy(name)
char *name;
{
    PADOFFSET off;
    SV *sv;

    if (!(isALPHA(name[1]) || name[1] == '_' && (int)strlen(name) > 2)) {
	if (!isPRINT(name[1])) {
	    name[3] = '\0';
	    name[2] = toCTRL(name[1]);
	    name[1] = '^';
	}
	croak("Can't use global %s in \"my\"",name);
    }
    if (dowarn && AvFILL(comppad_name) >= 0) {
	SV **svp = AvARRAY(comppad_name);
	for (off = AvFILL(comppad_name); off > comppad_name_floor; off--) {
	    if ((sv = svp[off])
		&& sv != &sv_undef
		&& SvIVX(sv) == 999999999       /* var is in open scope */
		&& strEQ(name, SvPVX(sv)))
	    {
		warn("\"my\" variable %s masks earlier declaration in same scope", name);
		break;
	    }
	}
    }
    off = pad_alloc(OP_PADSV, SVs_PADMY);
    sv = NEWSV(1102,0);
    sv_upgrade(sv, SVt_PVNV);
    sv_setpv(sv, name);
    av_store(comppad_name, off, sv);
    SvNVX(sv) = (double)999999999;
    SvIVX(sv) = 0;			/* Not yet introduced--see newSTATEOP */
    if (!min_intro_pending)
	min_intro_pending = off;
    max_intro_pending = off;
    if (*name == '@')
	av_store(comppad, off, (SV*)newAV());
    else if (*name == '%')
	av_store(comppad, off, (SV*)newHV());
    SvPADMY_on(curpad[off]);
    return off;
}

static PADOFFSET
#ifndef CAN_PROTOTYPE
pad_findlex(name, newoff, seq, startcv, cx_ix)
char *name;
PADOFFSET newoff;
U32 seq;
CV* startcv;
I32 cx_ix;
#else
pad_findlex(char *name, PADOFFSET newoff, U32 seq, CV* startcv, I32 cx_ix)
#endif
{
    CV *cv;
    I32 off;
    SV *sv;
    register I32 i;
    register CONTEXT *cx;
    int saweval;

    for (cv = startcv; cv; cv = CvOUTSIDE(cv)) {
	AV *curlist = CvPADLIST(cv);
	SV **svp = av_fetch(curlist, 0, FALSE);
	AV *curname;

	if (!svp || *svp == &sv_undef)
	    continue;
	curname = (AV*)*svp;
	svp = AvARRAY(curname);
	for (off = AvFILL(curname); off > 0; off--) {
	    if ((sv = svp[off]) &&
		sv != &sv_undef &&
		seq <= SvIVX(sv) &&
		seq > I_32(SvNVX(sv)) &&
		strEQ(SvPVX(sv), name))
	    {
		I32 depth;
		AV *oldpad;
		SV *oldsv;

		depth = CvDEPTH(cv);
		if (!depth) {
		    if (newoff) {
			if (SvFAKE(sv))
			    continue;
			return 0; /* don't clone from inactive stack frame */
		    }
		    depth = 1;
		}
		oldpad = (AV*)*av_fetch(curlist, depth, FALSE);
		oldsv = *av_fetch(oldpad, off, TRUE);
		if (!newoff) {		/* Not a mere clone operation. */
		    SV *namesv = NEWSV(1103,0);
		    newoff = pad_alloc(OP_PADSV, SVs_PADMY);
		    sv_upgrade(namesv, SVt_PVNV);
		    sv_setpv(namesv, name);
		    av_store(comppad_name, newoff, namesv);
		    SvNVX(namesv) = (double)curcop->cop_seq;
		    SvIVX(namesv) = 999999999;	/* A ref, intro immediately */
		    SvFAKE_on(namesv);		/* A ref, not a real var */
		    if (CvANON(compcv) || SvTYPE(compcv) == SVt_PVFM) {
			/* "It's closures all the way down." */
			CvCLONE_on(compcv);
			if (cv == startcv) {
			    if (CvANON(compcv))
				oldsv = Nullsv; /* no need to keep ref */
			}
			else {
			    CV *bcv;
			    for (bcv = startcv;
				 bcv && bcv != cv && !CvCLONE(bcv);
				 bcv = CvOUTSIDE(bcv)) {
				if (CvANON(bcv))
				    CvCLONE_on(bcv);
				else {
				    if (dowarn && !CvUNIQUE(cv))
					warn(
					  "Variable \"%s\" may be unavailable",
					     name);
				    break;
				}
			    }
			}
		    }
		    else if (!CvUNIQUE(compcv)) {
			if (dowarn && !SvFAKE(sv) && !CvUNIQUE(cv))
			    warn("Variable \"%s\" will not stay shared", name);
		    }
		}
		av_store(comppad, newoff, SvREFCNT_inc(oldsv));
		return newoff;
	    }
	}
    }

    /* Nothing in current lexical context--try eval's context, if any.
     * This is necessary to let the perldb get at lexically scoped variables.
     * XXX This will also probably interact badly with eval tree caching.
     */

    saweval = 0;
    for (i = cx_ix; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	default:
	    if (i == 0 && saweval) {
		seq = cxstack[saweval].blk_oldcop->cop_seq;
		return pad_findlex(name, newoff, seq, main_cv, 0);
	    }
	    break;
	case CXt_EVAL:
	    switch (cx->blk_eval.old_op_type) {
	    case OP_ENTEREVAL:
		saweval = i;
		break;
	    case OP_REQUIRE:
		/* require must have its own scope */
		return 0;
	    }
	    break;
	case CXt_SUB:
	    if (!saweval)
		return 0;
	    cv = cx->blk_sub.cv;
	    if (debstash && CvSTASH(cv) == debstash) {	/* ignore DB'* scope */
		saweval = i;	/* so we know where we were called from */
		continue;
	    }
	    seq = cxstack[saweval].blk_oldcop->cop_seq;
	    return pad_findlex(name, newoff, seq, cv, i-1);
	}
    }

    return 0;
}

PADOFFSET
pad_findmy(name)
char *name;
{
    I32 off;
    I32 pendoff = 0;
    SV *sv;
    SV **svp = AvARRAY(comppad_name);
    U32 seq = cop_seqmax;

    /* The one we're looking for is probably just before comppad_name_fill. */
    for (off = AvFILL(comppad_name); off > 0; off--) {
	if ((sv = svp[off]) &&
	    sv != &sv_undef &&
	    (!SvIVX(sv) ||
	     (seq <= SvIVX(sv) &&
	      seq > I_32(SvNVX(sv)))) &&
	    strEQ(SvPVX(sv), name))
	{
	    if (SvIVX(sv))
		return (PADOFFSET)off;
	    pendoff = off;	/* this pending def. will override import */
	}
    }

    /* See if it's in a nested scope */
    off = pad_findlex(name, 0, seq, CvOUTSIDE(compcv), cxstack_ix);
    if (off) {
	/* If there is a pending local definition, this new alias must die */
	if (pendoff)
	    SvIVX(AvARRAY(comppad_name)[off]) = seq;
	return off;
    }

    return 0;
}

void
pad_leavemy(fill)
I32 fill;
{
    I32 off;
    SV **svp = AvARRAY(comppad_name);
    SV *sv;
    if (min_intro_pending && fill < min_intro_pending) {
	for (off = max_intro_pending; off >= min_intro_pending; off--) {
	    if ((sv = svp[off]) && sv != &sv_undef)
		warn("%s never introduced", SvPVX(sv));
	}
    }
    /* "Deintroduce" my variables that are leaving with this scope. */
    for (off = AvFILL(comppad_name); off > fill; off--) {
	if ((sv = svp[off]) && sv != &sv_undef && SvIVX(sv) == 999999999)
	    SvIVX(sv) = cop_seqmax;
    }
}

PADOFFSET
pad_alloc(optype,tmptype)	
I32 optype;
U32 tmptype;
{
    SV *sv;
    I32 retval;

    if (AvARRAY(comppad) != curpad)
	croak("panic: pad_alloc");
    if (pad_reset_pending)
	pad_reset();
    if (tmptype & SVs_PADMY) {
	do {
	    sv = *av_fetch(comppad, AvFILL(comppad) + 1, TRUE);
	} while (SvPADBUSY(sv));		/* need a fresh one */
	retval = AvFILL(comppad);
    }
    else {
	SV **names = AvARRAY(comppad_name);
	SSize_t names_fill = AvFILL(comppad_name);
	for (;;) {
	    /*
	     * "foreach" index vars temporarily become aliases to non-"my"
	     * values.  Thus we must skip, not just pad values that are
	     * marked as current pad values, but also those with names.
	     */
	    if (++padix <= names_fill &&
		   (sv = names[padix]) && sv != &sv_undef)
		continue;
	    sv = *av_fetch(comppad, padix, TRUE);
	    if (!(SvFLAGS(sv) & (SVs_PADTMP|SVs_PADMY)))
		break;
	}
	retval = padix;
    }
    SvFLAGS(sv) |= tmptype;
    curpad = AvARRAY(comppad);
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad alloc %ld for %s\n", (long) retval, op_name[optype]));
    return (PADOFFSET)retval;
}

SV *
#ifndef CAN_PROTOTYPE
pad_sv(po)
PADOFFSET po;
#else
pad_sv(PADOFFSET po)
#endif /* CAN_PROTOTYPE */
{
    if (!po)
	croak("panic: pad_sv po");
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad sv %lu\n", (unsigned long)po));
    return curpad[po];		/* eventually we'll turn this into a macro */
}

void
#ifndef CAN_PROTOTYPE
pad_free(po)
PADOFFSET po;
#else
pad_free(PADOFFSET po)
#endif /* CAN_PROTOTYPE */
{
    if (!curpad)
	return;
    if (AvARRAY(comppad) != curpad)
	croak("panic: pad_free curpad");
    if (!po)
	croak("panic: pad_free po");
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad free %lu\n", (unsigned long)po));
    if (curpad[po] && !SvIMMORTAL(curpad[po]))
	SvPADTMP_off(curpad[po]);
    if ((I32)po < padix)
	padix = po - 1;
}

void
#ifndef CAN_PROTOTYPE
pad_swipe(po)
PADOFFSET po;
#else
pad_swipe(PADOFFSET po)
#endif /* CAN_PROTOTYPE */
{
    if (AvARRAY(comppad) != curpad)
	croak("panic: pad_swipe curpad");
    if (!po)
	croak("panic: pad_swipe po");
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad swipe %lu\n", (unsigned long)po));
    SvPADTMP_off(curpad[po]);
    curpad[po] = NEWSV(1107,0);
    SvPADTMP_on(curpad[po]);
    if ((I32)po < padix)
	padix = po - 1;
}

void
pad_reset()
{
    register I32 po;

    if (AvARRAY(comppad) != curpad)
	croak("panic: pad_reset curpad");
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad reset\n"));
    if (!tainting) {	/* Can't mix tainted and non-tainted temporaries. */
	for (po = AvMAX(comppad); po > padix_floor; po--) {
	    if (curpad[po] && !SvIMMORTAL(curpad[po]))
		SvPADTMP_off(curpad[po]);
	}
	padix = padix_floor;
    }
    pad_reset_pending = FALSE;
}

/* Destructor */

void
op_free(op)
OP *op;
{
    register OP *kid, *nextkid;

    if (!op || op->op_seq == (U16)-1)
	return;

    if (op->op_flags & OPf_KIDS) {
	for (kid = cUNOP->op_first; kid; kid = nextkid) {
	    nextkid = kid->op_sibling; /* Get before next freeing kid */
	    op_free(kid);
	}
    }

    switch (op->op_type) {
    case OP_NULL:
	op->op_targ = 0;	/* Was holding old type, if any. */
	break;
    case OP_ENTEREVAL:
	op->op_targ = 0;	/* Was holding hints. */
	break;
    default:
	if (!(op->op_flags & OPf_REF) || (check[op->op_type] != ck_ftst))
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_GV:
    case OP_AELEMFAST:
	SvREFCNT_dec(cGVOP->op_gv);
	break;
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	Safefree(cCOP->cop_label);
	SvREFCNT_dec(cCOP->cop_filegv);
	break;
    case OP_CONST:
	SvREFCNT_dec(cSVOP->op_sv);
	break;
    case OP_GOTO:
    case OP_NEXT:
    case OP_LAST:
    case OP_REDO:
	if (op->op_flags & (OPf_SPECIAL|OPf_STACKED|OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_TRANS:
	Safefree(cPVOP->op_pv);
	break;
    case OP_SUBST:
	op_free(cPMOP->op_pmreplroot);
	/* FALL THROUGH */
    case OP_PUSHRE:
    case OP_MATCH:
	pregfree(cPMOP->op_pmregexp);
	SvREFCNT_dec(cPMOP->op_pmshort);
	break;
    }

    if (op->op_targ > 0)
	pad_free(op->op_targ);

    Safefree(op);
}

static void
null(op)
OP* op;
{
    if (op->op_type != OP_NULL && op->op_targ > 0)
	pad_free(op->op_targ);
    op->op_targ = op->op_type;
    op->op_type = OP_NULL;
    op->op_ppaddr = ppaddr[OP_NULL];
}

/* Contextualizers */

#define LINKLIST(o) ((o)->op_next ? (o)->op_next : linklist((OP*)o))

OP *
linklist(op)
OP *op;
{
    register OP *kid;

    if (op->op_next)
	return op->op_next;

    /* establish postfix order */
    if (cUNOP->op_first) {
	op->op_next = LINKLIST(cUNOP->op_first);
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling) {
	    if (kid->op_sibling)
		kid->op_next = LINKLIST(kid->op_sibling);
	    else
		kid->op_next = op;
	}
    }
    else
	op->op_next = op;

    return op->op_next;
}

OP *
scalarkids(op)
OP *op;
{
    OP *kid;
    if (op && op->op_flags & OPf_KIDS) {
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    scalar(kid);
    }
    return op;
}

static OP *
scalarboolean(op)
OP *op;
{
    if (dowarn &&
	op->op_type == OP_SASSIGN && cBINOP->op_first->op_type == OP_CONST) {
	line_t oldline = curcop->cop_line;

	if (copline != NOLINE)
	    curcop->cop_line = copline;
	warn("Found = in conditional, should be ==");
	curcop->cop_line = oldline;
    }
    return scalar(op);
}

OP *
scalar(op)
OP *op;
{
    OP *kid;

    /* assumes no premature commitment */
    if (!op || (op->op_flags & OPf_WANT) || error_count
	 || op->op_type == OP_RETURN)
	return op;

    op->op_flags = (op->op_flags & ~OPf_WANT) | OPf_WANT_SCALAR;

    switch (op->op_type) {
    case OP_REPEAT:
	if (op->op_private & OPpREPEAT_DOLIST)
	    null(((LISTOP*)cBINOP->op_first)->op_first);
	scalar(cBINOP->op_first);
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOP->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalar(kid);
	break;
    case OP_SPLIT:
	if ((kid = ((LISTOP*)op)->op_first) && kid->op_type == OP_PUSHRE) {
	    if (!kPMOP->op_pmreplroot)
		deprecate("implicit split to @_");
	}
	/* FALL THROUGH */
    case OP_MATCH:
    case OP_SUBST:
    case OP_NULL:
    default:
	if (op->op_flags & OPf_KIDS) {
	    for (kid = cUNOP->op_first; kid; kid = kid->op_sibling)
		scalar(kid);
	}
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOP->op_first;
	scalar(kid);
	while (kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		scalar(kid);
	}
	curcop = &compiling;
	break;
    case OP_SCOPE:
    case OP_LINESEQ:
    case OP_LIST:
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		scalar(kid);
	}
	curcop = &compiling;
	break;
    }
    return op;
}

OP *
scalarvoid(op)
OP *op;
{
    OP *kid;
    char* useless = 0;
    SV* sv;

    /* assumes no premature commitment */
    if (!op || (op->op_flags & OPf_WANT) == OPf_WANT_LIST || error_count
	 || op->op_type == OP_RETURN)
	return op;

    op->op_flags = (op->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

    switch (op->op_type) {
    default:
	if (!(opargs[op->op_type] & OA_FOLDCONST))
	    break;
	/* FALL THROUGH */
    case OP_REPEAT:
	if (op->op_flags & OPf_STACKED)
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_WANTARRAY:
    case OP_GV:
    case OP_PADSV:
    case OP_PADAV:
    case OP_PADHV:
    case OP_PADANY:
    case OP_AV2ARYLEN:
    case OP_REF:
    case OP_REFGEN:
    case OP_SREFGEN:
    case OP_DEFINED:
    case OP_HEX:
    case OP_OCT:
    case OP_LENGTH:
    case OP_SUBSTR:
    case OP_VEC:
    case OP_INDEX:
    case OP_RINDEX:
    case OP_SPRINTF:
    case OP_AELEM:
    case OP_AELEMFAST:
    case OP_ASLICE:
    case OP_HELEM:
    case OP_HSLICE:
    case OP_UNPACK:
    case OP_PACK:
    case OP_JOIN:
    case OP_LSLICE:
    case OP_ANONLIST:
    case OP_ANONHASH:
    case OP_SORT:
    case OP_REVERSE:
    case OP_RANGE:
    case OP_FLIP:
    case OP_FLOP:
    case OP_CALLER:
    case OP_FILENO:
    case OP_EOF:
    case OP_TELL:
    case OP_GETSOCKNAME:
    case OP_GETPEERNAME:
    case OP_READLINK:
    case OP_TELLDIR:
    case OP_GETPPID:
    case OP_GETPGRP:
    case OP_GETPRIORITY:
    case OP_TIME:
    case OP_TMS:
    case OP_LOCALTIME:
    case OP_GMTIME:
    case OP_GHBYNAME:
    case OP_GHBYADDR:
    case OP_GHOSTENT:
    case OP_GNBYNAME:
    case OP_GNBYADDR:
    case OP_GNETENT:
    case OP_GPBYNAME:
    case OP_GPBYNUMBER:
    case OP_GPROTOENT:
    case OP_GSBYNAME:
    case OP_GSBYPORT:
    case OP_GSERVENT:
    case OP_GPWNAM:
    case OP_GPWUID:
    case OP_GGRNAM:
    case OP_GGRGID:
    case OP_GETLOGIN:
	if (!(op->op_private & OPpLVAL_INTRO))
	    useless = op_desc[op->op_type];
	break;

    case OP_RV2GV:
    case OP_RV2SV:
    case OP_RV2AV:
    case OP_RV2HV:
	if (!(op->op_private & OPpLVAL_INTRO) &&
		(!op->op_sibling || op->op_sibling->op_type != OP_READLINE))
	    useless = "a variable";
	break;

    case OP_NEXTSTATE:
    case OP_DBSTATE:
	curcop = ((COP*)op);		/* for warning below */
	break;

    case OP_CONST:
	sv = cSVOP->op_sv;
	if (dowarn) {
	    useless = "a constant";
	    if (SvNIOK(sv) && (SvNV(sv) == 0.0 || SvNV(sv) == 1.0))
		useless = 0;
	    else if (SvPOK(sv)) {
		if (strnEQ(SvPVX(sv), "di", 2) ||
		    strnEQ(SvPVX(sv), "ds", 2) ||
		    strnEQ(SvPVX(sv), "ig", 2))
			useless = 0;
	    }
	}
	null(op);		/* don't execute a constant */
	SvREFCNT_dec(sv);	/* don't even remember it */
	break;

    case OP_POSTINC:
	op->op_type = OP_PREINC;		/* pre-increment is faster */
	op->op_ppaddr = ppaddr[OP_PREINC];
	break;

    case OP_POSTDEC:
	op->op_type = OP_PREDEC;		/* pre-decrement is faster */
	op->op_ppaddr = ppaddr[OP_PREDEC];
	break;

    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOP->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;

    case OP_NULL:
	if (op->op_targ == OP_NEXTSTATE || op->op_targ == OP_DBSTATE)
	    curcop = ((COP*)op);		/* for warning below */
	if (op->op_flags & OPf_STACKED)
	    break;
	/* FALL THROUGH */
    case OP_ENTERTRY:
    case OP_ENTER:
    case OP_SCALAR:
	if (!(op->op_flags & OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_SCOPE:
    case OP_LEAVE:
    case OP_LEAVETRY:
    case OP_LEAVELOOP:
    case OP_LINESEQ:
    case OP_LIST:
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;
    case OP_ENTEREVAL:
	scalarkids(op);
	break;
    case OP_REQUIRE:
	/* all requires must return a boolean value */
	op->op_flags &= ~OPf_WANT;
	return scalar(op);
    case OP_SPLIT:
	if ((kid = ((LISTOP*)op)->op_first) && kid->op_type == OP_PUSHRE) {
	    if (!kPMOP->op_pmreplroot)
		deprecate("implicit split to @_");
	}
	break;
    }
    if (useless && dowarn)
	warn("Useless use of %s in void context", useless);
    return op;
}

OP *
listkids(op)
OP *op;
{
    OP *kid;
    if (op && op->op_flags & OPf_KIDS) {
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    list(kid);
    }
    return op;
}

OP *
list(op)
OP *op;
{
    OP *kid;

    /* assumes no premature commitment */
    if (!op || (op->op_flags & OPf_WANT) || error_count
	 || op->op_type == OP_RETURN)
	return op;

    op->op_flags = (op->op_flags & ~OPf_WANT) | OPf_WANT_LIST;

    switch (op->op_type) {
    case OP_FLOP:
    case OP_REPEAT:
	list(cBINOP->op_first);
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOP->op_first->op_sibling; kid; kid = kid->op_sibling)
	    list(kid);
	break;
    default:
    case OP_MATCH:
    case OP_SUBST:
    case OP_NULL:
	if (!(op->op_flags & OPf_KIDS))
	    break;
	if (!op->op_next && cUNOP->op_first->op_type == OP_FLOP) {
	    list(cBINOP->op_first);
	    return gen_constant_list(op);
	}
    case OP_LIST:
	listkids(op);
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOP->op_first;
	list(kid);
	while (kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		list(kid);
	}
	curcop = &compiling;
	break;
    case OP_SCOPE:
    case OP_LINESEQ:
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		list(kid);
	}
	curcop = &compiling;
	break;
    case OP_REQUIRE:
	/* all requires must return a boolean value */
	op->op_flags &= ~OPf_WANT;
	return scalar(op);
    }
    return op;
}

OP *
scalarseq(op)
OP *op;
{
    OP *kid;

    if (op) {
	if (op->op_type == OP_LINESEQ ||
	     op->op_type == OP_SCOPE ||
	     op->op_type == OP_LEAVE ||
	     op->op_type == OP_LEAVETRY)
	{
	    for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling) {
		if (kid->op_sibling) {
		    scalarvoid(kid);
		}
	    }
	    curcop = &compiling;
	}
	op->op_flags &= ~OPf_PARENS;
	if (hints & HINT_BLOCK_SCOPE)
	    op->op_flags |= OPf_PARENS;
    }
    else
	op = newOP(OP_STUB, 0);
    return op;
}

static OP *
modkids(op, type)
OP *op;
I32 type;
{
    OP *kid;
    if (op && op->op_flags & OPf_KIDS) {
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    mod(kid, type);
    }
    return op;
}

static I32 modcount;

OP *
mod(op, type)
OP *op;
I32 type;
{
    OP *kid;
    SV *sv;

    if (!op || error_count)
	return op;

    switch (op->op_type) {
    case OP_UNDEF:
	modcount++;
	return op;
    case OP_CONST:
	if (!(op->op_private & (OPpCONST_ARYBASE)))
	    goto nomod;
	if (eval_start && eval_start->op_type == OP_CONST) {
	    compiling.cop_arybase = (I32)SvIV(((SVOP*)eval_start)->op_sv);
	    eval_start = 0;
	}
	else if (!type) {
	    SAVEI32(compiling.cop_arybase);
	    compiling.cop_arybase = 0;
	}
	else if (type == OP_REFGEN)
	    goto nomod;
	else
	    croak("That use of $[ is unsupported");
	break;
    case OP_STUB:
	if (op->op_flags & OPf_PARENS)
	    break;
	goto nomod;
    case OP_ENTERSUB:
	if ((type == OP_UNDEF || type == OP_REFGEN) &&
	    !(op->op_flags & OPf_STACKED)) {
	    op->op_type = OP_RV2CV;		/* entersub => rv2cv */
	    op->op_ppaddr = ppaddr[OP_RV2CV];
	    assert(cUNOP->op_first->op_type == OP_NULL);
	    null(((LISTOP*)cUNOP->op_first)->op_first);	/* disable pushmark */
	    break;
	}
	/* FALL THROUGH */
    default:
      nomod:
	/* grep, foreach, subcalls, refgen */
	if (type == OP_GREPSTART || type == OP_ENTERSUB || type == OP_REFGEN)
	    break;
	yyerror(form("Can't modify %s in %s",
		     op_desc[op->op_type],
		     type ? op_desc[type] : "local"));
	return op;

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
    case OP_REPEAT:
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_CONCAT:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT:
    case OP_BIT_AND:
    case OP_BIT_XOR:
    case OP_BIT_OR:
    case OP_I_MULTIPLY:
    case OP_I_DIVIDE:
    case OP_I_MODULO:
    case OP_I_ADD:
    case OP_I_SUBTRACT:
	if (!(op->op_flags & OPf_STACKED))
	    goto nomod;
	modcount++;
	break;
	
    case OP_COND_EXPR:
	for (kid = cUNOP->op_first->op_sibling; kid; kid = kid->op_sibling)
	    mod(kid, type);
	break;

    case OP_RV2AV:
    case OP_RV2HV:
	if (!type && cUNOP->op_first->op_type != OP_GV)
	    croak("Can't localize through a reference");
	if (type == OP_REFGEN && op->op_flags & OPf_PARENS) {
	    modcount = 10000;
	    return op;		/* Treat \(@foo) like ordinary list. */
	}
	/* FALL THROUGH */
    case OP_RV2GV:
	if (scalar_mod_type(op, type))
	    goto nomod;
	ref(cUNOP->op_first, op->op_type);
	/* FALL THROUGH */
    case OP_AASSIGN:
    case OP_ASLICE:
    case OP_HSLICE:
    case OP_NEXTSTATE:
    case OP_DBSTATE:
    case OP_REFGEN:
    case OP_CHOMP:
	modcount = 10000;
	break;
    case OP_RV2SV:
	if (!type && cUNOP->op_first->op_type != OP_GV)
	    croak("Can't localize through a reference");
	ref(cUNOP->op_first, op->op_type); 
	/* FALL THROUGH */
    case OP_GV:
    case OP_AV2ARYLEN:
    case OP_SASSIGN:
    case OP_AELEMFAST:
	modcount++;
	break;

    case OP_PADAV:
    case OP_PADHV:
	modcount = 10000;
	if (type == OP_REFGEN && op->op_flags & OPf_PARENS)
	    return op;		/* Treat \(@foo) like ordinary list. */
	if (scalar_mod_type(op, type))
	    goto nomod;
	/* FALL THROUGH */
    case OP_PADSV:
	modcount++;
	if (!type)
	    croak("Can't localize lexical variable %s",
		SvPV(*av_fetch(comppad_name, op->op_targ, 4), na));
	break;

    case OP_PUSHMARK:
	break;
	
    case OP_KEYS:
	if (type != OP_SASSIGN)
	    goto nomod;
	/* FALL THROUGH */
    case OP_POS:
    case OP_VEC:
    case OP_SUBSTR:
	pad_free(op->op_targ);
	op->op_targ = pad_alloc(op->op_type, SVs_PADMY);
	assert(SvTYPE(PAD_SV(op->op_targ)) == SVt_NULL);
	if (op->op_flags & OPf_KIDS)
	    mod(cBINOP->op_first->op_sibling, type);
	break;

    case OP_AELEM:
    case OP_HELEM:
	ref(cBINOP->op_first, op->op_type);
	if (type == OP_ENTERSUB &&
	     !(op->op_private & (OPpLVAL_INTRO | OPpDEREF)))
	    op->op_private |= OPpLVAL_DEFER;
	modcount++;
	break;

    case OP_SCOPE:
    case OP_LEAVE:
    case OP_ENTER:
	if (op->op_flags & OPf_KIDS)
	    mod(cLISTOP->op_last, type);
	break;

    case OP_NULL:
	if (!(op->op_flags & OPf_KIDS))
	    break;
	if (op->op_targ != OP_LIST) {
	    mod(cBINOP->op_first, type);
	    break;
	}
	/* FALL THROUGH */
    case OP_LIST:
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    mod(kid, type);
	break;
    }
    op->op_flags |= OPf_MOD;

    if (type == OP_AASSIGN || type == OP_SASSIGN)
	op->op_flags |= OPf_SPECIAL|OPf_REF;
    else if (!type) {
	op->op_private |= OPpLVAL_INTRO;
	op->op_flags &= ~OPf_SPECIAL;
    }
    else if (type != OP_GREPSTART && type != OP_ENTERSUB)
	op->op_flags |= OPf_REF;
    return op;
}

static bool
scalar_mod_type(op, type)
OP *op;
I32 type;
{
    switch (type) {
    case OP_SASSIGN:
	if (op->op_type == OP_RV2GV)
	    return FALSE;
	/* FALL THROUGH */
    case OP_PREINC:
    case OP_PREDEC:
    case OP_POSTINC:
    case OP_POSTDEC:
    case OP_I_PREINC:
    case OP_I_PREDEC:
    case OP_I_POSTINC:
    case OP_I_POSTDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
    case OP_REPEAT:
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_I_MULTIPLY:
    case OP_I_DIVIDE:
    case OP_I_MODULO:
    case OP_I_ADD:
    case OP_I_SUBTRACT:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT:
    case OP_BIT_AND:
    case OP_BIT_XOR:
    case OP_BIT_OR:
    case OP_CONCAT:
    case OP_SUBST:
    case OP_TRANS:
    case OP_ANDASSIGN:	/* may work later */
    case OP_ORASSIGN:	/* may work later */
	return TRUE;
    default:
	return FALSE;
    }
}

OP *
refkids(op, type)
OP *op;
I32 type;
{
    OP *kid;
    if (op && op->op_flags & OPf_KIDS) {
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    ref(kid, type);
    }
    return op;
}

OP *
ref(op, type)
OP *op;
I32 type;
{
    OP *kid;

    if (!op || error_count)
	return op;

    switch (op->op_type) {
    case OP_ENTERSUB:
	if ((type == OP_DEFINED) &&
	    !(op->op_flags & OPf_STACKED)) {
	    op->op_type = OP_RV2CV;             /* entersub => rv2cv */
	    op->op_ppaddr = ppaddr[OP_RV2CV];
	    assert(cUNOP->op_first->op_type == OP_NULL);
	    null(((LISTOP*)cUNOP->op_first)->op_first);	/* disable pushmark */
	    op->op_flags |= OPf_SPECIAL;
	}
	break;
      
    case OP_COND_EXPR:
	for (kid = cUNOP->op_first->op_sibling; kid; kid = kid->op_sibling)
	    ref(kid, type);
	break;
    case OP_RV2SV:
	ref(cUNOP->op_first, op->op_type);
	/* FALL THROUGH */
    case OP_PADSV:
	if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV) {
	    op->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			       : type == OP_RV2HV ? OPpDEREF_HV
			       : OPpDEREF_SV);
	    op->op_flags |= OPf_MOD;
	}
	break;
      
    case OP_RV2AV:
    case OP_RV2HV:
	op->op_flags |= OPf_REF; 
	/* FALL THROUGH */
    case OP_RV2GV:
	ref(cUNOP->op_first, op->op_type);
	break;

    case OP_PADAV:
    case OP_PADHV:
	op->op_flags |= OPf_REF; 
	break;
      
    case OP_SCALAR:
    case OP_NULL:
	if (!(op->op_flags & OPf_KIDS))
	    break;
	ref(cBINOP->op_first, type);
	break;
    case OP_AELEM:
    case OP_HELEM:
	ref(cBINOP->op_first, op->op_type);
	if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV) {
	    op->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			       : type == OP_RV2HV ? OPpDEREF_HV
			       : OPpDEREF_SV);
	    op->op_flags |= OPf_MOD;
	}
	break;

    case OP_SCOPE:
    case OP_LEAVE:
    case OP_ENTER:
    case OP_LIST:
	if (!(op->op_flags & OPf_KIDS))
	    break;
	ref(cLISTOP->op_last, type);
	break;
    default:
	break;
    }
    return scalar(op);

}

OP *
my(op)
OP *op;
{
    OP *kid;
    I32 type;

    if (!op || error_count)
	return op;

    type = op->op_type;
    if (type == OP_LIST) {
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    my(kid);
    }
    else if (type != OP_PADSV &&
	     type != OP_PADAV &&
	     type != OP_PADHV &&
	     type != OP_PUSHMARK)
    {
	yyerror(form("Can't declare %s in my", op_desc[op->op_type]));
	return op;
    }
    op->op_flags |= OPf_MOD;
    op->op_private |= OPpLVAL_INTRO;
    return op;
}

OP *
sawparens(o)
OP *o;
{
    if (o)
	o->op_flags |= OPf_PARENS;
    return o;
}

OP *
bind_match(type, left, right)
I32 type;
OP *left;
OP *right;
{
    OP *op;

    if (dowarn &&
	(left->op_type == OP_RV2AV ||
	 left->op_type == OP_RV2HV ||
	 left->op_type == OP_PADAV ||
	 left->op_type == OP_PADHV)) {
	char *desc = op_desc[(right->op_type == OP_SUBST ||
			      right->op_type == OP_TRANS)
			     ? right->op_type : OP_MATCH];
	char *sample = ((left->op_type == OP_RV2AV ||
			 left->op_type == OP_PADAV)
			? "@array" : "%hash");
	warn("Applying %s to %s will act on scalar(%s)", desc, sample, sample);
    }

    if (right->op_type == OP_MATCH ||
	right->op_type == OP_SUBST ||
	right->op_type == OP_TRANS) {
	right->op_flags |= OPf_STACKED;
	if (right->op_type != OP_MATCH)
	    left = mod(left, right->op_type);
	if (right->op_type == OP_TRANS)
	    op = newBINOP(OP_NULL, OPf_STACKED, scalar(left), right);
	else
	    op = prepend_elem(right->op_type, scalar(left), right);
	if (type == OP_NOT)
	    return newUNOP(OP_NOT, 0, scalar(op));
	return op;
    }
    else
	return bind_match(type, left,
		pmruntime(newPMOP(OP_MATCH, 0), right, Nullop));
}

OP *
invert(op)
OP *op;
{
    if (!op)
	return op;
    /* XXX need to optimize away NOT NOT here?  Or do we let optimizer do it? */
    return newUNOP(OP_NOT, OPf_SPECIAL, scalar(op));
}

OP *
scope(o)
OP *o;
{
    if (o) {
	if (o->op_flags & OPf_PARENS || PERLDB_NOOPT || tainting) {
	    o = prepend_elem(OP_LINESEQ, newOP(OP_ENTER, 0), o);
	    o->op_type = OP_LEAVE;
	    o->op_ppaddr = ppaddr[OP_LEAVE];
	}
	else {
	    if (o->op_type == OP_LINESEQ) {
		OP *kid;
		o->op_type = OP_SCOPE;
		o->op_ppaddr = ppaddr[OP_SCOPE];
		kid = ((LISTOP*)o)->op_first;
		if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE){
		    SvREFCNT_dec(((COP*)kid)->cop_filegv);
		    null(kid);
		}
	    }
	    else
		o = newLISTOP(OP_SCOPE, 0, o, Nullop);
	}
    }
    return o;
}

int
block_start(full)
int full;
{
    int retval = savestack_ix;
    SAVEI32(comppad_name_floor);
    if (full) {
	if ((comppad_name_fill = AvFILL(comppad_name)) > 0)
	    comppad_name_floor = comppad_name_fill;
	else
	    comppad_name_floor = 0;
    }
    SAVEI32(min_intro_pending);
    SAVEI32(max_intro_pending);
    min_intro_pending = 0;
    SAVEI32(comppad_name_fill);
    SAVEI32(padix_floor);
    padix_floor = padix;
    pad_reset_pending = FALSE;
    SAVEI32(hints);
    hints &= ~HINT_BLOCK_SCOPE;
    return retval;
}

OP*
block_end(floor, seq)
I32 floor;
OP* seq;
{
    int needblockscope = hints & HINT_BLOCK_SCOPE;
    OP* retval = scalarseq(seq);
    LEAVE_SCOPE(floor);
    pad_reset_pending = FALSE;
    if (needblockscope)
	hints |= HINT_BLOCK_SCOPE; /* propagate out */
    pad_leavemy(comppad_name_fill);
    cop_seqmax++;
    return retval;
}

void
newPROG(op)
OP *op;
{
    if (in_eval) {
	eval_root = newUNOP(OP_LEAVEEVAL, ((in_eval & 4) ? OPf_SPECIAL : 0), op);
	eval_start = linklist(eval_root);
	eval_root->op_next = 0;
	peep(eval_start);
    }
    else {
	if (!op)
	    return;
	main_root = scope(sawparens(scalarvoid(op)));
	curcop = &compiling;
	main_start = LINKLIST(main_root);
	main_root->op_next = 0;
	peep(main_start);
	compcv = 0;

	/* Register with debugger */
	if (PERLDB_INTER) {
	    CV *cv = perl_get_cv("DB::postponed", FALSE);
	    if (cv) {
		dSP;
		PUSHMARK(sp);
		XPUSHs((SV*)compiling.cop_filegv);
		PUTBACK;
		perl_call_sv((SV*)cv, G_DISCARD);
	    }
	}
    }
}

OP *
localize(o, lex)
OP *o;
I32 lex;
{
    if (o->op_flags & OPf_PARENS)
	list(o);
    else {
	scalar(o);
	if (dowarn && bufptr > oldbufptr && bufptr[-1] == ',') {
	    char *s;
	    for (s = bufptr; *s && (isALNUM(*s) || strchr("@$%, ",*s)); s++) ;
	    if (*s == ';' || *s == '=')
		warn("Parens missing around \"%s\" list", lex ? "my" : "local");
	}
    }
    in_my = FALSE;
    if (lex)
	return my(o);
    else
	return mod(o, OP_NULL);		/* a bit kludgey */
}

OP *
jmaybe(o)
OP *o;
{
    if (o->op_type == OP_LIST) {
	o = convert(OP_JOIN, 0,
		prepend_elem(OP_LIST,
		    newSVREF(newGVOP(OP_GV, 0, gv_fetchpv(";", TRUE, SVt_PV))),
		    o));
    }
    return o;
}

OP *
fold_constants(o)
register OP *o;
{
    register OP *curop;
    I32 type = o->op_type;
    SV *sv;

    if (opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (opargs[type] & OA_TARGET)
	o->op_targ = pad_alloc(type, SVs_PADTMP);

    if ((opargs[type] & OA_OTHERINT) && (hints & HINT_INTEGER))
	o->op_ppaddr = ppaddr[type = ++(o->op_type)];

    if (!(opargs[type] & OA_FOLDCONST))
	goto nope;

    switch (type) {
    case OP_SPRINTF:
    case OP_UCFIRST:
    case OP_LCFIRST:
    case OP_UC:
    case OP_LC:
	if (o->op_private & OPpLOCALE)
	    goto nope;
    }

    if (error_count)
	goto nope;		/* Don't try to run w/ errors */

    for (curop = LINKLIST(o); curop != o; curop = LINKLIST(curop)) {
	if (curop->op_type != OP_CONST &&
		curop->op_type != OP_LIST &&
		curop->op_type != OP_SCALAR &&
		curop->op_type != OP_NULL &&
		curop->op_type != OP_PUSHMARK) {
	    goto nope;
	}
    }

    curop = LINKLIST(o);
    o->op_next = 0;
    op = curop;
    runops();
    sv = *(stack_sp--);
    if (o->op_targ && sv == PAD_SV(o->op_targ))	/* grab pad temp? */
	pad_swipe(o->op_targ);
    else if (SvTEMP(sv)) {			/* grab mortal temp? */
	(void)SvREFCNT_inc(sv);
	SvTEMP_off(sv);
    }
    op_free(o);
    if (type == OP_RV2GV)
	return newGVOP(OP_GV, 0, (GV*)sv);
    else {
	if ((SvFLAGS(sv) & (SVf_IOK|SVf_NOK|SVf_POK)) == SVf_NOK) {
	    IV iv = SvIV(sv);
	    if ((double)iv == SvNV(sv)) {	/* can we smush double to int */
		SvREFCNT_dec(sv);
		sv = newSViv(iv);
	    }
	    else
		SvIOK_off(sv);			/* undo SvIV() damage */
	}
	return newSVOP(OP_CONST, 0, sv);
    }
    
  nope:
    if (!(opargs[type] & OA_OTHERINT))
	return o;

    if (!(hints & HINT_INTEGER)) {
	if (type == OP_DIVIDE || !(o->op_flags & OPf_KIDS))
	    return o;

	for (curop = ((UNOP*)o)->op_first; curop; curop = curop->op_sibling) {
	    if (curop->op_type == OP_CONST) {
		if (SvIOK(((SVOP*)curop)->op_sv))
		    continue;
		return o;
	    }
	    if (opargs[curop->op_type] & OA_RETINTEGER)
		continue;
	    return o;
	}
	o->op_ppaddr = ppaddr[++(o->op_type)];
    }

    return o;
}

OP *
gen_constant_list(o)
register OP *o;
{
    register OP *curop;
    I32 oldtmps_floor = tmps_floor;

    list(o);
    if (error_count)
	return o;		/* Don't attempt to run with errors */

    op = curop = LINKLIST(o);
    o->op_next = 0;
    pp_pushmark();
    runops();
    op = curop;
    pp_anonlist();
    tmps_floor = oldtmps_floor;

    o->op_type = OP_RV2AV;
    o->op_ppaddr = ppaddr[OP_RV2AV];
    curop = ((UNOP*)o)->op_first;
    ((UNOP*)o)->op_first = newSVOP(OP_CONST, 0, SvREFCNT_inc(*stack_sp--));
    op_free(curop);
    linklist(o);
    return list(o);
}

OP *
convert(type, flags, op)
I32 type;
I32 flags;
OP* op;
{
    OP *kid;
    OP *last = 0;

    if (!op || op->op_type != OP_LIST)
	op = newLISTOP(OP_LIST, 0, op, Nullop);
    else
	op->op_flags &= ~OPf_WANT;

    if (!(opargs[type] & OA_MARK))
	null(cLISTOP->op_first);

    op->op_type = type;
    op->op_ppaddr = ppaddr[type];
    op->op_flags |= flags;

    op = CHECKOP(type, op);
    if (op->op_type != type)
	return op;

    if (cLISTOP->op_children < 7) {
	/* XXX do we really need to do this if we're done appending?? */
	for (kid = cLISTOP->op_first; kid; kid = kid->op_sibling)
	    last = kid;
	cLISTOP->op_last = last;	/* in case check substituted last arg */
    }

    return fold_constants(op);
}

/* List constructors */

OP *
append_elem(type, first, last)
I32 type;
OP* first;
OP* last;
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (first->op_type != type || type==OP_LIST && first->op_flags & OPf_PARENS)
	    return newLISTOP(type, 0, first, last);

    if (first->op_flags & OPf_KIDS)
	((LISTOP*)first)->op_last->op_sibling = last;
    else {
	first->op_flags |= OPf_KIDS;
	((LISTOP*)first)->op_first = last;
    }
    ((LISTOP*)first)->op_last = last;
    ((LISTOP*)first)->op_children++;
    return first;
}

OP *
append_list(type, first, last)
I32 type;
LISTOP* first;
LISTOP* last;
{
    if (!first)
	return (OP*)last;

    if (!last)
	return (OP*)first;

    if (first->op_type != type)
	return prepend_elem(type, (OP*)first, (OP*)last);

    if (last->op_type != type)
	return append_elem(type, (OP*)first, (OP*)last);

    first->op_last->op_sibling = last->op_first;
    first->op_last = last->op_last;
    first->op_children += last->op_children;
    if (first->op_children)
	last->op_flags |= OPf_KIDS;

    Safefree(last);
    return (OP*)first;
}

OP *
prepend_elem(type, first, last)
I32 type;
OP* first;
OP* last;
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (last->op_type == type) {
	if (type == OP_LIST) {	/* already a PUSHMARK there */
	    first->op_sibling = ((LISTOP*)last)->op_first->op_sibling;
	    ((LISTOP*)last)->op_first->op_sibling = first;
	}
	else {
	    if (!(last->op_flags & OPf_KIDS)) {
		((LISTOP*)last)->op_last = first;
		last->op_flags |= OPf_KIDS;
	    }
	    first->op_sibling = ((LISTOP*)last)->op_first;
	    ((LISTOP*)last)->op_first = first;
	}
	((LISTOP*)last)->op_children++;
	return last;
    }

    return newLISTOP(type, 0, first, last);
}

/* Constructors */

OP *
newNULLLIST()
{
    return newOP(OP_STUB, 0);
}

OP *
force_list(op)
OP* op;
{
    if (!op || op->op_type != OP_LIST)
	op = newLISTOP(OP_LIST, 0, op, Nullop);
    null(op);
    return op;
}

OP *
newLISTOP(type, flags, first, last)
I32 type;
I32 flags;
OP* first;
OP* last;
{
    LISTOP *listop;

    Newz(1101, listop, 1, LISTOP);

    listop->op_type = type;
    listop->op_ppaddr = ppaddr[type];
    listop->op_children = (first != 0) + (last != 0);
    listop->op_flags = flags;

    if (!last && first)
	last = first;
    else if (!first && last)
	first = last;
    else if (first)
	first->op_sibling = last;
    listop->op_first = first;
    listop->op_last = last;
    if (type == OP_LIST) {
	OP* pushop;
	pushop = newOP(OP_PUSHMARK, 0);
	pushop->op_sibling = first;
	listop->op_first = pushop;
	listop->op_flags |= OPf_KIDS;
	if (!last)
	    listop->op_last = pushop;
    }
    else if (listop->op_children)
	listop->op_flags |= OPf_KIDS;

    return (OP*)listop;
}

OP *
newOP(type, flags)
I32 type;
I32 flags;
{
    OP *op;
    Newz(1101, op, 1, OP);
    op->op_type = type;
    op->op_ppaddr = ppaddr[type];
    op->op_flags = flags;

    op->op_next = op;
    op->op_private = 0 + (flags >> 8);
    if (opargs[type] & OA_RETSCALAR)
	scalar(op);
    if (opargs[type] & OA_TARGET)
	op->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, op);
}

OP *
newUNOP(type, flags, first)
I32 type;
I32 flags;
OP* first;
{
    UNOP *unop;

    if (!first)
	first = newOP(OP_STUB, 0); 
    if (opargs[type] & OA_MARK)
	first = force_list(first);

    Newz(1101, unop, 1, UNOP);
    unop->op_type = type;
    unop->op_ppaddr = ppaddr[type];
    unop->op_first = first;
    unop->op_flags = flags | OPf_KIDS;
    unop->op_private = 1 | (flags >> 8);

    unop = (UNOP*) CHECKOP(type, unop);
    if (unop->op_next)
	return (OP*)unop;

    return fold_constants((OP *) unop);
}

OP *
newBINOP(type, flags, first, last)
I32 type;
I32 flags;
OP* first;
OP* last;
{
    BINOP *binop;
    Newz(1101, binop, 1, BINOP);

    if (!first)
	first = newOP(OP_NULL, 0);

    binop->op_type = type;
    binop->op_ppaddr = ppaddr[type];
    binop->op_first = first;
    binop->op_flags = flags | OPf_KIDS;
    if (!last) {
	last = first;
	binop->op_private = 1 | (flags >> 8);
    }
    else {
	binop->op_private = 2 | (flags >> 8);
	first->op_sibling = last;
    }

    binop = (BINOP*)CHECKOP(type, binop);
    if (binop->op_next)
	return (OP*)binop;

    binop->op_last = last = binop->op_first->op_sibling;

    return fold_constants((OP *)binop);
}

OP *
pmtrans(op, expr, repl)
OP *op;
OP *expr;
OP *repl;
{
    SV *tstr = ((SVOP*)expr)->op_sv;
    SV *rstr = ((SVOP*)repl)->op_sv;
    STRLEN tlen;
    STRLEN rlen;
    register U8 *t = (U8*)SvPV(tstr, tlen);
    register U8 *r = (U8*)SvPV(rstr, rlen);
    register I32 i;
    register I32 j;
    I32 delete;
    I32 complement;
    register short *tbl;

    tbl = (short*)cPVOP->op_pv;
    complement	= op->op_private & OPpTRANS_COMPLEMENT;
    delete	= op->op_private & OPpTRANS_DELETE;
    /* squash	= op->op_private & OPpTRANS_SQUASH; */

    if (complement) {
	Zero(tbl, 256, short);
	for (i = 0; i < tlen; i++)
	    tbl[t[i]] = -1;
	for (i = 0, j = 0; i < 256; i++) {
	    if (!tbl[i]) {
		if (j >= rlen) {
		    if (delete)
			tbl[i] = -2;
		    else if (rlen)
			tbl[i] = r[j-1];
		    else
			tbl[i] = i;
		}
		else
		    tbl[i] = r[j++];
	    }
	}
    }
    else {
	if (!rlen && !delete) {
	    r = t; rlen = tlen;
	}
	for (i = 0; i < 256; i++)
	    tbl[i] = -1;
	for (i = 0, j = 0; i < tlen; i++,j++) {
	    if (j >= rlen) {
		if (delete) {
		    if (tbl[t[i]] == -1)
			tbl[t[i]] = -2;
		    continue;
		}
		--j;
	    }
	    if (tbl[t[i]] == -1)
		tbl[t[i]] = r[j];
	}
    }
    op_free(expr);
    op_free(repl);

    return op;
}

OP *
newPMOP(type, flags)
I32 type;
I32 flags;
{
    PMOP *pmop;

    Newz(1101, pmop, 1, PMOP);
    pmop->op_type = type;
    pmop->op_ppaddr = ppaddr[type];
    pmop->op_flags = flags;
    pmop->op_private = 0 | (flags >> 8);

    if (hints & HINT_LOCALE)
	pmop->op_pmpermflags = (pmop->op_pmflags |= PMf_LOCALE);

    /* link into pm list */
    if (type != OP_TRANS && curstash) {
	pmop->op_pmnext = HvPMROOT(curstash);
	HvPMROOT(curstash) = pmop;
    }

    return (OP*)pmop;
}

OP *
pmruntime(op, expr, repl)
OP *op;
OP *expr;
OP *repl;
{
    PMOP *pm;
    LOGOP *rcop;

    if (op->op_type == OP_TRANS)
	return pmtrans(op, expr, repl);

    hints |= HINT_BLOCK_SCOPE;
    pm = (PMOP*)op;

    if (expr->op_type == OP_CONST) {
	STRLEN plen;
	SV *pat = ((SVOP*)expr)->op_sv;
	char *p = SvPV(pat, plen);
	if ((op->op_flags & OPf_SPECIAL) && strEQ(p, " ")) {
	    sv_setpvn(pat, "\\s+", 3);
	    p = SvPV(pat, plen);
	    pm->op_pmflags |= PMf_SKIPWHITE;
	}
	pm->op_pmregexp = pregcomp(p, p + plen, pm);
	if (strEQ("\\s+", pm->op_pmregexp->precomp)) 
	    pm->op_pmflags |= PMf_WHITE;
	hoistmust(pm);
	op_free(expr);
    }
    else {
	if (pm->op_pmflags & PMf_KEEP)
	    expr = newUNOP(OP_REGCMAYBE,0,expr);

	Newz(1101, rcop, 1, LOGOP);
	rcop->op_type = OP_REGCOMP;
	rcop->op_ppaddr = ppaddr[OP_REGCOMP];
	rcop->op_first = scalar(expr);
	rcop->op_flags |= OPf_KIDS;
	rcop->op_private = 1;
	rcop->op_other = op;

	/* establish postfix order */
	if (pm->op_pmflags & PMf_KEEP) {
	    LINKLIST(expr);
	    rcop->op_next = expr;
	    ((UNOP*)expr)->op_first->op_next = (OP*)rcop;
	}
	else {
	    rcop->op_next = LINKLIST(expr);
	    expr->op_next = (OP*)rcop;
	}

	prepend_elem(op->op_type, scalar((OP*)rcop), op);
    }

    if (repl) {
	OP *curop;
	if (pm->op_pmflags & PMf_EVAL)
	    curop = 0;
	else if (repl->op_type == OP_CONST)
	    curop = repl;
	else {
	    OP *lastop = 0;
	    for (curop = LINKLIST(repl); curop!=repl; curop = LINKLIST(curop)) {
		if (opargs[curop->op_type] & OA_DANGEROUS) {
		    if (curop->op_type == OP_GV) {
			GV *gv = ((GVOP*)curop)->op_gv;
			if (strchr("&`'123456789+", *GvENAME(gv)))
			    break;
		    }
		    else if (curop->op_type == OP_RV2CV)
			break;
		    else if (curop->op_type == OP_RV2SV ||
			     curop->op_type == OP_RV2AV ||
			     curop->op_type == OP_RV2HV ||
			     curop->op_type == OP_RV2GV) {
			if (lastop && lastop->op_type != OP_GV)	/*funny deref?*/
			    break;
		    }
		    else if (curop->op_type == OP_PADSV ||
			     curop->op_type == OP_PADAV ||
			     curop->op_type == OP_PADHV ||
			     curop->op_type == OP_PADANY) {
			     /* is okay */
		    }
		    else
			break;
		}
		lastop = curop;
	    }
	}
	if (curop == repl) {
	    pm->op_pmflags |= PMf_CONST;	/* const for long enough */
	    pm->op_pmpermflags |= PMf_CONST;	/* const for long enough */
	    prepend_elem(op->op_type, scalar(repl), op);
	}
	else {
	    Newz(1101, rcop, 1, LOGOP);
	    rcop->op_type = OP_SUBSTCONT;
	    rcop->op_ppaddr = ppaddr[OP_SUBSTCONT];
	    rcop->op_first = scalar(repl);
	    rcop->op_flags |= OPf_KIDS;
	    rcop->op_private = 1;
	    rcop->op_other = op;

	    /* establish postfix order */
	    rcop->op_next = LINKLIST(repl);
	    repl->op_next = (OP*)rcop;

	    pm->op_pmreplroot = scalar((OP*)rcop);
	    pm->op_pmreplstart = LINKLIST(rcop);
	    rcop->op_next = 0;
	}
    }

    return (OP*)pm;
}

OP *
newSVOP(type, flags, sv)
I32 type;
I32 flags;
SV *sv;
{
    SVOP *svop;
    Newz(1101, svop, 1, SVOP);
    svop->op_type = type;
    svop->op_ppaddr = ppaddr[type];
    svop->op_sv = sv;
    svop->op_next = (OP*)svop;
    svop->op_flags = flags;
    if (opargs[type] & OA_RETSCALAR)
	scalar((OP*)svop);
    if (opargs[type] & OA_TARGET)
	svop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, svop);
}

OP *
newGVOP(type, flags, gv)
I32 type;
I32 flags;
GV *gv;
{
    GVOP *gvop;
    Newz(1101, gvop, 1, GVOP);
    gvop->op_type = type;
    gvop->op_ppaddr = ppaddr[type];
    gvop->op_gv = (GV*)SvREFCNT_inc(gv);
    gvop->op_next = (OP*)gvop;
    gvop->op_flags = flags;
    if (opargs[type] & OA_RETSCALAR)
	scalar((OP*)gvop);
    if (opargs[type] & OA_TARGET)
	gvop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, gvop);
}

OP *
newPVOP(type, flags, pv)
I32 type;
I32 flags;
char *pv;
{
    PVOP *pvop;
    Newz(1101, pvop, 1, PVOP);
    pvop->op_type = type;
    pvop->op_ppaddr = ppaddr[type];
    pvop->op_pv = pv;
    pvop->op_next = (OP*)pvop;
    pvop->op_flags = flags;
    if (opargs[type] & OA_RETSCALAR)
	scalar((OP*)pvop);
    if (opargs[type] & OA_TARGET)
	pvop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, pvop);
}

void
package(op)
OP *op;
{
    SV *sv;

    save_hptr(&curstash);
    save_item(curstname);
    if (op) {
	STRLEN len;
	char *name;
	sv = cSVOP->op_sv;
	name = SvPV(sv, len);
	curstash = gv_stashpvn(name,len,TRUE);
	sv_setpvn(curstname, name, len);
	op_free(op);
    }
    else {
	sv_setpv(curstname,"<none>");
	curstash = Nullhv;
    }
    copline = NOLINE;
    expect = XSTATE;
}

void
utilize(aver, floor, version, id, arg)
int aver;
I32 floor;
OP *version;
OP *id;
OP *arg;
{
    OP *pack;
    OP *meth;
    OP *rqop;
    OP *imop;
    OP *veop;

    if (id->op_type != OP_CONST)
	croak("Module name must be constant");

    veop = Nullop;

    if(version != Nullop) {
	SV *vesv = ((SVOP*)version)->op_sv;

	if (arg == Nullop && !SvNIOK(vesv)) {
	    arg = version;
	}
	else {
	    OP *pack;
	    OP *meth;

	    if (version->op_type != OP_CONST || !SvNIOK(vesv))
		croak("Version number must be constant number");

	    /* Make copy of id so we don't free it twice */
	    pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)id)->op_sv));

	    /* Fake up a method call to VERSION */
	    meth = newSVOP(OP_CONST, 0, newSVpv("VERSION", 7));
	    veop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
			    append_elem(OP_LIST,
			    prepend_elem(OP_LIST, pack, list(version)),
			    newUNOP(OP_METHOD, 0, meth)));
	}
    }
     
    /* Fake up an import/unimport */
    if (arg && arg->op_type == OP_STUB)
	imop = arg;		/* no import on explicit () */
    else if(SvNIOK(((SVOP*)id)->op_sv)) {
	imop = Nullop;		/* use 5.0; */
    }
    else {
	/* Make copy of id so we don't free it twice */
	pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)id)->op_sv));
	meth = newSVOP(OP_CONST, 0,
	    aver
		? newSVpv("import", 6)
		: newSVpv("unimport", 8)
	    );
	imop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
		    append_elem(OP_LIST,
			prepend_elem(OP_LIST, pack, list(arg)),
			newUNOP(OP_METHOD, 0, meth)));
    }

    /* Fake up a require */
    rqop = newUNOP(OP_REQUIRE, 0, id);

    /* Fake up the BEGIN {}, which does its thing immediately. */
    newSUB(floor,
	newSVOP(OP_CONST, 0, newSVpv("BEGIN", 5)),
	Nullop,
	append_elem(OP_LINESEQ,
	    append_elem(OP_LINESEQ,
	        newSTATEOP(0, Nullch, rqop),
	        newSTATEOP(0, Nullch, veop)),
	    newSTATEOP(0, Nullch, imop) ));

    copline = NOLINE;
    expect = XSTATE;
}

OP *
newSLICEOP(flags, subscript, listval)
I32 flags;
OP *subscript;
OP *listval;
{
    return newBINOP(OP_LSLICE, flags,
	    list(force_list(subscript)),
	    list(force_list(listval)) );
}

static I32
list_assignment(op)
register OP *op;
{
    if (!op)
	return TRUE;

    if (op->op_type == OP_NULL && op->op_flags & OPf_KIDS)
	op = cUNOP->op_first;

    if (op->op_type == OP_COND_EXPR) {
	I32 t = list_assignment(cCONDOP->op_first->op_sibling);
	I32 f = list_assignment(cCONDOP->op_first->op_sibling->op_sibling);

	if (t && f)
	    return TRUE;
	if (t || f)
	    yyerror("Assignment to both a list and a scalar");
	return FALSE;
    }

    if (op->op_type == OP_LIST || op->op_flags & OPf_PARENS ||
	op->op_type == OP_RV2AV || op->op_type == OP_RV2HV ||
	op->op_type == OP_ASLICE || op->op_type == OP_HSLICE)
	return TRUE;

    if (op->op_type == OP_PADAV || op->op_type == OP_PADHV)
	return TRUE;

    if (op->op_type == OP_RV2SV)
	return FALSE;

    return FALSE;
}

OP *
newASSIGNOP(flags, left, optype, right)
I32 flags;
OP *left;
I32 optype;
OP *right;
{
    OP *op;

    if (optype) {
	if (optype == OP_ANDASSIGN || optype == OP_ORASSIGN) {
	    return newLOGOP(optype, 0,
		mod(scalar(left), optype),
		newUNOP(OP_SASSIGN, 0, scalar(right)));
	}
	else {
	    return newBINOP(optype, OPf_STACKED,
		mod(scalar(left), optype), scalar(right));
	}
    }

    if (list_assignment(left)) {
	modcount = 0;
	eval_start = right;	/* Grandfathering $[ assignment here.  Bletch.*/
	left = mod(left, OP_AASSIGN);
	if (eval_start)
	    eval_start = 0;
	else {
	    op_free(left);
	    op_free(right);
	    return Nullop;
	}
	op = newBINOP(OP_AASSIGN, flags,
		list(force_list(right)),
		list(force_list(left)) );
	op->op_private = 0 | (flags >> 8);
	if (!(left->op_private & OPpLVAL_INTRO)) {
	    static int generation = 100;
	    OP *curop;
	    OP *lastop = op;
	    generation++;
	    for (curop = LINKLIST(op); curop != op; curop = LINKLIST(curop)) {
		if (opargs[curop->op_type] & OA_DANGEROUS) {
		    if (curop->op_type == OP_GV) {
			GV *gv = ((GVOP*)curop)->op_gv;
			if (gv == defgv || SvCUR(gv) == generation)
			    break;
			SvCUR(gv) = generation;
		    }
		    else if (curop->op_type == OP_PADSV ||
			     curop->op_type == OP_PADAV ||
			     curop->op_type == OP_PADHV ||
			     curop->op_type == OP_PADANY) {
			SV **svp = AvARRAY(comppad_name);
			SV *sv = svp[curop->op_targ];
			if (SvCUR(sv) == generation)
			    break;
			SvCUR(sv) = generation;	/* (SvCUR not used any more) */
		    }
		    else if (curop->op_type == OP_RV2CV)
			break;
		    else if (curop->op_type == OP_RV2SV ||
			     curop->op_type == OP_RV2AV ||
			     curop->op_type == OP_RV2HV ||
			     curop->op_type == OP_RV2GV) {
			if (lastop->op_type != OP_GV)	/* funny deref? */
			    break;
		    }
		    else
			break;
		}
		lastop = curop;
	    }
	    if (curop != op)
		op->op_private = OPpASSIGN_COMMON;
	}
	if (right && right->op_type == OP_SPLIT) {
	    OP* tmpop;
	    if ((tmpop = ((LISTOP*)right)->op_first) &&
		tmpop->op_type == OP_PUSHRE)
	    {
		PMOP *pm = (PMOP*)tmpop;
		if (left->op_type == OP_RV2AV &&
		    !(left->op_private & OPpLVAL_INTRO) &&
		    !(op->op_private & OPpASSIGN_COMMON) )
		{
		    tmpop = ((UNOP*)left)->op_first;
		    if (tmpop->op_type == OP_GV && !pm->op_pmreplroot) {
			pm->op_pmreplroot = (OP*)((GVOP*)tmpop)->op_gv;
			pm->op_pmflags |= PMf_ONCE;
			tmpop = ((UNOP*)op)->op_first;	/* to list (nulled) */
			tmpop = ((UNOP*)tmpop)->op_first; /* to pushmark */
			tmpop->op_sibling = Nullop;	/* don't free split */
			right->op_next = tmpop->op_next;  /* fix starting loc */
			op_free(op);			/* blow off assign */
			right->op_flags &= ~OPf_WANT;
				/* "I don't know and I don't care." */
			return right;
		    }
		}
		else {
		    if (modcount < 10000 &&
		      ((LISTOP*)right)->op_last->op_type == OP_CONST)
		    {
			SV *sv = ((SVOP*)((LISTOP*)right)->op_last)->op_sv;
			if (SvIVX(sv) == 0)
			    sv_setiv(sv, modcount+1);
		    }
		}
	    }
	}
	return op;
    }
    if (!right)
	right = newOP(OP_UNDEF, 0);
    if (right->op_type == OP_READLINE) {
	right->op_flags |= OPf_STACKED;
	return newBINOP(OP_NULL, flags, mod(scalar(left), OP_SASSIGN), scalar(right));
    }
    else {
	eval_start = right;	/* Grandfathering $[ assignment here.  Bletch.*/
	op = newBINOP(OP_SASSIGN, flags,
	    scalar(right), mod(scalar(left), OP_SASSIGN) );
	if (eval_start)
	    eval_start = 0;
	else {
	    op_free(op);
	    return Nullop;
	}
    }
    return op;
}

OP *
newSTATEOP(flags, label, op)
I32 flags;
char *label;
OP *op;
{
    U32 seq = intro_my();
    register COP *cop;

    Newz(1101, cop, 1, COP);
    if (PERLDB_LINE && curcop->cop_line && curstash != debstash) {
	cop->op_type = OP_DBSTATE;
	cop->op_ppaddr = ppaddr[ OP_DBSTATE ];
    }
    else {
	cop->op_type = OP_NEXTSTATE;
	cop->op_ppaddr = ppaddr[ OP_NEXTSTATE ];
    }
    cop->op_flags = flags;
    cop->op_private = 0 | (flags >> 8);
#ifdef NATIVE_HINTS
    cop->op_private |= NATIVE_HINTS;
#endif
    cop->op_next = (OP*)cop;

    if (label) {
	cop->cop_label = label;
	hints |= HINT_BLOCK_SCOPE;
    }
    cop->cop_seq = seq;
    cop->cop_arybase = curcop->cop_arybase;

    if (copline == NOLINE)
        cop->cop_line = curcop->cop_line;
    else {
        cop->cop_line = copline;
        copline = NOLINE;
    }
    cop->cop_filegv = (GV*)SvREFCNT_inc(curcop->cop_filegv);
    cop->cop_stash = curstash;

    if (PERLDB_LINE && curstash != debstash) {
	SV **svp = av_fetch(GvAV(curcop->cop_filegv),(I32)cop->cop_line, FALSE);
	if (svp && *svp != &sv_undef && !SvIOK(*svp)) {
	    (void)SvIOK_on(*svp);
	    SvIVX(*svp) = 1;
	    SvSTASH(*svp) = (HV*)cop;
	}
    }

    return prepend_elem(OP_LINESEQ, (OP*)cop, op);
}

/* "Introduce" my variables to visible status. */
U32
intro_my()
{
    SV **svp;
    SV *sv;
    I32 i;

    if (! min_intro_pending)
	return cop_seqmax;

    svp = AvARRAY(comppad_name);
    for (i = min_intro_pending; i <= max_intro_pending; i++) {
	if ((sv = svp[i]) && sv != &sv_undef && !SvIVX(sv)) {
	    SvIVX(sv) = 999999999;	/* Don't know scope end yet. */
	    SvNVX(sv) = (double)cop_seqmax;
	}
    }
    min_intro_pending = 0;
    comppad_name_fill = max_intro_pending;	/* Needn't search higher */
    return cop_seqmax++;
}

OP *
newLOGOP(type, flags, first, other)
I32 type;
I32 flags;
OP* first;
OP* other;
{
    LOGOP *logop;
    OP *op;

    if (type == OP_XOR)		/* Not short circuit, but here by precedence. */
	return newBINOP(type, flags, scalar(first), scalar(other));

    scalarboolean(first);
    /* optimize "!a && b" to "a || b", and "!a || b" to "a && b" */
    if (first->op_type == OP_NOT && (first->op_flags & OPf_SPECIAL)) {
	if (type == OP_AND || type == OP_OR) {
	    if (type == OP_AND)
		type = OP_OR;
	    else
		type = OP_AND;
	    op = first;
	    first = cUNOP->op_first;
	    if (op->op_next)
		first->op_next = op->op_next;
	    cUNOP->op_first = Nullop;
	    op_free(op);
	}
    }
    if (first->op_type == OP_CONST) {
	if (dowarn && (first->op_private & OPpCONST_BARE))
	    warn("Probable precedence problem on %s", op_desc[type]);
	if ((type == OP_AND) == (SvTRUE(((SVOP*)first)->op_sv))) {
	    op_free(first);
	    return other;
	}
	else {
	    op_free(other);
	    return first;
	}
    }
    else if (first->op_type == OP_WANTARRAY) {
	if (type == OP_AND)
	    list(other);
	else
	    scalar(other);
    }
    else if (dowarn && (first->op_flags & OPf_KIDS)) {
	OP *k1 = ((UNOP*)first)->op_first;
	OP *k2 = k1->op_sibling;
	OPCODE warnop = 0;
	switch (first->op_type)
	{
	case OP_NULL:
	    if (k2 && k2->op_type == OP_READLINE
		  && (k2->op_flags & OPf_STACKED)
		  && (k1->op_type == OP_RV2SV || k1->op_type == OP_PADSV))
		warnop = k2->op_type;
	    break;

	case OP_SASSIGN:
	    if (k1->op_type == OP_READDIR
		  || k1->op_type == OP_GLOB
		  || k1->op_type == OP_EACH)
		warnop = k1->op_type;
	    break;
	}
	if (warnop) {
	    line_t oldline = curcop->cop_line;
	    curcop->cop_line = copline;
	    warn("Value of %s%s can be \"0\"; test with defined()",
		 op_desc[warnop],
		 ((warnop == OP_READLINE || warnop == OP_GLOB)
		  ? " construct" : "() operator"));
	    curcop->cop_line = oldline;
	}
    }

    if (!other)
	return first;

    if (type == OP_ANDASSIGN || type == OP_ORASSIGN)
	other->op_private |= OPpASSIGN_BACKWARDS;  /* other is an OP_SASSIGN */

    Newz(1101, logop, 1, LOGOP);

    logop->op_type = type;
    logop->op_ppaddr = ppaddr[type];
    logop->op_first = first;
    logop->op_flags = flags | OPf_KIDS;
    logop->op_other = LINKLIST(other);
    logop->op_private = 1 | (flags >> 8);

    /* establish postfix order */
    logop->op_next = LINKLIST(first);
    first->op_next = (OP*)logop;
    first->op_sibling = other;

    op = newUNOP(OP_NULL, 0, (OP*)logop);
    other->op_next = op;

    return op;
}

OP *
newCONDOP(flags, first, trueop, falseop)
I32 flags;
OP* first;
OP* trueop;
OP* falseop;
{
    CONDOP *condop;
    OP *op;

    if (!falseop)
	return newLOGOP(OP_AND, 0, first, trueop);
    if (!trueop)
	return newLOGOP(OP_OR, 0, first, falseop);

    scalarboolean(first);
    if (first->op_type == OP_CONST) {
	if (SvTRUE(((SVOP*)first)->op_sv)) {
	    op_free(first);
	    op_free(falseop);
	    return trueop;
	}
	else {
	    op_free(first);
	    op_free(trueop);
	    return falseop;
	}
    }
    else if (first->op_type == OP_WANTARRAY) {
	list(trueop);
	scalar(falseop);
    }
    Newz(1101, condop, 1, CONDOP);

    condop->op_type = OP_COND_EXPR;
    condop->op_ppaddr = ppaddr[OP_COND_EXPR];
    condop->op_first = first;
    condop->op_flags = flags | OPf_KIDS;
    condop->op_true = LINKLIST(trueop);
    condop->op_false = LINKLIST(falseop);
    condop->op_private = 1 | (flags >> 8);

    /* establish postfix order */
    condop->op_next = LINKLIST(first);
    first->op_next = (OP*)condop;

    first->op_sibling = trueop;
    trueop->op_sibling = falseop;
    op = newUNOP(OP_NULL, 0, (OP*)condop);

    trueop->op_next = op;
    falseop->op_next = op;

    return op;
}

OP *
newRANGE(flags, left, right)
I32 flags;
OP *left;
OP *right;
{
    CONDOP *condop;
    OP *flip;
    OP *flop;
    OP *op;

    Newz(1101, condop, 1, CONDOP);

    condop->op_type = OP_RANGE;
    condop->op_ppaddr = ppaddr[OP_RANGE];
    condop->op_first = left;
    condop->op_flags = OPf_KIDS;
    condop->op_true = LINKLIST(left);
    condop->op_false = LINKLIST(right);
    condop->op_private = 1 | (flags >> 8);

    left->op_sibling = right;

    condop->op_next = (OP*)condop;
    flip = newUNOP(OP_FLIP, flags, (OP*)condop);
    flop = newUNOP(OP_FLOP, 0, flip);
    op = newUNOP(OP_NULL, 0, flop);
    linklist(flop);

    left->op_next = flip;
    right->op_next = flop;

    condop->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(condop->op_targ), SVt_PVNV);
    flip->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(flip->op_targ), SVt_PVNV);

    flip->op_private =  left->op_type == OP_CONST ? OPpFLIP_LINENUM : 0;
    flop->op_private = right->op_type == OP_CONST ? OPpFLIP_LINENUM : 0;

    flip->op_next = op;
    if (!flip->op_private || !flop->op_private)
	linklist(op);		/* blow off optimizer unless constant */

    return op;
}

OP *
newLOOPOP(flags, debuggable, expr, block)
I32 flags;
I32 debuggable;
OP *expr;
OP *block;
{
    OP* listop;
    OP* op;
    int once = block && block->op_flags & OPf_SPECIAL &&
      (block->op_type == OP_ENTERSUB || block->op_type == OP_NULL);

    if (expr) {
	if (once && expr->op_type == OP_CONST && !SvTRUE(((SVOP*)expr)->op_sv))
	    return block;	/* do {} while 0 does once */
	if (expr->op_type == OP_READLINE || expr->op_type == OP_GLOB
	    || (expr->op_type == OP_NULL && expr->op_targ == OP_GLOB)) {
	    expr = newUNOP(OP_DEFINED, 0,
		newASSIGNOP(0, newSVREF(newGVOP(OP_GV, 0, defgv)), 0, expr) );
	}
    }

    listop = append_elem(OP_LINESEQ, block, newOP(OP_UNSTACK, 0));
    op = newLOGOP(OP_AND, 0, expr, listop);

    ((LISTOP*)listop)->op_last->op_next = LINKLIST(op);

    if (once && op != listop)
	op->op_next = ((LOGOP*)cUNOP->op_first)->op_other;

    if (op == listop)
	op = newUNOP(OP_NULL, 0, op);	/* or do {} while 1 loses outer block */

    op->op_flags |= flags;
    op = scope(op);
    op->op_flags |= OPf_SPECIAL;	/* suppress POPBLOCK curpm restoration*/
    return op;
}

OP *
newWHILEOP(flags, debuggable, loop, whileline, expr, block, cont)
I32 flags;
I32 debuggable;
LOOP *loop;
I32 whileline;
OP *expr;
OP *block;
OP *cont;
{
    OP *redo;
    OP *next = 0;
    OP *listop;
    OP *op;
    OP *condop;

    if (expr && (expr->op_type == OP_READLINE || expr->op_type == OP_GLOB
		 || (expr->op_type == OP_NULL && expr->op_targ == OP_GLOB))) {
	expr = newUNOP(OP_DEFINED, 0,
	    newASSIGNOP(0, newSVREF(newGVOP(OP_GV, 0, defgv)), 0, expr) );
    }

    if (!block)
	block = newOP(OP_NULL, 0);

    if (cont)
	next = LINKLIST(cont);
    if (expr) {
	cont = append_elem(OP_LINESEQ, cont, newOP(OP_UNSTACK, 0));
	if ((line_t)whileline != NOLINE) {
	    copline = whileline;
	    cont = append_elem(OP_LINESEQ, cont,
			       newSTATEOP(0, Nullch, Nullop));
	}
    }

    listop = append_list(OP_LINESEQ, (LISTOP*)block, (LISTOP*)cont);
    redo = LINKLIST(listop);

    if (expr) {
	op = newLOGOP(OP_AND, 0, expr, scalar(listop));
	if (op == expr && op->op_type == OP_CONST && !SvTRUE(cSVOP->op_sv)) {
	    op_free(expr);		/* oops, it's a while (0) */
	    op_free((OP*)loop);
	    return Nullop;		/* (listop already freed by newLOGOP) */
	}
	((LISTOP*)listop)->op_last->op_next = condop = 
	    (op == listop ? redo : LINKLIST(op));
	if (!next)
	    next = condop;
    }
    else
	op = listop;

    if (!loop) {
	Newz(1101,loop,1,LOOP);
	loop->op_type = OP_ENTERLOOP;
	loop->op_ppaddr = ppaddr[OP_ENTERLOOP];
	loop->op_private = 0;
	loop->op_next = (OP*)loop;
    }

    op = newBINOP(OP_LEAVELOOP, 0, (OP*)loop, op);

    loop->op_redoop = redo;
    loop->op_lastop = op;

    if (next)
	loop->op_nextop = next;
    else
	loop->op_nextop = op;

    op->op_flags |= flags;
    op->op_private |= (flags >> 8);
    return op;
}

OP *
#ifndef CAN_PROTOTYPE
newFOROP(flags,label,forline,sv,expr,block,cont)
I32 flags;
char *label;
line_t forline;
OP* sv;
OP* expr;
OP*block;
OP*cont;
#else
newFOROP(I32 flags,char *label,line_t forline,OP *sv,OP *expr,OP *block,OP *cont)
#endif /* CAN_PROTOTYPE */
{
    LOOP *loop;
    OP *wop;
    int padoff = 0;
    I32 iterflags = 0;

    if (sv) {
	if (sv->op_type == OP_RV2SV) {	/* symbol table variable */
	    sv->op_type = OP_RV2GV;
	    sv->op_ppaddr = ppaddr[OP_RV2GV];
	}
	else if (sv->op_type == OP_PADSV) { /* private variable */
	    padoff = sv->op_targ;
	    op_free(sv);
	    sv = Nullop;
	}
	else
	    croak("Can't use %s for loop variable", op_desc[sv->op_type]);
    }
    else {
	sv = newGVOP(OP_GV, 0, defgv);
    }
    if (expr->op_type == OP_RV2AV || expr->op_type == OP_PADAV) {
	expr = scalar(ref(expr, OP_ITER));
	iterflags |= OPf_STACKED;
    }
    loop = (LOOP*)list(convert(OP_ENTERITER, iterflags,
	append_elem(OP_LIST, mod(force_list(expr), OP_GREPSTART),
		    scalar(sv))));
    assert(!loop->op_next);
    Renew(loop, 1, LOOP);
    loop->op_targ = padoff;
    wop = newWHILEOP(flags, 1, loop, forline, newOP(OP_ITER, 0), block, cont);
    copline = forline;
    return newSTATEOP(0, label, wop);
}

OP*
newLOOPEX(type, label)
I32 type;
OP* label;
{
    OP *op;
    if (type != OP_GOTO || label->op_type == OP_CONST) {
	op = newPVOP(type, 0, savepv(
		label->op_type == OP_CONST
		    ? SvPVx(((SVOP*)label)->op_sv, na)
		    : "" ));
	op_free(label);
    }
    else {
	if (label->op_type == OP_ENTERSUB)
	    label = newUNOP(OP_REFGEN, 0, mod(label, OP_REFGEN));
	op = newUNOP(type, OPf_STACKED, label);
    }
    hints |= HINT_BLOCK_SCOPE;
    return op;
}

void
cv_undef(cv)
CV *cv;
{
    if (!CvXSUB(cv) && CvROOT(cv)) {
	if (CvDEPTH(cv))
	    croak("Can't undef active subroutine");
	ENTER;

	SAVESPTR(curpad);
	curpad = 0;

	if (!CvCLONED(cv))
	    op_free(CvROOT(cv));
	CvROOT(cv) = Nullop;
	LEAVE;
    }
    SvPOK_off((SV*)cv);		/* forget prototype */
    CvFLAGS(cv) = 0;
    SvREFCNT_dec(CvGV(cv));
    CvGV(cv) = Nullgv;
    SvREFCNT_dec(CvOUTSIDE(cv));
    CvOUTSIDE(cv) = Nullcv;
    if (CvPADLIST(cv)) {
	/* may be during global destruction */
	if (SvREFCNT(CvPADLIST(cv))) {
	    I32 i = AvFILL(CvPADLIST(cv));
	    while (i >= 0) {
		SV** svp = av_fetch(CvPADLIST(cv), i--, FALSE);
		SV* sv = svp ? *svp : Nullsv;
		if (!sv)
		    continue;
		if (sv == (SV*)comppad_name)
		    comppad_name = Nullav;
		else if (sv == (SV*)comppad) {
		    comppad = Nullav;
		    curpad = Null(SV**);
		}
		SvREFCNT_dec(sv);
	    }
	    SvREFCNT_dec((SV*)CvPADLIST(cv));
	}
	CvPADLIST(cv) = Nullav;
    }
}

#ifdef DEBUG_CLOSURES
static void
cv_dump(cv)
CV* cv;
{
    CV *outside = CvOUTSIDE(cv);
    AV* padlist = CvPADLIST(cv);
    AV* pad_name;
    AV* pad;
    SV** pname;
    SV** ppad;
    I32 ix;

    PerlIO_printf(Perl_debug_log, "\tCV=0x%lx (%s), OUTSIDE=0x%lx (%s)\n",
		  cv,
		  (CvANON(cv) ? "ANON"
		   : (cv == main_cv) ? "MAIN"
		   : CvUNIQUE(outside) ? "UNIQUE"
		   : CvGV(cv) ? GvNAME(CvGV(cv)) : "UNDEFINED"),
		  outside,
		  (!outside ? "null"
		   : CvANON(outside) ? "ANON"
		   : (outside == main_cv) ? "MAIN"
		   : CvUNIQUE(outside) ? "UNIQUE"
		   : CvGV(outside) ? GvNAME(CvGV(outside)) : "UNDEFINED"));

    if (!padlist)
	return;

    pad_name = (AV*)*av_fetch(padlist, 0, FALSE);
    pad = (AV*)*av_fetch(padlist, 1, FALSE);
    pname = AvARRAY(pad_name);
    ppad = AvARRAY(pad);

    for (ix = 1; ix <= AvFILL(pad_name); ix++) {
	if (SvPOK(pname[ix]))
	    PerlIO_printf(Perl_debug_log, "\t%4d. 0x%lx (%s\"%s\" %ld-%ld)\n",
			  ix, ppad[ix],
			  SvFAKE(pname[ix]) ? "FAKE " : "",
			  SvPVX(pname[ix]),
			  (long)I_32(SvNVX(pname[ix])),
			  (long)SvIVX(pname[ix]));
    }
}
#endif /* DEBUG_CLOSURES */

static CV *
cv_clone2(proto, outside)
CV* proto;
CV* outside;
{
    AV* av;
    I32 ix;
    AV* protopadlist = CvPADLIST(proto);
    AV* protopad_name = (AV*)*av_fetch(protopadlist, 0, FALSE);
    AV* protopad = (AV*)*av_fetch(protopadlist, 1, FALSE);
    SV** pname = AvARRAY(protopad_name);
    SV** ppad = AvARRAY(protopad);
    I32 fname = AvFILL(protopad_name);
    I32 fpad = AvFILL(protopad);
    AV* comppadlist;
    CV* cv;

    assert(!CvUNIQUE(proto));

    ENTER;
    SAVESPTR(curpad);
    SAVESPTR(comppad);
    SAVESPTR(comppad_name);
    SAVESPTR(compcv);

    cv = compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)cv, SvTYPE(proto));
    CvCLONED_on(cv);
    if (CvANON(proto))
	CvANON_on(cv);

    CvFILEGV(cv)	= CvFILEGV(proto);
    CvGV(cv)		= (GV*)SvREFCNT_inc(CvGV(proto));
    CvSTASH(cv)		= CvSTASH(proto);
    CvROOT(cv)		= CvROOT(proto);
    CvSTART(cv)		= CvSTART(proto);
    if (outside)
	CvOUTSIDE(cv)	= (CV*)SvREFCNT_inc(outside);

    if (SvPOK(proto))
	sv_setpvn((SV*)cv, SvPVX(proto), SvCUR(proto));

    comppad_name = newAV();
    for (ix = fname; ix >= 0; ix--)
	av_store(comppad_name, ix, SvREFCNT_inc(pname[ix]));

    comppad = newAV();

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)comppad_name);
    av_store(comppadlist, 1, (SV*)comppad);
    CvPADLIST(cv) = comppadlist;
    av_fill(comppad, AvFILL(protopad));
    curpad = AvARRAY(comppad);

    av = newAV();           /* will be @_ */
    av_extend(av, 0);
    av_store(comppad, 0, (SV*)av);
    AvFLAGS(av) = AVf_REIFY;

    for (ix = fpad; ix > 0; ix--) {
	SV* namesv = (ix <= fname) ? pname[ix] : Nullsv;
	if (namesv && namesv != &sv_undef) {
	    char *name = SvPVX(namesv);    /* XXX */
	    if (SvFLAGS(namesv) & SVf_FAKE) {   /* lexical from outside? */
		I32 off = pad_findlex(name, ix, SvIVX(namesv),
				      CvOUTSIDE(cv), cxstack_ix);
		if (!off)
		    curpad[ix] = SvREFCNT_inc(ppad[ix]);
		else if (off != ix)
		    croak("panic: cv_clone: %s", name);
	    }
	    else {				/* our own lexical */
		SV* sv;
		if (*name == '&') {
		    /* anon code -- we'll come back for it */
		    sv = SvREFCNT_inc(ppad[ix]);
		}
		else if (*name == '@')
		    sv = (SV*)newAV();
		else if (*name == '%')
		    sv = (SV*)newHV();
		else
		    sv = NEWSV(0,0);
		if (!SvPADBUSY(sv))
		    SvPADMY_on(sv);
		curpad[ix] = sv;
	    }
	}
	else {
	    SV* sv = NEWSV(0,0);
	    SvPADTMP_on(sv);
	    curpad[ix] = sv;
	}
    }

    /* Now that vars are all in place, clone nested closures. */

    for (ix = fpad; ix > 0; ix--) {
	SV* namesv = (ix <= fname) ? pname[ix] : Nullsv;
	if (namesv
	    && namesv != &sv_undef
	    && !(SvFLAGS(namesv) & SVf_FAKE)
	    && *SvPVX(namesv) == '&'
	    && CvCLONE(ppad[ix]))
	{
	    CV *kid = cv_clone2((CV*)ppad[ix], cv);
	    SvREFCNT_dec(ppad[ix]);
	    CvCLONE_on(kid);
	    SvPADMY_on(kid);
	    curpad[ix] = (SV*)kid;
	}
    }

#ifdef DEBUG_CLOSURES
    PerlIO_printf(Perl_debug_log, "Cloned inside:\n");
    cv_dump(outside);
    PerlIO_printf(Perl_debug_log, "  from:\n");
    cv_dump(proto);
    PerlIO_printf(Perl_debug_log, "   to:\n");
    cv_dump(cv);
#endif

    LEAVE;
    return cv;
}

CV *
cv_clone(proto)
CV* proto;
{
    return cv_clone2(proto, CvOUTSIDE(proto));
}

void
cv_ckproto(cv, gv, p)
CV* cv;
GV* gv;
char* p;
{
    if ((!p != !SvPOK(cv)) || (p && strNE(p, SvPVX(cv)))) {
	SV* msg = sv_newmortal();
	SV* name = Nullsv;

	if (gv)
	    gv_efullname3(name = sv_newmortal(), gv, Nullch);
	sv_setpv(msg, "Prototype mismatch:");
	if (name)
	    sv_catpvf(msg, " sub %_", name);
	if (SvPOK(cv))
	    sv_catpvf(msg, " (%s)", SvPVX(cv));
	sv_catpv(msg, " vs ");
	if (p)
	    sv_catpvf(msg, "(%s)", p);
	else
	    sv_catpv(msg, "none");
	warn("%_", msg);
    }
}

SV *
cv_const_sv(cv)
CV* cv;
{
    OP *o;
    SV *sv;
    
    if (!cv || !SvPOK(cv) || SvCUR(cv))
	return Nullsv;

    sv = Nullsv;
    for (o = CvSTART(cv); o; o = o->op_next) {
	OPCODE type = o->op_type;
	
	if (type == OP_NEXTSTATE || type == OP_NULL || type == OP_PUSHMARK)
	    continue;
	if (type == OP_LEAVESUB || type == OP_RETURN)
	    break;
	if (sv)
	    return Nullsv;
	if (type == OP_CONST)
	    sv = ((SVOP*)o)->op_sv;
	else if (type == OP_PADSV) {
	    AV* pad = (AV*)(AvARRAY(CvPADLIST(cv))[1]);
	    sv = pad ? AvARRAY(pad)[o->op_targ] : Nullsv;
	    if (!sv || (!SvREADONLY(sv) && SvREFCNT(sv) > 1))
		return Nullsv;
	}
	else
	    return Nullsv;
    }
    if (sv)
	SvREADONLY_on(sv);
    return sv;
}

CV *
newSUB(floor,op,proto,block)
I32 floor;
OP *op;
OP *proto;
OP *block;
{
    char *name = op ? SvPVx(cSVOP->op_sv, na) : Nullch;
    GV *gv = gv_fetchpv(name ? name : "__ANON__", GV_ADDMULTI, SVt_PVCV);
    char *ps = proto ? SvPVx(((SVOP*)proto)->op_sv, na) : Nullch;
    register CV *cv;
    I32 ix;

    if (op)
	SAVEFREEOP(op);
    if (proto)
	SAVEFREEOP(proto);

    if (!name || GvCVGEN(gv))
	cv = Nullcv;
    else if (cv = GvCV(gv)) {
	cv_ckproto(cv, gv, ps);
	/* already defined (or promised)? */
	if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
	    SV* const_sv;
	    if (!block) {
		/* just a "sub foo;" when &foo is already defined */
		SAVEFREESV(compcv);
		goto done;
	    }
	    /* ahem, death to those who redefine active sort subs */
	    if (curstack == sortstack && sortcop == CvSTART(cv))
		croak("Can't redefine active sort subroutine %s", name);
	    const_sv = cv_const_sv(cv);
	    if (const_sv || dowarn) {
		line_t oldline = curcop->cop_line;
		curcop->cop_line = copline;
		warn(const_sv ? "Constant subroutine %s redefined"
		     : "Subroutine %s redefined", name);
		curcop->cop_line = oldline;
	    }
	    SvREFCNT_dec(cv);
	    cv = Nullcv;
	}
    }
    if (cv) {				/* must reuse cv if autoloaded */
	cv_undef(cv);
	CvFLAGS(cv) = CvFLAGS(compcv);
	CvOUTSIDE(cv) = CvOUTSIDE(compcv);
	CvOUTSIDE(compcv) = 0;
	CvPADLIST(cv) = CvPADLIST(compcv);
	CvPADLIST(compcv) = 0;
	if (SvREFCNT(compcv) > 1) /* XXX Make closures transit through stub. */
	    CvOUTSIDE(compcv) = (CV*)SvREFCNT_inc((SV*)cv);
	SvREFCNT_dec(compcv);
    }
    else {
	cv = compcv;
	if (name) {
	    GvCV(gv) = cv;
	    GvCVGEN(gv) = 0;
	    sub_generation++;
	}
    }
    CvGV(cv) = (GV*)SvREFCNT_inc(gv);
    CvFILEGV(cv) = curcop->cop_filegv;
    CvSTASH(cv) = curstash;

    if (ps)
	sv_setpv((SV*)cv, ps);

    if (error_count) {
	op_free(block);
	block = Nullop;
	if (name) {
	    char *s = strrchr(name, ':');
	    s = s ? s+1 : name;
	    if (strEQ(s, "BEGIN")) {
		char *not_safe =
		    "BEGIN not safe after errors--compilation aborted";
		if (in_eval & 4)
		    croak(not_safe);
		else {
		    /* force display of errors found but not reported */
		    sv_catpv(GvSV(errgv), not_safe);
		    croak("%s", SvPVx(GvSV(errgv), na));
		}
	    }
	}
    }
    if (!block) {
	copline = NOLINE;
	LEAVE_SCOPE(floor);
	return cv;
    }

    if (AvFILL(comppad_name) < AvFILL(comppad))
	av_store(comppad_name, AvFILL(comppad), Nullsv);

    if (CvCLONE(cv)) {
	SV **namep = AvARRAY(comppad_name);
	for (ix = AvFILL(comppad); ix > 0; ix--) {
	    SV *namesv;

	    if (SvIMMORTAL(curpad[ix]))
		continue;
	    /*
	     * The only things that a clonable function needs in its
	     * pad are references to outer lexicals and anonymous subs.
	     * The rest are created anew during cloning.
	     */
	    if (!((namesv = namep[ix]) != Nullsv &&
		  namesv != &sv_undef &&
		  (SvFAKE(namesv) ||
		   *SvPVX(namesv) == '&')))
	    {
		SvREFCNT_dec(curpad[ix]);
		curpad[ix] = Nullsv;
	    }
	}
    }
    else {
	AV *av = newAV();			/* Will be @_ */
	av_extend(av, 0);
	av_store(comppad, 0, (SV*)av);
	AvFLAGS(av) = AVf_REIFY;

	for (ix = AvFILL(comppad); ix > 0; ix--) {
	    if (SvIMMORTAL(curpad[ix]))
		continue;
	    if (!SvPADMY(curpad[ix]))
		SvPADTMP_on(curpad[ix]);
	}
    }

    CvROOT(cv) = newUNOP(OP_LEAVESUB, 0, scalarseq(block));
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    peep(CvSTART(cv));

    if (name) {
	char *s;

	if (PERLDB_SUBLINE && curstash != debstash) {
	    SV *sv = NEWSV(0,0);
	    SV *tmpstr = sv_newmortal();
	    static GV *db_postponed;
	    CV *cv;
	    HV *hv;

	    sv_setpvf(sv, "%_:%ld-%ld",
		    GvSV(curcop->cop_filegv),
		    (long)subline, (long)curcop->cop_line);
	    gv_efullname3(tmpstr, gv, Nullch);
	    hv_store(GvHV(DBsub), SvPVX(tmpstr), SvCUR(tmpstr), sv, 0);
	    if (!db_postponed) {
		db_postponed = gv_fetchpv("DB::postponed", GV_ADDMULTI, SVt_PVHV);
	    }
	    hv = GvHVn(db_postponed);
	    if (HvFILL(hv) > 0 && hv_exists(hv, SvPVX(tmpstr), SvCUR(tmpstr))
		  && (cv = GvCV(db_postponed))) {
		dSP;
		PUSHMARK(sp);
		XPUSHs(tmpstr);
		PUTBACK;
		perl_call_sv((SV*)cv, G_DISCARD);
	    }
	}

	if ((s = strrchr(name,':')))
	    s++;
	else
	    s = name;
	if (strEQ(s, "BEGIN")) {
	    I32 oldscope = scopestack_ix;
	    ENTER;
	    SAVESPTR(compiling.cop_filegv);
	    SAVEI16(compiling.cop_line);
	    SAVEI32(perldb);
	    save_svref(&rs);
	    sv_setsv(rs, nrs);

	    if (!beginav)
		beginav = newAV();
	    DEBUG_x( dump_sub(gv) );
	    av_push(beginav, (SV *)cv);
	    GvCV(gv) = 0;
	    call_list(oldscope, beginav);

	    curcop = &compiling;
	    LEAVE;
	}
	else if (strEQ(s, "END") && !error_count) {
	    if (!endav)
		endav = newAV();
	    av_unshift(endav, 1);
	    av_store(endav, 0, (SV *)cv);
	    GvCV(gv) = 0;
	}
    }

  done:
    copline = NOLINE;
    LEAVE_SCOPE(floor);
    return cv;
}

#ifdef DEPRECATED
CV *
newXSUB(name, ix, subaddr, filename)
char *name;
I32 ix;
I32 (*subaddr)();
char *filename;
{
    CV* cv = newXS(name, (void(*)())subaddr, filename);
    CvOLDSTYLE_on(cv);
    CvXSUBANY(cv).any_i32 = ix;
    return cv;
}
#endif

CV *
newXS(name, subaddr, filename)
char *name;
void (*subaddr) _((CV*));
char *filename;
{
    GV *gv = gv_fetchpv(name ? name : "__ANON__", GV_ADDMULTI, SVt_PVCV);
    register CV *cv;

    if (cv = (name ? GvCV(gv) : Nullcv)) {
	if (GvCVGEN(gv)) {
	    /* just a cached method */
	    SvREFCNT_dec(cv);
	    cv = 0;
	}
	else if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
	    /* already defined (or promised) */
	    if (dowarn) {
		line_t oldline = curcop->cop_line;
		curcop->cop_line = copline;
		warn("Subroutine %s redefined",name);
		curcop->cop_line = oldline;
	    }
	    SvREFCNT_dec(cv);
	    cv = 0;
	}
    }

    if (cv)				/* must reuse cv if autoloaded */
	cv_undef(cv);
    else {
	cv = (CV*)NEWSV(1105,0);
	sv_upgrade((SV *)cv, SVt_PVCV);
	if (name) {
	    GvCV(gv) = cv;
	    GvCVGEN(gv) = 0;
	    sub_generation++;
	}
    }
    CvGV(cv) = (GV*)SvREFCNT_inc(gv);
    CvFILEGV(cv) = gv_fetchfile(filename);
    CvXSUB(cv) = subaddr;

    if (name) {
	char *s = strrchr(name,':');
	if (s)
	    s++;
	else
	    s = name;
	if (strEQ(s, "BEGIN")) {
	    if (!beginav)
		beginav = newAV();
	    av_push(beginav, (SV *)cv);
	    GvCV(gv) = 0;
	}
	else if (strEQ(s, "END")) {
	    if (!endav)
		endav = newAV();
	    av_unshift(endav, 1);
	    av_store(endav, 0, (SV *)cv);
	    GvCV(gv) = 0;
	}
    }
    else
	CvANON_on(cv);

    return cv;
}

void
newFORM(floor,op,block)
I32 floor;
OP *op;
OP *block;
{
    register CV *cv;
    char *name;
    GV *gv;
    I32 ix;

    if (op)
	name = SvPVx(cSVOP->op_sv, na);
    else
	name = "STDOUT";
    gv = gv_fetchpv(name,TRUE, SVt_PVFM);
    GvMULTI_on(gv);
    if (cv = GvFORM(gv)) {
	if (dowarn) {
	    line_t oldline = curcop->cop_line;

	    curcop->cop_line = copline;
	    warn("Format %s redefined",name);
	    curcop->cop_line = oldline;
	}
	SvREFCNT_dec(cv);
    }
    cv = compcv;
    GvFORM(gv) = cv;
    CvGV(cv) = (GV*)SvREFCNT_inc(gv);
    CvFILEGV(cv) = curcop->cop_filegv;

    for (ix = AvFILL(comppad); ix > 0; ix--) {
	if (!SvPADMY(curpad[ix]) && !SvIMMORTAL(curpad[ix]))
	    SvPADTMP_on(curpad[ix]);
    }

    CvROOT(cv) = newUNOP(OP_LEAVEWRITE, 0, scalarseq(block));
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    peep(CvSTART(cv));
    op_free(op);
    copline = NOLINE;
    LEAVE_SCOPE(floor);
}

OP *
newANONLIST(op)
OP* op;
{
    return newUNOP(OP_REFGEN, 0,
	mod(list(convert(OP_ANONLIST, 0, op)), OP_REFGEN));
}

OP *
newANONHASH(op)
OP* op;
{
    return newUNOP(OP_REFGEN, 0,
	mod(list(convert(OP_ANONHASH, 0, op)), OP_REFGEN));
}

OP *
newANONSUB(floor, proto, block)
I32 floor;
OP *proto;
OP *block;
{
    return newUNOP(OP_REFGEN, 0,
	newSVOP(OP_ANONCODE, 0, (SV*)newSUB(floor, 0, proto, block)));
}

OP *
oopsAV(o)
OP *o;
{
    switch (o->op_type) {
    case OP_PADSV:
	o->op_type = OP_PADAV;
	o->op_ppaddr = ppaddr[OP_PADAV];
	return ref(newUNOP(OP_RV2AV, 0, scalar(o)), OP_RV2AV);
	
    case OP_RV2SV:
	o->op_type = OP_RV2AV;
	o->op_ppaddr = ppaddr[OP_RV2AV];
	ref(o, OP_RV2AV);
	break;

    default:
	warn("oops: oopsAV");
	break;
    }
    return o;
}

OP *
oopsHV(o)
OP *o;
{
    switch (o->op_type) {
    case OP_PADSV:
    case OP_PADAV:
	o->op_type = OP_PADHV;
	o->op_ppaddr = ppaddr[OP_PADHV];
	return ref(newUNOP(OP_RV2HV, 0, scalar(o)), OP_RV2HV);

    case OP_RV2SV:
    case OP_RV2AV:
	o->op_type = OP_RV2HV;
	o->op_ppaddr = ppaddr[OP_RV2HV];
	ref(o, OP_RV2HV);
	break;

    default:
	warn("oops: oopsHV");
	break;
    }
    return o;
}

OP *
newAVREF(o)
OP *o;
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADAV;
	o->op_ppaddr = ppaddr[OP_PADAV];
	return o;
    }
    return newUNOP(OP_RV2AV, 0, scalar(o));
}

OP *
newGVREF(type,o)
I32 type;
OP *o;
{
    if (type == OP_MAPSTART)
	return newUNOP(OP_NULL, 0, o);
    return ref(newUNOP(OP_RV2GV, OPf_REF, o), type);
}

OP *
newHVREF(o)
OP *o;
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADHV;
	o->op_ppaddr = ppaddr[OP_PADHV];
	return o;
    }
    return newUNOP(OP_RV2HV, 0, scalar(o));
}

OP *
oopsCV(o)
OP *o;
{
    croak("NOT IMPL LINE %d",__LINE__);
    /* STUB */
    return o;
}

OP *
newCVREF(flags, o)
I32 flags;
OP *o;
{
    return newUNOP(OP_RV2CV, flags, scalar(o));
}

OP *
newSVREF(o)
OP *o;
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADSV;
	o->op_ppaddr = ppaddr[OP_PADSV];
	return o;
    }
    return newUNOP(OP_RV2SV, 0, scalar(o));
}

/* Check routines. */

OP *
ck_anoncode(op)
OP *op;
{
    PADOFFSET ix;
    SV* name;

    name = NEWSV(1106,0);
    sv_upgrade(name, SVt_PVNV);
    sv_setpvn(name, "&", 1);
    SvIVX(name) = -1;
    SvNVX(name) = 1;
    ix = pad_alloc(op->op_type, SVs_PADMY);
    av_store(comppad_name, ix, name);
    av_store(comppad, ix, cSVOP->op_sv);
    SvPADMY_on(cSVOP->op_sv);
    cSVOP->op_sv = Nullsv;
    cSVOP->op_targ = ix;
    return op;
}

OP *
ck_bitop(op)
OP *op;
{
    op->op_private = hints;
    return op;
}

OP *
ck_concat(op)
OP *op;
{
    if (cUNOP->op_first->op_type == OP_CONCAT)
	op->op_flags |= OPf_STACKED;
    return op;
}

OP *
ck_spair(op)
OP *op;
{
    if (op->op_flags & OPf_KIDS) {
	OP* newop;
	OP* kid;
	OPCODE type = op->op_type;
	op = modkids(ck_fun(op), type);
	kid = cUNOP->op_first;
	newop = kUNOP->op_first->op_sibling;
	if (newop &&
	    (newop->op_sibling ||
	     !(opargs[newop->op_type] & OA_RETSCALAR) ||
	     newop->op_type == OP_PADAV || newop->op_type == OP_PADHV ||
	     newop->op_type == OP_RV2AV || newop->op_type == OP_RV2HV)) {
	    
	    return op;
	}
	op_free(kUNOP->op_first);
	kUNOP->op_first = newop;
    }
    op->op_ppaddr = ppaddr[++op->op_type];
    return ck_fun(op);
}

OP *
ck_delete(op)
OP *op;
{
    op = ck_fun(op);
    op->op_private = 0;
    if (op->op_flags & OPf_KIDS) {
	OP *kid = cUNOP->op_first;
	if (kid->op_type == OP_HSLICE)
	    op->op_private |= OPpSLICE;
	else if (kid->op_type != OP_HELEM)
	    croak("%s argument is not a HASH element or slice",
		  op_desc[op->op_type]);
	null(kid);
    }
    return op;
}

OP *
ck_eof(op)
OP *op;
{
    I32 type = op->op_type;

    if (op->op_flags & OPf_KIDS) {
	if (cLISTOP->op_first->op_type == OP_STUB) {
	    op_free(op);
	    op = newUNOP(type, OPf_SPECIAL,
               newGVOP(OP_GV, 0, gv_fetchpv("main::ARGV", TRUE, SVt_PVAV)));
	}
	return ck_fun(op);
    }
    return op;
}

OP *
ck_eval(op)
OP *op;
{
    hints |= HINT_BLOCK_SCOPE;
    if (op->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOP->op_first;

	if (!kid) {
	    op->op_flags &= ~OPf_KIDS;
	    null(op);
	}
	else if (kid->op_type == OP_LINESEQ) {
	    LOGOP *enter;

	    kid->op_next = op->op_next;
	    cUNOP->op_first = 0;
	    op_free(op);

	    Newz(1101, enter, 1, LOGOP);
	    enter->op_type = OP_ENTERTRY;
	    enter->op_ppaddr = ppaddr[OP_ENTERTRY];
	    enter->op_private = 0;

	    /* establish postfix order */
	    enter->op_next = (OP*)enter;

	    op = prepend_elem(OP_LINESEQ, (OP*)enter, (OP*)kid);
	    op->op_type = OP_LEAVETRY;
	    op->op_ppaddr = ppaddr[OP_LEAVETRY];
	    enter->op_other = op;
	    return op;
	}
    }
    else {
	op_free(op);
	op = newUNOP(OP_ENTEREVAL, 0, newSVREF(newGVOP(OP_GV, 0, defgv)));
    }
    op->op_targ = (PADOFFSET)hints;
    return op;
}

OP *
ck_exec(op)
OP *op;
{
    OP *kid;
    if (op->op_flags & OPf_STACKED) {
	op = ck_fun(op);
	kid = cUNOP->op_first->op_sibling;
	if (kid->op_type == OP_RV2GV)
	    null(kid);
    }
    else
	op = listkids(op);
    return op;
}

OP *
ck_exists(op)
OP *op;
{
    op = ck_fun(op);
    if (op->op_flags & OPf_KIDS) {
	OP *kid = cUNOP->op_first;
	if (kid->op_type != OP_HELEM)
	    croak("%s argument is not a HASH element", op_desc[op->op_type]);
	null(kid);
    }
    return op;
}

OP *
ck_gvconst(o)
register OP *o;
{
    o = fold_constants(o);
    if (o->op_type == OP_CONST)
	o->op_type = OP_GV;
    return o;
}

OP *
ck_rvconst(op)
register OP *op;
{
    SVOP *kid = (SVOP*)cUNOP->op_first;

    op->op_private |= (hints & HINT_STRICT_REFS);
    if (kid->op_type == OP_CONST) {
	char *name;
	int iscv;
	GV *gv;

	name = SvPV(kid->op_sv, na);
	if ((hints & HINT_STRICT_REFS) && (kid->op_private & OPpCONST_BARE)) {
	    char *badthing = Nullch;
	    switch (op->op_type) {
	    case OP_RV2SV:
		badthing = "a SCALAR";
		break;
	    case OP_RV2AV:
		badthing = "an ARRAY";
		break;
	    case OP_RV2HV:
		badthing = "a HASH";
		break;
	    }
	    if (badthing)
		croak(
	  "Can't use bareword (\"%s\") as %s ref while \"strict refs\" in use",
		      name, badthing);
	}
	kid->op_type = OP_GV;
	iscv = (op->op_type == OP_RV2CV) * 2;
	for (gv = 0; !gv; iscv++) {
	    /*
	     * This is a little tricky.  We only want to add the symbol if we
	     * didn't add it in the lexer.  Otherwise we get duplicate strict
	     * warnings.  But if we didn't add it in the lexer, we must at
	     * least pretend like we wanted to add it even if it existed before,
	     * or we get possible typo warnings.  OPpCONST_ENTERED says
	     * whether the lexer already added THIS instance of this symbol.
	     */
	    gv = gv_fetchpv(name,
		iscv | !(kid->op_private & OPpCONST_ENTERED),
		iscv
		    ? SVt_PVCV
		    : op->op_type == OP_RV2SV
			? SVt_PV
			: op->op_type == OP_RV2AV
			    ? SVt_PVAV
			    : op->op_type == OP_RV2HV
				? SVt_PVHV
				: SVt_PVGV);
	}
	SvREFCNT_dec(kid->op_sv);
	kid->op_sv = SvREFCNT_inc(gv);
    }
    return op;
}

OP *
ck_ftst(op)
OP *op;
{
    I32 type = op->op_type;

    if (op->op_flags & OPf_REF)
	return op;

    if (op->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOP->op_first;

	if (kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE)) {
	    OP *newop = newGVOP(type, OPf_REF,
		gv_fetchpv(SvPVx(kid->op_sv, na), TRUE, SVt_PVIO));
	    op_free(op);
	    return newop;
	}
    }
    else {
	op_free(op);
	if (type == OP_FTTTY)
           return newGVOP(type, OPf_REF, gv_fetchpv("main::STDIN", TRUE,
				SVt_PVIO));
	else
	    return newUNOP(type, 0, newSVREF(newGVOP(OP_GV, 0, defgv)));
    }
    return op;
}

OP *
ck_fun(op)
OP *op;
{
    register OP *kid;
    OP **tokid;
    OP *sibl;
    I32 numargs = 0;
    int type = op->op_type;
    register I32 oa = opargs[type] >> OASHIFT;
    
    if (op->op_flags & OPf_STACKED) {
	if ((oa & OA_OPTIONAL) && (oa >> 4) && !((oa >> 4) & OA_OPTIONAL))
	    oa &= ~OA_OPTIONAL;
	else
	    return no_fh_allowed(op);
    }

    if (op->op_flags & OPf_KIDS) {
	tokid = &cLISTOP->op_first;
	kid = cLISTOP->op_first;
	if (kid->op_type == OP_PUSHMARK ||
	    kid->op_type == OP_NULL && kid->op_targ == OP_PUSHMARK)
	{
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
	if (!kid && opargs[type] & OA_DEFGV)
	    *tokid = kid = newSVREF(newGVOP(OP_GV, 0, defgv));

	while (oa && kid) {
	    numargs++;
	    sibl = kid->op_sibling;
	    switch (oa & 7) {
	    case OA_SCALAR:
		scalar(kid);
		break;
	    case OA_LIST:
		if (oa < 16) {
		    kid = 0;
		    continue;
		}
		else
		    list(kid);
		break;
	    case OA_AVREF:
		if (kid->op_type == OP_CONST &&
		  (kid->op_private & OPpCONST_BARE)) {
		    char *name = SvPVx(((SVOP*)kid)->op_sv, na);
		    OP *newop = newAVREF(newGVOP(OP_GV, 0,
			gv_fetchpv(name, TRUE, SVt_PVAV) ));
		    if (dowarn)
			warn("Array @%s missing the @ in argument %ld of %s()",
			    name, (long)numargs, op_desc[type]);
		    op_free(kid);
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2AV && kid->op_type != OP_PADAV)
		    bad_type(numargs, "array", op_desc[op->op_type], kid);
		mod(kid, type);
		break;
	    case OA_HVREF:
		if (kid->op_type == OP_CONST &&
		  (kid->op_private & OPpCONST_BARE)) {
		    char *name = SvPVx(((SVOP*)kid)->op_sv, na);
		    OP *newop = newHVREF(newGVOP(OP_GV, 0,
			gv_fetchpv(name, TRUE, SVt_PVHV) ));
		    if (dowarn)
			warn("Hash %%%s missing the %% in argument %ld of %s()",
			    name, (long)numargs, op_desc[type]);
		    op_free(kid);
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2HV && kid->op_type != OP_PADHV)
		    bad_type(numargs, "hash", op_desc[op->op_type], kid);
		mod(kid, type);
		break;
	    case OA_CVREF:
		{
		    OP *newop = newUNOP(OP_NULL, 0, kid);
		    kid->op_sibling = 0;
		    linklist(kid);
		    newop->op_next = newop;
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		break;
	    case OA_FILEREF:
		if (kid->op_type != OP_GV) {
		    if (kid->op_type == OP_CONST &&
		      (kid->op_private & OPpCONST_BARE)) {
			OP *newop = newGVOP(OP_GV, 0,
			    gv_fetchpv(SvPVx(((SVOP*)kid)->op_sv, na), TRUE,
					SVt_PVIO) );
			op_free(kid);
			kid = newop;
		    }
		    else {
			kid->op_sibling = 0;
			kid = newUNOP(OP_RV2GV, 0, scalar(kid));
		    }
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		scalar(kid);
		break;
	    case OA_SCALARREF:
		mod(scalar(kid), type);
		break;
	    }
	    oa >>= 4;
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
	op->op_private |= numargs;
	if (kid)
	    return too_many_arguments(op,op_desc[op->op_type]);
	listkids(op);
    }
    else if (opargs[type] & OA_DEFGV) {
	op_free(op);
	return newUNOP(type, 0, newSVREF(newGVOP(OP_GV, 0, defgv)));
    }

    if (oa) {
	while (oa & OA_OPTIONAL)
	    oa >>= 4;
	if (oa && oa != OA_LIST)
	    return too_few_arguments(op,op_desc[op->op_type]);
    }
    return op;
}

OP *
ck_glob(op)
OP *op;
{
    GV *gv;

    if ((op->op_flags & OPf_KIDS) && !cLISTOP->op_first->op_sibling)
	append_elem(OP_GLOB, op, newSVREF(newGVOP(OP_GV, 0, defgv)));

    if (!((gv = gv_fetchpv("glob", FALSE, SVt_PVCV)) && GvIMPORTED_CV(gv)))
	gv = gv_fetchpv("CORE::GLOBAL::glob", FALSE, SVt_PVCV);

    if (gv && GvIMPORTED_CV(gv)) {
	static int glob_index;

	append_elem(OP_GLOB, op,
		    newSVOP(OP_CONST, 0, newSViv(glob_index++)));
	op->op_type = OP_LIST;
	op->op_ppaddr = ppaddr[OP_LIST];
	((LISTOP*)op)->op_first->op_type = OP_PUSHMARK;
	((LISTOP*)op)->op_first->op_ppaddr = ppaddr[OP_PUSHMARK];
	op = newUNOP(OP_ENTERSUB, OPf_STACKED,
		     append_elem(OP_LIST, op, 
				 scalar(newUNOP(OP_RV2CV, 0,
						newGVOP(OP_GV, 0, gv)))));
	op = newUNOP(OP_NULL, 0, ck_subr(op));
	op->op_targ = OP_GLOB;		/* hint at what it used to be */
	return op;
    }
    gv = newGVgen("main");
    gv_IOadd(gv);
    append_elem(OP_GLOB, op, newGVOP(OP_GV, 0, gv));
    scalarkids(op);
    return ck_fun(op);
}

OP *
ck_grep(op)
OP *op;
{
    LOGOP *gwop;
    OP *kid;
    OPCODE type = op->op_type == OP_GREPSTART ? OP_GREPWHILE : OP_MAPWHILE;

    op->op_ppaddr = ppaddr[OP_GREPSTART];
    Newz(1101, gwop, 1, LOGOP);
    
    if (op->op_flags & OPf_STACKED) {
	OP* k;
	op = ck_sort(op);
        kid = cLISTOP->op_first->op_sibling;
	for (k = cLISTOP->op_first->op_sibling->op_next; k; k = k->op_next) {
	    kid = k;
	}
	kid->op_next = (OP*)gwop;
	op->op_flags &= ~OPf_STACKED;
    }
    kid = cLISTOP->op_first->op_sibling;
    if (type == OP_MAPWHILE)
	list(kid);
    else
	scalar(kid);
    op = ck_fun(op);
    if (error_count)
	return op;
    kid = cLISTOP->op_first->op_sibling; 
    if (kid->op_type != OP_NULL)
	croak("panic: ck_grep");
    kid = kUNOP->op_first;

    gwop->op_type = type;
    gwop->op_ppaddr = ppaddr[type];
    gwop->op_first = listkids(op);
    gwop->op_flags |= OPf_KIDS;
    gwop->op_private = 1;
    gwop->op_other = LINKLIST(kid);
    gwop->op_targ = pad_alloc(type, SVs_PADTMP);
    kid->op_next = (OP*)gwop;

    kid = cLISTOP->op_first->op_sibling;
    if (!kid || !kid->op_sibling)
	return too_few_arguments(op,op_desc[op->op_type]);
    for (kid = kid->op_sibling; kid; kid = kid->op_sibling)
	mod(kid, OP_GREPSTART);

    return (OP*)gwop;
}

OP *
ck_index(op)
OP *op;
{
    if (op->op_flags & OPf_KIDS) {
	OP *kid = cLISTOP->op_first->op_sibling;	/* get past pushmark */
	if (kid && kid->op_type == OP_CONST)
	    fbm_compile(((SVOP*)kid)->op_sv);
    }
    return ck_fun(op);
}

OP *
ck_lengthconst(op)
OP *op;
{
    /* XXX length optimization goes here */
    return ck_fun(op);
}

OP *
ck_lfun(op)
OP *op;
{
    OPCODE type = op->op_type;
    return modkids(ck_fun(op), type);
}

OP *
ck_rfun(op)
OP *op;
{
    OPCODE type = op->op_type;
    return refkids(ck_fun(op), type);
}

OP *
ck_listiob(op)
OP *op;
{
    register OP *kid;
    
    kid = cLISTOP->op_first;
    if (!kid) {
	op = force_list(op);
	kid = cLISTOP->op_first;
    }
    if (kid->op_type == OP_PUSHMARK)
	kid = kid->op_sibling;
    if (kid && op->op_flags & OPf_STACKED)
	kid = kid->op_sibling;
    else if (kid && !kid->op_sibling) {		/* print HANDLE; */
	if (kid->op_type == OP_CONST && kid->op_private & OPpCONST_BARE) {
	    op->op_flags |= OPf_STACKED;	/* make it a filehandle */
	    kid = newUNOP(OP_RV2GV, OPf_REF, scalar(kid));
	    cLISTOP->op_first->op_sibling = kid;
	    cLISTOP->op_last = kid;
	    kid = kid->op_sibling;
	}
    }
	
    if (!kid)
	append_elem(op->op_type, op, newSVREF(newGVOP(OP_GV, 0, defgv)) );

    op = listkids(op);

    op->op_private = 0;
#ifdef USE_LOCALE
    if (hints & HINT_LOCALE)
	op->op_private |= OPpLOCALE;
#endif

    return op;
}

OP *
ck_fun_locale(op)
OP *op;
{
    op = ck_fun(op);

    op->op_private = 0;
#ifdef USE_LOCALE
    if (hints & HINT_LOCALE)
	op->op_private |= OPpLOCALE;
#endif

    return op;
}

OP *
ck_scmp(op)
OP *op;
{
    op->op_private = 0;
#ifdef USE_LOCALE
    if (hints & HINT_LOCALE)
	op->op_private |= OPpLOCALE;
#endif

    return op;
}

OP *
ck_match(op)
OP *op;
{
    op->op_private |= OPpRUNTIME;
    return op;
}

OP *
ck_null(op)
OP *op;
{
    return op;
}

OP *
ck_repeat(op)
OP *op;
{
    if (cBINOP->op_first->op_flags & OPf_PARENS) {
	op->op_private |= OPpREPEAT_DOLIST;
	cBINOP->op_first = force_list(cBINOP->op_first);
    }
    else
	scalar(op);
    return op;
}

OP *
ck_require(op)
OP *op;
{
    if (op->op_flags & OPf_KIDS) {	/* Shall we supply missing .pm? */
	SVOP *kid = (SVOP*)cUNOP->op_first;

	if (kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE)) {
	    char *s;
	    for (s = SvPVX(kid->op_sv); *s; s++) {
		if (*s == ':' && s[1] == ':') {
		    *s = '/';
		    Move(s+2, s+1, strlen(s+2)+1, char);
		    --SvCUR(kid->op_sv);
		}
	    }
	    sv_catpvn(kid->op_sv, ".pm", 3);
	}
    }
    return ck_fun(op);
}

OP *
ck_retarget(op)
OP *op;
{
    croak("NOT IMPL LINE %d",__LINE__);
    /* STUB */
    return op;
}

OP *
ck_select(op)
OP *op;
{
    OP* kid;
    if (op->op_flags & OPf_KIDS) {
	kid = cLISTOP->op_first->op_sibling;	/* get past pushmark */
	if (kid && kid->op_sibling) {
	    op->op_type = OP_SSELECT;
	    op->op_ppaddr = ppaddr[OP_SSELECT];
	    op = ck_fun(op);
	    return fold_constants(op);
	}
    }
    op = ck_fun(op);
    kid = cLISTOP->op_first->op_sibling;    /* get past pushmark */
    if (kid && kid->op_type == OP_RV2GV)
	kid->op_private &= ~HINT_STRICT_REFS;
    return op;
}

OP *
ck_shift(op)
OP *op;
{
    I32 type = op->op_type;

    if (!(op->op_flags & OPf_KIDS)) {
	op_free(op);
	return newUNOP(type, 0,
	    scalar(newUNOP(OP_RV2AV, 0,
		scalar(newGVOP(OP_GV, 0, subline 
			       ? defgv 
			       : gv_fetchpv("ARGV", TRUE, SVt_PVAV) )))));
    }
    return scalar(modkids(ck_fun(op), type));
}

OP *
ck_sort(op)
OP *op;
{
    op->op_private = 0;
#ifdef USE_LOCALE
    if (hints & HINT_LOCALE)
	op->op_private |= OPpLOCALE;
#endif

    if (op->op_flags & OPf_STACKED) {
	OP *kid = cLISTOP->op_first->op_sibling;	/* get past pushmark */
	OP *k;
	kid = kUNOP->op_first;				/* get past rv2gv */

	if (kid->op_type == OP_SCOPE || kid->op_type == OP_LEAVE) {
	    linklist(kid);
	    if (kid->op_type == OP_SCOPE) {
		k = kid->op_next;
		kid->op_next = 0;
	    }
	    else if (kid->op_type == OP_LEAVE) {
		if (op->op_type == OP_SORT) {
		    null(kid);			/* wipe out leave */
		    kid->op_next = kid;

		    for (k = kLISTOP->op_first->op_next; k; k = k->op_next) {
			if (k->op_next == kid)
			    k->op_next = 0;
		    }
		}
		else
		    kid->op_next = 0;		/* just disconnect the leave */
		k = kLISTOP->op_first;
	    }
	    peep(k);

	    kid = cLISTOP->op_first->op_sibling;	/* get past pushmark */
	    null(kid);					/* wipe out rv2gv */
	    if (op->op_type == OP_SORT)
		kid->op_next = kid;
	    else
		kid->op_next = k;
	    op->op_flags |= OPf_SPECIAL;
	}
    }

    return op;
}

OP *
ck_split(op)
OP *op;
{
    register OP *kid;
    PMOP* pm;
    
    if (op->op_flags & OPf_STACKED)
	return no_fh_allowed(op);

    kid = cLISTOP->op_first;
    if (kid->op_type != OP_NULL)
	croak("panic: ck_split");
    kid = kid->op_sibling;
    op_free(cLISTOP->op_first);
    cLISTOP->op_first = kid;
    if (!kid) {
	cLISTOP->op_first = kid = newSVOP(OP_CONST, 0, newSVpv(" ", 1));
	cLISTOP->op_last = kid; /* There was only one element previously */
    }

    if (kid->op_type != OP_MATCH) {
	OP *sibl = kid->op_sibling;
	kid->op_sibling = 0;
	kid = pmruntime( newPMOP(OP_MATCH, OPf_SPECIAL), kid, Nullop);
	if (cLISTOP->op_first == cLISTOP->op_last)
	    cLISTOP->op_last = kid;
	cLISTOP->op_first = kid;
	kid->op_sibling = sibl;
    }
    pm = (PMOP*)kid;
    if (pm->op_pmshort && !(pm->op_pmflags & PMf_ALL)) {
	SvREFCNT_dec(pm->op_pmshort);	/* can't use substring to optimize */
	pm->op_pmshort = 0;
    }

    kid->op_type = OP_PUSHRE;
    kid->op_ppaddr = ppaddr[OP_PUSHRE];
    scalar(kid);

    if (!kid->op_sibling)
	append_elem(OP_SPLIT, op, newSVREF(newGVOP(OP_GV, 0, defgv)) );

    kid = kid->op_sibling;
    scalar(kid);

    if (!kid->op_sibling)
	append_elem(OP_SPLIT, op, newSVOP(OP_CONST, 0, newSViv(0)));

    kid = kid->op_sibling;
    scalar(kid);

    if (kid->op_sibling)
	return too_many_arguments(op,op_desc[op->op_type]);

    return op;
}

OP *
ck_subr(op)
OP *op;
{
    OP *prev = ((cUNOP->op_first->op_sibling)
	     ? cUNOP : ((UNOP*)cUNOP->op_first))->op_first;
    OP *o = prev->op_sibling;
    OP *cvop;
    char *proto = 0;
    CV *cv = 0;
    GV *namegv = 0;
    int optional = 0;
    I32 arg = 0;

    for (cvop = o; cvop->op_sibling; cvop = cvop->op_sibling) ;
    if (cvop->op_type == OP_RV2CV) {
	SVOP* tmpop;
	op->op_private |= (cvop->op_private & OPpENTERSUB_AMPER);
	null(cvop);		/* disable rv2cv */
	tmpop = (SVOP*)((UNOP*)cvop)->op_first;
	if (tmpop->op_type == OP_GV) {
	    cv = GvCVu(tmpop->op_sv);
	    if (cv && SvPOK(cv) && !(op->op_private & OPpENTERSUB_AMPER)) {
		namegv = CvANON(cv) ? (GV*)tmpop->op_sv : CvGV(cv);
		proto = SvPV((SV*)cv, na);
	    }
	}
    }
    op->op_private |= (hints & HINT_STRICT_REFS);
    if (PERLDB_SUB && curstash != debstash)
	op->op_private |= OPpENTERSUB_DB;
    while (o != cvop) {
	if (proto) {
	    switch (*proto) {
	    case '\0':
		return too_many_arguments(op, gv_ename(namegv));
	    case ';':
		optional = 1;
		proto++;
		continue;
	    case '$':
		proto++;
		arg++;
		scalar(o);
		break;
	    case '%':
	    case '@':
		list(o);
		arg++;
		break;
	    case '&':
		proto++;
		arg++;
		if (o->op_type != OP_REFGEN && o->op_type != OP_UNDEF)
		    bad_type(arg, "block", gv_ename(namegv), o);
		break;
	    case '*':
		proto++;
		arg++;
		if (o->op_type == OP_RV2GV)
		    goto wrapref;
		{
		    OP* kid = o;
		    o = newUNOP(OP_RV2GV, 0, kid);
		    o->op_sibling = kid->op_sibling;
		    kid->op_sibling = 0;
		    prev->op_sibling = o;
		}
		goto wrapref;
	    case '\\':
		proto++;
		arg++;
		switch (*proto++) {
		case '*':
		    if (o->op_type != OP_RV2GV)
			bad_type(arg, "symbol", gv_ename(namegv), o);
		    goto wrapref;
		case '&':
		    if (o->op_type != OP_RV2CV)
			bad_type(arg, "sub", gv_ename(namegv), o);
		    goto wrapref;
		case '$':
		    if (o->op_type != OP_RV2SV && o->op_type != OP_PADSV)
			bad_type(arg, "scalar", gv_ename(namegv), o);
		    goto wrapref;
		case '@':
		    if (o->op_type != OP_RV2AV && o->op_type != OP_PADAV)
			bad_type(arg, "array", gv_ename(namegv), o);
		    goto wrapref;
		case '%':
		    if (o->op_type != OP_RV2HV && o->op_type != OP_PADHV)
			bad_type(arg, "hash", gv_ename(namegv), o);
		  wrapref:
		    {
			OP* kid = o;
			o = newUNOP(OP_REFGEN, 0, kid);
			o->op_sibling = kid->op_sibling;
			kid->op_sibling = 0;
			prev->op_sibling = o;
		    }
		    break;
		default: goto oops;
		}
		break;
	    case ' ':
		proto++;
		continue;
	    default:
	      oops:
		croak("Malformed prototype for %s: %s",
			gv_ename(namegv), SvPV((SV*)cv, na));
	    }
	}
	else
	    list(o);
	mod(o, OP_ENTERSUB);
	prev = o;
	o = o->op_sibling;
    }
    if (proto && !optional &&
	  (*proto && *proto != '@' && *proto != '%' && *proto != ';'))
	return too_few_arguments(op, gv_ename(namegv));
    return op;
}

OP *
ck_svconst(op)
OP *op;
{
    SvREADONLY_on(cSVOP->op_sv);
    return op;
}

OP *
ck_trunc(op)
OP *op;
{
    if (op->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOP->op_first;

	if (kid->op_type == OP_NULL)
	    kid = (SVOP*)kid->op_sibling;
	if (kid &&
	  kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE))
	    op->op_flags |= OPf_SPECIAL;
    }
    return ck_fun(op);
}

/* A peephole optimizer.  We visit the ops in the order they're to execute. */

void
peep(o)
register OP* o;
{
    register OP* oldop = 0;
    if (!o || o->op_seq)
	return;
    ENTER;
    SAVESPTR(op);
    SAVESPTR(curcop);
    for (; o; o = o->op_next) {
	if (o->op_seq)
	    break;
	if (!op_seqmax)
	    op_seqmax++;
	op = o;
	switch (o->op_type) {
	case OP_NEXTSTATE:
	case OP_DBSTATE:
	    curcop = ((COP*)o);		/* for warnings */
	    o->op_seq = op_seqmax++;
	    break;

	case OP_CONCAT:
	case OP_CONST:
	case OP_JOIN:
	case OP_UC:
	case OP_UCFIRST:
	case OP_LC:
	case OP_LCFIRST:
	case OP_QUOTEMETA:
	    if (o->op_next->op_type == OP_STRINGIFY)
		null(o->op_next);
	    o->op_seq = op_seqmax++;
	    break;
	case OP_STUB:
	    if ((o->op_flags & OPf_WANT) != OPf_WANT_LIST) {
		o->op_seq = op_seqmax++;
		break; /* Scalar stub must produce undef.  List stub is noop */
	    }
	    goto nothin;
	case OP_NULL:
	    if (o->op_targ == OP_NEXTSTATE || o->op_targ == OP_DBSTATE)
		curcop = ((COP*)op);
	    goto nothin;
	case OP_SCALAR:
	case OP_LINESEQ:
	case OP_SCOPE:
	  nothin:
	    if (oldop && o->op_next) {
		oldop->op_next = o->op_next;
		continue;
	    }
	    o->op_seq = op_seqmax++;
	    break;

	case OP_GV:
	    if (o->op_next->op_type == OP_RV2SV) {
		if (!(o->op_next->op_private & OPpDEREF)) {
		    null(o->op_next);
		    o->op_private |= o->op_next->op_private & OPpLVAL_INTRO;
		    o->op_next = o->op_next->op_next;
		    o->op_type = OP_GVSV;
		    o->op_ppaddr = ppaddr[OP_GVSV];
		}
	    }
	    else if (o->op_next->op_type == OP_RV2AV) {
		OP* pop = o->op_next->op_next;
		IV i;
		if (pop->op_type == OP_CONST &&
		    (op = pop->op_next) &&
		    pop->op_next->op_type == OP_AELEM &&
		    !(pop->op_next->op_private &
		      (OPpLVAL_INTRO|OPpLVAL_DEFER|OPpDEREF)) &&
		    (i = SvIV(((SVOP*)pop)->op_sv) - compiling.cop_arybase)
				<= 255 &&
		    i >= 0)
		{
		    SvREFCNT_dec(((SVOP*)pop)->op_sv);
		    null(o->op_next);
		    null(pop->op_next);
		    null(pop);
		    o->op_flags |= pop->op_next->op_flags & OPf_MOD;
		    o->op_next = pop->op_next->op_next;
		    o->op_type = OP_AELEMFAST;
		    o->op_ppaddr = ppaddr[OP_AELEMFAST];
		    o->op_private = (U8)i;
		    GvAVn(((GVOP*)o)->op_gv);
		}
	    }
	    o->op_seq = op_seqmax++;
	    break;

	case OP_MAPWHILE:
	case OP_GREPWHILE:
	case OP_AND:
	case OP_OR:
	    o->op_seq = op_seqmax++;
	    peep(cLOGOP->op_other);
	    break;

	case OP_COND_EXPR:
	    o->op_seq = op_seqmax++;
	    peep(cCONDOP->op_true);
	    peep(cCONDOP->op_false);
	    break;

	case OP_ENTERLOOP:
	    o->op_seq = op_seqmax++;
	    peep(cLOOP->op_redoop);
	    peep(cLOOP->op_nextop);
	    peep(cLOOP->op_lastop);
	    break;

	case OP_MATCH:
	case OP_SUBST:
	    o->op_seq = op_seqmax++;
	    peep(cPMOP->op_pmreplstart);
	    break;

	case OP_EXEC:
	    o->op_seq = op_seqmax++;
	    if (dowarn && o->op_next && o->op_next->op_type == OP_NEXTSTATE) {
		if (o->op_next->op_sibling &&
			o->op_next->op_sibling->op_type != OP_DIE) {
		    line_t oldline = curcop->cop_line;

		    curcop->cop_line = ((COP*)o->op_next)->cop_line;
		    warn("Statement unlikely to be reached");
		    warn("(Maybe you meant system() when you said exec()?)\n");
		    curcop->cop_line = oldline;
		}
	    }
	    break;
	default:
	    o->op_seq = op_seqmax++;
	    break;
	}
	oldop = o;
    }
    LEAVE;
}
