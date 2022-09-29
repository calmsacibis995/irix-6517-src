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
 * tpusim.c - Mesa TPU hardware simulator driver.				*
 *										*
 * The following parameters may be set by DRIVER_ADMIN lines in the file	*
 * /var/sysgen/system/irix.sm:							*
 *										*
 * Number of simulated TPUs:							*
 *										*
 *	DRIVER_ADMIN: tpusim_ NDEV=n						*
 *										*
 ********************************************************************************/

#ident	"$Id: tpusim.c,v 1.4 1999/04/29 19:32:28 pww Exp $"

#include <sys/types.h>

#include <ksys/ddmap.h>
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
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <sys/iobus.h>
#include <sys/ioctl.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/poll.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/tpudrv.h>
#include <sys/xtalk/xtalk_private.h>

#if SN0
#include <sys/SN/klconfig.h>
#endif

extern int		tpu_attach (vertex_hdl_t);

/* ==============================================================================
 *		 MACROS
 */

/*
 * Object creation/deletion.
 */
#define	NEW(ptr)		(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)		(kmem_free(ptr, sizeof (*(ptr))))

/*
 * Limits.
 */
#define	MAXSIMDEVS		256		/* max number of simulated TPUs */

/* ==============================================================================
 *		 Device-Related Constants and Structures
 */

/*
 *	Per-device software state.
 */
typedef struct tpusim_soft_s {
	uint_t			lss_magic;	/* magic number */
	uint_t			lss_size;	/* sizeof tpusim_soft_t */

	vertex_hdl_t		lss_conn;	/* parent hwgraph vertex */
	vertex_hdl_t		lss_vhdl;	/* our device vertex */

	tpu_xtk_regs_t *	lss_xregs;	/* simulated xtalk widget registers */
	tpu_att_regs_t *	lss_aregs;	/* simulated address translation registers */
	tpu_dma_regs_t *	lss_d0regs;	/* simulated dma 0 registers */
	tpu_dma_regs_t *	lss_d1regs;	/* simulated dma 1 registers */
	tpu_ldi_regs_t *	lss_lregs;	/* simulated ldi registers */

} tpusim_soft_t;

#define tpusim_soft_set(v,i)	hwgraph_fastinfo_set ((v), (arbitrary_info_t) (i))
#define tpusim_soft_get(v)	((tpusim_soft_t *) hwgraph_fastinfo_get ((v)))


/* ==============================================================================
 *		Function Table of Contents
 */

extern const char *	tpusim_version (void);

extern void		tpusim_init (void);
extern int		tpusim_reg (void);
extern int		tpusim_unreg (void);
extern int		tpusim_attach (vertex_hdl_t);

extern int		tpusim_open (dev_t *, int, int, cred_t *);
extern int		tpusim_close (dev_t, int, int, cred_t *);

extern int		tpusim_map (dev_t, vhandl_t *, off_t, size_t, uint_t);
extern int		tpusim_unmap (dev_t, vhandl_t *);

extern int		tpusim_ioctl (dev_t, int, void *, int, cred_t *, int *);

caddr_t			tpusim_info_chk (vertex_hdl_t);

caddr_t			tpusim_piotrans_addr (vertex_hdl_t, device_desc_t, iopaddr_t, size_t, unsigned);
alenlist_t		tpusim_dmatrans_list (vertex_hdl_t, device_desc_t, alenlist_t, unsigned);
xtalk_intr_t		tpusim_intr_alloc (vertex_hdl_t, device_desc_t, vertex_hdl_t);
int			tpusim_intr_connect (xtalk_intr_t, intr_func_t, intr_arg_t, xtalk_intr_setfunc_t, void *, void *);
void			tpusim_intr_disconnect (xtalk_intr_t);

static void *		tpusim_pgalloc (int);


/* ==============================================================================
 *		 GLOBALS
 */

int			tpusim_devflag = D_MP;
static u_int		tpusim_devs = 8;	/* number of simulated devices */


/* ==============================================================================
 *		Version function
 */

const char *
tpusim_version (void)
{
	return ("$Revision: 1.4 $ Built: " __DATE__ " " __TIME__);
}

/* ==============================================================================
 *		Driver Initialization
 */

/********************************************************************************
 *										*
 *	tpusim_init: called once during system startup or when a loadable	*
 *	driver is loaded.							*
 *										*
 ********************************************************************************/
void
tpusim_init (void)
{
}

/********************************************************************************
 *										*
 *	tpusim_reg: called once during system startup or when a loadable	*
 *	driver is loaded.							*
 *										*
 ********************************************************************************/
int
tpusim_reg (void)
{
	int		i;
	char		name[32];
	char *		ndev_s;
	u_int		ndev_i;
	graph_error_t	status;
	vertex_hdl_t	vhdl_top;
	vertex_hdl_t	vhdl_mid;
	vertex_hdl_t	vhdl_sub[MAXSIMDEVS];

	/*
	 * Get the number of devices to simulate from a "DRIVER_ADMIN: NDEV="
	 * directive in irix.sm.
	 */
	if ((ndev_s = device_driver_admin_info_get ("tpusim_", "NDEV")) != NULL) {
		ndev_i = atoi (ndev_s);
		tpusim_devs = (ndev_i < MAXSIMDEVS) ? ndev_i : MAXSIMDEVS;
	}

	cmn_err (CE_DEBUG, "Starting TPU simulator with %d devices\n", tpusim_devs);

	/*
	 * Create the base /hw/tpusim simulator vertex.
	 */
	if ((status = hwgraph_vertex_create (&vhdl_top)) != GRAPH_SUCCESS) {
		cmn_err (CE_NOTE, "hwgraph_vertex_create failed for /hw/tpusim: %d", status);
		return 0;
	}
	if ((status = hwgraph_edge_add (hwgraph_root, vhdl_top, "tpusim")) != GRAPH_SUCCESS) {
		cmn_err (CE_NOTE, "hwgraph_edge_add failed for /hw/tpusim: %d", status);
		return 0;
	}

	/*
	 * Loop through the simulated devices.
	 */
	for (i = 0; i < tpusim_devs; i++) {

		/*
		 * Add numeric verticies for each simulated device.
		 */
		(void) sprintf (name, "%d", i);
		if ((status = hwgraph_vertex_create (&vhdl_mid)) != GRAPH_SUCCESS) {
			cmn_err (CE_NOTE, "hwgraph_vertex_create failed for %v/%d: %d", vhdl_top, status);
			return 0;
		}
		if ((status = hwgraph_edge_add (vhdl_top, vhdl_mid, name)) != GRAPH_SUCCESS) {
			cmn_err (CE_NOTE, "hwgraph_edge_add failed for %v/%d: %d", vhdl_top, status);
			return 0;
		}

		/*
		 * Add parent verticies for each simulated device.
		 */
		if ((status = hwgraph_vertex_create (&vhdl_sub[i])) != GRAPH_SUCCESS) {
			cmn_err (CE_NOTE, "hwgraph_vertex_create failed for %v/tpu: %d", vhdl_mid, status);
			return 0;
		}
		if ((status = hwgraph_edge_add (vhdl_mid, vhdl_sub[i], "tpu")) != GRAPH_SUCCESS) {
			cmn_err (CE_NOTE, "hwgraph_edge_add failed for %v/tpu: %d", vhdl_mid, status);
			return 0;
		}

		/*
		 * Add simulator device nodes and fill in the parent vertex.
		 */
		(void) tpusim_attach (vhdl_sub[i]);
	}

	/*
	 * Call the tpu driver attach routine for each simulated device.
	 */
	for (i = 0; i < tpusim_devs; i++) {
		(void) tpu_attach (vhdl_sub[i]);
	}

	return 0;
}

/********************************************************************************
 *										*
 *	tpusim_unreg: called when a loadable driver is unloaded.		*
 *										*
 ********************************************************************************/
int
tpusim_unreg (void)
{
	return 0;
}

/********************************************************************************
 *										*
 *	tpusim_attach: called by tpusim_reg once for each vertex		*
 *	representing a simulated crosstalk widget.				*
 *										*
 ********************************************************************************/
int
tpusim_attach 
	(
	vertex_hdl_t	conn
	)
{
	vertex_hdl_t	vhdl_sim;
	graph_error_t	status;
	tpusim_soft_t *	soft;

	/*
	 * Add a device to provide access to the simulator driver.
	 */
	if ((status = hwgraph_char_device_add (conn, "sim", "tpusim_", &vhdl_sim)) != GRAPH_SUCCESS) {
		cmn_err (CE_NOTE, "hwgraph_char_device_add failed for %v/sim: %d", conn, status);
		return 0;
	}

	hwgraph_chmod (vhdl_sim, 0666);

	/*
	 * Allocate a tpusim_soft_s structure for this sim device and link to it
	 * from both the parent (simulated xtalk) and device verticies.
	 */
	soft = (tpusim_soft_t *) kmem_zalloc (sizeof (tpusim_soft_t), KM_SLEEP);

	tpusim_soft_set (vhdl_sim, soft);
	tpusim_soft_set (conn, soft);

	soft->lss_magic = TPUSIM_MAGIC;
	soft->lss_size = sizeof (tpusim_soft_t);

	soft->lss_xregs  = (tpu_xtk_regs_t *) tpusim_pgalloc (sizeof (tpu_xtk_regs_t));
	soft->lss_aregs  = (tpu_att_regs_t *) tpusim_pgalloc (sizeof (tpu_att_regs_t));
	soft->lss_d0regs = (tpu_dma_regs_t *) tpusim_pgalloc (sizeof (tpu_dma_regs_t));
	soft->lss_d1regs = (tpu_dma_regs_t *) tpusim_pgalloc (sizeof (tpu_dma_regs_t));
	soft->lss_lregs  = (tpu_ldi_regs_t *) tpusim_pgalloc (sizeof (tpu_ldi_regs_t));

	/*
	 * Initialize some register values.
	 */
	soft->lss_xregs->x_cfg.w_id = 0x123;	/***temp***/

	return 0;
}

/* ==============================================================================
 *	      DRIVER OPEN/CLOSE
 */

/********************************************************************************
 *										*
 *	tpusim_open: called when a sim device special file is opened.		*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpusim_open 
	(
	dev_t *		devp, 
	int		oflag, 
	int		otyp, 
	cred_t *	crp
	)
{
	printf ("tpusim_open()\n");

	return 0;
}

/********************************************************************************
 *										*
 *	tpusim_close: called when a sim device special file is closed by a	*
 *	process and no other processes still have it open ("last close").	*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpusim_close 
	(
	dev_t 		dev, 
	int		oflag, 
	int		otyp, 
	cred_t *	crp
	)
{
	printf ("tpusim_close()\n");

	return 0;
}

/* ==============================================================================
 *		MEMORY MAP ENTRY POINTS
 */

/********************************************************************************
 *										*
 *	tpusim_map: called for a mmap(2) on an tpusim device special file.	*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpusim_map 
	(
	dev_t		dev, 
	vhandl_t *	vt, 
	off_t		off, 
	size_t		len, 
	uint		prot
	)
{
	vertex_hdl_t	vhdl = dev_to_vhdl (dev);
	tpusim_soft_t *	soft = tpusim_soft_get (vhdl);
	vertex_hdl_t	conn = soft->lss_conn;
	void *		kaddr;

	cmn_err (CE_CONT, "tpusim_map(%v)\n", dev);

	/*
	 * Get the requested simulated register address.
	 */
	if ((kaddr = (void *) tpusim_piotrans_addr (conn, 0, off, len, 0)) == NULL)
		return EINVAL;

	/*
	 * Round len up to a page boundary.
	 */
	len = ptob (btopr (len));

	/*
	 * Map the kernal address range into the user's address space.
	 */
	return (v_mapphys (vt, kaddr, len));
}

/********************************************************************************
 *										*
 *	tpusim_unmap: called for a munmap(2) on an tpusim device special file.	*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpusim_unmap 
	(
	dev_t		dev, 
	vhandl_t *	vt
	)
{
	cmn_err (CE_CONT, "tpusim_unmap(%v)\n", dev);

	return 0;
}


/* ==============================================================================
 *		CONTROL ENTRY POINT
 */

/********************************************************************************
 *										*
 *	tpusim_ioctl: a user has made an ioctl request for an open sim		*
 *	device.									*
 *										*
 *	cmd and arg are as specified by the user; arg is a pointer to something	*
 *	in the user's address space, so we need to use copyin() to read through	*
 *	it and copyout() to write through it.					*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpusim_ioctl 
	(
	dev_t		dev, 
	int		cmd, 
	void *		arg, 
	int		mode, 
	cred_t *	crp, 
	int *		rvalp
	)
{
	vertex_hdl_t		vhdl;
	graph_error_t		status;
	char			path[256];

	cmn_err (CE_CONT, "tpusim_ioctl(%v)\n", dev);

	switch (cmd) {

	case TPUSIM_ATTACH:

		if (copyin (arg, (caddr_t) path, sizeof (path)))
			return (EFAULT);

		path[255] = '\0';

		cmn_err (CE_CONT, "tpusim_ioctl - path: %s\n", path);

		if ((status = hwgraph_traverse (hwgraph_root, path, &vhdl)) != GRAPH_SUCCESS) {
			cmn_err (CE_CONT, "tpusim_ioctl - hwgraph_traverse returned %d\n", (int) status);
			return (EFAULT);
		}

		cmn_err (CE_CONT, "tpusim_ioctl - attaching to %v\n", vhdl);

		(void) tpu_attach (vhdl);

		return 0;
	}

	return (EINVAL);
}

/* ==============================================================================
 *		Simulated xwidget functions.
 */

/********************************************************************************
 *										*
 *	tpusim_info_chk: similar to xwidget_info_chk().				*
 *										*
 ********************************************************************************/
caddr_t
tpusim_info_chk
	(
	vertex_hdl_t	vhdl			/* check for this widget */
	)
{
	tpusim_soft_t *	soft = tpusim_soft_get (vhdl);

	if ((soft == NULL) || (soft->lss_magic != TPUSIM_MAGIC))
		return NULL;

	return ((caddr_t) soft);
}

/* ==============================================================================
 *		Simulated xtalk provider functions.
 */

/********************************************************************************
 *										*
 *	tpusim_piotrans_addr: simulate xtalk_piotrans_addr().			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
caddr_t
tpusim_piotrans_addr 
	(
	vertex_hdl_t	dev,			/* translate for this device */
	device_desc_t	dev_desc,		/* device descriptor */
	iopaddr_t	xtalk_addr,		/* Crosstalk address */
	size_t		byte_count,		/* map this many bytes */
	unsigned	flags			/* (currently unused) */
	)
{
	tpusim_soft_t *	soft = device_info_get (dev);

	/*
	 * Return the address of the appropriate simulated register set based on
	 * the offset.
	 */
	switch (xtalk_addr) {

	case TPU_XTK_REGS_OFFSET:
		return ((caddr_t) soft->lss_xregs);

	case TPU_ATT_REGS_OFFSET:
		return ((caddr_t) soft->lss_aregs);

	case TPU_DMA0_REGS_OFFSET:
		return ((caddr_t) soft->lss_d0regs);

	case TPU_DMA1_REGS_OFFSET:
		return ((caddr_t) soft->lss_d1regs);

	case TPU_LDI_REGS_OFFSET:
		return ((caddr_t) soft->lss_lregs);
	}

	return NULL;
}

/********************************************************************************
 *										*
 *	tpusim_dmatrans_list: simulate xtalk_dmatrans_list().			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
alenlist_t
tpusim_dmatrans_list
	(
	vertex_hdl_t	dev,			/* translate for this device */
	device_desc_t	dev_desc,		/* device descriptor */
	alenlist_t	palenlist,		/* system address/length list */
	unsigned	flags
	)
{
	return palenlist;
}

/********************************************************************************
 *										*
 *	tpusim_intr_alloc: simulate xtalk_intr_alloc().				*
 *										*
 ********************************************************************************/
/* ARGSUSED */
xtalk_intr_t
tpusim_intr_alloc 
	(
	vertex_hdl_t	dev,			/* which Crosstalk device */
	device_desc_t	dev_desc,		/* device descriptor */
	vertex_hdl_t	owner_dev		/* owner of this interrupt */
	)
{
	xtalk_intr_t	intr_hdl;
	static int	vector = 1;

	NEW (intr_hdl);

	intr_hdl->xi_dev = owner_dev;
	intr_hdl->xi_target = 9;
	intr_hdl->xi_vector = vector++;
	intr_hdl->xi_addr = 0x123456789abcULL;

	return intr_hdl;
}

/********************************************************************************
 *										*
 *	tpusim_intr_conect: simulate xtalk_intr_conect().			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
int
tpusim_intr_connect
	(
	xtalk_intr_t		intr_hdl,	/* xtalk intr resource handle */
	intr_func_t		intr_func,	/* xtalk intr handler */
	intr_arg_t		intr_arg,	/* arg to intr handler */
	xtalk_intr_setfunc_t	setfunc,	/* func to set intr hw */
	void *			setfunc_arg,	/* arg to setfunc */
	void *			thread		/* intr thread to use */
	)
{
	intr_hdl->xi_setfunc = setfunc;
	intr_hdl->xi_sfarg = setfunc_arg;

	(*setfunc) (intr_hdl);

	return 0;
}

/********************************************************************************
 *										*
 *	tpusim_intr_disconnect: simulate xtalk_intr_disconnect().			*
 *										*
 ********************************************************************************/
/* ARGSUSED */
void
tpusim_intr_disconnect
	(
	xtalk_intr_t		intr_hdl	/* xtalk intr resource handle */
	)
{
}

/* ==============================================================================
 *		Utility functions.
 */

/********************************************************************************
 *										*
 *	tpusim_pgalloc: allocate page-aligned chunks of kernel memory.		*
 *										*
 ********************************************************************************/
static void *
tpusim_pgalloc 
	(
	int		len			/* number of bytes needed */
	)
{
	return ((void *) ptob (btopr ((ulong_t) kmem_zalloc (len + ptob (1), KM_SLEEP))));
}

