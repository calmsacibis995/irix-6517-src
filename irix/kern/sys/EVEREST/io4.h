/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * io4.h -- Everest I/O controller register definitions and
 *          associated data structures.
 */

#ifndef _SYS_IO4_
#define _SYS_IO4_

#ident "$Revision: 1.36 $"

#if _LANGUAGE_C
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/dmamap.h>
#include <sys/pio.h>
#include <sys/EVEREST/everest.h>
#endif /* _LANGUAGE_C */

/*
 * Type and revision information for the IO4 board.  These values
 * are encoded in the small window address register (reg #1) in
 * the low 8 bits.  You must mask appropriately.
 */

#define IO4_TYPE_VALUE	3
#define IO4_REV_LEVEL	1

#define IO4_TYPE_MASK	0x0f
#define IO4_TYPE_SHFT	0
#define IO4_REV_MASK	0xf0
#define IO4_REV_SHFT	4


/*
 * Physical adapter 0 is not valid, reserved for map ram.
 */
#define IO4_MAX_PADAPS	8

/*
 * The adapter type shown in IO4_CONF_IODEV as defined by h/w.
 */
#define IO4_ADAP_NULL	0
#define IO4_ADAP_SCSI	0x0D
#define IO4_ADAP_EPC	0x0E	
#define IO4_ADAP_FCHIP	0x0F
#define IO4_ADAP_SCIP	0x1D

/*
 * This s/w definition is used to further distinguish the adapter types that
 * connects to the F-chip.
 */
#define IO4_ADAP_VMECC	0x11
#define IO4_ADAP_HIPPI	0x12
#define IO4_ADAP_FCG	0x20
#define IO4_ADAP_DANG	0x2B
#define IO4_ADAP_GIOCC	0x42
#define IO4_ADAP_HIP1A	0x30
#define IO4_ADAP_HIP1B	0x31

#if _LANGUAGE_C

typedef struct io4 {
	int		slot;
	__psunsigned_t	mapram;
} io4_t;
extern io4_t io4[EV_MAX_IO4S];


/*
 * iamap manages a chunk of the IA+ID mapping ram
 */
typedef struct iamap {
	lock_t		lock;
	sv_t		out;
	struct map	*map;
	struct map	*dirty;
	uint		*table;		/* lvl-2 map pages addr, kseg1 */
	uint		size;		/* lvl-2 map page size in bytes */
	iopaddr_t	iostart;	/* defines the io space mapped */
	iopaddr_t	ioend;		/* must be 4MB aligned */
} iamap_t;


/*
 * While program the ia mapping ram (both 1 and 2 level), we use ppfn instead
 * of the real physical address since we cannot put paddr in int.
 * The 1-level map looks like this,
 *	(IA MAP RAM)-->memory block of 2MB.
 * The 2-level map looks like this,
 *	(IA MAP RAM)-->memory page on 2k boundary-->4k page-->dma page in 4k.
 */
#define IAMAP_L1_BLOCKSIZE	0x200000	/* 2MB block */
#define IAMAP_L2_BLOCKSIZE1	0x800		/* 2KB block */
#define IAMAP_L2_BLOCKSIZE	0x1000		/* 4KB block */
#define IAMAP_L1_BLOCKSHIFT	21
#define IAMAP_L2_BLOCKSHIFT	12

#define IAMAP_L1_ADDRSHIFT	11
#define IAMAP_L2_FLAG		0xc0000000

#define IAMAP_L1_ADDR(m,a)	((m)+(((a)>>IAMAP_L1_BLOCKSHIFT) << 3))
#define IAMAP_L1_ADDR_PAIR(m,a)	((m)+(((a)>>(IAMAP_L1_BLOCKSHIFT+1)) << 4))

#define IAMAP_L1_ENTRY(a)	((a)>>IAMAP_L1_ADDRSHIFT)
#define IAMAP_L2_ENTRY(a)	((a)>>IAMAP_L1_ADDRSHIFT | IAMAP_L2_FLAG)

#define	DMA_TYPES	3
#define DMA_SCSI	0
#define DMA_VMEA24	1
#define DMA_VMEA32	2

/*
 * The struct ioadap describes the common attributes of io4 adapters.
 * It doesn't exist by itself and should be part of the adapter specific struct.
 * 
 * phys-to-virt can be obtained from adapmap[slot][padap].
 * virt-to-phys can be obtained from ioadap.slot and ioadap.padap.
 */
typedef struct ioadap {
	unchar		slot;		/* the physical slot number */
	unchar		padap;		/* the physical adap number */
	unchar		type;		/* VME, SCSI, universal ... */
	unchar		mapramid;	/* specify the slice of mapping ram */
	__psunsigned_t	swin;		/* kv addr (kseg1) to small window */
	__psunsigned_t	lwin;		/* kv addr (kseg2) to large window */
} ioadap_t;

#endif /* _LANGUAGE_C */

/*
 * SWIN_BASE(region,padap) - small window virtual address. Because
 *	small windows are located at different places on the IP19
 *	and the IP21, we conditionally change the small window base
 *	address based on which CPU we're compiling for.
 *
 *	region:	0-7 are for io boards (i.e. same as large window id)
 *		8-F are for future use
 *	padap:	physical adapter id
 */

#ifdef IP19
#  define SWINDOW_BASE		SBUS_TO_KVU(0x10000000)
#  define SWINDOW_CEILING	SBUS_TO_KVU(0x18000000)
#else
#  define SWINDOW_BASE		0x9000000400000000ll
#  define SWINDOW_CEILING	0x9000000408000000ll
#endif

#define SWINDOW_PPFN		0x00400000

#define SWIN_REGIONSHIFT	19
#define SWIN_PADAPSHIFT		16

#define SWIN_BASE(region,padap)	(SWINDOW_BASE			+ \
				(region << SWIN_REGIONSHIFT)	+ \
				(padap  << SWIN_PADAPSHIFT))
#define SWIN_REGION(addr)	(((__psunsigned_t)addr>>SWIN_REGIONSHIFT) & 0xF)
#define SWIN_PADAP(addr)	(((__psunsigned_t)addr>>SWIN_PADAPSHIFT) & 0x7)

#define SWIN_SIZE		0x00010000	/* 64KB */


/*
 * Large window information
 */

#define LWINDOW_BASE		0x9000000440000000ll
#define LWINDOW_CEILING		0x9000000600000000ll

#define LWIN_SIZE		0x08000000	/* 128 MB */
#define LWIN_REGIONSHIFT	30
#define LWIN_PADAPSHIFT		27

/* These defs should be used when accessing the iotlb which expects
 * a 4k based pfn even though on K32, the iotlb page size is 4megs.
 */

#define LWIN_PFN_BASE		0x00400000	/* 0x400000000 */
#define LWIN_PFN_WINDOWSHIFT	18		/* 18+12 => 1G window size */
#define LWIN_PFN_PADAPSHIFT	15		/* 15+12 => 128M adap size */
#define LWIN_PFN(window,padap)	(LWIN_PFN_BASE			| \
				 window << LWIN_PFN_WINDOWSHIFT	| \
				 padap  << LWIN_PFN_PADAPSHIFT)
#define LWIN_WINDOW(pfn)	(pfn >> LWIN_PFN_WINDOWSHIFT) & 0x7
#define LWIN_PADAP(pfn)		(pfn >> LWIN_PFN_PADAPSHIFT) & 0x7

/* These defs should be used when doing translations with the
 * systems tlb where page size is NBPP.
 */
#define LWIN_SYSPFN_BASE	(0x400000000ll >> PNUMSHFT)
#define LWIN_SYSPFN_WINDOWSHIFT	(30-PNUMSHFT)	/* 1G window size -> bit 30 */
#define LWIN_SYSPFN_PADAPSHIFT	(27-PNUMSHFT)	/* 128M adap size -> bit 27 */
#define LWIN_SYSPFN(window,padap)	(LWIN_SYSPFN_BASE		| \
				 window << LWIN_SYSPFN_WINDOWSHIFT	| \
				 padap  << LWIN_SYSPFN_PADAPSHIFT)
#define LWIN_SYSWINDOW(pfn)	(pfn >> LWIN_SYSPFN_WINDOWSHIFT) & 0x7
#define LWIN_SYSPADAP(pfn)	(pfn >> LWIN_SYSPFN_PADAPSHIFT) & 0x7

#define LargeWindow(window)	(window | 0x10)
#define SmallWindow(window)	(window | 0x80000)

/*
 * Address Mapping RAM is at IOA-0 large window space.
 * Although the ram is 8192 deep and 32 bit wide, but each entry must be
 * accessed on double word boundary. Therefore a good way to see the ram is
 * 8192x64 and with the following 64-bit format.
 *
 *	63:32	zeros, physically non-existent
 *	31	mapping level bit, 0: 1-level, 1: 2-level
 *	30:0	mapping address
 *
 * The mapping ram must be written as word or double-word.
 *
 * Also, there are two kinds of mappings, 1-level and 2-level.
 * One-level is used primary for smaller size (less than 1-page) mappings,
 * the mapped Ebus physical page# is written into the mapping ram.
 * For two-level mapping, the half-page# is written to the mapping ram,
 * this address points to a 2kb (half-page) aligned second-level map in
 * memory.
 */

#define IO4_MAPRAM_SIZE		0x10000		/* the whole board */
#define IOA_MAPRAM_SIZE		0x4000		/* per adapter */

/*
 *	IO Board Configuration Registers.
 */

#define IO4_CONFIGADDR(slot,reg)	EV_CONFIGADDR(slot,0,reg)
#define IO4_GETCONF_REG(slot,reg)	EV_GETCONFIG_REG(slot,0,reg)
#define IO4_GETCONF_REG_NOWAR(slot,reg)	EV_GETCONFIG_REG_NOWAR(slot,0,reg)
#define IO4_SETCONF_REG(slot,reg,value) EV_SETCONFIG_REG(slot,0,reg,value)

#define IO4_CONF_LW		0x00	/* 9:0   rw  large window map	*/
#define IO4_CONF_SW		0x01	/* 20:0  rw  small window map	*/
#define IO4_CONF_REVTYPE	0x01	/* 7:0	 r   type & rev info	*/
#define IO4_CONF_ADAP		0x02	/* 31:0  rw  config register	*/
#define IO4_CONF_INTRVECTOR	0x03	/* 15:0  rw  dest-id/intr-no	*/
#define IO4_CONF_GFXCOMMAND	0x04	/* 7:0   rw  gfx command	*/
#define IO4_CONF_IODEV0		0x05	/* 31:0  r   io device config	*/
#define IO4_CONF_IODEV1		0x06	/* 47:32 r   	ditto		*/
#define IO4_CONF_IBUSERROR	0x07	/* 31:0  r   read error		*/
#define IO4_CONF_IBUSERRORCLR	0x08	/* 31:0  r   read error & clear	*/
#define IO4_CONF_EBUSERROR	0x09	/* 31:0  r   read error		*/
#define IO4_CONF_EBUSERRORCLR	0x0A	/* 31:0  r   read error & clear	*/
#define IO4_CONF_EBUSERROR1	0x0B	/* 31:0  r   supplemental error	*/
#define IO4_CONF_EBUSERROR2	0x0C	/* 63:32 r   supplemental error	*/
#define IO4_CONF_RESET		0x0D	/* 0:0   rw  hard reset board 	*/
#define IO4_CONF_ENDIAN		0x0E	/* 0:0   rw  reset to 0 little	*/
#define IO4_CONF_ETIMEOUT	0x0F	/* 31:0  rw  EBUS timeout value	*/
#define IO4_CONF_RTIMEOUT	0x10	/* 31:0  rw  resrc timeout val	*/
#define IO4_CONF_INTRMASK	0x11	/* 31:0  rw  EBUS timeout value	*/
#define IO4_CONF_CACHETAG0L	0x12	/* 31:0  r   cache tag		*/
#define IO4_CONF_CACHETAG0U	0x13	/*  1:0  r   cache tag		*/
#define IO4_CONF_CACHETAG1L	0x14	/* 31:0  r   cache tag		*/
#define IO4_CONF_CACHETAG1U	0x15	/*  1:0  r   cache tag		*/
#define IO4_CONF_CACHETAG2L	0x16	/* 31:0  r   cache tag		*/
#define IO4_CONF_CACHETAG2U	0x17	/*  1:0  r   cache tag		*/
#define IO4_CONF_CACHETAG3L	0x18	/* 31:0  r   cache tag		*/
#define IO4_CONF_CACHETAG3U	0x19	/*  1:0  r   cache tag		*/

#if _LANGUAGE_C

typedef struct iBusError {
	unsigned int	unused		:13,
			ioa		:3,
			command		:1,
			pioReadRespCmnd	:1,
			dmaWriteCommand	:1,
			dmaWriteData	:1,
			pioReadRespData	:1,
			pioWriteCommand	:1,
			pioWriteData	:1,
			pioReadCommand	:1,
			gfxWriteCommand	:1,
			dmaReadRespCommand :1,
			iaRespData	:1,
			oneLevelAddr	:1,
			oneLevelData	:1,
			twoLevelAddr	:1,
			firstLevelData	:1,
			sticky		:1;
} iBusError_t;

#endif /* _LANGUAGE_C */

#ifdef _KERNEL
#ifdef _LANGUAGE_C
/* prototypes */
extern void	iamap_init(iamap_t*,ulong,ulong,iopaddr_t,iopaddr_t);
extern void	iamap_map(iamap_t*,dmamap_t*);
extern ulong	io4_virtopfn(void *);
#include <sys/hwgraph.h>
extern graph_error_t	io4_hwgraph_lookup(uint, vertex_hdl_t *);
extern void	*iospace_alloc(long, ulong);
extern uint	ev_kvtophyspnum(caddr_t);
extern int	io4hip_init(unchar, unchar, unchar, int);
struct piomap;
extern void	pio_mapfree_ibus(struct piomap *);
extern int	pio_mapfix_ibus(struct piomap *);
extern int	small_window(caddr_t, int *, int *, int *);
extern int	large_window(caddr_t, int *, int *, int *);
#ifndef _STANDALONE
extern void	dang_init(unchar, unchar, unchar);
#endif
#endif	/* _LANGUAGE_C */
#endif	/* KERNEL */

#endif
