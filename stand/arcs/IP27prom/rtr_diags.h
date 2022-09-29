/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.7 $"

#ifndef	_RTRDIAGS_H_
#define	_RTRDIAGS_H_


/*
 * Definitions for some threshold values for llp link errors
 */

#define LOW_CB_ERR_THRESHOLD    1
#define LOW_SN_ERR_THRESHOLD    1
#define MED_CB_ERR_THRESHOLD    5
#define MED_SN_ERR_THRESHOLD    50
#define HI_CB_ERR_THRESHOLD     100
#define HI_SN_ERR_THRESHOLD     1000

/*
 * Definitions for some data patterns.
 */

#define PATTERNF                0xFFFFFFFFFFFFFFFF

int	vector_status(int status);	
int	hub_local_link_diag(int diag_mode);
int 	hub_send_data_error_test(int diag_mode);
int 	rtr_send_data_error_test(int diag_mode);
int 	hub_link_vector_diag(int diag_mode,
                         pcfg_t *p,
                         int from_index,        /* Index on source chip */
                         int from_port,         /* Valid if from router */
                         int index,             /* Index on new chip */
                         net_vec_t from_path,   /* Path to source chip */
                         net_vec_t path);
int test_hub_error_detection(int diag_mode,
                         pcfg_t *p,
                         int from_index,        /* Index on source chip */
                         int from_port,         /* Valid if from router */
                         int index,             /* Index on new chip */
                         net_vec_t from_path,   /* Path to source chip */
                         net_vec_t path);       /* Path to new chip */
int rtr_link_vector_diag(int diag_mode,
                         pcfg_t *p,
                         int from_index,        /* Index on source chip */
                         int from_port,         /* Valid if from router */
                         int index,             /* Index on new chip */
                         net_vec_t from_path,   /* Path to source chip */
                         net_vec_t path);       /* Path to new chip */
int rtr_error_detection_diag(int diag_mode,
                         pcfg_t *p,
                         int from_index,        /* Index on source chip */
                         int from_port,         /* Valid if from router */
                         int index,             /* Index on new chip */
                         net_vec_t from_path,   /* Path to source chip */
                         net_vec_t path);       /* Path to new chip */

#endif /* _RTRDIAGS_H_ */
