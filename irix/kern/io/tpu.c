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
 * tpu.c - Tensor Processing Unit driver					*
 *										*
 * The following parameters may be set by DRIVER_ADMIN lines in the file	*
 * /var/sysgen/system/irix.sm:							*
 *										*
 * Crosstalk Xbar credit count (default = 4):					*
 *										*
 *	DRIVER_ADMIN: tpu_ XBAR_CREDITS=n					*
 *										*
 ********************************************************************************/

#ident	"$Id: tpu.c,v 1.4 1999/04/29 19:32:28 pww Exp $"

#include <sys/types.h>

#include <ksys/ddmap.h>
#if SN0
#include <sys/SN/addrs.h>
#endif
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/driver.h>
#include <sys/edt.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <sys/iobus.h>
#include <sys/ioctl.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/ksynch.h>
#include <sys/nic.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pio.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/tpudrv.h>
#include <sys/tpuerrno.h>

/* ==============================================================================
 *	MACROS
 */

/*
 * Constants.
 */
#define TPU_TIMEOUT_TICKS	(32*HZ)		/* default timeout period */
#define	TPU_SIM_TICKS		(2*HZ)		/* simulated interrupt delay */

/*
 * Object creation/deletion.
 */
#define	NEW(ptr)		(ptr = kmem_zalloc (sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)		(kmem_free (ptr, sizeof (*(ptr))))

/*
 * Short-term locking of a tpu soft structure.
 */
#define	TPU_LOCK_INIT(s)	MUTEX_INIT (&(s)->ls_mlock, MUTEX_DEFAULT, "tpu_mutex")
#define	TPU_LOCK(s)		MUTEX_LOCK (&(s)->ls_mlock, -1)
#define TPU_UNLOCK(s)		MUTEX_UNLOCK (&(s)->ls_mlock)

/*
 * Sleep/wakeup for a tpu soft structure.  The short-term TPU_LOCK() must be
 * held before calling TPU_SLEEP() or TPU_WAKEUP().  In the case of TPU_SLEEP(),
 * the short-term lock will be released after the process is put to sleep, thus
 * preventing any race conditions.
 */
#define	TPU_SLEEP_INIT(s)	SV_INIT (&(s)->ls_sv, SV_FIFO, "tpu_sv");
#define	TPU_SLEEP(s)		SV_WAIT_SIG (&(s)->ls_sv, &(s)->ls_mlock, 0);
#define TPU_WAKEUP(s)		SV_SIGNAL (&(s)->ls_sv);

/*
 * Select the correct xtalk provider function for a particular device.
 */
#define XTALK_FUNC(s, fcn)	(s)->ls_service->fcn

/*
 * Event counting.
 */
#define	TPU_EVENT(c)		TPU_EVENT_INC ((c), 1)
#define	TPU_EVENT_INC(c,i)	(c)->count += (i) ; (c)->timestamp = absolute_rtc_current_time ()

/*
 * Tracing.
 */
#define	TPU_TRACE_0(b, s)			\
	if ((b) != NULL)			\
		(void) tpu_trace ((b), (s));	\

#define	TPU_TRACE_1(b, s, p0)			\
	if ((b) != NULL) {			\
		__uint64_t *_p;			\
		_p = tpu_trace ((b), (s));	\
		_p[0] = (__uint64_t) (p0);	\
	}

#define	TPU_TRACE_2(b, s, p0, p1)		\
	if ((b) != NULL) {			\
		__uint64_t *_p;			\
		_p = tpu_trace ((b), (s));	\
		_p[0] = (__uint64_t) (p0);	\
		_p[1] = (__uint64_t) (p1);	\
	}

#define	TPU_TRACE_3(b, s, p0, p1, p2)		\
	if ((b) != NULL) {			\
		__uint64_t *_p;			\
		_p = tpu_trace ((b), (s));	\
		_p[0] = (__uint64_t) (p0);	\
		_p[1] = (__uint64_t) (p1);	\
		_p[2] = (__uint64_t) (p2);	\
	}

#define	TPU_TRACE_4(b, s, p0, p1, p2, p3)	\
	if ((b) != NULL) {			\
		__uint64_t *_p;			\
		_p = tpu_trace ((b), (s));	\
		_p[0] = (__uint64_t) (p0);	\
		_p[1] = (__uint64_t) (p1);	\
		_p[2] = (__uint64_t) (p2);	\
		_p[3] = (__uint64_t) (p3);	\
	}

#define	TPU_TRACE_5(b, s, p0, p1, p2, p3, p4)	\
	if ((b) != NULL) {			\
		__uint64_t *_p;			\
		_p = tpu_trace ((b), (s));	\
		_p[0] = (__uint64_t) (p0);	\
		_p[1] = (__uint64_t) (p1);	\
		_p[2] = (__uint64_t) (p2);	\
		_p[3] = (__uint64_t) (p3);	\
		_p[4] = (__uint64_t) (p4);	\
	}

#define	TPU_TRACE_G0(s)				TPU_TRACE_0(tpu_global_trace_buffer, (s))
#define	TPU_TRACE_G1(s, p0)			TPU_TRACE_1(tpu_global_trace_buffer, (s), (p0))
#define	TPU_TRACE_G2(s, p0, p1)			TPU_TRACE_2(tpu_global_trace_buffer, (s), (p0), (p1))
#define	TPU_TRACE_G3(s, p0, p1, p2)		TPU_TRACE_3(tpu_global_trace_buffer, (s), (p0), (p1), (p2))
#define	TPU_TRACE_G4(s, p0, p1, p2, p3)		TPU_TRACE_4(tpu_global_trace_buffer, (s), (p0), (p1), (p2), (p3))
#define	TPU_TRACE_G5(s, p0, p1, p2, p3, p4)	TPU_TRACE_5(tpu_global_trace_buffer, (s), (p0), (p1), (p2), (p3), (p4))

#define	TPU_TRACE_D0(s)				TPU_TRACE_0(soft->ls_tbuffer, (s))
#define	TPU_TRACE_D1(s, p0)			TPU_TRACE_1(soft->ls_tbuffer, (s), (p0))
#define	TPU_TRACE_D2(s, p0, p1)			TPU_TRACE_2(soft->ls_tbuffer, (s), (p0), (p1))
#define	TPU_TRACE_D3(s, p0, p1, p2)		TPU_TRACE_3(soft->ls_tbuffer, (s), (p0), (p1), (p2))
#define	TPU_TRACE_D4(s, p0, p1, p2, p3)		TPU_TRACE_4(soft->ls_tbuffer, (s), (p0), (p1), (p2), (p3))
#define	TPU_TRACE_D5(s, p0, p1, p2, p3, p4)	TPU_TRACE_5(soft->ls_tbuffer, (s), (p0), (p1), (p2), (p3), (p4))


/* ==============================================================================
 *	FUNCTION TABLE OF CONTENTS
 */

extern const char *	tpu_version (void);

extern void		tpu_init (void);
extern int		tpu_reg (void);
extern int		tpu_unreg (void);
extern int		tpu_attach (vertex_hdl_t);

extern int		tpu_open (dev_t *, int, int, cred_t *);
extern int		tpu_close (dev_t, int, int, cred_t *);

extern int		tpu_map (dev_t, vhandl_t *, off_t, size_t, uint_t);
extern int		tpu_unmap (dev_t, vhandl_t *);

extern int		tpu_ioctl (dev_t, int, void *, int, cred_t *, int *);

static int		tpu_ioctl_run (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_resume (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_halt (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_regs (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_inst (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_stat (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_stat_list (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_stats (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_gstats (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_config (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_config_down (tpu_soft_t *);
static int		tpu_ioctl_config_up (tpu_soft_t *);
static int		tpu_ioctl_set_flag (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_get_flag (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_set_fault (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_get_fault (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_get_soft (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_get_trace (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_get_gtrace (tpu_soft_t *, void *, int, cred_t *, int *);
static int		tpu_ioctl_ext_test (tpu_soft_t *, void *, int, cred_t *, int *);

static tpu_soft_t *	tpu_create_device (vertex_hdl_t);
static void		tpu_intr_connect (tpu_soft_t *);
static void		tpu_intr_disconnect (tpu_soft_t *);
static void		tpu_conv_add (tpu_soft_t *);
static void		tpu_inventory_add (tpu_soft_t *);

static int		tpu_any (vertex_hdl_t);
static int		tpu_admin (vertex_hdl_t);
static int		tpu_simulated (tpu_soft_t *);
static tpu_soft_t *	tpu_assign (vertex_hdl_t);
static void		tpu_unassign (vertex_hdl_t, tpu_soft_t *);
static int		tpu_pagecode (int);
static void		tpu_list_init (tpu_list_t *);
static void		tpu_alist_add (tpu_soft_t *);
static int		tpu_pin (tpud_run_t *, int *);
static void		tpu_unpin (tpud_run_t *, int *);
static void		tpu_create_alenlist (tpu_soft_t *, tpud_run_t *);
static void		tpu_destroy_alenlist (tpu_soft_t *);
static void		tpu_stat (tpu_soft_t *, tpud_stat_t *);
static char *		lfu (__uint64_t, int, int, char *, char);

static int		tpu_fault (tpu_soft_t *, tpud_fault_type_t);
static int		tpu_fault_status (tpu_soft_t *);
static void		tpu_fault_regs (tpu_soft_t *, tpud_run_t *);

static void		tpu_reset (tpu_soft_t *);
static void		tpu_wid_init (tpu_soft_t *);
static void		tpu_xtk_init (tpu_soft_t *);
static void		tpu_dma_init (tpu_soft_t *);
static void		tpu_dma_preset (tpu_soft_t *);
static void		tpu_att_init (tpu_soft_t *);
static void		tpu_att_inval (tpu_soft_t *, int);
static int		tpu_att_load (tpu_soft_t *);
static void		tpu_ldi_init (tpu_soft_t *);
static void		tpu_boot (tpu_soft_t *);
static void		tpu_dma_leds (tpu_soft_t *, widgetreg_t);
static void		tpu_get_regs (tpu_soft_t *, tpud_regs_t *);
static void		tpu_get_inst (tpu_soft_t *, tpud_inst_t *);

static void             tpu_timeout (intr_arg_t arg);

static void             tpu_dmaError_intr (intr_arg_t arg);
static void             tpu_ldiBarrier_intr (intr_arg_t arg);
static void             tpu_ldiError_intr (intr_arg_t arg);
static void             tpu_ldiCError_intr (intr_arg_t arg);

static int              tpu_dmaError_setfunc (xtalk_intr_t intr);
static int              tpu_ldiBarrier_setfunc (xtalk_intr_t intr);
static int              tpu_ldiError_setfunc (xtalk_intr_t intr);
static int              tpu_ldiCError_setfunc (xtalk_intr_t intr);

static void		tpu_trace_init (void);
static tpud_traceb_t *	tpu_trace_alloc (void);
static  __uint64_t *	tpu_trace (tpud_traceb_t *, char *);
static int		tpu_trace_copyout (tpud_traceb_t *, tpud_tracex_t *);


/* ==============================================================================
 *	GLOBALS
 */

/*
 * Indicate that this driver is multithreaded.
 */
int			tpu_devflag = D_MP;

/*
 * Head of TPU soft structure list.
 */
tpu_list_t		tpu_active;

/*
 * Vertex handle for /hw/tpu.
 */
vertex_hdl_t		tpu_root;

/*
 * Vertex handle for /hw/tpu/any/tpu.
 */
vertex_hdl_t		tpu_any_tpu;

/*
 * Vertex handle for /hw/tpu/any/admin.
 */
vertex_hdl_t		tpu_any_admin;

/*
 * Global trace buffer for entries not associated with a specific device.
 */
tpud_traceb_t *		tpu_global_trace_buffer = NULL;

/*
 * Global driver statistics.
 */
tpud_gstats_t		tpu_gstats;

/*
 * Crossbar credit count.  The hardware default value is 3.  A value of 4 should
 * improve DMA performance and has been shown to work for properly coded chains
 * The default value specified here may be overridden by a "DRIVER_ADMIN:
 * XBAR_CREDTS=" directive in irix.sm.
 */
static u_int		xbar_credits = 4;


/* ==============================================================================
 *	XTALK SERVICE PROVIDER MAPS
 */

const tpu_service_t	tpu_map_xtalk = 
			{
			xtalk_piotrans_addr,
			xtalk_dmatrans_list,
			xtalk_intr_alloc,
			xtalk_intr_connect,
			xtalk_intr_disconnect
			};

const tpu_service_t	tpu_map_sim = 
			{
			tpusim_piotrans_addr,
			tpusim_dmatrans_list,
			tpusim_intr_alloc,
			tpusim_intr_connect,
			tpusim_intr_disconnect
			};


/* ==============================================================================
 *	VERSION FUNCTION
 */

const char *
tpu_version (void)
{
	return ("$Revision: 1.4 $ Built: " __DATE__ " " __TIME__);
}


/* ==============================================================================
 *	DRIVER INITIALIZATION
 */

/********************************************************************************
 *										*
 *	tpu_init: called once during system startup or when a loadable		*
 *	driver is loaded.							*
 *										*
 ********************************************************************************/
void
tpu_init (void)
{
	int		status;
	vertex_hdl_t	tpu_any_vertex;


	/*
	 * Start tracing.
	 */
	tpu_trace_init ();

	/*------------------------------*/
	TPU_TRACE_G0 ("tpu_init - entry");
	/*------------------------------*/

	/*
	 * Initialize the active tpu list.
	 */
	tpu_list_init (&tpu_active);

	/*
	 * Initialize the idbg functions.
	 */
	idbg_tpu_init ();

	/*
	 * Create the base /hw/tpu vertex.
	 */
	if ((status = hwgraph_vertex_create (&tpu_root)) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_vertex_create failed for /hw/tpu: %d", status);
		return;
	}

	if ((status = hwgraph_edge_add (hwgraph_root, tpu_root, "tpu")) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_edge_add failed for /hw/tpu: %d", status);
		return;
	}

	/*
	 * Create the /hw/tpu/any vertex and special devices.
	 */
	if ((status = hwgraph_vertex_create (&tpu_any_vertex)) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_vertex_create failed for /hw/tpu/any: %d", status);
		return;
	}

	if ((status = hwgraph_edge_add (tpu_root, tpu_any_vertex, "any")) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_edge_add failed for /hw/tpu/any: %d", status);
		return;
	}

	if ((status = hwgraph_char_device_add (tpu_any_vertex, "tpu", "tpu_", &tpu_any_tpu)) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_char_device_add failed for /hw/tpu/any/tpu: %d", status);
		return;
	}

	if ((status = hwgraph_char_device_add (tpu_any_vertex, "admin", "tpu_", &tpu_any_admin)) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_char_device_add failed for /hw/tpu/any/admin: %d", status);
		return;
	}

	hwgraph_chmod (tpu_any_tpu, 0666);
	hwgraph_chmod (tpu_any_admin, 0666);

	/*-----------------------------*/
	TPU_TRACE_G0 ("tpu_init - exit");
	/*-----------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_reg: called once during system startup or when a loadable driver	*
 *	is loaded.								*
 *										*
 *	NOTE: a bus provider register routine should always be called from	*
 *	_reg, rather than from _init. In the case of a loadable module, the	*
 *	devsw is not hooked up when the _init routines are called.		*
 *										*
 ********************************************************************************/
int
tpu_reg (void)
{
	int		status;
	char *		param_s;

	/*
	 * Process any DRIVER_ADMIN directives from irix.sm.
	 */
	if ((param_s = device_driver_admin_info_get ("tpu_", "XBAR_CREDITS")) != NULL) 
		xbar_credits = atoi (param_s);

	/*
	 * Register with the system.
	 */
	status = xwidget_driver_register (TPU_WIDGET_PART_NUM, TPU_WIDGET_MFGR_NUM, "tpu_", 0);

	/*------------------------------------------*/
	TPU_TRACE_G1 ("tpu_reg - status: %d", status);
	/*------------------------------------------*/

  	return 0;
}

/********************************************************************************
 *										*
 *	tpu_unreg: called when a loadable driver is unloaded.			*
 *										*
 ********************************************************************************/
int
tpu_unreg (void)
{
	xwidget_driver_unregister ("tpu_");

	/*-----------------------*/
	TPU_TRACE_G0 ("tpu_unreg");
	/*-----------------------*/

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_attach: called by the xtalk infrastructure once for each vertex	*
 *	representing a crosstalk widget.					*
 *										*
 *	In large configurations, it is possible for a huge number of CPUs to	*
 *	enter this routine all at nearly the same time, for different specific	*
 *	instances of the device.						*
 *										*
 ********************************************************************************/
int
tpu_attach 
	(
	vertex_hdl_t		conn		/* parent xtalk vertex */
	)
{
	tpu_soft_t *		soft;		/* our new device table */

	/*
	 * Create the new device and initialize our internal data structures.
	 */
	soft = tpu_create_device (conn);

	/*
	 * Create a convienence edge for this device and link it to the /hw/tpu vertex.
	 */
	tpu_conv_add (soft);

	/*
	 * Update the harware inventory for this device.
	 */
	tpu_inventory_add (soft);

	/*
	 * Initialize the TPU hardware.
	 */
	tpu_reset (soft);
	tpu_xtk_init (soft);
	tpu_dma_init (soft);
	tpu_att_init (soft);
	tpu_ldi_init (soft);

	/*
	 * Connect the interrupt handlers.
	 */
	tpu_intr_connect (soft);

	/*
	 * Mark the device online.
	 */
	soft->ls_online = 1;

	return 0;
}


/* ==============================================================================
 *	DRIVER OPEN/CLOSE
 */

/********************************************************************************
 *										*
 *	tpu_open: called when a tpu device special file is opened.		*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpu_open 
	(
	dev_t *		devp, 
	int		oflag, 
	int		otyp, 
	cred_t *	crp
	)
{
	vertex_hdl_t	vhdl = dev_to_vhdl (*devp);
	int		exclusive_req = ((oflag & FEXCL) != 0);
	tpu_soft_t *	soft;
	int		status;

	if (tpu_any (vhdl) || tpu_admin (vhdl))
		return 0;

	soft = TPU_SOFT_GET (vhdl);

	/*------------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_open - entry, vhdl: %d, flags: 0x%x", vhdl, oflag);
	/*------------------------------------------------------------------*/

	TPU_EVENT (&soft->ls_stat.open);

	TPU_LOCK (soft);

	if (soft->ls_exclusive) {
		/*--------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_open - already open exclusive, open count: %d",
				soft->ls_open);
		/*--------------------------------------------------------------*/
		status = EBUSY;
	}
	else if (exclusive_req && soft->ls_open) {
		/*----------------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_open - exclusive requested but already open, open count: %d", 
				soft->ls_open);
		/*----------------------------------------------------------------------------*/
		status = EBUSY;
	}
	else if (exclusive_req && soft->ls_assigned) {
		/*------------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_open - exclusive requested but assigned, open count: %d", 
				soft->ls_open);
		/*------------------------------------------------------------------------*/
		status = EAGAIN;
	}
	else {
		soft->ls_exclusive = exclusive_req;
		soft->ls_open++;
		status = 0;
	}

	TPU_UNLOCK (soft);

	/*-------------------------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_open - exit, errno: %d, open count: %d", status, soft->ls_open);
	/*-------------------------------------------------------------------------------*/

	return status;
}

/********************************************************************************
 *										*
 *	tpu_close: called when a tpu device special file is closed by a		*
 *	process and no other processes still have it open ("last close").	*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpu_close 
	(
	dev_t		dev, 
	int		oflag, 
	int		otyp, 
	cred_t *	crp
	)
{
	vertex_hdl_t	vhdl = dev_to_vhdl (dev);
	tpu_soft_t *	soft;

	if (tpu_any (vhdl) || tpu_admin (vhdl))
		return 0;

	soft = TPU_SOFT_GET (vhdl);

	/*----------------------------------------*/
	TPU_TRACE_D1 ("tpu_close - vhdl: %d", vhdl);
	/*----------------------------------------*/

	TPU_EVENT (&soft->ls_stat.close);

	TPU_LOCK (soft);

	soft->ls_open = 0;
	soft->ls_exclusive = 0;

	TPU_UNLOCK (soft);

	return 0;
}


/* ==============================================================================
 *	MEMORY MAP ENTRY POINTS
 */

/********************************************************************************
 *										*
 *	tpu_map: called for a mmap(2) on a tpu device special file.		*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpu_map 
	(
	dev_t		dev, 
	vhandl_t *	vt, 
	off_t		off, 
	size_t		len, 
	uint		prot
	)
{
	vertex_hdl_t	vhdl = dev_to_vhdl (dev);
	tpu_soft_t *	soft;
	void *		kaddr;
	int		uaddr;

	if (tpu_any (vhdl) || !cap_able (CAP_DEVICE_MGT))
		return EPERM;

	soft = TPU_SOFT_GET (vhdl);

	/*------------------------------------------------------------------------*/
	TPU_TRACE_D3 ("tpu_map - off: 0x%x, len: 0x%x, prot: 0x%x", off, len, prot);
	/*------------------------------------------------------------------------*/

	TPU_EVENT (&soft->ls_stat.mmap);

	/*
	 * Round len up to a page boundary.
	 */
	len = ptob (btopr (len));

	/*
	 * Get the requested physical address.
	 */
	if ((kaddr = (void *) XTALK_FUNC(soft, piotrans_addr) (soft->ls_conn, 0, off, len, 0)) == NULL) {
		/*--------------------------------------------*/
		TPU_TRACE_D0 ("tpu_map - piotrans_addr failed");
		/*--------------------------------------------*/
		return EINVAL;
	}

	/*
	 * Map the kernal address range into the user's address space.
	 */
	uaddr = v_mapphys (vt, kaddr, len);

	/*------------------------------------------*/
	TPU_TRACE_D1 ("tpu_map - uaddr: 0x%x", uaddr);
	/*------------------------------------------*/

	return uaddr;
}

/********************************************************************************
 *										*
 *	tpu_unmap: called for a munmap(2) on a tpu device special file.		*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpu_unmap 
	(
	dev_t		dev, 
	vhandl_t *	vt
	)
{
	vertex_hdl_t	vhdl = dev_to_vhdl (dev);
	tpu_soft_t *	soft;

	if (tpu_any (vhdl) || !cap_able (CAP_DEVICE_MGT))
		return EPERM;

	soft = TPU_SOFT_GET (vhdl);

	/*-----------------------*/
	TPU_TRACE_D0 ("tpu_unmap");
	/*-----------------------*/

	TPU_EVENT (&soft->ls_stat.munmap);

	return 0;
}


/* ==============================================================================
 *	CONTROL ENTRY POINT
 */

/********************************************************************************
 *										*
 *	tpu_ioctl: a user has made an ioctl request for an open tpu		*
 *	device.									*
 *										*
 *	cmd and arg are as specified by the user; arg is a pointer to		*
 *	something in the user's address space, so we need to use copyin() to	*
 *	read through it and copyout() to write through it.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpu_ioctl 
	(
	dev_t		dev, 
	int		cmd, 
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	vertex_hdl_t	vhdl = dev_to_vhdl (dev);
	tpu_soft_t *	soft;
	int		status;

	/*
	 * Check for commands that aren't allowed on admin devices.
	 */
	switch (cmd) {

	case TPU_RUN:
	case TPU_RESUME:
		if (tpu_admin (vhdl)) 
			return EINVAL;
	}

	/*
	 * Check for commands that aren't allowed for dynamically assigned devices.
	 */
	switch (cmd) {

	case TPU_RESUME:
	case TPU_HALT:
	case TPU_CONFIG:
	case TPU_SET_FLAG:
	case TPU_SET_FAULT:
	case TPU_EXT_TEST:
		if (tpu_any (vhdl)) 
			return EPERM;
	}

	/*
	 * Check for commands that require privelege.
	 */
	switch (cmd) {

	case TPU_CONFIG:
	case TPU_SET_FLAG:
	case TPU_SET_FAULT:
	case TPU_EXT_TEST:
		if (!cap_able (CAP_DEVICE_MGT))
			return EPERM;
	}

	/*
	 * Assign a device for this request.
	 */
	if ((soft = tpu_assign (vhdl)) == NULL)
		return EAGAIN;

	/*
	 * Process the request.
	 */
	switch (cmd) {

	case TPU_RUN:
		status = tpu_ioctl_run (soft, arg, mode, crp, rvalp);
		break;

	case TPU_RESUME:
		status = tpu_ioctl_resume (soft, arg, mode, crp, rvalp);
		break;

	case TPU_HALT:
		status = tpu_ioctl_halt (soft, arg, mode, crp, rvalp);
		break;

	case TPU_REGS:
		status = tpu_ioctl_regs (soft, arg, mode, crp, rvalp);
		break;

	case TPU_INST:
		status = tpu_ioctl_inst (soft, arg, mode, crp, rvalp);
		break;

	case TPU_STAT:
		status = tpu_ioctl_stat (soft, arg, mode, crp, rvalp);
		break;

	case TPU_STAT_LIST:
		status = tpu_ioctl_stat_list (soft, arg, mode, crp, rvalp);
		break;

	case TPU_STATS:
		status = tpu_ioctl_stats (soft, arg, mode, crp, rvalp);
		break;

	case TPU_GSTATS:
		status = tpu_ioctl_gstats (soft, arg, mode, crp, rvalp);
		break;

	case TPU_CONFIG:
		status = tpu_ioctl_config (soft, arg, mode, crp, rvalp);
		break;

	case TPU_SET_FLAG:
		status = tpu_ioctl_set_flag (soft, arg, mode, crp, rvalp);
		break;

	case TPU_GET_FLAG:
		status = tpu_ioctl_get_flag (soft, arg, mode, crp, rvalp);
		break;

	case TPU_SET_FAULT:
		status = tpu_ioctl_set_fault (soft, arg, mode, crp, rvalp);
		break;

	case TPU_GET_FAULT:
		status = tpu_ioctl_get_fault (soft, arg, mode, crp, rvalp);
		break;

	case TPU_GET_SOFT:
		status = tpu_ioctl_get_soft (soft, arg, mode, crp, rvalp);
		break;

	case TPU_GET_TRACE:
		status = tpu_ioctl_get_trace (soft, arg, mode, crp, rvalp);
		break;

	case TPU_GET_GTRACE:
		status = tpu_ioctl_get_gtrace (soft, arg, mode, crp, rvalp);
		break;

	case TPU_EXT_TEST:
		status = tpu_ioctl_ext_test (soft, arg, mode, crp, rvalp);
		break;

	default:
		/*--------------------------------------------------------------------*/
		TPU_TRACE_D2 ("tpu_ioctl - vhdl: %d, invalid command: 0x%x", vhdl, cmd);
		/*--------------------------------------------------------------------*/
		status = EINVAL;
		break;
	}

	/*
	 * Release the device assignment.
	 */
	tpu_unassign (vhdl, soft);

	return status;
}


/********************************************************************************
 *										*
 *	tpu_ioctl_run: Execute a chain.						*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_run
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	int		errno;
	int		sig_status;
	int		status = TPU_OK;
	__uint64_t	sysStartClock = absolute_rtc_current_time ();
	__uint64_t	tpuStartClock;
	__uint64_t	tpuEndClock;
	tpud_run_t	tpu_run;
	int		userdma_cookie[TPU_PAGES_MAX];

	/*-----------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_run - entry");
	/*-----------------------------------*/

	TPU_EVENT (&soft->ls_stat.ioctlRun);

	/*
	 * Get a copy of the user's tpud_run_t argument structure.
	 */
	if (copyin (arg, (caddr_t) &tpu_run, sizeof (tpu_run))) 
		return EFAULT;

	/*
	 * Sanity check the argument values.
	 */
	if ((tpu_run.page_count < 1) || (tpu_run.page_count > TPU_PAGES_MAX)) {
		#pragma mips_frequency_hint NEVER
		/*------------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_run - invalid page count: %d", tpu_run.page_count);
		/*------------------------------------------------------------------------*/
		status = TPU_PAGECOUNT;
		goto end3;
	}

	if ((tpu_run.ldi_offset & 3) != 0 || (tpu_run.ldi_offset >= (tpu_run.page_count * tpu_run.page_size))) {
		#pragma mips_frequency_hint NEVER
		/*------------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_run - invalid ldi offset: %d", tpu_run.ldi_offset);
		/*------------------------------------------------------------------------*/
		status = TPU_LDIOFFSET;
		goto end3;
	}

	/*
	 * Convert the page size in bytes to a hardware page size code;
	 */
	if (tpu_pagecode (tpu_run.page_size) < 0) {
		#pragma mips_frequency_hint NEVER
		/*----------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_run - invalid page size: %d", tpu_run.page_size);
		/*----------------------------------------------------------------------*/
		status = TPU_PAGESIZE;
		goto end3;
	}

	/*--------------------------------------------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_ioctl_run - page count: %d, page size: %d", tpu_run.page_count, tpu_run.page_size);
	/*--------------------------------------------------------------------------------------------------*/

	/*
	 * Pin the user's pages into memory.
	 */
	if ((status = tpu_pin (&tpu_run, userdma_cookie)) != TPU_OK) {
		#pragma mips_frequency_hint NEVER
		/*------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_run - pin failed, status: %d", status);
		/*------------------------------------------------------------*/
		goto end3;
	}

	/*------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_run - pin complete");
	/*------------------------------------------*/

	TPU_LOCK (soft);

	/*
	 * Check that the device is online.
	 */
	if (!soft->ls_online) {
		#pragma mips_frequency_hint NEVER
		/*--------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_run - device offline");
		/*--------------------------------------------*/
		status = TPU_OFFLINE;
		goto end2;
	}

	/*
	 * Check for a simulated immediate fault condition.
	 */
	if (tpu_fault (soft, TPU_FAULT_IMMEDIATE)) {
		#pragma mips_frequency_hint NEVER
		/*----------------------------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_run - inject a simulated immediate fault");
		/*----------------------------------------------------------------*/
		TPU_EVENT (&soft->ls_stat.immediateFault);
		status = tpu_fault_status (soft);
		goto end2;
	}

	/*
	 * Check that the device is idle.
	 */
	if (soft->ls_state != TPU_IDLE) {
		#pragma mips_frequency_hint NEVER
		/*-----------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_run - device busy, ls_state: %d", soft->ls_state);
		/*-----------------------------------------------------------------------*/
		status = TPU_BUSY;
		goto end2;
	}

	/*
	 * Convert the user page table into an alenlist in the soft structure.
	 */
	tpu_create_alenlist (soft, &tpu_run);

	/*
	 * Save some user parameters for future reference.
	 */
	soft->ls_ldi_offset = tpu_run.ldi_offset;
	soft->ls_page_size = tpu_run.page_size;
	soft->ls_page_count = tpu_run.page_count;
	soft->ls_timeout = (tpu_run.timeout <= 0) ? TPU_TIMEOUT_TICKS : tpu_run.timeout * HZ;

	/*
	 * Preset interrupt status information that will be returned to the
	 * user.  In the event of a signal, halt-i/o, or timeout, these values
	 * will be returned as zero.
	 */
	soft->ls_ldiIntStatus = 0;
	soft->ls_ldiLdiStatus = 0;
	soft->ls_ldiLdiSource = 0;
	soft->ls_ldiD0Status = 0;
	soft->ls_ldiD1Status = 0;
	soft->ls_dma0Count = 0;
	soft->ls_dma0Ticks = 0;

	soft->ls_dma1Count = 0;
	soft->ls_dma1Ticks = 0;

	/*
	 * Reset the DMA engine performance counters and timers.
	 */
	tpu_dma_preset (soft);

	/*
	 * Program the address translation table.
	 */
	if ((status = tpu_att_load (soft)) != TPU_OK) {
		#pragma mips_frequency_hint NEVER
		/*-----------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_run - att load failed, status: %d", status);
		/*-----------------------------------------------------------------*/
		goto end1;
	}

	/*
	 * Preset the ending status in the soft structure.  If the interrupt
	 * handlers encounter an error, this will be updated.
	 */
	soft->ls_status = TPU_OK;

	/*
	 * Check for a simulated timeout fault.  If one is indicated, just skip
	 * the tpu_boot() process.  If we never start the LDI, we'll never get
	 * interrupted by it.
	 */
	if (tpu_fault (soft, TPU_FAULT_TIMEOUT)) {
		#pragma mips_frequency_hint NEVER
		/*--------------------------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_run - inject a simulated timeout fault");
		/*--------------------------------------------------------------*/
		TPU_EVENT (&soft->ls_stat.timeoutFault);
	}
	else {
		tpu_boot (soft);
	}

	tpuStartClock = absolute_rtc_current_time ();

	/*
	 * Update the device state.
	 */
	soft->ls_state = TPU_RUNNING;

	/*
	 * Start the timeout clock.
	 */
	soft->ls_timeout_id = itimeout (tpu_timeout, (void *) soft, soft->ls_timeout, pltimeout);

	/*
	 * Wait for the interrupt from the tpu.
	 */
	/*------------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_run - wait for interrupt");
	/*------------------------------------------------*/

	sig_status = TPU_SLEEP (soft);

	tpuEndClock = absolute_rtc_current_time ();

	TPU_LOCK (soft);

	/*-----------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_ioctl_run - wakeup, sig_status: %d, ls_state: %d", 
			sig_status, soft->ls_state);
	/*-----------------------------------------------------------------*/

	/*
	 * Retrieve the interrupt status.
	 */
	status = soft->ls_status;

	/*
	 * If we were interrupted by a signal, check for a race condition with
	 * an interrupt, timeout, or halt-io.  If none of these are in progress,
	 * shut down the TPU and stop the timeout clock.  Update the status in
	 * the tpu_run structure.
	 */
	if (sig_status == 0) {
		#pragma mips_frequency_hint NEVER
		if (soft->ls_state == TPU_RUNNING) {
			tpu_reset (soft);
			untimeout (soft->ls_timeout_id);
		}
		status = TPU_INTR;
	}

	/*
	 * Invalidate the page table before unpinning the user's memory.  This is extra
	 * insurance against a misbehaving LDI program corrupting system memory.
	 */
	tpu_att_inval (soft, soft->ls_page_count);

	/*
	 * Copy interrupt status information for return to the user.
	 */
	tpu_run.dmaStatusReg = soft->ls_ldiIntStatus;
	tpu_run.ldiStatusReg = soft->ls_ldiLdiStatus;
	tpu_run.ldiTimerReg = soft->ls_ldiLdiSource;
	tpu_run.dma0Status = soft->ls_ldiD0Status;
	tpu_run.dma1Status = soft->ls_ldiD1Status;
	tpu_run.dma0Count = soft->ls_dma0Count;
	tpu_run.dma0Ticks = soft->ls_dma0Ticks;
	tpu_run.dma1Count = soft->ls_dma1Count;
	tpu_run.dma1Ticks = soft->ls_dma1Ticks;

	/*
	 * Update the device state.
	 */
	soft->ls_state = TPU_IDLE;

	/*
	 * Check for a simulated deferred fault condition.
	 */
	if (tpu_fault (soft, TPU_FAULT_DEFERRED)) {
		#pragma mips_frequency_hint NEVER
		/*---------------------------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_run - inject a simulated deferred fault");
		/*---------------------------------------------------------------*/
		TPU_EVENT (&soft->ls_stat.deferredFault);
		tpu_fault_regs (soft, &tpu_run);
		status = tpu_fault_status (soft);
	}

	/*
	 * Clean up.
	 */
end1:
	tpu_destroy_alenlist (soft);
end2:
	TPU_UNLOCK (soft);
	/*-----------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_run - unpin user memory");
	/*-----------------------------------------------*/
	tpu_unpin (&tpu_run, userdma_cookie);
	/*--------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_run - unpin complete");
	/*--------------------------------------------*/

end3:

	/*
	 * Copy the ending status to the user's tpud_run_t argument structure.
	 */
	tpu_run.tpu_status = status;
	tpu_run.device = soft->ls_vhdl;

	/*
	 * Return the timestamps for this operation in the user's tpud_run_t
	 * argument structure.
	 */
	tpu_run.sysStartClock = sysStartClock;
	tpu_run.tpuStartClock = tpuStartClock;
	tpu_run.tpuEndClock   = tpuEndClock;
	tpu_run.sysEndClock   = absolute_rtc_current_time ();

	/*
	 * Update the user's tpud_run_t argument structure and set the system errno.
	 */
	if (copyout ((caddr_t) &tpu_run, arg, TPU_RUN_HDR_LEN))
		errno = EFAULT;
	else
		errno = _tpu_status[status].errno;

	/*------------------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_ioctl_run - exit, errno: %d, status: %d", errno, status);
	/*------------------------------------------------------------------------*/

	return errno;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_resume: Resume LDI from breakpoint.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_resume
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlResume);

	return ENOSYS;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_halt: Halt an outstanding TPU_RUN or TPU_RESUME.		*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_halt
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	/*------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_halt - entry");
	/*------------------------------------*/

	TPU_EVENT (&soft->ls_stat.ioctlHalt);

	TPU_LOCK (soft);

	/*
	 * Check that the device is online.
	 */
	if (!soft->ls_online) {
		/*---------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_halt - device offline");
		/*---------------------------------------------*/
		TPU_UNLOCK (soft);
		return EIO;
	}

	/*
	 * Check that there is a TPU_RUN operation in progress for this device,
	 * and that a TPU_HALT isn't already in progress.  This also catches
	 * race conditions with timeouts and interrupts.
	 */
	if (soft->ls_state != TPU_RUNNING) {
		/*--------------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_halt - no run active, ls_state: %d", soft->ls_state);
		/*--------------------------------------------------------------------------*/
		TPU_UNLOCK (soft);
		return EIO;
	}

	/*
	 * Stop the hardware.
	 */
	tpu_reset (soft);

	/*
	 * Stop the timeout clock.
	 */
	untimeout (soft->ls_timeout_id);

	/*
	 * Update the device state and ending status.
	 */
	soft->ls_state = TPU_HALTED;
	soft->ls_status = TPU_HIO;

	/*----------------------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_halt - exit, wake the user process");
	/*----------------------------------------------------------*/

	/*
	 * Wake the process that is blocked in tpu_ioctl_run().
	 */
	TPU_WAKEUP (soft);
	TPU_UNLOCK (soft);

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_regs: Copy out the TPU's hardware registers.			*
 *										*
 *	Note: since this entry point may be called frequently by polling	*
 *	utilities, we don't want to make any trace entries, lest we fill	*
 *	the trace buffer with uninteresting stuff.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_regs
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	int		errno = 0;
	tpud_regs_t	tpu_regs;

	TPU_EVENT (&soft->ls_stat.ioctlRegs);

	TPU_LOCK (soft);

	/*
	 * Check that the device is online.
	 */
	if (!soft->ls_online) {
		TPU_UNLOCK (soft);
		return EIO;
	}

	tpu_get_regs (soft, &tpu_regs);

	TPU_UNLOCK (soft);

	if (copyout ((caddr_t) &tpu_regs, arg, sizeof (tpud_regs_t)))
		errno = EFAULT;

	return errno;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_inst: Copy out the LDI instruction memory.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_inst
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	int		errno = 0;
	tpud_inst_t	tpu_inst;

	/*------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_inst - entry");
	/*------------------------------------*/

	TPU_EVENT (&soft->ls_stat.ioctlInst);

	TPU_LOCK (soft);

	/*
	 * Check that the device is online.
	 */
	if (!soft->ls_online) {
		/*---------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_inst - device offline");
		/*---------------------------------------------*/
		TPU_UNLOCK (soft);
		return EIO;
	}

	/*
	 * Check that the device is idle.  Reading the LDI instruction memory
	 * and modifying coefficient registers while the sequencer is running
	 * may be hazardous to our health.
	 */
	if (soft->ls_state != TPU_IDLE) {
		/*------------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ioctl_inst - device busy, ls_state: %d", soft->ls_state);
		/*------------------------------------------------------------------------*/
		TPU_UNLOCK (soft);
		return EBUSY;
	}

	/*
	 * Read the LDI instruction memory.
	 */
	tpu_get_inst (soft, &tpu_inst);

	TPU_UNLOCK (soft);

	if (copyout ((caddr_t) &tpu_inst, arg, sizeof (tpud_inst_t)))
		errno = EFAULT;

	/*-----------------------------------------------------*/
	TPU_TRACE_D1 ("tpu_ioctl_inst - exit, errno: %d", errno);
	/*-----------------------------------------------------*/

	return errno;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_stat: Copy out a device stat structure.			*
 *										*
 *	Note: since this entry point may be called frequently by polling	*
 *	utilities, we don't want to make any trace entries, lest we fill	*
 *	the trace buffer with uninteresting stuff.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_stat
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	tpud_stat_t	stat;

	TPU_EVENT (&soft->ls_stat.ioctlStat);

	tpu_stat (soft, &stat);

	if (copyout ((caddr_t) &stat, arg, sizeof (tpud_stat_t)))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_stat_list: Copy out an array of device stat structures.	*
 *										*
 *	Note: since this entry point may be called frequently by polling	*
 *	utilities, we don't want to make any trace entries, lest we fill	*
 *	the trace buffer with uninteresting stuff.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_stat_list
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	tpud_stat_list_t *	usr_list = (tpud_stat_list_t *) arg;
	tpud_stat_list_t	drv_list;
	int			i;
	int			limit;
	tpu_soft_t *		s;

	TPU_EVENT (&soft->ls_stat.ioctlStatList);

	/*
	 * Get the list header so we know how big the user's buffer is.
	 */
	if (copyin ((caddr_t) usr_list, (caddr_t) &drv_list, TPU_STAT_LIST_HDR_LEN))
		return EFAULT;

	/*
	 * Loop through the list of active devices, returning as many stat
	 * blocks as the user has room for.
	 */
	limit = (tpu_active.count < drv_list.max) ? tpu_active.count : drv_list.max;

	for (i = 0, s = tpu_active.head; i < limit; i++, s = s->ls_alink) {

		tpu_stat (s, drv_list.stat);

		if (copyout ((caddr_t) drv_list.stat, (caddr_t) &usr_list->stat[i], sizeof (tpud_stat_t)))
			return EFAULT;
	}

	drv_list.count = i;

	/*
	 * Update the number of stat blocks returned.
	 */
	if (copyout ((caddr_t) &drv_list, (caddr_t) usr_list, TPU_STAT_LIST_HDR_LEN))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_stats: Copy out device statistics.				*
 *										*
 *	Note: since this entry point may be called frequently by polling	*
 *	utilities, we don't want to make any trace entries, lest we fill	*
 *	the trace buffer with uninteresting stuff.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_stats
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlStats);

	if (copyout ((caddr_t) &soft->ls_stat, arg, sizeof (tpud_stats_t)))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_gstats: Copy out global statistics.				*
 *										*
 *	Note: since this entry point may be called frequently by polling	*
 *	utilities, we don't want to make any trace entries, lest we fill	*
 *	the trace buffer with uninteresting stuff.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_gstats
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlGstats);

	if (copyout ((caddr_t) &tpu_gstats, arg, sizeof (tpud_gstats_t)))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_config: Vary a TPU online/offline.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_config
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	long		state = (long) arg;

	TPU_EVENT (&soft->ls_stat.ioctlConfig);

	switch (state) {

	case TPU_CONFIG_DOWN:
		return (tpu_ioctl_config_down (soft));

	case TPU_CONFIG_UP:
		return (tpu_ioctl_config_up (soft));
	}

	/*-----------------------------------------------------------*/
	TPU_TRACE_D1 ("tpu_ioctl_config - invalid state: 0x%x", state);
	/*-----------------------------------------------------------*/

	return EINVAL;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_config_down: Vary a TPU offline.				*
 *										*
 ********************************************************************************/
static int
tpu_ioctl_config_down
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	/*-------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_config_down - entry");
	/*-------------------------------------------*/

	TPU_LOCK (soft);

	/*
	 * Check if there is a TPU_RUN operation in progress for this device.
	 * This also catches race conditions with TPU_HALT, timeouts, and
	 * interrupts.
	 */
	if (soft->ls_state == TPU_RUNNING) {

		/*----------------------------------------------------*/
		TPU_TRACE_D0 ("tpu_ioctl_config_down - halting device");
		/*----------------------------------------------------*/

		/*
		 * Stop the hardware.
		 */
		tpu_reset (soft);

		/*
		 * Stop the timeout clock.
		 */
		untimeout (soft->ls_timeout_id);

		/*
		 * Update the device state and ending status.
		 */
		soft->ls_state = TPU_HALTED;
		soft->ls_status = TPU_DOWNED;

		/*
		 * Wake the process that is blocked in tpu_ioctl_run().
		 */
		TPU_WAKEUP (soft);
	}

	if (soft->ls_online) {

		/*
		 * Disconnect the interrupt handlers.
		 */
		tpu_intr_disconnect (soft);

		/*
		 * Mark the device offline.
		 */
		soft->ls_online = 0;
	}

	TPU_UNLOCK (soft);

	/*------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_config_down - exit");
	/*------------------------------------------*/

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_config_up: Vary a TPU online.					*
 *										*
 ********************************************************************************/
static int
tpu_ioctl_config_up
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	/*-----------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_config_up - entry");
	/*-----------------------------------------*/

	TPU_LOCK (soft);

	if (!soft->ls_online) {

		/*
		 * (Re)initialize the TPU hardware.
		 */
		tpu_reset (soft);
		tpu_wid_init (soft);
		tpu_xtk_init (soft);
		tpu_dma_init (soft);
		tpu_att_init (soft);
		tpu_ldi_init (soft);

		/*
		 * Connect the interrupt handlers.
		 */
		tpu_intr_connect (soft);

		/*
		 * Mark the device online.
		 */
		soft->ls_online = 1;
	}

	TPU_UNLOCK (soft);

	/*----------------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_config_up - exit");
	/*----------------------------------------*/

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_set_flag:  Set user defined flag value.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_set_flag
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	/*--------------------------------------------------------------*/
	TPU_TRACE_D1 ("tpu_ioctl_set_flag - value: %d", (__uint64_t) arg);
	/*--------------------------------------------------------------*/

	TPU_EVENT (&soft->ls_stat.ioctlSetFlag);

	soft->ls_user_flag = (__uint64_t) arg;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_get_flag: Get user defined flag value.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_get_flag
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlGetFlag);

	if (copyout ((caddr_t) &soft->ls_user_flag, arg, sizeof (__uint64_t)))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_set_fault: Insert a simulated fault.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_set_fault
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	int		status = 0;

	TPU_EVENT (&soft->ls_stat.ioctlSetFault);

	TPU_LOCK (soft);

	if (copyin (arg, (caddr_t) &soft->ls_fault, sizeof (tpud_fault_t))) {
		status = EFAULT;
	}
	else {
		soft->ls_fault_interval_r = soft->ls_fault.interval;
		soft->ls_fault_duration_r = soft->ls_fault.duration;
	}

	TPU_UNLOCK (soft);

	/*-------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_ioctl_set_fault - status: %d, type: %d",
				status, soft->ls_fault.type);
	/*-------------------------------------------------------*/

	return status;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_get_fault: Get the current simulated fault information.	*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_get_fault
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlGetFault);

	if (copyout ((caddr_t) &soft->ls_fault, arg, sizeof (tpud_fault_t)))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_get_soft: Copy out the soft structure.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_get_soft
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlGetSoft);

	if (copyout ((caddr_t) soft, arg, sizeof (tpu_soft_t)))
		return EFAULT;

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ioctl_get_trace: Copy out the trace buffer.				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_get_trace
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlGetTrace);

	return (tpu_trace_copyout (soft->ls_tbuffer, (tpud_tracex_t *) arg));
}

/********************************************************************************
 *										*
 *	tpu_ioctl_get_gtrace: Copy out the global trace buffer.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_get_gtrace
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	TPU_EVENT (&soft->ls_stat.ioctlGetGTrace);

	return (tpu_trace_copyout (tpu_global_trace_buffer, (tpud_tracex_t *) arg));
}

/********************************************************************************
 *										*
 *	tpu_ioctl_ext_test: Enable the DMA external test pins.			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static int
tpu_ioctl_ext_test
	(
	tpu_soft_t *	soft,
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	widgetreg_t	value = (__uint64_t) arg & TPU_ENABLE_REG_MSK;

	/*--------------------------------*/
	TPU_TRACE_D0 ("tpu_ioctl_ext_test");
	/*--------------------------------*/

	TPU_EVENT (&soft->ls_stat.ioctlExtTest);

	tpu_dma_leds (soft, value);

	return 0;
}


/* =====================================================================
 *	UTILITY FUNCTIONS
 */

/********************************************************************************
 *										*
 *	tpu_create_device: Create a new tpu device.				*
 *										*
 ********************************************************************************/
static tpu_soft_t *	
tpu_create_device 
	(
	vertex_hdl_t		conn		/* parent xtalk vertex */
	)
{
	tpu_soft_t *		soft;		/* our new device table */
	vertex_hdl_t		vhdl;		/* our new tpu device vertex */
	vertex_hdl_t		admin;		/* our new admin device vertex */
	graph_error_t		status;		/* return status from various hwgraph functions */
	const tpu_service_t *	map;		/* pointer to correct service map */
	device_desc_t           error_intr_desc;


	/*
	 * Select the real or simulated xtalk_ services depending on whether
	 * the parent vertex is a real xtalk widget or not.
	 */
	if (xwidget_info_chk (conn) != NULL) {
		/*-------------------------------------------------------------*/
		TPU_TRACE_G1 ("tpu_create_device - conn: %d (real xtalk)", conn);
		/*-------------------------------------------------------------*/
		map = &tpu_map_xtalk;
	}
	else if (tpusim_info_chk (conn) != NULL) {
		/*------------------------------------------------------------------*/
		TPU_TRACE_G1 ("tpu_create_device - conn: %d (simulated xtalk)", conn);
		/*------------------------------------------------------------------*/
		map = &tpu_map_sim;
	}
	else {
		/*----------------------------------------------------------------------*/
		TPU_TRACE_G1 ("tpu_create_device - conn: %d (invalid widget type)", conn);
		/*----------------------------------------------------------------------*/
		cmn_err (CE_WARN, "tpu: attach %v - invalid widget type\n", conn);
		return 0;
	}

	/*
	 * Create the hardware graph character device vertex for this tpu.
	 */
	if ((status = hwgraph_char_device_add (conn, "tpu", "tpu_", &vhdl)) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_char_device_add failed for %v/tpu: %d", conn, status);
		return 0;
	}

	hwgraph_chmod (vhdl, 0666);

	/*
	 * Create the hardware graph admin vertex for this tpu.
	 */
	if ((status = hwgraph_char_device_add (conn, "admin", "tpu_", &admin)) != GRAPH_SUCCESS) {
		cmn_err (CE_WARN, "tpu: hwgraph_char_device_add failed for %v/admin: %d", conn, status);
		return 0;
	}

	hwgraph_chmod (admin, 0666);

	/*
	 * Allocate a tpu_soft_s structure for this device and link to it from
	 * the hwgraph verticies.
	 */
	NEW (soft);
	TPU_SOFT_SET (vhdl, soft);
	TPU_SOFT_SET (admin, soft);

	/*
	 * Fill in the tpu_soft_s structure for this device.
	 */
	soft->ls_magic = TPU_M_SOFT;
	soft->ls_size = sizeof (tpu_soft_t);
	soft->ls_conn = conn;
	soft->ls_vhdl = vhdl;
	soft->ls_admin = admin;
	soft->ls_online = 0;
	soft->ls_state = TPU_IDLE;
	soft->ls_open = 0;
	soft->ls_exclusive = 0;
	soft->ls_service = map;
	soft->ls_tbuffer = tpu_trace_alloc ();
	soft->ls_alink = NULL;
	soft->ls_fault.type = TPU_FAULT_NONE;

	TPU_LOCK_INIT (soft);
	TPU_SLEEP_INIT (soft);

	/*------------------------------------------------------------------*/
	TPU_TRACE_G2 ("tpu_create_device - vhdl: %d, soft: 0x%x", vhdl, soft);
	/*------------------------------------------------------------------*/
	/*-------------------------------*/
	TPU_TRACE_D0 ("tpu_create_device"); 
	/*-------------------------------*/

	/*
	 * Get pointers to our various register sets.
	 */
	soft->ls_xregs = (tpu_xtk_regs_t *) XTALK_FUNC(soft, piotrans_addr)
				(conn, 0, TPU_XTK_REGS_OFFSET, sizeof (tpu_xtk_regs_t), 0);

	soft->ls_aregs = (tpu_att_regs_t *) XTALK_FUNC(soft, piotrans_addr)
				(conn, 0, TPU_ATT_REGS_OFFSET, sizeof (tpu_att_regs_t), 0);

	soft->ls_d0regs = (tpu_dma_regs_t *) XTALK_FUNC(soft, piotrans_addr)
				(conn, 0, TPU_DMA0_REGS_OFFSET, sizeof (tpu_dma_regs_t), 0);

	soft->ls_d1regs = (tpu_dma_regs_t *) XTALK_FUNC(soft, piotrans_addr)
				(conn, 0, TPU_DMA1_REGS_OFFSET, sizeof (tpu_dma_regs_t), 0);

	soft->ls_lregs = (tpu_ldi_regs_t *) XTALK_FUNC(soft, piotrans_addr)
				(conn, 0, TPU_LDI_REGS_OFFSET, sizeof (tpu_ldi_regs_t), 0);

	/*
	 * Allocate interrupt handler vectors.
	 */
	error_intr_desc = device_desc_dup (vhdl);
	device_desc_flags_set (error_intr_desc,
				device_desc_flags_get(error_intr_desc) | D_INTR_ISERR);

	soft->ls_dmaError_intr   = XTALK_FUNC(soft, intr_alloc) (conn, error_intr_desc, vhdl);
	soft->ls_ldiBarrier_intr = XTALK_FUNC(soft, intr_alloc) (conn, 0, vhdl);
	soft->ls_ldiError_intr   = XTALK_FUNC(soft, intr_alloc) (conn, 0, vhdl);
	soft->ls_ldiCError_intr  = XTALK_FUNC(soft, intr_alloc) (conn, 0, vhdl);

	device_desc_free (error_intr_desc);

	/*
	 * Add this tpu to the active list.
	 */
	tpu_alist_add (soft);

	return soft;
}

/********************************************************************************
 *										*
 *	tpu_intr_connect: Connect interrupts for a TPU.				*
 *										*
 ********************************************************************************/
static void		
tpu_intr_connect
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	/*------------------------------*/
	TPU_TRACE_D0 ("tpu_intr_connect"); 
	/*------------------------------*/

	/*
	 * Connect the DMA error interrupt handler.
	 */
	XTALK_FUNC(soft, intr_connect) (soft->ls_dmaError_intr,
				tpu_dmaError_intr, soft,
				tpu_dmaError_setfunc, soft,
				(void *) 0);

	/*
	 * Connect the LDI barrier interrupt handler.
	 */
	XTALK_FUNC(soft, intr_connect) (soft->ls_ldiBarrier_intr,
				tpu_ldiBarrier_intr, soft,
				tpu_ldiBarrier_setfunc, soft,
				(void *) 0);

	/*
	 * Connect the LDI error interrupt handler.
	 */
	XTALK_FUNC(soft, intr_connect) (soft->ls_ldiError_intr,
				tpu_ldiError_intr, soft,
				tpu_ldiError_setfunc, soft,
				(void *) 0);

	/*
	 * Connect the LDI channel error interrupt handler.
	 */
	XTALK_FUNC(soft, intr_connect) (soft->ls_ldiCError_intr,
				tpu_ldiCError_intr, soft,
				tpu_ldiCError_setfunc, soft,
				(void *) 0);
}

/********************************************************************************
 *										*
 *	tpu_intr_disconnect: Disconnect interrupts for a TPU.			*
 *										*
 ********************************************************************************/
static void		
tpu_intr_disconnect
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_intr_disconnect"); 
	/*---------------------------------*/

	/*
	 * Disconnect the DMA error interrupt handler.
	 */
	XTALK_FUNC(soft, intr_disconnect) (soft->ls_dmaError_intr);

	/*
	 * Disconnect the LDI barrier interrupt handler.
	 */
	XTALK_FUNC(soft, intr_disconnect) (soft->ls_ldiBarrier_intr);

	/*
	 * Disconnect the LDI error interrupt handler.
	 */
	XTALK_FUNC(soft, intr_disconnect) (soft->ls_ldiError_intr);

	/*
	 * Disconnect the LDI channel error interrupt handler.
	 */
	XTALK_FUNC(soft, intr_disconnect) (soft->ls_ldiCError_intr);
}

/********************************************************************************
 *										*
 *	tpu_conv_add - Add a convience path for a tpu.				*
 *										*
 *	Note: the way we construct the short name from the full hwgfs path	*
 *	name is a bit of a kludge, and is currently specific to the Origin and	*
 *	Octane I/O layout.  If this driver is to be used on other architectures	*
 *	this code will need to be enhanced.					*
 *										*
 *	The following name conversions are implemented:				*
 *										*
 *	/hw/tpusim/6/tpu          -> /hw/tpu/sim006				*
 *	/hw/module/2/slot/io4/tpu -> /hw/tpu/m02s04	(IP27)			*
 *	/hw/node/xtalk/9/tpu      -> /hw/tpu/slot09	(IP30)			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
static void
tpu_conv_add 
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	char		name[MAXDEVNAME];
	char *		p1;
	char *		p2;
	int		status;

	if (hwgraph_vertex_name_get (soft->ls_conn, name, MAXDEVNAME) != GRAPH_SUCCESS)
		return;

	/*
	 * Simulated TPUs.
	 */
	if (tpu_simulated (soft)) {
		soft->ls_module = 0;
		soft->ls_slot = 0;
		strcpy (soft->ls_name, "sim");
		lfu (atoi (name+11), 10, 3, soft->ls_name+3, '0');
	}

	/*
	 * Hardware TPUs in a SN0 (IP27).
	 */
	else if (((p1 = strstr (name, "/module/")) != NULL) && 
		 ((p2 = strstr (name, "/slot/io")) != NULL)) {
		soft->ls_module = atoi (p1+8);
		soft->ls_slot = atoi (p2+8);
		strcpy (soft->ls_name, "m");
		lfu (soft->ls_module, 10, 2, soft->ls_name+1, '0');
		soft->ls_name[3] = 's';
		lfu (soft->ls_slot, 10, 2, soft->ls_name+4, '0');
	}

	/*
	 * Hardware TPUs in an Octane (IP30).
	 */
	else if ((p1 = strstr (name, "/node/xtalk/")) != NULL) {
		soft->ls_module = 0;
		soft->ls_slot = atoi (p1+12);
		strcpy (soft->ls_name, "slot");
		lfu (soft->ls_slot, 10, 2, soft->ls_name+4, '0');
	}

	/*
	 * Add support for other architectures here.
	 */
	else {		
		cmn_err (CE_NOTE, "tpu: unable to construct name for: %s\n", name);	
		return;
	}

	/*
	 * Add an edge from /hw/tpu to our parent vertex.
	 */
	if ((status = hwgraph_edge_add (tpu_root, soft->ls_conn, soft->ls_name)) != GRAPH_SUCCESS)
		cmn_err (CE_NOTE, "tpu: hwgraph_edge_add failed for /hw/tpu/%s (%v -> %v): %d", 
				soft->ls_name, tpu_root, soft->ls_conn, status);

}

/********************************************************************************
 *										*
 *	tpu_inventory_add: Update the harware inventory information for a TPU.	*
 *										*
 ********************************************************************************/
static void
tpu_inventory_add 
	(
	tpu_soft_t *	soft
	)
{
	char *		nic_string;
	int		tpu_type;

	if (tpu_simulated (soft))
		return;

	/*
	 * Decode the NIC and hang its stuff off our connection point where
	 * hinv can get at it.
	 */
 	nic_string = nic_hq4_vertex_info (soft->ls_conn, (nic_data_t) &soft->ls_xregs->x_nic);

	/*
	 * Save a copy of the NIC string in the soft structure so that it is
	 * available in TPU dumps.
	 */
	if (nic_string != NULL)
		strncpy (soft->ls_nic_string, nic_string, sizeof (soft->ls_nic_string));

	/*
	 * Determine if this is an external (Mesa) TPU or an internal (XIO) TPU.
	 */
	if (strstr (nic_string, "Part:001-3423-900") != NULL)
		tpu_type = INV_TPU_EXT;
	else
		tpu_type = INV_TPU_XIO;

	/*
	 * Add our record to the inventory.
	 */
	device_inventory_add (soft->ls_conn, INV_TPU, tpu_type, 
				soft->ls_module, soft->ls_slot, 0);
}

/********************************************************************************
 *										*
 *	tpu_any - Return true if this is /hw/tpu/any/tpu.			*
 *										*
 ********************************************************************************/
static int		
tpu_any 
	(
	vertex_hdl_t	vhdl		/* vertex handle from user request */
	)
{
	return ((vhdl == tpu_any_tpu) || (vhdl == tpu_any_admin));
}

/********************************************************************************
 *										*
 *	tpu_admin - Return true if this is an admin device.			*
 *										*
 ********************************************************************************/
static int		
tpu_admin
	(
	vertex_hdl_t	vhdl		/* vertex handle from user request */
	)
{
	if (vhdl == tpu_any_tpu)
		return 0;
	else if (vhdl == tpu_any_admin)
		return 1;
	else
		return (vhdl == (TPU_SOFT_GET (vhdl))->ls_admin);
}

/********************************************************************************
 *										*
 *	tpu_simulated - Return true if this is a simulated tpu.			*
 *										*
 ********************************************************************************/
static int		
tpu_simulated 
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	return (soft->ls_service != &tpu_map_xtalk);
}

/********************************************************************************
 *										*
 *	tpu_assign - Assign a TPU to process a request.				*
 *										*
 ********************************************************************************/
static tpu_soft_t *
tpu_assign
	(
	vertex_hdl_t 	vhdl		/* vertex handle from user request */
	)
{
	tpu_soft_t *	soft;

	/*
	 * Check if the user has requested a specific device.  If so, just use
	 * it.
	 */
	if (!tpu_any (vhdl)) {

		soft = TPU_SOFT_GET (vhdl);

		if (!tpu_admin (vhdl)) {
			TPU_EVENT (&tpu_gstats.assign_spec);
			atomicAddInt (&soft->ls_assigned, 1);
		}

		return soft;
	}

	TPU_EVENT (&tpu_gstats.assign_any);

	/*
	 * If this assignment is for any/admin, just grab the first soft
	 * structure on the list since any will do.
	 */
	if (tpu_admin (vhdl))
		return (tpu_active.head);

	/*
	 * Search for a suitable device.
	 */
	for (soft = tpu_active.head; soft != NULL; soft = soft->ls_alink) {

		TPU_EVENT (&tpu_gstats.assign_chk);

		if (tpu_simulated (soft))
			continue;

		if (soft->ls_exclusive || soft->ls_assigned || !soft->ls_online) 
			continue;

		TPU_LOCK (soft);
		if (soft->ls_exclusive || soft->ls_assigned || !soft->ls_online) {
			TPU_UNLOCK (soft);
			continue;
		}

		soft->ls_assigned++;
		TPU_UNLOCK (soft);

		return soft;
	}

	TPU_EVENT (&tpu_gstats.assign_none);

	/*
	 * There were no devices available at this time.  In the future, we can
	 * add code to sleep waiting for a device to become available, but for
	 * now, just return null.
	 */
	return NULL;	/***FIXME***/
}

/********************************************************************************
 *										*
 *	tpu_unassign - Release device assignment.				*
 *										*
 ********************************************************************************/
static void	
tpu_unassign
	(
	vertex_hdl_t 	vhdl,		/* vertex handle from user request */
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	if (!tpu_admin (vhdl))
		atomicAddInt (&soft->ls_assigned, -1);
}

/********************************************************************************
 *										*
 *	tpu_pagecode - Return a hardware page size code given a page size in	*
 *			bytes.							*
 *										*
 ********************************************************************************/
static int		
tpu_pagecode
	(
	int		page_size		/* page size in bytes */
	)
{
	switch (page_size) {

	case 0x4000:				/* 16K */
		return ATT_PAGE_SIZE_16K;

	case 0x10000:				/* 64K */
		return ATT_PAGE_SIZE_64K;

	case 0x40000:				/* 256K */
		return ATT_PAGE_SIZE_256K;

	case 0x100000:				/* 1M */
		return ATT_PAGE_SIZE_1M;

	case 0x400000:				/* 4M */
		return ATT_PAGE_SIZE_4M;

	case 0x1000000:				/* 16M */
		return ATT_PAGE_SIZE_16M;
	}

	return -1;				/* invalid page size */
}

/********************************************************************************
 *										*
 *	tpu_list_init - Initialize a tpu list header.				*
 *										*
 ********************************************************************************/
static void
tpu_list_init
	(
	tpu_list_t *	list		/* head of active tpu list */
	)
{
	list->head = NULL;
	list->count = 0;
	MUTEX_INIT (&list->mlock, MUTEX_DEFAULT, "tpulist_mutex");
}

/********************************************************************************
 *										*
 *	tpu_alist_add - Add a tpu to the active list.				*
 *										*
 ********************************************************************************/
static void
tpu_alist_add
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	MUTEX_LOCK (&tpu_active.mlock, -1);

	ASSERT (soft->ls_alink == NULL);

	soft->ls_alink = tpu_active.head;
	tpu_active.head = soft;
	tpu_active.count++;

	MUTEX_UNLOCK (&tpu_active.mlock);
}

/********************************************************************************
 *										*
 *	tpu_pin: Pin user pages into memory.					*
 *										*
 ********************************************************************************/
static int
tpu_pin
	(
	tpud_run_t *	tpu_run,	/* user's argument structure */
	int *		cookie		/* cookie array for userdma/undma */
	)
{
	int		i;
	int		errno;

	for (i = 0; i < tpu_run->page_count; i++) {

		if ((errno = fast_userdma ((void *) tpu_run->page_addr[i], tpu_run->page_size, B_WRITE, &cookie[i])) != 0) {

			while (--i >= 0) 
				fast_undma ((void *) tpu_run->page_addr[i], tpu_run->page_size, B_WRITE, &cookie[i]);

			if (errno == EFAULT)
				return TPU_PIN_ADDR;
			else if (errno == EAGAIN)
				return TPU_PIN_SYS;
			else
				return TPU_PIN_UNK;
		}
	}

	return TPU_OK;
}

/********************************************************************************
 *										*
 *	tpu_unpin: Unpin user pages from memory.				*
 *										*
 ********************************************************************************/
static void
tpu_unpin
	(
	tpud_run_t *	tpu_run,	/* user's argument structure */
	int *		cookie		/* cookie array for userdma/undma */
	)
{
	int		i;

	for (i = 0; i < tpu_run->page_count; i++)
		fast_undma ((void *) tpu_run->page_addr[i], tpu_run->page_size, B_WRITE, &cookie[i]);
}

/********************************************************************************
 *										*
 *	tpu_create_alenlist: Translate user page table to alenlist.		*
 *										*
 ********************************************************************************/
static void
tpu_create_alenlist
	(
	tpu_soft_t *	soft,		/* tpu soft structure */
	tpud_run_t *	tpu_run		/* user's argument structure */
	)
{
	int		i;
	alenlist_t	al;

	/*-----------------------------------------*/
	TPU_TRACE_D0 ("tpu_create_alenlist - entry");
	/*-----------------------------------------*/

	/*
	 * Create an empty alenlist.
	 */
	soft->ls_al = alenlist_create (0);

	/*
	 * Convert the individual user pages to individual alenlists and append
	 * them to the master list.
	 */
	for (i = 0; i < tpu_run->page_count; i++) {
		al = uvaddr_to_alenlist (NULL,	(uvaddr_t) tpu_run->page_addr[i], 
						(size_t) tpu_run->page_size, 0);
		alenlist_concat (al, soft->ls_al);
		alenlist_done (al);
	}

	/*
	 * Translate the alenlist addresses from CPU/physical to DMA/physical
	 * for the TPU widget in question.
	 */
	soft->ls_al = XTALK_FUNC(soft, dmatrans_list) (soft->ls_conn, 0, soft->ls_al, XTALK_INPLACE);

	/*----------------------------------------*/
	TPU_TRACE_D0 ("tpu_create_alenlist - exit");
	/*----------------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_destroy_alenlist: Remove alenlist created by tpu_create_alenlist().	*
 *										*
 ********************************************************************************/
static void
tpu_destroy_alenlist
	(
	tpu_soft_t *	soft		/* tpu soft structure */
	)
{
	alenlist_destroy (soft->ls_al);

	soft->ls_al = NULL;
}

/********************************************************************************
 *										*
 *	tpu_stat: Fill in the stat block for a device.				*
 *										*
 *	Note: since this function may be called frequently by polling		*
 *	utilities, we don't want to make any trace entries, lest we fill	*
 *	the trace buffer with uninteresting stuff.				*
 *										*
 ********************************************************************************/
static void
tpu_stat 
	(
	tpu_soft_t *	soft, 		/* tpu soft structure */
	tpud_stat_t *	stat		/* stat block to fill in */
	)
{
	strcpy (stat->name, soft->ls_name);

	stat->vhdl      = soft->ls_vhdl;
	stat->admin     = soft->ls_admin;
	stat->online    = soft->ls_online;
	stat->state     = soft->ls_state;
	stat->open      = soft->ls_open;
	stat->exclusive = soft->ls_exclusive;
	stat->simulated = tpu_simulated (soft);
	stat->module    = soft->ls_module;
	stat->slot      = soft->ls_slot;
	stat->flag      = soft->ls_user_flag;
	stat->dma0Count = soft->ls_stat.dma0Count;
	stat->dma0Ticks = soft->ls_stat.dma0Ticks;
	stat->dma1Count = soft->ls_stat.dma1Count;
	stat->dma1Ticks = soft->ls_stat.dma1Ticks;
}

/********************************************************************************
 *										*
 *	lfu - unsigned binary to ascii with leading fill.			*
 *										*
 *	If sprintf() implemented field widths this wouldn't be needed.		*
 *										*
 ********************************************************************************/

static char * 
lfu
	(
	__uint64_t	val,		/* value to convert */
	int		base,		/* radix */
	int		min,		/* minimum number of digits */
	char *		s,		/* output buffer */
	char		f		/* fill character */
	)
{
	char		buf[128];
	char *		p = buf;
	char *		q = s;
	int		d;

	do {
		*p++ = ((d = val % base) < 10) ? (d + '0') : (d + 'a' - 10);
		min--;

	} while ((val /= base) > 0);

	for (; min > 0; min--)
		*p++ = f;

	while (--p >= buf)
		*q++ = *p;

	*q = '\0';

	return s;
}


/* =====================================================================
 *	FAULT INSERTION FUNCTIONS
 *
 *	These functions are used to insert simulated fault conditions
 *	into a normally operating TPU, as an aid in developing application
 *	error handling.
 */

/********************************************************************************
 *										*
 *	tpu_fault: Return true if a simulated fault condition is present.	*
 *										*
 ********************************************************************************/
static int
tpu_fault 
	(
	tpu_soft_t *		soft,		/* tpu soft structure */
	tpud_fault_type_t	type		/* type of fault to check for */
	)
{
	/*
	 * Check for a fault of the matching type with cycles remaining.
	 */
	if ((type != soft->ls_fault.type) || (soft->ls_fault.cycles <= 0))
		return 0;

	/*
	 * Check if the interval is complete.
	 */
	if (--soft->ls_fault.interval >= 0)
		return 0;

	/*
	 * At this point a simulated fault is indicated.  Check if this is the
	 * last fault in the repeat group.  If it is, reload the counters for
	 * the next cycle.
	 */
	if (--soft->ls_fault.duration <= 0) {
		soft->ls_fault.interval = soft->ls_fault_interval_r;
		soft->ls_fault.duration = soft->ls_fault_duration_r;
		soft->ls_fault.cycles--;
	}

	return 1;
}

/********************************************************************************
 *										*
 *	tpu_fault_status: Return the tpu_run.tpu_status value for a fault.	*
 *										*
 ********************************************************************************/
static int
tpu_fault_status 
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	return ((soft->ls_fault.tpu_status > TPU_ERR_MAX) ? TPU_ERR_MAX : soft->ls_fault.tpu_status);
}

/********************************************************************************
 *										*
 *	tpu_fault_regs: Modify status registers returned to the user.		*
 *										*
 ********************************************************************************/
/*ARGSUSED*/
static void	
tpu_fault_regs 
	(
	tpu_soft_t *		soft, 		/* tpu soft structure */
	tpud_run_t *		rp		/* user's tpu_run_t structure */
	)
{
	rp->dmaStatusReg = (rp->dmaStatusReg & ~soft->ls_fault.dmaStatusReg_mask) |
				(soft->ls_fault.dmaStatusReg & soft->ls_fault.dmaStatusReg_mask);

	rp->ldiStatusReg = (rp->ldiStatusReg & ~soft->ls_fault.ldiStatusReg_mask) |
				(soft->ls_fault.ldiStatusReg & soft->ls_fault.ldiStatusReg_mask);

	rp->ldiTimerReg = (rp->ldiTimerReg & ~soft->ls_fault.ldiTimerReg_mask) |
				(soft->ls_fault.ldiTimerReg & soft->ls_fault.ldiTimerReg_mask);

	rp->dma0Status = (rp->dma0Status & ~soft->ls_fault.dma0Status_mask) |
				(soft->ls_fault.dma0Status & soft->ls_fault.dma0Status_mask);

	rp->dma1Status = (rp->dma1Status & ~soft->ls_fault.dma1Status_mask) |
				(soft->ls_fault.dma1Status & soft->ls_fault.dma1Status_mask);
}


/* =====================================================================
 *	HARDWARE INTERFACE FUNCTIONS
 *
 *	Upper-half routines that directly manipulate the TPU hardware registers
 *	should reside in this section.
 */

/********************************************************************************
 *										*
 *	tpu_reset: Reset the LDI hardware.					*
 *										*
 ********************************************************************************/
static void
tpu_reset
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	/*-------------------------------*/
	TPU_TRACE_D0 ("tpu_reset - entry");
	/*-------------------------------*/

	if (tpu_simulated (soft))
		untimeout (soft->ls_sim_id);

	soft->ls_xregs->x_control |=  TPU_SOFT_RESET;
	soft->ls_xregs->x_control &= ~TPU_SOFT_RESET;

	/*
	 * Wait for the reset to complete.
	 */
	if (soft->ls_xregs->x_control)
		;

	/*------------------------------*/
	TPU_TRACE_D0 ("tpu_reset - exit");
	/*------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_wid_init: Initialize the widget ID number.				*
 *										*
 ********************************************************************************/
static void
tpu_wid_init 
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_xtk_regs_t *	pXtk = soft->ls_xregs;
	__uint64_t		wid = ((__uint64_t) pXtk >> 24) & 0xf;	/* OK for IP27 and IP30 */

	/*-------------------------------------------------*/
	TPU_TRACE_D1 ("tpu_wid_init - widget id: 0x%x", wid);
	/*-------------------------------------------------*/

	pXtk->x_control = (pXtk->x_control & ~TPU_WIDGET_ID_MSK) | (wid << TPU_WIDGET_ID_SHFT);
}

/********************************************************************************
 *										*
 *	tpu_xtk_init: Initialize the xtalk widget hardware.			*
 *										*
 ********************************************************************************/
static void
tpu_xtk_init 
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_xtk_regs_t *	pXtk = soft->ls_xregs;

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_xtk_init - entry");
	/*----------------------------------*/

	/*
	 * Set the crossbar credit count.
	 */
	pXtk->x_control = (pXtk->x_control & ~TPU_LLP_XBAR_CREDIT_MSK) | 
				(xbar_credits << TPU_LLP_XBAR_CREDIT_SHFT);

	/*
	 * Reset all the interrupt status bits.
	 */
	pXtk->x_int_status_reset = TPU_INT_STATUS_REG_MSK;

	/*
	 * Preset the interrupt addresses and vectors to a known (and invalid)
	 * value.
	 */
	pXtk->x_int_addr0 = 0x00ffffff;
	pXtk->x_int_addr1 = 0x00ffffff;
	pXtk->x_int_addr2 = 0x00ffffff;
	pXtk->x_int_addr3 = 0x00ffffff;
	pXtk->x_int_addr4 = 0x00ffffff;
	pXtk->x_int_addr5 = 0x00ffffff;
	pXtk->x_int_addr6 = 0x00ffffff;

	/*
	 * Gate a solid zero value on the 8 external testpoint pins.
	 */
	pXtk->x_leds = TPU_ENABLE_SELECT;

	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_xtk_init - exit");
	/*---------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_dma_init: Initialize the DMA hardware.				*
 *										*
 ********************************************************************************/
static void
tpu_dma_init 
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_dma_regs_t *	pDma;

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_dma_init - entry");
	/*----------------------------------*/

	/*
	 * Initialize dma engine 0.  The crosstalk destination ID will be set
	 * later when we assign the interrupt target.
	 */
	pDma = soft->ls_d0regs;

	pDma->d_config0 = 0x100000;	/* set coherent transaction -- required for heart */
	pDma->d_config1 = 0x100000;
	pDma->d_diag_mode = 0;
	pDma->d_perf_timer = 0;
	pDma->d_perf_count = 0;
	pDma->d_perf_config = 0x1d;

	/*
	 * Initialize dma engine 1.  The crosstalk destination ID will be set
	 * later when we assign the interrupt target.
	 */
	pDma = soft->ls_d1regs;

	pDma->d_config0 = 0x100000;	/* set coherent transaction -- required for heart */
	pDma->d_config1 = 0x100000;
	pDma->d_diag_mode = 0;
	pDma->d_perf_timer = 0;
	pDma->d_perf_count = 0;
	pDma->d_perf_config = 0x1d;

	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_dma_init - exit");
	/*---------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_dma_preset: Reset the DMA performance counters and timers.		*
 *										*
 ********************************************************************************/
static void
tpu_dma_preset
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_dma_regs_t *	pDma;

	/*
	 * Reset dma engine 0 counter/timer.
	 */
	pDma = soft->ls_d0regs;

	pDma->d_perf_timer = 0;
	pDma->d_perf_count = 0;
	pDma->d_perf_config = 0x1d;

	/*
	 * Reset dma engine 1 counter/timer.
	 */
	pDma = soft->ls_d1regs;

	pDma->d_perf_timer = 0;
	pDma->d_perf_count = 0;
	pDma->d_perf_config = 0x1d;
}

/********************************************************************************
 *										*
 *	tpu_att_init: Initialize the DMA address translation hardware.		*
 *										*
 ********************************************************************************/
static void
tpu_att_init 
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_att_regs_t *	pAtt = soft->ls_aregs;
	int			i;

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_att_init - entry");
	/*----------------------------------*/

	pAtt->a_config = ATT_ENABLE_0 | ATT_ENABLE_1;
	pAtt->a_diag = 0;

	for (i = 0; i < TPU_ATT_SIZE; i++)
		pAtt->a_atte[i] = 0;

	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_att_init - exit");
	/*---------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_att_inval: Invalidate DMA address translation entries.		*
 *										*
 ********************************************************************************/
static void
tpu_att_inval
	(
	tpu_soft_t *		soft,		/* tpu soft structure */
	int			count		/* number of entries to invalidate */
	)
{
	int			i;

	/*-----------------------------------*/
	TPU_TRACE_D0 ("tpu_att_inval - entry");
	/*-----------------------------------*/

	for (i = 0; i < count; i++) {
#if 0
		soft->ls_aregs->a_atte[i] &= ~ATT_TABLE_ENTRY_VALID;
#else
		soft->ls_aregs->a_atte[i] = 0;
#endif
	}

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_att_inval - exit");
	/*----------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_att_load: Program the DMA address translation hardware.		*
 *										*
 ********************************************************************************/
static int
tpu_att_load
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	int			i;
	alenaddr_t		paddr;
	size_t			plength;
	int			pagecode = tpu_pagecode (soft->ls_page_size);

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_att_load - entry");
	/*----------------------------------*/

	/*
	 * Load the configuration and diagnostic registers.
	 */
	soft->ls_aregs->a_config = ATT_ENABLE_0 | ATT_ENABLE_1 | pagecode;
	soft->ls_aregs->a_diag   = 0;

	/*
	 * Load the table entries.
	 */
	alenlist_cursor_init (soft->ls_al, 0, NULL);

	for (i = 0; alenlist_get (soft->ls_al, NULL, soft->ls_page_size, &paddr, &plength, 0) == ALENLIST_SUCCESS; i++) {

		if (i >= TPU_PAGES_MAX) {
			#pragma mips_frequency_hint NEVER
			/*--------------------------------------------------*/
			TPU_TRACE_D1 ("tpu_att_load - too many pages: %d", i);
			/*--------------------------------------------------*/
			tpu_att_init (soft);
			return TPU_PHYSMAX;
		}
		else if (plength != soft->ls_page_size) {
			#pragma mips_frequency_hint NEVER
			/*-----------------------------------------------------------------------*/
			TPU_TRACE_D2 ("tpu_att_load - page %d, invalid page size: %d", i, plength);
			/*-----------------------------------------------------------------------*/
			tpu_att_init (soft);
			return TPU_PHYSCONTIG;
		}

		soft->ls_aregs->a_atte[i] = (tpu_reg_t) paddr | ATT_TABLE_ENTRY_VALID;
	}

	if (i != soft->ls_page_count) {
		#pragma mips_frequency_hint NEVER
		/*-------------------------------------------------------------------------------------------*/
		TPU_TRACE_D2 ("tpu_att_load - incorrect page count: %d, expected: %d", i, soft->ls_page_count);
		/*-------------------------------------------------------------------------------------------*/
		tpu_att_init (soft);
		return TPU_PHYSCOUNT;
	}

	TPU_EVENT_INC (&soft->ls_stat.page[pagecode], i);

	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_att_load - exit");
	/*---------------------------------*/

	return TPU_OK;
}

/********************************************************************************
 *										*
 *	tpu_ldi_init: Initialize the LDI hardware.				*
 *										*
 ********************************************************************************/
static void
tpu_ldi_init 
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_ldi_regs_t *	pLdi = soft->ls_lregs;
	int			i;

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_ldi_init - entry");
	/*----------------------------------*/

	pLdi->bFlag	   = LDI_P_OVERRIDE;
	pLdi->riscIOpcode  = 0x0000000000000000ull;		/*  0 0x2e0 */
	pLdi->riscMode	   = 0x0000000000000000ull;		/*  1 0x2e8 */
	pLdi->xRiscSadr	   = 0x0000000000000000ull;		/*  2 0x2f0 */
	pLdi->coefBs	   = 0x0000000000000000ull;		/*  3 0x2f8 */
	pLdi->coefInc	   = 0x0000000000000000ull;		/*  4 0x300 */
	pLdi->coefInit	   = 0x0000000000000000ull;		/*  5 0x308 */
	pLdi->dataBs	   = 0x0000000000000000ull;		/*  6 0x310 */
	pLdi->saBlkInc	   = 0x0000000000000000ull;		/*  7 0x318 */
	pLdi->saBlkSize	   = 0x0000000000000000ull;		/*  8 0x320 */
	pLdi->saBlkInit	   = 0x0000000000000000ull;		/*  9 0x328 */
								/* 10 0x330 Status Reg */
	pLdi->sampInit	   = 0x0000000000000000ull;		/* 11 0x338 */
	pLdi->vectInitInc  = 0x0000000000000000ull;		/* 12 0x340 */
	pLdi->vectInit	   = 0x0000000000000000ull;		/* 13 0x348 */
	pLdi->vectInc	   = 0x0000000000000000ull;		/* 14 0x350 */
	pLdi->errorMask	   = 0x0000000000000000ull;		/* 15 0x358 */
	pLdi->lbaBlkSize   = 0x0000000000000000ull;		/* 16 0x360 */
	pLdi->loopCnt	   = 0x0000000000000000ull;		/* 17 0x368 */
								/* 18 0x370 Diag Reg */
								/* 19 0x378 Source */
	pLdi->pipeInBus[0] = 0x0000000000000000ull;		/* 20 0x380 */
	pLdi->pipeInBus[1] = 0x0000000000000000ull;		/* 21 0x388 */
	pLdi->pipeInBus[2] = 0x0000000000000000ull;		/* 22 0x390 */
	pLdi->pipeInBus[3] = 0x0000000000000000ull;		/* 23 0x398 */
	pLdi->pipeInBus[4] = 0x0000000000000000ull;		/* 24 0x3a0 */
	pLdi->pipeInBus[5] = 0x0000000000000000ull;		/* 25 0x3a8 */
	pLdi->pipeInBus[6] = 0x0000000000000000ull;		/* 26 0x3b0 */
	pLdi->pipeInBus[7] = 0x0000000000000000ull;		/* 27 0x3b8 */
	pLdi->rRiscSadr	   = 0x0000000000000000ull;		/* 28 0x3c0 */
	pLdi->sampInc	   = 0x0000000000000000ull;		/* 29 0x3c8 */
	pLdi->oLineSize	   = 0x0000000000000000ull;		/* 30 0x3d0 */
	pLdi->iMask	   = LDI_S_HOST_BARRIER_DISABLE;	/* 31 0x3d8 */
	pLdi->tFlag	   = 0x0000000000000000ull;		/* 32 0x3e0 */
								/* 33 0x3e8 bFlag */
	pLdi->diagPart	   = 0xdeadbeefcafefeedull;		/* 34 0x3f0 */
	pLdi->jmpCnd	   = 0x0000000000000000ull;		/* 35 0x3f8 */

	/* 
	 * Set and clear SYS_GO LDI_CHAIN_GO = Clear xDataSadr Path 
	 */
	pLdi->bFlag	   = LDI_P_OVERRIDE | LDI_SYS_GO;
	pLdi->bFlag	   = LDI_P_OVERRIDE | LDI_CHAIN_GO;  
	pLdi->bFlag	   = LDI_P_OVERRIDE;

	for (i = 0; i < LDI_I_SIZE; i++) {
		pLdi->inst [i][LDI_I_UPPER]  = LDI_I_IBAR_WAIT_UPPER;
		pLdi->inst [i][LDI_I_LOWER]  = LDI_I_IBAR_WAIT_LOWER;
	}

	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_ldi_init - exit");
	/*---------------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_boot: Boot a chain into the LDI.					*
 *										*
 ********************************************************************************/
static void
tpu_boot
	(
	tpu_soft_t *		soft		/* tpu soft structure */
	)
{
	tpu_ldi_regs_t *	pLdi = soft->ls_lregs;
	__uint64_t		block = 36 + (LDI_I_SIZE * 2);

	/*------------------------------*/
	TPU_TRACE_D0 ("tpu_boot - entry");
	/*------------------------------*/

	/*
	 * Initial flag settings.
	 */
	pLdi->bFlag = LDI_C_ERR_STAT_CLEAR    |
			LDI_R_SYS_GO	      |
			LDI_BOOT	      |
			LDI_P_OVERRIDE	      |
			LDI_DIAG_STAT_CLEAR   |
			LDI_ERR_STAT_CLEAR    |
			LDI_SYS_GO	      |
			LDI_B_INTERRUPT_CLEAR |
			LDI_P_RESET	      |
			LDI_TIMER_STAT_CLEAR;

	/*
	 * Reinitialize the LDI registers.  Not all of this is probably
	 * necessary, but we need to do more work to find the minimum set that
	 * will allow the bootstrap DMA to work if the registers were corrupted
	 * by the previous chain.  For now, take the safe route and clear
	 * everything.
	 */
	pLdi->riscIOpcode  = 0x0000000000000000ull;		/*  0 0x2e0 */
	pLdi->riscMode	   = 0x0000000000000000ull;		/*  1 0x2e8 */
	pLdi->xRiscSadr	   = 0x0000000000000000ull;		/*  2 0x2f0 */
	pLdi->coefBs	   = 0x0000000000000000ull;		/*  3 0x2f8 */
	pLdi->coefInc	   = 0x0000000000000000ull;		/*  4 0x300 */
	pLdi->coefInit	   = 0x0000000000000000ull;		/*  5 0x308 */
	pLdi->dataBs	   = 0x0000000000000000ull;		/*  6 0x310 */
	pLdi->saBlkInc	   = 0x0000000000000000ull;		/*  7 0x318 */
	pLdi->saBlkSize	   = 0x0000000000000000ull;		/*  8 0x320 */
	pLdi->saBlkInit	   = 0x0000000000000000ull;		/*  9 0x328 */
								/* 10 0x330 Status Reg */
	pLdi->sampInit	   = 0x0000000000000000ull;		/* 11 0x338 */
	pLdi->vectInitInc  = 0x0000000000000000ull;		/* 12 0x340 */
	pLdi->vectInit	   = 0x0000000000000000ull;		/* 13 0x348 */
	pLdi->vectInc	   = 0x0000000000000000ull;		/* 14 0x350 */
	pLdi->errorMask	   = 0x0000000000000000ull;		/* 15 0x358 */
	pLdi->lbaBlkSize   = 0x0000000000000000ull;		/* 16 0x360 */
	pLdi->loopCnt	   = 0x0000000000000000ull;		/* 17 0x368 */
								/* 18 0x370 Diag Reg */
								/* 19 0x378 Source */
	pLdi->pipeInBus[0] = 0x0000000000000000ull;		/* 20 0x380 */
	pLdi->pipeInBus[1] = 0x0000000000000000ull;		/* 21 0x388 */
	pLdi->pipeInBus[2] = 0x0000000000000000ull;		/* 22 0x390 */
	pLdi->pipeInBus[3] = 0x0000000000000000ull;		/* 23 0x398 */
	pLdi->pipeInBus[4] = 0x0000000000000000ull;		/* 24 0x3a0 */
	pLdi->pipeInBus[5] = 0x0000000000000000ull;		/* 25 0x3a8 */
	pLdi->pipeInBus[6] = 0x0000000000000000ull;		/* 26 0x3b0 */
	pLdi->pipeInBus[7] = 0x0000000000000000ull;		/* 27 0x3b8 */
	pLdi->rRiscSadr	   = 0x0000000000000000ull;		/* 28 0x3c0 */
	pLdi->sampInc	   = 0x0000000000000000ull;		/* 29 0x3c8 */
	pLdi->oLineSize	   = 0x0000000000000000ull;		/* 30 0x3d0 */
	pLdi->iMask	   = LDI_S_HOST_BARRIER_DISABLE;	/* 31 0x3d8 */
	pLdi->tFlag        = 0x0000000000000000ull;		/* 32 0x3e0 */
								/* 33 0x3e8 bFlag */
	pLdi->diagPart	   = 0xdeadbeefcafefeedull;		/* 34 0x3f0 */
	pLdi->jmpCnd	   = 0x0000000000000000ull;		/* 35 0x3f8 */

	/*
	 * Load the bootstrap DMA instruction.
	 */
	pLdi->inst[0][LDI_I_UPPER] = (LDI_I_CODE       (LDI_ID_DMA_RESET) |
					LDI_I_XSEL     (LDI_IX_INST)	  |
					LDI_I_MASK     (LDI_IM_X_DONE)	  |
					LDI_I_REG_ADDR (92)		  );

	pLdi->inst[0][LDI_I_LOWER] = (LDI_I_HOST_ADDR  (soft->ls_ldi_offset) | 
					LDI_I_BLOCK    (block)		     );

	/*
	 * Start the sequencer.
	 */
	pLdi->bFlag = LDI_P_OVERRIDE | LDI_BOOT;
	pLdi->bFlag = LDI_BOOT;
	pLdi->bFlag = LDI_R_SYS_GO;
	pLdi->bFlag = 0;

	if (tpu_simulated (soft))
		soft->ls_sim_id = itimeout (tpu_ldiBarrier_intr, (void *) soft, TPU_SIM_TICKS, pltimeout);

	/*-----------------------------*/
	TPU_TRACE_D0 ("tpu_boot - exit");
	/*-----------------------------*/
}

/********************************************************************************
 *										*
 *	tpu_dma_leds: Enable the external test pins (LED drivers) on the	*
 *			DMA ASIC.						*
 *										*
 ********************************************************************************/
static void
tpu_dma_leds
	(
	tpu_soft_t *		soft,		/* tpu soft structure */
	widgetreg_t		value		/* new register value */
	)
{
	/*-----------------------------------------------*/
	TPU_TRACE_D1 ("tpu_dma_leds - value: 0x%x", value);
	/*-----------------------------------------------*/

	soft->ls_xregs->x_leds = value;
}

/********************************************************************************
 *										*
 *	tpu_get_regs - Fill in a tpu_regs argument structure.			*
 *										*
 ********************************************************************************/
static void
tpu_get_regs
	(
	tpu_soft_t *		soft,		/* tpu soft structure */
	tpud_regs_t *		s		/* tpu_regs argument structure */
	)
{
	int			i;

	/*
	 * Crosstalk registers.
	 */
	s->xtk_regs.id			= soft->ls_xregs->x_id;	
	s->xtk_regs.status		= soft->ls_xregs->x_status;
	s->xtk_regs.err_upper_addr	= soft->ls_xregs->x_err_upper_addr;
	s->xtk_regs.err_lower_addr	= soft->ls_xregs->x_err_lower_addr;
	s->xtk_regs.control		= soft->ls_xregs->x_control;
	s->xtk_regs.req_timeout		= soft->ls_xregs->x_req_timeout;
	s->xtk_regs.intdest_upper_addr	= soft->ls_xregs->x_intdest_upper_addr;
	s->xtk_regs.intdest_lower_addr	= soft->ls_xregs->x_intdest_lower_addr;
	s->xtk_regs.err_cmd_word	= soft->ls_xregs->x_err_cmd_word;
	s->xtk_regs.llp_cfg		= soft->ls_xregs->x_llp_cfg;
	s->xtk_regs.ede			= soft->ls_xregs->x_ede;
	s->xtk_regs.int_status		= soft->ls_xregs->x_int_status;
	s->xtk_regs.int_enable		= soft->ls_xregs->x_int_enable;
	s->xtk_regs.int_addr0		= soft->ls_xregs->x_int_addr0;
	s->xtk_regs.int_addr1		= soft->ls_xregs->x_int_addr1;
	s->xtk_regs.int_addr2		= soft->ls_xregs->x_int_addr2;
	s->xtk_regs.int_addr3		= soft->ls_xregs->x_int_addr3;
	s->xtk_regs.int_addr4		= soft->ls_xregs->x_int_addr4;
	s->xtk_regs.int_addr5		= soft->ls_xregs->x_int_addr5;
	s->xtk_regs.int_addr6		= soft->ls_xregs->x_int_addr6;
	s->xtk_regs.sense		= soft->ls_xregs->x_sense;
	s->xtk_regs.leds		= soft->ls_xregs->x_leds;

	/*
	 * Address translation registers.
	 */
	s->att_regs.config		= soft->ls_aregs->a_config;
	s->att_regs.diag		= soft->ls_aregs->a_diag;
	for (i = 0; i < TPU_ATT_SIZE; i++)
		s->att_regs.atte[i]	= soft->ls_aregs->a_atte[i];

	/*
	 * DMA 0 registers.
	 */
	s->dma0_regs.config0		= soft->ls_d0regs->d_config0;
	s->dma0_regs.config1		= soft->ls_d0regs->d_config1;
	s->dma0_regs.status		= soft->ls_d0regs->d_status;
	s->dma0_regs.diag_mode		= soft->ls_d0regs->d_diag_mode;
	s->dma0_regs.perf_timer		= soft->ls_d0regs->d_perf_timer;
	s->dma0_regs.perf_count		= soft->ls_d0regs->d_perf_count;
	s->dma0_regs.perf_config	= soft->ls_d0regs->d_perf_config;

	/*
	 * DMA 1 registers.
	 */
	s->dma1_regs.config0		= soft->ls_d1regs->d_config0;
	s->dma1_regs.config1		= soft->ls_d1regs->d_config1;
	s->dma1_regs.status		= soft->ls_d1regs->d_status;
	s->dma1_regs.diag_mode		= soft->ls_d1regs->d_diag_mode;
	s->dma1_regs.perf_timer		= soft->ls_d1regs->d_perf_timer;
	s->dma1_regs.perf_count		= soft->ls_d1regs->d_perf_count;
	s->dma1_regs.perf_config	= soft->ls_d1regs->d_perf_config;

	/*
	 * LDI registers.
	 */
	s->ldi_regs.riscIOpcode		= soft->ls_lregs->riscIOpcode;
	s->ldi_regs.riscIOpcode		= soft->ls_lregs->riscIOpcode;
	s->ldi_regs.riscMode		= soft->ls_lregs->riscMode;
	s->ldi_regs.xRiscSadr		= soft->ls_lregs->xRiscSadr;
	s->ldi_regs.coefBs		= soft->ls_lregs->coefBs;
	s->ldi_regs.coefInc		= soft->ls_lregs->coefInc;
	s->ldi_regs.coefInit		= soft->ls_lregs->coefInit;
	s->ldi_regs.dataBs		= soft->ls_lregs->dataBs;
	s->ldi_regs.saBlkInc		= soft->ls_lregs->saBlkInc;
	s->ldi_regs.saBlkSize		= soft->ls_lregs->saBlkSize;
	s->ldi_regs.saBlkInit		= soft->ls_lregs->saBlkInit;
	s->ldi_regs.statusReg		= soft->ls_lregs->statusReg;
	s->ldi_regs.sampInit		= soft->ls_lregs->sampInit;
	s->ldi_regs.vectInitInc		= soft->ls_lregs->vectInitInc;
	s->ldi_regs.vectInit		= soft->ls_lregs->vectInit;
	s->ldi_regs.vectInc		= soft->ls_lregs->vectInc;
	s->ldi_regs.errorMask		= soft->ls_lregs->errorMask;
	s->ldi_regs.lbaBlkSize		= soft->ls_lregs->lbaBlkSize;
	s->ldi_regs.loopCnt		= soft->ls_lregs->loopCnt;
	s->ldi_regs.diagReg		= soft->ls_lregs->diagReg;
	s->ldi_regs.iSource		= soft->ls_lregs->iSource;
	s->ldi_regs.pipeInBus[0]	= soft->ls_lregs->pipeInBus[0];
	s->ldi_regs.pipeInBus[1]	= soft->ls_lregs->pipeInBus[1];
	s->ldi_regs.pipeInBus[2]	= soft->ls_lregs->pipeInBus[2];
	s->ldi_regs.pipeInBus[3]	= soft->ls_lregs->pipeInBus[3];
	s->ldi_regs.pipeInBus[4]	= soft->ls_lregs->pipeInBus[4];
	s->ldi_regs.pipeInBus[5]	= soft->ls_lregs->pipeInBus[5];
	s->ldi_regs.pipeInBus[6]	= soft->ls_lregs->pipeInBus[6];
	s->ldi_regs.pipeInBus[7]	= soft->ls_lregs->pipeInBus[7];
	s->ldi_regs.rRiscSadr		= soft->ls_lregs->rRiscSadr;
	s->ldi_regs.sampInc		= soft->ls_lregs->sampInc;
	s->ldi_regs.oLineSize		= soft->ls_lregs->oLineSize;
	s->ldi_regs.iMask		= soft->ls_lregs->iMask;
	s->ldi_regs.tFlag		= soft->ls_lregs->tFlag;
	s->ldi_regs.bFlag		= soft->ls_lregs->bFlag;
	s->ldi_regs.diagPart		= soft->ls_lregs->diagPart;
	s->ldi_regs.jmpCnd		= soft->ls_lregs->jmpCnd;
}

/********************************************************************************
 *										*
 *	tpu_get_inst - Fill in a tpu_inst argument structure.			*
 *										*
 ********************************************************************************/
static void
tpu_get_inst
	(
	tpu_soft_t *		soft,		/* tpu soft structure */
	tpud_inst_t *		s		/* tpu_inst argument structure */
	)
{
	int			i;
	tpu_reg_t		bFlag;
	tpu_reg_t		xRiscSadr;

	/*----------------------------------*/
	TPU_TRACE_D0 ("tpu_get_inst - entry");
	/*----------------------------------*/

	bFlag =  soft->ls_lregs->bFlag;
	xRiscSadr =  soft->ls_lregs->xRiscSadr;

	soft->ls_lregs->bFlag = bFlag | LDI_P_OVERRIDE;
	soft->ls_lregs->xRiscSadr = xRiscSadr | LDI_S_READ_BACK;

	for (i = 0; i < TPU_LDI_I_SIZE; i++) {
		s->inst[i][0] = soft->ls_lregs->inst[i][0];
		s->inst[i][1] = soft->ls_lregs->inst[i][1];
	}

	soft->ls_lregs->xRiscSadr = xRiscSadr;
	soft->ls_lregs->bFlag = bFlag;

	/*---------------------------------*/
	TPU_TRACE_D0 ("tpu_get_inst - exit");
	/*---------------------------------*/
}


/* =====================================================================
 *	INTERRUPT HANDLERS
 *
 *	IRIX will look for (and not find) tpu_intr; generally, the interrupt
 *	requirements for crosstalk widgets are more complex (besides, someone
 *	needs to set up the hardware).
 *
 *	xtalk infrastructure clients explicitly notify the infrastructure of
 *	their interrupt needs as each device instance is attached.
 */

/********************************************************************************
 *										*
 *	tpu_timeout: timeout interrupt handler.					*
 *										*
 ********************************************************************************/
static void
tpu_timeout
	(
	intr_arg_t	arg
	)
{
	tpu_soft_t *	soft = (tpu_soft_t *) arg;

	TPU_LOCK (soft);

	TPU_EVENT (&soft->ls_stat.timeout);

	/*-------------------------*/
	TPU_TRACE_D0 ("tpu_timeout");
	/*-------------------------*/

	/*
	 * Check for a race condition between the timeout and the device
	 * interrupt, halt-io ioctl, or signal.  If the device isn't active,
	 * just return without doing anything.
	 */
	if (soft->ls_state != TPU_RUNNING) {
		/*-----------------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_timeout - no run active, ls_state: %d", soft->ls_state);
		/*-----------------------------------------------------------------------*/
		TPU_UNLOCK (soft);
		return;
	}

	/*
	 * Update the device state and ending status.
	 */
	soft->ls_state = TPU_TIMEDOUT;
	soft->ls_status = TPU_TIMEOUT;

	/*
	 * Stop the hardware.
	 */
	tpu_reset (soft);

	/*-------------------------------------------------*/
	TPU_TRACE_D0 ("tpu_timeout - wake the user process");
	/*-------------------------------------------------*/

	/*
	 * Wake up the sleeping process.
	 */
	TPU_WAKEUP (soft);
	TPU_UNLOCK (soft);
}

/********************************************************************************
 *										*
 *	tpu_dmaError_intr: xtalk widget error interrupt handler.		*
 *										*
 *	Currently, this interrupt is simply logged without affecting the 	*
 *	state of the device.							*
 *										*
 ********************************************************************************/
static void
tpu_dmaError_intr
	(
	intr_arg_t	arg
	)
{
	tpu_soft_t *	soft = (tpu_soft_t *) arg;

	TPU_LOCK (soft);

	/*
	 * Save status and acknowledge the interrupt.
	 */
	soft->ls_dmaErrorIntStatus = soft->ls_xregs->x_int_status;
	soft->ls_dmaErrorD0Status  = soft->ls_d0regs->d_status;
	soft->ls_dmaErrorD1Status  = soft->ls_d1regs->d_status;

	soft->ls_xregs->x_int_status_reset = soft->ls_dmaErrorIntStatus;

	TPU_EVENT (&soft->ls_stat.dmaError);

	/*---------------------------------------------------------------------------------------*/
	TPU_TRACE_D3 ("tpu_dmaError_intr - int status: 0x%x, dma0 status: 0x%x, dma1 status: 0x%x", 
			soft->ls_dmaErrorIntStatus,
			soft->ls_dmaErrorD0Status,
			soft->ls_dmaErrorD1Status);
	/*---------------------------------------------------------------------------------------*/

	TPU_UNLOCK (soft);

	cmn_err (CE_WARN, "tpu: %v - DMA error interrupt\n", soft->ls_vhdl);
}

/********************************************************************************
 *										*
 *	tpu_ldiBarrier_intr: ibar_wait interrupt handler.			*
 *										*
 ********************************************************************************/
static void
tpu_ldiBarrier_intr
	(
	intr_arg_t	arg
	)
{
	tpu_soft_t *	soft = (tpu_soft_t *) arg;

	TPU_LOCK (soft);

	/*
	 * Save status and acknowledge the interrupt.
	 */
	soft->ls_ldiBarrierIntStatus = soft->ls_xregs->x_int_status;
	soft->ls_ldiBarrierLdiStatus = soft->ls_lregs->statusReg;
	soft->ls_ldiBarrierLdiSource = soft->ls_lregs->iSource;

	soft->ls_lregs->bFlag |= LDI_B_INTERRUPT_CLEAR;
	soft->ls_lregs->bFlag ^= LDI_B_INTERRUPT_CLEAR;

	soft->ls_xregs->x_int_status_reset = 1;

	TPU_EVENT (&soft->ls_stat.ldiBarrier);

	/*---------------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_ldiBarrier_intr - int status: 0x%x, LDI source: 0x%x", 
			soft->ls_ldiBarrierIntStatus,
			soft->ls_ldiBarrierLdiSource);
	/*---------------------------------------------------------------------*/

	/*
	 * Check for a spurrious interrupt or a race condition between the
	 * interrupt and a timeout, halt-io ioctl, or signal.  If the device
	 * isn't active, just return without doing anything.
	 */
	if (soft->ls_state != TPU_RUNNING) {
		#pragma mips_frequency_hint NEVER
		/*--------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ldiBarrier_intr - no run active, ls_state: %d", 
				soft->ls_state);
		/*--------------------------------------------------------------*/
		TPU_UNLOCK (soft);
		return;
	}

	/*
	 * Update the device state.
	 */
	soft->ls_state = TPU_INTERRUPTED_B;

	/*
	 * Stop the timeout clock.
	 */
	untimeout (soft->ls_timeout_id);

	/*
	 * Copy interrupt status information for return to the user.
	 */
	soft->ls_ldiIntStatus = soft->ls_ldiBarrierIntStatus;
	soft->ls_ldiLdiStatus = soft->ls_ldiBarrierLdiStatus; 
	soft->ls_ldiLdiSource = soft->ls_ldiBarrierLdiSource;
	soft->ls_ldiD0Status = soft->ls_d0regs->d_status;
	soft->ls_ldiD1Status = soft->ls_d1regs->d_status;
	soft->ls_dma0Count = soft->ls_d0regs->d_perf_count;
	soft->ls_dma0Ticks = soft->ls_d0regs->d_perf_timer;
	soft->ls_dma1Count = soft->ls_d1regs->d_perf_count;
	soft->ls_dma1Ticks = soft->ls_d1regs->d_perf_timer;

	/*
	 * Update the DMA statistics.
	 */
	soft->ls_stat.dma0Count += soft->ls_dma0Count;
	soft->ls_stat.dma0Ticks += soft->ls_dma0Ticks;
	soft->ls_stat.dma1Count += soft->ls_dma1Count;
	soft->ls_stat.dma1Ticks += soft->ls_dma1Ticks;

	/*---------------------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ldiBarrier_intr - wake the user process");
	/*---------------------------------------------------------*/

	/*
	 * Wake up the sleeping process.
	 */
	TPU_WAKEUP (soft);
	TPU_UNLOCK (soft);
}

/********************************************************************************
 *										*
 *	tpu_ldiError_intr: LDI error interrupt handler.				*
 *										*
 ********************************************************************************/
static void
tpu_ldiError_intr
	(
	intr_arg_t	arg
	)
{
	tpu_soft_t *	soft = (tpu_soft_t *) arg;

	TPU_LOCK (soft);

	/*
	 * Save status, reset the TPU, and acknowledge the interrupt.  This type
	 * of interrupt can continue to reoccur as long as the LDI continues to
	 * run, so we need to stop the LDI to prevent a hot interrupt condition.
	 */
	soft->ls_ldiErrorIntStatus = soft->ls_xregs->x_int_status;
	soft->ls_ldiErrorLdiStatus = soft->ls_lregs->statusReg;

	TPU_EVENT (&soft->ls_stat.ldiError);

	/*-------------------------------------------------------------------*/
	TPU_TRACE_D2 ("tpu_ldiError_intr - int status: 0x%x, LDI status: 0x%x", 
			soft->ls_ldiErrorIntStatus,
			soft->ls_ldiErrorLdiStatus);
	/*-------------------------------------------------------------------*/

	tpu_reset (soft);

	soft->ls_lregs->bFlag |= LDI_ERR_STAT_CLEAR;
	soft->ls_lregs->bFlag ^= LDI_ERR_STAT_CLEAR;

	soft->ls_xregs->x_int_status_reset = 1<<1;

	/*
	 * Check for a spurrious interrupt or a race condition between the
	 * interrupt and a timeout, halt-io ioctl, or signal.  If the device
	 * isn't active, just return without doing anything.
	 */
	if (soft->ls_state != TPU_RUNNING) {
		/*------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ldiError_intr - no run active, ls_state: %d", 
				soft->ls_state);
		/*------------------------------------------------------------*/
		TPU_UNLOCK (soft);
		return;
	}

	/*
	 * Update the device state and ending status.
	 */
	soft->ls_state = TPU_INTERRUPTED_L;
	soft->ls_status = TPU_LERROR;

	/*
	 * Stop the timeout clock.
	 */
	untimeout (soft->ls_timeout_id);

	/*
	 * Copy interrupt status information for return to the user.
	 */
	soft->ls_ldiIntStatus = soft->ls_ldiCErrorIntStatus;
	soft->ls_ldiLdiStatus = soft->ls_ldiCErrorLdiStatus;

	/*-------------------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ldiError_intr - wake the user process");
	/*-------------------------------------------------------*/

	/*
	 * Wake up the sleeping process.
	 */
	TPU_WAKEUP (soft);
	TPU_UNLOCK (soft);
}

/********************************************************************************
 *										*
 *	tpu_ldiCError_intr: LDI channel error interrupt handler.		*
 *										*
 ********************************************************************************/
static void
tpu_ldiCError_intr
	(
	intr_arg_t	arg
	)
{
	tpu_soft_t *	soft = (tpu_soft_t *) arg;

	TPU_LOCK (soft);

	/*
	 * Save status and acknowledge the interrupt.
	 */
	soft->ls_ldiCErrorIntStatus = soft->ls_xregs->x_int_status;
	soft->ls_ldiCErrorLdiStatus = soft->ls_lregs->statusReg;
	soft->ls_ldiCErrorD0Status  = soft->ls_d0regs->d_status;
	soft->ls_ldiCErrorD1Status  = soft->ls_d1regs->d_status;

	soft->ls_lregs->bFlag |= LDI_C_ERR_STAT_CLEAR;
	soft->ls_lregs->bFlag ^= LDI_C_ERR_STAT_CLEAR;

	soft->ls_xregs->x_int_status_reset = soft->ls_ldiCErrorIntStatus;

	TPU_EVENT (&soft->ls_stat.ldiCError);

	/*----------------------------------------------------------------------------------------------------------*/
	TPU_TRACE_D4 ("tpu_ldiCError_intr - int status: 0x%x, LDI status: 0x%x, dma0 status: 0x%x, dma1 status: 0x%x", 
			soft->ls_ldiCErrorIntStatus,
			soft->ls_ldiCErrorLdiStatus,
			soft->ls_ldiCErrorD0Status,
			soft->ls_ldiCErrorD1Status);
	/*----------------------------------------------------------------------------------------------------------*/

	/*
	 * Check for a spurrious interrupt or a race condition between the
	 * interrupt and a timeout, halt-io ioctl, or signal.  If the device
	 * isn't active, just return without doing anything.
	 */
	if (soft->ls_state != TPU_RUNNING) {
		/*-------------------------------------------------------------*/
		TPU_TRACE_D1 ("tpu_ldiCError_intr - no run active, ls_state: %d", 
				soft->ls_state);
		/*-------------------------------------------------------------*/
		TPU_UNLOCK (soft);
		return;
	}

	/*
	 * Update the device state and ending status.
	 */
	soft->ls_state = TPU_INTERRUPTED_C;
	soft->ls_status = TPU_CERROR;

	/*
	 * Stop the timeout clock.
	 */
	untimeout (soft->ls_timeout_id);

	/*
	 * Copy interrupt status information for return to the user.
	 */
	soft->ls_ldiIntStatus = soft->ls_ldiCErrorIntStatus;
	soft->ls_ldiLdiStatus = soft->ls_ldiCErrorLdiStatus;
	soft->ls_ldiLdiSource = 0;				/* unused */
	soft->ls_ldiD0Status = soft->ls_ldiCErrorD0Status;
	soft->ls_ldiD1Status = soft->ls_ldiCErrorD1Status;
	soft->ls_dma0Count = soft->ls_d0regs->d_perf_count;
	soft->ls_dma0Ticks = soft->ls_d0regs->d_perf_timer;
	soft->ls_dma1Count = soft->ls_d1regs->d_perf_count;
	soft->ls_dma1Ticks = soft->ls_d1regs->d_perf_timer;

	/*
	 * Update the DMA statistics.
	 */
	soft->ls_stat.dma0Count += soft->ls_dma0Count;
	soft->ls_stat.dma0Ticks += soft->ls_dma0Ticks;
	soft->ls_stat.dma1Count += soft->ls_dma1Count;
	soft->ls_stat.dma1Ticks += soft->ls_dma1Ticks;

	/*--------------------------------------------------------*/
	TPU_TRACE_D0 ("tpu_ldiCError_intr - wake the user process");
	/*--------------------------------------------------------*/

	/*
	 * Wake up the sleeping process.
	 */
	TPU_WAKEUP (soft);
	TPU_UNLOCK (soft);
}


/* =====================================================================
 *	INTERRUPT DESTINATION SELECTION
 *
 *	It may be necessary to migrate xtalk widget interrupts around the
 *	system; without provocation from the driver, the infrastructure could
 *	call our setfunc entry points, presented to it at attach time.
 */

/********************************************************************************
 *										*
 *	tpu_dmaError_setfunc: set the DMA error interrupt target.		*
 *										*
 *	This function also sets the XBOW port that DMA data will be sent out,	*
 *	using the rule that the DMA data will follow the same path as the	*
 *	interrupt.								*
 *										*
 ********************************************************************************/
static int
tpu_dmaError_setfunc
	(
	xtalk_intr_t	intr
	)
{
	xwidgetnum_t		targ = xtalk_intr_target_get (intr);
	iopaddr_t		addr = xtalk_intr_addr_get (intr);
	xtalk_intr_vector_t	vect = xtalk_intr_vector_get (intr);
	tpu_soft_t *		soft = (tpu_soft_t *) xtalk_intr_sfarg_get (intr);

	soft->ls_xregs->x_intdest_upper_addr =
		(0xff000000 & (vect << 24)) |
		(0x000f0000 & (targ << 16)) |
		(0x0000ffff & (addr >> 32)) ;

	soft->ls_xregs->x_intdest_lower_addr = 
		  0xffffffff & addr;

	soft->ls_d0regs->d_config0 = 
		(soft->ls_d0regs->d_config0 & ~0xf0000) | ((targ << 16) & 0xf0000);
	soft->ls_d0regs->d_config1 = 
		(soft->ls_d0regs->d_config1 & ~0xf0000) | ((targ << 16) & 0xf0000);

	soft->ls_d1regs->d_config0 = 
		(soft->ls_d1regs->d_config0 & ~0xf0000) | ((targ << 16) & 0xf0000);
	soft->ls_d1regs->d_config1 = 
		(soft->ls_d1regs->d_config1 & ~0xf0000) | ((targ << 16) & 0xf0000);

	/*----------------------------------------------------------------------------*/
	TPU_TRACE_D3 ("tpu_dmaError_setfunc - widget: 0x%x, offset: 0x%x, vector: 0x%x",
			targ, addr, vect);
	/*----------------------------------------------------------------------------*/

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ldiBarrier_setfunc: set the LDI barrier interrupt target.		*
 *										*
 ********************************************************************************/
static int
tpu_ldiBarrier_setfunc
	(
	xtalk_intr_t	intr
	)
{
	xwidgetnum_t		targ = xtalk_intr_target_get (intr);
	iopaddr_t		addr = xtalk_intr_addr_get (intr);
	xtalk_intr_vector_t	vect = xtalk_intr_vector_get (intr);
	tpu_soft_t *		soft = (tpu_soft_t *) xtalk_intr_sfarg_get (intr);

	soft->ls_xregs->x_int_addr0 =
		(0x00ffff00 & (addr >> 24)) |
		(0x000000ff &  vect       ) ;

	/*------------------------------------------------------------------------------*/
	TPU_TRACE_D3 ("tpu_ldiBarrier_setfunc - widget: 0x%x, offset: 0x%x, vector: 0x%x",
			targ, addr, vect);
	/*------------------------------------------------------------------------------*/

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ldiError_setfunc: set the LDI error interrupt target.		*
 *										*
 ********************************************************************************/
static int
tpu_ldiError_setfunc
	(
	xtalk_intr_t	intr
	)
{
	xwidgetnum_t		targ = xtalk_intr_target_get (intr);
	iopaddr_t		addr = xtalk_intr_addr_get (intr);
	xtalk_intr_vector_t	vect = xtalk_intr_vector_get (intr);
	tpu_soft_t *		soft = (tpu_soft_t *) xtalk_intr_sfarg_get (intr);

	soft->ls_xregs->x_int_addr1 =
		(0x00ffff00 & (addr >> 24)) |
		(0x000000ff &  vect       ) ;

	/*----------------------------------------------------------------------------*/
	TPU_TRACE_D3 ("tpu_ldiError_setfunc - widget: 0x%x, offset: 0x%x, vector: 0x%x",
			targ, addr, vect);
	/*----------------------------------------------------------------------------*/

	return 0;
}

/********************************************************************************
 *										*
 *	tpu_ldiCError_setfunc: set the LDI channel error interrupt target.	*
 *										*
 ********************************************************************************/
static int
tpu_ldiCError_setfunc
	(
	xtalk_intr_t	intr
	)
{
	xwidgetnum_t		targ = xtalk_intr_target_get (intr);
	iopaddr_t		addr = xtalk_intr_addr_get (intr);
	xtalk_intr_vector_t	vect = xtalk_intr_vector_get (intr);
	tpu_soft_t *		soft = (tpu_soft_t *) xtalk_intr_sfarg_get (intr);

	soft->ls_xregs->x_int_addr2 =
		(0x00ffff00 & (addr >> 24)) |
		(0x000000ff &  vect       ) ;

	/*-----------------------------------------------------------------------------*/
	TPU_TRACE_D3 ("tpu_ldiCError_setfunc - widget: 0x%x, offset: 0x%x, vector: 0x%x",
			targ, addr, vect);
	/*-----------------------------------------------------------------------------*/

	return 0;
}


/* ==============================================================================
 *	 TRACE FUNCTIONS
 */

/********************************************************************************
 *										*
 *	tpu_trace_init: Initialize tpu driver tracing.				*
 *										*
 ********************************************************************************/
static void
tpu_trace_init (void)
{
	tpu_global_trace_buffer = tpu_trace_alloc ();
}

/********************************************************************************
 *										*
 *	tpu_trace_alloc: Allocate a trace buffer.				*
 *										*
 ********************************************************************************/
static tpud_traceb_t *
tpu_trace_alloc (void)
{
	tpud_traceb_t *	b;

	if ((b = (tpud_traceb_t *) kmem_alloc (sizeof (tpud_traceb_t), KM_SLEEP)) != NULL) {
		b->magic = TPU_M_TRACE;
		b->count = 0;
		b->create_sec = (__int64_t) time;
		b->create_rtc = absolute_rtc_current_time ();
		b->rtc_nsec = timer_unit;
	}

	return (b);
}

/********************************************************************************
 *										*
 *	tpu_trace: Make a trace entry.						*
 *										*
 ********************************************************************************/
static __uint64_t *
tpu_trace
	(
	tpud_traceb_t *	b,
	char *		fmt
	)
{
	int		index;

	index = (int) b->count++ & (TPU_TRACEB_LEN-1);

	b->e[index].cpu = cpuid ();
	b->e[index].fmt = fmt;
	b->e[index].timestamp = absolute_rtc_current_time ();

	return (&(b->e[index].param[0]));
}

/********************************************************************************
 *										*
 *	tpu_trace_copyout: Export a trace buffer to user space.			*
 *										*
 ********************************************************************************/
static int
tpu_trace_copyout
	(
	tpud_traceb_t *	tb,
	tpud_tracex_t *	ub
	)
{
	int		i;

	if (tb == NULL)
		return EIO;

	if (copyout ((caddr_t) tb, (caddr_t) ub, sizeof (tpud_traceb_t)))
		return EFAULT;

	for (i = 0; (i < TPU_TRACEB_LEN) && (i < tb->count); i++)
		if (copyout ((caddr_t) tb->e[i].fmt, (caddr_t) &(ub->f[i][0]), strlen (tb->e[i].fmt) + 1))
			return EFAULT;

	return 0;
}
