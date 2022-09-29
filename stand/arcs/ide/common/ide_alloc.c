
/*
 * ide_alloc.c: ide memory allocation routines
 *
 * All memory is allocated from static pools.
 *
 */

#ident "$Revision: 1.39 $"

#include <sys/sbd.h>
#include <ide_msg.h>
#include "libsc.h"
#include "uif.h"
#include "ide.h"

static void destroyexpr(node_t *);
static void destroyif(node_t *);
static void destroyfor(node_t *);
static void destroyrepeat(node_t *);
static void destroycmd(node_t *);
static void destroyswitch(node_t *);
static void destroycaselist(node_t *);
static void destroywhile(node_t *);


/* 
 * talloc allocates memory from one of five pools, containing 
 * strings of 16, 32, 64, 128, and 256 bytes.  
 *
 * The larger size memory-pools result in an IDE executable
 * that bothers the ARCS-prom loader.  Everest prom doesn't
 * mind (and needs more symbols for MP tests anyway).
 */
#if !defined(EVEREST)

#if (defined(IP22) || defined(IP28) || defined(IP30))
#define NEWSYMC 3000            /* total number of symbol records */
#define NEWNODEC 9800           /* total number of node records */
#define NSTR1   1024            /* size of 16 character string pool */
#define NSTR2   512             /* size of 32 character string pool */
#define NSTR3   256             /* size of 64 character string pool */
#else 
#define NEWSYMC  1280 /* total number of symbol records */
#define NEWNODEC 4096	/* total number of node records */
#define NSTR1 256	/* size of 16 character string pool */
#define NSTR2 256	/* size of 32 character string pool */
#define NSTR3 128	/* size of 64 character string pool */
#endif
#define NSTR4 128	/* size of 128 character string pool */
#define NSTR5 64	/* size of 256 character string pool */
#else
#define NEWSYMC  4096	/* total number of symbol records */
#define NEWNODEC 16384	/* total number of node records */

#define NSTR1 2048	/* size of 16 character string pool */
#define NSTR2 1024	/* size of 32 character string pool */
#define NSTR3 256	/* size of 64 character string pool */
#define NSTR4 256	/* size of 128 character string pool */
#define NSTR5 256	/* size of 256 character string pool */
#endif /* !EVEREST */

#define TOTALSTRS	(NSTR1 + NSTR2 + NSTR3 + NSTR4 + NSTR5)

sym_t	*freesym;	/* list of freed symbol records */
sym_t	new__sym[NEWSYMC];/* symbol record pool */
int	new__symc;	/* index into newsym */

node_t	*freenode;	/* list of freed node records */
node_t	new__node[NEWNODEC];/* node record pool */
int	new__nodec;	/* index into newnode */

/*
 * The following arrays are used for string allocation.
 * Strings are allocated on a best fit of a fixed length
 * storage block. A null byte in the first position of an
 * allocation string indicates that the string is free.
 */
char	string1[NSTR1][STR1LEN];
char	string2[NSTR2][STR2LEN];
char	string3[NSTR3][STR3LEN];
char	string4[NSTR4][STR4LEN];
char	string5[NSTR5][STR5LEN];

int	last1;	/* last string allocated from string1 */
int	last2;	/*  "     "        "      "   string2 */
int	last3;	/*  "     "        "      "   string3 */
int	last4;	/*  "     "        "      "   string4 */
int	last5;	/*  "     "        "      "   string5 */
int	_total_strs = TOTALSTRS;


/* 
 * keep stats on string-, symbol-, and node-pools
 */

/* allocate an extra slot (index 0) for combined totals */
#define NUMSTRSTATS	(NUMSTRPOOLS+1)

typedef struct alloc_info {
	int used;
	int builtins;
	int perm;
	int prev_used;
} alloc_info_t;

alloc_info_t		_nodes;
alloc_info_t		_syms;
alloc_info_t		strstats[NUMSTRSTATS];

int _sympoolsize=0;	

#define nnodes		_nodes.used
#define nsyms		_syms.used
#define STRTOTS		0	/* slot 0 for totals of all 5 strlen pools */
#define STR1SLOT	1
#define STR2SLOT	2
#define STR3SLOT	3
#define STR4SLOT	4
#define STR5SLOT	5

#define nstrs	strstats[STRTOTS].used
#define nstr1	strstats[STR1SLOT].used
#define nstr2	strstats[STR2SLOT].used
#define nstr3	strstats[STR3SLOT].used
#define nstr4	strstats[STR4SLOT].used
#define nstr5	strstats[STR5SLOT].used


#define FREE_ZERO	0xBAD
#define ALLOC_ZERO	0xDAB

extern int	inrawcmd; /* parsing diag cmd that doesn't want parsed args */
extern int	_do_dumphtab(int, char **);

static void	zero_sym(sym_t *, int);
static void	zero_node(node_t *);
static int	pool_idx(char *);


int
_initsyms(void)
{
	int i;

	ASSERT(!_sympoolsize);

	for (i = 0; i < NEWSYMC; i++) {
		zero_sym(&new__sym[i], FREE_ZERO);
	}
	_sympoolsize = i;

	return(i);
}

/*
 * ide_init() calls initmstats() after filling the hash table with all
 * the builtins from awk-created .c files.  initmstats() snaps the 
 * counts to save a record of the resources required by the builtins
 * symbols.  After '_ide_initialized' is true, addsym() bumps the counts
 * when new permanent symbols are created.
 */
int
initmstats()
{
	int i;
	alloc_info_t *ap;

	_nodes.perm =  _nodes.builtins = _nodes.prev_used = _nodes.used;
	_syms.perm = _syms.builtins = _syms.prev_used = _syms.used;

	for (i = 0; i < NUMSTRSTATS; i++) {
		ap = &strstats[i];
		ap->perm=ap->builtins=ap->prev_used = ap->used;
	}	
	if (Reportlevel > INFO) {
		msg_printf(JUST_DOIT, "  Initial memory-allocation stats:\n");
		(void)showmstats(0, 0);
	}

	if (_Verbose >= 3)
		dump_symdefs();

	if (_Verbose >= 2)
		_do_dumphtab(0, (char **)0);

	return(0);

} /* initmstats */

/*ARGSUSED*/
int
showmstats(int argc, sym_t *argv[])
{
	register int i;

	msg_printf(JUST_DOIT, "\n  NODES: %d of %d used (prev %d):  %d free\n", 
		nnodes, NEWNODEC, _nodes.prev_used, NEWNODEC-nnodes);
	msg_printf(JUST_DOIT,"  SYMBOLS: %d of %d used (prev %d): %d free\n",
		nsyms, NEWSYMC, _syms.prev_used, NEWSYMC-nsyms);
	msg_printf(JUST_DOIT,"  STRINGS: %d of %d used (prev %d): %d free\n",
		nstrs, _total_strs, strstats[STRTOTS].prev_used,
		(_total_strs-nstrs));
	msg_printf(JUST_DOIT,
	    "    --> %d(prev %d) x %d, %d(%d) x %d, %d(%d) x %d\n",
	    nstr1, strstats[1].prev_used, STR1LEN,
	    nstr2, strstats[2].prev_used, STR2LEN,
	    nstr3, strstats[3].prev_used, STR3LEN);
	msg_printf(JUST_DOIT,
	    "        %d (%d) x %d, %d (%d) x %d\n\n",
	    nstr4, strstats[4].prev_used, STR4LEN,
	    nstr5, strstats[5].prev_used, STR5LEN);

	_nodes.prev_used =  nnodes;
	_syms.prev_used = nsyms;
	for (i = 0; i < NUMSTRSTATS; i++) {
		strstats[i].prev_used = strstats[i].used;
	}	

	/* optional info */
	msg_printf(VDBG, 
		"  Builtins required %d symbols and %d strings (%d nodes)\n\n",
		_syms.builtins, strstats[0].builtins, _nodes.builtins);

	/* VERY optional info */
	msg_printf(VDBG+1, 
		"  freesym 0x%x, new_symc %d; freenode 0x%x, new_nodec %d\n\n",
		freesym, new__symc, freenode, new__nodec);

	return 0;
} /* showmstats */

int
bumpmstats(sym_t *psym)
{
	int idx;
	int _error=0;

	msg_printf(DBG, "    <bumpmstats: psym 0x%x>\n", psym);

	if (!_ide_initialized) {
		msg_printf(IDE_ERR,
		    "!!--> bad bumpmstats() call (!ide_initialized)\n");
		return(-1);
	}
	ASSERT(psym);

	/*
	 * bump `perm' and `prev_used' counts to keep everything consistent.
	 * (talloc() and makesym() already adjusted the `temp' fields).
	 */
	_syms.perm++;
	_syms.prev_used++;

	if (psym->sym_name) {

#ifndef IP21
		ASSERT(_ITSAPTR(psym->sym_name));
#endif
		if (idx = pool_idx(psym->sym_name)) {	/* can't be 0 */
			strstats[idx].perm++;
			strstats[idx].prev_used++;
			strstats[STRTOTS].perm++;
			strstats[STRTOTS].prev_used++;
		} else {
			msg_printf(IDE_ERR,
				"    !!namestr (0x%x) not from pool; idx %d\n",
				psym->sym_name, idx);
			_error--;
		}
	}
	if (psym->sym_basetype==SYMB_STR || psym->sym_basetype==SYMB_SETSTR) { 
#ifndef IP21
		ASSERT(_ITSAPTR(psym->sym_s));
#endif
		if (idx = pool_idx(psym->sym_s)) {
			strstats[idx].perm++;
			strstats[idx].prev_used++;
			strstats[STRTOTS].perm++;
			strstats[STRTOTS].prev_used++;
		} else {
			msg_printf(IDE_ERR,
				"    !!sym_s (0x%x) not from pool; idx %d\n",
				psym->sym_s, idx);
			_error--;
		}
	}
	return(_error);

} /* bumpmstats */

static int
pool_idx(char *s)
{
	ASSERT(s);

	switch(*(s-1)) {

	    case '1':
		return(1);
	    case '2':
		return(2);
	    case '3':
		return(3);
	    case '4':
		return(4);
	    case '5':
		return(5);
	    default:
		return(0);
	}
	/*NOTREACHED*/

} /* pool_idx */


/*
 * descend a parse tree freeing each node
 *
 * The descent path must match the tree pattern created in ide_gram.y
 *
 */
void
destroytree(node_t *pnode)
{
	if ( ! pnode )
		return;

	switch ( pnode->node_type ) {
		case OP_STMT:
			destroytree( pnode->node_right);
			destroytree( pnode->node_left);
			break;
		case OP_PRINT:
			destroyexpr(pnode->node_left);
			destroynode(pnode);
			break;
		case OP_IF:
			destroyif(pnode);
			break;
		case OP_WHILE:
			destroywhile(pnode);
			break;
		case OP_FOR:
			destroyfor(pnode);
			break;
		case OP_RETURN:
			destroynode(pnode);
			break;
		case OP_BREAK:
			destroynode(pnode);
			break;
		case OP_CONTINUE:
			destroynode(pnode);
			break;
		case OP_DO:
			destroywhile(pnode);
			break;
		case OP_SWITCH:
			destroyswitch(pnode);
			break;
		case OP_CMD:
			destroycmd(pnode);
			break;
		case OP_REPEAT:
			destroyrepeat(pnode);
			break;
		default:
			destroyexpr(pnode);
			break;
		}
}

static
void
destroyrepeat(node_t *pnode)
{
	ASSERT(pnode);
	destroytree(pnode->node_right);
	destroysym(pnode->node_psym);
	destroynode(pnode);
}
static
void
destroycmd(node_t *pnode)
{
	ASSERT(pnode);
	destroyargv(pnode->node_right);
	destroynode(pnode);
}

void
destroyargv(node_t *pnode)
{
	if ( ! pnode )
		return;

	destroyargv(pnode->node_right);
	destroyexpr(pnode->node_left);
	destroynode(pnode);
}

static
void
destroyswitch(node_t *pnode)
{
	ASSERT(pnode);
	destroysym(pnode->node_psym);
	destroycaselist(pnode->node_right);
	destroynode(pnode);

}

static
void
destroycaselist(node_t *pnode)
{
	if ( ! pnode )
		return;

	if ( pnode->node_type == OP_CASELIST ) {
		destroycaselist( pnode->node_right );
		pnode = pnode->node_left;
		}
	
	if ( pnode->node_psym )
		destroysym(pnode->node_psym);
	
	destroytree( pnode->node_right );
}

static
void
destroyif(node_t *pnode)
{
	ASSERT(pnode);
	destroyexpr(pnode->node_left);
	if ( pnode->node_right->node_type == OP_ELSE ) {
		destroytree(pnode->node_right->node_left);
		destroytree(pnode->node_right->node_right);
		}
	
	destroynode(pnode->node_right);
	destroynode(pnode);
}

static
void
destroyfor(node_t *pnode)
{
	node_t *n1,*n2,*n3,*n4;

	ASSERT(pnode);
	n1 = pnode->node_left->node_left;
	n2 = pnode->node_left->node_right;
	n3 = pnode->node_right->node_left;
	n4 = pnode->node_right->node_right;

	if (n1) destroyexpr(n1);
	if (n2) destroyexpr(n2);
	if (n3) destroyexpr(n3);
	destroytree(n4);

	destroynode(pnode->node_left);
	destroynode(pnode->node_right);
	destroynode(pnode);
}

static
void
destroywhile(node_t *pnode)
{
	ASSERT(pnode);
	destroyexpr(pnode->node_left);
	destroytree(pnode->node_right);
	destroynode(pnode);
}

void
destroyexpr(node_t *pnode)
{
	if ( ! pnode )
		return;

	switch(pnode->node_type) {
		case OP_SHL:	case OP_SHR:
		case OP_ADD:	case OP_SUB:
		case OP_MUL:	case OP_DIV:
		case OP_MOD:	case OP_LT:	case OP_LE:
		case OP_GT:	case OP_GE:	case OP_EQ:	case OP_NE:
		case OP_OR:	case OP_AND:	case OP_ANDAND:	case OP_OROR:
			destroyexpr(pnode->node_right);
			destroyexpr(pnode->node_left);
			break;
		case OP_ASSIGN:
		case OP_ADDASS: case OP_SUBASS:
		case OP_MULASS: case OP_DIVASS: case OP_MODASS:
		case OP_SHLASS: case OP_SHRASS:
		case OP_ANDASS: case OP_ORASS:
			destroyexpr(pnode->node_right);
			/* Assignments are only made to things in the
			   symbol table so
			   destroysym(pnode->node_psym); is not necessary */
			break;
		case OP_UMINUS:
		case OP_COM:
		case OP_NOT:
			destroyexpr(pnode->node_right);
			break;
		case OP_PRED:
		case OP_PREI:
		case OP_POSTD:
		case OP_POSTI:
			break;
		case OP_VAR:
			destroysym(pnode->node_psym);
			break;
		case OP_FCALL:
		case OP_ARGLIST:
			break;
		}

	destroynode(pnode);
}

/*
 * destroyed nodes are put at the head of the free list
 *
 */
void
destroynode(node_t *pnode)
{
	ASSERT(pnode);

	nnodes--;
	zero_node(pnode);
	pnode->node_right = freenode;
	freenode = pnode;
	return;
}

/*
 * destroyed symbols are put at the head of the free list
 *
 */
void
destroysym(sym_t *psym)
{

	ASSERT(psym);

	/* symbols with names are permanent */
	if ( psym->sym_flags & SYM_INTABLE ) {
		return;
	} 

#if defined(noSET_DEBUG)
	if (psym->sym_basetype == SYMB_SET) {
	    char setbuffer[SETBUFSIZ];

	    sprintf_cset(setbuffer, psym->sym_set);
	    msg_printf(INFO,
		"   destroysym: %s set '%s': type %d flags 0x%x:  %s",
		(psym->sym_flags & SYM_INTABLE? "PERM" : "TMP"),
		_SAFESTR(psym->sym_name), psym->sym_type, psym->sym_flags,
		setbuffer);
	}
#endif

	nsyms--;

	if ((psym->sym_basetype == SYMB_STR) ||
	    (psym->sym_basetype == SYMB_SETSTR)) {
		destroystr(psym->sym_s);
		psym->sym_s = NULL;
	}
	zero_sym(psym, FREE_ZERO);
	
	psym->sym_hlink = freesym;
	freesym = psym;

	return;
}

/*
 * free a string by setting its string table allocation byte
 * to null.
 */
void
destroystr(char *s)
{

	if ( ! s ) {
#if IDEBUG
		msg_printf(DBG+1, "    ->destroystr: NULL string ptr (s)\n");
#endif
		return;
	}

	if ( *(s-1) == '1' )
		nstr1--;
	else
	if ( *(s-1) == '2' )
		nstr2--;
	else
	if ( *(s-1) == '3' )
		nstr3--;
	else
	if ( *(s-1) == '4' )
		nstr4--;
	else
	if ( *(s-1) == '5' )
		nstr5--;
	else {
		char panicbuf[128];

		sprintf(panicbuf, "Bad call to destroystr, string addr 0x%x",
			s);
		ide_panic(panicbuf);
	}

	*(s-1) = '\0';
	nstrs--;

}

/*
 * create a new symbol record by either taking one off of the free list
 * or getting another one out of the pool.
 *
 */
sym_t *
makesym(void)
{
	sym_t *tmp;

	nsyms++;

	if ( freesym ) {
		tmp = freesym;
		freesym = tmp->sym_hlink;
		zero_sym(tmp, ALLOC_ZERO);
		return tmp;
		}

	if ( new__symc <  NEWSYMC ) {
		tmp = &new__sym[new__symc++];
		zero_sym(tmp, ALLOC_ZERO);		/* just to be sure... */
		return(tmp);
	} 

	/* err, bad thing */
	printf("!!! makesym() fails, out of symbols.  Dump stats:\n");
	showmstats(0, 0);
	ide_panic("Out of symbols\n");
	return NULL;
}

static void
zero_sym(sym_t *psym, int how)
{
    ASSERT(psym);

    psym->sym_hlink	= 0;
    psym->sym_basetype = (short)SYMB_FREE;
    psym->sym_type	= (short)SYM_FREE;
    psym->sym_logidx	= FREE_LOGIDX;
    psym->sym_reserved  = (short)0;
    psym->sym_flags	= 0;
    psym->sym_set	= 0;	/* gets both hi and low ints! */

    if (how == ALLOC_ZERO) {	/* so _dumpsym will know whether it's valid */
	psym->sym_basetype = SYMB_BADVAL;
	psym->sym_type = SYM_UNDEFINED;
	psym->sym_logidx = ALLOC_LOGIDX;
    }
}

/*
 * same algorithm as above for nodes
 */
node_t *
makenode(void)
{
	node_t *tmp;

	nnodes++;

	if ( freenode ) {
		tmp = freenode;
		freenode = tmp->node_right;
		zero_node(tmp);
		return tmp;
		}
	
	if ( new__nodec < NEWNODEC ) {
		tmp = &new__node[new__nodec++];
		zero_node(tmp);
		return tmp;
		}
	else 

	printf("!!! makenode() fails, out of nodes.  Dump stats:\n");
	showmstats(0, 0);
	ide_panic("Out of nodes\n");
	return NULL;
}

static void
zero_node(node_t *tmp)
{
	ASSERT(tmp);
	tmp->node_type  = 0;
	tmp->node_right = 0;
	tmp->node_left  = 0;

	return;
}


/*
 * allocate space for string
 *
 * A best fit out of 5 possible pools algorithm is used.  The
 * -1 position of the string stores a status byte, null indicates
 * that the string is free.
 *
 */
char *
talloc(size_t len)
{
	char *t;
	int i;

#if IDEBUG
	if ((len == 0) && (Reportlevel >= VDBG))
		printf("  talloc: 0 'len' arg!!!\n");
#endif
	len += 1;
	if ( len <= STR1LEN )
		for ( i = last1; i < last1+NSTR1; i++ )
			if ( string1[i % NSTR1][0] == '\0' ) {
				last1 = i % NSTR1;
				t = &string1[last1][1];
				string1[last1][0] = '1';
				nstr1++;
				return t;
				}

	if ( len <= STR2LEN )
		for ( i = last2; i < last2+NSTR2; i++ )
			if ( string2[i % NSTR2][0] == '\0' ) {
				last2 = i % NSTR2;
				t = &string2[last2][1];
				string2[last2][0] = '2';
				nstr2++;
				return t;
				}

	if ( len <= STR3LEN )
		for ( i = last3; i < last3+NSTR3; i++ )
			if ( string3[i % NSTR3][0] == '\0' ) {
				last3 = i % NSTR3;
				t = &string3[last3][1];
				string3[last3][0] = '3';
				nstr3++;
				return t;
				}

	if ( len <= STR4LEN )
		for ( i = last4; i < last4+NSTR4; i++ )
			if ( string4[i % NSTR4][0] == '\0' ) {
				last4 = i % NSTR4;
				t = &string4[last4][1];
				string4[last4][0] = '4';
				nstr4++;
				return t;
				}

	if ( len <= STR5LEN )
		for ( i = last5; i < last5+NSTR5; i++ )
			if ( string5[i % NSTR5][0] == '\0' ) {
				last5 = i % NSTR5;
				t = &string5[last5][1];
				string5[last5][0] = '5';
				nstr5++;
				return t;
				}

	printf("!!! talloc(%d) fails, out of string space\n", len);
	showmstats(0, 0);
	ide_panic("talloc out of string space");
	return NULL;
}
/*
 * get space for a string and copy into it
 */
char *
makestr(char *s)
{
	size_t len;
	char *t;

	nstrs++;

	if (s == 0) {
		len = 0;
	} else 
		len = strlen(s);

	t = talloc((int)len+1);
	if (t == 0) {
		msg_printf(IDE_ERR, "makestr's talloc(len %d) returned NULL\n",
			len);
		ide_panic("makestr: talloc");
	}

	if (len == 0)		/* return a string of length 0 */
		*t = '\0';
	else
		strcpy(t, s);

	return t;
}

/*
 * make a symbol out of an integer
 */
sym_t *
makeisym(int i)
{
	sym_t *tmp = makesym();

	ASSERT(tmp != 0);

	tmp->sym_basetype = SYMB_INT;
	tmp->sym_type = SYM_CON;
	tmp->sym_i = i;

	return tmp;
} /* makeisym */


/*
 * make a symbol out of a string
 */
sym_t *
makessym(char *s)
{
    sym_t *psym, *tsym;

    ASSERT(s != NULL);

    psym = makesym();
    ASSERT(psym);

    if (_SHOWIT(EVAL_DBG|DBG+4))
	msg_printf(JUST_DOIT," makessym, %s: instring \"%s\", (len %d): ",
	    (inrawcmd ? "*RAW*" : "*COOKED*"), _SAFESTR(s),
	    (_SAFESTR(s) ? strlen(s) : -1));

    if (!inrawcmd) {	/* make a string constant */
	psym->sym_type = SYM_CON;
	psym->sym_basetype = SYMB_STR;
	psym->sym_s = s;   
	msg_printf((EVAL_DBG|DBG+4),"STR CONST and return!\n");
	return(psym);
    }

    /* 
     * all remaining cases are raw (functions taking char* arg format)
     */
    if (s[0] != '$') {		/* must be string const, eg. "123" */

	psym->sym_type = SYM_CON;
	psym->sym_basetype = SYMB_STR;
	psym->sym_s = s;   
	msg_printf((EVAL_DBG|DBG+4)," non-dollar STR CONST; strlen %d\n",
		strlen(psym->sym_s));
	return(psym);
    }

    /* 
     * now they all begin with '$': as in "$<blah>"
     */
    msg_printf((EVAL_DBG|DBG+4)," ->$");

    if (isdigit(s[1])) {		/* $n == formal parameter */
 	psym->sym_type = SYM_PARAM;
 	psym->sym_indx = (int)(s[1] - '0');
	psym->sym_s = s;
	if (_SHOWIT(EVAL_DBG|DBG+4)) {
		msg_printf(JUST_DOIT, "formal param $%d, strlen %d\n",
			psym->sym_indx, strlen(psym->sym_s));
	        _dumpsym(psym);
	}
	return(psym);
    } else {	/* $xyz: named-identifier used as argument */
	char *tmpp = &s[1];

	msg_printf((EVAL_DBG|DBG+4)," ->$xyz!");
	if (!(tsym = findsym(tmpp))) {	/* temp sym (declared inside a block) */
 	    psym->sym_basetype = SYMB_STR;
 	    psym->sym_type = SYM_VAR;
	    psym->sym_s = s;
	    if (_SHOWIT(EVAL_DBG|DBG+4)) {
		msg_printf(JUST_DOIT, " temp. VAR symbol!\n  ");
	        _dumpsym(psym);
	    }
	    return(psym);
	}

	destroysym(psym);
	/* 
	 * rawcmds need the variable's value promoted to string
	 * and placed in psym->sym_s.
	 */
	if (_SHOWIT(EVAL_DBG|DBG+4)) {
	    msg_printf(JUST_DOIT, " symbol before pro_str:\n  ");
	    _dumpsym(tsym);
	}

	tsym = pro_str(tsym);
	if (_SHOWIT(EVAL_DBG|DBG+4)) {
	    msg_printf(JUST_DOIT, " result symbol (after pro_str):\n  ");
	    _dumpsym(tsym);
	}
	return(tsym);
    }
} /* makessym */

/*  Returns a chunk of free memory for a diag to use over it's life.  Other
 * diags may claim this space, so it should only be over the life of a diag.
 *
 * **** Cannot be mixed with ide_malloc! ****
 *
 * Still in ide_alloc.c to ensure ide_malloc.c is linked correctly.
 */
void *
get_chunk(int size)
{
	if (init_ide_malloc(size) == 0)
		return 0;

	return ide_malloc(size);
}
