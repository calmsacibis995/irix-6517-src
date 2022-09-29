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

#ident "sys/vde_if.h: $Revision: 1.1 $"

/*
 * This file contains declarations for data structures and routines to
 * be used by driver writers and others wishing to access the
 * functionality of the VMAX hardware.
 */

typedef struct vde_xfer {
    ulong	lcl_addr;	/* Kernel virtual */
    ulong	vme_addr;	/* Base VME */
    unsigned	count;		/* Bytes to xfer */
    int		stride;		/* Size of VME xfer (1,2,4) */
    unsigned	vme_am;		/* VMEbus address modifier (see below) */
    unsigned	flags;		/* See bit defs below */
} VdeXfer;

#define	VDE_FLAG_DIREC		0x1	/* 0=Write, 1=Read */
#define	VDE_FLAG_CACHE_OK	0x2	/* 0=vde takes care of cache */
#define	VDE_FLAG_LOCK_MEM	0x4	/* Unused for now.  SET TO 0! */
#define	VDE_FLAG_ASYNC		0x8	/* Don't wait for completion */
#define VDE_FLAG_MASK		0xB	/* Since LOCK_MEM not used. */

#define VDE_READ		1	/* DIREC bit in flags entry */
#define VDE_WRITE		0	/* DIREC bit in flags entry */

/*
 * Address modifiers settable in the vde_am entry in a VdeXfer struct.
 */
#define VDE_BLOCK_A32NP		0x0B
#define VDE_BLOCK_A32SUP	0x0F
#define VDE_BLOCK_A24NP		0x3B
#define VDE_BLOCK_A24SUP	0x3F

#define VDE_PIO_A32NP		0x09
#define VDE_PIO_A32SUP		0x0D
#define VDE_PIO_A24NP		0x39
#define VDE_PIO_A24SUP		0x3D
#define VDE_PIO_A16NP		0x29
#define VDE_PIO_A16SUP		0x2D

/*
 * Externally available function prototypes.
 */
int vde_do_block_xfer		(VdeXfer *, int, int);
int vde_set_release_type	(int);
int vde_wait			(void);

/*
 * Error codes returnable by vde_do_block_xfer
 */
#define	VDE_BERR	1		/* VME bus error */
#define	VDE_VME_ALIGN	2		/* VME alignment */
#define	VDE_LCL_ALIGN	3		/* GIO alignment */
#define	VDE_VME_ADDR	4		/* Illegal VME address */
#define	VDE_LCL_ADDR	5		/* Illegal GIO address */
#define	VDE_OVERFLOW	6		/* Transfer too large */
#define	VDE_STRIDE	7		/* Bad transfer size */
#define VDE_COUNT_ALIGN	8		/* Count not multiple of stride */
#define VDE_AM		9		/* Bad VME address modifier */
#define VDE_SIGNAL	10		/* Sleep on sema awakened by signal */
#define VDE_NO_BD	11		/* No VMAX/V2G board present */
#define	VDE_XYL		12		/* Xylinx not programmed. */
#define	VDE_NOT_READY	13		/* Prev. DMA not done. */
#define	VDE_BAD_FLAGS	14		/* Bad flags entry. */
#define	VDE_OVR_XDESC	15		/* Too many transaction descriptors */

/*
 * Release type definitions.
 */
#define	VDE_RELEASE_RWD	0		/* Be a release when done master */
#define	VDE_RELEASE_ROR	1		/* Be a release on reqeust master */

/*
 * Resource limits built into driver
 */
#define VDE_NUM_DESC	500	/* Hardware transfer descriptors */
#define VDE_MAX_SIZE	0x80000 /* 512 K (easily fits in 500 VMAX desc.) */
#define VDE_MAX_XDESC	10	/* 10 transaction descriptors */

/*
 * PIO functionality.
 * NOTE: if VPI_STRUCT is not defined for the user's code, it will
 * generate undefined externals.
 */

struct vpi_vme {
    unsigned		am;
    unsigned long	vme_base;
};

struct vpi {
    int			flags;
    int			size;
    int			maxunits;
    struct vpi_vme	*vme_if;
};

#define VDE_VPI_FLAG_SETUP  0x1

/*
 * Function prototypes.
 */
extern	ulong	vde_byte_read	(unsigned am, ulong addr);
extern	ulong	vde_short_read	(unsigned am, ulong addr);
extern	ulong	vde_long_read	(unsigned am, ulong addr);

extern	void	vde_byte_write	(unsigned am, ulong addr, ulong val);
extern	void	vde_short_write	(unsigned am, ulong addr, ulong val);
extern	void	vde_long_write	(unsigned am, ulong addr, ulong val);

extern	int	vde_badaddr	(unsigned am, ulong addr, int size);
extern	int	vde_wbadaddr	(unsigned am, ulong addr, int size);

/*
 * Routines to support the macro interface
 */
extern void *
vps (struct vpi *lcl_vpi, int maxunits, int size, int unit, unsigned am, unsigned long base);

extern void
vde_internal_read (struct vpi *lcl_vpi, void *var, int size, volatile void *vref, int vsize);

extern void
vde_internal_write (struct vpi *lcl_vpi, volatile void *vref, int vsize, ulong val);

extern unsigned long
vde_internal_fetch (struct vpi *lcl_vpi, volatile void *vref, int vsize);

extern int
vde_internal_badaddr (struct vpi *lcl_vpi, void *vref, int size);

extern void
vde_internal_wbadaddr (struct vpi *lcl_vpi, void *vref, int size);

/*
 * Macro interface
 * What about signed vs unsigned?  Does it matter?
 */
#define vde_pio_setup(maxu, size, unit, am, base) \
	vps (&VPI_STRUCT, maxu, size, unit, am, base)

#define vde_pio_read(var, vme_ref)	\
	vde_internal_read (&VPI_STRUCT, &(var), sizeof (var), &(vme_ref), sizeof (vme_ref))

#define vde_pio_write(vme_ref, val) \
	vde_internal_write (&VPI_STRUCT, &(vme_ref), sizeof(vme_ref), (val))

#define vde_pio_fetch(vme_ref) \
	vde_internal_fetch (&VPI_STRUCT, &(vme_ref), sizeof(vme_ref))

#define vde_pio_badaddr(vme_ref, size) \
	vde_internal_badaddr (&VPI_STRUCT, vme_ref, size)

#define vde_pio_wbadaddr(vme_ref, size) \
	vde_internal_wbadaddr (&VPI_STRUCT, vme_ref, size)
