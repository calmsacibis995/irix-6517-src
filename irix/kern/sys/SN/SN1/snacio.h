/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1996-1998, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#ifndef __SYS_SN_SN1_SNACIO_H__
#define __SYS_SN_SN1_SNACIO_H__

#include <sys/SN/SN1/snacioregs.h>

#define IIO_NUM_CRBS		8	/* Number of CRBs */

#if _KERNEL
#if _LANGUAGE_C
#include <sys/alenlist.h>
#include <sys/dmamap.h>
#include <sys/iobus.h>
#include <sys/pio.h>
#include <sys/xtalk/xtalk.h>

/* PIO MANAGEMENT */
typedef struct hub_piomap_s *hub_piomap_t;

extern hub_piomap_t
hub_piomap_alloc(vertex_hdl_t dev,	/* set up mapping for this device */
		device_desc_t dev_desc,	/* device descriptor */
		iopaddr_t xtalk_addr,	/* map for this xtalk_addr range */
		size_t byte_count,
		size_t byte_count_max, 	/* maximum size of a mapping */
		unsigned flags);		/* defined in sys/pio.h */

extern void hub_piomap_free(hub_piomap_t hub_piomap);

extern caddr_t
hub_piomap_addr(hub_piomap_t hub_piomap,	/* mapping resources */
		iopaddr_t xtalk_addr,		/* map for this xtalk addr */
		size_t byte_count);		/* map this many bytes */

extern void
hub_piomap_done(hub_piomap_t hub_piomap);

extern caddr_t
hub_piotrans_addr(	vertex_hdl_t dev,	/* translate to this device */
			device_desc_t dev_desc,	/* device descriptor */
			iopaddr_t xtalk_addr,	/* Crosstalk address */
			size_t byte_count,	/* map this many bytes */
			unsigned flags);	/* (currently unused) */


/* DMA MANAGEMENT */
typedef struct hub_dmamap_s *hub_dmamap_t;

extern hub_dmamap_t
hub_dmamap_alloc(	vertex_hdl_t dev,	/* set up mappings for dev */
			device_desc_t dev_desc,	/* device descriptor */
			size_t byte_count_max, 	/* max size of a mapping */
			unsigned flags);	/* defined in dma.h */

extern void 
hub_dmamap_free(hub_dmamap_t dmamap);

extern iopaddr_t
hub_dmamap_addr(	hub_dmamap_t dmamap,	/* use mapping resources */
			paddr_t paddr,		/* map for this address */
			size_t byte_count);	/* map this many bytes */

extern alenlist_t
hub_dmamap_list(	hub_dmamap_t dmamap,	/* use mapping resources */
			alenlist_t alenlist,	/* map this Addr/Length List */
			unsigned flags);

extern void
hub_dmamap_done(	hub_dmamap_t dmamap);

extern void
hub_dmamap_done(	hub_dmamap_t dmamap);	/* done w/ mapping resources */

extern iopaddr_t
hub_dmatrans_addr(	vertex_hdl_t dev,	/* translate for this device */
			device_desc_t dev_desc,	/* device descriptor */
			paddr_t paddr,		/* system physical address */
			size_t byte_count,	/* length */
			unsigned flags);		/* defined in dma.h */

extern alenlist_t
hub_dmatrans_list(	vertex_hdl_t dev,	/* translate for this device */
			device_desc_t dev_desc,	/* device descriptor */
			alenlist_t palenlist,	/* system addr/length list */
			unsigned flags);		/* defined in dma.h */

extern void
hub_dmamap_drain(	hub_dmamap_t map);

extern void
hub_dmaaddr_drain(	vertex_hdl_t vhdl,
			paddr_t addr,
			size_t bytes);

extern void
hub_dmalist_drain(	vertex_hdl_t vhdl,
			alenlist_t list);


/* INTERRUPT MANAGEMENT */
typedef struct hub_intr_s *hub_intr_t;

extern hub_intr_t
hub_intr_alloc(	vertex_hdl_t dev,		/* which device */
		device_desc_t dev_desc,		/* device descriptor */
		vertex_hdl_t owner_dev);	/* owner of this interrupt */

extern void
hub_intr_free(hub_intr_t intr_hdl);

extern int
hub_intr_connect(	hub_intr_t intr_hdl,	/* xtalk intr resource hndl */
			intr_func_t intr_func,	/* xtalk intr handler */
			void *intr_arg,		/* arg to intr handler */
			xtalk_intr_setfunc_t setfunc,
						/* func to set intr hw */
			void *setfunc_arg,	/* arg to setfunc */
			void *thread);		/* intr thread to use */

extern void
hub_intr_disconnect(hub_intr_t intr_hdl);

extern vertex_hdl_t
hub_intr_cpu_get(hub_intr_t intr_hdl);

/* CONFIGURATION MANAGEMENT */

extern void
hub_provider_startup(vertex_hdl_t hub);

extern void
hub_provider_shutdown(vertex_hdl_t hub);

#define HUB_PIO_CONVEYOR	0x1	/* PIO in conveyor belt mode */
#define HUB_PIO_FIRE_N_FORGET	0x2	/* PIO in fire-and-forget mode */

/* Flags that make sense to hub_widget_flags_set */
#define HUB_WIDGET_FLAGS	(		 		\
				 HUB_PIO_CONVEYOR 	|	\
				 HUB_PIO_FIRE_N_FORGET		\
				 )

typedef int	hub_widget_flags_t;

/* Set the PIO mode for a widget.  These two functions perform the
 * same operation, but hub_device_flags_set() takes a hardware graph
 * vertex while hub_widget_flags_set() takes a nasid and widget
 * number.  In most cases, hub_device_flags_set() should be used.
 */
extern int    	hub_widget_flags_set(nasid_t            nasid,
				     xwidgetnum_t       widget_num,
				     hub_widget_flags_t	flags); 

/* Depending on the flags set take the appropriate actions */
extern int	hub_device_flags_set(vertex_hdl_t  	widget_dev,
				     hub_widget_flags_t	flags); 


/* Error Handling. */
struct io_error_s;
extern int hub_ioerror_handler(vertex_hdl_t, int, int, struct io_error_s *);
extern int kl_ioerror_handler(cnodeid_t, cnodeid_t, cpuid_t,
			      int, paddr_t, caddr_t, ioerror_mode_t);
extern int hub_error_devenable(vertex_hdl_t, int, int);

extern void hub_widget_reset(vertex_hdl_t, xwidgetnum_t);
/* 
 * Convert from a vertex that represents the xtalk provider
 * portion of a hub to the vertex that represents the hub itself.
 * 	/hw/..../hub/xtalk  -->  /hw/..../hub
 */
#define hubiov_to_hubv(hubiov) hwgraph_connectpt_get(hubiov)

#endif /* _LANGUAGE_C */
#endif /* _KERNEL */

#define HUB_WIDGET_ID_MIN	0x8
#define HUB_WIDGET_ID_MAX	0xf
/* IO Translation Table Entries */
#define IIO_NUM_ITTES	7		/* ITTEs numbered 0..6 */

#define HUB_NUM_BIG_WINDOW	IIO_NUM_ITTES - 1
					/* Hw manuals number them 1..7! */

/*
 * SN0 -> SN1 compatibilty definitions
 */
#define IIO_IOPRB_0		II_IPRB0
#define IIO_OUTWIDGET_ACCESS	II_IOWA
#define IIO_WCR			II_WCR
#define IIO_LLP_CSR		II_ILCSR    /* LLP control and status */

#define IIO_LLP_CSR_IS_UP		0x00002000

#define hubii_wcr_t	ii_wcr_u_t
#define iwcr_dir_con	wcr_fld_s.w_dir_con
#define wcr_reg_value	wcr_regval

#define iprb_t		ii_iprb0_u_t
#define iprb_regval	iprb0_regval
#define iprb_ovflow	iprb0_fld_s.i_of_cnt
#define	iprb_error	iprb0_fld_s.i_error
#define	iprb_ff		iprb0_fld_s.i_f
#define	iprb_mode	iprb0_fld_s.i_m
#define	iprb_bnakctr	iprb0_fld_s.i_nb
#define	iprb_anakctr	iprb0_fld_s.i_na
#define	iprb_xtalkctr	iprb0_fld_s.i_c

#endif  /* __SYS_SN_SN1_SNACIO_H__ */
