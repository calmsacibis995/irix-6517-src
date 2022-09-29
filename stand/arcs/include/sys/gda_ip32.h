/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1998, Silicon Graphics, Inc.               *
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

#ident "$Revision: 1.1 $"

#ifndef __SYS_GDA_IP32_H__
#define __SYS_GDA_IP32_H__

#define	GDA_ADDR		(K0_RAMBASE+0x400)

/**************************************/

#define GDA_MAGIC	0x58464552

#define G_MAGICOFF	0x00	/* g_magic */
#define G_PROMOPOFF	0x04	/* g_promop */
#define G_NMISRSAVE	0x04	/* g_nmi_sr_save */
#define G_NMIVECOFF	0x08	/* g_nmivec */
#define G_NMIEPCSAVE	0x0c	/* g_nmi_epc_save */
#define G_MASTEROFF	0x10	/* g_masterspnum */
#define G_HKDNORMOFF	0x18	/* g_hooked_norm */
#define G_HKDUTLBOFF	0x20	/* g_hooked_utlb */
#define G_HKDXUTLBOFF	0x28	/* g_hooked_xtlb */
#define G_VDSOFF	0x30	/* g_vds */

#ifdef _LANGUAGE_C

typedef struct gda {
	uint	g_magic;	/* GDA magic number */
	uint	g_promop;	/* Passes requests from the kernel to prom */
	uint	g_nmivec;	/* The address to jump to on an NMI */
	uint	g_nmi_epc_save;	/* The saved epc at NMI */
	uint	g_masterspnum;	/* The SPNUM (not vpid) of the master cpu */
	uint	g_fill0;	/* 0x14 */
	inst_t	**g_hooked_norm;/* ptr to pda loc for norm hndlr */
	uint	_fill1;		/* 0x1c */
	inst_t	**g_hooked_utlb;/* ptr to pda loc for utlb hndlr */
	uint	_fill2;		/* 0x24 */
	inst_t  **g_hooked_xtlb;/* ptr to pda loc for xtlb hndlr */
	uint	_fill3;		/* 0x2c */
	ulong	g_vds;		/* Store the virtual dipswitches here */
} gda_t;

#define g_nmi_sr_save	g_promop

#define GDA ((gda_t *)GDA_ADDR)

#endif /* __LANGUAGE_C */


/*
 * The following requests can be sent to the PROM during startup.
 */

#define PROMOP_HALT             1
#define PROMOP_POWERDOWN        2
#define PROMOP_RESTART          3
#define PROMOP_REBOOT           4
#define PROMOP_IMODE            5

#endif /* __SYS_GDA_IP32_H__ */
