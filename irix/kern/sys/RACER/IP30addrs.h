#if IP30
#ifndef __SYS_IP30ADDRS_H__
#define __SYS_IP30ADDRS_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.12 $"

/*
          Definitions of physical memory usage for the IP30

				---------------------------------
				| SASH/FX (text,data,bss)	| 3
0xa80000002??????? (relocated)	---------------------------------

0x9000000021800000		---------------------------------
				| test scratch area (8MB)	|
0xa800000021000000		---------------------------------
				| IP30prom stack v 		| 1
				|  ---------------------------  |
				| IP30prom bss			| 2
0xa800000020f00000		---------------------------------
		 		| IDE (data,bss) (6MB)		| 6
0x9000000020980000		---------------------------------
		 		| IDE (text)			| 6
0xa800000020800000		---------------------------------
				| IP30 debug prom bss		| 2
0x9000000020700000		---------------------------------
				| IP30 debug prom text/data	| 2
0x9000000020600000		---------------------------------
				| unix.kdebug			| 4
0xa800000020080000		---------------------------------
				| Symmon			| 5
0xa80000002000a000		---------------------------------

0xa800000020008000		---------------------------------
				| Symmon's stack for CPU 1 v	| 1
0xa800000020006000		---------------------------------
				| Symmon's stack for CPU 0 v	| 1
				---------------------------------
				| UNIX	(non debug)		| 4
0xa800000020004000		---------------------------------

					..............

0xffffffff9fd00000		---------------------------------
				| prom text & read only data	| 2 A
0xffffffff9fc00000		---------------------------------


				<<<< Address Space Gap >>>>>>>>>> 9
0xa800000000002008		---------------------------------
				| rprom slave vector		|
0xa800000000002000		---------------------------------
				| Private Vector 		| 8
0xa800000000001c00		---------------------------------
				| Firmware Vector		| 8
0xa800000000001800		---------------------------------
				| System Parameter Block	| 7
0xa800000000001000		---------------------------------
				| Cache Error stack v (TBD)	|
				|  ---------------------------  |
				| Early NMI vector buffer	|
0xa800000000000800		---------------------------------
				| MPCONF			| C
0xa800000000000600		---------------------------------
				| GDA				| B
0xa800000000000400		---------------------------------
				| General Exception handler	|
0xffffffff80000180		---------------------------------
				| ECC Exception handler		|
0xffffffff80000100		---------------------------------
				| eXtended TLB Miss handler	|
0xffffffff80000080		---------------------------------
				| UTLB Miss handler		|
0xffffffff80000000		---------------------------------

NOTES:

1. Defined in kern/sys/RACER/IP30.h
2. Defined in arcs/IP30prom/Makefile
3. Relocated as required by PROM
4. Defined in kern/master.d/system.gen
5. Defined in arcs/symmon/Makefile
6. Defined in arcs/ide/Makefile
7. Defined in kern/sys/arcs/spb.h
8. Defined in arcs/lib/libsk/spb.c
9. Physical memory from 0 - 16KB is aliased at address 0.
A. Prom text may be accessed cached or uncached.
B. Defined in kern/sys/RACER/gda.h
C. Defined in kern/sys/RACER/racermp.h

*/

#define IP30_SCRATCH_MEM	(PHYS_RAMBASE+0x01000000)
#define IP30_EARLY_NMI		(0x800)
#define IP30_RPROM_SLAVE	(0x2000)

#endif /* __SYS_IP30ADDRS_H__ */
#endif /* IP30 */
