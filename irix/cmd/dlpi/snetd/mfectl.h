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
 * mfectl.h of snet module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

/*
 * MFE control functions and definitions
 */

#ifndef MFEDEV
#define MFEDEV	"/dev/mfe"
#endif

#ifndef MFE_ADDVC
#include <sys/snet/uint.h>
#include <sys/snet/mfe_control.h>
#endif

extern int mfe_addvc(int, uint32, uint32, uint32, mfe_tune_t *);
extern int mfe_getvc(int, uint32, uint32, uint32 *, mfe_tune_t *);
extern int mfe_remvc(int, uint32, uint32, uint32);
extern int mfe_addprot(int, mfe_proto_t *);
extern int mfe_getprot(int, uint32, mfe_proto_t *);
extern int mfe_remprot(int, uint32);
extern int mfe_getstats(int, uint32, uint32, uint32 *, mfe_stats_t *);
extern int mfe_zerostats(int, uint32, uint32);
