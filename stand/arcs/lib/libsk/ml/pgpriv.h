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

#ident "$Revision: 1.3 $"

/* 
 * pgpriv.h
 *
 * Proto definitions for all call back functions used
 * in pg.c and pglib.c
 */

vertex_hdl_t 	create_graph(char *) ;
extern void 	print_pg_paths(graph_hdl_t, vertex_hdl_t, char *) ;

void pgCreateBrdDefVoid		(lboard_t *, vertex_hdl_t) ;

void pgCreateBrdNode	(lboard_t *, vertex_hdl_t) ;
void pgCreateBrdIo  	(lboard_t *, vertex_hdl_t) ;
void pgCreateBrdNone	(lboard_t *, vertex_hdl_t) ;

void pgCreateBrdRouter	(lboard_t *, vertex_hdl_t) ;
void pgCreateBrdGfx 	(lboard_t *, vertex_hdl_t) ;

void pgAddNicInfo 	(lboard_t *, vertex_hdl_t) ;

void pgCreateCompNone	(lboard_t *, klinfo_t *, vertex_hdl_t) ;
void pgCreateCompCpu 	(lboard_t *, klinfo_t *, vertex_hdl_t) ;
void pgCreateCompMem 	(lboard_t *, klinfo_t *, vertex_hdl_t) ;
void pgCreateCompPci 	(lboard_t *, klinfo_t *, vertex_hdl_t) ;
void pgCreateCompGfx 	(lboard_t *, klinfo_t *, vertex_hdl_t) ;


