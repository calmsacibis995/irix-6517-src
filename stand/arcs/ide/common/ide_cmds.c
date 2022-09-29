

#ident "$Revision: 1.70 $"

/*
 * arcs/ide/ide_cmds.c: ide builtin commands
 */

#include <sys/types.h>
#include <sys/fpu.h>
#include <sys/sbd.h>
#include <genpda.h>

#include <arcs/io.h>
#include <arcs/time.h>
#include <string.h>
#include <parser.h>
#include <stringlist.h>
#include "uif.h"
#include "ide.h"
#include "ide_sets.h"
#include <ide_msg.h>
#include "ide_mp.h"
#include "sys/dir.h"
#include "sys/file.h"
#include <libsk.h>
#include <libsc.h>

#if EVEREST
#include "sys/EVEREST/evconfig.h"
#include "sys/EVEREST/evmp.h"
#include "sys/EVEREST/evintr.h"
#include "sys/EVEREST/everest.h"
#endif /* EVEREST */

extern int	date_cmd(int, char **);
extern void	cpu_get_tod(TIMEINFO *);
extern int	dump_pda(int, char **);
extern uint	clear_ip19intrs(int, char **);

static int		_do_dbg_help(void);
static int		_do_loop_help(void);
static int              _do_help(char* cmd);

extern struct reg_desc sr_desc[];

static jmp_buf fault_buf;
static ULONG fd;
static ULONG cc;
static char bufr[1024];
static char *bp = bufr;

int ide_dosource = 0;

/*
 * execution-environment variables
 */
xenv_snames_t xenv_snames[] = {
	{ ERRLOGVAL_SNAME, ERRLOGCMD_SNAME },		/* ERRLOGGING */
	{ ICEXECVAL_SNAME, ICEXECCMD_SNAME },		/* ICACHED_EXEC */
	{ QMODEVAL_SNAME,  QMODECMD_SNAME },		/* QUICKMODE */
	{ C_ON_ERRVAL_SNAME, C_ON_ERRCMD_SNAME }	/* C_ON_ERROR */
};

/* optimize: only fetch symbols once at init-time */
xenv_component_t xenv_syms[] = {	{ NULL, NULL }, { NULL, NULL },
					{ NULL, NULL }, { NULL, NULL }
};

/* for Modular IDE */
extern builtin_t* diagcmds_ptr; /* can be used by module to pt to its cmds */
extern int* ndiagcmds_ptr;

/* 
 * set_global is no longer necessary.  Globals only worked correctly
 * for simple assignment anyway (e.g. '<<=' didn't propagate the
 * symbol's new value to its runtime counterpart).
 */
/*ARGSUSED*/
int
set_global( sym_t *lsym, sym_t *rsym )
{
    return(0);

} /* set_global */

/*
 * find_cpu (used by mfg diags for IP32)
 * Return 1 if r5k/r5271, 2 if r10k, 3 if r12k
 */
int find_cpu()
{
	int ret, cputype;

	cputype = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT);

	switch (cputype) {
		case C0_IMP_R5000: ret = 1; break;
		case C0_IMP_R10000: ret = 2; break;
		case C0_IMP_R12000: ret = 3; break;
		default : ret = 0; break;
	}

	return (ret);
}

/* 
 *  BEGIN IDE_ENV ROUTINES:
 * 
 *  do_errlog, do_runcached, do_qmode, c_on_error, and do_em_all
 */

/* 
 * do_errlog provides ide scripts with the ability to query and/or
 * change the value of the 'error_logging' symbol.
 * call: errlog [on/off]
 * argc==1 --> query the current value.
 * Returns 0 if errlogging is off, 1 if on.
 */
int
do_errlog(int argc, char **argv)
{
    msg_printf(JUST_DOIT, "in do_errlog: argc %d, argv[0] = '%s'\n",
	argc, argv[0]);

    return(do_xenv(ERRLOGGING, argc, argv));

} /* do_errlog */

/* 
 * do_runcached provides ide scripts with the ability to
 * query and/or change the value of the 'icached_exec' symbol.
 * call: runcached [on/off]
 * argc == 1 --> query the current value.
 * Returns 0 if ContinueOnError mode is off, 1 if on.
 */
int
do_runcached(int argc, char **argv)
{
    return(do_xenv(ICACHED_EXEC, argc, argv));

} /* do_runcached */
 

/* 
 * do_qmode provides ide scripts with the ability to query and/or
 * change the value of 'quick_mode' symbol.
 * call: qmode [on/off]
 * argc==1 --> query the current value.
 * Returns 0 if quickmode is off, 1 if on.
 */
int
do_qmode(int argc, char **argv)
{

    return(do_xenv(QUICKMODE, argc, argv));

} /* do_qmode */

/* 
 * c_on_error provides ide scripts with the ability to query and/or
 * change the value of 'continue_on_err'.
 * call: c_on_error [on/off]
 * argc == 1 --> query the current value.
 * Returns 0 if ContinueOnError mode is off, 1 if on.
 */
int
c_on_error(int argc, char **argv)
{
    return(do_xenv(C_ON_ERROR, argc, argv));

} /* c_on_error */
 

/* 
 * do_em_all displays the settings of all ide execution-environment
 * variables.
 * call: xenv
 * Returns 0.
 */
/*ARGSUSED*/
int
do_em_all(int argc, char **argv)
{
    xenv_index_t which;
    __psint_t savedrep = Reportlevel;

    Reportlevel = 2;

    for (which = 0; which < XENV_COMPONENTS; which++) {
	do_xenv(which, 1, argv);
    }

    Reportlevel = savedrep;
    return(0);

} /* do_em_all */

 
/* do_xenv is the execution-environment-mod clearinghouse that does the work */
int
do_xenv(int which, int argc, char **argv)
{
    int val;
    sym_t *tsym;

    ASSERT(which >= 0 && which < XENV_COMPONENTS);
    ASSERT(xenv_syms[which].val_symp);
    ASSERT(xenv_syms[which].cmd_symp);

#ifdef IDEBUG
    if (Reportlevel > DBG) {
	msg_printf(JUST_DOIT, "    cmd_sname %s, val_sname %s:  %d", 
		xenv_snames[which].cmd_sname, xenv_snames[which].val_sname,
		xenv_syms[which].val_symp->sym_i);
	if (Reportlevel >= VDBG)
	    _dump_argvs(argc, argv);
    }
#endif
    
    val = _ide_xenv(which, 0, 0);
    tsym = xenv_syms[which].val_symp;

    if (argc == 1) {	/* query only */
	msg_printf(JUST_DOIT, "  %s (cmdname: `%s') is %s\n", tsym->sym_name,
		xenv_snames[which].cmd_sname, (val==DISABLED ? "OFF" : "ON"));
	return(val);
    }
    if (!strcasecmp(argv[1], "ON"))
	val = _ide_xenv(which, 1, ENABLED);
    else
	val = _ide_xenv(which, 1, DISABLED);

    return(val);

} /* _do_xenv */


int
_do_dumpsym(int argc, char **argv)
{
    sym_t *psym;

    if (argc != 2) {
	_ide_usage(argv[0]);
    } else {
	psym = findsym(argv[1]);
	if (!psym) {
	    msg_printf(USER_ERR, "%s: symbol `%s' not found\n",
		argv[0], argv[1]);
	    return(-1);
	}
	_dumpsym(psym);
    }

    return 0;
} /* _do_dumpsym */



/* 
 * test_charcmd(): Display the arguments received.  Convenient way of 
 * verifying parameter substitution when testing nested user-defined 
 * functions.  test_charcmd()'s args are char *argv[] format, which is
 * not ide's native format; therefore ide_cmd.c:docmd() must convert
 * the sym_t arguments into strings before invoking CMD-type functions
 * (flagged `SYM_CHARARV'). 
 */
int
test_charcmd(int argc, char **argv)
{
    uint vid_me = cpuid();

    msg_printf(JUST_DOIT, ">>test_CHARcmd(v%d), %d args:\n", vid_me, argc);
    _dump_argvs(argc, argv);

    return(0);

} /* test_charcmd */

/*
 * test_symcmd(): Display the arguments received.  Verifies correct
 * parameter substitution for functions whose args are of the
 * sym_t *argv format (the native format of ide's interpreter).
 */
int
test_symcmd(int argc, sym_t *argv[])
{
#if defined(DOIT_THE_HARD_WAY)
    char setbuf[SETBUFSIZ];
    char outbuf[MAXLINELEN];
    sym_t *tmp;
#endif
    int i;
    int _error=0;

    msg_printf(JUST_DOIT, ">>test_SYMcmd(argc %d);  args:", argc);

#if !defined(DOIT_THE_HARD_WAY)
    for ( i = 0; i < argc; i++ ) {
	msg_printf(JUST_DOIT, "  %d: ", i);
	_dumpsym(argv[i]);
    }
    msg_printf(JUST_DOIT, "\n\n");

    return(_error);


#else /* nuke this one */
    if (Reportlevel >= DBG) {
	msg_printf(JUST_DOIT, "\n    ");
	_dumpsym(argv[0]);
	msg_printf(JUST_DOIT, "\n");
    }

    for ( i = 0; i < argc; i++ ) {
	switch(argv[i]->sym_basetype) {

	    tmp = argv[i];
	    if (!VALID_TYPE(tmp->sym_type)) {
		msg_printf(IDE_ERR, "argv[%d]: bad type `%d!' ", i,
			tmp->sym_type);
		tmp->sym_type = SYM_BADVAL;
	    }

	case SYMB_INT:
	    msg_printf(JUST_DOIT, "%s %d <btype INT, %s> ", 
		(i ? ", " : "  "), argv[i]->sym_i, sym_tstrs[tmp->sym_type]);
	    break;
	case SYMB_STR:
	    if ( argv[i]->sym_type == SYM_CMDFLAG )
		msg_printf(JUST_DOIT, " \"%s\" <btype STR, %s>",tmp->sym_s,
		    sym_tstrs[tmp->sym_type]);
	    else
		msg_printf(JUST_DOIT, " `%s'<%s, %s>", argv[i]->sym_s,
		    sym_tstrs[tmp->sym_basetype], sym_tstrs[tmp->sym_type]);
	    break;

	case SYMB_SET:
	    sprintf_cset(setbuf, argv[i]->sym_set);
	    msg_printf(JUST_DOIT, " <%s, %s> 0x%x: %s\n", 
		sym_tstrs[tmp->sym_basetype], sym_tstrs[tmp->sym_type],
	    	argv[i]->sym_set, setbuf);
	    break;

	case SYMB_SETSTR:
	    msg_printf(JUST_DOIT, "<%s, %s> \"%s\": sym_set 0x%x\n", 
		sym_tstrs[tmp->sym_basetype], sym_tstrs[tmp->sym_type],
		_SAFESTR(argv[i]->sym_s), argv[i]->sym_set);
	    break;

	default:
	    _error++;
	    msg_printf(IDE_ERR, "test_symcmd argv[%d]: bad basetype-->%d\n",
		i, argv[i]->sym_basetype);
	    _dumpsym(argv[i]);
	    break;
	}
    }
    return(_error);

#endif /* HARDWAY */


} /* test_symcmd */

long
_dopro_set(int argc, char **argv)
{
	char setbuf[SETBUFSIZ];
	char *setname;
	sym_t *symptr;

	if (argc < 2) {
	    _ide_usage(argv[0]);
	    return(1);
	}

	ASSERT(argc >= 2 && (argv[1] != NULL));

	symptr = findsym(argv[1]);
	if (!symptr) {
	    msg_printf(USER_ERR, "%s: symbol `%s' not found\n",
		argv[0], argv[1]);
	    return(-1);
	}

	setname = symptr->sym_s;
	ASSERT(setname);

	msg_printf(JUST_DOIT, "  in \"%s\", argc %d:  ",
		argv[0], argc);

	msg_printf(JUST_DOIT,
	    " sym_name \"%s\", type %d, btype %d, ",
	    _SAFESTR(symptr->sym_name),symptr->sym_type,symptr->sym_basetype);

	if (symptr->sym_basetype != SYMB_SETSTR) {
		msg_printf(USER_ERR, "\"%s\" is btype %d, not SETSTR (%d)\n",
			_SAFESTR(setname), symptr->sym_basetype, SYMB_SETSTR); 
		return(-5);
	}

	msg_printf(JUST_DOIT, "setstr \"%s\", before pro_set call\n    ",
		_SAFESTR(symptr->sym_s));

	symptr = pro_set(symptr);

	if (symptr->sym_basetype == SYMB_SET) {
		sprintf_cset(setbuf, symptr->sym_set);
		msg_printf(JUST_DOIT,
		    "pro_set output: set 0Lx%Lx, btype SET(%d), type %d\n",
		    symptr->sym_set, symptr->sym_basetype, symptr->sym_type);
	} else {
		msg_printf(JUST_DOIT,
		    "pro_set failed: btype %d (0x%x), type %d (0x%x)\n",
		    symptr->sym_basetype, symptr->sym_basetype, 
		    symptr->sym_type, symptr->sym_type);
	}

	return((long)symptr);

} /* _dopro_set */

int
_dopro_setstr(int argc, char **argv)
{
	char *setname;
	sym_t *symptr;

	if (argc < 2) {
	    _ide_usage(argv[0]);
	    return(1);
	}

	ASSERT(argc >= 2 && (argv[1] != NULL));

	symptr = findsym(argv[1]);
	if (!symptr) {
	    msg_printf(USER_ERR, "%s: symbol `%s' not found\n",
		argv[0], argv[1]);
	    return(-1);
	}

	setname = symptr->sym_s;
	ASSERT(setname);

	msg_printf(JUST_DOIT, "  in \"%s\", argc %d:  ",
		argv[0], argc);

	msg_printf(JUST_DOIT,
	    " sym_name \"%s\", type %d, btype %d, ",
	    _SAFESTR(symptr->sym_name),symptr->sym_type,symptr->sym_basetype);

	if (symptr->sym_basetype != SYMB_SET) {
		msg_printf(USER_ERR, "\"%s\" is btype %d, not SET (%d)\n",
			_SAFESTR(setname), symptr->sym_basetype, SYMB_SET); 
		return(-5);
	}

	msg_printf(JUST_DOIT, "set 0Lx%Lx before pro_setstr call\n    ",
		symptr->sym_set);

	symptr = pro_setstr(symptr);

	if (symptr->sym_basetype == SYMB_SETSTR) {
		msg_printf(JUST_DOIT,
		    "pro_setstr output: setstr \"%s\", (0x%x) \n    ",
		    symptr->sym_s, symptr->sym_i);
		msg_printf(JUST_DOIT,
		    "basetype %d (0x%x), type %d (0x%x)\n",
		    symptr->sym_basetype, symptr->sym_basetype,
		    symptr->sym_type, symptr->sym_type);
	} else {
		msg_printf(JUST_DOIT,
		    "pro_setstr failed: btype %d (0x%x), type %d (0x%x)\n",
		    symptr->sym_basetype, symptr->sym_basetype, 
		    symptr->sym_type, symptr->sym_type);
		msg_printf(JUST_DOIT, "    _i val %d (0x%x)\n",
		    symptr->sym_i, symptr->sym_i);
	}

	return(0);

} /* _dopro_setstr */


int
_do_fill(int argc, char **argv)
{
	int i, size;
	char *arg;

	if (argc < 2)
	{
fill_usage:
	    printf("Usage: fill [-(b|h|w|d)] [-v VAL] ADDR1:ADDR2|ADDR#COUNT\n");
	    printf("       ADDR, ADDR1 and ADDR2 must be unmapped address\n");
	    return(1);
	}
	size = 4;
	for (i = 1; i < argc; i++)
	{
	    if (*argv[i] != '-')
	    {
		struct range range;

		arg = argv[i];
		if (! parse_range(arg, size, &range)) goto fill_usage;
		if (!IS_KSEGDM(range.ra_base) &&
		    !IS_COMPAT_PHYS(range.ra_base))
			goto fill_usage;
		break;
	    }
	    else
	    {
		arg = argv[i];
		switch (*++arg) {
		case 'b':
			size = SW_BYTE;
			break;

		case 'h':
			size = SW_HALFWORD;
			break;

		case 'w':
			size = SW_WORD;
			break;

		case 'd':
			size = SW_DOUBLEWORD;
			break;

		case 'v':
			i++;
			break;
		default:
			goto fill_usage;
		}
	    }
	}
	return fill(argc, argv);
} /* _do_fill */

int
_do_dump(int argc, char **argv)
{
	extern int dump(int,char**);
	int i, size;
	char *arg;

	if (argc < 2)
	{
dump_usage:
	    printf("Usage: dump [-(b|h|w|d)] [-(o|d|u|x|c|B)] ADDR1:ADDR2|ADDR#COUNT\n");
	    printf("       ADDR, ADDR1 and ADDR2 must be unmapped address\n");
	    return(1);
	}
	size = 4;
	for (i = 1; i < argc; i++)
	{
	    if (*argv[i] != '-')
	    {
		struct range range;

		arg = argv[i];
		if (! parse_range(arg, size, &range)) goto dump_usage;
		if (!IS_KSEGDM(range.ra_base) &&
		    !IS_COMPAT_PHYS(range.ra_base))
			goto dump_usage;
		break;
	    }
	    else
	    {
		arg = argv[i];
		switch (*++arg) {
		case 'b':
			size = SW_BYTE;
			break;

		case 'h':
			size = SW_HALFWORD;
			break;

		case 'w':
			size = SW_WORD;
			break;

		case 'd':
			size = SW_DOUBLEWORD;
			break;

		default:
			goto dump_usage;
		}
	    }
	}
	return dump(argc, argv);
} /* _do_dump */

/*
 * unformatted print
 */
_doprint(int argc, sym_t *argv[])
{
    char setbuffer[SETBUFSIZ];
    int i;
    char *pvar;

    for ( i = 1; i < argc; i++ ) {
	argv[i] = pro_str(argv[i]);

	if (argv[i]->sym_basetype == SYMB_SET) {
	    sprintf_cset(setbuffer, argv[i]->sym_set);
	    msg_printf(JUST_DOIT, "%s ", setbuffer);
	} else {
	    msg_printf(JUST_DOIT, "%s ", argv[i]->sym_s);
	}
    }
    msg_printf(JUST_DOIT,"\n");

    return 0;
}
/*
 *
 * formatted print
 *
 */
char dpbuf[PUTBUFSIZE];

_doprintf(argc, argv)
int argc;
sym_t *argv[];
{
	ide_int_type pargv[MAXARGC+1];
	char *fmt;
	int pargc;

	if ( argc < 2 )
		return 1;

	fmt = argv[1]->sym_s;

	for ( pargc = 2; pargc < argc; pargc++ )
		pargv[pargc-2] = argv[pargc]->sym_i;
	
	pargv[pargc-2] = 0;
	
	vsprintf(dpbuf, fmt, (char *)pargv);
	msg_printf(JUST_DOIT, "%s", dpbuf);

	return 0;
}
/*
 * read - shell style read
 *
 * usage: read x y z
 *
 * Assigns variables x, and y to the first two space seperated words
 * and z to all remaining characters.
 *
 */
int
_doread(int argc, sym_t *argv[])
{
	char readbuffer[MAXSTRLEN+1];
	char wordbuffer[MAXSTRLEN+1];
	char *rp = readbuffer;
	char *wp = wordbuffer;
	sym_t *psym = 0;

	if ( ! gets(readbuffer) )
		return 0;

	if ( ! --argc )
		return 0;

	do	{
		if ( !*rp || ((*rp == ' ' || *rp == '\t') && argc) ) {
			if ( wp != wordbuffer ) {
				*wp = '\0';
				if ( ! psym )
					psym = argv[1];
				if ( psym->sym_type == SYM_UNDEFINED ) {
					psym->sym_type = SYM_VAR;
					psym->sym_basetype= SYMB_UNDEFINED;
					psym->sym_name = psym->sym_s;
					psym->sym_s = 0;
					addsym(psym);
					}
				if ( psym->sym_type == SYM_VAR ) {
					if ((psym->sym_basetype == SYMB_STR) ||
					    (psym->sym_basetype==SYMB_SETSTR)) {
						destroystr(psym->sym_s);
						psym->sym_s = NULL;
						}
					psym->sym_basetype = SYMB_STR;
					psym->sym_s = makestr(wordbuffer);
					}
				psym++;
				argc--;
				wp = wordbuffer;
				}
			if ( ! *rp )
				break;
			}
		else
			*wp++ = *rp++;
		} while ( 1 );
	
	return 0;
}

static int curline;			/* current line within the page */
static char h_usage[] = "\thelp          : this message\n\thelp all      : prints all ide commands & diagnostics\n\thelp commands : prints builtin commands only\n\thelp sets     : prints set commands only\n\thelp loops    : prints loop commands only\n\thelp debug    : prints debug commands only\n\thelp string   : returns help for commands & diagnostics matching string";
static int cmds_only = 0;

int
_dohelp(int argc, char *argv[])
{
    char *help;

    /* XXX fix usage and add any_command */
    /* XXX maybe force all,...,debug with caps */

    /* 'help' with no args displays subsystem choices and returns */
    if (argc == 1) {
	msg_printf(JUST_DOIT, "usage:\n%s\n", h_usage);
	return(0);
    }

    if (argc > 1) 
      help = argv[1];
    else
      return(1);

    curline = 0;

    /* help all */
    if (!strcmp(help,"all")) {
	doqhelp();
	_do_loop_help();
	return(0);
    }

    /* help commands */
    if (!strcmp(help,"commands")) {
      cmds_only = 1;
      doqhelp();
      cmds_only = 0;
    }

    /* help sets */
    if (!strcmp(help,"sets")) {
	_do_set_help();
	return(0);
    }

    /* help loops */
    if (!strcmp(help,"loops"))
      _do_loop_help();

    /* help debug */
    if (!strcmp(help,"debug")) {
	_do_dbg_help();
	return(0);
    }
    
    /* otherwise try to get help string on first argument */
    _do_help(help);

    return(0);

} /* _dohelp2 */


static int 
_do_help(char* cmd) {

  char pagebuf[PAGEBUFSIZE];
  int i;
  int found = 0;
  int cmd_len;

  /* get length of command */
  cmd_len = strlen(cmd);

  /* search builtins for command */
  for ( i = 0; i < nbuiltins; i++ ) 
    if (!strncmp(cmd,builtins[i].name,cmd_len)) {
	sprintf(pagebuf,"%-10s - %s\n",builtins[i].name,builtins[i].help);
	pager(pagebuf, pagesize, &curline);
	found = 1;
    }

  /* search diagcmds for command */
  for ( i = 0; i < ndiagcmds; i++ ) 
    if (!strncmp(cmd,diagcmds[i].name,cmd_len)) {
	sprintf(pagebuf,"%-10s - %s\n",diagcmds[i].name,diagcmds[i].help);
	pager(pagebuf, pagesize, &curline);
	found = 1;
    }

  /* couldnt find command */
  if (!found) {
    sprintf(pagebuf,"%s is not a command\n",cmd);
    pager(pagebuf, pagesize, &curline);  
  }

  /* success */
  return(0);
}

int
doqhelp(void)
{
    char pagebuf[PAGEBUFSIZE];
    int i;
    char *systype = inv_findcpu();
    int ndiagcmds = *ndiagcmds_ptr;
    builtin_t *diagcmds = diagcmds_ptr;

    curline = 0;
    sprintf(pagebuf, "           IDE Generic Commands\n");
    pager(pagebuf, pagesize, &curline);

/* "*builtins[i].help" checked for "" ('\0' in first byte) */

    /* first the generic routines IDE provides to scripts */
    for ( i = 0; i < nbuiltins; i++ )
	if (builtins[i].type == B_CMD || builtins[i].type == B_SCMD) {
	    sprintf(pagebuf, "%-10s - %s",builtins[i].name, builtins[i].help);
	    pager(pagebuf, pagesize, &curline);
	}

    /* next IDE primitives that involve platform-specific hardware */
    sprintf(pagebuf, "\n           IDE Architecture-Specific Commands:\n");
    pager(pagebuf, pagesize, &curline);
    for ( i = 0; i < ndiagcmds; i++ )
	if ( (diagcmds[i].type == B_CPUSPEC_CMD ||
	      diagcmds[i].type == B_CPUSPEC_SCMD) && *diagcmds[i].help)
		if (!(diagcmds[i].name[0] == '.' && Reportlevel < VRB)) {
			sprintf(pagebuf, "%-10s - %s", (diagcmds[i].name[0] == '.' ?
				&diagcmds[i].name[1] : diagcmds[i].name), diagcmds[i].help);
			pager(pagebuf, pagesize, &curline);
		}

#ifdef MULTIPROCESSOR
    /* next set-manipulation and display routines */
    sprintf(pagebuf, "\n           IDE CPU-set Commands:\n");
    pager(pagebuf, pagesize, &curline);
    for ( i = 0; i < nbuiltins; i++ )
	if (builtins[i].type == B_SET_CMD) {
	    sprintf(pagebuf, "%-10s - %s", builtins[i].name, builtins[i].help);
	    pager(pagebuf, pagesize, &curline);
	}
#endif

    if (!cmds_only) {
	sprintf(pagebuf, "\n            %s Diagnostics:\n",
		(systype ? systype : ""));
	pager(pagebuf, pagesize, &curline);
	for ( i = 0; i < ndiagcmds; i++ ) {
	    if ( (diagcmds[i].type != B_CPUSPEC_CMD &&
	         diagcmds[i].type != B_CPUSPEC_SCMD) && *diagcmds[i].help)
			if (!(diagcmds[i].name[0] == '.' && Reportlevel < VRB)) {
			sprintf(pagebuf, "%-10s - %s", (diagcmds[i].name[0] == '.' ?
				&diagcmds[i].name[1] : diagcmds[i].name), diagcmds[i].help);
				pager(pagebuf, pagesize, &curline);
	    	}
	}
    }

    return 0;

} /* doqhelp */

static int
_do_dbg_help(void)
{
    char pagebuf[PAGEBUFSIZE];
    int i;
    int ndiagcmds = *ndiagcmds_ptr;
    builtin_t *diagcmds = diagcmds_ptr;

    sprintf(pagebuf, "      IDE-debugging commands:\n");
    pager(pagebuf, pagesize, &curline);

    for ( i = 0; i < nbuiltins; i++ )
	if (builtins[i].type==B_DEBUG_CMD || builtins[i].type==B_DEBUG_SCMD) {
	    sprintf(pagebuf, "      %-10s", builtins[i].name);
	    pager(pagebuf, pagesize, &curline);
	}

    sprintf(pagebuf, "\n      IDE arch-specific debugging commands:\n");
    pager(pagebuf, pagesize, &curline);
    for ( i = 0; i < ndiagcmds; i++ )
	if ( (diagcmds[i].type == B_CPUSPEC_CMD ||
	      diagcmds[i].type == B_CPUSPEC_SCMD) && *diagcmds[i].help) {
	    sprintf(pagebuf, "%-10s", diagcmds[i].name);
	    pager(pagebuf, pagesize, &curline);
	}

    return 0;

} /* _do_dbg_help */

static int
_do_loop_help(void)
{
    char pagebuf[PAGEBUFSIZE];


    pager("repeat n cmd\n        -- repeat `cmd' n times",
	  pagesize, &curline);
    pager("while ( expr ) cmd\n        -- execute `cmd' while expr is true",
	  pagesize, &curline);
    pager("for ( expr1; expr2; expr3 ) cmd\n        -- execute `cmd' while expr2 is true",
	  pagesize, &curline);
    pager("{ cmd ; cmd ... }\n        -- commands may grouped with `{' and `}'",
	  pagesize, &curline);
    pager("if ( expr ) cmd1 [ else cmd2 ] [fi]\n        -- execute cmd1 if expr is true, cmd2 otherwise; `fi' is optional",
	  pagesize, &curline);

    return 0;

} /* _do_loop_help */


/*ARGSUSED*/
int
_doexit(int argc, sym_t *argv[])
{
#ifdef _STANDALONE		/* EIM() returns no value */
	EnterInteractiveMode();
#else
	if ( argc > 1 && argv[1]->sym_basetype == SYMB_INT )
		exit(argv[1]->sym_i);
	else
		exit(0);
#endif
	/*NOTREACHED*/
}

_dosource(int argc, char *argv[])
{
	if (argc != 3 || strcmp(argv[1],"-f") ) {
		msg_printf(USER_ERR, "usage: source -f FILESPEC\n");
		return(1);
	}

	if ( ide_dosource == 1 ) {
		if ( bp != bufr && bp != bufr+cc )
			msg_printf(USER_ERR, "WARNING: sourcing a new file before the end of old\n");
		bp = bufr;
		Close(fd);
	}

	if (Open((CHAR *)argv[2], OpenReadOnly, &fd)) {
		msg_printf(USER_ERR, "can't open %s\n", *argv);
		ide_dosource = 0;
		return(1);
	}
	ide_dosource = 1;
	return(0);
} /* _dosource */

/* 
 * _dodelay provides ide scripts with the ability to pause (sleep)
 * for an interval which may be specified in usecs (-u), msecs (-m),
 * or seconds (-s).
 * Returns 0 if option and arg syntax is legit; else 1.
 */
int
_dodelay(int argc, char **argv)
{
    int res1, index, val;
    uint total_us = 0;
    optlist_t optlist;
    option_t *optionp;
#ifdef NOTRIGHTNOW
    int res2;
#endif

#ifdef IDEBUG
    if (Reportlevel >= VDBG) {
	msg_printf(JUST_DOIT, "    %s", "ide_delay");
	if (Reportlevel >= VDBG)
	    _dump_argvs(argc, argv);
    }
#endif

    optlist.maxopts = (int)OPTCCNT(DELAY_OPTS);
    if (res1 = _get_options(argc, argv, DELAY_OPTS, &optlist)) {
	msg_printf(USER_ERR, "%s: bad option or argument (error %d)\n",
		"ide_delay", res1);
#ifdef NOTRIGHTNOW
	res2 = _ide_usage(argv[0]);
	ASSERT(res2 != SYM_NOT_FOUND);
	ASSERT(res2 != NOHELPSTRING);
#endif
	return(res1);
    }

    if (optlist.optcount <= 0) {
	msg_printf(USER_ERR, "%s: must specify delay duration\n", "ide_delay");
#ifdef NOTRIGHTNOW
	res2 = _ide_usage(argv[0]);
	ASSERT(res2 != SYM_NOT_FOUND);
	ASSERT(res2 != NOHELPSTRING);
#endif
	return(BADOPTCOUNT);
    }

#ifdef IDEBUG
    if (Reportlevel >= VDBG) {
	_dump_optlist(&optlist);
    }
#endif

    for (index = 0; index < optlist.optcount; index++) {
	optionp = (option_t *)&optlist.options[index];
	if (!atob(optionp->args[0], &val) || val < 0) {
	    msg_printf(USER_ERR, "%s: '%c's arg must be a positive integer\n",
		"ide_delay", optionp->opt);
#ifdef NOTRIGHTNOW
	    _ide_usage(argv[0]);
#endif
	    return(OPTARGERR);
	}

	switch(optionp->opt) {
	case 'u':
	    total_us += val;
	    break;
	case 'm':
	    total_us += (val * US_PER_MS);
	    break;
	case 's':
	    total_us += (val * US_PER_SEC);
	    break;
	default:
	    msg_printf(IDE_ERR, "%s: illegal opt '%c'\n", 
		"ide_delay", optionp->opt);
	    ide_panic(argv[0]);
	}
    }
#ifdef IDEBUG
    if (Reportlevel >= VDBG) {
	msg_printf(DBG, "    %s: delay %d usecs\n", "ide_delay",
		total_us);
    }
#endif

    us_delay(total_us);

    return(0); 

} /* _dodelay */


/*
 * code assumes \n at end of file
 */
int
getsfromsource(char *p)
{
	char c;
	int i = 0;
	long rc;

	while ( 1 ) {
		/* refill buffer if needed */
		if ( bp == bufr ) {
			if ((rc=Read(fd,bufr,sizeof(bufr),&cc))||!cc) {
				if ( rc ) 
					msg_printf(IDE_ERR,
						"*** error on read\n");
				Close(fd);
				ide_dosource = 0;
				return(i);
			}
		}
	
		while ( bp < (bufr + cc) ) {
			c = *bp++;
			*p++ = c;
			i++;
			if ( c == '\n' )
				return(i);
		}
		bp = bufr;
	}
}



int
testfn(int argc, char **argv)
{
    int val = 123;

    msg_printf(JUST_DOIT, "\n      in testfn, dump args:\n\t");
    _dump_argvs(argc, argv);
    msg_printf(JUST_DOIT, "\n      exit testfn, return %d\n", val);

    return(123);

}

#if EVEREST
int
dump_mpconf(int argc, char **argv)
{
    register volatile struct mpconf_blk *mpconf;
    uint my_vpid = cpuid();	/* fetch logical cpu# */ 
    uint virt_id;

    msg_printf(JUST_DOIT, "%s:\n", argv[0]);

    for (virt_id = 0; virt_id < EV_MAX_CPUS; virt_id++) {
	mpconf = &MPCONF[virt_id];

	if (mpconf->mpconf_magic == 0) {
	    continue;
	} else if (mpconf->mpconf_magic == MPCONF_MAGIC) {
	    msg_printf(JUST_DOIT,
	        "  vloop 0x%x: _virt_id 0x%x, _phys_id 0x%x, magic 0x%x\n",
		virt_id,mpconf->virt_id,mpconf->phys_id,mpconf->mpconf_magic);
	    if (virt_id != mpconf->virt_id)
		msg_printf(IDE_ERR, " !!! ===> virt_id != virt_id <=== !!!\n");
	} else {
	    msg_printf(IDE_ERR,
		"!!vidcnt 0x%x BAD MAGIC (0x%x): _v_id 0x%x _p_id 0x%x\n",
		virt_id,mpconf->mpconf_magic,mpconf->virt_id,mpconf->phys_id);
	}
    }

    return(0);

} /* dump_mpconf */
#endif /* EVEREST */


char default_fname[] = "test_charcmd";
struct string_list exec_slist;
char *strptrs[4];
/*
 * exec -f <fn> -v <vids> -a <args>
 * opt defaults are 'test_charcmd', 0, and NULL, resp.
 */
int
do_exec(int argc, char **argv)
{
    optlist_t optlist;
    option_t *optionp;
    setlist_t slist;
    fnlist_t fn_symlist;
    vplist_t vplist;
    set_t runset, mask;
    int itsadiag=0;
    int vidx,sidx,aidx,fidx, i, rc;
    int floop;
    uint launch_vid, sflags=0;
    argdesc_t fnargs;
    sym_t *tsym=0;
    int fnargc, local_toldem=0;
    char **fnargv;
    char fname[MAXIDENT];
    volvp func;				/* routine to exec */
    char setbuffer[SETBUFSIZ];
    uint vid_me = cpuid();
#if EVEREST
    uint my_pidx = vid_to_phys_idx[vid_me];
    char buf[MAXIDENT];
    char *test_result;			/* set to PASSED, SKIPPED, or FAIL */
#endif
#if defined(LAUNCH_DEBUG)
    char linebuf[MAXSTRLEN];
#endif

    if (_get_options(argc, argv, EXEC_OPTS, &optlist)) {
	_ide_usage(argv[0]);
	return(1);
    }

#if defined(LAUNCH_DEBUG)
    if (Reportlevel >= V4DBG) {
	msg_printf(JUST_DOIT, "  %s, %d arg%s:  ", argv[0],
		optlist.optcount, (optlist.optcount != 1 ? "s" : ""));
	_dump_optlist(&optlist);
    }
#endif /* LAUNCH_DEBUG */

    strcpy(fname, default_fname);
    runset = 0;
    mask = 0;

    /*
     * -v and -s opts are mutually exclusive
     */
    vidx = _opt_index('v', optlist.opts);
    sidx = _opt_index('s', optlist.opts);
    if (sidx >= 0 && vidx >= 0) {
	msg_printf(USER_ERR, "%s: '-v' and '-s' are mutually-exclusive\n");
	_ide_usage(argv[0]);
	return(1);
    }

    /*
     * see if script requested execution across a set of cpus
     */
    if (sidx >= 0) {
	slist.setcnt = 1;
	if (rc = _do_setfnprep(&optlist, &slist, NULL, 0)) {
	    return(rc);
	}
	ASSERT(slist.setcnt == 1);
	runset = slist.setptrs[SRC1]->sym_set;
	sprintf_cset(setbuffer, runset);
	msg_printf(VRB, "  -s,cpu set: %s (0x%x)\n", setbuffer, runset);
    } else {
	/*
	 * figure out the set of processors on which to iteratively execute 
	 * the function: if no 'v' opt specified, default to local processor.
	 */
	if (vidx < 0) {
	    if (Reportlevel > DBG) {
		msg_printf(JUST_DOIT, " no -v opt: run locally (vid %d)\n",
			vid_me);
	    }
	    vplist.vpcnt = 1;
	    vplist.vps[0] = vid_me;
	} else {
	    /* range-check the vids */
	    vplist.vpcnt = ONE_OR_MORE_ARGS;

	    /* _do_setfnprep() displays any pertinent error messages */
	    if (rc = _do_setfnprep(&optlist, NULL, &vplist, 0)) {
		return(rc);
	    }

#if defined(LAUNCH_DEBUG)
	    if (Reportlevel >= V4DBG) {
		msg_printf(JUST_DOIT,"    %s:  %d good vid%s: ", argv[0],
			vplist.vpcnt, (vplist.vpcnt == 1 ? "" : "s"));
		for (i = 0; i < vplist.vpcnt; i++) {
		    msg_printf(JUST_DOIT, " %d ", vplist.vps[i]);
	        }
	        msg_printf(JUST_DOIT, "\n");
	    }
#endif /* LAUNCH_DEBUG */
	}

	/*
	 * convert the list of vids to a set for convenience.
	 * Range-check using the macro that verifies against
	 * the currently active cpus (not just MAXSETSIZE)
	 */
	for (i = 0; i < vplist.vpcnt; i++) {
		ASSERT(!(VID_OUTARANGE(vplist.vps[i])));
		runset |= (SET_ZEROBIT >> vplist.vps[i]);
	}

#if defined(LAUNCH_DEBUG)
	sprintf_cset(setbuffer, runset);
	msg_printf(VRB, "  vid-list: %s\n", setbuffer);
#endif

    } /* end 's' or 'v' opt */

    /*
     * pass arguments to function? ('a')
     */
    if ((aidx = _opt_index('a', optlist.opts)) < 0) {
	msg_printf(V3DBG, " -no args- ");
	strptrs[0] = fname;
	strptrs[1] = NULL;
	fnargs.argc = 1;
	fnargs.argv = &strptrs[0];
    } else {
	/* 
	 * copy arg strings and array of strptrs into mastervid's 
	 * arg_str struct
	 */
	optionp = (option_t *)&optlist.options[aidx];
	if (optionp->argcount > 0)
	    msg_printf(V3DBG, "   exec> parsed %d arg%s:  ", optionp->argcount,
		(optionp->argcount != 1 ? "s" : ""));
	for (i = 0; i < optionp->argcount; i++) {
	    msg_printf(V3DBG, "%s %s\n", (i ? "," : ""), optionp->args[i]);
	}
	msg_printf(V3DBG, "\n");

	fnargc = optionp->argcount;
	fnargv = &optionp->args[0];
	ide_initargv(&fnargc, &fnargv, &exec_slist);

	fnargs.argc = exec_slist.strcnt;
	fnargs.argv = exec_slist.strptrs;

	if (Reportlevel > VDBG) {
	    msg_printf(JUST_DOIT, " exec> dump copied args: ");
	    _dump_argvs(fnargs.argc, fnargs.argv);
	}

	/*
	 * use the default fname for now in arg[0] as a placeholder.
	 * swap it later: we may be iterating through functions anyway.
	 */
	msg_printf(VDBG+1, "\n exec> shift up for '%s' in position %d\n",
		fname, 0);

	_insert_arg(&fnargs, fname, 0);

	if (Reportlevel > VDBG) {
	    msg_printf(JUST_DOIT, " and dump again: ");
	    _dump_argvs(fnargs.argc, fnargs.argv);
	}
	if (Reportlevel > DBG) {
	    _dump_argvs(fnargs.argc, fnargs.argv);
	}
    } /* end of -a opt */


    /*
     * first parse the function(s) to execute.  If no 'f' opt is 
     * specified, run test_charcmd() (dumps the arg list it received)
     */
    if ((fidx = _opt_index('f', optlist.opts)) < 0) {
	msg_printf(V3DBG,
	    "exec: invoke fn `%s' by default (no -f flag)\n", default_fname);
	fn_symlist.fn_symptrs[0] = findsym(default_fname);
	ASSERT(fn_symlist.fn_symptrs[0]);
	fn_symlist.fncnt = 1;
    } else {
	optionp = (option_t *)&optlist.options[fidx];
	rc = _check_fns(optionp, &fn_symlist);
	if (rc != 0) {	/* at least one fnname failed */
	    msg_printf(USER_ERR,"%s: function-name error %d\n",
		argv[0], rc);
	    return(rc);
	}
    }

    if (Reportlevel >=  DBG) {
	msg_printf(DBG,"    %d fnname%s verified", fn_symlist.fncnt, 
	    _PLURAL(fn_symlist.fncnt));
#if defined(LAUNCH_DEBUG)
	if (Reportlevel > DBG) {
	    char *bp = &linebuf[0];

	    for (i = 0; i < fn_symlist.fncnt; i++) {
		bp += sprintf(bp," %s",fn_symlist.fn_symptrs[i]->sym_name);
	    }
	    msg_printf(JUST_DOIT,"\n%s\n", linebuf);
	}
#endif
    }

    for (floop = 0; floop < fn_symlist.fncnt; floop++) {
	int logidx;
	int logit = error_logging();

	tsym = fn_symlist.fn_symptrs[floop];
	strcpy(fname, tsym->sym_name);
	func = (volvp)tsym->sym_func;
	logidx = tsym->sym_logidx;
	sflags = tsym->sym_flags;
	if (DIAG_EXEC(sflags))
	    itsadiag = 1;
	else
	    itsadiag = 0;
	msg_printf(DBG, "  exec %s `%s' on %s\n",
		(itsadiag ? "diagnostic":"function"), fname, setbuffer);

#if EVEREST
	if (ID_DIAG(sflags))
	    msg_printf(JUST_DOIT, "  %s:\n", fname);
#endif
	mask = SET_ZEROBIT;

	for (launch_vid = 0; launch_vid <= _ide_info.vid_max; launch_vid++) {

	    if (itsadiag)
		local_toldem += check_state(vid_me, local_toldem);

	    if (mask & runset) {
#if EVEREST
		if (ID_DIAG(sflags))
		    msg_printf(JUST_DOIT, "    vid %d -", launch_vid);
#endif
		rc = _do_launch(launch_vid, func, &fnargs);

		if (itsadiag && logit) {
		    switch (rc)
		    {
			case TEST_PASSED:
			    statlog[logidx].passcnt[launch_vid]++;
			    statlog[logidx].pass_tot++;
#if EVEREST
			    test_result = "PASSED";
#endif
			    break;

			case TEST_SKIPPED:
			    statlog[logidx].skipcnt[launch_vid]++;
			    statlog[logidx].skip_tot++;
#if EVEREST
			    test_result = "SKIPPED";
#endif
			    break;

			/*
			 * any value other than above is failure
			 */
			default:
			    statlog[logidx].failcnt[launch_vid]++;
			    statlog[logidx].fail_tot++;
#if EVEREST
			    test_result = "FAILED";
#endif
		    }
		}
#if EVEREST
		if (ID_DIAG(sflags)) {
		    if (Reportlevel > ERR || (Reportlevel == ERR && rc))
			sprintf(buf, "      ");
		    else
			sprintf(buf, "-- ");
		    msg_printf(JUST_DOIT,"%s%s\n", buf, test_result);
	        }
#endif
	    }
#if EVEREST
	    if (itsadiag)
		local_toldem = check_state(vid_me, local_toldem);
#endif
	    mask >>= 1;	/* next element (cpu) */

	} /* cpu loop */

    } /* fn loop */

    msg_printf(V1DBG, "  %s returning %d (0x%x)\n",argv[0], rc, rc);

    return(rc);

} /* do_exec */


#ifdef IP32
/*
 * _do_date(): display date and time of script-execution
 */
int
_do_date(int argc, char **argv)
{
    int rc;

    msg_printf(DBG, "  _do_date: call date_cmd()\n");
    rc = date_cmd(argc, argv);
    if (rc)
	msg_printf(IDE_ERR, "_do_date: date_cmd returned %d\n", rc);

    return(rc);

} /* _do_date */
#endif /* IP32 */

#if EVEREST
/*
 * _do_cpumap(): display map of (slot/slice) <--> vids
 */
int
_do_cpumap(int argc, char **argv)
{
    int slot, slice, mvid, vid;
    evreg_t cpumask = (EV_GET_LOCAL(EV_SYSCONFIG)&EV_CPU_MASK) >> EV_CPU_SHFT;

    mvid = _ide_info.master_vid;
    slot = vid_to_slot[mvid];
    slice = vid_to_slice[mvid];

    /* XXX insufficient detail for TFP XXX */
    msg_printf(JUST_DOIT, "  %d cpu boards containing %d active cpus:\n",
	evboards.ip19, _ide_info.vid_cnt);

    /* translate (slot/slice) tuple to corresponding virtual processor id */
    for (slot=0; slot < EV_BOARD_MAX; slot++) {
	if (cpumask & 1) {	/* slot has a cpu board in it */
	    for (slice=0; slice<EV_CPU_PER_BOARD; slice++) {
		vid = ss_to_vid[slot][slice];
		if (vid == BAD_CPU) {
		    msg_printf(JUST_DOIT,
			"    < slice %d in slot %d is defective >\n",
			slice, slot);
		} else {
		    if (vid != EV_CPU_NONE)
		        msg_printf(JUST_DOIT,
				"    vid %d --> (slot%d/slice%d)%s",
				vid, slot, slice, (slice&0x1 ? "\n" : ""));
		}
	    }
	    msg_printf(JUST_DOIT, "\n");
	}
	cpumask = cpumask >> 1;
    }
    msg_printf(JUST_DOIT, "\n");

} /* _do_cpumap */
#endif /* EVEREST */


/* 
 * _doclearlog (clearlog) and _dodumplog (dumplog) manipulate IDE's
 * diagnostic-execution logging facility, which tracks for all cpus 
 * the results of tests for the duration of the IDE session.  The
 * script-usable function names are in parens.
 */

/* 
 * _do_clearlog zeroes out the pass and fail counts, but doesn't
 * bother the ptrs back to the symbols: log slots are assigned 
 * at bootup for the duration of the IDE session
 */
/*ARGSUSED*/
int
_doclearlog(int argc, char **argv)
{
    int dloop, clp;

    for (dloop = 0; dloop < MAXDIAG; dloop++) {
	statlog[dloop].pass_tot = statlog[dloop].fail_tot =
	statlog[dloop].skip_tot = 0;
	for (clp = 0; clp <= _ide_info.vid_max; clp++) {
	    statlog[dloop].passcnt[clp] = statlog[dloop].failcnt[clp] =
	    statlog[dloop].skipcnt[clp] = 0;
	}
    }
    msg_printf(JUST_DOIT,"  < %d log entries cleared >\n", diag_tally);

    return(0);

} /* _doclearlog */

/*ARGSUSED*/
int
_dodumplog(int argc, char **argv)
{
    register int dlp, clp, pass, skip, fail;
    register int totpass, totfail, totskip, totall;
    sym_t *tsym;

#ifdef NOO
    optlist_t optlist;
    option_t *optionp;

    if (Reportlevel >= DBG) {
	msg_printf(DBG,"    %s: ", argv[0]);
	_dump_argvs(argc, argv);
    }

    if (_get_options(argc, argv, CREATE_SET_OPTS, &optlist)) {
/* 	msg_printf(USER_ERR,"create_set: bad option or argument\n"); */
	_ide_set_usage(CREATE_SET, argv[0], 1);
	return(BADOPTSORARGS);
    }

    /* get setnames & initialization desired, if any; only allow one method */
    for (i = 0; i < strlen(optlist.opts); i++) {
	switch(optlist.opts[i]) {
	    case 's':
		if (sindex != -1) {
		    msg_printf(USER_ERR,
			"create_set: `-s' option repeated\n");
		    _ide_set_usage(CREATE_SET, argv[0], 1);
		    return(2);
		} else {
		    sindex = i;
		}
		break;
	    case 'a':
		if (initopt != CS_UNSPECIFIED)
		    initopt = CS_ILLEGAL;
		else
		    initopt = CS_ACTIVE;
		break;
	    case 'e':
		if (initopt != CS_UNSPECIFIED)
		    initopt = CS_ILLEGAL;
		else
		    initopt = CS_EMPTY;
		break;
	    case 'c':
		if (initopt != CS_UNSPECIFIED)
		    initopt = CS_ILLEGAL;
		else {
		    initopt = CS_COPYSET;
		    cindex = i;
		}
		break;
	    default:	/* getopts screwed up somehow. */
		msg_printf(IDE_ERR,"??? create_set; option %c???\n",
			optlist.opts[i]);
		return(-1);
	}
    }

#endif


    if (argc == 3) {
	msg_printf(JUST_DOIT,"\nDisplay diagnostic result-log (%d entries):\n", 
		diag_tally);
	for (dlp = 0; dlp < diag_tally; dlp++) {
	    if (!(tsym = statlog[dlp].diagsym))
		continue;
	    if (Reportlevel > DBG) {
		msg_printf(JUST_DOIT, "  logrec %d -->'%s' %s    %s", dlp,
		    tsym->sym_name,(tsym->sym_logidx==dlp?"":"!!NOMATCH!!"),
		    ((dlp & 0x1) ? "\n" : ""));
	    } else {
		msg_printf(JUST_DOIT, "  %d: '%s': %d pass/%d skip/%d fail%s ", 
			dlp, tsym->sym_name, statlog[dlp].pass_tot,
			statlog[dlp].skip_tot,
			statlog[dlp].fail_tot, (dlp % 2 ? " " : "\n"));
	    }
	}
    } else {
	msg_printf(JUST_DOIT,
		"\nDisplay results of all diags that have been run\n");
	for (dlp = 0; dlp < diag_tally; dlp++) {
	    if (!(tsym = statlog[dlp].diagsym))
		continue;
	    totpass=statlog[dlp].pass_tot;
	    totskip=statlog[dlp].skip_tot;
	    totfail=statlog[dlp].fail_tot;
	    if (!totpass && !totfail && !totskip)
		continue;
	    totall = totpass+totfail+totskip;
	    if (!totfail && !totskip) {
		msg_printf(JUST_DOIT,"  %s:  %d run%s; no skips or failures\n", 
			tsym->sym_name, totall, _PLURAL(totall));
	    } else {
		msg_printf(JUST_DOIT,
			"  %s:  %d run%s; %d failure %d skip %s total\n", 
			tsym->sym_name, totall, _PLURAL(totall), totfail,
			totskip, _PLURAL(totfail+totskip));
	    }
	    /*
	     * loop must include highest vid allocated (vid_max), not just
	     * vid_cnt.  These may not be the same.
	     */
	    for (clp = 0; clp <= _ide_info.vid_max; clp++) {
		pass = statlog[dlp].passcnt[clp];
		skip = statlog[dlp].skipcnt[clp];
		fail = statlog[dlp].failcnt[clp];
		if (!pass && !fail && !skip)
		    continue;
		msg_printf(JUST_DOIT,
			"  vid %d(%d run%s): %d pass/%d skip/%d fail\n",
			clp, (pass+fail+skip), _PLURAL(pass+fail+skip),
			pass, skip, fail);
	    }
	}
    }

    return 0;
} /* _dodumplog */

int
_do_dump_pda(int argc, char **argv)
{
    int vid_me = cpuid();
    int dumpvid;

    if (argc == 1) {
	dumpvid = vid_me;
    } else {
	if (!atob(argv[1], &dumpvid)) {
	    msg_printf(USER_ERR, "%s: must specify vid\n", argv[0]);
	    return(-1);
	}
    }

#ifdef notdef
    if (dumpvid == vid_me) {
        msg_printf(JUST_DOIT, "  dump mine! (vid %d); sp = 0x%x\n",
	    vid_me, get_sp());
    }
#endif

    msg_printf(JUST_DOIT,
	"  dump vid %d's PDAs (gpda 0x%x, ide_pda 0x%x)\n",
	    dumpvid, (__psunsigned_t)&gen_pda_tab[dumpvid],
	    (__psunsigned_t)&ide_pda_tab[dumpvid]);
    
    dump_gpda(dumpvid);
    dump_idepda(dumpvid);
    dump_saved_uregs(dumpvid);

    return(0);

} /* _do_dump_pda */


/*ARGSUSED*/
int
_do_printregs(int argc, char **argv)
{
    _ide_printregs();

    return(0);
} /* _do_printregs */


int
_do_showsym(int argc, char **argv)
{
    sym_t *psym;

    if (argc != 2) {
	_ide_usage(argv[0]);
    } else {
	psym = findsym(argv[1]);
	if (!psym) {
	    msg_printf(USER_ERR, "%s: symbol `%s' not found\n",
		argv[0], argv[1]);
	    return(-1);
	}
	_dumpsym(psym);
    }

    return 0;
} /* _do_showsym */

#if EVEREST
/*ARGSUSED*/
uint
clear_ip19intrs(int argc, char **argv)
{
    long long tmp;

    for (tmp = 0; tmp < EVINTR_MAX_LEVELS; tmp++)
	    EV_SET_LOCAL(EV_CIPL0, tmp);
    EV_SET_LOCAL(EV_IP0, 0);
    EV_SET_LOCAL(EV_IP1, 0);

    /*
     * as long as interrupts still pending, clear them
     */

    while (tmp = EV_GET_LOCAL(EV_HPIL)) {
	msg_printf(DBG, "interrupt level %x still pending\n", (int) tmp);
	EV_SET_LOCAL(EV_CIPL0, tmp);
    }
    EV_SET_REG(EV_CERTOIP, 0xffff);

    return(0);

} /* clear_ip19intrs */
#else
/*ARGSUSED*/
uint
clear_ip19intrs(int argc, char **argv)
{
    return 0;
}
#endif /* EVEREST */



#define DIVIDEND        0
#define FPUINVALID      0x10f80         /* all exceptions enabled , V cause */
#define SR_EXCEPT       (SR_CU0 | SR_CU1 | SR_IEC & ~SR_BEV)

/*
 * _do_fp_dump(): ensure that the SR_CU1 bit in the SR is not set, then
 * do an FP access.
 */
/*ARGSUSED*/
int	sregs[NREGS], s1regs[NREGS];

/*ARGSUSED*/
int
_do_fp_dump(int argc, char **argv)
{
    extern k_machreg_t get_pc(void);
    volatile float f1;    
    int vid_me = cpuid();
    int retval;
    ulong osr;

    msg_printf(JUST_DOIT, "  _do_fp_dump(vid %d, sp 0x%x): dump pda\n",
	vid_me, get_sp());
    dump_gpda(vid_me);

    if (setjmp(fault_buf)) {
	msg_printf(JUST_DOIT, "vid %d awake in setjmp! FP stat : 0x%x, pda:\n",
		vid_me, GetFPSR());

	dump_gpda(vid_me);

	if ((GetFPSR() & ~FP_COND) != FPUINVALID) {
	    msg_printf(JUST_DOIT, "FPU isn't FPUINVALID (0x%x)\n",
		(GetFPSR() & ~FP_COND));
	    retval = 1;
	}
	SetFPSR(0);
    }
    else
    {
	msg_printf(JUST_DOIT, "  setup: sp == 0x%x\n", get_sp());

	msg_printf(JUST_DOIT, "nofault before setting 0x%x\n",
		GPME(pda_nofault));
	set_nofault(fault_buf);
	msg_printf(JUST_DOIT, "nofault set to 0x%x\n", GPME(pda_nofault));

	/* clear cause register */
	set_cause(0);

	/* enable cache and fpu - cache ecc errors still enabled */
	osr = GetSR();
	msg_printf(JUST_DOIT, "  Original status reg setting : 0x%x\n", osr);
	SetSR(osr | SR_CU1);
	msg_printf(JUST_DOIT,"  Status reg setting for test 0x%x, cause 0x%x\n",
		(uint)GetSR(), (uint)get_cause());

	/* clear the FPSR */
/* Dont do it now
	saveregs(sregs);
	SetFPSR(0);
	saveregs(s1regs);
	msg_printf(JUST_DOIT, "Registers before exception: \n");
	for (retval = 0; retval < NREGS; retval++){
	    msg_printf(JUST_DOIT, "0x%10x ",sregs[retval]);
	    if ((retval & 0x7) == 0x7)
		msg_printf(JUST_DOIT, "\n");
	}
	msg_printf(JUST_DOIT, "Registers after exception: \n");
	for (retval = 0; retval < NREGS; retval++){
	    msg_printf(JUST_DOIT, "0x%10x ",s1regs[retval]);
	    if ((retval & 0x7) == 0x7)
		msg_printf(JUST_DOIT, "\n");
	}
*/
	/* set up fpu status register for exception */
	SetFPSR(FP_ENABLE);

	msg_printf(JUST_DOIT, "here we go...\n  --> ");
	putchar('\n');
	/* Convert to floating point */
	f1 = 0.0;
	f1 = f1/f1;

	us_delay(10);

	msg_printf(JUST_DOIT, "  ---> there we went...\n");

	/*
	 * if the error does not generate an exception, print the error
	 * message and exit
	 */
	msg_printf(ERR,
	    "  Divide by zero didn't dump! (pc 0x%x, sp 0x%x)\n", 
	    	get_pc(), get_sp());
	retval = 1;
	clear_nofault();
    }
    SetSR(osr);
    msg_printf(JUST_DOIT, "Completed fp_dump!\n");

    /* report any error */
    return(retval);

} /* _do_fp_dump */

/*
 * _enable_fp(): set the SR_CU1 bit in the SR and clear out the system
 */
/*ARGSUSED*/
int
_enable_fp(int argc, char **argv)
{
    ulong osr;

    msg_printf(JUST_DOIT, "  _enable_fp: set C1 bit in SR\n");
   
    set_cause(0);		/* clear cause register */

    /* enable cache and fpu - cache ecc errors still enabled */
    osr = GetSR();
    msg_printf(INFO, "  Original status register setting : %R\n", osr, sr_desc);
    SetSR(osr | SR_CU1);
    msg_printf(INFO, "  Current SR setting: %R\n", GetSR(), sr_desc);

    /* clear the FPSR */
    SetFPSR(0);

    /* set up fpu status register for exception */
    SetFPSR(FP_ENABLE);
    return(0);

} /* _enable_fp */


/* temporary kludge until I fix the ide bug that prevents symbols
 * from remaining in sync with their runtime counterparts (e.g. 
 * the C variable "Reportlevel" and the ide script-visable symbol
 * "report"
 * I'll break existing scripts if I remove the entrypoint, so it'll
 * just return.
 */
/*ARGSUSED*/
int
_sync(int argc, char **argv)
{
    msg_printf(JUST_DOIT, "in _sync, argv[0]: `%s'\n",argv[0]);
    return(0);

} /* _sync */


#ifdef NONO
+ /*
+  * date_cmd.c - print out the date stored in the BB clock
+  */
+ 
+ #include <arcs/types.h>
+ #include <arcs/time.h>
+ #include <libsc.h>
+ 
+ static char *month[] = {
+     "",
+     "January",
+     "February",
+     "March",
+     "April",
+     "May",
+     "June",
+     "July",
+     "August",
+     "September",
+     "October",
+     "November",
+     "December"
+ };
+ 
+ /*ARGSUSED*/
+ int
+ date_cmd(int argc, char **argv)
+ {
+     TIMEINFO *t;
+     
+     t = GetTime();
+ 
+ #ifdef	BASE_OFFSET
+ /* old proms used to return year offset since 1970
+  */
+ #define BASE_YEAR	1970		/* should be 0 */
+ 
+     if (t->Year < BASE_YEAR)
+ 	t->Year += BASE_YEAR;
+ #endif	/* BASE_OFFSET */
+ 
+     printf ("Decimal date: %u %u %u %u %u %u %u.\n",
+ 	t->Month, t->Day, t->Year, t->Hour, t->Minutes, t->Seconds,
+ 	t->Milliseconds);
+     if ((t->Month < 1 || t->Month > 12) ||
+ 	    (t->Day < 1 || t->Day > 31) ||
+ 	    (t->Year < 1 || t->Year > 2010) ||
+ 	    (t->Hour > 23) || (t->Minutes > 59) || (t->Seconds > 59) ||
+ 	    (t->Milliseconds > 999)) {
+ 	printf ("Error in date\n");
+ 	return 0;
+     }
+     printf ("%u %s %u, %02u:%02u:%02u\n", t->Day, month[t->Month], t->Year,
+ 	t->Hour, t->Minutes, t->Seconds);
+ 
+     return 0;
+ }
#endif /* NONONO */

#define RESET_BUSYCNT	500

/*ARGSUSED*/
int
_testbusy(int argc, char **argv)
{
    int busy_count = 0;

    msg_printf(JUST_DOIT, "  begin _testbusy()\n");

    busy(0);
    while (1) {
	/*
	 * perform one iteration of some test here
	 */
	us_delay(US_PER_MS);

	if (!busy_count--) {
	    busy_count = RESET_BUSYCNT;
	    busy(1);
	}
    }
} /* _testbusy */


/*
 * EVEREST/uif/msg_printf.c is now much different from the IP2{0,2}
 * versions.  Ensure that all versions unconditionally display all
 * the special new msglevels used by IDE.
 */
/*ARGSUSED*/
int
_do_msgtest(int argc, char **argv)
{
    register int cnt=0;
    register uint mask;
    __psint_t oldrep = Reportlevel;

    msg_printf(JUST_DOIT,
	"  Begin %s(_do_msgtest)--expect the following msglevels:\n", argv[0]);
    msg_printf(JUST_DOIT, "    USER_ERR (0x%x), IDE_ERR (0x%x), and each ",
	USER_ERR, IDE_ERR);
    msg_printf(JUST_DOIT, "of the\n    upper 16 bits separately.");
    msg_printf(JUST_DOIT, "  Messages 0x4000 and 0x8000\n   should NOT ");
    msg_printf(JUST_DOIT, "display.  Reportlevel will be `SUM' (%d)\n\n", SUM);
    Reportlevel = SUM;

    msg_printf(USER_ERR, "This is a user error message (%d/0x%x)\n", 
	USER_ERR, USER_ERR);
    msg_printf(IDE_ERR, "This is an IDE-internal error message (%d/0x%x)\n\n",
	IDE_ERR, IDE_ERR);

    /*
     * USER_ERR and IDE_ERR levels are bits 12 and 13, so begin testing
     * at bit 14 (msglevels of bits 14 and 15 shouldn't show)
     */
    for (cnt = 14; cnt < 32; cnt++) {
	mask = (0x1 << cnt);
	msg_printf(mask, "This message is level 0x%x/%d\n", mask, mask);
    }
    Reportlevel = oldrep;

    msg_printf(JUST_DOIT,
	"\n  %s tested msglevels 0x1000, 0x2000, and 0x10000-0x10000000\n\n");

    return(0);

} /* _do_msgtest */


#define PANIC_MSG	"ALL SHOOK UP"

/*ARGSUSED*/
void
_do_panic(int argc, char *argv[])
{
	msg_printf(JUST_DOIT, "IDE(v%d): Here we go, panic'ing for fun\n!",
		   cpuid());

	ide_panic(PANIC_MSG);
} /* _do_panic */
