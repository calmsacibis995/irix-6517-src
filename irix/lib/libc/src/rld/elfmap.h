#ifndef __ELFMAP_H__
#define __ELFMAP_H__

/*
 * elfmap.h
 *
 *	Map an executable or shared object in ELF format into
 *      memory in much the same way that exec() would.
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

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#include "rld_defs.h"

#define _EM_ERROR -1

#define _EM_NO_FORCE 0x1
#define _EM_DEBUG_TEXT 0x2
#define _EM_SILENT 0x4

ELF_ADDR __elfmap(char *,int,int);
ELF_ADDR __elfunmap(ELF_ADDR, int);

#endif /* C || C++ */

#endif /* !__ELFMAP_H__ */
