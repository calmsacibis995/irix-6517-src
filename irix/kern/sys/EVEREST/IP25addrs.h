/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990,1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_EVEREST_IP25ADDRS_H__
#define __SYS_EVEREST_IP25ADDRS_H__

#ident "$Revision: 1.5 $"

/*
 * Definitions of physical memory usage for EVEREST
 * 
 * Physical memory usage for the EVEREST
 * 
 *
 *
 *				+-------------------------------+
 *				| IP25 PROM text and r/o data   |
 * 0x1fc00000			+-------------------------------+
 *				|				|
 *				    (Free space)	
 *				|				|
 * 0x02000000 (32.0M)		+-------------------------------+
 *				| Debug IO4 text/data/bss/stack |
 * 0x01c00000 (28.0M)		+-------------------------------+
 *				| IO4/EPC enet rcv/xmt buffers  | 
 * 0x01b00000 (27.0M)		+-------------------------------+
 *				| IO4 PROM text/data/bss/stack	|
 * 0x01800000 (24.0M)		+-------------------------------+
 *				| Grinch/Venice gfx PROM (copy) |
 * 0x01700000 (23.0M)		+-------------------------------+
 *				| IO4 Flash PROM transfer buffer|
 * 0x01600000 (22.0M)		+-------------------------------+
 *				|				|
 *				|  FREE MEMORY			|
 *				|				| 
 * 0x00680000 (5.0M)		+-------------------------------+
 *				| UNIX (debug version)		|
 * 0x00100000 (1M)		+-------------------------------+
 *				| Symmon (text/data/bss)	|
 * 0x0002c000 (176K)		+-------------------------------+
 * (64bit: 0x54000		| Symmon stack for CPU #39	|
 * 0x0002b000 (172K)		+-------------------------------+
 * (64bit: 0x52000)		|             ..		|
 *				|	      ..		|
 *				|             ..		|
 *				+-------------------------------+
 *				| Symmon stack for CPU #1	|
 * 0x00005000 (20K)		+-------------------------------+ 
 * (64bit: 0x6000)		| Symmon stack for CPU #0	|
 *				|   (if debugging)		|
 *				|       ---------------		|
 *				| UNIX (non-debugging)		| 
 * 0x00004000 (16K)		+-------------------------------+
 *				| Everest MPCONF block		|
 * 0x00003000 (12K)		+-------------------------------+
 *                              | Everest error log block       |
 * 0x00002800 (10K)		+-------------------------------+
 *				| Ev sysconfig block 		|
 * 0x00002000 (8K)		+-------------------------------+
 *				| ARCS System Param Block 	|
 * 0x00001000 (4K)		+-------------------------------+
 *				| Cache err EFRAME	   	|
 * 0x00000e00 (3.5k)		+-------------------------------+
 *				| Everest extended error	|
 * 0x00000a00 (2.5K)		+-------------------------------+
 *				| PROM/Kernel GDA Block		|
 * 0x00000400 (1K)		+-------------------------------+
 *				| General Exception Handler     |
 * 0x00000180			+-------------------------------+
 *				| Xutlbmiss handler 		|
 * 0x00000080			+-------------------------------+
 *				| utlbmiss handler		|
 * 0x00000000 (0K)		+-------------------------------+
 *
 * General Notes:  This memory map supports a maximum of 40 CPUS
 * (limited by number of symmon stacks).  Given the organization
 * of the system bus, there probably isn't any way to squeeze more
 * processors onto a backplane, so forty should be ample. 
 */


/*
 * Address definitions
 */
#define PHYS_RAMBASE		0x00000000
#define K0_RAMBASE		PHYS_TO_K0(PHYS_RAMBASE)

#define	GDA_ADDR		PHYS_TO_K1(0x00000400)
#define SYSPARAM_ADDR		PHYS_TO_K1(0x00001000)
#define EVCFGINFO_ADDR		PHYS_TO_K1(0x00002000)
#define EVERROR_ADDR		PHYS_TO_K1(0x00002800)
#define EVERROR_EXT_ADDR		PHYS_TO_K1(0x00000a00)
#define MPCONF_ADDR		PHYS_TO_K1(0x00003000)

#define SYMMON_STACK_SIZE	0x2000
#define SYMMON_STACK_ADDR(_x)	(PHYS_TO_K1(0x00006000) + \
						SYMMON_STACK_SIZE * (_x))  
#define SYMMON_STACK		SYMMON_STACK_ADDR(0)
				/* early_arcs_stack overlaps symmon stacks */
#define EARLY_ARCS_STACK	PHYS_TO_K0(0x00007000)
#define NODEBUGUNIX_ADDR	PHYS_TO_K0(0x00004000)
#define FREEMEM_BASE		PHYS_TO_K0(0x02000000)
#define	IP25_CONFIG_ADDR	PHYS_TO_K1(0x1fc00400)

/*
 * IP25 PROM vectors
 */

#define IP25PROM_RESTART	SBUS_TO_KVU(0x1fc00008)
#define IP25PROM_RESLAVE	SBUS_TO_KVU(0x1fc00010)
#define IP25PROM_PODMODE	SBUS_TO_KVU(0x1fc00018)
#define IP25PROM_EPCUARTPOD	SBUS_TO_KVU(0x1fc00020)
#define IP25PROM_FLASHLEDS	SBUS_TO_KVU(0x1fc00028)

#define EV_PROM_RESTART		IP25PROM_RESTART
#define EV_PROM_RESLAVE		IP25PROM_RESLAVE
#define EV_PROM_PODMODE		IP25PROM_PODMODE
#define EV_PROM_EPCUARTPOD	IP25PROM_EPCUARTPOD
#define EV_PROM_FLASHLEDS	IP25PROM_FLASHLEDS

/*
 * EVEREST IO4 PROM addresses
 */
#ifdef SABLE
#define IP25PROM_STACK		PHYS_TO_K0(0x00780000)
#else
#define IP25PROM_STACK		PHYS_TO_K0(0x00980000)
#endif
#define IP25PROM_BASE		PHYS_TO_K1(0x1fc00000)
#define IP25PROM_SIZE		0x40000

#define PROM_BASE		PHYS_TO_K0(0x01800000)
#define PROM_SIZE		0x300000
#define DPROM_BASE		PHYS_TO_K0(0x01c00000)
#define DPROM_SIZE		0x400000
#define GFXPROM_BASE		PHYS_TO_K0(0x01700000)
#define GFXPROM_SIZE		0x100000
#define	ENETBUFS_BASE		0x01b00000	/* physical */
#define ENETBUFS_SIZE		0x100000
#define FLASHBUF_BASE		PHYS_TO_K0(0x01600000)
#define FLASHBUF_SIZE		0x100000
#define SLAVESTACK_BASE		PHYS_TO_K0(0x01580000)
#define SLAVESTACK_SIZE		0x40000
#define IO4STACK_SIZE		(128 * 1024)
	  
/* Location of the eframe and stack for the ecc handler.
 * Since the R4000 'replaces' KUSEG with an unmapped, uncached space
 * corresponding to physical memory when a cache error occurs, these are
 * the actual addresses used.
 * 
 * THIS STUFF DOESN'T BELONG IN THIS FILE.  IT IS KERNEL-SPECIFIC.
 */
#define CACHE_ERR_EFRAME	(0x1000 - EF_SIZE)
#define CACHE_ERR_ECCFRAME	(CACHE_ERR_EFRAME - ECCF_SIZE)

/* 4 arguments on the stack. */
#define CACHE_ERR_SP_PTR		(CACHE_ERR_ECCFRAME - 4 * sizeof(long))
#define CACHE_ERR_STACK_SIZE		(NBPP)
#endif	/* __SYS_EVEREST_IP25ADDRS_H__ */
