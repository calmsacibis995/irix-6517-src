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

typedef struct saareg{
        char    subaddr;
        char    data;
}SAAREG;

typedef struct saa7186 {
        SAAREG  fmt_seq;
        SAAREG  Out_pix_line;
        SAAREG  Out_pix_line1;
        SAAREG  In_pix_line;
        SAAREG  In_pix_line1;
        SAAREG  Horiz_win_size;
        SAAREG  pix_dec_filter;

        SAAREG  Out_line_field;
        SAAREG  Out_line_field1;
        SAAREG  In_line_field;
        SAAREG  In_line_field1;
        SAAREG  Vert_win_start;
        SAAREG  AFS_vert;

        SAAREG  Vert_bypass_start;
        SAAREG  Vert_bypass_start1;
        SAAREG  Vert_bypass_count;
        SAAREG  Vert_bypass_count1;

        SAAREG  Chroma_VL;
        SAAREG  Chroma_VU;
        SAAREG  Chroma_UL;
        SAAREG  Chroma_UU;

        SAAREG  Byte10;
}SAA7186;


/**
 ** HORIZANTAL CONTROL 
 **/ 
#define SUB_FMT_SEQ 0x00
#define RTB	0x80
#define RTB_DEFAULT	0x1	/* anti-gamma table bypass */

#define OFx_DEFAULT	0x0	/* both fields non-interlaced */
#define OF1	0x40
#define OF0	0x20

#define VPE_DEFAULT	0x1
#define VPE	0x10

#define LWx_DEFAULT	0x0	/* ?? check with  H/W */
#define LW1	0x08
#define LW0	0x04

#define FS0_1_DEFAULT	0x1	/* YUV4:2:2 16 bit */
#define FS1	0x02
#define FS0	0x01

#define	SUB_PIXLINE_OUT	0x01
#define XDx_DEFAULT	0xff

#define SUB_PIXLINE_OUT1	0x04

#define	SUB_PIXLINE_IN	0x02
#define XSx_DEFAULT	0xff

#define SUB_PIXLINE_IN1 0x04

#define SUB_HSTART	0x03
#define XOx_DEFAULT	0x10	/* ?? */

#define SUB_PIXDEC_FIL	0x04
#define HFx_DEFAULT	0x4	/* ?? */
#define HF2 	0x80
#define HF1 	0x40
#define HF0 	0x20

#define XO8_DEFAULT 	0x0
#define XO8 	0x10

#define XS89_DEFAULT	0x3
#define XS9 	0x08
#define XS8 	0x04

#define XD89_DEFAULT	0x3
#define XD9 	0x02
#define XD8 	0x01

/**
 ** VERTICAL CONTROL 
 **/ 

#define SUB_LINEFIELD_OUT 0x05
#define YDx_DEFAULT	0xff	

#define SUB_LINEFIELD_IN 0x06
#define	YSx_DEFAULT	0xff

#define SUB_VSTART 0x07
#define	YOx_DEFAULT 	0x06

#define SUB_AFSV	0x08
#define AFS_DEFAULT 	0x1
#define AFS 	0x80

#define VPx_DEFAULT	0	
#define VP1 	0x40
#define VP0 	0x20

#define YO8x_DEFAULT	0
#define YO8 	0x10

#define YS89_DEFAULT	3
#define YS9 	0x08
#define YS8 	0x04

#define YD89_DEFAULT	3
#define YD9 	0x02
#define YD8 	0x01


/**
 ** BYPASS CONTROL 
 **/

#define SUB_VBYPASS 0x09
#define VSx_DEFAULT	0

#define SUB_SUB_VBYPASS1 0x0B
#define VS8 	0x10

#define SUB_VBYPASS_COUNT	0x0A
#define VCx_DEFAULT		0

#define SUB_VBYPASS_COUNT1 	0x0B
#define TCC_DEFAULT  	0x0
#define VS8_DEFAULT 	0x0
#define VC8_DEFAULT 	0x0
#define POE_DEFAULT 	0x1
#define TCC  	0x80	/* Data book is wrong about position of this bit */
#define VS8 	0x10
#define VC8 	0x04
#define POE 	0x01

/**
 ** CHROMA KEY CONTROL
 **/

#define SUB_VL		0x0C
#define VLx_DEFAULT	0x00

#define SUB_VU		0x0D
#define VUx_DEFAULT	0xff

#define SUB_UL		0x0E
#define ULx_DEFAULT	0x00

#define SUB_UU		0x0F
#define UUx_DEFAULT	0x00


#define SUB_10bit	0x10
#define MCT_DEFAULT 	0x0
#define QPL_DEFAULT 	0x0
#define QPP_DEFAULT  	0x1
#define TTR_DEFAULT 	0x1
#define EFE_DEFAULT 	0x1
#define MCT 	0x10
#define QPL 	0x08
#define QPP  	0x04
#define TTR 	0x02
#define EFE 	0x01

#define SUBADDRESS 17
#define SAA7186_I2C_ADDR_W	0xB8
#define SAA7186_I2C_ADDR_R	0xB9

