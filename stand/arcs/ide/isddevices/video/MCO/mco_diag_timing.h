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
** $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/isddevices/video/MCO/RCS/mco_diag_timing.h,v 1.1 1996/03/23 01:13:59 davidl Exp $
*/

#include "sys/mgras.h"


extern int mgras_num_monitor_defaults;

/* Utility functions */
extern void MgrasCalcScreenSize(struct mgras_timing_info *tinfo,
				unsigned short *w, unsigned short *h);

#endif /* __MGRAS_TIMING_H__ */
