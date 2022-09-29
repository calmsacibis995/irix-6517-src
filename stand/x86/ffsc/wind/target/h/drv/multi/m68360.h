/* m68360.h - Motorola MC68360 CPU control registers */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,24jan94,dzb  fixed size of M360_CPM_GSMR_L3, M360_CPM_PSMR3, M360_CPM_PCINT.
01b,05oct93,dzb  added ethernet macros.  broke out CPM interrupt vectors.
		 added SCC_DEV abstraction.  added CPM command macros.
01a,04aug93,dzb  written.
*/

/*
This file contains I/O addresses and related constants for the MC68360.
*/

#ifndef __INCm68360h
#define __INCm68360h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

#ifndef	INCLUDE_TY_CO_DRV_50
#include "tylib.h"
#endif	/* INCLUDE_TY_CO_DRV_50 */

/* MC68360 Module Base Address Register fixed locations */

#define M360_SIM_MBAR		0x0003ff00  /* master base address register */
#define M360_SIM_MBAR_SLAVE	0x0003ff04  /* slave base address register */
#define M360_SIM_MBARE		0x0003ff08  /* slave MBAR enable address */

/* MC68360 Dual Ported Ram addresses */

#define M360_DPR_SCC1(base)	((UINT32 *) (base + 0xc00))
#define M360_DPR_MISC(base)	((UINT32 *) (base + 0xcb0))
#define M360_DPR_SCC2(base)	((UINT32 *) (base + 0xd00))
#define M360_DPR_TMR(base)	((UINT32 *) (base + 0xd70))
#define M360_DPR_SPI(base)	((UINT32 *) (base + 0xd80))
#define M360_DPR_SCC3(base)	((UINT32 *) (base + 0xe00))
#define M360_DPR_IDMA1(base)	((UINT32 *) (base + 0xe70))
#define M360_DPR_SMC1(base)	((UINT32 *) (base + 0xe80))
#define M360_DPR_SCC4(base)	((UINT32 *) (base + 0xf00))
#define M360_DPR_IDMA2(base)	((UINT32 *) (base + 0xf70))
#define M360_DPR_SMC2(base)	((UINT32 *) (base + 0xf80))

/* MC68360 register addresses in parameter ram */

#define M360_REGB_OFFSET	0x1000	/* offset to internal registers */

/* System Integration Module register addresses */

#define M360_SIM_MCR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x000))
#define M360_SIM_AVR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x008))
#define M360_SIM_RSR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x009))
#define M360_SIM_CLKOCR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x00c))
#define M360_SIM_PLLCR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x010))
#define M360_SIM_CDVCR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x014))
#define M360_SIM_PEPAR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x016))
#define M360_SIM_SYPCR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x022))
#define M360_SIM_SWIV(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x023))
#define M360_SIM_PICR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x026))
#define M360_SIM_PITR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x02a))
#define M360_SIM_SWSR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x02f))
#define M360_SIM_BKAR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x030))
#define M360_SIM_BKCR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x034))
#define M360_SIM_GMR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x040))
#define M360_SIM_MSTAT(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x044))
#define M360_SIM_BR0(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x050))
#define M360_SIM_OR0(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x054))
#define M360_SIM_BR1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x060))
#define M360_SIM_OR1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x064))
#define M360_SIM_BR2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x070))
#define M360_SIM_OR2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x074))
#define M360_SIM_BR3(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x080))
#define M360_SIM_OR3(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x084))
#define M360_SIM_BR4(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x090))
#define M360_SIM_OR4(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x094))
#define M360_SIM_BR5(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x0a0))
#define M360_SIM_OR5(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x0a4))
#define M360_SIM_BR6(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x0b0))
#define M360_SIM_OR6(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x0b4))
#define M360_SIM_BR7(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x0c0))
#define M360_SIM_OR7(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x0c4))

/* Communication Processor Module register addresses */

#define M360_CPM_ICCR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x500))
#define M360_CPM_CMR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x504))
#define M360_CPM_SAPR1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x508))
#define M360_CPM_DAPR1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x50c))
#define M360_CPM_BCR1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x510))
#define M360_CPM_FCR1(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x514))
#define M360_CPM_CMAR1(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x516))
#define M360_CPM_CSR1(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x518))
#define M360_CPM_SDSR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x51c))
#define M360_CPM_SDCR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x51e))
#define M360_CPM_SDAR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x520))
#define M360_CPM_CMR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x526))
#define M360_CPM_SAPR2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x528))
#define M360_CPM_DAPR2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x52c))
#define M360_CPM_BCR2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x530))
#define M360_CPM_FCR2(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x534))
#define M360_CPM_CMAR2(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x536))
#define M360_CPM_CSR2(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x538))
#define M360_CPM_CICR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x540))
#define M360_CPM_CIPR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x544))
#define M360_CPM_CIMR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x548))
#define M360_CPM_CISR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x54c))
#define M360_CPM_PADIR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x550))
#define M360_CPM_PAPAR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x552))
#define M360_CPM_PAODR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x554))
#define M360_CPM_PADAT(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x556))
#define M360_CPM_PCDIR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x560))
#define M360_CPM_PCPAR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x562))
#define M360_CPM_PCSO(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x564))
#define M360_CPM_PCDAT(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x566))
#define M360_CPM_PCINT(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x568))
#define M360_CPM_TGCR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x580))
#define M360_CPM_TMR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x590))
#define M360_CPM_TMR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x592))
#define M360_CPM_TRR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x594))
#define M360_CPM_TRR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x596))
#define M360_CPM_TCR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x598))
#define M360_CPM_TCR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x59a))
#define M360_CPM_TCN1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x59c))
#define M360_CPM_TCN2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x59e))
#define M360_CPM_TMR3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5a0))
#define M360_CPM_TMR4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5a2))
#define M360_CPM_TRR3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5a4))
#define M360_CPM_TRR4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5a6))
#define M360_CPM_TCR3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5a8))
#define M360_CPM_TCR4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5aa))
#define M360_CPM_TCN3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5ac))
#define M360_CPM_TCN4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5ae))
#define M360_CPM_TER1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5b0))
#define M360_CPM_TER2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5b2))
#define M360_CPM_TER3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5b4))
#define M360_CPM_TER4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5b6))
#define M360_CPM_CR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5c0))
#define M360_CPM_RCCR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5c4))
#define M360_CPM_RTER(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5d6))
#define M360_CPM_RTMR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x5da))
#define M360_CPM_BRGC1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x5f0))
#define M360_CPM_BRGC2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x5f4))
#define M360_CPM_BRGC3(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x5f8))
#define M360_CPM_BRGC4(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x5fc))
#define M360_CPM_GSMR_L1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x600))
#define M360_CPM_GSMR_H1(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x604))
#define M360_CPM_PSMR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x608))
#define M360_CPM_TODR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x60c))
#define M360_CPM_DSR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x60e))
#define M360_CPM_SCCE1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x610))
#define M360_CPM_SCCM1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x614))
#define M360_CPM_SCCS1(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x617))
#define M360_CPM_GSMR_L2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x620))
#define M360_CPM_GSMR_H2(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x624))
#define M360_CPM_PSMR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x628))
#define M360_CPM_TODR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x62c))
#define M360_CPM_DSR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x62e))
#define M360_CPM_SCCE2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x630))
#define M360_CPM_SCCM2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x634))
#define M360_CPM_SCCS2(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x637))
#define M360_CPM_GSMR_L3(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x640))
#define M360_CPM_GSMR_H3(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x644))
#define M360_CPM_PSMR3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x648))
#define M360_CPM_TODR3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x64c))
#define M360_CPM_DSR3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x64e))
#define M360_CPM_SCCE3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x650))
#define M360_CPM_SCCM3(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x654))
#define M360_CPM_SCCS3(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x657))
#define M360_CPM_GSMR_L4(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x660))
#define M360_CPM_GSMR_H4(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x664))
#define M360_CPM_PSMR4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x668))
#define M360_CPM_TODR4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x66c))
#define M360_CPM_DSR4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x66e))
#define M360_CPM_SCCE4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x670))
#define M360_CPM_SCCM4(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x674))
#define M360_CPM_SCCS4(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x677))
#define M360_CPM_SMCMR1(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x682))
#define M360_CPM_SMCE1(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x686))
#define M360_CPM_SMCM1(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x68a))
#define M360_CPM_SMCMR2(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x692))
#define M360_CPM_SMCE2(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x696))
#define M360_CPM_SMCM2(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x69a))
#define M360_CPM_SPMODE(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x6a0))
#define M360_CPM_SPIE(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x6a6))
#define M360_CPM_SPIM(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x6aa))
#define M360_CPM_SPCOM(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x6ad))
#define M360_CPM_PIPC(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x6b2))
#define M360_CPM_PTPR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x6b6))
#define M360_CPM_PBDIR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x6b8))
#define M360_CPM_PBPAR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x6bc))
#define M360_CPM_PBODR(base)	((UINT16 *) (base + M360_REGB_OFFSET + 0x6c2))
#define M360_CPM_PBDAT(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x6c4))
#define M360_CPM_SIMODE(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x6e0))
#define M360_CPM_SIGMR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x6e4))
#define M360_CPM_SISTR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x6e6))
#define M360_CPM_SICMR(base)	((UINT8  *) (base + M360_REGB_OFFSET + 0x6e7))
#define M360_CPM_SICR(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x6ec))
#define M360_CPM_SIRP(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x6f2))
#define M360_CPM_SIRAM(base)	((UINT32 *) (base + M360_REGB_OFFSET + 0x700))

/*** SIM - register definitions for the System Integration Module ***/

/* SIM - System Integration Module */

/* SIM Module Configuration Register definitions */

#define SIM_MCR_IARB_OFF	0x00000000	/* interrupt arb = off */
#define SIM_MCR_IARB_LO		0x00000001	/* interrupt arb = 0x1 */
#define SIM_MCR_IARB_HI		0x0000000f	/* interrupt arb = 0xf */
#define SIM_MCR_BCLRISM		0x00000070	/* bus clear in arb ID */
#define SIM_MCR_BCLRIID		0x00000070	/* bus clear int service mask */
#define SIM_MCR_SUPV		0x00000080	/* supervisor/user data space */
#define SIM_MCR_SHEN		0x00000300	/* show cycle enable */
#define SIM_MCR_BCLROID		0x00001c00	/* bus clear out arb ID */
#define SIM_MCR_FRZ0		0x00002000	/* freeze enable - bus mon */
#define SIM_MCR_FRZ1		0x00004000	/* freeze enable - SWE & PIT */
#define SIM_MCR_ASTM		0x00008000	/* arb synch timing mode */
#define SIM_MCR_BSTM		0x00010000	/* bus synch timing mode */
#define SIM_MCR_BR040ID		0xe0000000	/* bus request 68040 arb ID */

/* SIM Clock Out Control Register definitions */

#define SIM_CLKOCR_COM1_FULL	0x00		/* CLKO1 full strength output */
#define SIM_CLKOCR_COM1_TWO	0x01		/* CLKO1 two thirds strength */
#define SIM_CLKOCR_COM1_ONE	0x02		/* CLKO1 one third strength */
#define SIM_CLKOCR_COM1_DIS	0x03		/* CLKO1 disabled */
#define SIM_CLKOCR_COM2_FULL	0x00		/* CLKO2 full strength output */
#define SIM_CLKOCR_COM2_TWO	0x04		/* CLKO2 two thirds strength */
#define SIM_CLKOCR_COM2_ONE	0x08		/* CLKO2 one third strength */
#define SIM_CLKOCR_COM2_DIS	0x0c		/* CLKO2 disabled */
#define SIM_CLKOCR_RSTEN	0x20		/* loss of lock causes reset */
#define SIM_CLKOCR_CLKOWP	0x80		/* CLKOCR write protect */

/* SIM PLL Control Register definitions */

#define SIM_PLLCR_MF		0x0fff		/* multiplication factor */
#define SIM_PLLCR_STSIM		0x1000		/* stop mode - VCO drives SIM */
#define SIM_PLLCR_PREEN		0x2000		/* prescaller enable */
#define SIM_PLLCR_PLLWP		0x4000		/* PLLCR write protect */
#define SIM_PLLCR_PLLEN		0x8000		/* PLL enable */

/* SIM Clock Divider Control Register defintions */

#define SIM_CDVCR_CSRC		0x0001		/* high/low freq system clk */
#define SIM_CDVCR_DFNH_1	0x0000		/* divide high freq clk by 1 */
#define SIM_CDVCR_DFNH_2	0x0002		/* divide high freq clk by 2 */
#define SIM_CDVCR_DFNH_4	0x0004		/* divide high freq clk by 4 */
#define SIM_CDVCR_DFNH_8	0x0006		/* divide high freq clk by 8 */
#define SIM_CDVCR_DFNH_16	0x0008		/* divide high freq clk by 16 */
#define SIM_CDVCR_DFNH_32	0x000a		/* divide high freq clk by 32 */
#define SIM_CDVCR_DFNH_64	0x000c		/* divide high freq clk by 64 */
#define SIM_CDVCR_DFNL_2	0x0000		/* divide low freq clk by 2 */
#define SIM_CDVCR_DFNL_4	0x0010		/* divide low freq clk by 4 */
#define SIM_CDVCR_DFNL_8	0x0020		/* divide low freq clk by 8 */
#define SIM_CDVCR_DFNL_16	0x0030		/* divide low freq clk by 16 */
#define SIM_CDVCR_DFNL_32	0x0040		/* divide low freq clk by 32 */
#define SIM_CDVCR_DFNL_64	0x0050		/* divide low freq clk by 64 */
#define SIM_CDVCR_DFNL_256	0x0070		/* divide low freq clk by 256 */
#define SIM_CDVCR_RRQEN		0x0080		/* RISC idle -> high freq clk */
#define SIM_CDVCR_INTEN		0x0700		/* interrupt -> high freq clk */
#define SIM_CDVCR_DFTM_1	0x0000		/* divide BRGCLK by 1 - norm */
#define SIM_CDVCR_DFTM_4	0x0800		/* divide BRGCLK by 4 */
#define SIM_CDVCR_DFTM_16	0x1000		/* divide BRGCLK by 16 */
#define SIM_CDVCR_DFTM_64	0x1800		/* divide BRGCLK by 64 */
#define SIM_CDVCR_DFSY_1	0x0000		/* divide SyncClk by 1 - norm */
#define SIM_CDVCR_DFSY_4	0x2000		/* divide SyncClk by 4 */
#define SIM_CDVCR_DFSY_16	0x4000		/* divide SyncClk by 16 */
#define SIM_CDVCR_DFSY_64	0x6000		/* divide SyncClk by 64 */
#define SIM_CDVCR_CDVWP		0x8000		/* CDVCR write protect */

/* SIM Auto Vector Register definitions */

#define SIM_AVR_LVL1		0x02		/* auto vector on IRQ1 */
#define SIM_AVR_LVL2		0x04		/* auto vector on IRQ2 */
#define SIM_AVR_LVL3		0x08		/* auto vector on IRQ3 */
#define SIM_AVR_LVL4		0x10		/* auto vector on IRQ4 */
#define SIM_AVR_LVL5		0x20		/* auto vector on IRQ5 */
#define SIM_AVR_LVL6		0x40		/* auto vector on IRQ6 */
#define SIM_AVR_LVL7		0x80		/* auto vector on IRQ7 */

/* SIM Based Interrupt Autovector addresses */

#define INT_VEC_AV1		0x19		/* vector location of AV1 */
#define INT_VEC_AV2		0x1a		/* vector location of AV2 */
#define INT_VEC_AV3		0x1b		/* vector location of AV3 */
#define INT_VEC_AV4		0x1c		/* vector location of AV4 */
#define INT_VEC_AV5		0x1d		/* vector location of AV5 */
#define INT_VEC_AV6		0x1e		/* vector location of AV6 */
#define INT_VEC_AV7		0x1f		/* vector location of AV7 */

/* SIM Reset Status Register definitions */

#define SIM_RSR_SRSTP		0x01		/* soft reset pin */
#define SIM_RSR_SRST		0x02		/* soft reset */
#define SIM_RSR_LOC		0x04		/* loss of clock reset */
#define SIM_RSR_DBF		0x10		/* double bus fault reset */
#define SIM_RSR_SW		0x20		/* software watchdog reset */
#define SIM_RSR_POW		0x40		/* power up reset */
#define SIM_RSR_EXT		0x80		/* external reset */

/* SIM System Protection Control Register definitions (write once only) */

#define SIM_SYPCR_BMT_1024	0x00		/* 1024 clk monitor timeout */
#define SIM_SYPCR_BMT_512	0x01		/* 512 clk monitor timeout */
#define SIM_SYPCR_BMT_256	0x02		/* 256 clk monitor timeout */
#define SIM_SYPCR_BMT_128	0x03		/* 128 clk monitor timeout */
#define SIM_SYPCR_BME		0x04		/* bus monitor enable */
#define SIM_SYPCR_DBFE		0x08		/* double bus fault enable */
#define SIM_SYPCR_SWT_9		0x00		/* 2^9  clk cycles (SWP=0) */
#define SIM_SYPCR_SWT_11 	0x10		/* 2^11 clk cycles (SWP=0) */
#define SIM_SYPCR_SWT_13	0x20		/* 2^13 clk cycles (SWP=0) */
#define SIM_SYPCR_SWT_15	0x30		/* 2^15 clk cycles (SWP=0) */
#define SIM_SYPCR_SWT_18	0x00		/* 2^18 clk cycles (SWP=1) */
#define SIM_SYPCR_SWT_20	0x10		/* 2^20 clk cycles (SWP=1) */
#define SIM_SYPCR_SWT_22	0x20		/* 2^22 clk cycles (SWP=1) */
#define SIM_SYPCR_SWT_24	0x30		/* 2^24 clk cycles (SWP=1) */
#define SIM_SYPCR_SWRI		0x40		/* soft watchdog resets CPU */
#define SIM_SYPCR_SWE		0x80		/* soft watchdog enable */

/* SIM Periodic Interrupt Control Register definitions */

#define SIM_PICR_PIV		0x00ff		/* PIT int vector */
#define SIM_PICR_PIRQL		0x0700		/* PIT int request level */

/* SIM Periodic Interrupt Timer Register definitions */

#define SIM_PITR_PITR_CNT	0x00ff		/* PIT counter register */
#define SIM_PITR_PTP		0x0100		/* PIT prescale bit */
#define SIM_PITR_SWP		0x0200		/* soft watchdog prescale bit */

/* SIM Software Watchdog Service Register definitions */

#define SIM_SWSR_ACK1		0x55		/* software watchdog ack. 1 */
#define SIM_SWSR_ACK2		0xaa		/* software watchdog ack. 2 */

/* EBI - External Bus Interface */

/* EBI Port E Pin Assignment Register definitions */

#define EBI_PEPAR_IACK5		0x0001		/* select IACK5* output func */
#define EBI_PEPAR_IACK7		0x0002		/* select IACK7* output func */
#define EBI_PEPAR_IACK1_2	0x0004		/* select IACK1,2 output func */
#define EBI_PEPAR_IACK3_6	0x0010		/* select IACK3,6 output func */
#define EBI_PEPAR_PWW		0x0020		/* PEPAR was written */
#define EBI_PEPAR_AMUX		0x0040		/* select AMUX output func */
#define EBI_PEPAR_WE		0x0080		/* select WE* output function */
#define EBI_PEPAR_RAS1DD	0x0100		/* select RAS1DD* output func */
#define EBI_PEPAR_CF1MODE	0x0600		/* CONFIG1/BCLRO*/RAS2DD* */
#define EBI_PEPAR_SINTOUT	0x7000		/* slave interrupt out mode */

/* MEMC - Memory Controller */

/* MEMC Global Memory Register definitions */

#define MEMC_GMR_GMAX		0x00000020	/* global address mux enable */
#define MEMC_GMR_DW40		0x00000040	/* delay write for 68EC040 */
#define MEMC_GMR_DWQ		0x00000080	/* delay write for QUICC */
#define MEMC_GMR_NCS		0x00000100	/* suppress CS/RAS on CPU sp. */
#define MEMC_GMR_TSS40		0x00000200	/* TS* sample for 68EC040 */
#define MEMC_GMR_PBEE		0x00000400	/* parity bus error enable */
#define MEMC_GMR_OPAR		0x00000800	/* odd/even parity */
#define MEMC_GMR_EMWS		0x00001000	/* external master wait state */
#define MEMC_GMR_SYNC		0x00002000	/* synch external accesses */
#define MEMC_GMR_WBTQ		0x00004000	/* wait between QUICC trans */
#define MEMC_GMR_WBT40		0x00008000	/* wait between 68EC040 trans */
#define MEMC_GMR_DPS_32		0x00000000	/* DRAM port size is 32 bits */
#define MEMC_GMR_DPS_16		0x00010000	/* DRAM port size is 16 bits */
#define MEMC_GMR_DPS_EX		0x00030000	/* external DSACK* support */
#define MEMC_GMR_PGS_128K	0x00000000	/* page size is 128Kbytes */
#define MEMC_GMR_PGS_256K	0x00040000	/* page size is 256Kbytes */
#define MEMC_GMR_PGS_512K	0x00080000	/* page size is 512Kbytes */
#define MEMC_GMR_PGS_1M		0x000c0000	/* page size is 1Mbytes */
#define MEMC_GMR_PGS_2M		0x00100000	/* page size is 2Mbytes */
#define MEMC_GMR_PGS_4M		0x00140000	/* page size is 4Mbytes */
#define MEMC_GMR_PGS_8M		0x00180000	/* page size is 8Mbytes */
#define MEMC_GMR_PGS_16M	0x001c0000	/* page size is 16Mbytes */
#define MEMC_GMR_RCYC_4		0x00000000	/* refresh cycle 4 clocks */
#define MEMC_GMR_RCYC_6		0x00200000	/* refresh cycle 6 clocks */
#define MEMC_GMR_RCYC_7		0x00400000	/* refresh cycle 7 clocks */
#define MEMC_GMR_RCYC_8		0x00600000	/* refresh cycle 8 clocks */
#define MEMC_GMR_RFEN		0x00800000	/* refresh enable */
#define MEMC_GMR_RCNT		0xff000000	/* refresh counter period */

/* MEMC Status Register definitions */

#define MEMC_MSTAT_PER1		0x0001		/* write protect err - bank 1 */
#define MEMC_MSTAT_PER2		0x0002		/* write protect err - bank 2 */
#define MEMC_MSTAT_PER3		0x0004		/* write protect err - bank 3 */
#define MEMC_MSTAT_PER4		0x0008		/* write protect err - bank 4 */
#define MEMC_MSTAT_PER5		0x0010		/* write protect err - bank 5 */
#define MEMC_MSTAT_PER6		0x0020		/* write protect err - bank 6 */
#define MEMC_MSTAT_PER7		0x0040		/* write protect err - bank 7 */
#define MEMC_MSTAT_PER8		0x0080		/* write protect err - bank 8 */
#define MEMC_MSTAT_WPER		0x0100		/* write protect error */

/* MEMC Base Register definitions */

#define MEMC_BR_V		0x00000001	/* DRAM/SRAM bank is valid */
#define MEMC_BR_WP		0x00000002	/* write protect */
#define MEMC_BR_PAREN		0x00000004	/* parity checking enable */
#define MEMC_BR_CSNTQ		0x00000008	/* CS negate timing - QUICC */
#define MEMC_BR_CSNT40		0x00000010	/* CS negate timing - 68EC040 */
#define MEMC_BR_BACK40		0x00000020	/* burst acknowledge 68EC040 */
#define MEMC_BR_TRLXQ		0x00000040	/* timing relax */
#define MEMC_BR_FC		0x00000780	/* function code spec */
#define MEMC_BR_BA		0xfffff800	/* base address spec */

/* MEMC Option Register definitions */

#define MEMC_OR_DSSEL		0x00000001	/* DRAM/SRAM select */
#define MEMC_OR_SPS_32		0x00000000	/* SRAM port size is 32 bits */
#define MEMC_OR_SPS_16		0x00000002	/* SRAM port size is 16 bits */
#define MEMC_OR_SPS_8		0x00000004	/* SRAM port size is 8 bits */
#define MEMC_OR_SPS_EX		0x00000006	/* external DSACK* support */
#define MEMC_OR_PGME		0x00000008	/* page mode enable (DRAM) */
#define MEMC_OR_BCYC_1		0x00000000	/* burst cycle 1 clock */
#define MEMC_OR_BCYC_2		0x00000020	/* burst cycle 2 clock */
#define MEMC_OR_BCYC_3		0x00000040	/* burst cycle 3 clock */
#define MEMC_OR_BCYC_4		0x00000060	/* burst cycle 4 clock */
#define MEMC_OR_FC		0x00000780	/* function code mask */
#define MEMC_OR_AM		0x0ffff800	/* addresss mask */
#define MEMC_OR_TCYC		0xf0000000	/* bus cycle length in clocks */

/*** CPM - register definitions for the Communication Processor Module ***/

/* CPM - Communication Processor Module */

/* CPM Configuration Register definitions */

#define CPM_CR_FLG		0x0001		/* flag - command executing */
#define CPM_CR_CHANNEL_SCC1	0x0000		/* SCC1 channel */
#define CPM_CR_CHANNEL_SCC2	0x0040		/* SCC2 channel */
#define CPM_CR_CHANNEL_SPI	0x0050		/* SPI channel */
#define CPM_CR_CHANNEL_RTMR	0x0050		/* RISC timer channel */
#define CPM_CR_CHANNEL_SCC3	0x0080		/* SCC3 channel */
#define CPM_CR_CHANNEL_SMC1	0x0090		/* SMC1 channel */
#define CPM_CR_CHANNEL_IDMA1	0x0090		/* IDMA1 channel */
#define CPM_CR_CHANNEL_SCC4	0x00c0		/* SCC4 channel */
#define CPM_CR_CHANNEL_SMC2	0x00d0		/* SMC2 channel */
#define CPM_CR_CHANNEL_IDMA2	0x00d0		/* IDMA2 channel */
#define CPM_CR_OPCODE		0x0f00		/* command opcode */

#define CPM_CR_SCC_INIT_RT	0x0000		/* initialize SCC rx and tx*/
#define CPM_CR_SCC_INIT_R	0x0100		/* initialize SCC rx only */
#define CPM_CR_SCC_INIT_T	0x0200		/* initialize SCC tx only */
#define CPM_CR_SCC_HUNT		0x0300		/* enter rx frame hunt mode */
#define CPM_CR_SCC_STOP		0x0400		/* stop SCC tx */
#define CPM_CR_SCC_GRSTOP	0x0500		/* gracefully stop SCC tx */
#define CPM_CR_SCC_RESTART	0x0600		/* restart SCC tx */
#define CPM_CR_SCC_CLOSE	0x0700		/* close SCC rx buffer */
#define CPM_CR_SCC_SET_GROUP	0x0800		/* set SCC group address */
#define CPM_CR_SCC_RESET_BCS	0x0a00		/* reset bisync seq calc */

#define CPM_CR_RST		0x8000		/* software reset command */

/* CPIC - CPM Interrupt Controller */

/* CPM Interrupt Configuration Register definitions */

#define CPIC_CICR_SPS		0x00000001	/* spread SCC priority scheme */
#define CPIC_CICR_VBA		0x000000e0	/* vector base address */
#define CPIC_CICR_HP		0x00001f00	/* highest priority source */
#define CPIC_CICR_IRL		0x0000e000	/* interrupt request level */
#define CPIC_CICR_SCaP_1	0x00000000	/* SCCa will be used by SCC1 */
#define CPIC_CICR_SCaP_2	0x00010000	/* SCCa will be used by SCC2 */
#define CPIC_CICR_SCaP_3	0x00020000	/* SCCa will be used by SCC3 */
#define CPIC_CICR_SCaP_4	0x00030000	/* SCCa will be used by SCC4 */
#define CPIC_CICR_SCbP_1	0x00000000	/* SCCb will be used by SCC1 */
#define CPIC_CICR_SCbP_2	0x00040000	/* SCCb will be used by SCC2 */
#define CPIC_CICR_SCbP_3	0x00080000	/* SCCb will be used by SCC3 */
#define CPIC_CICR_SCbP_4	0x000c0000	/* SCCb will be used by SCC4 */
#define CPIC_CICR_SCcP_1	0x00000000	/* SCCc will be used by SCC1 */
#define CPIC_CICR_SCcP_2	0x00100000	/* SCCc will be used by SCC2 */
#define CPIC_CICR_SCcP_3	0x00200000	/* SCCc will be used by SCC3 */
#define CPIC_CICR_SCcP_4	0x00300000	/* SCCc will be used by SCC4 */
#define CPIC_CICR_SCdP_1	0x00000000	/* SCCd will be used by SCC1 */
#define CPIC_CICR_SCdP_2	0x00400000	/* SCCd will be used by SCC2 */
#define CPIC_CICR_SCdP_3	0x00800000	/* SCCd will be used by SCC3 */
#define CPIC_CICR_SCdP_4	0x00c00000	/* SCCd will be used by SCC4 */

/* CPM Interrupt Pending, Mask, and In-Serivice Register definitions */

#define CPIC_CIXR_PC11		0x00000002	/* PC11 interrupt source */
#define CPIC_CIXR_PC10		0x00000004	/* PC10 interrupt source */
#define CPIC_CIXR_SMC2		0x00000008	/* SMC2 interrupt source */
#define CPIC_CIXR_SMC1		0x00000010	/* SMC1 interrupt source */
#define CPIC_CIXR_SPI		0x00000020	/* SPI interrupt source */
#define CPIC_CIXR_PC9		0x00000040	/* PC9 interrupt source */
#define CPIC_CIXR_TMR4		0x00000080	/* TMR4 interrupt source */
#define CPIC_CIXR_PC8		0x00000200	/* PC8 interrupt source */
#define CPIC_CIXR_PC7		0x00000400	/* PC7 interrupt source */
#define CPIC_CIXR_PC6		0x00000800	/* PC6 interrupt source */
#define CPIC_CIXR_TMR3		0x00001000	/* TMR3 interrupt source */
#define CPIC_CIXR_PC5		0x00004000	/* PC5 interrupt source */
#define CPIC_CIXR_PC4		0x00008000	/* PC4 interrupt source */
#define CPIC_CIXR_RTT		0x00020000	/* RTT interrupt source */
#define CPIC_CIXR_TMR2		0x00040000	/* TMR2 interrupt source */
#define CPIC_CIXR_IDMA2		0x00100000	/* IDMA2 interrupt source */
#define CPIC_CIXR_IDMA1		0x00200000	/* IDMA1 interrupt source */
#define CPIC_CIXR_SDMA		0x00400000	/* SDMA interrupt source */
#define CPIC_CIXR_PC3		0x00800000	/* PC3 interrupt source */
#define CPIC_CIXR_PC2		0x01000000	/* PC2 interrupt source */
#define CPIC_CIXR_TMR1		0x02000000	/* TMR1 interrupt source */
#define CPIC_CIXR_PC1		0x04000000	/* PC1 interrupt source */
#define CPIC_CIXR_SCC4		0x08000000	/* SCC4 interrupt source */
#define CPIC_CIXR_SCC3		0x10000000	/* SCC3 interrupt source */
#define CPIC_CIXR_SCC2		0x20000000	/* SCC2 interrupt source */
#define CPIC_CIXR_SCC1		0x40000000	/* SCC1 interrupt source */
#define CPIC_CIXR_PC0		0x80000000	/* PC0 interrupt source */

/* CPM Based Interrupt Vectors offsets */

#define INT_OFF_ERR		0x0
#define INT_OFF_PC11		0x1
#define INT_OFF_PC10		0x2
#define INT_OFF_SMC2		0x3
#define INT_OFF_SMC1		0x4
#define INT_OFF_SPI		0x5
#define INT_OFF_PC9		0x6
#define INT_OFF_TMR4		0x7
#define INT_OFF_RES1		0x8
#define INT_OFF_PC8		0x9
#define INT_OFF_PC7		0xa
#define INT_OFF_PC6		0xb
#define INT_OFF_TMR3		0xc
#define INT_OFF_RES2		0xd
#define INT_OFF_PC5		0xe
#define INT_OFF_PC4		0xf
#define INT_OFF_RES3		0x10
#define INT_OFF_RISC		0x11
#define INT_OFF_TMR2		0x12
#define INT_OFF_RES4		0x13
#define INT_OFF_IDMA2		0x14
#define INT_OFF_IDMA1		0x15
#define INT_OFF_SDMA		0x16
#define INT_OFF_PC3		0x17
#define INT_OFF_PC2		0x18
#define INT_OFF_TMR1		0x19
#define INT_OFF_PC1		0x1a
#define INT_OFF_SCC4		0x1b
#define INT_OFF_SCC3		0x1c
#define INT_OFF_SCC2		0x1d
#define INT_OFF_SCC1		0x1e
#define INT_OFF_PC0		0x1f

/* CPM Based Interrupt Vectors addresses */

#define INT_CPM_BASE(base)	(*M360_CPM_CICR(base) & CPIC_CICR_VBA)

#define INT_VEC_ERR(base)	(INT_OFF_ERR   | INT_CPM_BASE(base))
#define INT_VEC_PC11(base)	(INT_OFF_PC11  | INT_CPM_BASE(base))
#define INT_VEC_PC10(base)	(INT_OFF_PC10  | INT_CPM_BASE(base))
#define INT_VEC_SMC2(base)	(INT_OFF_SMC2  | INT_CPM_BASE(base))
#define INT_VEC_SMC1(base)	(INT_OFF_SMC1  | INT_CPM_BASE(base))
#define INT_VEC_SPI(base)	(INT_OFF_SPI   | INT_CPM_BASE(base))
#define INT_VEC_PC9(base)	(INT_OFF_PC9   | INT_CPM_BASE(base))
#define INT_VEC_TMR4(base)	(INT_OFF_TMR4  | INT_CPM_BASE(base))
#define INT_VEC_RES1(base)	(INT_OFF_RES1  | INT_CPM_BASE(base))
#define INT_VEC_PC8(base)	(INT_OFF_PC8   | INT_CPM_BASE(base))
#define INT_VEC_PC7(base)	(INT_OFF_PC7   | INT_CPM_BASE(base))
#define INT_VEC_PC6(base)	(INT_OFF_PC6   | INT_CPM_BASE(base))
#define INT_VEC_TMR3(base)	(INT_OFF_TMR3  | INT_CPM_BASE(base))
#define INT_VEC_RES2(base)	(INT_OFF_RES2  | INT_CPM_BASE(base))
#define INT_VEC_PC5(base)	(INT_OFF_PC5   | INT_CPM_BASE(base))
#define INT_VEC_PC4(base)	(INT_OFF_PC4   | INT_CPM_BASE(base))
#define INT_VEC_RES3(base)	(INT_OFF_RES3  | INT_CPM_BASE(base))
#define INT_VEC_RISC(base)	(INT_OFF_RISC  | INT_CPM_BASE(base))
#define INT_VEC_TMR2(base)	(INT_OFF_TMR2  | INT_CPM_BASE(base))
#define INT_VEC_RES4(base)	(INT_OFF_RES4  | INT_CPM_BASE(base))
#define INT_VEC_IDMA2(base)	(INT_OFF_IDMA2 | INT_CPM_BASE(base))
#define INT_VEC_IDMA1(base)	(INT_OFF_IDMA1 | INT_CPM_BASE(base))
#define INT_VEC_SDMA(base)	(INT_OFF_SDMA  | INT_CPM_BASE(base))
#define INT_VEC_PC3(base)	(INT_OFF_PC3   | INT_CPM_BASE(base))
#define INT_VEC_PC2(base)	(INT_OFF_PC2   | INT_CPM_BASE(base))
#define INT_VEC_TMR1(base)	(INT_OFF_TMR1  | INT_CPM_BASE(base))
#define INT_VEC_PC1(base)	(INT_OFF_PC1   | INT_CPM_BASE(base))
#define INT_VEC_SCC4(base)	(INT_OFF_SCC4  | INT_CPM_BASE(base))
#define INT_VEC_SCC3(base)	(INT_OFF_SCC3  | INT_CPM_BASE(base))
#define INT_VEC_SCC2(base)	(INT_OFF_SCC2  | INT_CPM_BASE(base))
#define INT_VEC_SCC1(base)	(INT_OFF_SCC1  | INT_CPM_BASE(base))
#define INT_VEC_PC0(base)	(INT_OFF_PC0   | INT_CPM_BASE(base))

/* IDMA - Independent Direct Memory Access */
 
/* IDMA Configuration Register definitions */

#define IDMA_ICCR_IAID		0x0070		/* IDMA arbitration ID */
#define IDMA_ICCR_ISM		0x0700		/* interrupt service mask */
#define IDMA_ICCR_ARBP_1	0x0000		/* ch 1 priority over ch 2 */
#define IDMA_ICCR_ARBP_2	0x1000		/* ch 2 priority over ch 1 */
#define IDMA_ICCR_ARBP_ROT	0x2000		/* rotating priority of IDMAs */
#define IDMA_ICCR_FRZ_DIS	0x0000		/* ignore the FREEZE signal */
#define IDMA_ICCR_FRZ_EN	0x4000		/* freeze on FREEZE signal */
#define IDMA_ICCR_STP		0x8000		/* stop sys clk to IDMA chnls */

/* SDMA - Serial Direct Memory Access */
 
/* SDMA Configuration Register definitions */

#define SDMA_SDCR_INTB		0x0001		/* breakpoint interrupt mask */
#define SDMA_SDCR_INTE		0x0002		/* bus error interrupt mask */
#define SDMA_SDCR_INTR		0x0004		/* reserved interrupt mask */
#define SDMA_SDCR_SAID		0x0070		/* SDMA arbitration ID */
#define SDMA_SDCR_SISM		0x0700		/* interrupt service mask */
#define SDMA_SDCR_FRZ_DIS	0x0000		/* ignore the FREEZE signal */
#define SDMA_SDCR_FRZ_EN	0x4000		/* freeze on FREEZE signal */

/* TMR - Timers */

/* TMR Global Configuration Register definitions */

#define TMR_TGCR_RST1		0x0001		/* reset/enable timer 1 */
#define TMR_TGCR_STP1		0x0002		/* stop timer 1 */
#define TMR_TGCR_FRZ1		0x0004		/* halt timer 1 on FREEZE */
#define TMR_TGCR_GM1		0x0008		/* normal gate mode on TGATE1 */
#define TMR_TGCR_RST2		0x0010		/* reset/enable timer 2 */
#define TMR_TGCR_STP2		0x0020		/* stop timer 2 */
#define TMR_TGCR_FRZ2		0x0040		/* halt timer 2 on FREEZE */
#define TMR_TGCR_CAS2		0x0080		/* cascade timers 1 and 2 */
#define TMR_TGCR_RST3		0x0100		/* reset/enable timer 3 */
#define TMR_TGCR_STP3		0x0200		/* stop timer 3 */
#define TMR_TGCR_FRZ3		0x0400		/* halt timer 3 on FREEZE */
#define TMR_TGCR_GM2		0x0800		/* normal gate mode on TGATE2 */
#define TMR_TGCR_RST4		0x1000		/* reset/enable timer 4 */
#define TMR_TGCR_STP4		0x2000		/* stop timer 4 */
#define TMR_TGCR_FRZ4		0x4000		/* halt timer 4 on FREEZE */
#define TMR_TGCR_CAS4		0x8000		/* cascade timers 3 and 4 */

/* TMR Mode Register definitions */

#define TMR_TMR_GE		0x0001		/* enable TGATE signal */
#define TMR_TMR_ICLK_CAS	0x0000		/* input is output of timer */
#define TMR_TMR_ICLK_CLK	0x0002		/* input is general sys clock */
#define TMR_TMR_ICLK_CLK16	0x0004		/* input is sys clock div 16 */
#define TMR_TMR_ICLK_TIN	0x0006		/* input is TIN pin */
#define TMR_TMR_FRR		0x0008		/* free run/restart timer */
#define TMR_TMR_ORI		0x0010		/* output interrupt enable */
#define TMR_TMR_OM		0x0020		/* output mode */
#define TMR_TMR_CE		0x00c0		/* capture edge for TIN input */
#define TMR_TMR_PS		0xff00		/* prescaler value */

/* TMR Event Register definitions */

#define TMR_TER_CAP		0x0001		/* capture event occurred */
#define TMR_TER_REF		0x0002		/* reference event occurred */

/* SI - Serial Interface */

/* SI Mode Register definitions */

#define SI_SIMODE_TFSDa		0x00000003	/* transmit frame sync delay */
#define SI_SIMODE_GMa		0x00000004	/* grant mode TDM a */
#define SI_SIMODE_FEa		0x00000008	/* frame sync edge TDM a */
#define SI_SIMODE_CEa		0x00000010	/* clock edge TDM a */
#define SI_SIMODE_STZa		0x00000020	/* set L1TXDa to zero TDM a */
#define SI_SIMODE_CRTa		0x00000040	/* common Rx/Tx pins TDM a */
#define SI_SIMODE_DSCa		0x00000080	/* double speed clk TDM a */
#define SI_SIMODE_RFSDa		0x00000300	/* receive frame sync delay */
#define SI_SIMODE_SDMa		0x00000c00	/* SI diagnostic mode TDM a */
#define SI_SIMODE_SMC1CS	0x00007000	/* SMC1 clk source (NMSI) */
#define SI_SIMODE_SMC1		0x00008000	/* SMC1 connection (NMSI) */
#define SI_SIMODE_TFSDb		0x00030000	/* transmit frame sync delay */
#define SI_SIMODE_GMb		0x00040000	/* grant mode TDM b */
#define SI_SIMODE_FEb		0x00080000	/* frame sync edge TDM b */
#define SI_SIMODE_CEb		0x00100000	/* clock edge TDM b */
#define SI_SIMODE_STZb		0x00200000	/* set L1TXDb to zero TDM b */
#define SI_SIMODE_CRTb		0x00400000	/* common Rx/Tx pins TDM a */
#define SI_SIMODE_DSCb		0x00800000	/* double speed clock TDM b */
#define SI_SIMODE_RFSDb		0x03000000	/* receive frame sync delay */
#define SI_SIMODE_SDMb		0x0c000000	/* SI diagnostic mode TDM b */
#define SI_SIMODE_SMC2CS	0x70000000	/* SMC2 clk source (NMSI) */
#define SI_SIMODE_SMC2		0x80000000	/* SMC2 connection (NMSI) */

/* SI Clock Route Register definitions */

#define SI_SICR_TCS		0x07		/* transmit clock source SCC */
#define SI_SICR_RCS		0x38		/* receive clock source SCC */
#define SI_SICR_SC		0x40		/* SCC connection (NMSI) */
#define SI_SICR_GR		0x80		/* grant support by SCC */

/* BRG - Baud Rate Generator */

/* BRG Configuration Register definitions */

#define BRG_CR_DIV16		0x00001		/* BRG clock prescale div 16 */
#define BRG_CR_CD		0x01ffe		/* clock divider */
#define BRG_CR_ATB		0x02000		/* autobaud support */
#define BRG_CR_EXT_BRGCLK	0x00000		/* external clk source BRGCLK */
#define BRG_CR_EXT_CLK2		0x04000		/* external clk source CLK2 */
#define BRG_CR_EXT_CLK6		0x08000		/* external clk source CLK6 */
#define BRG_CR_EN		0x10000		/* enable BRG count */
#define BRG_CR_RST		0x20000		/* perform reset on BRG */

/* SCC - Serial Communication Controller */

/* General SCC Mode Register definitions */

#define SCC_GSMRL_HDLC		0x00000000	/* HDLC mode */
#define SCC_GSMRL_APPLETALK	0x00000002	/* AppleTalk mode (LocalTalk) */
#define SCC_GSMRL_SS7		0x00000003	/* SS7 mode (microcode) */
#define SCC_GSMRL_UART		0x00000004	/* UART mode */
#define SCC_GSMRL_PROFI_BUS	0x00000005	/* Profi-Bus mode (microcode) */
#define SCC_GSMRL_ASYNC_HDLC	0x00000006	/* async HDLC mode (microcode)*/
#define SCC_GSMRL_V14		0x00000007	/* V.14 mode */
#define SCC_GSMRL_BISYNC	0x00000008	/* BISYNC mode */
#define SCC_GSMRL_DDCMP		0x00000009	/* DDCMP mode (microcode) */
#define SCC_GSMRL_ETHERNET	0x0000000c	/* ethernet mode (SCC1 only) */
#define SCC_GSMRL_ENT		0x00000010	/* enable transmitter */
#define SCC_GSMRL_ENR		0x00000020	/* enable receiver */
#define SCC_GSMRL_LOOPBACK	0x00000040	/* local loopback mode */
#define SCC_GSMRL_ECHO		0x00000080	/* automatic echo mode */
#define SCC_GSMRL_TENC		0x00000700	/* transmitter encoding method*/
#define SCC_GSMRL_RENC		0x00003800	/* receiver encoding method */
#define SCC_GSMRL_RDCR_X8	0x00004000	/* receive DPLL clock x8 */
#define SCC_GSMRL_RDCR_X16	0x00008000	/* receive DPLL clock x16 */
#define SCC_GSMRL_RDCR_X32	0x0000c000	/* receive DPLL clock x32 */
#define SCC_GSMRL_TDCR_X8	0x00010000	/* transmit DPLL clock x8 */
#define SCC_GSMRL_TDCR_X16	0x00020000	/* transmit DPLL clock x16 */
#define SCC_GSMRL_TDCR_X32	0x00030000	/* transmit DPLL clock x32 */
#define SCC_GSMRL_TEND		0x00040000	/* transmitter frame ending */
#define SCC_GSMRL_TPP_00	0x00180000	/* Tx preamble pattern = 00 */
#define SCC_GSMRL_TPP_10	0x00080000	/* Tx preamble pattern = 10 */
#define SCC_GSMRL_TPP_01	0x00100000	/* Tx preamble pattern = 01 */
#define SCC_GSMRL_TPP_11	0x00180000	/* Tx preamble pattern = 11 */
#define SCC_GSMRL_TPL_NONE	0x00000000	/* no Tx preamble (default) */
#define SCC_GSMRL_TPL_8		0x00200000	/* Tx preamble = 1 byte */
#define SCC_GSMRL_TPL_16	0x00400000	/* Tx preamble = 2 bytes */
#define SCC_GSMRL_TPL_32	0x00600000	/* Tx preamble = 4 bytes */
#define SCC_GSMRL_TPL_48	0x00800000	/* Tx preamble = 6 bytes */
#define SCC_GSMRL_TPL_64	0x00a00000	/* Tx preamble = 8 bytes */
#define SCC_GSMRL_TPL_128	0x00c00000	/* Tx preamble = 16 bytes */
#define SCC_GSMRL_TINV		0x01000000	/* DPLL transmit input invert */
#define SCC_GSMRL_RINV		0x02000000	/* DPLL receive input invert */
#define SCC_GSMRL_TSNC		0x0c000000	/* transmit sense */
#define SCC_GSMRL_TCI		0x10000000	/* transmit clock invert */
#define SCC_GSMRL_EDGE		0x60000000	/* adjustment edge +/- */

#define SCC_GSMRH_RSYN		0x00000001	/* receive sync timing*/
#define SCC_GSMRH_RTSM		0x00000002	/* RTS* mode */
#define SCC_GSMRH_SYNL		0x0000000c	/* sync length */
#define SCC_GSMRH_TXSY		0x00000010	/* transmitter/receiver sync */
#define SCC_GSMRH_RFW		0x00000020	/* Rx FIFO width */
#define SCC_GSMRH_TFL		0x00000040	/* transmit FIFO length */
#define SCC_GSMRH_CTSS		0x00000080	/* CTS* sampling */
#define SCC_GSMRH_CDS		0x00000100	/* CD* sampling */
#define SCC_GSMRH_CTSP		0x00000200	/* CTS* pulse */
#define SCC_GSMRH_CDP		0x00000400	/* CD* pulse */
#define SCC_GSMRH_TTX		0x00000800	/* transparent transmitter */
#define SCC_GSMRH_TRX		0x00001000	/* transparent receiver */
#define SCC_GSMRH_REVD		0x00002000	/* reverse data */
#define SCC_GSMRH_TCRC		0x0000c000	/* transparent CRC */
#define SCC_GSMRH_GDE		0x00010000	/* glitch detect enable */

/* SCC UART protocol specific parameters */

typedef struct		/* SCC_UART_PROTO */
    {
    UINT32	res1;			/* reserved */
    UINT32	res2;			/* reserved */
    UINT16	maxIdl;			/* maximum idle characters */
    UINT16	idlc;			/* temporary idle counter */
    UINT16	brkcr;			/* break count register (transmit) */
    UINT16	parec;			/* receive parity error counter */
    UINT16	frmer;			/* receive framing error counter */
    UINT16	nosec;			/* receive noise counter */
    UINT16	brkec;			/* receive break condition counter */
    UINT16	brkln;			/* last received break length */
    UINT16	uaddr1;			/* uart address character 1 */
    UINT16	uaddr2;			/* uart address character 2 */
    UINT16	rtemp;			/* temp storage */
    UINT16	toseq;			/* transmit out-of-sequence character */
    UINT16	character1;		/* control character 1 */
    UINT16	character2;		/* control character 2 */
    UINT16	character3;		/* control character 3 */
    UINT16	character4;		/* control character 4 */
    UINT16	character5;		/* control character 5 */
    UINT16	character6;		/* control character 6 */
    UINT16	character7;		/* control character 7 */
    UINT16	character8;		/* control character 8 */
    UINT16	rccm;			/* receive control character mask */
    UINT16	rccr;			/* receive control character register */
    UINT16	rlbc;			/* receive last break character */
    } SCC_UART_PROTO;

/* SCC UART Protocol Specific Mode Register definitions */

#define SCC_UART_PSMR_TPM_ODD	0x0000		/* odd parity mode (Tx) */
#define SCC_UART_PSMR_TPM_LOW	0x0001		/* low parity mode (Tx) */
#define SCC_UART_PSMR_TPM_EVEN	0x0002		/* even parity mode (Tx) */
#define SCC_UART_PSMR_TPM_HIGH	0x0003		/* high parity mode (Tx) */
#define SCC_UART_PSMR_RPM_ODD	0x0000		/* odd parity mode (Rx) */
#define SCC_UART_PSMR_RPM_LOW	0x0004		/* low parity mode (Rx) */
#define SCC_UART_PSMR_RPM_EVEN	0x0008		/* even parity mode (Rx) */
#define SCC_UART_PSMR_RPM_HIGH	0x000c		/* high parity mode (Rx) */
#define SCC_UART_PSMR_PEN	0x0010		/* parity enable */
#define SCC_UART_PSMR_DRT	0x0040		/* disable Rx while Tx */
#define SCC_UART_PSMR_SYN	0x0080		/* synchronous mode */
#define SCC_UART_PSMR_RZS	0x0100		/* receive zero stop bits */
#define SCC_UART_PSMR_FRZ	0x0200		/* freeze transmission */
#define SCC_UART_PSMR_UM_NML	0x0000		/* noraml UART operation */
#define SCC_UART_PSMR_UM_MULT_M	0x0400		/* multidrop non-auto mode */
#define SCC_UART_PSMR_UM_MULT_A	0x0c00		/* multidrop automatic mode */
#define SCC_UART_PSMR_CL_5BIT	0x0000		/* 5 bit character length */
#define SCC_UART_PSMR_CL_6BIT	0x1000		/* 6 bit character length */
#define SCC_UART_PSMR_CL_7BIT	0x2000		/* 7 bit character length */
#define SCC_UART_PSMR_CL_8BIT	0x3000		/* 8 bit character length */
#define SCC_UART_PSMR_SL	0x4000		/* 1 or 2 bit stop length */
#define SCC_UART_PSMR_FLC	0x8000		/* flow control */
	
/* SCC UART Event and Mask Register definitions */

#define SCC_UART_SCCX_RX 	0x0001		/* buffer received */
#define SCC_UART_SCCX_TX 	0x0002		/* buffer transmitted */
#define SCC_UART_SCCX_BSY 	0x0004		/* busy condition */
#define SCC_UART_SCCX_CCR 	0x0008		/* control character received */
#define SCC_UART_SCCX_BRK_S 	0x0020	 	/* break start */
#define SCC_UART_SCCX_BRK_E 	0x0040		/* break end */
#define SCC_UART_SCCX_GRA 	0x0080		/* graceful stop complete */
#define SCC_UART_SCCX_IDL	0x0100		/* idle sequence stat changed */
#define SCC_UART_SCCX_AB  	0x0200		/* autobaud lock */
#define SCC_UART_SCCX_GL_T 	0x0800		/* glitch on Tx */
#define SCC_UART_SCCX_GL_R 	0x1000		/* glitch on Rx */

/* SCC UART Receive Buffer Descriptor definitions */

#define SCC_UART_RX_BD_CD	0x0001		/* carrier detect loss */
#define SCC_UART_RX_BD_OV	0x0002		/* receiver overrun */
#define SCC_UART_RX_BD_PR	0x0008		/* parity error */
#define SCC_UART_RX_BD_FR	0x0010		/* framing error */
#define SCC_UART_RX_BD_BR	0x0020		/* break received */
#define SCC_UART_RX_BD_AM	0x0080		/* address match */
#define SCC_UART_RX_BD_ID	0x0100		/* buf closed on IDLES */
#define SCC_UART_RX_BD_CM	0x0200		/* continous mode */
#define SCC_UART_RX_BD_ADDR	0x0400		/* buffer contains address */
#define SCC_UART_RX_BD_CNT	0x0800		/* control character */
#define SCC_UART_RX_BD_INT	0x1000		/* interrupt generated */
#define SCC_UART_RX_BD_WRAP	0x2000		/* wrap back to first BD */
#define SCC_UART_RX_BD_EMPTY	0x8000		/* buffer is empty */

/* SCC UART Transmit Buffer Descriptor definitions */

#define SCC_UART_TX_BD_CT	0x0001		/* cts was lost during tx */
#define SCC_UART_TX_BD_NS	0x0080		/* no stop bit transmitted */
#define SCC_UART_TX_BD_PREAMBLE	0x0100		/* enable preamble */
#define SCC_UART_TX_BD_CM	0x0200		/* continous mode */
#define SCC_UART_TX_BD_ADDR	0x0400		/* buffer contains address */
#define SCC_UART_TX_BD_CTSR	0x0800		/* normal cts error reporting */
#define SCC_UART_TX_BD_INT	0x1000		/* interrupt generated */
#define SCC_UART_TX_BD_WRAP	0x2000		/* wrap back to first BD */
#define SCC_UART_TX_BD_READY	0x8000		/* buffer is being sent */

/* SCC Ethernet protocol specific parameters */

typedef struct		/* SCC_ETHER_PROTO */
    {
    UINT32	c_pres;			/* preset CRC */
    UINT32	c_mask;			/* constant mask for CRC */
    UINT32	crcec;			/* CRC error counter */
    UINT32	alec;			/* alignment error counter */
    UINT32	disfc;			/* discard frame counter */
    UINT16	pads;			/* short frame pad value */
    UINT16	ret_lim;		/* retry limit threshold */
    UINT16	ret_cnt;		/* retry limit counter */
    UINT16	mflr;			/* maximum frame length register */
    UINT16	minflr;			/* minimum frame length register */
    UINT16	maxd1;			/* max DMA1 length register */
    UINT16	maxd2;			/* max DMA2 length register */
    UINT16	maxd;			/* Rx max DMA */
    UINT16	dma_cnt;		/* Rx DMA counter */
    UINT16	max_b;			/* max BD byte count */
    UINT16	gaddr1;			/* group address filter 1 */
    UINT16	gaddr2;			/* group address filter 2 */
    UINT16	gaddr3;			/* group address filter 3 */
    UINT16	gaddr4;			/* group address filter 4 */
    UINT32	tbuf0_data0;		/* save area 0 - current frame */
    UINT32	tbuf0_data1;		/* save area 1 - current frame */
    UINT32	tbuf0_rba0;		/* ? */
    UINT32	tbuf0_crc;		/* ? */
    UINT16	tbuf0_bcnt;		/* ? */
    UINT16	paddr1_h;		/* physical address 1 (MSB) */
    UINT16	paddr1_m;		/* physical address 1 */
    UINT16	paddr1_l;		/* physical address 1 (LSB) */
    UINT16	p_per;			/* persistence */
    UINT16	rfbd_ptr;		/* Rx first BD pointer */
    UINT16	tfbd_ptr;		/* Tx first BD pointer */
    UINT16	tlbd_ptr;		/* Tx last BD pointer */
    UINT32	tbuf1_data0;		/* save area 0 - next frame */
    UINT32	tbuf1_data1;		/* ? */
    UINT32	tbuf1_rba0;		/* ? */
    UINT32	tbuf1_crc;		/* ? */
    UINT16	tbuf1_bcnt;		/* ? */
    UINT16	tx_len;			/* Tx frame length counter */
    UINT16	iaddr1;			/* individual address filter 1 */
    UINT16	iaddr2;			/* individual address filter 2 */
    UINT16	iaddr3;			/* individual address filter 3 */
    UINT16	iaddr4;			/* individual address filter 4 */
    UINT16	boff_cnt;		/* backoff counter */
    UINT16	taddr_h;		/* temp address (MSB) */
    UINT16	taddr_m;		/* temp address */
    UINT16	taddr_l;		/* temp address (LSB) */
    } SCC_ETHER_PROTO;

/* SCC Ethernet Protocol Specific Mode Register definitions */

#define SCC_ETHER_PSMR_NIB_13	0x0000		/* SFD 13 bits after TENA */
#define SCC_ETHER_PSMR_NIB_14	0x0002		/* SFD 14 bits after TENA */
#define SCC_ETHER_PSMR_NIB_15	0x0004		/* SFD 15 bits after TENA */
#define SCC_ETHER_PSMR_NIB_16	0x0006		/* SFD 16 bits after TENA */
#define SCC_ETHER_PSMR_NIB_21	0x0008		/* SFD 21 bits after TENA */
#define SCC_ETHER_PSMR_NIB_22	0x000a		/* SFD 22 bits after TENA */
#define SCC_ETHER_PSMR_NIB_23	0x000c		/* SFD 23 bits after TENA */
#define SCC_ETHER_PSMR_NIB_24	0x000e		/* SFD 24 bits after TENA */
#define SCC_ETHER_PSMR_LCW	0x0100		/* late collision window */
#define SCC_ETHER_PSMR_SIP	0x0200		/* sample input pins */
#define SCC_ETHER_PSMR_LPB	0x0400		/* loopback operation */
#define SCC_ETHER_PSMR_SBT	0x0800		/* stop backoff timer */
#define SCC_ETHER_PSMR_BRO	0x0100		/* broadcast address */
#define SCC_ETHER_PSMR_PRO	0x0200		/* promiscuous mode */
#define SCC_ETHER_PSMR_CRC	0x0800		/* CRC selection */
#define SCC_ETHER_PSMR_IAM	0x1000		/* individual address mode */
#define SCC_ETHER_PSMR_RSH	0x2000		/* receive short frame */
#define SCC_ETHER_PSMR_FC	0x4000		/* force collision */
#define SCC_ETHER_PSMR_HBC	0x8000		/* heartbeat checking*/

/* SCC Ethernet Event and Mask Register definitions */

#define SCC_ETHER_SCCX_RXB 	0x0001		/* buffer received event */
#define SCC_ETHER_SCCX_TXB 	0x0002		/* buffer transmitted event */
#define SCC_ETHER_SCCX_BSY 	0x0004		/* busy condition */
#define SCC_ETHER_SCCX_RXF 	0x0008		/* frame received event */
#define SCC_ETHER_SCCX_TXE	0x0010		/* transmission error event */
#define SCC_ETHER_SCCX_GRA 	0x0080		/* graceful stop event */

/* SCC Ethernet Receive Buffer Descriptor definitions */

#define SCC_ETHER_RX_BD_CL	0x0001		/* collision condition */
#define SCC_ETHER_RX_BD_OV	0x0002		/* overrun condition */
#define SCC_ETHER_RX_BD_CR	0x0004		/* Rx CRC error */
#define SCC_ETHER_RX_BD_SH	0x0008		/* short frame received */
#define SCC_ETHER_RX_BD_NO	0x0010		/* Rx nonoctet aligned frame */
#define SCC_ETHER_RX_BD_LG	0x0020		/* Rx frame length violation */
#define SCC_ETHER_RX_BD_M	0x0100		/* miss bit for prom mode */
#define SCC_ETHER_RX_BD_F	0x0400		/* buffer is first in frame */
#define SCC_ETHER_RX_BD_L	0x0800		/* buffer is last in frame */
#define SCC_ETHER_RX_BD_I	0x1000		/* interrupt on receive */
#define SCC_ETHER_RX_BD_W	0x2000		/* last BD in ring */
#define SCC_ETHER_RX_BD_E	0x8000		/* buffer is empty */

/* SCC Ethernet Transmit Buffer Descriptor definitions */

#define SCC_ETHER_TX_BD_CSL	0x0001		/* carrier sense lost */
#define SCC_ETHER_TX_BD_UN	0x0002		/* underrun */
#define SCC_ETHER_TX_BD_RC	0x003c		/* retry count */
#define SCC_ETHER_TX_BD_RL	0x0040		/* retransmission limit */
#define SCC_ETHER_TX_BD_LC	0x0080		/* late collision */
#define SCC_ETHER_TX_BD_HB	0x0100		/* heartbeat */
#define SCC_ETHER_TX_BD_DEF	0x0200		/* defer indication */
#define SCC_ETHER_TX_BD_TC	0x0400		/* auto transmit CRC */
#define SCC_ETHER_TX_BD_L	0x0800		/* buffer is last in frame */
#define SCC_ETHER_TX_BD_I	0x1000		/* interrupt on transmit */
#define SCC_ETHER_TX_BD_W	0x2000		/* last BD in ring */
#define SCC_ETHER_TX_BD_PAD	0x4000		/* auto pad short frames */
#define SCC_ETHER_TX_BD_R	0x8000		/* buffer is ready */

/* SCC - Serial Comunications Controller */
 
typedef struct          /* SCC_BUF */
    {
    UINT16	statusMode;		/* status and control */
    UINT16	dataLength;		/* length of data buffer in bytes */
    u_char *	dataPointer;		/* points to data buffer */
    } SCC_BUF;
 
typedef struct          /* SCC_PARAM */
    {
    UINT16	rbase;			/* Rx buffer descriptor base address */
    UINT16	tbase;			/* Tx buffer descriptor base address */
    UINT8	rfcr;			/* Rx function code */
    UINT8	tfcr;			/* Tx function code */
    UINT16	mrblr;			/* maximum receive buffer length */
    UINT32	rstate;			/* Rx internal state */
    UINT32	res1;			/* reserved/internal */
    UINT16	rbptr;			/* Rx buffer descriptor pointer */
    UINT16	res2;			/* reserved/internal */
    UINT32	res3;			/* reserved/internal */
    UINT32	tstate;			/* Tx internal state */
    UINT32	res4;			/* reserved/internal */
    UINT16	tbptr;			/* Tx buffer descriptor pointer */
    UINT16	res5;			/* reserved/internal */
    UINT32	res6;			/* reserved/internal */
    UINT32	rcrc;			/* temp receive CRC */
    UINT32	tcrc;			/* temp transmit CRC */
    } SCC_PARAM;
 
typedef struct          /* SCC */
    {
    SCC_PARAM	param;			/* SCC parameters */
    char	prot[64];		/* protocol specific area */
    } SCC;

typedef struct          /* SCC_REG */
    {
    UINT32	gsmrl;			/* SCC general mode register - low */
    UINT32	gsmrh;			/* SCC eneral mode register - high */
    UINT16	psmr;			/* SCC protocol mode register */
    UINT16	res1;			/* reserved */
    UINT16	todr;			/* SCC transmit on demand */
    UINT16	dsr;			/* SCC data sync. register */
    UINT16	scce;			/* SCC event register */
    UINT16	res2;			/* reserved */
    UINT16	sccm;			/* SCC mask register */
    UINT8	res3;			/* reserved */
    UINT8	sccs;			/* SCC status register */
    } SCC_REG;

/* SCC device descriptor */

typedef struct          /* SCC_DEV */
    {
    int 		sccNum;		/* number of SCC device */
    int 		txBdNum;	/* number of transmit buf descriptors */
    int 		rxBdNum;	/* number of receive buf descriptors */
    SCC_BUF * 		txBdBase;	/* transmit BD base address */
    SCC_BUF * 		rxBdBase;	/* receive BD base address */
    u_char * 		txBufBase;	/* transmit buffer base address */
    u_char *		rxBufBase;	/* receive buffer base address */
    UINT32 		txBufSize;	/* transmit buffer size */
    UINT32 		rxBufSize;	/* receive buffer size */
    int			txBdNext;	/* next transmit BD to fill */
    int			rxBdNext;	/* next receive BD to read */
    volatile SCC *	pScc;		/* SCC parameter RAM */
    volatile SCC_REG *	pSccReg;	/* SCC registers */
    UINT32		intMask;	/* interrupt acknowledge mask */
    } SCC_DEV;
 
#ifndef INCLUDE_TY_CO_DRV_50

/* UART SCC device descriptor */

typedef struct          /* TY_CO_DEV */
    {
    TY_DEV		tyDev;		/* tyLib will handle this portion */
    BOOL		created;	/* device has been created */
    char		numChannels;	/* number of channels to support */
    int			clockRate;	/* CPU clock frequency (Hz) */
    int 		bgrNum;		/* number of BRG being used */
    UINT32 *		pBaud;		/* BRG registers */
    UINT32		regBase;	/* register/DPR base address */
    SCC_DEV		uart;		/* UART SCC device */
    } TY_CO_DEV;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

IMPORT  void    tyCoInt (TY_CO_DEV * pDv);

#else	/* __STDC__ */

IMPORT	void	tyCoInt ();

#endif	/* __STDC__ */
#endif	/* INCLUDE_TY_CO_DRV_50 */
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68360h */
