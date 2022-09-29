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
#include <sys/types.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/nvram.h>
#if !defined(FRUTEST) && !defined(_STANDALONE)
#include "../sn0_private.h"
#include <sys/debug.h>
#endif /* FRUTEST */
#include "sn0_fru_analysis.h"

#include <sys/SN/SN0/ip27config.h>

extern unchar get_nvreg(uint offset);

#ifdef FRUTEST
#include <stdarg.h>
#include <../../../sn_private.h>

extern kl_config_hdr_t		g_node[];
extern confidence_t		g_io_conf;
extern confidence_t		g_sn0net_conf;
extern confidence_t		g_xbow_conf;

#endif /* #ifdef FRUTEST */

#if defined(_STANDALONE)
#include <sys/SN/promcfg.h>
extern nasid_t		get_nasid(void);
#else
extern int		numnodes;
#endif	/* _STANDALONE */
/*
 * some global table declarations which are needed by the fru analyzer
 */


extern kf_rule_t	kf_rule_tab[];			/* table of rule encodings */
extern kf_cond_t	kf_cond_tab[];			/* table of condition encodings */
extern kf_action_set_t	kf_action_set_tab[];		/* table of action set encodings */
confidence_t		*kf_conf_tab[MAX_COMPS];	/* table of component confidence
							 * level pointers
							 */
hubreg_t		kf_reg_tab[MAX_ERR_REGS];	/* table of error register 
							 * pointers
							 */

kf_analysis_t		kf_analysis[MAX_GUESSES];	/* final analysis array */

int			(*kf_printf)(const char *,...);	/* printf function */

int			sn0_fru_enabled = 1;		/* to dynamically allow the fru analysis
							 * to be turned off in the debugger
							 */
confidence_t		software_confidence = 0;	/* placeholder for fru's software
							 * confidence 
							 */
kf_hint_t		node_hint[MAXCPUS / 2];	/* hints to the fru on a per-processor
							 * basis
							 */
char 			*fru_hint_table[] = {
	"kernel fault",
	"cache error"
};
int			fru_hint_table_size = sizeof(fru_hint_table) /
					      sizeof(fru_hint_table[0]);

int			kf_print_where = KF_PRINT_CONSOLE;	/* default */

#if defined(_STANDALONE)
int			fru_disable_confidence = ABSOLUTE_CONFIDENCE; 
                                                    /*never disable in prom*/
#else
extern int 		fru_disable_confidence;	/* mtune variable */
#endif
/*
 * get the cpu error info for a cpu of a given virtual id
 * ARGSUSED
 */

klcpu_err_t *
kf_cpu_err_info_get(lboard_t *board,klinfo_t **prev_cpu_comp,cpuid_t id)
{
	klinfo_t	*cpu_data;
	klcpu_err_t	*cpu_err;

	/* get the cpu component info for this cpu id */

	id		= id;
	cpu_data 	= find_component(board,*prev_cpu_comp,KLSTRUCT_CPU);
	*prev_cpu_comp 	= cpu_data;

	if (!cpu_data)
		return NULL;

	/* pull out the error info out of the component info */

	cpu_err = CPU_COMP_ERROR(board,cpu_data);
	KF_ASSERT(cpu_err);
	return cpu_err;
}

/*
 * get the error  info associated with the hub which lies on the
 * given IP27 board
 */

klhub_err_t *
kf_hub_err_info_get(lboard_t *board)
{
	klinfo_t	*hub_data;
	klhub_err_t	*hub_err;

	/* get the hub component info */
	hub_data = find_first_component(board,KLSTRUCT_HUB);

	KF_ASSERT(hub_data);

	/* pull out the error info out of the component info */

	hub_err = HUB_COMP_ERROR(board,hub_data);
	KF_ASSERT(hub_err);
	
	return hub_err;
	
}	
/*
 * get the router error info of the router component
 * on a given router board
 */

klrouter_err_t *
kf_router_err_info_get(lboard_t *board)
{
	klinfo_t	*router_data;
	klrouter_err_t	*router_err;
	lboard_t	*router_board;
	
	/* get the router board info */


	router_board = find_lboard(board,KLTYPE_ROUTER2);

	if (!router_board)
		return NULL;	/* might be a 2-cpu 1-hub system */

	/* get the router component info 
	 */
	
	router_data = find_first_component(router_board,KLSTRUCT_ROU);

	KF_ASSERT(router_data);

	/* pull out the error info out of the component info */

	router_err = ROU_COMP_ERROR(router_board, router_data);
	KF_ASSERT(router_err);
	
	return router_err;
	
}	
 /*
 * get the address of the confidence level of the memory & the dimm
 * on the node corr. to a given physical address
 */

kf_result_t
kf_mem_conf_get(paddr_t 	addr,
		confidence_t 	**mem_confidence,
		confidence_t 	**dimm_confidence)
{
	int		dimm;
	nasid_t		nasid;
	lboard_t	*board;		
	klinfo_t	*hub_info;
	klhub_err_t	*hub_err;
	
	nasid   	= NASID_GET(addr);
	dimm		= (addr & MEM_DIMM_MASK) >> MEM_DIMM_SHFT;

	if (is_valid_nasid(nasid)) {

#ifndef FRUTEST
		/* get the pointer to the start of the list of boards */
		board 		= (lboard_t *)KL_CONFIG_INFO(nasid);
#else
		board		= (lboard_t *)(g_node[nasid].ch_board_info);
#endif	/* #ifdef FRUTEST */
	} else {
		board = NULL;
	}

	if (!board) {
		*mem_confidence = NULL;		
		*dimm_confidence = NULL;
		return KF_FAILURE;
	}

	/* get the IP27 board structure on this node */
	board		= find_lboard(board,KLTYPE_IP27);

	if (!board)  {
		*mem_confidence = NULL;		
		*dimm_confidence = NULL;
		return KF_FAILURE;
	}

	/* get the hub component of this IP27 board */



	hub_info 	= KLCF_COMP(board,IP27_HUB_INDEX);
	KF_ASSERT(hub_info);

	/* get the hub error structure */
	hub_err		= HUB_COMP_ERROR(board,hub_info);
	KF_ASSERT(hub_err);


	/* finally get the reqd. mem confidence level address */
	*mem_confidence	= &(KF_MEM_CONF(hub_err));

	*dimm_confidence = &(KF_DIMM_CONF(hub_err,dimm));
        return KF_SUCCESS;

}


/*	
 * build the node part of the register address table and confidence level address table
 * by traversing the klconfig structure
 * SASHA will be building the io side of these tables
 */
kf_result_t
kf_node_tables_build(nasid_t nasid)
{
	lboard_t 	*board;
	klcpu_err_t	*cpu_err;	/* cpu error info */
	klhub_err_t	*hub_err;	/* hub error info */
	klrouter_err_t	*router_err;	/* router error info */
	int		i;
	klinfo_t	*cpu_comp;

#ifdef FRUTEST
	board	= (lboard_t *)g_node[nasid].ch_board_info;
#else
	board 	= (lboard_t *)KL_CONFIG_INFO(nasid);
#endif	/* #ifdef FRUTEST */
	board	= find_lboard(board,KLTYPE_IP27);


	cpu_comp = NULL;
	/* get the cpuA error info for this ip27 board */
	cpu_err = kf_cpu_err_info_get(board,&cpu_comp,0);

	if (cpu_err) {
		/* store the pointer to the cpuA cache error register 
		 * in  the error register address table
		 */
		kf_reg_tab[KF_CACHE_ERR0_INDEX]		= cpu_err->ce_cache_err_dmp.ce_cache_err;
		
		/* store the pointer to the confidence level of cpuA */
		
		kf_conf_tab[KF_CPU0_CONF_INDEX]		= &KF_CPU_CONF(cpu_err);
		kf_conf_tab[KF_IC0_CONF_INDEX]        	= &KF_ICACHE_CONF(cpu_err);
		kf_conf_tab[KF_DC0_CONF_INDEX]        	= &KF_DCACHE_CONF(cpu_err);
		kf_conf_tab[KF_SC0_CONF_INDEX]        	= &KF_SCACHE_CONF(cpu_err);
	}

	/* get the cpuB error info for this ip27 board */
	cpu_err = kf_cpu_err_info_get(board,&cpu_comp,1);

	if (cpu_err) {
		/* store the pointer to the cpuB cache error register 
		 * in  the error register address table
		 */
		kf_reg_tab[KF_CACHE_ERR1_INDEX]		= cpu_err->ce_cache_err_dmp.ce_cache_err;

		/* store the pointer to the confidence level of cpuB */	
		kf_conf_tab[KF_CPU1_CONF_INDEX]		= &KF_CPU_CONF(cpu_err);
		kf_conf_tab[KF_IC1_CONF_INDEX]        	= &KF_ICACHE_CONF(cpu_err);
		kf_conf_tab[KF_DC1_CONF_INDEX]        	= &KF_DCACHE_CONF(cpu_err);
		kf_conf_tab[KF_SC1_CONF_INDEX]        	= &KF_SCACHE_CONF(cpu_err);
	}
	
	/* get the hub error info for the hub in this ip27 board */
	hub_err	= kf_hub_err_info_get(board);
	KF_ASSERT(hub_err);
	/* 
	 * store the addresses of the hub error registers in the register address
	 * table 
	 */

	kf_reg_tab[KF_PI_ERR_INT_PEND_INDEX]	= KF_PI_ERR_INT_PEND(hub_err);
	kf_reg_tab[KF_PI_ERR_STS0_A_INDEX]	= KF_PI_ERR_STS0_A(hub_err);
	kf_reg_tab[KF_PI_ERR_STS0_B_INDEX]	= KF_PI_ERR_STS0_B(hub_err);
	kf_reg_tab[KF_PI_ERR_STS1_A_INDEX]	= KF_PI_ERR_STS1_A(hub_err);
	kf_reg_tab[KF_PI_ERR_STS1_B_INDEX]	= KF_PI_ERR_STS1_B(hub_err);


	kf_reg_tab[KF_MD_DIR_ERR_INDEX]		= KF_MD_DIR_ERR(hub_err);
	kf_reg_tab[KF_MD_MEM_ERR_INDEX]		= KF_MD_MEM_ERR(hub_err);
	kf_reg_tab[KF_MD_PROTO_ERR_INDEX]	= KF_MD_PROTO_ERR(hub_err);
	kf_reg_tab[KF_MD_MISC_ERR_INDEX]	= KF_MD_MISC_ERR(hub_err);

	kf_reg_tab[KF_II_WIDGET_STATUS_INDEX]	= KF_II_WIDGET_STATUS(hub_err);
	for(i = 0 ; i < IIO_NUM_CRBS;i++)
		kf_reg_tab[KF_II_CRB_ENT0_A_INDEX + i] = KF_II_CRB_ENTA(hub_err,i);
	kf_reg_tab[KF_II_BTE0_STS_INDEX]	= KF_II_BTE0_STS(hub_err);
	kf_reg_tab[KF_II_BTE1_STS_INDEX]	= KF_II_BTE1_STS(hub_err);
	kf_reg_tab[KF_II_BTE0_SRC_INDEX]	= KF_II_BTE0_SRC(hub_err);
	kf_reg_tab[KF_II_BTE1_SRC_INDEX]	= KF_II_BTE1_SRC(hub_err);
	kf_reg_tab[KF_II_BTE0_DST_INDEX]	= KF_II_BTE0_DST(hub_err);
	kf_reg_tab[KF_II_BTE1_DST_INDEX]	= KF_II_BTE1_DST(hub_err);

	kf_reg_tab[KF_NI_VECT_STS_INDEX]	= KF_NI_VECT_STS(hub_err);
	kf_reg_tab[KF_NI_PORT_ERR_INDEX]	= KF_NI_PORT_ERR(hub_err);
	
	/* hub error info structure contains the confidence levels of 
	 * hub , sysbus (connection to the processors), mem banks ,
	 * directory & memory buses. get them 
	 */
	kf_conf_tab[KF_SYSBUS_CONF_INDEX]   	= &KF_SYSBUS_CONF(hub_err);
	kf_conf_tab[KF_HUB_CONF_INDEX]   	= &KF_HUB_CONF(hub_err);
	kf_conf_tab[KF_HUB_LINK_CONF_INDEX] 	= &KF_HUB_LINK_CONF(hub_err);
	kf_conf_tab[KF_PI_CONF_INDEX]  		= &KF_PI_CONF(hub_err);
	kf_conf_tab[KF_MD_CONF_INDEX]  		= &KF_MD_CONF(hub_err);
	kf_conf_tab[KF_II_CONF_INDEX]  		= &KF_II_CONF(hub_err);
	kf_conf_tab[KF_NI_CONF_INDEX]  		= &KF_NI_CONF(hub_err);

	kf_conf_tab[KF_MEM_CONF_INDEX] 		= &KF_MEM_CONF(hub_err);

	for (i = 0; i < MAX_DIMMS; i++)
		kf_conf_tab[KF_DIMM0_CONF_INDEX + i] = &KF_DIMM_CONF(hub_err,i);


	/* get the router error info of the router connected to the
	 * hub on this ip27 board.
	 * pretty sure that router is a separate board as opposed to 
	 * being a component on the ip27 board.
	 * should change this properly.
	 */

	router_err = kf_router_err_info_get(board);
	if (router_err) {
		kf_conf_tab[KF_ROUTER_CONF_INDEX]     = &KF_ROUTER_CONF(router_err);
		for (i = 0; i < MAX_ROUTER_PORTS; i++) {
			kf_conf_tab[KF_ROUTER_LINK0_CONF_INDEX + i] = &KF_ROUTER_LINK_CONF(router_err,i);
			kf_reg_tab[KF_ROUTER_STS_ERR0_INDEX + i] = KF_ROUTER_STS_ERR(router_err,i);
		}
	}



#ifdef FRUTEST
	/* right now io confidence is a  separate variable.
	 * should incorporate this along with other confidence levels
	 * in the klconfig or change the relevant rules
	 */
	kf_conf_tab[KF_SOFTWARE_CONF_INDEX] 	= &(g_node[nasid].ch_sw_belief);
	kf_conf_tab[KF_XBOW_CONF_INDEX]		= &g_xbow_conf;
#else
	kf_conf_tab[KF_SOFTWARE_CONF_INDEX] 	= &(software_confidence);
#endif	/* #ifdef FRUTEST */

	return KF_SUCCESS;
}

/*
 * compute whether the  condition of a rule is true or false
 */
hubreg_t
kf_condition_compute(kf_cond_t *cond) {

	hubreg_t	eval_stack[MAX_COND_ELEMENTS];	
	                                        /* temporary place to hold the partially
						 * evaluated condition 
						 */
	kf_cond_t 	*curr_pos = cond; 	/* start at the beginning of the condition */
	int   	    	top = -1;		/* the index into the buffer where the next 
						 * literal will be inserted 
						 * -1 indicates the buffer is empty
						 */

	
	/* keep on going until the end of condition is reached */
	
	while(curr_pos->kc_type != KF_LIST_SENTINEL) {
		if (curr_pos->kc_type == KC_OPERAND) {

			/* currently we are looking at an operand in the condition
			 * perform the operation associated with the operand
			 */

			switch(KF_OPERAND(curr_pos)) {
			case KF_AND:
			{
				if (top < 1)
					return KF_FALSE;
				eval_stack[top-1] = eval_stack[top - 1] && eval_stack[top];	
				                                /* do a boolean-and of the top 2
								 * bits in the eval_stack
								 */
				top--;
				break;
			}
			case KF_OR:
			{
				if (top < 1)
					return KF_FALSE;
				eval_stack[top-1] = eval_stack[top - 1] || eval_stack[top];	
				                                /* do a boolean-or of the top 2
								 * bits in the eval_stack
								 */
				top--;
				break;
			}
			case KF_NOT:
			{
				if (top < 0)
					return KF_FALSE;
				eval_stack[top] = !eval_stack[top];	
				                                /* do a boolean-not of the top 
								 * bit in the eval_stack
								 */
				break;
			}
			}
		} else {
			/* currently we are looking at a literal in the condition
			 * evaluate it and put it on the eval stack
			 */
			top++;
			eval_stack[top] = KF_LITERAL_EVAL(curr_pos);
		}
		curr_pos++;
	}

	return eval_stack[0];
}

/*
 * perform the set of actions associated with a rule
 */
kf_result_t
kf_action_set_perform(kf_action_set_t *act_set)
{
	kf_action_set_t	*p = act_set;

	while(p->kas_type != KF_LIST_SENTINEL) {

		if (p->kas_type == KAS_ACTION) {
			if (kf_conf_tab[p->kas_conf_index]) {
				KF_CONF_UPDATE(*kf_conf_tab[p->kas_conf_index],
					       p->kas_conf_val);
			}
#ifdef FRU_DEBUG_RULES
			else {
				kf_print("Warning: conf tab index %d is NULL\n",
					 p->kas_conf_index);
			}
#endif
		}
		p++;
	}

	return KF_SUCCESS;
}
/*
 * trigger a rule 
 * should do proper validation
 */
kf_result_t
kf_rule_trigger(kf_rule_t *rule)
{
	if (kf_condition_compute(&kf_cond_tab[rule->kr_cond_tab_index])) {
		if (kf_action_set_perform(&kf_action_set_tab[rule->kr_action_set_tab_index]) != KF_SUCCESS) 
			return KF_FAILURE;
		KF_DEBUG_RULES("*\t\t\t\t\t\trule (%d , %d) triggered\n",
			       rule->kr_cond_tab_index,
			       rule->kr_action_set_tab_index);


	}
	return KF_SUCCESS;
}


extern nasid_t current_nasid;		/* nasid of the node whose
					 * error state is being analyzed
					 */
/*
 * analyze the list of local boards on a node 
 * for now we do ip27 analysis followed by router analysis
 * some work needs to be done on this function	
 */

kf_result_t
kf_node_analyze(nasid_t nasid,kf_analysis_t *curr_analysis)
{
	lboard_t 	*board;
	kf_result_t	rv = KF_SUCCESS;
#ifdef _STANDALONE
	int		i;
#endif

	KF_DEBUG("kf_node_analyze:doing node %d analysis......\n",nasid);

	current_nasid 	= nasid;
#ifndef FRUTEST
	/* Make sure that klconfig is setup properly */
	if (!(KL_CONFIG_CHECK_MAGIC(nasid))) {
	    printf("FRU:Local klconfig not setup properly\n");
	    return KF_FAILURE;
	}
#endif

	if (kf_node_tables_build(nasid) == KF_FAILURE)
	    return KF_FAILURE;	/* build the kf_conf_tab & kf_reg_tab */
#ifdef FRUTEST
	board	= find_lboard((lboard_t *)g_node[nasid].ch_board_info,KLTYPE_IP27);
#else
	board	= find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),KLTYPE_IP27);
#endif	/* #ifdef FRUTEST */

	if (curr_analysis) {
		/* Remember the nasid of the node for which this analysis
		 * is being collected 
		 */
		curr_analysis->kfa_nasid = nasid;
		curr_analysis->kfa_io_board = 0;
		curr_analysis->kfa_serial_number[0] = 0;

		if (kf_conf_tab[KF_SOFTWARE_CONF_INDEX] && 
		    *kf_conf_tab[KF_SOFTWARE_CONF_INDEX]) {			
			curr_analysis->kfa_info[KF_SOFTWARE_LEVEL].kfi_type 	= KFTYPE_SOFTWARE;
			curr_analysis->kfa_info[KF_SOFTWARE_LEVEL].kfi_inst 	= 0;
			curr_analysis->kfa_conf			= *kf_conf_tab[KF_SOFTWARE_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}

		curr_analysis->kfa_info[KF_MODULE_LEVEL].kfi_type = KFTYPE_MODULE;
		curr_analysis->kfa_info[KF_MODULE_LEVEL].kfi_inst = KLCF_MODULE_ID(board);
		curr_analysis->kfa_conf		    = 0;
	} else {
#ifdef _STANDALONE
		/* Initiliaze all the confidences (except io frus)to zeroes */
		for(i = 0 ; i <= KF_SOFTWARE_CONF_INDEX; i++)
			if (kf_conf_tab[i])
				*kf_conf_tab[i] = 0;
#endif
		/* If there is a kernel fault hint suspect the software */
		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
				      (node_hint[nasid].kh_hint_type == KF_HINT_KERNEL_FAULT),
				      90);
	}
	/* do the io analysis first so that xbow conf pointers etc. 
	 * get set up 
	 */
	kf_io_analyze(nasid,curr_analysis);

	/* analyze the ip27 board first  and then analyze the router board */

	while (board) {


		rv = kf_board_analyze(board,curr_analysis);	/* analyze the current board */
		if (rv != KF_SUCCESS)
			return rv;
		board = KLCF_NEXT(board);	/* go to the next board on this
						 * node 
						 */
	}	


	
	KF_DEBUG("kf_node_analyze:finished node %d analysis\n",nasid);

	return rv;

}

kf_result_t
kf_rule_tab_analyze(int	start_index)
{

	int 		i = start_index;

	KF_DEBUG("\t\t\t\t\tdoing rule table driven analysis.......\n");

	while (kf_rule_tab[i].kr_cond_tab_index != KF_LIST_SENTINEL) {
		if (kf_rule_trigger(&kf_rule_tab[i]) != KF_SUCCESS)
			return KF_FAILURE;
		i++;
	}

	KF_DEBUG("\t\t\t\t\tfinished  rule table driven analysis\n");


	return KF_SUCCESS;
}

#ifndef FRUTEST
/* entry point into the fru analyzer */
int
sn0_fru_entry(int (*print)(const char *,...))
{
	int		cnode;
	kf_analysis_t	curr_analysis;
#if defined(_STANDALONE)
	/* Traversed nodes is used to keep track off the way
	 * we traverse the nodes. It can be done either 
	 * through the GDA or Prom config.
	 */
	int		traversed_nodes = 0; 
	gda_t 		*gdap = GDA;
	pcfg_hub_t	*hub_cf;
#else
	static	int	nestedcall = 0;

	/* to eliminate recursive calls to fru code */
	if (nestedcall)
		return KF_FAILURE;
	else	
		nestedcall = 1;

#endif

	kf_printf	= print; 
	
	/* Print the starting fru analysis message */
	kf_fru_begin_msg_print();

	/* analyze the various nodes in the system */
#if !defined(_STANDALONE)
	for(cnode = 0; cnode < numnodes; cnode++) {
		nasid_t	 nasid;

		nasid = COMPACT_TO_NASID_NODEID(cnode);
		
		if (kf_node_analyze(nasid,NULL) == KF_FAILURE)
			kf_print("FRU : node (cnodeid = %d , nasid = %d)analysis failed\n",
				 cnode,nasid);
	}
#else
	/* initiliaze the node hint table */        
	for(cnode = 0 ; cnode < MAXCPUS / 2 ; cnode++)
		node_hint[cnode].kh_hint_type = -1;

	if ((gdap) && (gdap->g_magic == GDA_MAGIC) &&
	    (gdap->g_nasidtable[1] != INVALID_NASID)) {
		nasid_t	nasid;
		traversed_nodes = 1;

		for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++) {
			if ((nasid = gdap->g_nasidtable[cnode]) ==  
			    INVALID_NASID) 
				continue;
			if (kf_node_analyze(nasid,NULL) == KF_FAILURE)
				kf_print("FRU : node (cnodeid = %d ,"
					 "nasid = %d) analysis failed\n",
					 cnode,nasid);
		}	
	}

	if (traversed_nodes == 0) {


		/* Do the analysis for the node which started
		 * fru analysis first.
		 */
		if (kf_node_analyze(get_nasid(),NULL) 
		    == KF_FAILURE)
			kf_print("FRU : node (cnodeid = %d ,"
				 "nasid = %d) analysis failed\n",
				 cnode,get_nasid());
			

		for(cnode = 0; cnode < PCFG(get_nasid())->count;cnode++) {
			nasid_t	nasid;
			hub_cf = &PCFG_PTR(get_nasid(),cnode)->hub;
			if (hub_cf->type != PCFG_TYPE_HUB)
				continue;
			if (((nasid = hub_cf->nasid) != INVALID_NASID) && 
			    (nasid != get_nasid()))  {
				if (kf_node_analyze(nasid,NULL) == KF_FAILURE)
					kf_print("FRU : node (cnodeid = %d ,"
						 "nasid = %d) analysis failed\n",
						 cnode,nasid);
			}
		}
	}
	else
		traversed_nodes = 0;
#endif /* !_STANDALONE */
	/* do this initialization once so that all the 
	 * node analyses' guesses are preserved
	 */
	kf_analysis_tab_init();
	/* analyze the various nodes in the system */
#if !defined(_STANDALONE)
	for(cnode = 0; cnode < numnodes; cnode++) {
		nasid_t	 nasid;

		nasid = COMPACT_TO_NASID_NODEID(cnode);
		kf_analysis_init(&curr_analysis);		

		if (kf_node_analyze(nasid,&curr_analysis) == KF_FAILURE)
			kf_print("FRU : node (cnodeid = %d , nasid = %d)analysis failed\n",
				 cnode,nasid);
	}
#else
	if ((gdap) && (gdap->g_magic == GDA_MAGIC) &&
	    (gdap->g_nasidtable[1] != INVALID_NASID)) {
		nasid_t	nasid;
		traversed_nodes = 1;
		for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++) {
			if ((nasid = gdap->g_nasidtable[cnode]) ==  
			    INVALID_NASID)
				continue;
         	       kf_analysis_init(&curr_analysis);

			if (kf_node_analyze(nasid,&curr_analysis) == KF_FAILURE)
				kf_print("FRU : node (cnodeid = %d ,"
					 "nasid = %d) analysis failed\n",
					 cnode,nasid);
		}	
	}

	if (traversed_nodes == 0) {
                kf_analysis_init(&curr_analysis);


		/* Do the analysis for the node which started
		 * fru analysis first.
		 */

		if (kf_node_analyze(get_nasid(),&curr_analysis) 
		    == KF_FAILURE)
			kf_print("FRU : node (cnodeid = %d ,"
				 "nasid = %d) analysis failed\n",
				 cnode,get_nasid());
			
		for(cnode = 0; cnode < PCFG(get_nasid())->count;cnode++) {
			nasid_t	nasid;
			hub_cf = &PCFG_PTR(get_nasid(),cnode)->hub;
			if (hub_cf->type != PCFG_TYPE_HUB)
				continue;
			if (((nasid = hub_cf->nasid) != INVALID_NASID) &&
			    (nasid != get_nasid()))   {
				kf_analysis_init(&curr_analysis);
				if (kf_node_analyze(nasid,&curr_analysis) 
				    == KF_FAILURE)
					kf_print("FRU : node (cnodeid = %d ,"
						 "nasid = %d) analysis failed\n",
						 cnode,nasid);
			}
		}
	} else
		traversed_nodes = 0;


#endif /* !_STANDALONE */
#ifdef DEBUG
	kf_fru_summary_print(MAX_GUESSES);
#else
	kf_fru_summary_print(1);
#endif
	return 1;
}
#endif 

/* check if two analysis structure are equal in terms
 * of the heirarchical path they correspond to
 * if they are equal then if analysis a2's confidence is more
 * than that of a1's then return 2 otherwise return 1
 * this function is used when trying to find an analysis guess
 * with the least confidence level in the global array of guesses.
 * basically we try to see if there is already an analysis guess
 * which is identical to the current analysis guess that we are
 * trying to store. if there is one store the one with the higher
 * confidence level.
 */
int
kf_analysis_eq(kf_analysis_t *a1,kf_analysis_t *a2) 
{
	int	level;

	for (level = 0 ; level < MAX_LEVELS; level++) {
		if ((a1->kfa_info[level].kfi_type != a2->kfa_info[level].kfi_type) ||
		    (a1->kfa_info[level].kfi_inst != a2->kfa_info[level].kfi_inst))
			return 0;
	}
	return ((a1->kfa_conf > a2->kfa_conf) ? 1 : 2 );
}

/* removes the analysis guess with the lower confidence
 * level if two identical analysis guesses were found
 */
int
kf_analysis_dup_remove(kf_analysis_t *a)
{
	int 	guess;

	for (guess = 0; guess < MAX_GUESSES; guess++) {
		if (kf_analysis_eq(&kf_analysis[guess],a) == 2) {
			kf_analysis[guess] = *a;
			return 1;
		}
	}
	return 0;
}

/* find the analysis guess among the list of stored analysis
 * guesses which corresponds to the lowest confidence level
 */
kf_analysis_t *
kf_analysis_info_find_least(kf_analysis_t *curr_analysis) 
{
	int		i;
	kf_analysis_t	*min_analysis;


	if (kf_analysis_dup_remove(curr_analysis))
		return NULL;
	min_analysis = &(kf_analysis[0]);
	for(i = 0 ; i < MAX_GUESSES; i++) {
		if (min_analysis->kfa_conf > kf_analysis[i].kfa_conf)
			min_analysis = &(kf_analysis[i]);
	}

	return min_analysis;
}

/* put the current analysis guess in the table of guesses 
 * if there is a guess with a lower confidence level
 */
kf_result_t
kf_guess_put(kf_analysis_t *curr_analysis)
{
	kf_analysis_t 	save_curr_analysis;
	kf_analysis_t	*least_conf_anal_info;

	save_curr_analysis = *curr_analysis;

	if ((least_conf_anal_info = kf_analysis_info_find_least(curr_analysis)) == NULL) {
		*curr_analysis = save_curr_analysis;
		return KF_SUCCESS;
	}
	if (least_conf_anal_info->kfa_conf < curr_analysis->kfa_conf) {
	    *least_conf_anal_info	= *curr_analysis;
	}

	*curr_analysis = save_curr_analysis;
	return KF_SUCCESS;
	
}

/* initialize an analysis guess */
void 
kf_analysis_init(kf_analysis_t *analysis) 
{
	int level;

	for(level = 0 ; level < MAX_LEVELS; level++) {
		analysis->kfa_info[level].kfi_type = KF_UNKNOWN;
		analysis->kfa_info[level].kfi_inst = KF_UNKNOWN;
	}
}

/* initialize the list of analysis guesses */
void
kf_analysis_tab_init(void)
{
	int 	guess;
	
	for (guess = 0 ; guess < MAX_GUESSES; guess++)  {
		kf_analysis[guess].kfa_conf = 0;

		kf_analysis_init(&kf_analysis[guess]);
	}
}

typedef  struct fru_name_s {
	int	type;
	char	type_name[30];
} fru_name_t;

#if defined(DEBUG) && !defined(_STANDALONE)
char error_msg[25];
#endif

char *
type_name_get(int type,fru_name_t *name,int len)
{	
	int i;

	for (i = 0 ; i < len ; i++)
		if (type == name[i].type)
			return name[i].type_name;

	return "unknown";
}

/* print out an  analysis guess */


#define KF_TYPE_NAME_LEN	 sizeof(type_name)/ sizeof(fru_name_t)

void
kf_bridge_analysis_print(kf_analysis_t *anal, int level, 
			 fru_name_t type_name[], int name_len)
{
	int type = anal->kfa_info[++level].kfi_type;

	/* if there is something below the bridge? */
	switch (type) {
	case KFTYPE_BRIDGE_PCI_DEV:
		kf_print("/%s/%d\t\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_info[level].kfi_inst,
			 anal->kfa_conf);
#if !defined(FRUTEST) && !defined(_STANDALONE)
		if (anal->kfa_conf >= fru_disable_confidence) {
			kf_pci_component_disable(
			 anal->kfa_io_board,			 
			 anal->kfa_info[KF_MODULE_LEVEL].kfi_inst,
			 anal->kfa_info[KF_WIDGET_LEVEL].kfi_inst,
			 anal->kfa_info[KF_BRIDGE_PCI_DEV_LEVEL].kfi_inst);
		}
#endif /* !FRUTEST && !_STANDALONE */
		break;
	
	case KFTYPE_BRIDGE_SSRAM:
		kf_print("/bridge/%s\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_conf);
		break;
	
	case KFTYPE_BRIDGE_LINK:
		kf_print("/%s\t\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_conf);
		break;

	/* nope,the bridge is the suspected component */
	case KF_UNKNOWN:
		type = anal->kfa_info[--level].kfi_type;

		kf_print("/bridge\t\t: %d%%\n",
			 anal->kfa_conf);
		break;
	default:
#ifdef DEBUG
		kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
			 type, __LINE__, __FILE__);
#else
		kf_print("<undefined>\n");
#endif
		break;
	}
}


void
kf_cpu_analysis_print(kf_analysis_t *anal, int level, fru_name_t type_name[],
		      int name_len)
{
	int type = anal->kfa_info[++level].kfi_type;
	int slice = anal->kfa_info[level - 1].kfi_inst ;
#if !defined(FRUTEST) && !defined(_STANDALONE)
	/* If the confidence is more than the tunable "fru_disable_confidence"
	 * then disable the cpu 
	 */
	if (anal->kfa_conf >= fru_disable_confidence)
		kf_cpu_disable(slice,anal->kfa_nasid);
#endif /* !FRUTEST && !_STANDALONE */
	kf_print("/cpu/%c", (slice ? 'b' : 'a'));
	
	if (type == KF_UNKNOWN) {
		type = anal->kfa_info[--level].kfi_type;
		
		kf_print("\t\t\t: %d%%\n", anal->kfa_conf);		
	}
	else {
		kf_print("/%s\t\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_conf);		
	}

}


void
kf_memory_analysis_print(kf_analysis_t *anal, int level,
			 fru_name_t type_name[], int name_len)
{
	int type = anal->kfa_info[++level].kfi_type;
	int bank_dimm = anal->kfa_info[level].kfi_inst;
	if (type == KFTYPE_DIMM0) {

#if !defined(FRUTEST) && !defined(_STANDALONE)
		/* Disable the bank if the confidence is above the
		 * tunable "fru_disable_confidence"
		 */
		if (anal->kfa_conf >= fru_disable_confidence)
			kf_memory_bank_disable(bank_dimm,anal->kfa_nasid);
#endif /* !FRUTEST && !_STANDALONE */
#ifdef FRUTEST
		if (0) {
#else
		if (SN00) {
#endif
			/* Speedo dimm callout 
			 * bank 0 --> slots 1 & 2
			 * bank 1 --> slots 3 & 4
			 * bank 2 --> slots 5 & 6
			 * bank 3 --> slots 7 & 8
			 */
			if (bank_dimm < MAX_DIMM_BANKS_SN00)
 				kf_print("/%s/%d\n++\t\t\t\t"
 					 "[Physical Bank Labels %d or %d]\t\t: %d%%\n",
					 type_name_get(type,type_name,name_len),
					 anal->kfa_info[level].kfi_inst,
					 bank_dimm * 2 + 1,
					 bank_dimm * 2 + 2,
					 anal->kfa_conf);
			else
 				kf_print("/%s/%d\n++\t\t\t\t: %d%%\n",
					 type_name_get(type,type_name,name_len),
					 anal->kfa_info[level].kfi_inst,
					 anal->kfa_conf);

				

		} else {
			/* Lego dimm callout */
			kf_print("/%s/%d\n++\t\t",
				 type_name_get(type,type_name,name_len),
				 anal->kfa_info[level].kfi_inst);
			kf_print("[MM%cH%d OR MM%cL%d OR DIR%c%d",
				 bank_dimm < 4 ? 'X' : 'Y',
				 bank_dimm,
				 bank_dimm < 4 ? 'X' : 'Y',
				 bank_dimm,
				 bank_dimm < 4 ? 'X' : 'Y',
				 bank_dimm);
			kf_print("(premium mode only)]\t: %d%%\n",
				 anal->kfa_conf);
		}
		
	}
	else if (type == KF_UNKNOWN) {
		type = anal->kfa_info[--level].kfi_type;
		
		kf_print("/%s\t\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_conf);
		
	}
	else {
#ifdef DEBUG
		kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
			 type, __LINE__, __FILE__);
#else
		kf_print("<undefined>\n");	
#endif
	}
	
}


void
kf_hub_analysis_print(kf_analysis_t *anal, int level, fru_name_t type_name[],
		      int name_len)
{
	int type = anal->kfa_info[++level].kfi_type;

	switch(type) {
	case KFTYPE_HUB_LINK:
		kf_print("\n");
		kf_print("++\t\t[suspect faulty midplane connection]\t: %d%%\n",
			 anal->kfa_conf);
		break;
	case KFTYPE_MD:
		type = anal->kfa_info[++level].kfi_type;
		
		if (type == KFTYPE_MEM) {
			kf_memory_analysis_print(anal,level,type_name,
						 name_len);
		}
		else if (type == KF_UNKNOWN) {
			type = anal->kfa_info[--level].kfi_type;
			
			kf_print("/hub/%s\t\t: %d%%\n",
				 type_name_get(type,type_name,
					       name_len),
				 anal->kfa_conf);
		}
		else {
#ifdef DEBUG
			kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
				 type, __LINE__, __FILE__);
#else
			kf_print("<undefined>\n");
#endif
		}		
	break;

	case KFTYPE_II:
	case KFTYPE_PI:
	case KFTYPE_NI:
		kf_print("/hub/%s\t\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_conf);
	break;

	case KFTYPE_T5_WB_SURPRISE:
		kf_print("\n");
		kf_print("++\tFailed with the incident (T5 WB) error "
			 "signature,\n++\tplease contact your service "
			 "representaive.\t: %d%%\n",
			 anal->kfa_conf);
		break;
		

	case KFTYPE_BTE_PUSH :
		kf_print("\n");
		kf_print("++\tFailed with the incident (BTE PUSH) error "
			 "signature,\n++\tplease contact your service "
			 "representaive.\t: %d%%\n",
			 anal->kfa_conf);
		break;
		

	case KF_UNKNOWN:
		type = anal->kfa_info[--level].kfi_type;
		
		kf_print("/%s\t\t\t: %d%%\n",
			 type_name_get(type,type_name,name_len),
			 anal->kfa_conf);
	break;

	default:
#ifdef DEBUG
		kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
			 type, __LINE__, __FILE__);	
#else
		kf_print("<undefined>\n");
#endif
		break;
	}
}

void 
kf_analysis_print(kf_analysis_t *anal)
{
	int 			level;
	int             	type;
	char slot_name[SLOTNUM_MAXLENGTH];
	static fru_name_t 	type_name[] = 
	{
		{KFTYPE_WEIRDCPU,       "unknown_cpu"},
		{KFTYPE_IP27,		"node"},

		{KFTYPE_WEIRDIO,	"unknown"},
		{KFTYPE_BASEIO,		"baseio"},
		{KFTYPE_4CHSCSI,	"mscsi"},
		{KFTYPE_ETHERNET,       "menet"},
		{KFTYPE_FDDI,		"fddi"},
		{KFTYPE_GFX,            "graphics"},
		{KFTYPE_HAROLD,		"pci_xio"},
		{KFTYPE_PCI,		"pci"},
		{KFTYPE_VME,		"vme_remote"},
		{KFTYPE_MIO,		"mio"},
		{KFTYPE_FC,		"fibre_channel"},
		{KFTYPE_LINC,		"linc"},

		{KFTYPE_WEIRDROUTER,    "unknown_router"},
		{KFTYPE_NULL_ROUTER,   	"null_router"},
		{KFTYPE_META_ROUTER,   	"meta_router"},
		{KFTYPE_WEIRDMIDPLANE, 	"unknown_midplane"},
		{KFTYPE_MIDPLANE,      	"midplane"},

		{KFTYPE_CPU0,		"cpu"},
		{KFTYPE_IC0,		"icache"},
		{KFTYPE_DC0,		"dcache"},
		{KFTYPE_SC0,		"scache"},

		{KFTYPE_SYSBUS,		"himm"},
		{KFTYPE_HUB,		"hub"},
		{KFTYPE_PI,		"pi"},
		{KFTYPE_MD,		"md"},
		{KFTYPE_II,		"ii"},
		{KFTYPE_NI,		"ni"},

		{KFTYPE_MEM,		"memory"},
		{KFTYPE_DIMM0,		"memory/dimm_bank"},
		{KFTYPE_ROUTER,		"router"},
		{KFTYPE_ROUTER_LINK0,  	"link"},
		{KFTYPE_SOFTWARE,      	"Software"},
		{KFTYPE_XBOW,		"xbow"},
		{KFTYPE_BRIDGE_LINK,   	"link"},
		{KFTYPE_BRIDGE,		"bridge"},
		{KFTYPE_BRIDGE_PCI_DEV,	"pci"},
		{KFTYPE_BRIDGE_SSRAM,  	"ssram"},
		{KFTYPE_MODULE,         "module"}
	};

	level = KF_MODULE_LEVEL;
	type = anal->kfa_info[level].kfi_type;

	switch (type) {
	case KFTYPE_SOFTWARE:
		kf_print("++\t%s\t\t\t\t\t: %d%%\n",
			 type_name_get(type,type_name,KF_TYPE_NAME_LEN),
			 anal->kfa_conf);
		return;

	case KFTYPE_MODULE:
		if (anal->kfa_serial_number[0])
			kf_print("++\t[board serial number %s]\n",anal->kfa_serial_number);
		kf_print("++\t/hw/%s/%d/",
			 type_name_get(type,type_name,KF_TYPE_NAME_LEN),
			 anal->kfa_info[level].kfi_inst);
		break;

	case KF_UNKNOWN:
		/* no confideces were assigned so just return */
		return;

	default:
#ifdef DEBUG
		kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
			 type, __LINE__, __FILE__);	
#else
		kf_print("<undefined>\n");
#endif
		break;
	}

	type = anal->kfa_info[++level].kfi_type;

	switch (type) {
	case KFTYPE_ROUTER2:
	case KFTYPE_ROUTER:
#ifdef FRUTEST
	  kf_print("slot/r%d/%s",
		   anal->kfa_info[level].kfi_inst,
		   type_name_get(type,type_name,KF_TYPE_NAME_LEN));
#else	
	  /* convert encoded slot number to slot name string */
	  get_slotname(anal->kfa_info[level].kfi_inst, 
		       slot_name);
	  
	  kf_print("slot/%s/%s",slot_name,
		   type_name_get(type,type_name,KF_TYPE_NAME_LEN));
#endif
		type = anal->kfa_info[++level].kfi_type;
		
		if (type == KFTYPE_ROUTER_LINK0) {
			kf_print("/%s/%d\t\t\t: %d%%\n",
				 type_name_get(type,type_name,KF_TYPE_NAME_LEN),
				 anal->kfa_info[level].kfi_inst,
				 anal->kfa_conf);
		
		}
		else if (type == KF_UNKNOWN) {
			type = anal->kfa_info[--level].kfi_type;

			kf_print("\t\t\t\t\t: %d%%\n",
				 anal->kfa_conf);
			
		}
		else {
#ifdef DEBUG
			kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
				 type, __LINE__, __FILE__);
#else
			kf_print("<undefined>\n");
#endif
		}
		return;

	case KFTYPE_IP27:
#ifdef FRUTEST
		kf_print("slot/n%d/%s",
			 anal->kfa_info[level].kfi_inst + 1,
			 type_name_get(type,type_name, KF_TYPE_NAME_LEN));
#else
		/* convert encoded slot number to slot name string */
		get_slotname(anal->kfa_info[level].kfi_inst,slot_name);

		kf_print("slot/%s/%s",
			 slot_name,
			 type_name_get(type,type_name, KF_TYPE_NAME_LEN));
#endif
		type = anal->kfa_info[++level].kfi_type;

		switch (type) {
		case KFTYPE_CPU0:
			kf_cpu_analysis_print(anal,level,type_name,
					      KF_TYPE_NAME_LEN);
			break;

		case KFTYPE_SYSBUS:
			kf_print("/%s\t\t\t: %d%%\n",
				 type_name_get(type,type_name,
					       KF_TYPE_NAME_LEN),
				 anal->kfa_conf);
			
			break;

		case KFTYPE_HUB:
			kf_hub_analysis_print(anal,level,type_name,
					      KF_TYPE_NAME_LEN);
			break;

		case KFTYPE_PCOUNT:
			kf_print("\n");
			kf_print("++\tFailed with the incident 439797 error "
				 "signature,\n++\tplease contact your service "
				 "representative.\t: %d%%\n",
				 anal->kfa_conf);
			break;

		case KF_UNKNOWN:
			type = anal->kfa_info[level - 1].kfi_type;
			kf_print("\t: %d%%\n",
				 anal->kfa_conf);
			break;

		default:
#ifdef DEBUG
			kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
				 type, __LINE__, __FILE__);
#else
			kf_print("<undefined>\n");
#endif
			break;
		}
		break;

	case KFTYPE_XBOW:
#ifdef FRUTEST
		kf_print("slot/io%d/node/xtalk/0/xbow\t\t: %d%%\n",
			 anal->kfa_info[level].kfi_inst,
			 anal->kfa_conf);
#else
		/* convert encoded slot number to slot name string */
		get_slotname(anal->kfa_info[level].kfi_inst, 
			     slot_name);

		kf_print("slot/%s/node/xtalk/0/xbow\t\t: %d%%\n",
			 slot_name,
			 anal->kfa_conf);
		
#endif
		break;

	default:
		if (KLCLASS(type) == KLCLASS_IO) {
			/* if we have an io board */
#ifdef FRUTEST
			kf_print("slot/io%d/%s",
				 anal->kfa_info[level].kfi_inst,
				 type_name_get(type,type_name,KF_TYPE_NAME_LEN));
#else
			/* convert encoded slot number to slot name string */
			get_slotname(anal->kfa_info[level].kfi_inst, 
				     slot_name);
			
			kf_print("slot/%s/%s",slot_name,
				 type_name_get(type,type_name,KF_TYPE_NAME_LEN));
#endif
			
			type = anal->kfa_info[++level].kfi_type;
			
			if (type == KFTYPE_BRIDGE) {
				kf_bridge_analysis_print(anal,level,type_name, 
							 KF_TYPE_NAME_LEN);
			}
			else if (type == KF_UNKNOWN) {
				kf_print("\t\t\t: %d%%\n",
					 anal->kfa_conf);
			}
			else {
#ifdef DEBUG
				kf_print("\nError: Unknown KF_TYPE %d line: %d"
					 "file:%s\n",
					 type, __LINE__, __FILE__);
#else
				kf_print("<undefined>\n");
#endif

			}

		}

		else {
#ifdef DEBUG
		kf_print("\nError: Unknown KF_TYPE %d line: %d file:%s\n",
			 type, __LINE__, __FILE__);
#else
		kf_print("<undefined>\n");	

#endif
		}
		break;
	}
}



/* sort the table of analysis guesses in
 * descending order of confidence levels
 */
void
kf_analysis_tab_sort(void) 
{
	int 	       	i,j;
	kf_analysis_t	temp;
		    	
	for (j  = 0 ; j  < MAX_GUESSES - 1; j++) {
		for( i = j + 1; i < MAX_GUESSES; i++) {
			if (kf_analysis[i].kfa_conf > kf_analysis[j].kfa_conf)  {
				temp 			= kf_analysis[i];
				kf_analysis[i] 		= kf_analysis[j];
				kf_analysis[j] 		= temp;
			}
			
		}
	}	
}
/*
 * print out the table of analysis guesses
 */
kf_result_t
kf_analysis_tab_print(int num_guesses)
{
	int guess;

	kf_analysis_tab_sort();

	kf_print("++\n");
	kf_print("++\n");
	kf_print("++\t\t\tFRU Analysis Summary\n");
	kf_print("++\n");

	if (kf_analysis[0].kfa_info[KF_MODULE_LEVEL].kfi_type == KF_UNKNOWN) {
		/* if nothing is set */
		kf_print("++\t\t* Inconclusive hardware error state *\n");
		kf_print("++\n");
		kf_print("++FRU ANALYSIS END\n");
		return KF_SUCCESS;
	}

#if !defined(FRUTEST) || defined(FRUTEST_DEBUG_OUTPUT)
	guess = 1;
	while ((guess < MAX_GUESSES) && (kf_analysis[guess].kfa_conf == 
					 kf_analysis[guess - 1].kfa_conf)) {
		if (guess > MAX_EQ_FRU_CONFS) {
			kf_print("++\t\t* Inconclusive hardware error state *\n");
			kf_print("++\n");
			kf_print("++FRU ANALYSIS END\n");
			return KF_SUCCESS;
		}

		guess++;
	}
#endif

	KF_ASSERT((num_guesses <= MAX_GUESSES));

	guess  = 0 ;
	while ((guess < MAX_GUESSES) && 
	       (kf_analysis[guess].kfa_info[0].kfi_type != KF_UNKNOWN) &&
	       ((guess <= num_guesses - 1) || (kf_analysis[guess].kfa_conf == 
					       kf_analysis[guess - 1].kfa_conf))) {

		if ((kf_analysis[guess].kfa_conf > MAX_FRU_CONF) &&
		    (kf_analysis[guess].kfa_conf != FRU_FLAG_CONF)) {
		  /* this is an invalid confidence value, something is wrong */

#if !defined(_STANDALONE) && !defined(FRUTEST)
			if (kdebug) {
				kf_print("FRU error: invalid FRU confidence value: %d%%\n",
					 kf_analysis[guess].kfa_conf);
			}
#endif /* !defined(_STANDALONE) */
		}
		else
			kf_analysis_print(&kf_analysis[guess]);
		guess++;
	}
	
	kf_print("++\n");
	kf_print("++FRU ANALYSIS END\n");
	return KF_SUCCESS;
}


#if defined(_STANDALONE)
void
sn0_fru_node_entry(nasid_t nasid,int (*print)(const char *,...))
{
	int i;

	/* initiliaze the node hint table */        
	for(i = 0 ; i < MAXCPUS / 2 ; i++)
		node_hint[i].kh_hint_type = -1;
	
	kf_printf = print;
	
	/* Print the starting fru message */
	kf_fru_begin_msg_print();

	if (kf_node_analyze(nasid,NULL) == KF_FAILURE)
		kf_print("FRU : node ( nasid = %d)analysis failed\n",
			 nasid);
	else {	
		kf_analysis_t	curr_analysis;
		
		kf_analysis_tab_init();
		kf_analysis_init(&curr_analysis);
		if (kf_node_analyze(nasid,&curr_analysis)  == KF_FAILURE)
			kf_print("FRU : node (nasid = %d) analysis failed\n",
				 nasid);
		kf_fru_summary_print(MAX_GUESSES);
	}
}

#endif

/* 
 * set the hint for a node . This is later used
 * by the fru analyzer during its analysis phase
 */
void
kf_hint_set(nasid_t	nasid,char *fru_hint)
{
	static  int 	first_call = 1;
  	int		i;

	if (first_call) {
		first_call = 0;
		/* initiliaze the node hint table */        
		for(i = 0 ; i < MAXCPUS / 2 ; i++)
			node_hint[i].kh_hint_type = -1;
	}
	if (nasid != -1) {
		for (i = 0 ; i < fru_hint_table_size; i++)  {
			if (strcmp(fru_hint,fru_hint_table[i]))
				continue;
			node_hint[nasid].kh_hint_type  = i;
			break;
		}
	}
}

#if !defined(FRUTEST) && !defined(_STANDALONE)
/*================================================================*/
/* Fru support to disable faulty components
 *		for now sticking to the disabling of
 *			cpus,
 *			memory banks,
 *			pci components
 */

extern int	numcpus;

/* Disable cpu [A|B] on node with the given nasid */
kf_result_t
kf_cpu_disable(int slice,nasid_t nasid)
{
	char	*promlog_var_name[] = { DISABLE_CPU_A , DISABLE_CPU_B };
	char	disable_message[32];

	KF_ASSERT(slice < CPUS_PER_NODE);
	/* If we are trying to disable the last cpu then abort. */
	if (numcpus == 1)
		return(KF_FAILURE);
	/* Set DisableA or DisableB promlog variable on the appropriate
	 * node so that the during the next reset the cpu is disabled.
	 */
	sprintf(disable_message,"%d: %s",
		KLDIAG_CPU_DISABLED,
		KF_REASON_FRU_DISABLE);
	ip27log_setenv(nasid,promlog_var_name[slice],disable_message,0);
	return(KF_SUCCESS);
}

/* 
 * The MD_MEMORY_CONFIG hub register contains configuration information
 * about memory banks.  Memory is required to be present at physical
 * address 0, for exception vector placement.  If the memory bank in DIMM
 * 0 (i.e. the dimm in slot 0 of the board, hereafter referred to as
 * "locational bank 0") is disabled for any reason, the DIMM0_SEL field of
 * MD_MEMORY_CONFIG can be set to torque the addresses so that physical
 * address 0 maps to locational bank 1.  The hardware XOR's the value of
 * the DIMM0_SEL field with the address bits that generate the low order
 * physical bank select signals.  Consequently, the configuration
 * information for each bank must be swapped in the MD_MEMORY_CONFIG
 * register.
 *
 * This routine exists to address PV 669589.  In older prom versions, the
 * mdir_swap_bank0() routine (which has since been replaced by
 * swap_memory_banks()) would only swap the configuration information for
 * banks 0 and 1, not for the other pairs of banks.  This caused a kernel
 * panic if bank 0 was disabled and there were an uneven number of banks
 * (i.e. bank 2 was populated but bank 3 was not).  To fix this, we swap
 * all pairs of banks in the register.  However, routines that provide
 * information to the user need to use numbers which describe the
 * _location_ of the bank which is disabled.  This routine translates the
 * values in MD_MEMORY_CONFIG from the software-visible swizzled format
 * into non-swizzled locational format.
 *
 * NOTE: a copy of this routine is used in the prom - keep in sync with
 * the version in stand/arcs/lib/libkl/ml/SN0.c
 *
 * NOTE 2: This routine, and all the other swap-related routines, assume
 * that the DIMM0_SEL field is always set to either 0 or 2.  If this
 * changes, much trouble will ensue.
 */

__uint64_t
translate_hub_mcreg(__uint64_t mc)
{
    __uint64_t          mc0, mc1, bank;

    if (((mc & MMC_DIMM0_SEL_MASK) >> MMC_DIMM0_SEL_SHFT) > 0) {
	for (bank = 0; bank < MD_MEM_BANKS; bank += 2) {

	    mc0 = (mc & MMC_BANK_MASK(bank)) >> MMC_BANK_SHFT(bank) ;
	    mc1 = (mc & MMC_BANK_MASK(bank+1)) >> MMC_BANK_SHFT(bank+1) ;

	    mc &= ~(MMC_BANK_MASK(bank));
	    mc &= ~(MMC_BANK_MASK(bank+1));

	    mc |= (mc0 << MMC_BANK_SHFT(bank+1));
	    mc |= (mc1 << MMC_BANK_SHFT(bank));
	}
    }

    return mc;
}


/* Disable a memory bank on a node */
/* ARGSUSED */
kf_result_t
kf_memory_bank_disable(int bank,nasid_t	nasid)
{
	char		disable_mem_size[MD_MEM_BANKS + 1],  
		        disable_mem_reason[MD_MEM_BANKS + 1],
		        disable_mem_mask[MD_MEM_BANKS + 1];
	char		mem_size[MD_MEM_BANKS + 1],  
		        mem_mask[MD_MEM_BANKS + 1];
	char		bank_name[2];
	char		swap_bank[MD_MEM_BANKS + 1];
	unchar		swap_all_banks;
	hubreg_t	memory_config = 0;

	KF_ASSERT(bank < MD_MEM_BANKS);

	memory_config = REMOTE_HUB_L(nasid,MD_MEMORY_CONFIG);
	swap_all_banks = get_nvreg(NVOFF_SWAP_ALL_BANKS);
	
	/* 
	 * if the IO6prom has the capability to swap all the memory banks
	 * (all newer versions do), then switch to "locational" format.
	 * See comment in translate_hub_mcreg() above, and PV 669589.
	 */
	if (swap_all_banks == 'y') 
	    memory_config = translate_hub_mcreg(memory_config);

	/* Get the current values for promlog variables related to the
	 * disabled memory banks.
	 */
	if (ip27log_getenv(nasid,DISABLE_MEM_SIZE,disable_mem_size,
		       DEFAULT_ZERO_STRING,0) < 0)
		return(KF_FAILURE);
	if (ip27log_getenv(nasid,DISABLE_MEM_REASON,disable_mem_reason,
		       DEFAULT_ZERO_STRING,
		       0) < 0)
		return(KF_FAILURE);
	if (ip27log_getenv(nasid,DISABLE_MEM_MASK,disable_mem_mask,"",0))
		return(KF_FAILURE);

	if (ip27log_getenv(nasid,INV_MEM_SZ,mem_size,
		       DEFAULT_ZERO_STRING,0) < 0)
		return(KF_FAILURE);
	if (ip27log_getenv(nasid,INV_MEM_MASK,mem_mask,"",0))
		return(KF_FAILURE);

	if (ip27log_getenv(nasid,SWAP_BANK,swap_bank,"",0))
		return(KF_FAILURE);


	
	/* Make sure that all these strings are terminated by a null char */
	disable_mem_size[MD_MEM_BANKS] = 0;
	disable_mem_reason[MD_MEM_BANKS] = 0;
	disable_mem_mask[MD_MEM_BANKS] = 0;
	mem_size[MD_MEM_BANKS] = 0;
	mem_mask[MD_MEM_BANKS] = 0;


	/* Special case handling for disabling bank 0 . 
	 * Basically bank 0 cannot be disabled unless bank 1 exists
	 * and has been enabled.
	 */
	if (bank == 0) {
		/* Already physical bank 0 has been disabled and physical
		 * bank 1 is acting as logical bank 0 . Bail out.
		 */
		if (strcmp(swap_bank,""))
			return(KF_SUCCESS);

		/* Now check to see if there is bank 1 present and enabled
		 * If not bail out. Cannot disable bank 0 if no bank 1.
		 * Note the magic value of '2' indicates whether the bank
		 * is enabled or not.
		 */
		if (mem_mask[1] != '2' || 
		    mem_size[1] == '0')
			return(KF_FAILURE);
	}

	/* Set the disable mem mask to indicate that this bank is being
	 * disabled only if it already hasn't been disabled.
	 */
	sprintf(bank_name,"%d",bank);
	if (!strstr(disable_mem_mask,bank_name))
		strcat(disable_mem_mask,bank_name);
	/* Set the reason for disabling this bank as due to fru analysis */
	if (disable_mem_reason[bank] == '0')
		disable_mem_reason[bank] = '0' + MEM_DISABLE_FRU;
	/* Set the disable bank configured size */
	disable_mem_size[bank] = 
		'0' + 
		(memory_config & MMC_BANK_MASK(bank) >> MMC_BANK_SHFT(bank));

	/* Store the new values for promlog variables related to the
	 * disable memory banks.
	 */
	ip27log_setenv(nasid,DISABLE_MEM_SIZE,disable_mem_size,0);
	ip27log_setenv(nasid,DISABLE_MEM_REASON,disable_mem_reason,0);
	ip27log_setenv(nasid,DISABLE_MEM_MASK,disable_mem_mask,0);

	/* Set the SWAP_BANK promlog variable if we are disabling bank 0 */
	if (bank == 0)
		ip27log_setenv(nasid,SWAP_BANK,"2",0);
	return(KF_SUCCESS);
}


#include <sys/SN/nvram.h>

int
kf_component_index_get(int npci, lboard_t *lb)
{
	int 	i ;
	klinfo_t	*k ;

	for (i=0; i<lb->brd_numcompts; i++) {
		k = (klinfo_t *)(NODE_OFFSET_TO_K1(
						   lb->brd_nasid,
						   lb->brd_compts[i])) ;
		if (k->struct_type == KLSTRUCT_BRI)
			continue ;
		if (k->physid == npci)
			return(i);
	}
	return(-1);
}

/* Disable a component in the given pci slot of the a given io board in 
 * the given slot of the given module.
 */

kf_result_t
kf_pci_component_disable(lboard_t *io_board,char module,char slot,char comp)
{

	dict_entry_t	dict;
	unchar		index;
	int		comp_index;
	/* Create disable io component entry structure */
	dict.module 	= module;
	dict.slot 	= slot;
	if (!io_board ||
	    (comp_index = kf_component_index_get(comp,io_board)) == -1)
		return(KF_FAILURE);
	dict.comp 	= (unchar)comp_index;
	
	/* Get the first free slot in the nvram table */
	index	= nvram_dict_first_free_index_get();
	/* Put the entry correspoding to this io component
	 * in the table's slot.
	 */
	nvram_dict_index_set(&dict,index);
	return(KF_SUCCESS);
}
#endif /* !FRUTEST && !_STANDALONE */

/* FRU print wrapper routine */
int
kf_print(const char *fmt,...)
{
	va_list	ap;
	char	buf[128];

	va_start(ap,fmt);
	vsprintf(buf,(char *)fmt,ap);
	va_end(ap);

	/* Depending on the print target print the fru
	 * message either to the promlog or the console
	 */
	if (kf_print_where == KF_PRINT_PROMLOG) {
		if (buf[0] == '+')
			ip27log_printf(IP27LOG_INFO,"%s",buf);
		else
			ip27log_printf(IP27LOG_INFO,"++\t%s",buf);
	} else if (kf_print_where == KF_PRINT_CONSOLE)
		kf_printf("%s",buf);
#if !defined(_STANDALONE)
	else if (kf_print_where == KF_PRINT_ERRBUF) 
		errbuf_print(buf);
#endif /* !_STANDALONE */
	return(0);
}

/* Print the fru summary */
kf_result_t
kf_fru_summary_print(int max_guesses)
{
	kf_result_t 	rv;

	/* Print the fru summary in the promlog */

	kf_print_where = KF_PRINT_PROMLOG;  
	rv = kf_analysis_tab_print(max_guesses);
	if (rv != KF_SUCCESS)
		return(rv);

	/* Print the fru summary in the error buffer */
	kf_print_where = KF_PRINT_ERRBUF;  
	rv = kf_analysis_tab_print(max_guesses);
	if (rv != KF_SUCCESS)
		return(rv);

	

	/* Print the fru summary on the console*/

	kf_print_where = KF_PRINT_CONSOLE;  
	rv = kf_analysis_tab_print(max_guesses);
	return(rv);
}

/* Print the starting fru message */
void
kf_fru_begin_msg_print(void)
{
	/* Print the starting fru message in the prom log */
	kf_print_where = KF_PRINT_PROMLOG;
	kf_print("\n");
	kf_print("\n");
	kf_print("++FRU ANALYSIS BEGIN\n");

	/* Print the starting fru message in the error buffer */
	kf_print_where = KF_PRINT_ERRBUF;
	kf_print("\n");
	kf_print("\n");
	kf_print("++FRU ANALYSIS BEGIN\n");

	/* Print the starting fru message on the console */
	kf_print_where = KF_PRINT_CONSOLE;
	kf_print("\n");
	kf_print("\n");
	kf_print("++FRU ANALYSIS BEGIN\n");

}
