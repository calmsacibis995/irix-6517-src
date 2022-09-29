/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * gda.h -- For IP28 and IP30 contains the NMI vector hook.  For IP30
 *	    symmon information (hook values) are stored here.
 */

#ident "$Revision: 1.12 $"

#ifndef __SYS_RACER_GDA_H__
#define __SYS_RACER_GDA_H__

#ifdef IP28
#define	GDA_ADDR		(K1_RAMBASE+0x400)
#else
#define	GDA_ADDR		(K0BASE+0x400)
#endif

/**************************************/

#define GDA_MAGIC	0x58464552

#define G_MAGICOFF	0x00	/* g_magic */
#define G_PROMOPOFF	0x04	/* g_promop */
#define G_NMIVECOFF	0x08	/* g_nmivec */
#define G_MASTEROFF	0x10	/* g_masterspnum */
#define G_HKDNORMOFF	0x18	/* g_hooked_norm */
#define G_HKDUTLBOFF	0x20	/* g_hooked_utlb */
#define G_HKDXUTLBOFF	0x28	/* g_hooked_xtlb */
#define G_COUNT		0x30	/* g_count */

#if IP28	/* First 3 members are baked into IP28 prom */
#if (G_MAGICOFF != 0x00) || (G_PROMOPOFF != 0x04) || (G_NMIVECOFF != 0x08) || \
    (GDA_MAGIC != 0x58464552)
#error "IP28 GDA has changed!"
#endif
#endif

#ifdef _LANGUAGE_C

typedef struct gda {
	uint	g_magic;	/* GDA magic number */
	uint	g_promop;	/* Passes requests from the kernel to prom */
	ulong	g_nmivec;	/* The address to jump to on an NMI */
	ulong	g_masterspnum;	/* The SPNUM (not vpid) of the master cpu */
	inst_t	**g_hooked_norm;/* ptr to pda loc for norm hndlr */
	inst_t	**g_hooked_utlb;/* ptr to pda loc for utlb hndlr */
	inst_t  **g_hooked_xtlb;/* ptr to pda loc for xtlb hndlr */
#ifndef IP28
	ulong	g_count;	/* count of NMIs since reset */
#endif
} gda_t;

#define GDA ((gda_t *)GDA_ADDR)

#endif /* __LANGUAGE_C */

#endif /* __SYS_RACER_GDA_H__ */
