
#ifndef __TLB_H__
#define __TLB_H__

/*
 * IP17diags/include/tlb.h
 *
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.2 $"

#include <sys/immu.h>


#define TLBLO_DATA (TLBLO_PFNMASK|TLBLO_CACHMASK|TLBLO_D|TLBLO_V|TLBLO_G)
#define TLBHI_DATA (TLBHI_VPNMASK|TLBHI_PIDMASK)

#undef PG_LOCK				/* avoid warning about difference */
#define PG_LOCK		PG_GS		/* defined differently in immu.h */

#endif /* !__TLB_H__ */
