/* ivic.h - Ironics/VTCs VME Interface Chip (VIC) constants */

/*
modification history
--------------------
02d,22sep92,rrr  added support for c++
02c,26may92,rrr  the tree shuffle
02b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
02a,31jan91,rjt  initial release to windriver after many minor
                 fixes and alterations.
01a,25jul90,rjt  writen by modifying IV-SPRC sparcmon versions.
*/

#ifndef __INCivich
#define __INCivich

#ifdef __cplusplus
extern "C" {
#endif

/*=========================================================================*/
/*                                                                         */
/* NOTE:                                                                   */
/* assembly language files should turn off type declartions by not         */
/* including those portions of this include file, this is done by          */
/* placing a															   */
/*																		   */
/* #define _ASMLANGUAGE													   */
/*																		   */
/* statement before this file is included in any other files               */
/*                                                                         */
/* Also in order to incorparate stronger typing of all constants, and to   */
/* allow the same constants to be typed in C files,yet usable in           */
/* assembly files. An unorthadox method of TYPING is used by CONDITIONALLY */
/* #define'ing the constant TYPES. If no _ASMLANGUAGE #define is present,   */
/* then the type is defined to a blank space, otherwise it is defined      */
/* to be a C type. This looks weird but works nicely.                      */
/*                                                                         */
/*=========================================================================*/


/*=========================================================================*

Naming of register names and constants was done ( or was attempted ) so that
the front end of the names would indicate a related group while the tail
end of the names would further define the purpose of the name. Some groups
often contained sub-groups. Also some names don't follow this convention for
the sake of readability and other reasons.


An H_ prefix indicates that the constant is typed as a pointer to
unsigned short register.
An B_ prefix indicates that the constant is typed as a pointer to
unsigned char register.

Because of unpredictability of structure sizing and placement for
different compilers, we use contants to access each register.

Most constants are defined for consistancy in name AND intended usage.
BIT MANIPUATION is set up so that each bit field has 3 or more defines
associated with it.

1. BIT VALUE MASK
2. ASSERTED VALUE(S)
3. DEASERTED VALUE

where ASSERTED VALUE = 0 and dasserted value = 0xff for ACTIVE LOW SIGNALS
  OR visa versa
where ASSERTED VALUE = 0xff and dasserted value = 0 for ACTIVE HIGH SIGNALS

NOTE: The active high bits may also be something other than 0xff for
      multi-bit fields.  EG: for at 2 bit active high field there may be
      4 asserted values associated with it: 0x00 0x01 0x02 0x03.


the bit field should be altered as follows:

new_reg_value =   (old_reg_val & ~MASK) | (MASK & VALUE )


REGISTER BIT STATUS for a bit field has 2 or more defines associated with it.

1.  BIT MASK
2.  POSSIBLE BIT VALUE
3.     .
4      .

returned_bit_field_status  =  MASK & reg_value

Most constants are declared immediatly after the register they are used in.
Some constants are shared between more than 1 register and are only defined
once after the first register.


Some of names used here indicatate the following:

virqack - interrupt from the completion of vme irq acknowledge cycle
virq    - interrupt from vme
virqreq - interrupt to vme
lirq    - interrupt from the boards local stuff ie timers, sio etc.
misci   - interrupt from error,dma or vmeack
erri    - interrupt from error register
dmai    - interrupt from dma register
icgsi   - interrupt from icgs facilities
icmsi   - interrupt from icms facilities

======================================================================**/


/* IMPORTANT NOTICE: NOT ALL CONSTANTS HAVE BEEN TESTED */

/* The following #defines allow or disallow C types */
#ifndef _ASMLANGUAGE
/* C code type casting */

#ifndef P_VI_TYPE
#define P_VI_TYPE (unsigned char *)
#endif
#ifndef VI_TYPE
#define VI_TYPE  (unsigned char)
#endif

#else

/* No type casting allowed in assembly */
#define VI_TYPE
#define P_VI_TYPE
#endif	/* _ASMLANGUAGE */


#define VIC_ADR 0xfffc0000

#define VIC_TYPE unsigned char

/* IMPORTANT */
/* Some boards may require a special routine to access the vic registers.    */
/* ( ex: a board whose cache must be bypassed to ensure register integrity ) */
/* If they do, there should be  viXetReg() macros #define'd to call that     */
/* special routine. Otherwise the following viXetReg() macros will be used.  */
/* For portability reasons, it might be a good idea to only use viRegSet()   */
/* to alter any vic registers in C code.				     */
/*								             */
/* #undef the following macro to replace it if needed                        */
#ifndef viRegSet
#define viRegSet(bReg,val) *(bReg) = (VIC_TYPE)(val)
#endif
#ifndef viRegGet
#define viRegGet(bReg) *(bReg)
#endif


/* BEGIN REGISTER CONSTANTS */

/* VME Intr IACK cntl */
#define	B_VIC_VIRQACK_CTL	P_VI_TYPE (VIC_ADR +  0x03)
#define	VIRQACK_MASK	VI_TYPE 0x80
#define	VIRQACK_ENABLE	VI_TYPE 0x0
#define	VIRQACK_DISABLE	VI_TYPE 0x80

#define VIRQACK_IPL_MASK	VI_TYPE 0x07

/* VME IRQ handler cntl lev 1-7 */
#define B_VIC_VIRQ1_CTL  P_VI_TYPE (VIC_ADR +  0x07)
#define B_VIC_VIRQ2_CTL  P_VI_TYPE (VIC_ADR +  0x0b)
#define B_VIC_VIRQ3_CTL  P_VI_TYPE (VIC_ADR +  0x0f)
#define B_VIC_VIRQ4_CTL  P_VI_TYPE (VIC_ADR +  0x13)
#define B_VIC_VIRQ5_CTL  P_VI_TYPE (VIC_ADR +  0x17)
#define B_VIC_VIRQ6_CTL  P_VI_TYPE (VIC_ADR +  0x1b)
#define B_VIC_VIRQ7_CTL  P_VI_TYPE (VIC_ADR +  0x1f)

/* these are used a shift values to access the proper ctl reg */
#define	VIRQ1	VI_TYPE	0x01
#define	VIRQ2	VI_TYPE	0x02
#define	VIRQ3	VI_TYPE	0x03
#define	VIRQ4	VI_TYPE	0x04
#define	VIRQ5	VI_TYPE	0x05
#define	VIRQ6	VI_TYPE	0x06
#define	VIRQ7	VI_TYPE	0x07

#define         VIRQ_CTL_IPL_MASK	VI_TYPE 0x7

#define 	VIRQ_MASK	 	VI_TYPE 0x80
#define 	VIRQ_ENABLE   		VI_TYPE 0x0
#define 	VIRQ_DISABLE  		VI_TYPE 0x80


 /* DMA Status Interrupt cntl */
#define B_VIC_DMAI_CTL  P_VI_TYPE (VIC_ADR +  0x23)

#define         DMAI_IPL_MASK	0x7

#define DMAI_MASK	 VI_TYPE 0x80
#define	DMAI_ENABLE   VI_TYPE 0x0
#define DMAI_DISABLE  VI_TYPE 0x80

/* Local Interrupt cntl lev 1-7 */
#define B_VIC_LIRQ1_CTL  P_VI_TYPE (VIC_ADR +  0x27)
#define B_VIC_LIRQ2_CTL  P_VI_TYPE (VIC_ADR +  0x2b)
#define B_VIC_LIRQ3_CTL  P_VI_TYPE (VIC_ADR +  0x2f)
#define B_VIC_LIRQ4_CTL  P_VI_TYPE (VIC_ADR +  0x33)
#define B_VIC_LIRQ5_CTL  P_VI_TYPE (VIC_ADR +  0x37)
#define B_VIC_LIRQ6_CTL  P_VI_TYPE (VIC_ADR +  0x3b)
#define B_VIC_LIRQ7_CTL  P_VI_TYPE (VIC_ADR +  0x3f)

/* these are used as shift values to access the proper ctl reg */
/* these are also used for lirq status */
#define LIRQ1 			 VI_TYPE 0x01
#define LIRQ2 			 VI_TYPE 0x02
#define LIRQ3 			 VI_TYPE 0x03
#define LIRQ4 			 VI_TYPE 0x04
#define LIRQ5 			 VI_TYPE 0x05
#define LIRQ6 			 VI_TYPE 0x06
#define LIRQ7 			 VI_TYPE 0x07

#define LIRQ_MASK		 VI_TYPE 0x80
#define	LIRQ_ENABLE  		 VI_TYPE 0x0
#define LIRQ_DISABLE  	         VI_TYPE 0x80

#define SENCE_MASK		 VI_TYPE 0x70
#define ACTIVE_MASK    	  	 VI_TYPE 0x40
#define LEVEL_MASK		 VI_TYPE 0x20
#define VIC_VECTOR_MASK	         VI_TYPE 0x10

#define ACTIVE_HIGH         VI_TYPE 0x40
#define ACTIVE_LOW          VI_TYPE 0x00
#define EDGE_SENSITIVE	    VI_TYPE 0x20
#define LEVEL_SENSITIVE	    VI_TYPE 0x00
#define VIC_SUP_VECTOR	    VI_TYPE 0x10
#define VIC_NO_SUP_VECTOR   VI_TYPE 0x00

/* mask for state of LIRQ X VIC input signal */
#define LIRQ_INPUT_SIGNAL_MASK	 VI_TYPE 0x08

#define LIRQ_CTL_IPL_MASK	  VI_TYPE	0x07

#define B_VIC_ICGSI_CTL  P_VI_TYPE (VIC_ADR +  0x43)

#define ICGSI_ENABLE	VI_TYPE 	0x00
#define ICGSI_DISABLE	VI_TYPE 	0xf0

/*
These are actually masks which may be or'ed together.

NOTE:
These are used for shift base values, to access the control
bit in the B_VIC_ICGSI_CTL reg do  0x1 << (ICGSx + 4)
these constants are used for other ICF functions.
*/

#define ICGS3	 VI_TYPE 0x3
#define ICGS2	 VI_TYPE 0x2
#define ICGS1	 VI_TYPE 0x1
#define ICGS0	 VI_TYPE 0x0

#define ICGSI_IPL_MASK	VI_TYPE	0x07

#define B_VIC_ICMSI_CTL  P_VI_TYPE (VIC_ADR +  0x47)

#define ICMSI_ENABLE	VI_TYPE 	0x00
#define ICMSI_DISABLE	VI_TYPE 	0xf0

/* these are actually masks which may be or'ed together */
#define ICMS3	 VI_TYPE 0x3
#define ICMS2	 VI_TYPE 0x2
#define ICMS1	 VI_TYPE 0x1
#define ICMS0	 VI_TYPE 0x0

#define ICMSI_IPL_MASK	VI_TYPE	0x07

#define B_VIC_ERRI_CTL  P_VI_TYPE (VIC_ADR +  0x4b)

#define ERRI_ENABLE	VI_TYPE 	0x00
#define ERRI_DISABLE	VI_TYPE 	0xf0

/* these are actually masks which may be or'ed together */
#define ERRI_SYSFAIL		 VI_TYPE 0x10
#define ERRI_ARB_TIMEOUT 	 VI_TYPE 0x20
#define ERRI_WRITE_POST		 VI_TYPE 0x40
#define ERRI_ACFAIL		 VI_TYPE 0x80

#define ERRI_IPL_MASK	VI_TYPE	0x07

#define B_VIC_ICGSI_VBASE       P_VI_TYPE (VIC_ADR +  0x4f)
#define B_VIC_ICGSI_STAT        P_VI_TYPE (VIC_ADR +  0x4f)

#define ICGSI_VBASE_MASK   	VI_TYPE 0xfc
#define ICGSI_STAT_MASK	        VI_TYPE 0x03

/* defined above but shown here
	#define ICGS0		0
	#define ICGS1		1
	#define ICGS2		2
	#define ICGS3		3
*/

#define B_VIC_ICMSI_VBASE 	P_VI_TYPE (VIC_ADR +  0x53)
#define B_VIC_ICMSI_STAT 	P_VI_TYPE (VIC_ADR +  0x53)
#define ICMSI_VBASE_MASK   	VI_TYPE 0xfc
#define ICMSI_STAT_MASK	 	VI_TYPE 0x03

/* defined above but shown here
	#define ICMS0		0
	#define ICMS1		1
	#define ICMS2		2
	#define ICMS3		3
*/

#define B_VIC_LIRQ_VBASE P_VI_TYPE (VIC_ADR +  0x57)
#define B_VIC_LIRQ_STAT  P_VI_TYPE (VIC_ADR +  0x57)

#define LIRQ_VBASE_MASK   	 VI_TYPE 0xf8
#define LIRQ_STAT_MASK	         VI_TYPE 0x07


/* this was B_VIC_ERRI_xxxx */
#define B_VIC_MISCI_VBASE  	P_VI_TYPE (VIC_ADR +  0x5b)
#define B_VIC_MISCI_STAT  	P_VI_TYPE (VIC_ADR +  0x5b)

#define MISCI_VBASE_MASK   	 VI_TYPE 0xf8
#define MISCI_STAT_MASK	         VI_TYPE 0x07

#define MISCI_DMA		 VI_TYPE 0x5
#define MISCI_VME_IACK	 	 VI_TYPE 0x4
#define MISCI_SYSFAIL		 VI_TYPE 0x3
#define MISCI_ARB_TIMEOUT 	 VI_TYPE 0x2
#define MISCI_WRITE_POST 	 VI_TYPE 0x1
#define MISCI_ACFAIL		 VI_TYPE 0x0


#define B_VIC_IPC_SWITCH  P_VI_TYPE (VIC_ADR +  0x5f)
/* ICMSX and ICGSX contants above and shift accordingly */

#define B_VIC_IPC_REG0  P_VI_TYPE (VIC_ADR +  0x63)
#define B_VIC_IPC_REG1  P_VI_TYPE (VIC_ADR +  0x67)
#define B_VIC_IPC_REG2  P_VI_TYPE (VIC_ADR +  0x6b)
#define B_VIC_IPC_REG3  P_VI_TYPE (VIC_ADR +  0x6f)
#define B_VIC_IPC_REG4  P_VI_TYPE (VIC_ADR +  0x73)
#define B_VIC_IPC_REG5  P_VI_TYPE (VIC_ADR +  0x77)
#define B_VIC_IPC_REG6  P_VI_TYPE (VIC_ADR +  0x7b)
#define B_VIC_IPC_REG7  P_VI_TYPE (VIC_ADR +  0x7f)

#define B_VIC_VIRQREQ_CTL   P_VI_TYPE (VIC_ADR +  0x83)
#define B_VIC_VIRQREQ_STAT  P_VI_TYPE (VIC_ADR +  0x83)

#define VIRQREQ_MASK		 VI_TYPE 0x01
#define VIRQREQ_ENABLE		 VI_TYPE 0x01
#define VIRQREQ_DISABLE	 	 VI_TYPE 0x00

#define VIRQREQ_CTL_MASK	 VI_TYPE 0xfe
#define VIRQREQ_STAT	 	 VI_TYPE 0xfe

#define VIRQ1REQ		 VI_TYPE 0x02
#define VIRQ2REQ		 VI_TYPE 0x04
#define VIRQ3REQ		 VI_TYPE 0x08
#define VIRQ4REQ		 VI_TYPE 0x10
#define VIRQ5REQ		 VI_TYPE 0x20
#define VIRQ6REQ		 VI_TYPE 0x40
#define VIRQ7REQ		 VI_TYPE 0x80


#define B_VIC_VIRQ1REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x87)
#define B_VIC_VIRQ2REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x8b)
#define B_VIC_VIRQ3REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x8f)
#define B_VIC_VIRQ4REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x93)
#define B_VIC_VIRQ5REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x97)
#define B_VIC_VIRQ6REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x9b)
#define B_VIC_VIRQ7REQ_VECTOR  P_VI_TYPE (VIC_ADR +  0x9f)


#define B_VIC_TIMEOUTS  P_VI_TYPE (VIC_ADR +  0xa3)
#define B_VIC_LBUS_TIMING  P_VI_TYPE (VIC_ADR +  0xa7)
#define B_VIC_BLK_XFER_DEF  P_VI_TYPE (VIC_ADR +  0xab)
#define B_VIC_VME_CONFIG  P_VI_TYPE (VIC_ADR +  0xaf)
#define B_VIC_ARB_REQ_CONFIG  P_VI_TYPE (VIC_ADR +  0xb3)
#define B_VIC_AM_SRC  P_VI_TYPE (VIC_ADR +  0xb7)

/* bus error status reg */
#define B_VIC_BERR_STAT		 P_VI_TYPE (VIC_ADR +  0xbb)
#define LBUS_VME_ACQ_TIME_OUT    VI_TYPE 0x01
#define SELF_SLAVE1_ACC 	 VI_TYPE 0x02
#define SELF_SLAVE0_ACC 	 VI_TYPE 0x04
#define LBUS_TIME_OUT		 VI_TYPE 0x08
#define VME_TIMEOUT		 VI_TYPE 0x10
#define VME_BUS_ERROR		 VI_TYPE 0x20
#define LBUS_ERROR		 VI_TYPE 0x40
#define IAM_SYSCON		 VI_TYPE 0x80


#define B_VIC_DMA_STAT  P_VI_TYPE (VIC_ADR +  0xbf)

#define B_VIC_SLS0_CTL0  P_VI_TYPE (VIC_ADR +  0xc3)
#define TMR_MASK 	 VI_TYPE 0xc0
#define DISABLE_TMR	 VI_TYPE  0x0
#define ENABLE_HZ50	 VI_TYPE 0x40
#define ENABLE_HZ1000	 VI_TYPE 0x80
#define ENABLE_HZ100	 VI_TYPE 0xc0

#define ASIZE_MASK	VI_TYPE	0x0c
#define	ASIZE_32	VI_TYPE 0x00
#define	ASIZE_24	VI_TYPE 0x04
#define	ASIZE_16	VI_TYPE 0x08


#define B_VIC_SLS0_CTL1  P_VI_TYPE (VIC_ADR +  0xc7)
#define B_VIC_SLS1_CTL0  P_VI_TYPE (VIC_ADR +  0xcb)
#define B_VIC_SLS1_CTL1  P_VI_TYPE (VIC_ADR +  0xcf)

#define B_VIC_RLSE_CTL  P_VI_TYPE (VIC_ADR +  0xd3)
#define B_VIC_BLK_XFER_CTL  P_VI_TYPE (VIC_ADR +  0xd7)
#define B_VIC_BLK_XFER_LEN0  P_VI_TYPE (VIC_ADR +  0xdb)
#define B_VIC_BLK_XFER_LEN1  P_VI_TYPE (VIC_ADR +  0xdf)

#define B_VIC_SYS_RESET  P_VI_TYPE (VIC_ADR +  0xe3)
#define SYS_RESET_ENABLE	 VI_TYPE 0xf0

#ifdef __cplusplus
}
#endif

#endif /* __INCivich */
