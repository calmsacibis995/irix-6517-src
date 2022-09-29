#ifndef __MGRAS_TIMING_H__
#define __MGRAS_TIMING_H__

/*
** Copyright 1993, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
** $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/graphics/MGRAS/RCS/mgras_timing.h,v 1.3 1996/02/06 04:16:00 ack Exp $
*/

#include "sys/mgras.h"

typedef enum {
	TT_1280_1024_60 = 0,
	TT_1280_1024_72,
	TT_1024_768_60_P,
	TT_1280_1024_60_P
} mgras_timing_t;

typedef struct mgras_timing_permontype_s {
    int montype;
    mgras_timing_t table;
} mgras_timing_permontype_t;

extern int mgras_num_monitor_defaults;

extern mgras_timing_permontype_t mgras_default_timing[]; /*MonitorType, table*/
extern struct mgras_timing_info *mgras_timing_tables_1RSS[]; /*index by mgras_timing_t */
extern struct mgras_timing_info *mgras_timing_tables_2RSS[]; /* index by mgras_timing_t */
extern unsigned char mgras_pll_init_vals[][3];  /* index by mgras_timing_t */

/* Utility functions */
extern void MgrasCalcScreenSize(struct mgras_timing_info *tinfo,
				unsigned short *w, unsigned short *h);

#endif /* __MGRAS_TIMING_H__ */
