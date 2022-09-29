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
#ifndef __XTALK_XWIDGET_H__
#define __XTALK_XWIDGET_H__

#ident "$Revision: 1.32 $"

/*
 * xwidget.h - generic crosstalk widget header file
 */

#include "xtalk.h"
#if LANGUAGE_C
#include <sys/cdl.h>
#endif /* LANGUAGE_C */

#define WIDGET_ID			0x04
#define WIDGET_STATUS			0x0c
#define WIDGET_ERR_UPPER_ADDR		0x14
#define WIDGET_ERR_LOWER_ADDR		0x1c
#define WIDGET_CONTROL			0x24
#define WIDGET_REQ_TIMEOUT		0x2c
#define WIDGET_INTDEST_UPPER_ADDR	0x34
#define WIDGET_INTDEST_LOWER_ADDR	0x3c
#define WIDGET_ERR_CMD_WORD		0x44
#define WIDGET_LLP_CFG			0x4c
#define WIDGET_TFLUSH			0x54

/* WIDGET_ID */
#define WIDGET_REV_NUM			0xf0000000
#define WIDGET_PART_NUM			0x0ffff000
#define WIDGET_MFG_NUM			0x00000ffe
#define WIDGET_REV_NUM_SHFT		28
#define WIDGET_PART_NUM_SHFT		12
#define WIDGET_MFG_NUM_SHFT		1

#define XWIDGET_PART_NUM(widgetid) (((widgetid) & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT)
#define XWIDGET_REV_NUM(widgetid) (((widgetid) & WIDGET_REV_NUM) >> WIDGET_REV_NUM_SHFT)
#define XWIDGET_MFG_NUM(widgetid) (((widgetid) & WIDGET_MFG_NUM) >> WIDGET_MFG_NUM_SHFT)

/* WIDGET_STATUS */
#define WIDGET_LLP_REC_CNT		0xff000000
#define WIDGET_LLP_TX_CNT		0x00ff0000
#define WIDGET_PENDING			0x0000001f

/* WIDGET_ERR_UPPER_ADDR */
#define	WIDGET_ERR_UPPER_ADDR_ONLY	0x0000ffff

/* WIDGET_CONTROL */
#define WIDGET_F_BAD_PKT		0x00010000
#define WIDGET_LLP_XBAR_CRD		0x0000f000
#define	WIDGET_LLP_XBAR_CRD_SHFT	12
#define WIDGET_CLR_RLLP_CNT		0x00000800
#define WIDGET_CLR_TLLP_CNT		0x00000400
#define WIDGET_SYS_END			0x00000200
#define WIDGET_MAX_TRANS		0x000001f0
#define WIDGET_WIDGET_ID		0x0000000f

/* WIDGET_INTDEST_UPPER_ADDR */
#define WIDGET_INT_VECTOR		0xff000000
#define WIDGET_INT_VECTOR_SHFT		24
#define WIDGET_TARGET_ID		0x000f0000
#define WIDGET_TARGET_ID_SHFT		16
#define WIDGET_UPP_ADDR			0x0000ffff

/* WIDGET_ERR_CMD_WORD */
#define WIDGET_DIDN			0xf0000000
#define WIDGET_SIDN			0x0f000000
#define WIDGET_PACTYP			0x00f00000
#define WIDGET_TNUM			0x000f8000
#define WIDGET_COHERENT			0x00004000
#define WIDGET_DS			0x00003000
#define WIDGET_GBR			0x00000800
#define WIDGET_VBPM			0x00000400
#define WIDGET_ERROR			0x00000200
#define WIDGET_BARRIER			0x00000100

/* WIDGET_LLP_CFG */
#define WIDGET_LLP_MAXRETRY		0x03ff0000
#define WIDGET_LLP_MAXRETRY_SHFT	16
#define WIDGET_LLP_NULLTIMEOUT		0x0000fc00
#define WIDGET_LLP_NULLTIMEOUT_SHFT	10
#define WIDGET_LLP_MAXBURST		0x000003ff
#define WIDGET_LLP_MAXBURST_SHFT	0

/*
 * according to the crosstalk spec, only 32-bits access to the widget
 * configuration registers is allowed.  some widgets may allow 64-bits
 * access but software should not depend on it.  registers beyond the
 * widget target flush register are widget dependent thus will not be
 * defined here
 */
#if _LANGUAGE_C
typedef __uint32_t      widgetreg_t;

/* widget configuration registers */
typedef volatile struct widget_cfg {
    widgetreg_t             w_pad_0;	/* 0x00 */
    widgetreg_t             w_id;	/* 0x04 */
    widgetreg_t             w_pad_1;	/* 0x08 */
    widgetreg_t             w_status;	/* 0x0c */
    widgetreg_t             w_pad_2;	/* 0x10 */
    widgetreg_t             w_err_upper_addr;	/* 0x14 */
    widgetreg_t             w_pad_3;	/* 0x18 */
    widgetreg_t             w_err_lower_addr;	/* 0x1c */
    widgetreg_t             w_pad_4;	/* 0x20 */
    widgetreg_t             w_control;	/* 0x24 */
    widgetreg_t             w_pad_5;	/* 0x28 */
    widgetreg_t             w_req_timeout;	/* 0x2c */
    widgetreg_t             w_pad_6;	/* 0x30 */
    widgetreg_t             w_intdest_upper_addr;	/* 0x34 */
    widgetreg_t             w_pad_7;	/* 0x38 */
    widgetreg_t             w_intdest_lower_addr;	/* 0x3c */
    widgetreg_t             w_pad_8;	/* 0x40 */
    widgetreg_t             w_err_cmd_word;	/* 0x44 */
    widgetreg_t             w_pad_9;	/* 0x48 */
    widgetreg_t             w_llp_cfg;	/* 0x4c */
    widgetreg_t             w_pad_10;	/* 0x50 */
    widgetreg_t             w_tflush;	/* 0x54 */
} widget_cfg_t;


typedef struct {
    unsigned                didn:4;
    unsigned                sidn:4;
    unsigned                pactyp:4;
    unsigned                tnum:5;
    unsigned                ct:1;
    unsigned                ds:2;
    unsigned                gbr:1;
    unsigned                vbpm:1;
    unsigned                error:1;
    unsigned                bo:1;
    unsigned                other:8;
} w_err_cmd_word_f;

typedef union {
    widgetreg_t             r;
    w_err_cmd_word_f        f;
} w_err_cmd_word_u;

/* IO widget initialization function */
#include <sys/hwgraph.h>
#include <sys/edt.h>

typedef struct xwidget_info_s *xwidget_info_t;

/*
 * Crosstalk Widget Hardware Identification, as defined in the Crosstalk spec.
 */
typedef struct xwidget_hwid_s {
    xwidget_part_num_t      part_num;
    xwidget_rev_num_t       rev_num;
    xwidget_mfg_num_t       mfg_num;
}                      *xwidget_hwid_t;


/*
 * Returns 1 if a driver that handles devices described by hwid1 is able
 * to manage a device with hardwareid hwid2.  NOTE: We don't check rev
 * numbers at all.
 */
#define XWIDGET_HARDWARE_ID_MATCH(hwid1, hwid2) \
	(((hwid1)->part_num == (hwid2)->part_num) && \
	(((hwid1)->mfg_num == XWIDGET_MFG_NUM_NONE) || \
	((hwid2)->mfg_num == XWIDGET_MFG_NUM_NONE) || \
	((hwid1)->mfg_num == (hwid2)->mfg_num)))


/* Generic crosstalk widget initialization interface */
#if _KERNEL

extern int              xwidget_driver_register(xwidget_part_num_t part_num,
						xwidget_mfg_num_t mfg_num,
						char *driver_prefix,
						unsigned flags);

extern void             xwidget_driver_unregister(char *driver_prefix);

extern int
                        xwidget_init(struct xwidget_hwid_s *hwid,	/* widget's hardware ID */
				     vertex_hdl_t dev,	/* widget to initialize */
				     xwidgetnum_t id,	/* widget's target id (0..f) */
				     vertex_hdl_t master,	/* widget's master vertex */
				     xwidgetnum_t targetid,	/* master's target id (0..f) */
				     async_attach_t aa);

extern void
                        xwidget_error_register(
						  vertex_hdl_t xwidget,		/* connect point of interest */
						  error_handler_f * efunc,	/* function to call */
					      error_handler_arg_t einfo);	/* first parameter */

extern void             xwidget_reset(vertex_hdl_t xwidget);
extern void             xwidget_gfx_reset(vertex_hdl_t xwidget);
extern char		*xwidget_name_get(vertex_hdl_t xwidget);	

/* Generic crosstalk widget information access interface */
extern xwidget_info_t   xwidget_info_chk(vertex_hdl_t widget);
extern xwidget_info_t   xwidget_info_get(vertex_hdl_t widget);
extern void             xwidget_info_set(vertex_hdl_t widget, xwidget_info_t widget_info);
extern vertex_hdl_t     xwidget_info_dev_get(xwidget_info_t xwidget_info);
extern xwidgetnum_t     xwidget_info_id_get(xwidget_info_t xwidget_info);
extern int              xwidget_info_type_get(xwidget_info_t xwidget_info);
extern int              xwidget_info_state_get(xwidget_info_t xwidget_info);
extern vertex_hdl_t     xwidget_info_master_get(xwidget_info_t xwidget_info);
extern xwidgetnum_t     xwidget_info_masterid_get(xwidget_info_t xwidget_info);
extern xwidget_part_num_t xwidget_info_part_num_get(xwidget_info_t xwidget_info);
extern xwidget_rev_num_t xwidget_info_rev_num_get(xwidget_info_t xwidget_info);
extern xwidget_mfg_num_t xwidget_info_mfg_num_get(xwidget_info_t xwidget_info);


/*
 * TBD: DELETE THIS ENTIRE STRUCTURE!  Equivalent is now in
 * xtalk_private.h: xwidget_info_s
 * This is just here for now because we still have a lot of
 * junk referencing it.
 * However, since nobody looks inside ...
 */
typedef struct v_widget_s {
    unsigned                v_widget_s_is_really_empty;
#define	v_widget_s_is_really_empty	and using this would be a syntax error.
} v_widget_t;
#endif				/* _KERNEL */

#endif				/* _LANGUAGE_C */

#endif				/* __XTALK_XWIDGET_H__ */
