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
#define N_IFNET		2
#define N_ARPTAB	3
#define N_ARPTAB_SIZE	4
#define N_IPSTAT	5
#define N_IPFORWARDING	6
#define N_RTHOST	7
#define N_RTNET		8
#define N_RTHASHSIZE	9
#define N_ICMPSTAT	10
#define N_TCPSTAT	11
#define N_TCB		12
#define N_UDPSTAT	13
#define N_UDB		14

#ifdef sgi
extern struct nlist64 nl64[];
#endif /* sgi */
extern struct nlist nl[];

extern void init_nlist(char *);
extern off_t klseek(off_t, int = 0);
extern int kread(void *, int);
extern short is_elf64;
