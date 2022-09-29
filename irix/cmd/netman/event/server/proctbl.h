#ifndef _NV_PROCTBL_H_
#define _NV_PROCTBL_H_
/* proctbl.h  - keep track of event server client pids
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

#include <sys/types.h>
#include <sys/syslog.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <bstring.h>
#include <stdio.h>
#include <osfcn.h>
#include <syslog.h>
#include <signal.h>

enum Bool {FALSE = 0, TRUE = 1};

#define PROC_TBL_SIZE 8		//default # of simultaneous clients

class procTable {
public:
    procTable(int sz = PROC_TBL_SIZE);
    Bool add (pid_t pid);
    int scan (void);
private:
    int remove (pid_t pid);
    Bool extant (pid_t pid);
    int grow(void);
    void dump(void);
    int size;
    int pcount;
    pid_t *pa, *tpa;    
};

#endif
