#if IP26
#ifndef __SYS_IP26ADDRS_H__
#define __SYS_IP26ADDRS_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $"

/*
          Definitions of physical memory usage for the IP26



				---------------------------------
0xa80000001fc00000		| prom text & read only data	| 2 A
				---------------------------------

0x9000000009000000 (16M)	---------------------------------
				| IP26prom stack v 		| 1
				---------------------------------
0x9000000008f00000 (15M)	| IP26prom bss			| 2
				---------------------------------
0x9800000008c00000		| test scratch area (3MB)  	|
				---------------------------------
0xa800000008?????? 		| SASH/FX (text,data,bss)	| 3
				---------------------------------
0x9000000008940000 		| IDE (data,bss)		| 6
				---------------------------------
0xa800000008800000 		| IDE (text)			| 6
				---------------------------------
0x9000000008400000		| IP26 debug prom text/data/bss	| 2
				---------------------------------
0x980000000806c000		| unix.kdebug			| 4
				---------------------------------
0x9800000008007000		| Symmon			| 5
				---------------------------------
0x9000000008006000		| Symmon's stack		| 1
				---------------------------------
0xa800000008002000		| UNIX	(non debug)		| 4
				---------------------------------

				<<<< Address Space Gap >>>>>>>>>> 9

				---------------------------------
0x9800000000001c00 - 0x1fff	| Private Vector 		| 8
				---------------------------------
0x9800000000001800 - 0x1bff	| Firmware Vector		| 8
				---------------------------------
0x9800000000001000 - 0x17ff	| System Parameter Block	| 7
				---------------------------------
0x9800000000000000		| Free on TFP			|
				---------------------------------

NOTES:

1. Defined in kern/sys/mc.h
2. Defined in arcs/IP26prom/Makefile
3. Relocated as required by PROM
4. Defined in kern/master.d/system.gen
5. Defined in arcs/symmon/Makefile
6. Defined in arcs/ide/Makefile
7. Defined in kern/sys/arcs/spb.h
8. Defined in arcs/lib/libsk/spb.c
9. Physical memory from 0 - 512K is aliased at address 0 and at 128M.  
A. Prom text may be accessed cached or uncached.

*/

#endif /* __SYS_IP26ADDRS_H__ */
#endif /* IP26 */
