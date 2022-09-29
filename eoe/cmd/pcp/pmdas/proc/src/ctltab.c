/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: ctltab.c,v 1.1 1997/06/20 00:33:33 markgw Exp $"

#include <sys/procfs.h>
#include "pmapi.h"
#include "pglobal.h"
#include "pmemory.h"
#include "pracinfo.h"
#include "pscred.h"
#include "psinfo.h"
#include "pstatus.h"
#include "psusage.h"
#include "ctltab.h"

ctltab_entry ctltab[] = {
    { 1, pglobal_init,  pglobal_getdesc,  pglobal_setatom,  pglobal_getinfo,  pglobal_allocbuf },
    { 1, psinfo_init,   psinfo_getdesc,   psinfo_setatom,   psinfo_getinfo,   psinfo_allocbuf },
    { 1, pstatus_init,  pstatus_getdesc,  pstatus_setatom,  pstatus_getinfo,  pstatus_allocbuf },
    { 1, pscred_init,   pscred_getdesc,   pscred_setatom,   pscred_getinfo,   pscred_allocbuf },
    { 1, psusage_init,  psusage_getdesc,  psusage_setatom,  psusage_getinfo,  psusage_allocbuf },
    { 1, pmem_init,     pmem_getdesc,     pmem_setatom,     pmem_getinfo,     pmem_allocbuf },
#if defined(IRIX5_3)
    { 0, pracinfo_init, pracinfo_getdesc, pracinfo_setatom, pracinfo_getinfo, pracinfo_allocbuf }
#else
    { 1, pracinfo_init, pracinfo_getdesc, pracinfo_setatom, pracinfo_getinfo, pracinfo_allocbuf }
#endif
};

int nctltab = sizeof(ctltab) / sizeof (ctltab[0]);
