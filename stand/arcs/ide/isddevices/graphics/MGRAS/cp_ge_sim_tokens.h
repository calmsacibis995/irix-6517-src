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
** $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/isddevices/graphics/MGRAS/RCS/cp_ge_sim_tokens.h,v 1.6 1996/06/05 01:32:19 keiming Exp $
*/


/*
 * CP tokens used in the bringup CP microcode only
 * going obsolete
 */
#define GE_BGNLINE              0xe9
#define GE11_BGNLINE            (GE_BGNLINE << CMD_TOKEN)
#define GE_LINEVERTEX           0xe8
#define GE11_LINEVERTEX         (GE_LINEVERTEX << CMD_TOKEN)
#define GE_ENDLINE              0xe7
#define GE11_ENDLINE            (GE_ENDLINE << CMD_TOKEN)
#define GE_BGNTRIS              0xe6
#define GE11_BGNTRIS            (GE_BGNTRIS << CMD_TOKEN)
#define GE_TRISVERTEX           0xe5
#define GE11_TRISVERTEX         (GE_TRISVERTEX << CMD_TOKEN)
#define GE_ENDTRIS              0xe4
#define GE11_ENDTRIS            (GE_ENDTRIS << CMD_TOKEN)

/*
 * CP tokens used in C simulator only
 */
#define ISET_ATTR_STORE_PATTERN_TEST_END 0xb8
#define ISET_ATTR_STORE_PATTERN_TEST_1 0xb7
#define CP_PASS_THROUGH_NEXTGE 0xb6
#define CP_TEST1 0xb5
#define CP_CONV_WR8             0xb4
#define CP_CONV_CHECK8          0xb3
#define CP_CONV_WR16            0xb2
#define CP_CONV_CHECK16         0xb1
#define CP_READ_CONTEXT        0xb0
#define CP_CONV_CHECK32         0xe7
#define	DFIFO_RD		0xe5
#define	DFIFO_WR		0xe6
#define	FIFO_DRAIN_ONE		0xe4
#define CP_GE_POWER		0xe3
#define GE_ERAM_WR		0xe2
#define GE_ERAM_RD		0xe1

/* #define ISET_FETCH2048	0xe3 */ 
/* #define ISET_BRANCH3_7	0xe2 */
/* #define ISET_BRANCH3_6	0xe1 */
#define ISET_BRANCH3_5	0xe0
#define ISET_BRANCH3_4	0xdf
#define ISET_BRANCH3_3	0xde
#define ISET_BRANCH3_2	0xdd
#define ISET_BRANCH3_1	0xdc
#define ISET_FETCH1_6	0xdb
#define ISET_FETCH1_5	0xda
#define ISET_FETCH1_4	0xd9
#define ISET_FETCH1_3	0xd8
#define ISET_FETCH1_2	0xd7
#define ISET_FETCH1_1	0xd6
#define ISET_BRANCH12_1	0xd5
#define ISET_MIX5	0xd4
#define ISET_DMOVE5_6	0xd3
#define ISET_DMOVE5_5	0xd2
#define ISET_DMOVE5_4	0xd1
#define ISET_DMOVE5_3	0xd0
#define ISET_DMOVE5_2	0xcf
#define ISET_DMOVE5_1	0xce
#define ISET_MIX4	0xcd
#define ISET_DMOVE4_3	0xcc
#define ISET_DMOVE4_2	0xcb
#define ISET_DMOVE4_1	0xca
#define ISET_DMOVE3	0xc9
#define ISET_MIX3	0xc8
#define ISET_MIX2	0xc7
#define ISET_MIX1	0xc6
#define ISET_IMOVE2	0xc5
#define ISET_IMOVE1	0xc4
#define ISET_DMOVE12_4  0xc3
#define ISET_DMOVE12_3  0xc2
#define ISET_DMOVE12_2  0xc1
#define ISET_DMOVE12_1  0xc0

/*
 * CP tokens used in context switch friendly iset test variants
 */
#define ISET_MIX4_FRIENDLY	0xea
#define ISET_MIX5_FRIENDLY	0xeb
#define ISET_DMOVE12_FRIENDLY_4	0xec
#define ISET_DMOVE12_FRIENDLY_3	0xed
#define ISET_DMOVE12_FRIENDLY_2	0xee
#define ISET_DMOVE12_FRIENDLY_1	0xef
#define ISET_DMOVE3_FRIENDLY	0xf0
#define ISET_DMOVE5_FRIENDLY_6	0xf1
#define ISET_DMOVE5_FRIENDLY_5	0xf2
#define ISET_DMOVE5_FRIENDLY_4	0xbf
#define ISET_DMOVE5_FRIENDLY_3	0xbe
#define ISET_DMOVE5_FRIENDLY_2	0xbd
#define ISET_DMOVE5_FRIENDLY_1	0xbc
#define ISET_BRANCH12_2		0xbb
#define ISET_BRANCH4		0xba
#define ISET_MUKESH		0xb9



#define CP_DUMP_FOREVER			0xf3
#define CP_GE_NOSAVE_NEXT_SWITCH	0xf4
#define CP_GE_FTOF			0xf5
#define CP_GE_DMA_HEADER_SIZE		0xf6
#define CP_DELAY			0xf7
#define CP_SET_CP_FLAGS			0xf8
#define CP_PASS_THROUGH_GE_WRAM_5	0xf9
#define CP_PASS_THROUGH_GE_WRAM_4	0xfa
#define CP_PASS_THROUGH_GE_FIFO_3	0xfb
#define CP_PASS_THROUGH_GE_WRAM_DELAYED 0xfc
#define CP_PASS_THROUGH_GE_WRAM 0xfd
#define CP_PASS_THROUGH_GE_FIFO 0xfe
#define CP_GE_SAVE_IDLE_HQ_STATE	0xff

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
#define simge_FTOF_RD_FIRST	0xf001
#define simge_FTOF_RD_NEXT	0xf002
#define simge_FTOF_RD_LAST	0xf003
#define simge_FTOF_WR_FIRST	0xf004
#define simge_FTOF_WR_NEXT	0xf005
#define simge_FTOF_WR_LAST	0xf006
