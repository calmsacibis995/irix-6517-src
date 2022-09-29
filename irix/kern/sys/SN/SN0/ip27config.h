/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1999, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN0_IP27CONFIG_H__
#define __SYS_SN_SN0_IP27CONFIG_H__

#include <sys/mips_addrspace.h>

/*
 * Structure: 	ip27config_s
 * Typedef:	ip27config_t
 * Purpose: 	Maps out the region of the boot prom used to define
 *		configuration information.
 * Notes:       Corresponds to ip27config structure found in start.s.
 *		Members are ulong where possible to facilitate PROM fetches.
 */

#define CONFIG_INFO_OFFSET		0x60

#define IP27CONFIG_ADDR			(LBOOT_BASE	    + \
					 CONFIG_INFO_OFFSET)
#define IP27CONFIG_ADDR_NODE(n)		(NODE_RBOOT_BASE(n) + \
					 CONFIG_INFO_OFFSET)

#define IP27CONFIG_SN00_ADDR		(IP27CONFIG_ADDR + 48)
#define IP27CONFIG_SN00_ADDR_NODE(n)	(IP27CONFIG_ADDR_NODE(n) + 48)

/* Offset to the config_type field within local ip27config structure */
#define CONFIG_FLAGS_ADDR			(IP27CONFIG_ADDR + 72)
/* Offset to the config_type field in the ip27config structure on 
 * node with nasid n
 */
#define CONFIG_FLAGS_ADDR_NODE(n)		(IP27CONFIG_ADDR_NODE(n) + 72)

/* Meaning of each valid bit in the config flags 
 * Right now only bit 0 is valid.
 * If it is set the config corresponds to 12p 4io
 */
#define CONFIG_FLAG_12P4I		0x1
/*
 * Since 800 ns works well with various HUB frequencies, (such as 360,
 * 380, 390, and 400 MHZ), we now use 800ns rtc cycle time instead of
 * 1 microsec.
 */
#define IP27_RTC_FREQ			1250	/* 800ns cycle time */

#if _LANGUAGE_C

typedef	struct ip27config_s {		/* KEEP IN SYNC w/ start.s & below  */
    uint		time_const;	/* Time constant 		    */
    uint		r10k_mode;	/* R10k boot mode bits 		    */

    __uint64_t		magic;		/* CONFIG_MAGIC			    */

    __uint64_t		freq_cpu;	/* Hz 				    */
    __uint64_t		freq_hub;	/* Hz 				    */
    __uint64_t		freq_rtc;	/* Hz 				    */

    uint		ecc_enable;	/* ECC enable flag		    */
    uint		fprom_cyc;	/* FPROM_CYC speed control  	    */

    uint		mach_type;	/* Inidicate SN0 (0) or Sn00 (1)    */

    uint		check_sum_adj;	/* Used after config hdr overlay    */
					/* to make the checksum 0 again     */
    uint		flash_count;	/* Value incr'd on each PROM flash  */
    uint		fprom_wr;	/* FPROM_WR speed control  	    */

    uint		pvers_vers;	/* Prom version number		    */
    uint		pvers_rev;	/* Prom revision number		    */
    uint		config_type;	/* To support special configurations
					 * If bit 0 is set it means that
					 * we are looking 12P4I config
					 */
} ip27config_t;

typedef	struct {
    uint		r10k_mode;	/* R10k boot mode bits 		    */
    uint		freq_cpu;	/* Hz 				    */
    uint		freq_hub;	/* Hz 				    */
    char		fprom_cyc;	/* FPROM_CYC speed control  	    */
    char		mach_type;	/* Inidicate SN0 (0) or Sn00 (1)    */
    char		fprom_wr;	/* FPROM_WR speed control  	    */
} ip27config_modifiable_t;

#define IP27CONFIG		(*(ip27config_t *) IP27CONFIG_ADDR)
#define IP27CONFIG_NODE(n)	(*(ip27config_t *) IP27CONFIG_ADDR_NODE(n))
#define SN00			(*(uint *) (IP27CONFIG_SN00_ADDR))
#define SN00_NODE(n)		(*(uint *) (IP27CONFIG_SN00_ADDR_NODE(n)))

/* Get the config flags from local ip27config */
#define CONFIG_FLAGS		(*(uint *) (CONFIG_FLAGS_ADDR))
/* Get the config flags from ip27config on the node
 * with nasid n
 */
#define CONFIG_FLAGS_NODE(n)	(*(uint *) (CONFIG_FLAGS_ADDR_NODE(n)))

/* Macro to check if the local ip27config indicates a config
 * of 12 p 4io
 */
#define CONFIG_12P4I		(CONFIG_FLAGS & CONFIG_FLAG_12P4I)
/* Macro to check if the ip27config on node with nasid n
 * indicates a config of 12 p 4io
 */
#define CONFIG_12P4I_NODE(n)	(CONFIG_FLAGS_NODE(n) & CONFIG_FLAG_12P4I)

#endif /* _LANGUAGE_C */

#if _LANGUAGE_ASSEMBLY
	.struct		0		/* KEEP IN SYNC WITH C structure */

ip27c_time_const:	.word	0
ip27c_r10k_mode:	.word	0

ip27c_magic:		.dword	0

ip27c_freq_cpu:		.dword	0
ip27c_freq_hub:		.dword	0
ip27c_freq_rtc:		.dword	0

ip27c_ecc_enable:	.word	1
ip27c_fprom_cyc:	.word	0

ip27c_mach_type:	.word	0
ip27c_check_sum_adj:	.word	0

ip27c_flash_count:	.word	0
ip27c_fprom_wr:		.word	0

ip27c_pvers_vers:	.word	0
ip27c_pvers_rev:	.word	0

ip27c_config_type:	.word 	0	/* To recognize special configs */
#endif /* _LANGUAGE_ASSEMBLY */

/*
 * R10000 Configuration Cycle - These define the SYSAD values used
 * during the reset cycle.
 */

#define	IP27C_R10000_KSEG0CA_SHFT	0
#define	IP27C_R10000_KSEG0CA_MASK	(7 << IP27C_R10000_KSEG0CA_SHFT)
#define	IP27C_R10000_KSEG0CA(_B)	 ((_B) << IP27C_R10000_KSEG0CA_SHFT)

#define	IP27C_R10000_DEVNUM_SHFT	3
#define	IP27C_R10000_DEVNUM_MASK	(3 << IP27C_R10000_DEVNUM_SHFT)
#define	IP27C_R10000_DEVNUM(_B)		((_B) << IP27C_R10000_DEVNUM_SHFT)

#define	IP27C_R10000_CRPT_SHFT		5
#define	IP27C_R10000_CRPT_MASK		(1 << IP27C_R10000_CRPT_SHFT)
#define	IP27C_R10000_CPRT(_B)		((_B)<<IP27C_R10000_CRPT_SHFT)

#define	IP27C_R10000_PER_SHFT		6
#define	IP27C_R10000_PER_MASK		(1 << IP27C_R10000_PER_SHFT)
#define	IP27C_R10000_PER(_B)		((_B) << IP27C_R10000_PER_SHFT)

#define	IP27C_R10000_PRM_SHFT		7
#define	IP27C_R10000_PRM_MASK		(3 << IP27C_R10000_PRM_SHFT)
#define	IP27C_R10000_PRM(_B)		((_B) << IP27C_R10000_PRM_SHFT)

#define	IP27C_R10000_SCD_SHFT		9
#define	IP27C_R10000_SCD_MASK		(0xf << IP27C_R10000_SCD_MASK)
#define	IP27C_R10000_SCD(_B)		((_B) << IP27C_R10000_SCD_SHFT)

#define	IP27C_R10000_SCBS_SHFT		13
#define	IP27C_R10000_SCBS_MASK		(1 << IP27C_R10000_SCBS_SHFT)
#define	IP27C_R10000_SCBS(_B)		(((_B)) << IP27C_R10000_SCBS_SHFT)

#define	IP27C_R10000_SCCE_SHFT		14
#define	IP27C_R10000_SCCE_MASK		(1 << IP27C_R10000_SCCE_SHFT)
#define	IP27C_R10000_SCCE(_B)		((_B) << IP27C_R10000_SCCE_SHFT)

#define	IP27C_R10000_ME_SHFT		15
#define	IP27C_R10000_ME_MASK		(1 << IP27C_R10000_ME_SHFT)
#define	IP27C_R10000_ME(_B)		((_B) << IP27C_R10000_ME_SHFT)

#define	IP27C_R10000_SCS_SHFT		16
#define	IP27C_R10000_SCS_MASK		(7 << IP27C_R10000_SCS_SHFT)
#define	IP27C_R10000_SCS(_B)		((_B) << IP27C_R10000_SCS_SHFT)

#define	IP27C_R10000_SCCD_SHFT		19
#define	IP27C_R10000_SCCD_MASK		(7 << IP27C_R10000_SCCD_SHFT)
#define	IP27C_R10000_SCCD(_B)		((_B) << IP27C_R10000_SCCD_SHFT)

#define	IP27C_R10000_DDR_SHFT		23
#define	IP27C_R10000_DDR_MASK		(1 << IP27C_R10000_DDR_SHFT)
#define	IP27C_R10000_DDR(_B)		((_B) << IP27C_R10000_DDR_SHFT)

#define	IP27C_R10000_SCCT_SHFT		25
#define	IP27C_R10000_SCCT_MASK		(0xf << IP27C_R10000_SCCT_SHFT)
#define	IP27C_R10000_SCCT(_B)		((_B) << IP27C_R10000_SCCT_SHFT)

#define	IP27C_R10000_ODSC_SHFT		29
#define IP27C_R10000_ODSC_MASK		(1 << IP27C_R10000_ODSC_SHFT)
#define	IP27C_R10000_ODSC(_B)		((_B) << IP27C_R10000_ODSC_SHFT)

#define	IP27C_R10000_ODSYS_SHFT		30
#define	IP27C_R10000_ODSYS_MASK		(1 << IP27C_R10000_ODSYS_SHFT)
#define	IP27C_R10000_ODSYS(_B)		((_B) << IP27C_R10000_ODSYS_SHFT)

#define	IP27C_R10000_CTM_SHFT		31
#define	IP27C_R10000_CTM_MASK		(1 << IP27C_R10000_CTM_SHFT)
#define	IP27C_R10000_CTM(_B)		((_B) << IP27C_R10000_CTM_SHFT)

#define IP27C_MHZ(x)			(1000000 * (x))
#define IP27C_KHZ(x)			(1000 * (x))
#define IP27C_MB(x)			((x) << 20)

/*
 * PROM Configurations
 */

#define CONFIG_MAGIC		0x69703237636f6e66

#define CONFIG_TIME_CONST	0xb

#define CONFIG_ECC_ENABLE	1
#define CONFIG_CHECK_SUM_ADJ	0
#define CONFIG_DEFAULT_FLASH_COUNT    0
#define CONFIG_SN0_FPROM_WR 1
#define CONFIG_SN00_FPROM_WR 4
#define CONFIG_SN0_FPROM_CYC 8
#define CONFIG_SN00_FPROM_CYC 15

/*
 * Some promICEs have trouble if FPROM_CYC is too low.
 * The nominal value for 100 MHz hub is 5.
 * any update to the below should also reflected in the logic in
 *   IO6prom/flashprom.c function _verify_config_info and _fill_in_config_info
 */

#define SN0_MACH_TYPE	      0
#define SN00_MACH_TYPE	      1

#define CONFIG_FREQ_RTC	IP27C_KHZ(IP27_RTC_FREQ)

#if _LANGUAGE_C

/* we are going to define all the known configs is a table
 * for building hex images we will pull out the particular
 * slice we care about by using the IP27_CONFIG_XX_XX as
 * entries into the table
 * to keep the table of reasonable size we only include the
 * values that differ across configurations
 * please note then that this makes assumptions about what
 * will and will not change across configurations
 */

/* these numbers are as the are ordered in the table below */
#define	IP27_CONFIG_UNKNOWN 			-1
#define IP27_CONFIG_SN0_1MB_100MHZ_TABLE 	 0
#define IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE 	 1
#define IP27_CONFIG_SN0_1MB_95MHZ_TABLE 	 2
#define IP27_CONFIG_SN00_1MB_90MHZ_TABLE 	 3
#define IP27_CONFIG_SN0_4MB_100MHZ_TABLE 	 4
#define IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE 	 5
#define IP27_CONFIG_SN0_4MB_95MHZ_TABLE   	 6
#define IP27_CONFIG_SN0_1MB_90MHZ_TABLE 	 7
#define IP27_CONFIG_SN00_2MB_95MHZ_TABLE  	 8
#define	IP27_CONFIG_SN0_4MB_100_350_233_TABLE 	 9	/* IP31 PSC 0x6 R12KS SGI2100  	*/
#define IP27_CONFIG_SN0_8MB_100_400_267_TABLE 	10	/* IP31 PSC 0x8 R12KS	       	*/
#define IP27_CONFIG_SN0_8MB_100_300_200_TABLE 	11	/* IP31 PSC 0x9 R10K		*/
#define	IP27_CONFIG_SN0_8MB_97_5_390_258_TABLE	12	/* IP31 PSC 0xe R12KS fallback 	*/
#define IP27_CONFIG_SN0_4MB_100_250_250_TABLE 	13	/* IP31 PSC 0xa R10K		*/
#define IP27_CONFIG_SN00_2MB_225MHZ_TABLE  	14
#define IP27_CONFIG_SN00_4MB_90_270_180_TABLE 	15
#define IP27_CONFIG_SN00_4MB_90_360_240_TABLE 	16

#define NUMB_IP27_CONFIGS 17

#ifdef DEF_IP27_CONFIG_TABLE
/*
 * N.B.: A new entry needs to be added here everytime a new config is added
 * The table is indexed by the PIMM PSC value
 */

static int ip31_psc_to_flash_config[] = {
/* 0x0 */ IP27_CONFIG_UNKNOWN,
/* 0x1 */ IP27_CONFIG_UNKNOWN,
/* 0x2 */ IP27_CONFIG_UNKNOWN,
/* 0x3 */ IP27_CONFIG_UNKNOWN,
/* 0x4 */ IP27_CONFIG_UNKNOWN,
/* 0x5 */ IP27_CONFIG_UNKNOWN,
/* 0x6 */ IP27_CONFIG_SN0_4MB_100_350_233_TABLE,  /* R12KS SGI2100	*/
/* 0x7 */ IP27_CONFIG_UNKNOWN,		          /* R14K		*/
/* 0x8 */ IP27_CONFIG_SN0_8MB_100_400_267_TABLE,  /* R12KS		*/
/* 0x9 */ IP27_CONFIG_SN0_8MB_100_300_200_TABLE,
/* 0xa */ IP27_CONFIG_SN0_4MB_100_250_250_TABLE,
/* 0xb */ IP27_CONFIG_UNKNOWN,
/* 0xc */ IP27_CONFIG_UNKNOWN,			  
/* 0xd */ IP27_CONFIG_UNKNOWN,
/* 0xe */ IP27_CONFIG_SN0_8MB_97_5_390_258_TABLE, /* R12KS fallback  	*/
/* 0xf */ IP27_CONFIG_UNKNOWN
};

static ip27config_modifiable_t ip27config_table[NUMB_IP27_CONFIGS] = {
	/* the 1MB_100MHZ values */
{
/* 0 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)), 
	IP27C_MHZ(200),
	IP27C_MHZ(100),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 1MB_97_5MZ values */
{
/* 1 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(195),
	IP27C_KHZ(97500),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 1MB_95MZ values */
{
/* 2 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(190),
	IP27C_MHZ(95),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 1MB_90MZ values */
{
/* 3 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(180),
	IP27C_MHZ(90),
	CONFIG_SN00_FPROM_CYC,
	SN00_MACH_TYPE,
	CONFIG_SN00_FPROM_WR
},

/* the 4MB_100MZ values */
{
/* 4 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(3)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(200),
	IP27C_MHZ(100),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 4MB_97_5MZ values */
{
/* 5 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(3)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(195),
	IP27C_KHZ(97500),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 4MB_95MZ values */
{
/* 6 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(3)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(190),
	IP27C_MHZ(95),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 1MB_90MZ values for SN0 */
{
/* 7 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(180),
	IP27C_MHZ(90),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 2MB_97_5MZ SN00 values */
{
/* 8 */
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(2)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(195),
	IP27C_KHZ(97500),
	CONFIG_SN00_FPROM_CYC,
	SN00_MACH_TYPE,
	CONFIG_SN00_FPROM_WR
},

{
/* 9 */
	/*
	 *	IP27_CONFIG_SN0_4MB_100_350_233_TABLE
	 *	IP31 PSC 0x6 R12KS SGI2100	
	 */
	(IP27C_R10000_KSEG0CA(5) + 				\
	 IP27C_R10000_DEVNUM(0)	 + 				\
	 IP27C_R10000_CPRT(0)	 + 				\
	 IP27C_R10000_PER(0)	 + 				\
	 IP27C_R10000_PRM(3)	 + 				\
	 IP27C_R10000_SCD(6)	 +  	/* clk div 3.5 	*/	\
	 IP27C_R10000_SCBS(1)	 +  				\
	 IP27C_R10000_SCCE(0)	 +  				\
	 IP27C_R10000_ME(1)	 +  				\
	 IP27C_R10000_SCS(3)	 +  	/* 4 Mbyte L2	*/	\
	 IP27C_R10000_SCCD(2)	 +  	/* 233 MHZ L2	*/	\
	 IP27C_R10000_SCCT(0x9)	 +  	/* tap setting  */	\
	 IP27C_R10000_ODSC(0)	 +  				\
	 IP27C_R10000_ODSYS(0)	 + 				\
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(350),
	IP27C_MHZ(100),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},


{
/*10*/
	/*
	 *	IP27_CONFIG_SN0_8MB_100_400_267_TABLE
	 *	IP31 PSC 0x8 R12KS
	 */
	(IP27C_R10000_KSEG0CA(5) +  				\
	 IP27C_R10000_DEVNUM(0)	 +  				\
	 IP27C_R10000_CPRT(0)	 +  				\
	 IP27C_R10000_PER(0)	 +  				\
	 IP27C_R10000_PRM(3)	 +  				\
	 IP27C_R10000_SCD(7)	 +  	/* clk div 4.0 	*/	\
	 IP27C_R10000_SCBS(1)	 +  				\
	 IP27C_R10000_SCCE(0)	 +  				\
	 IP27C_R10000_ME(1)	 +  				\
	 IP27C_R10000_SCS(4)	 +  	/* 8 Mbyte L2	*/	\
	 IP27C_R10000_SCCD(2)	 +  	/* 267 MHZ L2	*/	\
	 IP27C_R10000_SCCT(0xa)	 +  	/* tap setting  */	\
	 IP27C_R10000_ODSC(0)	 +  				\
	 IP27C_R10000_ODSYS(0)	 +  				\
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(400),
	IP27C_MHZ(100),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},


/* the 8MB_100_300_200 values */
{
/*11*/
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(5)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(4)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(0xa)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(300),
	IP27C_MHZ(100),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

{
/*12*/
	/*
	 *	IP27_CONFIG_SN0_8MB_97_5_390_258_TABLE  
	 *	IP31 PSC 0xe R12KS fallback  
	 */
	(IP27C_R10000_KSEG0CA(5) +  				\
	 IP27C_R10000_DEVNUM(0)	 +  				\
	 IP27C_R10000_CPRT(0)	 +  				\
	 IP27C_R10000_PER(0)	 +  				\
	 IP27C_R10000_PRM(3)	 +  				\
	 IP27C_R10000_SCD(7)	 +  	/* clk div 4.0 	*/	\
	 IP27C_R10000_SCBS(1)	 +  				\
	 IP27C_R10000_SCCE(0)	 +  				\
	 IP27C_R10000_ME(1)	 +  				\
	 IP27C_R10000_SCS(4)	 +  	/* 8 Mbyte L2	*/	\
	 IP27C_R10000_SCCD(2)	 +  	/* 258 MHZ L2	*/	\
	 IP27C_R10000_SCCT(0xa)	 +  	/* tap setting  */	\
	 IP27C_R10000_ODSC(0)	 +  				\
	 IP27C_R10000_ODSYS(0)	 +  				\
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(390),
        IP27C_KHZ(97500),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 4MB_100_250_250 values */
{
/*13*/
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(4)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(3)	 + \
	 IP27C_R10000_SCCD(1)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(250),
	IP27C_MHZ(100),
	CONFIG_SN0_FPROM_CYC,
	SN0_MACH_TYPE,
	CONFIG_SN0_FPROM_WR
},

/* the 2MB_225MHZ SN00 values */
{
/*14*/
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(4)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(2)	 + \
	 IP27C_R10000_SCCD(1)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(225),
	IP27C_MHZ(90),
	CONFIG_SN00_FPROM_CYC,
	SN00_MACH_TYPE,
	CONFIG_SN00_FPROM_WR
},

/* the 4MB_90_270_180 values */
{
/*15*/
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(5)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(3)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0)),
	IP27C_MHZ(270),
	IP27C_MHZ(90),
	CONFIG_SN00_FPROM_CYC,
	SN00_MACH_TYPE,
	CONFIG_SN00_FPROM_WR
},


/* O200 R12KS 4MB_90_360_240 values */
{
/*16*/
        (IP27C_R10000_KSEG0CA(5) +  				\
         IP27C_R10000_DEVNUM(0)  +  				\
         IP27C_R10000_CPRT(0)    +  				\
         IP27C_R10000_PER(0)     +  				\
         IP27C_R10000_PRM(3)     +  				\
         IP27C_R10000_SCD(7)     +  	/* clk div 4.0	*/	\
         IP27C_R10000_SCBS(1)    +  				\
         IP27C_R10000_SCCE(0)    +  				\
         IP27C_R10000_ME(1)      +  				\
         IP27C_R10000_SCS(3)     +  	/*   4 MB  L2	 */	\
         IP27C_R10000_SCCD(2)    +  	/* 240 MHZ L2	 */	\
         IP27C_R10000_SCCT(9)    +  	/* expected 0x9  */	\
         IP27C_R10000_ODSC(0)    +  				\
         IP27C_R10000_ODSYS(0)   +  				\
         IP27C_R10000_CTM(0)),
        IP27C_MHZ(360),
        IP27C_MHZ(90),
        CONFIG_SN00_FPROM_CYC,
        SN00_MACH_TYPE,
        CONFIG_SN00_FPROM_WR
}


};
#else
extern	ip27config_modifiable_t	ip27config_table[];
#endif /* DEF_IP27_CONFIG_TABLE */


#ifdef IP27_CONFIG_SN0_1MB_100MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_1MB_100MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_1MB_100MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_1MB_100MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_1MB_100MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_1MB_100MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_1MB_100MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_1MB_100MHZ */


#ifdef IP27_CONFIG_SN0_1MB_97_5MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_1MB_97_5MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_1MB_97_5MHZ */

#ifdef IP27_CONFIG_SN0_1MB_95MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_1MB_95MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_1MB_95MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_1MB_95MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_1MB_95MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_1MB_95MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_1MB_95MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_1MB_95MHZ */

#ifdef IP27_CONFIG_SN00_1MB_90MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN00_1MB_90MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN00_1MB_90MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN00_1MB_90MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN00_1MB_90MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN00_1MB_90MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN00_1MB_90MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN00_1MB_90MHZ */

#ifdef IP27_CONFIG_SN00_2MB_225MHZ
#define CONFIG_CPU_MODE ip27config_table[IP27_CONFIG_SN00_2MB_225MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU ip27config_table[IP27_CONFIG_SN00_2MB_225MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB ip27config_table[IP27_CONFIG_SN00_2MB_225MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN00_2MB_225MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN00_2MB_225MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR ip27config_table[IP27_CONFIG_SN00_2MB_225MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN00_2MB_225MHZ */


#ifdef IP27_CONFIG_SN0_4MB_100MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_4MB_100MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_4MB_100MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_4MB_100MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_4MB_100MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_4MB_100MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_4MB_100MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_4MB_100MHZ */

#ifdef IP27_CONFIG_SN0_4MB_97_5MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_4MB_97_5MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_4MB_97_5MHZ */


#ifdef IP27_CONFIG_SN0_4MB_95MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_4MB_95MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_4MB_95MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_4MB_95MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_4MB_95MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_4MB_95MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_4MB_95MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_4MB_95MHZ */

#ifdef IP27_CONFIG_SN0_1MB_90MHZ
#define CONFIG_CPU_MODE	ip27config_table[IP27_CONFIG_SN0_1MB_90MHZ_TABLE].r10k_mode
#define CONFIG_FREQ_CPU	ip27config_table[IP27_CONFIG_SN0_1MB_90MHZ_TABLE].freq_cpu
#define CONFIG_FREQ_HUB	ip27config_table[IP27_CONFIG_SN0_1MB_90MHZ_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN0_1MB_90MHZ_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN0_1MB_90MHZ_TABLE].mach_type
#define CONFIG_FPROM_WR	ip27config_table[IP27_CONFIG_SN0_1MB_90MHZ_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN0_1MB_90MHZ */

#ifdef IP27_CONFIG_SN00_4MB_270MHZ
#define CONFIG_CPU_MODE ip27config_table[IP27_CONFIG_SN00_4MB_90_270_180_TABLE].r10k_mode
#define CONFIG_FREQ_CPU ip27config_table[IP27_CONFIG_SN00_4MB_90_270_180_TABLE].freq_cpu
#define CONFIG_FREQ_HUB ip27config_table[IP27_CONFIG_SN00_4MB_90_270_180_TABLE].freq_hub
#define CONFIG_FPROM_CYC ip27config_table[IP27_CONFIG_SN00_4MB_90_270_180_TABLE].fprom_cyc
#define CONFIG_MACH_TYPE ip27config_table[IP27_CONFIG_SN00_4MB_90_270_180_TABLE].mach_type
#define CONFIG_FPROM_WR ip27config_table[IP27_CONFIG_SN00_4MB_90_270_180_TABLE].fprom_wr
#endif /* IP27_CONFIG_SN00_4MB_270MHZ */

#endif /* _LANGUAGE_C */

#if _LANGUAGE_ASSEMBLY

/* these need to be in here since we need assemble definitions
 * for building hex images (as required by start.c)
 */
#ifdef IP27_CONFIG_SN0_4MB_97_5MHZ
#define CONFIG_CPU_MODE	\
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(3)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0))
#define CONFIG_FREQ_CPU	IP27C_MHZ(195)
#define CONFIG_FREQ_HUB	IP27C_KHZ(97500)
#define CONFIG_FPROM_CYC CONFIG_SN0_FPROM_CYC
#define CONFIG_MACH_TYPE SN0_MACH_TYPE
#define CONFIG_FPROM_WR	CONFIG_SN0_FPROM_WR
#endif /* IP27_CONFIG_SN0_4MB_97_5MHZ */

#ifdef IP27_CONFIG_SN0_1MB_90MHZ
#define CONFIG_CPU_MODE	\
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0))
#define CONFIG_FREQ_CPU	IP27C_MHZ(180)
#define CONFIG_FREQ_HUB	IP27C_MHZ(90)
#define CONFIG_FPROM_CYC CONFIG_SN0_FPROM_CYC
#define CONFIG_MACH_TYPE SN0_MACH_TYPE
#define CONFIG_FPROM_WR	CONFIG_SN0_FPROM_WR
#endif /* IP27_CONFIG_SN0_1MB_90MHZ */

/* define speedo config */
#ifdef IP27_CONFIG_SN00_1MB_90MHZ
#define CONFIG_CPU_MODE	\
	(IP27C_R10000_KSEG0CA(5) + \
	 IP27C_R10000_DEVNUM(0)	 + \
	 IP27C_R10000_CPRT(0)	 + \
	 IP27C_R10000_PER(0)	 + \
	 IP27C_R10000_PRM(3)	 + \
	 IP27C_R10000_SCD(3)	 + \
	 IP27C_R10000_SCBS(1)	 + \
	 IP27C_R10000_SCCE(0)	 + \
	 IP27C_R10000_ME(1)	 + \
	 IP27C_R10000_SCS(1)	 + \
	 IP27C_R10000_SCCD(2)	 + \
	 IP27C_R10000_SCCT(9)	 + \
	 IP27C_R10000_ODSC(0)	 + \
	 IP27C_R10000_ODSYS(0)	 + \
	 IP27C_R10000_CTM(0))
#define CONFIG_FREQ_CPU	IP27C_MHZ(180)
#define CONFIG_FREQ_HUB	IP27C_MHZ(90)
#define CONFIG_FPROM_CYC CONFIG_SN00_FPROM_CYC
#define CONFIG_MACH_TYPE SN00_MACH_TYPE
#define CONFIG_FPROM_WR	CONFIG_SN00_FPROM_WR
#endif /* IP27_CONFIG_SN00_1MB_90MHZ */

#ifdef IP27_CONFIG_SN00_2MB_225MHZ
#define CONFIG_CPU_MODE \
        (IP27C_R10000_KSEG0CA(5) + \
         IP27C_R10000_DEVNUM(0)  + \
         IP27C_R10000_CPRT(0)    + \
         IP27C_R10000_PER(0)     + \
         IP27C_R10000_PRM(3)     + \
         IP27C_R10000_SCD(4)     + \
         IP27C_R10000_SCBS(1)    + \
         IP27C_R10000_SCCE(0)    + \
         IP27C_R10000_ME(1)      + \
         IP27C_R10000_SCS(2)     + \
         IP27C_R10000_SCCD(1)    + \
         IP27C_R10000_SCCT(9)    + \
         IP27C_R10000_ODSC(0)    + \
         IP27C_R10000_ODSYS(0)   + \
         IP27C_R10000_CTM(0))
#define CONFIG_FREQ_CPU IP27C_MHZ(225)
#define CONFIG_FREQ_HUB IP27C_MHZ(90)
#define CONFIG_FPROM_CYC CONFIG_SN00_FPROM_CYC
#define CONFIG_MACH_TYPE SN00_MACH_TYPE
#define CONFIG_FPROM_WR CONFIG_SN00_FPROM_WR
#endif /* IP27_CONFIG_SN00_2MB_225MHZ */

#ifdef IP27_CONFIG_SN00_4MB_270MHZ
#define CONFIG_CPU_MODE \
        (IP27C_R10000_KSEG0CA(5) + \
         IP27C_R10000_DEVNUM(0)  + \
         IP27C_R10000_CPRT(0)    + \
         IP27C_R10000_PER(0)     + \
         IP27C_R10000_PRM(3)     + \
         IP27C_R10000_SCD(5)     + \
         IP27C_R10000_SCBS(1)    + \
         IP27C_R10000_SCCE(0)    + \
         IP27C_R10000_ME(1)      + \
         IP27C_R10000_SCS(3)     + \
         IP27C_R10000_SCCD(2)    + \
         IP27C_R10000_SCCT(9)    + \
         IP27C_R10000_ODSC(0)    + \
         IP27C_R10000_ODSYS(0)   + \
         IP27C_R10000_CTM(0))
#define CONFIG_FREQ_CPU IP27C_MHZ(270)
#define CONFIG_FREQ_HUB IP27C_MHZ(90)
#define CONFIG_FPROM_CYC CONFIG_SN00_FPROM_CYC
#define CONFIG_MACH_TYPE SN00_MACH_TYPE
#define CONFIG_FPROM_WR CONFIG_SN00_FPROM_WR
#endif /* IP27_CONFIG_SN00_4MB_270MHZ */

#endif /* _LANGUAGE_C */

#endif /* __SYS_SN_SN0_IP27CONFIG_H__ */
