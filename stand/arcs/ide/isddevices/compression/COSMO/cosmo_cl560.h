/******************************************************************************/
/******  Compression Monitor for C-Cube Microsystems CL550 processor     ******/
/******  June 14 , 1991                                                  ******/
/******  Version 2.1                                                     ******/
/******************************************************************************/


/*****************************************************************************
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

#ifndef __JPEG_CL560_H_
#define __JPEG_CL560_H_

#define _NumInitReg     11
#define _NumPixelMode   8
#define _RegName        12

typedef unsigned char *Ptr;
typedef unsigned char Byte;

#define xMAX 96
#define yMAX 96

#define _C_Data_size (xMAX*yMAX*2)  
#define _I_Data_size (xMAX*yMAX*4) 

extern unsigned short CL560HPeriod,CL560HDelay,CL560HActive,CL560HSync;

typedef struct RegStruct
{
    char name[80];
    unsigned long address;
    short permission;
    short width;
} JPEG_reg_struct;

#define MODE_MSTR   1
#define MODE_INTL  0 
#define MODE_FEND  1 


/* CL560 Chip registers */
#define _HPeriod            0
#define _HSync              1
#define _HDelay             2
#define _HActive            3
#define _VPeriod            4
#define _VSync              5
#define _VDelay             6
#define _VActive            7
#define _DCT                8
#define _Config             9
#define _Huff_Enable        10
#define _S_reset            11
#define _Start              12
#define _HV_Enable          13
#define _Flags              14
#define _NMRQ_Enable        15
#define _DRQ_Enable         16
#define _StartFrame         17  /*** 0x9020   550 only ***/
#define _Version            18
#define _Init_1             19
#define _Init_2             20
#define _CodecSeq           21
#define _CodecCompH         22
#define _CodecCompL         23
#define _CoderAttr          24
#define _CodingIntH         25
#define _CodingIntL         26
#define _DecLength          27
#define _DecMarker          28
#define _DecResume          29
#define _DecDPCM            30
#define _CodeOrder          31
#define _Init_3             32
#define _Quant              33
#define _QuantSelect        34
#define _QuantSync          35
#define _QuantYC            36
#define _QuantAB            37
#define _R2Y                38
#define _G2Y                39
#define _B2Y                40
#define _R2U                41
#define _G2U                42
#define _B2U                43
#define _R2V                44
#define _G2V                45
#define _B2V                46
#define _VideoLatency       47
#define _HControl           48
#define _VControl           49
#define _VertLineCount      50
#define _Init_4             51
#define _Init_7             52
#define _FIFO               53
#define _HuffYAC            54
#define _HuffYDC            55
#define _HuffCAC            56
#define _HuffCDC            57
#define _Codec              58
#define _Block              59
#define _BlockAddress       60
#define _Init_5             61
#define _Init_6             62
#define _IRQ1_Mask          63  /* 0x9018     560/570 only */
#define _IRQ2_Mask          64  /* 0x9028     560/570 only */
#define _FRMEND_Enable      65  /* 0x902c     560/570 only */
#define _CoderSync          66  /* 0xa020     560/570 only */
#define _CompWordCountH     67  /* 0xa024     560/570 only */
#define _CompWordCountL     68  /* 0xa028     560/570 only */
#define _CoderRCActive      69  /* 0xa02c     560 only */
#define _CoderRCEnable      70  /* 0xa030     560 only */
#define _CoderRobustness    71  /* 0xa034     560/570 only */
#define _CoderPadding       72  /* 0xa038     560/570 only */
#define _DecoderStart       73  /* 0xa820     560 Only */
#define _DecoderMismatch    74  /* 0xa824     560/570 only */
#define _DecoderMismatchEr  75  /* 0xa828     560/570 only */
#define _FIFO_LevelL        76  /* 0xda04     560/570 Only */
#define _QTable1            77  /* 0xb800 */   
#define _QTable2            78  /* 0xb900 */ 
#define _QTable3            79  /* 0xba00 */
#define _QTable4            80  /* 0xbb00 */ 



#define num_register        82 


/************************ Initialization Register ****************************/
/* Initialization Register numbers used to index into the initialization     */
/*    value arrays                                                           */

#define Init_1              0
#define Init_2              1
#define Init_3              2
#define Init_4              3
#define Init_5              4
#define Init_6              5
#define Init_7              6
#define QuantSync           7
#define QuantYC             8
#define QuantAB             9
#define VideoLatency        10

#define Config              0
#define CodecSeq            1
#define CodecCompH          2
#define CodecCompL          3
#define CoderAttr           4
#define CodingIntH          5
#define CodingIntL          6
#define DecLength           7
#define CodeOrder           8
#define QuantSelect         9
#define DCT                10

/* CL560 registers */
#define cl560CoderSync     11

/**************************** Flags bits ************************************
        These are masks that correspond to the CL560 status indicators.
        They are used when reading the Flags register or when writing to
        the NMRQ/IRQx and DRQ mask registers.
*****************************************************************************/

#define _Flags_FIFO_NotFull     0x00008000L
#define _Flags_FIFO_NotEmpty    0x00004000L
#define _Flags_Codec_RegBusy    0x00002000L
#define _Flags_BusError         0x00001000L
#define _Flags_FrameEnd         0x00001000L     /* 560/570 only */
#define _Flags_MarkerCode       0x00000800L
#define _Flags_VSync            0x00000400L
#define _Flags_VInactive        0x00000200L
#define _Flags_Field0           0x00000100L
#define _Flags_FIFO_Empty       0x00000080L
#define _Flags_FIFO_QFull       0x00000040L
#define _Flags_FIFO_HalfFull    0x00000020L
#define _Flags_FIFO_3QFull      0x00000010L
#define _Flags_FIFO_NotQFull    0x00000008L
#define _Flags_FIFO_NotHalfFull 0x00000004L
#define _Flags_FIFO_Not3QFull   0x00000002L
#define _Flags_FIFO_Late        0x00000001L
#define _Flags_FIFO_All_Q_Not_Full (_Flags_FIFO_Not3QFull|\
				    _Flags_FIFO_NotHalfFull|\
				    _Flags_FIFO_NotQFull)

/**************************** Interrupt bits ********************************/
/*  These represent NMRQ/DRQ register masks, used by various elements of    */
/*     the Compression Monitor.                                             */

#define _Int_FIFO_NotFull       0x00008000L
#define _Int_FIFO_NotEmpty      0x00004000L
#define _Int_Codec_RegBusy      0x00002000L
#define _Int_BusError           0x00001000L
#define _Int_MarkerCode         0x00000800L
#define _Int_VSync              0x00000400L
#define _Int_VInactive          0x00000200L
#define _Int_FrameEnd           0x00000100L
#define _Int_FIFO_Empty         0x00000080L
#define _Int_FIFO_QFull         0x00000040L
#define _Int_FIFO_HalfFull      0x00000020L
#define _Int_FIFO_3QFull        0x00000010L
#define _Int_FIFO_NotQFull      0x00000008L
#define _Int_FIFO_NotHalfFull   0x00000004L
#define _Int_FIFO_Not3QFull     0x00000002L
#define _Int_FIFO_Late          0x00000001L
#define FIFO_INTS	(_Int_FIFO_NotFull|_Int_FIFO_NotEmpty|\
			 _Int_FIFO_Empty|_Int_FIFO_QFull|_Int_FIFO_HalfFull|\
			 _Int_FIFO_3QFull|_Int_FIFO_NotQFull|\
			 _Int_FIFO_NotHalfFull|_Int_FIFO_Not3QFull)

extern unsigned short CL560HPeriod,CL560HDelay,CL560HActive,CL560HSync;
extern unsigned short CL560VPeriod,CL560VDelay,CL560VActive,CL560VSync;
extern unsigned short CL560ImageWidth,CL560ImageHeight;
extern unsigned short BoardConfig;

#define _num_init_reg     11
#define _num_pixel_mode    8
#define _reg_name         12 

extern JPEG_reg_struct CL560_register[];
extern JPEG_reg_struct board_register[];

#define _Decoder_Mismatch_Occurred	1

#endif /* __JPEG_CL560_ */

