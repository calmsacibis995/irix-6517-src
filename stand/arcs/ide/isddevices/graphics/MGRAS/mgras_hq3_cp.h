/*
** Copyright 1993, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
** $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/isddevices/graphics/MGRAS/RCS/mgras_hq3_cp.h,v 1.3 1996/04/08 18:22:16 kkn Exp $
*/

#ifdef __OBSOLETE___
/*
 * CP tokens used in the bringup CP microcode only
 * going obsolete
 */
#define GE_BGNLINE              506
#define GE11_BGNLINE            (GE_BGNLINE << CMD_TOKEN)
#define GE_LINEVERTEX           507
#define GE11_LINEVERTEX         (GE_LINEVERTEX << CMD_TOKEN)
#define GE_ENDLINE              508
#define GE11_ENDLINE            (GE_ENDLINE << CMD_TOKEN)
#define GE_BGNTRIS              509
#define GE11_BGNTRIS            (GE_BGNTRIS << CMD_TOKEN)
#define GE_TRISVERTEX           510
#define GE11_TRISVERTEX         (GE_TRISVERTEX << CMD_TOKEN)
#define GE_ENDTRIS              511
#define GE11_ENDTRIS            (GE_ENDTRIS << CMD_TOKEN)

/*
 * CP tokens used in C simulator only
 */
#define ISET_FETCH2048	0x123
#define ISET_BRANCH3_7	0x122
#define ISET_BRANCH3_6	0x121
#define ISET_BRANCH3_5	0x120
#define ISET_BRANCH3_4	0x11f
#define ISET_BRANCH3_3	0x11e
#define ISET_BRANCH3_2	0x11d
#define ISET_BRANCH3_1	0x11c
#define ISET_FETCH1_6	0x11b
#define ISET_FETCH1_5	0x11a
#define ISET_FETCH1_4	0x119
#define ISET_FETCH1_3	0x118
#define ISET_FETCH1_2	0x117
#define ISET_FETCH1_1	0x116
#define ISET_BRANCH12	0x115
#define ISET_MIX5	0x114
#define ISET_DMOVE5_6	0x113
#define ISET_DMOVE5_5	0x112
#define ISET_DMOVE5_4	0x111
#define ISET_DMOVE5_3	0x110
#define ISET_DMOVE5_2	0x10f
#define ISET_DMOVE5_1	0x10e
#define ISET_MIX4	0x10d
#define ISET_DMOVE4_3	0x10c
#define ISET_DMOVE4_2	0x10b
#define ISET_DMOVE4_1	0x10a
#define ISET_DMOVE3	0x109
#define ISET_MIX3	0x108
#define ISET_MIX2	0x107
#define ISET_MIX1	0x106
#define ISET_IMOVE2	0x105
#define ISET_IMOVE1	0x104
#define ISET_DMOVE12_4  0x103
#define ISET_DMOVE12_3  0x102
#define ISET_DMOVE12_2  0x101
#define ISET_DMOVE12_1  0x100

#define CP_TEST1 0xff
#endif /* __OBSOLETE__ */

#define CP_PASS_THROUGH_GE_FIFO 0xfe
#define CP_PASS_THROUGH_GE_WRAM 0xfd
#define CP_PASS_THROUGH_GE_WRAM_DELAYED 0xfc
#define CP_PASS_THROUGH_GE_FIFO_3	0xfb
#define CP_PASS_THROUGH_GE_WRAM_4	0xfa
#define CP_PASS_THROUGH_GE_WRAM_5	0xf9
#define CP_SET_CP_FLAGS			0xf8
#define CP_DELAY			0xf7
#define CP_GE_DMA_HEADER_SIZE		0xf6
#define CP_GE_FTOF_1GE			0xf5

/*
 * GE11 tokens used in ge-stub (in hq3 simulation) only
 * token is in the high-order 16 bits of the word.
 * for all except GE11_REAL_COMMAND, number of words following
 *	the token (arguments) are in the low-order 16-bits
 * for GE11_REAL_COMMAND, the number of arguments
 *	depends on the command (the ge ucode).
 */
#define GE11_REAL_COMMAND	0
#define GE11_SET_REBUS_SYNC	1
#define GE11_FB_FB_COPY		2
#define GE11_SAVE_RE_REGS	3
#define GE11_COMMENT		4
#define GE11_WRITE_RE_REG	5
#define GE11_SET_GE_FLAGS	6
#define GE11_RESET_WRAM_WRPTR	7
#define GE11_WRITE_HQ_REG       8

/*
 * tokens that will eventually become real ge commands
 */
#define simge_FTOF_DATA		0xf000
#define simge_FTOF_1GE_RD_FIRST	0xf001
#define simge_FTOF_1GE_RD_NEXT	0xf002
#define simge_FTOF_1GE_RD_LAST	0xf003
#define simge_FTOF_1GE_WR_FIRST	0xf004
#define simge_FTOF_1GE_WR_NEXT	0xf005
#define simge_FTOF_1GE_WR_LAST	0xf006
