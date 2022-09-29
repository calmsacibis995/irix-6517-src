/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/prctl.h>
#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include <ulocks.h>
#include "config.h"


extern void sema_init(void);
extern void sema_cleanup();
extern usema_t* sema_create(void);
extern void sema_destroy(usema_t* sema);
extern void sema_p(usema_t* sema);
extern void sema_v(usema_t* sema);
extern barrier_t* barrier_create();
extern void barrier_destroy(barrier_t* barrier);
