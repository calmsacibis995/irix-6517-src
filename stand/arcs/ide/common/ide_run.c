
#ident "$Revision: 1.33 $"

/*
 * arcs/ide/ide_run.c - Execute an ide parse tree
 *
 */

#include <sys/sbd.h>
#include <genpda.h>
#include <ide_msg.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include "ide.h"
#include "ide_mp.h"
#include "ide_sets.h"


#define	USE_OUR_OWN
#define EVAL_DEBUG

void doprint(context_t *context, node_t *pnode);
void doif(context_t *context, node_t *pnode);
void dowhile(context_t *context, node_t *pnode);
void dofor(context_t *context, node_t *pnode);
void dodo(context_t *context, node_t *pnode);
void doreturn(context_t *context, node_t *pnode);
void doswitch(context_t *context, node_t *pnode);
void dorepeat(context_t *context, node_t *pnode);
int findcase(context_t *, ide_int_type, node_t *, int);
sym_t *eval(context_t *, node_t *);
static int makeargv(context_t *, node_t *, sym_t **);

/* 
 * genpda.h evaluates MULTIPROCESSOR; ensure it's been included
 */
#ifndef __GENPDA_H__
dontcompile("#include ERROR, ide_run.c: genpda.h hasn't been included!")
#endif

/* 
 * dostmt - Top level entry point
 *
 * Establishes a context for "return" statements and initiates execution.
 *
 */

context_t 	global_context;
#define MS1	10000		/* 10 ms */

extern int inrawcmd;/* parsing a diag command that doesn't want parsed args */

/*
 * Create a global context for ide
 *
 * The $0..$n paramters are set up to be the same as the command
 * line arguments to ide.
 *
 */
int
init_context(int argc, char *argv[])
{
	int i;

	for ( i = 0; i < argc; i++ ) {
		global_context.argv[i] = makessym(argv[i]);
		global_context.argc++;
	}

	return 0;
}
/*
 *  handle interrupt - let ide scripts see and react to ^c
 *
 * Whenever ^C is pressed, the user defined function "trap"
 * is executed.  The trap function may be used to reset tty
 * or graphics or anything else.
 */
void
handle_interrupt(void)
{
	sym_t *psym;
	node_t *pnode;

	if ( (psym = findsym("trap")) && psym->sym_type == SYM_CMD ) {
		pnode = makenode();
		pnode->node_psym = psym;
		pnode->node_type = OP_CMD;
		dostmt(pnode);
		destroynode(pnode);
		}
	close_noncons();	/* close leftover fd's */
}
/*
 * dotree - execute a tree within a given context
 *
 */
void
dotree(context_t *context, node_t *pnode)
{
#ifdef _STANDALONE
	_scandevs();
#endif

	if ( ! pnode )
		return;

	switch ( pnode->node_type ) {
		case OP_STMT:
			dotree(context, pnode->node_right);
			dotree(context, pnode->node_left);
			break;
		case OP_PRINT:
			doprint(context, pnode);
			break;
		case OP_IF:
			doif(context, pnode);
			break;
		case OP_WHILE:
			dowhile(context, pnode);
			break;
		case OP_FOR:
			dofor( context, pnode);
			break;
		case OP_DO:
			dodo( context, pnode);
			break;
		case OP_RETURN:
			doreturn(context,pnode);
			break;
		case OP_BREAK:
			longjmp(context->lframe, 2);
			break;
		case OP_CONTINUE:
			longjmp(context->lframe, 3);
			break;
		case OP_SWITCH:
			doswitch(context,pnode);
			break;
		case OP_REPEAT:
			dorepeat(context,pnode);
			break;
		default:
			destroysym(eval(context, pnode));
			break;
		}
}
/*
 * Top level entry point into command executor
 *
 */
void
dostmt(node_t *pnode)
{

	if ( setjmp(global_context.rframe) )
		return;

	dotree(&global_context, pnode);

	return;
}


/*
 * Promote symbol to integer
 *
 * This routine is used whenever the context of an expression requires
 * an integer and we don't have one.
 *
 * - If the symbol is a string (basetype SYMB_STR), this routine will try
 *   to figure out if it looks like a string of digits and then converts
 *   it to binary (like awk).  The symbol's basetype is then SYMB_INT.
 *
 * - If symbol's basetype is SYMB_SETSTR, the sym_s field is a pointer to
 *   a string of ascii digits representing virtual cpu numbers, separated
 *   by commas.  This string is converted back to the bitfield representation
 *   used by the sym_set field: basetype SYMB_SET.
 *
 * - Symbols with basetypes of SYMB_INT and SYMB_SET therefore need no 
 *   conversion; pro_int returns the symbol unchanged.
 *
 * - pro_int checks for symbols with SYMB_RANGE or invalid basetypes.
 */

sym_t *
pro_int(sym_t *psym)
{
	char setbuf[SETBUFSIZ];
	sym_t *tmp;
	int	v = 0;
	int	base = 10;
	char	*p;

	if (!psym)
		ide_panic("pro_int: NULL psym");

	msg_printf(DBG+2,
		"pro_int sym \"%s\", initial strval \"%s\" type %d btype %d\n",
		_SAFESTR(psym->sym_name), _SAFESTR(psym->sym_s),
		psym->sym_type, psym->sym_basetype);

	/*
	 * SYMB_SET and SYMB_INT are already in pro_int()'s output format,
	 * and SYMB_RANGE shouldn't be here.
	 */
	switch (psym->sym_basetype) {

	    case SYMB_SET:
		sprintf_cset(setbuf, psym->sym_set);
		msg_printf(JUST_DOIT,
		    "  pro_int, symname \"%s\": btype SYMB_SET, type %d (%s)\n",
			_SAFESTR(psym->sym_name), psym->sym_type, setbuf);
		if (psym->sym_type != SYM_VAR) {
			msg_printf(IDE_ERR,
			    "!pro_int 0x%x=\"%s\", SET: type %d not VAR (%d)\n",
			    (__psunsigned_t)psym->sym_name, _SAFESTR(psym->sym_name),
			    psym->sym_type, SYM_VAR);
			msg_printf(JUST_DOIT, "    val 0x%x=%d, or \"%s\"\n",
			    psym->sym_i, psym->sym_i, _SAFESTR(psym->sym_s));
		}
		return psym;

	    case SYMB_INT:
		if (psym->sym_type != SYM_VAR && psym->sym_type != SYM_CON &&
		    psym->sym_type != SYM_UNDEFINED) {
			msg_printf(IDE_ERR,
			    "pro_int (0x%x=\"%s\"): btype INT, bad type %d\n  ",
			    (__psunsigned_t)psym->sym_name, _SAFESTR(psym->sym_name),
			    psym->sym_type);
			msg_printf(JUST_DOIT, " val 0x%x or \"%s\"\n",
				psym->sym_i, _XSAFESTR(psym->sym_s));
		}
		return psym;

	    case SYMB_RANGE:
		if (psym->sym_type != SYM_VAR && psym->sym_type != SYM_CON) {
			msg_printf(IDE_ERR,
			"pro_int RANGE (%d): type %d, not VAR or CON\n",
			psym->sym_basetype, psym->sym_type);
		}
		return psym;

	    case SYMB_SETSTR:
		msg_printf(DBG,
		    "pro_int SETSTR,pre: \"%s\": type %d, bt %d, _s \"%s\"\n",
			_SAFESTR(psym->sym_name), psym->sym_type, 
			psym->sym_basetype, _SAFESTR(psym->sym_s));
		tmp = pro_set(psym);
		ASSERT(tmp);
		msg_printf(DBG,
		    "pro_int SETSTR, after: NOW type %d, btype %d _i 0x%x\n",
		    tmp->sym_type, tmp->sym_basetype, tmp->sym_i);
		destroysym(psym);
		return(tmp);

	    case SYMB_STR:
		msg_printf(DBG+2,
		    "pro_int(STR=%d),pre:  name \"%s\", \"%s\", type %d\n",
		    psym->sym_type, _SAFESTR(psym->sym_name),
		    _SAFESTR(psym->sym_s), psym->sym_type);
		tmp = makesym();
		tmp->sym_basetype = SYMB_INT;
		tmp->sym_type = SYM_VAR;

		p = psym->sym_s;
		ASSERT(p);
		/*
		 * figure out the base the string wants to pack
		 */
		if ( *p == '0' ) {
			base = 8;
			p++;
			if ( *p == 'x' || *p == 'X' ) {
				base = 16;
				p++;
			}
			else
			if ( *p == 'b' || *p == 'B' ) {
				base = 2;
				p++;
			}
		}

		for ( ;isxdigit( *p ); p++ ) {		/* string-->int */
			if ( *p > '9' && base < 16 )
				break;
			if ( *p >= 'a' && *p <= 'f' )
				v = (v * base) + (*p - 'a') + 10;
			else
			if ( *p >= 'A' && *p <= 'F' )
				v = (v * base) + (*p - 'A') + 10;
			else
			if ( (*p - '0') >= base )
				break;
			v = (v * base) + (*p - '0');
	    	}
		if ( *p )	/* error in string */
			tmp->sym_i = 0;
		else
			tmp->sym_i = v;

		msg_printf(DBG+2,
		    "pro_int(STR) after:  outval 0Lx%Lx, btype %d type %d\n",
		    tmp->sym_set, tmp->sym_basetype, tmp->sym_type);
		destroysym(psym);
		return(tmp);

	    default:	/* possibly returning an invalid symbol from userdef */
		tmp = makesym();
		msg_printf(IDE_ERR,
		    "!!pro_int, default: \"%s\": bad btype %d, type %d\n",
		    _SAFESTR(psym->sym_name), psym->sym_basetype, 
		    psym->sym_type);
		msg_printf(DBG,"    _i 0Lx%Lx (\"%s\"), type %d flags 0x%x\n",
		    psym->sym_set, _XSAFESTR(psym->sym_s), psym->sym_type,
		    psym->sym_flags);
		tmp->sym_basetype = SYMB_INT;
		tmp->sym_type = SYM_VAR;
		tmp->sym_i = (int)0x80000000;	/* 2's comp NEGMAXINT */
		destroysym(psym);
		return(tmp);

	} /* switch */
	/*NOTREACHED*/

} /* pro_int */

/*
 * promote to string
 *
 * This routine is called whenever a string is needed.  It is much
 * simpler to convert to strings than to integers.
 * SYMB_INT and SYMB_SET symbols ---> SYMB_STR and SYMB_SETSTR, resp.
 * SYMB_STR and SYMB_SETSTR just return.
 */
sym_t *
pro_str(sym_t *psym)
{
	sym_t	*tmp;
	char	v[MAXSTRLEN];
	char	r[32];
	int	rp  = 0;
	char	*vp = v;
	ide_int_type	p;

	if (!psym)
		ide_panic("pro_str: NULL psym");

	msg_printf(DBG+2,
		"pro_str, symname \"%s\", _i %d _set 0Lx%x, type %d btype %d\n",
		_SAFESTR(psym->sym_name), psym->sym_i, psym->sym_set,
		psym->sym_type, psym->sym_basetype);

	switch(psym->sym_basetype) {

	    case SYMB_STR:
	    case SYMB_SETSTR:
		switch(psym->sym_type) {
		    case SYM_CON:
		    case SYM_PARAM:
		    case SYM_VAR:
			msg_printf(DBG+3,
			    "    symbol \"%s\", type %d btype %d ok: return\n",
			    _SAFESTR(psym->sym_name), psym->sym_type,
			    psym->sym_basetype);
			return(psym);

		    default:
			msg_printf(IDE_ERR,
			   "  ??bt %d: type %d, not CON, VAR, or PARAM\n",
			    psym->sym_basetype, psym->sym_type);
			return(psym);

		} /* sym_type switch */
		/* NOTREACHED */

	    case SYMB_SET:
		tmp = pro_setstr(psym);
		ASSERT(tmp);

		if (tmp->sym_basetype == SYMB_SETSTR) {
		    msg_printf(DBG+2,
			"pro_str(SETSTR): setstr _s \"%s\", type %d\n",
			_SAFESTR(tmp->sym_s), tmp->sym_type);
		} else {
		    msg_printf(IDE_ERR,
			"pro_str(t%d,bt%d): name\"%s\",setval=0Lx%Lx: %s\n",
			tmp->sym_type, tmp->sym_basetype,
			_SAFESTR(tmp->sym_name), tmp->sym_set,
			"needed SETSTR btype");
		}
		destroysym(psym);
		return(tmp);

	    case SYMB_INT:
		tmp = makesym();
		tmp->sym_basetype = SYMB_STR;
		tmp->sym_type = SYM_CON;
		break;

	    case SYMB_RANGE:
		msg_printf(IDE_ERR,
		   "??btype RANGE(%d), type %d: pro_str not applicable\n",
		    psym->sym_basetype, psym->sym_type);
		return(psym);

	    default:	/* possibly attempted echo of nonexistent symbol */
		msg_printf(USER_ERR, " invalid symbol \"%s\", restart\n",
			_SAFESTR(psym->sym_name));
		msg_printf(JUST_DOIT, "!!->pro_str, sym \"%s\": bad btype %d\n",
			_SAFESTR(psym->sym_name), psym->sym_basetype);
		msg_printf(JUST_DOIT, "    _i 0x%x, type %d, flags 0x%x\n",
			psym->sym_i, psym->sym_type, psym->sym_flags);
		badflag = 1;
		tmp = makesym();
		tmp->sym_basetype = SYMB_BADVAL;
		tmp->sym_type = SYM_BADVAL;
		tmp->sym_i = 0xdeadbeef;
		destroysym(psym);
			/* longjmp(intr_jump, SCRIPT_ERR_RESTART); */
			/* !! 		psignal(SIGINT); */
		return(tmp);
		/*NOTREACHED*/

	} /* btype switch */

	/* 
	 * only SYMB_INT basetypes remain: convert to ascii string
	 */
	if ( (p = psym->sym_i) < 0 ) {
		*vp++ = '-';
		p = -p;
	}
	do {
		r[rp++] = (char)((p % 10)+'0');
		p /= 10;
	} while ( p );
	while ( --rp >= 0 )
			*vp++ = r[rp];
	*vp = '\0';
	tmp->sym_s = makestr(v);
	msg_printf((EVAL_DBG|VDBG+1), "    pro_str: int %d --> str \"%s\"\n",
		psym->sym_i, _SAFESTR(tmp->sym_s));

	if ((Reportlevel & EVAL_DBG) || Reportlevel > VDBG) {
		msg_printf(JUST_DOIT, "    end pro_str, tmp: ");
		_dumpsym(tmp);
	}

	destroysym(psym);
	return tmp;

} /* pro_str */

/*
 * pro_set(): set symbols have basetypes of either SYMB_SET or SYMB_SETSTR.
 * SYMB_SET symbols use an unsigned long long bitfield to represent the
 * members in the 'sym_set' field of 'sym_t' structs.  SYMB_SETSTR uses an
 * ascii string of the format "0,1,2,3..", pointed to by the `sym_s' field.
 * pro_set() creates a new temporary symbol by promoting "psym's" symbol 
 * from btype SYMB_SET to SYMB_SETSTR.  For all other incoming basetypes,
 * pro_set displays an IDE_ERR message and returns a negative errno.
 *
 * pro_set() returns a sym_t*  ptr to the promoted set.
 *
 * Set layout is defined with member 0 represented by high bit of most
 * significant byte of unsigned long long.
 */
sym_t *
pro_set(sym_t *psym)
{
	char setbuf[SETBUFSIZ];
	sym_t *tmp;
	int bogus_bt=0, vcnt=0;
	option_t vidstr;
	vplist_t vidlist;

	if (!psym)
		ide_panic("pro_set: NULL symbol ptr!");

	msg_printf((EVAL_DBG|DBG+2),
	    "pro_set, name \"%s\":  &_s 0x%x, _s\"%s\", type %d btype %d\n",
	    _SAFESTR(psym->sym_name), (__psunsigned_t)psym->sym_s,
	    _SAFESTR(psym->sym_s), psym->sym_type, psym->sym_basetype);

	switch (psym->sym_basetype) {

	    case SYMB_SET:
	    	msg_printf((EVAL_DBG|DBG+2),
		    "pro_set: already btype %d; _set 0Lx%Lx\n", psym->sym_set);
		return(psym);

	    case SYMB_SETSTR:	/* string --> bitfield, type SYM_VAR */
	    	msg_printf((EVAL_DBG|DBG+1),
		    "pro_set, btype SYMB_SETSTR:  sym_s 0x%x or \"%s\"\n",
		    (__psunsigned_t)psym->sym_s, _SAFESTR(psym->sym_s));
		/* 
		 * use _get_arglist to break the comma-separated ascii
		 * vid-string into a list of args
		 */
		if (_get_arglist(psym->sym_s, &vidstr)) {	/* PUNT! */
	    	    msg_printf(IDE_ERR,
			"pro_set, SETSTR: %s failed get_arglist\n",
			_SAFESTR(psym->sym_name));
		    return(psym);
		}

		/* ascii-->binary and validate the vids */
		if (_check_vpids(&vidstr, &vidlist)) {
	    	    msg_printf(IDE_ERR,
			"pro_set, SETSTR: %s failed _check_vpids\n",
			_SAFESTR(psym->sym_name));
		    return(psym);
		}
		ASSERT(vidlist.vpcnt >= 0);

		tmp = makesym();
		ASSERT(tmp);

		tmp->sym_set = 0;
		for (vcnt = 0; vcnt < vidlist.vpcnt; vcnt++)
			tmp->sym_set |= (SET_ZEROBIT>>vidlist.vps[vcnt]);

		tmp->sym_basetype = SYMB_SET;
		tmp->sym_type = SYM_CON;

		if ((Reportlevel & EVAL_DBG) || Reportlevel > VDBG) {
		    sprintf_cset(setbuf, tmp->sym_set); 
	    	    msg_printf((EVAL_DBG|DBG+1),
			"pro_set: return a %d-cpu SYM_CON SYMB_SET: \"%s\"\n",
			vcnt, _SAFESTR(setbuf));
		}
		destroysym(psym);
		return(tmp);

	    default:
		bogus_bt = 1;	/* protect sym_tstrs array-access */
	    case SYMB_INT:
	    case SYMB_STR:
	    case SYMB_RANGE:
	    	msg_printf(IDE_ERR,
		    "pro_set, \"%s\":  bt %s(%d), _i %d, &_s 0x%x _s\"%s\"\n",
		    _SAFESTR(psym->sym_name),
		    (bogus_bt ? "?" : sym_tstrs[psym->sym_basetype]),
		    psym->sym_basetype, psym->sym_i, (__psunsigned_t)psym->sym_s,
		    _SAFESTR(psym->sym_s));
		return(psym);
	}
	/*NOTREACHED*/

} /* pro_set */


/*
 * pro_setstr() is the reverse of pro_set--promotes symbols of basetype
 * SYMB_SETSTR to SYMB_SET.  See pro_set() explanation.
 */
sym_t *
pro_setstr(sym_t *psym)
{
	char setbuf[SETBUFSIZ];
	char *cp= &setbuf[0];
	sym_t *tmp;
	setmask_t mask;
	set_t ullval;
	int i, bogus_bt=0, vcnt=0;

	if (!psym)
		ide_panic("pro_setstr: NULL symbol ptr!");

	msg_printf((EVAL_DBG|DBG+2),
	    "pro_setstr, name \"%s\":  _set 0Lx%Lx, type %d, btype %d\n",
	    _SAFESTR(psym->sym_name), psym->sym_set, psym->sym_type,
	    psym->sym_basetype);

	switch (psym->sym_basetype) {

	    case SYMB_SET:	/* bitfield --> ascii string, type SYM_CON */
		tmp = makesym();
		ASSERT(tmp);
		msg_printf((EVAL_DBG|DBG+2), "  : ");
		tmp->sym_type = SYM_CON;
		tmp->sym_basetype = SYMB_SETSTR;
		ullval = psym->sym_set;
		if (!ullval) {
		    tmp->sym_s = makestr("");
		    ASSERT(tmp->sym_s);
		    break;
		}

		mask = SET_ZEROBIT;
		for (i = 0; i < MAXSETSIZE; i++) {
		    if (mask & ullval)
			cp += sprintf(cp,"%s%d", (vcnt++?",":""), i);
		    mask >>= 1;
		}
		ASSERT(cp < (&setbuf[SETBUFSIZ]));
		tmp->sym_s = makestr(setbuf);
		ASSERT(tmp->sym_s);

	    	msg_printf((EVAL_DBG|DBG+1),
		    "pro_setstr: return a %d-cpu, SYM_CON SETSTR: \"%s\"\n",
		    vcnt, _SAFESTR(tmp->sym_s));
		break;

	    case SYMB_SETSTR:
	    	msg_printf((EVAL_DBG|DBG+2),
		    "--> already SYMB_SETSTR: type %d:  _s 0x%x, _s \"%s\"\n",
		    psym->sym_type, (__psunsigned_t)psym->sym_s, _SAFESTR(psym->sym_s));
		return(psym);

	    default:
		bogus_bt = 1;	/* protect sym_tstrs array-access */
	    case SYMB_INT:
	    case SYMB_STR:
	    case SYMB_RANGE:
	    	msg_printf(IDE_ERR,
		    "pro_setstr, \"%s\": bt %s(%d), _i %d, _s 0x%x _s\"%s\"\n",
		    _SAFESTR(psym->sym_name),
		    (bogus_bt ? "?" : sym_tstrs[psym->sym_basetype]),
		    psym->sym_basetype, psym->sym_i,(__psunsigned_t)psym->sym_s,
		    _SAFESTR(psym->sym_s));
		return(psym);

	}

	destroysym(psym);
	return(tmp);

} /* pro_setstr */


#if defined(STKDEBUG)
extern int _dcache_size, cp_dcache_size, cpied;
extern uint get_sp(void);
extern __psunsigned_t set_watch(__psunsigned_t, int);

int did_set_wwatch=0;
int doset_wwatch=0;
#endif /* STKDEBUG */

/*
 * docmd - run a command or function.
 *
 * There are two different types of commands:
 *  1. Those defined by the user as an ide function
 *  2. Those defined by ide as builtins.
 *
 */
sym_t *
docmd(context_t *context, node_t *pnode)
{
	context_t ccontext;
	sym_t *cmd = pnode->node_psym;
	sym_t *vsym = makesym();
	long rc;
	int i;
#ifdef EVEREST
	char *fnnamep = NULL;
	int bufndx;
#endif

#if defined(STKDEBUG)
	uint _SP = get_sp();

	if (cpied && (cp_dcache_size != _dcache_size)) {
		msg_printf(JUST_DOIT, 
		"!!!??eval: cp_size 0x%x _dc_size 0x%x cpied %d: sp 0x%x\n",
			cp_dcache_size, _dcache_size, cpied, _SP);
		cp_dcache_size = _dcache_size;
	}

#ifdef NOTNOW
	if (_SP <= bos) {
	    msg_printf(IDE_ERR, 
		"docmd: stack overflow!!! (tos 0x%x bos 0x%x sp 0x%x)\n",
		tos, bos, _SP);
	} else if (_SP <= safe_sp) {
	    msg_printf(JUST_DOIT, 
		"  !!docmd warning: < 0x%x bytes of stack remain:\n", 
		(safe_sp-bos));
	    msg_printf(JUST_DOIT, "    tos 0x%x bos 0x%x sp 0x%x)\n",
		tos, bos, _SP);
	}
#endif /* NOTNOW */
#endif /* STKDEBUG */

	ccontext = *context;

	if ( rc = setjmp(ccontext.rframe) ) {
		--rc;
		goto exit;
	}

	ccontext.argc = makeargv(context,pnode->node_right,&ccontext.argv[1])+1;
	ccontext.argv[0] = cmd;

	if ( cmd->sym_flags & SYM_UDEF ) {

#if defined(STKDEBUG)
		if (doset_wwatch && !did_set_wwatch) {
			__psunsigned_t k1addr = (__psunsigned_t)&_dcache_size;
			__psunsigned_t paddr = K1_TO_PHYS(k1addr);

			msg_printf(DBG, " eval, udef command:\n");
			msg_printf(DBG, 
				"&_dc_size 0x%Lx; val 0x%x cp_ 0x%x, sp 0x%x\n",
				paddr, _dcache_size, cp_dcache_size, _SP);
			
			msg_printf(DBG,
				"  set store-watchpt at paddr 0x%x\n", paddr);
			set_watch(paddr, WATCHLO_WTRAP);
			did_set_wwatch = 1;
		}
#endif /* STKDEBUG */

		dotree(&ccontext, cmd->sym_stmt);
	} else
	if ( cmd->sym_flags & SYM_CHARARGV ) {
		char *argv[MAXARGC+1];

#ifdef OPTION_DEBUG
		if (Reportlevel & OPTVDBG) {
			int i;

			msg_printf(DBG+4, "  docmd, sym_charargv!\n");
			for (i = 0; i < ccontext.argc; i++) {
				msg_printf(DBG+4, "\n  argv[%d]: ",i);
				if (Reportlevel >= DBG)
				    _dumpsym(ccontext.argv[i]);
			}
			msg_printf(DBG+4, "\n");
		}
#endif
		argv[0] = cmd->sym_name;
#ifdef EVEREST
		fnnamep = argv[0];
#endif

		for ( i = 1; i < ccontext.argc; i++ ) {
		    ccontext.argv[i] = pro_str(ccontext.argv[i]);
		    argv[i] = ccontext.argv[i]->sym_s;
		}
		argv[ccontext.argc] = 0;
#if SET_DEBUG
		if (_SHOWIT(EVAL_DBG|DBG+4)) {
		    msg_printf(JUST_DOIT, 
			"\n  docmd(char): ccntxt.argc %d\n  argvs: ",
			ccontext.argc);
		    for ( i = 0; i < ccontext.argc; i++ ) {
			msg_printf(JUST_DOIT,"  %d=\"%s\"",
				i, _SAFESTR(argv[i]));
		    }
		    msg_printf((EVAL_DBG|DBG+4), "\n");
		}
#endif
		rc = ide_dispatch(cmd->sym_func, ccontext.argc, argv);

	} else {	/* args in sym_t * format */

#ifdef EVEREST
		fnnamep = ccontext.argv[0]->sym_name;
#endif
#if SET_DEBUG
		if (_SHOWIT(EVAL_DBG|DBG+4)) {
		    msg_printf(JUST_DOIT, 
			"\n  docmd(sym_t): ccntxt.argc %d\n  argvs: ",
			ccontext.argc);
		    for ( i = 0; i < ccontext.argc; i++ ) {
			sym_t *tsym = ccontext.argv[i];

			if (tsym->sym_basetype == SYMB_STR) {
			    msg_printf(JUST_DOIT,
				"  (%d) \"%s\"", i, _SAFESTR(tsym->sym_s));
			} else {
			    msg_printf(JUST_DOIT,
				"  (%d) 0x%x (bt%d,t %d) ", i, tsym->sym_i,
				tsym->sym_basetype, tsym->sym_type);
			}
		    }
		    msg_printf(JUST_DOIT, "\n");
		}
#endif /* SET_DEBUG */

#if EVEREST
		if (ID_DIAG(cmd->sym_flags)) {
			msg_printf(SUM, "  %s:  ",fnnamep);
		}
 		bufndx = IDE_ME(putbufndx);
#endif /* EVEREST */

		rc = (*cmd->sym_func)(ccontext.argc, (void **)ccontext.argv);

#if EVEREST
		if (ID_DIAG(cmd->sym_flags)) {
		    char *test_result;

		    switch (rc) {
			case TEST_PASSED:
			    test_result = "PASSED";
			    break;
			case TEST_SKIPPED:
			    test_result = "SKIPPED";
			    break;
			default:
			    test_result = "FAILED";
		    }
		    msg_printf(JUST_DOIT, "%s%s",
			(bufndx == (IDE_ME(putbufndx)) ? " -- " : "\n    "),
			test_result);
		    if (Reportlevel > SUM)
			msg_printf(SUM, ": returned %d\n",rc);
		    else
			msg_printf(SUM, "\n");
		}
#endif /* EVEREST */
	}

	exit:

	for ( i = 1; i < ccontext.argc; i++ )
		destroysym(ccontext.argv[i]);
	
	vsym->sym_basetype = SYMB_INT;
	vsym->sym_type = SYM_CON;
	vsym->sym_i = rc;

	return vsym;

} /* fn docmd */


/*
 * build up an argv for a command
 *
 */
/*ARGSUSED3*/
static int
makeargv(context_t *context, node_t *pnode, sym_t **argv)
{
	int argc;

	if ( ! pnode )
		return 0;

	argc = makeargv( context, pnode->node_right, argv );

	if ( argc < MAXARGC )
		argv[argc++]= eval( context, pnode->node_left );

	return argc;
}
/*
 * doif - Do an if-then-else statement
 *
 */
void
doif(context_t *context, node_t *pnode)
{
	sym_t *tmp = pro_int(eval(context, pnode->node_left));

	us_delay (MS1); /* LATER */
	if ( pnode->node_right && pnode->node_right->node_type == OP_ELSE ) {
		if ( tmp->sym_i )
			dotree(context,pnode->node_right->node_left);
		else
			dotree(context,pnode->node_right->node_right);
		}
	else
		if ( tmp->sym_i)
			dotree(context,pnode->node_right);
	
	destroysym(tmp);
}
/*
 * dorepeat - do a repeat loop
 *
 */
void
dorepeat(context_t *context, node_t *pnode)
{
	ide_int_type tmp;

	tmp = pnode->node_psym->sym_i;

	if ( tmp )
		while( tmp-- )
			dotree(context, pnode->node_right);
	else
		while( 1 )
			dotree(context, pnode->node_right);

}
/*
 * dowhile - do a while loop
 *
 */
void
dowhile(context_t *context, node_t *pnode)
{
	sym_t *tmp;
	int done = 0;

	do	{
		tmp = pro_int(eval(context, pnode->node_left));
		if ( tmp->sym_i ) {
			context_t lcontext;
			int s;

			lcontext = *context;
			if ( s = setjmp( lcontext.lframe ) ) {
				if ( s == 2 ) /* break */
					done = 1;
				}
			else
				dotree(&lcontext, pnode->node_right);
			}
		else
			done = 1;
		destroysym(tmp);
		} while(!done);

}
/*
 * dofor - do a for loop
 *
 */
void
dofor(context_t *context, node_t *pnode)
{
	node_t *pre;
	node_t *con;
	node_t *end;
	node_t *bod;
	sym_t *tmp;
	context_t lcontext;
	int s;

	pre = pnode->node_left->node_left;
	con = pnode->node_left->node_right;
	end = pnode->node_right->node_left;
	bod = pnode->node_right->node_right;

	if (pre) destroysym(eval(context,pre));

	do	{
		if ( con ) {
			tmp = pro_int(eval(context, con));
			if ( ! tmp->sym_i ) {
				destroysym(tmp);
				break;
				}
			destroysym(tmp);
			}

		lcontext = *context;

		if ( s = setjmp( lcontext.lframe ) ) {
			if ( s == 2 ) /* break */
				break;
			}
		else
			dotree(&lcontext, bod);

		if ( end )
			destroysym(eval(context, end));
		} while (1);

}
/*
 * doreturn - return statement
 *
 */
void
doreturn(context_t *context, node_t *pnode)
{
	sym_t *rsym;
	int rc;

	if (pnode->node_right) {
		rsym = pro_int(eval(context,pnode->node_right));
		rc = (int)rsym->sym_i;
		destroysym(rsym);
		}
	else
		rc = 0;

	longjmp(context->rframe, rc+1);
	/*NOTREACHED*/
}
/*
 * dodo - do a do loop
 *
 */
void
dodo(context_t *context, node_t *pnode)
{
	sym_t *tmp;
	int done = 0;
	context_t lcontext;
	int s;

	do	{
		lcontext = *context;
		if ( s = setjmp( lcontext.lframe ) ) {
			if ( s == 2 ) { /* break = 2 */
				done = 1;
				destroysym(tmp);
				break;
				}
			/* continue = 1 */
			}
		else
			dotree(&lcontext, pnode->node_right);

		tmp = pro_int(eval(context, pnode->node_left));

		if ( !tmp->sym_i )
			done = 1;

		destroysym(tmp);

		} while(!done);
}

void
doswitch(context_t *context, node_t *pnode)
{
	context_t scontext;
	ide_int_type val;
	sym_t *psym = pro_int(eval(context, pnode->node_left));

	val = psym->sym_i;

	destroysym(psym);

	scontext = *context;

	if ( setjmp(scontext.lframe) )
		return; /* break */

	if (!findcase(&scontext, val, pnode->node_right, 0))
		findcase(&scontext, val, pnode->node_right, 1);
}

int
findcase(context_t *context, ide_int_type val, node_t *pnode, int flag)
{
	int found = 0;

	if ( ! pnode )
		return 0;

	if ( pnode->node_type == OP_CASELIST ) {
		found = findcase(context, val, pnode->node_right, flag);
		pnode = pnode->node_left;
		}
	
	if ( ! found ) {
		if ( flag && pnode->node_psym )
			return 0;

		if (!flag && !pnode->node_psym )
			return 0; /* hit default case but still looking */

		if (!flag && pnode->node_psym->sym_i != val)
			return 0;
		
		}
	
	dotree(context, pnode->node_right);

	return 1;
}

void
doprint(context_t *context, node_t *pnode)
{
	char setbuffer[SETBUFSIZ];
	sym_t *tmp = eval(context, pnode->node_right);

	if ( pnode->node_psym )
		msg_printf(JUST_DOIT, pnode->node_psym->sym_s, tmp->sym_i);
	else
	switch( tmp->sym_basetype ) {

		case SYMB_INT:
			msg_printf(JUST_DOIT, "INT value = %d\n", tmp->sym_i );
			break;
		case SYMB_STR:
			msg_printf(JUST_DOIT, "STRing = \"%s\"\n", tmp->sym_s );
			break;
		case SYMB_SET:
			_dumpsym(tmp);
			sprintf_cset(setbuffer, tmp->sym_set);
			msg_printf(JUST_DOIT, "SET = lx%Lx: (%s)\n", 
				tmp->sym_set, setbuffer);
#if PRO_DEBUG
			msg_printf(JUST_DOIT, "or: ");
			tmp = pro_setstr(tmp);
			_dumpsym(tmp);
#endif
			break;
		case SYMB_SETSTR:
			msg_printf(JUST_DOIT, "SETSTR = \"%s\"\n",
				tmp->sym_s );
			break;
		case SYMB_RANGE:
			msg_printf(JUST_DOIT,"range: start 0x%x, count 0x%x\n",
				tmp->sym_start, tmp->sym_count);
			break;
		case SYMB_FREE:
			msg_printf(JUST_DOIT,"symbol is unallocated\n");
			break;

		case SYMB_UNDEFINED:
			msg_printf(JUST_DOIT,
			    "symbol is allocated, but basetype is undefined\n");
			break;

		case SYMB_BADVAL:
			msg_printf(JUST_DOIT,
		            "symbol basetype was flagged as invalid\n");
			break;

		default:
			msg_printf(IDE_ERR,
			    "doprint, bad basetype %d=0x%x, (type %d)\n", 
				tmp->sym_basetype, tmp->sym_basetype,
				tmp->sym_type );
			break;
		}
	
	destroysym(tmp);

}


/*
 * eval - expression evaluator
 *
 * This is the heart of the ide interpreter.  Given an expression
 * tree this routine returns a symbol record containing the result
 * of the evaluation. The symbol returned is a temporary symbol and
 * should be destroyed after it is used.
 */
sym_t *
eval(context_t *context, node_t *pnode)
{
	sym_t *rsym;
	sym_t *lsym;
	sym_t *vsym;

	us_delay(MS1); /* LATER */

#ifdef EVAL_DEBUG
	msg_printf((EVAL_DBG|DBG+4), "begin eval: nodetype %d:\n",
		pnode->node_type);
#endif
	switch ( pnode->node_type ) {

	    /* VAR node types: parameters and legit cases for all 4 basetypes */
	    case OP_VAR:
		lsym = pnode->node_psym;
		vsym = makesym();
		if (_SHOWIT(EVAL_DBG|DBG+4)) {
	/* */		msg_printf(JUST_DOIT,
	/* */		    " +++Top of OP_VAR, lsym(@ 0x%x): ", (__psunsigned_t)lsym);
			_dumpsym(lsym);
		}

		if ( lsym->sym_type == SYM_PARAM ) {
	/* */		msg_printf((EVAL_DBG|DBG+4),
			    "  !+++- SYM_PARAM: -> %s <--\n",
			    (inrawcmd ? "RAW mode" : "COOKED mode"));

			/* `$$' (==argc including argv[0], unlike csh's `$$' */
			if ( lsym->sym_indx == MAXARGC+1 ) {
			    ASSERT(lsym->sym_type == SYM_PARAM);
	/* */		    msg_printf((EVAL_DBG|DBG+4)," #PARM: $$ ");
			    vsym->sym_basetype = SYMB_INT;
			    vsym->sym_type = SYM_VAR;
			    vsym->sym_i = context->argc;
			    break;
			}
			else	 /* $n where n > argc  */
			if ( lsym->sym_indx >= context->argc ) {
			    ASSERT(lsym->sym_type == SYM_PARAM);
	/* */		    msg_printf((EVAL_DBG|DBG+4), " #PARM: $n > argc ");
			    vsym->sym_basetype = SYMB_STR;
			    vsym->sym_type = SYM_VAR;
			    vsym->sym_s = makestr("");
			    break;
			}
			else { /* $n where n <= argc  */
			    ASSERT(lsym->sym_type == SYM_PARAM);
	/* */		    msg_printf((EVAL_DBG|DBG+4),
	/* */			" #PARM: n <= argc; (idx %d)\n",lsym->sym_indx);
			    lsym = context->argv[lsym->sym_indx];
			}
		} /* if SYM_PARAM */
			
	/* */	if (_SHOWIT(EVAL_DBG|DBG+4)) {
	/* */		msg_printf((EVAL_DBG|DBG+4)," P+++post SYMPAR: lsym: ");
	/* */		_dumpsym(lsym);
	/* */		msg_printf((EVAL_DBG|DBG+4),"  P+++: and vsym: ");
	/* */		_dumpsym(vsym);
	/* */   }
/* 5 legal basetypes */
		switch (lsym->sym_basetype) {
		    case SYMB_STR:
			vsym->sym_s = makestr(lsym->sym_s);
			ASSERT(vsym->sym_s);
	/* */		msg_printf((EVAL_DBG|DBG+4),
	/* */		    " >1 eval:type OP_VAR bt STR (l:\"%s\" v:\"%s\")\n",
	/* */		    _SAFESTR(lsym->sym_s), _SAFESTR(vsym->sym_s));
			break;

		    case SYMB_SETSTR:
			vsym->sym_s = makestr(lsym->sym_s);
			ASSERT(vsym->sym_s);
	/* */		msg_printf((EVAL_DBG|DBG+4),
	/* */		    " >2 eval:t OP_VAR bt SETSTR (l:\"%s\" v:\"%s\")\n",
	/* */		    _SAFESTR(lsym->sym_s), _SAFESTR(vsym->sym_s));
			break;

		    case SYMB_RANGE:
			vsym->sym_start = lsym->sym_start;
			vsym->sym_count = lsym->sym_count;
	/* */		msg_printf((EVAL_DBG|DBG+4),
	/* */		    " >3 eval: OP_VAR bt RNG:lstart %d, vstart %d\n",
	/* */		    lsym->sym_start, vsym->sym_start);
			break;

		    case SYMB_INT:
			vsym->sym_i = lsym->sym_i;
	/* */		msg_printf((EVAL_DBG|DBG+4),
	/* */		    " >4 eval: OP_VAR, bt INT lval %d vval\n",
	/* */		    lsym->sym_i, vsym->sym_i);
			break;

		    case SYMB_SET:
			vsym->sym_set = lsym->sym_set;
	/* */		msg_printf((EVAL_DBG|DBG+4),
	/* */		    " >5 eval: OP_VAR, bt SET(l: lx%Lx, v: lx%Lx)\n",
	/* */		    lsym->sym_set, vsym->sym_set);
			break;

		    default:	/* unknown basetype */
	/* */		msg_printf(IDE_ERR,
	/* */		    "!! >XXX 77 eval OP_VAR: bad lsym b-type (%d)\n",
	/* */			lsym->sym_basetype);
	/* */		msg_printf(JUST_DOIT,
	/* */		"    >X88 lsym type %d, val 0x%x or \"%s\"\n",
	/* */		    lsym->sym_type,lsym->sym_i,_XSAFESTR(lsym->sym_s));
			_dumpsym(lsym);
			destroysym(lsym);
#ifdef EXTREME_METHOD
			psignal(SIGINT);
			/*NOTREACHED*/
#else
	/* */		msg_printf(JUST_DOIT,
				">!!! SET BADFLAG, default case of OP_VAR\n");
			badflag = 1;
			vsym->sym_basetype = SYMB_BADVAL;
			vsym->sym_type = SYM_BADVAL;
			vsym->sym_i = 0xdeadbeef;
			break;
#endif
		} /* lsym->sym_basetype switch */

		vsym->sym_basetype = lsym->sym_basetype;
		vsym->sym_type = SYM_VAR;

	/* */	if (_SHOWIT(EVAL_DBG|DBG+4)) {
	/* */		msg_printf(JUST_DOIT,
			    "+++>7 END OP_VAR, vsym (addr 0x%x): ",(__psunsigned_t)vsym);
	/* */		_dumpsym(vsym);
	/* */	}
		break;

		case OP_ADD:
			rsym = eval(context, pnode->node_right);
			lsym = eval(context, pnode->node_left);
			vsym = makesym();

			ASSERT(rsym->sym_basetype != SYMB_SET);

			if ( rsym->sym_basetype == SYMB_STR &&
			     lsym->sym_basetype == SYMB_STR  ) {
				vsym->sym_basetype = SYMB_STR;
				vsym->sym_s = talloc( strlen(rsym->sym_s)+
						strlen(lsym->sym_s) + 1 );
				strcpy( vsym->sym_s, lsym->sym_s );
				strcat( vsym->sym_s, rsym->sym_s );
				}
			else	{
				rsym = pro_int(rsym);
				lsym = pro_int(lsym);
				vsym->sym_basetype = SYMB_INT;
				vsym->sym_i = lsym->sym_i + rsym->sym_i;
				}
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_SUB:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i - rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_MUL:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i * rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_DIV:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			if ( rsym->sym_i == 0 )
				vsym->sym_i = DIV0RESULT;
			else
				vsym->sym_i = lsym->sym_i / rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_MOD:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			if ( rsym->sym_i == 0 )
				vsym->sym_i = DIV0RESULT;
			else
				vsym->sym_i = lsym->sym_i % rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_OR:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i | rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_AND:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i & rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_XOR:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i ^ rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_COM: /* complement */
			rsym = pro_int(eval(context, pnode->node_right));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i =  ~rsym->sym_i;
			destroysym(rsym);
			break;
		case OP_NOT: /* logical not */
			rsym = pro_int(eval(context, pnode->node_right));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			if ( rsym->sym_i )
				vsym->sym_i = 0;
			else
				vsym->sym_i = 1;
			destroysym(rsym);
			break;
		case OP_ANDAND:
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;

			/* C evaluation semantics */
			if ( lsym->sym_i ) {
				rsym = pro_int(eval(context,pnode->node_right));
				if ( rsym->sym_i )
					vsym->sym_i = 1;
				destroysym(rsym);
				}
			else
				vsym->sym_i = 0;

			destroysym(lsym);
			break;
		case OP_OROR:
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;

			/* C evaluation semantics */
			if ( ! lsym->sym_i ) {
				rsym = pro_int(eval(context,pnode->node_right));
				if ( rsym->sym_i )
					vsym->sym_i = 1;
				destroysym(rsym);
				}
			else
				vsym->sym_i = 1;

			destroysym(lsym);
			break;
		case OP_LT:
		case OP_LE:
		case OP_GT:
		case OP_GE:
		case OP_EQ:
		case OP_NE:
			{
			ide_int_type i;
			rsym = eval(context, pnode->node_right);
			lsym = eval(context, pnode->node_left);

			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;

			if ( rsym->sym_basetype == SYMB_STR &&
			     lsym->sym_basetype == SYMB_STR  )
				i = strcmp(lsym->sym_s, rsym->sym_s);
			else	{
				rsym = pro_int(rsym);
				lsym = pro_int(lsym);

				i = lsym->sym_i - rsym->sym_i;
				}

			switch(pnode->node_type) {
				case OP_EQ:
					vsym->sym_i = (i == 0);
					break;
				case OP_NE:
					vsym->sym_i = (i != 0);
					break;
				case OP_GT:
					vsym->sym_i = (i >  0);
					break;
				case OP_LT:
					vsym->sym_i = (i <  0);
					break;
				case OP_GE:
					vsym->sym_i = (i >= 0);
					break;
				case OP_LE:
					vsym->sym_i = (i <= 0);
					break;
				}
			destroysym(rsym);
			destroysym(lsym);
			}
			break;
		case OP_POSTD:
			lsym = pnode->node_psym;
			if ( lsym->sym_basetype == SYMB_STR ) {
				destroystr(lsym->sym_s);
				lsym->sym_s = NULL;
				}
			lsym->sym_basetype = SYMB_INT;
			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i--;
			break;
		case OP_POSTI:
			lsym = pnode->node_psym;
			if ( lsym->sym_basetype == SYMB_STR ) {
				destroystr(lsym->sym_s);
				lsym->sym_s = NULL;
				}
			lsym->sym_basetype = SYMB_INT;
			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i++;
			break;
		case OP_PRED:
			lsym = pnode->node_psym;
			if ( lsym->sym_basetype == SYMB_STR ) {
				destroystr(lsym->sym_s);
				lsym->sym_s = NULL;
				}
			lsym->sym_basetype = SYMB_INT;
			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = --lsym->sym_i;
			break;
		case OP_PREI:
			lsym = pnode->node_psym;
			if ( lsym->sym_basetype == SYMB_STR ) {
				destroystr(lsym->sym_s);
				lsym->sym_s = NULL;
				}
			lsym->sym_basetype = SYMB_INT;
			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = ++lsym->sym_i;
			break;
		case OP_SHL:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i << rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;
		case OP_SHR:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pro_int(eval(context, pnode->node_left));
			vsym = makesym();
			vsym->sym_basetype = SYMB_INT;
			vsym->sym_i = lsym->sym_i >> rsym->sym_i;
			destroysym(rsym);
			destroysym(lsym);
			break;

		case OP_ASSIGN:

			rsym = eval(context, pnode->node_right);
			lsym = pnode->node_psym;
#ifdef EVAL_DEBUG
	/* */		if (Reportlevel >= DBG+4 || (Reportlevel & EVAL_DBG)) {
	    		    msg_printf((EVAL_DBG|DBG+4),
				" ***OP_ASS TOP: nodetype %d:\n",
				pnode->node_type);
			    msg_printf((EVAL_DBG|DBG+4),
				" ***OP_ASS: RIGHTSYM: -> ");
			    _dumpsym(rsym);
			    msg_printf((EVAL_DBG|DBG+4),
				" *** OP_ASS: LEFTSYM: ->  ");
			    _dumpsym(lsym);
			}
#endif /* EVAL_DEBUG */

			if (lsym->sym_basetype == SYMB_STR ||
			    lsym->sym_basetype == SYMB_SETSTR) {
				destroystr(lsym->sym_s);
				lsym->sym_s = NULL;
				}

			if ( lsym->sym_type == SYM_GLOBAL ) {
				if (_SHOWIT(EVAL_DBG|DBG+4)) {
				    msg_printf((EVAL_DBG|DBG+4),
				    	"**** CALLING set_global; LEFTsym:\n");
				    _dumpsym(lsym);
				    msg_printf((EVAL_DBG|DBG+4),"RIGHTsym:\n");
				    _dumpsym(rsym);
			        }
				set_global(lsym, rsym);
			} else {
				switch (rsym->sym_basetype) {
				    case SYMB_INT:
					lsym->sym_basetype = SYMB_INT;
					lsym->sym_i = rsym->sym_i;
					break;
				    case SYMB_STR:
					lsym->sym_basetype = SYMB_STR;
					lsym->sym_s = makestr(rsym->sym_s);
					break;
				    case SYMB_SETSTR:
					lsym->sym_basetype = SYMB_SETSTR;
					lsym->sym_s = makestr(rsym->sym_s);
					break;
				    case SYMB_SET:
					lsym->sym_basetype = SYMB_SET;
					lsym->sym_set = rsym->sym_set;
					break;
				    default:
					msg_printf(IDE_ERR,
					    "eval OP_ASS: bad rsym btype %d:\n",
					    rsym->sym_basetype);
					_dumpsym(rsym);
					break;
				}
				if (_SHOWIT(EVAL_DBG|DBG+4)) {
				    msg_printf(JUST_DOIT,
					"pre-destroy: RIGHTsym:\n");
					_dumpsym(rsym);
				}
				destroysym(rsym);
			}
			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_type = lsym->sym_type;
			if (lsym->sym_basetype == SYMB_STR ||
			    lsym->sym_basetype == SYMB_SETSTR)
				vsym->sym_s = makestr(lsym->sym_s);
			else
			if (lsym->sym_basetype == SYMB_SET)
				vsym->sym_set = lsym->sym_set;
			else
				vsym->sym_i = lsym->sym_i;

	/* */		if (_SHOWIT(EVAL_DBG|DBG+4)) {
	/* */			msg_printf((EVAL_DBG|DBG+4), 
	/* */			    " ****>END OP_ASS, vsym (@0x%x): ", vsym);
				_dumpsym(vsym);
	/* */			msg_printf((EVAL_DBG|DBG+4), 
	/* */			    "      ****>lsym (@0x%x): ", lsym);
				_dumpsym(lsym);
			}
			break;

		case OP_SUBASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i -= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_ADDASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i += rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_MULASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i *= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_DIVASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;

			if ( rsym->sym_i == 0 )
				lsym->sym_i = DIV0RESULT;
			else
				lsym->sym_i /= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_MODASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;

			if ( rsym->sym_i == 0 )
				lsym->sym_i = DIV0RESULT;
			else
				lsym->sym_i %= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_SHLASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i <<= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_SHRASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i >>= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_ANDASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i &= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_ORASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i |= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_XORASS:
			rsym = pro_int(eval(context, pnode->node_right));
			lsym = pnode->node_psym;

			lsym->sym_basetype = SYMB_INT;
			lsym->sym_i ^= rsym->sym_i;

			vsym = makesym();
			vsym->sym_basetype = lsym->sym_basetype;
			vsym->sym_i = lsym->sym_i;
			destroysym(rsym);
			break;
		case OP_CMD:
			vsym = docmd(context,pnode);
			break;
		default:
			vsym = makesym();
			vsym->sym_basetype = SYMB_BADVAL;
			vsym->sym_type = SYM_BADVAL;
			msg_printf(IDE_ERR, "eval default: bad node type %d\n",
				pnode->node_type);
			break;

	} /* switch pnode->node_type */

/* */	if (_SHOWIT(EVAL_DBG|DBG+4)) {
/* */		msg_printf((EVAL_DBG|DBG+4), 
/* */			"  bottom of eval, vsym (@0x%x): ", vsym);
/* */		_dumpsym(vsym);
/* */	}
	
	return vsym;

} /* eval */

