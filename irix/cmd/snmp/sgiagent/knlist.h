/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Kernel name list definitions
 *
 *	$Revision: 1.2 $
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

#include <nlist.h>

#define N_KERNEL_MAGIC	0
#define N_END		1
#define N_TCB		2
#define N_UDB		3

#ifdef sgi
extern struct nlist64 nl64[];
#endif /* sgi */
extern struct nlist nl[];

extern void init_nlist(char *);
extern off_t klseek(off_t, int = 0);
extern int kread(void *, int);
extern short is_elf64;
