/* am9513.h - AMD AM9513 parallel/timer chip header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/* Copyright 1989, Mizar, Inc. */

/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01c,30mar89,hep  fixed definition errors; got rid of data pointer
                 register defs (not needed).
01b,10feb89,mcl  cosmetics; fixes HIZ definition.
01a,01nov88,miz  written.
*/

/*
This file contains constants for the AMD am9513 parallel/timer chip.
*/

#ifndef __INCam9513h
#define __INCam9513h

#ifdef __cplusplus
extern "C" {
#endif

/* equates for control register */

#define STC_STATUS_BP	0x01		/* byte pointer */
#define STC_STATUS_OUT1	0x02		/* counter 1 output */
#define STC_STATUS_OUT2	0x04		/* counter 2 output */
#define STC_STATUS_OUT3	0x08		/* counter 3 output */
#define STC_STATUS_OUT4	0x10		/* counter 4 output */
#define STC_STATUS_OUT5	0x20		/* counter 5 output */

/* equates for load data pointer register commands */

#define	STC_LDPR_MR1	0xff01		/* ctr 1 mode register */
#define	STC_LDPR_MR2	0xff02		/* ctr 2 mode register */
#define	STC_LDPR_MR3	0xff03		/* ctr 3 mode register */
#define	STC_LDPR_MR4	0xff04		/* ctr 4 mode register */
#define	STC_LDPR_MR5	0xff05		/* ctr 5 mode register */

#define STC_LDPR_ALM1   0xff07          /* ctr 1 alarm register */

#define	STC_LDPR_LR1	0xff09		/* ctr 1 load register */
#define	STC_LDPR_LR2	0xff0a		/* ctr 2 load register */
#define	STC_LDPR_LR3	0xff0b		/* ctr 3 load register */
#define	STC_LDPR_LR4	0xff0c		/* ctr 4 load register */
#define	STC_LDPR_LR5	0xff0d		/* ctr 5 load register */

#define STC_LDPR_ALM2   0xff0f          /* ctr 2 alarm register */

#define	STC_LDPR_HR1	0xff11		/* ctr 1 hold register */
#define	STC_LDPR_HR2	0xff12		/* ctr 2 hold register */
#define	STC_LDPR_HR3	0xff13		/* ctr 3 hold register */
#define	STC_LDPR_HR4	0xff14		/* ctr 4 hold register */
#define	STC_LDPR_HR5	0xff15		/* ctr 5 hold register */

#define STC_LDPR_MMR    0xff17          /* master mode register */

#define	STC_LDPR_HRH1	0xff19		/* ctr 1 hold register hold cycle */
#define	STC_LDPR_HRH2	0xff1a		/* ctr 2 hold register hold cycle */
#define	STC_LDPR_HRH3	0xff1b		/* ctr 3 hold register hold cycle */
#define	STC_LDPR_HRH4	0xff1c		/* ctr 4 hold register hold cycle */
#define	STC_LDPR_HRH5	0xff1d		/* ctr 5 hold register hold cycle */

#define STC_LDPR_SR     0xff1f          /* status register */

/* equates for master mode register bit assignments */

#define STC_MMR_BCD	0x8000		/* 0: binary div,  1: bcd div */
#define STC_MMR_INC	0x4000		/* 0: enable    ,  1: disable */
#define STC_MMR_16bit	0x2000		/* 0: 8-bit data,  1: 16 bit data */
#define STC_MMR_FOUT    0x1000		/* 0: on,          1: off */
#define STC_MMR_FOUTDIV	0x0f00		/* FOUT divider */
#define STC_MMR_D15	0x0f00		/* divide by 15 */
#define STC_MMR_D14	0x0e00		/* divide by 14 */
#define STC_MMR_D13	0x0d00		/* divide by 13 */
#define STC_MMR_D12	0x0c00		/* divide by 12 */
#define STC_MMR_D11	0x0b00		/* divide by 11 */
#define STC_MMR_D10	0x0a00		/* divide by 10 */
#define STC_MMR_D09	0x0900		/* divide by 09 */
#define STC_MMR_D08	0x0800		/* divide by 08 */
#define STC_MMR_D07	0x0700		/* divide by 07 */
#define STC_MMR_D06	0x0600		/* divide by 06 */
#define STC_MMR_D05	0x0500		/* divide by 05 */
#define STC_MMR_D04	0x0400		/* divide by 04 */
#define STC_MMR_D03	0x0300		/* divide by 03 */
#define STC_MMR_D02	0x0200		/* divide by 02 */
#define STC_MMR_D01	0x0100		/* divide by 01 */
#define STC_MMR_D16	0x0000		/* divide by 16 */
#define STC_MMR_FOUTSRC	0x00f0		/* FOUT source  */
#define STC_MMR_F5	0x00f0		/* internal frequency 5 */
#define STC_MMR_F4	0x00e0		/* internal frequency 4 */
#define STC_MMR_F3	0x00d0		/* internal frequency 3 */
#define STC_MMR_F2	0x00c0		/* internal frequency 2 */
#define STC_MMR_F1	0x00b0		/* internal frequency 1 */
#define STC_MMR_GATE5	0x00a0		/* external gate pin 5 */
#define STC_MMR_GATE4	0x0090		/* external gate pin 4 */
#define STC_MMR_GATE3	0x0080		/* external gate pin 3 */
#define STC_MMR_GATE2	0x0070		/* external gate pin 2 */
#define STC_MMR_GATE1	0x0060		/* external gate pin 1 */
#define STC_MMR_SRC5	0x0050		/* external source pin 5 */
#define STC_MMR_SRC4	0x0040		/* external source pin 4 */
#define STC_MMR_SRC3	0x0030		/* external source pin 3 */
#define STC_MMR_SRC2	0x0020		/* external source pin 2 */
#define STC_MMR_SRC1	0x0010		/* external source pin 1 */
#define STC_MMR_E1	0x0000		/* default */
#define STC_MMR_CMP2	0x0008		/* compare 2 enable */
#define STC_MMR_CMP1	0x0004		/* compare 1 enable */
#define STC_MMR_TOD	0x0003		/* time of day mode */
#define STC_MMR_TOD10IP	0x0003		/* time of day enabled - 10 input */
#define STC_MMR_TOD6IP	0x0002		/* time of day enabled -  6 input */
#define STC_MMR_TOD5IP	0x0001		/* time of day enabled -  5 input */
#define STC_MMR_TODDIS	0x0000		/* time of day disabled */

/* equates for counter mode register bit assignments */

#define STC_CMR_GC	0xe000		/* gating control */
#define STC_CMR_NONE	0x0000		/* hep no gating */
#define STC_CMR_TCNHM	0x2000		/* hep active high level TCN - 1*/
#define STC_CMR_GNHP	0x4000		/* hep active high level GATE N + 1*/
#define STC_CMR_GNHM	0x6000		/* hep active high level GATE N - 1*/
#define STC_CMR_GNHL	0x8000		/* hep active high level GATE N */
#define STC_CMR_GNLL	0xa000		/* hep active low  level GATE N */
#define STC_CMR_GNHE	0xc000		/* hep active high edge  GATE N */
#define STC_CMR_GNLE	0xe000		/* hep active low  edge  GATE N */
#define STC_CMR_FALL	0x1000		/* 0: count rising, 1: count falling */
#define STC_CMR_FOUTSRC	0x0f00		/* FOUT source  */
#define STC_CMR_F5	0x0f00		/* internal frequency 5 */
#define STC_CMR_F4	0x0e00		/* internal frequency 4 */
#define STC_CMR_F3	0x0d00		/* internal frequency 3 */
#define STC_CMR_F2	0x0c00		/* internal frequency 2 */
#define STC_CMR_F1	0x0b00		/* internal frequency 1 */
#define STC_CMR_GATE5	0x0a00		/* external gate pin 5 */
#define STC_CMR_GATE4	0x0900		/* external gate pin 4 */
#define STC_CMR_GATE3	0x0800		/* external gate pin 3 */
#define STC_CMR_GATE2	0x0700		/* external gate pin 2 */
#define STC_CMR_GATE1	0x0600		/* external gate pin 1 */
#define STC_CMR_SRC5	0x0500		/* external source pin 5 */
#define STC_CMR_SRC4	0x0400		/* external source pin 4 */
#define STC_CMR_SRC3	0x0300		/* external source pin 3 */
#define STC_CMR_SRC2	0x0200		/* external source pin 2 */
#define STC_CMR_SRC1	0x0100		/* external source pin 1 */
#define STC_CMR_TCNM	0x0000		/* TCN -1 */
#define STC_CMR_SPECEN	0x0080		/* 0: disable special gate, 1: enable */
#define STC_CMR_FMHOLD	0x0040		/* 0: from load, 1: from load or hold */
#define STC_CMR_CONT	0x0020		/* 0: count once, 1: repetitively */
#define STC_CMR_BCD	0x0010		/* 0: binary count, 1: BCD count */
#define STC_CMR_UP	0x0008		/* 0: count down, 1: count up */
#define STC_CMR_TCPL	0x0005		/* active low  terminal count pulse */
#define STC_CMR_HIZ	0x0004		/* inactive, output high impedance */
#define STC_CMR_TOGGLE	0x0002		/* terminal count toggled */
#define STC_CMR_TCPH	0x0001		/* active high terminal count pulse */
#define STC_CMR_LOW	0x0000		/* inactive, output low /*

/* equates for commands */

#define STC_CMD_LDPEG	0xff00		/* load data pointer register */

#define STC_CMD_ARMS	0xff20		/* arm all selected counters */
#define STC_CMD_LOADS	0xff40		/* load all selected counters */
#define STC_CMD_LARMS	0xff60		/* load and arm all selected counters */
#define STC_CMD_DARMSVS	0xff80		/* disarm and save all selected */
#define STC_CMD_SAVES	0xffa0		/* save all selected in hold register */
#define STC_CMD_DARMS	0xffc0		/* disarm all selected counters */

#define STC_CMD_SETN	0xffe8		/* set   output bit n (000 < n > 110) */
#define STC_CMD_CLRN	0xffe0		/* clear output bit n (000 < n > 110) */
#define STC_CMD_STEPN	0xfff0		/* step counter n (000 < n > 110) */

#define STC_CMD_DPSOFF	0xffe8		/* clear MM14 (disable data ptr seq) */
#define STC_CMD_FOUTOFF	0xffee		/* set   MM12 (gate off FOUT) */
#define STC_CMD_16BIT	0xffef		/* set   MM13 (enter 16 bit bus mode) */

#define STC_CMD_DPSON	0xffe0		/* clear MM14 (enable data ptr seq) */
#define STC_CMD_FOUTON	0xffe6		/* clear MM12 (gate on  FOUT) */
#define STC_CMD_8BIT	0xffe7		/* clear MM13 (enter 8 bit bus mode) */
#define STC_CMD_RST	0xffff		/* master reset */

#define STC_CMD_EMODE	0x0000		/* mode register element */
#define STC_CMD_ELOAD	0x0008		/* load register element */
#define STC_CMD_EHOLD	0x0010		/* hold register element */
#define STC_CMD_EHOLDS	0x0018		/* hold registers only element */

#define STC_CMD_EALRM1	0x0000		/* alarm register 1 element */
#define STC_CMD_EALRM2	0x0008		/* alarm register 2 element */
#define STC_CMD_EMSTMD	0x0010		/* master mode register */
#define STC_CMD_ESTATS	0x0018		/* status register */

#define STC_CMD_G1	0x0001		/* group counter 1 */
#define STC_CMD_G2	0x0002		/* group counter 2 */
#define STC_CMD_G3	0x0003		/* group counter 3 */
#define STC_CMD_G4	0x0004		/* group counter 4 */
#define STC_CMD_G5	0x0005		/* group counter 5 */
#define STC_CMD_GCTL	0x0007		/* group control registers */

#define STC_CMD_SALL	0x001f		/* register select 5 */
#define STC_CMD_S5	0x0010		/* register select 5 */
#define STC_CMD_S4	0x0008		/* register select 4 */
#define STC_CMD_S3	0x0004		/* register select 3 */
#define STC_CMD_S2	0x0002		/* register select 2 */
#define STC_CMD_S1	0x0001		/* register select 1 */

#ifdef __cplusplus
}
#endif

#endif /* __INCam9513h */
