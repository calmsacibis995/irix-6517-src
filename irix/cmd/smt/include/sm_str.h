#ifndef SM_STR_H
#define SM_STR_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

extern char *sm_cond_str(u_short);
extern char *sm_nodeclass_str(int);
extern char *sm_conf_policy_str(u_long, char*);
extern char *sm_conn_policy_str(u_long, char*);
extern char *sm_phy_connp_str(u_long, char*);
extern char *sm_topology_str(u_long, char*);
extern char *sm_pathtype_str(u_long, char*);

extern char *sm_hold_state_str(HOLD_STATE);
extern char *sm_ecm_state_str(ECM_STATE);
extern char *sm_cfm_state_str(CFM_STATE);
extern char *sm_dupa_test_str(DUPA_TEST);
extern char *sm_flag_str(u_long);
extern char *sm_nn_state_str(u_long);
extern char *sm_bridge_str(u_long);
extern char *sm_rmtstate_str(RMT_STATE);
extern char *sm_rmt_flag_str(u_long, char *);
extern char *sm_msloop_str(u_long);
extern char *sm_ce_state_str(CE_STATE);
extern char *sm_conn_state_str(PHY_CONN_STATE);
extern char *sm_withhold_str(PHY_WITHHOLD);
extern char *sm_fotx_str(PMD_FOTX);
extern char *sm_ptype_str(u_long);

#endif
