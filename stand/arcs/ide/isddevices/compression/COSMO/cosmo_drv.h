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
#ifndef __JPEG_VIDEO_H_
#define __JPEG_VIDEO_H_

#include <sys/types.h>   /*for pid_t*/

/* TODO: the same as GFX_BASE for now */
#define JPEG_BASE	100 

/* TODO: Fix this later */
#ifndef B_WRITE
#define B_WRITE 0
#endif
#ifndef B_READ
#define B_READ 1
#endif

enum	CLNode_name	{NullNode=0, DRV_MEM, DRV_VIDEO, DRV_ANOLOG_VIDEO};
#define NullNode 0
#define DRV_MEM 1
#define DRV_VIDEO 2


/* TODO: the same as GFX_BASE for now */
#define JPEG_BASE	100 

#define COMPRESS    	0
#define DECOMPRESS  	1

#define	JPEG_BOARDS	1

#define MAX_JPEG_BOARDS 2
/* 
 Read/write permission
*/
#define RR 1
#define WW 2
#define RW 3
#define DF 7
 
#define NTSC_MAX_XSIZE  640
#define NTSC_MAX_YSIZE  480
#define PAL_MAX_XSIZE   768
#define PAL_MAX_YSIZE   576

/*
 * Theoretically, we may want to compress up to 4K by 4K images.
 * CL can do a 65K x 65K frame size. Therefore, frame tilting must 
 * be performed in CL to break up larger images into 
 * several small ones to be able to be handled by the hardware.
 * 
 */
 
#define MAX_XSIZE       	PAL_MAX_XSIZE
#define MAX_YSIZE       	PAL_MAX_YSIZE

#define VERT_BLANKING   	(MAX_XSIZE * 11)
#define MAX_YUV_DATA_SIZE   	(VERT_BLANKING + (MAX_XSIZE * (MAX_YSIZE / 2)))

#define YUV_DATA_SIZE(b,x,y)    (b + (x * (y / 2)))
#define YUV_VIS_DATA_SIZE(x,y)	(x * (y / 2))
#define BUFFLEN(x,y,z)		(x * y * z )

/*
 * There are two categories of ioctl; setup, and
 * runtime. The setup ones are divided into two
 * levels. These are the top-level ioctl.
 * You do not need to be board owner
 * (have attached to the board) to use these.
 */

#define	JPEG_GET_BOARD_INFO	(JPEG_BASE              +20)
#define	JPEG_ATTACH_BOARD	(JPEG_GET_BOARD_INFO 	+1)
#define	JPEG_DETACH_BOARD	(JPEG_ATTACH_BOARD	+1)
#define	JPEG_RESERVED		(JPEG_DETACH_BOARD	+1)

/*
 * These are the low-level ioctl values which allow
 * you to set up a particualr board and change the
 * hardware parameters.   You need to have gone 
 * through an attach sequence and become board owner
 * before you can use these. These also include the
 * "runtime" ioctls.
 */
#define	JPEG_INITIALIZE		(JPEG_RESERVED		+1)

#define JPEG_LOAD_HUFF_TABLE    (JPEG_INITIALIZE	+1)
#define JPEG_GET_HUFF_TABLE     (JPEG_LOAD_HUFF_TABLE	+1)

#define JPEG_LOAD_Q_TABLE       (JPEG_GET_HUFF_TABLE	+1)
#define JPEG_GET_Q_TABLE        (JPEG_LOAD_Q_TABLE	+1)

#if 0
  Removed, we use set param to set mode 
#define JPEG_SET_MODE		(JPEG_GET_Q_TABLE	+1)
#define JPEG_GET_MODE		(JPEG_SET_MODE		+1)
#endif 

#define JPEG_SET_SIZE		(JPEG_GET_Q_TABLE	+1)
#define JPEG_GET_SIZE		(JPEG_SET_SIZE		+1)

#define JPEG_PUT_QUEUE		(JPEG_GET_SIZE		+1)
#define JPEG_GET_QUEUE		(JPEG_PUT_QUEUE		+1)

#define JPEG_SET_PARAMS		(JPEG_GET_QUEUE		+1)
#define JPEG_GET_PARAMS		(JPEG_SET_PARAMS	+1)

#define JPEG_START		(JPEG_GET_PARAMS	+1)

#define JPEG_SETUP_DMA		(JPEG_START		+1)
#define JPEG_START_DMA		(JPEG_SETUP_DMA		+1)

#define JPEG_GET_STATUS		(JPEG_START_DMA		+1)

#define JPEG_FLUSH		(JPEG_GET_STATUS	+1)

#define JPEG_GET_REG		(JPEG_FLUSH		+1)
#define JPEG_SET_REG		(JPEG_GET_REG		+1)

#define JPEG_PASS		(JPEG_SET_REG		+1)

#define JPEG_USE_EXCLUSIVE	(JPEG_PASS		+1)

#define JPEG_SET_SCAN_PARAMETERS 	(JPEG_USE_EXCLUSIVE		+1)
#define JPEG_GET_SCAN_PARAMETERS 	(JPEG_SET_SCAN_PARAMETERS	+1)
#define JPEG_SET_FRAME_PARAMETERS 	(JPEG_GET_SCAN_PARAMETERS	+1)
#define JPEG_GET_FRAME_PARAMETERS 	(JPEG_SET_FRAME_PARAMETERS	+1)

/*
 * This is the struct returned
 * by the JPEG_GET_BOARD_INFO ioctl.
 */
#define JPEG_BOARD_NAME	"JPEG_DW_0"
#define J_INFO_NAME_SIZE 64 

typedef struct jpeg_info {
	int board_slot; /* board slot number */
        int board_type;		/* board id (contiguous from zero)	*/
        char name[J_INFO_NAME_SIZE]; /* unique name for this graphics	*/
}JInfo;


/*
 * This is the struct returned by 
 * the JPEG_BOARD_ATTACH and JPEG_BOARD_DEATTACH ioctl.
 */
#define NO_TYPE_JPEG           0
#define JPEG_TYPE_PX0          1

typedef struct jpeg_attach_board_args {
  unsigned long int board;
  unsigned long int vaddr;
}ATTACH_BOARD_ARGS;
        
typedef struct jpeg_deattach_board_args {
  unsigned long int board;
  unsigned long int vaddr;
}DEATTACH_BOARD_ARGS;

/*
 * This is the struct passed down by  
 * JPEG_LOAD_HUFF_TABLE and JPEG_LOAD_Q_TABLE ioctl.
 */

#define JPEG_TABLE_MAX_LENGTH	64
#define	JPEG_DHT_LUMINANCE_DC   0x00
#define	JPEG_DHT_LUMINANCE_AC   0x10
#define	JPEG_DHT_CHROMINANCE_DC 0x01
#define	JPEG_DHT_CHROMINANCE_AC 0x11

/*
 * This struct is passed down by LOAD Q and H
 * ioctl calls
 */
#if 0
struct jpeg_table_args{ 
	unsigned short	q_table[JPEG_TABLE_MAX_LENGTH];
};
#else 
struct jpeg_table_args{ 
	unsigned short	luma_table[JPEG_TABLE_MAX_LENGTH / 2];
	unsigned short	chroma_table[JPEG_TABLE_MAX_LENGTH / 2];
};
#endif


#define JPEG_MAX_COMPONENTS 10
/*
 * This struct is passed down by 
 * JPEG_SET_SCAN_PARAMETERS
 */

struct jpeg_scan_args {
	int	num_components;
	struct jpeg_scan_comp_info {
		unsigned char id;
		unsigned char table_id;
	} components[JPEG_MAX_COMPONENTS];
	unsigned char spectral_start;
	unsigned char spectral_end;
	unsigned char successive_approximation;
};


/*
 * This is the struct passed down by
 * JPEG_SET_FRAME_PARAMETERS
 * 
 * The idea is to pass in the parameters supplied in the 
 * regular JPEG data stream by the SOF0, DHT, DQT, and SOS markers.
 * I don't known just which really fall out of CL and into the H/W
 * yet, but most probably do.
 */
typedef struct jpeg_frame_args {
	int	precision;
	int	height;
	int	width;
	int	num_components;
	struct	jpeg_frame_comp_info {
		unsigned char id;
		unsigned char h_samp;
		unsigned char v_samp;
		unsigned char q_table_id;
	} components[JPEG_MAX_COMPONENTS];
}JPEG_FRAME_ARGS;


/*
 * The follow arguments struct is passed down with a
 * JPEG_SET_MODE ioctl.    Only non-zero fields are
 * acted upon, so that you can set or change the values
 * of selected parameters.   You can also read back
 * the current setup into the same struct, and then
 * change selected values.
 *
 * The invalid values in the following enums must be 0.
 */
#define DRV_RGB         0
#define DRV_GRAYSCALE   1
#define DRV_YUV         2
#define DRV_YUV422	3

enum	JImageDirection  {NO_DIR = 0,	GIO_D1,	D1_GIO,	GIO_GIO};
enum 	JColorSpace	{_BYPASS_= 0,	_422_,	_1T_444_,	_2T_444_,
					_3T_444_,	_4444_,
					_RGB_422_,	_444_422_, NO_560COLOR};

enum	JVIDSTD		{NO_JPEGSTD = 0, 	NTSCSTD, 	PALSTD};
enum	JMediaRate	{NO_Rate = 0, 	Rate_30_s, 	Rate_60_s};
enum	JCD_Mode	{NO_Mode = 0, 	Comp_Mode, 	DeComp_Mode};
enum	JImageSrc	{NO_ImageSRC = 0, SRC_GIO,	SRC_D1};
enum	JImageDest	{NO_ImageDest = 0, DEST_GIO,	DEST_D1};
enum	JScaleSize	{NO_ImageScale=0,_8_8, _7_8, _6_8, _5_8, _4_8, _3_8, _2_8, _1_8};
enum	JScaleFmt	{NO_ImageFmt=0,	_YUV42232, _YUV42216, _RGB888 };


#define ImageDirection_index	0x00
#define ImageDirection_flag	(1 << ImageDirection_index)

#define JColorSpace_index	0x01
#define JColorSpace_flag	(1 << JSRCColorSpace_index)

#define JD1ColorSpace_index	0x02
#define JD1ColorSpace_flag	(1 << JD1ColorSpace_index)

#define JMediaRate_index	0x03
#define JMediaRate_flag		(1 << JMediaRate_index)

#define JCD_Mode_index		0x04
#define JCD_Mode_flag		(1 << JCD_Mode_index)

#define JImageSrc_index		0x05
#define JImageSrc_flag		(1 << JImageSrc_index)

#define JImageDest_index		0x06
#define JImageDest_flag		(1 << JImageDest_index)

#define JScaleFmt_index		0x07
#define JScaleFmt_flag		(1 << JScaleFmt_index)

#define PHYS_TO_USER_MAP(x) (PHYS_TO_K1(x)-mapconst)

/*
 * Parameters
 */
#define DRV_MAX_NUMBER_OF_PARAMS (256)
#define DRV_ParamID(type, n)             ((n) | (((type) & 0x3) << 30))
#define DRV_ParamNum(paramid)	(paramid & 0x37777777)

#define DRV_VIDEO_OUT               (DRV_ParamID(1,0))
#define DRV_SCALED_WIDTH            (DRV_ParamID(1,1))
#define DRV_SCALED_HEIGHT           (DRV_ParamID(1,2))
#define DRV_WIDTH                   (DRV_ParamID(1,3))
#define DRV_HEIGHT                  (DRV_ParamID(1,4))
#define DRV_SOURCE                  (DRV_ParamID(1,5))
#define DRV_DEST                    (DRV_ParamID(1,6))
#define DRV_MODE                    (DRV_ParamID(1,7))
#define DRV_FRAME_CONTROL           (DRV_ParamID(1,8))
#define DRV_COLORSPACE		    (DRV_ParamID(1,9))
#define DRV_PRECISION		    (DRV_ParamID(1,10))
#define DRV_STOP		    (DRV_ParamID(1,11))
#define DRV_STATUS		    (DRV_ParamID(1,12))
#define DRV_LEFTBLANKING	    (DRV_ParamID(1,13))
#define DRV_RIGHTBLANKING	    (DRV_ParamID(1,14))
#define DRV_TOPBLANKING		    (DRV_ParamID(1,15))
#define DRV_BOTTOMBLANKING	    (DRV_ParamID(1,16))
#define DRV_CLIPPED_WIDTH           (DRV_ParamID(1,17))
#define DRV_CLIPPED_HEIGHT          (DRV_ParamID(1,18))

#define DRV_ORIGINAL_FORMAT         (DRV_ParamID(1,19))

#define MAX_PARAM		(20+17)
#define DRV_FRAME_ADV		0x0
#define DRV_FRAME_SKIP		0x1
#define	DRV_FRAME_REPEAT	0x2
#define DRV_AUTO_SCAN		0x3
#define DRV_MANUAL_SCAN		0x4
#define DRV_PIO                 0x5
#define DRV_DMA                 0x6
#define DRV_ASYNC_STOP          0x7
#define DRV_SPIN_ODD_FIELD      0x8

/*
 * MUST Keep Consistant with rb.h
 * THIS WILL ALL GO AWAY WITH THE NEW Ring Buffer
 * The ring buffers are maintained with
 */
typedef struct DrvRingBuffer{
    char *beginning;/* The begining of the entire buffer. */
    char *end;      /* The end of the entire buffer. */
    long tail;      /* An index to the beginning of valid data. */
    long head;      /* An index to the beginning of free data space (after the valid data). */
    long size;      /* The size of the total buffer */
    long done;      /* A flag to say no more data will be Put to the buffer */
    pid_t pid;      /* Process ID of blocked Query*() process */
} DrvRingBuffer;


enum DMA_Mode	{NO_DMA_MODE = 0, SINGLE_BUFF, CIRCULAR_BUFF};
enum DMA_CH	{NO_DMA_CH = 0,  CC_CH, UCC_CH };

/*
 * dma arg used to pass CIC and UIC dma information
 * for a particular open to the driver
 * 
 */
typedef struct  jpeg_dma_args {
        long            flags;		/* dma flags */

	/*
	 * Compress DMA channel
	 */
        long		CCdirection;	/* direction:   B_WRITE= 0 = GIO->JPEG,
							B_READ = 1 = JPEG->GIO 
							NULL   = -1	 */

	DrvRingBuffer	*CCqctrl;	/* CIC control circular buffer */
	DrvRingBuffer	*CCqdata;	/* data buffer, has to be a multiple of 8
					   for the current DMA to work
					 */

	/*
	 * UnCompress DMA Channel 
	 */
        long		UCCdirection;	/* direction:   B_WRITE= 0 = GIO->JPEG,
							B_READ = 1 = JPEG->GIO 
							NULL = -1	 */

	DrvRingBuffer	*UCCqctrl;	/* UIC control circular buffer */
	DrvRingBuffer	*UCCqdata;	/* data buffer, has to be a multiple of 8
					   for the current DMA to work
					 */

	/*
	 * CIC and UIC DMA parameters 
	 */
        long            xlen;
        long            ylen;
        int             nframes;	/* == NULL :: Q mode */
        int             burst;
        int             delay;
        enum DMA_Mode   dma_mode;

} JPEG_DMA_ARGS;

typedef struct dma_info {
	JPEG_DMA_ARGS *source;
	JPEG_DMA_ARGS *dest;
} JPEG_DMA_INFO;


/*
 * This struct is passed down with the JPEG_SET_PARAMS ioctl.
 * It allows you to set a variety of numeric parameters for
 * the jpeg board.
 */
typedef struct jpeg_params_args {
	int param;
	int value;
}JPEG_PARAMS_ARGS;

#define	APP_FIELDS	0x1
#define D1_FIELDS	0x2
#define SKIP_STATUS	0x4
#define ADV_STATUS	0x8
#define REP_STATUS	0x10

/*
 * Compressed Image Header, one for each entry in the compression queue
 */ 
typedef struct Iheader {
	unsigned	CImagesize;
	unsigned	CImageread;
	unsigned	timestamp;
	unsigned	framenumber;
	unsigned	Qid;
} ImageHeader;


#endif /* __JPEG_VIDEO_H_ */



