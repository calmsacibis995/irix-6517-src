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
#ifndef __XTALK_XSWITCH_H__
#define __XTALK_XSWITCH_H__

#ident "sys/xtalk/xswitch.h: $Revision: 1.8 $"

/*
 * xswitch.h - controls the format of the data
 * provided by xswitch verticies back to the
 * xtalk bus providers.
 */

#if LANGUAGE_C

typedef struct xswitch_info_s *xswitch_info_t;

typedef int
                        xswitch_reset_link_f(vertex_hdl_t xconn);

typedef struct xswitch_provider_s {
    xswitch_reset_link_f   *reset_link;
} xswitch_provider_t;

extern void             xswitch_provider_register(vertex_hdl_t sw_vhdl, xswitch_provider_t * xsw_fns);

xswitch_reset_link_f    xswitch_reset_link;

extern xswitch_info_t   xswitch_info_new(vertex_hdl_t vhdl);

extern void             xswitch_info_link_is_ok(xswitch_info_t xswitch_info,
						xwidgetnum_t port);
extern void             xswitch_info_vhdl_set(xswitch_info_t xswitch_info,
					      xwidgetnum_t port,
					      vertex_hdl_t xwidget);
extern void             xswitch_info_master_assignment_set(xswitch_info_t xswitch_info,
						       xwidgetnum_t port,
					       vertex_hdl_t master_vhdl);

extern xswitch_info_t   xswitch_info_get(vertex_hdl_t vhdl);

extern int              xswitch_info_link_ok(xswitch_info_t xswitch_info,
					     xwidgetnum_t port);
extern vertex_hdl_t     xswitch_info_vhdl_get(xswitch_info_t xswitch_info,
					      xwidgetnum_t port);
extern vertex_hdl_t     xswitch_info_master_assignment_get(xswitch_info_t xswitch_info,
						      xwidgetnum_t port);

extern int		xswitch_id_get(vertex_hdl_t vhdl);
extern void		xswitch_id_set(vertex_hdl_t vhdl,int xbow_num);

#endif				/* LANGUAGE_C */

#endif				/* __XTALK_XSWITCH_H__ */
