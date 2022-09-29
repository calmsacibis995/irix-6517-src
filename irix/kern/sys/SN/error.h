/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: error.h
 *	SN0 specific error header file.
 */

#ifndef __SYS_SN_ERROR_H__
#define __SYS_SN_ERROR_H__

#include <sys/PCI/bridge.h>
#include <sys/iograph.h>
#include <sys/SN/xbow.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error_info.h>

/*
 * WARNING! Changes to the size of these structures should be done with
 *	    care. The prom is aware of these structures and allocates 
 *	    the structures. Any change will probably mean a new prom.
 */

/*
 *	Cpu errors.
 */

typedef struct cache_err_s {
	uint	ce_cache_err;
	__uint64_t	ce_tags[2];
} cache_err_t;

#if !defined (_STANDALONE)
#define ERROR_TIMESTAMP 	GET_LOCAL_RTC
#else
#define ERROR_TIMESTAMP		HUB_REG_PTR_L(LOCAL_HUB_ADDR(0), PI_RT_COUNT)
#endif /* !_STANDALONE */

typedef struct klcpu_err_s {
	klerr_info_t	ce_info;	/* information on previous error */
	cache_err_t	ce_cache_err_log;
	cache_err_t	ce_cache_err_dmp;	/* the error information */
	unchar		ce_log_valid;		/* error in log valid	 */
	unchar		ce_valid;		/* valid error		 */
	kf_cpu_t	ce_confidence_info;	/* confidence that the cpu is bad */
} klcpu_err_t;


/*
 * 	Hub PI errors.
 */

typedef struct hub_pierr_s {
	klerr_info_t	hp_err_info[2];	 /* information on logged error */
	hubreg_t	hp_err_int_pend; /* hub pi error_int_pend reg */
	klerr_info_t	hp_err_sts_info[2]; /* information on logged error */
	hubreg_t	hp_err_sts0[2];  /* hub pi error_status0_A/B reg */
	hubreg_t	hp_err_sts1[2];  /* hub pi error_status1_A/B reg */
	confidence_t	hp_confidence; 	/* confidence that pi is bad*/
	confidence_t	hp_sysbus_confidence;/* confidence that sysbus is bad */
} hub_pierr_t;

/*
 *	Hub MD errors
 */

typedef struct hub_mderr_s {
	klerr_info_t	hm_dir_err_info;  /* information on logged error */
	hubreg_t	hm_dir_err;	  /* hub md dir_error reg */
	klerr_info_t	hm_mem_err_info;  /* information on logged error */
	hubreg_t	hm_mem_err;	  /* hub md mem_error reg */
	klerr_info_t	hm_proto_err_info;  /* information on logged error */
	hubreg_t	hm_proto_err;	  /* hub md protocol_error reg */
	klerr_info_t	hm_misc_err_info;  /* information on logged error */
	hubreg_t	hm_misc_err; 	  /* hub md misc_error reg */
	confidence_t	hm_confidence;	  /* confidence that md is bad*/
	kf_mem_t	hm_mem_conf_info; /* confidence that mem is bad*/
} hub_mderr_t;

/*
 *	Hub IO errors.
 */

typedef struct hub_ioerr_s {
	klerr_info_t	hi_err_info;
	hubreg_t	hi_wstat;	/* hub io widget status */
	/*
	 * Do we need the PRB entry registers here?? 
	 */
	hubreg_t	hi_crb_entA[IIO_NUM_CRBS]; /* regular and partial*/
	hubreg_t	hi_bte0_sts;	/* Bte status */
	hubreg_t	hi_bte1_sts;
	hubreg_t	hi_bte0_src;	/* Source address. Could be useful */
	hubreg_t	hi_bte1_src;	/* when error points to BTE	*/
	hubreg_t	hi_bte0_dst;	/* destination address */
	hubreg_t	hi_bte1_dst;
	confidence_t	hi_confidence;	/* confidence that ii is bad */
} hub_ioerr_t;

/*
 *	Hub NI errors.
 */

typedef struct hub_nierr_s {
	klerr_info_t	hn_err_info;
	hubreg_t	hn_vec_sts;	/* hub ni vector status */
	hubreg_t	hn_port_err;	/* hub ni port errors register */
	confidence_t	hn_confidence;	/* confidence that ni is bad */
} hub_nierr_t;


/*
 *	All Hub errors.
 */


typedef struct hub_error_s {
	int		reset_flag;
	hub_pierr_t	hb_pi;		/* info on hub pi errors */	
	hub_mderr_t	hb_md;		/* info on hub md errors */
	hub_ioerr_t	hb_io;		/* info on hub io errors */
	hub_nierr_t	hb_ni;		/* info on hub ni errors */
} hub_error_t;


typedef struct klhub_err_s {
	hub_error_t	he_log;		/* error logged on an earlier error */
	hub_error_t	he_dmp;		/* dump information on a panic	*/
	confidence_t	he_hub_confidence; /* confindence that hub is bad */
	confidence_t	he_link_confidence;/* confidence that the node's link
					      to the midplane is bad */
	klconf_off_t	he_extension;	/* Extension to HUB error state */
}klhub_err_t;	

typedef struct klhub_err_ext_s {
	hubreg_t	hi_crb_entB[IIO_NUM_CRBS]; /* regular and partial*/
	hubreg_t	hi_crb_entC[IIO_NUM_CRBS]; /* regular and partial*/
	/* Add new stuff for the HUB here! */
} klhub_err_ext_t;

/*
 *	Bridge errors.
 */

typedef struct bridge_error_s {
	bridgereg_t	br_err_upper;	/* error upper address */
	bridgereg_t	br_err_lower;	/* error lower address */
	bridgereg_t	br_err_cmd;	/* error command word */
	bridgereg_t	br_aux_err;	/* aux error command word */
	bridgereg_t	br_resp_upper;	/* resp buffer error upper addr */
	bridgereg_t	br_resp_lower;	/* resp buffer error lower addr */
	bridgereg_t	br_ram_perr;	/* ssram parity error */
	bridgereg_t	br_pci_upper;	/* PCI error upper addr */
	bridgereg_t	br_pci_lower;	/* PCI error lower addr */
	bridgereg_t	br_int_status;
} bridge_error_t;

	
typedef struct klbridge_err_s {
	klerr_info_t	be_info;	/* information on previous error */
	bridge_error_t  be_log;		/* info logged on any error */
	bridge_error_t	be_dmp;		/* error dump info */
	confidence_t	be_confidence;	/* confidence that the bridge is bad */
	confidence_t	be_pci_dev[BRIDGE_DEV_CNT]; /* confidences that a pci 
						       device is bad */
        confidence_t    be_ssram_conf;  /* confidence that the bridge ssram is
					   bad */
	confidence_t	be_link_conf;	/* confidence that the board's link to
					the midplane is bad */
} klbridge_err_t;


/*
 *	XBOW errors
 */

typedef struct xbow_error_s {
	xbowreg_t	xb_status;	/* crossbow global status */
	xbowreg_t	xb_err_upper;	/* error upper address */
	xbowreg_t	xb_err_lower;	/* error lower address */
	xbowreg_t	xb_err_cmd;	/* error command word  */
	xbowreg_t	xb_link_status[MAX_XBOW_PORTS]; /* Link status  */
	xbowreg_t	xb_link_aux_status[MAX_XBOW_PORTS]; /* Link aux status  */
	                                        /* registers, port 8..15 */
} xbow_error_t;

typedef struct klxbow_err_s {
	klerr_info_t	xe_info;	/* information on previous error */
	xbow_error_t	xe_log;		/* info logged on any error */
	xbow_error_t	xe_dmp;		/* error dump info */
	confidence_t	xe_confidence;	/* confidence that the xbow is bad */
	confidence_t	xe_link_confidence[MAX_XBOW_PORTS]; /* confidences 
				     that an xbow link is bad, no longer used, 
				     use bridge be_link_conf instead*/
} klxbow_err_t;


/*
 *	Router errors
 */

typedef struct klrouter_err_s {
	klerr_info_t	re_info;	/* generic info for error      */
	hubreg_t	re_status_error[6];	/* 6 router links used */
	confidence_t	re_confidence;	/* confidence that the router is bad */
	confidence_t	re_link_confidence[MAX_ROUTER_PORTS]; /* confidence 
						that the router link is bad */
} klrouter_err_t;



/* 
 * The following come from ml/SN0/error.h. Note, if any error structure
 * increases in size (or changes), we will have to recompile the prom....
 */

typedef union klerrinfo_s {
	klcpu_err_t	ei_cpu_err;
	klhub_err_t	ei_hub_err;
	klbridge_err_t	ei_bridge_err;
	klxbow_err_t	ei_xbow_err;
	klrouter_err_t	ei_rou_err;
} klerrinfo_t ;

typedef struct klgeneric_error_s {
	char reserver[32];
} klgeneric_error_t;

typedef struct err_dump_param {
	void	(*dp_printf)();
	int	dp_prarg;	
	int	dp_flag;
} edump_param_t;


#define NODENAME 	"IP27"
#define MIDPLANE	"XBow"
#define IONAME		"IO Board"
#define ROUTERNAME	"Router"


#define KL_ERROR_DUMP	0x1
#define KL_ERROR_LOG	0x2
#define KL_ERROR_RESTORE 0x4	

#define CPU_COMP_ERROR(_brd, _cmp)  (klcpu_err_t *)KLCF_COMP_ERROR(_brd, _cmp)
#define HUB_COMP_ERROR(_brd,_cmp)   (klhub_err_t *)KLCF_COMP_ERROR(_brd, _cmp)
#define XBOW_COMP_ERROR(_brd,_cmp)  (klxbow_err_t *)KLCF_COMP_ERROR(_brd, _cmp)
#define ROU_COMP_ERROR(_brd, _cmp)  (klrouter_err_t *)KLCF_COMP_ERROR(_brd, _cmp)
#define BRI_COMP_ERROR(_brd, _cmp)  (klbridge_err_t *)KLCF_COMP_ERROR(_brd, _cmp)


#define CPU_ERROR_STRUCT(_cerr, _f) 		\
          (((_f) & KL_ERROR_DUMP) ? 		\
	   &((_cerr)->ce_cache_err_dmp) : &((_cerr)->ce_cache_err_log))

#define HUB_ERROR_STRUCT(_herr, _f) 		\
          (((_f) & KL_ERROR_DUMP) ? &((_herr)->he_dmp) : &((_herr)->he_log))

#define BRIDGE_ERROR_STRUCT(_berr, _f) 		\
          (((_f) & KL_ERROR_DUMP) ? &((_berr)->be_dmp) : &((_berr)->be_log))

#define XBOW_ERROR_STRUCT(_xerr, _f) 		\
          (((_f) & KL_ERROR_DUMP) ? &((_xerr)->xe_dmp) : &((_xerr)->xe_log))


#define ERROR_DISCARD_TIME 	90000000

int	memerror_handle_uce(__uint32_t, __uint64_t, eframe_t *, void *);

void	kl_dump_hw_state(void);
void	kl_log_hw_state(void);
void	kl_error_show(char *, char *,edump_param_t *);
void	kl_error_show_dump(char *, char *);
void	kl_error_show_log(char *, char *);
void	kl_error_show_idbg(char *, char *);
void	kl_error_show_reset_ste(char *, char *, void (*)());
int	kl_bitcount(__uint64_t);


void save_err_node_comp(nasid_t, lboard_t *, edump_param_t *);
void save_err_io_comp(nasid_t, lboard_t *, edump_param_t *);
void save_err_xbow_comp(nasid_t, lboard_t *, edump_param_t *);
void save_err_router_comp(nasid_t, lboard_t *, edump_param_t *);
void save_bridge_err(nasid_t, klbridge_err_t *, int, edump_param_t *);
void save_hub_err(nasid_t, klhub_err_t *, edump_param_t *);
void save_xbow_err(nasid_t , klxbow_err_t *, edump_param_t *);
void save_cache_error(unsigned int, __uint64_t, __uint64_t);

void save_hub_pi_error(__psunsigned_t, hub_pierr_t *);
void save_hub_md_error(__psunsigned_t, hub_mderr_t *);
void save_hub_io_error(__psunsigned_t, hub_ioerr_t *);
void save_hub_ni_error(__psunsigned_t, hub_nierr_t *);
void save_hub_err_ext(__psunsigned_t, klhub_err_ext_t *);

void show_cache_err(klcpu_err_t *, int, edump_param_t *);
void show_hub_err(klhub_err_t *, edump_param_t *, nasid_t);
void show_err_node_comp(lboard_t *, edump_param_t *);
void show_err_io_comp(lboard_t *, edump_param_t *);
void show_err_xbow_comp(lboard_t *, edump_param_t *);
void show_err_router_comp(lboard_t *, edump_param_t *);
void show_hub_pi_err(hub_pierr_t *);
void show_hub_io_err(klhub_err_t *, int, nasid_t);
void show_hub_md_err(hub_mderr_t *);
void show_hub_ni_err(hub_nierr_t *);
void show_bridge_err(klbridge_err_t *, edump_param_t *);
void show_xbow_err(klxbow_err_t *, edump_param_t *);

void errorste_reset(void);

/*
 * These routines can be used to block and unblock error interrupts
 * in the PI_ERR_INT_MASK register for the calling CPU.  Always specify 
 * the CPU A versions of the masks, such as PI_ERR_SPUR_MSG_A.  Masks can be 
 * OR'ed together.  If clear_pending is 1, the given error interrupts are 
 * cleared before being unmasked.  The calling CPU must have interrupts
 * disabled.  PI_ERR_MD_UNCORR is not allowed because it's not per-CPU.
 */
void err_int_block_mask(hubreg_t mask);
void err_int_unblock_mask(hubreg_t mask, int clear_pending);

#endif /* __SYS_SN_ERROR_H__ */

