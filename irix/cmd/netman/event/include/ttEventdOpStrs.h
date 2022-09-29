#ifndef _TT_EVENTD_OPSTRS_
#define _TT_EVENTD_OPSTRS_
/*
 * ttEventdOpStrs.h -- the strings used by tooltalk that define message types
 *                     handled by the ptype for the event daemon nveventd
 * $Revision: 1.1 $
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


#include "ttEventdOps.h"

static char *evdOpStrs [] = {
    "", "NV_EV_Start_Stop_Snoop", "NV_EV_detect_node", "NV_EV_detect_net", 
	"NV_EV_detect_proto", "NV_EV_detect_conv", "NV_EV_rate_rpt", 
	"NV_EV_topN_rpt", "NV_EV_hilite_node", "NV_EV_hilite_net", 
	"NV_EV_hilite_conv", "NV_EV_register"
};


#endif
