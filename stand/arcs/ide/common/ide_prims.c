
#ident "$Revision: 1.37 $"

/*
 * arcs/ide/ide_prims.c: functions provided by IDE to diagnostic writers
 */

/*
 * ide_prims.c contains routines which provide support to functions
 * which are script-callable, but are themselves callable only internally.
 * Routines which are called from ide scripts should not be in this file.
 */

#include <arcs/types.h>
#include <arcs/signal.h>

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/param.h>

#include <arcs/io.h>
#include <genpda.h>
#include <stringlist.h>
#include "libsc.h"
#include "uif.h"
#include <ide_msg.h>
#include <alocks.h>

#include "ide.h"
#include "ide_sets.h"
#include "ide_mp.h"
#include <libsc.h>
#include <libsk.h>

#if EVEREST
#include "sys/EVEREST/IP19addrs.h"
#include "sys/EVEREST/evconfig.h"
#include "sys/EVEREST/evmp.h"
#include "sys/EVEREST/evintr.h"
#include "sys/EVEREST/everest.h"
#endif /* EVEREST */

extern void	_ide_init_hashtab(void);
extern int	ide_update_prompt(void);

static void	_initlog(void);
static int	ide_init_consts(void);
static void	ide_check_tblsizes(void);
extern int	_get_arglist(char *, option_t *);
static int	ide_getopt(int, char *const *, const char *);
static void	_init_getopt(void);

extern int	pager(char *, int, int *);
extern void	_assert(const char *, const char *, int);
extern int	_get_options(int, char **, char *, optlist_t *);
extern int	_opt_index(char, char *);
extern void	_dump_getopt_vars(int);
extern void	_dump_argvs(int, char **);
extern int	_dump_optlist(optlist_t *);
builtin_t	*_get_tbl_entry(char *);
extern int	get_mvid(void);
extern int	_ide_usage(char *);
extern char	*_strtok(register char *, const char *);
extern char	*_strtok_r(char *, const char *, char **);
extern char	*_strpbrk(register const char *, const char *);
extern size_t	_strspn(const char *, const char *);
extern char	*_strchr(const char *, int);
extern int	_charcnt(const char *, char);
extern void	ide_initargv(int *, char ***, struct string_list *);
extern int	test_charcmd();
extern char	*inv_findcpu(void);
extern int	initmstats(void);

#if EVEREST
static int	_slaves_to_prom(void);
#endif

extern int	did_htab_init;
extern int	sbtype_tblsiz, stype_tblsiz, sflag_tblsiz;

/*
 * register-format descriptions, used by ide_printregs
 */
extern struct reg_desc sr_desc[], cause_desc[], r4k_config_desc[];

/*
 * boot_rlevel determines the msg-verbosity level while IDE is booting
 * (before scripts have access to $report).  Since C won't preinitialize
 * unions, point Reportlevel to boot_rlevel until Reportsym is initialized.
 */
ide_int_type boot_rlevel = INFO;

/*
 * all other instances of "Reportlevel" in ide* files are #defined to 
 * be (*Reportlevel) to save changing all of them right now.  The
 * actual declaration must therefore be shielded.  The 'Reportlevel'
 * global is referenced frequently within IDE, so the the normal method
 * of sharing the script-symbol values with IDE's runtime code--using
 * findsym() to fetch the symbol, then using the value field of that
 * symbol--was optimized as a pointer to that field.
 */
#undef Reportlevel
ide_int_type *Reportlevel = &boot_rlevel;	/* use until Reportsym is set up */
#define Reportlevel	(*Reportlevel)

/*
 * Slave_Timeout can wait for initialization, since it is only (currently)
 * used in parallel execution mode.  By the time it is called, the values
 * have been initialized.  The speed optimization may be unnecessary, but
 * since it is used in some tight polling loops it seemed like a good idea
 */
ide_int_type *_Slave_Timeout;

/*
 * variables used by ide_getopt (adapted from libc's getopt())
 */
int optind = 1;
int optopt = 0;
int opterr = 1;		/* suppress ide_getopt error message display if == 0 */
char *optarg = 0;
int _sp = 1;

/* msg_printf mustn't flash LEDs when system is running coherently */ 
volatile int _c_state = NON_COHERENT;
volatile int _c_pending = NON_COHERENT;

int pagesize;
int _ide_initialized = 0;


mp_logrec_t statlog[MAXDIAG];
int diag_tally = 0;


int __diag_cmd_syms = 0;
int __debug_cmd_syms = 0;
int __set_cmd_syms = 0;
int __ide_cmd_syms = 0;


#if defined(STKDEBUG)
extern int _dcache_size;
extern char master_stack[];

#if defined NOTYET
+ float ms_tos=0.0;
+ float ms_bos=0.0;
+ float ms_ssize = MASTER_STACKSIZE;
+ float ms_curlen=0.0, ms_sp_biggest;
+ float ms_sp_now;
#endif

int cp_dcache_size;
int cpied = 0;
#endif /* STKDEBUG */

builtin_t* diagcmds_ptr = diagcmds; /* can be used by module to pt to its cmds */
int* ndiagcmds_ptr = &ndiagcmds;

/* #define MSG_TEST */

/*
 * build up symbol table
 */
int
ide_init(int argc, char *argv[])
{
	int promptlen;
	ULONG rc;
	int cnt;
#if EVEREST
	int slaveret = _slaves_to_prom();
	unsigned long intlevel, olevel=0;
#endif
#ifdef STKDEBUG
	char outbuf[MAXMSGLENGTH];
#endif

	ASSERT(!_ide_initialized);  /* buffered printing among other things */
#ifdef DEBUG
	msg_printf(INFO, "    %d symbols in freepool\n", _initsyms());
#else
	(void)_initsyms();
#endif
	_initlog();
	_ide_init_hashtab();		/* zero hashtable pointers */

#if defined(STKDEBUG)
+ #ifdef NOTYET
+ 	ms_bos = (float)((int)(&master_stack[0]));
+ 	ms_tos = (float)(ms_bos + MASTER_STACKSIZE);
+ 	ms_sp_biggest = ms_tos;	/* track biggest stack (most < top of stack) */
+ 	ms_ssize = MASTER_STACKSIZE;
+ 	ms_sp_now = (float)get_sp();
+ 
+ 	ASSERT(ms_sp_now < ms_tos);
+ 
+ 	sprintf(outbuf, "  ide_init: &_dcache_size 0x%x, val 0x%x:\n", 
+ 		(uint)&_dcache_size, _dcache_size);
+ 	msg_printf(JUST_DOIT, "%s", outbuf);
+ 
+ 	sprintf(outbuf, "    TOS %.0f BOS %.0f size %.0f sp now %f\n",
+ 		ms_tos, ms_bos, ms_ssize, ms_sp_now);
+ 	msg_printf(JUST_DOIT, "%s", outbuf);
+ #endif
+ 	cp_dcache_size = _dcache_size;
+ 	cpied = 1;
+ 
#endif /* STKDEBUG */

	msg_printf(VRB, "\n  ide_init:  -builtins-");
	ide_init_builtins(nbuiltins, builtins, B_IDE_COMMANDS);

	msg_printf(VRB, " -diagcmds- ");
	ide_init_builtins(*ndiagcmds_ptr, diagcmds_ptr, B_DIAGNOSTICS);

	/* must init consts only after builtins have been read-in */
	cnt = ide_init_consts();
	msg_printf(VRB, " - %d consts- ", cnt);

	msg_printf(VRB, " -ide prompt- ");
	promptlen = ide_update_prompt();
	msg_printf(VRB, "  strlen of new prompt is %d\n", promptlen);
	ASSERT(promptlen < MAXPROMPTLEN);

	msg_printf(VRB, "\n");

	if (isgraphic(StandardOut)) {
		pagesize = getHtpPs();
		LOCK_ARCS_UI();
		Write(StandardOut,"\033[37h",5,&rc);	/* auto wrap on */
		UNLOCK_ARCS_UI();
	}
	else
		pagesize = 24;

#ifdef MSG_TEST
	if (Reportlevel >= VDBG) {
	    int saveR = Reportlevel;

	    msg_printf(JUST_DOIT,
	    "\n  Demo error message format and level-control facility\n");
	    saveR = Reportlevel;	
	    Reportlevel = ERR;
	    msg_printf(JUST_DOIT,
		"\n    Reportlevel: %d (0x%x)\n", Reportlevel, Reportlevel);
	    msg_printf(ERR,
		"    This is the format of hw diagnostic error messages\n");
	    msg_printf(USER_ERR,
		"    This is user-error (script) message format\n");
	    msg_printf(IDE_ERR,
		"    This is the IDE-internal error message format\n");
	    Reportlevel = saveR;
	}
#endif

#if 0
/* 	c_state(COHERENT);	*/ 
 	_c_state = NON_COHERENT;	/* no LED flashing allowed now */
#endif

#if	EVEREST
    /* Clear the interrupts generated as a result of launching slaves into
     * IP19prom. It takes a while for the slaves to send interrupt to the 
     * master. So clearing them here instead of _slaves_to_prom eliminates
     * the delay we would have to put in _slaves_to_prom
     */
    while (intlevel = EV_GET_LOCAL(EV_HPIL)){
    	EV_SET_LOCAL(EV_CIPL0, intlevel);
    	if (olevel == intlevel) /* Caution against stuck interrupts */
            break;
        else
       	    olevel = intlevel;
    }
#endif
	init_context(argc,argv);
	initmstats();
#ifdef IP32
	initTiles();
#endif

	_ide_initialized = IDE_PDAMAGIC;	/* last thing */

	return(0);

} /* ide_init */


#if EVEREST
static int
_slaves_to_prom()
{
    unsigned cpu;
    int res, cnt=0;

    msg_printf(DBG, "\r  _slaves_to_prom:\n");

    /* Kick all of the slave processors back into the slave loop */
    for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {

	if (cpu == cpuid())
	    continue;

	/* Skip MPCONF fields which are obviously bogus */
	if (MPCONF[cpu].mpconf_magic != MPCONF_MAGIC ||
	    MPCONF[cpu].virt_id != cpu)
	    continue;

	/* Try pushing all processors back into slave loop */
	res = launch_slave(cpu, (void (*)(int)) IP19PROM_RESLAVE, 0, 0, 0, 0);
	if (res) {
		printf("\r    _slaves_to_prom: cpu %d launch res %d\n",
			cpu, res);
	} else {
		cnt++;
		msg_printf(VDBG, "\r    cpu %d launched into prom\n", cpu);
	}
    }
    msg_printf(VRB, "\n  %d slave%s sent into prom idle-loop\n", 
	cnt, _PLURAL(cnt));
    return(cnt);

} /* _slaves_to_prom */
#endif /* EVEREST */

/*
 * ide_init_consts must not be called until after the hash table has been
 * completely initialized at bootup.
 */

static int
ide_init_consts(void)
{
    sym_t *cmdsym, *valsym;
    int count = 0;

    ASSERT(_sympoolsize);

    if (did_htab_init != 1) {
	msg_printf(IDE_ERR,"hash table hasn't been initialized!!\n");
	ide_panic("ide_init_consts");
    }

    valsym = findsym(REPORT_SNAME);
    ASSERT(valsym);

#undef Reportlevel
    Reportlevel = &(valsym->sym_i);	/* now symbol's value is fetched */
#define Reportlevel	(*Reportlevel)

#if IDEBUG
    msg_printf(DBG,"    reportsym: name %s, btype %d, type %d, val %d\n",
	valsym->sym_name,valsym->sym_basetype,valsym->sym_type,valsym->sym_i);
#endif
    msg_printf(DBG, "    bootup %s:  %d\n", valsym->sym_name, Reportlevel);
    count++;

    valsym = findsym(STOUT_SNAME);
    ASSERT(valsym);

    /* symbols value fetched */
    _Slave_Timeout = (ide_int_type *)&(valsym->sym_i);

    msg_printf(DBG, "    bootup %s:  %d\n", valsym->sym_name, Slave_Timeout);
    count++;

    valsym = findsym(ERRLOGVAL_SNAME);
    cmdsym = findsym(ERRLOGCMD_SNAME);
    ASSERT(valsym);
    ASSERT(cmdsym);
    xenv_syms[ERRLOGGING].val_symp = valsym;
    xenv_syms[ERRLOGGING].cmd_symp = cmdsym;
    msg_printf(VRB, ", %s  (command: `%s'):  %s\n", valsym->sym_name,
    		cmdsym->sym_name, (valsym->sym_i==ENABLED ? "ON" : "OFF"));
    count++;

    valsym = findsym(ICEXECVAL_SNAME);
    cmdsym = findsym(ICEXECCMD_SNAME);
    ASSERT(valsym);
    ASSERT(cmdsym);
    xenv_syms[ICACHED_EXEC].val_symp = valsym;
    xenv_syms[ICACHED_EXEC].cmd_symp = cmdsym;
    msg_printf(VRB, ", %s  (command: `%s') --  %s\n", valsym->sym_name,
		cmdsym->sym_name, (valsym->sym_i==ENABLED ? "ON" : "OFF"));
    count++;

    valsym = findsym(QMODEVAL_SNAME);
    cmdsym = findsym(QMODECMD_SNAME);
    ASSERT(valsym);
    ASSERT(cmdsym);
    xenv_syms[QUICKMODE].val_symp = valsym;
    xenv_syms[QUICKMODE].cmd_symp = cmdsym;
    msg_printf(VRB, ", %s  (command: `%s'):  %s\n", valsym->sym_name,
		cmdsym->sym_name, (valsym->sym_i==ENABLED ? "ON" : "OFF"));
    count++;

    valsym = findsym(C_ON_ERRVAL_SNAME);
    cmdsym = findsym(C_ON_ERRCMD_SNAME);
    ASSERT(valsym);
    ASSERT(cmdsym);
    xenv_syms[C_ON_ERROR].val_symp = valsym;
    xenv_syms[C_ON_ERROR].cmd_symp = cmdsym;
    msg_printf(VRB, ", %s  (command `%s'):  %s\n", valsym->sym_name,
		cmdsym->sym_name, (valsym->sym_i==ENABLED ? "ON" : "OFF"));
    count++;

    return(count);

} /* ide_init_consts */



/*
 * each diagnostic has <passcnt,failcnt> tuple per cpu.  
 * init_builtins assigns a sequence number to each SYM_DIAG
 * symbol as it initializes the hash table; that number is
 * the diag's error log.
 * _doclearlog() and _dodumplog()--defined in ide_cmds.c--allow
 * scripts to reinitialize the log's results (clearlog)
 * and display the log's contents in various forms (dumplog).
 */

/*
 * _initlog zeroes out the entire log.  This routine must
 * not be exported to the shell; _clear_errlog is provided to
 * nondestructively reset the pass/fail counts upon command.
 */
/*ARGSUSED*/
static void
_initlog(void)
{
    int diag;

    msg_printf(VRB,
	"    _initlog: clear 0x%x (%d) bytes; start addr 0x%x\n",
	sizeof(statlog), sizeof(statlog), statlog);

    bzero(statlog, sizeof(statlog));
    diag_tally = 0;

    for (diag = 0; diag < MAXDIAG; diag++) {
	statlog[diag].diagsym = ((sym_t *)(long)-1);
    }

    return;

} /* _initlog */


void
ide_init_builtins(int n, builtin_t *b, int tab_type)
{
    char *systype = inv_findcpu();
    sym_t *psym;
    int i;

    ASSERT(_sympoolsize);

    if (did_htab_init != 1) {
	msg_printf(IDE_ERR,"hash table hasn't been initialized!!\n");
	ide_panic("ide_init_builtins");
    }

    ide_check_tblsizes();

    for ( i = 0; i < n; i++, b++ ) {
	psym = makesym();

	if (tab_type == B_DIAGNOSTICS) { 
	  if (b->addr == NULL) {
	    printf("Command %s was not found\n",b->name);
	    ide_panic("ide_init_builtins");
	  }	  
	}

	/*
	 * ide builtins, diags, and ide-internal debugging routines may
	 * all use either the sym_t* or the char* argv[] argument format.
	 * Routines that use sym_t* method are SCMDs, those using the 
	 * char* method are CMDs.  All of the set operations use the char*
	 * format, so we don't need SET_SCMDS.  This code is pretty ugly:
	 * an attempt to obtain enough info to provide a more precise,
	 * intuitively-organized help facility and allow IDE to introduce
	 * diagnostics and display the results, while suppressing all that
	 * for all non-diagnostic script commands (like "print").  To do
	 * this, IDE assumes that no diagnostics are declared in 'builtins':
	 * they live in each architecture's 'cpu_diagcmds'.  However,
	 * cpu-specific (but non-diagnostic) commands cannot go into
	 * 'builtins'; the CPUSPEC hack allows these commands to share
	 * the 'cpu_diagcmds' files with the diags.
	 */
	switch(b->type) {

	case B_SCMD:
	    if (tab_type == B_DIAGNOSTICS)
		psym->sym_flags |= SYM_DIAG;
	    /* fall through */
	case B_CPUSPEC_SCMD:
	case B_DEBUG_SCMD:
	    psym->sym_type = SYM_CMD;
	    psym->sym_func = (ide_int_type (*)())b->addr;
	    if (b->type == B_DEBUG_SCMD)	/* ide-internal debugging fn */
		psym->sym_flags |= SYM_DEBUG_CMD;
	    break;

	/*
	 * All diags which invoke special execution features,
	 * and all ide SET primitives use the char *argv[] format
	 */
	case B_CMD:
	    if (tab_type == B_DIAGNOSTICS) {
		psym->sym_flags |= SYM_DIAG;
		__diag_cmd_syms++;
	    }
	case B_CPUSPEC_CMD:
	case B_DEBUG_CMD:
	case B_SET_CMD:
	    psym->sym_type = SYM_CMD;
	    psym->sym_func = (ide_int_type (*)())b->addr;
	    psym->sym_flags |= SYM_CHARARGV;

	    if (b->type == B_DEBUG_CMD) {
		psym->sym_flags |= SYM_DEBUG_CMD;
		__debug_cmd_syms++;
	    } else if (b->type == B_SET_CMD) {
		psym->sym_flags |= SYM_SET_CMD;
		__set_cmd_syms++;
	    }
	    break;

	case B_STR:
	    psym->sym_type = SYM_VAR;
	    psym->sym_basetype = SYMB_STR;
	    psym->sym_s    = makestr((char *)b->addr);
	    break;

	case B_SETSTR:
	    psym->sym_type = SYM_VAR;
	    psym->sym_basetype = SYMB_SETSTR;
	    psym->sym_s    = makestr((char *)b->addr);
	    break;

	case B_INT:
	    psym->sym_type = SYM_VAR;
	    psym->sym_basetype = SYMB_INT;
	    psym->sym_i    = (ide_int_type) b->addr;
	    break;

	default:
	    msg_printf(IDE_ERR,"ide_init_builtins: unknown type %d\n",b->type);
	    return;
	}

	psym->sym_name = b->name;

	/* if there is an inviso period as the first character, skip it */
	if (psym->sym_name[0] == '.')
		psym->sym_name++;

	if (psym->sym_flags & SYM_DIAG) {
	    if (diag_tally >= MAXDIAG) {
		msg_printf(IDE_ERR,
		    "ide_init_builtins: ?!?bad diag_tally (%d)\n",diag_tally);
		ide_panic("ide_init_builtins");
	    }
	    psym->sym_logidx = diag_tally;
	    statlog[diag_tally].diagsym = psym;
	    diag_tally++;
	}
	addsym(psym);
    }

    if (tab_type == B_DIAGNOSTICS) {
	if (systype)
	    msg_printf(DBG,"\n    %d %s Diagnostics\n\n", diag_tally, systype);
	else
	    msg_printf(DBG,"\n    %d diagnostics available\n", diag_tally);
    }

    return;

} /* ide_init_builtins */


#if EVEREST
int
ide_update_prompt()
{
    sym_t *symptr;
    char *xptr1;
    int i;
    uint my_vid = (uint)cpuid();
    int slot, slice;
    char outbuf[MAXMSGLENGTH];
    char idstr[MAXPROMPTLEN], *cp=&idstr[0];
    char newprompt[MAXPROMPTLEN];
    char *npptr = newprompt, *opptr;

    slot = vid_to_slot[my_vid];
    slice = vid_to_slice[my_vid];
    msg_printf(VDBG, "vid %d: slot %d slice %d\n", my_vid,slot,slice);

    cp += sprintf(cp, "%d:%s%d/0%d", my_vid, (slot < 10 ? "0" : ""),
	slot, slice);

    ASSERT(cp < &idstr[MAXPROMPTLEN]);

#if defined(LAUNCH_DEBUG)
    msg_printf(VDBG, "  - ide_update_prompt: vid %d, idstr `%s' (len %d)\n",
	my_vid, idstr, strlen(idstr));
#endif /* LAUNCH_DEBUG */

    symptr = findsym("PS1");
    if (!symptr) {
	msg_printf(IDE_ERR, "ide_update_prompt: findsym of PS1 failed\n");
	ide_panic("ide_update_prompt");
    }

    opptr = symptr->sym_s;
    if (!(xptr1 = _strchr(opptr, '*'))) {
	msg_printf(IDE_ERR,"!!!ide_update_prompt: _strchr `*' char failed\n");
	return(2);
    }

    while (opptr != xptr1)
	*npptr++ = *opptr++;

    for (i = 0; i < strlen(idstr); i++)
	*npptr++ = idstr[i];

    opptr++;	/* don't copy the place-holding '*' */
    while (*opptr)
	*npptr++ = *opptr++;
    
    *npptr = '\0';

    opptr = symptr->sym_s;	/* we'll free this string */
    symptr->sym_s = makestr((char *)newprompt);
    destroystr(opptr);
    
#ifdef IF_TROUBLE
    if (Reportlevel >= VDBG) {
	msg_printf(DBG+1,"ide_update_prompt, new PS1: \"%s\"\n", symptr->sym_s);
    }
#endif /* IF_TROUBLE */

    return(strlen(symptr->sym_s));
    
} /* ide_update_prompt */

#else /* non-EVEREST architectures */

int
ide_update_prompt(void)
{
    char *SP_ide_prompt = "ide>> ";
    sym_t *symptr;
    char *opptr;

    symptr = findsym("PS1");
    if (!symptr) {
	msg_printf(IDE_ERR, "ide_update_prompt: findsym of PS1 failed!!\n");
	ide_panic("ide_update_prompt");
    }

    opptr = symptr->sym_s;	/* free this string */
    symptr->sym_s = makestr((char *)SP_ide_prompt);
    destroystr(opptr);
    
    msg_printf(DBG, "ide_update_prompt, new PS1: \"%s\"\n", symptr->sym_s);

    return(0);

} /* ide_update_prompt */
#endif /* EVEREST */


static void
ide_check_tblsizes(void)
{
    int _err=0;

    if (TBT_TOTAL != sym_tstr_tabsiz) {
	msg_printf(IDE_ERR, 
		"sym_tstrs (sym type+btype) table size mismatch!\n");
	msg_printf(JUST_DOIT, 
		"    %d slots in sym_tstrs, %d total sym types+btypes!\n",
		TBT_TOTAL, sym_tstr_tabsiz);
	_err++;
    }
    if (SYM_FLAG_CNT != sflag_tblsiz) {
	msg_printf(IDE_ERR, "sflags (sym flags) table size-mismatch!\n");
	_err++;
    }
    if (_err)
	ide_panic("ide_check_tblsizes");

    return;

} /* ide_check_tblsizes */


/* #define	LINESIZE	80  use bigger one defined elsewhere */

/* WARNING: this routine may not handle \t characters correctly */
int
pager(char *buffer, int pagesize, int *curline)
{
	char	linebuf[LINESIZE];
	char	*ptr1 = buffer;
	char	*ptr2;
	char	saved_ch;

	while (1) {
		if (*ptr1 == 0)			/* nothing to display */
			break;

		if (*curline == pagesize - 2) {		/* screen full */
			msg_printf(JUST_DOIT,
				"\n    - Press <ENTER> to continue -  ");
			*curline = 0;
			gets(linebuf);
		}

		ptr2 = index(ptr1, '\n');	/* find newline in text */
		if (ptr2 == (char *)0) {	/* no embedded \n */
			if (strlen(ptr1) < LINESIZE) {	/* length OK */
				(*curline)++;
				msg_printf(JUST_DOIT, "%s\n", ptr1);
				break;
			}
			else {		/* chop up text if necessary */
				saved_ch = ptr1[LINESIZE - 1];
				ptr1[LINESIZE - 1] = '\0';
				(*curline)++;
				msg_printf(JUST_DOIT, "%s\n", ptr1);
				ptr1[LINESIZE - 1] = saved_ch;
				ptr1 += LINESIZE - 1;
				continue;
			}
		}
		else {			/* handle embedded \n character */
			if (ptr2 - ptr1 < LINESIZE) {
				(*curline)++;
				*ptr2 = '\0';
				msg_printf(JUST_DOIT, "%s\n", ptr1);
				*ptr2 = '\n';
				ptr1 = ptr2 + 1;
				continue;
			}
			else {		/* chop up text if necessary */
				saved_ch = ptr1[LINESIZE - 1];
				ptr1[LINESIZE - 1] = '\0';
				(*curline)++;
				msg_printf(JUST_DOIT, "%s\n", ptr1);
				ptr1[LINESIZE - 1] = saved_ch;
				ptr1 += LINESIZE - 1;
				continue;
			}
		}

	}

	return 0;
} /* pager */


#if EVEREST
void
ide_panic(char *s)
{
    char cbuf[2*MAXLINELEN];
    register uint my_vid = (uint)cpuid();
    register evreg_t slotproc;
    register uint slot, slice, phys_idx;
    char *who;

    slotproc = EV_GET_LOCAL(EV_SPNUM);
    slot = (slotproc & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    slice = (slotproc & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
    phys_idx = SS_TO_PHYS(slot, slice);

    if (IDE_ME(ide_mode)!=IDE_MASTER || _ide_info.master_vid!=my_vid)
	who = "slave";
    else
	who = "master";

    sprintf(cbuf, "IDE %s(%d:%s%d/0%d): %s", who, my_vid,
	(slot < 10 ? "0" : ""), slot, slice,
	(_SAFESTR(s) ? s : "<BAD PANIC STRING>"));
    /*
     * don't want to exit, because if we are on the graphics tube, the
     * screen clears so fast that the message can't be read! (panic in
     * libsa.a now loops for this reason.)
     */
    panic(cbuf);

}

#else	/* SP and non-Everest hardware */

void
ide_panic(char *s)
{
	char cbuf[2*MAXLINELEN];

	/*
	 * don't want to exit, because if we are on the graphics tube, the
	 * screen clears so fast that the message can't be read! (panic in
	 * libsa.a now loops for this reason.)
	 */
	sprintf(cbuf, "IDE: %s", (_SAFESTR(s) ? s : "<BAD PANIC STRING>"));
	panic(cbuf); 
	/*NOTREACHED*/
}
#endif /* EVEREST */

#ifdef DEBUG
static char assbuf[240];
/* _assert grabbed from libc/port/gen/assert.c */

void
_assert(const char *assertion, const char *filename, int line_num)
{
	static char linestr[] = ", line NNNNN\n";
	register char *p = &linestr[7];
	register int div, digit;

	for (div = 10000; div != 0; line_num %= div, div /= 10)
		if ((digit = line_num/div) != 0 || p != &linestr[7] || div == 1)
			*p++ = digit + '0';
	*p++ = '\n';
	*p = '\0';

	sprintf(assbuf,"assertion failed (%s), file %s%s",
	    assertion, filename, linestr);

	ide_panic(assbuf);

} /* _assert */
#endif


/* arguments to options may be separated by commas, commas followed by
 * a space, or just a space if the entire argument list is quoted */
#define ARGSEPS		", "

/* accepting only the options allowed by 'optstr', parse the argv
 * strings, filling the optlist struct from the calling routine.
 * For opts requiring arguments, set successive argument pointers
 * to the head of each string-argument; also change the comma or
 * <sp> argument delimiter (depending upon input-parameter format
 * used by invoking routine) into a NULL byte to terminate each arg.
 */

int
_get_options(int argc, char **argv, char *optstr, optlist_t *optlistp)
{
    int c, res;
    option_t *optionp;

    /* error-check incoming parameters */
    if (!optlistp) {
	msg_printf(IDE_ERR, "?? _get_options (via %s): NULL optlist ptr\n", 
	    argv[0]);
	return(NOOPTLIST);
    }

    if (argc <= 0) {
	msg_printf(IDE_ERR, "?? _get_options (via %s): invalid argc (%d)\n", 
		argv[0], argc);
	return(BADARGC);
    }

    optlistp->maxopts = (int)OPTCCNT(optstr);
    ASSERT(optlistp->maxopts > 0 && optlistp->maxopts <= MAX_OPTS);

    optlistp->optcount = 0;
    optlistp->opts[0] = optlistp->opts[MAX_OPTARGS] = '\0';

#ifdef OPTION_DEBUG
    if (Reportlevel >= OPTDBG) {
	msg_printf(DBG, "    _get_options:  optstr \"%s\"  maxopts %d\n\t",
	    optstr, optlistp->maxopts);
    	_dump_argvs(argc, argv);
	msg_printf(DBG, "\n");
    }
#endif

    _init_getopt();
    while ((c = ide_getopt(argc, argv, optstr)) != -1) {

#ifdef OPTION_DEBUG
   	if (Reportlevel >= OPTDBG) {
	    msg_printf(DBG, "    opt == '%c' (0x%x); optcount %d\n",
		(char)c,c,optlistp->optcount);
	}
#endif
	switch(c) {

	case DD_RETURN:	/* ide_getopt saw an explicit "--" */
#ifdef OPTION_DEBUG
	    if (Reportlevel >= OPTVDBG)
		msg_printf(DBG, "    _get_options: got DBLDASH!\n");
#endif
	    return(c);
	case '?':
	    /* msg_printf(USER_ERR,"%s: bad opt '%c' (0x%x)\n",argv[0],c,c); */
	    return(c);

        default:
	    /*
	     * all valid options come here.  ide_getopt() sets/clears
	     * 'optarg' to indicate whether the fetched opt requires
	     * an argument.  Also check now for too many opts.
	     */
#ifdef OPTION_DEBUG
    	    if (Reportlevel >= OPTVDBG) {
	        msg_printf(JUST_DOIT, "    _get_options: opt %c,  %s", (char)c,
		    (optarg ? "requires args\n" : "no args required\n"));
	    }
#endif
	    if (optlistp->optcount >= optlistp->maxopts) {
		msg_printf(USER_ERR, "%s: too many options (> %d)\n", argv[0],
		    optlistp->optcount);
		return(BADOPTCOUNT);
	    }

	    optlistp->opts[optlistp->optcount] = (char)c;
	    optlistp->opts[optlistp->optcount+1] = '\0';
	    optionp = (option_t *)&(optlistp->options[optlistp->optcount]);
	    optionp->opt = c;
	    optionp->argcount = 0;
	    optionp->args[0] = NULL;

	    /* if no args with this option, we're done with it */
	    if (!optarg) {
		optlistp->optcount++;
		break;
	    }

	    /* else grab arg or args */
	    if ((res = _get_arglist(optarg, optionp)) == 0) {
		optlistp->optcount++;
		break;
	    } else {
		optlistp->opts[optlistp->optcount] = '\0';
		msg_printf(DBG, "_get_options: _get_arglist returned %d\n",res);
		return(res);
	    }

	} /* switch */
    } /* while */

    optlistp->opts[optlistp->optcount] = '\0';

    if (optlistp->optcount > optlistp->maxopts) {
	msg_printf(USER_ERR,
		"%s: too many opts (%d)\n", argv[0], optlistp->optcount);
	return(BADOPTCOUNT);
    }

#ifdef OPTION_DEBUG
    if (Reportlevel & OPTDBG) {
	msg_printf(DBG,
	    "    _get_options: parsed %d opt%s (%s)\n", optlistp->optcount,
	    (optlistp->optcount == 1 ? "" : "s"), optlistp->opts);
	if (Reportlevel & OPTVDBG)
	    _dump_optlist(optlistp);
    }
#endif

    return(0);

} /* _get_options */

int
_opt_index(char c, char *str)
{
    char *cp;

    if (!(cp = _strchr(str, c)))
	return(-1);
    else
	return((int)(cp - str));

} /* _opt_index */


/*
 * _get_arglist: 
 * 'argstr' points to head of an argument (argv) string.
 * Accept arguments in <arg1,arg2,...>, <arg1, arg2,..>,
 * or "arg1 arg2" format and place them in the option_t
 * struct pointed to by 'optionp'.
 */
int
_get_arglist(char *argstr, option_t *optionp)
{
    char *cptr;

    if (!optionp) {
	msg_printf(IDE_ERR, "   _get_arglist: NULL optionp ptr\n");
	return(NOOPTARG);
    }

    if ((Reportlevel & OPTVDBG) || Reportlevel >= VDBG) {
	if (optionp->opt) 
	    msg_printf(JUST_DOIT,
		"  _get_arglist: argstr \"%s\", opt '%c', argc %d\n",
		argstr, (char)optionp->opt, optionp->argcount);
	else
	    msg_printf(JUST_DOIT, "    _get_arglist:  argstr \"%s\", argc %d\n",
		argstr, optionp->argcount);
    }

    optionp->argcount = 0;	/* don't rely on calling routine */

    cptr = _strtok(argstr, ARGSEPS);	/* init _strtok and set up 0'th arg  */

    /* cptr returns NULL when user invokes fn with no parameters (argc==1) */
    if (cptr == NULL) {
	msg_printf(DBG+1,
		"_get_arglist: _strtok returned NULL on 0'th call\n");
	return(OPTARGERR);
    }
    optionp->args[optionp->argcount] = cptr;
    optionp->argcount++;

    while (optionp->argcount < MAX_OPTARGS) {
	cptr = _strtok(NULL, ARGSEPS);
	if (!cptr) {	/* end of arg list */
	    break;
	}
	optionp->args[optionp->argcount] = cptr;
	optionp->argcount++;
    }

    if (optionp->argcount >= MAX_OPTARGS) {
	if (optionp->opt)
	    msg_printf(USER_ERR,"_get_arglist: '-%c' opt--too many args (%d)\n",
		optionp->opt, optionp->argcount);
	else
	    msg_printf(USER_ERR,"_get_arglist:  too many args (%d)\n",
		optionp->argcount);
	return(BADARGCOUNT);
    } else {
	if (Reportlevel >= OPTVDBG) {
	    int i;

	    if (optionp->opt)
		msg_printf(VDBG, "    _get_arglist: %d arg%s to '%c' opt: ",
		    optionp->argcount,(optionp->argcount == 1 ? "" : "s"),
		    (char)optionp->opt);
	    else
		msg_printf(VDBG, "    _get_arglist: %d arg%s parsed\n",
		    optionp->argcount,(optionp->argcount == 1 ? "" : "s"));

	    for (i = 0; i < optionp->argcount; i++)
		msg_printf(VDBG, " \"%s\"", optionp->args[i]);
	    msg_printf(VDBG, "\n");
	}
	return(0);
    }

} /* _get_arglist */


void
_dump_getopt_vars(int retval)
{
    if (retval != _SKIP_IT) {
	if (retval > 0)
	    msg_printf(JUST_DOIT, "retval %d (0x%x or `%c'): ", retval, 
		retval, (char)retval);
	else
	    msg_printf(JUST_DOIT, "retval %d: ", retval);
    }
    msg_printf(JUST_DOIT, "_sp %d, ind %d, opt %d, err %d, arg %s\n",
	_sp, optind, optopt, opterr, (optarg ? optarg : "<NULL>"));

} /* dump_getopt_vars */

void
_dump_argvs(int argc, char **argv)
{
    char outbuf[MAXLINELEN];
    char *bptr = &outbuf[0];
    int i;

    outbuf[0] = '\0';
    bptr += sprintf(bptr, " argc %d:  ",argc);

    if (Reportlevel >= VDBG)
	bptr += sprintf(bptr, "(argv 0x%x):  ",argv);

    for (i = 0; i < argc; i++) {
	bptr += sprintf(bptr, " `%s'", _SAFESTR(argv[i]));
    }
    
    if (argv[argc] != NULL) {
	char *cp = argv[argc];

	bptr += sprintf(bptr, "!! argc %d but argv[%d] !NULL", argc, argc);

	for (i = 0; (i < 10 && *cp); i++) { 
	    putchar(*cp++);
	}
	putchar('\n');

    }
    msg_printf(JUST_DOIT, "%s\n",outbuf);

} /* _dump_argvs */

int
_dump_optlist(optlist_t *optlistp)
{
    option_t *optionp;
    int option, arg;

    if (Reportlevel & NAUSEOUS)
	msg_printf(JUST_DOIT, "  %d option%s:  ",
	    optlistp->optcount, _PLURAL(optlistp->optcount));

    for (option = 0; option < optlistp->optcount; option++) {
	optionp = (option_t *)&(optlistp->options[option]);
	msg_printf(JUST_DOIT, "        option `%c', %s ", (char)optionp->opt,
		(optionp->argcount ? "args: " : "no args"));
	for (arg = 0; arg < optionp->argcount; arg++)
	    msg_printf(JUST_DOIT, " \"%s\"", optionp->args[arg]);
	msg_printf(JUST_DOIT, "\n");
    }

    return 0;
} /* _dump_optlist */


/* 
 * ide_getopt() is lifted from libc's getopt(), with some modification (it
 * didn't exist in libs{c,k}). ide_getopt() returns DD_RETURN when a "--"
 * argument is encountered instead of EOF, which it returns when it finds
 * an *implicit* end of args. ("--" signals the end of prepended special
 * execution flags, which are stripped and interpretted by ide_dispatch).
 */
static int
ide_getopt(int argc, char *const *argv, const char *opts)
{
    char c;
    char *cp;

    if(_sp == 1) {
	if(optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
	    return(EOF);
	else if(strcmp(argv[optind], "--") == NULL) {
	    optind++;
	    msg_printf((OPTVDBG|DBG), "\texit ide_getopt, case DBLDASH\n");
	    return(DD_RETURN);
	}
    }
    optopt = c = (unsigned char)argv[optind][_sp];
    if(c == ':' || (cp=_strchr(opts, c)) == NULL) {
	if(opterr)
	    msg_printf(USER_ERR,
		"ide_getopt, %s:1:Illegal option -- %c\n", argv[0], c);
	if(argv[optind][++_sp] == '\0') {
	    optind++;
	    _sp = 1;
	}
	return('?');
    }
    if(*++cp == ':') {
	if(argv[optind][_sp+1] != '\0')
	    optarg = &argv[optind++][_sp+1];
	else if (++optind >= argc) {
	    if (opterr)		/* suppress msg if opterr was set to 0 */
		msg_printf(USER_ERR,
		    "%s: uxlibc:2:Option `%c' requires an argument\n",
			argv[0], c);
	    _sp = 1;
	    return('?');
	} else
		optarg = argv[optind++];
	_sp = 1;
    } else {
	if(argv[optind][++_sp] == '\0') {
	    _sp = 1;
	    optind++;
	}
	optarg = NULL;
    }
    if (Reportlevel >= OPTVDBG) {
	msg_printf(VDBG, "\texiting ide_getopt normally:  c = `%c'\n", c);
    }

    return(c);
} /* ide_getopt */


/*
 * libc getopt is designed to be called once when a program begins to
 * parse its arguments, so the necessary variables are statically 
 * initialized.  This won't work for ide, which repeatedly calls
 * functions whose parameters are in the <argc,argv> format.
 */
static void
_init_getopt(void)
{
  _sp = opterr = optind = 1;
  optopt = 0;
  optarg = 0;
} /* _init_getopt */



builtin_t *
_get_tbl_entry(char *entryname)
{
    int i;

    for ( i = 0; i < nbuiltins; i++ )
	if (!strcmp(entryname, builtins[i].name))
	    return(&builtins[i]);

    return(NULL);

} /* _get_tbl_entry */

int
get_mvid(void)
{
    return(_ide_info.master_vid);
}


int
_ide_usage(char *fnname)
{
    builtin_t *bptr;

    ASSERT(fnname != NULL);

    bptr = _get_tbl_entry(fnname);
    if (bptr) {
	if (bptr->help[0] == NULL) {
	    msg_printf(JUST_DOIT, "%s: argument or option usage error\n",
		fnname);
	    return(IENOHELPSTRING);
	}
	msg_printf(JUST_DOIT, "usage: %s\n",bptr->help);
	return(0);
    } else {
#ifdef IDEBUG
	msg_printf(IDE_ERR, "_ide_usage: symbol for function `%s' not found!\n",
		fnname);
#endif
	return(IESYM_NOT_FOUND);
    }

} /* _ide_usage */


/* ++++++++++++++++++++++++++ STRING PRIMITIVES +++++++++++++++++++++++++++++ */

/*
 * uses _strpbrk and _strspn to break string into tokens on
 * sequentially subsequent calls.  returns NULL when no
 * non-separator characters remain.
 * `subsequent' calls are calls with first argument NULL.
 */

char *
_strtok(register char *string, const char *sepset)
{
	register char	*q, *r;
	static char	*savept;

	/*first or subsequent call*/
	if (string == NULL)
		string = savept;

	if(string == 0)	{	/* return if no tokens remaining */
		msg_printf(DBG+4, "_strtok: NULL string arg\n");
		return(NULL);
	}

	q = string + _strspn(string, sepset);	/* skip leading separators */

	if(*q == '\0')		/* return if no tokens remaining */
		return(NULL);

	if((r = _strpbrk(q, sepset)) == NULL)	/* move past token */
		savept = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		savept = r+1;
	}
	return(q);

} /* _strtok */

char *
_strtok_r(char *string, const char *sepset, char **lasts)
{
	register char	*q, *r;

	/*first or subsequent call*/
	if (string == NULL)
		string = *lasts;

	if(string == 0)		/* return if no tokens remaining */
		return(NULL);

	q = string + _strspn(string, sepset);	/* skip leading separators */

	if(*q == '\0')		/* return if no tokens remaining */
		return(NULL);

	if((r = _strpbrk(q, sepset)) == NULL)	/* move past token */
		*lasts = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		*lasts = r+1;
	}

	return(q);

} /* _strtok_r */


/*
 * Return ptr to first occurence of any character from `brkset'
 * in the character string `string'; NULL if none exists.
 */

char *
_strpbrk(register const char *string, const char *brkset)
{
	register const char *p;

	do {
		for(p=brkset; *p != '\0' && *p != *string; ++p)
			;
		if(*p != '\0')
			return((char *)string);
	}
	while(*string++);

	return(NULL);

} /* _strpbrk */


/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters from charset.
 */
size_t
_strspn(const char *string, const char *charset)
{
	register const char *p, *q;

	for(q=string; *q != '\0'; ++q) {
		for(p=charset; *p != '\0' && *p != *q; ++p)
			;
		if(*p == '\0')
			break;
	}

	return(q-string);

} /* _strspn */


/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */
char *
_strchr(const char *sp, int c)
{
    char ch = (char)c;
    do {
	if(*sp == ch)
	    return((char *)sp);
    } while(*sp++);

    return(NULL);

} /* _strchr */


/*
 * _charcnt: return the number of occurences of character 'c' in 'string'.
 */
int
_charcnt(const char *string, char c)
{
    register int cnt = 0;
    register const char *p;

    for(p=string; *p != '\0'; ++p) {
	if (*p == c)
	    cnt++;
    }
    return(cnt);

} /* _charcnt */


/*
 * ide_initargv - copy incoming argv pointers and the strings to which
 * they point someplace safe (into our pda).  This routine was lifted 
 * from libsc, then modified to accept the destination of the copy
 * as a third parameter.
 */
void
ide_initargv(int *ac, char ***av, struct string_list *argv_strp)
{
    msg_printf(VDBG,
	    "    ide_initargv:  ac 0x%x, av 0x%x, argv_strp 0x%x\n",
	    ac, av, argv_strp);

    init_str(argv_strp);
    while ((*ac)--) {
	if (**av)
	    new_str1(**av, argv_strp);
	++(*av);
    }
    *ac = argv_strp->strcnt;
    *av = argv_strp->strptrs;
}

int
_insert_arg(argdesc_t *argcvp, char *newarg, int argindex)
{
    int argc, i;
    char **argv;

    if (!argcvp || !newarg || (argindex < 0)) {
	msg_printf(IDE_ERR,
	    "_insert_arg, bad parms: cvp 0x%x, newarg 0x%x, idx %d\n",
	    argcvp, newarg, argindex);
	ide_panic("_insert_arg");
    }

    argc = argcvp->argc;
    argv  = argcvp->argv;

    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"    _insert_arg: newarg '%s' argidx %d;  in-args: ",
	    newarg, argindex);
	_dump_argvs(argc, argv);
    }

    if (argindex > MAXARGC-1 || argc > MAXARGC-1) {
	msg_printf(IDE_ERR, "\n!!! argindex %d or argc %d > %d!!\n",
		argindex,argc,MAXARGC-1);
	ide_panic("_insert_arg");
    }
    if (argv[argc]) {
	msg_printf(IDE_ERR, "\n!!! argv[%d] not NULL! ('%s')\n",
		argc,argv[argc]);
	ide_panic("_insert_arg");
    }
    if (argindex > argc) {
	msg_printf(IDE_ERR, "\n!!! argindex (%d) > argc (%d)\n",
		argindex, argc);
	ide_panic("_insert_arg");
    }

    for (i = argc; i > argindex; i--) {
	argv[i] = argv[i-1];
    }
    argv[argindex] = newarg;
    argcvp->argc++;

    ASSERT(argcvp->argc < MAXARGC);
    argv[argcvp->argc] = NULL;

    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"\n    _insert_arg, modified args: ");
	_dump_argvs(argcvp->argc, argcvp->argv);
	msg_printf(VDBG,"\n");
    }

    return(0);

} /* _insert_arg */

/*
 * 'optionp' points to an 'option_t' struct containing strings.
 * _check_fns verifies that there's a SYM_CMD symbol for each string
 * in the option_t args list, storing the sym_t ptrs in the caller's
 * fnlist in the process.  fnlistp->fncnt returns the number of 
 * verified functions.  Stringnames which fail are not included in the
 * tally or fncnt, but the entire list is checked regardless.
 * _check_fns returns 0 if all incoming fnnames were ok, -1 otherwise.
 * 'silent' suppresses all error message display other than those 
 * reporting ide-internal problems.
 */
int
_check_fns(option_t *optionp, fnlist_t *fnlistp)
{
    int i, _error=0, rc=0;
    sym_t *tsym;
    char *fname;

    if (!fnlistp) {
	msg_printf(IDE_ERR," _check_fns: NULL fnlist pointer\n");
	ide_panic("_check_fns");
    }
    if (!optionp || optionp->argcount <= 0 || optionp->argcount > MAX_OPTARGS) {
	msg_printf(IDE_ERR, "_check_fns: bad option list\n");
	ide_panic("_check_fns");
    }

    fnlistp->fncnt = 0;

    for (i = 0; i < optionp->argcount; i++) {
	fname = optionp->args[i];
	tsym = findsym(fname);

	if (!tsym) {
	    msg_printf(USER_ERR, "symbol for function `%s' not found\n", fname);
	    rc = OPTARGERR;
	    _error++;
	    continue;
	}
	if (tsym->sym_type != SYM_CMD) {
	    msg_printf(USER_ERR,
		"symbol '%s' is type %d, not SYM_CMD (%d)\n", fname,
		tsym->sym_type, SYM_CMD);
	    rc = ERRNOEXEC;
	    _error++;
	    continue;
	}
	if (tsym->sym_flags & SYM_UDEF) {
	    msg_printf(USER_ERR,
		"'%s': exec of user-defined functions is illegal\n", fname);
	    rc = ERRNOEXEC;
	    _error++;
	    continue;
	}

	/* it's a legit C routine */
	fnlistp->fn_symptrs[i] = tsym;
	fnlistp->fncnt++;
    }

    if (Reportlevel >= VDBG) {
	if (_error) {
	    msg_printf(VDBG,"    (%d bad fnname%s:  verified %d fn%s)", 
		_error, _PLURAL(_error), 
		fnlistp->fncnt, _PLURAL(fnlistp->fncnt));
	} else {
	    msg_printf(VDBG,"    verified %d fn%s", fnlistp->fncnt,
		_PLURAL(fnlistp->fncnt));
	}
	if (Reportlevel > VDBG) {
	    msg_printf(JUST_DOIT,":\n      ");
	    for (i = 0; i < fnlistp->fncnt; i++)
		msg_printf(JUST_DOIT,"%s ",fnlistp->fn_symptrs[i]->sym_name);
	} else {
	    msg_printf(JUST_DOIT,"\n");
	}
    }

    return(rc);

} /* _check_fns */


/* ++++++++++++++++++++++++ END STRING PRIMITIVES +++++++++++++++++++++++++++ */


/* 
 * ide_dispatch calls _get_disopts when the arg list of a function begins
 * with "--", indicating that the user wants special execution help.
 * _get_disopts validates and strips all dispatcher flags, returning 
 * the function's argc in argc_ptr, the modified arg list in argv, and
 * the list of dispatcher options it processed in optlistp.  If the user
 * doesn't end the spx flags with another "--", return SPXERR_NOARGEND.
 * If _get_disopts succeeds, return 0.
 */
 
int
_get_disopts(argdesc_t *arginfop, optlist_t *optlistp)
{
    int argc = arginfop->argc;
    char **argv = arginfop->argv;
    int res;

    /* fetch the spx options via get_options() call.  Adjust argptrs 
     * to make argv[0] show dispatcher as calling routine.
     */
    ASSERT((argc > 1) && argv[1] && !strcmp(argv[1], "--"));

    /* spx opts begin after 1st "--", so change that slot to point to
     * dispatcher name and make it &argv[0]. */
    argv++;
    argv[0] = IDE_PMGR;
    argc--;

#ifdef LAUNCH_DEBUG
    if (Reportlevel & DISPVDBG) {
	msg_printf(VDBG, "    _get_disopts: optstr `%s';\n", SPX_OPTS);
	msg_printf(VDBG,
	    "  spx_argc %d, spx_argv[0] = `%s', spx_argv[1] = `%s'\n", 
	    argc, argv[0], argv[1]);
    }
#endif /* LAUNCH_DEBUG */

    res = _get_options(argc, argv, SPX_OPTS, optlistp);

#ifdef LAUNCH_DEBUG
    if (Reportlevel & DISPVDBG) {
	    msg_printf(VDBG,"  _get_disopts after `--': %d opts parsed\n  ",
		optlistp->optcount);
	    _dump_optlist(optlistp);
	    _dump_getopt_vars(res);
    }
#endif /* LAUNCH_DEBUG */

    if (res != DD_RETURN) {
	msg_printf(USER_ERR,
	    "_get_disopts: must end spx options with \"--\"\n");
	if (Reportlevel >= DISPDBG) {
	    _dump_optlist(optlistp);
	    _dump_getopt_vars(res);
	}
	return(SPXERR_NOARGEND);
    }

    /* if we're here, the arg list contained only valid spx opts and was
     * correctly terminated.  argv[optind-1] points to the 2nd "--";
     * this will be the diag's argv[0], so swap pointers and adjust 
     * the outgoing arg{c,v}.
     */
    argv[optind-1] = arginfop->argv[0];
    arginfop->argv = &argv[optind-1];
    arginfop->argc = argc - (optind-1);

    return(0);

} /* _get_disopts */



/*
 * begin ide execution-environment querying and setting routines
 */

/*
 * 'which' determines the environment variable, 'changeit' is 0 if only
 * querying, else non-zero; if changing, 'newval' contains new value.
 */
int
_ide_xenv(xenv_index_t which, int changeit, int newval)
{
    sym_t *tsym;

    if (which < 0 || which >= XENV_COMPONENTS) {
	msg_printf(IDE_ERR, "ide_xenv: bad 'which' (%d)\n",which);
	ide_panic("ide_xenv");
    }
    if (!(tsym = xenv_syms[which].val_symp)) {
	msg_printf(IDE_ERR, "ide_xenv: %d's value symbol not initialized!!\n",
		which);
	ide_panic("ide_xenv");
    }
    if (!(xenv_syms[which].cmd_symp)) {
	msg_printf(IDE_ERR, "ide_xenv: %d's cmd symbol not initialized!!\n",
		which);
	ide_panic("ide_xenv");
    }

    if (!changeit) {
	return(tsym->sym_i ? ENABLED : DISABLED);
    } else {
	if (newval)
	    tsym->sym_i = ENABLED;
	else
	    tsym->sym_i = DISABLED;
	return(tsym->sym_i ? ENABLED : DISABLED);
    }

} /* _ide_xenv */


/* 
 * error_logging: return 1 If IDE is currently logging the results of
 * diagnostic execution, 0 Otherwise.
 */ 
int 
error_logging(void)
{
    return(_ide_xenv(ERRLOGGING, 0, 0));
}

/* 
 * icached_exec(): return 1 If IDE is adjusting the PCs of diagnostics
 * to be in K0 space before invoking them, 0 Otherwise (IDE is linked
 * in K1SEG to allow it to execute on machines with broken caches).
 */ 
int 
icached_exec(void)
{
    return(_ide_xenv(ICACHED_EXEC, 0, 0));
}

/* 
 * quick_mode(): return 1 If IDE is running in Quick mode, 0 Otherwise.
 */ 
int 
quick_mode()
{
    return(_ide_xenv(QUICKMODE, 0, 0));
}

/*
 * Return: 1 if flag is set to continue executing a diagnostic in spite of
 * errors.  0 Otherwise.
 */
int
continue_on_error()
{
    return(_ide_xenv(C_ON_ERROR, 0, 0));
}


__psunsigned_t
icache_it(void * addr)
{
    __psunsigned_t caddr = (__psunsigned_t)addr;

    if (IS_KSEG0(addr)) {
	msg_printf(DBG, " ?fn address (0x%x) is already in K0SEG!\n", addr);
    } else if (IS_KSEG2(addr)) {
	msg_printf(JUST_DOIT, " ?fnaddr (0x%x) is in K2SEG!\n", addr);
    } else if (IS_KUSEG(addr)) {
	caddr = PHYS_TO_K0(addr);
	msg_printf(JUST_DOIT, 
	    "WARNING: fnaddr (0x%x) in KUSEG!  Launching icached (0x%x)\n",
	    addr, caddr);
    } else {
	caddr = K1_TO_K0(addr);
	msg_printf(DBG, " -run icached (0x%x --> 0x%x)\n", addr, caddr);
    }

    return(caddr);

} /* icache_it */


#define	CONFIG_RWBITS	(CONFIG_IC|CONFIG_DB|CONFIG_CU|CONFIG_K0)
int
_ide_printregs(void)
{
    uint vid_me = (uint)cpuid();
    ulong SR, cause;
    k_machreg_t sp;
#if R4000
    uint config;
#endif

    SR = get_sr();
    sp = get_sp();
    cause = get_cause();

    msg_printf(JUST_DOIT, "\n    %s(v%d): SR %R, sp 0x%x\n",
	(IDE_ME(ide_mode) == IDE_MASTER ? "MASTER" : "  slave"),
	vid_me, SR, sr_desc, sp);
    msg_printf(JUST_DOIT, "      [ignore IP8==(CNT/CMP)]: cause %R\n",
	cause, cause_desc);

#if R4000
    if (_SHOWIT(DBG+1)) {
	config = r4k_getconfig();
	if (!_SHOWIT(DBG+1))	/* only display the bits SW can change */
	    config &= CONFIG_RWBITS;
	msg_printf(JUST_DOIT, "      SW-writeable config bits: %R\n\n",
	    config, r4k_config_desc);
    }
#endif

    return(vid_me);

} /* _ide_printregs */

