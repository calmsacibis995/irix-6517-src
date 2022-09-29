/* vic068.h - VMEbus Interface Controller */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01k,22sep92,rrr  added support for c++
01j,26may92,rrr  the tree shuffle
01i,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01h,06may91,nfs  moved ifdef's to ensure clean mangening
01g,30apr91,nfs  added ifdef for documentation
01f,25apr91,nfs  corrected VME_ICGS and VME_ICMS defines (rs33 document error)
		 corrected VME_SET_ADRS and VME_CLR_ADRS (VIC document error)
                 ICR5 defines removed, revisions are referred to by their hex
		 code (f1,f2,f3,f4,f5 and f8 are currently valid)
01e,25mar91,del  re-definition of VIC_ADRS and VME_ICF_ADRS permitted
01d,25mar91,nfs  re-written
            jcf
01c,25nov90,gae  WRS cleanup.
01b,22mar90,eve	 general cleanup
01a,12mar89,fwa	 written.
*/

#ifdef DOC
#define INCvic068h
#endif	/* DOC */

#ifndef __INCvic068h
#define __INCvic068h

#ifdef __cplusplus
extern "C" {
#endif

/*
This file contains constants for the VTC VMEbus Interface controller.
The macro VIC_BASE_ADRS must be defined when including this header.

The following standards have been adopted in the creation of this file.

Register definitions which provide access to the VIC chip locally
(on-board) have the prefix "VIC".  Register definitions which provide
access to the VIC chip via the VMEbus (off-board) have the prefix
"VME".

The registers are listed in ascending (numerical) order; the definitions
for each register are started with a header eg.

 Register    Register Register          Register
 Mnemonic     Number  Address             Name
    |           |        |                 |
    v           v        v                 v
*****************************************************************
* VIICR        0x01    0x03     VME Interrupter Irq Control Reg *
*****************************************************************

in some cases where a number of registers have the same definitions the
header looks like this eg.

****************************************************************
* VICR1        0x02    0x07     VME Interrupt Control Reg #1   *
* to                                                           *
* VICR7        0x08    0x1f     VME Interrupt Control Reg #7   *
****************************************************************

The format of the definitions is as follows; the define name always
starts with the register mnemonic it is associated with. The [7-0]
number at the end of the comment indicates the bit position the define
applies to; the definition ICGICR_IRQ_EN_ICGS3, in the following
example, the 7 applies to bit 7
	   |								|
	   v								v
 define	ICGICR_IRQ_EN_ICGS3	0x00	 * enable  ICGS3 local int	7 *

If the define applies to more than one bit, then the applicable bit
range is specified by two digits; in the following example ICGICR_IRQ_EN_ICGS,
applies to the four bit field, bits 7-4.
									|
									v
 define	ICGICR_IRQ_EN_ICGS	0x00	 * enable  all ICGS local int's 74 *

If no bit field is given, then the define applies to the whole register.
*/

#ifdef	_ASMLANGUAGE
#define CAST
#else
/* #define CAST	(UCHAR *) */
#define CAST (char *)
#endif	/* _ASMLANGUAGE */

/* on-board access, register definitions */

#define	VIC_REG_INTERVAL	4

#ifndef VIC_ADRS	/* to permit alternative board addressing */
#define VIC_ADRS(reg)   (CAST (VIC_BASE_ADRS + (reg * VIC_REG_INTERVAL) + 3))
#endif	/* VIC_ADRS */

#define VIC_VIICR	VIC_ADRS(0x00)	/* VME Interrupter Irq Control Reg */
#define VIC_VICR1	VIC_ADRS(0x01)	/* VME Interrupt Control Reg #1    */
#define VIC_VICR2	VIC_ADRS(0x02)	/* VME Interrupt Control Reg #2    */
#define VIC_VICR3	VIC_ADRS(0x03)	/* VME Interrupt Control Reg #3    */
#define VIC_VICR4	VIC_ADRS(0x04)	/* VME Interrupt Control Reg #4    */
#define VIC_VICR5	VIC_ADRS(0x05)	/* VME Interrupt Control Reg #5    */
#define VIC_VICR6	VIC_ADRS(0x06)	/* VME Interrupt Control Reg #6    */
#define VIC_VICR7	VIC_ADRS(0x07)	/* VME Interrupt Control Reg #7    */
#define VIC_DSICR	VIC_ADRS(0x08)	/* DMA Status Int Control Reg      */
#define VIC_LICR1	VIC_ADRS(0x09)	/* Local Interrupt Control Reg #1  */
#define VIC_LICR2	VIC_ADRS(0x0a)	/* Local Interrupt Control Reg #2  */
#define VIC_LICR3	VIC_ADRS(0x0b)	/* Local Interrupt Control Reg #3  */
#define VIC_LICR4	VIC_ADRS(0x0c)	/* Local Interrupt Control Reg #4  */
#define VIC_LICR5	VIC_ADRS(0x0d)	/* Local Interrupt Control Reg #5  */
#define VIC_LICR6	VIC_ADRS(0x0e)	/* Local Interrupt Control Reg #6  */
#define VIC_LICR7	VIC_ADRS(0x0f)	/* Local Interrupt Control Reg #7  */
#define VIC_ICGICR	VIC_ADRS(0x10)	/* ICGS Interrupt Control Reg      */
#define VIC_ICMICR	VIC_ADRS(0x11)	/* ICMS Interrupt Control Reg      */
#define VIC_EGICR	VIC_ADRS(0x12)	/* Error Group Int Control Reg     */
#define VIC_ICGIVBR	VIC_ADRS(0x13)	/* ICGS Interrupt Vector Base Reg  */
#define VIC_ICMIVBR	VIC_ADRS(0x14)	/* ICMS Interrupt Vector Base Reg  */
#define VIC_LIVBR	VIC_ADRS(0x15)	/* Local Int Vector Base Reg       */
#define VIC_EGIVBR	VIC_ADRS(0x16)	/* Error Group Int Vec Base Reg    */
#define VIC_ICSR	VIC_ADRS(0x17)	/* Interprocessor Comm Switch Reg  */
#define VIC_ICR0	VIC_ADRS(0x18)	/* Interprocessor Comm. Reg #0     */
#define VIC_ICR1	VIC_ADRS(0x19)	/* Interprocessor Comm. Reg #1     */
#define VIC_ICR2	VIC_ADRS(0x1a)	/* Interprocessor Comm. Reg #2     */
#define VIC_ICR3	VIC_ADRS(0x1b)	/* Interprocessor Comm. Reg #3     */
#define VIC_ICR4	VIC_ADRS(0x1c)	/* Interprocessor Comm. Reg #4     */
#define VIC_ICR5	VIC_ADRS(0x1d)	/* Interprocessor Comm. Reg #5     */
#define VIC_ICR6	VIC_ADRS(0x1e)	/* Interprocessor Comm. Reg #6     */
#define VIC_ICR7	VIC_ADRS(0x1f)	/* Interprocessor Comm. Reg #7     */
#define VIC_VIRSR	VIC_ADRS(0x20)	/* VME Interrupt Request/Stat. Reg */
#define VIC_VIVR1	VIC_ADRS(0x21)	/* VME Interrupt Vector Reg #1     */
#define VIC_VIVR2	VIC_ADRS(0x22)	/* VME Interrupt Vector Reg #2     */
#define VIC_VIVR3	VIC_ADRS(0x23)	/* VME Interrupt Vector Reg #3     */
#define VIC_VIVR4	VIC_ADRS(0x24)	/* VME Interrupt Vector Reg #4     */
#define VIC_VIVR5	VIC_ADRS(0x25)	/* VME Interrupt Vector Reg #5     */
#define VIC_VIVR6	VIC_ADRS(0x26)	/* VME Interrupt Vector Reg #6     */
#define VIC_VIVR7	VIC_ADRS(0x27)	/* VME Interrupt Vector Reg #7     */
#define VIC_TTR		VIC_ADRS(0x28)	/* Transfer Time-out Reg           */
#define VIC_LBTR	VIC_ADRS(0x29)	/* Local Bus Timing Reg            */
#define VIC_BTDR	VIC_ADRS(0x2a)	/* Block Transfer Definition Reg   */
#define VIC_ICR		VIC_ADRS(0x2b)	/* Interface Configuration Reg     */
#define VIC_ARCR	VIC_ADRS(0x2c)	/* Arbiter/Requester Config. Reg   */
#define VIC_AMSR	VIC_ADRS(0x2d)	/* AM Source Reg                   */
#define VIC_BESR	VIC_ADRS(0x2e)	/* Bus Error Status Reg            */
#define VIC_DSR		VIC_ADRS(0x2f)	/* DMA Status Reg                  */
#define VIC_SS0CR0	VIC_ADRS(0x30)	/* Slave Select 0/Control Reg #0   */
#define VIC_SS0CR1	VIC_ADRS(0x31)	/* Slave Select 0/Control Reg #1   */
#define VIC_SS1CR0	VIC_ADRS(0x32)	/* Slave Select 1/Control Reg #0   */
#define VIC_SS1CR1	VIC_ADRS(0x33)	/* Slave Select 1/Control Reg #1   */
#define VIC_RCR		VIC_ADRS(0x34)	/* Release Control Reg             */
#define VIC_BTCR	VIC_ADRS(0x35)	/* Block Transfer Control Reg      */
#define VIC_BTLR0	VIC_ADRS(0x36)	/* Block Transfer Length Reg #0    */
#define VIC_BTLR1	VIC_ADRS(0x37)	/* Block Transfer Length Reg #1    */
#define VIC_SRR		VIC_ADRS(0x38)	/* System Reset Reg                */

/* off-board access, register definitions */

#define	VME_ICF_REG_INTERVAL	2

#ifndef VME_ICF_ADRS	/* to permit alternative off-board addressing */
#define VME_ICF_ADRS(adrs, icf, type, reg) \
		(CAST (adrs + icf + (reg * VME_ICF_REG_INTERVAL) + type ))
#endif	/* VME_ICF_ADRS */

/* Interprocessor Communication Facility - icf */

#define VME_ICR		0x00	/* IC Registers       ICR0-7  */
#define VME_ICGS	0x10	/* IC Global Switches ICGS0-3 */
#define VME_ICMS	0x20	/* IC Module Switches ICMS0-3 */

/* Type of register access - type */

#define VME_ODD_ADRS	0x01	/* ICR always accessed at odd byte addresses */

#define VME_SET_ADRS	0x01	/* ICGS and ICMS set    switch access */
#define VME_CLR_ADRS	0x00	/* ICGS and ICMS clear  switch access */

#define	VME_ICR0(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,0)
#define	VME_ICR1(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,1)
#define	VME_ICR2(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,2)
#define	VME_ICR3(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,3)
#define	VME_ICR4(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,4)
#define	VME_ICR5(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,5)
#define	VME_ICR6(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,6)
#define	VME_ICR7(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICR,VME_ODD_ADRS,7)

#define	VME_ICGS0_SET(baseAdrs) VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_SET_ADRS,0)
#define	VME_ICGS0_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_CLR_ADRS,0)
#define	VME_ICGS1_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_SET_ADRS,1)
#define	VME_ICGS1_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_CLR_ADRS,1)
#define	VME_ICGS2_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_SET_ADRS,2)
#define	VME_ICGS2_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_CLR_ADRS,2)
#define	VME_ICGS3_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_SET_ADRS,3)
#define	VME_ICGS3_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICGS,VME_CLR_ADRS,3)

#define	VME_ICMS0_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_SET_ADRS,0)
#define	VME_ICMS0_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_CLR_ADRS,0)
#define	VME_ICMS1_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_SET_ADRS,1)
#define	VME_ICMS1_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_CLR_ADRS,1)
#define	VME_ICMS2_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_SET_ADRS,2)
#define	VME_ICMS2_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_CLR_ADRS,2)
#define	VME_ICMS3_SET(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_SET_ADRS,3)
#define	VME_ICMS3_CLR(baseAdrs)	VME_ICF_ADRS(baseAdrs,VME_ICMS,VME_CLR_ADRS,3)

/****************************************************************
* VIICR        0x00    0x03     VME Interrupter Irq Control Reg *
*****************************************************************/

#define	VIICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	7 */
#define	VIICR_IRQ_DISABLE	0x80	/* disable interrupter IRQ      7 */

/***************************************************************
* VICR1        0x01    0x07     VME Interrupt Control Reg #1   *
* to                                                           *
* VICR7        0x07    0x1f     VME Interrupt Control Reg #7   *
****************************************************************/

#define	VICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	7 */
#define	VICR_IRQ_DISABLE	0x80	/* disable interrupt IRQ	7 */

/***************************************************************
* DSICR        0x08    0x23     DMA Status Int Control Reg     *
****************************************************************/

#define	DSICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	7 */
#define	DSICR_IRQ_DISABLE	0x80	/* disable DMA termination IRQ	7 */

/***************************************************************
* LICR1        0x09    0x27     Local Interrupt Control Reg #1 *
* to                                                           *
* LICR7        0x0f    0x3f     Local Interrupt Control Reg #7 *
****************************************************************/

#define	LICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	  */
#define	LICR_IRQ_STATE		0x08	/* LIRQ(i) state		3 */
#define	LICR_IRQ_VEC_DEV	0x00	/* device supplies vector	4 */
#define	LICR_IRQ_VEC_VIC	0x10	/* VIC supplies vector		4 */
#define	LICR_IRQ_LEVEL		0x00	/* level sensitive		5 */
#define	LICR_IRQ_EDGE		0x20	/* edge sensitive		5 */
#define	LICR_IRQ_ACTIVEL	0x00	/* active low			6 */
#define	LICR_IRQ_ACTIVEH	0x40	/* active high			6 */
#define	LICR_IRQ_ENABLE		0x00	/* enable  IRQ			7 */
#define	LICR_IRQ_DISABLE	0x80	/* disable IRQ			7 */

/***************************************************************
* ICGICR       0x10    0x43     ICGS Interrupt Control Reg     *
****************************************************************/

#define	ICGICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	  */
#define	ICGICR_IRQ_ICGS0_ENA	0x00	/* enable  ICGS0 local interrpt 4 */
#define	ICGICR_IRQ_ICGS0_DIS	0x10	/* disable ICGS0 local interrpt 4 */
#define	ICGICR_IRQ_ICGS1_ENA	0x00	/* enable  ICGS1 local interrpt 5 */
#define	ICGICR_IRQ_ICGS1_DIS    0x20	/* disable ICGS1 local interrpt 5 */
#define	ICGICR_IRQ_ICGS2_ENA	0x00	/* enable  ICGS2 local interrpt 6 */
#define	ICGICR_IRQ_ICGS2_DIS	0x40	/* disable ICGS2 local interrpt 6 */
#define	ICGICR_IRQ_ICGS3_ENA	0x00	/* enable  ICGS3 local interrpt 7 */
#define	ICGICR_IRQ_ICGS3_DIS	0x80	/* disable ICGS3 local interrpt 7 */

/***************************************************************
* ICMICR       0x11    0x47     ICMS Interrupt Control Reg     *
****************************************************************/

#define	ICMICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	  */
#define	ICMICR_IRQ_ICMS0_DIS	0x10	/* disable ICMS0 local interrpt 4 */
#define	ICMICR_IRQ_ICMS1_DIS	0x20	/* disable ICMS1 local interrpt 5 */
#define	ICMICR_IRQ_ICMS2_DIS	0x40	/* disable ICMS2 local interrpt 6 */
#define	ICMICR_IRQ_ICMS3_DIS	0x80	/* disable ICMS3 local interrpt 7 */

/***************************************************************
* EGICR        0x12    0x4b     Error Group Int Control Reg    *
****************************************************************/

#define	EGICR_IRQ_LVL_MASK	0x07	/* local interrupt level mask	  */
#define	EGICR_IRQ_SYSFAIL_ENA	0x00	/* enable  SYSFAIL* local int	4 */
#define	EGICR_IRQ_SYSFAIL_DIS	0x10	/* disable SYSFAIL* local int	4 */
#define	EGICR_IRQ_ABTTO_ENA	0x00	/* enable  arbitration time out	5 */
#define	EGICR_IRQ_ABTTO_DIS	0x20	/* disable arbitration time out	5 */
#define	EGICR_IRQ_WPFAIL_ENA	0x00	/* enable  write post fail	6 */
#define	EGICR_IRQ_WPFAIL_DIS	0x40	/* disable write post fail	6 */
#define	EGICR_IRQ_ACFAIL_ENA	0x00	/* enable  ACFAIL* local int	7 */
#define	EGICR_IRQ_ACFAIL_DIS	0x80	/* disable ACFAIL* local int	7 */

/***************************************************************
* ICGIVBR      0x13    0x4f     ICGS Interrupt Vector Base Reg *
****************************************************************/

/* Identity of interrupting ICGS (read only)	*/

#define	ICGIVBR_ICGS0		0x00	/* ICGS0		10 */
#define	ICGIVBR_ICGS1		0x01	/* ICGS1		10 */
#define	ICGIVBR_ICGS2		0x02	/* ICGS2		10 */
#define	ICGIVBR_ICGS3		0x03	/* ICGS3		10 */
#define	ICGIVBR_VEC_MASK	0xfc	/* ICGS vector base mask   */

/***************************************************************
* ICMIVBR      0x14    0x53     ICMS Interrupt Vector Base Reg *
****************************************************************/

/* Identity of interrupting ICMS (read only)	*/

#define	ICMIVBR_ICMS0		0x00	/* ICMS0		10 */
#define	ICMIVBR_ICMS1		0x01	/* ICMS1		10 */
#define	ICMIVBR_ICMS2		0x02	/* ICMS2		10 */
#define	ICMIVBR_ICMS3		0x03	/* ICMS3		10 */
#define	ICMIVBR_VEC_MASK	0xfc	/* ICMI vector base mask   */

/***************************************************************
* LIVBR        0x15    0x57     Local Int Vector Base Reg      *
****************************************************************/

/* Identity of local interrupt (read only)	*/

#define	LIVBR_LIRQ1		0x01	/* LIRQ1		20 */
#define	LIVBR_LIRQ2		0x02	/* LIRQ2		20 */
#define	LIVBR_LIRQ3		0x03	/* LIRQ3		20 */
#define	LIVBR_LIRQ4		0x04	/* LIRQ4		20 */
#define	LIVBR_LIRQ5		0x05	/* LIRQ5		20 */
#define	LIVBR_LIRQ6		0x06	/* LIRQ6		20 */
#define	LIVBR_LIRQ7		0x07	/* LIRQ7		20 */
#define	LIVBR_VEC_MASK		0xf8	/* LIRQ vector base mask   */

/***************************************************************
* EGIVBR       0x16    0x5b     Error Group Int Vec Base Reg   *
****************************************************************/

/* Error Conditions (read only)	*/

#define	EGIVBR_ACFAIL		0x00	/* ACFAIL* error condition	20 */
#define	EGIVBR_WPFAIL		0x01	/* write post fail error cond	20 */
#define	EGIVBR_ABTTO		0x02	/* arbitration time out err con	20 */
#define	EGIVBR_SYSFAIL		0x03	/* SYSFAIL* error condition	20 */
#define	EGIVBR_VIIA		0x04	/* VMEbus Interrupter Interrupt	20 */
					/* Acknowledge error condition	20 */
#define	EGIVBR_DMAI		0x05	/* DMA Interrupt		20 */
#define	EGIVBR_VEC_MASK		0xf8	/* EGIRQ vector base mask   */

/***************************************************************
* ICSR         0x17    0x5f     Interprocessor Comm Switch Reg *
****************************************************************/

#define ICSR_ICMS0		0x01	/* set   ICMS0			 0 */
#define ICSR_ICMS1		0x02	/* set   ICMS1			 1 */
#define ICSR_ICMS2		0x04	/* set   ICMS2			 2 */
#define ICSR_ICMS3		0x08	/* set   ICMS3			 3 */

/* ICGS (read only) */

#define ICSR_ICGS0		0x10	/* set   ICGS0			 4 */
#define ICSR_ICGS1		0x20	/* set   ICGS1			 5 */
#define ICSR_ICGS2		0x40	/* set   ICGS2			 6 */
#define ICSR_ICGS3		0x80	/* set   ICGS3			 7 */

/***************************************************************
* ICR0         0x18    0x63     Interprocessor Comm. Reg #0    *
* to                                                           *
* ICR4         0x1c    0x73     Interprocessor Comm. Reg #4    *
****************************************************************/

/* none */

/***************************************************************
* ICR5         0x1d    0x77     Interprocessor Comm. Reg #5    *
*                               (VIC ID Register)	       *
****************************************************************/

/* none - revisions are referred to by the hex code */

/***************************************************************
* ICR6         0x1e    0x7b     Interprocessor Comm. Reg #6    *
****************************************************************/

/* XXX should the following fields be 6 or 7 bits wide ? */

#define ICR6_HALT		0x3d	/* VIC detected VME HALT	  50 */
#define ICR6_IRESET		0x3e	/* VIC performing reset function  50 */
#define ICR6_SYSRESET		0x3f	/* VMEbus SYSRESET		  50 */
#define ICR6_HALT_IRESET	0x40	/* VMEbus HALT or IRESET asserted  6 */
#define ICR6_SYSFAIL		0x40	/* assert SYSFAIL		   6 */
#define ICR6_ACFAIL		0x80	/* ACFAIL state			   7 */

/***************************************************************
* ICR7         0x1f    0x7f     Interprocessor Comm. Reg #7    *
****************************************************************/

#define ICR7_ICR0		0x01	/* ICR0 semaphore		0 */
#define ICR7_ICR1		0x02	/* ICR1 semaphore		1 */
#define ICR7_ICR2		0x04	/* ICR2 semaphore		2 */
#define ICR7_ICR3		0x08	/* ICR3 semaphore		3 */
#define ICR7_ICR4		0x10	/* ICR4 semaphore		4 */
#define ICR7_MASTER    		0x20    /* the VIC is VMEbus master     5 */
#define ICR7_RESET		0x40	/* assert  RESET* and HALT* 	6 */
#define ICR7_SYSFAIL_INH	0x80	/* inhibit SYSFAIL* assertion	7 */

/*****************************************************************
* VIRSR        0x20    0x83     VME Interrupt Request/Status Reg *
******************************************************************/

#define VIRSR_IRQ_ENA		0x01     /* VMEbus interrupt enable 	0 */
#define VIRSR_IRQ1		0x02     /* assert on VMEbus, VME IRQ 1 1 */
#define VIRSR_IRQ2		0x04     /* assert on VMEbus, VME IRQ 2 2 */
#define VIRSR_IRQ3		0x08     /* assert on VMEbus, VME IRQ 3 3 */
#define VIRSR_IRQ4		0x10     /* assert on VMEbus, VME IRQ 4 4 */
#define VIRSR_IRQ5		0x20     /* assert on VMEbus, VME IRQ 5 5 */
#define VIRSR_IRQ6		0x40     /* assert on VMEbus, VME IRQ 6 6 */
#define VIRSR_IRQ7		0x80     /* assert on VMEbus, VME IRQ 7 7 */

/***************************************************************
* VIVR1        0x21    0x87     VME Interrupt Vector Reg #1    *
* to                                                           *
* VIVR7        0x27    0xaf     VME Interrupt Vector Reg #7    *
****************************************************************/

/* none */

/***************************************************************
* TTR          0x28    0xa3     Transfer Time-out Reg          *
****************************************************************/

#define	TTR_VTO_ACQ_EXC		0x00	/* exclude aquisition time	 0 */
#define	TTR_VTO_ACQ_INC		0x01	/* include aquisition time	 0 */
#define	TTR_ARBTO		0x02	/* arbitration timeout occured	 1 */
#define	TTR_LTO_4		0x00	/* local 4 us time out		42 */
#define	TTR_LTO_16		0x04	/* local 16 us time out 	42 */
#define	TTR_LTO_32		0x08	/* local 32 us time out 	42 */
#define	TTR_LTO_64		0x0c	/* local 64 us time out		42 */
#define	TTR_LTO_128		0x10	/* local 128 us time out	42 */
#define	TTR_LTO_256		0x14	/* local 256 us time out	42 */
#define	TTR_LTO_512		0x18	/* local 512 us time out	42 */
#define	TTR_LTO_INF		0x1c	/* local infinite time out	42 */
#define	TTR_VTO_4		0x00	/* VMEbus 4 us time out		75 */
#define	TTR_VTO_16		0x20	/* VMEbus 16 us time out	75 */
#define	TTR_VTO_32		0x40	/* VMEbus 32 us time out	75 */
#define	TTR_VTO_64		0x60	/* VMEbus 64 us time out	75 */
#define	TTR_VTO_128		0x80	/* VMEbus 128 us time out	75 */
#define	TTR_VTO_256		0xa0	/* VMEbus 256 us time out	75 */
#define	TTR_VTO_512		0xc0	/* VMEbus 512 us time out	75 */
#define	TTR_VTO_INF		0xe0	/* VMEbus infinite time out	75 */

/***************************************************************
* LBTR         0x29    0xa7     Local Bus Timing Reg           *
****************************************************************/

#define	LBTR_PASL_2		0x00	/* PAS low 2 * 64 MHz clocks	30 */
#define	LBTR_PASL_3		0x01	/* PAS low 3 * 64 MHz clocks	30 */
#define	LBTR_PASL_4		0x02	/* PAS low 4 * 64 MHz clocks	30 */
#define	LBTR_PASL_5		0x03	/* PAS low 5 * 64 MHz clocks	30 */
#define	LBTR_PASL_6		0x04	/* PAS low 6 * 64 MHz clocks	30 */
#define	LBTR_PASL_7		0x05	/* PAS low 7 * 64 MHz clocks	30 */
#define	LBTR_PASL_8		0x06	/* PAS low 8 * 64 MHz clocks	30 */
#define	LBTR_PASL_9		0x07	/* PAS low 9 * 64 MHz clocks	30 */
#define	LBTR_PASL_10		0x08	/* PAS low 10 * 64 MHz clocks	30 */
#define	LBTR_PASL_11		0x09	/* PAS low 11 * 64 MHz clocks	30 */
#define	LBTR_PASL_12		0x0a	/* PAS low 12 * 64 MHz clocks	30 */
#define	LBTR_PASL_13		0x0b	/* PAS low 13 * 64 MHz clocks	30 */
#define	LBTR_PASL_14		0x0c	/* PAS low 14 * 64 MHz clocks	30 */
#define	LBTR_PASL_15		0x0d	/* PAS low 15 * 64 MHz clocks	30 */
#define	LBTR_PASL_16		0x0e	/* PAS low 16 * 64 MHz clocks	30 */
#define	LBTR_PASL_17		0x0f	/* PAS low 17 * 64 MHz clocks	30 */

#define	LBTR_DS_1		0x00	/* DS 1 * 64 MHz clocks 	 4 */
#define	LBTR_DS_2		0x10	/* DS 2 * 64 MHz clocks 	 4 */

#define	LBTR_PASH_1		0x00	/* PAS high 1 * 64 MHz clocks	75 */
#define	LBTR_PASH_2		0x20	/* PAS high 2 * 64 MHz clocks	75 */
#define	LBTR_PASH_3		0x40	/* PAS high 3 * 64 MHz clocks	75 */
#define	LBTR_PASH_4		0x60	/* PAS high 4 * 64 MHz clocks	75 */
#define	LBTR_PASH_5		0x80	/* PAS high 5 * 64 MHz clocks	75 */
#define	LBTR_PASH_6		0xa0	/* PAS high 6 * 64 MHz clocks	75 */
#define	LBTR_PASH_7		0xc0	/* PAS high 7 * 64 MHz clocks	75 */
#define	LBTR_PASH_8		0xe0	/* PAS high 8 * 64 MHz clocks	75 */

/***************************************************************
* BTDR         0x2a    0xab     Block Transfer Definition Reg  *
****************************************************************/

#define	BTDR_DPADEN		0x01	/* enable dual path DPADEN	0 */
#define	BTDR_AMSR		0x02	/* use AMSR for BT add modifier 1 */
#define	BTDR_LBTDMA_ENA		0x04	/* enable local BT 256 byte DMA 2 */
#define	BTDR_VBTDMA_ENA		0x08    /* enable  VME BT 256 byte DMA	3 */

/***************************************************************
* ICR          0x2b    0xaf     Interface Configuration Reg    *
****************************************************************/

#define	ICR_SCON		0x01	/* system controller pin           0 */
#define	ICR_TURBO		0x02	/* speed up transfers              1 */
#define	ICR_MORE_SETTLE_TIME	0x04	/* add 1 64 MHz intvl to async pins2 */
#define	ICR_DLK_RMC_NO_RETRY	0x08	/* enable deadlock signalling      3 */
#define	ICR_DLK_RETRY		0x10	/* signal deadlock with HALT*      4 */
#define	ICR_REQ_RMC		0x20	/* request bus if RMC* is asserted 5 */
#define	ICR_AS_STRETCH		0x40	/* strech AS*                      6 */
#define	ICR_AS_SIZE		0x80	/* use size information for AS*    7 */

/***************************************************************
* ARCR         0x2c    0xb3     Arbiter/Requester Config. Reg  *
****************************************************************/

#define	ARCR_NOT_FAIR		0x00	/* disable fairness               30 */
#define	ARCR_NO_TOUT		0x0f	/* disable timeout                30 */
#define	ARCR_DRAM_REFRESH	0x10	/* enable DRAM refresh             4 */

#define	ARCR_VBRL_BR0		0x00	/* VME bus request level BR0      65 */
#define	ARCR_VBRL_BR1		0x20	/* VME bus request level BR1      65 */
#define	ARCR_VBRL_BR2		0x40	/* VME bus request level BR2      65 */
#define	ARCR_VBRL_BR3		0x60	/* VME bus request level BR3      65 */

#define	ARCR_RND_ROBIN		0x00	/* select round robin arbitration  7 */
#define	ARCR_PRIORITY		0x80	/* select priority arbitration     7 */

/***************************************************************
* AMSR         0x2d    0xb7     AM Source Reg                  *
****************************************************************/

	/* Extended (A32) address modifier codes   */

#define AMSR_EXT_USR_DATA	0x09	/* user data		       50 */
#define AMSR_EXT_USR_PROG	0x0a	/* user program	access	       50 */
#define AMSR_EXT_USR_BLOCK	0x0b	/* user block transfer access  50 */
#define AMSR_EXT_SPR_DATA	0x0d	/* supervisory data access     50 */
#define AMSR_EXT_SPR_PROG	0x0e	/* supervisory program access  50 */
#define AMSR_EXT_SPR_BLOCK	0x0f	/* super block transfer access 50 */

	/* AMSR_Short (A16) address modifier codes      */

#define AMSR_SHT_USR_ACCESS	0x29	/* user access                 50 */
#define AMSR_SHT_SPR_ACCESS	0x2d	/* supervisory access          50 */

	/* AMSR_Standard (A24) address modifier codes   */

#define AMSR_STD_USR_DATA	0x39    /* user data                   50 */
#define AMSR_STD_USR_PROG	0x3a    /* user program access         50 */
#define AMSR_STD_USR_BLOCK	0x3b    /* user block transfer access  50 */
#define AMSR_STD_SPR_DATA	0x3d    /* supervisory data access     50 */
#define AMSR_STD_SPR_PROG	0x3e    /* supervisory program access  50 */
#define AMSR_STD_SPR_BLOCK	0x3f    /* super block transfer access 50 */

	/* AMSR_during slave access/cycles   */

#define AMSR_SLV_CMP_ALL	0x00	/* all 6 add modifier bits comp	   6 */
#define AMSR_SLV_CMP_3MSB	0x40	/* only 3 most significant add	   6 */
					/* modifier bits compared	     */

	/* Master access/cycles	     */

#define AMSR_MSTR_CMP_ALL	0x00	/* all 6 add modifier bits used	   7 */
#define AMSR_MSTR_CMP_3MSB	0x80	/* only 3 most significant add	   7 */
					/* modifier bits used		     */

/***************************************************************
* BESR         0x2e    0xbb     Bus Error Status Reg           *
****************************************************************/

#define BESR_LBTOA	0x01	/* local  bus time out during		0 */
				/* attempted acquisition		  */
#define BESR_SA_SLSEL1	0x02	/* self-access by SLSEL1		1 */
#define BESR_SA_SLSEL0	0x04	/* self-access by SLSEL0		2 */
#define BESR_LBTO	0x08	/* local  bus time out			3 */
#define BESR_VBTO	0x10	/* VMEbus time out (bus master)		4 */
#define BESR_VBERR	0x20	/* VMEbus bus error BERR*		5 */
#define BESR_LBERR	0x40	/* local  bus error LBERR*		6 */
#define BESR_MASTER	0x80	/* the VIC is VMEbus master		7 */

/***************************************************************
* DSR          0x2f    0xbf     DMA Status Reg                 *
****************************************************************/

#define DSR_INTER_DMA	0x01	/* interleaved DMA in progress		0 */
#define DSR_LBERR_DMA	0x02	/* local  bus error LBERR* during DMA	1 */
#define DSR_VBERR_DMA	0x04	/* VMEbus bus error BERR*  during DMA	2 */
#define DSR_LBERR	0x08	/* local  bus error LBERR* asserted    	3 */
#define DSR_VBERR	0x10	/* VMEbus bus error BERR*  asserted	4 */
#define DSR_BTDMA	0x60	/* VME/local 256 byte address error    65 */
#define	DSR_MASTER_WP	0x80	/* master write post info is stored	7 */

/****************************************************************/
/* SS0CR0       0x30    0xc3     Slave Select 0/Control Reg #0  */
/* and								*/
/* SS1CR0       0x32    0xcb     Slave Select 1/Control Reg #0  */
/****************************************************************/

/* SS0CR0 and SS1CR0 */

#define	SSCR0_BLT_NONE		0x00	/* no block transfer allowed	   10 */
#define	SSCR0_BLT_LOCAL		0x01	/* emulate blck trans on local bus 10 */
#define	SSCR0_BLT_ACC		0x02	/* accelerated block transfer      10 */

#define	SSCR0_ASIZ_A32		0x00	/* A32   address size		   32 */
#define	SSCR0_ASIZ_A24		0x04	/* A24   address size		   32 */
#define	SSCR0_ASIZ_A16		0x08	/* A16   address size		   32 */
#define	SSCR0_ASIZ_AMREG	0x0c	/* use Addr Modifier source reg    32 */

#define	SSCR0_SLSEL_D32		0x10	/* D32 SLSEL[01]* data size	    4 */
#define	SSCR0_SLSEL_D16		0x00	/* D32 SLSEL[01]* data size	    4 */

#define	SSCR0_SLSEL_SPR		0x20 /* supervisory SLSEL[01]* access only  5 */
#define	SSCR0_SLSEL_ALL		0x00 /* all modes   SLSEL[01]* access	    5 */

/* SS1CR0 */

#define SSCR0_MASTER_WP     	0x40    /* enable master write posting	    6 */
#define SSCR0_MASTER_NWP     	0x00    /* no     master write posting	    6 */

#define SSCR0_SLAVE_WP      	0x80    /* enable slave  write posting	    7 */
#define SSCR0_SLAVE_NWP      	0x00    /* no     slave  write posting	    7 */

/* SS0CR0 */

#define	SSCR0_TIMER_MASK	0x3f	/* timer field mask                   */
#define	SSCR0_TIMER_DIS		0x00	/* timer disabled		   76 */
#define	SSCR0_TIMER_50		0x40	/* 50  Hz timer (output on LIRQ2*) 76 */
#define	SSCR0_TIMER_1000	0x80	/* 1000 Hz timer (outpt on LIRQ2*) 76 */
#define	SSCR0_TIMER_100		0xc0	/* 100 Hz timer (output on LIRQ2*) 76 */

/***************************************************************
* SS0CR1       0x31    0xc7     Slave Select 0/Control Reg #1  *
* and							       *
* SS1CR1       0x33    0xcf     Slave Select 1/Control Reg #1  *
****************************************************************/

#define	SSCR1_DTACK_0		0x00	/* 0   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_2		0x01	/* 2   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_25		0x02	/* 2.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_3		0x03	/* 3   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_35		0x04	/* 3.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_4		0x05	/* 4   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_45		0x06	/* 4.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_5		0x07	/* 5   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_55		0x08	/* 5.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_6		0x09	/* 6   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_65		0x0a	/* 6.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_7		0x0b	/* 7   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_75		0x0c	/* 7.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_8		0x0d	/* 8   x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_85		0x0e	/* 8.5 x 64 Mhz DTACK* delay	30 */
#define	SSCR1_DTACK_9		0x0f	/* 9   x 64 Mhz DTACK* delay	30 */

#define	SSCR1_DSACK_0		0x00	/* 0   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_2		0x10	/* 2   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_25		0x20	/* 2.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_3		0x30	/* 3   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_35		0x40	/* 3.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_4		0x50	/* 4   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_45		0x60	/* 4.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_5		0x70	/* 5   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_55		0x80	/* 5.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_6		0x90	/* 6   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_65		0xa0	/* 6.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_7		0xb0	/* 7   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_75		0xc0	/* 7.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_8		0xd0	/* 8   x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_85		0xe0	/* 8.5 x 64 Mhz DTACK* delay	74 */
#define	SSCR1_DSACK_9		0xf0	/* 9   x 64 Mhz DTACK* delay	74 */

/***************************************************************
* RCR          0x34    0xd3     Release Control Reg            *
****************************************************************/

#define	RCR_ROR			0x00	/* release on request		76 */
#define	RCR_RWD			0x40	/* release when done		76 */
#define	RCR_ROC			0x80	/* release on BCLR*		76 */
#define	RCR_BCAP		0xc0	/* bus capture and hold		76 */

/***************************************************************
* BTCR         0x35    0xd7     Block Transfer Control Reg     *
****************************************************************/

#define	BTCR_LCIP_MASK		0x0f	/* local cycle interl've period mk 30 */

#define	BTCR_DMA_WRITE		0x10	/* write DMA enable		    4 */

	/* Note: bits 7, 6 and 5 are mutually exclusive        */

#define	BTCR_MBT		0x20	/* enable   MOVEM type block trsfer 5 */
#define	BTCR_VDMA_GO		0x40	/* initiate VME   DMA block trsfer  6 */
#define	BTCR_LDMA_GO		0x80	/* initiate local DMA block trsfer  7 */

/***************************************************************
* BTLR0        0x36    0xdb     Block Transfer Length Reg #0   *
* and							       *
* BTLR1        0x37    0xdf     Block Transfer Length Reg #1   *
****************************************************************/

/* none */

/***************************************************************
* SRR          0x38    0xe3     System Reset Reg               *
****************************************************************/

#define	SRR_SYSRESET	0xf0	/* assert SYSRESET on the VMEbus and	*/
				/* reset all reset-able VIC registers	*/

#ifdef __cplusplus
}
#endif

#endif /* __INCvic068h */
