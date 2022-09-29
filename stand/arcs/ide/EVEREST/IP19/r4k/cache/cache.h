
/*
 * IP19/r4k/cache/cache.h
 *
 *
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
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

#ident "$Revision: 1.1 $"

#define PI_SIZE         (config_info.icache_size)
#define PIL_SIZE        (config_info.iline_size)
#define PD_SIZE         (config_info.dcache_size)
#define PDL_SIZE        (config_info.dline_size)
#define SID_SIZE        (config_info.sidcache_size)
#define SIDL_SIZE       (config_info.sidline_size)

#define SECLINEMASK     (config_info.sidline_size-1)

/* tag_regs structs hold the contents of the TagHi and TagLo registers,
 * which cache tag instructions use to read and write the primary and
 * secondary caches.
 */
typedef struct tag_regs {
        uint tag_lo;
        uint tag_hi;
} tag_regs_t;


