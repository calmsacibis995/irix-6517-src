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
 * gda.h -- Contains the data structure for the global data area,
 * 	The GDA contains information communicated between the
 *	PROM, SYMMON, and the kernel. 
 */

#ifndef __SYS_EVEREST_GDA_H__
#define __SYS_EVEREST_GDA_H__

#include <sys/EVEREST/evaddrmacros.h>

#define GDA_MAGIC	0x58464552

#define G_MAGICOFF	0
#define G_PROMOPOFF	4
#define G_NMIVECOFF	8
#define G_MASTEROFF	16
#define G_HKDNORMOFF	24
#define G_HKDUTLBOFF	32
#define G_VDSOFF	40
#define	G_EPC		0x40

#ifdef _LANGUAGE_C

/*
 * Since the gda structure is used by the PROM as well as Unix, it's
 * important that locations don't change as we throw the switch from
 * 32-bit to 64-bit kernels.  Elements of both versions of these structures
 * are at identical offsets form the start.
 *
 */
#if (_MIPS_SZPTR == 64)

typedef struct gda {
	uint	g_magic;	/* GDA magic number */
	uint	g_promop;	/* Passes requests from the kernel to prom */
	uint 	g_nmivec;	/* The address to jump to on an NMI */
	uint	reserved1;	/* Reserved for 64-bit address expansion */
	uint	g_masterspnum;	/* The SPNUM (not vpid) of the master cpu */
	uint	reserved2;	/* Pad to a doubleword boundary */
	inst_t	**g_hooked_norm;/* ptr to pda loc for norm hndlr */
	inst_t	**g_hooked_utlb;/* ptr to pda loc for utlb hndlr */
	uint	g_vds;		/* Store the virtual dipswitches here */
	uint	reserved5;	/* Pad to a doubleword boundary */
	inst_t  **g_hooked_xtlb;/* ptr to pda loc for xtlb hndlr */
	uint	everror_vers;	/* This contains the everror structure version
				 * number after everror's been set up. */
	uint	everror_valid;	/* Place a specific pattern here if 
				 * everror structure is valid */
	void	*g_epc;		/* Ptr to EPC - for Prom on re-entry */

} gda_t;

#elif (_MIPS_SZPTR == 32)

typedef struct gda {
	uint	g_magic;	/* GDA magic number */
	uint	g_promop;	/* Passes requests from the kernel to prom */
	uint 	g_nmivec;	/* The address to jump to on an NMI */
	uint	reserved1;	/* Reserved for 64-bit address expansion */
	uint	g_masterspnum;	/* The SPNUM (not vpid) of the master cpu */
	uint	reserved2;	/* Pad to a doubleword boundary */
	inst_t	**g_hooked_norm;/* ptr to pda loc for norm hndlr */
	uint	reserved3;	/* Reserved for 64-bit address expansion */
	inst_t	**g_hooked_utlb;/* ptr to pda loc for utlb hndlr */
	uint	reserved4;	/* Reserved for 64-bit address expansion */
	uint	g_vds;		/* Store the virtual dipswitches here */
	uint	reserved5;	/* Pad to a doubleword boundary */
	inst_t  **g_hooked_xtlb;/* ptr to pda loc for xtlb hndlr */
	uint	reserved6;	/* Reserved for 64-bit address expansion*/
	uint	everror_vers;	/* This contains the everror structure version
				 * number after everror's been set up. */
	uint	everror_valid;	/* Place a specific pattern here if 
				 * everror structure is valid */
	void	*epc;		/* Ptr to EPC - for Prom on re-entry */
	uint	reserved7;
} gda_t;

#endif /* _MIPS_SZPTR */

#define GDA ((gda_t*) GDA_ADDR)

#endif /* __LANGUAGE_C */


/*
 * The following requests can be sent to the PROM during startup.
 */

#define PROMOP_HALT             1
#define PROMOP_POWERDOWN        2
#define PROMOP_RESTART          3
#define PROMOP_REBOOT           4
#define PROMOP_IMODE            5

#endif /* __SYS_EVEREST_GDA_H__ */
