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

#ifndef _SN0_FRU_ANALYSIS_H_
#define _SN0_FRU_ANALYSIS_H_
#include <sys/types.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/gda.h>
#include <sys/SN/error.h>
#include <sys/SN/router.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/arch.h>
#ifndef FRUTEST
#include <sys/systm.h>
#endif

/*************************************************************************
 *  Following is the organisation of the various units in the system for
 *  fru analysis
 *
 *
 *
 *					o	
 *  ------------------------------------+-------------------------------
 *  |			|				    |		|
 *  SW			BOARD			            IO	    SN0NET
 *  			  |				     |		|
 * -----------------------+----------			    XBOW    ROUTER
 *  	   	 |    |   |	 |   |			     |	
 *               MEM  HUB SYSBUS A   B			    BRIDGE	
 *		 |    |		     |			     |	
 *		 |    +------------  +----------	     +-----------
 *	       DIMM#  |  |   |    |  |   |     |	     |		|
 *		      PI MD  NI   II IC	 DC    SC (caches)  PCI		GIO
 *							     |		 |
 *					---------------------+         DANG
 *					|        |           |
 *					LINC    SCSI   	    IOC3	
 *
 *************************************************************************
 */

#define FORCE				1	 /* force the confidence
						  * level
						  */
#define NOT_FORCE			0	 /* update the confidence
						  * level only if the current
						  * belief is higher
						  */

/*
 * Following new,assert & printf macros need to be defined properly
 */

#undef FRU_DEBUG
#ifdef FRU_DEBUG
#define FRU_DEBUG_RULES
#define KF_DEBUG	if (1) kf_print
#else
#define KF_DEBUG	if (0) kf_print
#endif	/* #ifdef DEBUG */

#ifdef FRU_DEBUG_RULES
#define KF_DEBUG_RULES	if (1) kf_print
#else
#define KF_DEBUG_RULES	if (0) kf_print
#endif	/* #ifdef FRU_DEBUG_RULES */

#ifdef FRUTEST
#include <stdio.h>
#include <assert.h>

#define KF_ASSERT(_x)	assert(_x)
#define KF_PRINTF	kf_print


#else

#if defined(DEBUG) && !defined(_STANDALONE)
#define KF_ASSERT(_x)	if (!(_x)) { kf_print(">>>>>>>>ASSERT FAILURE : file = %s line = %d!<<<<<<<\n",__FILE__,__LINE__);debug("FRU ASSERT");}
#define KF_PRINTF(_x)	(kf_print(_x))		
#else
#define KF_ASSERT(_x)	
#define KF_PRINTF(_x)	
#endif
#endif	/* #ifdef FRUTEST */

/* higest confidence level, used for error checking */
#define MAX_FRU_CONF  90

/* this is a flag confidence used for special rules */
#define FRU_FLAG_CONF 95

#define ABSOLUTE_CONFIDENCE 100

/*
 * update the confidence level only if the belief is higher
 */

#define KF_CONF_UPDATE(_conf,_belief) 	(_conf = ((_conf < _belief) ? _belief : _conf))
/*
 * conditional update -
 * update the confidence level (if belief is higher ) if the condition is true
 */
/*
 * conditional force - 
 * force the confidence level to belief if the condition is true
 */


#ifdef FRU_DEBUG_RULES
#define KF_CONDITIONAL_UPDATE(_conf,_cond,_belief)				  		  		\
                                   {                                                              		\
                                        if (_cond) {                                              		\
                                                KF_DEBUG_RULES("++\t\t\t\t\tconditional update:\n"); 		\
                                                KF_DEBUG_RULES("++\t\t\t\t\t\tfile = %s\n",__FILE__); 		\
						KF_DEBUG_RULES("++\t\t\t\t\t\tline = %d\n",__LINE__);    	\
					}                                                         		\
 				        if (_conf)						 		\
						*_conf = (_cond  ? KF_CONF_UPDATE(*_conf,_belief) : *_conf); 	\
					else {	    						 		\
					        KF_DEBUG_RULES("++\t\t\t\t\tnull confidence pointer\n");		\
                                                KF_DEBUG_RULES("++\t\t\t\t\t\tfile = %s\n",__FILE__); 	 	\
						KF_DEBUG_RULES("++\t\t\t\t\t\tline = %d\n",__LINE__);   		\
					}                                                        		\
				   }

#define KF_CONDITIONAL_FORCE(_conf,_cond,_belief) 			         		 \
                                   {                                                             \
					if (_conf)						 \
						*_conf = (_cond  ? _belief : *_conf);          	 \
					else {	    						 \
					        KF_DEBUG_RULES("++\t\t\t\t\tnull confidence pointer\n");\
                                                KF_DEBUG_RULES("++\t\t\t\t\t\tfile = %s\n",__FILE__); 	 \
						KF_DEBUG_RULES("++\t\t\t\t\t\tline = %d\n",__LINE__);   \
					}                                                        \
				   }

#else
#define KF_CONDITIONAL_UPDATE(_conf,_cond,_belief)				  		  		\
                                   {                                                              		\
 				        if (_conf)						 		\
						*_conf = (_cond  ? KF_CONF_UPDATE(*_conf,_belief) : *_conf); 	\
				   }

#define KF_CONDITIONAL_FORCE(_conf,_cond,_belief) 			         		 \
                                   {                                                             \
					if (_conf)						 \
						*_conf = (_cond  ? _belief : *_conf);          	 \
				   }

#endif	/* FRU_DEBUG_RULES*/
                                        



/*
 * This file contains the structures to maintain the fru analyzer rules
 */

/*
 * following constants are approximate.
 * should update with the accurate values very soon 
 */

#define MAX_RULES			50
#define MAX_CONDS			200
#define MAX_ACTION_SETS			200
#define MAX_COMPS			50

#define MAX_COND_ELEMENTS		10

#define MAX_CONF_LEVELS			5


#define KF_LIST_SENTINEL 		-1		/* type of terminator of a sequence of table
							 * entries
							 */

typedef enum kf_result_s {
        KF_FAILURE      ,
        KF_SUCCESS
} kf_result_t;

/*
 * boolean data type  
 */

typedef enum kf_boolean_s {
	KF_FALSE	,
	KF_TRUE
} kf_boolean_t;

/*
 * type of boolean operand
 */

typedef enum kf_operand_s {	
	KF_AND		,
	KF_OR		,
	KF_NOT
} kf_operand_t;
							 
/*
 * structure of a fru rule table entry
 */

typedef struct kf_rule_s {
	int	kr_cond_tab_index;	/* index into the table which encodes conditions on
					 * bits of error registers
					 */
	int	kr_action_set_tab_index;/* index into the table which encodes a sequence of
					 * actions to be done if the condtion is true
					 */
} kf_rule_t;


/*
 * types of a condition table entry
 */

#define KC_LITERAL	0	
#define KC_OPERAND	1	
/*
 * structure of a fru rule's condition table entry
 */

typedef struct kf_cond_s {
	int	kc_type;		/* type of condition entry - literal or operand */
	union 	{
		struct {
			int		kl_reg_tab_index;/* index into the table of register 
							  * addresses 
							  */
			hubreg_t	kl_mask;	/* mask for the register value */
		} kc_literal;				/* literal entry */
		kf_operand_t	kc_operand;		/* operand entry */
	} kc_data;		
	
} kf_cond_t;

/*
 * useful macros to traverse the nested structures in the condition entry
 */

#define KF_LITERAL(_cond)	(_cond->kc_data.kc_literal)	/* literal in  a condition */
#define KF_OPERAND(_cond)	(_cond->kc_data.kc_operand)	/* operand in a condition */	

/* check if the error register has a particular set of bits set */

#define KF_LITERAL_EVAL(_cond)	(KF_LITERAL(_cond).kl_mask & 	\
				 kf_reg_tab[KF_LITERAL(_cond).kl_reg_tab_index])
/*
 * types of action set entries
 */

#define KAS_ACTION	0
#define KAS_RULE	1       

/*
 * structure of an entry in the table of action sets
 */

typedef struct kf_action_set_s {
	int	kas_type;			/* type of action - confidence level update or
						 *		    trigger another rule
						 */

	int	kas_conf_index;        /* index into the confidence table */
	int	kas_conf_val;          /* confidence value to assign */
} kf_action_set_t;


/* 
 * structure of a fru rule's action
 */

typedef struct kf_action_s {
	int		ka_conf_tab_index;	/* index into the table of component confidence table 
						 * of addresses
						 */
	confidence_t	ka_conf_value;		/* confidence level to which the above component
						 * confidence variable is to be set
						 */
} kf_action_t;

/*
 * structures to print out the final analysis 
 */
typedef struct kf_fru_info_s {
	int		kfi_type;		/* type of the fru at a particular level 
						 * in the heirarchy of hardware
						 */
	int		kfi_inst;		/* instance of the fru type */
} kf_fru_info_t;

#define MAX_LEVELS		6		/* max. levels in a hardware heirarchy */


#ifdef FRUTEST
#define MAX_GUESSES		20              /* this is only for testing */
#else
#define MAX_GUESSES		5		/* max number of confidences kept 
						   track of */
#endif

#define MAX_EQ_FRU_CONFS	3	/* maximum number of equal conclusions
					   before we decide inconclusive */

typedef struct kf_analysis_s   {
	kf_fru_info_t	kfa_info[MAX_LEVELS];	/* fru info */
	confidence_t	kfa_conf;		/* confidence associated with the lowest level fru 
						 */
    	nasid_t		kfa_nasid;		/* nasid of the node during
						 * whose analysis the fru was
						 * found.
						 */
	lboard_t	*kfa_io_board;		/* Pointer to the io board
						 * if the analysis corr.
						 * to io 
						 */
	char		kfa_serial_number[100]; /* Serial number of the board if any */
} kf_analysis_t;

/* types */
#define KF_UNKNOWN				-1

#define KFTYPE_MODULE				1000

/* Some hint types. Make sure that this order subsumes
 * same as the fru_hint table in sn0_fru_analysis.c
 */
#define KF_HINT_KERNEL_FAULT			0
#define KF_HINT_CACHE_ERROR			1

/* Hint information being passed to the FRU analyzer */
typedef struct kf_hint_s {
	short		kh_hint_type;		/* type of hint */
} kf_hint_t;


/* Max. number of dimm-bank pairs on a node  */
#define MAX_DIMM_BANKS_SN00			4
#define MAX_DIMM_BANKS_SN0			8

/*
 * definitions of indices into the register address table	    
 */
/* the next line is needed by the parser, do not remove! */
/* START OF REGISTER ADDRESS TABLE INDICIES */
#define	KF_CACHE_ERR0_INDEX			0	/* cache err register of cpuA */
#define KF_CACHE_ERR1_INDEX			1	/* cache err register of cpuB */

#define KF_PI_ERR_INT_PEND_INDEX		2	/* pi err int pend register */
#define KF_PI_ERR_STS0_A_INDEX			3	/* pi err status0 register for cpuA */
#define KF_PI_ERR_STS0_B_INDEX			4	/* pi err status0 register for cpuB */
#define KF_PI_ERR_STS1_A_INDEX			5	/* pi err status1 register for cpuA */
#define KF_PI_ERR_STS1_B_INDEX			6	/* pi err status1 register for cpuB */

#define KF_MD_DIR_ERR_INDEX			7	/* md directory error register */
#define KF_MD_MEM_ERR_INDEX			8	/* md memory error register */
#define KF_MD_PROTO_ERR_INDEX			9	/* md protocol error register */
#define KF_MD_MISC_ERR_INDEX			10	/* md miscellaneous error register */

#define KF_II_WIDGET_STATUS_INDEX		11	/* ii widget status register */
#define KF_II_BTE0_STS_INDEX			12	/* ii bte0 status register */
#define KF_II_BTE1_STS_INDEX			13	/* ii bte1 status register */
#define KF_II_BTE0_SRC_INDEX			14	/* ii bte0 source address register */
#define KF_II_BTE1_SRC_INDEX			15	/* ii bte1 source address register */
#define KF_II_BTE0_DST_INDEX			16	/* ii bte0 destination address register */
#define KF_II_BTE1_DST_INDEX			17	/* ii bte1 destination address register */

#define KF_II_CRB_ENT0_A_INDEX			18	/* ii crb entry a 0 index
							 * there are 15 such entries
							 */
#define KF_NI_VECT_STS_INDEX			33	/* ni pio vector status register */
#define KF_NI_PORT_ERR_INDEX			34 	/* ni port error register */

#define KF_ROUTER_STS_ERR0_INDEX		35	/* router status error register for port0 */
/* NOTE THE NEXT INDEX SHOULD START FROM 20+6(NO OF ROUTER PORTS) */

#define KF_XBOW_WIDGET_0_STATUS_INDEX           41
#define KF_XBOW_LINK_STATUS_INDEX               42
#define KF_BRIDGE_RAM_PERR_INDEX                43
#define KF_BRIDGE_INT_STATUS_INDEX              44

#define MAX_ERR_REGS				45	/* update this whenever a new error
							 * register is added 
							 */
/* END OF REGISTER ADDRESS TABLE INDICIES */
/* the previous line is needed by the parser, do not remove! */


/*
 * definitions of indices into the confidence level address table	    
 */
/* the next line is needed by the parser, do not remove! */
/* START OF CONFIDENCE LEVEL ADDRESS TABLE INDICIES */
#define KF_CPU0_CONF_INDEX			0	/* cpuA confidence */
#define KF_CPU1_CONF_INDEX			1	/* cpuB confidence */
#define KF_IC0_CONF_INDEX			2	/* cpuA's i-cache confidence */
#define KF_IC1_CONF_INDEX			3	/* cpuB's i-cache confidence */
#define KF_DC0_CONF_INDEX			4	/* cpuA's d-cache confidence */
#define KF_DC1_CONF_INDEX			5	/* cpuB's d-cache confidence */
#define KF_SC0_CONF_INDEX			6	/* cpuA's s-cache confidence */
#define KF_SC1_CONF_INDEX			7	/* cpuB's s-cache confidence */

#define KF_SYSBUS_CONF_INDEX			8	/* system bus confidence */
#define KF_HUB_CONF_INDEX			9	/* hub confidence */
#define KF_PI_CONF_INDEX			10	/* pi confidence */
#define KF_MD_CONF_INDEX			11	/* md confidence */
#define KF_II_CONF_INDEX			12	/* ii confidence */
#define KF_NI_CONF_INDEX			13	/* ni confidence */

#define KF_MEM_CONF_INDEX			14	/* memory confidence */
#define KF_DIMM0_CONF_INDEX			15	/* dimm0 confindence */
#define KF_ROUTER_CONF_INDEX			23	/* router confidence */
#define KF_ROUTER_LINK0_CONF_INDEX		24	/* router link0 confidence */
#define KF_ROUTER_LINK1_CONF_INDEX		25	/* router link1 confidence */
#define KF_ROUTER_LINK2_CONF_INDEX		26	/* router link2 confidence */
#define KF_ROUTER_LINK3_CONF_INDEX		27	/* router link3 confidence */
#define KF_ROUTER_LINK4_CONF_INDEX		28	/* router link4 confidence */
#define KF_ROUTER_LINK5_CONF_INDEX		29	/* router link5 confidence */
#define KF_SOFTWARE_CONF_INDEX			30
#define KF_XBOW_CONF_INDEX                      31
#define KF_BRIDGE_LINK_CONF_INDEX               32
#define KF_BRIDGE_CONF_INDEX                    33

/* There is no corresponding KFTYPE for this because it is only used as
   an index. The actual FRU will be either a PCI Dev or the bridge x*/
#define KF_BRIDGE_PCI_MASTER_CONF_INDEX         34

#define KF_BRIDGE_PCI_DEV_CONF_INDEX            35
#define KF_BRIDGE_SSRAM_CONF_INDEX              36

#define KF_HUB_LINK_CONF_INDEX			37

/* This is not being used because there is no confidence_t for it. We are using
   the hub's confidence with the flag value to indicate this condition. */
#define KF_PCOUNT_CONF_INDEX			38

/* This is not being used because there is no confidence_t for it. We are using
   the md's confidence with the flag value to indicate this condition. */
#define KF_T5_WB_SURPRISE_CONF_INDEX		39
/* This is not being used because there is no confidence_t for it. We are using
   the ii's confidence with the flag value to indicate this condition. */
#define KF_BTE_PUSH_CONF_INDEX			40

#define KF_MAX_CONF_INDICES			41	/* update this whenever new indices are
							 * added above
							 */
/* END OF CONFIDENCE LEVEL ADDRESS TABLE INDICIES */
/* the previous line is needed by the parser, do not remove! */


/* defines for use in FRU output */

 /* this is used because KLTYPE numbers and KF_CONF_INDEX numbers overlap */
#define KFTYPE_CONF_INDEX_BASE                  0x100

#define KLTYPE_TO_KFTYPE(_kltype)
#define CONF_INDEX_TO_KFTYPE(_conf_index)       (_conf_index + KFTYPE_CONF_INDEX_BASE)

#define KFTYPE_WEIRDCPU 	  	   	KLTYPE_WEIRDCPU
#define KFTYPE_IP27				KLTYPE_IP27

#define KFTYPE_WEIRDIO				KLTYPE_WEIRDIO
#define KFTYPE_BASEIO				KLTYPE_BASEIO
#define KFTYPE_4CHSCSI				KLTYPE_4CHSCSI
#define KFTYPE_ETHERNET				KLTYPE_ETHERNET
#define KFTYPE_FDDI				KLTYPE_FDDI
#define KFTYPE_GFX				KLTYPE_GFX
#define KFTYPE_HAROLD				KLTYPE_HAROLD
#define KFTYPE_PCI				KLTYPE_PCI
#define KFTYPE_VME				KLTYPE_VME
#define KFTYPE_MIO				KLTYPE_MIO
#define KFTYPE_FC				KLTYPE_FC
#define KFTYPE_LINC				KLTYPE_LINC
#define KFTYPE_TPU				KLTYPE_TPU
#define KFTYPE_GSN_A				KLTYPE_GSN_A
#define KFTYPE_GSN_B				KLTYPE_GSN_B

#define KFTYPE_WEIRDROUTER			KLTYPE_WEIRDROUTER
#define KFTYPE_ROUTER2				KLTYPE_ROUTER2
#define KFTYPE_NULL_ROUTER			KLTYPE_NULL_ROUTER
#define KFTYPE_META_ROUTER			KLTYPE_META_ROUTER
#define KFTYPE_WEIRDMIDPLANE			KLTYPE_WEIRDMIDPLANE
#define KFTYPE_MIDPLANE				KLTYPE_MIDPLANE8

#define KFTYPE_CPU0	       	CONF_INDEX_TO_KFTYPE(KF_CPU0_CONF_INDEX)
#define KFTYPE_CPU1	       	CONF_INDEX_TO_KFTYPE(KF_CPU1_CONF_INDEX)
#define KFTYPE_IC0	       	CONF_INDEX_TO_KFTYPE(KF_IC0_CONF_INDEX)
#define KFTYPE_IC1	       	CONF_INDEX_TO_KFTYPE(KF_IC1_CONF_INDEX)
#define KFTYPE_DC0	       	CONF_INDEX_TO_KFTYPE(KF_DC0_CONF_INDEX)
#define KFTYPE_DC1	       	CONF_INDEX_TO_KFTYPE(KF_DC1_CONF_INDEX)
#define KFTYPE_SC0	       	CONF_INDEX_TO_KFTYPE(KF_SC0_CONF_INDEX)
#define KFTYPE_SC1	       	CONF_INDEX_TO_KFTYPE(KF_SC1_CONF_INDEX)

#define KFTYPE_SYSBUS	       	CONF_INDEX_TO_KFTYPE(KF_SYSBUS_CONF_INDEX)
#define KFTYPE_HUB	       	CONF_INDEX_TO_KFTYPE(KF_HUB_CONF_INDEX)
#define KFTYPE_HUB_LINK		CONF_INDEX_TO_KFTYPE(KF_HUB_LINK_CONF_INDEX)
#define KFTYPE_PI	       	CONF_INDEX_TO_KFTYPE(KF_PI_CONF_INDEX)
#define KFTYPE_MD	       	CONF_INDEX_TO_KFTYPE(KF_MD_CONF_INDEX)
#define KFTYPE_II	       	CONF_INDEX_TO_KFTYPE(KF_II_CONF_INDEX)
#define KFTYPE_NI	       	CONF_INDEX_TO_KFTYPE(KF_NI_CONF_INDEX)

#define KFTYPE_MEM	       	CONF_INDEX_TO_KFTYPE(KF_MEM_CONF_INDEX)
#define KFTYPE_DIMM0	       	CONF_INDEX_TO_KFTYPE(KF_DIMM0_CONF_INDEX)
#define KFTYPE_ROUTER	       	CONF_INDEX_TO_KFTYPE(KF_ROUTER_CONF_INDEX)
#define KFTYPE_ROUTER_LINK0    	CONF_INDEX_TO_KFTYPE(KF_ROUTER_LINK0_CONF_INDEX)
#define KFTYPE_ROUTER_LINK1     CONF_INDEX_TO_KFTYPE(KF_ROUTER_LINK1_CONF_INDEX)
#define KFTYPE_ROUTER_LINK2    	CONF_INDEX_TO_KFTYPE(KF_ROUTER_LINK2_CONF_INDEX)
#define KFTYPE_ROUTER_LINK3    	CONF_INDEX_TO_KFTYPE(KF_ROUTER_LINK3_CONF_INDEX)
#define KFTYPE_ROUTER_LINK4    	CONF_INDEX_TO_KFTYPE(KF_ROUTER_LINK4_CONF_INDEX)
#define KFTYPE_ROUTER_LINK5    	CONF_INDEX_TO_KFTYPE(KF_ROUTER_LINK5_CONF_INDEX)
#define KFTYPE_SOFTWARE	       	CONF_INDEX_TO_KFTYPE(KF_SOFTWARE_CONF_INDEX)
#define KFTYPE_XBOW	       	CONF_INDEX_TO_KFTYPE(KF_XBOW_CONF_INDEX)
#define KFTYPE_BRIDGE_LINK     	CONF_INDEX_TO_KFTYPE(KF_BRIDGE_LINK_CONF_INDEX)
#define KFTYPE_BRIDGE	       	CONF_INDEX_TO_KFTYPE(KF_BRIDGE_CONF_INDEX)
#define KFTYPE_BRIDGE_PCI_DEV   CONF_INDEX_TO_KFTYPE(KF_BRIDGE_PCI_DEV_CONF_INDEX)
#define KFTYPE_BRIDGE_SSRAM    	CONF_INDEX_TO_KFTYPE(KF_BRIDGE_SSRAM_CONF_INDEX)
#define KFTYPE_PCOUNT		CONF_INDEX_TO_KFTYPE(KF_PCOUNT_CONF_INDEX)
#define KFTYPE_T5_WB_SURPRISE 	CONF_INDEX_TO_KFTYPE(KF_T5_WB_SURPRISE_CONF_INDEX)
#define KFTYPE_BTE_PUSH 	CONF_INDEX_TO_KFTYPE(KF_BTE_PUSH_CONF_INDEX)


/* defines for levels of output struct */

#define KF_MODULE_LEVEL         0
#define KF_SOFTWARE_LEVEL	0	


#define KF_ROUTER2_LEVEL	KF_MODULE_LEVEL + 1
#define KF_ROUTER_LEVEL		KF_MODULE_LEVEL + 1
#define KF_ROUTER_LINK_LEVEL	KF_ROUTER_LEVEL + 1

#define KF_IP27_LEVEL		KF_MODULE_LEVEL + 1

#define KF_CPU_LEVEL		KF_IP27_LEVEL + 1

#define KF_IC_LEVEL		KF_CPU_LEVEL + 1
#define KF_DC_LEVEL		KF_CPU_LEVEL + 1
#define KF_SC_LEVEL		KF_CPU_LEVEL + 1

#define KF_PCOUNT_LEVEL		KF_IP27_LEVEL + 1

#define KF_SYSBUS_LEVEL		KF_IP27_LEVEL + 1

#define KF_HUB_LEVEL 		KF_IP27_LEVEL + 1

#define KF_HUB_LINK_LEVEL	KF_HUB_LEVEL + 1

#define KF_PI_LEVEL 		KF_HUB_LEVEL + 1

#define KF_MD_LEVEL 		KF_HUB_LEVEL + 1
#define KF_T5_WB_SURPRISE_LEVEL KF_HUB_LEVEL + 1
#define KF_BTE_PUSH_LEVEL 	KF_HUB_LEVEL + 1

#define KF_MEM_LEVEL		KF_MD_LEVEL + 1
#define KF_DIMM_LEVEL 		KF_MEM_LEVEL + 1	

#define KF_II_LEVEL 		KF_HUB_LEVEL + 1

#define KF_NI_LEVEL 		KF_HUB_LEVEL + 1

#define KF_WIDGET_LEVEL    	KF_MODULE_LEVEL + 1
#define KF_BRIDGE_LEVEL         KF_WIDGET_LEVEL + 1
#define KF_BRIDGE_LINK_LEVEL    KF_BRIDGE_LEVEL + 1
#define KF_BRIDGE_PCI_DEV_LEVEL KF_BRIDGE_LEVEL + 1
#define KF_BRIDGE_SSRAM_LEVEL   KF_BRIDGE_LEVEL + 1



/*
 * macros to get to the required confidence field from the error  structures
 */

/* _err is a pointer to klcpu_err_t */					 
#define KF_CPU_CONF(_err)			(_err->ce_confidence_info.kc_confidence)
#define KF_ICACHE_CONF(_err)			(_err->ce_confidence_info.kc_icache)
#define KF_DCACHE_CONF(_err)			(_err->ce_confidence_info.kc_dcache)
#define KF_SCACHE_CONF(_err)			(_err->ce_confidence_info.kc_scache)


/* _err is a pointer to klhub_err_t */
#define	KF_HUB_CONF(_err)			(_err->he_hub_confidence)
#define KF_HUB_LINK_CONF(_err)			(_err->he_link_confidence)

#define KF_PI_CONF(_err)			(_err->he_dmp.hb_pi.hp_confidence)
#define	KF_SYSBUS_CONF(_err)			(_err->he_dmp.hb_pi.hp_sysbus_confidence)

#define KF_MD_CONF(_err)			(_err->he_dmp.hb_md.hm_confidence)
#define	KF_MEM_CONF(_err)			(_err->he_dmp.hb_md.hm_mem_conf_info.km_confidence)
#define KF_DIMM_CONF(_err,_i)			(_err->he_dmp.hb_md.hm_mem_conf_info.km_dimm[_i])

#define KF_II_CONF(_err)			(_err->he_dmp.hb_io.hi_confidence)

#define KF_NI_CONF(_err)			(_err->he_dmp.hb_ni.hn_confidence)

/* _err is a pointer to klrouter_err_t structure */
#define KF_ROUTER_CONF(_err)			(_err->re_confidence)
#define KF_ROUTER_LINK_CONF(_err,_i)		(_err->re_link_confidence[_i])

/*
 * macros to get to the hub error registers 
 */
#define KF_PI_ERR_INT_PEND(_err)		(_err->he_dmp.hb_pi.hp_err_int_pend)
#define KF_PI_ERR_STS0_A(_err)			(_err->he_dmp.hb_pi.hp_err_sts0[0])
#define KF_PI_ERR_STS0_B(_err)			(_err->he_dmp.hb_pi.hp_err_sts0[1])
#define KF_PI_ERR_STS1_A(_err)			(_err->he_dmp.hb_pi.hp_err_sts1[0])
#define KF_PI_ERR_STS1_B(_err)			(_err->he_dmp.hb_pi.hp_err_sts1[1])

#define KF_MD_DIR_ERR(_err)			(_err->he_dmp.hb_md.hm_dir_err)		
#define KF_MD_MEM_ERR(_err)			(_err->he_dmp.hb_md.hm_mem_err)		
#define KF_MD_PROTO_ERR(_err)			(_err->he_dmp.hb_md.hm_proto_err)		
#define KF_MD_MISC_ERR(_err)			(_err->he_dmp.hb_md.hm_misc_err)		

#define KF_II_WIDGET_STATUS(_err)		(_err->he_dmp.hb_io.hi_wstat)
#define KF_II_CRB_ENTA(_err,_i)			(_err->he_dmp.hb_io.hi_crb_entA[_i])
#define KF_II_BTE0_STS(_err)			(_err->he_dmp.hb_io.hi_bte0_sts)	
#define KF_II_BTE1_STS(_err)			(_err->he_dmp.hb_io.hi_bte1_sts)	
#define KF_II_BTE0_SRC(_err)			(_err->he_dmp.hb_io.hi_bte0_src)	
#define KF_II_BTE1_SRC(_err)			(_err->he_dmp.hb_io.hi_bte1_src)	
#define KF_II_BTE0_DST(_err)			(_err->he_dmp.hb_io.hi_bte0_dst)	
#define KF_II_BTE1_DST(_err)			(_err->he_dmp.hb_io.hi_bte1_dst)	

#define KF_NI_VECT_STS(_err)			(_err->he_dmp.hb_ni.hn_vec_sts)
#define KF_NI_PORT_ERR(_err)			(_err->he_dmp.hb_ni.hn_port_err)

#define KF_ROUTER_STS_ERR(_err,_i)		(_err->re_status_error[_i])	

/* useful IO macros */
#define BRIDGE_CONF(_err)               (_err->be_confidence)
#define BRIDGE_SSRAM_CONF(_err)         (_err->be_ssram_conf)
#define BRIDGE_PCI_DEV_CONF(_err,_ndx)  (_err->be_pci_dev[_ndx])
#define BRIDGE_LINK_CONF(_err)		(_err->be_link_conf)
#define XBOW_CONF(_err)                 (_err->xe_confidence)

/* subtract the base from the port number because only lower ports are used */
#define XBOW_PORT_BOARD(_xbow,_port)    ((lboard_t*)\
	    NODE_OFFSET_TO_K1(_xbow->xbow_port_info[_port - BASE_XBOW_PORT].port_nasid,\
			      _xbow->xbow_port_info[_port - BASE_XBOW_PORT].port_offset))

/* masks for IO error registers */

/* for ERROR_CMD reg */
#define XBOW_SRC_WIDGET_MASK            0xf
#define XBOW_SRC_WIDGET_SHIFT           24   /* 27-24 */
#define XBOW_DEST_WIDGET_MASK           0xf
#define XBOW_DEST_WIDGET_SHIFT          28   /* 31-28 */

/* for LINK_STATUS regs */
#define XBOW_LINK_ALIVE_MASK            0x1 << 31
#define XBOW_LINK_ILL_DEST_MASK         0x1 << 17
#define XBOW_LINK_OVER_ALLOC_MASK       0x1 << 16
#define XBOW_LINK_MAX_TRANS_RETRY_MASK  0x1 << 5
#define XBOW_LINK_LLP_REC_ERR_MASK      0x1 << 4
#define XBOW_LINK_PKT_TOUT_DEST_MASK    0x1 << 2
#define XBOW_LINK_CONN_TOUT_ERR_MASK    0x1 << 1
#define XBOW_LINK_PKT_TOUT_SRC_MASK     0x1

/* for ERR_UPPER reg */
#define BRIDGE_PCI_DEV_MASTER_MASK      0x1 << 20
#define BRIDGE_PCI_DEV_NUM_SHIFT        16
#define BRIDGE_PCI_DEV_NUM_MASK         0x7

/* for INT_STATUS reg */
#define BRIDGE_PCI_RETRY_CNT_MASK       0x1 << 10
#define BRIDGE_PCI_MASTER_TOUT_MASK     0x1 << 11
#define BRIDGE_PCI_PERR_MASK		0x1 << 12
#define BRIDGE_PCI_SERR_MASK		0x1 << 13
#define BRIDGE_PCI_PARITY_MASK		0x1 << 14
#define BRIDGE_SSRAM_PERR_MASK          0x1 << 16

/* for SSRAM parity err reg */
#define BRIDGE_PAR_BYTE_MASK            0xff << 16
#define BRIDGE_SIZE_FAULT_MASK          0x1 << 28

/* useful macro to initiliaze the action set table with basic actions like
 * setting a component's confidence to a particular level
 */
#define COMP_CONF(_index,_conf)			(_index * MAX_CONF_LEVELS + _conf)


/*
 * mask used in the initialization of the condtion table
 */
/*
 * to get the bits corresponding to syscmd sysad & sysstate errors in
 * err_int_pend
 */
#define SYSBUS_ERR_A       			(PI_ERR_SYSSTATE_A         |	\
						 PI_ERR_SYSAD_DATA_A       |	\
						 PI_ERR_SYSAD_ADDR_A       |	\
						 PI_ERR_SYSCMD_DATA_A      |	\
						 PI_ERR_SYSCMD_ADDR_A      |	\
						 PI_ERR_SYSSTATE_TAG_A)
#define SYSBUS_ERR_B      			(PI_ERR_SYSSTATE_B         |	\
						 PI_ERR_SYSAD_DATA_B       |	\
						 PI_ERR_SYSAD_ADDR_B       |	\
						 PI_ERR_SYSCMD_DATA_B      |	\
						 PI_ERR_SYSCMD_ADDR_B      |	\
						 PI_ERR_SYSSTATE_TAG_B)


#define ERR_STS0_TYPE_MASK			0x3

/* W-bit of CRB status in the err_status_1 resgister */
#define W_MASK					0x1000000000000ull

/* H bit of CRB status field in err_status_1 register */
#define H_MASK					0x800000000000ull

/* I bit of CRB status field in err_status_1 register */
#define I_MASK					0x400000000000ull

/* T bit of CRB status field in err_status_1 register */
#define T_MASK					0x200000000000ull


#define DIR_ERR_UCE_AE_MASK			0xc000000000000000
#define MEM_ERR_UCE_MASK			0x8000000000000000
#define PROT_ERR_VALID_MASK			0x8000000000000000
#define MISC_ERR_ILL_PKT_MASK			0x2a8
#define MISC_ERR_BAD_DATA_MASK			0x3

#define VEC_STAT_ERR_MASK			0x7

#define NI_NW_PORT_ERR_MASK			0x2e00000000

#define NI_INTERNAL_ERR_MASK			0x1000000000

#define CACHE_ERR_TYPE_MASK			0xc0000000	


#define NODE_ID_MASK				0xff00000000	
#define NODE_ID_SHFT				32
#define BTE_ADDR_NODE_ID(_r)  			((_r & NODE_ID_MASK) >> NODE_ID_SHFT)		

#define MEM_DIMM_MASK				0xe0000000
#define MEM_DIMM_SHFT				29



/* macro to get the size of arrays */
#define ARRAY_LEN(_a)				(sizeof(_a)/sizeof(*_a))

/*  Promlog reason indication why a particular hardware component
 *  has been disabled.
 */
#define KF_REASON_FRU_DISABLE			"disabled by fru"

/* Where to print the fru messages */
#define KF_PRINT_PROMLOG			1	/* promlog */
#define KF_PRINT_CONSOLE			2	/* console */


/*
 * function prototypes
 */



extern klcpu_err_t 	*kf_cpu_err_info_get(lboard_t *,klinfo_t **,cpuid_t);
extern klhub_err_t 	*kf_hub_err_info_get(lboard_t *);
extern klrouter_err_t 	*kf_router_err_info_get(lboard_t *);

extern kf_result_t	kf_mem_conf_get(paddr_t,confidence_t **,confidence_t **);

extern kf_result_t	kf_node_tables_build(nasid_t);

extern hubreg_t		kf_condition_compute(kf_cond_t *);
extern kf_result_t	kf_action_set_perform(kf_action_set_t *);
extern kf_result_t	kf_rule_trigger(kf_rule_t *);

extern kf_result_t	kf_node_analyze(nasid_t,kf_analysis_t *);	
extern kf_result_t	kf_board_analyze(lboard_t *,kf_analysis_t *);
extern kf_result_t	kf_comp_analyze(lboard_t *,klinfo_t *,kf_analysis_t *);
extern kf_result_t	kf_router_analyze(kf_analysis_t *);
extern kf_result_t	kf_hub_analyze(kf_analysis_t *);
extern kf_result_t	kf_pi_analyze(kf_analysis_t *);
extern kf_result_t	kf_md_analyze(kf_analysis_t *);
extern kf_result_t	kf_ii_analyze(kf_analysis_t *);
extern kf_result_t	kf_ii_crb_analyze(hubreg_t);
extern kf_result_t	kf_ni_analyze(kf_analysis_t *);
extern kf_result_t	kf_cpu_analyze(klcpu_err_t *,unsigned int,kf_analysis_t *);

#if 0
extern void             kf_print_io_analysis(nasid_t nasid,FILE *out_file);
#endif
extern kf_result_t	kf_io_analyze(nasid_t nasid,kf_analysis_t *);


extern kf_result_t	kf_rule_tab_analyze(int);

extern void 		kf_analysis_init(kf_analysis_t *);
extern void		kf_analysis_tab_init(void);
extern void 		kf_analysis_print(kf_analysis_t *);
extern kf_result_t	kf_analysis_tab_print(int);
extern kf_result_t	kf_fru_summary_print(int);

extern kf_result_t	kf_guess_put(kf_analysis_t *);
	
/* print function passed to fru */
extern int		(*kf_printf)(const char *,...);
extern int		sn0_fru_entry(int (*)(const char *,...));
extern void		kf_hint_set(nasid_t,char *);

extern int		sn0_fru_enabled;				

extern kf_result_t	kf_cpu_disable(int,nasid_t);
extern kf_result_t	kf_memory_bank_disable(int,nasid_t);
extern kf_result_t	kf_pci_component_disable(lboard_t *,char,char,char);

extern int		kf_print(const char *fmt,...);
extern void 		kf_fru_begin_msg_print(void);
#endif /* #ifndef _SN0_FRU_ANALYSIS_H_ */
