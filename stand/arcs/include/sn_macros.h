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

#ident "$Revision: 1.3 $"

#ifndef __SN_MACROS_H__
#define __SN_MACROS_H__

/* some useful macros for the SN PROM */

#define CHECK_GDA(g)					\
        ((((g) = (gda_t *)GDA_ADDR(get_nasid()))->g_magic) == GDA_MAGIC)

#define ForAllValidNasids(i,n,g)			\
        for ((i) = 0; (i) < MAX_COMPACT_NODES; (i)++)   \
                if (((n) = (g)->g_nasidtable[(i)]) != INVALID_NASID)

#define MAX_CPUS_PER_NODE_SN0	2

/*
 * This value should match the number of valid entries in
 * CheckEdFunc in ed.c
 */

#define MAX_ED_COMPONENTS	2

#define ForAllEdComponents(i)	for ((i)=0; (i)<MAX_ED_COMPONENTS; (i)++)

#define ForAllCpusPerSN0Nasid(i)	\
				for ((i)=0; (i)<MAX_CPUS_PER_NODE_SN0; (i)++)

#define ForAllMemBanks(i)	for ((i)=0; (i)<MD_MEM_BANKS; (i)++)

#define ForNonZeroMemBanks(i)	for ((i)=1; (i)<MD_MEM_BANKS; (i)++)

#define ForAllXIOPCIDevs(i)	for ((i)=0; (i)<MAX_PCI_DEVS; (i)++)

#endif /* __SN_MACROS_H__ */

