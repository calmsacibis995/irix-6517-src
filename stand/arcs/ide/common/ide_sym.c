
#ident "$Revision: 1.17 $"

/*
 * ide_sym.c - Symbol table manager for ide
 *
 * This is a simple hashed symbol table.
 *
 */

#include <sys/sbd.h>
#include <genpda.h>
#include "libsc.h"
#include "uif.h"
#include "ide.h"
#include "ide_mp.h"
#include <ide_msg.h>

sym_t *symhash[HASHSIZE] = {0};
int did_htab_init = 0;

char *sym_tstrs[] = {

	/* 8 SYM BASETYPES:  0 --> 7 */
/* 0 */	" (bt)-UNDEFINED",
	" (bt)-INT-     ",
	" (bt)-STR -    ",
	" (bt)-RANGE-   ",
	" (bt)-SET-     ",
	" (bt)-SETSTR - ",
	" (bt)-FREE-    ",
/* 7 */	" (bt)-BADVAL-  ",

	/* 9 SYM TYPES:  8 --> 16, plus end pad */
/* 8 */	"type UNDEFINED",
	"type `VAR'    ",
	"type `CONST'  ",
	"type `CMD'",
	"type `CMDFLAG'",
	"type `PARAM'  ",
	"type `GLOBAL' ",
	"type `FREE'   ",
/*16 */	"type `BADVAL' ", 
};
int sym_tstr_tabsiz = (sizeof(sym_tstrs) / sizeof(char *));

#define NF_SPACES	"       ------       "
#define TF_SPACES	"             "
#define BTF_SPACES	"        "

#define SYM_NFIELDSIZ	(strlen(NF_SPACES))	/* sym_name field-width */ 
#define SYM_TFIELDSIZ	(strlen(TF_SPACES))	/* sym_type field-width */ 
#define SYM_BTFIELDSIZ	(strlen(BTF_SPACES))	/* sym_btype field-width */
#define SYM_FFIELDSIZ	20	/* sym_flag field-width */


char *sf_strs[] = {	"perm", "udef", "char*", "diag", "setcmd", "debug" };
int sflag_tblsiz = (sizeof(sf_strs) / sizeof(char *));

#define STR_SYMTARGV	"sym_t*"

char *cmdtype_strs[] = {	"INVALID cmd", "UDEF cmd", "char* DIAG",
				"symt* DIAG", "char* SETOP", "symt* SETOP",
				"char* DBGCMD", "symt* DBGCMD"
};

/* xxxx static int curline = 0; */


static size_t	_dump_name(sym_t *, int *);
static size_t	_dump_type(sym_t *, int *);
static size_t	_dump_btype(sym_t *, int *);
static size_t	_dump_sflags(sym_t *, int *);
static int	_flagcnt(uint);


#define CMD_FLAGS(FLGS) (FLGS & (SYM_DIAG|SYM_SET_CMD|SYM_DEBUG_CMD))

#define IDE_CMD(S_PTR)	\
	(S_PTR->sym_type==SYM_CMD && !(S_PTR->sym_flags & (SYM_DIAG|SYM_UDEF)))

void
_ide_init_hashtab()
{
    int i;

    ASSERT(did_htab_init == 0);

    for (i = 0; i < HASHSIZE; i++)
	symhash[i] = NULL;

    did_htab_init = 1;

    return;

} /* _ide_init_hashtab */

unsigned
makehash(char *s)
{
    unsigned int h = 0;

    while ( *s )
	h = (h << 1) + *s++;
	
    return(h % HASHSIZE);
}

sym_t *
findsym(char *s)
{
    sym_t *psym;

    for ( psym = symhash[makehash(s)]; psym; psym = psym->sym_hlink )
	if ( *psym->sym_name == *s && !strcmp( psym->sym_name, s))
		return psym;
	
    return(0);
}

void
addsym(sym_t *psym)
{
    int h;

    ASSERT(psym);
    h = makehash(psym->sym_name);

    psym->sym_hlink = symhash[h];
    symhash[h] = psym;

    psym->sym_flags |= SYM_INTABLE;

    /*
     * initmstats() sync'ed the memory-allocation stats at
     * the conclusion of IDE's bootup initialization; from then
     * on all permanent symbols (and their strings) must be 
     * tracked from this routine.
     */
    if (_ide_initialized)
	bumpmstats(psym);
}



/* ************ assorted routines for symbol-table display **************  */

/* symbols have BASETYPES and TYPES: */

/*
 * BASETYPES:  < SYMB_UNDEFINED, SYMB_INT, SYMB_STR, SYMB_RANGE, SYMB_SET,
 *		 SYMB_SETSTR, SYMB_FREE, SYMB_BADVAL >
 *
 * TYPES:  < SYM_UNDEFINED, SYM_VAR, SYM_CON, SYM_CMD, SYM_CMDFLAG, 
 *	     SYM_PARAM, SYM_GLOBAL, SYM_FREE, SYM_BADVAL >
 */

#define DO_NOTHING	-2
#define DO_KEY		-1
#define DO_EM_ALL	 0
#define DO_CMDS		 1
#define DO_UDEF		 2
#define DO_DIAGS	 3
#define DO_VARS		 4
#define DO_GLOBALS	 5
#define DO_INTS		 6
#define DO_STRS		 7
#define DO_SETS		 8
#define DO_SETOPS	 9
#define DO_DEBUGS	10
#define DO_CNT		(DO_DEBUGS+1)

char symtype_help [] = "cmds|udefs|diags|vars|globals|ints|strs|sets|setops|debug|all";
/*
 * symbols [-l|-k KEYSTR|-t {cmds|udefs|diags|vars|globals|ints|strs|sets|setops|debug|all}]
 */
int
_dosymbols(int argc, char **argv)
{
    char *systype = inv_findcpu();
    char outbuf[2*MAXLINELEN], *outp = &outbuf[0];
    char *key;
    int i, idx;
    int vid_me = cpuid();
    sym_t *psym;
    int which = DO_NOTHING;
    int printed=0;
    char *cp = 0;
    optlist_t optlist;
    size_t key_len;

    if (argc == 1) {
	_ide_usage(argv[0]);
	return(DO_CNT);
    }

    ASSERT(_sympoolsize);

    if (_SHOWIT(DBG)) {
	msg_printf(DBG,"  %s[==_dosymbols](v%d): ", argv[0], vid_me);
	_dump_argvs(argc, argv);
    }

    if (_get_options(argc, argv, SHOWSYM_OPTS, &optlist)) {
	_ide_usage(argv[0]);
	return(-1);
    }
    *outp = '\0';

    /* xxxx curline = 0; */
    /* 
     * -l lists only the symbol-types available--not even argv[0] 
     */
    if (_opt_index('l', optlist.opts) >= 0) {
	msg_printf(JUST_DOIT, "%s", symtype_help);
	return(-2);
    }

    /*
     * -k matches strlen(key) characters of specified string
     */
    else if ((idx = _opt_index('k', optlist.opts)) >= 0) {
	key = optlist.options[idx].args[0];
	if (!key || (optlist.optcount == 0)) {
	    _ide_usage(argv[0]);
	    return(-3);
	}
	key_len = strlen(key);
        which = DO_KEY;
	outp += sprintf(outp, "symbols matching %d-letter key `%s'",
		key_len, key);
    }
    else if ((idx = _opt_index('t', optlist.opts)) >= 0) {
	if (which != DO_NOTHING) {
	    msg_printf(USER_ERR,"%s %s opts are mutually exclusive\n",
		outbuf, argv[0]);
	    _ide_usage(argv[0]);
	    return(-4);
	}
	cp = optlist.options[idx].args[0];
	if (!cp || (optlist.optcount == 0)) {
	    _ide_usage(argv[0]);
	    return(-5);
	}

	if (!strncasecmp(cp, "cmds", 2)) {
            which = DO_CMDS;
	    outp += sprintf(outp, "IDE Builtin Commands");
	} else if (!strncasecmp(cp, "udef", 2)) {
            which = DO_UDEF;
	    outp += sprintf(outp, "Script-Defined Commands");
	} else if (!strncasecmp(cp, "diags", 2)) {
            which = DO_DIAGS;
	    outp += sprintf(outp, "%s Diagnostics",
		(systype ? systype : "IDE"));
	} else if (!strncasecmp(cp, "vars", 2)) {
            which = DO_VARS;
	    outp += sprintf(outp, "Variables");
	} else if (!strncasecmp(cp, "globals", 2)) {
            which = DO_GLOBALS;
	    outp += sprintf(outp, "Globals");
	} else if (!strncasecmp(cp, "ints", 2)) {
            which = DO_INTS;
	    outp += sprintf(outp, "Integers");
	} else if (!strncasecmp(cp, "strs", 3)) {
            which = DO_STRS;
	    outp += sprintf(outp, "Strings");
	} else if (!strncasecmp(cp, "sets", 4)) {
            which = DO_SETS;
	    outp += sprintf(outp, "CPU Sets");
	} else if (!strncasecmp(cp, "setops", 4)) {
            which = DO_SETOPS;
	    outp += sprintf(outp, "CPU-Set Commands");
	} else if (!strncasecmp(cp, "debug", 3)) {
            which = DO_DEBUGS;
	    outp += sprintf(outp, "IDE Debugging Commands");
	} else if (!strncasecmp(cp, "all", 2)) {
            which = DO_EM_ALL;
	    outp += sprintf(outp, "Permanent Symbols");
	} else {
	    msg_printf(JUST_DOIT, "%s\n%s: ??Unknown type `%s'\n", outbuf,
		argv[0], cp);
	    _ide_usage(argv[0]);
	    return(-6);
	}
    }
    else {
        _ide_usage(argv[0]);
        return(-1);
    }

    ASSERT(which != DO_NOTHING);

    for ( i = 0; i < HASHSIZE; i++ ) {
	for ( psym = symhash[i]; psym; psym = psym->sym_hlink ) {
	    switch(which) {

		case DO_KEY:
		    if (strncasecmp(psym->sym_name, key, key_len)) {
			continue;
		    }
		    break;

		case DO_EM_ALL:
		    break;
		case DO_CMDS:
	            if (!IDE_CMD(psym))
			continue;
		    break;
		case DO_UDEF:
	            if (!(psym->sym_flags & SYM_UDEF))
			continue;
		    break;
		case DO_DIAGS:
	            if (!(psym->sym_flags & SYM_DIAG))
			continue;
		    break;
		case DO_VARS:
	            if (psym->sym_type != SYM_VAR)
			continue;
		    break;
		case DO_GLOBALS:
		    if (psym->sym_type != SYM_GLOBAL)
			continue;
		    break;
		case DO_INTS:
		    if (psym->sym_basetype != SYMB_INT)
			continue;
		    break;
		case DO_STRS:
		    if (psym->sym_basetype != SYMB_STR)
			continue;
		    break;
		case DO_SETS:
		    if ((psym->sym_basetype != SYMB_SET) &&
			(psym->sym_basetype != SYMB_SETSTR))
			continue;
		    break;
		case DO_SETOPS:
		    if (!(psym->sym_flags & SYM_SET_CMD))
			continue;
		    break;
		case DO_DEBUGS:
	            if (!(psym->sym_flags & SYM_DEBUG_CMD))
			continue;
		    break;
		default:
		    msg_printf(IDE_ERR, "\n%s: invalid `which' arg (%d)\n", 
			argv[0], which);
		    return(-7);
	    }
	    if (!printed) {
		msg_printf(JUST_DOIT, "\n-->> List all %s\n\n", outbuf);
/* xxxx 	pager(outbuf, pagesize, &curline);		 */

		outbuf[0] = '\0';
		outp = &outbuf[0];
	    }
	    _dumpsym(psym);
	    printed++;
	}
    }
    if (!printed) {
	msg_printf(JUST_DOIT, "\n  -- No %s found\n", outbuf);
    }

    return(printed);

} /* _dosymbols */

void
dump_symdefs(void)
{
    int i;

    msg_printf(JUST_DOIT, "\tbasetypes:\n");
    for (i = SYMB_UNDEFINED; i < SYMB_END; i++) {
	msg_printf(JUST_DOIT, "\t%d: \"%s\"%s", i, sym_tstrs[i],
		(i&0x1 ? "\n" : ""));
    }
    msg_printf(JUST_DOIT, "\ttypes:\n");
    for (i = SYM_UNDEFINED; i < SYM_END; i++) {
	msg_printf(JUST_DOIT, "\t%d: \"%s\"%s", i, sym_tstrs[i],
		(i&0x1 ? "\n" : ""));
    }

    msg_printf(JUST_DOIT, "\n\tNow one by one\n");

    msg_printf(JUST_DOIT, "\t%d =%s, %d =%s,  %d =%s, %d =%s\n",
		SYMB_UNDEFINED,"SYMB_UNDEFINED", SYMB_INT,"SYMB_INT",
		SYMB_STR,"SYMB_STR", SYMB_RANGE,"SYMB_RANGE");
    msg_printf(JUST_DOIT, "\t%d =%s, %d =%s, %d =%s, %d =%s\n",
		SYMB_SET, "SYMB_SET", SYMB_SETSTR, "SYMB_SETSTR",
		SYMB_FREE, "SYMB_FREE", SYMB_BADVAL, "SYMB_BADVAL");

    msg_printf(JUST_DOIT, "\t%d =%s, %d =%s, %d =%s, %d =%s\n",
		SYM_UNDEFINED,"SYM_UNDEFINED", SYM_VAR,"SYM_VAR",
		SYM_CON,"SYM_CON", SYM_CMD,"SYM_CMD");

    msg_printf(JUST_DOIT, "\t%d =%s, %d =%s, %d =%s, %d =%s, %d =%s\n",
		SYM_CMDFLAG,"SYM_CMDFLAG", SYM_PARAM,"SYM_PARAM",
		SYM_GLOBAL,"SYM_GLOBAL", SYM_FREE,"SYM_FREE",
		SYM_BADVAL,"SYM_BADVAL");

    return;

} /* _dump_symdefs */


static int tabledump = 0;

/*ARGSUSED*/
void
_do_dumphtab(int argc,char **argv)
{
    int i;
    sym_t *psym;

    ASSERT(_sympoolsize);

    tabledump = 1;
    for ( i = 0; i < HASHSIZE; i++ ) {
	for ( psym = symhash[i]; psym; psym = psym->sym_hlink ) {
	    _dumpsym(psym);
	}
    }
    tabledump = 0;

} /* _do_dumphtab */

static int dumpidx = 0;
int
_dumpsym(sym_t *psym)
{
    size_t i, ncnt, tcnt, btcnt, fcnt;

    ASSERT(_sympoolsize);

    if (!psym) {
	msg_printf(IDE_ERR, "\n        ERROR, _dumpsym: NULL psym!\n");
	ide_panic("_dumpsym");
    }

    if ((psym->sym_logidx != ALLOC_LOGIDX) && !DIAG_EXEC(psym->sym_flags))
	msg_printf(IDE_ERR,
		"\n!!-> logidx %d (0x%x) on allocated, non-diag sym (\"%s\")!",
		psym->sym_logidx, psym->sym_logidx, _SAFESTR(psym->sym_name));

    if (!DIAG_EXEC(psym->sym_flags) && (psym->sym_logidx != FREE_LOGIDX &&
		psym->sym_logidx != ALLOC_LOGIDX))
	    msg_printf(IDE_ERR,
		"  !! logidx %d (0x%x) on non-diag sym (\"%s\")!!\n",
		psym->sym_logidx, psym->sym_logidx, _SAFESTR(psym->sym_name));

    if (_STRPTR(psym->sym_name) && !_PERM(psym)) {
	    msg_printf(JUST_DOIT, "temp!!-> ", _XSAFE(psym->sym_name));
    }

    dumpidx = 0;
    ncnt = _dump_name(psym, &dumpidx);
    for (i = ncnt; i < SYM_NFIELDSIZ; i++)
	msg_printf(JUST_DOIT, " ");

    tcnt = _dump_type(psym, &dumpidx);
    for (i = tcnt; i < SYM_TFIELDSIZ; i++)
	msg_printf(JUST_DOIT, " ");

    btcnt = _dump_btype(psym, &dumpidx);
    for (i = btcnt; i < SYM_BTFIELDSIZ; i++)
	msg_printf(JUST_DOIT, " ");

    fcnt = _dump_sflags(psym, &dumpidx);

    if (_SHOWIT(DBG+1))
	msg_printf(JUST_DOIT, " (%d:%d:%d:%d)\n", ncnt, tcnt, btcnt, fcnt);
    else
	msg_printf(JUST_DOIT, "\n");

    return(0);

} /* _dumpsym */

/*ARGSUSED1*/
static size_t
_dump_name(sym_t *psym, int *colcnt)
{
    char outbuf[MAXLINELEN];
    char *outp = &outbuf[0];
    char *altstr = (tabledump ? NF_SPACES : " [ANON] ");
    size_t charcnt=0;

    ASSERT(psym);
    *outp = '\0';

    outp += sprintf(outp, "%s ", (_PERM(psym) ? "perm>" : "temp>"));

    switch( psym->sym_type ) {
        case SYM_VAR:
	case SYM_CON:
	case SYM_GLOBAL:
	case SYM_CMD:
	case SYM_CMDFLAG:
	case SYM_PARAM:
	default:
	    outp += sprintf(outp, "\"%s\": ",
		(_STRPTR(psym->sym_name) ? psym->sym_name : altstr));
	    break;
    }

    ASSERT(outp < &outbuf[MAXLINELEN]);
    charcnt = (outp - &outbuf[0]);

    msg_printf(JUST_DOIT, "%s", outbuf);

    return(charcnt);

} /* _dump_name */

/*ARGSUSED1*/
static size_t
_dump_type(sym_t *psym, int *colcnt)
{
    char outbuf[MAXLINELEN];
    char *outp = &outbuf[0];
    size_t charcnt=0;

    ASSERT(psym);
    *outp = '\0';

    if (!VALID_TYPE(psym->sym_type)) {
	msg_printf(IDE_ERR, "  !!!bad type %d (0x%x) ", psym->sym_type,
		psym->sym_type);
	psym->sym_type = SYM_BADVAL;
    }

    switch( psym->sym_type ) {

        case SYM_VAR:
	    outp += sprintf(outp, "%s ", sym_tstrs[psym->sym_type]);
	    break;
	case SYM_CON:
	    outp += sprintf(outp, "%s ", sym_tstrs[psym->sym_type]);
	    break;
	case SYM_CMD:
/* 	  xxx  whichcmd = cmd_check(psym);		*/

	    if (!DIAG_EXEC(psym->sym_flags)) {
		outp += sprintf(outp, "%s ", sym_tstrs[psym->sym_type]);
	    } else {
		if (LOGIDX_INRANGE(psym->sym_logidx))
		    outp += sprintf(outp, "%s(idx#%d)",
			sym_tstrs[psym->sym_type],
			psym->sym_logidx);
		else
		    outp+=sprintf(outp,"%s (bad idx %d=0x%x) ",
			sym_tstrs[psym->sym_type], psym->sym_logidx,
			psym->sym_logidx);
	    }
	    break;
	case SYM_CMDFLAG:
	    outp += sprintf(outp, "-- (%s) ",
			sym_tstrs[psym->sym_type]);
	    break;
	case SYM_PARAM:
	    outp += sprintf(outp, "-- (%s) ",
			sym_tstrs[psym->sym_type]);
	    break;
	case SYM_GLOBAL:
	    outp += sprintf(outp, "(%s) ", sym_tstrs[psym->sym_type]);
	    break;
	default:
	    outp += sprintf(outp, "\n  !dt->bad type %d ", psym->sym_type);
	    break;
    }
    ASSERT(outp < &outbuf[MAXLINELEN]);
    charcnt = (outp - &outbuf[0]);

    msg_printf(JUST_DOIT, "%s", outbuf);

    return(charcnt);
} /* _dump_type */


/*ARGSUSED2*/
static size_t
_dump_btype(sym_t *psym, int *colcnt)
{
#ifdef PRE_SETSTR_DAYS
    char setbuf[SETBUFSIZ];
#endif
    char outbuf[MAXLINELEN+SETBUFSIZ];
    char *outp = &outbuf[0];
    size_t charcnt=0;

    ASSERT(psym);
    *outp = '\0';

    switch(psym->sym_basetype) {
        case SYMB_INT:
	    outp += sprintf(outp, " INT: %d or 0x%x ", 
		psym->sym_i, psym->sym_i);
	    break;
	case SYMB_STR:
	    outp += sprintf(outp, " STR=\"%s\" ", _SAFESTR(psym->sym_s));
	    break;
	case SYMB_RANGE:
	    outp += sprintf(outp, " RANGE: start 0x%x:cnt 0x%x ",
		psym->sym_start, psym->sym_count);
	    break;
	case SYMB_SET:
#ifdef PRE_SETSTR_DAYS
	    sprintf_cset(setbuf, psym->sym_set);
	    outp += sprintf(outp, " SET %s", setbuf);
#else
	    outp += sprintf(outp, " SET 0Lx%Lx", psym->sym_set);
#endif
	    break;
	case SYMB_SETSTR:
	    outp += sprintf(outp, " SETSTR= \"%s\" ", _SAFESTR(psym->sym_s));
	    break;

	default:
	    /* suppress warning for cases which don't set or use basetype */
	    if ((psym->sym_type == SYM_CMD) ||
		(psym->sym_type == SYM_PARAM) ||
		(psym->sym_flags & SYM_UDEF) ) {
		    if (_SHOWIT(DBG+1)) {
			outp += sprintf(outp, " (btype NA--`%s' == %d) ",
				sym_tstrs[psym->sym_basetype],
				psym->sym_basetype);
		    } else {
			outp += sprintf(outp, "%s", BTF_SPACES);
		    }
		break;
	    }
	    if (psym->sym_basetype == SYMB_UNDEFINED &&
			psym->sym_type == SYM_VAR) {
		outp += sprintf(outp, "[btype %d & SYM_VAR]", 
			psym->sym_basetype);
		if (psym->sym_i) {
		    outp += sprintf(outp, "sym_i %d=0x%x", 
			psym->sym_i, psym->sym_i);
		}
		if (IS_KSEG1(psym->sym_s)) {
		    outp += sprintf(outp,"str \"%s\"",_SAFESTR(psym->sym_s));
		}
		break;
	    }
	    outp += sprintf(outp,"->btype %d: sym_i %d==0x%x, sym_s \"%s\" ",
		    psym->sym_basetype, psym->sym_i, psym->sym_i,
		    _XSAFESTR(psym->sym_s));
	    break;
    }

    ASSERT(outp < &outbuf[MAXLINELEN+SETBUFSIZ]);
    charcnt = (outp - &outbuf[0]);

    msg_printf(JUST_DOIT, "%s", outbuf);

    return(charcnt);
} /* _dump_btype */

/*ARGSUSED1*/
static size_t
_dump_sflags(sym_t *psym, int *colcnt)
{
    uint flags, mutex;
    int i, fcnt, didone=0;
    char outbuf[MAXLINELEN], *outp = &outbuf[0];
    size_t charcnt=0;

    ASSERT(psym);
    *outp = '\0';

    if (!psym) {
	ide_panic("_dump_sflags: NULL psym!");
    }

    flags = psym->sym_flags;
    if (!flags) {
	outp += sprintf(outp, " <no flags>");
    } else {
	mutex = (flags & (SYM_DIAG|SYM_SET_CMD|SYM_DEBUG_CMD));
	fcnt = _flagcnt(mutex);
	if (fcnt > 1) {
	    outp += sprintf(outp,
		"IDE_ERROR: _dumpsflags: mutex violation (0x%x)\n",
		mutex);
	}

	outp += sprintf(outp, " 0x%x:<", flags);
	for (i = 0; i < SYM_FLAG_CNT; i++) {
	    if (flags & (0x1 << i))
		outp += sprintf(outp,"%s%s",(didone++ ? "|" : ""), sf_strs[i]);
	}
	outp += sprintf(outp, ">");

    }
    ASSERT(outp < &outbuf[MAXLINELEN]);
    charcnt = (outp - &outbuf[0]);

    msg_printf(JUST_DOIT, "%s", outbuf);

    return(charcnt);
} /* _dump_sflags */

int
_flagcnt(uint flags)
{
    int loop, total;


    for (total=loop=0; loop < SYM_FLAG_CNT; loop++) {
	if (flags & (0x1 << loop))
	    total++;
    }
#if defined(IDEBUG)
    if (_SHOWIT(VDBG)) {
	if (total)
	    msg_printf(JUST_DOIT, "    flagcnt %d (0x%x)\n",total, flags);
    }
#endif

    return(total);
}

#ifdef NOTYET
int
cmd_check(sym_t *psym)
{


    ASSERT(psym);

    if (psym->sym_types != SYM_CMD) {
	msg_printf(IDE_ERR,
	    "cmd_check: sym \"%s\" is type %d, not %s(%d)\n",
	    _SAFESTR(psym->sym_name), psym->sym_type,
	    _CMDTYPE_STR(SYM_CMD), SYM_CMD);
	_dumpsym(psym);
	return(CMD_INVAL_T);
    }
    if (psym->sym_flags & SYM_UDEF) {
	if ((psym->sym_flags & (MUTEX_FLAGS|SYM_CHARARGV)) != SYM_UDEF) {
		msg_printf(IDE_ERR,
	            "cmd_check: UDEF sym \"%s\": flag mutex violation: ",
		    _SAFESTR(psym->sym_name);
	    	_dump_sflags(psym, &dumpidx);
	}
	return(CMD_UDEF_T);
    }
    if (psym->sym_flags & SYM_DIAG) {
	if ((psym->sym_flags & MUTEX_FLAGS) != SYM_DIAG) {
		msg_printf(IDE_ERR,
	            "cmd_check: DIAG sym \"%s\": flag mutex violation: ",
		    _SAFESTR(psym->sym_name);
		_dump_sflags(psym, &dumpidx);
	}
	return(CHAR_ARGVS(psym) ? CMD_DIAG_T : CMD_SDIAG_T);
    }
    if (psym->sym_flags & SYM_SET_CMD) {
	if ((psym->sym_flags & MUTEX_FLAGS) != SYM_SET_CMD) {
	    msg_printf(IDE_ERR,
	        "cmd_check: SETCMD sym \"%s\": flag mutex violation: ",
		_SAFESTR(psym->sym_name);
	    _dump_sflags(psym, &dumpidx);
	}
	return(CHAR_ARGVS(psym) ? CMD_SET_T : CMD_SSET_T);
    }
    if (psym->sym_flags & SYM_DEBUG_CMD) {
	if ((psym->sym_flags & MUTEX_FLAGS) != SYM_DEBUG_CMD) {
		msg_printf(IDE_ERR,
		    "cmd_check: DEBUG_CMD sym \"%s\": flag mutex violation: ",
		    _SAFESTR(psym->sym_name);
		_dump_sflags(psym, &dumpidx);
	}
	return(CHAR_ARGVS(psym) ? CMD_DEBUG_T : CMD_SDEBUG_T);
    }
    msg_printf(IDE_ERR, "cmd_check, symbol \"%s\": no match!\n",
	_SAFESTR(psym->sym_name);


} /* cmd_check */

#endif /* NOTYET */
