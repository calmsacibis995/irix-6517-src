
#ifndef __PATTERN_H__
#define __PATTERN_H__

/*
 * pattern.h
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.6 $"

/* various defines for message */

#define 	VERBOSE_ON		0x80000000
#define 	LOGGED			0x40000000
#define 	NORMAL			0x0
/*
 * width defines for memory operations
 */
#define	SW_BYTE		1
#define	SW_HALFWORD		2
#define	SW_WORD		4
#define	SW_DBLWORD		8

#define MEG		(1*1024*1024)

/* define for return value	*/

#define PASS	0
#define FAIL	1
#define SKIP	2
#define NO_CPUDONE 3

/* define master terminal enable/disable */
#define ENABLED		0xff
#define DISABLED	0x99

/* define for io3 and mp test */
#define F_SLOT		0
#define E_SLOT  	1
#define BOTH_SLOT 	2

/* defines for quick_mode increment levels */
#define QUICK_INC		48   /* normal mode inc is 1 */
#define QUICK_INC_CACHEMEM	96   /* normal mode inc is 4 */
#define QUICK_INC_BRW		720  /* normal mode inc is 1 */
#define QUICK_INC_ECC		24   /* normal mode inc is 1 */
#define QUICK_INC_MEMWLK	1920 /* normal mode inc is 1 */

/*
 *
 * Various defines for CPU LED's
 *
 * LED defines 
 */
#define		SDDATA			0x1
#define		SDDATA_MATS		0x2
#define		SDTAG			0x3
#define		SDENDIAN		0x4
#define		SDWORD			0x5
#define		SDVALID			0x6
#define		SDBYTERW		0x7
#define		SDWALLY			0x8
#define		SDSWALK			0x9
#define		IBDATA			0xa
#define		IBTAGS			0xb
#define		TIMER			0xc
#define 	TIMER_INTR		0xd
#define		TLB_RAM			0xe
#define		TLB_PROBE		0xf
#define		TLB_XLATE		0x10
#define		TLB_VALID		0x11
#define		TLB_MOD			0x12
#define		TLB_PID			0x13
#define		TLB_G			0x14
#define		TLB_N			0x15
#define		DUARTS			0x16
#define		FDCACHE			0x17
#define		ICACHE			0x18
#define		CACHE_PAR		0x19
#define		SCACHE_ECC		0x1a
#define		CACHE_HOLD		0x1b
#define		CACHE_REFRESH		0x1c

#define		MCLED			0x20
#define		REGS			0x21
#define		BUSERR			0x22
#define		CONNECT			0x23
#define		WALK_ADDRESS		0x24
#define 	ADDRU			0x25
#define 	READ_WRITE		0x26
#define		MEM_WALK 		0x27
#define		MARCH_X			0x28
#define		MARCH_Y			0x29
#define		MEM_PAT 		0x2a
#define		RDCONFIG_REG		0x2b
#define		MEM_ECC			0x2f
#define 	CACHEMEM		0x2f
/* #define		MEM_KH			0x28
#define		BYTE_KH			0x29
#define		BYTE_PAT		0x2b
#define		PE			0x2c
#define		MAP_RAM			0x2d
#define		SOUND			0x2e
#define		NVRAM			0x2f
 */

#define		ICACHE_IB		0x30
#define		ICACHE_2CA		0x31
#define		ICACHE_MEM		0x32
#define		IBFUNC			0x33
#define		DCACHE_R		0x33
#define		DCACHE_W		0x34
#define		DCACHE_BW		0x35
#define		SCACHE_R		0x36
#define		SCACHE_W		0x37
#define		RMISS			0x39
#define		MEM_R			0x3a
#define		MEM_R_INVAL		0x3b
#define		MEM_W			0x3c
#define		MEM_W_INVAL		0x3d
#define		WBACK_R			0x3e
#define		WBACK_W			0x3f
#define		CACHE_STRESS		0x40
#define		LOCK_CACHES		0x40
#define		BLOCK_D			0x41
#define		BLOCK_I			0x42
#define 	THREE_WAY		0x43
#define 	THREE_WAYB		0x44

#define 	SEMA_RAM		0x46
#define 	SBC_REGS		0x47
#define 	SYNC_INTR		0x48
#define 	SELF_PEND		0x49
#define 	SELF_INTR		0x4a
#define 	SEMA_OP			0x4b

#define 	SCSI_START		0x50
#define 	DATA_TEST		0x51
#define 	SCSIREG			0x51
#define 	DMA_W1			0x53
#define 	DMA_W2			0x54
#define 	DMA_R1			0x55
#define 	DMA_R2			0x56
#define 	VME24_START		0x57
#define 	VME32_START		0x58
#define 	SCSI_DMA		0x59
#define 	SCSI_INTR		0x46
#define 	VME_INTR		0x47
#define 	SBUS_ERR		0x48
#define 	VBUS_ERR		0x49
#define 	ECC_INTR		0x4a
#define 	V32_INTR		0x4b
#define 	LOG_RAM			0x4c
#define 	VMESELF			0x4c
#define		SMARTWATCH		0x4d
#define 	SCSISELF		0x4e

#define		FADDSUBD                0x60
#define         FADDSUBS		0x61
#define		FMULDIVD		0x62
#define		FMULDIVS		0x63
#define		FMULSUBD		0x64
#define		FMULSUBS		0x65
#define		FPREG			0x66
#define		FPMEM			0x67
#define		FINVALID		0x68
#define		FDIVZERO		0x69
#define 	FOVERFLOW		0x70
#define		FUNDERFLOW		0x71
#define		FINEXACT		0x72
#define 	FPCMPUT		  	0x73

#define		ENET			0x65
#define		ENET_REG		0x66
#define 	ENET_LPBK		0x67
#define		ENET_BERR		0x68

#define 	MPCACHE_R1		0x70
#define 	MPCACHE_R2		0x71
#define 	MPCACHE_W1		0x72
#define 	MPCACHE_W2		0x73
#define 	EXMOD_R			0x74
#define 	EXMOD_W			0x75
#define 	SD_STRESS		0x76
#define 	WB_STRESS		0x76
#define 	SD1_STRESS		0x77
#define 	SD2_STRESS		0x78
#define		SOFTSEMA		0x79
#define		HARDERSEMA		0x79
#define		HARDESTSEMA		0x70
#define		DMASEMA			0x79
#define		DEVSEMA			0x79
#define 	MPSEMA			0x7a
#define 	SM_STRESS		0x7b
#define 	SM1_STRESS		0x7c
#define 	SPINLOCKS		0x7d
#define 	MPINTR			0x7e
#define 	GROUPINTR		0x7f
#define 	OVERRUN			0x60
#define 	MPDMA_R			0x61
#define 	MPDMA_W			0x62
#define 	MPVME24			0x63
#define 	VMAP_SCSIDEV		0x64
#define 	VMAP_SCSIDMA		0x65
#define 	VDEV_SMAP		0x66
#define 	VME2432			0x67
#define 	MPVME32			0x68
#define 	VMESCSI			0x69
#define 	INTRPIO			0x6a
#define 	SVINTR			0x6b
#define 	MPVMEINTR		0x6c
#define 	SINTR_VDEV		0x6d
#define 	VDEV_VINTR		0x6e
#define 	VINTR_SDEV		0x6f
#define 	VINTRSTRESS		0x70
#define		WG_BUFFER		0x71
#define		SCSI0SCSI1		0x72

/* begin new routines for IP17 */
#define		HAMMER_PDCACHE		0x7a
#define		HAMMER_SCACHE		0x7b

#define		PD_TAG_RAMS		0x80
#define		PI_TAG_RAMS		0x81
#define		SC_TAG_RAMS		0x82
#define		DCACHE_DRAMS		0x83

#define		CACHE_INSTRS1		0x85
#define		CACHE_INSTRS2		0x86
#define		PD_CACHEOP_ALIGNMENT	0x87
#define		SD_CACHEOP_ALIGNMENT	0x88
#define		CACHE_STATES		0x89

/* IP17 stress tests */
#define		IP17STRESS		0x90
#define		PDC_STRESS		0x91
#define		SC_STRESS		0x92

#define		UCREADSTRESS		0x95
#define		CREADSTRESS		0x96
#define		UCWRITESTRESS		0x97
#define		CWRITESTRESS		0x98
#define		UCWRITEBACKSTRESS	0x99
#define		CWRITEBACKSTRESS	0x9a


#define		BUSERR_INTR		0xa3
#define		COUNTER			0xa4

#define		GPIB_DIAG		0xa5
#define		GPIB_DMA		0xa6
#define		GPIB_INTR		0xa7

#define		ENTRYPT_1		0xaa
#define		ENTRYPT_2		0xab
#define		ENTRYPT_3		0xac

#define		TLB_MAPUC		0xad
#define		RMI_ECC			0xae
#define		UM_ACCESS		0xaf

/* defines for readconfig_reg */
#define	RDCONFIG_INFO		0x1
#define	RDCONFIG_MEMSIZE	0X2
#define RDCONFIG_DEBUG		0x3

#ifndef LOCORE
#ifdef pon 
#define printf	printf
#define SetLed(x)	printf("Running x\n")	
/* #define clear_be() */
#else
/* #define SetLed(x)	dset_leds(x);	\
			message(VERBOSE_ON, "\n (x)\n")
*/
#define SetLed(x)	printf("SetLed\n");
#define quickmode()	0
#endif

#endif

#define err_lev 2

#undef DELAY
#define	DELAY(n)	us_delay(n)

#define RETERROR	if (err_lev == 2)			\
				return(1)

#define DisplayError(s, t, y, z) 			\
				printf("s test fail!\nAddress : 0x%x, Expected : 0x%x, Actual : 0x%x\n", (__psunsigned_t)t, (uint)y, (uint)z);	\
				RETERROR;
#define FpError(s, t, y) 					\
				printf("s test fail!\nExpected : %x, Actual : %x\n", t, y);							\
				RETERROR;

#ifndef pon
#define SlaveError(s, t, y, z) 					\
			printf("s test fail!\nAddress : 0x%x, Expected : 0x%x, Actual : 0x%x\n", (__psunsigned_t)t, (uint)y, (uint)z);					\
			if (err_lev == 2) {			\
				slave_shar();			\
				return(1);		\
			}

#define FpslaveError(s, t, y) 					\
			printf("s test fail!\nExpected : %f, Actual : %f\n", t, y);					\
			if (err_lev == 2) {			\
				slave_shar();			\
				return(1);		\
			}
#endif /* !pon */


#endif /* !__PATTERN_H__ */
