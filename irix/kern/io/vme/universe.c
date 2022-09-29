/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure. 
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/* 
 * Universe instance of the VMEbus service provider 
 */

#ident "$Revision: 1.17 $"

#if SN0

#include <sys/types.h>
#include <sys/proc.h>           /* proc_t                               */
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/pda.h>
#include <ksys/xthread.h> 
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>      
#include <sys/driver.h>         /* device_driver_t class                */
#include <sys/map.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/runq.h>
#include <sys/ddi.h>
#include <sys/nic.h>
#include <sys/errno.h>
#include <sys/vmereg.h>
#include <sys/hwgraph.h>
#include <ksys/hwg.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/pcibr.h>
#include <sys/vme/vmeio.h>
#include <sys/vme/universe.h>
#include "vmeio_private.h"
#include "universe_private.h"
#include "ude_private.h"
#include "usrvme_private.h"

int universe_devflag = D_MP; 

/*
 * Hardware 
 */

#if UNIVERSE_REVNUM_WAR
#define UNIVERSE_CHIP_MR_VERSION   (3)    /* Use board revision to track
					     chip revision               */
#endif /* UNIVERSE_REVNUM_WAR */

/* Universe chip */
unsigned universe_pcisimg_offset[UNIVERSE_NUM_OF_PCISIMGS] = {0x100, 
							      0x114, 
							      0x128, 
							      0x13c,
							      0x1a0,
							      0x1b4,
							      0x1c8,
							      0x1dc};

unsigned universe_vmesimg_offset[UNIVERSE_NUM_OF_VMESIMGS] = {0xf00, 
							      0xf14, 
							      0xf28, 
							      0xf3c,
							      0xf90,
							      0xfa4,
							      0xfb8,
							      0xfcc};



/* xxx ficus only */
#undef  EDGE_LBL_VME
#define EDGE_LBL_VME "vme"

#define UNIVERSE_PIO_SPEC_SIZE            (64*1024*1024)
#define UNIVERSE_PIO_A32_SIZE             (32*1024*1024)
#define UNIVERSE_PIO_UNFIXED_SIZE         (32*1024*1024)

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof(*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof(*(ptr))))

#define UNIVERSE_PCI_ADDR_FAILED (0)

/* ------------------------------------------------------------------------
   
   Local data structures

   ------------------------------------------------------------------------ */

/* A list to record the valid DMA maps */
typedef struct universe_dmamap_list_item_s {
	struct universe_dmamap_s *dmamap;
	struct universe_dmamap_list_item_s *next;
} * universe_dmamap_list_item_t;

typedef struct universe_dmamap_list_s {
	sema_t                      lock[1];    
	universe_dmamap_list_item_t head;
} * universe_dmamap_list_t;

#define	universe_dmamaps_lock(dmamaps)	        psema((dmamaps)->lock, PZERO)
#define	universe_dmamaps_unlock(dmamaps)	vsema((dmamaps)->lock)

/* ------------------------------------------------------------------------
  
   Operations on data memebers of universe_state_t class

   ------------------------------------------------------------------------ */

/* Locking macros */
#define UNIVERSE_LOCK(universe_state) (                                        \
        mutex_spinlock(&((universe_state)->lock)))
#define UNIVERSE_UNLOCK(universe_state, s) (                                   \
        mutex_spinunlock(&((universe_state)->lock), s))
#define	FLOCK_UNIV(universe_state) (                                           \
        mutex_spinlock(&((universe_state)->unfixlock)))
#define	FUNLOCK_UNIV(universe_state, s) (                                      \
        mutex_spinunlock(&((universe_state)->unfixlock), s))
#define UNIVERSE_DMA_ENGINE_LOCK(universe_state) (                             \
        mutex_spinlock(&((universe_state)->dma_engine_lock)))
#define UNIVERSE_DMA_ENGINE_UNLOCK(universe_state, s) (                        \
        mutex_spinunlock(&((universe_state)->dma_engine_lock), s))

/* -------------------------------------------------------------------

   Operations on regular PCI slave image

   -------------------------------------------------------------------  */

/* Get the user count of the slave image */
#define UNIVERSE_PCISIMG_USERS_GET(slave_image) (                            \
	(slave_image)->users                                                 \
)

/* Increment the user count of the slave image */
#define UNIVERSE_PCISIMG_USERS_INC(slave_image) {                            \
	((slave_image)->users)++;                                            \
}

/* Decrement the user count of the slave image */
#define UNIVERSE_PCISIMG_USERS_DEC(slave_image) {                            \
	((slave_image)->users)--;                                            \
}

/* Increment the user count of the slave image atomically */
#define UNIVERSE_PCISIMG_USERS_INC_ATOMIC(slave_image, universe_state) {     \
        int s;                                                               \
        s = UNIVERSE_LOCK(universe_state);                                   \
	((slave_image)->users)++;                                            \
	UNIVERSE_UNLOCK(universe_state, s);                                  \
}

/* Decrement the user count of the slave image atomically */
#define UNIVERSE_PCISIMG_USERS_DEC_ATOMIC(slave_image, universe_state) {     \
        int s;                                                               \
        s = UNIVERSE_LOCK(universe_state);                                   \
	((slave_image)->users)--;                                            \
	UNIVERSE_UNLOCK(universe_state, s);                                  \
}

/* Get the size of the PCI slave image */
#define UNIVERSE_PCISIMG_SIZE_GET(slave_image) (                             \
        (slave_image)->pci_bound - (slave_image)->pci_base                   \
)

/* -------------------------------------------------------------------

   Operations on special PCI slave image

   -------------------------------------------------------------------  */

/* Get the user count of the slave image */
#define PCISIMG_SPEC_USERS_GET(image) (                                    \
	(image)->users                                                     \
)

/* Increment the user count of the slave image */
#define UNIVERSE_PCISIMG_SPEC_USERS_INC(image) {                           \
	((image)->users)++;                                                \
}

/* Decrement the user count of the slave image */
#define UNIVERSE_PCISIMG_SPEC_IMAGE_USERS_DEC(image) {                     \
	((image)->users)--;                                                \
}

/* Increment the user count of the slave image atomically */
#define UNIVERSE_PCISIMG_SPEC_USERS_INC_ATOMIC(image, universe_state) {    \
        int s;                                                             \
        s = UNIVERSE_LOCK(universe_state);                                 \
	((image)->users)++;                                                \
	UNIVERSE_UNLOCK(universe_state, s);                                \
}

/* Decrement the user count of the slave image atomically */
#define UNIVERSE_PCISIMG_SPEC_USERS_DEC_ATOMIC(image, universe_state) {    \
        int s;                                                             \
        s = UNIVERSE_LOCK(universe_state);                                 \
	((image)->users)--;                                                \
	UNIVERSE_UNLOCK(universe_state, s);                                \
}


/* ------------------------------------------------------------------------
  
   Operations on VME slave image

   ------------------------------------------------------------------------ */

/* Get the user count of the slave image */
#define UNIVERSE_VMESIMG_USERS_GET(image) ((image)->users)             

/* Increment the user count of the slave image */
#define UNIVERSE_VMESIMG_USERS_INC(image) ((image)->users++)

/* Decrement the user count of the slave image */
#define UNIVERSE_VMESIMG_USERS_DEC(image) {                                 \
        ASSERT((image)->users >= 1);                                        \
	(image)->users--;                                                   \
}

/* ------------------------------------------------------------------------
  
   Forward declarations of internal functions

   ------------------------------------------------------------------------ */

vmeio_provider_t universe_provider;

static int 
universe_pio_setup(universe_state_t);

static int 
universe_dma_setup(universe_state_t);

static void 
universe_intr_setup(universe_state_t);

static int 
universe_dma_engine_setup(universe_state_t);

static int
universe_pio_user_setup(universe_state_t);

/* Universe adapter interrupt handling */
static void 
universe_intr_handler(intr_arg_t);

/* Universe adapter error handling */
static int 
universe_error_handler(error_handler_arg_t errinfo,
		       int err_code,
		       ioerror_mode_t mode,
		       ioerror_t *ioerr);

static void
universe_vmebus_intr_handler(universe_state_t , vmeio_intr_level_t);

static void
universe_pio_postwrite_error_handler(universe_state_t universe_state);

static void
universe_dma_postwrite_error_handler(universe_state_t universe_state);

static void universe_piomaps_add(universe_state_t, universe_piomap_t);
static void universe_piomaps_delete(universe_state_t, universe_piomap_t);
static universe_piomap_t
universe_piomap_lookup_by_pciaddr(universe_state_t, iopaddr_t);
static universe_piomap_t
universe_piomap_lookup_by_vmeaddr(universe_state_t, iopaddr_t);

/* VMEbus probing */

static int 
universe_badaddr_val(universe_piomap_t,
		     iopaddr_t /*vmeaddr*/,
		     size_t /*byte_count*/, 
		     void * /*ptr*/);

/* PIO service */
static universe_piomap_t
_universe_piomap_alloc(universe_state_t,
		       vmeio_am_t,
		       iopaddr_t,
		       size_t,
		       size_t,
		       unsigned);

static void
_universe_piomap_free(universe_piomap_t);

caddr_t
universe_piomap_addr(universe_piomap_t universe_piomap,
		     iopaddr_t vmeaddr,
		     size_t byte_count);

static iopaddr_t 
universe_pci_addr(universe_state_t,
		  vmeio_am_t,
		  iopaddr_t,
		  size_t,
		  universe_pcisimg_num_t *); 

static long
universe_pcisimg_spec_space_offset_get(vmeio_am_t);

static void 
universe_piomap_lock(universe_piomap_t);

static void 
universe_piomap_unlock(universe_piomap_t);

static size_t 
universe_piomap_adjust(universe_piomap_t,
		       iopaddr_t,
		       size_t);

static int universe_pio_use_unfix(universe_piomap_t /*piomap*/, 
				  iopaddr_t /*addr*/, 
				  size_t /*size*/);
static int bcopyn(void * /*src*/, 
		  void * /*dst*/, 
		  int /*count*/,
		  int /*itmsz*/);
static int bcopyb(register char * /*src*/, register char * /* dest*/, 
		  int count);
static int bcopyh(register short * /*src*/, register short * /*dest*/, 
		  int count);
static int bcopyw(register int * /*src*/, register int * /*dest*/, 
		  register int /*count*/);
static int bcopyd(register long long * /*src*/, 
		  register long long * /*dest*/, 
		  int /*count*/);

/* DMA */
static void 
universe_dmamaps_add(universe_state_t, 
		     universe_dmamap_t);
static void 
universe_dmamaps_delete(universe_state_t, 
			universe_dmamap_t);
static unsigned
universe_to_pciio_flags(unsigned);

static device_register_f	universe_device_register;

/* ------------------------------------------------------------------------
  
	                 Universe -- initialization 

   ------------------------------------------------------------------------ */

/*
 * Initialize the driver, when the driver is configured into the
 * system, no matter if we have a universe or not.  
 * The _attach routine will be called if a universe is located.
 */
void
universe_init(void)
{
	pciio_driver_register(UNIVERSE_VENDOR_ID, 
			      UNIVERSE_DEVICE_ID, 
			      "universe_", 
			      0);
}

/* ------------------------------------------------------------------------
  
   Initialization of the Universe adapter device

   ------------------------------------------------------------------------ */

/*
 * This routine is responsible for placing the Universe controller in 
 * a known state. It needs to initialize the PCI specific registers.
 * This should be called once for each instance of the Universe chip found
 * on the system.
 * Called by: very early io sub-system initialization code.
 * Return: 0 if success, -1 otherwise
 */

int
universe_attach(vertex_hdl_t pci_conn)
{
	vertex_hdl_t            universe;
	vertex_hdl_t		universe_cdev;
	vertex_hdl_t            board_vertex;
	char                    board_nic_number_s[32];
	universe_state_t        universe_state;
	universe_chip_t *       universe_chip;
	caddr_t                 uvbase_addr;
	caddr_t	                uvcfg_addr;
	graph_error_t           rv;
	pciio_piomap_t          cmap;
	pciio_piomap_t          rmap;

	/* Add VMEbus in the hardware graph */
	rv = hwgraph_path_add(pci_conn, EDGE_LBL_VME, &universe);
	if (rv != GRAPH_SUCCESS) {
		cmn_err(CE_PANIC, "%v: vmebus provider init failed", pci_conn);
	}

	universe_state = 0;
	NEW(universe_state);
	if (universe_state == 0){
		cmn_err(CE_PANIC, "%v: vmebus provider init failed", pci_conn);
	}

	universe_state->vertex = universe; 
	universe_state->ctlr = -1;	/* controller number not yet known */

#if VME_DEBUG
	printf("Initializing Universe chip (%v) (universe_state 0x%x)\n",
		    universe, universe_state);
#endif /* VME_DEBUG */

	/* 
	 * Hardware initialization 
	 */

	uvcfg_addr = (caddr_t) pciio_pio_addr(pci_conn, 
					      0, 
					      PCIIO_SPACE_CFG, 
					      0, 
					      0x40, 
					      &cmap,
					      0);
	if (uvcfg_addr == 0) {
		cmn_err(CE_PANIC,
			"%v: unable to get PIO mapping for PCI CFG space",
			pci_conn);
		return(-1);
	}

	/* Get address of the Universe Base register */
	uvbase_addr = (caddr_t) pciio_pio_addr(pci_conn, 
					       0, 
					       PCIIO_SPACE_WIN(0), 
					       0, 
					       UNIVERSE_REGISTERS_SIZE,
					       &rmap,
					       0);
	if (uvbase_addr == 0) {
		cmn_err(CE_PANIC,
			"%v: unable to get PIO mapping for PCI MEM space",
			pci_conn);
		return(-1);
	}

	universe_chip = (universe_chip_t *) uvbase_addr;
	universe_state->chip = universe_chip;	

	if (universe_chip->pci_class.rid == UNIVERSE_PCI_CLASS_RID_0) {
		cmn_err(CE_PANIC, "VME hardware is downrev");
	}
	else { 
		universe_state->rev = UNIVERSE_REV_1;
	}

#if UNIVERSE_REVNUM_WAR
	universe_state->board_rev = 0;
	rv = hwgraph_traverse(pci_conn, "../..", &board_vertex);
	ASSERT(rv == GRAPH_SUCCESS);
	strncpy(board_nic_number_s, nic_vertex_info_get(board_vertex), 17);
        universe_state->board_rev = 
		atoi(board_nic_number_s + 14);
	if (universe_state->board_rev < UNIVERSE_CHIP_MR_VERSION) {
		cmn_err(CE_PANIC, "%v: VME hardware is downrev", board_vertex);
	}
#endif /* UNIVERSE_REVNUM_WAR */

#if VME_DEBUG
	printf("Universe Rev = 0x%x\n", universe_state->rev);
#if UNIVERSE_REVNUM_WAR
	printf("Universe Board Rev = 0x%x\n", universe_state->board_rev);
#endif /* UNIVERSE_REVNUM_WAR */
	printf("Universe RID = 0x%x\n", universe_chip->pci_class.rid);
#endif /* VME_DEBUG */

	universe_chip->pci_csr.d_pe = UNIVERSE_PCI_CSR_D_PE_CLEAR;
	universe_chip->pci_csr.s_serr = UNIVERSE_PCI_CSR_S_SERR_CLEAR;
	universe_chip->pci_csr.r_ma = UNIVERSE_PCI_CSR_R_MA_CLEAR;
	universe_chip->pci_csr.r_ta = UNIVERSE_PCI_CSR_R_TA_CLEAR;
	universe_chip->pci_csr.s_ta = UNIVERSE_PCI_CSR_S_TA_CLEAR;
	ASSERT(universe_chip->pci_csr.devsel == 
	       UNIVERSE_PCI_CSR_DEVSEL_MEDIUM);
	universe_chip->pci_csr.dp_d = UNIVERSE_PCI_CSR_DP_D_CLEAR;
	ASSERT(universe_chip->pci_csr.tfbbc == 
	       UNIVERSE_PCI_CSR_TFBBC_DISABLED);
	ASSERT(universe_chip->pci_csr.mfbbc == 
	       UNIVERSE_PCI_CSR_MFBBC_DISABLED);
	universe_chip->pci_csr.serr_en = UNIVERSE_PCI_CSR_SERR_EN_ENABLE;
	ASSERT(universe_chip->pci_csr.wait == UNIVERSE_PCI_CSR_WAIT_0);
	universe_chip->pci_csr.peresp = UNIVERSE_PCI_CSR_PERESP_ENABLE;
	ASSERT(universe_chip->pci_csr.vgaps == 
	       UNIVERSE_PCI_CSR_VGAPS_DISABLE);
	ASSERT(universe_chip->pci_csr.mwi_en == 
	       UNIVERSE_PCI_CSR_MWI_EN_DISABLED);
	ASSERT(universe_chip->pci_csr.sc == UNIVERSE_PCI_CSR_SC_DISABLED);
	ASSERT(universe_chip->pci_csr.bm == UNIVERSE_PCI_CSR_BM_ENABLED);
	ASSERT(universe_chip->pci_csr.ms == UNIVERSE_PCI_CSR_MS_ENABLED);
	ASSERT(universe_chip->pci_csr.ios == UNIVERSE_PCI_CSR_IOS_ENABLED); 

	/* Allow for 128B burst */
	universe_chip->pci_misc0.ltimer = UNIVERSE_PCI_MISC0_LTIMER_40CLKS;
	
	/*
	 * Clear the power-up option: SYSFAIL pin was asserted.
	 * No extensive on-board diagnostics are needed.
	 */
	universe_chip->vcsr_clr.sysfail = UNIVERSE_VCSR_CLR_SYSFAIL_NEGATE;
	ASSERT(universe_chip->vcsr_clr.sysfail == 
	       UNIVERSE_VCSR_CLR_SYSFAIL_NOTASSERTED);

	us_delay(1); /* Delay 10ms to wait for sysfail to float back high */

	universe_chip->lint_stat = UNIVERSE_LINT_SYSFAIL;

	ASSERT(universe_chip->mast_ctl.maxrtry == 
	       UNIVERSE_MAST_CTL_MAXRTRY_512TMS);
	ASSERT(universe_chip->mast_ctl.vrl == UNIVERSE_MAST_CTL_VRL_3);
	universe_chip->mast_ctl.pabs = UNIVERSE_MAST_CTL_PABS_128B;

	/* 
	 * Set the miscellaneous control register 
	 * -- Universe is the system controllor
	 * -- VMEbus arbitration mode is priority based
	 */
	universe_chip->misc_ctl.syscon = UNIVERSE_MISC_CTL_SYSCON_ON;
	universe_chip->misc_ctl.varb = UNIVERSE_MISC_CTL_VARB_PRI; 
	ASSERT(universe_chip->misc_stat.lclsize == 
	       UNIVERSE_MISC_STAT_LCLSIZE_64); /* Universe is on D64 PCIbus */

	/* 
	 * Software initialization 
	 */

	initnlock(&universe_state->lock, "unlock");
	initnlock(&universe_state->unfixlock, "unfixlck");

	universe_state->pci_conn = pci_conn;

	/* Associate the Universe state to the Universe vertex */
	device_info_set(universe, universe_state);

	/* Provide the VMEbus to VME device drivers */
	vmeio_provider_register(universe, &universe_provider);
	vmeio_provider_startup(universe);

	/* Register the error handler */
	pciio_error_register(pci_conn, universe_error_handler, universe_state);

	/* provide openable controller vertex */
	rv = hwgraph_char_device_add
		(universe, EDGE_LBL_CONTROLLER, "universe_", &universe_cdev);
	ASSERT(rv == GRAPH_SUCCESS);
	device_info_set(universe_cdev, universe_state);

	/* controller number will come from ioconfig. */
	device_inventory_add(universe_cdev, INV_BUS, INV_BUS_VME, -1, -1, -1);

	/* Support kernel level PIO, DMA, INT access */ 
#if VME_DEBUG
	sync_printf("\tUniverse PIO setup\n");
#endif /* VME_DEBUG */
	universe_pio_setup(universe_state);   /* PIO related initialization  */

#if VME_DEBUG
	sync_printf("\tUniverse DMA setup\n");
#endif /* VME_DEBUG */
	universe_dma_setup(universe_state);   /* DMA related initialization  */

#if VME_DEBUG
	sync_printf("\tUniverse interrupt setup\n");
#endif /* VME_DEBUG */
	universe_intr_setup(universe_state);  /* INT related initialization  */

#if VME_DEBUG
	sync_printf("\tUniverse DMA engine setup\n");
#endif /* VME_DEBUG */
	universe_dma_engine_setup(universe_state);  /* DMA engine */

	/* Support user level PIO */
	universe_pio_user_setup(universe_state);    /* User PIO/INTR */

#if VME_DEBUG
	sync_printf("Universe chip %v is initialized\n", universe);
#endif /* VME_DEBUG */

#if 0	/* XXX turn this to support "any adapter" VECTOR lines */
	/* get the "unspecified controller" connections set up */
	vmeio_edtscan(universe, 0, universe_device_register);
#endif

	return(0);
}

vertex_hdl_t
universe_device_register(vertex_hdl_t master,
			 int          controller,
			 vmeio_am_t   am,
			 iopaddr_t    base)
{
	vmeio_info_t            vinfo;
	vertex_hdl_t            vconn;
	char                    name[32];

	switch (am) {

	default:			/* catch-all for bogus values */
		sprintf(name, "0x%x/0x%x", am, base);
		break;

	case 0:
		if (base == 0)		/* special case for the neutral node */
			strcpy(name, "direct");
		else			/* bogus but do something rational */
			sprintf(name, "none/0x%x", base);
		break;

		/* normal cases: known space, specific base */
	case VMEIO_AM_A16 | VMEIO_AM_N:
		sprintf(name, "%s/0x%x", EDGE_LBL_A16N, base);
		break;
	case VMEIO_AM_A16 | VMEIO_AM_S:
		sprintf(name, "%s/0x%x", EDGE_LBL_A16S, base);
		break;
	case VMEIO_AM_A24 | VMEIO_AM_N:
		sprintf(name, "%s/0x%x", EDGE_LBL_A24N, base);
		break;
	case VMEIO_AM_A24 | VMEIO_AM_S:
		sprintf(name, "%s/0x%x", EDGE_LBL_A24S, base);
		break;
	case VMEIO_AM_A32 | VMEIO_AM_N:
		sprintf(name, "%s/0x%x", EDGE_LBL_A32N, base);
		break;
	case VMEIO_AM_A32 | VMEIO_AM_S:
		sprintf(name, "%s/0x%x", EDGE_LBL_A32S, base);
		break;
	}

	/* create or locate the connection point.
	 */
	if (GRAPH_SUCCESS !=
	    hwgraph_path_add(master, name, &vconn))
		return GRAPH_VERTEX_NONE;

	vinfo = device_info_get(vconn);
	if (!vinfo) {
		NEW(vinfo);
		vinfo->conn = vconn;
		vinfo->am = am;
		vinfo->addr = base;
		vinfo->state = 0;
		vinfo->provider = master;
		vinfo->provider_fastinfo =
		    (arbitrary_info_t) device_info_get(master);
		vinfo->errhandler = 0;
		vinfo->errinfo = 0;
		vinfo->private_info = 0;
		device_info_set(vconn, vinfo);
		device_master_set(vconn, master);
	}
	/* controller may have been changed.
	 * if it's stored in vmeio_info,
	 * update it here.
	 */
	vinfo->ctlr = controller;

	return vconn;
}

/* ARGSUSED */
int
universe_open(dev_t *devp, int oflag, int otyp, cred_t *credp)
{
	if (!_CAP_CRABLE(credp, CAP_DEVICE_MGT)) {
#if DEBUG_UNIVERSE
		printf("universe_open: !CAP_DEVICE_MGT\n");
#endif
		return EPERM;
	}
#if DEBUG_UNIVERSE
	printf("universe_open: ok!\n");
#endif

	return 0;
}

/*ARGSUSED */
int
universe_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	return 0;
}

/*ARGSUSED */
int
universe_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp)
{
	vertex_hdl_t            vhdl = dev_to_vhdl(dev);
	universe_state_t        universe_state = device_info_get(vhdl);
	unsigned                ctlr;
	vertex_hdl_t            cvhdl;
	char                    name[32];

#if DEBUG_UNIVERSE
	printf("universe_ioctl(%d,0x%x,0x%x,0x%x,0x%x,0x%x)\n"
	       "\t%v\n",
	       vhdl, cmd, uarg, mode, crp, rvalp, vhdl);
#endif

	if (cmd == VMEIOCSETCTLR) {
		inventory_t *pinv;
		invplace_t invplace = INVPLACE_NONE;
		ctlr = hwgraph_controller_num_get(vhdl);

		if ((pinv = device_inventory_get_next(vhdl, &invplace)) != NULL) {
			pinv->inv_state = ctlr;
		}


		/* ignore repeats of previous assignments.
		 * ioconfig seems to punch us twice.
		 */
		if (universe_state->ctlr != ctlr) {
			universe_state->ctlr = ctlr;

#if DEBUG_UNIVERSE
			printf("universe: I must be controller number %d.\n"
			       "\t%v\n",
			       ctlr, vhdl);
		
#endif

			/* XXX- "ctlr" needs to be copied back
			 * into the "state" field of our
			 * inventory entry, so hinv reports
			 * the bus adapter number.
			 *
			 * How do we do this?
			 */

			if (GRAPH_SUCCESS !=
			    hwgraph_path_add(hwgraph_root, EDGE_LBL_VME, &cvhdl)) {
				printf("vmeio_edtscan: unable to create convenience directory\n"
				       "\t%v/%s\n",
				       hwgraph_root, EDGE_LBL_VME);
				return *rvalp = ENOMEM;
			}

			sprintf(name, "%d", ctlr);

			if (GRAPH_SUCCESS !=
			    hwgraph_edge_add(cvhdl, universe_state->vertex, name)) {
				printf("vmeio_edtscan: unable to create convenience link\n"
				       "\t%v/%s\n",
				       cvhdl, name);
				return *rvalp = ENOMEM;
			}

			/* now pick up EDT entries that specify
			 * a controller number.
			 */
			vmeio_edtscan(universe_state->vertex, 
				      ctlr, 
				      universe_device_register);
		}

		return *rvalp = 0;
	}

	return *rvalp = EINVAL;
}

/*
 * Probe the desired address
 * Called by: syssgi system call
 * XXX Current interface of SGI_IOPROBE syssgi call is preserved.
 * XXX Adapter no will be gone, vertex will be passed in
 * XXX The interface is a compromise between old API and new API
 * Mem: piomap allocated and freed here
 * Return: 0 if successful, -1 if failure.
 *
 * NOTE: if this is called after ioconfig, we can go back
 * to using the adapter number; the vmebus will be found
 * at ("vmebus/%d", adapter) ...
 */

int
universe_probe(char *universe_name, iospace_t *iospace, int *rv)
{
	vertex_hdl_t      universe;
	universe_state_t  universe_state;
	universe_piomap_t univ_piomap;
	vmeio_am_t        am;
	iopaddr_t         vmeaddr;
	size_t            byte_count;
	int               error;

	error = 0;

	universe = hwgraph_path_to_vertex(universe_name);
	if (universe == GRAPH_VERTEX_NONE) {
		ASSERT(0);
		return(-1);
	}

	am = iospace_to_vmeioam(iospace->ios_type);
	ASSERT(am != 0);

	universe_state = (universe_state_t)device_info_get(universe);
	if (universe_state == 0) {
		ASSERT(0);
		return(ENODEV);
	}
	vmeaddr = iospace->ios_iopaddr;
	byte_count = iospace->ios_size;

	/* XXX has to be multiples of page size */
	/* byte_count = (ctob(1) > byte_count) ? ctob(1) : byte_count; */

	univ_piomap = _universe_piomap_alloc(universe_state, am, 
					     vmeaddr, byte_count,
					     byte_count, 0);
	if (univ_piomap == 0) {
		ASSERT(0);
		return(ENODEV);
	}

	/* Check for bus error when reading an addr */
	if (universe_badaddr_val(univ_piomap, vmeaddr, byte_count, rv)) {
		_universe_piomap_free(univ_piomap);
		return(ENODEV);
	}

	_universe_piomap_free(univ_piomap);
	return(error);
}

/*
 * Note: Has to deal with unfixed mapping here.
 *
 * XXX- doesn't universe_piomap_addr take care
 * of locking and adjusting the map? If not,
 * then how does vmeio_piomap_{alloc,addr}
 * work?
 */
static int
universe_badaddr_val(universe_piomap_t piomap,
		     iopaddr_t vmeaddr,
		     size_t byte_count, 
		     void *ptr)
{
	caddr_t               kvaddr;
	int                   rv;
	
	if (piomap->uv_flags != VMEIO_PIOMAP_FIXED) {
		universe_piomap_lock(piomap);
		universe_piomap_adjust(piomap, vmeaddr, byte_count);
	}

	kvaddr = universe_piomap_addr(piomap,
				      vmeaddr,
				      byte_count);
	rv = badaddr_val(kvaddr, byte_count, ptr);

	if (piomap->uv_flags != VMEIO_PIOMAP_FIXED) {
		universe_piomap_unlock(piomap);
	}
	
	return(rv);
}



/* ------------------------------------------------------------------------
  
   Universe configuration management

   ------------------------------------------------------------------------ */

/* ARGSUSED */
/* REFERENCED */
void
universe_provider_startup(vertex_hdl_t universe)
{
}

/* ARGSUSED */
/* REFERENCED */
void
universe_provider_shutdown(vertex_hdl_t universe)
{
}

/* -----------------------------------------------------------------------
   
                          PIO -- initialization 

   ----------------------------------------------------------------------- */

/*
 * Set up resources for PIO access
 * Return: 0 if success, -1 if failure
 */

static int
universe_pio_setup(universe_state_t universe_state)
{ 
	universe_chip_t	*                chip;
	universe_pcisimg_t *             pcisimg;
	caddr_t		                 cpuvaddr;
	iopaddr_t                        pciaddr;
	iopaddr_t                        pcibase;
	size_t		                 wsize;
	int		                 i;
	pciio_piomap_t	                 universe_piomap;
	universe_piomap_list_t           piomaps;
	universe_pcisimg_state_t *       pcisimg_state;
	universe_pcisimg_spec_state_t *  pcisimg_spec_state;

	chip = universe_state->chip;


	/* 
	 * Get a 64MB sized chunk of for use by Special slave image. 
	 * -- The alignment is imposed by the Universe hardware.
	 */

	pciaddr = pciio_piospace_alloc(universe_state->pci_conn,
				       0,
				       PCIIO_SPACE_MEM32,
				       UNIVERSE_PIO_SPEC_SIZE,
				       64*1024*1024);
#if VME_DEBUG
	printf("PCI base for specifal slave image: 0x%x\n", pciaddr);
#endif 
	universe_piomap = pciio_piomap_alloc(universe_state->pci_conn,
					     0,
					     PCIIO_SPACE_MEM32,
					     pciaddr,
					     UNIVERSE_PIO_SPEC_SIZE,
					     UNIVERSE_PIO_SPEC_SIZE,
					     PCIIO_BYTE_STREAM | PIOMAP_FIXED);
	if (universe_piomap == 0){
		cmn_err(CE_PANIC, 
			"%v: No PIO space for VME",
			universe_state->vertex);
	}

	pcisimg_spec_state = &(universe_state->pcisimg_spec_state);
	pcisimg_spec_state->kv_base = 
		pciio_piomap_addr(universe_piomap, 
				  pciaddr,
				  UNIVERSE_PIO_SPEC_SIZE);
	ASSERT(pcisimg_spec_state->kv_base);
	pcisimg_spec_state->pci_base = 
		pciio_pio_pciaddr_get(universe_piomap);

	ASSERT(!(pcisimg_spec_state->pci_base & ALIGN_64M));	

	pcisimg_spec_state->pci_piomap = universe_piomap;
		
	/* 
	 * Control the PCI special slave image
	 */
	ASSERT(chip->pcisimg_spec.pwen == UNIVERSE_PCISIMG_SPEC_PWEN_OFF);

	chip->pcisimg_spec.pwen = UNIVERSE_PCISIMG_SPEC_PWEN_ON;

	chip->pcisimg_spec.vdw3 = UNIVERSE_PCISIMG_SPEC_VDW_16; 
	chip->pcisimg_spec.vdw2 = UNIVERSE_PCISIMG_SPEC_VDW_32; 
	chip->pcisimg_spec.vdw1 = UNIVERSE_PCISIMG_SPEC_VDW_16; 
	chip->pcisimg_spec.vdw0 = UNIVERSE_PCISIMG_SPEC_VDW_32; 

	chip->pcisimg_spec.pgm3 = UNIVERSE_PCISIMG_SPEC_PGM_DATA;
	chip->pcisimg_spec.pgm2 = UNIVERSE_PCISIMG_SPEC_PGM_DATA;
	chip->pcisimg_spec.pgm1 = UNIVERSE_PCISIMG_SPEC_PGM_DATA;
	chip->pcisimg_spec.pgm0 = UNIVERSE_PCISIMG_SPEC_PGM_DATA;

	chip->pcisimg_spec.super3 = UNIVERSE_PCISIMG_SPEC_SUPER_N;
	chip->pcisimg_spec.super2 = UNIVERSE_PCISIMG_SPEC_SUPER_N;
	chip->pcisimg_spec.super1 = UNIVERSE_PCISIMG_SPEC_SUPER_S;
	chip->pcisimg_spec.super0 = UNIVERSE_PCISIMG_SPEC_SUPER_S;

	chip->pcisimg_spec.bs = pcisimg_spec_state->pci_base 
		>> UNIVERSE_PCISIMG_SPEC_BS_SHFT;

	ASSERT(chip->pcisimg_spec.las == UNIVERSE_PCISIMG_SPEC_LAS_MEM);
		
	ASSERT(chip->pcisimg_spec.en == UNIVERSE_PCISIMG_SPEC_EN_OFF);
	chip->pcisimg_spec.en = UNIVERSE_PCISIMG_SPEC_EN_ON;
		

	/* Image for A32 unfixed mappings */
	pciaddr = pciio_piospace_alloc(universe_state->pci_conn,
				       0,
				       PCIIO_SPACE_MEM32,
				       UNIVERSE_PIO_UNFIXED_SIZE,
				       32*1024*1024);
	
#if VME_DEBUG
	printf("PCI base for A32 unfixed mapping: 0x%x\n", pciaddr);
#endif 
				       
	universe_piomap = pciio_piomap_alloc(universe_state->pci_conn,
					     0,
					     PCIIO_SPACE_MEM32,
					     pciaddr,
					     UNIVERSE_PIO_UNFIXED_SIZE,
					     UNIVERSE_PIO_UNFIXED_SIZE,
					     PCIIO_BYTE_STREAM | PIOMAP_FIXED);
	if (universe_piomap == 0) {
		cmn_err(CE_PANIC, 
			"%v: Insufficient PIO space for VME",
			universe_state->vertex);
	}

	cpuvaddr = pciio_pio_kvaddr_get(universe_piomap);
	pcibase  = pciio_pio_pciaddr_get(universe_piomap);
	wsize    = pciio_pio_mapsz_get(universe_piomap);

	ASSERT((pcibase & ALIGN_4K) == 0); 

	pcisimg_state = &(universe_state->pcisimg_unfixed);
	pcisimg_state->pci_piomap = universe_piomap;
	pcisimg_state->kv_base = cpuvaddr;
	pcisimg_state->pci_base  = pcibase;
	pcisimg_state->pci_bound = (pcibase + wsize);

	/* 
	 * Control of the slave image:
	 * -- Leave the the image disabled
	 * -- Enable posted write
	 * -- VMEbus datawidth is 64bits
	 * -- Addr space is A32
	 * -- Data mode
	 * -- We'll give the AM mode later.
	 * -- single mode
	 * -- PCI mem space
	 */

	pcisimg = (universe_pcisimg_t *) 
		UNIVERSE_REG_ADDR(chip, 
				  universe_pcisimg_offset
				  [UNIVERSE_PCISIMG_UNFIXED_7]);
	
	ASSERT(pcisimg->ctl.en == UNIVERSE_PCISIMG_CTL_EN_OFF);
	pcisimg->ctl.vdw = UNIVERSE_PCISIMG_CTL_VDW_64;
	pcisimg->ctl.vas = UNIVERSE_PCISIMG_CTL_VAS_A32;
	ASSERT(pcisimg->ctl.pgm == UNIVERSE_PCISIMG_CTL_PGM_DATA);
	ASSERT(pcisimg->ctl.super == UNIVERSE_PCISIMG_CTL_SUPER_N);
	ASSERT(pcisimg->ctl.vct == UNIVERSE_PCISIMG_CTL_VCT_SINGLE);
	ASSERT(pcisimg->ctl.las == UNIVERSE_PCISIMG_CTL_SUPER_N);

	pcisimg->ctl.pwen = UNIVERSE_PCISIMG_CTL_PWEN_ON;

	for (i = 0; i <= UNIVERSE_NUM_OF_PCISIMGS_FIXED; i++) {
		universe_piomap = 0;

		pciaddr = pciio_piospace_alloc(universe_state->pci_conn,
					       0,
					       PCIIO_SPACE_MEM32,
					       UNIVERSE_PIO_A32_SIZE,
					       32*1024*1024);
#if VME_DEBUG
		printf("PCI base for slave image #%d: 0x%x\n", i, pciaddr);
#endif 

		universe_piomap =
			pciio_piomap_alloc(universe_state->pci_conn,
					   0,
					   PCIIO_SPACE_MEM32,
					   pciaddr,
					   UNIVERSE_PIO_A32_SIZE,
					   UNIVERSE_PIO_A32_SIZE,
					   PCIIO_BYTE_STREAM | PIOMAP_FIXED);
		if (universe_piomap == 0) {
			cmn_err(CE_PANIC, 
				"%v: insufficient PIO space for VME", 
				universe_state->vertex);
		}
	
		universe_state->pcisimgs_fixed[i].pci_piomap = universe_piomap;
	        universe_state->pcisimgs_fixed[i].kv_base = 
			pciio_piomap_addr(universe_piomap, 
					  pciaddr,
					  UNIVERSE_PIO_A32_SIZE);
		if (universe_state->pcisimgs_fixed[i].kv_base == 0) {
			cmn_err(CE_PANIC,
				"%v: invalid kvaddr for A32 PIO map",
				universe_state->vertex);
		}
		universe_state->pcisimgs_fixed[i].pci_base = 
			pciio_pio_pciaddr_get(universe_piomap);
		universe_state->pcisimgs_fixed[i].pci_bound = 
			universe_state->pcisimgs_fixed[i].pci_base + 
			pciio_pio_mapsz_get(universe_piomap);
		ASSERT(universe_state->pcisimgs_fixed[i].users == 0);

		
		/* Program the registers */
		pcisimg = (universe_pcisimg_t *) 
			UNIVERSE_REG_ADDR(universe_state->chip, 
					  universe_pcisimg_offset[i]);

		/* 
		 * Program the control register:
		 * -- Don't enable the image until there's a request
		 * -- Enable post write
		 * -- VMEbus datawidth is 64bits
		 * -- Addr space is A32
		 * -- Data mode
		 * -- Don't enable Supervisor mode until there's a request
		 * -- Single mode
		 * -- PCI mem space
		 */

		ASSERT(pcisimg->ctl.en == UNIVERSE_PCISIMG_CTL_EN_OFF);
		pcisimg->ctl.pwen = UNIVERSE_PCISIMG_CTL_PWEN_ON;
		pcisimg->ctl.vas = UNIVERSE_PCISIMG_CTL_VAS_A32;
		ASSERT(pcisimg->ctl.pgm == UNIVERSE_PCISIMG_CTL_PGM_DATA);
		ASSERT(pcisimg->ctl.vct == UNIVERSE_PCISIMG_CTL_VCT_SINGLE);
		ASSERT(pcisimg->ctl.las == UNIVERSE_PCISIMG_CTL_LAS_MEM);

		pcisimg->bs = universe_state->pcisimgs_fixed[i].pci_base;
		pcisimg->bd = universe_state->pcisimgs_fixed[i].pci_bound;
	}

	/* Initialize the PIO map list for error handling */
	piomaps = 0;
	NEW(piomaps);
	ASSERT(piomaps);
	ASSERT(piomaps->head == 0);
	initnsema(piomaps->lock, 1, "vme_piomaps_lock");
	universe_state->piomaps = piomaps;

	return(0);
}

/* ------------------------------------------------------------------------
  
                          PIO -- exported functions 

   ------------------------------------------------------------------------ */

/*
 * Allocate the PIO resources
 * 
 * Return: a handle of the PIO resources if success, 0 otherwise.
 * XXX kvaddr is not supposed to fixed at this time, only PCI address
 *     should be known
 */

/*ARGSUSED6*/
universe_piomap_t
universe_piomap_alloc(vertex_hdl_t  vme_conn,
		      device_desc_t dev_desc,
		      vmeio_am_t    am,
		      iopaddr_t     vmeaddr,
		      size_t        byte_count,
		      size_t        byte_count_max,
		      unsigned      flags)
{
	universe_state_t  universe_state;
	vmeio_info_t      vme_info;
	universe_piomap_t univ_piomap;

	vme_info = vmeio_info_get(vme_conn);
	universe_state = (universe_state_t) 
		vmeio_info_provider_fastinfo_get(vme_info);
	ASSERT(universe_state != 0);

	univ_piomap = _universe_piomap_alloc(universe_state, 
					     am, 
					     vmeaddr, 
					     byte_count, 
					     byte_count_max,
					     flags);
	if (univ_piomap == 0) {
		return(0);
	}

	univ_piomap->uv_dev = vme_conn;

	universe_piomaps_add(universe_state, univ_piomap);

#if VME_DEBUG
	printf("%v: universe_piomap_alloc: ", univ_piomap->uv_dev);
	printf("vmeaddr 0x%x pci_addr 0x%x kvaddr 0x%x\n",
	       univ_piomap->uv_vmeaddr, 
	       univ_piomap->pci_addr, 
	       univ_piomap->uv_kvaddr);
#endif 

	return(univ_piomap);
}

/* 
 * Workhorse of universe_piomap_alloc()
 * Called by: universe_piomap_alloc(), universe_probe()
 * Note: This function is necessary for those callers w/o connection points.
 */

/* ARGSUSED */
static universe_piomap_t
_universe_piomap_alloc(universe_state_t   universe_state,
		       vmeio_am_t         am,
		       iopaddr_t          vmeaddr,
		       size_t             byte_count,
		       size_t             byte_count_max,
		       unsigned           flags)
{
	universe_piomap_t                univ_piomap;
	universe_pcisimg_num_t           image_num;
	iopaddr_t                        pciaddr;
	universe_pcisimg_state_t *       pcisimg_state;
	universe_pcisimg_spec_state_t *  pcisimg_spec_state;
	/* REFERENCED */
	pciio_piomap_t	                 pci_piomap;
	caddr_t                          kv_base;    

	/* Find the PCI PIO map used for the specified space */
	if (am & VMEIO_AM_A32) {

		/* Try to find a legal PCIbus address */
		pciaddr = universe_pci_addr(universe_state, 
					    am, 
					    vmeaddr, 
					    byte_count, 
					    &image_num);

		if (pciaddr == 0) {

			/* 
			 * The PCI slave images associated with the fixed map 
			 * don't have the support the required VMEbus 
			 * address range.  An unfixed map will be created if 
			 * it is OK for the user.
			 */

			if (flags == VMEIO_PIOMAP_UNFIXED) {
				image_num = UNIVERSE_PCISIMG_UNFIXED_7;
				pci_piomap = universe_state->
					pcisimg_unfixed.pci_piomap;
			}
			else {
				return(0);
			}
		}
		else {
			/* We're guaranteed to get a fixed map now */

			pcisimg_state = 
				&(universe_state->pcisimgs_fixed[image_num]);

			ASSERT(pcisimg_state->kv_base);

			/* Record the PCIbus resource map */
			pci_piomap = pcisimg_state->pci_piomap;

			/* 
			 * Here, we also know the base kvaddr for 
			 * the VMEbus PIO map 
			 */
			kv_base = pcisimg_state->kv_base + 
				(vmeaddr - (pcisimg_state->pci_base + 
					    pcisimg_state->to));

			/* Update the resource map */
			flags = VMEIO_PIOMAP_FIXED;
		}
	}

	else if ((am & VMEIO_AM_A24) || (am &  VMEIO_AM_A16)) {

		/* 
		 * Is there a PCIbus address available for the VMEbus addr?
		 */
		pciaddr = universe_pci_addr(universe_state, 
					    am,
					    vmeaddr, 
					    byte_count,
					    &image_num); 
		if (pciaddr == 0) {
			/* 
			 * In the case of A16 and A24, all the addr spaces
			 * are available from the special PCI slave image.
			 * If the PCIbus address is not found, the VMEbus
			 * address must be an illegal one.
			 */
			return(0);
		}

		pcisimg_spec_state = &(universe_state->pcisimg_spec_state);
		pci_piomap = pcisimg_spec_state->pci_piomap;

		/* Get the base kvaddr */
		kv_base = pcisimg_spec_state->kv_base + 
			universe_pcisimg_spec_space_offset_get(am) +
			vmeaddr;

		/* All A16 and A24 requesters will be using this fixed map */
		flags = VMEIO_PIOMAP_FIXED;

		/* Update the state of the Universe adapter */
		UNIVERSE_PCISIMG_SPEC_USERS_INC_ATOMIC(pcisimg_spec_state,
						       universe_state);   
	}

	else {
		ASSERT(0);
		pci_piomap =0;
	}

	if (pci_piomap) {
		/* 
		 * We have successfully got the PIO resources.
		 * It's time to set the user resource map up.
		 * Some of the items are already set in the previous 
		 * stage of identifying the PCIbus resource map.
		 */

		/* Allocate a piomap */
		univ_piomap = 0;
		NEW(univ_piomap);  
		if (univ_piomap == 0) {
			return(0); 
		}

		univ_piomap->universe_state = universe_state;
		univ_piomap->uv_flags = flags;  
		univ_piomap->uv_am = am;
		univ_piomap->uv_vmeaddr = vmeaddr;
		univ_piomap->uv_mapsz = byte_count;
		univ_piomap->image_num = image_num;
		univ_piomap->pci_addr = pciaddr;
		univ_piomap->pci_piomap = pci_piomap;

		if (univ_piomap->uv_flags & VMEIO_PIOMAP_FIXED) {
			/* 
			 * Since a pciio_piomap_addr() is already called,
			 * we can get the base kvaddr of the PCIbus PIO map
			 * and use it to calculate the base kvaddr for 
			 * our VMEbus PIO map.
			 */
			univ_piomap->uv_kvaddr = kv_base;
		}
		else {	
			/* 
			 * System address will be set later 
			 * for VMEbus 
			 */
			ASSERT(univ_piomap->uv_kvaddr == 0);
		}
	} 
	else {
		univ_piomap = 0;
	}

	return(univ_piomap);
}

/*
 * Free the PIO resources 
 * Called by: vmeio_piomap_free()
 * Note: Since all the PCIbus resources are fixed at the startup time, we only
 * need free the VMEbus resources. 
 */
void
universe_piomap_free(universe_piomap_t piomap)
{

	universe_piomaps_delete(piomap->universe_state, piomap);

	_universe_piomap_free(piomap);
}

/* 
 * Free PIO resources -- Workhorse
 *   -- Decrement reference count for the image
 *   -- Disable the image if the reference count goes down to zero
 * Called by: 
 *    universe_piomap_free()
 *    user level PIO driver
 *    user level DMA engine driver
 *    universe_probe()
 * XXX disable A16 and A24 when ref count drops to 0
 * XXX Sync?
 */

void
_universe_piomap_free(universe_piomap_t piomap) 
{
	universe_pcisimg_t *         pcisimg;        

	universe_state_t             universe_state;
	universe_pcisimg_state_t *   pcisimg_state;  
	int                          s;

	universe_state = piomap->universe_state;

	if (piomap->uv_flags == VMEIO_PIOMAP_FIXED) {
		if (piomap->uv_am & VMEIO_AM_A32) {
			pcisimg_state = &(universe_state->pcisimgs_fixed
					  [piomap->image_num]);
			
			UNIVERSE_PCISIMG_USERS_DEC_ATOMIC
				(pcisimg_state, universe_state);
			if (UNIVERSE_PCISIMG_USERS_GET(pcisimg_state) == 0) {
				pcisimg = (universe_pcisimg_t *) 
					UNIVERSE_REG_ADDR(universe_state->chip,
							  universe_pcisimg_offset[piomap->image_num]);
				pcisimg->ctl.en = UNIVERSE_PCISIMG_CTL_EN_OFF;
			}
		}

		else if ((piomap->uv_am & VMEIO_AM_A24) ||
			 (piomap->uv_am & VMEIO_AM_A16)) {

			UNIVERSE_PCISIMG_SPEC_USERS_DEC_ATOMIC
				(&(universe_state->pcisimg_spec_state), 
				 universe_state);
		}

		else {
			ASSERT(0);
		}
	}
	else {
		ASSERT(piomap->uv_flags == VMEIO_PIOMAP_UNFIXED);
		
		s = FLOCK_UNIV(universe_state);

		pcisimg_state = &(universe_state->pcisimg_unfixed);

		UNIVERSE_PCISIMG_USERS_DEC(pcisimg_state);
		
		/* Disable the slave image */
		if (UNIVERSE_PCISIMG_USERS_GET(pcisimg_state) == 0) {
			pcisimg = (universe_pcisimg_t *) 
				UNIVERSE_REG_ADDR(universe_state->chip,
						  universe_pcisimg_offset
						  [piomap->image_num]);
			pcisimg->ctl.en = UNIVERSE_PCISIMG_CTL_EN_OFF;
		}

		FUNLOCK_UNIV(universe_state, s);
	}

	DEL(piomap); 
}

/*
 * Get kvaddr from vmeaddr 
 * -- Since we already cached the kvaddr for the PIO map, we can figure the 
 * corresponding kvaddr out by looking at it.
 * Driver use pio_mapaddr() to obtain kv address only for PIOMAP_FIXED piomaps.
 * Otherwise pio_mapaddr() is called from pio_"access" functions only.
 */
/* ARGSUSED */
caddr_t
universe_piomap_addr(universe_piomap_t piomap,
		     iopaddr_t         vmeaddr,
		     size_t            byte_count)
{
	caddr_t kvaddr;

	ASSERT(piomap);
	ASSERT_ALWAYS((vmeaddr >= piomap->uv_vmeaddr) &&
		      (vmeaddr + byte_count  <= 
		       piomap->uv_vmeaddr + piomap->uv_mapsz));

	kvaddr = piomap->uv_kvaddr + 
		(vmeaddr - piomap->uv_vmeaddr);
	ASSERT(kvaddr);

#if VME_DEBUG
	printf("vmeaddr 0x%x => [universe_piomap_addr] => kvaddr 0x%x\n",
		vmeaddr, kvaddr);
#endif 

	return(kvaddr);
}

/* Invalide PIO resources */
/* ARGSUSED */
void
universe_piomap_done(universe_piomap_t universe_piomap)
{
}

/* ------------------------------------------------------------------------
  
                           PIO -- internal functions

   ------------------------------------------------------------------------ */

/*
 * Translate the VMEbus addr to PCIbus addr 
 * Called by: universe_piomap_alloc()
 */
/* ARGSUSED */
static iopaddr_t
universe_pci_addr(universe_state_t         universe_state,
		  vmeio_am_t               am,
		  iopaddr_t                vmeaddr,
		  size_t                   byte_count,
		  universe_pcisimg_num_t * image_num)
{
	universe_pcisimg_t *            pcisimg;

	universe_pcisimg_state_t *      pcisimg_state;
	universe_pcisimg_spec_state_t * pcisimg_spec_state;
	iopaddr_t	                pciaddr;
	int                             i;
	int                             s;
	
#if VME_DEBUG
	printf("universe_pci_addr: am_code 0x%x vmeaddr 0x%x byte_count 0x%x\n", 
	       am, vmeaddr, byte_count);
#endif /* VME_DEBUG */

	if (am & VMEIO_AM_A32) {
		/* Try to find an image covering the fixed mappgins */
		for (i = 0; i < UNIVERSE_NUM_OF_PCISIMGS_FIXED; i++) {
			pcisimg_state = &(universe_state->pcisimgs_fixed[i]);
			s = UNIVERSE_LOCK(universe_state);
			if (pcisimg_state->users != 0) {
				if ((vmeaddr >= (pcisimg_state->pci_base +
						pcisimg_state->to)) &&
				    ((vmeaddr + byte_count) < 
				     (pcisimg_state->pci_bound +
				      pcisimg_state->to)) &&
				    (am == pcisimg_state->am)) {
					pcisimg_state->users++;
					pciaddr = pcisimg_state->pci_base + 
						(vmeaddr - 
						 (pcisimg_state->pci_base + 
						  pcisimg_state->to));
					*image_num = i;
#if VME_DEBUG
					printf("vmeaddr 0x%x => [universe_pci_addr] => pciaddr 0x%x\n",
					       vmeaddr, pciaddr);

					pcisimg = (universe_pcisimg_t *) 
						UNIVERSE_REG_ADDR(universe_state->chip,
								  universe_pcisimg_offset[i]);
					ASSERT(pcisimg->ctl.en == 
					       UNIVERSE_PCISIMG_CTL_EN_ON);
#endif /* VME_DEBUG */
					UNIVERSE_UNLOCK(universe_state, s);
					return(pciaddr);
				}
			}
			UNIVERSE_UNLOCK(universe_state, s);
		}

		/* Try to find an unused image */
		for (i = 0; i < UNIVERSE_NUM_OF_PCISIMGS_FIXED; i++) {
			pcisimg_state = &(universe_state->pcisimgs_fixed[i]);

			s = UNIVERSE_LOCK(universe_state);

			if (pcisimg_state->users == 0) {

			        pcisimg_state->am = am;
				pcisimg_state->to = 
					(UNIVERSE_PCISIMG_ADDR_ALIGN(vmeaddr) -
					 pcisimg_state->pci_base) & 
					ADDR_32_MASK;

				if ((vmeaddr < 
				     ((pcisimg_state->pci_base + 
				       pcisimg_state->to) & ADDR_32_MASK)) ||
				    ((vmeaddr + byte_count) > 
				     ((pcisimg_state->pci_bound +
				      pcisimg_state->to) & ADDR_32_MASK))) {

					UNIVERSE_UNLOCK(universe_state, s);

					return(0);
				}

				pcisimg_state->users++;
				    
				pcisimg = (universe_pcisimg_t *) 
					UNIVERSE_REG_ADDR(universe_state->chip,
							  universe_pcisimg_offset[i]);
				pcisimg->to = pcisimg_state->to;
				if (am & VMEIO_AM_S) {
					pcisimg->ctl.super = 
						UNIVERSE_PCISIMG_CTL_SUPER_S;
				}
				else {
					ASSERT(am & VMEIO_AM_N);
					pcisimg->ctl.super = 
						UNIVERSE_PCISIMG_CTL_SUPER_N;
				}

				if (am & VMEIO_AM_D64) {
					pcisimg->ctl.vdw = 
						UNIVERSE_PCISIMG_CTL_VDW_64;
				}
				else if (am & VMEIO_AM_D32) {
					pcisimg->ctl.vdw =
						UNIVERSE_PCISIMG_CTL_VDW_32;
				}
				else if (am & VMEIO_AM_D16) {
					pcisimg->ctl.vdw =
						UNIVERSE_PCISIMG_CTL_VDW_16;
				}
				else if (am & VMEIO_AM_D8) {
					pcisimg->ctl.vdw =
						UNIVERSE_PCISIMG_CTL_VDW_8;
				}
				else {
					pcisimg->ctl.vdw = 
						UNIVERSE_PCISIMG_CTL_VDW_64;
				}
				
				ASSERT(pcisimg->ctl.en ==
				       UNIVERSE_PCISIMG_CTL_EN_OFF);
				pcisimg->ctl.en = UNIVERSE_PCISIMG_CTL_EN_ON;
				
				pciaddr = pcisimg_state->pci_base + 
					(vmeaddr - (pcisimg_state->pci_base + 
						    pcisimg_state->to));
				*image_num = i;
#if VME_DEBUG
				printf("vmeaddr 0x%x => [universe_pci_addr] => pciaddr 0x%x\n",
				       vmeaddr, pciaddr);
#endif /* VME_DEBUG */

				UNIVERSE_UNLOCK(universe_state, s);

				return(pciaddr);
			}

			UNIVERSE_UNLOCK(universe_state, s);
		}

		pciaddr = 0;
	}

	else if (am & VMEIO_AM_A24) {
		if ((vmeaddr < UNIVERSE_PIO_A24_START) || 
		    (vmeaddr + byte_count - 1 > UNIVERSE_PIO_A24_END)) {
			return(0);
		}
		pcisimg_spec_state = &(universe_state->pcisimg_spec_state);
		if (am & VMEIO_AM_S) {
			if (am & VMEIO_AM_D32) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A24S_D32_BS + 
					vmeaddr;
			}
			else if (am & VMEIO_AM_D16) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A24S_D16_BS + 
					vmeaddr;
			}
			else {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A24S_D32_BS + 
					vmeaddr;
			}
		}
		else if (am & VMEIO_AM_N) {
			if (am & VMEIO_AM_D32) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A24N_D32_BS + 
					vmeaddr;
			}
			else if (am & VMEIO_AM_D16) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A24N_D16_BS + 
					vmeaddr;
			}
			else {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A24N_D32_BS + 
					vmeaddr;
			}
		}
		else {
			ASSERT(0);
			pciaddr = 0;
		}
	}

	else if (am & VMEIO_AM_A16) {
		if (vmeaddr + byte_count - 1 > UNIVERSE_PIO_A16_END) {
			return(0);
		}
		pcisimg_spec_state = &(universe_state->pcisimg_spec_state);
		if (am & VMEIO_AM_S) {
			if (am & VMEIO_AM_D32) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A16S_D32_BS + 
					vmeaddr;
			}
			else if (am & VMEIO_AM_D16) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A16S_D16_BS + 
					vmeaddr;
			}
			else {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A16S_D32_BS + 
					vmeaddr;
			}
		}
		else if (am & VMEIO_AM_N) {
			if (am & VMEIO_AM_D32) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A16N_D32_BS + 
					vmeaddr;
			}
			else if (am & VMEIO_AM_D16) {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A16N_D16_BS + 
					vmeaddr;
			}
			else {
				pciaddr = pcisimg_spec_state->pci_base + 
					UNIVERSE_PCISIMG_SPEC_A16N_D32_BS + 
					vmeaddr;
			}
		}
		else {
			ASSERT(0);
			pciaddr = 0;
		}
	}
	
	else {
		ASSERT(0);
		pciaddr = 0;
	}

#if VME_DEBUG
	printf("vmeaddr 0x%x => [universe_pci_addr] => pciaddr 0x%x\n",
	       vmeaddr, pciaddr);
#endif /* VME_DEBUG */
	return(pciaddr);
}

/* 
 * Get the offset for an address space in the special PCI slave image.
 */
static long
universe_pcisimg_spec_space_offset_get(vmeio_am_t am)
{
	unsigned long offset;

	if (am & VMEIO_AM_A24) {
		if (am & VMEIO_AM_S) {
			if (am & VMEIO_AM_D32) {
				offset = UNIVERSE_PCISIMG_SPEC_A24S_D32_BS;
			}
			else if (am & VMEIO_AM_D16) {
				offset = UNIVERSE_PCISIMG_SPEC_A24S_D16_BS;
			}
			else {
				offset = UNIVERSE_PCISIMG_SPEC_A24S_D32_BS;
			}
		}
		else if (am & VMEIO_AM_N) {
			if (am & VMEIO_AM_D32) {
				offset = UNIVERSE_PCISIMG_SPEC_A24N_D32_BS;
			}
			else if (am & VMEIO_AM_D16) {
				offset = UNIVERSE_PCISIMG_SPEC_A24N_D16_BS;
			}
			else {
				offset = UNIVERSE_PCISIMG_SPEC_A24N_D32_BS;
			}
		}
	}
	else if (am & VMEIO_AM_A16) {
		if (am & VMEIO_AM_S) {
			if (am & VMEIO_AM_D32) {
				offset = UNIVERSE_PCISIMG_SPEC_A16S_D32_BS;
			}
			else if (am & VMEIO_AM_D16) {
				offset = UNIVERSE_PCISIMG_SPEC_A16S_D16_BS;
			}
			else {
				offset = UNIVERSE_PCISIMG_SPEC_A16S_D32_BS;
			}
		}
		else if (am & VMEIO_AM_N) {
			if (am & VMEIO_AM_D32) {
				offset = UNIVERSE_PCISIMG_SPEC_A16N_D32_BS;
			}
			else if (am & VMEIO_AM_D16) {
				offset = UNIVERSE_PCISIMG_SPEC_A16N_D16_BS;
			}
			else {
				offset = UNIVERSE_PCISIMG_SPEC_A16N_D32_BS;
			}
		}
	}
	else {
		ASSERT(0);
		offset = -1;
	}

	return(offset);
}

static void
universe_piomaps_add(universe_state_t  universe_state,
		     universe_piomap_t piomap)
{
	universe_piomap_list_item_t  item;

	NEW(item);
	ASSERT(item);
	item->piomap = piomap;
		
	universe_piomaps_lock(universe_state->piomaps);

	item->next = universe_state->piomaps->head;
	universe_state->piomaps->head = item;

	universe_piomaps_unlock(universe_state->piomaps);
}

static void
universe_piomaps_delete(universe_state_t  universe_state,
			universe_piomap_t piomap)
{
	universe_piomap_list_item_t  item, prev_item;

	universe_piomaps_lock(universe_state->piomaps);
	
	for (item = universe_state->piomaps->head; 
	     item; 
	     prev_item = item, item = item->next) {
		if (item->piomap == piomap) {
			if (item == universe_state->piomaps->head) {
				universe_state->piomaps->head = item->next;
			}
			else {
				prev_item->next = item->next;
			}

			universe_piomaps_unlock(universe_state->piomaps);

			DEL(item);

			return;
		}
	}


	ASSERT(0);
}

static iopaddr_t
universe_piomap_pciaddr_to_vmeaddr(universe_piomap_t piomap,
				   iopaddr_t         pciaddr)
{
	universe_state_t                universe_state;
	universe_pcisimg_num_t          image_num;
	universe_pcisimg_state_t *      pcisimg_state;
	iopaddr_t                       vmeaddr;
	
	universe_state = piomap->universe_state;
	
	if (piomap->uv_am & VMEIO_AM_A32) {
		image_num = piomap->image_num;
		pcisimg_state = &(universe_state->pcisimgs_fixed[image_num]);
		vmeaddr = (pciaddr + pcisimg_state->to) & ADDR_32_MASK;
	}
	else if (piomap->uv_am & VMEIO_AM_A24) {
		vmeaddr = pciaddr & ADDR_24_MASK;
	}
	else {
		ASSERT(piomap->uv_am & VMEIO_AM_A16);
		vmeaddr = pciaddr & ADDR_16_MASK;
	}

	return(vmeaddr);
}
	
static universe_piomap_t
universe_piomap_lookup_by_pciaddr(universe_state_t universe_state, 
				  iopaddr_t        pci_addr)
{
	universe_piomap_list_item_t item;
	universe_piomap_t           piomap;

	/* Find the device by the given VMEbus address */
	for (item = universe_state->piomaps->head; 
	     item;
	     item = item->next) {
		piomap = item->piomap;
		if ((pci_addr >= piomap->pci_addr) && 
		    (pci_addr < piomap->pci_addr + piomap->uv_mapsz)) {
			return(piomap);
		}
	}
	
	return(0);
}

static universe_piomap_t
universe_piomap_lookup_by_vmeaddr(universe_state_t universe_state, 
				  iopaddr_t        vmeaddr)
{
	universe_piomap_list_item_t piomap_item;
	universe_piomap_t           piomap;

	/* Find the device by the given VMEbus address */
	for (piomap_item = universe_state->piomaps->head; 
	     piomap_item != 0;
	     piomap_item = piomap_item->next) {
		piomap = piomap_item->piomap;
		if ((vmeaddr >= piomap->uv_vmeaddr) && 
		    (vmeaddr < piomap->uv_vmeaddr + piomap->uv_mapsz)) {
			return(piomap);
		}
	}
	
	return(0);
}

/* ------------------------------------------------------------------------
  
                      PIO bcopy -- extern interface

   ------------------------------------------------------------------------ */

/* 
 * Copy data from VMEbus to System 
 */

/* ARGSUSED */
size_t
universe_pio_bcopyin(universe_piomap_t piomap,
		     iopaddr_t	       vmeaddr, /* VMEbus addr of the source */
		     caddr_t	       dest_sys_addr, /* Sys addr of the destination */
		     size_t	       size,    /* Size of the transfer */
		     int	       itmsz,             
		     unsigned int      flags)
{
	caddr_t src_sys_addr;
	int cnt;
	size_t msize;
	size_t totcnt;

#if VME_DEBUG
	printf("universe_pio_bcopyin: piomap = 0x%x vmeaddr = 0x%x dest_addr = 0x%x size = %d itmsz = %d flags = 0x%x\n", piomap, vmeaddr, dest_sys_addr, size, itmsz, flags);
#endif

	/* basic range checking */
	/* XXX Is it valid for UNFIXED Map ? */
	if( (vmeaddr < piomap->uv_vmeaddr) || 
	    (vmeaddr >= piomap->uv_vmeaddr + piomap->uv_mapsz) ) {
		return(0);
	}
	
	if (piomap->uv_flags == VMEIO_PIOMAP_FIXED) {
		/* 
		 * Since the resources are fixed, we just get the 
		 * the source system address by the given resources and
		 * source VMEbus address.
		 */ 
		src_sys_addr = universe_piomap_addr(piomap, vmeaddr, size);

		
		totcnt = bcopyn(src_sys_addr, dest_sys_addr, size, itmsz);

		return(totcnt);
	}
	else {
		ASSERT(piomap->uv_flags == VMEIO_PIOMAP_UNFIXED);

		/* Lock the underlying PCI slave image */
		universe_piomap_lock(piomap);

		for(totcnt = msize = cnt = 0 ; 
		    (totcnt < size) || (cnt != msize) ; 
		    totcnt += cnt ) {

			msize = universe_piomap_adjust(piomap, vmeaddr, size);

			if(msize == 0) {
				break;
			}

			ASSERT(msize <= size);

			src_sys_addr = universe_piomap_addr(piomap, 
							    vmeaddr,
							    msize);
			cnt = bcopyn(src_sys_addr, dest_sys_addr, 
				     msize, itmsz);
	
			/* 
			 * Roll the VMEbus addr and destination system addr
			 * to the new bcopy position.
			 */
			vmeaddr += cnt;
			
			/* XXX */
			dest_sys_addr = (void *)((__psunsigned_t)
						 dest_sys_addr + 
						 cnt); 
		}

		/* Unlock the underlying PCI slave image */
		universe_piomap_unlock(piomap);
		return(totcnt);
	}
}

/* 
 * Copy data from systme to VMEbus
 */

/* ARGSUSED */
size_t
universe_pio_bcopyout(universe_piomap_t piomap,
		      iopaddr_t		vmeaddr,
		      caddr_t		src_sys_addr,
		      size_t		size,
		      int		itmsz,
		      unsigned int	flags)
{
	caddr_t dest_sys_addr;
	int cnt;
	size_t totcnt, msize; 

#if VME_DEBUG
	printf("universe_pio_bcopyout: piomap = 0x%x vmeaddr = 0x%x src_addr = 0x%x size = %d itmsz = %d flags = 0x%x\n", piomap, vmeaddr, src_sys_addr, size, itmsz, flags);
#endif

	/* Basic range checking */
	/* XXX */
	if((vmeaddr < piomap->uv_vmeaddr) || 
	   (vmeaddr >= piomap->uv_vmeaddr + piomap->uv_mapsz)) {
		return 0;
	}

	/* The simple case */
	if(piomap->uv_flags == VMEIO_PIOMAP_FIXED) {
		dest_sys_addr = universe_piomap_addr(piomap, vmeaddr, size);
		totcnt = bcopyn(src_sys_addr, dest_sys_addr, size, itmsz);
		return(totcnt);
	}
	else {
		ASSERT(piomap->uv_flags == VMEIO_PIOMAP_UNFIXED);
		
		/* Lock the underlying PCI slave image */
		universe_piomap_lock(piomap);

		for(totcnt = msize = cnt = 0; 
		    (totcnt < size) || (cnt != msize); 
		    totcnt += cnt) {

			msize = universe_piomap_adjust(piomap, vmeaddr, size);

			if(msize == 0) {
				break;
			}

			ASSERT(msize <= size);

			dest_sys_addr = universe_piomap_addr(piomap, 
							     vmeaddr,
							     msize);
			cnt = bcopyn(src_sys_addr, dest_sys_addr, 
				     msize, itmsz);

			vmeaddr += cnt;
			src_sys_addr = (void *)((__psunsigned_t)src_sys_addr +
						cnt);
		}

		/* Unlock the underlying PCI slave image */
		universe_piomap_unlock(piomap);

		return totcnt;
	}
}

/* ------------------------------------------------------------------------
  
   PIO bcopy: internal supportive functions -- Resource Reservation

   ------------------------------------------------------------------------ */

/*
 * 	Lock down the resources needed for Unfixed mapping. 
 *	This routine will try to lock the unfixed mapping, and if it 
 *	doesnot get the resource for a while, it's going to spin
 *	waiting for the resource. This can't be changed to sleeping
 *	since this routine can be called from interrupt handling.
 *
 *	CONCERN: This routine is always supposed to succeed, but
 *	there are situations where it could perhaps fail.
 *	Need a way to address it without panicing.
 * 
 * Called by: 
 *     universe_pio_bcopyin()
 *     universe_pio_bcopyout()
 */

static void
universe_piomap_lock(universe_piomap_t piomap)
{
	universe_state_t           universe_state;
	universe_pcisimg_state_t * pcisimg_state;
	int		           s;
	int                        loopcount = 128;  /* XXX */

	ASSERT(piomap->uv_flags == VMEIO_PIOMAP_UNFIXED);

	/* Get the state of the Universe adapter */
	universe_state = piomap->universe_state;
	ASSERT(universe_state);

	if (piomap->uv_am & VMEIO_AM_A32) {
		pcisimg_state = &(universe_state->pcisimg_unfixed);

		/* Loop till we get hold of the image */
		while (--loopcount) {
			s = FLOCK_UNIV(universe_state);

			/* 
			 * Check if the unfixed image is in use.
			 * Increment the user count to effectively lock the
			 * image if no one is using it.
			 */
			if (UNIVERSE_PCISIMG_USERS_GET(pcisimg_state) == 0) {
				UNIVERSE_PCISIMG_USERS_INC(pcisimg_state);

				/* Unlock the state of Universe adapter */
				FUNLOCK_UNIV(universe_state, s); 

				return;
			}

			FUNLOCK_UNIV(universe_state, s);
		}
	}
	else {
		ASSERT(0);
	}

	return;
}

/* 
 * Unlock the underlying PCI slave image
 * Called by: 
 *     univrese_pio_bcopyin()
 *     universe_pio_bcopyout()
 */

static void
universe_piomap_unlock(universe_piomap_t piomap)
{
	universe_pcisimg_t *         pcisimg;

	universe_state_t             universe_state;
	universe_pcisimg_state_t *   pcisimg_state;
	int		             s;

	ASSERT(piomap->uv_flags == VMEIO_PIOMAP_FIXED);

	/* Get the state of the Universe adapter */
	universe_state = piomap->universe_state;     
	ASSERT(universe_state); 

	if (piomap->uv_am & VMEIO_AM_A32) {
		/* 
		 * Reduce the numusers on unfixed mapping and free if
		 * we are the last users
		 */
		pcisimg_state = &(universe_state->pcisimg_unfixed);

		s = FLOCK_UNIV(universe_state);

		UNIVERSE_PCISIMG_USERS_DEC(pcisimg_state);

		if (UNIVERSE_PCISIMG_USERS_GET(pcisimg_state) == 0) {
			pcisimg = (universe_pcisimg_t *) 
				UNIVERSE_REG_ADDR(universe_state->chip,
						  universe_pcisimg_offset
						  [piomap->image_num]);
			pcisimg->ctl.en = UNIVERSE_PCISIMG_CTL_EN_OFF;
		}

		FUNLOCK_UNIV(universe_state, s);
	}
	else {
		ASSERT(0);
	}

	return;
}

/*
 * Routine called to do unfixed mapping.
 * Remember that unvme_pio_map_vme is called to either set up the mapping
 * the first time around, or readjust the mapping to point to a 
 * new window, if not setup properly. 
 * Also remember that no other unfixed image users can use this
 * map at the same time, unless we come up with a complex scheme
 * to rendezvous the multiple Unfixed image users !! 
 * Sync: Lock on the state of the Universe adapter held by the caller
 * Return: 
 */
static size_t
universe_piomap_adjust(universe_piomap_t piomap, 
		       iopaddr_t         vmeaddr, 
		       size_t            size)
{
	if (piomap->uv_am & VMEIO_AM_A32) {
		if (piomap->uv_flags == VMEIO_PIOMAP_FIXED){
			ASSERT(piomap->uv_mapsz <= size);
			return(size);
		}

		return(universe_pio_use_unfix(piomap, vmeaddr, size));
	}
	else if ((piomap->uv_am & VMEIO_AM_A24) ||
		 (piomap->uv_am & VMEIO_AM_A16)) {
		ASSERT(piomap->uv_mapsz >= size);
		return(size);
	}
	else {
		ASSERT(0);
	}

	return(0);
}

/* 
 * Map the Floating image to the mapping requirements suggested by piomap
 * This sets up the data structure, and also updates the PCI Slave image
 * registers. 
 * Expects the Lock for the Floating image to be held by the caller.
 * Return: 0 if mapping could not be done.
 *	   size of mapping otherwise
 */

/* REFERENCED */
static int
universe_pio_use_unfix(universe_piomap_t piomap, 
		       iopaddr_t         vmeaddr, 
		       size_t            size)
{
	universe_pcisimg_t *       pcisimg;

	universe_state_t           universe_state;
	universe_pcisimg_state_t * pcisimg_state;

	universe_state = piomap->universe_state;
	ASSERT(universe_state); 

	ASSERT(piomap->uv_am & VMEIO_AM_A32);
	ASSERT(piomap->uv_flags == VMEIO_PIOMAP_UNFIXED);

	pcisimg_state = &(universe_state->pcisimg_unfixed);
	
	ASSERT(UNIVERSE_PCISIMG_USERS_GET(pcisimg_state) == 1);	
	
	if (size > UNIVERSE_PCISIMG_SIZE_GET(pcisimg_state)) {
		/* If bigger than what this map can support, adjust it. */
		size = UNIVERSE_PCISIMG_SIZE_GET(pcisimg_state);
	}

	/* Adjust the TO to be properly aligned. */
	pcisimg_state->to = vmeaddr & ~ALIGN_4K; 
	
	/*
	 * Set the control of the slave image:
	 * -- Enable the image
	 * -- Specify the addr modifier mode (supervisory or non-privilidged)
	 * Only A32 unfixed mapping are supposed to call 
	 * this routine. So, assert it, and setup the control register
	 * appropriately.
	 */
	pcisimg = (universe_pcisimg_t *) 
		UNIVERSE_REG_ADDR(universe_state->chip,
				  universe_pcisimg_offset[piomap->image_num]);

	if (piomap->uv_am & VMEIO_AM_N) {
		pcisimg->ctl.super = UNIVERSE_PCISIMG_CTL_SUPER_N;
	}
	else if (piomap->uv_am & VMEIO_AM_S) {
		pcisimg->ctl.super = UNIVERSE_PCISIMG_CTL_SUPER_S;
	}
	else {
		ASSERT(0);
	}

	pcisimg->ctl.en = UNIVERSE_PCISIMG_CTL_EN_ON;
	pcisimg->to = pcisimg_state->to;
	
	/* XXX, set PCIbus addr too -- kvaddr should be inferred */	
	piomap->uv_kvaddr = pcisimg_state->kv_base + vmeaddr - 
		(vmeaddr & ALIGN_4K);

	/* Normalize it wrt the base IO address of the PIOmap.
	 * This is needed for proper calculation in pio_mapaddr, where
	 * we do return piomap->pio_vaddr + (addr - piomap->pio_iopaddr)
	 * THIS COULD MAKE pio_vaddr Negative. Is that a CONCERN ??
	 * XXX Think this through again.
	 */
	piomap->uv_kvaddr -=  (vmeaddr - piomap->uv_vmeaddr);
	
	return(size);		
}

/* ------------------------------------------------------------------------
  
   PIO bcopy: internal supportive functions -- copy engines

   ------------------------------------------------------------------------ */

/* REFERENCED */
static int
bcopyn(void *src, void *dst, int count, int itmsz)
{
	switch(itmsz) {
		case 1: /* XXX */
			return(bcopyb(src, dst, count));
		case 2:
			return(bcopyh(src, dst, count));
		case 4:
			return(bcopyw(src, dst, count));
		case 8:
			return(bcopyd(src, dst, count));
	}

	return -1;
}

/* REFERENCED */
static int
bcopyb(register char *src, register char *dest, int count)
{
	register int i;

	for(i = 0; i < count; i++) {
		*dest++ = *src++;
	}

	return count;
}

/* REFERENCED */
static int
bcopyh(register short *src, register short *dest, int count)
{
	register int i, rem;

	/* Make sure alignments are correct */
	if(((__psunsigned_t)src & 1) || ((__psunsigned_t)dest & 1)) {
		return(-1);
	}

	rem = count % 2;
	if (rem)
		count -= rem;

	for(i = 0; i < count/2; i++) {
		*dest++ = *src++;
	}

	if (bcopyb((char *)src, (char *)dest, rem) == -1)
		return(-1);

	return(count + rem);
}

/* REFERENCED */
static int
bcopyw(register int *src, register int *dest, register int count)
{
	register int i, rem;

	/* Make sure alignments are correct */
	if(((__psunsigned_t)src & 3) || ((__psunsigned_t)dest & 3)) {
		return -1;
	}

	rem = count % 4;
	if (rem)
		count -= rem;

	for(i = 0; i < count/4; i++) {
		*dest++ = *src++;
	}

	if (bcopyh((short *)src, (short *)dest, rem) == -1)
		return(-1);

	return(count + rem);
}

/* REFERENCED */
static int
bcopyd(register long long *src, register long long *dest, int count)
{
	register int i, rem;

	/* Make sure alignments are correct */
	if(((__psunsigned_t)src & 7) || ((__psunsigned_t)dest & 7)) {
		return -1;
	}

	rem = count % 8;
	if (rem)
		count -= rem;

	for(i = 0 ; i < count/8; i++) {
		*dest++ = *src++;
	}

	if (bcopyw((int *)src, (int *)dest, rem) == -1)
		return(-1);

	return(count + rem);
}


/* -----------------------------------------------------------------------
   
                          DMA -- initialization 

   ----------------------------------------------------------------------- */

/*
 * Prepare the Universe hardware and software for DMA.
 * Return: 0 if success
 */

static int
universe_dma_setup(universe_state_t universe_state)
{
	int                       i;
	universe_vmesimg_t *      vmesimg;
	universe_dmamap_list_t    dmamaps;

	/* Program hardware */
	for(i = 0; i < UNIVERSE_NUM_OF_VMESIMGS; i++){
		vmesimg = (universe_vmesimg_t *) 
			UNIVERSE_REG_ADDR(universe_state->chip,
					  universe_vmesimg_offset[i]);
		ASSERT(vmesimg->ctl.en == UNIVERSE_VMESIMG_CTL_EN_OFF);
		ASSERT(vmesimg->ctl.pwen == UNIVERSE_VMESIMG_CTL_PWEN_OFF);
		ASSERT(vmesimg->ctl.pren == UNIVERSE_VMESIMG_CTL_PREN_OFF);
		ASSERT(vmesimg->ctl.pgm == UNIVERSE_VMESIMG_CTL_PGM_BOTH);
		ASSERT(vmesimg->ctl.super == UNIVERSE_VMESIMG_CTL_SUPER_BOTH);
		vmesimg->ctl.ld64en = UNIVERSE_VMESIMG_CTL_LD64EN_ON;
		vmesimg->ctl.llrmw = UNIVERSE_VMESIMG_CTL_LLRMW_OFF;
		ASSERT(vmesimg->ctl.las == UNIVERSE_VMESIMG_CTL_LAS_MEM);
	}

	/* Initialize DMA map list */
	dmamaps = 0;
	NEW(dmamaps);
	ASSERT(dmamaps);
	ASSERT(dmamaps->head == 0);
	initnsema(dmamaps->lock, 1, "vme_dmamaps_lock");
	universe_state->dmamaps = dmamaps; 
	
	return(0);
}

/* ------------------------------------------------------------------------
  
			  DMA -- exported interface
   
   ------------------------------------------------------------------------ */

/*
 * Allocate the resources needed for the DMA operation
 * Called by: vmeio_dmamap_alloc()
 */

/* ARGSUSED */
universe_dmamap_t
universe_dmamap_alloc(vertex_hdl_t  dev,
		      device_desc_t dev_desc,
		      vmeio_am_t    am,
		      size_t        byte_count_max,
		      unsigned      flags)
{
	vmeio_info_t               info;
	universe_state_t           universe_state;
	unsigned                   pci_flags;
	pciio_dmamap_t             pci_dmamap;
	universe_vmesimg_num_t     image_num;
	universe_vmesimg_state_t * vmesimg_state;
	universe_vmesimg_t *       vmesimg;
	iopaddr_t                  vmeaddr; 
	universe_dmamap_t          dmamap;
	int                        s;
	int                        i;

	if (byte_count_max == 0) {
		return(0);
	}

	info = vmeio_info_get(dev);
	universe_state = (universe_state_t)
		vmeio_info_provider_fastinfo_get(info);

	/* Allocate PCI resources */
	if (am & VMEIO_AM_A32) {
		if (byte_count_max > VMEIO_DMA_A32_SIZE) {
			return(0);
		}
		if (flags & VMEIO_DMA_DATA) {
			pci_flags = PCIIO_DMA_DATA;
		}
		else {
			pci_flags = PCIIO_DMA_CMD;
		}
	}
	else if (am & VMEIO_AM_A24) {
		if (byte_count_max > VMEIO_DMA_A24_SIZE) {
			return(0);
		}
		if (flags & VMEIO_DMA_DATA) {
			pci_flags = PCIIO_DMA_DATA;
		}
		else {
			pci_flags = PCIIO_DMA_CMD;
		}
	}
	else {
		cmn_err(CE_PANIC, "Unsupported VME DMA space");
	}

	pci_dmamap = pciio_dmamap_alloc(universe_state->pci_conn,
					0,
					byte_count_max,
					PCIIO_BYTE_STREAM | pci_flags);
	if (pci_dmamap == 0) {
		return(0);
	}

	/* Allocate VME resources */
	image_num = -1;
	s = UNIVERSE_LOCK(universe_state);
	for (i = 0; i < UNIVERSE_NUM_OF_VMESIMGS; i++) {
		if (universe_state->vmesimgs[i].users == 0) {
			universe_state->vmesimgs[i].users++;
			image_num = i;
			break;
		}
	}
	if (image_num == -1) {
		UNIVERSE_UNLOCK(universe_state, s);
		return(0);
	}

	vmeaddr = VMEIO_DMA_INVALID_ADDR;

	UNIVERSE_UNLOCK(universe_state, s);

	vmesimg_state = &(universe_state->vmesimgs[image_num]);

	/* Program hardware */
	vmesimg = (universe_vmesimg_t *) 
		UNIVERSE_REG_ADDR(universe_state->chip,
				  universe_vmesimg_offset[image_num]);
	if (am & VMEIO_AM_A32) {
		vmesimg->ctl.vas = UNIVERSE_VMESIMG_CTL_VAS_A32;
		vmesimg->to = UNIVERSE_VMESIMG_A32_TO;
		vmesimg_state->to = UNIVERSE_VMESIMG_A32_TO;
	}
	else {
		vmesimg->ctl.vas = UNIVERSE_VMESIMG_CTL_VAS_A24;
		vmesimg->to = UNIVERSE_VMESIMG_A24_TO;
		vmesimg_state->to = UNIVERSE_VMESIMG_A24_TO;
	}

	if (flags & VMEIO_DMA_DATA) {
#if UNIVERSE_REVNUM_WAR
		if (universe_state->board_rev >= UNIVERSE_CHIP_MR_VERSION) {
			vmesimg->ctl.pwen = UNIVERSE_VMESIMG_CTL_PWEN_ON;
		}
#else 
		vmesimg->ctl.pwen = UNIVERSE_VMESIMG_CTL_PWEN_ON;
#endif /* UNIVERSE_REVNUM_WAR */
		vmesimg->ctl.pren = UNIVERSE_VMESIMG_CTL_PREN_ON;
	}
	else {
		ASSERT(vmesimg->ctl.pwen == UNIVERSE_VMESIMG_CTL_PWEN_OFF);
		ASSERT(vmesimg->ctl.pren == UNIVERSE_VMESIMG_CTL_PREN_OFF);
	}

	/*
	 * Assemble the DMA map
	 */
       	NEW(dmamap);
	if (dmamap == 0) {
		return(0);
	}

	dmamap->ud_dev = dev;
	dmamap->ud_am = am;
	dmamap->ud_vmeaddr = vmeaddr;
	dmamap->ud_mapsz = byte_count_max;
	dmamap->ud_flags = flags;
	dmamap->pci_dmamap = pci_dmamap;
	dmamap->image_num = image_num;
	dmamap->universe_state = universe_state;

#if VME_DEBUG
	printf("vmeio_dmamap_alloc(): image_num %d\n", image_num);
#endif

	universe_dmamaps_add(universe_state, dmamap);

	return(dmamap);
}

/* 
 * Free the resources allocated for DMA 
 * -- Free PCI resources
 * -- Release VME slave image
 */
void
universe_dmamap_free(universe_dmamap_t dmamap)
{
	universe_state_t             universe_state;
	universe_vmesimg_state_t *   vmesimg_state;
	universe_vmesimg_t *         vmesimg;
	int                          s;

	pciio_dmamap_free(dmamap->pci_dmamap);

	universe_state = dmamap->universe_state;
	vmesimg_state = &(universe_state->vmesimgs[dmamap->image_num]);

	s = UNIVERSE_LOCK(universe_state);
	ASSERT(vmesimg_state->users == 1);
	vmesimg_state->users--;
	UNIVERSE_UNLOCK(universe_state, s);

	universe_dmamaps_delete(universe_state, dmamap);
	DEL(dmamap);

	vmesimg = (universe_vmesimg_t *) 
		UNIVERSE_REG_ADDR(universe_state->chip,
				  universe_vmesimg_offset[dmamap->image_num]);

	/* Clear hardware */
	if (dmamap->ud_flags & VMEIO_DMA_DATA) {
#if UNIVERSE_REVNUM_WAR
		if (universe_state->board_rev >= UNIVERSE_CHIP_MR_VERSION) {
			vmesimg->ctl.pwen = UNIVERSE_VMESIMG_CTL_PWEN_OFF;
		}
#else
		vmesimg->ctl.pwen = UNIVERSE_VMESIMG_CTL_PWEN_OFF;
#endif /* UNIVERSE_REVNUM_WAR */
		vmesimg->ctl.pren = UNIVERSE_VMESIMG_CTL_PREN_OFF;
	}
	else {
		ASSERT(vmesimg->ctl.pwen == UNIVERSE_VMESIMG_CTL_PWEN_OFF);
		ASSERT(vmesimg->ctl.pren == UNIVERSE_VMESIMG_CTL_PREN_OFF);
	}

	vmesimg->ctl.en = UNIVERSE_VMESIMG_CTL_EN_OFF;
}

/* 
 * Establish the mapping from phys addr range to VME bus addr range
 * Return: VME bus address mapped to the physical addr range if success,
 *         0 otherwise.
 */

iopaddr_t
universe_dmamap_addr(universe_dmamap_t dmamap,     /* The DMA mapping used */
		     paddr_t           paddr,      /* System phys addr     */
		     size_t            byte_count) /* Num of bytes to 
						      be used              */
{
	iopaddr_t              pciaddr;
	iopaddr_t              vmeaddr;
	universe_reg_t         to;
	universe_vmesimg_t *   vmesimg;
	
	ASSERT(dmamap);

	ASSERT_ALWAYS(byte_count <= dmamap->ud_mapsz);

	pciaddr = pciio_dmamap_addr(dmamap->pci_dmamap, paddr, byte_count);
	if (pciaddr == 0){
		return(VMEIO_DMA_INVALID_ADDR);
	}

	dmamap->pciaddr = pciaddr;

	/* Program hardware */
	vmesimg = (universe_vmesimg_t *) 
		UNIVERSE_REG_ADDR(dmamap->universe_state->chip,
				  universe_vmesimg_offset[dmamap->image_num]);

	if (dmamap->ud_am & VMEIO_AM_A32) {
		to = vmesimg->to;
		vmeaddr = (iopaddr_t) (__uint32_t) (pciaddr - to);
	}
	else {
		ASSERT(dmamap->ud_am & VMEIO_AM_A24);
		to = vmesimg->to;
		vmeaddr = (iopaddr_t) (__uint32_t) (pciaddr - to);

	}

	vmesimg->bs = UNIVERSE_VMESIMG_ADDR_TO_BS(vmeaddr);
	vmesimg->bd = UNIVERSE_VMESIMG_ADDR_TO_BD(vmeaddr + byte_count);
		
	vmesimg->ctl.en = UNIVERSE_VMESIMG_CTL_EN_ON;

#if VME_DEBUG
	printf("universe_dmamap_addr: phys 0x%x -> pci 0x%x -> vme 0x%x to -> 0x%x\n",
	       paddr, pciaddr, vmeaddr, to);
#endif

	return(vmeaddr);
}


alenlist_t
universe_dmamap_list(universe_dmamap_t dmamap,
		     alenlist_t        phys_alenlist,
		     unsigned          flags) 
{
	alenlist_t           pci_alenlist;
	alenlist_t           vme_alenlist;
	unsigned             alenlist_flags;
	iopaddr_t            pciaddr;
	iopaddr_t            vmeaddr;
	size_t               byte_count;
	int                  rv;
	iopaddr_t            min_vmeaddr;
	iopaddr_t            max_vmeaddr;
	size_t               max_byte_count;
	universe_reg_t       to;
	universe_vmesimg_t * vmesimg;

	alenlist_flags = (flags & VMEIO_NOSLEEP) ? AL_NOSLEEP : 0;

	pci_alenlist = pciio_dmamap_list(dmamap->pci_dmamap,
					 phys_alenlist,
					 universe_to_pciio_flags(flags));
	if (pci_alenlist == 0) {
		return(0);
	}

	alenlist_cursor_init(pci_alenlist, 0, 0);

	if (flags & VMEIO_INPLACE) {
		vme_alenlist = pci_alenlist;
	}
	else {
		vme_alenlist = alenlist_create((flags & VMEIO_NOSLEEP) ?
					       AL_NOSLEEP : 0);
		if (vme_alenlist == 0) {
			alenlist_done(pci_alenlist);
			return(0);
		}
	}

	vmesimg = (universe_vmesimg_t *) 
		UNIVERSE_REG_ADDR(dmamap->universe_state->chip,
				  universe_vmesimg_offset[dmamap->image_num]);

	if (dmamap->ud_am & VMEIO_AM_A32) {
		min_vmeaddr = VMEIO_DMA_A32_END;
		max_vmeaddr = VMEIO_DMA_A32_START;
		to = vmesimg->to;
	}
	else {
		min_vmeaddr = VMEIO_DMA_A24_END;
		max_vmeaddr = VMEIO_DMA_A24_START;
		to = vmesimg->to;
	}

	while (alenlist_get(pci_alenlist, 
			    0, 
			    0, 
			    (alenaddr_t *) &pciaddr, 
			    &byte_count, 
			    alenlist_flags) == 
	       ALENLIST_SUCCESS) {

		if (dmamap->ud_am & VMEIO_AM_A32) {
			vmeaddr = (pciaddr - to) & ADDR_32_MASK;
		}
		else {
			ASSERT(dmamap->ud_am & VMEIO_AM_A24);
			vmeaddr = (pciaddr - to) & ADDR_24_MASK;
		}

		if (vmeaddr < min_vmeaddr) {
			min_vmeaddr = vmeaddr;
		}
		if (vmeaddr > max_vmeaddr) {
			max_vmeaddr = vmeaddr;
			max_byte_count = byte_count;
		}
		
		if (flags & VMEIO_INPLACE) {
			rv = alenlist_replace(vme_alenlist, 
					      0,
					      (alenaddr_t *) &vmeaddr, 
					      &byte_count, 
					      alenlist_flags);
			if (rv != ALENLIST_SUCCESS) {
				return(0);
			}
		}
		else {
			rv = alenlist_append(vme_alenlist, vmeaddr,
					     byte_count, alenlist_flags);
			if (rv != ALENLIST_SUCCESS) {
				alenlist_done(vme_alenlist);
				return(0);
			}
		}
	}

	if (!(flags & VMEIO_INPLACE)) {
		alenlist_done(pci_alenlist);
	}

	alenlist_cursor_init(vme_alenlist, 0, 0);

	/* Program hardware */
	vmesimg->bs = UNIVERSE_VMESIMG_ADDR_TO_BS(min_vmeaddr);
	vmesimg->bd = UNIVERSE_VMESIMG_ADDR_TO_BD(max_vmeaddr + 
						  max_byte_count);
	vmesimg->ctl.en = UNIVERSE_VMESIMG_CTL_EN_ON;

#if VME_DEBUG
	printf("universe_dmamap_list: pci 0x%x -> vme 0x%x to -> 0x%x\n",
	       pciaddr, vmeaddr, to);
	printf("universe_dmamap_list: vmesimg->bs = 0x%x vmesimg->bd = 0x%x\n",vmesimg->bs, vmesimg->bd);
#endif

	return(vme_alenlist);
}

/*
 * Invalide DMA mapping
 */
void
universe_dmamap_done(universe_dmamap_t dmamap)
{
	universe_vmesimg_t *  vmesimg;

	pciio_dmamap_done(dmamap->pci_dmamap);

	/* Disable the image */
	vmesimg = (universe_vmesimg_t *) 
		UNIVERSE_REG_ADDR(dmamap->universe_state->chip,
				  universe_vmesimg_offset[dmamap->image_num]);
	vmesimg->ctl.en = UNIVERSE_VMESIMG_CTL_EN_OFF;
}

/* ------------------------------------------------------------------------
  
   DMA mapping -- internal functions

   ------------------------------------------------------------------------ */

static unsigned
universe_to_pciio_flags(unsigned flags)
{
	unsigned int pciio_flags;

	pciio_flags = 0;
	if (flags & VMEIO_NOSLEEP) {
		pciio_flags |= PCIIO_NOSLEEP;
	}

	if (flags & VMEIO_INPLACE) {
		pciio_flags |= PCIIO_INPLACE;
	}

	if (flags & VMEIO_DMA_CMD) {
		pciio_flags |= PCIIO_DMA_CMD;
	}

	if (flags & VMEIO_DMA_DATA) {
		pciio_flags |= PCIIO_DMA_DATA;
	}

	return(pciio_flags);
}

static void
universe_dmamaps_add(universe_state_t  universe_state,
		     universe_dmamap_t dmamap)
{
	universe_dmamap_list_item_t  item;

	NEW(item);
	ASSERT(item);
	item->dmamap = dmamap;
		
	universe_dmamaps_lock(universe_state->dmamaps);

	item->next = universe_state->dmamaps->head;
	universe_state->dmamaps->head = item;

	universe_dmamaps_unlock(universe_state->dmamaps);
}

static void
universe_dmamaps_delete(universe_state_t  universe_state,
			universe_dmamap_t dmamap)
{
	universe_dmamap_list_item_t  item, prev_item;

	universe_dmamaps_lock(universe_state->dmamaps);
	
	for (item = universe_state->dmamaps->head; 
	     item; 
	     prev_item = item, item = item->next) {
		if (item->dmamap == dmamap) {
			if (item == universe_state->dmamaps->head) {
				universe_state->dmamaps->head = item->next;
			}
			else {
				prev_item->next = item->next;
			}

			universe_dmamaps_unlock(universe_state->dmamaps);

			DEL(item);

			return;
		}
	}


	ASSERT(0);
}

/* 
 * Flush DMA FIFOs 
 * -- actually, wait for DMA FIFOs to be empty 
 */
static void
universe_dma_flush(universe_state_t universe_state)
{
	universe_state->chip->mast_ctl.vown = UNIVERSE_MAST_CTL_VOWN_ACQ;
	while(universe_state->chip->mast_ctl.vown_ack == 
	      UNIVERSE_MAST_CTL_VOWN_ACK_NO);
	while(universe_state->chip->misc_stat.txfe == 
	      UNIVERSE_MISC_STAT_TXFE_NOTEMPTY);
	while(universe_state->chip->misc_stat.rxfe == 
	      UNIVERSE_MISC_STAT_RXFE_NOTEMPTY);
	universe_state->chip->mast_ctl.vown = UNIVERSE_MAST_CTL_VOWN_REL;
}

static universe_dmamap_t
universe_dmamap_lookup_by_pciaddr(universe_state_t universe_state, 
				  iopaddr_t        pciaddr)
{
	universe_dmamap_list_item_t item;
	universe_dmamap_t           dmamap;

	/* Find the device by the given VMEbus address */
	for (item = universe_state->dmamaps->head; 
	     item;
	     item = item->next) {
		dmamap = item->dmamap;
		if ((pciaddr >= dmamap->pciaddr) && 
		    (pciaddr < dmamap->pciaddr + dmamap->ud_mapsz)) {
			return(dmamap);
		}
	}
	
	return(0);
}
		  
/* -----------------------------------------------------------------------
  
                    interrupt handling -- initialization 

   ------------------------------------------------------------------------ */

/*
 * Set up the interrupt support of the Univserse bridge
 */

#define	NUITABLE	8 /* one row for each VME interrupt level */

static struct {
	unsigned	ibits;
	vertex_hdl_t    intr_target;   
	int		swlevel;
} uitable[NUITABLE];

static void
universe_intr_setup(universe_state_t universe_state)
{
	vertex_hdl_t		conn;
	vertex_hdl_t		vhdl;
	graph_error_t           rc;
	vertex_hdl_t            ipl_master;
	vertex_hdl_t            ipls[8];
	int                     bbit;
	unsigned                ibits;
	char                    ipl_s[2];
	char                    intr_name_s[32];
	device_desc_t		dev_desc;
	device_desc_t		ipl_dev_desc;
	/* REFERENCED */
	int                     rv;
	vmeio_intr_level_t      lvl;            /* VME INTR level          */
	unsigned char           ln;             /* PCI INTR line           */
	unsigned char           lvl_handled[8];
	universe_intr_info_t    iinfo;
	pciio_intr_t            pci_intr;

#if XIOVME_INTRLINE_WAR
	/* 
	 *  Interrupt source             output pin        pci input pin
	 *  IRQ?                         1                 5
	 *  IRQ?                         0                 4
	 *  IRQ?                         7                 3
	 *  IRQ?                         6                 2
	 *  IRQ?                         5                 1
	 *  IRQ?                         4                 0
	 *  other intrs                  2                 6
	 *  reset                        3                 7
         */

	unsigned char           bridge_to_universe_line[8] = {4,
							      5,
							      6,
							      7,
							      0,
							      1,
							      2,
							      3};
#else
	unsigned char           bridge_to_universe_line[8] = {0,
							      1,
							      2,
							      3,
							      4,
							      5,
							      6,
							      7};
#endif
	int i;

	/*
	 * Clear uitable
	 */
	for(i=0; i<NUITABLE; i++) {
		uitable[i].ibits = 0;
		uitable[i].intr_target = 0;
		uitable[i].swlevel = 0;
	}
	uitable[6].swlevel = 2;
	uitable[6].ibits = UNIVERSE_LINT_OTHER;
	uitable[7].swlevel = 2;

	conn = universe_state->pci_conn;
	vhdl = universe_state->vertex;

	/* Set up VME resources */
	rc = hwgraph_path_add(universe_state->vertex, 
			      "ipl",
			      &ipl_master);

	ASSERT(rc == GRAPH_SUCCESS);

	for (lvl = VMEIO_INTR_LEVEL_1; 
	     lvl <= VMEIO_INTR_LEVEL_7; 
	     lvl++) {
		sprintf(ipl_s, "%d", lvl);
		rc = hwgraph_char_device_add(ipl_master, 
					      ipl_s,
					      "universe_",
					      &ipls[lvl]);
		ASSERT(rc == GRAPH_SUCCESS);
	}

	ln = 0;
	for (lvl = VMEIO_INTR_LEVEL_1;
	     lvl <= VMEIO_INTR_LEVEL_7; 
	     lvl++) {
		ipl_dev_desc = device_desc_default_get(ipls[lvl]);
		if (ipl_dev_desc) {
			lvl_handled[lvl] = 1;
			ASSERT(uitable[ln].ibits == 0);
			uitable[ln].ibits = UNIVERSE_LINT_VIRQ(lvl);
			uitable[ln].intr_target = 
				device_desc_intr_target_get(ipl_dev_desc);
			universe_state->chip->lint_map0 |= 
				UNIVERSE_INTRMAP_VME_TO_PCI
				(lvl, bridge_to_universe_line[ln]);
			ln++;
		}
		else {
			lvl_handled[lvl] = 0;
		}
	}

	for (lvl = VMEIO_INTR_LEVEL_1;
	     lvl <= VMEIO_INTR_LEVEL_7; 
	     lvl++) {
		if (lvl_handled[lvl] == 0) {
			if (ln == 6) {
				ln--;
				ASSERT(uitable[ln].ibits);
				uitable[ln].ibits |= UNIVERSE_LINT_VIRQ(lvl);
			}
			else {
				ASSERT(uitable[ln].ibits == 0);
				uitable[ln].ibits = UNIVERSE_LINT_VIRQ(lvl);
			}
			universe_state->chip->lint_map0 |= 
				UNIVERSE_INTRMAP_VME_TO_PCI
				(lvl, bridge_to_universe_line[ln]);
			ln++;
		}
	}

	universe_state->chip->lint_map1 = 0x22000222;

	/* Set up PCI resources */
	for (bbit = 0; bbit < NUITABLE; ++bbit) {
		ibits = uitable[bbit].ibits;

		/* The universe_intr_handler does NOT
		 * run on a thread; rather, it fires
		 * off threads for interrrupts that
		 * come in from the VME bus.
		 */
		dev_desc = device_desc_dup(conn);
		device_desc_flags_set(dev_desc,
				      (device_desc_flags_get(dev_desc) | D_INTR_NOTHREAD));
		device_desc_intr_swlevel_set(dev_desc, uitable[bbit].swlevel);
		if (ibits == 0) {
			device_desc_intr_name_set(dev_desc, 
						  "universe_vme_reset");
		}
		else {
			strcpy(intr_name_s, "universe_intr");
			for (lvl = VMEIO_INTR_LEVEL_1; 
			     lvl <= VMEIO_INTR_LEVEL_7;
			     lvl++) {
				if (ibits & UNIVERSE_LINT_VIRQ(lvl)) {
					sprintf(ipl_s, "_%d", lvl);
					strcat(intr_name_s, ipl_s);
				}
			}
			device_desc_intr_name_set(dev_desc, intr_name_s);
		}

		if (uitable[bbit].intr_target) {
			device_desc_intr_target_set(dev_desc, 
						    uitable[bbit].intr_target);
		}

		pci_intr = pciio_intr_alloc(conn, dev_desc, PCIIO_INTR_LINE(bbit), vhdl);
		if (pci_intr == NULL) {
			cmn_err(CE_ALERT,
				"universe: unable to allocate intr channel for bit %d\n", bbit);
			continue;
		}

		for (lvl = VMEIO_INTR_LEVEL_1; 
		     lvl <= VMEIO_INTR_LEVEL_7;
		     lvl++) {
			if (ibits & UNIVERSE_LINT_VIRQ(lvl)) {
				universe_state->vmebus_intrs[lvl].cpu = 
					pciio_intr_cpu_get(pci_intr);
			}
		}

		NEW(iinfo);
		iinfo->state = universe_state;
		iinfo->ibits = ibits;
		universe_state->iinfo[bbit] = iinfo;

		rv = pciio_intr_connect(pci_intr,
					universe_intr_handler,
					(intr_arg_t) iinfo,
					(void *) 0);
		ASSERT(rv == 0);
		if (rv != 0) {
			cmn_err(CE_PANIC,
				"universe: unable to connect intr channel for bit %d\n", bbit);
			continue;
		}
	}

	/* Clear INTR status */
	universe_state->chip->lint_stat = 0;

	/* Enable ACFail, VMEbus error, and PCIbus error */
	universe_state->chip->lint_en = 
		UNIVERSE_ACFAIL_BIT |
		UNIVERSE_LERR_BIT |
		UNIVERSE_VERR_BIT;
}

/* -----------------------------------------------------------------------
  
                   interrupt handling -- interrupt chain

   ------------------------------------------------------------------------ */

/*
 * Interrupt handler 
 * Invoked by: Interrupt sources
 *
 * NOTE: THIS FUNCTION IS RUNNING ON THE INTERRUPT
 * STACK, NOT AS A THREAD.
 */
static void
universe_intr_handler(intr_arg_t arg)
{
	universe_intr_info_t    uiinfo = (universe_intr_info_t) arg;
	universe_state_t        universe_state = uiinfo->state;
	unsigned		ibits = uiinfo->ibits;
	unsigned                intr_status;
	unsigned *              intr_status_p;

#if VME_DEBUG
	cmn_err(CE_CONT, "universe_intr_handler\n");
#endif

	intr_status_p = (unsigned *)
	    UNIVERSE_REG_ADDR(universe_state->chip,
			      UNIVERSE_LINT_STAT);

	if (ibits == 0) {
		if (universe_state->ctlr == -1)
			cmn_err(CE_ALERT, "vmebus: VME RESET observed at\n\t\n",
				universe_state->vertex);
		else
			cmn_err(CE_ALERT, "vmebus%d: VME RESET observed\n",
				universe_state->ctlr);
		return;
	}

	while (1) {
		intr_status = ibits & *intr_status_p;

		/*
		 * There might be multiple interrupts happening,
		 * we scan all the bits for potential sources,
		 * in descending priority order; service the
		 * highest priority pending interrupt, then
		 * come back and check again.
		 */
		if (intr_status & UNIVERSE_LINT_ACFAIL) {
			cmn_err(CE_PANIC, "VMEbus power failure");
		}
		if (intr_status & UNIVERSE_LINT_SYSFAIL) {
			if (universe_state->ctlr == -1) {
				cmn_err(CE_PANIC, 
					"vmebus(%v): SYSFAIL remains low",
					universe_state->vertex);
			}
			else
				cmn_err(CE_PANIC, 
					"vmebus(%d): SYSFAIL remains low",
					universe_state->ctlr);
		}
		if (intr_status & UNIVERSE_LINT_VERR) {
			universe_pio_postwrite_error_handler(universe_state);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_LERR) {
			universe_dma_postwrite_error_handler(universe_state);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ7) {
			universe_vmebus_intr_handler(universe_state, 7);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ6) {
			universe_vmebus_intr_handler(universe_state, 6);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ5) {
			universe_vmebus_intr_handler(universe_state, 5);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ4) {
			universe_vmebus_intr_handler(universe_state, 4);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ3) {
			universe_vmebus_intr_handler(universe_state, 3);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ2) {
			universe_vmebus_intr_handler(universe_state, 2);
			continue;
		}
		if (intr_status & UNIVERSE_LINT_VIRQ1) {
			universe_vmebus_intr_handler(universe_state, 1);
			continue;
		}

		/* If we get here, there was nothing to do,
		 * so stop looping and go away.
		 */
		break;
	}
}



/* 
 * Deal with interrupt caused by vmebus signals
 *
 * NOTE: THIS FUNCTION IS RUNNING ON THE INTERRUPT
 * STACK, NOT AS A THREAD.
 */

static void
universe_vmebus_intr_handler(universe_state_t      universe_state, 
			     vmeio_intr_level_t    intr_level)
{
	universe_virq_statid_t *    statid;
	vmeio_intr_vector_t         vector;
	universe_intr_t             univ_intr;
	universe_reg_t *            intr_status;
	int                         s;

	s = UNIVERSE_LOCK(universe_state);
	if (universe_state->vmebus_intrs[intr_level].users == 0) {
		ASSERT((universe_state->chip->lint_en & 
			UNIVERSE_LINT_VIRQ(intr_level)) == 0);
		UNIVERSE_UNLOCK(universe_state, s);
		return;
	}
	UNIVERSE_UNLOCK(universe_state, s);

	statid = (universe_virq_statid_t *) 
		UNIVERSE_REG_ADDR(universe_state->chip, 
				  UNIVERSE_VIRQ_STATID(intr_level));

	if (statid->err == UNIVERSE_VIRQ_STATID_ERR) {
		cmn_err(CE_PANIC, "vme");
	}

	vector = statid->statid;
	ASSERT(vector);

	/* Waiting for DMA FIFOs to be empty */
	universe_dma_flush(universe_state);

#if VME_DEBUG
	printf("vmebus interrupt level %d vector %d\n", 
		    intr_level, vector);
#endif
		   
	univ_intr = universe_state->intr_table[vector];
	if (univ_intr != 0) {
		if (univ_intr->ui_tinfo.thd_flags & THD_OK)
			vsema(&univ_intr->ui_tinfo.thd_isync);
		else
			univ_intr->ui_func(univ_intr->ui_arg);
	}
	else {
		cmn_err(CE_ALERT, "vmebus%d: no interrupt handler registered for interrupt vector 0x%x on level %d", 
			universe_state->ctlr, vector, intr_level);
	}
		
	/* 
	 * Clear the interrupt status register.
	 * PCI Bridge expects that be cleaned in order 
	 * for the next to be generated. 
	 */
	intr_status = (universe_reg_t *) 
		UNIVERSE_REG_ADDR(universe_state->chip,
				  UNIVERSE_LINT_STAT);
	*intr_status = UNIVERSE_LINT_VIRQ(intr_level);
}


/* -----------------------------------------------------------------------
  
                   interrupt handling -- exported interface

   ------------------------------------------------------------------------ */

/*
 * Allocate Interrupt resources:
 * -- If a user likes to get a specific vector, he/she would state a better 
 *    chance to get it by stating it out in the VECTOR line, since the vectors
 *    are being given away here. 
 * -- Users must ask for a specific interrupt level at the kernel configuration
 *    time.
 * -- Here, users can ask for an specific interrupt vector or 
 * Resources allocated:
 *   VMEbus: interrupt vector
 * Return: handle of the interrupt resources if success, 0 if failure.
 */

/* ARGSUSED */
universe_intr_t
universe_intr_alloc(vertex_hdl_t         vme_conn,
		    device_desc_t        dev_desc,
		    vmeio_intr_vector_t  vector_input,
		    vmeio_intr_level_t   level,
		    vertex_hdl_t         owner_dev,
		    unsigned             flags)
{
	vmeio_info_t          info;
	universe_state_t      universe_state;
	universe_intr_t       univ_intr;
	vmeio_intr_vector_t   vector;
	vmeio_intr_vector_t   i;
	int                   s;
	unsigned	      dev_desc_flags = 0;
	universe_reg_t        lint_en;

	if (dev_desc) {
		dev_desc_flags = device_desc_flags_get(dev_desc);
	}

	info = vmeio_info_get(vme_conn);
	universe_state = (universe_state_t)
		vmeio_info_provider_fastinfo_get(info);

	univ_intr = 0;
	NEW(univ_intr);
	if (univ_intr == 0) {
		return(0);
	}

	vector = VMEIO_INTR_VECTOR_NONE;
	s = UNIVERSE_LOCK(universe_state);
	if (vector_input == VMEIO_INTR_VECTOR_ANY) {
		for (i = VMEIO_INTR_VECTOR_FIRST; 
		     i <= VMEIO_INTR_VECTOR_LAST; 
		     i++) {
			if (universe_state->intr_table[i] == 0) {
				vector = i;
				universe_state->intr_table[i] = univ_intr;
				break;
			}
		}
	}
	else {
		if (universe_state->intr_table[vector_input] != 0) {
			vector = VMEIO_INTR_VECTOR_NONE;
		}
		else {
			vector = vector_input;
			universe_state->intr_table[vector] = univ_intr;
		}
	}
	
	/* Enable this interrupt level if not used before */
	if (universe_state->vmebus_intrs[level].users == 0) {
		lint_en = universe_state->chip->lint_en;
		ASSERT((lint_en & UNIVERSE_LINT_VIRQ(level)) == 0);
		lint_en |= UNIVERSE_LINT_VIRQ(level);
		universe_state->chip->lint_en = lint_en;
	}

	universe_state->vmebus_intrs[level].users++;

	UNIVERSE_UNLOCK(universe_state, s);

	if (vector == VMEIO_INTR_VECTOR_NONE) {
		DEL(univ_intr);
		return(0);
	}

	univ_intr->ui_dev = vme_conn;
	univ_intr->ui_level = level;
	univ_intr->ui_vec = vector;
	univ_intr->ui_universe_state = universe_state;
	univ_intr->ui_cpu = universe_state->vmebus_intrs[level].cpu;

	if (!(dev_desc_flags & D_INTR_NOTHREAD)) {
		atomicSetInt(&univ_intr->ui_tinfo.thd_flags, THD_ISTHREAD);
	}

	return(univ_intr);
}

/*
 * Release interrupt resources
 * XXX -- Don't release the hardwired interrupt vector
 * Resources released:
 *   VMEbus: interrupt vector
 */
void
universe_intr_free(universe_intr_t intr)
{
	vmeio_info_t          info;
	universe_state_t      universe_state;
	unsigned              lint_en;
	int                   s;

	/* Check the integrity of the interrupt object */
	ASSERT(intr != 0);
	ASSERT(intr->ui_dev != 0);
	ASSERT(intr->ui_level != 0);
	ASSERT((intr->ui_vec >= VMEIO_INTR_VECTOR_FIRST) &&
	       (intr->ui_vec <= VMEIO_INTR_VECTOR_LAST));
	ASSERT(intr->ui_universe_state != 0);
	ASSERT(intr->ui_universe_state->intr_table[intr->ui_vec] != 0);

	info = vmeio_info_get(intr->ui_dev);
	universe_state = (universe_state_t)
		vmeio_info_provider_fastinfo_get(info);

	s = UNIVERSE_LOCK(universe_state);

	intr->ui_universe_state->intr_table[intr->ui_vec] = 0; 
	
	/* Disable this intr level if it's no longer used */
	universe_state->vmebus_intrs[intr->ui_level].users--;
	if (universe_state->vmebus_intrs[intr->ui_level].users == 0) {
		lint_en = universe_state->chip->lint_en;
		ASSERT(lint_en & UNIVERSE_LINT_VIRQ(intr->ui_level));
		lint_en &= ~UNIVERSE_LINT_VIRQ(intr->ui_level);
		universe_state->chip->lint_en = lint_en;		
	}

	UNIVERSE_UNLOCK(universe_state, s);

	DEL(intr); 
}

/* Interrupt Delivery helper functions
 */

static void
universe_intrd(universe_intr_t intr)
{
	intr->ui_func(intr->ui_arg);
	ipsema(&intr->ui_tinfo.thd_isync);
	/* NOTREACHED */
}

static void
universe_intrd_start(universe_intr_t intr)
{
	if (intr->ui_cpu != GRAPH_VERTEX_NONE) {
		setmustrun(cpuvertex_to_cpuid(intr->ui_cpu));
	}

	xthread_set_func(KT_TO_XT(curthreadp),
			 (xt_func_t *)universe_intrd, 
			 intr);
	ipsema(&intr->ui_tinfo.thd_isync);
	/* NOTREACHED*/
}

/* 
 * Associate software interrupt handler to hardware interrupt resources.
 * Return: 0 if success, -1 if failure.
 */

/*ARGSUSED*/
int
universe_intr_connect(universe_intr_t  intr,
		      intr_func_t      intr_func,
		      intr_arg_t       intr_arg,
		      void *           thread)
{
	char	tname[32];
	int	tpri;

	ASSERT_ALWAYS(intr != 0);
	ASSERT_ALWAYS((intr->ui_vec >= VMEIO_INTR_VECTOR_FIRST) && 
		      (intr->ui_vec <= VMEIO_INTR_VECTOR_LAST));
	ASSERT_ALWAYS(intr_func != 0);

	intr->ui_func = intr_func;
	intr->ui_arg = intr_arg;

	if (!(intr->ui_tinfo.thd_flags & THD_ISTHREAD)) {
		return 0;
	}

	atomicSetInt(&intr->ui_tinfo.thd_flags, THD_REG);

	/* Spread tpri from 230 to 250
	 * as ui_level goes from 1 to 7.
	 * Actually uses 231, 234, 237,
	 * 240, 243, 246, 249.
	 */
	tpri = 228 + 3 * intr->ui_level;

	/* XXX the threads team never did tell me
	 * what the heck to do with that final
	 * thread parameter. <sigh>
	 */
	sprintf(tname, "universe_xthread(%d:%d)", intr->ui_dev, intr->ui_vec);
	xthread_setup(tname, tpri, &intr->ui_tinfo,
		      (xt_func_t *)universe_intrd_start, intr);

	return(0);
}

/* 
 * Disassociate the software interrupt handler from the hardware interrupt
 * resources.
 */

void
universe_intr_disconnect(universe_intr_t intr)
{
	ASSERT_ALWAYS(intr != 0);
	ASSERT_ALWAYS(intr->ui_func !=0);

	intr->ui_func = 0;
	intr->ui_arg = 0;
}

vertex_hdl_t
universe_intr_cpu_get(universe_intr_t intr)
{
	ASSERT_ALWAYS(intr != 0);
	return(intr->ui_cpu);
}
		      
/*
 * Prevent the specified interrupt from reaching the current cpu.
 * Note: The caller MUST ensure it stays on the same cpu.
 */
/* ARGSUSED */
void
universe_intr_block(universe_intr_t intr)
{
}

/*
 * Allow the specified interupt to reach to the current cpu again.
 * Note: The caller MUST ensure it stays on the same cpu.
 */
/* ARGSUSED */
void 
universe_intr_unblock(universe_intr_t intr)
{
}


/* ------------------------------------------------------------------------
  
   Atomic operations -- compare and swap

   ------------------------------------------------------------------------ */

/* 
 * Compare and swap atomic operation
 * Sync: only one guy can use this facility at one time.
 * Return: 0 if operation is successful, -1 if operation aborts abnoramlly
 */
int
universe_compare_and_swap(universe_piomap_t piomap,
			  iopaddr_t iopaddr,
			  __uint32_t old,
			  __uint32_t new)
{
	universe_state_t universe_state;
	
	universe_state = piomap->universe_state;

	UNIVERSE_REG_SET(universe_state->chip, UVSPL_SCYC_ADDR, iopaddr);
	UNIVERSE_REG_SET(universe_state->chip, UVSPL_SCYC_CTL, SCYC_CTL_RMW);

	/* XXX */
	UNIVERSE_REG_SET(universe_state->chip, UVSPL_SCYC_EN, 0xffffffff);
	
	UNIVERSE_REG_SET(universe_state->chip, UVSPL_SCYC_CMP, old);
	UNIVERSE_REG_SET(universe_state->chip, UVSPL_SCYC_SWP, new);

	/* Initiate the operation */
	/* XXX */

	return(0);
}
			  


/* ------------------------------------------------------------------------
  
   Universe instance of the VMEbus service provider 

   ------------------------------------------------------------------------ */

vmeio_provider_t universe_provider = {

	/* PIO */
	(vmeio_piomap_alloc_f *)        universe_piomap_alloc,
	(vmeio_piomap_free_f *)         universe_piomap_free,
	(vmeio_piomap_addr_f *)		universe_piomap_addr,
	(vmeio_piomap_done_f *)		universe_piomap_done,
	(vmeio_pio_bcopyin_f *)         universe_pio_bcopyin,
	(vmeio_pio_bcopyout_f *)        universe_pio_bcopyout,

	/* DMA */
	(vmeio_dmamap_alloc_f *)	universe_dmamap_alloc,
	(vmeio_dmamap_free_f *)		universe_dmamap_free,
	(vmeio_dmamap_addr_f *)		universe_dmamap_addr,
	(vmeio_dmamap_list_f *)		universe_dmamap_list,
	(vmeio_dmamap_done_f *)		universe_dmamap_done,

	/* INTR */
	(vmeio_intr_alloc_f *)		universe_intr_alloc,
	(vmeio_intr_free_f *)		universe_intr_free,
	(vmeio_intr_connect_f *)	universe_intr_connect,
	(vmeio_intr_disconnect_f *)	universe_intr_disconnect,

	/* Generic configuration management */
	(vmeio_provider_startup_f *)	universe_provider_startup,
	(vmeio_provider_shutdown_f *)	universe_provider_shutdown,

	/* Error management */
	(vmeio_error_devenable_f *)     universe_error_devenable,

	/* Device probing */
	(vmeio_probe_f *)               universe_probe,

	/* Atomic operations */
	(vmeio_compare_and_swap_f *)    universe_compare_and_swap,
};

/* -----------------------------------------------------------------------

   Supporting old interface

   ----------------------------------------------------------------------- */

/* ARGSUSED */
int
iointr_at_cpu(int ipl, int cpu, int iadap)
{
	return(0);
}


/* -----------------------------------------------------------------------

                             error handling

   ----------------------------------------------------------------------- */

/* 
 * Handle errors received by the Universe chip
 * -- Deal with errors happening on access of the Universe chip registers
 * -- Invoke error handler for an individual device if the driver for that
 *    device has registered an error handler
 * -- Invoke error handler for user level PIO/Interrupt device driver and
 *    user level DMA engine access driver
 * Called by: upper layer error handler 
 *
 * NOTE: THIS FUNCTION IS RUNNING ON THE INTERRUPT
 * STACK, NOT AS A THREAD.
 */

static int
universe_error_handler(error_handler_arg_t errinfo,
		       int                 err_code,
		       ioerror_mode_t      err_mode,
		       ioerror_t *         ioerr)
{
	universe_state_t    universe_state;
	vertex_hdl_t        vme_conn;
	vmeio_info_t        info;
	iopaddr_t           pciaddr;
	universe_piomap_t   piomap;
	error_handler_f *   errhandler;
	/* REFERENCED */
	error_handler_arg_t arg;
	iopaddr_t           vmeaddr;
	/* REFERENCED */
	vmeio_am_t          am;
	int                 rv;

	universe_state = (universe_state_t) errinfo;
	ASSERT(universe_state != 0);

#if VME_DEBUG
	printf("%v: universe_error_handler\n", universe_state->vertex);
	IOERROR_DUMP("universe_error_handler", err_code, err_mode, ioerr);
#endif /* VME_DEBUG */

	if (err_code & IOECODE_PIO) {

		if (err_code & IOECODE_READ) {

			/* 
			 * We're called by upper layer error handler.
			 * Get the PCIbus address and figure out the 
			 * VMEbus address and address modifier.
			 */
			pciaddr = IOERROR_GETVALUE(ioerr, busaddr);
			piomap = universe_piomap_lookup_by_pciaddr(universe_state,
								   pciaddr);

			ASSERT(piomap != 0);
			vmeaddr = universe_piomap_pciaddr_to_vmeaddr(piomap,
								     pciaddr);
		
			/* Set the VMEbus addr for the device driver 
			   error handler */
			IOERROR_SETVALUE(ioerr, busaddr, vmeaddr);

#if VME_DEBUG
			sync_printf("vme pio read error at 0x%x\n",
				    vmeaddr);
#endif 

			if (vmeio_error_handler(piomap->uv_dev,
						err_code, 
						err_mode,
						ioerr) ==
			    IOERROR_HANDLED) {
				return(IOERROR_HANDLED);	
			}
						
			panic("vme(%d): PIO READ bus error at vmeaddr 0x%x",
			      universe_state->ctlr, vmeaddr);

		}
		else {
			ASSERT(err_code & IOECODE_WRITE);

			/* 
			 * We are called by the interrupt handler 
			 */
			vmeaddr = IOERROR_GETVALUE(ioerr, busaddr);
		}
		
	}
	else {
		ASSERT(err_code & IOECODE_DMA);
		if (err_code & IOECODE_READ) {
			;
		}
		else {
			ASSERT(err_code & IOECODE_WRITE);
			vmeaddr = IOERROR_GETVALUE(ioerr, busaddr);
		}
	}

	vme_conn = piomap->uv_dev;
	ASSERT(vme_conn != 0);
	info = vmeio_info_get(vme_conn);
	ASSERT(info != 0);

	errhandler = vmeio_info_errhandler_get(info);
	if (errhandler != 0) {
		arg = vmeio_info_errinfo_get(info);
		rv = errhandler(errinfo, err_code, err_mode, ioerr);
		return(rv);
	}
	else {
		
		/* 
		 * We're here if both usrvme and device driver can't handle 
		 * this error.
		 * XXX We ought to disable the device here.
		 */
		cmn_err(CE_PANIC, "vme: please disalbe the device");
	}

	return(0);
}

/* 
 * Handle postwrite mode PIO error 
 * Called by: universe_intr_handler
 *
 * NOTE: THIS FUNCTION IS RUNNING ON THE INTERRUPT
 * STACK, NOT AS A THREAD.
 */
static void
universe_pio_postwrite_error_handler(universe_state_t universe_state)
{
	iopaddr_t                vmeaddr;
	universe_piomap_t        piomap;
	ioerror_t	         ioerror[1];
	
	if (universe_state->chip->v_amerr.v_stat == 
	    UNIVERSE_V_AMERR_VSTAT_INVALID) {
		panic("vme(%d): invalid PIO WRITE error log",
		      universe_state->ctlr);
	}

#if UNIVERSE_V_AERR_WAR
	if ((universe_state->chip->v_amerr.amerr == VME_A16NPAMOD) || 
	    (universe_state->chip->v_amerr.amerr == VME_A16SAMOD)) {
		vmeaddr = universe_state->chip->v_aerr & ADDR_16_MASK;
	}
	else if ((universe_state->chip->v_amerr.amerr == VME_A24NPAMOD) || 
		 (universe_state->chip->v_amerr.amerr == VME_A24NPBAMOD) || 
		 (universe_state->chip->v_amerr.amerr == VME_A24SAMOD) || 
		 (universe_state->chip->v_amerr.amerr == VME_A24SBAMOD)) {
		vmeaddr = universe_state->chip->v_aerr & ADDR_24_MASK;
	}
	else {
		vmeaddr = universe_state->chip->v_aerr;
	}
#else
	vmeaddr = universe_state->chip->v_aerr;
#endif 

	piomap = universe_piomap_lookup_by_vmeaddr(universe_state, vmeaddr);
	ASSERT_ALWAYS(piomap);

	IOERROR_INIT(ioerror);
	IOERROR_SETVALUE(ioerror, busaddr, vmeaddr);

	if (vmeio_error_handler(piomap->uv_dev,
				PIO_WRITE_ERROR,
				MODE_DEVERROR,
				ioerror) != IOERROR_HANDLED) {
		panic("vme(%d): PIO WRITE bus error at vmeaddr 0x%x",
		      universe_state->ctlr, vmeaddr);
	}

	/* Clear error log */
	universe_state->chip->v_amerr.v_stat = UNIVERSE_V_AMERR_VSTAT_CLEAR;

	/* Enable the interrupt */
	universe_state->chip->lint_stat = UNIVERSE_VERR_BIT;
}


/* 
 * Handle postwrite mode DMA error
 * Called by: universe_interrupt_handler
 *
 * NOTE: THIS FUNCTION IS RUNNING ON THE INTERRUPT
 * STACK, NOT AS A THREAD.
 */
static void
universe_dma_postwrite_error_handler(universe_state_t universe_state)
{
	iopaddr_t         pciaddr;
	universe_dmamap_t dmamap;
	ioerror_t	  ioerror[1];
	
	if (universe_state->chip->l_cmderr.l_stat == 
	    UNIVERSE_L_CMDERR_LSTAT_INVALID) {
		panic("vme(%d): invalid DMA POST WRITE error log",
		      universe_state->ctlr);
	}

	pciaddr = universe_state->chip->l_aerr;

	dmamap = universe_dmamap_lookup_by_pciaddr(universe_state, pciaddr);
	ASSERT(dmamap);

	IOERROR_INIT(ioerror);
	IOERROR_SETVALUE(ioerror, busaddr, pciaddr);

	if (vmeio_error_handler(dmamap->ud_dev,
				DMA_WRITE_ERROR,
				MODE_DEVERROR,
				ioerror) != IOERROR_HANDLED) {
		panic("vme(%d): DMA POST WRITE bus error at pciaddr 0x%x",
		      universe_state->ctlr, pciaddr);
	}

	/* Clear error log */
	universe_state->chip->l_cmderr.l_stat = UNIVERSE_L_CMDERR_LSTAT_CLEAR;

	/* Enable the interrupt */
	universe_state->chip->lint_stat = UNIVERSE_LERR_BIT;
}

/* 
 * Reenable a device after the error is handled
 */
 
/* ARGSUSED */
int
universe_error_devenable(vertex_hdl_t vme_conn, int err_code)
{
	return(0);
}


/* ------------------------------------------------------------------------
  
		 User level PIO/INTR -- initialization 

   ------------------------------------------------------------------------ */

/* 
 * Set up for user-level PIO
 * Return: 0 if success
 */

/* ARGSUSED */
static int
universe_pio_user_setup(universe_state_t universe_state)
{
	/* Register the user vme device */
	universe_state->usrvme = 
		usrvme_device_register(universe_state->vertex);
	if (universe_state->usrvme == 0) {
		cmn_err(CE_ALERT|CE_SYNC, 
			"%v: User PIO device registration failed",
			universe_state->vertex);
	}

	return(0);
}

/* ------------------------------------------------------------------------
  
                      DMA engine -- initialization 

   ------------------------------------------------------------------------ */

static uint32_t
word_swap(uint32_t value)
{
	value = ((0x0000FFFF & value) << 16) | (0x0000FFFF & (value >> 16));
	value = ((0x00FF00FF & value) <<  8) | (0x00FF00FF & (value >>  8));
	return value;
}

/* 
 * Set up the resources for DMA engine:
 * -- Reserve memory for the linked list mode
 * -- Set up DMA engine hardware
 *    -- set DCPP to the first command packet 
 *    -- set the CHAIN bit
 * Return: 0 upon success, -1 upon failure.
 */

static int
universe_dma_engine_setup(universe_state_t universe_state)
{
	/* Tunable parameter */
	unsigned                                  npgs;
	void *                                    head_kvaddr;
	paddr_t                                   head_paddr;
	iopaddr_t                                 pciaddr;
	iopaddr_t                                 head_pciaddr;
	pciio_dmamap_t                            pci_dmamap;
	universe_dma_engine_regs_t *              regs;
	universe_dma_engine_linked_list_packet_t  packet;
	int                                       i;
	
	spinlock_init(&(universe_state->dma_engine.lock), 
		      "universe_dma_engine");

	/* Reserve memory for the linked list area */
	npgs = btoc(UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE);
	head_kvaddr = kvpalloc(npgs, 0, 0);  
	head_paddr = kvtophys(head_kvaddr); 

	universe_state->dma_engine.ll_kvaddr = head_kvaddr;
	universe_state->dma_engine.pci_dmamaps = 0;

	/* Prepare the DMA map for the linked list area */
	pci_dmamap = pciio_dmamap_alloc(universe_state->pci_conn, 
					0,
					ctob(npgs), 
					PCIIO_BYTE_STREAM);
	ASSERT(pci_dmamap);

	pciaddr = pciio_dmamap_addr(pci_dmamap, head_paddr, btoc(npgs));
	ASSERT(pciaddr);

	head_pciaddr = pciaddr;

#if VME_DEBUG
	printf("pciio_dmamap_addr: kvaddr 0x%x -> phys 0x%x -> pciaddr 0x%x\n",
	       head_kvaddr, head_paddr, pciaddr);
#endif 

	/* 
	 * Prepare the linked list 
	 * -- The list is being traversed linearly and circularly
	 * -- All the addresses of the next packets are fixed statically
	 */
	packet = (universe_dma_engine_linked_list_packet_t) head_kvaddr;
	for (i = 0; 
	     i < UNIVERSE_DMA_ENGINE_LINKED_LIST_MAX_PACKETS; 
	     i++) {
		pciaddr += UNIVERSE_DMA_ENGINE_LINKED_LIST_PACKET_SIZE;
		if (i == UNIVERSE_DMA_ENGINE_LINKED_LIST_MAX_PACKETS - 1) {
			packet->cpp.addr_31_3 = 
				word_swap(head_pciaddr) >> 
					  UNIVERSE_DMA_CPP_ADDR_SHIFT;
		}
		else {
			packet->cpp.addr_31_3 = 
				word_swap(pciaddr) >>
					  UNIVERSE_DMA_CPP_ADDR_SHIFT;
		}
		packet++;
	}
	
	/* 
	 * Initialize the DMA engine hardware for use from both 
	 * User processes and device drivers.
	 */
	regs = (universe_dma_engine_regs_t *) 
		UNIVERSE_REG_ADDR(universe_state->chip, UNIVERSE_DMA_CTL);
	regs->cpp.addr_31_3 = head_pciaddr >> UNIVERSE_DMA_CPP_ADDR_SHIFT;

	regs->gcs.chain = UNIVERSE_DMA_GCS_CHAIN_LINKEDLIST; /* Linked list 
								mode is used 
								always */

	/* Register the pseudo device for user level access */
	universe_state->dma_engine.vertex = 
		ude_device_register(universe_state->vertex);
	if (universe_state->dma_engine.vertex == 0) {
		cmn_err(CE_ALERT, "%v: unable to register ude device",
			universe_state->vertex);
		return(-1);
	}

	return(0);
}

#endif /* SN0 */










