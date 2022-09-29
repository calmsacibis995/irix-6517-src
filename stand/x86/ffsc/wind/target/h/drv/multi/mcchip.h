/* mcchip.h - Memory Controller ASIC (MCchip) */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01a,06jan93,ccc	 written.
*/

#ifdef	DOC
#define INCmcchiph
#endif	/* DOC */

#ifndef __INCmccchiph
#define __INCmccchiph

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file contains constants for the Memory Controller ASIC (MCchip).
 * The macro MCC_BASE_ADRS must be defined when including this header.
 */

#ifdef	_ASMLANGUAGE
#define CAST
#define	CASTINT
#else
#define CAST (char *)
#define	CASTINT (int *)
#endif	/* _ASMLANGUAGE */

/* on-board access, register definitions */

#define	MCC_REG_INTERVAL	1

#ifndef MCC_ADRS	/* to permit alternative board addressing */
#define MCC_ADRS(reg)   (CAST (MCC_BASE_ADRS + (reg * MCC_REG_INTERVAL)))
#define	MCC_ADRS_INT(reg)  (CASTINT (MCC_BASE_ADRS + \
				   (reg * MCC_REG_INTERVAL)))
#endif	/* MCC_ADRS */

#define MCC_ID			MCC_ADRS(0x00)	/* Chip ID 		      */
#define	MCC_REVISION		MCC_ADRS(0x01)	/* Chip Revision 	      */
#define	MCC_GCR			MCC_ADRS(0x02)	/* General Control Register   */
#define	MCC_VBR			MCC_ADRS(0x03)	/* Vector Base Register	      */
#define	MCC_TIMER1_CMP		MCC_ADRS_INT(0x04) /* Tick Timer 1 Comp Reg  */
#define	MCC_TIMER1_CMP_UU	MCC_ADRS(0x04)	/* Tick Timer 1 Comp Reg - UU */
#define	MCC_TIMER1_CMP_UL	MCC_ADRS(0x05)	/*			 - UL */
#define	MCC_TIMER1_CMP_LU	MCC_ADRS(0x06)	/*			 - LU */
#define	MCC_TIMER1_CMP_LL	MCC_ADRS(0x07)	/*			 - LL */
#define	MCC_TIMER1_CNT		MCC_ADRS_INT(0x08) /* Tick Timer 1 Count Reg */
#define	MCC_TIMER1_CNT_UU	MCC_ADRS(0x08)	/* Tick Timer 1 Cnt Reg - UU  */
#define	MCC_TIMER1_CNT_UL	MCC_ADRS(0x09)	/*			- UL  */
#define	MCC_TIMER1_CNT_LU	MCC_ADRS(0x0a)	/*			- LU  */
#define	MCC_TIMER1_CNT_LL	MCC_ADRS(0x0b)	/*			- LL  */
#define	MCC_TIMER2_CMP		MCC_ADRS_INT(0x0c) /* Tick Timer 2 Comp Reg  */
#define	MCC_TIMER2_CMP_UU	MCC_ADRS(0x0c)	/* Tick Timer 2 Comp Reg - UU */
#define	MCC_TIMER2_CMP_UL	MCC_ADRS(0x0d)	/*			 - UL */
#define	MCC_TIMER2_CMP_LU	MCC_ADRS(0x0e)	/*			 - LU */
#define	MCC_TIMER2_CMP_LL	MCC_ADRS(0x0f)	/*			 - LL */
#define	MCC_TIMER2_CNT		MCC_ADRS_INT(0x10) /* Tick Timer 2 Count Reg */
#define	MCC_TIMER2_CNT_UU	MCC_ADRS(0x10)	/* Tick Timer 2 Cnt Reg - UU  */
#define	MCC_TIMER2_CNT_UL	MCC_ADRS(0x11)	/*			- UL  */
#define	MCC_TIMER2_CNT_LU	MCC_ADRS(0x12)	/*			- LU  */
#define	MCC_TIMER2_CNT_LL	MCC_ADRS(0x13)	/*			- LL  */
#define	MCC_PRESCALE		MCC_ADRS(0x14)	/* Prescaler Count Register   */
#define	MCC_PRESCALE_CLK_ADJ	MCC_ADRS(0x15)	/* Prescaler Clock Adjust     */
#define	MCC_TIMER2_CR		MCC_ADRS(0x16)	/* Tick Timer 2 Ctrl Reg      */
#define	MCC_TIMER1_CR		MCC_ADRS(0x17)	/* Tick Timer 1 Ctrl Reg      */
#define	MCC_T4_IRQ_CR		MCC_ADRS(0x18)	/* Tick Timer 4 Inter CR      */
#define	MCC_T3_IRQ_CR		MCC_ADRS(0x19)	/* Tick Timer 3 Inter CR      */
#define	MCC_T2_IRQ_CR		MCC_ADRS(0x1a)	/* Tick Timer 2 Inter CR      */
#define	MCC_T1_IRQ_CR		MCC_ADRS(0x1b)	/* Tick Timer 1 Inter CR      */
#define	MCC_PARITY_ICR		MCC_ADRS(0x1c)	/* DRAM Parity Int Ctrl Reg   */
#define	MCC_SCC_ICR		MCC_ADRS(0x1d)	/* SCC Inter Ctrl Reg         */
#define	MCC_TIMER4_CR		MCC_ADRS(0x1e)	/* Tick Timer 4 Ctrl Reg      */
#define	MCC_TIMER3_CR		MCC_ADRS(0x1f)	/* Tick Timer 3 Ctrl Reg      */
#define	MCC_DRAM_BASE_AR_HIGH	MCC_ADRS(0x20)	/* DRAM Space Base Addr (hi)  */
#define	MCC_DRAM_BASE_AR_LOW	MCC_ADRS(0x21)	/* DRAM Space Base Addr (low) */
#define	MCC_SRAM_BASE_AR_HIGH	MCC_ADRS(0x22)	/* SRAM Space Base Addr (hi)  */
#define	MCC_SRAM_BASE_AR_LOW	MCC_ADRS(0x23)	/* SRAM Space Base Adrs (low) */
#define	MCC_DRAM_SPACE_SIZE	MCC_ADRS(0x24)	/* DRAM Space Size            */
#define	MCC_DRAM_SRAM_OPTIONS	MCC_ADRS(0x25)	/* DRAM/SRAM Options          */
#define	MCC_SRAM_SPACE_SIZE	MCC_ADRS(0x26)	/* SRAM Space Size            */
#define	MCC_LANC_ERR_SR		MCC_ADRS(0x28)	/* LANC Error Status Register */
#define	MCC_LANC_IRQ_CR		MCC_ADRS(0x2a)	/* LANC Inter Ctrl Reg	      */
#define	MCC_LANC_BEICR		MCC_ADRS(0x2b)	/* LANC Bus Error Inter CR    */
#define	MCC_SCSI_ERR_SR		MCC_ADRS(0x2c)	/* SCSI Error Status Register */
#define	MCC_GENERAL_INPUT	MCC_ADRS(0x2d)	/* General Purpose Input Reg  */
#define	MCC_VERSION_REG		MCC_ADRS(0x2e)	/* Board Version              */
#define	MCC_SCSI_IRQ_CR		MCC_ADRS(0x2f)	/* SCSI Inter Control Reg     */
#define	MCC_TIMER3_CMP		MCC_ADRS_INT(0x30) /* Tick Timer 3 Cmp Reg    */
#define	MCC_TIMER3_CMP_UU	MCC_ADRS(0x30)	/* Tick Timer 3 Cmp Reg - UU  */
#define	MCC_TIMER3_CMP_UL	MCC_ADRS(0x31)	/*                      - UL  */
#define	MCC_TIMER3_CMP_LU	MCC_ADRS(0x32)	/*                      - LU  */
#define	MCC_TIMER3_CMP_LL	MCC_ADRS(0x33)	/*                      - LL  */
#define	MCC_TIMER3_CNT		MCC_ADRS_INT(0x34) /* Tick Timer 3 Count Reg  */
#define	MCC_TIMER3_CNT_UU	MCC_ADRS(0x34)	/* Tick Timer 3 Cnt Reg - UU  */
#define	MCC_TIMER3_CNT_UL	MCC_ADRS(0x35)	/*                      - UL  */
#define	MCC_TIMER3_CNT_LU	MCC_ADRS(0x36)	/*                      - LU  */
#define	MCC_TIMER3_CNT_LL	MCC_ADRS(0x37)	/*                      - LL  */
#define	MCC_TIMER4_CMP		MCC_ADRS_INT(0x38) /* Tick Timer 4 Cmp Reg    */
#define MCC_TIMER4_CMP_UU       MCC_ADRS(0x38)  /* Tick Timer 4 Cmp Reg - UU  */
#define MCC_TIMER4_CMP_UL       MCC_ADRS(0x39)  /*                      - UL  */
#define MCC_TIMER4_CMP_LU       MCC_ADRS(0x3a)  /*                      - LU  */
#define MCC_TIMER4_CMP_LL       MCC_ADRS(0x3b)  /*                      - LL  */
#define MCC_TIMER4_CNT          MCC_ADRS_INT(0x3c) /* Tick Timer 4 Count Reg  */
#define MCC_TIMER4_CNT_UU       MCC_ADRS(0x3c)  /* Tick Timer 4 Cnt Reg - UU  */
#define MCC_TIMER4_CNT_UL       MCC_ADRS(0x3d)  /*                      - UL  */
#define MCC_TIMER4_CNT_LU       MCC_ADRS(0x3e)  /*                      - LU  */
#define MCC_TIMER4_CNT_LL       MCC_ADRS(0x3f)  /*                      - LL  */
#define	MCC_BUS_CLK_REG		MCC_ADRS(0x40)	/* Bus Clock Register         */
#define	MCC_PROM_ACCESS_TIME	MCC_ADRS(0x41)	/* PROM access time register  */
#define	MCC_FLASH_ACCESS_TIME	MCC_ADRS(0x42)	/* FLASH access time register */
#define	MCC_ABORT_ICR		MCC_ADRS(0x43)	/* ABORT switch int cont reg  */
#define	MCC_RESET_CR		MCC_ADRS(0x44)	/* RESET switch control reg   */
#define	MCC_WD_TIMER_CR		MCC_ADRS(0x45)	/* Watchdog timer control reg */
#define	MCC_WD_TIMEOUT_REG	MCC_ADRS(0x46)	/* Access and watchdog times  */
#define	MCC_DRAM_CONTROL_REG	MCC_ADRS(0x48)	/* DRAM control register      */
#define	MCC_MPU_STATUS_REG	MCC_ADRS(0x4a)	/* MPU status register        */
#define	MCC_PRESCALE_COUNT	MCC_ADRS_INT(0x4c) /* 32-bit prescale count   */

/****************************************************************
 * GCR			0x02	General Control Register	*
 ****************************************************************/

#define	GCR_FAST_ON	0x01	/* Enable fast access for BBRAM		0 */
#define	GCR_FAST_OFF	0x00	/* Disable fast access for BBRAM	0 */
#define	GCR_MIEN_ON	0x02	/* Master Interrupt Enable		1 */
#define GCR_MIEN_OFF	0x00	/* Master Interrupt Enable OFF		1 */

/****************************************************************
 * VBR			0x03	Vector Base Register		*
 ****************************************************************/

#define	MCC_INT_TT4		0x3	/* Tick Timer 4 IRQ		*/
#define	MCC_INT_TT3		0x4	/* Tick Timer 3 IRQ		*/
#define	MCC_INT_SCSI		0x5	/* SCSI IRQ			*/
#define	MCC_INT_LANC_ERR	0x6	/* LANC ERR			*/
#define	MCC_INT_LANC		0x7	/* LANC IRQ			*/
#define	MCC_INT_TT2		0x8	/* Tick Timer 2 IRQ		*/
#define	MCC_INT_TT1		0x9	/* Tick Timer 1 IRQ		*/
#define	MCC_INT_PARITY_ERROR	0xb	/* Parity Error IRQ		*/
#define	MCC_INT_ABORT		0xe	/* ABORT switch IRQ		*/

/****************************************************************
 * TIMER2_CR		0x16	Tick Timer 2 Control Register	*
 ****************************************************************/

#define	TIMER2_CR_CEN	0x01	/* Counter Enable			0 */
#define	TIMER2_CR_DIS	0x00	/* Counter Disable			0 */
#define	TIMER2_CR_COC	0x02	/* Clear On Compare			1 */
#define	TIMER2_CR_COVF	0x04	/* Clear Overflow Counter		2 */

/****************************************************************
 * TIMER1_CR		0x17	Tick Timer 1 Control Register	*
 ****************************************************************/

#define	TIMER1_CR_CEN	0x01	/* Counter Enable			0 */
#define	TIMER1_CR_DIS	0x00	/* Counter Disable			0 */
#define	TIMER1_CR_COC	0x02	/* Clear On Compare			1 */
#define	TIMER1_CR_COVF	0x04	/* Clear Overflow Counter		2 */

/************************************************************************
 * T4_IRQ_CR            0x18    Tick Timer 4 Interrupt Control Register *
 ************************************************************************/
 
#define T4_IRQ_CR_ICLR  0x08    /* Clear IRQ                            3 */
#define T4_IRQ_CR_IEN   0x10    /* Interrupt Enable                     4 */
#define T4_IRQ_CR_DIS   0x00    /* Interrupt Disable                    4 */
#define T4_IRQ_CR_INT   0x20    /* Interrupt Status                     5 */
 
/************************************************************************
 * T3_IRQ_CR            0x19    Tick Timer 3 Interrupt Control Register *
 ************************************************************************/
 
#define T3_IRQ_CR_ICLR  0x08    /* Clear IRQ                            3 */
#define T3_IRQ_CR_IEN   0x10    /* Interrupt Enable                     4 */
#define T3_IRQ_CR_DIS   0x00    /* Interrupt Disable                    4 */
#define T3_IRQ_CR_INT   0x20    /* Interrupt Status                     5 */

/************************************************************************
 * T2_IRQ_CR		0x1a	Tick Timer 2 Interrupt Control Register	*
 ************************************************************************/

#define	T2_IRQ_CR_ICLR	0x08	/* Clear IRQ				3 */
#define	T2_IRQ_CR_IEN	0x10	/* Interrupt Enable			4 */
#define	T2_IRQ_CR_DIS	0x00	/* Interrupt Disable			4 */
#define	T2_IRQ_CR_INT	0x20	/* Interrupt Status			5 */

/************************************************************************
 * T1_IRQ_CR		0x1b	Tick Timer 1 Interrupt Control Register	*
 ************************************************************************/

#define	T1_IRQ_CR_ICLR	0x08	/* Clear IRQ				3 */
#define	T1_IRQ_CR_IEN	0x10	/* Interrupt Enable			4 */
#define	T1_IRQ_CR_DIS	0x00	/* Interrupt Disable			4 */
#define	T1_IRQ_CR_INT	0x20	/* Interrupt Status			5 */

/*****************************************************************************
 * PARITY_ICR		0x1c	DRAM Parity Error Interrupt Control Register *
 *****************************************************************************/

#define	PARITY_ICR_ICLR	0x08	/* Clear IRQ				3 */
#define	PARITY_ICR_IEN	0x10	/* Interrupt Enable			4 */
#define	PARITY_ICR_DIS	0x00	/* Interrupt Disable			4 */
#define	PARITY_ICR_INT	0x20	/* Interrupt Status			5 */

/****************************************************************
 * SCC_ICR		0x1d	SCC Interrupt Control Register	*
 ****************************************************************/

#define	SCC_ICR_IEN	0x10	/* Interrupt Enable			4 */
#define	SCC_ICR_DIS	0x00	/* Interrupt Disable			4 */
#define	SCC_ICR_IRQ	0x20	/* An Interrupt has occured		5 */

/****************************************************************
 * TIMER4_CR            0x1e    Tick Timer 4 Control Register   *
 ****************************************************************/
 
#define TIMER4_CR_CEN   0x01    /* Counter Enable                       0 */
#define TIMER4_CR_DIS   0x00    /* Counter Disable                      0 */
#define TIMER4_CR_COC   0x02    /* Clear On Compare                     1 */
#define TIMER4_CR_COVF  0x04    /* Clear Overflow Counter               2 */
 
/****************************************************************
 * TIMER3_CR            0x1f    Tick Timer 3 Control Register   *
 ****************************************************************/
 
#define TIMER3_CR_CEN   0x01    /* Counter Enable                       0 */
#define TIMER3_CR_DIS   0x00    /* Counter Disable                      0 */
#define TIMER3_CR_COC   0x02    /* Clear On Compare                     1 */
#define TIMER3_CR_COVF  0x04    /* Clear Overflow Counter               2 */

/*********************************************************
 * DRAM_SPACE_SIZE	0x24	DRAM Space Size Register *
 *********************************************************/

#define	DRAM_SPACE_SIZE_1MB	0x00	/* 1 MB with 4 Mbit DRAMs        2-0 */
#define	DRAM_SPACE_SIZE_2MB	0x01	/* 2 MB with 4 Mbit DRAMs            */
#define	DRAM_SPACE_SIZE_4MB4	0x03	/* 4 MB with 4 Mbit DRAMs interleave */
#define	DRAM_SPACE_SIZE_4MB16	0x04	/* 4 MB with 16 Mbit DRAMs           */
#define	DRAM_SPACE_SIZE_8MB	0x05	/* 8 MB with 16 Mbit DRAMs           */
#define	DRAM_SPACE_SIZE_NONE	0x06	/* DRAM mezzanine is not present     */
#define	DRAM_SPACE_SIZE_16MB	0x07	/* 16 MB with 16 Mbit DRAMs int'leve */

/***********************************************************
 * DRAM_SRAM_OPTIONS	0x25	DRAM/SRAM Options Register *
 ***********************************************************/

#define	DRAM_SRAM_OPTIONS_DMASK	0x07	/* Mask for DRAM bits         2-0 */
#define	DRAM_SRAM_OPTIONS_SMASK 0x18	/* Mask for SRAM bits         4-3 */

/*********************************************************
 * SRAM_SPACE_SIZE	0x26	SRAM Space Size Register *
 *********************************************************/

#define	SRAM_SPACE_512K		0x01	/* 512 KB */
#define	SRAM_SPACE_1MB		0x02	/* 1MB */
#define	SRAM_SPACE_2MB		0x03	/* 2MB */
#define	SRAM_SPACE_ENABLE	0x04	/* SRAM enable */

/****************************************************************
 * LANC_ERR_SR		0x28	LANC Error Status Register	*
 ****************************************************************/

#define	LANC_ERR_SR_SCLR	0x01	/* Clear Error Status Bits	0 */
#define	LANC_ERR_SR_LTO		0x02	/* Local Time out error		1 */
#define	LANC_ERR_SR_EXT		0x04	/* VMEbus error			2 */
#define	LANC_ERR_SR_PRTY	0x08	/* DRAM Parity Error		3 */

/****************************************************************
 * LANC_IRQ_CR		0x2a	LANC Interrupt Control Register	*
 ****************************************************************/

#define	LANC_IRQ_CR_ICLR	0x08	/* Clear IRQ in edge mode	3 */
#define	LANC_IRQ_CR_IEN		0x10	/* Interrupt Enable		4 */
#define	LANC_IRQ_CR_DIS		0x00	/* Interrupt Disable		4 */
#define	LANC_IRQ_CR_INT		0x20	/* Interrupt Status		5 */
#define	LANC_IRQ_CR_EDGE	0x40	/* Edge sensitive IRQ		6 */
#define	LANC_IRQ_CR_LEVEL	0x00	/* Level sensitive IRQ		6 */
#define	LANC_IRQ_CR_HIGH_LOW	0x00	/* IRQ on RISING or HIGH	7 */
#define	LANC_IRQ_CR_LOW_HIGH	0x80	/* IRQ on FALLING or LOW	7 */

/**************************************************************************
 * LANC_BEICR		0x2b	LANC Bus Error Interrupt Control Register *
 **************************************************************************/

#define	LANC_BEICR_ICLR		0x08	/* Clear IRQ			3 */
#define	LANC_BEICR_IEN		0x10	/* Interrupt Enable		4 */
#define	LANC_BEICR_DIS		0x00	/* Interrupt Disable		4 */
#define	LANC_BEICR_IRQ		0x20	/* Interrupt Status		5 */
#define	LANC_BEICR_NO_SNOOP	0x00	/* Inhibit Snoop		7-6 */
#define	LANC_BEICR_SINK_DATA	0x40	/* Sink Data			7-6 */
#define	LANC_BEICR_INVALIDATE	0x80	/* Invalidate Line		7-6 */

/****************************************************************
 * SCSI_ERR_SR		0x2c	SCSI Error Status Register	*
 ****************************************************************/

#define	SCSI_ERR_SR_SCLR	0x01	/* Clear Error Status Bits	0 */
#define	SCSI_ERR_SR_LTO		0x02	/* Local Time out error		1 */
#define	SCSI_ERR_SR_EXT		0x04	/* VMEbus error			2 */
#define	SCSI_ERR_SR_PRTY	0x08	/* DRAM Parity Error		3 */

/*************************************************
 * VERSION_REG		0x2e	Version Register *
 *************************************************/

#define	VERSION_REG_SPEED	0x01	/* 0=25MHz, 1=33MHz             0 */
#define	VERSION_REG_VMECHIP	0x02	/* 0=present, 1=not installed   1 */
#define	VERSION_REG_SCSI	0x04	/* 0=present, 1=not installed   2 */
#define	VERSION_REG_ETHERNET	0x08	/* 0=present, 1=not installed   3 */
#define	VERSION_REG_MC68040	0x10	/* 1=MC68040, 0=MC68LC040       4 */
#define	VERSION_REG_FLASH	0x20	/* address location             5 */
#define	VERSION_REG_IPIC2	0x40	/* 0=present, 1=not installed   6 */
#define	VERSION_REG_IPIC1	0x80	/* 0=present, 1=not installed   7 */

/****************************************************************
 * SCSI_IRQ_CR		0x2f	SCSI Interrupt Control Register	*
 ****************************************************************/

#define	SCSI_IRQ_CR_IEN		0x10	/* Interrupt Enable		4 */
#define	SCSI_IRQ_CR_DIS		0x00	/* Interrupt Disable		4 */
#define	SCSI_IRQ_CR_IRQ		0x20	/* Interrupt Status		5 */

/*******************************************************************
 * PROM_ACCESS_TIME	0x41	PROM Access Timer Control Register *
 *******************************************************************/

#define	PROM_ACCESS_25M_60NS	0x00	/* 25 MHz -  60 ns */
#define	PROM_ACCESS_25M_100NS	0x01	/*        - 100 ns */
#define	PROM_ACCESS_25M_140NS	0x02	/*        - 140 ns */
#define	PROM_ACCESS_25M_180NS	0x03	/*        - 180 ns */
#define	PROM_ACCESS_25M_220NS	0x04	/*        - 220 ns */
#define	PROM_ACCESS_25M_260NS	0x05	/*        - 260 ns */
#define	PROM_ACCESS_25M_300NS	0x06	/*        - 300 ns */
#define	PROM_ACCESS_25M_340NS	0x07	/*        - 340 ns */

#define PROM_ACCESS_33M_40NS    0x00    /* 33 MHz -  40 ns */
#define PROM_ACCESS_33M_70NS    0x01    /*        -  70 ns */
#define PROM_ACCESS_33M_100NS   0x02    /*        - 100 ns */
#define PROM_ACCESS_33M_130NS   0x03    /*        - 130 ns */
#define PROM_ACCESS_33M_160NS   0x04    /*        - 160 ns */
#define PROM_ACCESS_33M_190NS   0x05    /*        - 190 ns */
#define PROM_ACCESS_33M_210NS   0x06    /*        - 210 ns */
#define PROM_ACCESS_33M_240NS   0x07    /*        - 240 ns */

/********************************************************************
 * FLASH_ACCESS_TIME     0x42    PROM Access Timer Control Register *
 ********************************************************************/

#define FLASH_ACCESS_25M_60NS   0x00    /* 25 MHz -  60 ns */
#define FLASH_ACCESS_25M_100NS  0x01    /*        - 100 ns */
#define FLASH_ACCESS_25M_140NS  0x02    /*        - 140 ns */
#define FLASH_ACCESS_25M_180NS  0x03    /*        - 180 ns */
#define FLASH_ACCESS_25M_220NS  0x04    /*        - 220 ns */
#define FLASH_ACCESS_25M_260NS  0x05    /*        - 260 ns */
#define FLASH_ACCESS_25M_300NS  0x06    /*        - 300 ns */
#define FLASH_ACCESS_25M_340NS  0x07    /*        - 340 ns */

#define FLASH_ACCESS_33M_40NS   0x00    /* 33 MHz -  40 ns */
#define FLASH_ACCESS_33M_70NS   0x01    /*        -  70 ns */
#define FLASH_ACCESS_33M_100NS  0x02    /*        - 100 ns */
#define FLASH_ACCESS_33M_130NS  0x03    /*        - 130 ns */
#define FLASH_ACCESS_33M_160NS  0x04    /*        - 160 ns */
#define FLASH_ACCESS_33M_190NS  0x05    /*        - 190 ns */
#define FLASH_ACCESS_33M_210NS  0x06    /*        - 210 ns */
#define FLASH_ACCESS_33M_240NS  0x07    /*        - 240 ns */

/************************************************************************
 * ABORT_ICR            0x43    ABORT Switch Interrupt Control Register *
 ************************************************************************/

#define ABORT_ICR_ICLR  0x08    /* Clear IRQ                            3 */
#define ABORT_ICR_IEN   0x10    /* Interrupt Enable                     4 */
#define ABORT_ICR_DIS   0x00    /* Interrupt Disable                    4 */
#define ABORT_ICR_INT   0x20    /* Interrupt Status                     5 */

/**************************************************************
 * RESET_CR		0x44	RESET Switch Control Register *
 **************************************************************/

#define	RESET_CR_BRFLI	0x10	/* Board Fail Status                    4 */
#define	RESET_CR_PURS	0x08	/* Power-up Reset Status                3 */
#define	RESET_CR_CPURS	0x04	/* Clear Power-up Reset                 2 */
#define	RESET_CR_BDFLO	0x02	/* Board Fail Assert                    1 */
#define	RESET_CR_RSWE	0x01	/* RESET Switch Enable                  0 */

/****************************************************************
 * WD_TIMER_CR		0x45	Watchdog Timer Control Register *
 ****************************************************************/

#define	WD_TIMER_CR_WDCS  0x40	/* Clear Watchdog Timeout Status        6 */
#define	WD_TIMER_CR_WDCC  0x20	/* Clear Watchdog Counter               5 */
#define	WD_TIMER_CR_WDTO  0x10	/* Watchdog Timer Status Bit            4 */
#define	WD_TIMER_CR_WDBFE 0x08	/* Watchdog Timeout Asserts Fail        3 */
#define	WD_TIMER_CR_WDRSE 0x02	/* Watchdog Timeout Asserts LRESET      1 */
#define	WD_TIMER_CR_WDEN  0x01	/* Watchdog Timer Enable                0 */
#define	WD_TIMER_CR_DIS   0x00  /* Watchdog Timer Disable                 */

/*************************************************************************
 * WD_TIMEOUT_REG	0x46	Access and Watchdog Time Base Select Reg *
 *************************************************************************/

#define	WD_TIMEOUT_REG_WD_512US	0x00	/* Watchdog Timeout:  512us    3-0 */
#define	WD_TIMEOUT_REG_WD_1MS	0x01	/*                      1ms        */
#define	WD_TIMEOUT_REG_WD_2MS	0x02	/*                      2ms        */
#define WD_TIMEOUT_REG_WD_4MS   0x03    /*                      4ms        */
#define WD_TIMEOUT_REG_WD_8MS   0x04    /*                      8ms        */
#define WD_TIMEOUT_REG_WD_16MS  0x05    /*                     16ms        */
#define WD_TIMEOUT_REG_WD_32MS  0x06    /*                     32ms        */
#define WD_TIMEOUT_REG_WD_64MS  0x07    /*                     64ms        */
#define WD_TIMEOUT_REG_WD_128MS 0x08    /*                    128ms        */
#define WD_TIMEOUT_REG_WD_256MS 0x09    /*                    256ms        */
#define WD_TIMEOUT_REG_WD_512MS 0x0a    /*                    512ms        */
#define WD_TIMEOUT_REG_WD_1S    0x0b    /*                      1s         */
#define WD_TIMEOUT_REG_WD_4S    0x0c    /*                      4s         */
#define WD_TIMEOUT_REG_WD_16S   0x0d    /*                     16s         */
#define WD_TIMEOUT_REG_WD_32S   0x0e    /*                     32s         */
#define WD_TIMEOUT_REG_WD_64S   0x0f    /*                     64s         */

#define WD_TIMEOUT_REG_LB_8US   0x00    /* Local Bus Timeout:   8us    5-4 */
#define WD_TIMEOUT_REG_LB_64US  0x01    /*                     64us        */
#define WD_TIMEOUT_REG_LB_256US 0x02    /*                    256us        */
#define WD_TIMEOUT_REG_LB_NONE  0x03    /*                     none        */

/******************************************************
 * DRAM_CONTROL_REG	0x48	DRAM Control Register *
 ******************************************************/

#define	DRAM_CONTROL_REG_RAM_EN	0x01	/* DRAM Enable                   0 */
#define	DRAM_CONTROL_REG_PAR_EN	0x02	/* DRAM Parity Check Enable      1 */
#define	DRAM_CONTROL_REG_PAR_IN	0x04	/* DRAM Parity Error -> IRQ      2 */

/****************************************************
 * MPU_STATUS_REG	0x4a	MPU Status Register *
 ****************************************************/

#define	MPU_STATUS_REG_MLTO	0x01	/* MPU received Local Bus Timeout 0 */
#define	MPU_STATUS_REG_MLPE	0x02	/* MPU received Parity Error      1 */
#define	MPU_STATUS_REG_MLBE	0x04	/* MPU received TEA               2 */
#define	MPU_STATUS_REG_MCLR	0x08	/* Clear MPU status bits          3 */

#ifdef __cplusplus
}
#endif

#endif /* __INCmccchiph */
