#if IP28
#ifndef __SYS_IP28ADDRS_H__
#define __SYS_IP28ADDRS_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
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
          Definitions of physical memory usage for the IP28



				+-------------------------------+
0xa80000001fc00000		| prom text & read only data	| 2 A
				+-------------------------------+

0x9000000021000000 (16M)	+-------------------------------+
				| IP28prom stack v 		| 1
				+-------------------------------+
0x9000000020f00000 (15M)	| IP28prom bss			| 2
				+-------------------------------+
0x9800000020c00000		| test scratch area (3MB)  	|
				+-------------------------------+
0xa800000020?????? 		| SASH/FX (text,data,bss)	| 3
				+-------------------------------+
0x9000000020940000 		| IDE (data,bss)		| 6
				+-------------------------------+
0xa800000020800000 		| IDE (text)			| 6
				+-------------------------------+
0x9000000020600000		| IP28 debug prom text/data/bss	| 2
				+-------------------------------+
0x9800000020080000		| unix.kdebug			| 4
				+-------------------------------+
0x9800000020007000		| Symmon			| 5
				+-------------------------------+
0x9000000020006000		| Symmon's stack		| 1
				+-------------------------------+
0xa800000020003000		| UNIX	(non debug)		| 4
				+-------------------------------+

				<<<< Address Space Gap >>>>>>>>>> 9

				+-------------------------------+
0x0000000000002000 - 0x23ff	| ECC temp eframes for IP26 bb  | 1
				+-------------------------------+
0x9800000000001c00 - 0x1fff	| Private Vector 		| 8
				+-------------------------------+
0x9800000000001800 - 0x1bff	| Firmware Vector		| 8
				+-------------------------------+
0x9800000000001000 - 0x17ff	| System Parameter Block	| 7
				+-------------------------------+
0x9000000000000c00 - 0x0dff	| NMI and ecc test buffer	|
				+-------------------------------+
0x0000000000000500 - 0x0bff	| CPU ECC error spring board	|
				+-------------------------------+
0x9000000000000400 - 0x0440	| GDA				| B
				+-------------------------------+
0xffffffff80000180		| General Exception handler	|
				+-------------------------------+
0xffffffff80000100		| ECC Exception handler		|
				+-------------------------------+
0xffffffff80000080		| eXtended TLB Miss handler	|
				+-------------------------------+
0xffffffff80000000		| UTLB Miss handler		|
				+-------------------------------+

NOTES:

1. Defined in kern/sys/mc.h
2. Defined in arcs/IP28prom/Makefile
3. Relocated as required by PROM
4. Defined in kern/master.d/system.gen
5. Defined in arcs/symmon/Makefile
6. Defined in arcs/ide/Makefile
7. Defined in kern/sys/arcs/spb.h
8. Defined in arcs/lib/libsk/spb.c
9. Physical memory from 0 - 512K is aliased at address 0 and at 128M.  
A. Prom text may be accessed cached or uncached.
B. Defined in kern/sys/RACER/gda.h (shared with IP30).

*/

#endif /* __SYS_IP28ADDRS_H__ */
#endif /* IP28 */
