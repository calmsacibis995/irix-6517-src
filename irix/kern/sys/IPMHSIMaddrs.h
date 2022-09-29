#if IPMHSIM
#ifndef __SYS_IPMHSIMADDRS_H__
#define __SYS_IPMHSIMADDRS_H__

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
#ident "$Revision: 1.1 $"

/*
          Definitions of physical memory usage for the IPMHSIM



				---------------------------------
0x1fc00000			| prom text & read only data	| 2 A
				---------------------------------

0xa0800000 (8M)			---------------------------------
				| IP22prom stack v 		| 1
				---------------------------------
0xa0740000 (7.25M)		| IP22prom bss			| 2
				---------------------------------
0x80?????? 			| SASH/FX (text,data,bss)	| 3
				---------------------------------
0xa0600000 			| IDE (data,bss)		| 6
				---------------------------------
0x804c0000 			| IDE (text)			| 6
				---------------------------------
0xa0400000			| 				| 2
				---------------------------------
0x8005e000			| unix.kdebug			| 4
				---------------------------------
0xa0007010			| 				| 5
				---------------------------------
0x80006000			| Symmon's stack		| 1
				---------------------------------
0x80002000			| UNIX	(non debug)		| 4
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

1. Defined in kern/sys/IPMHSIM.h
3. Relocated as required by PROM
4. Defined in kern/master.d/system.gen
7. Defined in kern/sys/arcs/spb.h
8. Defined in arcs/lib/libsk/spb.c
A. Prom text may be accessed cached or uncached.

*/

#endif /* __SYS_IPMHSIMADDRS_H__ */
#endif /* IP22 */
