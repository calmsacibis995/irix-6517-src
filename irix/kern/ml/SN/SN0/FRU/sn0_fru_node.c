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

#include "sn0_fru_analysis.h"
#include <sys/reg.h>
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/R10k.h>

#ifdef PCOUNT_WAR
#include <sys/SN/SN0/ip27log.h>
#endif /* PCOUNT_WAR */

#ifdef FRUTEST
#include <string.h>
	
extern	kl_config_hdr_t		g_node[];

/* 
 * HOW DO WE INTEGRATE THESE CONFIDENCE LEVELS INTO THE KLCONFIG
 * STRUCTURES
 */
extern	confidence_t		g_io_conf;
extern 	confidence_t		g_sn0net_conf;
extern 	confidence_t		g_xbow_conf;
extern  int			g_ce_valid;
#endif	/* #ifdef FRUTEST */
#ifdef _STANDALONE
extern nasid_t	get_nasid(void);
#endif


extern 	confidence_t 		*kf_conf_tab[];		/* table of component confidence level
							 * pointers
							 */
extern 	hubreg_t		kf_reg_tab[];		/* table of error registers*/

nasid_t				current_nasid;		/* nasid of the node
							 * that is being
							 * analyzed
							 */

/* 
 * analyze each component on this board
 */


kf_result_t
kf_board_analyze(lboard_t *board,kf_analysis_t *curr_analysis)
{
	kf_result_t		rv = KF_SUCCESS;
	int			comp;

	kf_analysis_t		save_curr_analysis;
	
	KF_DEBUG("\tkf_board_analyze:doing board analysis......\n");

	KF_ASSERT(board);

	if (curr_analysis) {
		save_curr_analysis = *curr_analysis;
		board_serial_number_get(board,curr_analysis->kfa_serial_number);
	}
	/* check to see if we have already analyzed this board
	 * during a previous node analysis
	 */

	if (board->brd_flags & DUPLICATE_BOARD) {
		KF_DEBUG("\tkf_board_analyze:finished board analysis -- duplicate board\n");
		return KF_SUCCESS;
	}
	

	/*
	 * analyze each of the components of this ip27 board 
	 */


	for (comp = 0; comp < KLCF_NUM_COMPS(board);comp++) {
		if ((rv = kf_comp_analyze(board,KLCF_COMP(board,comp),curr_analysis)) 
		    != KF_SUCCESS) {
			KF_DEBUG("\tkf_board_analyze:finished board analysis -- component analysis failed\n");
			return rv;
		}
	}
	KF_DEBUG("\tkf_board_analyze:finished board analysis\n");		
	if (curr_analysis)
		*curr_analysis = save_curr_analysis;
	return KF_SUCCESS;
	
}

/*
 * analyze the component
 * ARGSUSED
 */
kf_result_t
kf_comp_analyze(lboard_t 	*board,
		klinfo_t 	*comp,
		kf_analysis_t	*curr_analysis)
{

	kf_result_t	rv = KF_SUCCESS;
	kf_analysis_t	save_curr_analysis;
	KF_DEBUG("\t\tkf_comp_analyze:doing component analysis......\n");

	if (curr_analysis)
		save_curr_analysis = *curr_analysis;
	KF_ASSERT(comp);
	switch(KLCF_COMP_TYPE(comp)) {

	case KLSTRUCT_CPU:
		if (curr_analysis) {
			curr_analysis->kfa_info[KF_IP27_LEVEL].kfi_type = KFTYPE_IP27;
			curr_analysis->kfa_info[KF_IP27_LEVEL].kfi_inst = BOARD_SLOT(board);
			if (kf_conf_tab[KF_SYSBUS_CONF_INDEX]) {
				curr_analysis->kfa_info[KF_SYSBUS_LEVEL].kfi_type = KFTYPE_SYSBUS;
				curr_analysis->kfa_info[KF_SYSBUS_LEVEL].kfi_inst = 0;
				curr_analysis->kfa_conf = *kf_conf_tab[KF_SYSBUS_CONF_INDEX];
				kf_guess_put(curr_analysis);
				curr_analysis->kfa_info[KF_SYSBUS_LEVEL].kfi_type = KF_UNKNOWN;
				curr_analysis->kfa_info[KF_SYSBUS_LEVEL].kfi_inst = KF_UNKNOWN;
				curr_analysis->kfa_conf = 0;
				
			}
			
		}


		rv = kf_cpu_analyze(CPU_COMP_ERROR(board,comp),comp->physid,curr_analysis);
		break;

	case KLSTRUCT_HUB:
		if (curr_analysis) {
			curr_analysis->kfa_info[KF_IP27_LEVEL].kfi_type = KFTYPE_IP27;
			curr_analysis->kfa_info[KF_IP27_LEVEL].kfi_inst = BOARD_SLOT(board);
		}
		rv = kf_hub_analyze(curr_analysis);
		break;

	case KLSTRUCT_ROU:
		if (curr_analysis) {
			curr_analysis->kfa_info[KF_ROUTER_LEVEL].kfi_inst = BOARD_SLOT(board);
		}
		rv = kf_router_analyze(curr_analysis);
		break;
	default:
		KF_DEBUG("\t\t\tkf_comp_analyze:unknown component type %d\n",KLCF_COMP_TYPE(comp));
		break;
	}

	if (curr_analysis)
		*curr_analysis = save_curr_analysis;
	KF_DEBUG("\t\tkf_comp_analyze:finished component analysis\n");
	
	return rv;
	
}

/*
 * do the router error analysis
 */
kf_result_t
kf_router_analyze(kf_analysis_t *curr_analysis)
{
	kf_result_t	rv = KF_SUCCESS;
	int		port;
	hubreg_t	link_status;

	kf_analysis_t	save_curr_analysis;

#define PORT_ERR_MASK	0x3ff000000	/* mask for the port error bits in status_error
					 * register of the router for a port 
					 */
#define ILL_PORT_MASK	0x200000000	/* illegal port direction error bit mask 
					 * in the status error register for a port
					 */
	KF_DEBUG("\t\t\tkf_router_analyze:doing router analysis..........\n");
	
	if (curr_analysis) {

		save_curr_analysis = *curr_analysis;
		curr_analysis->kfa_info[KF_ROUTER_LEVEL].kfi_type = KFTYPE_ROUTER;
		
		if (kf_conf_tab[KF_ROUTER_CONF_INDEX]) {
			curr_analysis->kfa_conf = *kf_conf_tab[KF_ROUTER_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}
	}
	
	/* go through each port's status register and update the
	 * confidence levels
	 */

	for(port = 0; port < MAX_ROUTER_PORTS;port++) {
		if (curr_analysis) {
			if (kf_conf_tab[KF_ROUTER_LINK0_CONF_INDEX + port]) {
				curr_analysis->kfa_info[KF_ROUTER_LINK_LEVEL].kfi_type = KFTYPE_ROUTER_LINK0;
				curr_analysis->kfa_info[KF_ROUTER_LINK_LEVEL].kfi_inst = port;
				if (kf_conf_tab[KF_ROUTER_LINK0_CONF_INDEX + port]) {
					curr_analysis->kfa_conf = *kf_conf_tab[KF_ROUTER_LINK0_CONF_INDEX + port];
					kf_guess_put(curr_analysis);
				}
			}
		} else {
				
			link_status	= 	kf_reg_tab[KF_ROUTER_STS_ERR0_INDEX + port];

			KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_ROUTER_LINK0_CONF_INDEX + port],
					      link_status & PORT_ERR_MASK,
					      60);

			KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
					      link_status & ILL_PORT_MASK,
					      70);
		}
	}
		
	if (curr_analysis)
		*curr_analysis = save_curr_analysis;
	KF_DEBUG("\t\t\tkf_router_analyze:finished router analysis\n");
	return rv;
}

/*
 * do the hub error analysis
 */
kf_result_t
kf_hub_analyze(kf_analysis_t *curr_analysis)
{
	kf_result_t	rv = KF_SUCCESS;
	kf_analysis_t	save_curr_analysis;

	KF_DEBUG("\t\t\tkf_hub_analyze:doing hub analysis..........\n");	
	if (curr_analysis) {

		save_curr_analysis = *curr_analysis;
		curr_analysis->kfa_info[KF_HUB_LEVEL].kfi_type = KFTYPE_HUB;
		curr_analysis->kfa_info[KF_HUB_LEVEL].kfi_inst = 0;

#ifdef PCOUNT_WAR

#define KF_ICRB_EXC(_icrbb_val) ((_icrbb_val >> 25) & 0x1f)

		if (kf_conf_tab[KF_HUB_CONF_INDEX] && 
		    (*kf_conf_tab[KF_HUB_CONF_INDEX] == FRU_FLAG_CONF)) {
			char slot_name[SLOTNUM_MAXLENGTH];
			kf_analysis_t new_analysis = *curr_analysis;
		  
			new_analysis.kfa_info[KF_PCOUNT_LEVEL].kfi_type = 
				KFTYPE_PCOUNT;
			new_analysis.kfa_info[KF_PCOUNT_LEVEL].kfi_inst = 0;
			new_analysis.kfa_conf = 
				*kf_conf_tab[KF_HUB_CONF_INDEX];
			
			kf_guess_put(&new_analysis);

#if !defined(FRUTEST)
			get_slotname(new_analysis.kfa_info[KF_IP27_LEVEL].kfi_inst,slot_name);

			/* Write to the PROM log in case we can't write to 
			the console. */
			ip27log_printf(IP27LOG_FATAL, 
				       "/hw/module/%d/slot/%s/node failed with "
				       "the incident 439797 error signature",
				       new_analysis.kfa_info[KF_MODULE_LEVEL].kfi_inst,
				       slot_name);
#endif /* !FRUTEST */
		}
		/* We are using the flag confidence value to indicate the
		node got the pcount hang. */
		if ((kf_conf_tab[KF_HUB_CONF_INDEX]) && 
		    (*kf_conf_tab[KF_HUB_CONF_INDEX] != FRU_FLAG_CONF)) {
#else /* !PCOUNT_WAR */
		if (kf_conf_tab[KF_HUB_CONF_INDEX]) {
#endif /* !PCOUNT_WAR */
			curr_analysis->kfa_conf = *kf_conf_tab[KF_HUB_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}
		if (kf_conf_tab[KF_HUB_LINK_CONF_INDEX]) {
			kf_analysis_t	tmp_analysis = *curr_analysis;

			curr_analysis->kfa_info[KF_HUB_LINK_LEVEL].kfi_type = KFTYPE_HUB_LINK;
			curr_analysis->kfa_info[KF_HUB_LINK_LEVEL].kfi_inst = 0;
			curr_analysis->kfa_conf = *kf_conf_tab[KF_HUB_LINK_CONF_INDEX];	
			kf_guess_put(curr_analysis);

			*curr_analysis = tmp_analysis;
		}
	}
#if defined(PCOUNT_WAR) && !defined(FRUTEST)
	else {
		__psunsigned_t hub_base;
		int i, crb, num_dex = 0;
		hubreg_t crb_entB;
		
		hub_base = (__psunsigned_t)REMOTE_HUB_ADDR(current_nasid, 0);

#define KF_PCOUNT_READ_REPEAT 2

		for (i = 0; i < KF_PCOUNT_READ_REPEAT; i++) {
			/* check multiple times just in case we caught the 
			ICRBs rigth before an ejection */

			for (crb = 0; crb < IIO_NUM_CRBS; crb++) {
				crb_entB = HUB_REG_PTR_L(hub_base,
							 IIO_ICRB_B(crb));

#ifdef PCOUNT_TEST
				printf("*****ICRB_B %d is: 0x%llx\n",crb,
				       crb_entB);
#endif
			
				/* If the entry is in DEX or RDEX mode,
				increment. */
				if ((KF_ICRB_EXC(crb_entB) == 11) ||
				    (KF_ICRB_EXC(crb_entB) == 6))
					num_dex++;
			}
		}

#ifdef PCOUNT_TEST
		printf("*****num_dex is %d\n",num_dex);
#endif

		if (num_dex == (2 * IIO_NUM_CRBS)) {
			/* If we read the registers twice and both times all
			ICRBs were in DEX or RDEX we have hit the problem. */

			KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_HUB_CONF_INDEX],
					      1,
					      FRU_FLAG_CONF);
		}
	}
#endif /* PCOUNT_WAR && !FRUTEST */
	
	if ((rv = kf_pi_analyze(curr_analysis)) != KF_SUCCESS)
		return rv;
	if ((rv = kf_md_analyze(curr_analysis)) != KF_SUCCESS)
		return rv;
	if ((rv = kf_ii_analyze(curr_analysis)) != KF_SUCCESS)
		return rv;
	rv = kf_ni_analyze(curr_analysis);

	if (curr_analysis)
		*curr_analysis = save_curr_analysis;
	KF_DEBUG("\t\t\tkf_hub_analyze:finished hub analysis\n");
	return rv;



}

/*
 * analyze if there are pi errors
 * in this function we look at the 
 *	PI_ERR_INT_PEND
 *	PI_ERR_STATUS0
 * 	PI_ERR_STATUS1
 */ 
/*
 * some useful macros for pi analysis
 */
/*
 * to get the bits corresponding to syscmd sysad & sysstate errors in
 * err_int_pend
 */
#define SYSBUS_ERR_A       	(PI_ERR_SYSSTATE_A         |	\
				 PI_ERR_SYSAD_DATA_A       |	\
				 PI_ERR_SYSAD_ADDR_A       |	\
				 PI_ERR_SYSCMD_DATA_A      |	\
				 PI_ERR_SYSCMD_ADDR_A      |	\
				 PI_ERR_SYSSTATE_TAG_A)
#define SYSBUS_ERR_B      	(PI_ERR_SYSSTATE_B         |	\
				 PI_ERR_SYSAD_DATA_B       |	\
				 PI_ERR_SYSAD_ADDR_B       |	\
				 PI_ERR_SYSCMD_DATA_B      |	\
				 PI_ERR_SYSCMD_ADDR_B      |	\
				 PI_ERR_SYSSTATE_TAG_B)

kf_result_t
kf_pi_analyze(kf_analysis_t *curr_analysis)
{

	/*ERR_INT_PEND
	 *	<23>		[A,60]
	 *	<17>		[HUB,70]
	 *	<15>
	 *	<13>
	 *	<11>
	 *	<9>
	 *
	 *	<22>		[B,60]
	 *	<16>		[HUB,70]
	 *	<14>
	 *	<12>
	 *	<10>
	 *	<8>
	 *
	 *	<24>		can't tell anything
	 *
	 *	<4>
	 *	<5>		[HUB,70]
	 *			[MEM,70]
	 *			[MD,60]
	 *
	 *	<6>
	 *	<7>		[MEM,70]
	 *
	 *ERR_STATUS0
	 *ERR_STATUS1
	 *	VALID
	 *
	 *	ERROR_TYPE
	 *		RTERR | WTERR |	[MEM,70]
	 *
	 *	   	UPWERR | UPRERR 
	 *
	 *
	 *	RRB STATUS BIT SEMANTICS
	 *		V - valid
	 *		E - type of req
	 *		T - target of an incoming intervention 
	 *		I - target of an incoming invalidate
	 *		R - resp. data given to T5
	 *		A - data ack recvd.
	 *		H - gathering invalidates
	 *		W - waiting for write to complete
	 *		P - double , single or partial word read
	 *		
	 *	WRB STATUS BIT SEMANTICS
	 *		V - valid
	 *		T - target of an incoming intervention 
	 *		W - received a WBBAK     
	 *		P - double, single or Partial word write 
	 *
	 *	CRB_STATUS
	 *
	 *		T
	 *		RRB & I		[MEM,70]
	 *				
	 *
	 *		W		[MEM,70]
	 *				
	 */
	
	kf_result_t	rv = KF_SUCCESS;

	hubreg_t	pi_err_sts0_a,pi_err_sts0_b;	
	hubreg_t	pi_err_sts1_a,pi_err_sts1_b;	

	extern int	KF_PI_RULE_INDEX;	
	nasid_t		nasid_a,nasid_b;
	paddr_t		addr_a , addr_b;

	KF_DEBUG("\t\t\t\tkf_pi_analyze:doing pi analysis........\n");

	if (curr_analysis) {
		kf_analysis_t	save_curr_analysis;

		save_curr_analysis = *curr_analysis;
		curr_analysis->kfa_info[KF_PI_LEVEL].kfi_type = KFTYPE_PI;	
		curr_analysis->kfa_info[KF_PI_LEVEL].kfi_inst = 0;	
		if (kf_conf_tab[KF_PI_CONF_INDEX]) {
			curr_analysis->kfa_conf = *kf_conf_tab[KF_PI_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}
		*curr_analysis = save_curr_analysis;
		return KF_SUCCESS;
	}

	/* first do the rule table driven analysis */
	kf_rule_tab_analyze(KF_PI_RULE_INDEX);



	/* copy the pi error registers into local varibles */

	pi_err_sts0_a	= kf_reg_tab[KF_PI_ERR_STS0_A_INDEX];
	pi_err_sts0_b	= kf_reg_tab[KF_PI_ERR_STS0_B_INDEX];
	pi_err_sts1_a	= kf_reg_tab[KF_PI_ERR_STS1_A_INDEX];
	pi_err_sts1_b	= kf_reg_tab[KF_PI_ERR_STS1_B_INDEX];
	


	/* to check the validity of any  of the err_status0 registers  */

#define MEM_ACC_ERR(_r1,_r2)	((_r1 & PI_ERR_ST0_VALID_MASK) ||	\
				 (_r2 & PI_ERR_ST0_VALID_MASK))


	/* 
	 * on page 9 of the hub programming manual it says that for a given
	 * cache line A[39:7] the Hspec address of the corr. directory entry 
	 * is given as A[39:31],1,1,A[30:12],1,A[11:7],DWbit,000.
	 *		     ------
	 *
 	 * on page 102 of the hub programming manual it says that the bank selects 
	 * for the DIMMs are always taken from the incoming address[31:29]
	 * 
	 * this means DIMM_SEL for a directory access can be either a 3 (M-mode) 
	 * or 7 (N-mode) only
	 *	
	 * IS THIS INTERPRETATION CORRECT ????
	 */

#define DIR_ERR_DIMM_SEL(_r)		((_r >> 27) & 0x7)



#define ERR_STS0_TYPE_MASK	0x3
#define	ERR_STS0_VALID(_r)	(_r &  PI_ERR_ST0_VALID_MASK)
#define ERR_STS0_ADDR(_r)	(_r >> 22)



#define ERR_STS0_ADDR_DIMM(_r)	((ERR_STS0_ADDR(_r) & MEM_DIMM_MASK) >> MEM_DIMM_SHFT)
	/* macros to get the error type from the err_status0 register */

	/* does the err_status0 register indicate a write error? */
#define WERR(_r)		((_r & ERR_STS0_TYPE_MASK) == 0x0)
	/* does the err_status0 register indicate an uncached partial read or write error? */
#define UPRW_ERR(_r)		((_r & ERR_STS0_TYPE_MASK) == 0x1)
	/* does the err_status0 register indicate a directory error? */
#define DERR(_r)		((_r & ERR_STS0_TYPE_MASK) == 0x2)
	/* does the err_status0 register indicate a timeout error? */
#define TO_ERR(_r)		((_r & ERR_STS0_TYPE_MASK) == 0x3)

	/* CRB type bit in the err_status_1 register */
#define WRB(_r)			(((_r & PI_ERR_ST1_WRBRRB_MASK) >> PI_ERR_ST1_WRBRRB_SHFT) == 1)


	addr_a 	= ERR_STS0_ADDR(pi_err_sts0_a);
	addr_b 	= ERR_STS0_ADDR(pi_err_sts0_b);
	nasid_a = NASID_GET(addr_a);
	nasid_b = NASID_GET(addr_b);


	/*
	 *
	 *	WERR  --> [MEM,70]
	 *	WERR & WRB ---> [SOFTWARE , 80]
	 */

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
			      ((ERR_STS0_VALID(pi_err_sts0_a) && WERR(pi_err_sts0_a) && WRB(pi_err_sts1_a)) ||
			       (ERR_STS0_VALID(pi_err_sts0_b) && WERR(pi_err_sts0_b) && WRB(pi_err_sts1_b))),
			      80);


	/* update the appropriate DIMM's confidence */

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + ERR_STS0_ADDR_DIMM(pi_err_sts0_a)],
			      ((nasid_a == current_nasid)  &&
			      (ERR_STS0_VALID(pi_err_sts0_a) && WERR(pi_err_sts0_a) && WRB(pi_err_sts1_a))),
			      70);
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + ERR_STS0_ADDR_DIMM(pi_err_sts0_b)],
			      ((nasid_b == current_nasid)  &&
    			      (ERR_STS0_VALID(pi_err_sts0_b) && WERR(pi_err_sts0_b) && WRB(pi_err_sts1_b))),
			      70);


	/*
	 *
	 *	UPRERR | UPWERR --> [MEM,70]
	 */
	/* update the appropriate DIMM's confidence */

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + ERR_STS0_ADDR_DIMM(pi_err_sts0_a)],
			      ((nasid_a == current_nasid)  &&
			      (ERR_STS0_VALID(pi_err_sts0_a) && UPRW_ERR(pi_err_sts0_a))),
			      70);
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + ERR_STS0_ADDR_DIMM(pi_err_sts0_b)],
			      ((nasid_b == current_nasid)  &&
			       (ERR_STS0_VALID(pi_err_sts0_b) && UPRW_ERR(pi_err_sts0_b))),
			      70);


	/* update the appropriate DIMM's confidence */

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + ERR_STS0_ADDR_DIMM(pi_err_sts0_a)],
			      ((nasid_a == current_nasid)  &&
			      (ERR_STS0_VALID(pi_err_sts0_a) && DERR(pi_err_sts0_a))),
			      70);

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + ERR_STS0_ADDR_DIMM(pi_err_sts0_b)],
			      ((nasid_b == current_nasid)  &&
			      (ERR_STS0_VALID(pi_err_sts0_b) && DERR(pi_err_sts0_b))),
			      70);

	/* CRB type bit in the err_status_1 register */
#define RRB(_r)			(((_r & PI_ERR_ST1_WRBRRB_MASK) >> PI_ERR_ST1_WRBRRB_SHFT) == 0)
	/* H bit of CRB status field in err_status_1 register */
#define H_MASK			0x800000000000ull
#define H(_r)		       	(_r & H_MASK)
#define RRB_N_H(_r1,_r2)	(ERR_STS0_VALID(_r1) && (RRB(_r2)) && (H(_r2)))

	 
	/* T bit of CRB status field in err_status_1 register */
#define T_MASK			0x200000000000ull
#define	T(_r)			(_r & T_MASK)

	/* I bit of CRB status field in err_status_1 register */
#define I_MASK			0x400000000000ull
#define I(_r)		       	(_r & I_MASK)
#define RRB_N_I(_r)		((RRB(_r)) && (I(_r)))
#define T_OR_RRB_N_I(_r1,_r2)   (ERR_STS0_VALID(_r1) && (T(_r2) || RRB_N_I(_r2)))

	/*
	 * T | (RRB & I) ---> [MEM,70]
	 */
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_MEM_CONF_INDEX],
			      (((nasid_a == current_nasid)  && T_OR_RRB_N_I(pi_err_sts0_a,pi_err_sts1_a)) ||
			       ((nasid_b == current_nasid)  && T_OR_RRB_N_I(pi_err_sts0_b,pi_err_sts1_b))),
			      70);

	/* W-bit of CRB status in the err_status_1 resgister */
#define W_MASK			0x1000000000000ull
#define	W(_r1,_r2)		(ERR_STS0_VALID(_r1) && (_r2 & W_MASK))

	/*
	 * W  ---> [MEM,70] 
	 */

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_MEM_CONF_INDEX],
			      (((nasid_a == current_nasid) && W(pi_err_sts0_a,pi_err_sts1_a)) || 
			       ((nasid_b == current_nasid) && W(pi_err_sts0_b,pi_err_sts1_b))),
			      70);

	KF_DEBUG("\t\t\t\tkf_pi_analyze:finished pi analysis\n");
	return rv;
}

#define SYNDROME(_r)		((_r >> 32) & 0xff)	/* syndrome bits in MD_MEM_ERROR register */
#define BAD_ECC_FORCED(_r)	(SYNDROME(_r) == 0xff)	/* check if syndrome == 0xff in MEM_ERROR */

#if 0
/* Check if bad ecc was written by MD (0xff) due to other
 * errors. Return 1 if this is true 0 otherwise
 */	    
static int
check_bad_ecc_forced(paddr_t paddr) 
{
	unsigned char ecc;

#ifdef FRUTEST
	extern int force_bad_ecc;
	if (force_bad_ecc)
		ecc = (uint)0xff;
	else
		ecc = 0;
#else
	/* get the ecc byte*/
	ecc = *((volatile unsigned char *)BDECC_ENTRY(paddr));
#endif

#ifdef FRU_DEBUG_RULES
	kf_print("++check_bad_ecc_forced : "
		 "paddr = 0x%llx ECC addr=0x%llx ecc = 0x%x\n",
		 paddr,BDECC_ENTRY(paddr),ecc);
#endif

	if (ecc == 0xff) {
#ifdef FRU_DEBUG_RULES
		kf_print("++check_bad_ecc_forced: Bad ECC external cause\n");
#endif 
		return 1;
	} else {
#ifdef FRU_DEBUG_RULES
		kf_print("++check_bad_ecc_forced: Not Bad ECC due to external"
			 "cause : 0x%x\n",ecc);
#endif 
	}

	return 0;
}	

/* Check if the memory error is due to a bad dimm 
 * return the belief that memory is the suspect.
 */
static confidence_t
kf_check_mem_error(int valid,paddr_t  offset) 
{
	paddr_t		paddr;

	/* check if the error is valid */
	if (!valid)
		return 0;

	/* Form the physical address from the nasid & the node offset */
	paddr = TO_NODE(current_nasid,offset);

#ifdef FRU_DEBUG_RULES
	kf_print("++kf_check_mem_error: paddr = 0x%llx nasid = 0x%x offset = 0x%x\n",
		 paddr,current_nasid,offset);
#endif
	/* If MD was not forced to write 0xff as ecc
	 * then it is definitely a memory bank problem
	 */
	if (!check_bad_ecc_forced(paddr))
		return 90;	/* Suspect memory with a high prob 90% */

	return 70; /* Suspect memory with a prob 70% */		
}
#endif
/* macros to check whether the cache err register indicates
 * the primary instruction cache , primary data cache ,
 * secondary cache or system interface error
 */

#ifdef FRUTEST
#define R10K_CACHE_ERR_VALID		g_ce_valid
#else
#define R10K_CACHE_ERR_VALID		1
#endif	/* FRUTEST */


/* primary instruction cache error ? */
#define R10K_ICACHE_ERR(_r)		(R10K_CACHE_ERR_VALID &&	\
					 ((_r & CE_TYPE_MASK) == CE_TYPE_I))

/* primary data cache error ? */
#define R10K_DCACHE_ERR(_r)		(R10K_CACHE_ERR_VALID && 	\
					 ((_r & CE_TYPE_MASK) == CE_TYPE_D))

/* secondary cache error ? */
#define R10K_SCACHE_ERR(_r)		(R10K_CACHE_ERR_VALID && 	\
					 ((_r & CE_TYPE_MASK) == CE_TYPE_S))

/* system interface error ? */
#define R10K_SYSINTF_ERR(_r)		(R10K_CACHE_ERR_VALID && 	\
					 ((_r & CE_TYPE_MASK) == CE_TYPE_SIE))

/*
 * Get the MD_MEM_ERROR register on a node with
 * the given nasid
 */		

hubreg_t
kf_mem_error_get(nasid_t nasid) 
{
	lboard_t	*board;		/* IP27 board pointer */
	klhub_err_t	*hub_err;	/* hub info pointer */

#ifdef FRUTEST
	board	= (lboard_t *)g_node[nasid].ch_board_info;
#else
	board 	= (lboard_t *)KL_CONFIG_INFO(nasid);
#endif	/* #ifdef FRUTEST */
	board	= find_lboard(board,KLTYPE_IP27);

	/* Sanity check */
	if (!board)
		return 0;
	/* get the hub error info for the hub in this ip27 board */
	hub_err	= kf_hub_err_info_get(board);

	/* Sanity check */
	if (!hub_err)
		return 0;

	return KF_MD_MEM_ERR(hub_err);

}
/* Some useful macros to operate on the MD_MEM_ERROR 
 * register
 */
#define MEM_ERR_UCE_MASK	0x8000000000000000
#define MEM_ERR_UCE_SHFT	63
#define	MEM_ERR_UCE(_r)		((_r & MEM_ERR_UCE_MASK) >> MEM_ERR_UCE_SHFT)
#define MEM_ERR_ECC_MASK	0xff00000000
#define MEM_ERR_ECC_SHFT	32	
#define MEM_ERR_ADDR_DIMM(_r)	((_r & MEM_DIMM_MASK) >> MEM_DIMM_SHFT)
#define MEM_ERR_ECC_FORCED(_r)	(((_r & MEM_ERR_ECC_MASK) >> MEM_ERR_ECC_SHFT) == 0xff)
#define MEM_ERR_ADDR(_r)	(_r & 0xfffffff8)
#define SCACHE_LINE_MASK	0xffffff80

/*
 *	Return the belief that scache is bad.
 */
static int
kf_check_scache_way_error(uint cerr,__uint64_t tag) 
{
	hubreg_t      	mem_err;	/* MD_MEM_ERROR register from the relevant node */
	nasid_t		mem_err_nasid;	/* nasid of the relevant node */
	paddr_t		paddr,mem_paddr;/* paddr = phy. addr. from the cache 
					 * mem_paddr = phy. addr from the MD_MEM_ERROR
					 */

	/* Form the physical address from the cache tag & index */
	paddr 		= SCACHE_ERROR_ADDR(cerr,tag);
	mem_err_nasid 	= NASID_GET(paddr);	/* Get the nasid corr. to the home node
						 * of this address
						 */
	mem_err 	= kf_mem_error_get(mem_err_nasid);	/* Get the mem error register
								 * on this node 
								 */
	/* If only the scache error is set and the
	 * mem error is not set then it is definitely
	 * the scache
	 */
	if (!(MEM_ERR_UCE(mem_err))) {
		if (R10K_SCACHE_ERR(cerr))
			return 90;	/* Suspect scache with 90% */
	}

	/* Form the physical address from the MD_MEM_ERROR
	 * register
	 */
	mem_paddr	= TO_NODE(mem_err_nasid,MEM_ERR_ADDR(mem_err));
		
#ifdef FRU_DEBUG_RULES
	kf_print("++kf_check_scache_way_error: "
		 "MEM_ERR_ADDR(mem_err) = 0x%x"
		 " mem_paddr = 0x%llx\n"
		 "*kf_check_scache_way_error: "
		 "cache_addr = 0x%llx"
		 " mem_err = 0x%llx mem_err_nasid = 0x%x\n",
		 MEM_ERR_ADDR(mem_err),mem_paddr,paddr,
		 mem_err,mem_err_nasid);
#endif	
		
	/* check if there is already memory error
	 * for this cache line
	 */
		
	if ((mem_paddr & CACHE_SLINE_MASK) == 
	    (paddr & CACHE_SLINE_MASK)) {
		/* Check that md has not written bad ecc
		 * In this case donot suspect scache it is a
		 * memory problem.
		 */
		if (!BAD_ECC_FORCED(mem_err))
			return 0;	
	} else if (R10K_SCACHE_ERR(cerr))
		return 90;	/* If there is an scache error
				 * and the MD_MEM_ERROR is set for
				 * a different phs. addr. suspect
				 * scache with 90% prob. 
				 */
		
	return -1; 	/* Not able to decide */

}
/* Check if the scache or sysinf error is actually 
 * due to a bad memory .
 * Update the scache confidence appropriately.
 */
static int
kf_check_scache_error(uint cerr, cache_err_t cache_err) {

	int 	belief = 0;

	/* Check if the scache error is valid */
	if (!cerr && !(R10K_SCACHE_ERR(cerr)) && !(R10K_SYSINTF_ERR(cerr))) {
#ifdef FRU_DEBUG_RULES
		kf_print("++cache error reg = 0x%x\n",cerr);
#endif
		return 0;
	}


	/* Check if there is a tag error. This is definitely the case for
	 * a bad scache.
	 */
	if ((cerr & CE_TA_WAY0) || (cerr & CE_TA_WAY1))
		return(90);

	/* Check if there is a memory ecc error for an address
	 * in this cache line
	 */

	/* Form the physical address that caused the scache error */
	if (cerr & CE_D_WAY0) {
		belief = kf_check_scache_way_error(cerr,cache_err.ce_tags[0]);
	}

	if (cerr & CE_D_WAY1)  {
		int temp_belief = kf_check_scache_way_error(cerr,
							    cache_err.ce_tags[1]);
		belief = (temp_belief < belief ? belief : temp_belief);
	}		

	if (belief >= 0)
		return belief;	/* If we are able to assign confidence then
				 * return that
				 */
	if (R10K_SYSINTF_ERR(cerr))	/* Dont suspect scache on system
					 * interface error and we are
					 * not able to conclude anything
					 * concrete
					 */
		return 0;
	return 70;	/* Default Suspect scache with a prob 70% */
}
/*
 * analyze if there are md errors
 */
kf_result_t
kf_md_analyze(kf_analysis_t *curr_analysis)
{


	/*
	 *DIR_ERROR
	 * 	UCE_VALID	[MEM,70]
	 *	AE_VALID	
	 * 			[MD,60]
	 *			[HUB,60]
			
	 *PROTOCOL_ERROR
	 *	valid bit	[SOFTWARE,60]
	 *			[HUB,60]

	 *MEM_ERROR
	 *	UCE_VALID	[MEM,70]
	 *			[MD,60]
	 *			[HUB,60]
	 *
	 *MISC_ERROR
	 *	ILL_MSG		[HUB,70]
	 *	ILL_REVISION	
	 *	LONG_PACK	
	 *	SHORT_PACK
	 *
	 *	BAD_DATA	[HUB,0]
	 *			[MD,0]
	 *			[MEM,0]
	 *			[SYSBUS,60]
	 *			[A,60]
	 *			[B,60]
	 */
	
	kf_result_t		rv = KF_SUCCESS;
	hubreg_t		dir_err,mem_err,proto_err;
	extern 	int		KF_MD_RULE_INDEX;
	int			suspect_belief = 0;

	KF_DEBUG("\t\t\t\tkf_md_analyze:doing md analysis........\n");
	
	if (curr_analysis) {

		kf_analysis_t 	save_curr_analysis;
		int		dimm;


		/* Check if we have put in a special confidence in the
		 * MD confidence which is not being used for any other
		 * purpose. If so then we have hit a t5 writeback surprise
		 * protocol bug.
		 */
		if (kf_conf_tab[KF_MD_CONF_INDEX] && 
		    (*kf_conf_tab[KF_MD_CONF_INDEX] == FRU_FLAG_CONF)) {

			kf_analysis_t 	new_analysis = *curr_analysis;
		  
			new_analysis.kfa_info[KF_T5_WB_SURPRISE_LEVEL].kfi_type = 
				KFTYPE_T5_WB_SURPRISE;
			new_analysis.kfa_info[KF_T5_WB_SURPRISE_LEVEL].kfi_inst = 0;
			new_analysis.kfa_conf = 
				*kf_conf_tab[KF_MD_CONF_INDEX];
		
			kf_guess_put(&new_analysis);

		}
		save_curr_analysis = *curr_analysis;
		curr_analysis->kfa_info[KF_MD_LEVEL].kfi_type = KFTYPE_MD;	
		curr_analysis->kfa_info[KF_MD_LEVEL].kfi_inst = 0;	

		/* If we had already stored FRU_FLAG_CONF value in md confidence
		 * make sure that it doesn't get interpreted as faulty md
		 */
		if (kf_conf_tab[KF_MD_CONF_INDEX]) {
			curr_analysis->kfa_conf =  
				(*kf_conf_tab[KF_MD_CONF_INDEX] == 
				 FRU_FLAG_CONF) ? 0 :
				*kf_conf_tab[KF_MD_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}
		curr_analysis->kfa_info[KF_MEM_LEVEL].kfi_type = KFTYPE_MEM;	
		curr_analysis->kfa_info[KF_MEM_LEVEL].kfi_inst = 0;	
		if (kf_conf_tab[KF_MEM_CONF_INDEX]) {
			curr_analysis->kfa_conf = *kf_conf_tab[KF_MEM_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}
		
		for(dimm = 0 ; dimm < 8; dimm++) {
			curr_analysis->kfa_info[KF_DIMM_LEVEL].kfi_type = KFTYPE_DIMM0;
			curr_analysis->kfa_info[KF_DIMM_LEVEL].kfi_inst = dimm;
			if (kf_conf_tab[KF_DIMM0_CONF_INDEX + dimm])  {
				curr_analysis->kfa_conf = *kf_conf_tab[KF_DIMM0_CONF_INDEX + dimm];
				kf_guess_put(curr_analysis);
			}
		}

		*curr_analysis = save_curr_analysis;
		return KF_SUCCESS;
			
	}


	/* first do the rule table driven analysis */
	kf_rule_tab_analyze(KF_MD_RULE_INDEX);

	/* copy the error registers into local variables */

	dir_err			= kf_reg_tab[KF_MD_DIR_ERR_INDEX];
	mem_err			= kf_reg_tab[KF_MD_MEM_ERR_INDEX];
	proto_err		= kf_reg_tab[KF_MD_PROTO_ERR_INDEX];
	/* macros to check if there is an uncorrectable directory ecc
	 * error or an access error
	 */

#define DIR_ERR_UCE_AE_MASK	0xc000000000000000
#define DIR_ERR_UCE_AE_SHFT	62
#define	DIR_ERR_UCE_AE(_r)	((_r & DIR_ERR_UCE_AE_MASK) >> DIR_ERR_UCE_AE_SHFT)
#define DIR_ERR_HSPEC_ADDR_MASK	0x3ffffff8
#define DIR_ERR_HSPEC_ADDR(_r)	(_r & DIR_ERR_HSPEC_ADDR_MASK)

	/*
	 * DIR_ERR[UCE_VALID | AE_VALID] ---> [MEM,80],[SOFTWARE,40]
	 */
	
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + 
					  DIR_ERR_DIMM_SEL(dir_err)],
			      DIR_ERR_UCE_AE(dir_err),
			      80);
	

	if (!BAD_ECC_FORCED(mem_err))
		suspect_belief = 90;
	else
		suspect_belief = 70;

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DIMM0_CONF_INDEX + 
					  MEM_ERR_ADDR_DIMM(mem_err)],
			      MEM_ERR_UCE(mem_err),
			      suspect_belief);

#define PROTO_ERR_VALID(_r)	((_r >> 63) & 0x1)	

	/* If we hit a protocol error this is due to the
	 * T5 writeback surprise . Recognize this by storing
	 * a special value in the MD confidence.
	 */
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_MD_CONF_INDEX],
			      PROTO_ERR_VALID(proto_err),
			      FRU_FLAG_CONF);
			      

	KF_DEBUG("\t\t\t\tkf_md_analyze:finished md analysis\n");
	return rv;

}
/*
 * analyze if there are io errors
 */
kf_result_t
kf_ii_analyze(kf_analysis_t *curr_analysis)
{


	/*
	 * BTE ERR STATUS
	 *		[SRCMEM,60]
	 *		[DESTMEM,60]
	 *		[SOFTWARE,60]
	 * 	
	 */

	kf_result_t	rv = KF_SUCCESS;
#if 0
	confidence_t	*mem_confidence;
	confidence_t	*dimm_confidence;
	hubreg_t	bte0_sts,bte1_sts;
	hubreg_t	bte0_src,bte1_src;
	hubreg_t	bte0_dst,bte1_dst;	
#endif
	hubreg_t	widget_status;	
	int 		i;

	KF_DEBUG("\t\t\t\tkf_ii_analyze:doing ii analysis..........\n");

	if (curr_analysis) {
		kf_analysis_t	save_curr_analysis;

		/* Check if we have put in a special confidence in the
		 * MD confidence which is not being used for any other
		 * purpose. If so then we have hit a t5 writeback surprise
		 * protocol bug.
		 */
		if (kf_conf_tab[KF_II_CONF_INDEX] && 
		    (*kf_conf_tab[KF_II_CONF_INDEX] == FRU_FLAG_CONF)) {

			kf_analysis_t 	new_analysis = *curr_analysis;
		  
			new_analysis.kfa_info[KF_BTE_PUSH_LEVEL].kfi_type = 
				KFTYPE_BTE_PUSH;
			new_analysis.kfa_info[KF_BTE_PUSH_LEVEL].kfi_inst = 0;
			new_analysis.kfa_conf = 
				*kf_conf_tab[KF_II_CONF_INDEX];
		
			kf_guess_put(&new_analysis);

		}
		save_curr_analysis = *curr_analysis;
		curr_analysis->kfa_info[KF_II_LEVEL].kfi_type = KFTYPE_II;	
		curr_analysis->kfa_info[KF_II_LEVEL].kfi_inst = 0;	
		if (kf_conf_tab[KF_II_CONF_INDEX]) {
			curr_analysis->kfa_conf = 
				(*kf_conf_tab[KF_II_CONF_INDEX] == 
				 FRU_FLAG_CONF) ? 0 : 
				*kf_conf_tab[KF_II_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}

		*curr_analysis = save_curr_analysis;
		return KF_SUCCESS;
	}

#if 0
	/* copy the error register values into local variables */

	bte0_sts 	= kf_reg_tab[KF_II_BTE0_STS_INDEX];
	bte1_sts 	= kf_reg_tab[KF_II_BTE1_STS_INDEX];
	bte0_src 	= kf_reg_tab[KF_II_BTE0_SRC_INDEX];
	bte1_src 	= kf_reg_tab[KF_II_BTE1_SRC_INDEX];
	bte0_dst 	= kf_reg_tab[KF_II_BTE0_DST_INDEX];
	bte1_dst 	= kf_reg_tab[KF_II_BTE1_DST_INDEX];


	/* to check if the bte status indicates an error */

#define BTE_ERR(_r) 		((_r >> IBLS_ERROR_SHFT) & 0x1)

	/*
	 *     BTE ERROR ---> [SOFTWARE,60], 
	 *                    [SRCMEM | DSTMEM  , 60]?
	 */
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
			      BTE_ERR(bte0_sts) || BTE_ERR(bte1_sts) ,
			      60);

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_II_CONF_INDEX],
			      BTE_ERR(bte0_sts) || BTE_ERR(bte1_sts) ,
			      60);


	/* M mode #defines to get the node id out of a physical address
	 * SHOULD EVENTUALLY TAKE CARE OF BOTH THE MODES . 
	 * LOOK FOR #defs IN sys/SN HEADERS
	 */



	kf_mem_conf_get(bte0_src,&mem_confidence,&dimm_confidence);

	if (mem_confidence) {
		KF_CONDITIONAL_UPDATE(mem_confidence,
				      BTE_ERR(bte0_sts),
				      60);
	}
	if (dimm_confidence) {
		KF_CONDITIONAL_UPDATE(dimm_confidence,
				      BTE_ERR(bte0_sts),
				      60);
	}
	kf_mem_conf_get(bte1_src,&mem_confidence,&dimm_confidence);

	if (mem_confidence)
		KF_CONDITIONAL_UPDATE(mem_confidence,
				      BTE_ERR(bte1_sts),
				      60);

	if (dimm_confidence) {
		KF_CONDITIONAL_UPDATE(dimm_confidence,
				      BTE_ERR(bte1_sts),
				      60);
	}
	kf_mem_conf_get(bte0_dst,&mem_confidence,&dimm_confidence);
	
	if (mem_confidence)
		KF_CONDITIONAL_UPDATE(mem_confidence,
				      BTE_ERR(bte0_sts),
				      60);
	if (dimm_confidence) {
		KF_CONDITIONAL_UPDATE(dimm_confidence,
				      BTE_ERR(bte0_sts),
				      60);
	}

	kf_mem_conf_get(bte1_dst,&mem_confidence,&dimm_confidence);

	if (mem_confidence)
		KF_CONDITIONAL_UPDATE(mem_confidence,
				      BTE_ERR(bte1_sts),
				      60);

	if (dimm_confidence) {
		KF_CONDITIONAL_UPDATE(dimm_confidence,
				      BTE_ERR(bte1_sts),
				      60);
	}
#endif
#define CRAZY(_r)		((_r >> 32) & 0x1)	/* crazy bit in widget status */
	widget_status = kf_reg_tab[KF_II_WIDGET_STATUS_INDEX];

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_HUB_LINK_CONF_INDEX],
			      CRAZY(widget_status),
			      80);

	/*
	 * IO CRB ENTRY A analysis 
	 */
	
	for (i = 0; i < IIO_NUM_CRBS; i++) {
		    KF_DEBUG("\t\t\t\tdoing ii crb %d analysis..........\n",i);
		    kf_ii_crb_analyze(kf_reg_tab[KF_II_CRB_ENT0_A_INDEX + i]);
		    KF_DEBUG("\t\t\t\tfinished ii crb analysis\n");
	}

	KF_DEBUG("\t\t\t\tkf_ii_analyze:finished ii analysis\n");
	return rv;
}
/* States in which capture the sequence of bits seen so far
 * in the crb address
 */
#define STATE00		0	/* sequence seen so far is 0* */
#define STATE01		1	/* sequence seen so far is 0*1+ */
#define STATE10		2	/* seqeunce seen so far is 0*1+0+ */
#define STATE11		3	/* seqeunce seen so far is 0*1+0+1+ */

/* 
 * Check if the CRB address corresponds to a pattern which 
 * occurs on a BTE PUSH pattern bug
 */
int
kf_bte_push_pattern_check(paddr_t crb_addr)
{
	char	state = STATE00;
	char	bit = 0;    

	/* Crb address that we are getting here is 
	 * 	bits [39-7] of the actual crb error address
	 * 	which are in turn [32:0] of "crb_addr".
	 * Make sure that bit 32 of crb_addr is 0 for this case.
	 */
	if (crb_addr >> 32)
		return(0);

	while(crb_addr) {
		bit = crb_addr & 1;
		/* This switch statement implements the state machine to 
		 * recognize the pattern 0*1+0*
		 */
		switch(state) {
		case STATE00:
			/* STATE00 ---(1)--- STATE01
			 * |	 |
			 * +-(0)-+
			 */
			state = bit ? STATE01 : STATE00;
			break;
		case STATE01:
			/* STATE01 ---(0)--- STATE10
			 * |	 |
			 * +-(1)-+
			 */
			state = bit ? STATE01 : STATE10;
			break;
		case STATE10:
			/* STATE10 ---(1)--- STATE11
			 * |	 |
			 * +-(0)-+
			 */
			state = bit ? STATE11 : STATE10;
			break;
		case STATE11:
			/* +-(1)-+
			 * |     |
			 * STATE11
			 * |	 |
			 * +-(0)-+
			 */
			state = bit ? STATE11 : STATE11;
			break;
		}
		crb_addr >>= 1;
	}
	if (state == STATE11 || state == STATE00)
		return(0);
	return(1);
		
}




/*
 * analyze the io crb entryA registers
 */
kf_result_t
kf_ii_crb_analyze(hubreg_t	crb_ent_a)
{
	kf_result_t	rv = KF_SUCCESS;
	paddr_t		crb_addr;
	confidence_t	*mem_conf = 0,*dimm_conf = 0;

#define CRB_ADDR(_r) 		((_r & 0xfffffffffc) << 1)/* Addr. of the req. in legonet fmt */
#define CRB_SIDN(_r)		((_r >> 45) & 0xf)	/* Source ID number of the req */
#define CRB_TNUM(_r)		((_r >> 40) & 0x1f)	/* Transaction number of the req . */
#define CRB_ERR_VALID(_r)	((_r >> 55) & 0x1)	/* An error was encountered with this
							 * CRB 
							 */
#define CRB_ERRCODE(_r)		((_r >> 52) & 0x7)
#define CRB_PERR(_r)		(CRB_ERRCODE(_r) == 1)	/* poison error */
#define CRB_WERR(_r)		(CRB_ERRCODE(_r) == 2)	/* write error */
#define CRB_AERR(_r)		(CRB_ERRCODE(_r) == 3)	/* access error */
#define CRB_PWERR(_r)		(CRB_ERRCODE(_r) == 4)	/* partial write error */
#define CRB_PRERR(_r)		(CRB_ERRCODE(_r) == 5)	/* partial read error */
#define CRB_TO_ERR(_r)		(CRB_ERRCODE(_r) == 6)	/* timeout error */
#define CRB_XTERR(_r)		(CRB_ERRCODE(_r) == 7)	/* xtalk packet has hdr or sideband error bit set*/
#define CRB_DERR(_r)		(CRB_ERRCODE(_r) == 0)	/* directory error */
#define CRB_LN_UCE_MASK		0x08000000000000
#define CRB_LN_UCE(_r)		(_r & CRB_LN_UCE_MASK)	/* uncorrectable error on sn0net data */
#define CRB_XT_ERR_MASK		0x02000000000000
#define CRB_XT_ERR(_r)		(_r & CRB_XT_ERR_MASK)	/* xtalk request has error bit set */

	/* WERR | AERR | WERR | PRERR | PWERR --> [SOFTWARE,70] */
	
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
			      (CRB_ERR_VALID(crb_ent_a) &&
			       (CRB_WERR(crb_ent_a) || CRB_AERR(crb_ent_a) ||
				CRB_PWERR(crb_ent_a) || CRB_PRERR(crb_ent_a))),
			      70);

	/* PERR --> [SOFTWARE , 50] */
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
			      (CRB_ERR_VALID(crb_ent_a) &&
			       CRB_PERR(crb_ent_a)),
			      50);

	

	/* DERR --> [MD , 60] */
	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_MD_CONF_INDEX],
			      (CRB_ERR_VALID(crb_ent_a) &&
			       CRB_DERR(crb_ent_a)),
			      60);
	/* Get the crb error address in hub internal format */
	crb_addr = CRB_ADDR(crb_ent_a);

	/* Convert the addr into R10k format */
	crb_addr = TO_NODE(NASID_GET(crb_addr),
			   TO_NODE_ADDRSPACE(crb_addr));


	kf_mem_conf_get(crb_addr,&mem_conf,&dimm_conf);
	
	if (dimm_conf)
		KF_CONDITIONAL_UPDATE(dimm_conf,
				      (CRB_ERR_VALID(crb_ent_a) &&
				       CRB_DERR(crb_ent_a)),
				      60);
	/* Most significant 6 bits of CRB entry A are reserved. These
	 * are being used for special hacks like recognizing a BTE PUSH 
	 * op etc.
	 */
#define CRB_RESERVED_MASK	0x3f	/* 6 reserved bits */
#define CRB_RESERVED_SHFT	58	/* bits 63-58 are reserved */
#define CRB_RESERVED(_r)	((_r >> CRB_RESERVED_SHFT) & CRB_RESERVED_MASK)
#define BTE_OP(_r)		(CRB_RESERVED(_r) == 0x1)  /* bte operation */

	KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_II_CONF_INDEX],
			      BTE_OP(crb_ent_a) && 
			      kf_bte_push_pattern_check(crb_addr >> 0x7),
			      FRU_FLAG_CONF);
		
	return rv;
}
/*
 * analyze if there are ni errors
 */
kf_result_t
kf_ni_analyze(kf_analysis_t *curr_analysis)
{


	/*NI_VECTOR_STATUS
	 *	STATUS_VALID
	 *
	 *	TYPE
	 *		ADDR ERROR	[SOFTWARE,70]
	 *		COMMAND ERROR	
	 *		PROT VIOLATION	
	 *
	 *				
	 *		UNKNOWN RESPONSE 	
	 *					[REMOTE-BOARD,60]
	 *
	 *PORT_ERROR
	 *
	 *	NETWORK ERROR BIT SET	
	 *	INTERNAL ERROR BIT SET  [HUB,90]
	 *				[PI/MD/II,60]
	 */
	kf_result_t	rv = KF_SUCCESS;       
	hubreg_t	vec_sts;
	extern 	int	KF_NI_RULE_INDEX;


	vec_sts		= kf_reg_tab[KF_NI_VECT_STS_INDEX];
	

	KF_DEBUG("\t\t\t\tkf_ni_analyze:doing ni analysis.........\n");
	
	if (curr_analysis) {
		kf_analysis_t	save_curr_analysis;
		
		save_curr_analysis = *curr_analysis;
		curr_analysis->kfa_info[KF_NI_LEVEL].kfi_type = KFTYPE_NI;	
		curr_analysis->kfa_info[KF_NI_LEVEL].kfi_inst = 0;	
		if (kf_conf_tab[KF_NI_CONF_INDEX]) {
			curr_analysis->kfa_conf = *kf_conf_tab[KF_NI_CONF_INDEX];
			kf_guess_put(curr_analysis);
		}

		*curr_analysis = save_curr_analysis;

		return KF_SUCCESS;
	}

	/* first do the table driven analysis */
	kf_rule_tab_analyze(KF_NI_RULE_INDEX);

        /* to check if the vector status is valid */

#define VEC_STAT_VALID(_r)              (_r & NVS_VALID)
#define VEC_STAT_ERR_MASK               0x7

        /* to check if the vector status register indicates an
         *              address  error
         *              command  error
         *              protection violation
         */

#define  ADDR_CMD_PROT_ERR(_r)          (VEC_STAT_VALID(_r) &&                \
                                         (((_r & VEC_STAT_ERR_MASK)  != 7) && \
                                         ((_r & VEC_STAT_ERR_MASK)  >= 4)))
        /*
         * ADDR | COMMAND | PROTECTION ERROR ---> [SOFTWARE,70]
         */
        KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
                              ADDR_CMD_PROT_ERR(vec_sts),
                              70);
	/* to check if the vector status register indicates an unknown
	 * response
	 */
#define UNKNOWN_RESP(_r)		((_r & VEC_STAT_ERR_MASK)  == 7)

	KF_DEBUG("\t\t\t\tkf_ni_analyze:finished ni analysis\n");
	return rv;
}

/*
 * analyze if there are any cache errors
 */
kf_result_t
kf_cpu_analyze(klcpu_err_t *cpu_err,unsigned int cpuid,kf_analysis_t *curr_analysis)
{
	/* Make sure that the physical id of the cpu is valid */
	ASSERT_ALWAYS(cpuid < CPUS_PER_NODE);

	if (!curr_analysis)	{
		uint	cache_err;
		int	belief = 0;
		/* if the cache error saved is not valid 
		 * there is no need to analyze that
		 */
		if (cpu_err->ce_valid != 1)		       
			return KF_SUCCESS;
		/* get the dumped cache error register */
		cache_err	=  cpu_err->ce_cache_err_dmp.ce_cache_err;

		KF_DEBUG("\t\t\tkf_cpu_analyze:doing cpu%d analysis........\n",
			 cpuid);
	


		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_IC0_CONF_INDEX + cpuid],
				      R10K_ICACHE_ERR(cache_err),
				      90);

		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_DC0_CONF_INDEX + cpuid],
				      R10K_DCACHE_ERR(cache_err),
				      90);

		belief = kf_check_scache_error(cache_err,
					       cpu_err->ce_cache_err_dmp);
		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SC0_CONF_INDEX + cpuid],
				      belief,
				      belief);

		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SYSBUS_CONF_INDEX],
				      R10K_SYSINTF_ERR(cache_err) &&
				      cache_err & (CE_SA | CE_SC | CE_SR) ,
				      70);

		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_HUB_CONF_INDEX],
				      R10K_SYSINTF_ERR(cache_err) &&
				      cache_err & (CE_SA | CE_SC | CE_SR) ,
				      70);

		KF_DEBUG("\t\t\tkf_cpu_analyze:finished cpu analysis\n");
	} else {
		kf_analysis_t	save_curr_analysis;

		save_curr_analysis = *curr_analysis;

		curr_analysis->kfa_info[KF_CPU_LEVEL].kfi_type = KFTYPE_CPU0;	
		curr_analysis->kfa_info[KF_CPU_LEVEL].kfi_inst = cpuid;	
		if (kf_conf_tab[KF_CPU0_CONF_INDEX + cpuid]) {
			curr_analysis->kfa_conf = *kf_conf_tab[KF_CPU0_CONF_INDEX+cpuid];
			kf_guess_put(curr_analysis);
		}
		
		if (kf_conf_tab[KF_IC0_CONF_INDEX + cpuid]) {
			curr_analysis->kfa_info[KF_IC_LEVEL].kfi_type = KFTYPE_IC0;
			curr_analysis->kfa_info[KF_IC_LEVEL].kfi_inst = cpuid;
			curr_analysis->kfa_conf = *kf_conf_tab[KF_IC0_CONF_INDEX+cpuid];
			kf_guess_put(curr_analysis);
		}
		if (kf_conf_tab[KF_DC0_CONF_INDEX + cpuid]) {
			curr_analysis->kfa_info[KF_DC_LEVEL].kfi_type = KFTYPE_DC0;
			curr_analysis->kfa_info[KF_DC_LEVEL].kfi_inst = cpuid;
			curr_analysis->kfa_conf = *kf_conf_tab[KF_DC0_CONF_INDEX+cpuid];
			kf_guess_put(curr_analysis);
		}
		if (kf_conf_tab[KF_SC0_CONF_INDEX + cpuid]) {
			curr_analysis->kfa_info[KF_SC_LEVEL].kfi_type = KFTYPE_SC0;
			curr_analysis->kfa_info[KF_SC_LEVEL].kfi_inst = cpuid;
			curr_analysis->kfa_conf = *kf_conf_tab[KF_SC0_CONF_INDEX+cpuid];
			kf_guess_put(curr_analysis);
		}

		*curr_analysis = save_curr_analysis;

	}

	return KF_SUCCESS;
}

