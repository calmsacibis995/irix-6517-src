/* pccchip2.h - Peripheral Channel Controller */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,12aug91,ccc  added CASTINT for long word access to register (lint error).
01a,17jun91,ccc	 written.
*/

#ifdef	DOC
#define INCpccchip2h
#endif	/* DOC */

#ifndef __INCpccchip2h
#define __INCpccchip2h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file contains constants for the Paripheral Channel controller.
 * The macro PCC2_BASE_ADRS must be defined when including this header.
 */

#ifdef	_ASMLANGUAGE
#define CAST
#define	CASTINT
#else
#define CAST (char *)
#define	CASTINT (int *)
#endif	/* _ASMLANGUAGE */

/* on-board access, register definitions */

#define	PCC2_REG_INTERVAL	1

#ifndef PCC2_ADRS	/* to permit alternative board addressing */
#define PCC2_ADRS(reg)   (CAST (PCC2_BASE_ADRS + (reg * PCC2_REG_INTERVAL)))
#define	PCC2_ADRS_INT(reg)  (CASTINT (PCC2_BASE_ADRS + \
				   (reg * PCC2_REG_INTERVAL)))
#endif	/* PCC2_ADRS */

#define PCC2_ID			PCC2_ADRS(0x00)	/* Chip ID 		      */
#define	PCC2_REVISION		PCC2_ADRS(0x01)	/* Chip Revision 	      */
#define	PCC2_GCR		PCC2_ADRS(0x02)	/* General Control Register   */
#define	PCC2_VBR		PCC2_ADRS(0x03)	/* Vector Base Register	      */
#define	PCC2_TIMER1_CMP		PCC2_ADRS_INT(0x04) /* Tick Timer 1 Comp Reg  */
#define	PCC2_TIMER1_CMP_UU	PCC2_ADRS(0x04)	/* Tick Timer 1 Comp Reg - UU */
#define	PCC2_TIMER1_CMP_UL	PCC2_ADRS(0x05)	/*			 - UL */
#define	PCC2_TIMER1_CMP_LU	PCC2_ADRS(0x06)	/*			 - LU */
#define	PCC2_TIMER1_CMP_LL	PCC2_ADRS(0x07)	/*			 - LL */
#define	PCC2_TIMER1_CNT		PCC2_ADRS_INT(0x08) /* Tick Timer 1 Count Reg */
#define	PCC2_TIMER1_CNT_UU	PCC2_ADRS(0x08)	/* Tick Timer 1 Cnt Reg - UU  */
#define	PCC2_TIMER1_CNT_UL	PCC2_ADRS(0x09)	/*			- UL  */
#define	PCC2_TIMER1_CNT_LU	PCC2_ADRS(0x0a)	/*			- LU  */
#define	PCC2_TIMER1_CNT_LL	PCC2_ADRS(0x0b)	/*			- LL  */
#define	PCC2_TIMER2_CMP		PCC2_ADRS_INT(0x0c) /* Tick Timer 2 Comp Reg  */
#define	PCC2_TIMER2_CMP_UU	PCC2_ADRS(0x0c)	/* Tick Timer 2 Comp Reg - UU */
#define	PCC2_TIMER2_CMP_UL	PCC2_ADRS(0x0d)	/*			 - UL */
#define	PCC2_TIMER2_CMP_LU	PCC2_ADRS(0x0e)	/*			 - LU */
#define	PCC2_TIMER2_CMP_LL	PCC2_ADRS(0x0f)	/*			 - LL */
#define	PCC2_TIMER2_CNT		PCC2_ADRS_INT(0x10) /* Tick Timer 2 Count Reg */
#define	PCC2_TIMER2_CNT_UU	PCC2_ADRS(0x10)	/* Tick Timer 2 Cnt Reg - UU  */
#define	PCC2_TIMER2_CNT_UL	PCC2_ADRS(0x11)	/*			- UL  */
#define	PCC2_TIMER2_CNT_LU	PCC2_ADRS(0x12)	/*			- LU  */
#define	PCC2_TIMER2_CNT_LL	PCC2_ADRS(0x13)	/*			- LL  */
#define	PCC2_PRESCALE		PCC2_ADRS(0x14)	/* Prescaler Count Register   */
#define	PCC2_PRESCALE_CLK_ADJ	PCC2_ADRS(0x15)	/* Prescaler Clock Adjust     */
#define	PCC2_TIMER2_CR		PCC2_ADRS(0x16)	/* Tick Timer 2 Ctrl Reg      */
#define	PCC2_TIMER1_CR		PCC2_ADRS(0x17)	/* Tick Timer 1 Ctrl Reg      */
#define	PCC2_GPIICR		PCC2_ADRS(0x18)	/* GPI Interrupt CR	      */
#define	PCC2_GPIOCR		PCC2_ADRS(0x19)	/* GPI/O Control Register     */
#define	PCC2_T2_IRQ_CR		PCC2_ADRS(0x1a)	/* Tick Timer 2 Inter CR      */
#define	PCC2_T1_IRQ_CR		PCC2_ADRS(0x1b)	/* Tick Timer 1 Inter CR      */
#define	PCC2_SCC_ERR_SR		PCC2_ADRS(0x1c)	/* SCC Error Status Register  */
#define	PCC2_SCC_MICR		PCC2_ADRS(0x1d)	/* SCC Modem Inter Ctrl Reg   */
#define	PCC2_SCC_TICR		PCC2_ADRS(0x1e)	/* SCC Transmit Inter CR      */
#define	PCC2_SCC_RICR		PCC2_ADRS(0x1f)	/* SCC Receive Inter CR	      */
#define	PCC2_MODEM_PIACK	PCC2_ADRS(0x23)	/* Modem PIACK Register	      */
#define	PCC2_TRANS_PIACK	PCC2_ADRS(0x25)	/* Transmit PIACK Register    */
#define	PCC2_REC_PIACK		PCC2_ADRS(0x27)	/* Receive PIACK Register     */
#define	PCC2_LANC_ERR_SR	PCC2_ADRS(0x28)	/* LANC Error Status Register */
#define	PCC2_LANC_IRQ_CR	PCC2_ADRS(0x2a)	/* LANC Inter Ctrl Reg	      */
#define	PCC2_LANC_BEICR		PCC2_ADRS(0x2b)	/* LANC Bus Error Inter CR    */
#define	PCC2_SCSI_ERR_SR	PCC2_ADRS(0x2c)	/* SCSI Error Status Register */
#define	PCC2_SCSI_IRQ_CR	PCC2_ADRS(0x2f)	/* SCSI Inter Control Reg     */
#define	PCC2_PRINTER_ACK_IRQ	PCC2_ADRS(0x30)	/* Printer ACK Inter Ctrl Reg */
#define	PCC2_PRINTER_FAULT_IRQ	PCC2_ADRS(0x31)	/* Printer FAULT Inter CR     */
#define	PCC2_PRINTER_SEL_IRQ	PCC2_ADRS(0x32)	/* Printer SEL Inter Ctrl Reg */
#define	PCC2_PRINTER_PE_IRQ	PCC2_ADRS(0x33)	/* Printer PE Inter Ctrl Reg  */
#define	PCC2_PRINTER_BUSY_IRQ	PCC2_ADRS(0x34)	/* Printer BUSY Inter CR      */
#define	PCC2_PRINTER_ISR	PCC2_ADRS(0x36)	/* Printer Input Status Reg   */
#define	PCC2_PRINTER_PCR	PCC2_ADRS(0x37)	/* Printer Port Control Reg   */
#define	PCC2_CHIP_SPEED		PCC2_ADRS(0x38)	/* Chip Speed Register (WORD) */
#define	PCC2_PRINTER_DATA	PCC2_ADRS(0x3a)	/* Printer Data Register      */
#define	PCC2_IPLR		PCC2_ADRS(0x3e)	/* Inter Priority Level Reg   */
#define	PCC2_IMLR		PCC2_ADRS(0x3f)	/* Interrupt Mask Level Reg   */

/****************************************************************
 * GCR			0x02	General Control Register	*
 ****************************************************************/

#define	GCR_FAST_ON	0x01	/* Enable fast access for BBRAM		0 */
#define	GCR_FAST_OFF	0x00	/* Disable fast access for BBRAM	0 */
#define	GCR_MIEN_ON	0x02	/* Master Interrupt Enable		1 */
#define GCR_MIEN_OFF	0x00	/* Master Interrupt Enable OFF		1 */
#define	GCR_C040	0x04	/* The CPU is an MC68040		2 */

/****************************************************************
 * VBR			0x03	Vector Base Register		*
 ****************************************************************/

#define	PCC2_INT_PP_BSY		0x0	/* Printer Port-BSY		*/
#define	PCC2_INT_PP_PE		0x1	/* Printer Port-PE		*/
#define	PCC2_INT_PP_SELECT	0x2	/* Printer Port-SELECT		*/
#define	PCC2_INT_PP_FAULT	0x3	/* Printer Port-FAULT		*/
#define	PCC2_INT_PP_ACK		0x4	/* Printer Port-ACK		*/
#define	PCC2_INT_SCSI		0x5	/* SCSI IRQ			*/
#define	PCC2_INT_LANC_ERR	0x6	/* LANC ERR			*/
#define	PCC2_INT_LANC		0x7	/* LANC IRQ			*/
#define	PCC2_INT_TT2		0x8	/* Tick Timer 2 IRQ		*/
#define	PCC2_INT_TT1		0x9	/* Tick Timer 1 IRQ		*/
#define	PCC2_INT_GPIO		0xa	/* GPIO IRQ			*/
#define	PCC2_INT_SERIAL_MODEM	0xb	/* Serial Modem IRQ (DO NOT USE)*/
#define	PCC2_INT_SERIAL_RX	0xc	/* Serial RX IRQ (DO NOT USE)	*/
#define	PCC2_INT_SERIAL_TX	0xd	/* Serial TX IRQ (DO NOT USE)	*/

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

/*************************************************************************
 * GPIICR	0x18	General Purpose Input Interrupt Control Register *
 *************************************************************************/

#define	GPIICR_ICLR	0x08	/* Clear IRQ in edge-sensitive mode	3 */
#define	GPIICR_IEN	0x10	/* Interrupt Enable			4 */
#define	GPIICR_DIS	0x00	/* Interrupt Disable			4 */
#define	GPIICR_INT	0x20	/* Interrupt has occured		5 */
#define	GPIICR_EDGE	0x40	/* Interrupt is Edge sensitive		6 */
#define	GPIICR_LEVEL	0x00	/* Interrupt is Level sensitive		6 */
#define	GPIICR_HIGH_LOW	0x80	/* Interrupt is High to Low (Falling)	7 */
#define	GPIICR_LOW_HIGH	0x00	/* Interrupt is Low to High (rising)	7 */

/**************************************************************************
 * GPIOCR	0x19	General Purpose Input/Output Pin Control Register *
 **************************************************************************/

#define	GPIOCR_GPO_HIGH	0x01	/* Set GPIO Pin to a HIGH		0 */
#define	GPIOCR_GPO_LOW	0x00	/* Set GPIO Pin to a LOW		0 */
#define	GPIOCR_GPOE	0x02	/* Set GPIO Pin as an Output		1 */
#define	GPIOCR_GPI	0x04	/* Status of the GPIO bit		2 */

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

/****************************************************************
 * SCC_ERR_SR		0x1c	SCC Error Status Register	*
 ****************************************************************/

#define	SCC_ERR_SR_SCLR	0x01	/* Clear Error Status Bits		0 */
#define	SCC_ERR_SR_LTO	0x02	/* Local Time out error			1 */
#define	SCC_ERR_SR_EXT	0x04	/* VMEbus error				2 */
#define	SCC_ERR_SR_PRTY	0x08	/* DRAM Parity Error			3 */
#define	SCC_ERR_SR_RTRY	0x10	/* Retry was needed			4 */

/************************************************************************
 * SCC_MICR		0x1d	SCC Modem Interrupt Control Register	*
 ************************************************************************/

#define	SCC_MICR_AVEC	0x08	/* The PCC supplies IVEC (do not use)	3 */
#define	SCC_MICR_IEN	0x10	/* Interrupt Enable			4 */
#define	SCC_MICR_DIS	0x00	/* Interrupt Disable			4 */
#define	SCC_MICR_IRQ	0x20	/* An Interrupt has occured		5 */

/************************************************************************
 * SCC_TICR		0x1e	SCC Transmit Interrupt Control Register	*
 ************************************************************************/

#define	SCC_TICR_AVEC	0x08	/* The PCC supplies IVEC (do no use)	3 */
#define	SCC_TICR_IEN	0x10	/* Interrupt Enable			4 */
#define	SCC_TICR_DIS	0x00	/* Interrupt Disable			4 */
#define	SCC_TICR_IRQ	0x20	/* An Interrupt has occured		5 */

/************************************************************************
 * SCC_RICR		0x1f	SCC Receive Interrupt Control Register	*
 ************************************************************************/

#define	SCC_RICR_AVEC		0x08	/* PCC supply IVEC (do not use)	3 */
#define	SCC_RICR_IEN		0x10	/* Interrupt Enable		4 */
#define	SCC_RICR_DIS		0x00	/* Interrupt Disable		4 */
#define	SCC_RICR_IRQ		0x20	/* An Intrrupt has occured	5 */
#define	SCC_RICR_NO_SNOOP	0x00	/* Inhibit Snoop		7-6 */
#define	SCC_RICR_SINK_DATA	0x40	/* Sink Data 			7-6 */
#define	SCC_RICR_INVALIDATE	0x80	/* Invalidate Line		7-6 */

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

/****************************************************************
 * SCSI_IRQ_CR		0x2f	SCSI Interrupt Control Register	*
 ****************************************************************/

#define	SCSI_IRQ_CR_IEN		0x10	/* Interrupt Enable		4 */
#define	SCSI_IRQ_CR_DIS		0x00	/* Interrupt Disable		4 */
#define	SCSI_IRQ_CR_IRQ		0x20	/* Interrupt Status		5 */

/***********************************************************************
 * PRINTER_ACK_IRQ	0x30	Printer ACK Interrupt Control Register	*
 ************************************************************************/

#define	PRINTER_ACK_IRQ_ICLR	0x08	/* Clear IRQ in edge mode	3 */
#define PRINTER_ACK_IRQ_IEN	0x10	/* Interrupt Enable		4 */
#define	PRINTER_ACK_IRQ_DIS	0x00	/* Interrupt Disable		4 */
#define	PRINTER_ACK_IRQ_INT	0x20	/* Interrupt Status		5 */
#define	PRINTER_ACK_IRQ_EDGE	0x40	/* Edge Sensitive IRQ		6 */
#define	PRINTER_ACK_IRQ_LEVEL	0x00	/* Level Sensitive IRQ		6 */
#define	PRINTER_ACK_IRQ_HIGH_LOW 0x00	/* IRQ on HIGH to LOW		7 */
#define	PRINTER_ACK_IRQ_LOW_HIGH 0x80	/* IRQ on LOW to HIGH		7 */

/*************************************************************************
 * PRINTER_FAULT_IRQ	0x31	Printer FAULT Interrupt Control Register *
 *************************************************************************/

#define	PRINTER_FAULT_ICLR	0x08	/* Clear IRQ in edge mode	3 */
#define	PRINTER_FAULT_IEN	0x10	/* Interrupt Enable		4 */
#define	PRINTER_FAULT_IRQ_DIS	0x00	/* Interrupt Disable		4 */
#define	PRINTER_FAULT_INT	0x20	/* Interrupt Status		5 */
#define	PRINTER_FAULT_EDGE	0x40	/* Edge Sensitive IRQ		6 */
#define	PRINTER_FAULT_LEVEL	0x00	/* Level Sensitive IRQ		6 */
#define	PRINTER_FAULT_HIGH_LOW	0x00	/* IRQ on HIGH to LOW		7 */
#define	PRINTER_FAULT_LOW_HIGH	0x80	/* IRQ on LOW to HIGH		7 */

/************************************************************************
 * PRINTER_SEL_IRQ	0x32	Printer SEL Interrupt Control Register	*
 ************************************************************************/

#define	PRINTER_SEL_IRQ_ICLR	0x08	/* Clear IRQ in edge mode	3 */
#define	PRINTER_SEL_IRQ_IEN	0x10	/* Interrupt Enable		4 */
#define	PRINTER_SEL_IRQ_DIS	0x00	/* Interrupt Disable		4 */
#define	PRINTER_SEL_IRQ_INT	0x20	/* Interrupt Status		5 */
#define	PRINTER_SEL_IRQ_EDGE	0x40	/* Edge Sensitive IRQ		6 */
#define	PRINTER_SEL_IRQ_LEVEL	0x00	/* Level Sensitive IRQ		6 */
#define	PRINTER_SEL_IRQ_HIGH_LOW 0x80	/* IRQ on HIGH to LOW		7 */
#define	PRINTER_SEL_IRQ_LOW_HIGH 0x00	/* IRQ on LOW to HIGH (rising)	7 */

/************************************************************************
 * PRINTER_PE_IRQ	0x33	Printer PE Interrupt Control Register	*
 ************************************************************************/

#define	PRINTER_PE_IRQ_ICLR	0x08	/* Clear IRQ in edge mode	3 */
#define	PRINTER_PE_IRQ_IEN	0x10	/* Interrupt Enable		4 */
#define	PRINTER_PE_IRQ_DIS	0x00	/* Interrupt Disable		4 */
#define	PRINTER_PE_IRQ_INT	0x20	/* Interrupt Status		5 */
#define	PRINTER_PE_IRQ_EDGE	0x40	/* Edge Sensitive IRQ		6 */
#define	PRINTER_PE_IRQ_LEVEL	0x00	/* Level Sensitive IRQ		6 */
#define	PRINTER_PE_IRQ_HIGH_LOW	0x80	/* IRQ on HIGH to LOW (falling)	7 */
#define	PRINTER_PE_IRQ_LOW_HIGH	0x00	/* IRQ on LOW to HIGH (rising)	7 */

/************************************************************************
 * PRINTER_BUSY_IRQ	0x34	Printer BUSY Interrupt Control Register	*
 ************************************************************************/

#define	PRINTER_BUSY_IRQ_ICLR	0x08	/* Clear IRQ in edge mode	3 */
#define	PRINTER_BUSY_IRQ_IEN	0x10	/* Interrupt Enable		4 */
#define	PRINTER_BUSY_IRQ_DIS	0x00	/* Interrupt Disable		4 */
#define	PRINTER_BUSY_IRQ_INT	0x20	/* Interrupt Status		5 */
#define	PRINTER_BUSY_IRQ_EDGE	0x40	/* Edge Sensitive IRQ		6 */
#define	PRINTER_BUSY_IRQ_LEVEL	0x00	/* Level Sensitive IRQ		6 */
#define	PRINTER_BUSY_IRQ_HIGH_LOW 0x80	/* IRQ on HIGH to LOW (falling)	7 */
#define	PRINTER_BUSY_IRQ_LOW_HIGH 0x00	/* IRQ on LOW to HIGH (rising)	7 */

/****************************************************************
 * PRINTER_ISR		0x36	Printer Input Status Register	*
 ****************************************************************/

#define	PRINTER_ISR_BSY		0x01	/* Printer Busy input pin	0 */
#define	PRINTER_ISR_PE		0x02	/* Printer Paper Error input	1 */
#define	PRINTER_ISR_SEL		0x04	/* Printer Select input		2 */
#define	PRINTER_ISR_FLT		0x08	/* Printer FAULT input		3 */
#define	PRINTER_ISR_ACK		0x10	/* Printer Acknowledge input	4 */
#define	PRINTER_ISR_PINT	0x80	/* Printer Interrupt Status	7 */

/****************************************************************
 * PRINTER_PCR		0x37	Printer Port Control Register	*
 ****************************************************************/

#define	PRINTER_PCR_MAN		0x01	/* Manual Strobe Control	0 */
#define	PRINTER_PCR_AUTO	0x00	/* Automatic Strobe Control	0 */
#define	PRINTER_PCR_FAST	0x02	/* Fast Strobe Time		1 */
#define	PRINTER_PCR_SLOW	0x00	/* Slow Strobe Time		1 */
#define	PRINTER_PCR_STB_ON	0x04	/* Manual Strobe Active		2 */
#define	PRINTER_PCR_STB_OFF	0x00	/* Manual Strobe Off		2 */
#define	PRINTER_PCR_INP_ON	0x08	/* Input Prime Active		3 */
#define	PRINTER_PCR_INP_OFF	0x00	/* Input Prime Off		3 */
#define	PRINTER_PCR_DOEN	0x10	/* Printer Data Output Enable	4 */

#ifdef __cplusplus
}
#endif

#endif /* __INCpccchip2h */
