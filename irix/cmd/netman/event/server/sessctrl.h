#ifndef _NV_SESSCTRL_H_
#define _NV_SESSCTRL_H_
/* sessctrl.h  - keep track of applications connected and terminate
 * 
 *	NetVisualyzer Event Server
 *
 *	$Revision: 1.1 $
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

#define SELF_KILL_TIME	120

class EV_event;

class sessCtrl {
public:
    sessCtrl (unsigned int timeout = SELF_KILL_TIME);
    void doCtrl (EV_event *ep);
};


#endif
