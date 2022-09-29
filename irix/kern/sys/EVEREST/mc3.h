/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * mc3.h -- Definitions pertinent to the Everest MC3 memory controller
 *	board.
 */

#ifndef __SYS_MC3_H__
#define __SYS_MC3_H__

#ident "$Revision: 1.13 $"

#define MC3_TYPE_VALUE 0x02
#define MC3_REV_LEVEL  0x00

#define MC3_NUM_LEAVES      2	/* 2 memory leaves on an MC3 board */
#define MC3_BANKS_PER_LEAF  4	/* 4 banks in each memory leaf */
#define MC3_NUM_BANKS	    (MC3_NUM_LEAVES * MC3_BANKS_PER_LEAF)

#define MC3_NOBANK	7

#define MC3_BIST_TOUT	0xb2d050	/* About one minute at 50MHz */

#define MC3_ERRINT_ENABLE	0x10000	/* enable interrupt bit */
#define MC3_BIST_MA_REV		0x30000	/* bits 17:16 on means MA rev3 */

/*
 * MC3 memory board, per-BOARD configuration registers.
 */
#define MC3_BANKENB	0x00	/* 8  RW Specifies enabled banks */
#define MC3_TYPE	0x01	/* 32 R  Contains board type for MC3 */
#define MC3_REVLEVEL	0x02	/* 32 R  Revision level of board */
#define MC3_ACCESS	0x03	/* 3  RW Access Control */
#define MC3_MEMERRINT	0x04	/* 17 RW Memeory Error Interrrupt Register */
#define MC3_EBUSERRINT	0x05	/* 17 RW EBUS Error Interrupt Register */
#define MC3_BISTRESULT	0x06	/* 20 RW BIST result register */
#define MC3_DRSCTIMEOUT	0x07	/* 20 RW DRSC timeout register */
#define MC3_EBUSERROR	0x08	/* 4  R  EBUS error register (clear on read) */
#define MC3_REFRESHCNT  0x09	/* 0  W  Refresh count initialize */
#define MC3_LEAFCTLENB	0x0a	/* 4  RW Leaf Control enable */
#define MC3_RESET	0x0f	/* 0  W  Board reset */

/*
 * Macros for getting Bank configuration register numbers
 */
#define MC3_BANK(_leaf, _bnk, _reg) (_leaf*0x40 + _bnk*0x4 + _reg + 0x10)
#define BANK_SIZE	0	/* 3  RW Encoded bank size register*/
#define BANK_BASE	1	/* 32 RW Base Address for Bank's interleave */
#define BANK_IF		2      	/* 3  RW Interleave Factor register */
#define BANK_IP		3	/* 4  RW Interleave Position register */

/*
 * Per-leaf configuration registers
 */
#define MC3_LEAF(_leaf, _reg)	(_leaf*0x40 + _reg)
#define MC3_LEAF1_OFFSET 	0x40	/* Leaf 1 registers start at 0x40 */
#define MC3LF_ERROR	0x20	/* 4  R  Error status register */
#define MC3LF_ERRORCLR	0x21	/* 4  R  Error status, clear on read */
#define MC3LF_ERRADDRHI 0x22	/* 8  R  ErrorAddressHigh */
#define MC3LF_ERRADDRLO	0x23	/* 32 R  ErrorAddressLo */
#define MC3LF_BIST	0x24	/* 24 RW Bist status register */
#define MC3LF_SYNDROME0 0x30	/* 16 R  Syndrome slice register 0 */
#define MC3LF_SYNDROME1 0x31	/* 16 R  Sundrome slice register 1 */
#define MC3LF_SYNDROME2 0x32	/* 16 R  Syndrome slice register 2 */
#define MC3LF_SYNDROME3 0x33	/* 16 R  Syndrome slice register 3 */

/* 
 * MC3 memory board, interleave factor codes (BANK_IF).
 * Magic formula converts "interleave factor code" to
 * "interleaf factor".  Currently, only valid  codes are 
 * 0,1,2,3,4.
 */
#define MC3_INTERLEAV(code) (1<<(code))


/*
 * Interleave type values.  These values are passed to the interleave
 * routine to specify the interleave algorithm.
 */
#define INTLV_ONEWAY	0
#define INTLV_STABLE	1
#define INTLV_OPTIMAL	2


/*
 * Because the bit fields in the Bank Enable register are so
 * pathologically bizarre, the following following macro
 * is provided. The bank enable bit field is
 *
 *  _______________________________________________________________
 * | L1 B3 | L1 B2 | L0 B3 | L0 B2 | L1 B1 | L1 B0 | L0 B1 | L0 B0 |
 * +---------------------------------------------------------------+ 
 *   Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
 */
#define MC3_BENB(_l, _b) \
    ( 1 << ( (((_b)&0x2) >> 1) * 4 + (_l)*2 + ((_b) & 0x1)) )

#define	MC3_BENB_LF0  (MC3_BENB(0,0)|MC3_BENB(0,1)|MC3_BENB(0,2)|MC3_BENB(0,3))
#define	MC3_BENB_LF1  (MC3_BENB(1,0)|MC3_BENB(1,1)|MC3_BENB(1,2)|MC3_BENB(1,3))

/*
 * leaf config reg and proc config reg displacement are the same, so
 * leaf n starts the same relative place as proc n.
 */
#define MC3_GETLEAFREG(s,l,r)	EV_GETCONFIG_REG(s,l,r)
#define MC3_SETLEAFREG(s,l,r,v)	EV_SETCONFIG_REG(s,l,r,v)

#define MC3_GETREG(s,r)		EV_GETCONFIG_REG(s,0,r)
#define MC3_SETREG(s,r,v)	EV_SETCONFIG_REG(s,0,r,v)

#define MC3_GETLEAFREG_NOWAR(s,l,r)	EV_GETCONFIG_REG_NOWAR(s,l,r)
#define MC3_GETREG_NOWAR(s,r)		EV_GETCONFIG_REG_NOWAR(s,0,r)

#ifdef _LANGUAGE_C
extern void	mc3_init(void);
extern int	mc3_intr_mem_error(void);
extern void	mc3_intr_ebus_error(void);
extern void	mc3_decode_addr(void (*)(char *, ...), uint, uint);
#ifndef _STANDALONE
extern void	mc3_get_sbe_count(int, int *, int *);
extern void	mc3_clr_sbe_count(int);
#endif
#endif	/* _LANGUAGE_C */

#endif /* _SYS_MC3_H_ */
