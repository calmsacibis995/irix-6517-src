/* ivac.h - Ironics/VTC's VME Address Controller (VAC) constants */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,07jul92,jwt  cleaned up ANSI compiler warnings.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,31jan91,rjt  initial release to windriver after many minor fixes.
01a,25jul90,rjt  writen by modifying IV-SPRC sparcmon version.
*/

#ifndef __INCivach
#define __INCivach

#ifdef __cplusplus
extern "C" {
#endif

/*
Assembly language files should turn off type declarations by not
including those portions of this include file, this is done by
placing a "#define _ASMLANGUAGE" statement before this file is
included in any other files.

Also, in order to incorparate stronger typing of all constants, and to
allow the same constants to be typed in C files,yet usable in
assembly files. An unorthadox method of TYPING is used by CONDITIONALLY
#define'ing the constant TYPES. If no _ASMLANGUAGE #define is present,
then the type is defined to a blank space, otherwise it is defined
to be a C type. This looks weird but works nicely.
*/


/*
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

Where ASSERTED VALUE = 0 and dasserted value = 0xff for ACTIVE LOW SIGNALS
or vice versa,
where ASSERTED VALUE = 0xff and dasserted value = 0 for ACTIVE HIGH SIGNALS.

The active high bits may also be something other than 0xff for
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
*/


/* IMPORTANT NOTICE: NOT ALL CONSTANTS HAVE BEEN TESTED */


/* The following #defines allow or disallow C types */
#ifndef _ASMLANGUAGE
/* C code typing */
#ifndef P_VA_TYPE
#define P_VA_TYPE (unsigned short *)
#endif
#ifndef VA_TYPE
#define VA_TYPE  (unsigned short)
#endif
#else
/* No typing allowing in assembly */
#define VA_TYPE
#define P_VA_TYPE
#endif	/* _ASMLANGUAGE */


#define VAC_ADR  0xfffd0000
#define VAC_TYPE unsigned short

/*
IMPORTANT
Some boards may require a special routine to access the VAC registers.
Example: a board whose cache must be bypassed to ensure register integrity.
If they do, there should be  vaXetReg() macros #define'd to call that
special routine. Otherwise the following vaXetReg() macros will be used.
For portability reasons, it might be a good idea to only use vaXetReg()
to alter any VAC registers in C code.  If this macro is insufficient
then "#undef vaRegSet" and write a C routine (or another macro) to replace it.
*/

#ifndef vaRegSet
#define vaRegSet(hReg,val) (*(hReg) = (VAC_TYPE) (val))
#endif	/* vaRegSet */
#ifndef vaRegGet
#define vaRegGet(bReg) *(bReg)
#endif	/* vaRegGet */

/*  Slave Select  mask and base registers */
#define H_VAC_SLS1_MASK  P_VA_TYPE (VAC_ADR + 0x0000)
#define H_VAC_SLS1_BASE  P_VA_TYPE (VAC_ADR + 0x0100)
#define H_VAC_SLS0_MASK  P_VA_TYPE (VAC_ADR + 0x0200)
#define H_VAC_SLS0_BASE  P_VA_TYPE (VAC_ADR + 0x0300)


/*  group & module Addr. decodes for IPC Reg. */
#define H_VAC_ICFSEL		P_VA_TYPE(VAC_ADR + 0x0400)
#define GROUP_BASE_MASK		0xff00
#define MODULE_BASE_MASK	0x00ff
#define GROUP_SHIFT			0x0000
#define MODULE_SHIFT		0x0008

/* DRAM Upper Limit Mask Register */
#define H_VAC_DRAM_MASK  P_VA_TYPE (VAC_ADR + 0x0500)

/* Boundry 2 Address Reg. */
#define H_VAC_B2_BASE  P_VA_TYPE (VAC_ADR + 0x0600)

/* Boundry 3 Address Reg. */
#define H_VAC_B3_BASE  P_VA_TYPE (VAC_ADR + 0x0700 )

/* A24 Base Address Reg. */
#define H_VAC_A24_BASE  P_VA_TYPE (VAC_ADR + 0x0800 )

/*  Region 1,2,3 Attribute Reg. */
#define H_VAC_R1_ATTRIB  P_VA_TYPE (VAC_ADR + 0x0900)
#define H_VAC_R2_ATTRIB  P_VA_TYPE (VAC_ADR + 0x0a00)
#define H_VAC_R3_ATTRIB  P_VA_TYPE (VAC_ADR + 0x0b00 )

#define ATR_DATA_SZ_MASK		VA_TYPE 0x8000
#define ATR_D16  			VA_TYPE 0x8000
#define ATR_D32  			VA_TYPE 0x0

#define ATR_ADR_SZ_MASK			VA_TYPE 0x6000
#define ATR_A16  			VA_TYPE 0x2000
#define ATR_A24  			VA_TYPE 0x0000
#define ATR_A32  			VA_TYPE 0x4000

#define ATR_CACHE_MASK			VA_TYPE 0x1000
#define ATR_CACHE_INHIBIT	VA_TYPE 0x1000
#define ATR_CACHE_ENABLE	VA_TYPE 0x0000

#define ATR_SEGMENT_MAP_MASK	VA_TYPE 0x0c00
#define ATR_NULL 			VA_TYPE 0x0
#define ATR_SHR  			VA_TYPE 0x0400
#define	ATR_VSB  			VA_TYPE 0x0800
#define ATR_VME  			VA_TYPE 0x0c00


/*  Dsack Timing Control Reg. */
#define H_VAC_IO4_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x0c00)
#define H_VAC_IO5_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x0d00)
#define H_VAC_SHR_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x0e00)
#define H_VAC_EPROM_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x0f00)
#define H_VAC_IO0_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x1000)
#define H_VAC_IO1_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x1100)
#define H_VAC_IO2_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x1200)
#define H_VAC_IO3_DSACK_CTL  P_VA_TYPE (VAC_ADR + 0x1300)

#define DSACK_DELAY_MASK	0xE000
#define DSACK_DELAY_0		0x0000
#define DSACK_DELAY_1		0x2000
#define DSACK_DELAY_2		0x4000
#define DSACK_DELAY_3		0x6000
#define DSACK_DELAY_4		0x8000
#define DSACK_DELAY_5		0xA000
#define DSACK_DELAY_6		0xC000
#define DSACK_DELAY_7		0xE000

#define DSACK1_ENABLE		0x1000
#define DSASK0_ENABLE		0x0800

#define DSACK_RECOVERY_MASK	0x0700
#define RECOVERY_DELAY_0	0x0000
#define RECOVERY_DELAY_1	0x0100
#define RECOVERY_DELAY_2	0x0200
#define RECOVERY_DELAY_3	0x0300
#define RECOVERY_DELAY_4	0x0400
#define RECOVERY_DELAY_5	0x0500
#define RECOVERY_DELAY_6	0x0600
#define RECOVERY_DELAY_7	0x0700

#define IORD_DELAY_MASK		0x00c0
#define IORD_DELAY_0		0x0000
#define IORD_DELAY_1		0x0040
#define IORD_DELAY_2		0x0080
#define IORD_DELAY_3		0x00c0

#define IOWR_DELAY_MASK		0x0030
#define IOWR_DELAY_0		0x0000
#define IOWR_DELAY_1		0x0010
#define IOWR_DELAY_2		0x0020
#define IOWR_DELAY_3		0x0030

#define IOSELI_DELAY_MASK	0x000c
#define IOSELI_DELAY_0		0x0000
#define IOSELI_DELAY_1		0x0004
#define IOSELI_DELAY_2		0x0008
#define IOSELI_DELAY_3		0x000c

#define IORD_CYCLE_END_CTL	0x0002
#define IOWR_CYCLE_END_CTL	0x0001

/*  Decode Control Register */
#define H_VAC_DECODE_CTL  P_VA_TYPE (VAC_ADR + 0x1400)

#define	SLACKDR 	VA_TYPE 0x8000
#define QDRAM		VA_TYPE 0x4000

/* slave 1 device selection */
#define SL1_MASK	VA_TYPE 0x3000
#define SL1DS1		VA_TYPE 0x2000
#define SL1DS0		VA_TYPE 0x1000
#define	SL1_EPROM	VA_TYPE 0x0000
#define SL1_VSB		VA_TYPE 0x1000
#define SL1_SHRCS	VA_TYPE 0x2000
#define SL1_DRAM	VA_TYPE 0x3000

#define SL1A32		VA_TYPE 0x0800
#define SL1A16		VA_TYPE 0x0400
#define QSL0		VA_TYPE 0x0200
#define	QSL1		VA_TYPE 0x0100
#define QICF		VA_TYPE 0x0080
#define QBNDRY		VA_TYPE 0x0040
#define ACKDR		VA_TYPE 0x0020
#define REDSL1		VA_TYPE 0x0010
#define REDSL0		VA_TYPE 0x0008
#define DRDLY1		VA_TYPE 0x0004
#define DRDLY0		VA_TYPE 0x0002
#define QBFPU		VA_TYPE 0x0001

/* VAC interrupt status register and its bit mask constants  */
#define H_VAC_IRQ_STAT  P_VA_TYPE (VAC_ADR +0x1500 )


#define VAC_IRQ_PIO9 VA_TYPE 0x8000
#define VAC_IRQ_PIO8 VA_TYPE 0x4000
#define VAC_IRQ_PIO7 VA_TYPE 0x2000
#define VAC_IRQ_PIO4 VA_TYPE 0x1000
#define VAC_IRQ_MBX  VA_TYPE 0x0800
#define VAC_IRQ_TMR  VA_TYPE 0x0400
#define VAC_IRQ_SIOA VA_TYPE 0x0200
#define VAC_IRQ_SIOB VA_TYPE 0x0100


/* VAC IRQ signal map and control register  */
#define H_VAC_IRQ_CTL  P_VA_TYPE (VAC_ADR +0x1600 )

/*
The values are designed to be used as follows.  The interrupt
source on the VAC and the actual PIO signals to the VIC are
combined to produce a VAC interrupt control value, or bit map,
that can be used with a mask value to toggle the control bits.

Example:
			  control bit map 	       IRQ pin dest     VAC IRQ source
	#define   IRQ_VAC_TMR_CTL_MAP	(IRQ_DEST_PIO10 << IRQ_VAC_SRC_TMR)

To remove a VAC interrupt you must disable and enable the VAC interrupt
via the VAC Interrupt Control Register.

Clear the IRQ map to disable the IRQ:

	sysVacFieldSet (H_VAC_IRQ_CTL,  IRQ_VAC_TMR_CTL_MASK,
					(IRQ_VAC_TMR_CTL_MAP & VAC_IRQ_DISABLE));

Map it back in to enable the IRQ:

	sysVacFieldSet (H_VAC_IRQ_CTL, IRQ_VAC_TMR_CTL_MASK,
					(IRQ_VAC_TMR_CTL_MAP & VAC_IRQ_ENABLE));
*/

#define IRQ_PIO9_CTL_MASK  VA_TYPE 0xc000
#define IRQ_PIO8_CTL_MASK  VA_TYPE 0x3000
#define IRQ_PIO7_CTL_MASK  VA_TYPE 0x0c00
#define IRQ_PIO4_CTL_MASK  VA_TYPE 0x0300
#define IRQ_MBX_CTL_MASK   VA_TYPE 0x00c0
#define IRQ_SIOA_CTL_MASK  VA_TYPE 0x0030
#define IRQ_SIOB_CTL_MASK  VA_TYPE 0x000c
#define IRQ_VAC_TMR_CTL_MASK  VA_TYPE 0x0003

#define VAC_IRQ_DISABLE    0
#define VAC_IRQ_ENABLE     0xffff


/* shift values to map the pio signal destination */
#define IRQ_SRC_PIO9	VA_TYPE 0xE
#define IRQ_SRC_PIO8	VA_TYPE 0xC
#define IRQ_SRC_PIO7	VA_TYPE 0xA
#define IRQ_SRC_PIO4	VA_TYPE 0x8
#define IRQ_SRC_MBX		VA_TYPE 0x6
#define IRQ_SRC_SIOA	VA_TYPE 0x4
#define IRQ_SRC_SIOB	VA_TYPE 0x2
#define IRQ_SRC_VAC_TMR	VA_TYPE 0x0

/* pio signal destination */
#define IRQ_DEST_NULL      0
#define IRQ_DEST_PIO7      1
#define IRQ_DEST_PIO10     2
#define IRQ_DEST_PIO11     3



/* device present flags  */
#define H_VAC_DEVICE_LOC  P_VA_TYPE (VAC_ADR +0x1700 )

#define IOSEL5_PRESENT VA_TYPE 0x0020
#define IOSEL4_PRESENT VA_TYPE 0x0010
#define IOSEL3_PRESENT VA_TYPE 0x0008
#define IOSEL2_PRESENT VA_TYPE 0x0004
#define IOSEL1_PRESENT VA_TYPE 0x0002
#define IOSEL0_PRESENT VA_TYPE 0x0001

/* this reg used to init pio data out values   */
#define H_VAC_PIO_DATA  P_VA_TYPE (VAC_ADR +0x1800 )

/* read only - contains login levels of pio pins  */
#define H_VAC_PIO_PIN  P_VA_TYPE (VAC_ADR +0x1900 )

/* set up the direction of pio */
#define H_VAC_PIO_DIR  P_VA_TYPE (VAC_ADR + 0x1a00)

/* assign functions to the pio  */
#define H_VAC_PIO_FUNC  P_VA_TYPE (VAC_ADR + 0x1b00)

/* pio bit mask defines */
#define PIO0   VA_TYPE 0x0001
#define PIO1   VA_TYPE 0x0002
#define PIO2   VA_TYPE 0x0004
#define PIO3   VA_TYPE 0x0008
#define PIO4   VA_TYPE 0x0010
#define PIO5   VA_TYPE 0x0020
#define PIO6   VA_TYPE 0x0040
#define PIO7   VA_TYPE 0x0080
#define PIO8   VA_TYPE 0x0100
#define PIO9   VA_TYPE 0x0200
#define PIO10  VA_TYPE 0x0400
#define PIO11  VA_TYPE 0x0800
#define PIO12  VA_TYPE 0x1000
#define PIO13  VA_TYPE 0x2000
#define PIO9_DEBOUNCE_ENABLE   VA_TYPE 0x4000
#define CPU_SPACE_EMULATE VA_TYPE 0x8000

#define TXDA_TO_PIO0  			VA_TYPE 0x0001
#define RXTA_TO_PIO1  			VA_TYPE 0x0002
#define TXDB_TO_PIO2     		VA_TYPE 0x0004
#define TXDB_TO_PIO3     		VA_TYPE 0x0008
#define IORD_TO_PIO4     		VA_TYPE 0x0010
#define IOWR_TO_PIO5     		VA_TYPE 0x0020
#define IOSEL3_TO_PIO6   		VA_TYPE 0x0040
#define PIO7IRQ_TO_PIO7  		VA_TYPE 0x0080
#define IOSEL4_TO_PIO8   		VA_TYPE 0x0100
#define IOSEL5_TO_PIO9   		VA_TYPE 0x0200
#define PIO10IRQ_TO_PIO10 		VA_TYPE 0x0400
#define PIO11IRQ_TO_PIO10 		VA_TYPE 0x0800
#define SHRCS_TO_PIO12    		VA_TYPE 0x1000
#define IOSEL2_TO_PIO13   		VA_TYPE 0x2000
#define PIO9_DEBOUNCE_DISABLE        	VA_TYPE 0x4000
#define CPU_SPACE_EMULATE_VIA_IOSEL5	VA_TYPE 0x8000

/* SIO STUFF */

/*
Get table for BAUD_PRESCALE_INIT VALS:

NOTE
The SIO baud rate divisor is dependent on CPU_CLOCK
and is derived from the following formula

                      /          CPU clock rate              \
	 divisor = truncate |  ---------------------------- + 0.5  |
			    		\  16 * (MAX baud rate desired)        /

where divisor > 8.

This value is loaded into the top byte of the VAC CPU
clock divisor.

The baud rate selection table will allow selection of
8 different baud rates. Each lower than the MAX baud rate
by a power of 2. If 9600 baud is the MAX baud rate, then
the following powers are available:

baud select value           baud rate
				7          76800   don't use
				6	       19200
				5			9600
				4			4800
				3			2400
				2			1200
				1			600
				0			300
*/

/*      max rate      cpu clk   divisor   */
/* check these values for roundup */
#define	BAUD_9600_CLK_20MHz	0x2000
#define	BAUD_9600_CLK_25MHz	0x2800
#define	BAUD_9600_CLK_32MHz	0x3400
#define	BAUD_9600_CLK_33MHz	0x3500

#define BAUD_19200_CLK_20MHz    0x0fff /*subtract one from above formula*/
#define	BAUD_19200_CLK_25MHz	0x1400
#define	BAUD_19200_CLK_32MHz	0x1a00
#define	BAUD_19200_CLK_33MHz	0x1a00
#define	BAUD_19200_CLK_40MHz	0x2000

/*Baud Rate Prescaler  used on CPU Clk. */
#define H_VAC_BAUD_PRESCALE  P_VA_TYPE (VAC_ADR + 0x1c00)

/* control register  */
#define H_VAC_SIOA_MODE  P_VA_TYPE (VAC_ADR + 0x1d00)

/* control bits for both sioa and siob */
#define SIO_CTL_ENABLE 		VA_TYPE	0xfcff
#define SIO_CTL_DISABLE 	VA_TYPE	0x0300

/* the following are masks */
#define PARITY_ENABLE 	VA_TYPE 0x8000
#define EVEN_PARITY 	VA_TYPE 0x4000
#define EIGHT_BITS 	VA_TYPE 0x2000

#define BAUD_RATE_MASK  VA_TYPE 0x1c00

#define RESET_RECV 	VA_TYPE 0x0200
#define RESET_XMIT 	VA_TYPE 0x0100
#define XMITER	 	VA_TYPE 0x0080
#define RECVER	 	VA_TYPE 0x0040
#define SEND_BREAK  	VA_TYPE 0x0020
#define TXDA_TO_RXDA 	VA_TYPE 0x0010

/* NOTE: vac sio data bytes are taken from high 8 bits !! */

/* xmit data  */
#define H_VAC_SIOA_XMIT  P_VA_TYPE (VAC_ADR + 0x1e00)

/* control register  */
#define H_VAC_SIOB_MODE  P_VA_TYPE (VAC_ADR + 0x1f00)

/* rec data */
#define H_VAC_SIOA_RECV  P_VA_TYPE (VAC_ADR + 0x2000)
#define H_VAC_SIOB_RECV  P_VA_TYPE (VAC_ADR + 0x2100)

/* xmit data  */
#define H_VAC_SIOB_XMIT  P_VA_TYPE (VAC_ADR + 0x2200)

/* sio interrupt mask regs    */
#define H_VAC_SIOA_IRQ_MASK  P_VA_TYPE (VAC_ADR + 0x2300)
#define H_VAC_SIOB_IRQ_MASK  P_VA_TYPE (VAC_ADR + 0x2400)

#define	SINGLE_CHAR_RECV_MASK   VA_TYPE 0x8000
#define	FIFO_FULL_MASK	        VA_TYPE 0x4000
#define	BREAK_CHANGE_MASK       VA_TYPE 0x2000
#define	ERROR_MASK  	        VA_TYPE 0x1000
#define	XMIT_READY_MASK         VA_TYPE 0x0800
#define	XMIT_EMPTY_MASK         VA_TYPE 0x0400

#define SIO_MASK_ENABLE     VA_TYPE	0xffff
#define SIO_MASK_DISABLE    VA_TYPE	0x0000


/* sio status regs   */
#define H_VAC_SIOA_STAT  P_VA_TYPE (VAC_ADR + 0x2500)
#define H_VAC_SIOB_STAT  P_VA_TYPE (VAC_ADR + 0x2600)

#define	CHAR_READY    	VA_TYPE 0x8000
#define	FIFO_FULL	VA_TYPE 0x4000
#define	BREAK_CHANGE    VA_TYPE 0x2000
#define	PARITY_ERROR    VA_TYPE 0x1000
#define	FRAMING_ERROR   VA_TYPE 0x0800
#define	OVERRUN_ERROR   VA_TYPE 0x0400
#define	XMIT_READY 	VA_TYPE 0x0200
#define	XMIT_EMPTY	VA_TYPE 0x0100

/* VAC TIMER STUFF */

#define H_VAC_TMR_DATA  P_VA_TYPE (VAC_ADR + 0x2700)
#define H_VAC_TMR_CTL  P_VA_TYPE (VAC_ADR + 0x2800)

#define PRESCALER_IN_MASK    	VA_TYPE 0x3f00
#define PRESCALER_IN_SHIFT 	8

/* use this to read the instantaneous count value of prescaler */
#define PRESCALER_OUT_MASK    	VA_TYPE 0x003f

#define  VAC_20MHZ_CPU_1MHZ_PRESCALER   (19 << PRESCALER_IN_SHIFT)
#define  VAC_25MHZ_CPU_1MHZ_PRESCALER	(24 << PRESCALER_IN_SHIFT)
#define  VAC_33MHZ_CPU_1MHZ_PRESCALER	(32 << PRESCALER_IN_SHIFT)
#define  VAC_40MHZ_CPU_1MHZ_PRESCALER	(39 << PRESCALER_IN_SHIFT)

/* Load command disables timer and then loads the interal timer registers    */
/* with the values in the prescaler control bits and the timer data register */
#define RUN_LOAD_MASK		VA_TYPE 0x4000
#define RUN_CMD			VA_TYPE 0x4000
#define STOP_N_LOAD_CMD		VA_TYPE 0x0000

/* You can set the timer to only produce a single irq after the time  */
/* has elapsed or reset the counter for continuous irqs               */
#define SINGLE_CONT_MASK	VA_TYPE 0x8000
#define SINGLE_IRQ_CMD		VA_TYPE 0x8000
#define CONT_IRQ_CMD		VA_TYPE 0x0000

/*************************************************************************
* The vac timer rate is calculated as follows:
*
*
*	          /      CPU_CLOCK      \
*                 |  -------------------  |
*	          \ prescaler value + 1 /
*timer rate =  ------------------------------
*	      (65536 - time data reg. value)
*
*
*	where prescaler is set to an arbitrary value.
*
*
*this gives us a time data register value of
*
*
*	       		       /      CPU_CLOCK      \
*			      |  -------------------  |
*	                       \ prescaler value + 1 /
*timer data val =   65536 -  ----------------------------
*				reqested timer rate
*
*
*
*
*PROGRAMING EXAMPLE:
*
*
*to get a 100Hz time rate with a 25MHz cpu clock
*
*  - arbitrarily set prescaler value to 24 which will produce a
*    1MHz time period for each count in the time data register
*
*
*			       		 /  25000000 \
*				        |  ---------  |
*		                 \   24 + 1  /
* time data val = 65536 - ------------- = 55536 = 0xd8f0
*			    		      100
*
*
* - set prescaler
* *H_VAC_TMR_CTL =  (*H_VAC_TMR_CTL & ~PRSCALER_IN_MASK) |
*				VAC_25MHZ_CPU_1MHZ_PRESCALER ;
* - set timer data register
* *H_VAC_TMR_DATA = 0x8df0 ;
*
* - disable timer and load above values into internal registers
* *H_VAC_TMR_CTL =  (*H_VAC_TMR_CTL & ~RUN_LOAD_MASK) | STOP_N_LOAD_CMD ;
*
* - set control for continuous operation
* *H_VAC_TMR_CTL =  (*H_VAC_TMR_CTL & ~SINGLE_CONT_MASK) | CONT_CMD ;
*
* - start timer
* *H_VAC_TMR_CTL =  (*H_VAC_TMR_CTL & ~RUN_LOAD_MASK) | RUN_CMD ;
*
***************************************************************************/


/* Vac ID Reg. Read to Startup  */
#define H_VAC_ID_GO  P_VA_TYPE (VAC_ADR + 0x2900)



/* now for some generic io Macros 									 	  */
/* These macros are designed so that they can be dropped into a generic   */
/* serial handler, They are intended to be the basic building blocks used */
/* to port any serial handler to use the VAC. 							  */

#ifndef _ASMLANGUAGE


#define SIOA	0
#define SIOB	1

/*the following macros expect SIOA or SIOB for u arg-there's no error checking*/

#define XMITER_EMPTY(u) ( XMIT_EMPTY & vaRegGet( H_VAC_SIOA_STAT + \
			   ((u)* ( H_VAC_SIOB_STAT - H_VAC_SIOA_STAT ) ) ) )
#define XMITER_READY(u) ( XMIT_READY & vaRegGet( H_VAC_SIOA_STAT + \
			   ((u)* ( H_VAC_SIOB_STAT - H_VAC_SIOA_STAT ) ) ) )
#define RECVER_READY(u) ( CHAR_READY & vaRegGet( H_VAC_SIOA_STAT + \
			   ((u)* ( H_VAC_SIOB_STAT - H_VAC_SIOA_STAT ) ) ) )

#define XMIT_CHAR(u,ch)   vaRegSet( H_VAC_SIOA_XMIT + \
			   ((u)* ( H_VAC_SIOB_XMIT - H_VAC_SIOA_XMIT ) ) , \
				(unsigned short)( ((unsigned short)(ch))<<8 ) )

#define RECV_CHAR(u,pc)    *(pc) = (unsigned char)(0x00ff & \
				vaRegGet( H_VAC_SIOA_RECV + \
			   ((u)* ( H_VAC_SIOB_RECV - H_VAC_SIOA_RECV ) ) ) )

#define POLLING

/* The following P_GETC and P_PUTC macros are designed for POLLING only!!!  */
/* NOTE: The delay loops may need to be altered depending on the board rev. */
#ifdef POLLING

/* P_PUTC(u,ch) - output char "ch" to "u" where "u" is SIOA or SIOB */
/* add delay, NOTE: no fifo stuff allowed */

#define	P_PUTC(u,ch)  \
    while (  ! XMITER_READY( (u) )  ) ;  \
    XMIT_CHAR( (u),(ch) );  \
    while (   XMITER_READY( (u) )  )


/* P_GETC(u,pc) - input char into char pointer "pc" from */
/*                "u" where "u" is SIOA or SIOB */

#define	P_GETC(u,pc) \
    { \
    int x; \
    while ( ! RECVER_READY( (u) ) ) \
    for(x=0;x<100;x++) ; \
    RECV_CHAR( (u), (pc) )  ; \
    }

#endif	/* POLLING */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCivach */
