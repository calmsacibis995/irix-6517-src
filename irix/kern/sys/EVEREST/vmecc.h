/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef _VMECC_H_
#define _VMECC_H_

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/EVEREST/fchip.h>

/*
 * VMECC Internal Register Map
 */


/*
 * RMW Group
 */

#define VMECC_RMWMASK		0x00001080	/* 31:0 R/W and		*/
#define VMECC_RMWSET		0x00001088	/* 31:0 R/W or		*/
#define VMECC_RMWADDR		0x00001090	/* 31:0 R/W address	*/
#define VMECC_RMWAM		0x00001098	/*  5:0 R/W Addr Modfr	*/
#define VMECC_RMWTRIG		0x00002000	/*  var R   RMW trigger	*/


/*
 * Error Group
 */

#define VMECC_ERRADDREBUS	0x00001004
#define VMECC_ERRADDRVME	0x00001018
#define VMECC_ERRXTRAVME	0x00001280

#define VMECC_ERRADDREBUS_WR	0x00000002
#define VMECC_ERRADDREBUS_AV	0x00000001

#define VMECC_ERRXTRAVME_AM	0x0000003F
#define VMECC_ERRXTRAVME_IACK	0x00000040
#define VMECC_ERRXTRAVME_WR	0x00000080
#define VMECC_ERRXTRAVME_LWORD	0x00000100
#define VMECC_ERRXTRAVME_DS0	0x00000200
#define VMECC_ERRXTRAVME_DS1	0x00000400
#define VMECC_ERRXTRAVME_AS	0x00000800

#define VMECC_ERRXTRAVME_DATA_MASK	0x00000f00
#define VMECC_ERRXTRAVME_DATA_SHIFT	8
#define VMECC_ERRXTRAVME_AMSUPER	0x00000004

#define VMECC_ERRXTRAVME_GLVL_MASK	0x00007000
#define VMECC_ERRXTRAVME_GLVL_SHIFT	12
#define VMECC_ERRXTRAVME_GLVL(e)	((e & 0x00007000) >> 12)

#define VMECC_ERRXTRAVME_AVALID	0x00008000
#define VMECC_ERRXTRAVME_XVALID	0x00010000

#define VMECC_ERRORCAUSES	0x00001218
#define VMECC_ERRCAUSECLR	0x00001238


/*
 * DMA Engine Group. For PIO operation without tying up the cpu.
 */

#define VMECC_DMAVADDR		0x00001100
#define VMECC_DMAEADDR		0x00001108
#define VMECC_DMABCNT		0x00001110
#define VMECC_DMAPARMS		0x00001118


/*
 * The Configuration Group.
 */

#define VMECC_CONFIG		0x00001000
#define VMECC_A64SLVMATCH	0x00001008
#define VMECC_A64MASTER		0x00001010

#define VMECC_CONFIG_RESET	0x00000001
#define VMECC_CONFIG_PIORWD	0x00000002
#define VMECC_CONFIG_DMAENGL2	0x00000004
#define VMECC_CONFIG_FCIID_SH	0x3
#define	VMECC_CONFIG_SUPERVISOR	0x20	/* vmecc respond to supervisory mode. */
#define VMECC_CONFIG_A24ENABLE	0x00003000	/* block 0 and 1 */
#define VMECC_CONFIG_A32ENABLE	0x7F000000	/* block 08 - 0E */

#define VMECC_CONFIG_VALUE	0x7F003063


/*
 * Interrupt Group.
 */

	/* The (vector+dest) map. */

#define VMECC_VECTORERROR	0x00001300
#define VMECC_VECTORIRQ1	0x00001308
#define VMECC_VECTORIRQ2	0x00001310
#define VMECC_VECTORIRQ3	0x00001318
#define VMECC_VECTORIRQ4	0x00001320
#define VMECC_VECTORIRQ5	0x00001328
#define VMECC_VECTORIRQ6	0x00001330
#define VMECC_VECTORIRQ7	0x00001338
#define VMECC_VECTORDMAENG	0x00001340
#define VMECC_VECTORAUX0	0x00001348
#define VMECC_VECTORAUX1	0x00001350


#define VMECC_IACK(i)		(0x00001280+(i<<3))

#define VMECC_IACK1		0x00001288
#define VMECC_IACK2		0x00001290
#define VMECC_IACK3		0x00001298
#define VMECC_IACK4		0x000012A0
#define VMECC_IACK5		0x000012A8
#define VMECC_IACK6		0x000012B0
#define VMECC_IACK7		0x000012B8

#define VMECC_INT_ENABLE	0x00001200
#define VMECC_INT_REQUESTSM	0x00001208
#define VMECC_INT_ENABLESET	0x00001240
#define VMECC_INT_ENABLECLR	0x00001220

#define VMECC_INUMERROR		0
#define VMECC_INUMIRQ1		1
#define VMECC_INUMIRQ2		2
#define VMECC_INUMIRQ3		3
#define VMECC_INUMIRQ4		4
#define VMECC_INUMIRQ5		5
#define VMECC_INUMIRQ6		6
#define VMECC_INUMIRQ7		7
#define	VMECC_INUMOTHERS	8
#define VMECC_INUMDMAENG	8
#define VMECC_INUMAUX0		9
#define VMECC_INUMAUX1		10

#define VMECC_PIOTIMER		0x00001180
#define VMECC_PIOTIMER_VALUE	0x00000000

#define VMECC_PIOREG(i)		(0x1380+(i<<3))

#define VMECC_LOOPBACK		0x0000		/* still exist? */


/*
 * VMECC PIO Large Window PIO Mapping
 */

#define VMECC_PIOREG_MAX		16
#define VMECC_PIOREG_A24S		15
#define VMECC_PIOREG_A24N		14
#define VMECC_PIOREG_MAXPOOL		13	/* max for pool usages */
#define VMECC_PIOREG_MAXFIX		12	/* last possible fixed reg */
#define VMECC_LW_SIZE			0x08000000		/* full 128MB */
#define VMECC_LW_MAPSIZE		0x00800000		/* 8MB */
#define VMECC_PIOREG_MAPSIZE		0x00800000		/* 8MB each */
#define VMECC_PIOREG_MAPSIZEMASK	(VMECC_PIOREG_MAPSIZE-1)
#define VMECC_PIOREG_MAPSIZESHIFT	23
#define VMECC_PIOREG_MAPADDR(i)		(i<<VMECC_PIOREG_MAPSIZESHIFT)
#define VMECC_PIOREG_MAPADDRMASK	0xFF800000

#define LWIN_PFN_OFF(reg)		((VMECC_LW_MAPSIZE * (reg))>>IO_PNUMSHFT)

/*
typedef struct vmeccPioMapReg {
	unsigned int	unused:16,
			am:6,
			a64:1,
			extend:9;
} vmeccPioReg_t;
*/

#define VMECC_PIOREG_AMSHIFT		10	/* left */
#define VMECC_PIOREG_A64SHIFT		9	/* left */
#define VMECC_PIOREG_ADDRSHIFT		23	/* right */

/*
 * The following functions are not exported and called from the interface
 * functions (see below) only. Each piomap may own just one map reg at a time,
 * pio_maplock is called at the beginning of the interface function to reserve
 * one mapping reg for the operation. pio_map will update the mapping register.
 * pio_mapunlock is called at the end of the interface function to change the
 * map reg state from used to alloc'ed. This allows us to use one map reg for
 * the whole piomap but still able to map much larger io space.
 *
 * pio_maplock	- alloc and lock a map reg for the piomap
 * pio_mapunlock- unlock the map reg so that it may be reallocated.
 * pio_map	- update the reserved map regsiter to do the mapping.
 *
 * Pio_mapfix is used solely to map the fixed piomaps.
 * It calls the map type dependent routine to discover possible overlaps.
 *
 * pio_mapfix	- map fixed/unlikely to go away maps.
 */
extern void	pio_maplock(piomap_t*);
extern void	pio_mapunlock(piomap_t*);
extern ulong	pio_map(piomap_t*, iopaddr_t, int);
extern int	pio_mapfix(piomap_t*);
extern int	pio_mapfix_vme(piomap_t*);
extern int 	vmecc_xtoiadap(int);
extern int 	vmecc_itoxadap(int);
extern void	vmecc_intr(eframe_t *, void *);
extern int	vmecc_error_intr(eframe_t *, void *);
extern int	vmecc_rd_error(int *, int *);
extern int	vmecc_io4atova(int);
extern ulong	pio_map_vmecc(int, int, iopaddr_t, ulong, int, int);
extern ulong	pio_map_vme(piomap_t *, iopaddr_t, int);
extern void	pio_mapfree_vme(piomap_t *);
extern void	vmecc_init(unchar, unchar, unchar, unchar, ulong);
extern void	vmecc_lateinit(void);
extern void	vmecc_bus_invent_add(void);
extern void	pio_maplock_vme(piomap_t *);
extern void	pio_mapunlock_vme(piomap_t *);
extern void	vmecc_preinit(void);
extern int	uvme_errclr(eframe_t *);

extern int	vme_address(caddr_t, int *, ulong *, ulong *);

/*
 * pioreg_t - A generic struct to specify the mapping register for pio.
 *
 * The struct could be in the adaptor (like VMECC) dependent struct. The kvaddr
 * mapping to program each mapping reg is fixed from  boot time.
 * The "from" kvaddr for the map is also fixed and not changed thru the life
 * of the system.
 *
 */

typedef struct pioreg {
	unchar		flag;		/* everybody needs a flag */
	unchar		maptype;	/* the map space type */
	unchar		refcnt;		/* number of piomaps using this reg */
	iopaddr_t	iopaddr;	/* the part of the io space mapped */
	void *		kvaddr;		/* k2seg address */
} pioreg_t;

#define PIOREG_FREE	0		/* not used */
#define PIOREG_INUSE	1		/* Currently in use but not fixed */
#define PIOREG_FIXED	2		/* semi-permanent */

/*
 * pioregpool manages pio mapping registers.
 *
 * The spin lock is the only protection since both alloc and free s/b fast.
 * Nregs is the number of mapping registers in this pool, each reg has its own
 * struct as shown above.
 * The state of the registers are,
 *	fixed	- those alloc'ed for traditional pio.
 *	imuse	- those being used (thru interface functions).
 *	free	- not alloc'ed.
 *
 */

typedef struct pioregpool {
	lock_t		lock;		/* a spin lock to protect this pool */
	unchar		freefix;	/* number of free fixed regs */
	pioreg_t	pioregs[VMECC_PIOREG_MAX];
} pioregpool_t;

extern int	pioregpool_alloc(piomap_t*, int);
extern void	pioregpool_free(piomap_t*);

typedef struct vmecc {
	ioadap_t	ioadap;
	__psunsigned_t	a16kv;
	__psunsigned_t	mapram;
	iamap_t		a32map;
	iamap_t		a24map;
	lock_t		rmwlock;
	lock_t		dmalock;
	pioregpool_t	pioregpool;
	int		window;
	short		irqmask;
	char		xadap;
	char		dmaeng;
} vmecc_t;

extern	vmecc_t	vmecc[];
extern	int	nvmecc;

/*
 * dma_xin	- from io to mem, returns size (in units) transfered
 * dma_xout	- from mem to io, returns size (in units) transfered
 */
/*
extern int	dma_bin  (ioaddr_t*, dmamap_t*, int);
extern int	dma_bout (dmamap_t*, ioaddr_t*, int);
extern int	dma_hin  (ioaddr_t*, dmamap_t*, int);
extern int	dma_hout (dmamap_t*, ioaddr_t*, int);
extern int	dma_win  (ioaddr_t*, dmamap_t*, int);
extern int	dma_wout (dmamap_t*, ioaddr_t*, int);
extern int	dma_din  (ioaddr_t*, dmamap_t*, int);
extern int	dma_dout (dmamap_t*, ioaddr_t*, int);
*/

typedef struct vmeam {
	int	am;
	char	*name;
} vmeam_t;
extern vmeam_t vmeam[];

#endif
