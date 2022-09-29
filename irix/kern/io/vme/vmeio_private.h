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

#ifndef __VME_VMEIO_PRIVATE_H__
#define __VME_VMEIO_PRIVATE_H__

/* Informations associated with each VME device */
struct vmeio_info_s {
	vertex_hdl_t        conn;              /* Connection point           */
	vmeio_am_t          am;                /* Addr modifier              */
	iopaddr_t           addr;              /* addr                       */
	int                 state;             /* Device state               */
	int                 ctlr;
	vertex_hdl_t        provider;          /* Bus provider               */
	arbitrary_info_t    provider_fastinfo; /* info of the provider       */
	error_handler_f *   errhandler;        /* Error handler              */
	error_handler_arg_t errinfo;           /* Error handler arg          */
	void *              private_info;      /* Device private data        */
};

/* Generic VME PIO map */
struct vmeio_piomap_s {
        vertex_hdl_t        dev;    
        vmeio_am_t          am;      
        iopaddr_t           vmeaddr; 
        size_t              mapsz;   
        caddr_t             kvaddr; 
	unsigned            flags;           
};

/* Generic VME DMA map */
struct vmeio_dmamap_s {
        vertex_hdl_t        dev;     
	vmeio_am_t          am;      
        iopaddr_t           vmeaddr; 
        size_t              mapsz;   
	unsigned            flags; 
};

/* Intr control information */
struct vmeio_intr_s {
        vertex_hdl_t        dev;   /* Associated VME device                 */
	vmeio_intr_level_t  level; /* VMEbus interrupt level                */
        vmeio_intr_vector_t vec;   /* VMEbus interrupt vector               */
	intr_func_t         func;  /* Interrupt function                    */
	intr_arg_t          arg;   /* Argument passed of interrupt function */
};


/* 
 * Access function of vmeio_info_t object
 */
extern void vmeio_info_set(vertex_hdl_t, vmeio_info_t);
extern void vmeio_info_dev_set(vmeio_info_t vme_info, vertex_hdl_t vme_conn);
extern void vmeio_info_provider_set(vmeio_info_t, vertex_hdl_t); 
extern void vmeio_info_master_set(vmeio_info_t vme_info, vertex_hdl_t master);
extern vertex_hdl_t vmeio_info_master_get(vmeio_info_t vme_info);
extern void vmeio_info_provider_fastinfo_set(vmeio_info_t, arbitrary_info_t); 
extern void vmeio_info_master_fastinfo_set(vmeio_info_t vmeio_info, 
					   arbitrary_info_t master_fastinfo);
extern arbitrary_info_t 
vmeio_info_master_fastinfo_get(vmeio_info_t vmeio_info);
extern void vmeio_info_am_set(vmeio_info_t, vmeio_am_t);
extern void vmeio_info_addr_set(vmeio_info_t, iopaddr_t);
extern vmeio_am_t vmeio_info_am_get(vmeio_info_t);

extern void 
vmeio_info_errhandler_set(vmeio_info_t, error_handler_f *);

extern error_handler_f *
vmeio_info_errhandler_get(vmeio_info_t);

extern void 
vmeio_info_errinfo_set(vmeio_info_t, error_handler_arg_t);

extern error_handler_arg_t 
vmeio_info_errinfo_get(vmeio_info_t);
extern void vmeio_info_private_set(vmeio_info_t, void *);

extern int
vmeio_error_handler(vertex_hdl_t   vme_conn,
		    int            err_code,
		    ioerror_mode_t err_mode,
		    ioerror_t *    ioerr);


/* hardware graph edges */
#define EDGE_LBL_A16N           "a16n"
#define EDGE_LBL_A16S           "a16s"
#define EDGE_LBL_A24N           "a24n"
#define EDGE_LBL_A24S           "a24s"
#define EDGE_LBL_A32N           "a32n"
#define EDGE_LBL_A32S           "a32s"

#endif /* __VME_VMEIO_PRIVATE_H__ */






