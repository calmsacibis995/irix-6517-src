/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/********************************************************************************
 *										*
 * sys/tpudrv.h - Mesa TPU driver definitions.					*
 *										*
 ********************************************************************************/

#ifndef __SYS_TPUDRV_H__
#define __SYS_TPUDRV_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident  "$Id: tpudrv.h,v 1.3 1999/02/05 21:58:32 pww Exp $"

#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xwidget.h>
#include <sys/tpucom.h>
#include <sys/tpureg.h>

/*
 *	Map for xtalk (real or simulated) functions.
 */
typedef struct tpu_service_s {

	xtalk_piotrans_addr_f *		piotrans_addr;		/* xtalk_piotrans_addr() */
	xtalk_dmatrans_list_f *		dmatrans_list;		/* xtalk_dmatrans_list() */
	xtalk_intr_alloc_f *		intr_alloc;		/* xtalk_intr_alloc() */
	xtalk_intr_connect_f *		intr_connect;		/* xtalk_intr_connect() */
	xtalk_intr_disconnect_f *	intr_disconnect;	/* xtalk_intr_disconnect() */

} tpu_service_t;

/*
 *	Device state codes.
 */
typedef volatile enum {

	TPU_IDLE = 1,
	TPU_RUNNING,
	TPU_INTERRUPTED_B,
	TPU_INTERRUPTED_C,
	TPU_INTERRUPTED_L,
	TPU_HALTED,
	TPU_TIMEDOUT

} tpu_statecode_t;

/*
 *	Head of linked list of tpu soft structures.
 */
typedef struct tpu_list_s {

	struct tpu_soft_s *	head;			/* first tpu in list */
	int			count;			/* number of tpus in the list */
	mutex_t			mlock;			/* mutex lock for list head */

} tpu_list_t;

/*
 *	Per-device software state.
 */
typedef struct tpu_soft_s {

	uint_t			ls_magic;		/* magic number (TPU_M_SOFT) */
	uint_t			ls_size;		/* sizeof tpu_soft_t */

	char			ls_name[TPU_NAME_MAX];	/* device name */

	struct tpu_soft_s *	ls_alink;		/* chain of active tpu soft structures */

	vertex_hdl_t		ls_conn;		/* parent hwgraph vertex */
	vertex_hdl_t		ls_vhdl;		/* our tpu device vertex */
	vertex_hdl_t		ls_admin;		/* our admin device vertex */

	tpu_statecode_t		ls_state;		/* device state */
	int			ls_online;		/* device on-line flag */
	int			ls_exclusive;		/* exclusive open flag */
	int			ls_open;		/* open count */
	int			ls_assigned;		/* assigned flag */
	int			ls_status;		/* status code from interrupt handler */

	__uint64_t		ls_user_flag;		/* set by user with TPU_SET_FLAG ioctl */

	int			ls_module;		/* physical module number */
	int			ls_slot;		/* physical io slot number */

	const tpu_service_t *	ls_service;		/* service provider functions */

	ulong_t			ls_ldi_offset;		/* offset to start of ldi code */
	long			ls_timeout;		/* timeout period (ticks) */
	size_t			ls_page_size;		/* page size */
	int			ls_page_count;		/* number of pages */
	alenlist_t		ls_al;			/* alenlist for user buffer */

	widgetreg_t		ls_dmaErrorIntStatus;	/* widget error interrupt: x_int_status */
	tpu_reg_t		ls_dmaErrorD0Status;	/* widget error interrupt: DMA0 status */
	tpu_reg_t		ls_dmaErrorD1Status;	/* widget error interrupt: DMA1 status */

	widgetreg_t		ls_ldiBarrierIntStatus;	/* LDI barrier interrupt: x_int_status */
	tpu_reg_t		ls_ldiBarrierLdiStatus;	/* LDI barrier interrupt: LDI statusReg (reg 10) */
	tpu_reg_t		ls_ldiBarrierLdiSource;	/* LDI barrier interrupt: LDI iSource (reg 19) */

	widgetreg_t		ls_ldiErrorIntStatus;	/* LDI error interrupt: x_int_status */
	tpu_reg_t		ls_ldiErrorLdiStatus;	/* LDI error interrupt: LDI statusReg (reg 10) */

	widgetreg_t		ls_ldiCErrorIntStatus;	/* LDI channel error interrupt: x_int_status */
	tpu_reg_t		ls_ldiCErrorLdiStatus;	/* LDI channel error interrupt: LDI statusReg (reg 10) */
	tpu_reg_t		ls_ldiCErrorD0Status;	/* LDI channel error interrupt: DMA0 status */
	tpu_reg_t		ls_ldiCErrorD1Status;	/* LDI channel error interrupt: DMA1 status */

	widgetreg_t		ls_ldiIntStatus;	/* x_int_status */
	tpu_reg_t		ls_ldiLdiStatus;	/* LDI statusReg (reg 10) */
	tpu_reg_t		ls_ldiLdiSource;	/* LDI iSource (reg 19) */
	tpu_reg_t		ls_ldiD0Status;		/* DMA0 status */
	tpu_reg_t		ls_ldiD1Status;		/* DMA1 status */
	tpu_reg_t		ls_dma0Count;		/* Words output */
	tpu_reg_t		ls_dma0Ticks;		/* Output time (10ns ticks) */
	tpu_reg_t		ls_dma1Count;		/* Words input */
	tpu_reg_t		ls_dma1Ticks;		/* Input time (10ns ticks) */

	xtalk_intr_t		ls_dmaError_intr;	/* handler for widget error interrupts */
	xtalk_intr_t		ls_ldiBarrier_intr;	/* handler for LDI barrier interrupts */
	xtalk_intr_t		ls_ldiError_intr;	/* handler for LDI error interrupts */
	xtalk_intr_t		ls_ldiCError_intr;	/* handler for LDI channel error interrupts */

	tpu_xtk_regs_t *	ls_xregs;		/* xtalk widget registers */
	tpu_att_regs_t *	ls_aregs;		/* address translation registers */
	tpu_dma_regs_t *	ls_d0regs;		/* dma 0 registers */
	tpu_dma_regs_t *	ls_d1regs;		/* dma 1 registers */
	tpu_ldi_regs_t *	ls_lregs;		/* ldi registers */

	mutex_t			ls_mlock;		/* mutex lock for this device */
	sv_t			ls_sv;			/* synchronization variable for this device */

	toid_t			ls_timeout_id;		/* ID of active timeout */
	toid_t			ls_sim_id;		/* ID of interrupt simulator timeout */

	tpud_stats_t		ls_stat;		/* device statistics */

	tpud_fault_t		ls_fault;		/* fault insertion information */
	__int64_t		ls_fault_interval_r;	/* fault interval reload value */
	__int64_t		ls_fault_duration_r;	/* fault duration reload value */

	char 			ls_nic_string[1024];	/* NIC string from hardware */

	tpud_traceb_t *		ls_tbuffer;		/* trace buffer */

} tpu_soft_t;

/*
 * Get a pointer to the 'soft' structure for a tpu device given a hwgraph node.
 */
#define TPU_SOFT_SET(v,i)	hwgraph_fastinfo_set ((v), (arbitrary_info_t) (i))
#define TPU_SOFT_GET(v)		((tpu_soft_t *) hwgraph_fastinfo_get ((v)))

/*
 * External idbg functions.
 */
extern void		idbg_tpu_init (void);

/*
 * External tpu simulator functions.
 */
extern caddr_t		tpusim_info_chk (vertex_hdl_t);
extern caddr_t		tpusim_piotrans_addr (vertex_hdl_t, device_desc_t, iopaddr_t, size_t, unsigned);
extern alenlist_t	tpusim_dmatrans_list (vertex_hdl_t, device_desc_t, alenlist_t, unsigned);
extern xtalk_intr_t	tpusim_intr_alloc (vertex_hdl_t, device_desc_t, vertex_hdl_t);
extern int		tpusim_intr_connect (xtalk_intr_t, intr_func_t, intr_arg_t, xtalk_intr_setfunc_t, void *, void *);
extern void		tpusim_intr_disconnect (xtalk_intr_t);

#ifdef IP22

#define xwidget_driver_register(part_num, mfg_num, driver_prefix, flags) (0)
#define xwidget_driver_unregister(driver_prefix) 

#endif /* IP22 */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TPUDRV_H__ */
