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
 * File: hwperf.h
 *	hwperf specific defines/declarations for kernel use only.
 *	Things from hwperfmacros/hwperftypes.h will ultimately move 
 *	into this file.
 */

#ifndef __SYS_HWPERF_H__
#define __SYS_HWPERF_H__

#if defined (SN0)

#include <sys/hwperftypes.h>

typedef struct md_perf_monitor {
	short		mpm_plex;
	short		mpm_flags;
	short 		mpm_current;
	mutex_t 	mpm_mutex;
	cnodeid_t 	mpm_cnode;
	nasid_t 	mpm_nasid;
	int		mpm_gen;
	md_perf_values_t mpm_vals;
	md_perf_control_t mpm_ctrl;
} md_perf_monitor_t;

extern int  md_perf_enable(md_perf_control_t *ctrl, int *rvalp);
extern int  md_perf_disable(void);
extern int  md_perf_get_count(md_perf_values_t *val, int *rvalp);
extern int  md_perf_get_ctrl(md_perf_control_t *ctrl, int *rvalp);
extern void md_perf_init(void);
extern int  md_perf_node_enable(md_perf_control_t *ctrl, cnodeid_t cnode, int *rvalp);
extern int  md_perf_node_disable(cnodeid_t cnode);
extern int  md_perf_get_node_count(cnodeid_t cnode, md_perf_values_t *val, int *rvalp);
extern int  md_perf_get_node_ctrl(cnodeid_t cnode, md_perf_control_t *ctrl, int *rvalp);
extern void md_perf_start_count(md_perf_monitor_t *mdp_mon);
extern void md_perf_stop_count(md_perf_monitor_t *mdp_mon);
extern void md_perf_update_count(md_perf_monitor_t *mdp_mon);
extern void md_perf_multiplex(cnodeid_t cnode);

typedef struct io_perf_monitor {
	short	iopm_plex;
	short	iopm_flags;
	short 	iopm_current;
	mutex_t iopm_mutex;
	cnodeid_t iopm_cnode;
	nasid_t iopm_nasid;
	int	iopm_gen;
	io_perf_values_t  iopm_vals;
	io_perf_control_t iopm_ctrl;
} io_perf_monitor_t;

extern void io_perf_init(void);
extern void io_perf_init_lfsr(void);

extern void io_perf_start_count(io_perf_monitor_t *iop_mon);
extern void io_perf_stop_count(io_perf_monitor_t *iop_mon);
extern void io_perf_multiplex(cnodeid_t cnode);
extern int io_perf_update_count(io_perf_monitor_t *iop_mon);
extern int io_perf_turnon_node(io_perf_control_t *ctrl, cnodeid_t cnode, int flag);
extern int io_perf_turnoff_node(cnodeid_t cnode, int mode);

extern int io_perf_enable(io_perf_control_t *ctrl, int *rvalp);
extern int io_perf_disable(void);
extern int io_perf_get_count(io_perf_values_t *val, int *rvalp);
extern int io_perf_get_ctrl(io_perf_control_t *ctrl, int *rvalp);

extern int io_perf_node_enable(io_perf_control_t *ctrl, cnodeid_t cnode, int *rvalp);
extern int io_perf_node_disable(cnodeid_t cnode);
extern int io_perf_get_node_count(cnodeid_t cnode, io_perf_values_t *val, int *rvalp);
extern int io_perf_get_node_ctrl(cnodeid_t cnode, io_perf_control_t *ctrl, int *rvalp);


#endif /* SN0 */

#endif /* __SYS_HWPERF_H__ */
