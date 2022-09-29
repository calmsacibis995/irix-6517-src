#ifndef _TT_EVENTDOPS_H_
#define _TT_EVENTDOPS_H_
/*
 * ttEventOps.h -- opnums for the eventd ptype file and for the event daemon
 * 
 * $Revision: 1.2 $
 * This file must be kept current with the files nveventd_types, eventStrings.h
 * msg.c++ tteventdOps.h and tteventdOpStrs.h 
 * 
 */
 
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

 
 
#define NV_EV_START_STOP_SNOOP		1
#define NV_EV_DETECT_NODE		2
#define	NV_EV_DETECT_NET		3
#define NV_EV_DETECT_PROTO		4
#define NV_EV_DETECT_CONV		5
#define NV_EV_RATE_RPT			6
#define NV_EV_TOPN_RPT			7
#define NV_EV_HILITE_NODE		8
#define NV_EV_HILITE_NET		9
#define NV_EV_HILITE_CONV		10
#define NV_EV_REGISTER			11

#define TT_NOSEND_ARG_NUM		13  // the arg number for the noSendList

#endif
