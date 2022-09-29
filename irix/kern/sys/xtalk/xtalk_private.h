
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __XTALK_XTALK_PRIVATE_H__
#define __XTALK_XTALK_PRIVATE_H__

#include <sys/ioerror.h>	/* for error function and arg types */
/*
 * xtalk_private.h -- private definitions for xtalk
 * crosstalk drivers should NOT include this file.
 */

#ident "$Revision: 1.12 $"

/*
 * All Crosstalk providers set up PIO using this information.
 */
struct xtalk_piomap_s {
    vertex_hdl_t            xp_dev;	/* a requestor of this mapping */
    xwidgetnum_t            xp_target;	/* target (node's widget number) */
    iopaddr_t               xp_xtalk_addr;	/* which crosstalk addr is mapped */
    size_t                  xp_mapsz;	/* size of this mapping */
    caddr_t                 xp_kvaddr;	/* kernel virtual address to use */
};

/*
 * All Crosstalk providers set up DMA using this information.
 */
struct xtalk_dmamap_s {
    vertex_hdl_t            xd_dev;	/* a requestor of this mapping */
    xwidgetnum_t            xd_target;	/* target (node's widget number) */
};

/*
 * All Crosstalk providers set up interrupts using this information.
 */
struct xtalk_intr_s {
    vertex_hdl_t            xi_dev;	/* requestor of this intr */
    xwidgetnum_t            xi_target;	/* master's widget number */
    xtalk_intr_vector_t     xi_vector;	/* 8-bit interrupt vector */
    iopaddr_t               xi_addr;	/* xtalk address to generate intr */
    void                   *xi_sfarg;	/* argument for setfunc */
    xtalk_intr_setfunc_t    xi_setfunc;		/* device's setfunc routine */
};

/*
 * Xtalk interrupt handler structure access functions
 */
#define	xtalk_intr_arg(xt)	((xt)->xi_sfarg)

#define	xwidget_hwid_is_xswitch(_hwid)	\
		(((_hwid)->part_num == XBOW_WIDGET_PART_NUM ) &&  	\
		 ((_hwid)->mfg_num == XBOW_WIDGET_MFGR_NUM ))

/* common iograph info for all widgets,
 * stashed in FASTINFO of widget connection points.
 */
struct xwidget_info_s {
    char                   *w_fingerprint;
    vertex_hdl_t            w_vertex;	/* back pointer to vertex */
    xwidgetnum_t            w_id;	/* widget id */
    struct xwidget_hwid_s   w_hwid;	/* hardware identification (part/rev/mfg) */
    vertex_hdl_t            w_master;	/* CACHED widget's master */
    xwidgetnum_t            w_masterid;		/* CACHED widget's master's widgetnum */
    error_handler_f        *w_efunc;	/* error handling function */
    error_handler_arg_t     w_einfo;	/* first parameter for efunc */
    char		   *w_name;	/* canonical hwgraph name */	
};

extern char             widget_info_fingerprint[];

#endif				/* __XTALK_XTALK_PRIVATE_H__ */
