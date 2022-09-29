#ifndef	__SYS_VMEREG_H__
#define	__SYS_VMEREG_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1999, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 3.33 $"

/*
 * VME Machine Independent (e.g. Bus specific stuff)
 */


/* Address Modifiers */
#define	VME_A16NPAMOD	0x29		/* a16 non-privileged addr modifier */
#define	VME_A16SAMOD	0x2d		/* a16 supervisor addr modifier */
#define	VME_A24NPAMOD	0x39		/* a24 non-privileged addr modifier */
#define	VME_A24SAMOD	0x3d		/* a24 supervisor addr modifier */
#define	VME_A32NPAMOD	0x09		/* a32 non-privileged addr modifier */
#define	VME_A32NPBLOCK	0x0b		/* a32 non-privileged block transfer */
#define	VME_A32SAMOD	0x0d		/* a32 supervisor addr modifier */

/* block mode Address Modifiers */
#define VME_A24NPBAMOD	0x3b		/* a24 np block mode AM */
#define VME_A24SBAMOD	0x3f		/* a24 sup block mode AM */
#define VME_A32NPBAMOD	0x0b		/* a32 np block mode AM */
#define VME_A32SBAMOD	0x0f		/* a32 sup block mode AM */

/* VME I/O address table */
struct vme_ioaddr {
	unsigned long	v_base;		/* virtual base */
	unsigned long	v_phys;		/* physical base */
	unsigned long	v_length;	/* in bytes */
};
extern struct vme_ioaddr vme_ioaddr[][6];

/* indexes into VME address table */
#define VME_A16NP	0
#define VME_A16S	1
#define VME_A24NP	2
#define VME_A24S	3
#define VME_A32NP	4
#define VME_A32S	5

#define MAX_VME_INTRS 255		/* cannot have more than this many */
#define MAX_VME_VECTS (MAX_VME_INTRS+1)	/* cannot have more than this many */
#define VME_VEC(a, v) (a * MAX_VME_VECTS + v)

typedef struct vme_intrs {
	int	(*v_vintr)();		/* interrupt routine */
	unsigned char	v_vec;		/* vme vector */
	unsigned char	v_brl;		/* interrupt priority level */
	unsigned char	v_unit;		/* software identifier */
} vme_intrs_t;

#ifdef _KERNEL

typedef struct irix5_vme_intrs {
	app32_ptr_t	v_vintr;
	unsigned char	v_vec;
	unsigned char	v_brl;
	unsigned char	v_unit;
} irix5_vme_intrs_t;

#endif /* _KERNEL */

/*
 * vme interrupt vectors
 */
struct thd_int_s;

typedef struct vme_ivec {
	int	  (*vm_intr)(int);	/* interrupt handler */
	int	  vm_arg;		/* argument for interrupt handler */
	int	  vm_ivec_ioflush;	/* Flush IO caches 	*/
#if defined(EVEREST) && defined(ULI)
        struct thd_int_s *vm_tinfo;     /* ithread/xthread info */
        struct hw_info_s {
	  int inum;
	  unchar adapter;
	} hw_info;
#endif /* EVEREST && ULI */
}vme_ivec_t;

#if defined(EVEREST) && defined(ULI)
/*
 * execution mode for VME device interrupt handlers
 */
#define EXEC_MODE_ISTK 0   /* execute on the interrupt stack */
#define EXEC_MODE_THRD 1   /* execute as an ithread */
#endif /* defined(EVEREST) && defined(ULI) */

/*
 * VME address conversion macros
 */

#ifdef _STANDALONE
/*
 * VME address conversion macros
 */
#define VMESA16_TO_PHYS(x)	((unsigned)(x)+VME_A16SBASE)
#define VMENPA16_TO_PHYS(x)	((unsigned)(x)+VME_A16NPBASE)
#define VMESA24_TO_PHYS(x)	((unsigned)(x)+VME_A24SBASE)
#define VMENPA24_TO_PHYS(x)	((unsigned)(x)+VME_A24NPBASE)

#define VMESA32_TO_PHYS(x)	((unsigned)(x)+VME_A32SBASE)
#define VMENPA32_TO_PHYS(x)	((unsigned)(x)+VME_A32NPBASE)
#define VME2NPA32_TO_PHYS(x)	((unsigned)(x)+VME_2A32NPBASE)

/* for io3 E space */
#define VME1SA24_TO_PHYS(x)	((unsigned)(x)+VME1_A24SBASE)
#define VME1NPA24_TO_PHYS(x)	((unsigned)(x)+VME1_A24NPBASE)

#else /* kernel */

#define	VMENPA16_TO_PHYS(x)	(vme_to_phys((x), VME_A16NP))
#define	VMESA16_TO_PHYS(x)	(vme_to_phys((x), VME_A16S))
#define	VMENPA24_TO_PHYS(x)	(vme_to_phys((x), VME_A24NP))
#define	VMESA24_TO_PHYS(x)	(vme_to_phys((x), VME_A24S))
#define	VMENPA32_TO_PHYS(x)	(vme_to_phys((x), VME_A32NP))
#define	VMESA32_TO_PHYS(x)	(vme_to_phys((x), VME_A32S))

/* misnamed macro */
#define vme_to_phys(addr, type) (vme_ioaddr[(vme_adapter((void *)addr))][(type)].v_base + (addr))

#if EVEREST
#define IS_VME_A16NP(x)	(is_vme_space((x), VME_A16NP))
#define IS_VME_A16S(x)	(is_vme_space((x), VME_A16S))
#define IS_VME_A24NP(x)	(is_vme_space((x), VME_A24NP))
#define IS_VME_A24S(x)	(is_vme_space((x), VME_A24S))
#define IS_VME_A32NP(x)	(is_vme_space((x), VME_A32NP))
#define IS_VME_A32S(x)	(is_vme_space((x), VME_A32S))
#else /* !EVEREST */
#define IS_VME_A16NP(x) (0)
#define IS_VME_A16S(x) (0)
#define IS_VME_A24NP(x) (0)
#define IS_VME_A24S(x) (0)
#define IS_VME_A32NP(x) (0)
#define IS_VME_A32S(x) (0)
#endif /* EVEREST */

#endif /* _STANDALONE */

struct edt;

#if _KERNEL
extern int	vme_init_adap (struct edt *);
extern int	vme_adapter(void *);

#if EVEREST
extern int	is_vme_space(void *, int);
extern ulong	vme_virtopfn(void *);
#else /* !EVEREST */
#define  vme_virtopfn(addr) (0)
#endif /* EVEREST */

extern int	vme_ivec_set(int, int, int (*)(int), int);
#ifdef ULI
extern int	vmeuli_ivec_reset(int, int, int);
#endif
extern int	vme_ivec_alloc(uint_t);
struct eframe_s;
#if defined(EVEREST) && defined(ULI)
extern void	vme_handler(struct eframe_s *, unsigned char,
			    int, __psunsigned_t, unsigned char);
#else
extern void	vme_handler(struct eframe_s *, unsigned char,
			    int, unsigned char);
#endif /* defined(EVEREST) && defined(ULI) */
extern void	vme_ivec_free(int, int);
extern void	vme_yield(unsigned int, unsigned int);

#include <sys/hwgraph.h>

extern	graph_error_t	vme_hwgraph_lookup(uint, vertex_hdl_t *);

#ifdef	EVEREST
extern int 	vme_ivec_ioflush_disable(int, int);
#endif	/* EVEREST */


#if EVEREST
extern int vmecc_xtoiadap(int);
extern int vmecc_itoxadap(int);
#define VMEADAP_XTOI(x)		(vmecc_xtoiadap(x))
#define VMEADAP_ITOX(i)		(vmecc_itoxadap(i))
#else
#define VMEADAP_XTOI(x)		(x)
#define VMEADAP_ITOX(x)		(x)
#endif /* !EVEREST */

#if IP21
#define VME_DBE_NOTFND		-1
#define VME_DBE_OWNER		0
#define VME_DBE_IGNORE		1
extern int uvme_async_read_error(int);
#endif /* IP21 */
#endif /* _KERNEL */

#endif /* !__SYS_VMEREG_H__ */

