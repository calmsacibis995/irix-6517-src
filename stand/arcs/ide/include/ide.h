#ifndef __IDE_H__
#define __IDE_H__

/*
 * ide.h
 *
 * Data structures used by the Interactive Diagnostic Environment
 * interpreter.
 *
 */

#ident "$Revision: 1.41 $"

#include <sys/types.h>
#include <sgidefs.h>
#include <setjmp.h>
#include <ctype.h>

#ifdef _STANDALONE
#include <saioctl.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#endif

#define ide_int_type __psint_t
/* scripts which invoke fns returning binary results can use these values */
#define SCRIPT_TRUE	1
#define SCRIPT_FALSE	0


/*
 * MP ide consists of a single master process and multiple slave processes.
 * Only the IDE_MASTER executes the interpreter (i.e. script) code, and
 * only one master can exist at any time.  All other processors run as slaves.
 * The master mode may be given away on command, however, which causes the
 * current master to begin executing slave code after switching to the 
 * private slave stack allocated for that processor, and the upcoming 
 * master to begin execution where the previous master stopped, after
 * switching to the (huge) master stack.
 */
#define IDE_MASTER	1
#define IDE_SLAVE	2

#if _MIPS_SIM == _ABI64
#define IDE_MASTER_SR	SR_KX|SR_CU1|SR_FR
#else
#define IDE_MASTER_SR	0|SR_CU1
#endif


/* 3.10 compilers are necessary for long long datatype support */
typedef unsigned long long set_t;
typedef unsigned long long setmask_t;

/* all statements are parsed into binary trees by ide_gram.y.  The tree
   is made up of nodes of the following form: */

typedef struct node_s {
	int node_type;			/* type, defined below */
	struct node_s *node_right;	/* right child, zero if none */
	union	{
		struct node_s *_left;
		struct sym_s *_psym;
	} _val;
#define node_left _val._left		/* left child if another node */
#define node_psym _val._psym		/* left child sometimes a symbol */
} node_t;

/* symbol structure */
typedef struct sym_s {
	struct sym_s *sym_hlink;	/* hash link */
	union	{
		char	*_name;
		int	_indx;
	} _tag;
#define sym_name _tag._name		/* pointer to symbol name */
#define sym_indx _tag._indx		/* index of parameter (e.g. $1, $2) */
	short	sym_basetype;		/* data type: int, string, etc. */
	short	sym_type;		/* symbol type */
	short	sym_logidx;		/* if diag, its log-entry's index */
	short	sym_reserved;		/* extra from shortening type & btype */
	uint	sym_flags;
	union	{
		ide_int_type _i;
		char	*_s;
		ide_int_type ((*_func)(int, void **));
		node_t	*_stmt;
		struct	{
			int _start;
			int _count;
		} _range;
		set_t _set;
	} _val;
#define sym_i _val._i
#define sym_s _val._s
#define sym_func  _val._func
#define sym_stmt  _val._stmt
#define sym_start _val._range._start
#define sym_count _val._range._count
#define sym_set _val._set
} sym_t;


/* symbol base types */
typedef enum sym_btypes_s {
SYMB_UNDEFINED=0,	/* 0: symbol is allocated but not yet basetyped */
SYMB_INT,		/* 1 */
SYMB_STR,		/* 2 */
SYMB_RANGE,		/* 3 */
SYMB_SET,		/* 4: longlong bitfield representing sets of cpus */
SYMB_SETSTR,		/* 5: ascii string ala "0,1,2,3" representing cpus */
SYMB_FREE,		/* 6: symbol is currently free (unallocated) */
SYMB_BADVAL,		/* 7: symbol had invalid basetype */
SYMB_END		/* 8 */
} sym_btypes_t;

#define SYMBTYPE_CNT		(SYMB_END - SYMB_UNDEFINED)
#define VALID_BTYPE(B_TYP)	((B_TYP>=SYMB_UNDEFINED) && (B_TYP<SYMB_END))


/* symbol types */
typedef enum sym_types_s {
SYM_UNDEFINED=SYMB_END,	/*  8: immediately above SYMB_BADVAL */
SYM_VAR,		/*  9 */
SYM_CON,		/* 10 */
SYM_CMD,		/* 11 */
SYM_CMDFLAG,		/* 12 */
SYM_PARAM,		/* 13 */
SYM_GLOBAL,		/* 14 */
SYM_FREE,		/* 15 */
SYM_BADVAL,		/* 16 */
SYM_END			/* 17 */
} sym_types_t;

#define SYMTYPE_CNT		(SYM_END - SYM_UNDEFINED)
#define VALID_TYPE(_T_YPE)	((_T_YPE>=SYM_UNDEFINED) && (_T_YPE<SYM_END))

#define TBT_TOTAL		(SYMTYPE_CNT + SYMBTYPE_CNT)
extern char *sym_tstrs[];
extern int sym_tstr_tabsiz;

/* symbol flags */
#define SYM_INTABLE	0x01	/* symbol is in symbol table */
#define SYM_UDEF	0x02	/* user defined function */
#define SYM_CHARARGV	0x04	/* pass arguments in "char *argv[]" format */
#define SYM_DIAG	0x08	/* fn is a diagnostic, not an ide routine */
#define SYM_SET_CMD	0x10	/* fn is an ide 'set' primitive */
#define SYM_DEBUG_CMD	0x20	/* fn is used for the debugging of ide itself */
#define SYM_FLAG_CNT	(6)

#define _PERM(_P_SYM)	((_P_SYM) && (_P_SYM->sym_flags & SYM_INTABLE))


/* program limits and tunables */
#define HASHSIZE	101	/* should be prime */
#define MAXIDENT	48	/* maximum identifier length */
#define MAXSTRLEN	256	/* maximum string length */
#define MAXARGC		32	/* maximum number of passed arguments */
#define MAXLINELEN	256
#define MAXPROMPTLEN	(32)

#define PUTBUFSIZE	512	/* slaves & master have private outbufs */
#define PAGEBUFSIZE	240	/* 3 * 80 cols: buffers passed to pager */

/*
 * max # of diagnostic fns: this sizes errlog.  The last slot must be
 * zeroed to indicate the list end
 */
#if IP22 || IP30
#define MAXDIAG         600
#else
#define MAXDIAG         255
#endif

#ifndef EOF
#define EOF (-1)
#endif

/* node types */
#define OP_VAR		1
#define OP_ADD		2
#define OP_SUB		3
#define OP_UMINUS	4
#define OP_MUL		5
#define OP_DIV		6
#define OP_MOD		7
#define OP_OR		8
#define OP_AND		9
#define OP_XOR		10
#define OP_COM		11
#define OP_NOT		12
#define OP_ANDAND	13
#define OP_OROR		14
#define OP_LT		15
#define OP_LE		16
#define OP_GT		17
#define OP_GE		18
#define OP_EQ		19
#define OP_NE		20
#define OP_PRED		21
#define OP_PREI		22
#define OP_POSTD	23
#define OP_POSTI	24
#define OP_SHL		25
#define OP_SHR		26
#define OP_ASSIGN	27
#define OP_SUBASS	29
#define OP_ADDASS	28
#define OP_MULASS	30
#define OP_DIVASS	31
#define OP_MODASS	32
#define OP_SHLASS	33
#define OP_SHRASS	34
#define OP_ANDASS	35
#define OP_ORASS	36
#define OP_XORASS	37

#define OP_RETURN	38
#define OP_CONTINUE	39
#define OP_BREAK	40
#define OP_DO		41
#define OP_SWITCH	42
#define OP_CASELIST	43
#define OP_CASE		44
#define OP_CMD		45
#define OP_STMT		46
#define OP_IF		47
#define OP_ELSE		48
#define OP_FCALL	49
#define OP_ARGLIST	50
#define OP_PRINT	51
#define OP_WHILE	52
#define OP_FOR		53
#define OP_REPEAT	54

/* execution context frame */
typedef struct context_s {
	jmp_buf lframe; /* loop frame */
	jmp_buf rframe; /* return frame */
	sym_t	*argv[MAXARGC];
	int	argc;
} context_t;

extern int	showmstats(int, sym_t **);
extern int	version(void);
extern int	yylex(void);
extern char	*talloc(size_t);
extern int	set_global(sym_t *, sym_t *);
extern void	handle_interrupt(void);
extern int	getsfromsource(char *);
extern void	dump_symdefs(void);
extern void	destroyargv(node_t *);
extern void	addsym(sym_t *);
extern int	 doqhelp(void);
extern node_t	*makenode(void);
extern void	dostmt(node_t *);
extern void	destroytree(node_t *);
extern void	destroynode(node_t *);
extern char	*makestr(char *);
extern sym_t	*makesym(void);
extern void	dump_symdefs(void);
extern void	destroysym(sym_t *psym);
extern sym_t	*makeisym(int);
extern sym_t	*makessym(char *);
extern sym_t	*findsym(char *);
extern void	ide_panic(char *);
extern void	destroystr(char *);
extern int	ide_refill(void);

extern int 	bumpmstats(sym_t *);
extern sym_t	*pro_str(sym_t *);
extern sym_t	*pro_set(sym_t *);
extern sym_t	*pro_setstr(sym_t *);

extern int	yyparse(void);

extern char	*sf_strs[];

/* 
 * talloc allocates memory from one of five pools, containing 
 * strings of 16, 32, 64, 128, and 256 bytes.  
 */
#define NUMSTRPOOLS	  5
#define STR1LEN		 16
#define STR2LEN		 32
#define STR3LEN		 64
#define STR4LEN		128
#define STR5LEN		256


#define DIV0RESULT 0x7fffffff /* result of divide or modulo by zero */

/* structure used for initializing symbols (can't initialize unions!) */
typedef struct builtin_s {
	char	*name;
	char	*addr;
	int	type;
	char	*help;
} builtin_t;

/* symbol type values for .c tables */
#define B_CMD		 1	/* ide commands w/ args in char * argv format */
#define B_STR		 2	/* symbol is a string */
#define B_INT		 3	/* symbol is an integer */
#define B_SETSTR	 4	/* symbol is a string-format cpu list */
#define B_SCMD		 5	/* ide commands w/ sym_t * argv format */
#define B_SET_CMD	 6	/* ide 'set' prims (args are char * format) */
#define B_DEBUG_CMD	 7	/* useful for debugging ide itself */
#define B_DEBUG_SCMD	 8	/* ide debugging (sym_t * argv args) */
#define B_CPUSPEC_CMD	 9	/* cpu-specific builtin command */
#define B_CPUSPEC_SCMD	10	/* cpu-specific builtin sym_t *command */

/* for ide_init_builtins tab_type argument */ 
#define B_IDE_COMMANDS		1
#define B_DIAGNOSTICS		2
#define B_GLOBALS		3
void	ide_init_builtins(int, builtin_t *, int);

/* 
 * this is a stab at providing an "execution environment"
 */


/* 
 * below are the expected symbol-names for the xenv components.
 * Each component has a default-value (as read in from the 
 * cpu_diagcmds table at bootup) and a command to query/change
 * that value, and these are distinct symbols.  The stuff below
 * sets up a translation mechanism.
 * The naming convention uses <blah>VAL_SNAME for the value symbols,
 * and <blah>CMD_SNAME for the command symbols.
 */
#define ERRLOGVAL_SNAME		"error_logging"
#define ERRLOGCMD_SNAME		"errlog"

#define ICEXECVAL_SNAME		"icached_exec"
#define ICEXECCMD_SNAME		"runcached"

#define QMODEVAL_SNAME		"quick_mode"
#define QMODECMD_SNAME		"qmode"

#define C_ON_ERRVAL_SNAME	"continue_on_err"
#define C_ON_ERRCMD_SNAME	"c_on_error"

typedef enum xenv_index {	/* indices of the correct xenv slot */
	ERRLOGGING=0, ICACHED_EXEC, QUICKMODE, C_ON_ERROR
} xenv_index_t;
#define XENV_COMPONENTS		4

typedef struct xenv_component_s {
	sym_t *val_symp;
	sym_t *cmd_symp;
} xenv_component_t;

typedef struct xenv_snames_s {
	char *val_sname;
	char *cmd_sname;
} xenv_snames_t;
extern xenv_snames_t xenv_snames[];
extern xenv_component_t xenv_syms[];

/*
 * argc, char *argv[] arg-format (exported to scripts)
 */
extern int do_xenv(int, int, char **);
extern int do_errlog(int, char **);
extern int do_runcached(int, char **);
extern int do_qmode(int, char **);
extern int c_on_error(int, char **);
extern int do_em_all(int, char **);

extern int _ide_xenv(xenv_index_t, int, int);
extern int init_context(int, char **);
extern int _initsyms(void);
extern int _sympoolsize;

extern int cmd_check(sym_t *);

#define STOUT_SNAME     "slave_timeout"
extern ide_int_type	*_Slave_Timeout;
#define Slave_Timeout	(*_Slave_Timeout)
extern sym_t    Stoutsym;

#define REPORT_SNAME		"report"
#define Reportlevel		(*Reportlevel)
extern sym_t	Reportsym;

#define _RLEVEL			(((int)Reportlevel) & NORM_MMASK)
#define _RBITS			(((int)Reportlevel) & ~NORM_MMASK)
#define _SHOWIT(MIN_LEV)	\
			(((_RLEVEL) >= (MIN_LEV)) || ((_RBITS) & (MIN_LEV)))

extern int	_ide_initialized;
extern int	_Verbose, _Debug;
extern jmp_buf	intr_jump;

/*
 * puts() behaves differently in standalone mode
 */
#ifdef _STANDALONE
#define ide_puts(t) msg_printf(JUST_DOIT, "%s", t)
#else
#define ide_puts(x) printf("%s", x)
#endif

/*
 * ide calls a different print routine during initialization.
 * Therefore, it can resolve it to an errprint routine, or to 
 * nothing (to suppress)
 */
/* #define ide_init_printf(_lev, _fmt, _buf)	printf(_fmt, _buf) */
/* #define ide_init_printf(_lev, _fmt, _buf)	msg_printf(_lev, _fmt, _buf) */

typedef enum cmdtypes_s {	CMD_INVAL_T=0, CMD_UDEF_T, CMD_DIAG_T, 
				CMD_SDIAG_T, CMD_SET_T, CMD_SSET_T,
				CMD_DEBUG_T, CMD_SDEBUG_T
} cmdtypes_t;
#define NUM_CMDTYPES		8


#define _ITSAPTR(_STRP)		((_STRP) && (IS_KSEG0(_STRP)||IS_KSEG1(_STRP)))
#define _XITSAPTR(_STRP)	(_STRP && IS_KSEG1(_STRP))

#define _STRPTR(_STR_P)		(_ITSAPTR(_STR_P) && \
					(isalnum(*((char *)(_STR_P)))) )
#define _XSAFE(S_TR_P)		((_STRPTR(S_TR_P)) ? S_TR_P : "        ")


#define _SAFESTR(STR_P)		((_ITSAPTR(STR_P)) ? STR_P : "<NULL>")
#define _XSAFESTR(STR_P)		((_XITSAPTR(STR_P)) ? STR_P : "<XNULL>")

#define _PLURAL(_OBJECT)	(_OBJECT != 1 ? "s" : "")


#define _CMDTYPE_STR(S_TYPE)	\
    ((S_TYPE>=SYM_INT && S_TYPE < NUM_CMDTYPES) ? st_strs[S_TYPE]:"bad type")


#define LOGIDX_INRANGE(I_DX)		(I_DX >= 0 && I_DX < MAXDIAG)
#define FREE_LOGIDX	((short)-1)
#define ALLOC_LOGIDX	((short)-2)

/*
 * iff exec'ing a diagnostic routine, ide_dispatch() must display its 
 * name and the outcome of the run; suppress this stuff for user 
 * defined functions.
 */ 
#define DIAG_EXEC(_S_FLAGS)	((_S_FLAGS & (SYM_DIAG|SYM_UDEF)) == SYM_DIAG)
#define ID_DIAG(_SFLAGS)	(DIAG_EXEC(_SFLAGS) && Reportlevel > NO_REPORT)

#define MUTEX_FLAGS		(SYM_UDEF|SYM_DIAG|SYM_SET_CMD|SYM_DEBUG_CMD)

#define CHAR_ARGVS(SYM_P)	\
			((SYM_P) && (((SYM_P)->sym_flags&SYM_CHARARGV)?1:0))

extern builtin_t builtins[];
extern int nbuiltins;

extern builtin_t diagcmds[];
extern int ndiagcmds;

/*
 * ide_env() uses these values for its binary decisions
 */
#define ENABLED			1
#define DISABLED		0

#define MS_PER_SEC	(1000)
#define US_PER_MS	(1000)
#define US_PER_SEC	(US_PER_MS * MS_PER_SEC)

#define _COLON		(':')
#define _SKIP_IT	0xdeadbeef

/*
 * The opt* vars below were yanked from libc getopt().  DD_RETURN is the
 * value returned by ide's version of _getopt when "--" is encountered.
 * Although this special flag is legal syntax and the accepted method
 * for explicitly indicating arg-list end, the standard getopt() returns
 * the same value for this state as it does for implicit arglist end
 * (==EOF).  Since the special execution mode of ide_dispatcher()--once
 * invoked via the SPX_BEGIN flag in argv[1]--strips and interprets all
 * args until _getopt hits a 2nd "--" and returns DD_RETURN, leaving any 
 * remaining args for the called routine--it must be able to differentiate
 * between these cases.
 */
#define	DD_RETURN	(-2)
#define	SPX_BEGIN	DD_RETURN
#define	SPX_END		DD_RETURN



/*
 * ide scripts wishing to invoke any of the special-execution 
 * (spx) capabilities provided by ide_dispatcher do so by prepending
 * the desired arguments to the called-function's argument list.
 * The first parameter (i.e. argv[1]) must be the special string
 * "--", which tells the dispatcher that special treatment is desired;
 * therefore, the spx options begin at argv[2].  The spx arglist must
 * also be terminated with "--", after which any args for the invoked
 * test begin.  The dispatcher returns SPX_NOARGEND for non-terminated
 * spx arglists.  
 */
#define SPX_OPTS	"s:ip"		/* set execution, icache diags, */
#define DELAY_OPTS	"u:m:s:"	/* delay interval in us, ms, or secs */
#define SHOWSYM_OPTS	"-lk:t:"	/* -l|-k KEY|-t TYPE */
#define CPU_INFO_OPTS	"s:"		/* [ -s set1[,set2...]] */
#define EXEC_OPTS	"f:a:v:s:"	/* -f FNS -a ARGS {-v VIDS|-s SETS} */
#define DUMP_LOG_OPTS	"s:v:a"		/* -f FNS -v VID[S] -a (all) */


/* 
 * OPTCCNT counts the number of characters in 'optstr', and subtracts
 * the number of colon chars included in that tally, yielding the tally
 * of separate options contained in a getopt()-format optstr.
 */
#define OPTCCNT(optstr) (strlen(optstr) - _charcnt(optstr,_COLON))


/* 
 * MAX_OPTS is the max number of unique options that any ide 
 * function can accept; MAX_OPTARGS is the maximum number of arguments
 * accompanying an option, specified either as an unbroken string
 * using comma separators, or as a quoted string with spaces as
 * separators (fn name and the option use up 2 of MAXARGC).  
 */
#define MAX_OPTS	10
#define MAX_OPTARGS	(MAXARGC - 2)


/* 
 * optargs_t structs are used by _get_options() to describe an option
 * together with its accompanying arguments (if any).  'optnames' are
 * the integer value of the option-character.  If the option requires
 * arguments 'argcount' keeps the tally, and 'args' are set to point 
 * to the heads of each successive argument within the argv string.
 * _get_options accepts both optarg formats described in intro(1):
 * comma-separated lists or quoted strings using <sp> as delimiters.
 * i.e. 'fn -s x,y,z' or 'fn -s "x y z"'.
 */
typedef struct option_s {
	int opt;		/* byte-value of option */
	int argcount;		/* count of args (if any) to the opt. */
	char *args[MAX_OPTARGS+1];  /* args converted into strings */
} option_t;

typedef struct optlist_s {
	int maxopts;	/* number of distinct options calling fn accepts */
	int optcount;	/* number of options _get_options found */
	char *fnname;	/* usually argv[0]; for msgs by called primitives */
	char *getopt_str;	/* options accepted by fn: getopt() format */
	char opts[MAX_OPTARGS+1];	/* string of option-chars entered */
	option_t options[MAX_OPTS];	/* one option_t struct per option */
} optlist_t;


/*
 * 'exec' accepts 0 or more fn names with the '-f' opt, and calls
 * _check_fns() to verify that each name is a fn and a permanent symbol
 * (i.e. in the symbol hashtable).
 */
typedef struct fnlist_s { 
  int fncnt;				/* # of fns in 'fn_symptrs' list */
  sym_t *fn_symptrs[MAX_OPTARGS];	/* ptrs to fns' symbols */
} fnlist_t;



/* ide error definitions: script-level (of interest to ide users)
 * and ide-internal (of interest to diagnostic writers)
 */

/* errors returned to scripts by ide builtins */
#define	BADOPTCOUNT	(-3)
#define	BADARGCOUNT	(-4)
#define BADOPTSORARGS	(-8)
#define	NOOPTLIST	(-10)
#define	NOOPTIONS	(-11)
#define	NOOPTARG	(-12)
#define	BADARGC		(-13)
#define	OPTARGERR	(-14)
#define	BADSETARGS	(-21)
#define	BADVPARGS	(-22)

#define	ERRNOEXEC	(-25)	/* attempt to exec user-defined function */

/* specific set-related errors */
#define EMUTEXOPTS	(-50)
#define ENOSETNAME	(-51)

#define ERR_VP_RANGE	(-70)	/* specified virt_id < 0 || >= vp_max_vid */
#define ERR_PP_RANGE	(-71)	/* phys processor index < 0 || >= EV_MAX_CPUS */

#define SPXERR_NOARGEND	(-81)
#define SPXERR_BADSETS	(-82)
#define SPXERR_SETCNT	(-83)

/* ide-internal error values: returned to diagnostics by ide primitives */
#define IEBAD_MPCONF		(-201)	/* mpconf area is scrogged */
#define IEBAD_IDE_PDA		(-203)	/* ide pda area is scrogged */
#define IENOHELPSTRING		(-210)
#define	IEBADMAXOPT		(-215)
#define	IEBADOPT		(-216)
#define	IEBADOPTSTRUCT		(-217)

#define	IEBADCREATE		(-225)
#define IESYM_NOT_FOUND		(-227)
#define IESYM_NOT_SETTYPE	(-228)

#define SCRIPT_ERR_RESTART	( -10)
#define MASTER_INT_RESTART	( -11)
#define SLAVE_INT_RESTART	( -12)



/* NO_VPID suggests that the prom's config tree could be corrupt.  DISABLED
 * processors were turned off either by command or due to error.  NORESP
 * means the prom got no response from the processor: DOA. */
#define ERR_PP_NO_VPID	(-230)	/* vpid field of phys_index's slot not valid */
#define ERR_PP_DISABLED	(-231)	/* physical processor is disabled */
#define ERR_PP_NORESP	(-232)	/* pp didn't respond to prom's queries */

/* phys-processor range-check macro (PHYS_PID_OUTARANGE) defined in ide_mp.h */

/* modes of set-display verbosity (parameter to _display_set) */
#define D_SILENT	0	/* set format only */
#define D_DOHEX		1	/* dump hex equivalent before set format */

#define SETBUFSIZ	256	/* 64 * "34," = 194 + lots of slop */
#define EV_SYSNAME	"IP19"

#ifdef MOVED_TO_IDE_MP_H
+ typedef struct argdesc_s {
+ 	int argc;
+ 	char **argv;
+ } argdesc_t;
#endif /* MOVED_TO_IDE_MP_H */


extern uint	low_sp;	/* used to monitor size of ide_master's stack */
extern int	global_toldem;

extern int	_get_arglist(char *, option_t *);
extern char	*inv_findcpu(void);

extern int	pagesize;	/* used by pager */
extern int	badflag;	/* indicates bad cmd: must not be executed */

/* prototypes of script-callable ide routines */
extern int	_dodelay(int, char **);


/* 
 * prototypes for primitives providing services to diagnostic writers (as
 * opposed to routines used by the IDE interpreter to provide script-level
 * services).  These routines are primarily in ide_prims.c and ide_sets.c.
 */
extern int	pager(char *, int, int *);
extern int	do_test_launch(int, char**);

extern int      _cpu_count(void);
extern int      _vp_set(sym_t *);

extern int	_get_options(int, char **, char *, optlist_t *);
extern int	_opt_index(char, char *);

extern void	_dump_getopt_vars(int);
extern void	_dump_argvs(int, char **);
extern int	_dump_optlist(optlist_t *);

extern int	_show_syms(int, char **);
extern int	_dumpsym(sym_t *);
extern int	_ide_printregs(void);
extern int	check_state(int, int);

extern builtin_t *      _get_tbl_entry(char *);
extern int	_do_set_help(void);
extern int	_ide_usage(char *);
extern int      _print_cset(set_t);
extern size_t   sprintf_cset(char *, set_t);
extern int      _check_fns(option_t *, fnlist_t *);

extern char *	setfnstrs[];

extern void	sum_error (char *);
extern void	okydoky (void);
extern int	error_logging(void);
extern int	icached_exec(void);


/* a whole slew of string primitives, mostly lifted from libc.a */
extern char *	_strtok(register char *, const char *);
extern char *	_strtok_r(char *, const char *, char **);
extern char *	_strpbrk(register const char *, const char *);
extern size_t	_strspn(const char *, const char *);
extern char *	_strchr(const char *, int);
extern int	_charcnt(const char *, char);


extern int	ide_dispatch(ide_int_type(*)(int, void **), int, char **);
extern int 	ide_init(int, char *[]);
extern int	icached_exec(void);
extern __psunsigned_t	icache_it(void *);


#if EVEREST
extern int	dump_mpconf(int, char**);
extern int	_flash_cc_leds(int,int);
extern int	_toggle_cc_leds(int,int);
extern int	_set_cc_leds(int);
#endif


#ifndef DEBUG
#define ASSERT(EX) ((void)0)
#else
extern void _assert(const char *, const char *, int);
#ifdef __ANSI_CPP__
#define ASSERT(EX)  ((EX)?((void)0):_assert( # EX , __FILE__, __LINE__))
#else
#define ASSERT(EX)  ((EX)?((void)0):_assert("EX", __FILE__, __LINE__))
#endif /* __ANSI_CPP */
#endif /* DEBUG */

#endif /* __IDE_H__ */
