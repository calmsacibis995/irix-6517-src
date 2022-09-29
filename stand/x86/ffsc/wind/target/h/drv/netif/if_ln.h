/* if_ln.h - AMD LANCE Ethernet interface header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01p,22sep92,rrr  added support for c++
01o,29jul92,rfs  moved driver specific items to driver file, where they belong
01n,26may92,rrr  the tree shuffle
01m,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01l,25jul91,jwt  removed bit fields (added masks) for portability.
01k,18may91,gae  reworked lncsr_3b register for Radstone's APEX driver,
		 and potentially more portable code.
01j,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01i,07may90,hjb  added LN_RMD_RLEN, LN_TMD_TLEN, LN_NUM_RESERVES definitions;
		 added bufSize, canLoanRmds, freeBufs, nFree and deleted
		 numOutStandingLoans, loanStats from struct ls_softc.
01h,19apr90,hjb  added LN_MIN_FIRST_DATA_CHAIN, LN_MAX_MDS, RMD_ERR, TMD_OWN,
		 TMD_ERR, TMD_BUFF, TMD_UFLO, TMD_LCAR, ls_flags,
		 loanRefCnt, loanStats, LS_PROMISCUOUS_FLAG,
		 LS_MEM_ALLOC_FLAG, LS_PAD_USED_FLAG,
		 LS_RCV_HANDLING_FLAG, LS_START_OUTPUT_FLAG;
		 deleted xmitHandlingAtTaskLevel, recvHandlingTaskLevel.
01g,18mar90,hjb  added padEnable boolean variable in ls_softc structure.
01f,10nov89,dab  added padding to CSR structure for Tadpole lance driver.
01e,02nov89,shl  fixed syntax error.
01d,24may89,jcf  added recvHandlingAtTaskLevel to ls_softc structure.
01c,21apr89,jcf  added memBase, csr0Errs, and xmitHandlingAtTaskLevel
	   +dnw   to ls_softc structure.
01b,09aug88,gae  bumped up LN_BUFSIZE to solve large packet problems.
01a,15apr88,dfm  written.
*/

#ifndef __INCif_lnh
#define __INCif_lnh

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Control block definitions for AMD LANCE (Ethernet) chip.
 * This has some of the same (mis)features as the Intel 82586 with
 * regards to byte ordering.  It assumes that a memory address specifies
 * the location of the least significant byte of a multi-byte value.  This
 * is correct for most Intel & DEC processors, but is wrong for 680x0s.
 * As a result, all addresses specified to the chip must have their
 * words swapped.  At least it has a control bit to automatically swap
 * bytes during data transfer dma.  (The 82586 is much worse.)
 */

/*
 * Control and Status Register access structure
 * WARNING: bit operations should not be done on CSR0!
 * Many of the bits are Read/Clear and will be cleared by any other
 * bit operation on the register.  Note especially that if the
 * stop bit is set, all other operations will retain the stopped
 * condition.  This means that a bit set to do init and start will
 * always fail because stop has priority.
 */
typedef struct
    {
	/* This union describes a plethora of ways to access the data register */
    union
		{
		u_short	    RDP;
        u_short	    lnCSR0;		/* RDP if (RAP==0) */
		u_short		lnCSR1;		/* RDP if (RAP==1) */
		u_short		iadr_l;		/* Init Block addr bits 15:00 */

		u_short		lnCSR2;		/* RDP if (RAP==2) */
		u_short		iadr_h;		/* Init Block addr bits 23:16 */

		u_short		lnCSR3;		/* RDP if (RAP==3) */
		u_short		ln_csr3b;

		} ln_csr;

    u_short	RAP;
    u_short	pad;	/* XXX Tadpole padding */
    } LN_DEVICE;

/* Definitions for fields and bits in the LN_DEVICE */

#define  lncsr_ERR    0x8000	/* (RO) error flag (BABL|CERR|MISS|MERR) */
#define  lncsr_BABL   0x4000	/* (RC) babble transmitter timeout */
#define  lncsr_CERR   0x2000	/* (RC) collision error */
#define  lncsr_MISS   0x1000	/* (RC) missed packet */
#define  lncsr_MERR   0x0800	/* (RC) memory error */
#define  lncsr_RINT   0x0400	/* (RC) receiver interrupt */
#define  lncsr_TINT   0x0200	/* (RC) transmitter interrupt */
#define  lncsr_IDON   0x0100	/* (RC) initialization done */
#define  lncsr_INTR   0x0080	/* (RO) interrupt flag */
#define  lncsr_INEA   0x0040	/* (RW) interrupt enable */
#define  lncsr_RXON   0x0020	/* (RO) receiver on */
#define  lncsr_TXON   0x0010	/* (RO) transmitter on */
#define  lncsr_TDMD   0x0008	/* (WOO)transmit demand */
#define  lncsr_STOP   0x0004	/* (WOO)stop (& reset) chip */
#define  lncsr_STRT   0x0002	/* (RW) start chip */
#define  lncsr_INIT   0x0001	/* (RW) initialize (acces init block) */

#define lncsr3_BSWP     0x0004  /* Byte Swap */
#define lncsr3_ACON     0x0002  /* ALE Control */
#define lncsr3_BCON     0x0001  /* Byte Control */

#define lncsr_0		ln_csr.lnCSR0
#define lncsr_iadr_l	ln_csr.iadr_l
#define lncsr_iadr_h	ln_csr.iadr_h

#define lncsr_INTMASK (lncsr_BABL \
		     | lncsr_CERR \
		     | lncsr_MISS \
		     | lncsr_MERR \
		     | lncsr_RINT \
		     | lncsr_TINT \
		     | lncsr_IDON \
		     | lncsr_INEA)
/*
 * Initialization Block.
 * Specifies addresses of receive and transmit descriptor rings.
 */
typedef struct lnIB
    {
    u_short	lnIBMode;		/* mode register */
    char	lnIBPadr [6]; /* PADR: byte swapped ethernet physical address */
					/* LADRF: logical address filter */
    u_short	lnIBLadrfLow;		/* least significant word */
    u_short	lnIBLadrfMidLow;	/* low middle word */
    u_short	lnIBLadrfMidHigh;  	/* high middle word */
    u_short	lnIBLadrfHigh;		/* most significant word */
					/* RDRA: read ring address */
    u_short	lnIBRdraLow;		/* low word */
    u_short	lnIBRdraHigh;		/* high word */
					/* TDRA: transmit ring address */
    u_short	lnIBTdraLow;		/* low word */
    u_short	lnIBTdraHigh;		/* high word */
    } ln_ib;

/*
 * Receive Message Descriptor Entry.
 * Four words per entry.  Number of entries must be a power of two.
 */
typedef struct lnRMD
    {
    u_short	lnRMD0;		/* bits 15:00 of receive buffer address */
    union
		{
        u_short	lnRMD1;	        /* bits 23:16 of receive buffer address */
		u_short ln_rmd1b;
        } ln_rmd1;

    u_short	lnRMD2;			/* buffer byte count (negative) */
    u_short	lnRMD3;			/* message byte count */
    } ln_rmd;

#define lnrmd1_OWN      0x8000          /* Own */
#define lnrmd1_ERR      0x4000          /* Error */
#define lnrmd1_FRAM     0x2000          /* Framming Error */
#define lnrmd1_OFLO     0x1000          /* Overflow */
#define lnrmd1_CRC      0x0800          /* CRC */
#define lnrmd1_BUFF     0x0400          /* Buffer Error */
#define lnrmd1_STP      0x0200          /* Start of Packet */
#define lnrmd1_ENP      0x0100          /* End of Packet */
#define lnrmd1_HADR     0x00FF          /* High Address */

#define RMD_ERR		0x6000

#define	rbuf_ladr	lnRMD0
#define rbuf_rmd1	ln_rmd1.lnRMD1
#define	rbuf_bcnt	lnRMD2
#define	rbuf_mcnt	lnRMD3

/*
 * Transmit Message Descriptor Entry.
 * Four words per entry.  Number of entries must be a power of two.
 */
typedef struct lnTMD
    {
    u_short	lnTMD0;		/* bits 15:00 of transmit buffer address */

    union
		{
        u_short	    lnTMD1;	/* bits 23:16 of transmit buffer address */
		u_short     ln_tmd1b;
		} ln_tmd1;

    u_short	lnTMD2;			/* message byte count */

    union
		{
		u_short	    lnTMD3;		/* errors */
		u_short     lntmd3b;
        } ln_tmd3;
    } ln_tmd;

#define lntmd1_OWN      0x8000          /* Own */
#define lntmd1_ERR      0x4000          /* Error */
#define lntmd1_MORE     0x1000          /* More than One Retry */
#define lntmd1_ONE      0x0800          /* One Retry */
#define lntmd1_DEF      0x0400          /* Deferred */
#define lntmd1_STP      0x0200          /* Start of Packet */
#define lntmd1_ENP      0x0100          /* End of Packet */
#define lntmd1_HADR     0x00FF          /* High Address */

#define lntmd3_BUFF     0x8000          /* Buffer Error */
#define lntmd3_UFLO     0x4000          /* Underflow Error */

#define lntmd3_LCOL     0x1000          /* Late Collision */
#define lntmd3_LCAR     0x0800          /* Lost Carrier */
#define lntmd3_RTRY     0x0400          /* Retry Error */
#define lntmd3_TDR      0x03FF          /* Time Domain Reflectometry */

#define TMD_OWN		0x8000
#define TMD_ERR		0x6000

#define TMD_BUFF	0x8000
#define TMD_UFLO	0x4000
#define TMD_LCAR	0x0800

#define	tbuf_ladr	lnTMD0
#define tbuf_tmd1	ln_tmd1.lnTMD1
#define	tbuf_bcnt	lnTMD2
#define tbuf_tmd3	ln_tmd3.lnTMD3

#ifdef __cplusplus
}
#endif

#endif /* __INCif_lnh */
