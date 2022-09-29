#if IP20
#ifndef __SYS_IP20ADDRS_H__
#define __SYS_IP20ADDRS_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.5 $"

/*
          Definitions of physical memory usage for the IP20



				---------------------------------
0x1fc00000			| prom text & read only data	| 2
				---------------------------------

0xa8800000 (8M)			---------------------------------
				| IP20prom stack v 		| 1
				---------------------------------
0xa8740000 (7.25M)		| IP20prom bss			| 2
				---------------------------------
0x88?????? 			| SASH/FX (text,data,bss)	| 3
				---------------------------------
0xa85a0000 			| IDE (data,bss)		| 6
				---------------------------------
0x88500000 			| IDE (text)			| 6
				---------------------------------
0xa8400000			| IP20 debug prom text/data/bss	| 2
				---------------------------------
0x88062000			| unix.kdebug			| 4
				---------------------------------
0xa8007010			| Symmon			| 5
				---------------------------------
0x88006000			| Symmon's stack		| 1
				---------------------------------
0x88002000			| UNIX	(non debug)		| 4
				---------------------------------

				<<<< Address Space Gap >>>>>>>>>> 9

				---------------------------------
0x80001c00 - 0x80001fff		| Private Vector		| 8
				---------------------------------
0x80001800 - 0x80001bff		| Firmware Vector               | 8
				---------------------------------
0x80001000 - 0x800017ff		| System Parameter Block	| 7
				---------------------------------
				| Cache Error stack             | 1
				_________________________________
0x80000180			| General Exception handler	|
				_________________________________
0x80000100			| ECC Exception handler		|
				_________________________________
0x80000080			| eXtended TLB Miss handler	|
				---------------------------------
0x80000000			| UTLB Miss handler		|
				---------------------------------

NOTES:


1. Defined in irix/sys/IP20.h
2. Defined in arcs/IP20prom/Makefile
3. Relocated as required by PROM
4. Defined in irix/master.d/system.gen
5. Defined in arcs/symmon/Makefile
6. Defined in arcs/ide/Makefile
7. Defined in irix/sys/arcs/spb.h
8. Defined in arcs/lib/libsk/spb.c
9. Physical memory from 0 - 512K is aliased at address 0 and at 128M.  

*/

#endif /* __SYS_IP20ADDRS_H__ */
#endif /* IP20 */
