/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_KSYS_SERVICE_H_
#define	_KSYS_SERVICE_H_	1
#ident "$Id: service.h,v 1.10 1997/09/08 12:59:39 henseler Exp $"

#ifndef CELL
#error included by non-CELL configuration
#endif

/*
 * Definitions relating to distributed services
 */

/*
 * Service numbers. 
 */
#define SVC_PROCESS_MGMT	0
#define SVC_CFS			1
#define SVC_SVIPC		2
#define SVC_VSOCK		3
#define SVC_PGRP		4
#define SVC_OBJECT		5
#define SVC_VFILE		6
#define SVC_WPD_MGR		7
#define SVC_WPD_SVR		8
#define SVC_HOST		9
#define SVC_SESSION		10
#define SVC_HEART_BEAT		11
#define SVC_SPECFS		12
#define SVC_CREDID		13
#define	SVC_CMSID		14
#define	SVC_EXIM		15
#define	SVC_CXFS		16
#define	SVC_CELL_TEST		17
#define	SVC_UTILITY		18

#define SVC_MAX			18

#define NUMSVCS			(SVC_MAX+1)

/*
 * We make this a struct so that it is harder to pass a cell number off
 * as a service_t.
 */
typedef struct {
	union {
		struct {
			short	cell;
			short	svcnum;
		} struct_value;
		int	int_value;
	} un;
} service_t;

#define	s_cell		un.struct_value.cell
#define	s_svcnum	un.struct_value.svcnum
#define	s_intval	un.int_value

#define SERVICE_MAKE(svc, cell, svcnum)				\
        { 							\
	        (svc).s_cell = cell; 				\
		(svc).s_svcnum = svcnum; 			\
	}

#define SERVICE_MAKE_NULL(svc)		(svc).s_cell = -1
#define SERVICE_IS_NULL(svc)		((svc).s_cell == -1)

#define	SERVICE_TO_CELL(svc)		(svc).s_cell
#define SERVICE_TO_SVCNUM(svc)		(svc).s_svcnum
#define SERVICE_EQUAL(svc1, svc2)	((svc1).s_intval == (svc2).s_intval)

#define	SERVICE_TO_WP_VALUE(svc)	((svc).s_intval)
#define SERVICE_FROMWP_VALUE(svc, val)	((svc).s_intval = (val))

#endif	/* _KSYS_SERVICE_H_ */
