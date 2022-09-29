/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1996-1998, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN1_SNACMD_H__
#define __SYS_SN_SN1_SNACMD_H__

#include <sys/SN/SN1/snacmdregs.h>

#define 	MD_MEM_BANKS	4	/* 4 banks of memory max on SNaC */


#if defined (PSEUDO_SN1)

/*
 * Most of these are blindly copied from SN0. Guaranteed to be WRONG.
 */


#define MD_PAGE_SIZE		4096	 /* Page size in bytes		    */

/* Directory entry states for both premium and standard SIMMs. */

#define MD_DIR_SHARED		(UINT64_CAST 0x0)	/* 000 */
#define MD_DIR_POISONED		(UINT64_CAST 0x1)	/* 001 */
#define MD_DIR_EXCLUSIVE	(UINT64_CAST 0x2)	/* 010 */
#define MD_DIR_BUSY_SHARED	(UINT64_CAST 0x3)	/* 011 */
#define MD_DIR_BUSY_EXCL	(UINT64_CAST 0x4)	/* 100 */
#define MD_DIR_WAIT		(UINT64_CAST 0x5)	/* 101 */
#define MD_DIR_UNOWNED		(UINT64_CAST 0x7)	/* 111 */

/* Protection and migration field values */

#define MD_PROT_RW		(UINT64_CAST 0x6)
#define MD_PROT_RO		(UINT64_CAST 0x3)
#define MD_PROT_NO		(UINT64_CAST 0x0)
#define MD_PROT_BAD		(UINT64_CAST 0x5)

#define MMC_FPROM_CYC_SHFT	49	/* Have to use UINT64_CAST, instead */
#define MMC_FPROM_CYC_MASK	(UINT64_CAST 31 << 49)	/* of 'L' suffix,   */
#define MMC_FPROM_WR_SHFT	44			/* for assembler    */
#define MMC_FPROM_WR_MASK	(UINT64_CAST 31 << 44)
#define MMC_UCTLR_CYC_SHFT	39
#define MMC_UCTLR_CYC_MASK	(UINT64_CAST 31 << 39)
#define MMC_UCTLR_WR_SHFT	34
#define MMC_UCTLR_WR_MASK	(UINT64_CAST 31 << 34)
#define MMC_DIMM0_SEL_SHFT	29
#define MMC_DIMM0_SEL_MASK	(UINT64_CAST 3 << 29)
#define MMC_IO_PROT_EN_SHFT	27
#define MMC_IO_PROT_EN_MASK	(UINT64_CAST 1 << 27)
#define MMC_IO_PROT		(UINT64_CAST 1 << 27)
#define MMC_IO_PROT_IGN_SHFT	26
#define MMC_IO_PROT_IGN_MASK	(UINT64_CAST 1 << 26)
#define MMC_CPU_PROT_IGN_SHFT	25
#define MMC_CPU_PROT_IGN_MASK	(UINT64_CAST 1 << 25)
#define MMC_ARB_MLSS_SHFT	25
#define MMC_ARB_MLSS_MASK	(UINT64_CAST 1 << 25)
#define MMC_ARB_MLSS		(UINT64_CAST 1 << 25)
#define MMC_IGNORE_ECC_SHFT	23
#define MMC_IGNORE_ECC_MASK	(UINT64_CAST 1 << 23)
#define MMC_IGNORE_ECC		(UINT64_CAST 1 << 23)
#define MMC_DIR_PREMIUM_SHFT	22
#define MMC_DIR_PREMIUM_MASK	(UINT64_CAST 1 << 22)
#define MMC_DIR_PREMIUM		(UINT64_CAST 1 << 22)
#define MMC_REPLY_GUAR_SHFT	21
#define MMC_REPLY_GUAR_MASK	(UINT64_CAST 15 << 21)
#define MMC_WRITE_GUAR_SHFT	17
#define MMC_WRITE_GUAR_MASK	(UINT64_CAST 15 << 17)
#define MMC_INTERLEAVE_SHFT	13
#define MMC_INTERLEAVE_MASK	(UINT64_CAST 3 << 13)

#define MMC_BANK_SHFT(_b)	((_b) * 3)
#define MMC_BANK_MASK(_b)	(UINT64_CAST 7 << MMC_BANK_SHFT(_b))
#define MMC_BANK_ALL_MASK	0xfff
#define MMC_RESET_DEFAULTS	(UINT64_CAST 0x0f << MMC_FPROM_CYC_SHFT | \
				 UINT64_CAST 0x07 << MMC_FPROM_WR_SHFT | \
				 UINT64_CAST 0x1f << MMC_UCTLR_CYC_SHFT | \
				 UINT64_CAST 0x0f << MMC_UCTLR_WR_SHFT | \
				 MMC_IGNORE_ECC | MMC_DIR_PREMIUM | \
				 UINT64_CAST 0x0f << MMC_REPLY_GUAR_SHFT | \
				 MMC_BANK_ALL_MASK)
/* Premium SIMM protection entry shifts and masks. */

#define MD_PPROT_SHFT		0			/* Prot. field 	    */
#define MD_PPROT_MASK		7
#define MD_PPROT_MIGMD_SHFT	3			/* Migration mode   */
#define MD_PPROT_MIGMD_MASK	(3 << 3)
#define MD_PPROT_REFCNT_SHFT	5			/* Reference count  */
#define MD_PPROT_REFCNT_WIDTH	0x7ffff
#define MD_PPROT_REFCNT_MASK	(MD_PPROT_REFCNT_WIDTH << 5)

#define MD_PPROT_IO_SHFT	45			/* I/O Prot field   */
#define MD_PPROT_IO_MASK	(UINT64_CAST 7 << 45)

/* Standard SIMM protection entry shifts and masks. */

#define MD_SPROT_SHFT		0			/* Prot. field 	    */
#define MD_SPROT_MASK		7
#define MD_SPROT_MIGMD_SHFT	3			/* Migration mode   */
#define MD_SPROT_MIGMD_MASK	(3 << 3)
#define MD_SPROT_REFCNT_SHFT	5			/* Reference count  */
#define MD_SPROT_REFCNT_WIDTH	0x7ff
#define MD_SPROT_REFCNT_MASK	(MD_SPROT_REFCNT_WIDTH << 5)

#endif /* PSEUDO_SN1 */
#endif  /* __SYS_SN_SN1_SNACMD_H__ */
