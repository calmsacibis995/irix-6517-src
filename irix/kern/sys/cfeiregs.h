/*
 *	c v _ d e v i c e . h		Cray Intf HW register layout
 *
 ******************************************************************************
 *	The software described herein is public domain and may be used,  
 *	copied, or modified by anyone without restriction.
 ******************************************************************************
 *
 *	cv_device.h describes the layout of the registers in the
 *	Cray-VME Interface hardware.  For an explanation of any
 *	registers or bits, see the Programming Specifcation
 *	PRN-0879.
 */
struct  cv_device {
/*
 *      Cray/VME Interface Control
 */
        volatile unsigned short  sr;             /* Status Read */
        volatile unsigned short  intclr;         /* Interrupt Clear */
        volatile unsigned short  iclr;           /* Input Clear */
        	 unsigned short  fill;
        volatile unsigned short  oclr;           /* Output Clear */
        	 unsigned short  fill0[2];
        volatile unsigned short  mclr;           /* Master Clear */
/*
 *      Am2942 DMA Address Generator
 */
        volatile unsigned short  ic;             /* Input Control */
        volatile unsigned short  oc;             /* Output Control */
        	 unsigned short  fill1[6];       /* fill */
/*
 *      MC68153 Bus Interrupter Module
 */
#define VDISC 0
#define VMCLR 1
#define VDMA1 2
#define VDMA0 3
        	 char	x1; 
	volatile char   icrdisc;		/* Intrpt Con Reg 0 (disconn) */
        	 char	x2; 
	volatile char   icrmclr;		/* Intrpt Con Reg 1 (mc) */
        	 char	x3; 
	volatile char   icrdma1;		/* Intrpt Con Reg 2 (read) */
        	 char	x4; 
	volatile char   icrdma0;		/* Intrpt Con Reg 3 (write) */
        	 char	x5; 
	volatile char   ivrdisc;		/* Intrpt Vect Reg 0 (discon) */
        	 char	x6; 
	volatile char   ivrmclr;		/* Intrpt Vect Reg 1 (mc) */
        	 char	x7; 
	volatile char   ivrdma1;		/* Intrpt Vect Reg 2 (read)*/
        	 char	x8; 
	volatile char   ivrdma0;		/* Intrpt Vect Reg 3 (write) */
        	 char   fill11[16];		/* fill */
/*
 *      CRAY INPUT (DMA0)
 */
        volatile unsigned short  iwcr;           /* Write Control Register */
        volatile unsigned short  ircr;           /* Read Control Register */
        volatile unsigned short  irwc;           /* Read Word Counter */
        volatile unsigned short  irac;           /* Read Address Counter */
        volatile unsigned short  irc;            /* Reinitialize Counters */
        volatile unsigned short  ila;            /* Load Address */
        volatile unsigned short  ilwc;           /* Load Word Count */
        volatile unsigned short  iwua;           /* Write Upper Address */
        	 unsigned short  fill12[24];     /* fill */
/*
 *      CRAY OUTPUT (DMA1)
 */
        volatile unsigned short  owcr;           /* Write Control Register */
        volatile unsigned short  orcr;           /* Read Control Register */
        volatile unsigned short  orwc;           /* Read Word Counter */
        volatile unsigned short  orac;           /* Read Address Counter */
        volatile unsigned short  orc;            /* Reinitialize Counters */
        volatile unsigned short  ola;            /* Load Address */
        volatile unsigned short  olwc;           /* Load Word Count */
        volatile unsigned short  owua;           /* Write Upper Address */
        	 unsigned short  fill13[56];     /* fill */
/*
 *      DATA BUFFER
 */
        volatile unsigned short  db;             /* Input/Output (wr/rd) Buf */
};
/*
 *	Definitions for some of the above registers' bit fields 
 *	Note: read (0) is done on the "output" channel, due to Cray's mindset
 *	about input and output (1) referring to the cray cpu's point of view,
 *	never mind that another FEI-3 could be on the other end.
 */

/* Interface Status Register */
#define SR0DONE	0x01	/* Note: bit off means input DMA0 done */
#define SR0BUSERR	0x02	/* Cray input DTB encountered bus error */
#define SR0RESUME	0x04	/* HS: Resume received and not responded to */
				/* LS: Ready sent and no resume response */
#define SR0BUFEMP	0x08	/* Cray input data buffer empty */
/*	  +---- 0 is DMA0 i.e. to cray, 1 is DMA1 i.e. from cray */
#define SR1DONE		0x10	/* Note: bit off means input done */
#define SR1BUSERR	0x20	/* Cray output DTB encountered bus error */
#define SR1READY	0x40	/* Ready received and not responded to */
#define	SR1DBUF		0x80	/* Cray output buffer contained xfered data */
#define SR1PE		0x100	/* Output channel parity error */
#define SR1DISC		0x200	/* Cray output did disconnect */
#define SR1MC		0x400	/* Cray output did programmed master clear */
#define  SRRDERR	0x120	/* Read error */
#define  SRWRERR	0x02	/* Write error */

/* Input Control Register */
#define IC0EN		0x01	/* DMA0 Enabled */
#define ICIDISC		0x02	/* Cray input disconnect */
#define	ICIOMC		0x04	/* IO Master Clear sent on Cray Input channel */
#define ICCPUMC		0x08	/* CPU Master clear sent on Cray Input chan */
#define ICDEADDUMP	0x10	/* Deaddump signal sent on Cray Input channel */
#define ICCHMC		0x20	/* Channel MCLR sent on Cray Input channel */

/* Output Control Register */
#define OC1EN		0x01	/* DMA1 Enabled */

 /* Interrupt Control Registers */
#define INTIL		0x07	/* Interrupt level (0 means ints disabled) */
#define	INTIAC		0x08	/* Interrupt Auto Clear (clears Int Enable) */
#define INTIE		0x10	/* Interrupt Enable - enable bus interrupt */
#define INTIAR		0x20	/* Interrupt Acknowledge Response */
#define INTFAC		0x40	/* Flag Auto Clear */
#define INTF		0x80	/* Flag */

/* DMA control modes (DMA0 and DMA1 control registers) */
#define WORDC0		0x00	/* Word count equals zero, control mode 0 */
#define WORDCC		0x101	/* Word count compare */
#define ADDRC		0x202	/* Address compare */

/*
 *	c v s t a t   s t r u c t u r e
 *
 *   Some of the registers on the interface are write only and therefore
 * the driver cannot use C constructions like |=, &= +=, etc.  In order
 * to manipulate bits in these registers, copies of them are kept in the
 * cvstat structure (one per interface). The required bit operation
 * is first performed on the copy, then on the hardware register itself
 * as a simple assignment.  For example:
 *
 *	cvstat->ic |= IC0EN;		add DMA0 enable bit
 *	cv->ic = cvstat->ic;		write HW reg
 */
struct  cv_stat {
        unsigned short  ic;             /* Copy of input control register */
        unsigned short  oc;             /* Copy of output control register */
        unsigned long   startra;        /* Physical starting read address */
        unsigned long   startwa;        /* Physical starting write address */
};
