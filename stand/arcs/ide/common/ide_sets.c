
#ident "$Revision: 1.12 $"

/*
 * arcs/ide/ide_sets.c: ide [cpu] set operations
 */

#include <genpda.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <uif.h>
#include "ide.h"
#include "ide_sets.h"
#include "ide_mp.h"
#include "ide_msg.h"
#include <libsc.h>

#if EVEREST
#include <sys/EVEREST/evconfig.h>
#endif /* EVEREST */

extern sym_t	Reportsym;

static sym_t *	_create_set(char *);
static void	_ide_set_usage(setfns_t, char *, int);

static int	_do_vp(setfns_t, int, char **);
static int	_do_3setfn(setfns_t, int, char **);
static int	_do_2setfn(setfns_t, int, char **);
static int	_do_1setfn(setfns_t, int, char **);

/* 
 * array of function-name strings 'setfnstrs' is indexed by enumerated
 * type 'setfns_t', typedef'ed in ide_sets.h
 */
char *setfnstrs[] = {
    {"create_set"}, {"copy_set"}, {"delete_set"},		/* 0-2 */
    {"display_set"}, {"add_cpu"},{"del_cpu"},			/* 3-5 */
    {"cpu_info"}, {"padslot7"}, {"padslot8"},			/* 6-8 */
    {"padslot9"}, {"padslot10"}, {"set_union"},			/* 9-11 */
    {"set_difference"}, {"set_intersection"}, {"set_equality"},	/* 12-14 */
    {"set_inequality"}, {"set_inclusion"}, {"cpu_in"},		/* 15-17 */
    {"set_empty"}, {"set_exists"}, {"padslot20"},		/* 18-20 */
};


/* +++++++++++++++++++++++++++ide CPU sets ++++++++++++++++++++++++++++++++++ */

/*
 * Many of the set routines have a user-callable form (generally in
 * <argc, char **argv> argument format), and an internal form, usually
 * expecting a symbol pointer and named identically to the former version
 * except with a leading underscore.  The user version provides varied
 * option choices and careful error-checking, then (generally) calls 
 * the internal form which actually performs the guts.
 */


/* exported (user-callable) set routines */

/* 
 * create_set -s set1[,set2,...] [ -a | -e | -c CopySetName ]
 *	-a: (default) initialize new set to the set of active cpus
 *	-e: initialize set to empty
 *	-c: initialize set with same members as <CopySetName>
 */

/* a,e, and c are mutually exclusive */
#define CS_UNSPECIFIED	0
#define CS_ACTIVE	1
#define CS_EMPTY	2
#define CS_COPYSET	3
#define CS_ILLEGAL	4

int wcount = 0;

int
create_set(int argc, char **argv)
{
    int sindex=-1, cindex=-1, _error=0;
    int i, setcnt=0;
    char *sname;
    int initopt = CS_UNSPECIFIED;
    sym_t *symptr=0, *csymptr=0;
    optlist_t optlist;
    option_t *optionp;
    r_kludge_t rk;

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
    if (initopt == CS_ILLEGAL) {
	msg_printf(USER_ERR,
		"create_set: only one of {a,e,c} options allowed\n");
	_ide_set_usage(CREATE_SET, argv[0], 1);
	return(EMUTEXOPTS);
    }
    if (sindex == -1) {		/* no setname(s) specified */
	msg_printf(USER_ERR,
		"create_set: setname (`-s' option) required\n");
	_ide_set_usage(CREATE_SET, argv[0], 1);
	return(ENOSETNAME);
    }
    if (initopt == CS_UNSPECIFIED)    /* init sets to active cpus by default */
	initopt = CS_ACTIVE;

    /* if CS_COPYSET, check that the specified set exists */
    if (initopt == CS_COPYSET) {
	optionp = (option_t *)&optlist.options[cindex];
	if (optionp->opt != 'c' || optionp->argcount <= 0) {
	    msg_printf(IDE_ERR, "create_set: bad opt '%c' or argcnt %d\n",
		optionp->opt, optionp->argcount);
	    ide_panic("create_set");
	}
	rk.silent = 0;
	if ((csymptr = _set_exists(optionp->args[0], &rk)) == (sym_t *)0) {
	    msg_printf(USER_ERR, "create_set: invalid copy-set \"%s\"\n", 
		optionp->args[0]);
	    return(rk.errno);
	}
	msg_printf(VDBG,"      copy from set \"%s\"\n",csymptr->sym_name);
    }

    if (Reportlevel >= VDBG)
	_dump_optlist(&optlist);

    optionp = (option_t *)&optlist.options[sindex];

    for (i = 0; i < optionp->argcount; i++) {
	sname = optionp->args[i];
	msg_printf(VDBG,"    create_set: setname \"%s\"\n", sname);

	/* is 'sname' already a symbol? */
	symptr = findsym(sname);
	if (symptr) {
	    msg_printf(USER_ERR," create_set: symbol \"%s\" already exists\n",
		sname);
	    _error++;
	    continue;	/* nonfatal */
	} else {
	    setcnt++;
	}

	if ((symptr = _create_set(sname)) == (sym_t *)0) {
	    msg_printf(IDE_ERR,"\r???? create_set: _create_set failed!!!\n");
	    return(IEBADCREATE);	/* fatal! */
	}

	/* initialize the set as specified */
	switch(initopt) {
	    case CS_ACTIVE:
		_vp_set(symptr);
		break;
	    case CS_EMPTY:
	    	symptr->sym_set = 0;
	    	break;
	    case CS_COPYSET:
	    	symptr->sym_set = csymptr->sym_set;
	    	break;
	    default:
	    	msg_printf(IDE_ERR,"\r??? create_set initopt %d???\n", 
			initopt);
	    	return(IEBADOPT);
	}
    }
    if (Reportlevel >= DBG) {
	msg_printf(DBG,"    %s: created %d set%s\n", argv[0], setcnt,
		_PLURAL(setcnt));
    }
    return(_error ? 1 : 0); 

} /* create_set */


/*
 * copy_set -s FROMSET,TOSET1[,TOSET2...]
 */
int
copy_set(int argc, char **argv)
{
    char setbuf[SETBUFSIZ];
    sym_t *src_setp, *dest_setp;
    int i, index, _error=0;
    optlist_t optlist;
    option_t *optionp;
    char *sname=NULL;
    r_kludge_t rk;

    if (Reportlevel >= DBG) {
	msg_printf(VDBG,"    %s: ", argv[0]);
	_dump_argvs(argc, argv);
    }

    if (_get_options(argc, argv, COPY_SET_OPTS, &optlist)) {
	msg_printf(USER_ERR,"%s: bad option or argument\n", argv[0]);
	_ide_set_usage(COPY_SET, argv[0], 1);
	return(BADOPTSORARGS);
    }

    /* getopts only ensured that iff -s appeared it had args. */
    if ((index = _opt_index('s', optlist.opts)) < 0) {
	msg_printf(USER_ERR,"copy_set: setname required\n");
	_ide_set_usage(COPY_SET, argv[0], 1);
	return(BADSETARGS);
    }
    
    if (Reportlevel >= VDBG) {
	_dump_optlist(&optlist);
    }

    optionp = (option_t *)&optlist.options[index];

    rk.silent = 1;
    /* this source set must already exist */
    if ((src_setp = _set_exists(optionp->args[0], &rk)) == (sym_t *)0) {
	msg_printf(USER_ERR, "%s: invalid source set \"%s\"\n", argv[0],
		optionp->args[0]);
	return(1);
    }

    /* and this/these source set(s) must NOT already exist */
    for (i = 1; i < optionp->argcount; i++) {
	if ((dest_setp = _set_exists(optionp->args[i], &rk)) != (sym_t *)0) {
	    msg_printf(USER_ERR, "%s: symbol or set \"%s\" already exists\n",
		argv[0], optionp->args[i]);
	    _error++;
	    continue;
	}
	sname = optionp->args[i];
	if ((dest_setp = _create_set(sname)) == (sym_t *)0) {
	    msg_printf(IDE_ERR,
		"\n??copy_set: _create_set of \"%s\" failed!!!\n", sname);
	    return(IEBADCREATE);	/* fatal! */
	}
	dest_setp->sym_set = src_setp->sym_set;

	sprintf_cset(setbuf, dest_setp->sym_set);
	msg_printf(DBG, "  copy_set: new set \"%s\": %s\n", sname, setbuf);
    }

    return(_error ? 1 : 0); 

} /* copy_set */


/*ARGSUSED1*/
int
delete_set(int argc, char **argv)
{
    msg_printf(JUST_DOIT,"<< %s stubbed >>\n",argv[0]);
    return(1);
} /* delete_set */

/* 
 * display_set requires the string name of set to display.  
 * Returns 1 if the specified set doesn't exist, else 0.
 */
int
display_set(int argc, char **argv)
{
    char setbuffer[SETBUFSIZ];
    sym_t *symptr = 0;
    int i, index, _error=0;
    optlist_t optlist;
    option_t *optionp;
    r_kludge_t rk;

    if (Reportlevel >= DBG) {
	msg_printf(VDBG,"    %s: ", argv[0]);
	_dump_argvs(argc, argv);
    }

    if (_get_options(argc, argv, DISPLAY_SET_OPTS, &optlist)) {
	msg_printf(USER_ERR,"%s: bad option or argument\n", argv[0]);
	_ide_set_usage(DISPLAY_SET, argv[0], 1);
	return(BADOPTSORARGS);
    }

    /* getopts only ensured that iff -s appeared it had args. */
    if ((index = _opt_index('s', optlist.opts)) < 0) {
	msg_printf(USER_ERR,"display_set: set specification required\n");
	_ide_set_usage(DISPLAY_SET, argv[0], 1);
	return(BADSETARGS);
    }
    
    if (Reportlevel >= VDBG) {
	_dump_optlist(&optlist);
    }

    optionp = (option_t *)&optlist.options[index];

    rk.silent = 1;
    for (i = 0; i < optionp->argcount; i++) {
	if ((symptr = _set_exists(optionp->args[i], &rk)) == (sym_t *)0) {
	    msg_printf(USER_ERR, "%s: invalid set \"%s\"\n", argv[0],
		optionp->args[i]);
	    _error++;
	    continue;
	}
	sprintf_cset(setbuffer, symptr->sym_set);
	msg_printf(JUST_DOIT,"  `%s':  %s\n", optionp->args[i], setbuffer);
    }

    return(_error ? 1 : 0); 

} /* display_set */


/* _add_cpu: add virtual processor(s) to specified set(s).  Ranges 
 * of vids are checked (0.._ide_info.vid_cnt), and all setnames
 * must already exist and be of type SYM_SET.
 * 
 * usage: add_cpu -v vid1[,vid2...] -s s1[,s2...]
 */
int
_add_cpu(int argc, char **argv)
{
    if (Reportlevel >= DBG) {
	msg_printf(VDBG,"  %s: ",argv[0]);
	_dump_argvs(argc, argv);
    }

    /* _do_vp displays any appropriate error messages */
    return(_do_vp(ADD_CPU, argc, argv));

} /* _add_cpu */

int
_del_cpu(int argc, char **argv)
{
    if (Reportlevel >= DBG) {
	msg_printf(VDBG,"  %s: ",argv[0]);
	_dump_argvs(argc, argv);
    }

    return(_do_vp(DEL_CPU, argc, argv));

} /* _del_cpu */


/*
 * _do_cpuinfo [-s set1[,set2,..]]
 * _do_cpuinfo returns the number of active processors in the system
 * (based on information obtained from the prom configuration tree).
 * Negative return values indicate errors, which are defined in ide.h.
 * All flags are optional: -s fills one or more sets (which must already
 * exist) with members representing the list of active virtual processor#s.
 * The -r flag forces _do_cpuinfo to re-traverse the tree, recalculating 
 * vid_set and vid_cnt.  (Otherwise, static values--calculated
 * during ide bootup by ide_cpu_probe--are returned.  The prom currently
 * does not update the configuration tree dynamically, so the -r opt isn't
 * useful). 
 *
 * Note that although IP{12,20,22} proms do not build this config tree,
 * these are all single processor machines, so _do_cpuinfo() returns a
 * processor count of 1, and sets consisting of { 0 } as the sole member.
 */
int
_do_cpuinfo(int argc, char **argv)
{
    char setbuffer[SETBUFSIZ];
    optlist_t optlist;
    option_t *optionp;
    setlist_t slist;
    int retval, i, index;
    int recalc=0, dosets=0;

#if defined(IDEBUG) || defined(LAUNCH_DEBUG) || defined(SET_DEBUG)
    if (Reportlevel >= DBG) {
	msg_printf(VDBG,"    %s: ", argv[0]);
	_dump_argvs(argc, argv);
	if (_ide_info.vid_cnt == (uint)-1) {	/* unitialized as yet */
	    msg_printf(VDBG,"%s: initial call\n", argv[0]);
	}
    }
#endif

    if (_get_options(argc, argv, CPU_INFO_OPTS, &optlist)) {
/* 	msg_printf(USER_ERR," %s: bad option or argument\n", argv[0]); */
	_ide_set_usage(CPU_INFO, argv[0], 1);
	return(BADOPTSORARGS);
    }

#ifdef NOTDEF
    /* check for -r "recalc" option */
    if ((index = _opt_index('r', optlist.opts)) >= 0) {
#if FUTURE
	msg_printf(SUM, "%s: recalculate option is not yet implemented\n",
		argv[0]);
	return(0);
#endif
	if (Reportlevel >= DBG) {
	    msg_printf(VDBG,"    %s: recalculate vid count and set", argv[0]);
	}
	recalc = 1;
    }
#endif
    
    /* and for -s "initialize sets with active vids" option */
    if ((index = _opt_index('s', optlist.opts)) >= 0) {

	optionp = (option_t *)&optlist.options[index];
	retval = _check_sets(optionp, &slist, 0);
	if (retval != 0) {	/* set problem; return */
	    msg_printf(USER_ERR,"%s: set argument error %d\n", 
		argv[0], retval);
	    return(retval);
	}

	if (Reportlevel >=  VDBG) {
	    msg_printf(VDBG,"    %d set%s verified:", slist.setcnt, 
		_PLURAL(slist.setcnt));
	    if (Reportlevel >=  VDBG) {
		msg_printf(VDBG,"\n");
		for (i = 0; i < slist.setcnt; i++) {
		    _display_set(slist.setptrs[i], D_SILENT);
		    msg_printf(VDBG,"\n");
		}
	    } else {	/* just display setnames */
		for (i = 0; i < slist.setcnt; i++) {
		    msg_printf(DBG,"%s %s", (i == 0 ? " " : ", "),
			slist.setptrs[i]->sym_name);
		}
		msg_printf(DBG,"\n");
	    }
	}

	dosets = 1;

    } /* -s opt */

    if (recalc) {
#if defined(FUTURE)
 ***	ide_init_vpinfo(&_ide_info);
#else
	msg_printf(IDE_ERR, "  %s: `-r' option not supported\n", argv[0]);
#endif
    }

    if (dosets) {
	for (i = 0; i < slist.setcnt; i++)
	    slist.setptrs[i]->sym_set = _ide_info.vid_set;
    }

    if (Reportlevel > NO_REPORT) {
	sprintf_cset(setbuffer, _ide_info.vid_set);
	msg_printf(JUST_DOIT,"  %d active processor%s: %s\n",
	    _ide_info.vid_cnt, _PLURAL(_ide_info.vid_cnt), setbuffer);
    }

    return((int)_ide_info.vid_cnt);

} /* _do_cpuinfo */


/* 
 * next 9 routines return info about sets and their contents.
 * '+' == set union, '-' == set difference.
 * 
 * - set_union - Set A + Set B --> Set C
 * - set_difference - set A - set B --> set C
 * - set_intersection
 * 
 *   The next six routines return SCRIPT_TRUE (1) or SCRIPT_FALSE (0)
 * - set_equality
 * - set_inequality
 * - set_inclusion
 * - cpu_in
 * - set_empty
 * - set_exists
*/

/*
 * set_union() set operation.
 * 3 sets (<srcset1, srcset2, destset>) are mandatory; only destset is
 * modified (unless one of the source-sets is named as the dest). 
 * Returns 0 (== success) if no errors occurred, else a negative
 * value explaining why.
 */
int
set_union(int argc, char **argv)
{
    /* _do_3setfn displays any appropriate error messages */
    return(_do_3setfn(SET_UNION, argc, argv));

} /* set_union */

int
set_differ(int argc, char **argv)
{
    return(_do_3setfn(SET_DIFFER, argc, argv));

} /* set_differ */

int
set_inter(int argc, char **argv)
{
    return(_do_3setfn(SET_INTER, argc, argv));

} /* set_intersection */

/*
 * set_equality() set operation: is set A == set B.
 * Requires 2 sets exactly.  Returns SCRIPT_TRUE if both sets contain
 * exactly the same vpids, else SCRIPT_FALSE.
 * Errors return negative values, as defined in ide_sets.h.
 */
int
set_equality(int argc, char **argv)
{
    return(_do_2setfn(SET_EQUALITY, argc, argv));

} /* set_equality */

/*
 * set_inequality() set operation: is set A != set B.
 * Requires 2 sets exactly.
 * Returns SCRIPT_TRUE if the sets do not contain
 * exactly the same vps, else SCRIPT_FALSE.
 * Errors return negative values, as defined in ide_sets.h.
 */
int
set_inequality(int argc, char **argv)
{
    return(_do_2setfn(SET_INEQUALITY, argc, argv));

} /* set_inequality */

/*
 * set_inclusion() set operation: is set A a proper subset of set B.
 * Requires 2 sets exactly.  NOTE: as per mathematical notation,
 * this function determines whether every vid in set A is also
 * a member of set B.
 * Returns SCRIPT_TRUE if A is a subset of B, else SCRIPT_FALSE.
 * Errors return negative values defined in ide_sets.h.
 */
int
set_inclusion(int argc, char **argv)
{
    return(_do_2setfn(SET_INCLUSION, argc, argv));

} /* set_inclusion */

/*
 * cpu_in() set operation: is physical processor P a member of set A.
 * Requires exactly one set and one vpid.  
 * For vids in the range of (0 <= vid < MAXCPU), cpu_in returns 
 * SCRIPT_TRUE if the vpid is a member, SCRIPT_FALSE if not,
 * and a variety of negative error values.  vids outside of this
 * range are clearly invalid, and therefore generate an error (unlike
 * vids which are larger than the number of cpus on a particular
 * machine, but NOT larger than the machine's architecture permits).
 */
int
cpu_in(int argc, char **argv)
{
    return(_do_1setfn(CPU_IN, argc, argv));

} /* cpu_in */

/* 
 * set_empty returns SCRIPT_TRUE if specified set contains no elements,
 * else SCRIPT_FALSE.  
 */
int
set_empty(int argc, char **argv)
{
    return(_do_1setfn(SET_EMPTY, argc, argv));

} /* set_empty */


/*
 * set_exists: Determine whether a symbol with specified name exists, 
 * and whether it is of type 'set'.
 * Returns IESYM_NOT_FOUND if no symbol by that name exists,
 * IESYM_NOT_SETTYPE if it exists but is not of basetype SYMB_SET,
 * and SCRIPT_TRUE if everything is jake.
 */
int
set_exists(int argc, char **argv)
{
    optlist_t optlist;
    option_t *optionp;
    int index;
    r_kludge_t rk;

#if defined(IDEBUG)
    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"    %s: ", argv[0]);
	_dump_argvs(argc, argv);
    }
#endif

    if (_get_options(argc, argv, SET_EXISTS_OPTS, &optlist)) {
	msg_printf(USER_ERR,"%s: bad option or argument\n", argv[0]);
	_ide_set_usage(SET_EXISTS, argv[0], 1);
	return(BADOPTSORARGS);
    }

    if ((index = _opt_index('s', optlist.opts)) < 0) {
	msg_printf(USER_ERR,"%s: setname required\n", argv[0]);
	_ide_set_usage(SET_EXISTS, argv[0], 1);
	return(BADSETARGS);
    }
    
#if defined(IDEBUG)
    if (Reportlevel >= VDBG) {
	_dump_optlist(&optlist);
    }
#endif

    optionp = (option_t *)&optlist.options[index];
    if (optionp->argcount != 1) {
	msg_printf(USER_ERR,"%s: only one setname allowed\n", argv[0]);
	_ide_set_usage(SET_EXISTS, argv[0], 1);
	return(BADARGCOUNT);
    }

    rk.silent = 1;

    /*
     * set_exists is intended to be called with invalid symbols, so
     * returning errors isn't proper.  This is a binary return.
     */
    if (_set_exists(optionp->args[0], &rk))
	return(1);
    else 
	return(0);

} /* set_exists */



/*
 * begin set-related primitives which aren't exported to script-level:
 * ide-internal use only
 */

int
_cpu_count(void)
{
    ASSERT(_ide_info.g_magic == IDE_GMAGIC);

    msg_printf(JUST_DOIT, "   %d processor%s\n", 
		(int)_ide_info.vid_cnt);
    return((int)_ide_info.vid_cnt);

} /* _cpu_count */

int
_vp_set(sym_t *symptr)
{
    char setbuffer[SETBUFSIZ];

    ASSERT(_ide_info.g_magic == IDE_GMAGIC);

    if (!symptr) {
	msg_printf(IDE_ERR,"!!!??_vp_set: NULL symptr??!!!\n");
	ide_panic("_vp_set");
    }

    symptr->sym_set = _ide_info.vid_set;

    if (Reportlevel >= VDBG) {
	sprintf_cset(setbuffer, symptr->sym_set);
	msg_printf(DBG,"  set \"%s\",  %d (virtually-numbered) cpus: %s\n",
	    (symptr->sym_name ? symptr->sym_name : "?unnamed set?"),
	    (int)_ide_info.vid_cnt, setbuffer);
    }

    return(0);

} /* _vp_set */


/*
 * procedures for set_{union,difference,intersection} are identical
 * except for final operation, so all three call _do_3setfn().
 */
static int
_do_3setfn(setfns_t fnindex, int argc, char **argv)
{
#if defined(IDEBUG)
    char setbuffer[SETBUFSIZ];
#endif
    optlist_t optlist;
    setlist_t slist;
    int retval;
    char *getopt_str;

#if defined(IDEBUG)
    if (Reportlevel >= DBG) {
	msg_printf(VDBG,"    %s (fnindex %d): ", argv[0], fnindex);
	_dump_argvs(argc, argv);
    }
#endif

    switch(fnindex) {

    case SET_UNION:
	getopt_str = SET_UNION_OPTS;
	break;
    case SET_DIFFER:
        getopt_str = SET_DIFF_OPTS;
	break;
    case SET_INTER:
	getopt_str = SET_INTER_OPTS;
	break;
    default:
	msg_printf(IDE_ERR,"_do_3setfn (%s): bad `fnindex' (%d)\n",
		argv[0], fnindex);
	ide_panic("_do_3setfn");
    }

    if (_get_options(argc, argv, getopt_str, &optlist)) {
	msg_printf(USER_ERR,"%s: bad option or argument\n", argv[0]);
	_ide_set_usage(fnindex, argv[0], 1);
	return(BADOPTSORARGS);
    }

    slist.setcnt = 3;
    if (retval = _do_setfnprep(&optlist, &slist, NULL, 0)) {
	return(retval);
    }
    ASSERT(slist.setcnt == 3);

    switch(fnindex) {

    case SET_UNION:
	slist.setptrs[DEST]->sym_set =
		(slist.setptrs[SRC1]->sym_set | slist.setptrs[SRC2]->sym_set);
	break;
    case SET_DIFFER:
	slist.setptrs[DEST]->sym_set =
	       (slist.setptrs[SRC1]->sym_set & ~(slist.setptrs[SRC2]->sym_set));
	break;
    case SET_INTER:
	slist.setptrs[DEST]->sym_set =
		(slist.setptrs[SRC1]->sym_set & slist.setptrs[SRC2]->sym_set);
	break;
    }

#if defined(IDEBUG)
    if (Reportlevel >= VDBG) {
	sprintf_cset(setbuffer, slist.setptrs[DEST]->sym_set);
	msg_printf(VDBG,"  %s, result set `%s': %s\n",argv[0],
		slist.setptrs[DEST]->sym_name, setbuffer);
    }
#endif

    return(0); 

} /* _do_3setfn */

/*
 * ide_{equality, inequality, and inclusion} share _do_2setfn().
 */
static int
_do_2setfn(setfns_t fnindex, int argc, char **argv)
{
    optlist_t optlist;
    setlist_t slist;
    int i, retval;
    char *getopt_str;
    set_t mask, subset, superset;

#if defined(IDEBUG)
    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"    %s (fnindex %d): ", argv[0], fnindex);
	_dump_argvs(argc, argv);
    }
#endif

    switch(fnindex) {

    case SET_EQUALITY:
	getopt_str = SET_EQUALITY_OPTS;
	break;
    case SET_INEQUALITY:
	getopt_str = SET_EQUALITY_OPTS;
	break;
    case SET_INCLUSION:
	getopt_str = SET_INCLUSION_OPTS;
	break;
    default:
	msg_printf(IDE_ERR, "_do_2setfn (from %s): bad `fnindex' (%d)\n",
		argv[0], fnindex);
	ide_panic("_do_2setfn");
    }

    if (_get_options(argc, argv, getopt_str, &optlist)) {
	_ide_set_usage(fnindex, argv[0], 1);
	return(BADOPTSORARGS);
    }

    slist.setcnt = 2;
    if (retval = _do_setfnprep(&optlist, &slist, NULL, 0)) {
	return(retval);
    }
    ASSERT(slist.setcnt == 2);

    switch(fnindex) {

    case SET_EQUALITY:
	if (slist.setptrs[SRC1]->sym_set == slist.setptrs[SRC2]->sym_set)
	    return(SCRIPT_TRUE);
	else
	    return(SCRIPT_FALSE);
    case SET_INEQUALITY:
	if (slist.setptrs[SRC1]->sym_set != slist.setptrs[SRC2]->sym_set)
	    return(SCRIPT_TRUE);
	else
	    return(SCRIPT_FALSE);
    case SET_INCLUSION:
	subset = slist.setptrs[SRC1]->sym_set;
	superset = slist.setptrs[SRC2]->sym_set;
	mask = SET_ZEROBIT;

	for (i = 0; i < MAXSETSIZE; i++) {
	    /* every set bit in 'subset' must also be set in 'superset' */
	    if ((mask & subset) && !(mask & superset))
		return(SCRIPT_TRUE);
	    mask >>= 1;
	}
	return(SCRIPT_FALSE);
    }
    return -1;
} /* _do_2setfn */

/* cpu_in() and set_empty() call this routine */
static int
_do_1setfn(setfns_t fnindex, int argc, char **argv)
{
    optlist_t optlist;
    setlist_t slist;
    vplist_t vplist;
    int retval;
    char *getopt_str;

#if defined(IDEBUG)
    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"    %s (fnindex %d): ", argv[0], fnindex);
	_dump_argvs(argc, argv);
    }
#endif

    switch(fnindex) {

    case SET_EMPTY:
	getopt_str = SET_EMPTY_OPTS;
	slist.setcnt = 1;
	vplist.vpcnt = 0;
	break;
    case CPU_IN:
	getopt_str = CPU_IN_OPTS;
	slist.setcnt = 1;
	vplist.vpcnt = 1;
	break;
    default:
	msg_printf(IDE_ERR, "_do_1setfn (%s): bad `fnindex' (%d)\n",
		argv[0], fnindex);
	ide_panic("_do_1setfn");
    }

    if (_get_options(argc, argv, getopt_str, &optlist)) {
	msg_printf(USER_ERR,"`%s': bad option or argument\n", argv[0]);
	_ide_set_usage(fnindex, argv[0], 1);
	return(BADOPTSORARGS);
    }

    if (retval = _do_setfnprep(&optlist, &slist, &vplist, 0)) {
	return(retval);
    }

    ASSERT(slist.setcnt == 1);

    switch(fnindex) {

    case SET_EMPTY:
	if (slist.setptrs[SRC1]->sym_set == 0)
	    return(SCRIPT_TRUE);
	else
	    return(SCRIPT_FALSE);
    case CPU_IN:
	ASSERT(vplist.vpcnt == 1);
	ASSERT(PLAUSIBLE_VID(vplist.vps[0]));

	if (slist.setptrs[SRC1]->sym_set & (SET_ZEROBIT >> vplist.vps[0]))
	    return(SCRIPT_TRUE);
	else
	    return(SCRIPT_FALSE);
    }
    return -1;
} /* _do_1setfn */




/* _do_vp: add/del virtual id(s) to/from specified set(s).  
 * Called from _addcpu and _delcpu, _do_vp actually does the work.
 * Range-checks the virtual numbers (>= 0 && < ide_info.vid_max),
 * and all setnames must exist and be of basetype SYMB_SET.
 */
static int
_do_vp(setfns_t fnindex, int argc, char **argv)
{
    optlist_t optlist;
    setlist_t slist;
    vplist_t vplist;
    int i, j, retval;
    sym_t *symptr;
    char *getopt_str;

    if (fnindex == ADD_CPU) {
	getopt_str = ADD_CPU_OPTS;

    } else if (fnindex == DEL_CPU) {
	getopt_str = DEL_CPU_OPTS;

    } else {
	msg_printf(IDE_ERR," _do_vp (%s): illegal `fnindex' (%d)\n",
		argv[0], fnindex);
	ide_panic("_do_vpid");
    }

    if (_get_options(argc, argv, getopt_str, &optlist)) {
	msg_printf(USER_ERR,"%s: bad option or argument\n", argv[0]);
	_ide_set_usage(fnindex, argv[0], 1);
	return(BADOPTSORARGS);
    }

#if defined(SET_DEBUG)
    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"_do_vp, optlist: \n");
	_dump_optlist(&optlist);
    }
#endif

    /* add/del one or more vpids to/from one or more sets */
    slist.setcnt = ONE_OR_MORE_ARGS;
    vplist.vpcnt = ONE_OR_MORE_ARGS;
    /* if called with nonzero 'silent' parm, _do_setfnprep() displays 
     * any pertinent error messages */
    if (retval = _do_setfnprep(&optlist, &slist, &vplist, 0)) {
	return(retval);
    }

#if defined(SET_DEBUG)
    msg_printf(VDBG,"    %s (fnindex %d):  validated %d set%s & %d vid%s\n",
	  argv[0], fnindex, slist.setcnt, _PLURAL(slist.setcnt), 
	  vplist.vpcnt, _PLURAL(vplist.vpcnt));
#endif


    for (i = 0; i < slist.setcnt; i++) {
	symptr = slist.setptrs[i];
 	for (j = 0; j < vplist.vpcnt; j++) {
	    ASSERT(PLAUSIBLE_VID(vplist.vps[0]));

	    if (fnindex == ADD_CPU)
	        symptr->sym_set |= (SET_ZEROBIT >> vplist.vps[j]);
	    else
	        symptr->sym_set &= ~(SET_ZEROBIT >> vplist.vps[j]);
	}
    }

    return(0); 

} /* _do_vp */



static sym_t *
_create_set(char *sname)
{
    sym_t *symptr;

    ASSERT(sname != NULL);

    msg_printf(VDBG,"    _create_set: setname `%s'\n",
	(sname ? sname : "!NULL SNAME!"));

    symptr = makesym();
    symptr->sym_basetype = SYMB_SET;
    symptr->sym_type = SYM_VAR;
    symptr->sym_name = makestr(sname);
    addsym(symptr);	/* becomes permanent: addsym sets SYM_INTABLE flag */

    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"    exit _create_set: new set named \"%s\"\n", 
		symptr->sym_name);
    }

    return(symptr);

} /* _create_set */


/*
 * _display_set: internal (sym_t ptr-based) set-display routine.
 * Note that this routine is NOT static: it is needed by other
 * ide files.
 */
/*ARGSUSED2*/
int
_display_set(sym_t *symptr, int d_mode)
{
    char setbuffer[SETBUFSIZ];
    set_t set;

    if (!symptr)
	ide_panic("!!!_display_set: NULL symptr!!!\n");

    msg_printf(VDBG,"setname \"%s\": ", _SAFESTR(symptr->sym_name));

    set = symptr->sym_set;

    sprintf_cset(setbuffer, set);
    msg_printf(JUST_DOIT, "%s", setbuffer);

    return(0);

} /* _display_set */

/*
 * _set_exists checks that 'setname' symbol exists and is of basetype
 * SYMB_SET.  Returns a sym_t pointer to the symbol's struct if both
 * criteria are satisfied.  Returns NULL ptr with an rk->error of 
 * IESYM_NOT_SETTYPE if a symbol by that name exists but isn't a set,
 * else NULL with rk->error of IESYM_NOT_FOUND.
 * 'silent' suppresses error-message display. 
 */
sym_t *
_set_exists(char *setname, r_kludge_t *rk)
{
    sym_t *symptr = 0;
    int silent = rk->silent;

    if (!setname)
	ide_panic("_set_exists: NULL setname\n");

    if ((Reportlevel >= PRIMDBG) && !silent) {
	msg_printf(VDBG+1,"  \"%s\":", setname);
    }

    symptr = findsym(setname);
    if (!symptr || (symptr->sym_basetype != SYMB_SET)) {
	if (!silent) {
	    msg_printf(USER_ERR,"  setname \"%s\" %s\n", setname,
		(!symptr ? "not found" : "not set-type"));
	}
	if (!symptr) {
	    rk->errno = IESYM_NOT_FOUND;
	} else {
	    rk->errno = IESYM_NOT_SETTYPE;
	}
	return(NULL);
    }

    return(symptr);

} /* _set_exists */

/* 'optionp' points to an 'option_t' struct containing string setnames.
 * _check_sets verifies that each set in the option_t args list exists
 * as a symbol and is of basetype SYMB_SET, fetching each set's sym_t 
 * pointer in the process.  The verified sets' pointers are returned in
 * the 'setlistp' struct, along with the number of sets verified.  Sets
 * which fail are not included in the tally or setptr count, but the
 * entire list is checked regardless.  (Routines requiring ONE_OR_MORE_ARGS
 * may not consider one bad set to be a fatal error.) _check_sets returns
 * 0 if all incoming setnames were ok, -1 otherwise.  'silent' suppresses
 * all error message display other than those reporting ide-internal 
 * problems.
 */
/*ARGSUSED3 -- XXX nuke parameter? */
int
_check_sets(option_t *optionp, setlist_t *setlistp, int silent)
{
    int i, _error=0;
    sym_t *symptr;
    r_kludge_t rk;

    if (!setlistp) {
	msg_printf(IDE_ERR," _check_sets: NULL setlist pointer\n");
	ide_panic("_check_sets");
    }
    if (!optionp || optionp->argcount <= 0 || optionp->argcount > MAX_OPTARGS) {
	msg_printf(IDE_ERR, "_check_sets: bad option list\n");
	ide_panic("_check_sets");
    }

    setlistp->setcnt = 0;
    rk.silent = 0;

    for (i = 0; i < optionp->argcount; i++) {
	if ((symptr = _set_exists(optionp->args[i], &rk)) != (sym_t *)0) {
	    setlistp->setptrs[i] = symptr;
	    setlistp->setcnt++;
	} else {
	    _error++;
	}
    }

    if (Reportlevel >= DBG) {
	if (_error) {
	    msg_printf(VDBG,"    (%d bad set%s:  verified %d set%s)",
		_error, _PLURAL(_error),
		setlistp->setcnt, _PLURAL(setlistp->setcnt));
	} else {
	    msg_printf(DBG,"    verified %d set%s", setlistp->setcnt,
		_PLURAL(setlistp->setcnt));
	}
	if (Reportlevel >= VDBG) {
	    msg_printf(VDBG,":\n      ");
	    for (i = 0; i < setlistp->setcnt; i++)
		msg_printf(VDBG,"%s ",setlistp->setptrs[i]->sym_name);
	} else {
	    msg_printf(VDBG,"\n");
	}
    }

    return(_error ? -1 : 0);

} /* _check_sets */


int
_check_vpids(option_t *optionp, vplist_t *vplistp)
{
    int i, _error=0;
    int vpid;

    if (!vplistp) {
	msg_printf(IDE_ERR," _check_vpids: NULL vplist pointer\n");
	ide_panic("_check_vpids");
    }
    if (!optionp||optionp->argcount <= 0 || optionp->argcount > MAX_OPTARGS) {
	msg_printf(IDE_ERR," _check_vpids: bad option list\n");
	ide_panic("_check_vpids");
    }

    vplistp->vpcnt = 0;

    /* convert argument strings into vids (ints), and range-check them */
    for (i = 0; i < optionp->argcount; i++) {
 	if (!atob(optionp->args[i], &vpid)) {
	    _error++;
	    msg_printf(USER_ERR,"vid `%s' NAN\n", optionp->args[i]);
	} else {
	    if (PLAUSIBLE_VID(vpid)) {
		vplistp->vps[i] = vpid;
		vplistp->vpcnt++;
	    } else {
		_error++;
		msg_printf(USER_ERR, "vid `%d' out of range\n", vpid);
	    }
	}
    }

    if (Reportlevel >= DBG) {
	if (_error) {
	    msg_printf(DBG,"    %d good vid%s (%d bad)", vplistp->vpcnt,
		_PLURAL(vplistp->vpcnt), _error);
	} else {
	    msg_printf(DBG,"    %d vid%s", vplistp->vpcnt,
		_PLURAL(vplistp->vpcnt));
	}
	if (Reportlevel >= VDBG) {
	    msg_printf(VDBG,":\n      ");
	    for (i = 0; i < vplistp->vpcnt; i++)
		msg_printf(VDBG,"%d ",vplistp->vps[i]);
	} else {
	    msg_printf(DBG,"\n");
	}
    }

    return(_error ? -1 : 0);

} /* _check_vpids */

static char dosetprep[] = "_do_setfnprep";
/* need optlist filled, slistp->setcnt and vplistp->vpcnt set to
 * the required number of each (if slistp/vplistp == NULL, _do_setfnprep
 * assumes that caller doesn't need any sets/vps, resp.  'silent' 
 * suppresses all error message display other than those reporting
 * ide-internal problems, and propagates to called routines.
 */
int
_do_setfnprep(
  optlist_t *optlistp,
  setlist_t *slistp,
  vplist_t *vplistp,
  int silent)
{
    option_t *setoptp, *vpoptp;
    int setsreqd, vpsreqd;
    int pindex, sindex;
    int rc;

    if (!optlistp) {
	msg_printf(IDE_ERR,"_do_setfnprep: optlist ptr is NULL\n");
	ide_panic("_do_setfnprep");
    }
    if (Reportlevel >= VDBG) {
	msg_printf(VDBG,"_do_setfnprep, optlist: ");
	_dump_optlist(optlistp);
    }

    /* NULL slistp or setcnt == 0 means calling routine doesn't need any
     * sets verified. ONE_OR_MORE_ARGS is defined as (MAX_OPTARGS+1).
     */
    if (slistp && slistp->setcnt != 0) {
	if (slistp->setcnt < 0 || slistp->setcnt > ONE_OR_MORE_ARGS) {
	    msg_printf(IDE_ERR," %s: bad `setcnt' (%d); no get_options?\n",
		dosetprep, slistp->setcnt);
	    ide_panic("_do_setfnprep");
	}

	/* verify that the -s option was found (getopts ensured that--if
	 * it appeared--it had accompanying args) */
	if ((sindex = _opt_index('s', optlistp->opts)) < 0) {
	    if (!silent)
		msg_printf(USER_ERR," set name required\n");
	    return(BADSETARGS);
	}
	setoptp = (option_t *)&optlistp->options[sindex];
	setsreqd = slistp->setcnt;
	slistp->setcnt = 0;
	rc = _check_sets(setoptp, slistp, silent);

	/* 
	 * error if  a) _check_sets return != 0,  b) caller required >= 1
	 * args and we don't have any, or  c) caller needed an exact 
	 * number of args & argcnt doesn't match
	 */
	if (rc != 0 || (setsreqd==ONE_OR_MORE_ARGS && slistp->setcnt <= 0) ||
	     (setsreqd != ONE_OR_MORE_ARGS && setsreqd != slistp->setcnt) ) {
#ifdef notdef
	    if (!silent)
		msg_printf(USER_ERR,
			" invalid set args\n");
#endif 

	    /*
	     * this is as close to accuracy as I can figure: _check_sets
	     * returned 0 cuz the symbol didn't exist or wasn't of btype
	     * SYMB_SET.
	     */
	    if (!rc)
	        return(SCRIPT_FALSE);
	    else
	        return(BADSETARGS);
	}
    }

    /* now convert string vid args to integers and range-check them.
     * NULL vplistp or 0 vpcnt means caller doesn't need vpids.
     */
    if (vplistp && vplistp->vpcnt != 0) {
	if (vplistp->vpcnt < 0 || vplistp->vpcnt > ONE_OR_MORE_ARGS) {
	    msg_printf(IDE_ERR," %s: invalid `vpcnt' (%d==0x%x)\n", dosetprep,
		vplistp->vpcnt, vplistp->vpcnt);
	    ide_panic("_do_setfnprep");
	}

	if ((pindex = _opt_index('v', optlistp->opts)) < 0) {
	    if (!silent)
		msg_printf(USER_ERR,"virtual cpu-number(s) required\n");
	    return(BADVPARGS);
	}
	vpoptp = (option_t *)&optlistp->options[pindex];
	vpsreqd = vplistp->vpcnt;
	vplistp->vpcnt = 0;
	rc = _check_vpids(vpoptp, vplistp);

	/* error criteria same as for set arguments, above */
	if (rc != 0 || (vpsreqd == ONE_OR_MORE_ARGS && vplistp->vpcnt <= 0) ||
	     (vpsreqd != ONE_OR_MORE_ARGS && vpsreqd != vplistp->vpcnt) ) {
#if VERBOSITY_DESIRED
!	    if (!silent)
!		msg_printf(USER_ERR,"invalid VID args\n");
#endif
	    return(BADVPARGS);
	}
    }

    return(0); 

} /* _do_setfnprep */


int
_do_set_help(void)
{
    int i;
    int tblsiz = (sizeof(setfnstrs)/ sizeof(char *));
    int fncnt = 0;
    builtin_t *bptr;

    for ( i = 0; i < tblsiz; i++ )
	if (strncmp(setfnstrs[i], "pad", 3))	/* skip padded slots */
	    fncnt++;
    
    msg_printf(JUST_DOIT,"IDE set-related commands (%d):\n",fncnt);

    for ( i = 0; i < tblsiz; i++ )
	if (strncmp(setfnstrs[i], "pad", 3)) {	/* skip padded slots */
	    if (bptr = _get_tbl_entry(setfnstrs[i])) {
		msg_printf(JUST_DOIT,"    %s:  \"%s\"\n", 
		    (bptr->name ? bptr->name : "NULL"),
		    (bptr->help ? bptr->help : ""));
	    }
	}

    return 0;

} /* _do_set_help */

static void
_ide_set_usage(setfns_t fnindex, char *fnname, int verbose)
{
#ifdef DEBUG
    builtin_t *bp = _get_tbl_entry(fnname);

    ASSERT(bp != NULL);
#endif

    (void)_ide_usage(fnname);
    if (!verbose)
	return;

    /* some set fns provide more detailed help */
    switch(fnindex) {

      case CREATE_SET:
	msg_printf(JUST_DOIT,"       -s: name(s) of new set(s)\n");
	msg_printf(JUST_DOIT,"       -a: initialize to set of vids\n");
	msg_printf(JUST_DOIT,"       -e: initialize new set(s) to empty\n");
	msg_printf(JUST_DOIT,"       -c: copy `copysetname' into new set(s)\n");
	return;

      case CPU_INFO:
	msg_printf(JUST_DOIT,
		"       default: return # of virtual processors in system\n");
	msg_printf(JUST_DOIT,
		"       -s: initialize set(s) to list of active vids\n");
	return;

      case SET_INCLUSION:
	msg_printf(JUST_DOIT,
		"       -s: each virtual pid in SUBset is also in SUPERset\n");
	msg_printf(JUST_DOIT,
		"       PLEASE NOTE THE ORDERING OF SUB vs. SUPER SETS.\n");
	return;


      case COPY_SET:
      case DELETE_SET:
      case DISPLAY_SET:
      case ADD_CPU:
      case DEL_CPU:
      case SET_UNION:
      case SET_DIFFER:
      case SET_INTER:
      case SET_EQUALITY:
      case SET_INEQUALITY:
      case CPU_IN:
      case SET_EMPTY:
      case SET_EXISTS:
	return;

      default:
	msg_printf(IDE_ERR, "_ide_set_usage (from %s): bad fnindex (%d)\n",
		fnname, (int)fnindex);
	ide_panic(fnname);
    }

} /* _ide_set_usage */

/*
 * display members of specified set, using comma-separated decimal numbers.
 * Set layout is defined with member 0 represented by high bit of most
 * significant byte of unsigned long long.
 */
int
_print_cset(set_t ullval)
{
    int i, didcomma = 0;
    setmask_t mask;

    mask = SET_ZEROBIT;
    if (!ullval) {
	msg_printf(JUST_DOIT,"{ }");
	return(0);
    }

    msg_printf(JUST_DOIT,"{ ");

    mask = SET_ZEROBIT;
    for (i = 0; i < MAXSETSIZE; i++) {
	if (mask & ullval) {
	    msg_printf(JUST_DOIT,"%s%d", (!didcomma ? "" : ","), i);
	    if (!didcomma)
		didcomma++;
	}
	mask >>= 1;
    }
    msg_printf(JUST_DOIT," }");

    return(0);


} /* _print_cset */


/*
 * sprintf_cset(): sprintf set characters into caller-provided buffer
 * in the same format as _print_cset() (above).
 * Set layout is defined with member 0 represented by high bit of most
 * significant byte of unsigned long long.
 */
size_t
sprintf_cset(char *bufptr, set_t ullval)
{
    int i, didcomma = 0;
    setmask_t mask;
    char *cp= bufptr;


    if (!bufptr)
	ide_panic("sprintf_cset: NULL bufptr!");

    mask = SET_ZEROBIT;
    if (!ullval) {
	sprintf(bufptr, "{ }");
	return(strlen(bufptr));
    }

    sprintf(bufptr,"{ ");
    cp += strlen(bufptr);

    mask = SET_ZEROBIT;
    for (i = 0; i < MAXSETSIZE; i++) {
	if (mask & ullval) {
	    sprintf(cp,"%s%d", (!didcomma ? "" : ","), i);
	    cp += strlen(cp);
	    if (!didcomma)
		didcomma++;
	}
	mask >>= 1;
    }
    sprintf(cp," }");

    return(strlen(bufptr));

} /* sprintf_cset */

