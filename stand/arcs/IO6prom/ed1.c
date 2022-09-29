#ident	"IO6prom/ed.c : $Revision: 1.6 $"

/*
 * File: ed.c
 *
 * enable and disable command handler.
 *
 */

#ifndef __ED1_C__
#define __ED1_C__        1

#include <kllibc.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>

#include <sys/SN/SN0/ip27config.h> 	/* Definition of SN00 */
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/gda.h>

#include "io6prom_private.h"
#include "ed.h"

static int 		ed_debug ;
#define db_printf	if (ed_debug) printf

#define PLOG_SET 	1
#define PLOG_UNSET	2
#define PLOG_GET 	3

#define MAX_REASON_BUFSZ 	64
#define MAX_PLOG_VARSZ 		32

void
ed_errmsg(cmdinf_t *ci, char *emsg)
{
    printf("%s: Module %d, Board %s - %s",
	ci->cmdstr, ci->mod, ci->sltstr, emsg) ;
}

#ifdef NEW_ED_CMD

/*
 * Function table for various combination
 * of commands.
 */

/* XXX - We need a single entry point for a brd type */
/* Easier to add entry points. */

int (*edfunc[])(cmdinf_t *) = {
enable_node	,
disable_node	,
enable_io	,
disable_io	,
enable_mb	,
disable_mb	,
NULL
} ;

typedef struct n_k_s {
	char	*name ;
	int	key  ;
} n_k_t ;

/* Shifts for each component type */
/* The #defines are tied to the string positions */

n_k_t cmpnky[] = {
#define CMP_NONE	0
"",	0,
#define CMP_CPU		1
"CPU", CMSK_CPUA_SHIFT,
#define CMP_MEM		2
"MEM", CMSK_MEMBNK_SHIFT,
#define CMP_PCI		3
"PCI", CMSK_PCI_SHIFT,
NULL,  CMSK_NEW_SHIFT
} ;

/* XXX Mask if all components were selected */

int cmp_masks[] = {
0,
CMSK_CPU,
CMSK_MEM,
CMSK_PCI
} ;

/* 
 * If your slot name is any one of the keys, use the 
 * value to index into edfunc.
 */

n_k_t cmdnky[] = {
"n",  		CMD_E_NODE,
"io", 		CMD_E_IO,
NULL, 0
} ;

n_k_t sn00_cmdnky[] = {
"MotherBoard", 	CMD_E_MB,
NULL, 0
} ;

n_k_t clsnky[] = {
"n", 		KLCLASS_CPU,
"MotherBoard", 	KLCLASS_CPU,
"io", 		KLCLASS_IO,
NULL, 		0
} ;

static cmdinf_t		cmd ;

static int 		parse_exec(int, char **) ;
static int 		write_edtab(cmdinf_t *) ;

static void
pr_ed_usage(char *cmd)
{
	printf("%s : %s -m modid [-s slotid] [-CPU a|b|ab]\n", cmd, cmd);
	printf("                                 [-MEM [1234567]]\n") ;
	printf("                                 [-PCI [01234567]]\n") ;
	printf("The system must be Reset for the %s to take effect.\n", cmd);
}

static void
pr_ed_status(char *cmd)
{
	printf("Reset the system for the %s to take effect.\n", cmd) ;
}

int
ed_cmd(int argc, char **argv)
{
	if (parse_exec(argc, argv) != 0)
		pr_ed_usage(argv[0]) ;
	return 0 ;
}

/* ----------------------------------------------------------------- */

/*
 * Command line parsing support
 */

int
check_max_indx(int max, int ind)
{
    if ((ind + 1) == max)
	return -1 ;
    return 1 ;
}

int
check_opt(char **argv, int indx)
{
    if (argv[indx][2])
	return -1 ;
    return 1 ;
}

int
check_edarg(char **argv, int argc, int indx)
{
    /* Check for -m or -s only. Avoid -mxxx */
    if ((check_opt(argv, indx) < 0) || 
        (check_max_indx(argc, indx) < 0))
            return -1 ;
    return 1 ;
}

int
get_arg_val(char *valp)
{
    int val = 0 ;
    if (isdigit(*valp))
        if (val = atoi(valp))
	    return val ;
    return -1 ;
}

/* Search for a string in the given table
 * and return index.
 */
int
find_string(char *argp, n_k_t *nk)
{
    int i = 0 ;
    char *cp ;

    cp = nk[i].name ;
    while(cp) {
	if (!strncmp(cp, argp, strlen(cp)))
	    return i ;
	cp = nk[++i].name ;
    }
    return -1 ;
}

/* Searches 2 global tables for a 
 * match on the board name. As a side
 * effect, returns the tab ptr in rnk.
 */
int
get_cmd_indx(char *cp, n_k_t **rnk)
{
    int         cindx ;
    n_k_t       *nk ;

    nk = cmdnky ;                                       /* Global var */
    cindx = find_string(cp, nk) ;
    if ((cindx < 0) && (SN00)) {
        nk = sn00_cmdnky ;
        cindx = find_string(cp, nk) ;
    }
    if ((cindx >= 0) && rnk)
        *rnk = nk ;
    return cindx ;
}

/* Get the index of the function to
 * use for a command. XXX
 */
int
get_cb(cmdinf_t	*ci)
{
	int 	cindx ;
	n_k_t   *nk = NULL;

    	cindx = get_cmd_indx(ci->sltstr, &nk) ;
	if (cindx < 0)
		return -1 ;
	return(nk[cindx].key+ci->cmdtyp) ;
}

/* return the ptr to the char next to the
 * keyword.
 */
char *
check_slot_arg(char *argp)
{
    char 	*sp ;
    int   	cindx ;
    n_k_t	*nk = NULL;

    cindx = get_cmd_indx(argp, &nk) ;
    if (cindx < 0)
	return NULL ;
    sp = &argp[strlen(nk[cindx].name)] ;
    return sp ;
}

edmask_t
get_cpu_msk(char *argp)
{
    edmask_t	cmpmsk = 0 ;

    if (strchr(argp, 'a') || strchr(argp, 'A'))
        cmpmsk |= CMSK_CPUA ;
    if (strchr(argp, 'b') || strchr(argp, 'B'))
        cmpmsk |= CMSK_CPUB ;
    return(cmpmsk) ;
}

edmask_t
get_num_msk(char *argp)
{
    int j = 0 ;
    edmask_t	cmpmsk = 0 ;

    while (argp[j]) {
        if (isdigit(argp[j]))
            cmpmsk |= (1 << (argp[j]-'0')) ;
        j++ ;
    }
    return cmpmsk ;
}

/* Get cpu or number mask. If we indeed go
 * a cpu mask, set it as that mode.
 */
edmask_t
get_raw_cmp_msk(char *argp, cmdinf_t *cmdp)
{
    if (!(cmdp->cmpmsk = get_cpu_msk(argp)))
        cmdp->cmpmsk   = get_num_msk(argp) ;
    else
	cmdp->cmdmde |= MODE_CPU_MSK ;

    return (cmdp->cmpmsk) ;
}

int
get_cmp_typ(char *argp, cmdinf_t *cmdp, n_k_t *nk)
{
    int cindx ;

    cindx = find_string(argp, nk) ;
    if (cindx < 0)
	return -1 ;
    cmdp->cmptyp = cindx ;
    /* Do this check here for now. Can it be checked later XXX */
    if ((cmdp->cmptyp != CMP_CPU) &&
	(cmdp->cmdmde & MODE_CPU_MSK))
	return -1 ;

    cmdp->cmpmsk = (cmdp->cmpmsk << (nk[cindx].key)) ;
    return 1 ;
}

/* 
 * prscmp
 *     parse the component portion of the command
 */

int
prscmp(char **argv, int argc, int indx, cmdinf_t *ci)
{
    if (check_max_indx(argc, indx) < 0)
	return -1 ;

    get_raw_cmp_msk(argv[indx+1], ci) ;

    if (get_cmp_typ(&argv[indx][1], ci, cmpnky) < 0)
	return -1 ;

    return 1 ;
}

void
dump_prsinf(cmdinf_t *ci)
{
    if (!ED_MODE_DEBUG(ci))
	return ;

    printf("typ    = %d\n", ci->cmdtyp) ;
    printf("cmdstr = %s\n", ci->cmdstr) ;
    printf("mod    = %d\n", ci->mod) ;
    printf("slt    = %d\n", ci->slt) ;
    printf("sltstr = %s\n", ci->sltstr) ;
    printf("cmptyp = %d\n", ci->cmptyp) ;
    printf("cmpmsk = %x\n", ci->cmpmsk) ;
    printf("cmdmde = %x\n", ci->cmdmde) ;
}

void
init_cmdinf(cmdinf_t *ci)
{
    ci->mod 		= -1 ;
    ci->slt 		= -1 ;
    ci->sltstr[0] 	= 0 ;
    ci->cmptyp 		= CMP_NONE ;
    ci->cmpmsk 		= 0 ;
    ci->cmdmde		= MODE_UNKNOWN ;
    ci->edr		= NULL ;
}

/*
 * get_my_node_slot_class
 *     visit_lboard callback.
 */

static int
get_my_node_slot_class(lboard_t *lb, void *ap, void *rp)
{
    unchar      *nsclass = (unchar *)rp ;

    if (lb->brd_type == KLTYPE_IP27) {
      *nsclass = SLOTNUM_GETCLASS(lb->brd_slot) ;
      return 0 ;
    }

    return 1 ;
}

lboard_t *
get_brd_klc(cmdinf_t *ci)
{
	lboard_t	lb ;
	uchar      	bclass, nsclass = 0 ;
    
	bclass = clsnky[find_string(ci->sltstr, clsnky)].key ;
        visit_lboard(get_my_node_slot_class, NULL, (void *)&nsclass) ;

        lb.brd_type   = bclass | ci->slt ;
        lb.brd_module = ci->mod ;
        lb.brd_slot   = (bclass == KLCLASS_IO) ? 
                      (SLOTNUM_XTALK_CLASS|ci->slt) :
                      (nsclass            |ci->slt) ;

	return(get_match_lb(&lb), 0) ;
}

int
check_params_set_mode(cmdinf_t *ci)
{
    if ((ci->mod < 0) || (ci->slt < 0))
	return -1 ;
    if ((ci->lbd = get_brd_klc(ci)) == NULL) {
	ed_errmsg(ci, "Cannot find board.\n") ;
	return -1 ;
    }
    if (ci->cmptyp == CMP_NONE)
	ci->cmdmde |= MODE_SLOT ;
    return 1 ;
}

/*
 * parse_exec
 *     main parse routine for enable/disable arguments
 *     Do sanity checks.
 *     Call the respective functions for CPU, MEM and IO.
 */

int
parse_exec(int argc, char **argv)
{
	int i, fi, rv ;

	/* Check arguments */

	if (argc < 2)                   /* too less */
		return -1 ;

	init_cmdinf(&cmd) ;

	/* Get command type */

	if (argv[0][0] == 'e') {
		cmd.cmdtyp = CMD_ENABLE ;
	}
	else if (argv[0][0] == 'd') {
		cmd.cmdtyp = CMD_DISABLE ;
	}
	else
		cmd.cmdtyp = -1 ;

	strcpy(cmd.cmdstr, argv[0]) ;

	ed_debug 	= 0  ;

	for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
		switch (argv[i][1]) {
			case 'm':
		    		if (check_edarg(argv, argc, i++) < 0)
					return -1 ;
	 	    		if ((cmd.mod = (moduleid_t)
					get_arg_val(argv[i])) < 0)
					return -1 ;
	 			break ;

			case 's': {
		    		char *sp ;
		    		if (check_edarg(argv, argc, i++) < 0)
					return -1 ;
		    		if ((sp = check_slot_arg(argv[i])) == NULL)
					return -1 ;
	 	    		if ((cmd.slt = get_arg_val(sp)) < 0)
					return -1 ;
		    		strcpy(cmd.sltstr, argv[i]) ;
				}
				break ;

			case 'd':
		    		if (check_opt(argv, i) < 0)
					return -1 ;
		    		cmd.cmdmde |= MODE_DEBUG ;
		    		ed_debug = 1 ;
				break ;

			default:
		    		if (cmd.slt < 0)
					return -1 ;
		    		if (prscmp(argv, argc, i, &cmd) < 0)
					return -1 ;
				break ;

	    	} /* switch */
            } /* if */
    	} /* for */

	if (check_params_set_mode(&cmd) < 0)
		return -1 ;

	dump_prsinf(&cmd) ;

	if ((fi = get_cb(&cmd)) < 0)
		return -1 ;

	if ((rv = ((edfunc[fi])(&cmd))) < 0)
		return -1 ;

	return rv ;
}

/* Check if Component type matches with the mask 
   that is set.
 */
int
check_cmp_msk(cmdinf_t *ci)
{
    if (ci->cmptyp == CMP_NONE)
	return 1 ;
    if (!(ci->cmpmsk & cmp_masks[ci->cmptyp]))
        return -1 ;
    if (ci->cmpmsk & ~cmp_masks[ci->cmptyp])
	return -1 ;
    return 1 ;
}

int
check_node_cmp(cmdinf_t *ci)
{
    if (ci->cmptyp == CMP_PCI)
        return -1 ;
    if (check_cmp_msk(ci) < 0)
	return -1 ;

    return 1 ;
}

int
check_io_cmp(cmdinf_t *ci)
{
    if ((ci->cmptyp == CMP_CPU) ||
        (ci->cmptyp == CMP_MEM))
        return -1 ;
    if (ci->slt > MAX_IO_SLOT_NUM)
	return -1 ;
    if (check_cmp_msk(ci) < 0)
	return -1 ;
    return 1 ;
}

/* ----------------------------------------------------------------- */

/*
 * CPU and Memory enable/disable support
 */

int
do_cpu(nasid_t n, char *k, char *s, int f)
{
    int         rv  ;
    char        buf[MAX_PLOG_VARSZ] ;

    rv = 1 ;
    if (f == PLOG_UNSET)
        rv = do_promlog(n, k, buf, PLOG_GET, NULL) ;
    if (rv >= 0)
        rv = do_promlog(n, k, s, f, NULL) ;
    /* XXX - Print status msg here */
    return rv ;
}

/*
 * do_cpus
 * enable/disable CPUs
 */

static void
do_cpus(cmdinf_t *ci)
{
    char 	*k, s[MAX_REASON_BUFSZ] ;
    nasid_t	n   = ci->lbd->brd_nasid ;
    edmask_t	msk = ci->cmpmsk ;
    int		flg ;

    if (ci->cmdtyp == CMD_DISABLE) {
        sprintf(s, "%d: CPU disabled from IO6 POD.", 
    	    KLDIAG_CPU_DISABLED);
	flg = PLOG_SET ;
    } else {
	flg = PLOG_UNSET ;
    }

    if (msk & CMSK_CPUA) {
	k = DISABLE_CPU_A ;
	do_cpu(n,k,s,flg) ;
    }
    if (msk & CMSK_CPUB) {
	k = DISABLE_CPU_B ;
	do_cpu(n,k,s,flg) ;
    }
}

/*
 * enable/disable support for Memory.
 */

/*
 * plog_swapbank
 * Enable or disable the promlog var SwapBank
 */

static int
plog_swapbank(nasid_t n, int b)
{
    char        *k = SWAP_BANK ;
    char        s[MD_MEM_BANKS+1] ;
    int         rv, flg ;

    s[0] = (b ? ('0' + b) : 0) ;
    s[1] = 0 ;

    if (*s)
	flg = PLOG_SET ;
    else
	flg = PLOG_UNSET ;

    rv = do_promlog(n, k, s, flg, NULL) ;

    return rv ;
}

/*
 * change_mem_plog
 *
 * change the memlog struct, values of bank elem
 * to mskval, szeval, rsnval.
 */

edmask_t
change_mem_plog(memlog_t *mlog, int elem, 
	 	int mskval, int szeval, int rsnval)
{
    edmask_t	dismsk = mlog->msk ;

    if (!mskval)
	dismsk &= ~(MASK_BIT << elem) ;
    mlog->sze[elem] = '0' + szeval ;
    /* Do not clobber reason if one already exists */
    if ((rsnval) && (mlog->rsn[elem] == '0'))
    	mlog->rsn[elem] = '0' + rsnval ;
    return dismsk ;
}

edmask_t
check_bank0(cmdinf_t *ci, klmembnk_t  *km, 
	    memlog_t *mlog)
{
    edmask_t    dismsk = mlog->msk ;

    if (ci->cmdtyp == CMD_ENABLE) {
	/* Remove the SwapBank plog variable */
    	plog_swapbank(ci->lbd->brd_nasid, 0) ;
	/* Remove bank 0 from Mask */
	return(change_mem_plog(mlog, 0, 0, 0, 0)) ;
    }

    /* Do not disable Bank 0 if we are 
     * not disabling the entire board  or
     * if bank 1 does not exist or if
     * bank 1 is going to be disabled. 
     */
    if ((!ED_MODE_SLOT(ci)) ||
        (km->membnk_bnksz[1] == 0) ||
	(dismsk & ((edmask_t)1 << 1))) {
	return(change_mem_plog(mlog, 0, 0, 0, 0)) ;
    }
    plog_swapbank(ci->lbd->brd_nasid, 2) ;
    return(change_mem_plog(mlog, 0, 1, 
		(km->membnk_bnksz[0] >> 3), MEM_DISABLE_USR)) ;
}

edmask_t
check_mem_sze(cmdinf_t *ci, memlog_t *mlog)
{
	int		i ;
	klmembnk_t 	*km ;
	lboard_t 	*lb 	= ci->lbd ;

	/* get klconfig for mem */
	km = (klmembnk_t *)find_first_component(lb, KLSTRUCT_MEMBNK) ;
	if (km) {
		if (mlog->msk & MASK_BIT)
	    		mlog->msk = check_bank0(ci, km, mlog) ;

		for (i=1; i < MD_MEM_BANKS; i++) {
	    		/* XXX - implement old 'md_size' */
 			if (km->membnk_bnksz[i] == 0) {
				/* Zero out plog for all banks with no memory */
				mlog->msk = change_mem_plog(mlog, i, 0, 0, 0) ;
	    		}
 	    		else {
				/* Add values for banks found */
				mlog->msk = change_mem_plog(mlog, i, 1, 
				(km->membnk_bnksz[i] >> 3), MEM_DISABLE_USR) ;
	    		}
		}
    	}
	return mlog->msk ;
}

void
get_str_from_msk(edmask_t msk, char *str)
{
    int i, next = 0 ;
    for (i=0; i<MD_MEM_BANKS; i++) {
    	if (msk & ((edmask_t)1 << i))
	    str[next++] = i + '0' ;
    }
    str[next] = 0 ;
}

void
do_mem(cmdinf_t *ci)
{
	char		dm_str[MAX_PLOG_VARSZ],
                	dm_sze[MAX_PLOG_VARSZ],
                	dm_rsn[MAX_PLOG_VARSZ];
	int 		strrv, szerv, rsnrv ;
	edmask_t	argmsk, dismsk ;
	int		flg ;
	nasid_t		n = ci->lbd->brd_nasid ;
	memlog_t	mlog ;

	/* Get the values of 3 strings related to Memory */

	flg = PLOG_GET ;
	szerv = do_promlog(n, DISABLE_MEM_SIZE, 	
		dm_sze, flg, NULL) ;
	if (szerv < 0)
		strcpy(dm_sze, DEFAULT_ZERO_STRING) ;

	rsnrv = do_promlog(n, DISABLE_MEM_REASON, 	
		dm_rsn, flg, NULL) ;
	if (rsnrv < 0)
		strcpy(dm_rsn, DEFAULT_ZERO_STRING) ;

	strrv = do_promlog(n, DISABLE_MEM_MASK, 	
		dm_str, flg, NULL) ;

	/* Convert the mask to bit map */
	dismsk = get_num_msk(dm_str) ;
	/* Convert the given argument to a bit map */
	argmsk = ci->cmpmsk >> CMSK_MEMBNK_SHIFT ;
	/* Combine the 2 to get resultant mask */
	if (ci->cmdtyp == CMD_DISABLE)
        	dismsk |= argmsk ;
	else if (ci->cmdtyp == CMD_ENABLE)
		dismsk &= (~argmsk) ;

	/* Fill up a mem prom log struct */
	mlog.str = dm_str ;
	mlog.sze = dm_sze ;
	mlog.rsn = dm_rsn ;
	mlog.msk = dismsk ;

	/* Handle bank 0 special processing. */
	/* remove bogus bank nos, if the bank is not present. */
	dismsk = check_mem_sze(ci, &mlog) ;

	/* convert mask back to string */
	get_str_from_msk(dismsk, dm_str) ;
	if (*dm_str)
		flg = PLOG_SET ;
	else
		flg = PLOG_UNSET ;

	/* set the memory related strings */

	if ((flg == PLOG_UNSET) && (szerv >= 0))
        	do_promlog(n, DISABLE_MEM_SIZE,     dm_sze, flg, NULL) ;
	if ((flg == PLOG_UNSET) && (rsnrv >= 0))
        	do_promlog(n, DISABLE_MEM_REASON,   dm_rsn, flg, NULL) ;
	if ((flg == PLOG_UNSET) && (strrv >= 0))
        	do_promlog(n, DISABLE_MEM_MASK,     dm_str, flg, NULL) ;
}

/*
 * Support for disabling individual boards.
 */

int
enable_node(cmdinf_t *ci)
{
	if (disable_node(ci) < 0)
		return -1 ;
	return 1 ;
}

int
disable_node(cmdinf_t *ci)
{
    if (check_node_cmp(ci) < 0)
        return -1 ;
    if (ED_MODE_SLOT(ci)) {
	/* disable_node_brd(ci) ; */
	ed_errmsg(ci, "disable_node_brd To Be Implemented.\n") ;
    }
    if (ci->cmptyp == CMP_CPU)
	do_cpus(ci) ;
    if (ci->cmptyp == CMP_MEM)
	do_mem(ci) ;
    return 1 ;
}

int
enable_mb(cmdinf_t *ci)
{
    if (check_node_cmp(ci) < 0)
        return -1 ;
    return 1 ;
}

int
disable_mb(cmdinf_t *ci)
{
    if (check_node_cmp(ci) < 0)
        return -1 ;
    return 1 ;
}


/* ------------------------------------------------------------ */

/*
 * Support for IO enable/disable.
 */

int
disable_io(cmdinf_t *ci)
{
    int rv ;
    if (check_io_cmp(ci) < 0)
        return -1 ;
    rv = write_edtab(ci) ;
    return rv ;
}

int
enable_io(cmdinf_t *ci)
{
    int rv ;
    if ((rv = disable_io(ci)) < 0)
        return -1 ;
    return rv ;
}

#endif /* NEW_ED_CMD */

static int
do_promlog(nasid_t n, char *k, char *s, int flg, char *d)
{
    int rv = 0 ;
    if (flg == PLOG_GET) {
	if (s)
	    rv = ip27log_getenv(n, k, s, d, 0) ;
	return rv ;
    }
    if (ed_debug) {
	printf("%s %s on nasid(%d) - string %s\n",
	(flg == PLOG_SET ? "Set" : "Unset"),
	k, n, s) ;
	return rv ;
    }
    if (flg == PLOG_SET)
	rv = ip27log_setenv(n, k, s, IP27LOG_VERBOSE) ;
    else if (flg == PLOG_UNSET)
	rv = ip27log_unsetenv(n, k, IP27LOG_VERBOSE) ;
    return rv ;
}

void
prarr(char *a, int cnt)
{
    int j ;
    for (j = 0; j < cnt; j++)
        db_printf("%x", a[j]) ;
    db_printf("\n") ;
}

void
get_state_from_msk(cmdinf_t *ci, char *str)
{
    int 	i ;
    klinfo_t	*k ;
    lboard_t	*lb = ci->lbd ;
    edmask_t	dismsk = (ci->cmpmsk >> CMSK_PCI_SHIFT) ;

    for (i = 0; i < KLCF_NUM_COMPS(lb); i++) {
	k = KLCF_COMP(lb, i) ;
	if (KLCF_COMP_TYPE(k) == KLSTRUCT_BRI)
	    continue ;
	if (dismsk & (MASK_BIT << k->physid))
	    str[i] = (DISABLE(ci))?
			NVEDRSTATE_DISABLE:NVEDRSTATE_ENABLE ;
    }
    str[i] = 0 ;
}

/* ---------------------------------------------------------------*/

/* 
 * ed - enable, disable table management routines
 * The nvram has 4K of space reserved for INVENTORY INFO. 
 * This space will be used to maintain a enable/disable
 * table for the system.
 * The edtab - ed table - is a sequence of records
 * indexed on a moduleid slotid key. Each record in 
 * this area represents a 'disabled' board. There can 
 * holes in this sequence when a previously disabled
 * board is enabled.
 * This scheme uses structures instead of individual
 * nvram offsets for each field. This makes programming
 * easier, but may result in loss of space due to struct
 * alignment and difficult to write individual bytes.
 */

static void
predh(edtabhdr_t *edh)
{
    db_printf("magic = %x	valid = %x	cnt = %d	nic = %x\n",
            edh->magic, edh->valid, edh->cnt, edh->nic) ;
}

static void
predr(edrec_t *edr, int i)
{
    db_printf(" %d -> 	%x 	%d 	%x	",
            i, edr->flag, edr->modid, edr->slotid) ; 
    prarr((char *)edr->state, 8) ;
}

/*
 * update_edinfo
 *     update lboard with disable info
 *     if board is disabled do not check components.
 */

static int
update_edinfo(lboard_t *l, void *ap, void *rp)
{
    edrec_t	*edr 	= (edrec_t *)ap ;
    klinfo_t	*k ;
    int		i ;

    /* If it is not our brd, just return */
    if ((edr->modid  	!= l->brd_module) ||
	(edr->slotid  	!= l->brd_slot))
	return 1 ; /*continue to search */

    if (l->brd_flags & GLOBAL_MASTER_IO6)
	return 1 ;

    for (i = 0; i < KLCF_NUM_COMPS(l); i++) {
	k = KLCF_COMP(l, i) ;
	if (edr->state[i] == NVEDRSTATE_DISABLE) { /*disabled */
	    db_printf("disabling compt, m = %d, slot = %x id = %d\n",
			   l->brd_module, l->brd_slot, i) ;
	    if (!ed_debug)
		k->flags &= ~KLINFO_ENABLE ;
	} else if (edr->state[i] == NVEDRSTATE_ENABLE) {
	    db_printf("enabling compt, m = %d, slot = %x id = %d\n",
			   l->brd_module, l->brd_slot, i) ;
	    if (!ed_debug)
		k->flags |= KLINFO_ENABLE ;
	}
    }

    if (edr->flag & NVEDREC_BRDDIS) {
        if (!ed_debug)
            l->brd_flags &= ~ENABLE_BOARD ;
        db_printf("disabling board, m = %d, slot = %x\n",
                   l->brd_module, l->brd_slot) ;
    } else {
        if (!ed_debug)
            l->brd_flags |= ENABLE_BOARD ;
        db_printf("enabling board, m = %d, slot = %x\n",
                   l->brd_module, l->brd_slot) ;
    }

    return 0 ; 
}

edtabhdr_t *
get_edh(char *buf)
{
    cpu_get_nvram_buf(NVOFF_INVENTORY, sizeof(edtabhdr_t), buf) ;
    return ((edtabhdr_t *)buf) ;
}

edtabhdr_t *
put_edh(char *buf)
{
    cpu_set_nvram_offset(NVOFF_INVENTORY, sizeof(edtabhdr_t), buf) ;
    return ((edtabhdr_t *)buf) ;
}

edrec_t *
get_edr(int num, char *buf)
{
    cpu_get_nvram_buf(NVOFF_EDREC + (num*sizeof(edrec_t)),
                        sizeof(edrec_t), buf) ;
    return ((edrec_t *)buf) ;
}

edrec_t *
put_edr(int num, char *buf)
{
    cpu_set_nvram_offset(NVOFF_EDREC + (num*sizeof(edrec_t)),
                                sizeof(edrec_t), buf) ;
    return ((edrec_t *)buf) ;
}

/*
 * init_edh
 *     init ed header with defaults
 */

static void
init_edh(edtabhdr_t *edh)
{
    int i ;

    /* clear the nvram inventory area */

    for (i=NVOFF_EDHDR;i<NVOFF_EDHDR+NVLEN_EDHDR;i++)
        set_nvreg(i,0) ;

    edh->magic = NVEDTAB_MAGIC ;
    edh->valid = NVEDTAB_VALID ;
    edh->cnt   = 0 ;
    edh->nic   = get_sys_nic() ;
    db_printf("get_sys_nic: sys nic %lx\n", edh->nic) ;
    put_edh((char *)edh) ;
    update_checksum();

    if (ed_debug) {
        get_edh((char *)edh) ;
        db_printf("new header written\n") ;
        predh(edh) ;
    }
}

int
check_edh(cmdinf_t *ci, edtabhdr_t *edh)
{
    if ((edh->magic != NVEDTAB_MAGIC) ||
        (!(edh->valid))) {
	printf("Enable/Disable table not setup in NVRAM\n") ;
	return -1 ;
    }
    if (!check_sys_nic(edh->nic)) {
	printf("The NVRAM did not previously belong to this system.\n") ;
	return -1 ;
    }

    if (ci) {
        if ((ci->cmdtyp == CMD_ENABLE) && (!edh->cnt)) {
            ed_errmsg(ci, "Nothing disabled.\n") ;
            return -1 ;
        }
	if (ci->lbd->brd_flags & GLOBAL_MASTER_IO6) {
	    printf("Cannot %s the Master BASEIO.\n",
			ci->cmdstr) ;
	    return -1 ;
	}
    }

    return 1 ;
}

/*
 * check_edtab
 *     Check all entries in the table and update the
 *     klconfig info, disabling components/boards.
 * Called:
 *     during io6prom, after gda is ready, before
 *     init_prom_soft, after nvram_base is initted.
 *     If the edtab is not initted, init it first.
 */

void check_pci_compt(edrec_t *, int) ;

int
check_edtab1(int flag)
{
    edtabhdr_t	*edh, edhs ;
    edrec_t	edrs, *edr ;
    int 	i, j ;

    db_printf("Magic offset : %x\n", NVOFF_EDHDR) ;

    edh = &edhs ;
    /* check header validity. If invalid, write a new header */
    get_edh((char *)edh) ;

    predh(edh) ;

    if (check_edh(NULL, edh) < 0) {
        db_printf("Header not valid. Re-initializing ... \n") ;
        /* Might have trashed it while writing */
        init_edh(edh) ;
	return 0 ;
    }

    /* Scan all records and update klconfig */

    db_printf("check_edtab: num entries %d\n", edh->cnt) ;

    edr = &edrs ;
    for (i=0; i<edh->cnt; i++) {
	get_edr(i, (char *)edr) ;
        predr(edr, i) ;
	if (!(edr->flag & NVEDREC_VALID))
	    continue ;
	if (flag)
		check_pci_compt(edr, i) ;
	else
		visit_lboard(update_edinfo, (void *)edr, NULL) ;
    }
    return 1 ;
}

void
change_edh(edtabhdr_t *edh, int vld)
{
    if (vld == NVEDTAB_VALID)
        edh->valid |= NVEDTAB_VALID ;
    else if (vld == NVEDTAB_INVALID)
        edh->valid &= ~NVEDTAB_VALID ;
    put_edh((char *)edh) ;
}

int
find_edr_slot(cmdinf_t *ci, edrinf_t *ei, edtabhdr_t *edh)
{
    int 	i ;
    edrec_t	dummmy ; /* alignment ?? */
    edrec_t	edrec, *edr = &edrec ;
    lboard_t	*l = ci->lbd ;

    ei->cnt = edh->cnt ;

    /* Look at all records for a free slot
     * or a matching record.
     */
    for (i=0; i<edh->cnt; i++) {
	get_edr(i, (char *)edr) ;
  	predr(edr, i) ;
        if (!(edr->flag & NVEDREC_VALID)) {
	    ei->inv = i ;
	    continue ;
	}
        if ((l->brd_module == edr->modid) &&
            (l->brd_slot   == edr->slotid)) {
	    ei->mtc = i ;
	    break ;
	}
    }

    /* determine which record to use depending
     * on various conditions.
     */

    /* If the command is enable we need a match */

    if (ENABLE(ci)) {
        if (ei->mtc < 0) {
            ed_errmsg(ci, "Not disabled.\n") ;
	    return -1 ;
        } else {
	    ei->use = ei->mtc ;
	    goto found_use ;
        }
    }

    /* Did we reach the end without finding
     * a empty slot? If we found a match
     * we will never reach the end.
     */
    if ((i == edh->cnt) && (ei->inv < 0)) {
	ei->end = i ;
	/* It is worthwhile mentioning here that
	 * we wont be here if a match was found
	 * and if a invalid rec was found. So
	 * the use field will not be overwritten.
	 */
	ei->use = ei->end ;
	ei->cnt ++ ;
	goto found_use ;
    }

    /* If the command is disable, try to use a 
     * matching slot first. If there is none then
     * use the first invalid record.
     */
    if (DISABLE(ci)) {
        if (ei->mtc >= 0)
            ei->use = ei->mtc ;
        else if (ei->inv >= 0)
            ei->use = ei->inv ;
    }

found_use:
    db_printf("find_slot: found %d\n", ei->use) ;
    return ei->use ;
}

edmask_t
get_msk_from_state(char *state)
{
    edmask_t 	msk = 0 ;
    int 	i ;

    for (i = 0; i < MAX_COMPTS_PER_BRD; i++)
	if (state[i] == NVEDRSTATE_DISABLE)
	    msk |= MASK_BIT << i ;

    return(msk) ;
}

void
make_new_edr(cmdinf_t *ci, edrinf_t *ei, edrec_t *edr)
{
    lboard_t 	*l = ci->lbd ;
    edmask_t	msk, savmsk ;

    savmsk = ci->cmpmsk ;
    /* get the record if a match was found */
    if (ei->mtc >= 0)	
	get_edr(ei->use, (char *)edr) ;
    /* Else we have a clean slate */

    edr->modid  = l->brd_module ;
    edr->slotid = l->brd_slot ;
    if (ED_MODE_SLOT(ci)) {
	ci->cmpmsk = CMSK_PCI ;
    }
    get_state_from_msk(ci, (char *)edr->state) ;
    ci->cmpmsk = savmsk ;

    if (ENABLE(ci)) {
        msk = get_msk_from_state((char *)edr->state) ;
	if (!msk) {
            edr->flag &= ~(NVEDREC_BRDDIS | NVEDREC_VALID) ;
            if (ei->use == (ei->cnt - 1)) /* last record */
                ei->cnt -- ;
	}
    } else if (DISABLE(ci)) {
        edr->flag |= NVEDREC_VALID ;
        if (ED_MODE_SLOT(ci)) {
            edr->flag |= NVEDREC_BRDDIS ;
	}
    }
}

int
check_cnt(int edhcnt)
{
    int i ;
    edrec_t	edrec, *edr = &edrec ;

    for (i = 0; i < edhcnt; i++) {
	get_edr(i, (char *)edr) ;
	if (edr->flag & NVEDREC_VALID)
	    return 1 ;
    }
    return 0 ;
}

/*
 * write_edtab
 *     Write a ed record on user's request.
 */

static int
write_edtab(cmdinf_t *ci)
{
    edtabhdr_t	*edh, edhs ;
    edrec_t	*edr, edrs ;
    int 	i, j, new = 0 ; 
    lboard_t 	*l = ci->lbd ;
    edrinf_t	edrinf = {-1, -1, -1, -1, -1} ;

    /* Get the header */
    edh = get_edh((char *)&edhs) ;
    predh(edh) ;

    /* check for validity */
    if (check_edh(ci, edh) < 0)
	return -1 ;

    /* invalidate header before writing */
    change_edh(edh, NVEDTAB_INVALID) ;

    /* scan for the first free slot or go to the end */
    if ((new = find_edr_slot(ci, &edrinf, edh)) < 0)
	goto validate_hdr ;

    edr = &edrs ;
    bzero((char *)edr, sizeof(edrec_t)) ;

    /* fill up a edr record in memory */
    make_new_edr(ci, &edrinf, edr) ;

    /* XXX write klconfig ??? */
    /* Yes. The kernel needs to know about disabled compts now! */
    update_edinfo(l, (void *)edr, NULL) ;

    predr(edr, new) ;

    edh->cnt = edrinf.cnt ;

    put_edr(new, (char *)edr) ;

validate_hdr:
    /* Check if no records are present */
    if (!check_cnt(edh->cnt))
        edh->cnt = 0 ;
    change_edh(edh, NVEDTAB_VALID) ;
    predh(edh) ;

    return 0 ;
}
#endif /* __ED1_C__ */


/* Support for dealing with ALL Disabled devices */

#include <sn_macros.h>

int 	yes_flag, list_flag ;

int
confirm_user(void)
{
	char 	buf[256] ;

	while (1) {
		printf("Enable? (y|n) [y]:") ;
		gets(buf) ;
                if ((buf[0] == '\0') || (buf[0] == 'y'))
			return 1 ;
		else
			return 0 ;
	}
}

void
check_cpu(nasid_t n)
{
	int		i ; 
	char		slice ;
	char		val[MAX_REASON_BUFSZ], label[MAX_PLOG_VARSZ] ;
	char		reason[MAX_REASON_BUFSZ] ;

	ForAllCpusPerSN0Nasid(i) {
		slice = 'A' + i ;
		sprintf(label, "Disable%c", slice) ;
		if (do_promlog(n, label, val, PLOG_GET, NULL) < 0)
			continue ;
		if (!yes_flag)
			printf("Nasid %d: CPU %c disabled. Reason: %s\n", 
				n, slice, val) ;
		if (list_flag)
			continue ;
		if (!yes_flag && (!confirm_user()))
			continue ;
		if (do_promlog(n, label, NULL, PLOG_UNSET, NULL) <0)
			continue ;
	}
}

#define LIST 		1
#define CONFIRM_ENABLE 	2

char *
map_mem_dis_reason(char r)
{
	switch (r) {
	   case MEM_DISABLE_USR:
		return get_diag_string(KLDIAG_MEMORY_DISABLED);
	   case MEM_DISABLE_AUTO:
		return get_diag_string(KLDIAG_MEMTEST_FAILED);
	   case MEM_DISABLE_256MB:
		return get_diag_string(KLDIAG_IP27_256MB_BANK);
	   default:
	   	return ("Unknown\n") ;
	}
}

void
do_mem_banks(char *mb, char *size, char *reason, int flag, int *bank0)
{
	char	*buf = mb ;
	int	i = 0, j = 0 ;
	char	newmsk[MAX_PLOG_VARSZ] ;

	while (buf[i]) {
		if (!yes_flag)
			printf("    Bank %c: Reason: %s\n", buf[i], 
				map_mem_dis_reason(reason[buf[i]-'0']-'0')) ;
		if (flag == LIST)
			goto incr ;
		if (!yes_flag && (!confirm_user())) {
			newmsk[j++] = buf[i] ;
		}
		else {
			if (buf[i] == '0')
				*bank0 = 1 ;
			size[buf[i]-'0'] = '0' ;
			reason[buf[i]-'0'] = '0' ;
		}
incr:
		i++ ;
	}
	newmsk[j] = 0 ;
	strcpy(mb, newmsk) ;
}

void
check_mem(nasid_t n)
{
	int		i ;
        char            val[MAX_REASON_BUFSZ] ;
	char		mask[MAX_PLOG_VARSZ] ;
	char		reason[MAX_REASON_BUFSZ] ;
	char		size[MAX_REASON_BUFSZ] ;
	int		flg = 0 ;

	/* Get list of disabled banks */
        if (do_promlog(n, DISABLE_MEM_MASK, val, PLOG_GET, NULL) < 0)
                return ;

	if (!yes_flag)
        	printf("Nasid %d: MEM disabled ... \n", n) ;

	/* Get the other 2 assoc strings */
        if (do_promlog(	n, DISABLE_MEM_SIZE, 
			size, PLOG_GET, DEFAULT_ZERO_STRING) < 0)
		*size = 0 ;
        if (do_promlog(	n, DISABLE_MEM_REASON, 
			reason, PLOG_GET, DEFAULT_ZERO_STRING) < 0)
		*reason = 0 ; 

        if (list_flag)
		do_mem_banks(val, size, reason, LIST, &flg) ;
        else {
        	do_mem_banks(val, size, reason, CONFIRM_ENABLE, &flg) ;
		/* if flg is set, we are dealing with bank 0 */
		if (flg)
                        do_promlog(     n, SWAP_BANK,
                                        NULL, PLOG_UNSET, NULL) ;

		if (*val)
			flg = PLOG_SET ; 	/* flg re-used */
		else
			flg = PLOG_UNSET ;

                do_promlog(n, DISABLE_MEM_SIZE, 	size, flg, NULL) ;
                do_promlog(n, DISABLE_MEM_REASON, 	reason, flg, NULL) ;
                do_promlog(n, DISABLE_MEM_MASK, 	val, flg, NULL) ;
	}
}

void
check_pci_compt(edrec_t *edr, int indx)
{
	char		hwg[MAX_HWG_PATH_LEN] ;
	int		i ;
	unsigned char	*state ;
	edmask_t	msk ;
	edtabhdr_t  	*edh, edhs ;

	state = edr->state ;
	make_hwg_path(edr->modid, edr->slotid, hwg) ;
	/* 
	 * The +1 is for the bridge element. Each item
	 * of the state arr represents a XIO compt in klconfig.
	 */
	for(i=0; i< (MAX_PCI_DEVS + 1); i++) {
		if (state[i] == NVEDRSTATE_DISABLE) {
			if (!yes_flag)
				printf("%s, PCI id %d disabled\n", 
					hwg, i-1) ;
                	if (list_flag)
                        	continue ;
                	if (!yes_flag && (!confirm_user()))
                        	continue ;
			state[i] = NVEDRSTATE_ENABLE ;
		}
	}

        msk = get_msk_from_state((char *)edr->state) ;
        if (!msk) {	/* No component disabled. Free entry */
		edh = get_edh((char *)&edhs) ;
		change_edh(edh, NVEDTAB_INVALID) ;
		edh->cnt -= 1 ;
		if (!check_cnt(edh->cnt))
        		edh->cnt = 0 ;
		change_edh(edh, NVEDTAB_VALID) ;
	}
	else {		/* Put back new entry */
		put_edr(indx, (char *)edr) ;
	}
}

void
check_pci()
{
	/* 
	 * check enable disable table. Calls check pci compt for
	 * each table entry.
	 */
	check_edtab1(1) ;
}

/* Check functions that need to be called once per system. */

void (*CheckEdFunc[])() = {
	check_pci,
	NULL,
} ;

/* Check functions that need to be called once per nasid. */

void (*CheckEdFuncNasid[])(nasid_t) = {
	check_cpu,
	check_mem,
	NULL,
} ;

/*
 * Procedure 	: 	enableall_cmd
 * Descprition  : 	Called from command line.
 * 			For all nasids scan all disablable components.
 *			List or enable them with confirmation
 *			if they are disabled.
 *			-y overrides confirmation.
 *			For PCI devices just run thro the database once.
 */

int
enableall_cmd(int argc, char **argv)
{
        cnodeid_t       cnode ;
        nasid_t         nasid ;
	int		i ;
	gda_t		*g ;

	list_flag = yes_flag = 0 ;

	if (argc > 1) {
		if (!strcmp(argv[1], "-list"))
			list_flag = 1 ;
		else if (!strcmp(argv[1], "-y"))
			yes_flag = 1 ; 
	}

	/* gdap is assigned to g */

	if (!CHECK_GDA(g))
		return 1 ;

	ForAllValidNasids(cnode, nasid, g) {
		ForAllEdComponents(i) {
			(*CheckEdFuncNasid[i])(nasid) ;
		}
	}

	i = 0 ;
	while (CheckEdFunc[i]) {
		(*CheckEdFunc[i++])() ;
	}

	return 0 ;
}






 

