/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: George Wilkie
 *
 * frsctl.h of snet module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

/*
 * FRS control functions and definitions
 */

#ifndef FRSDEV
#define FRSDEV	"/dev/ifr"
#endif

#ifndef FR_SETSNID
#include <sys/snet/uint.h>
#include "frs_control.h"
#endif

extern int frs_set_snid(int, int, uint32);
extern int frs_get_ppa_stats(int, uint32, struct fr_ppa_stats *);
extern int frs_zero_ppa_stats(int, uint32);
extern int frs_get_pvc_stats(int, uint32, uint32, struct fr_pvc_stats *);
extern int frs_zero_pvc_stats(int, uint32, uint32);
extern int frs_get_ppa_status(int, uint32, uint32 *, uint32 *, unsigned long  **);
extern int frs_settune(int, uint32, struct fr_ppa_tune *);
extern int frs_gettune(int, uint32, struct fr_ppa_tune *);
extern int frs_setctune(int, uint32, uint32, struct fr_pvc_tune *);
extern int frs_getctune(int, uint32, uint32, struct fr_pvc_tune *);
extern int frs_dereg_pvc(int, uint32, uint32);
