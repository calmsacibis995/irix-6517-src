/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: error_dump.c
 *	This file contains SN0 specific handling of errors.
 */


#include <sys/types.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/slotnum.h>

#if defined (_STANDALONE)
#include <stdarg.h>
#include <libkl.h>
#include <sys/SN/gda.h>
#else /* !_STANDALONE */
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include "sn0_private.h"
#endif /* _STANDALONE */
#include <sys/idbgentry.h>
#include <sys/R10k.h>

#if defined (_STANDALONE)
extern vsprintf(char *, char *, va_list);
#define bcopy(a,b,len) memcpy(b,a,len)
#define bzero(a, len) memset(a, 0, len)
#endif /* _STANDALONE */


/*
 * PLEASE READ:
 * This file will be shared  with the standalone code. ANY changes
 * to this file needs to keep in mind a couple of things:
 * 1) A change here should not necessitate shipping a new prom revision.
 * 2) A change here should be tested for both the kernel and standalone code.
 * 3) If this file is changed, please change the file in the 
 *	kernel/standalone tree as well.
 */

/* 
 * This file is for error saving and displaying the hardware error status. 
 * This typically is called when the machine is  about to be brought down 
 * due to some errors.
 *
 * The display routines display a message per bit set in the register, in 
 * most cases. However, there are several cases where a set of bits in the
 * register hold a value and that value needs to be displayed.
 * The reg_desc structure used by cmn_err seems especially approriate to 
 * for displaying these, but there are various places where bits within a
 * register mean different things depending on other bits within the register
 * or even other registers.
 *
 * Hence, some of the displaying of error messages are done by the individual
 * show_error_xxx routines. If this gets too ugly, we will probably come
 * up with something better.
 */


/**************************************************************************
 * 		File private declarations and defines			  *
 **************************************************************************/

#define NONE_LVL	0
#define NODE_LVL	1
#define SLOT_LVL	2
#define ASIC_LVL	3
#define REG_LVL		4
#define BIT_LVL		5

#define KL_FINAL_MSG	0x10
#define KL_LOG_MSG	0x20

#define PERR_LEVELS	BIT_LVL + 1
#define	NEST_DEPTH	2

#define DMPBUF_SIZE 16384
#define TMPBUF_SIZE 256

struct err_dmp_buf {
    int		dmpndx;
    char	dmpbuf[DMPBUF_SIZE];
    char        *levelbuf[PERR_LEVELS];
    char	line[PERR_LEVELS][TMPBUF_SIZE]; /* one per level */
    char	msg[TMPBUF_SIZE];
};

#if !defined (_STANDALONE)
struct err_dmp_buf err_buf;

#define ERR_BUF		(&err_buf)
#else

#define ERR_BUF		((struct err_dmp_buf *) \
			 TO_NODE(get_nasid(), IP27PROM_ERRDMP))

#endif

#define EBUF_MSG	(ERR_BUF->msg)
#define EBUF_LINE(_x)	(ERR_BUF->line[(_x)])
#define EBUF_DUMP_NDX	(ERR_BUF->dmpndx)
#define EBUF_DUMP(_x)	(&(ERR_BUF->dmpbuf[(_x)]))
#define EBUF_LEVADDR(_x)	(&(ERR_BUF->levelbuf[(_x)]))


/*
 * Function	: kl_perr
 * Parameters	: level   -> message level
 *		  format  -> format string
 *		  ...     -> varargs.
 * Purpose	: to save an information string in a reserved buffer.
 * Assumptions  : 
 * Returns	: None.
 */

void
kl_perr(int level, char *fmt, ...)
{
    va_list ap;
    char 	*line;
    char	**level_str;
    int 	i, len;
    int final, log;
    int safe_len;
    
    va_start(ap, fmt);
    vsprintf(EBUF_MSG, fmt, ap);
    va_end(ap);
    if (final = (level & KL_FINAL_MSG))
	level &= ~KL_FINAL_MSG;
    if (log = (level & KL_LOG_MSG))
	level &= ~KL_LOG_MSG;

    if (level == BIT_LVL) 	final = 1;
    
    line = EBUF_LINE(level);

    *line++ = '!';

    if (level) {
	*line++ = '+';
	for (i = 0; i < level*NEST_DEPTH; i++)
	    *line++ = ' ';
    }
    
    safe_len = (strlen(EBUF_LINE(level)) + strlen(EBUF_MSG) < TMPBUF_SIZE -1 ) ?
        strlen(EBUF_MSG) : (TMPBUF_SIZE - 1 - strlen(EBUF_LINE(level)));
    strncpy(line, EBUF_MSG, safe_len);
    *(line+safe_len) = '\0';

    level_str = EBUF_LEVADDR(level);
    *level_str = NULL;
    
    if (!final) return;
    
    for (i = 0; i <= level; i++) {
	if (len = strlen(EBUF_LINE(i))) {
	    level_str = EBUF_LEVADDR(i);
	    *level_str = EBUF_DUMP(EBUF_DUMP_NDX);
	    safe_len = (len + 1 + EBUF_DUMP_NDX < DMPBUF_SIZE ) ?
                len : DMPBUF_SIZE - EBUF_DUMP_NDX - 1;
            bcopy (EBUF_LINE(i),
                   EBUF_DUMP(EBUF_DUMP_NDX), safe_len + 1);
            EBUF_DUMP_NDX += safe_len;
	    *(EBUF_LINE(i) + 0) = '\0';
	}
    }
    if (!log) {
	for (i = 0; i < PERR_LEVELS; i++) {
	    level_str = EBUF_LEVADDR(i);
	    if (*level_str) **level_str = ' ';
	    *level_str = NULL;
	}
    }
}


void
kl_perr_dumpbuf(void (*prfunc)(), int arg)
{
    char *dmpbuf = EBUF_DUMP(0);
    char oldchar, *line;

    line = dmpbuf;    
    while (*line && (line < EBUF_DUMP(EBUF_DUMP_NDX))) {
	while (*line != '\0' && (*line++ != '\n'))
	    ;
	oldchar = *line;
	*line = '\0';
	if (arg)
	    (*prfunc)(arg, dmpbuf);
	else {
	    if (*dmpbuf != '!')
		(*prfunc)(dmpbuf);
	}
	*line = oldchar;
	dmpbuf = line;
    }
    EBUF_DUMP_NDX = 0;
}

static unsigned char
brd_sversion(nasid_t nasid, int brd_type)
{
	lboard_t	*l;

	l = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid), brd_type);

	if (l && (l->brd_nasid == nasid))
		return l->brd_sversion;
	else
		return 2;
}

void
save_cache_error(unsigned int cerr_reg, __uint64_t tag0, __uint64_t tag1)
{
    lboard_t *klconf;
    nasid_t  nasid;
    klinfo_t *component;
    int comp;
    klcpu_err_t *cpu_err;

    nasid	= get_nasid();
    klconf = (lboard_t *)KL_CONFIG_INFO(nasid);

    while (klconf) {
	if (KLCF_CLASS(klconf) == KLCLASS_NODE) break;
	klconf = KLCF_NEXT(klconf);
    }
	
    if (!klconf)
	return;

    for (comp = 0; comp < KLCF_NUM_COMPS(klconf); comp++) {
	component = KLCF_COMP(klconf, comp);
	if (KLCF_COMP_TYPE(component) == KLSTRUCT_CPU) {
	    if ((HUB_REG_PTR_L(LOCAL_HUB_ADDR(0), PI_CPU_NUM)) !=
		((klcpu_t *)component)->cpu_info.physid)
		continue;
	    cpu_err = CPU_COMP_ERROR(klconf, component);
	    cpu_err->ce_cache_err_log.ce_cache_err = cerr_reg;
	    cpu_err->ce_cache_err_log.ce_tags[0] = tag0;
	    cpu_err->ce_cache_err_log.ce_tags[1] = tag1;

	    cpu_err->ce_info.ei_time = ERROR_TIMESTAMP;
	    cpu_err->ce_valid = 1;
	    break;
	}
    }
}


/*
 * Function	: kl_save_hw_state_node
 * Parameters	: nasid -> node whose error info is to be saved.
 *		: flag  -> error structures to be used.
 * Purpose	: saves all known error registers across the entire system
 * Returns	: None.
 */

void
kl_save_hw_state_node(nasid_t nasid, edump_param_t *edp)
{
    lboard_t *klconf;

    /*
     * If the nasid is a special nasid, the caller intends to save state
     * of the current node.
     */
    if (nasid == INVALID_NASID)
	nasid = get_nasid();

    klconf = (lboard_t *)KL_CONFIG_INFO(nasid);
    while (klconf) {

	if (KLCF_REMOTE(klconf)) {
	    (*edp->dp_printf)
		("Warning: fixme() kl_dmp_hw_state: remote board in klconfig");

	    klconf = KLCF_NEXT(klconf);
	    continue;
	}
	if (KL_CONFIG_DUPLICATE_BOARD(klconf)) {
	    klconf = KLCF_NEXT(klconf);
	    continue;
	}

	switch (KLCF_CLASS(klconf))  {
	case KLCLASS_NODE:
	    save_err_node_comp(nasid, klconf, edp);
	    break;
	case KLCLASS_IO:
	    save_err_io_comp(nasid, klconf, edp);
	    break;
	case KLCLASS_ROUTER:
	    save_err_router_comp(nasid, klconf, edp);
	    break;
	case KLCLASS_MIDPLANE:
	    save_err_xbow_comp(nasid, klconf, edp);
	    break;				
	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: kl_dmp_hw_state: Unknown class 0x%x in klconf\n", 
		 KLCF_CLASS(klconf));
#endif /* ERR_DEBUG */
	    break;
	}
	klconf = KLCF_NEXT(klconf);
    }
}

/*
 * Function	: kl_save_hw_state
 * Parameters	: None.
 * Purpose	: saves all known error registers across the entire system
 * Returns	: None.
 */

void
kl_save_hw_state_all(edump_param_t *edp)
{
    int cnode;
#if defined (_STANDALONE)
    int i;
    pcfg_hub_t *hub_cf;
    gda_t *gdap = GDA;
    nasid_t nasid;
    partid_t partition = PCFG(get_nasid())->array[0].hub.partition;
#endif /* _STANDALONE */

    /*
     * loop through all the nodes known to this partition.
     */
#if !defined (_STANDALONE)
    for (cnode = 0; cnode < numnodes; cnode++) 
	kl_save_hw_state_node(COMPACT_TO_NASID_NODEID(cnode), edp);
#else  /* !_STANDALONE */

    if ((gdap) && (gdap->g_magic == GDA_MAGIC) &&
	(gdap->g_nasidtable[1] != INVALID_NASID)) {
	for (i = 0; i < MAX_COMPACT_NODES; i++) {
	    if ((nasid = gdap->g_nasidtable[i]) == INVALID_NASID)
		break;
	    kl_save_hw_state_node(nasid, edp);
	}
	return;
    }

    kl_save_hw_state_node(get_nasid(), edp);
	if(PCFG(get_nasid())->magic != PCFG_MAGIC)
		return;
    for (i = 0; i < PCFG(get_nasid())->count; i++) {
	    hub_cf = &PCFG_PTR(get_nasid(), i)->hub;
	    if (hub_cf->type != PCFG_TYPE_HUB) continue;
	    
	    if ((hub_cf->nasid != INVALID_NASID) && 
		(hub_cf->nasid != get_nasid()) &&
                (hub_cf->partition == partition)) 
		kl_save_hw_state_node(hub_cf->nasid, edp);

    }
#endif /* !_STANDALONE */
}

void
restore_cpu_err(klcpu_err_t *cpu_err)
{
    if (cpu_err->ce_valid) {
	if ((ERROR_TIMESTAMP - cpu_err->ce_info.ei_time) < ERROR_DISCARD_TIME)
	    cpu_err->ce_cache_err_dmp = cpu_err->ce_cache_err_log;
	else
	    cpu_err->ce_valid = 0;
    }
}


void
restore_hub_err(klhub_err_t *klhub_err)
{
    int i;

    hub_error_t *hub_dmp = HUB_ERROR_STRUCT(klhub_err, KL_ERROR_DUMP);
    hub_error_t *hub_log = HUB_ERROR_STRUCT(klhub_err, KL_ERROR_LOG);

    if ((hub_dmp->hb_md.hm_mem_err == 0) && (hub_log->hb_md.hm_mem_err)) {
	if ((ERROR_TIMESTAMP - hub_log->hb_md.hm_mem_err_info.ei_time) 	
	    < ERROR_DISCARD_TIME)
	    hub_dmp->hb_md.hm_mem_err = hub_log->hb_md.hm_mem_err;
    }
    if ((hub_dmp->hb_md.hm_dir_err == 0) && (hub_log->hb_md.hm_dir_err)) {
	if ((ERROR_TIMESTAMP - hub_log->hb_md.hm_dir_err_info.ei_time) 	
	    < ERROR_DISCARD_TIME)
	    hub_dmp->hb_md.hm_dir_err = hub_log->hb_md.hm_dir_err;
    }
    if ((hub_dmp->hb_md.hm_proto_err == 0) && (hub_log->hb_md.hm_proto_err)) {
	if ((ERROR_TIMESTAMP - hub_log->hb_md.hm_proto_err_info.ei_time) 
	    < ERROR_DISCARD_TIME)
	    hub_dmp->hb_md.hm_proto_err = hub_log->hb_md.hm_proto_err;
    }
    if ((hub_dmp->hb_md.hm_misc_err == 0) && (hub_log->hb_md.hm_misc_err)) {
	if ((ERROR_TIMESTAMP - hub_log->hb_md.hm_misc_err_info.ei_time)
	    < ERROR_DISCARD_TIME)
	    hub_dmp->hb_md.hm_misc_err = hub_log->hb_md.hm_misc_err;
    }

    for (i = 0; i < 2; i++) {
	if ((hub_dmp->hb_pi.hp_err_sts0[i] == 0) &&
	    (hub_log->hb_pi.hp_err_sts0[1])) {
	    if ((ERROR_TIMESTAMP - hub_log->hb_pi.hp_err_sts_info[i].ei_time)
		< ERROR_DISCARD_TIME) {
		hub_dmp->hb_pi.hp_err_sts0[i] = hub_log->hb_pi.hp_err_sts0[i];
		hub_dmp->hb_pi.hp_err_sts1[i] = hub_log->hb_pi.hp_err_sts1[i];
	    }
	}
    }
}

/*
 * Function	: save_err_node_comp
 * Parameters	: nasid -> Numa address space id of node we are interested in
 *		  board -> board structure.
 *		  type  -> Use ERROR_DUMP or ERROR_LOG structures..
 * Purpose	: Save errors belonging to components of the node board
 * Assumptions	: 
 * Returns	: None.
 */

void
save_err_node_comp(nasid_t nasid, lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;
    /* 
     * Acceptable components of node board are cpu, hub and memory.
     * address each component and save errors.
     */

    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_CPU:
	    /*
	     * cpu component. No error saving. If there was
	     * an error, the cache errors would save state 
	     */
	    if (edp->dp_flag & KL_ERROR_RESTORE)
		restore_cpu_err(CPU_COMP_ERROR(board, component));
	    break;
	case KLSTRUCT_HUB:
	    save_hub_err(nasid, HUB_COMP_ERROR(board, component), edp);
	    if (edp->dp_flag & KL_ERROR_RESTORE)
		restore_hub_err(HUB_COMP_ERROR(board, component));
	    break;

	case KLSTRUCT_MEMBNK:
	    /*
	     * nothing identified yet.
	     */
	    break;

	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: save_err_node_comp:unknown component 0x%x\n", 
		 KLCF_COMP(board, comp));
#endif /* ERR_DEBUG */
	    break;
	} /* switch */
    } /* for */
}



/*
 * Function	: save_err_io_comp
 * Parameters	: nasid -> Numa address space id of node we are interested in
 *		  board -> board structure.
 *		  flag  -> error structure to use: DUMP or LOG.
 * Purpose	: save errors of the components of an io board
 * Assumptions	: 
 * Returns	: None.
 */

void
save_err_io_comp(nasid_t nasid, lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;

    /* 
     * Acceptable component is bridge, IOC3, SuperIO, SCSI, FDDI. 
     * PCI?
     * where do the other devices fit in? 
     */
    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_BRI:
	    save_bridge_err(nasid, 
			    BRI_COMP_ERROR(board, component),
			    KLCF_BRIDGE_W_ID(component), edp);
	    break;
			
	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: fixme: save_err_io_comp: unhandled type 0x%x", 
		 KLCF_COMP_TYPE(component));
#endif
	    break;
	}
    }
}





/*
 * Function	: save_err_router_comp
 * Parameters	: nasid -> Numa address space id of node we are interested in
 *		  board -> board structure.
 *		  flag	-> which structure to save in (dump or log)
 * Purpose	: save errors of the components of the router board
 * Assumptions	: 
 * Returns	: None.
 */
/*ARGSUSED */
void
save_err_router_comp(nasid_t nasid, lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;
	
    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_ROU:
	    /* save_router_err(nasid, 
	       KL_ROUTECOMP_ERROR(component)); */
	    break;

	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: save_err_router_comp:unknown component type 0x%x\n",
		 KLCF_COMP_TYPE(component));
#endif /* ERR_DEBUG */
	    break;
	}
    }
}

/*
 * Save this for later, needs vector routing.
 */
/*ARGSUSED */
void 
save_router_err(nasid_t nasid, klrouter_err_t *router_err, edump_param_t *edp)
{
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving ROUTER error on NASID %d\n", nasid);
        printf("\tROUTER error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    bzero(router_err, sizeof(klrouter_err_t));

    (edp->dp_printf)("Fixme: save_router_err in hw state dump path\n");

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}



/*
 * Function	: save_err_xbow_comp
 * Parameters	: nasid -> Numa address space id of node we are interested in
 *		  board -> board structure.
 *		  flag  -> error structure to use: dump or log.
 * Purpose	: save errors of the components of the xbow board
 * Assumptions	: 
 * Returns	: None.
 */
void
save_err_xbow_comp(nasid_t nasid, lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;
	
    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_XBOW:
	    save_xbow_err(nasid, XBOW_COMP_ERROR(board, component), edp);
	    break;

	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: save_err_xbow_comp:unknown component type 0x%x\n", 
		 KLCF_COMP_TYPE(component));
#endif /* ERR_DEBUG */
	    break;
	}
    }
}




void
save_hub_pi_error(__psunsigned_t hub_base, hub_pierr_t *pi_err)
{
    hubreg_t sts;
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving HUBPI error on NASID %d\n", NASID_GET(hub_base));
        printf("\tHUBPI error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    bzero(pi_err, sizeof(hub_pierr_t));
    pi_err->hp_err_int_pend =  HUB_REG_PTR_L(hub_base, PI_ERR_INT_PEND);
    sts = HUB_REG_PTR_L(hub_base, PI_ERR_STATUS0_A);

    if (sts & PI_ERR_ST0_VALID_MASK) {
#if defined (HUB_ERR_STS_WAR)
	pi_err_stat0_t err_sts0;
	err_sts0.pi_stat0_word = sts;
	if (err_sts0.pi_stat0_fmt.s0_addr != 
	    (ERR_STS_WAR_PHYSADDR >> ERR_STAT0_ADDR_SHFT))
#endif /* HUB_ERR_STS_WAR  */
	{
	    pi_err->hp_err_sts0[0] = HUB_REG_PTR_L(hub_base, PI_ERR_STATUS0_A);
	    pi_err->hp_err_sts1[0] = HUB_REG_PTR_L(hub_base, PI_ERR_STATUS1_A);
	}
    }
    
    sts = HUB_REG_PTR_L(hub_base, PI_ERR_STATUS0_B);
    if (sts & PI_ERR_ST0_VALID_MASK) {
#if defined (HUB_ERR_STS_WAR)
	pi_err_stat0_t err_sts0;
	err_sts0.pi_stat0_word = sts;
	if (err_sts0.pi_stat0_fmt.s0_addr !=
	    (ERR_STS_WAR_PHYSADDR >> ERR_STAT0_ADDR_SHFT))
#endif /* HUB_ERR_STS_WAR  */
	{
	    pi_err->hp_err_sts0[1] = HUB_REG_PTR_L(hub_base, PI_ERR_STATUS0_B);
	    pi_err->hp_err_sts1[1] = HUB_REG_PTR_L(hub_base, PI_ERR_STATUS1_B);
	}
    }
#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}


void
save_hub_md_error(__psunsigned_t hub_base, hub_mderr_t *md_err)
{			
    md_dir_error_t	dir_err;
    md_mem_error_t	mem_err;
    md_proto_error_t    proto_err;
    hubreg_t misc_err;
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving HUBMD error on NASID %d\n", NASID_GET(hub_base));
        printf("\tHUBMD error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */
    bzero(md_err, sizeof(hub_mderr_t));

    dir_err.derr_reg = HUB_REG_PTR_L(hub_base, MD_DIR_ERROR);
    if (dir_err.derr_fmt.uce_vld || 
	dir_err.derr_fmt.ce_vld || dir_err.derr_fmt.ae_vld)
	    md_err->hm_dir_err   =  dir_err.derr_reg;

    mem_err.merr_reg = HUB_REG_PTR_L(hub_base, MD_MEM_ERROR);
    if (mem_err.merr_fmt.uce_vld || mem_err.merr_fmt.ce_vld)
	    md_err->hm_mem_err   =	mem_err.merr_reg;

    proto_err.perr_reg =  HUB_REG_PTR_L(hub_base, MD_PROTOCOL_ERROR);
    if (proto_err.perr_fmt.valid)
	md_err->hm_proto_err = proto_err.perr_reg;
    
    misc_err = HUB_REG_PTR_L(hub_base, MD_MISC_ERROR);
    if (misc_err)
	md_err->hm_misc_err  =	misc_err;

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}


void
save_hub_io_error(__psunsigned_t hub_base, hub_ioerr_t *io_err)
{
    int crb;
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving HUBIO error on NASID %d\n", NASID_GET(hub_base));
        printf("\tHUBIO error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    bzero(io_err, sizeof(hub_ioerr_t));

    io_err->hi_wstat     = 	HUB_REG_PTR_L(hub_base, IIO_WIDGET_STAT);
    io_err->hi_bte0_sts  = 	HUB_REG_PTR_L(hub_base, IIO_IBLS_0);
    io_err->hi_bte0_src  = 	HUB_REG_PTR_L(hub_base, IIO_IBSA_0);
    io_err->hi_bte0_dst  = 	HUB_REG_PTR_L(hub_base, IIO_IBDA_0);
    io_err->hi_bte1_sts  = 	HUB_REG_PTR_L(hub_base, IIO_IBLS_1);
    io_err->hi_bte1_src  = 	HUB_REG_PTR_L(hub_base, IIO_IBSA_1);
    io_err->hi_bte1_dst  = 	HUB_REG_PTR_L(hub_base, IIO_IBDA_1);
			
    for (crb = 0; crb < IIO_NUM_CRBS; crb++) {
	icrbc_t icrbc;
	io_err->hi_crb_entA[crb] =
	    HUB_REG_PTR_L(hub_base, IIO_ICRB_A(crb));
	icrbc.reg_value = HUB_REG_PTR_L(hub_base, IIO_ICRB_C(crb));
	/* Use the reserved bits (63:58) of iicrb entry A for special
	 * purposes. In this case use code 1 to mean that it corresponds
	 * to a bte op. This is being used to detect the BTE PUSH bug
	 * crb error address pattern.
	 */
	io_err->hi_crb_entA[crb] |= (icrbc.c_bteop ?
					(1ull << 58) : 0);
    }

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}

void
save_hub_err_ext(__psunsigned_t hub_base, klhub_err_ext_t *klhub_err_ext)
{
    int crb;
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving HUBIO error on NASID %d\n", NASID_GET(hub_base));
        printf("\tHUBIO error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    bzero(klhub_err_ext, sizeof(klhub_err_ext_t));

    for (crb = 0; crb < IIO_NUM_CRBS; crb++) {
        klhub_err_ext->hi_crb_entB[crb] =
                        HUB_REG_PTR_L(hub_base, IIO_ICRB_B(crb));
        klhub_err_ext->hi_crb_entC[crb] =
                        HUB_REG_PTR_L(hub_base, IIO_ICRB_C(crb));
    }

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}

void
save_hub_ni_error(__psunsigned_t hub_base, hub_nierr_t *ni_err)
{	
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving HUBNI error on NASID %d\n", NASID_GET(hub_base));
        printf("\tHUBNI error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    bzero(ni_err, sizeof(hub_nierr_t));

    ni_err->hn_vec_sts   =	HUB_REG_PTR_L(hub_base, NI_VECTOR_STATUS);
    ni_err->hn_port_err  =	HUB_REG_PTR_L(hub_base, NI_PORT_ERROR);

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}



/*
 * Function	: save_hub_err
 * Parameters	: nasid   -> nasid of the node to which this hub belongs.
 *		  hub_err -> save area for hub errors.
 *		  flag    -> error structure to use: dump or log.
 * Purpose	: save all the hub errors in the hub_err save area.
 * Assumptions	: 
 * Returns	: None.
 */
void
save_hub_err(nasid_t nasid, klhub_err_t *klhub_err, edump_param_t *edp)
{
    hub_error_t *hub_err = HUB_ERROR_STRUCT(klhub_err, edp->dp_flag);
    __psunsigned_t hub_base;

    if (nasid == INVALID_NASID)
	hub_base = (__psunsigned_t)LOCAL_HUB_ADDR(0);
    else
	hub_base = (__psunsigned_t)REMOTE_HUB_ADDR(nasid, 0);

    save_hub_pi_error(hub_base, &hub_err->hb_pi);
    save_hub_md_error(hub_base, &hub_err->hb_md);
    save_hub_io_error(hub_base, &hub_err->hb_io);
    save_hub_ni_error(hub_base, &hub_err->hb_ni);

    if (brd_sversion(NASID_GET(hub_base), KLTYPE_IP27) > 2)
        save_hub_err_ext(hub_base, (klhub_err_ext_t *)
		NODE_OFFSET_TO_K1(NASID_GET(hub_base), klhub_err->he_extension));
}


/*
 * Function	: save_bridge_err
 * Parameters	: nasid -> numa address space id of node.
 *		: bridge_err -> bridge error structure.
 *		: flag	-> which error structure to use: dmp or log.
 * Purpose	: save all errors for the bridge ASIC.
 * Assumptions	: 
 * Returns	: None.
 */
void
save_bridge_err(nasid_t nasid, klbridge_err_t *bridge_err, int w_id, 
		edump_param_t *edp)
{
    __uint64_t brbase = SN0_WIDGET_BASE(nasid, w_id);
    bridge_error_t	*br_err;
#if defined _STANDALONE
    jmp_buf 		new_buf;
    void 		*old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving BRIDGE error on NASID %d in WIDGETID %d\n", nasid, w_id);
        printf("\tBRIDGE error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    br_err = BRIDGE_ERROR_STRUCT(bridge_err, edp->dp_flag);

    bzero(br_err, sizeof(bridge_error_t));

    br_err->br_err_upper = *BRIDGE_REG_PTR(brbase, BRIDGE_WID_ERR_UPPER);
    br_err->br_err_lower = *BRIDGE_REG_PTR(brbase, BRIDGE_WID_ERR_LOWER);
    br_err->br_err_cmd   = *BRIDGE_REG_PTR(brbase, BRIDGE_WID_ERR_CMDWORD);
    br_err->br_aux_err   = *BRIDGE_REG_PTR(brbase, BRIDGE_WID_AUX_ERR);
    br_err->br_resp_upper= *BRIDGE_REG_PTR(brbase, BRIDGE_WID_RESP_UPPER);
    br_err->br_resp_lower= *BRIDGE_REG_PTR(brbase, BRIDGE_WID_RESP_LOWER);
    br_err->br_ram_perr  = *BRIDGE_REG_PTR(brbase, BRIDGE_RAM_PERR);
    br_err->br_pci_upper = *BRIDGE_REG_PTR(brbase, BRIDGE_PCI_ERR_UPPER);
    br_err->br_pci_lower = *BRIDGE_REG_PTR(brbase, BRIDGE_PCI_ERR_LOWER);
    br_err->br_int_status= *BRIDGE_REG_PTR(brbase, BRIDGE_INT_STATUS);

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}


/*
 * Function	: save_xbow_err
 * Parameters	: nasid -> numa address space id of node.
 *		: xbow_err -> xbow error structure.
 * Purpose	: save all error registers of the xbow ASIC
 * Assumptions	:  
 * Returns	: None.
 */

void
save_xbow_err(nasid_t nasid, klxbow_err_t *xbow_err, edump_param_t *edp)
{
    int link;
    __uint64_t xbase = XBOW_SN0_BASE(nasid);
    xbow_error_t *xb_err;
#if defined _STANDALONE
    jmp_buf new_buf;
    void *old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception while saving XBOW error on NASID %d\n", nasid);
        printf("\tXBOW error state might not be complete. Continuing...\n");
        restorefault(old_buf);
        return;
    }
#endif /* _STANDALONE */

    xb_err = XBOW_ERROR_STRUCT(xbow_err, edp->dp_flag);

    bzero(xb_err, sizeof(xbow_error_t));

    xb_err->xb_status    = *XBOW_REG_PTR(xbase, XBOW_WID_STAT);

    xb_err->xb_err_upper = *XBOW_REG_PTR(xbase, XBOW_WID_ERR_UPPER);
    xb_err->xb_err_lower = *XBOW_REG_PTR(xbase, XBOW_WID_ERR_LOWER);
    xb_err->xb_err_cmd   = *XBOW_REG_PTR(xbase, XBOW_WID_ERR_CMDWORD);
    for (link = 0; link < MAX_XBOW_PORTS; link++) {
	xb_err->xb_link_status[link] = 
	    *XBOW_REG_PTR(xbase, XB_LINK_STATUS(link + BASE_XBOW_PORT));
	xb_err->xb_link_aux_status[link] = 
	    *XBOW_REG_PTR(xbase, XB_LINK_AUX_STATUS(link + BASE_XBOW_PORT));
    }	

#if defined _STANDALONE
    restorefault(old_buf);
#endif /* _STANDALONE */
}		



/*
 * Function	: kl_error_show_node
 * Parameters	: nasid -> nasid of node on which we need error
 *		: flag  -> which structure to show: dump or log
 * Purpose	: To dump errors of said node.
 * Assumptions	: None.
 * Returns	: None.
 */


void 
kl_error_show_node(nasid_t nasid, edump_param_t *edp)
{
    lboard_t *klconf;

    if (nasid == INVALID_NASID)
	nasid = get_nasid();

    kl_perr(NODE_LVL, "Errors on node Nasid 0x%x (%d)\n", nasid, nasid);

    klconf = (lboard_t *)KL_CONFIG_INFO(nasid);
    while (klconf) {
	if (KLCF_REMOTE(klconf)) {
	    (*edp->dp_printf)
		("Warning: fixme() kl_dmp_hw_state:remote board in klconfig\n");
	    klconf = KLCF_NEXT(klconf);
	    continue;
	}
	if (KL_CONFIG_DUPLICATE_BOARD(klconf)) {
	    klconf = KLCF_NEXT(klconf);
	    continue;
	}
			
	switch (KLCF_CLASS(klconf))  {
	case KLCLASS_NODE:
	    show_err_node_comp(klconf, edp);
	    break;
	case KLCLASS_IO:
	    show_err_io_comp(klconf, edp);
	    break;
	case KLCLASS_ROUTER:
	    show_err_router_comp(klconf, edp);
	    break;
	case KLCLASS_MIDPLANE:
	    show_err_xbow_comp(klconf, edp);
	    break;				
	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: kl_dmp_hw_state: Unknown class 0x%x in klconf\n",
		 KLCF_CLASS(klconf));
#endif /* ERR_DEBUG */
	    break;
	}
	klconf = KLCF_NEXT(klconf);
    }
    kl_perr_dumpbuf(edp->dp_printf, edp->dp_prarg);
}


/*
 * Function	: kl_error_show
 * Parameters	: flag -> which error structure to show: DUMP or LOG
 * Purpose	: displays all errors that have been recorded.
 * Assumptions	: 
 * Returns	: None.
 */
void
kl_error_show(char *dump_str, char *end_str, edump_param_t *edp)
{
    int cnode;
    volatile __int64_t i = 0;
#if defined (_STANDALONE)
    gda_t *gdap = GDA;
    nasid_t nasid;
    pcfg_hub_t *hub_cf;
    partid_t partition = PCFG(get_nasid())->array[0].hub.partition;
#endif /* _STANDALONE */
	
    kl_perr(NONE_LVL | KL_FINAL_MSG, dump_str);
#if !defined (_STANDALONE)
    for (cnode = 0; cnode < numnodes; cnode++) {
	kl_error_show_node(COMPACT_TO_NASID_NODEID(cnode), edp);
    }
	
#else  /* _STANDALONE */
    if ((gdap) && (gdap->g_magic == GDA_MAGIC) &&
        (gdap->g_nasidtable[1] != INVALID_NASID)) {
        for (i = 0; i < MAX_COMPACT_NODES; i++) {
            if ((nasid = gdap->g_nasidtable[i]) == INVALID_NASID)
                break;
            kl_error_show_node(nasid, edp);
        }
    }
    else {
        kl_error_show_node(get_nasid(), edp);
        if( PCFG(get_nasid())->magic == PCFG_MAGIC) {
            for (i = 0; i < PCFG(get_nasid())->count; i++) {
                 hub_cf = &PCFG_PTR(get_nasid(), i)->hub;  
                 if (hub_cf->type != PCFG_TYPE_HUB)
                     continue;
                 if (IS_HUB_MEMLESS(hub_cf))
                     continue ;
                 if ((hub_cf->nasid != INVALID_NASID) &&
                     (hub_cf->nasid != get_nasid()) &&
                     (hub_cf->partition == partition))
                     kl_error_show_node(hub_cf->nasid, edp);
            }
        }
    }
#endif /* _STANDALONE */
    kl_perr(NONE_LVL | KL_FINAL_MSG, end_str);
	
    kl_perr_dumpbuf(edp->dp_printf, edp->dp_prarg);
}


#if defined (_STANDALONE)
int
kl_error_check_power_on(void)
{
    klinfo_t *component;
    pcfg_hub_t *hub_cf;
    lboard_t *klconf;
    nasid_t nasid;
    int i, comp;
    hub_error_t *hub_err;
    klhub_err_t *klhub_err;
    partid_t partition = PCFG(get_nasid())->array[0].hub.partition;

    for (i = 0; i < PCFG(get_nasid())->count; i++) {
	hub_cf = &PCFG_PTR(get_nasid(), i)->hub;
	if (hub_cf->type != PCFG_TYPE_HUB) continue;
	    
	if ((hub_cf->nasid == INVALID_NASID) ||
		(hub_cf->partition != partition))
            continue;

	nasid = hub_cf->nasid;
	klconf = (lboard_t *)KL_CONFIG_INFO(nasid);
	while (klconf) {
	    if (KLCF_CLASS(klconf) == KLCLASS_NODE)
		break;
	    klconf = KLCF_NEXT(klconf);
	}
	if (klconf == NULL) continue;
	for (comp = 0; comp < KLCF_NUM_COMPS(klconf); comp++) {
	    component = KLCF_COMP(klconf, comp);
	    if (KLCF_COMP_TYPE(component) == KLSTRUCT_HUB) {
		klhub_err = HUB_COMP_ERROR(klconf, component);
		hub_err = HUB_ERROR_STRUCT(klhub_err, KL_ERROR_LOG);
		if (hub_err->reset_flag) return 1;
	    }
	}
    }
    return 0;
}

#endif


void
kl_dump_hw_state(void)
{
    edump_param_t edp;

    edp.dp_flag = KL_ERROR_DUMP | KL_ERROR_RESTORE;
    edp.dp_printf = (void (*)())(printf);
    edp.dp_prarg = 0;
    kl_save_hw_state_all(&edp);
}

void
kl_log_hw_state(void)
{
    edump_param_t edp;

    edp.dp_flag = KL_ERROR_LOG;
    edp.dp_printf = (void (*)())(printf);
    edp.dp_prarg = 0;
    kl_save_hw_state_all(&edp);
}


void
kl_error_show_dump(char *str1, char *str2)
{
    edump_param_t edp;

    edp.dp_flag = KL_ERROR_DUMP;
#if !defined (_STANDALONE)
    edp.dp_printf = (void (*)())(cmn_err);
    edp.dp_prarg = CE_CONT;
#else
    edp.dp_printf = (void (*)())(printf);
    edp.dp_prarg = 0;
#endif

    kl_error_show(str1, str2, &edp);
}

void
kl_error_show_log(char *str1, char *str2)
{
    edump_param_t edp;

    edp.dp_flag = KL_ERROR_LOG;
    edp.dp_printf = (void (*)())(printf);
    edp.dp_prarg = 0;
    kl_error_show(str1, str2, &edp);
}

#if !defined (_STANDALONE)
void
kl_error_show_idbg(char *str1, char *str2)
{
    edump_param_t edp;

    edp.dp_flag = KL_ERROR_LOG;
    edp.dp_printf = (void (*)())(qprintf);
    edp.dp_prarg = 0;
    kl_error_show(str1, str2, &edp);
}

void
kl_error_show_reset_ste(char *str1, char *str2, void (*prf)())
{
    edump_param_t edp;

    edp.dp_flag = KL_ERROR_LOG;
    edp.dp_printf = (void (*)())(prf);
    edp.dp_prarg = 0;
    kl_error_show(str1, str2, &edp);
}
#endif

/*
 * Function	: show_err_node_comp
 * Parameters	: board -> board structure.
 * Purpose	: show all relevent errors of the components on the node brd.
 * Assumptions	: 
 * Returns	: None.
 */

void
show_err_node_comp(lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;
    klcpu_err_t *cpu_err;
    char slotname[100];

    /* 
     * Acceptable components of node board are cpu, hub and memory.
     */
    get_slotname(BOARD_SLOT(board), slotname);
    kl_perr(SLOT_LVL, "%s in /hw/module/%d/slot/%s \n", 
	    NODENAME,
	    KLCF_MODULE_ID(board),
	    slotname);

	
    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_CPU:
	    cpu_err = CPU_COMP_ERROR(board, component);
	    if (cpu_err->ce_cache_err_dmp.ce_cache_err)
		show_cache_err(cpu_err, component->physid, edp);
	    break;
	case KLSTRUCT_HUB:
	    show_hub_err(HUB_COMP_ERROR(board, component), edp, board->brd_nasid);
	    break;

	case KLSTRUCT_MEMBNK:
	    break;

	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: show_err_node_comp:unknown component 0x%x\n",
		 KLCF_COMP(board, comp));
#endif /* ERR_DEBUG */
	    break;
	}
		
    }
	
}

/*
 * The ps_cache_err encodes messages for certain bits set in the  
 * cache error register in the case of primary or secondary cache errors.
 */
char *ps_cache_error[] = {
    "20: TM, Uncorrectable tag mod array error, way 0",
    "21: TM, Uncorrectable tag mod array error, way 1",
    "22: TS, Uncorrectable tag state array error, way 0",
    "23: TS, Uncorrectable tag state array error, way 1",
    "24: TA, Uncorrectable tag address array error, way 0",
    "25: TA, Uncorrectable tag address array error, way 1",
    "26:  D, Uncorrectable data array error, way 0",
    "27:  D, Uncorrectable data array error, way 1",
    "28: EE, Error Dirty Exclusive Inconsistent state",
    "29: EW, Error while holding previous error",
};

#define	PS_CERR_MAX	(sizeof(ps_cache_error) / sizeof(ps_cache_error[0]))

/*
 * The sie_cache_err encodes messages for certain bits set in the  
 * cache error register in the case of system interface errors.
 */
char *sie_cache_error[] = {
    "23: SR, Uncorrectable system response bus error",
    "24: SC, Uncorrectable system command bus error",
    "25: SA, Uncorrectable system address bus error",
    "26:  D, Uncorrectable data array error, way 0",
    "27:  D, Uncorrectable data array error, way 1",
    "28: EE, Error Dirty Exclusive state",
    "29: EW, Error while holding previous error",
};

#define	SIE_CERR_MAX	(sizeof(sie_cache_error) / sizeof(sie_cache_error[0]))



/*
 * Function	: show_cache_err
 * Parameters	: cpu_err -> cpu error structure.
 *		  cpuid   -> cpuid.
 * Purpose	: show the relevent cache errors that are set.
 * Assumptions	: 
 * Returns	: None.
 */

#if !defined (_STANDALONE)
void
show_cache_err(klcpu_err_t *cpu_err, int cpuid, edump_param_t *edp)
{
    cache_err_t *cache_err;
    uint cerr, err;
    __uint64_t tag0, tag1;
    paddr_t paddr0, paddr1;
    int i;
    char buf0[64];
    char buf1[64];

    paddr0 = 0;
    paddr1 = 0;
    
    sprintf(buf0, "");
    sprintf(buf1, "");

    if ((cache_err = CPU_ERROR_STRUCT(cpu_err, edp->dp_flag)) == 0) return;
    err = cache_err->ce_cache_err;
    tag0 = cache_err->ce_tags[0];
    tag1 = cache_err->ce_tags[1];

    kl_perr(ASIC_LVL, "CPU cache on cpu %d\n", cpuid);
    kl_perr(REG_LVL, "Cache error register: 0x%x\n", err);
	
    switch (err & CE_TYPE_MASK) {
    case CE_TYPE_I:
	if (err & CE_D_WAY0) {
	    paddr0 = PCACHE_ERROR_ADDR(err, tag0);
	    sprintf(buf0, "Way 0 addr 0x%x ", paddr0);
	}
	
	if (err & CE_D_WAY1) {
	    paddr1 = PCACHE_ERROR_ADDR(err, tag1);
	    sprintf(buf1, "Way 1 addr 0x%x ", paddr1);
	}
	
	kl_perr(BIT_LVL,"Primary icache error: pidx 0x%x %s %s\n",
		(err & CACHERR_PIDX_MASK), buf0, buf1);
	cerr = err & ~CE_TM_MASK;
	cerr >>= 20;
	break;
    case CE_TYPE_D:
	if (err & CE_D_WAY0) {
	    paddr0 = PCACHE_ERROR_ADDR(err, tag0);
	    sprintf(buf0, "Way 0 addr 0x%x ", paddr0);
	}
	
	if (err & CE_D_WAY1) {
	    paddr1 = PCACHE_ERROR_ADDR(err, tag1);
	    sprintf(buf1, "Way 1 addr 0x%x ", paddr1);
	}
	
	kl_perr(BIT_LVL,"Primary dcache error: pidx 0x%x %s %s\n", 
		(err & CACHERR_PIDX_MASK), buf0, buf1);
	cerr = err;
	cerr >>= 20;
	break;
    case CE_TYPE_S:
	if (err & CE_D_WAY0) {
	    paddr0 = SCACHE_ERROR_ADDR(err, tag0);
	    sprintf(buf0, "Way 0 addr 0x%x ", paddr0);
	}
	
	if (err & CE_D_WAY1) {
	    paddr1 = SCACHE_ERROR_ADDR(err, tag1);
	    sprintf(buf1, "Way 1 addr 0x%x ", paddr1);
	}
	
	kl_perr(BIT_LVL,"Secondary cache error: sidx 0x%x %s %s\n", 
		(err & CACHERR_SIDX_MASK), buf0, buf1);
	cerr = err & ~CE_TM_MASK;
	cerr &= ~CE_TS_MASK;
	cerr >>= 20;
	break;

    case CE_TYPE_SIE:
	if (err & CE_D_WAY0) {
	    paddr0 = SCACHE_ERROR_ADDR(err, tag0);
	    sprintf(buf0, "Way 0 addr 0x%x ", paddr0);
	}
	
	if (err & CE_D_WAY1) {
	    paddr1 = SCACHE_ERROR_ADDR(err, tag1);
	    sprintf(buf1, "Way 1 addr 0x%x ", paddr1);
	}
	kl_perr(BIT_LVL,"System interface error: sidx 0x%x %s %s\n", 
		(err & CACHERR_SIDX_MASK), buf0, buf1);
	cerr = err;
	cerr >>= 23;
	break;

    default:
	(*edp->dp_printf)("Warning: Unknown cache error type: %x",
			   (err & CE_TYPE_MASK));
	break;
    }

    switch (err & CE_TYPE_MASK) {
    case CE_TYPE_I:
    case CE_TYPE_D:
    case CE_TYPE_S:
	for (i = 0; i < PS_CERR_MAX; i++) {
	    if (cerr & (1 << i))
		kl_perr(BIT_LVL, "%s\n", ps_cache_error[i]);
	}
	break;
    case CE_TYPE_SIE:
	for (i = 0; i < SIE_CERR_MAX; i++) {
	    if (cerr & (1 << i))
		kl_perr(BIT_LVL, "%s\n", sie_cache_error[i]);
	}
	break;
    }
}

#else /* _STANDALONE */

void
show_cache_err(klcpu_err_t *cpu_err, int cpuid, edump_param_t *edp)
{
    cache_err_t *cache_err;
    uint cerr, err;
    int i;
    
    if ((cache_err = CPU_ERROR_STRUCT(cpu_err, edp->dp_flag)) == 0) return;
    err = cache_err->ce_cache_err;

    kl_perr(ASIC_LVL, "CPU cache on cpu %d\n", cpuid);
    kl_perr(REG_LVL, "Cache error register: 0x%x\n", err);
	
    switch (err & CE_TYPE_MASK) {
    case CE_TYPE_I:
	kl_perr(BIT_LVL,"Primary icache error: pidx 0x%x\n",
		(err & CACHERR_PIDX_MASK));
	cerr = err & ~CE_TM_MASK;
	cerr >>= 20;
	break;
    case CE_TYPE_D:
	kl_perr(BIT_LVL,"Primary dcache error: pidx 0x%x\n", 
		(err & CACHERR_PIDX_MASK));
	cerr = err;
	cerr >>= 20;
	break;
    case CE_TYPE_S:
	kl_perr(BIT_LVL,"Secondary cache error: sidx 0x%x\n", 
		(err & CACHERR_SIDX_MASK));
	cerr = err & ~CE_TM_MASK;
	cerr &= ~CE_TS_MASK;
	cerr >>= 20;
	break;

    case CE_TYPE_SIE:
	kl_perr(BIT_LVL,"System interface error: sidx 0x%x\n", 
		(err & CACHERR_SIDX_MASK));
	cerr = err;
	cerr >>= 23;
	break;

    default:
	(*edp->dp_printf)("Warning: Unknown cache error type: %x",
			   (err & CE_TYPE_MASK));
	break;
    }

    switch (err & CE_TYPE_MASK) {
    case CE_TYPE_I:
    case CE_TYPE_D:
    case CE_TYPE_S:
	for (i = 0; i < PS_CERR_MAX; i++) {
	    if (cerr & (1 << i))
		kl_perr(BIT_LVL, "%s\n", ps_cache_error[i]);
	}
	break;
    case CE_TYPE_SIE:
	for (i = 0; i < SIE_CERR_MAX; i++) {
	    if (cerr & (1 << i))
		kl_perr(BIT_LVL, "%s\n", sie_cache_error[i]);
	}
	break;
    }
}

#endif /* _STANDALONE */



/*
 * Function	: show_hub_err
 * Parameters	: hub_err -> hub error structure.
 *		: flag    -> flag: use DUMP or LOG error info.
 * Purpose	: show the errors signalled in the hub.
 * Assumptions	: 
 * Returns	: None.
 */
void
show_hub_err(klhub_err_t *klhub_err, edump_param_t *edp, nasid_t nasid)
{
    hub_error_t *hub_err = HUB_ERROR_STRUCT(klhub_err, edp->dp_flag);
    kl_perr(ASIC_LVL, "HUB signalled following errors.\n");

    show_hub_pi_err(&hub_err->hb_pi);
    show_hub_md_err(&hub_err->hb_md);
    show_hub_io_err(klhub_err, edp->dp_flag, nasid);
    show_hub_ni_err(&hub_err->hb_ni);

    return;
}


/*
 * error encoding for ERROR_INT_PEND hub register.
 */

char *hub_eint_pend[] = {
    " 0: CPU B spool count compare",
    " 1: CPU A spool count compare",
    " 2: CPU B spurious message",
    " 3: CPU A spurious message",
    " 4: CPU B write response block, timeout error",
    " 5: CPU A write response block, timeout error",
    " 6: CPU B write response block, write error",
    " 7: CPU A write response block, write error",
    " 8: CPU B SysState parity error",
    " 9: CPU A SysState parity error",
    "10: CPU B SysAd error during data cycle",
    "11: CPU A SysAd error during data cycle",
    "12: CPU B SysAd error during address cycle",
    "13: CPU A SysAd error during address cycle",
    "14: CPU B SysCmd parity error during address cycle",
    "15: CPU A SysCmd parity error during address cycle",
    "16: CPU B SysCmd parity error during data cycle",
    "17: CPU A SysCmd parity error during data cycle",
    "18: CPU B write or timeout error during spool",
    "19: CPU A write or timeout error during spool",
    "20: CPU B received uncorrectable error during uncached load",
    "21: CPU A received uncorrectable error during uncached load",
    "22: CPU B SysState tag error",
    "23: CPU B SysState tag error",
    "24: Memory/Directory uncorrectable error",
};

#define MAX_HUB_EINT 	(sizeof(hub_eint_pend) / sizeof(hub_eint_pend[0]))


char *hub_rrb_err_type[] = {
    "0 Uncached Access Error",
    "1 Uncached Partial Error",
    "2 Directory Error",
    "3 Timeout Error",
};

char *hub_wrb_err_type[] = {
    "0 Write Error",
    "1 Partial Write Error",
    "2 UNKNOWN",
    "3 Timeout Error",
};

/*
 * Function	: show_hub_pi_err
 * Parameters	: pi_err ->  PI err structure saved in hub_err
 * Purpose	: show the errors in the PI section of the hub ASIC.
 * Assumptions	: 
 * Returns	: None.
 */


void
show_hub_pi_err(hub_pierr_t *pi_err)
{
    int i, j;
    pi_err_stat0_t err_sts0;
    pi_err_stat1_t err_sts1;
    hubreg_t err;

    if (err = pi_err->hp_err_int_pend) {
	kl_perr(REG_LVL, "HUB error interrupt register: 0x%lx\n",err);
	for (i = 0; i < MAX_HUB_EINT; i++) {	
	    if (err & (1 << i)) 
		kl_perr (BIT_LVL, "%s\n", hub_eint_pend[i]);
	}
    }
	
    for (j = 0; j < 2; j++) {
	char cnum;
	int log = 0;
	cnum = j ? 'B' : 'A';
	err_sts0.pi_stat0_word = pi_err->hp_err_sts0[j];

	if (err_sts0.pi_stat0_fmt.s0_valid == 0) 
	    continue;
	err_sts1.pi_stat1_word = pi_err->hp_err_sts1[j];

	kl_perr(REG_LVL | log, "HUB error status 0 %c register: 0x%lx\n",
		cnum, err_sts0.pi_stat0_word);
	if (err_sts1.pi_stat1_fmt.s1_rw_rb) { /* WRB */
	    kl_perr(BIT_LVL | log, "02<->00: wrb error type %s\n",
		    hub_wrb_err_type[err_sts0.pi_stat0_fmt.s0_err_type]);
	}
	else {				/* RRB */
	    kl_perr(BIT_LVL | log, "02<->00: rrb error type %s\n",
		    hub_rrb_err_type[err_sts0.pi_stat0_fmt.s0_err_type]);

	    kl_perr(BIT_LVL | KL_LOG_MSG,
		    "05<->03: T5 RRB req. number 0x%x\n", 
		    err_sts0.pi_stat0_fmt.s0_t5_req);
	}
	kl_perr(BIT_LVL | KL_LOG_MSG,
		"16<->06: message supplemental 0x%x\n",
		err_sts0.pi_stat0_fmt.s0_supl);

	kl_perr(BIT_LVL | KL_LOG_MSG, "24<->17: message command 0x%x\n", 
		err_sts0.pi_stat0_fmt.s0_cmd);
/*
FIXME
*/
	kl_perr(BIT_LVL | log, "61<->25: error address 0x%lx << 3 = (0x%x)\n",
		err_sts0.pi_stat0_fmt.s0_addr,
		err_sts0.pi_stat0_fmt.s0_addr << 3);

	if (err_sts0.pi_stat0_fmt.s0_ovr_run)
	    kl_perr(BIT_LVL | log, "62<->62: over_run\n");
	kl_perr(BIT_LVL | log, "63<->63: error status valid\n");
		
	kl_perr(REG_LVL | log, "HUB error status 1 %c register: 0x%lx\n", cnum, err_sts1.pi_stat1_word);

	if (err_sts1.pi_stat1_fmt.s1_spl_cnt)
	    kl_perr(BIT_LVL | log, "20<->00: spool count 0x%x\n", 
		    err_sts1.pi_stat1_fmt.s1_spl_cnt);
	
	kl_perr(BIT_LVL | KL_LOG_MSG,
		"28<->21: crb timeout counter 0x%x\n", 
		err_sts1.pi_stat1_fmt.s1_to_cnt);
	
	kl_perr(BIT_LVL | KL_LOG_MSG,
		"38<->29: invalidate counter 0x%x\n", 
		err_sts1.pi_stat1_fmt.s1_inval_cnt);

	if (err_sts1.pi_stat1_fmt.s1_rw_rb)
	    kl_perr(BIT_LVL | KL_LOG_MSG,"41<->39: WRB number 0x%x\n", 
		    err_sts1.pi_stat1_fmt.s1_crb_num);
	else
	    kl_perr(BIT_LVL | KL_LOG_MSG,"41<->39: RRB number 0x%x\n",
		    err_sts1.pi_stat1_fmt.s1_crb_num);

	kl_perr(BIT_LVL | KL_LOG_MSG, "52<->43: crb status 0x%x\n",
		err_sts1.pi_stat1_fmt.s1_crb_sts);

	kl_perr(BIT_LVL | KL_LOG_MSG,"63<->53: message source 0x%x\n", 
		err_sts1.pi_stat1_fmt.s1_src);
    }
}

/*
 * 
 * Hub Directory state array 
 */
char *dir_state_str[] = {
	"Directory shared",
	"Directory poisoned",
	"Directory exclusive",
	"Directory busy shared",
	"Directory busy exclusive",
	"Directory wait",
	"Reserved state",
	"Directory unowned",
};

char *dir_owner_str[] = {
	"CPU A",
	"CPU B",
	"IO",
	"IO",
};


/*
 *	MISC error register in the hub MD section
 */

char *hmd_misc_error[] = {
    "0: overrun, data marked bad written to memory",
    "1: valid, data marked bad written to memory",
    "2: overrun, incoming message was too short",
    "3: valid, incoming message was too short",
    "4: overrun, incoming message was too long",
    "5: valid,  incoming message was too long",
    "6: overrun, incoming revision was to illegal address",
    "7: valid, incoming revision was to illegal address",
    "8: overrun, incoming message was an illegal type",
    "9: valid, incoming message was an illegal type",
};

#define HMD_MISC_MAX_ERR (sizeof(hmd_misc_error) / sizeof(hmd_misc_error[0]))

/*
 * Function	: show_hub_md_err
 * Parameters	: md_err -> the md_err part of the hub_err.
 * Purpose	: show the errors that are set in the MD part of HUB ASIC
 * Assumptions	: 
 * Returns	: None.
 */

void
show_hub_md_err(hub_mderr_t *md_err)
{
    hubreg_t err;
    int i;
    md_dir_error_t	dir_err;
    md_mem_error_t	mem_err;
    md_proto_error_t proto_err;
    __uint64_t		ha;

    dir_err.derr_reg = md_err->hm_dir_err;
    if (dir_err.derr_fmt.uce_vld || dir_err.derr_fmt.ae_vld ||
	dir_err.derr_fmt.ce_vld) {
	kl_perr(REG_LVL, "HUB MD Dir Error register: 0x%lx\n", 
		dir_err.derr_reg);
		
	if (dir_err.derr_fmt.ce_vld && dir_err.derr_fmt.ce_ovr)
	    kl_perr(BIT_LVL,"0: multiple correctable dir ecc\n");
	if (dir_err.derr_fmt.ae_vld && dir_err.derr_fmt.ae_ovr)
	    kl_perr(BIT_LVL,"1: multiple protection rights error\n");
	if (dir_err.derr_fmt.uce_vld && dir_err.derr_fmt.uce_ovr)
	    kl_perr(BIT_LVL,"2: multiple uncorrectable dir ecc\n");
        ha = dir_err.derr_fmt.hspec_addr << 3;
        /*
         * Workaround:  If the directory error HSPEC address appears to be
         * munged due to Hub errata #402537, compensate by de-translating.
         */
        if ((ha & 0xf0000000) == 0x30000000)
            ha = BDADDR_IS_PRT(ha) ? BDPRT_TO_MEM(ha) : BDDIR_TO_MEM(ha);

	kl_perr(BIT_LVL, "28<->3: bad BDDIR space address 0x%x (phys addr 0x%x)\n",	
		dir_err.derr_fmt.hspec_addr, ha);
		
	kl_perr(BIT_LVL, "38<->32: bad directory syndrome 0x%x\n", 
		dir_err.derr_fmt.bad_syn);
	kl_perr(BIT_LVL, "41<->39: bad access encoding 0x%x\n", 
		dir_err.derr_fmt.bad_prot);
	if (dir_err.derr_fmt.uce_vld)
	    kl_perr(BIT_LVL, "61: uncorrectable directory ecc\n");
	if (dir_err.derr_fmt.ae_vld)
	    kl_perr(BIT_LVL, "62: protection rights ecc error\n");
	if (dir_err.derr_fmt.ce_vld)
	    kl_perr(BIT_LVL, "63: correctable directory ecc\n");
    }
	
    mem_err.merr_reg = md_err->hm_mem_err;
    if (mem_err.merr_fmt.uce_vld) {
	kl_perr(REG_LVL, "HUB MD Mem Error Register: 0x%lx\n", 
		mem_err.merr_reg);
	if (mem_err.merr_fmt.ce_ovr) 
	    kl_perr(BIT_LVL,"00<->00: multiple correctable mem ecc\n");
	if (mem_err.merr_fmt.uce_ovr) 
	    kl_perr(BIT_LVL,"01<->01: multiple uncorrectable mem ecc\n");
	if (mem_err.merr_fmt.address)
	    kl_perr(BIT_LVL, "31<->03: bad entry pointer 0x%x << 3 = (0x%x)\n",
		    mem_err.merr_fmt.address,
		    mem_err.merr_fmt.address << 3);
	if (mem_err.merr_fmt.bad_syn)
	    kl_perr(BIT_LVL, "39<->32: bad memory syndrome 0x%x\n", 
		    mem_err.merr_fmt.bad_syn);
	if (mem_err.merr_fmt.ce_vld) 
	    kl_perr(BIT_LVL, "62<->62: correctable memory ecc\n");
	if (mem_err.merr_fmt.uce_vld)	/* redundant check */
	    kl_perr(BIT_LVL, "63<->63 uncorrectable memory ecc\n");
    }
	
    proto_err.perr_reg = md_err->hm_proto_err;
    if (proto_err.perr_fmt.valid) {
	kl_perr(REG_LVL, "HUB MD Protocol Error Register: 0x%lx\n", 
		proto_err.perr_reg);
	if (proto_err.perr_fmt.overrun)
	    kl_perr(BIT_LVL,"00<->00: multiple protocol errors occurred\n");
	if (proto_err.perr_fmt.address)
	    kl_perr(BIT_LVL,"31<->03: request address 0x%x << 3 = (0x%x)\n",
		    proto_err.perr_fmt.address,
		    proto_err.perr_fmt.address<<3);
	if (proto_err.perr_fmt.pointer_me)
	    kl_perr(BIT_LVL,"32<->32: initiator same as dir pointer\n");
	else
	    kl_perr(BIT_LVL,"32<->32: initiator not same as dir pointer\n");
	if (proto_err.perr_fmt.dir_state)
	    kl_perr(BIT_LVL,"36<->33: directory state: %s\n",
		    dir_state_str[proto_err.perr_fmt.dir_state >> 1]);
	if (proto_err.perr_fmt.priority)
	    kl_perr(BIT_LVL | KL_LOG_MSG,"37: requestor priority 0x%x\n",
		    proto_err.perr_fmt.priority);
	if (proto_err.perr_fmt.access)
	    kl_perr(BIT_LVL | KL_LOG_MSG,"39<->38: initiator access 0x%x\n",
		    proto_err.perr_fmt.access);
	if (proto_err.perr_fmt.msg_type)
	    kl_perr(BIT_LVL, "47<->40: type of request 0x%x\n",
		    proto_err.perr_fmt.msg_type);
	if (proto_err.perr_fmt.msg_type)
	    kl_perr(BIT_LVL | KL_LOG_MSG,"49<->48: backoff control 0x%x\n",
		    proto_err.perr_fmt.msg_type);
	if (proto_err.perr_fmt.initiator)
	    kl_perr(BIT_LVL,"60<->50: request initiator: Nasid %d, %s\n",
		    (proto_err.perr_fmt.initiator >> 2),
		    dir_owner_str[proto_err.perr_fmt.initiator & 3]);
	if (proto_err.perr_fmt.valid)	/* redundant check */
	    kl_perr (BIT_LVL, "63: valid protocol error\n");
    }
	
    err = md_err->hm_misc_err & MMCE_VALID_MASK;
	
    if (err) {
	kl_perr(REG_LVL, "HUB MD Misc Error Regiser: 0x%lx\n", err);
	for (i = 0; i < HMD_MISC_MAX_ERR; i++) {
	    if (err & (1 << i)) 
		if (i & ~MMCE_BAD_DATA_MASK)
		    kl_perr (BIT_LVL, "%s\n", hmd_misc_error[i]);
		else
		    kl_perr (BIT_LVL | KL_LOG_MSG, "%s\n", hmd_misc_error[i]);
	}
    }
    return;
}

static char *crbb_reqtype[] = {
	"Partial Read Cmd (PRDC)",
	"Block Read (BLKRD)",
	"DEX Eject Pending (DXPND)",
	"Eject Pending (EJPND)",
	"RSHU - BTE (BSHU)",
	"REXU - BTE (BEXU)",
	"RDEX",
	"WINV",
	"PIO Read (PIORD)",
	"PIO Write (PIOWR)",
	"WB",
	"Retained DEX cache line (DEX)",
	"Not WB and Not DEX (NWBDX)",
	"Not WB and Not DEX (NWBDX)",
	"Not WB and Not DEX (NWBDX)",
	"Not WB and Not DEX (NWBDX)",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown"
};

static char *crbb_initiator[] = {
	"Xtalk",
	"BTE_0, HUB_IO",
	"CrayLink",
	"CRB",
	"Unknown",
	"Bte_1",
	"Unknown",
	"Unknown"
};

static char *crbb_imsgtype[] = {
	"Xtalk",
	"BTE_0, HUB_IO",
	"CrayLink",
	"CRB"
};

static char *crbb_srcinittype[] = {
	"Processor 0",
	"Processor 1",
	"Guaranteed IO",
	"Normal IO"
};

static char *crbb_xtsize[] = {
	"Double word",
	"32 bytes",
	"128 bytes",
	"Rsvd"
};

/*
 * Function	: show_hub_io_error
 * Parameters	: io_err -> io error structure saved in hub error.
 * Purpose	: show the errors that are set in the IIO part of the HUB
 * Assumptions	: 
 * Returns	: None.
 */

void
show_hub_io_err(klhub_err_t *klhub_err, int dp_flag, nasid_t nasid)
{
    hubreg_t err;
    int crb;
    icrba_t  crba;
    icrbb_t  crbb;
    icrbc_t  crbc;
    hub_error_t *hub_err;
    hub_ioerr_t *io_err;

    hub_err = HUB_ERROR_STRUCT(klhub_err, dp_flag);
    io_err = &hub_err->hb_io;

    err = io_err->hi_wstat;
    if (err & IIO_WST_ERROR_MASK) {
	kl_perr(REG_LVL, "HUB IIO Widget Status Register: 0x%lx\n",
		err);
	kl_perr(BIT_LVL, "32: Error (crazy) bit set\n");
    }

    for (crb = 0; crb < IIO_NUM_CRBS; crb++) {
	crba.reg_value = io_err->hi_crb_entA[crb];
	if (crba.icrba_fields_s.error == 0)
	    continue;
	kl_perr(REG_LVL, "ICRB%x_A entry Register: 0x%lx\n", 
		crb, crba.reg_value);
	if (crba.icrba_fields_s.iow)
	    kl_perr(BIT_LVL,"00<->00: IO Write operation\n");
	if (crba.icrba_fields_s.valid)
	    kl_perr(BIT_LVL,"01<->01: CRB valid.\n");
	kl_perr(BIT_LVL,"39<->02: SN0Net address 0x%lx\n",
		crba.icrba_fields_s.addr);
	kl_perr(BIT_LVL | KL_LOG_MSG,"44<->40: TNUM of XTalk req. 0x%lx\n",
		crba.icrba_fields_s.tnum);
	kl_perr(BIT_LVL | KL_LOG_MSG,"48<->45: SIDN of XTalk req. 0x%lx\n",
		crba.icrba_fields_s.sidn);
	if (crba.icrba_fields_s.xerr)
	    kl_perr(BIT_LVL,
		    "49<->49: Error set in incoming XTalk request\n");
	if (crba.icrba_fields_s.mark)
	    kl_perr(BIT_LVL,
		    "50<->50: CRB entry has been marked\n");
	if (crba.icrba_fields_s.lnetuce)
	    kl_perr(BIT_LVL,
		    "51<->51: UCE detected on incoming data from SN0Net\n");
	if (crba.icrba_fields_s.mark)
	    kl_perr(BIT_LVL, "52<->55: CRB error code 0x%x\n",
		    crba.icrba_fields_s.ecode);
	kl_perr(BIT_LVL, "56<->56: CRB has an error\n");

	if ((brd_sversion(nasid, KLTYPE_IP27) > 2) &&
		(klhub_err->he_extension != -1)) {
		klhub_err_ext_t *klhub_err_ext = (klhub_err_ext_t *)
			NODE_OFFSET_TO_K1(NASID_GET(klhub_err),
			klhub_err->he_extension);

		crbb.reg_value = klhub_err_ext->hi_crb_entB[crb];
		kl_perr(REG_LVL | KL_FINAL_MSG, "ICRB%x_B entry Register: 0x%lx\n", 
			crb, crbb.reg_value);
#if 0
		if (crbb.icrbb_field_s.stall_intr)
		    kl_perr(BIT_LVL,"00<->00: Stall internal interrupts\n");
		if (crbb.icrbb_field_s.stall_ib)
		    kl_perr(BIT_LVL,"01<->01: Stall Ibuf\n");
		if (crbb.icrbb_field_s.intvn)
		    kl_perr(BIT_LVL,"02<->02: Intervention\n");
		if (crbb.icrbb_field_s.wb_pend)
		    kl_perr(BIT_LVL,"03<->03: Writeback pending\n");
		if (crbb.icrbb_field_s.hold)
		    kl_perr(BIT_LVL,"04<->04: Hold\n");
		if (crbb.icrbb_field_s.ack)
		    kl_perr(BIT_LVL,"05<->05: Received data Acknowledge\n");
		if (crbb.icrbb_field_s.resp)
		    kl_perr(BIT_LVL,"06<->06: Data Response given to processor\n");
		kl_perr(BIT_LVL,"07<->17: Invalidate Acknowledges cnt 0x%x\n",
			crbb.icrbb_field_s.ackcnt);
		kl_perr(BIT_LVL,"25<->29: Request type %s\n",
			crbb_reqtype[crbb.icrbb_field_s.reqtype]);
		kl_perr(BIT_LVL,"30<->32: Initiator %s\n",
			crbb_initiator[crbb.icrbb_field_s.initator]);
		kl_perr(BIT_LVL,"33<->40: Incoming message 0x%x\n",
			crbb.icrbb_field_s.imsg);
		kl_perr(BIT_LVL,"41<->42: Incoming message type %s\n",
			crbb_imsgtype[crbb.icrbb_field_s.imsgtype]);
		if (crbb.icrbb_field_s.useold)
		    kl_perr(BIT_LVL,"43<->43: Use Old command\n");
		kl_perr(BIT_LVL,"44<->45: Source Initiator %s\n",
			crbb_srcinittype[crbb.icrbb_field_s.srcinit]);
		kl_perr(BIT_LVL,"46<->54: Source Initiator node 0x%x\n",
			crbb.icrbb_field_s.srcnode);
		kl_perr(BIT_LVL,"55<->56: XTSize %s\n",
			crbb_xtsize[crbb.icrbb_field_s.xtsize]);
		if (crbb.icrbb_field_s.cohtrans)
		    kl_perr(BIT_LVL,"57<->57: Coherent transaction\n");
		kl_perr(BIT_LVL,"58<->58: Bte %d\n", crbb.icrbb_field_s.btenum);
#endif

		crbc.reg_value = klhub_err_ext->hi_crb_entC[crb];
		kl_perr(REG_LVL | KL_FINAL_MSG, "ICRB%x_C entry Register: 0x%lx\n", 
			crb, crbc.reg_value);
#if 0
		if (crbc.icrbc_field_s.gbr)
		    kl_perr(BIT_LVL,"00<->00: GBR bit set in xtalk packet\n");
		if (crbc.icrbc_field_s.doresp)
		    kl_perr(BIT_LVL,"01<->01: Xtalk req needs a resp\n");
		if (crbc.icrbc_field_s.barrop)
		    kl_perr(BIT_LVL,"02<->02: Barrier Op bit set in xtalk req\n");
		kl_perr(BIT_LVL,"03<->13: Suppl field 0x%x\n",
			crbc.icrbc_field_s.suppl);
		kl_perr(BIT_LVL,"14<->47: Push addr, Byte enable 0x%lx\n",
			crbc.icrbc_field_s.push_be);
		if (crbc.icrbc_field_s.bteop)
		    kl_perr(BIT_LVL,"48<->48: Bte Operation\n");
		kl_perr(BIT_LVL,"49<->52: Priority Pre scalar 0x%x\n",
			crbc.icrbc_field_s.pripsc);
		kl_perr(BIT_LVL,"53<->56: Priority count with Read Req 0x%x\n",
			crbc.icrbc_field_s.pricnt);
		if (crbc.icrbc_field_s.sleep)
		    kl_perr(BIT_LVL,"57<->57: Sleep mode\n");
#endif
	}
    }

    err = io_err->hi_bte0_sts;
    if (err & IBLS_ERROR) {
	kl_perr(REG_LVL, "HUB IIO BTE0 Status Register: 0x%lx\n",err);
	kl_perr(BIT_LVL,"15<->0: Transfer length (cache lines) 0x%x\n",
		(err & IBLS_LENGTH_MASK));
	kl_perr(BIT_LVL,"16: Error detected\n");
	kl_perr(REG_LVL | KL_FINAL_MSG | KL_LOG_MSG, 
		"HUB IIO BTE0 Source address Register: 0x%lx\n",
		io_err->hi_bte0_src);
	kl_perr(REG_LVL | KL_FINAL_MSG | KL_LOG_MSG, 
		"HUB IIO BTE0 Destinataion address Register: 0x%lx\n",
		io_err->hi_bte0_src);
    }
    err = io_err->hi_bte1_sts;
    if (err & IBLS_ERROR) {
	kl_perr(REG_LVL, "HUB IIO BTE1 Status Register: 0x%lx\n",err);
	kl_perr(BIT_LVL, "15<->0: Transfer length (cache lines) 0x%x\n",
		(err & IBLS_LENGTH_MASK));
	kl_perr(BIT_LVL, "16: Error detected\n");
	kl_perr(REG_LVL | KL_FINAL_MSG | KL_LOG_MSG, 
		"HUB IIO BTE1 Source address Register: 0x%lx\n",
		io_err->hi_bte1_src);
	kl_perr(REG_LVL | KL_FINAL_MSG | KL_LOG_MSG, 
		"HUB IIO BTE1 Destinataion address Register: 0x%lx\n",
		io_err->hi_bte1_src);
    }
}

/* 
 * hub_ni_vserr: vector status interpretation of hub NI
 */

const char *hub_ni_vserr[] = {
    "4: Adress error",
    "5: Command error",
    "6: Protection violation",
    "7: Unknown response or error",
};

#define HUB_NI_MAX_VSERR (sizeof(hub_ni_vserr)/ sizeof(hub_ni_vserr[0]))

/* 
 * hub_ni_peerr: Port Error register interpretation of errors.
 */

const char *hub_ni_peerr[] = {
    "24: Virtual channel 0 Receiver tail timeout",
    "25: Virtual channel 1 Receiver tail timeout",
    "26: Virtual channel 2 Receiver tail timeout",
    "27: Virtual channel 3 Receiver tail timeout",
    "28: Virtual channel 0 Credit timeout",
    "29: Virtual channel 1 Credit timeout",
    "30: Virtual channel 2 Credit timeout",
    "31: Virtual channel 3 Credit timeout",
    "32: Reserved",
    "33: Receiver's FIFO overflowed",
    "34: Message received with wrong destination",
    "35: Detected bad network message",
    "36: Detected bad internal message",
    "37: Detected a link reset",
};

#define HUB_NI_MAX_PEERR (sizeof(hub_ni_peerr) / sizeof(hub_ni_peerr[0]))

char *get_ni_port_error_bit(int bit)
{
#ifndef RO_CONST_FIXED
    /* Until the compilers are fixed (in 7.1), we can't trust them
     * to put constants in the read-only data section so they won't
     * be available if we lose network connectivity.
     */
    switch(bit) {
	case 0:
	    return "24: Virtual channel 0 Receiver tail timeout";
	case 1:
	    return "25: Virtual channel 1 Receiver tail timeout";
	case 2:
	    return "26: Virtual channel 2 Receiver tail timeout";
	case 3:
	    return "27: Virtual channel 3 Receiver tail timeout";
	case 4:
	    return "28: Virtual channel 0 Credit timeout";
	case 5:
	    return "29: Virtual channel 1 Credit timeout";
	case 6:
	    return "30: Virtual channel 2 Credit timeout";
	case 7:
	    return "31: Virtual channel 3 Credit timeout";
	case 8:
	    return "32: Reserved";
	case 9:
	    return "33: Receiver's FIFO overflowed";
	case 10:
	    return "34: Message received with wrong destination";
	case 11:
	    return "35: Detected bad network message";
	case 12:
	    return "36: Detected bad internal message";
	case 13:
	    return "37: Detected a link reset";
    }
    return "??: ???";
#else
    return hub_ni_peerr[bit];
#endif
}

void
show_ni_port_error(hubreg_t err, int level,
                     void (*pf)(int, char *, ...))
{
	int i;

	err >>= NPE_TAILTO_SHFT;
	for (i = 0; i < HUB_NI_MAX_PEERR; i++)
	    if (err & (1 << i))
		pf(level, "%s\n", get_ni_port_error_bit(i));
}


/*
 * Function	: show_hub_ni_err
 * Parameters	: ni_err -> ni error structure from hub error.
 * Purpose	: show the errors set in the NI part of the HUB ASIC.
 * Assumptions	: 
 * Returns	: None.
 */

void
show_hub_ni_err(hub_nierr_t *ni_err)
{
    hubreg_t err;
    int i;
    int cnt;
    int log;

    err = ni_err->hn_vec_sts;

    if ((err & NVS_VALID) && (err & NVS_ERROR_MASK)) {
	kl_perr(REG_LVL, "HUB NI VECTOR Status Register: 0x%lx\n", err);
	for (i = 0; i < HUB_NI_MAX_VSERR; i++)
	    if (err & (1 << i))
		kl_perr(BIT_LVL, "%s\n", hub_ni_vserr[i]);
    }

    if (err = ni_err->hn_port_err) {
	hubreg_t fatal =  NPE_LINKRESET | NPE_INTERNALERROR |
	    NPE_BADMESSAGE | NPE_BADDEST | NPE_FIFOOVERFLOW | 
		NPE_CREDITTO_MASK | NPE_TAILTO_MASK;
	if (err & fatal == 0) log = KL_LOG_MSG;
	else log = 0;

	kl_perr(REG_LVL | log, "HUB NI Port error Register: 0x%lx\n", err);
	if (cnt = (err & NPE_SNERRCOUNT_MASK) >> NPE_SNERRCOUNT_SHFT)
	    kl_perr(BIT_LVL | KL_LOG_MSG,
		    "07<->00:Number of LLP SN errors 0x%x\n", cnt);
	if (cnt = (err & NPE_CBERRCOUNT_MASK) >> NPE_CBERRCOUNT_SHFT)
	    kl_perr(BIT_LVL | KL_LOG_MSG,
		    "15<->08:Number of LLP CB errors 0x%x\n", cnt);
	if (cnt = (err & NPE_RETRYCOUNT_MASK) >> NPE_RETRYCOUNT_SHFT)
	    kl_perr(BIT_LVL | KL_LOG_MSG,
		    "23<->16: Number of LLP retries 0x%x\n", cnt);
	err >>= NPE_TAILTO_SHFT;
	for (i = 0; i < HUB_NI_MAX_PEERR; i++)
	    if (err & (1 << i))
		kl_perr(BIT_LVL | log, "%s\n", hub_ni_peerr[i]);
    }
}




/*
 * Function	: show_err_io_comp.
 * Parameters	: board -> board strucuter.
 *		: flag  -> show DUMP or LOG structure.
 * Purpose	: show errors that are found in components of a io board.
 * Assumptions	: 
 * Returns	: None.
 */
void
show_err_io_comp(lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;
    char slotname[100];

    /* 
     * Acceptable component is bridge, IOC3, SuperIO, SCSI, FDDI. 
     * PCI?
     */
    get_slotname(BOARD_SLOT(board), slotname);
    kl_perr(SLOT_LVL, "%s in /hw/module/%d/slot/%s \n", 
	    IONAME, 
	    KLCF_MODULE_ID(board),
	    slotname);
    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_BRI:
	    show_bridge_err(BRI_COMP_ERROR(board, component), edp);
	    break;
			
	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: fixme: show_err_io_comp: unhandled type %x",
		 KLCF_COMP_TYPE(component));
#endif /* ERR_DEBUG */
	    break;
	}
    }
}



char *br_isr_errs[] = {
    "08: GIO non-contiguous byte enable in crosstalk packet",
    "09: PCI to Crosstalk read request timeout",
    "10: PCI retry operation count exhausted.",
    "11: Device select timeout on PCI bus or GIO timeout",
    "12: PCI/GIO parity error interrupt status",
    "13: PCI Address/Cmd parity interrupt status",
    "14: Bridge detected parity error",
    "15: PCI abort condition interrupt status",
    "16: SSRAM parity error interrupt status",
    "17: LLP Transmitter Retry count interrupt status",
    "18: LLP Trx Retry required on the transmitter side of LLP",
    "19: LLP Rcv retry count interrupt status",
    "20: LLP Rcv check bit error, retry was required on LLP receiver",
    "21: LLP Rcvr sequence number error",
    "22: Request packet overflow",
    "23: Request operation not supported by bridge",
    "24: Request packet has invalid address for bridge widget",
    "25: Incoming request command word error bit set or invalid sideband",
    "26: Incoming response command word error bit set or invalid sideband",
    "27: Framing error, request cmd data size does not match actual",
    "28: Framing error, response cmd data size does not match actual",
    "29: Unexpected response arrived",
    "30: Access to SSRAM beyond device limits",
    "31: Multiple errors occurred",
};

#define BR_ISR_MAX_ERRS (sizeof(br_isr_errs) / sizeof(br_isr_errs[0]))



/*
 * Function	: show_bridge_err
 * Parameters	: bridge_err -> the bridge error structure.
 *		: flag	     -> which error structure to use: DUMP or LOG.
 * Purpose	: show the errors indicated by the bridge asic
 * Assumptions	: 
 * Returns	: None.
 */

void
show_bridge_err(klbridge_err_t *br_err, edump_param_t *edp)
{
    bridgereg_t err;
    int i;
    bridge_error_t *bridge_err = BRIDGE_ERROR_STRUCT(br_err, edp->dp_flag);
    int error_occurred = 0;

    /*
     * The multi-error bit gets set on recoverable errors so it will
     * almost definitely always be set when we get here. Ignore it
     * to avoid printing garbage.
     */
    if (!(bridge_err->br_int_status & ~(BRIDGE_ISR_INT_MSK|BRIDGE_ISR_MULTI_ERR)))
	return;
	
    kl_perr(ASIC_LVL, "Bridge ASIC errors:\n");
    if (err = bridge_err->br_int_status) {
	kl_perr(REG_LVL, "Bridge interrupt status register: 0x%x\n",
		err);
	kl_perr(BIT_LVL, "INT_N status: 0x%x\n",
		err & BRIDGE_ISR_INT_MSK);
	err >>= 8;
	for (i = 0; i < BR_ISR_MAX_ERRS; i++) {
	    if (err & (1 << i)) {
		error_occurred = 1;
		kl_perr(BIT_LVL, "%s\n", br_isr_errs[i]);
	    }
	}
    }
    if (bridge_err->br_err_cmd & WIDGET_ERROR) {
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"Bridge error command word register 0x%x\n", 
		bridge_err->br_err_cmd);
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"Bridge error upper address register 0x%x\n",
		bridge_err->br_err_upper);
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"Bridge error lower address register 0x%x\n",
		bridge_err->br_err_lower);
    }
    if (bridge_err->br_aux_err & WIDGET_ERROR) {
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"Bridge aux error command word register 0x%x\n", 
		bridge_err->br_aux_err);
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"Bridge response error upper address register 0x%x\n",
		bridge_err->br_resp_upper);
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"Bridge response error lower address register 0x%x\n",
		bridge_err->br_resp_lower);
    }
    /*
     * The following need to be revisited, and presented in a little
     * bit more detail.
     */
	   
    kl_perr(REG_LVL | KL_FINAL_MSG,
	    "Bridge SSRAM parity error register 0x%x\n", 
	    bridge_err->br_ram_perr);
    if (error_occurred) {
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"PCI/GIO error upper address register 0x%x\n", 
		bridge_err->br_pci_upper);
	kl_perr(REG_LVL | KL_FINAL_MSG,
		"PCI/GIO error lower address register 0x%x\n", 
		bridge_err->br_pci_lower);
    }
}

/*
 * Function	: show_err_xbow_comp.
 * Parameters	: board ->board structure for the xbow.
 * Purpose	: show errors that are found in components of a io board.
 * Returns	: None.
 */
void
show_err_xbow_comp(lboard_t *board, edump_param_t *edp)
{
    int comp;
    klinfo_t *component;
	
    kl_perr(SLOT_LVL, "%s in /hw/module/%d\n", MIDPLANE, 
	    KLCF_MODULE_ID(board));
    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
	component = KLCF_COMP(board, comp);
	switch (KLCF_COMP_TYPE(component)) {
	case KLSTRUCT_XBOW:
	    show_xbow_err(XBOW_COMP_ERROR(board, component), edp);
	    break;

	default:
#if defined (ERR_DEBUG)
	    (*edp->dp_printf)
		("Warning: show_err_xbow_comp: unknown component type %x\n", 
		 KLCF_COMP_TYPE(component));
#endif /* ERR_DEBUG */
	    break;
	}
    }
}



char *xbow_sts_err[] = {
    " 0: Multiple errors occurred",
    " 1: Reserved",
    " 2: Crosstalk error. (error bit set in cmd word or sideband",
    " 3: Reserved.",	"4: Reserved.",
    " 5: Register access error.",
    " 6: Reserved.", " 7: Reserved.", "08: Reserved.", "09: Reserved.",
    "10: Reserved.", "11: Reserved.", "12: Reserved.", "13: Reserved.",
    "14: Reserved.", "15: Reserved.", "16: Reserved.", "17: Reserved.",
    "18: Reserved.", "19: Reserved.", "20: Reserved.", "21: Reserved.",
    "22: Reserved.", 
    "23: Widget 0 interrupt request",
    "24: Link 8 interrupt request",
    "25: Link 9 interrupt request",
    "26: Link A interrupt request",
    "27: Link B interrupt request",
    "28: Link C interrupt request",
    "29: Link D interrupt request",
    "30: Link E interrupt request",
    "31: Link F interrupt request",
};

#define XBOW_STS_MAX_ERR	sizeof(xbow_sts_err)/sizeof(xbow_sts_err[0])


char *xbow_lnksts_err[] = {
    " 0: Packet time-out error source",
    " 1: Max request timeout",
    " 2: Reserved",
    " 3: LLP transmitter retry",
    " 4: LLP receiver error",
    " 5: LLP Max transmitter retry",
    " 6: LLP transmitter retry counter overflow",
    " 7: LLP receiver retry counter overflow",
    " 8: Link F, Bandwidth allocation error",
    " 9: Link E, Bandwidth allocation error",
    "10: Link D, Bandwidth allocation error",
    "11: Link C, Bandwidth allocation error",
    "12: Link B, Bandwidth allocation error",
    "13: Link A, Bandwidth allocation error",
    "14: Link 9, Bandwidth allocation error",
    "15: Link 8, Bandwidth allocation error",
    "16: Input overallocation error",
    "17: Illegal destination",
    "18: Multiple errors occurred",
};

#define XBOW_LNKSTS_MAX_ERR sizeof(xbow_lnksts_err)/sizeof(xbow_lnksts_err[0])
#define XBOW_MASK_ERRORS 	0xffffff07

void
show_xbow_err(klxbow_err_t *xb_err, edump_param_t *edp)
{
    xbow_error_t *xbow_err = XBOW_ERROR_STRUCT(xb_err, edp->dp_flag);
    xbowreg_t err;
    int link, i;

    kl_perr(ASIC_LVL, "XBOW signalled following errors.\n");
    if (err = xbow_err->xb_status) {
	kl_perr(REG_LVL, "XBOW Widget 0 status register: 0x%x\n", err);

	for (i = 0; i < XBOW_STS_MAX_ERR; i++) {
	    if (err & (1 << i))
		kl_perr(BIT_LVL, "%s\n", xbow_sts_err[i]);
	}
    }
    for (link = 0; link < MAX_XBOW_PORTS; link++) {
	if ((xbow_err->xb_link_aux_status[link] & XB_AUX_STAT_PRESENT) == 0)
	    continue;
	if ((err = xbow_err->xb_link_status[link]) == 0)
	    continue;
	if (err & XBOW_MASK_ERRORS) {
	    kl_perr(REG_LVL, "XBOW Link %x status register: 0x%x\n", 
		    (link + 8), err);
	}
	else {
	    kl_perr(REG_LVL | KL_FINAL_MSG | KL_LOG_MSG,
		    "XBOW Link %x status register: 0x%x\n", (link + 8), err);
	    continue;
	}
	
	for (i = 0; i < XBOW_LNKSTS_MAX_ERR; i++) {
	    if (err & (1 << i)) 
		kl_perr(BIT_LVL, "%s\n", xbow_lnksts_err[i]);
	}
    }
    if ((err = xbow_err->xb_err_cmd) & WIDGET_ERROR) {
	kl_perr(REG_LVL | KL_FINAL_MSG, "XBOW error command word register: 0x%x\n",
		err); 	
	kl_perr(REG_LVL | KL_FINAL_MSG, "XBOW error upper address register: 0x%x\n",
		xbow_err->xb_err_upper); 	
	kl_perr(REG_LVL | KL_FINAL_MSG, "XBOW error lower address register: 0x%x\n",
		xbow_err->xb_err_lower);
    }
}


/*
 * Dump router error bits for rr_error_status to a specified function
 * with the specified first parameter.
 */
void
show_rr_error_status(router_reg_t rr_error_status, int port, int level,
		     void (*pf)(int, char *, ...), int log_nonfatal)
{
    if (rr_error_status & RSERR_FIFOOVERFLOW)
       pf(level, "1: Port %d: Receiver FIFO overflow\n", port);
    if (rr_error_status & RSERR_ILLEGALPORT)
       pf(level, "2: Port %d: Received packet for illegal port\n", port);
    if (rr_error_status & RSERR_DEADLOCKTO_MASK)
       pf(level, "3: Port %d: Deadlock timeout bits = 0x%x\n", port,
	  (rr_error_status & RSERR_DEADLOCKTO_MASK) >> RSERR_DEADLOCKTO_SHFT);
    if (rr_error_status & RSERR_RECVTAILTO_MASK)
       pf(level, "4: Port %d: Receiver tail timeout bits = 0x%x\n", port,
	  (rr_error_status & RSERR_RECVTAILTO_MASK) >> RSERR_RECVTAILTO_SHFT);
    if (log_nonfatal) {
	if (rr_error_status & RSERR_RETRYCNT_MASK)
	   pf(level, "5: Port %d: Retry count = %d\n", port,
	      (rr_error_status & RSERR_RETRYCNT_MASK) >> RSERR_RETRYCNT_SHFT);
	if (rr_error_status & RSERR_CBERRCNT_MASK)
	   pf(level, "6: Port %d: Checkbit error count = %d\n", port,
	      (rr_error_status & RSERR_CBERRCNT_MASK) >> RSERR_CBERRCNT_SHFT);
	if (rr_error_status & RSERR_SNERRCNT_MASK)
	   pf(level, "7: Port %d: Sequence number error count = %d\n", port,
	      (rr_error_status & RSERR_SNERRCNT_MASK) >> RSERR_SNERRCNT_SHFT);
    }
}

void
show_rr_status_rev_id(router_reg_t status, int portmask, int level,
		      void (*pf)(int, char *, ...))
{
   int i;

   if (status & RSRI_LOCALTAILERR)
     pf(level, "24: Local tail error\n");
   if (status & RSRI_LOCALBADVEC)
     pf(level, "25: Bad vector\n");
   if (status & RSRI_LOCALSTUCK)
     pf(level, "26: Stuck bit\n");
   if (status & RSRI_LOCALSBERROR)
     pf(level, "27: Single bit error\n");
   for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
     if (!((1 << (i - 1)) & portmask))
	continue;
     switch ((status & (RSRI_LINKWORKING(i) | RSRI_LINKRESETFAIL(i)))
		>> RSRI_LSTAT_SHFT(i)) {
	case RSRI_LSTAT_WENTDOWN:
		pf(level, "    Port %d LINK FAILED\n", i);
		break;
	case RSRI_LSTAT_RESETFAIL:
		pf(level, "    Port %d LINK DIDN'T COME OUT OF RESET\n", i);
		break;
	case RSRI_LSTAT_LINKUP:
	case RSRI_LSTAT_NOTUSED:
		pf(level, "%d: Port %d Link working\n",
			  RSRI_LINKWORKING_BIT(i), i);
		break;
     }
     if(status & RSRI_LINK8BIT(i))
	 pf(level, "%d: Port %d Eight-bit mode\n",
		RSRI_LINK8BIT_BIT(i), i);
   }
}

/*
 * Function	: show_err_router_comp.
 * Parameters	: board -> Board structure for the router.
 * Purpose	: show errors that are found in components of a router board.
 * Assumptions	: 
 * Returns:	None.
 */
/*ARGSUSED */
void
show_err_router_comp(lboard_t *board, edump_param_t *edp)
{
    int i, comp;
    klinfo_t *component;
    klrouter_err_t *rr_error_info;
    hubreg_t rr_error_status;
    char slotname[100];

    get_slotname(BOARD_SLOT(board), slotname);
    kl_perr(SLOT_LVL, "%s in Module %d slot %s%d \n", ROUTERNAME, 
	    KLCF_MODULE_ID(board),
	    slotname);

    kl_perr(ASIC_LVL, "ROUTER signalled following errors.\n");

    for (comp = 0; comp < KLCF_NUM_COMPS(board); comp++) {
        component = KLCF_COMP(board, comp);
        switch (KLCF_COMP_TYPE(component)) {
           case KLSTRUCT_ROU:
                             rr_error_info = 0;
                             rr_error_info = ROU_COMP_ERROR(board, component);

                             for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
                                rr_error_status = rr_error_info->re_status_error[i];
                                if (rr_error_status)
                                   kl_perr(REG_LVL, "ROUTER Port %d: Error status register : 0x%lx \n", i, rr_error_status);
                                else
                                   continue;

				if (rr_error_status & RSERR_FIFOOVERFLOW)
                                   kl_perr(BIT_LVL, "1: Port %d: Receiver FIFO overflow \n", i);
				if (rr_error_status & RSERR_ILLEGALPORT)
                                   kl_perr(BIT_LVL, "2: Port %d: Received packet for illegal port \n", i);
                                if (rr_error_status & RSERR_DEADLOCKTO_MASK)
				   kl_perr(BIT_LVL, "3: Port %d: Deadlock timeout bits = 0x%x\n", i, (rr_error_status & RSERR_DEADLOCKTO_MASK) >> RSERR_DEADLOCKTO_SHFT);
                                if (rr_error_status & RSERR_RECVTAILTO_MASK)
				   kl_perr(BIT_LVL, "4: Port %d: Receiver tail timeout bits = 0x%x\n", i, (rr_error_status & RSERR_RECVTAILTO_MASK) >> RSERR_RECVTAILTO_SHFT);
                                if (rr_error_status & RSERR_RETRYCNT_MASK)
				   kl_perr(BIT_LVL | KL_LOG_MSG, "5: Port %d: Retry count = %d\n", i, (rr_error_status & RSERR_RETRYCNT_MASK) >> RSERR_RETRYCNT_SHFT);
                                if (rr_error_status & RSERR_CBERRCNT_MASK)
				   kl_perr(BIT_LVL | KL_LOG_MSG, "6: Port %d: Checkbit error count = %d\n", i, (rr_error_status & RSERR_CBERRCNT_MASK) >> RSERR_CBERRCNT_SHFT);
                                if (rr_error_status & RSERR_SNERRCNT_MASK)
				   kl_perr(BIT_LVL | KL_LOG_MSG, "7: Port %d: Sequence number error count = %d\n", i, (rr_error_status & RSERR_SNERRCNT_MASK) >> RSERR_SNERRCNT_SHFT);
                             }
                             break;
           default:
                             break;
        }
    }
}


