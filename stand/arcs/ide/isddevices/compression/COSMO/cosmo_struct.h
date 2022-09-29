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

#ifndef __JPEG_STRUCT_H_
#define __JPEG_STRUCT_H_
#include	"cosmo_drv.h"

typedef DrvRingBuffer RingBuffer;

#define UCCDMAHACK 1
#ifdef DEBUG
#define        PRINTF(x,y) printf(x,y);
#else
#define        PRINTF(x,y) /*x,y*/
#endif

#define TUD_MAX	64
#define TUND_MAX 128

/*
 * Mode block is passed between driver components at 
 * init and setup time to setup registers.
 * The actual register values are decopled through the 
 * use of a look up table indexed by the mode values. 
 */

#define NOT_SCALED_PATH	0
#define SCALE_X		1
#define SCALE_Y		2

struct JPEGMode {
	/* enum CL560_mode	{_BYPASS=0, _422=1, _1T_444=2, _2T_444=3,
	 *                         _3T_444=4, _4444=5, _RGB_422=6, _444_422=7};
	 */
	int	CL560_mode;
	int	SAA1786_Fmt;
	int	Wscale_pixel;
	int	Hscale_line;
        int	PCD8584_mode;
        int	JPEG_reg_mode;
	int     CL560Direction; /* enum Direction {COMPRESS=0, DECOMPRESS=1};*/
	int	path;		/* enum path {NO_DIR=0,GIO_D1,D1_GIO,GIO_GIO};*/
	int	QFactor;  
	int	LeftBlanking;
	int 	RightBlanking;
	int	TopBlanking;
	int	BottomBlanking;
	int	CL560ImageWidth;
	int	CL560ImageHeight;
	int	frame_rate;		/* frame rate control */
	int	read_ctl;		/* VFC field buffer read control */
	int	WClipR_pixel;		/* clip pixels right side */	
	int	HClipB_line;		/* Clip lines from bottom */
	/*
	 *	enum FilterAlgorithm {
	 *			      0-	ByPass
	 *			      1-	1/2(1+z**(-1))
	 *			      2-	1/4(1+2z**(-1)+z**(-2))) 
	 *			      3-	1/8(1+2z**(-1)+2z**(-2)+z**(-4))
	 *			      4-	1/16(1+2z**(-1)+2z**(-2)+2z**(-3)+2z**(-4)+
	 *					     2z**(-5)+2z**(-6)+2z**(-8))
	 *			      5-	ByPass + delay in Y Channel of 1T
	 *			      6-	1/16(1+3z**(-1)+3z**(-2)+z**(-2)+z**(-3)
	 *					    +z**(-4)+z**(-5)+z**(-6)+z**(-7))
	 *			      7-	1/8(1+3z**(-1)+3z**(-2)+z**(-3))
	 *			      }
	 */
	int	FilterAlgorithm;	/* Filtering Algorithm */
	int	ScaleFlag;		/* Say if 7186 is in the path */ 
};
typedef struct JPEGMode         JPEG_MODE;

/*
 *  DMA Transfer Control Records
 */
typedef struct transfer_control_record {
        paddr_t 	tcr_paddr;	/* physical address host memory */
        unsigned long	tcr_bcnt;	/* byte count (multiple of 4) */
        paddr_t		next_tcr;	/* physical address host memory */
} TCR_t;

#if 0
#define TCR_BYTE_COUNT(x)       ((x+3)/4)
#else
#define TCR_BYTE_COUNT(x)       (x)
#endif

#define JPEG_END_DMA_TCR        0x80000000
#define JPEG_INT_END_DMA_TCR    0x40000000


/*
 * Descriptor for a dma buffer that will allow us to locate
 * it at any time.   These dma buffers live in the user
 * process's address space, but they are mapped permanently
 * into kernel memory at the start of dma.
 */
struct jpeg_buf_desc {
        struct  jpeg_buf_desc   *next_desc;
        char    *bufVP;			/* user virtual address of buffer */
        char    *bufKP;			/* kernel virtual address of buffer */
        int     length;
        int     *buf_pfns;
        int     buf_npages;
        int     buf_offset;
        int     lock_err;
        int     dmadap_len;
        struct jpeg_dma_info   *infoPP;
};

/*
 * The following state flags are used in node dma block
 * to track the dma states for CIC and UCIC
 */

#define	DMA_INACTIVE	0x00000000 /* dma channel is ready to use */
#define	DMA_PENDING	0x00000001 /* setup for a "chunk" and ready start  */
#define DMA_ACTIVE	0x00000002 /* dma channel running, dma intr pending */
#define	DMA_COMPLETE	0x00000004 /* rcvd a dma interrupt, need to tear down */
#define DMA_TERMINATE	0x00000008 /* All channel data has been dma-ed */
#define	DMA_ERROR	0xFFFFFFFF /* Something bad happend */


/*
 * descriptor for dma activity.
 */
typedef struct jpeg_dma_block {
        long    dma_flag;		
        long    dma_state;              /* used in dma state machine */
	long 	direction;		/* direction:   B_WRITE= 0 = GIO->JPEG,
                                                        B_READ = 1 = JPEG->GIO
                                                        NULL   = -1      */
	enum	DMA_CH	dma_channel;	/* {NO_DMA_CH = 0,  CC_CH, UCC_CH } */
	enum 	DMA_Mode   dma_mode;	/* {NO_DMA_MODE = 0, SINGLE_BUFF, CIRCULAR_BUFF} */ 

	RingBuffer	*Qctrl;		/* kernel address of Qctrl_user */
	RingBuffer	*Qctrl_user;	/* control circular buffer */
	RingBuffer	*Qdata;		/* kernel address of Qdata_user */
	RingBuffer	*Qdata_user;	/* data buffer */

        char	*bufp;                  /* user virtual buffer address */
        int     length;                 /* user buffer length */
	int	nframes;		/* number of frames in buffer */
        int     page_offset;            /* byte offset of first page in buffer */

	/* "chunk" dma parameters used to remember dma states between bursts */
        char	*ChunkBufp;		/* user chunk virtual buffer address */
        int     ChunkLength;		/* user chunk buffer length */
        int     ChunkLengthLeft;	/* chunk length left to dma */
        char	*PIOChunkBufp;		/* user chunk virtual buffer address */
        int     PIOChunkLength;		/* user chunk buffer length */
	int	ChunkNframes;		/* number of frames in the chunk buffer */
        int     ChunkPage_offset;	/* chunk byte offset of first page in buffer */
        int	ChunkDdap_pages;	/* chunk page count for dma desc tbl */
        TCR_t	*ChunkDma_dap;		/* pointer to chunk dma desc tbl */
#ifdef UCCDMAHACK
        TCR_t	*next_dap;		/* next dap to use */
#endif /* UCCDMAHACK */
	int	*ChunkPfns;		/* pointer to array of page frame numbers for the chunk */
	int	special_length;		/* non multiple of eight length */
	char 	*special_bufp;		/* special buffer */
        int     FBChunkLength;          /* FB dma length for UIC READ */
        int     FBChunkLengthLeft;      /* FB dma length left to UIC READ */
	/* runtime activity flags */
        int     circularframe;          /* which buffer in a circular list */
	unsigned src_total_bytes_dma;	/* total dma for source */
	unsigned dest_total_bytes_field;	/* total dma for source */
	unsigned dest_total_bytes_dma;	/* total dma for destination */
        struct jpeg_buf_desc *buf_descF;     /* first buf desc pointer */
        struct jpeg_buf_desc *buf_descP;     /* current buf desc pointer */

}DmaBlock;

/* One of these per node. 
 * JPEG board has three bidirectional ports; GIO , D1 and video I/O.
 * We setup a source and a drain JNode for each of the following 
 * data path:
 *              GIO  - > D1     Decompression
 *              D1   - > GIO    Compression
 *              GIO  - > GIO    Decompression/Compression
 *		video- > GIO	Compression
 *		GIO  - > video  Decompression
 *
 * NOTE: Currently, we support one data path for a particular
 *       open(). Latter, to support a monitor path for 
 *	 the last case, i.e.,
 *
 *		video-|-> GIO	Compression
 *                    |-> GIO   Uncompress monitor channel
 * 
 *      
 */

#define MODE_30	30
#define MODE_60	60

typedef struct {
	enum	CLNode_name name;	/* node name {NullNode = 0, 
						DRV_MEM, DRV_VIDEO, 
						DRV_ANOLOG_VIDEO}; */
	unsigned fbstate;		/* field buffer state for this node */
	unsigned fbcurrent;		/* field buffer currently in use */
	unsigned fbnext;		/* next field buffer needed */
	int	decim_sf;		/* decimation scale factor */
	int	field_count;		/* total number of fields 
					   processed through this node*/
	int	sync_field_count;	/* total number of fields 
					   processed after the last sysnc
					   point through this node*/
	int	vid_std;		/* ntsc or pal timing */
	int	pix_format;		/* pixel format RGB, YUV, etc */
	int	pix_size;		/* pixel size in bytes for pix_format */
	int	alpha;			/* alpha value */
	DmaBlock	*dma_block;     /* pointer to dma block for this channel */
} JNode;


#define JPEG_MAX_FEQ_COUNT 80
#define JPEG_MAX_LOCKED_PAGES 64
struct _jpeg_lock_tbl {
	unsigned	pagebase;
	unsigned ucount;
};

typedef struct jpath{
	unsigned	path_state;
	void	*path_openp;
	int	path_timer_id;
	JNode		*src_node;
	JNode		*dest_node;
	JPEG_MODE	*j_mode;
	unsigned	j_qtselected;
	short		*j_qtable;
	unsigned	src_field_count;
	unsigned	src_field_byte_total;
	unsigned	dest_field_count;
	unsigned	dest_field_byte_total;
	unsigned	scale_timer;
	unsigned	scale_counter;
	unsigned	feq_iindx;
	unsigned	feq_oindx;
	unsigned	feq_count;
	unsigned	feq[JPEG_MAX_FEQ_COUNT];
	unsigned pages_locked_count;
	struct	_jpeg_lock_tbl	locked_pages[JPEG_MAX_LOCKED_PAGES];
} JPATH;
#define	decmp_eof_count	src_field_count
#define	decmp_eof_total	src_field_byte_total
#define	cmp_frame_count	dest_field_count
#define	cmp_frame_total	dest_field_byte_total
#define	ucc_frame_count	src_field_count
#define	ucc_frame_total	src_field_byte_total
#define	uic_frame_count	dest_field_count
#define	uic_frame_total	dest_field_byte_total


/* values for j_qtselected */
#define QTAB_A	0
#define QTAB_B	1
/* 
 * SETUP_DMA called, no dma started yet for SRC/DEST, START_DMA may have
 * been called
 */
#define PATH_SRC_DMA_NOT_STARTED	(1<<0)
#define PATH_DEST_DMA_NOT_STARTED	(1<<1)
/* 
 * Set once the first dma is started on SRC/DEST, cleared when START_DMA
 * is complete and ready to exit.
 */
#define PATH_SRC_DMA_NOT_COMPLETE	(1<<2)
#define PATH_DEST_DMA_NOT_COMPLETE	(1<<3)
/*
 * Set at SETUP_DMA time, cleared after the initialization is done (before
 * any dma can be started),  Can be set later at other times when sharing
 * the board.
 */
#define PATH_SRC_NOT_INITIALIZED	(1<<4)
#define PATH_DEST_NOT_INITIALIZED	(1<<5)
/*
 * set at open time, path structure creation, cleared at SETUP_DMA time.
 */
#define PATH_SRC_NOT_SETUP		(1<<6)
#define PATH_DEST_NOT_SETUP		(1<<7)
/*
 * set when a close of the path is initiated, causes all path activity
 * to (eventually) stop.  Should be checked at all 'completion' spots
 * in the driver.
 */
#define PATH_IS_CLOSING			(1<<8)
/*
 * set when field buffers have been initialized
 */
#define PATH_FIELD_BUFFERS_INITIALIZED	(1<<9)
/*
 * set when dma cannot be started until 560 FIFO is primed.
 */
#define PATH_PRIME_NEEDED		(1<<10)
/*
 * indicates CC qctrl is in use, ring buffer exists for frame status 
 */
#define PATH_IS_USING_Q_CTRL		(1<<11)
/*
 * inidcates that any one of the states for this pat h has changed
 * by some event.
 */
#define PATH_STATE_HAS_CHANGED		(1<<12)
#define PATH_ABORTED			(1<<13)

#ifdef DEBUG
#define PATH_IS_SLEEPING		(1<<30)
#define IS_PATH_SLEEPING(jp)		(jp->path_state & PATH_IS_SLEEPING)
#define SET_PATH_SLEEPING(jp)		(jp->path_state |= PATH_IS_SLEEPING)
#define CLEAR_PATH_SLEEPING(jp)		(jp->path_state &= ~PATH_IS_SLEEPING)
#endif /* DMADEBUG */
/*
 * set for each field buffer at intialization, then as the buffers
 * are chased arount each other these change
 */

#define PATH_DMA_NOT_COMPLETE(jp)	(jp->path_state& \
					  (PATH_SRC_DMA_NOT_STARTED| \
					   PATH_DEST_DMA_NOT_STARTED| \
					   PATH_SRC_DMA_NOT_COMPLETE| \
					   PATH_DEST_DMA_NOT_COMPLETE))
#define SET_PATH_SRC_DMA_COMPLETE(jp)	(jp->path_state &= \
					  ~(PATH_SRC_DMA_NOT_STARTED| \
					   PATH_SRC_DMA_NOT_COMPLETE))
#define SET_PATH_SRC_STARTED(jp)	(jp->path_state &= \
					  ~PATH_SRC_DMA_NOT_STARTED)
#define SET_PATH_DEST_STARTED(jp)	(jp->path_state &= \
					  ~PATH_DEST_DMA_NOT_STARTED)
#define SET_PATH_DEST_DMA_COMPLETE(jp)	(jp->path_state &= \
					  ~(PATH_DEST_DMA_NOT_STARTED| \
					   PATH_DEST_DMA_NOT_COMPLETE))
#define PATH_DMA_NOT_ACTIVE(jp)		(((jp->path_state & \
					  (PATH_SRC_DMA_NOT_STARTED| \
					   PATH_DEST_DMA_NOT_STARTED)) == \
					  (PATH_SRC_DMA_NOT_STARTED| \
					   PATH_DEST_DMA_NOT_STARTED)) \
						|| \
					 (((jp->path_state & \
					  (PATH_SRC_DMA_NOT_STARTED| \
					   PATH_SRC_DMA_NOT_COMPLETE)) == \
					  (PATH_SRC_DMA_NOT_STARTED| \
					   PATH_SRC_DMA_NOT_COMPLETE)) || \
					 ((jp->path_state & \
					  (PATH_DEST_DMA_NOT_STARTED| \
					   PATH_DEST_DMA_NOT_COMPLETE)) == \
					  (PATH_DEST_DMA_NOT_STARTED| \
					   PATH_DEST_DMA_NOT_COMPLETE))))
#define PATH_NOT_INITIALIZED(jp)	(jp->path_state& \
					 (PATH_SRC_NOT_INITIALIZED| \
					 PATH_DEST_NOT_INITIALIZED))
#define PATH_NEEDS_PRIMING(jp)		(jp->path_state& \
					 PATH_PRIME_NEEDED)
#define SET_PATH_NEEDS_PRIMING(jp)	(jp->path_state |= \
					 PATH_PRIME_NEEDED)
#define CLEAR_PATH_NEEDS_PRIMING(jp)	(jp->path_state &= \
					 ~PATH_PRIME_NEEDED)
#define PATH_IS_ABORTED(jp)		(jp->path_state& \
					 PATH_ABORTED)
#define SET_PATH_ABORTED(jp)		(jp->path_state |= \
					 PATH_ABORTED)
#define HAS_PATH_STATE_CHANGED(jp)	(jp->path_state& \
					 PATH_STATE_HAS_CHANGED)
#define SET_PATH_STATE_HAS_CHANGED(jp)	(jp->path_state |= \
					 PATH_STATE_HAS_CHANGED)
#define CLEAR_PATH_STATE_HAS_CHANGED(jp)	(jp->path_state &= \
					 ~PATH_STATE_HAS_CHANGED)
#define PATH_USING_Q_CTRL(jp)		(jp->path_state& \
					 PATH_IS_USING_Q_CTRL)
#define SET_PATH_USING_Q_CTRL(jp)	(jp->path_state |= \
					 PATH_IS_USING_Q_CTRL)
#define PATH_FB_INITIALIZED(jp)		(jp->path_state& \
					 PATH_FIELD_BUFFERS_INITIALIZED)
#define SET_PATH_INITIALIZED(jp)	(jp->path_state &= \
					 ~(PATH_SRC_NOT_INITIALIZED| \
					 PATH_DEST_NOT_INITIALIZED))
#define SET_PATH_NOT_INITIALIZED(jp)	(jp->path_state |= \
					 (PATH_SRC_NOT_INITIALIZED| \
					 PATH_DEST_NOT_INITIALIZED))
#define SET_PATH_SRC_SETUP_ONLY(jp)	jp->path_state &= \
						~PATH_SRC_NOT_SETUP; \
					 jp->path_state |= \
						 PATH_SRC_DMA_NOT_STARTED
#define SET_PATH_DEST_SETUP_ONLY(jp)	jp->path_state &= \
						~PATH_DEST_NOT_SETUP; \
					 jp->path_state |= \
						 PATH_DEST_DMA_NOT_STARTED
#define SET_PATH_SRC_NOT_STARTED(jp)	jp->path_state |= \
						 PATH_SRC_DMA_NOT_STARTED
#define SET_PATH_DEST_NOT_STARTED(jp)	jp->path_state |= \
						 PATH_DEST_DMA_NOT_STARTED

#define	FB_0	0
#define	FB_1	1
#define	FB_2	2
#define	FB_3	3
#define NO_FB_YET 0xffffffff

/*	M Y   N O T E S  
 ***************************************
 *
 * Field buffer state machine operation:
 * 	'state' notation:
 *		1- EMPTY ::= field buffer does not have valid data
 *		2- WANT  ::= pined ::= UCC dma can began to read or write
 *				the field buffer
 *
 ***************************************
 * GIO -> GIO DeCompression State Sequence:
 *
 *	DEST node 'fbstate' manages the buffers
 *	560 fills a field buffer (or fb pair)
 *	VFC DECOMP changes states from 
 *
 *				EMPTY & UNWANT  ->  FULL & UNWANT
 *
 *	DMAChannelReady() snoops the field buffer states, next_Rfb_avail() 
 *	 'pins' a buffer pair for DMA operation and sets the WANT flag after 
 *	 560 has filled a pair of buffers. This triggers DMA READ (write to memory).
 *
 *				FULL & UNWANT -> FULL & WANT
 *	 
 *	UCC DMA completion changes field buffer states (in check_path_dma)
 *
 *				FULL & WANT  ->  EMPTY & UNWANT
 *
 *	Field buffer pairs are switched only and only if:
 *		one pair is FULL & WANT 'AND'
 *		the other pair is EMPTY & UNWANT
 *
 *	Inital State:
 *		Filed buffers EMPTY and UNWANT
 *		fbstart = FB_0   560 field buffer
 *		fbnext  = FB_2   DMA field buffer 
 *
 * TODO: I think it would be better to change the 
 *	 fbstart and fbnext to fb560 and fbdma to better 
 *	 associate field buffer with their SRC and DEST owners, i.e., 
 *	
 *		DeCompression:
 *			fb560 = FB_0 ::= 560 is WRITING the field buffers 0 and 1
 *			fbdma = FB_2 ::= DMA is READING the field buffers 2 and 3
 *			
 *		Compression:
 *			fb560 = FB_0 ::= 560 is READING the field buffers 0 and 1
 *			fbdma = FB_2 ::= DMA is WRITING the field buffers 2 and 3
 *
 **************************************
 * GIO -> GIO Compression State Sequence:
 * 
 *	SRC node 'fbstate' manages the buffers
 *	Inital State:
 *		Filed buffers EMPTY and UNWANT (E.U)
 *		fbstart = FB_0   DMA field buffer set to E.U
 *		fbnext  = FB_2   560 field buffer set to E.U 
 *
 *	DMAChannelReady() changes states:
 *
 *				 [E.U] to [E.W] 
 *				 [E.U]    [E.U]
 *
 *	It then fills the field buffer with *only* ONE frame of UnCompressed
 *	 date
 *	UIC DMA completion interrupt changes field buffer state from 
 *
 *				[E.W] -> [F.U]
 *
 *		NOTE:   TCR chain must dma exactly one field into field
 *			buffer. The DMA completion interrupt triggers 560
 *			compression.
 *
 *	next_Wfb_avail() switches field buffers when:
 *
 *				[F.U & E.U] OR [E.U & F.U] 
 *
 *	So, fb read pointer is switched.
 *	560 starts compressing the [F.U] buffer
 *	560 EOF interrupt changes states:
 *
 *				[F.U ] -> [E.U ]
 *
 *	And, this takes us back to a privious stats.
 *
			*/

#define NODE_FB_0_FULL			(1<<0) 
#define NODE_FB_1_FULL			(1<<1)
#define NODE_FB_2_FULL			(1<<2)
#define NODE_FB_3_FULL			(1<<3)

#define NODE_FB_0_WANT			(1<<4)
#define NODE_FB_1_WANT			(1<<5)
#define NODE_FB_2_WANT			(1<<6)
#define NODE_FB_3_WANT			(1<<7)

#define NODE_FB_0_USE			(1<<8)
#define NODE_FB_1_USE			(1<<9)
#define NODE_FB_2_USE			(1<<10)
#define NODE_FB_3_USE			(1<<11)

#define FB_0_FULL(np)		np->fbstate |= NODE_FB_0_FULL
#define FB_0_EMPTY(np)	np->fbstate &= ~NODE_FB_0_FULL
#define FB_1_FULL(np)		np->fbstate |= NODE_FB_1_FULL
#define FB_1_EMPTY(np)	np->fbstate &= ~NODE_FB_1_FULL
#define FB_2_FULL(np)		np->fbstate |= NODE_FB_2_FULL
#define FB_2_EMPTY(np)	np->fbstate &= ~NODE_FB_2_FULL
#define FB_3_FULL(np)		np->fbstate |= NODE_FB_3_FULL
#define FB_3_EMPTY(np)	np->fbstate &= ~NODE_FB_3_FULL


#define FB_01_FULL(np)		np->fbstate |= \
					(NODE_FB_0_FULL|NODE_FB_1_FULL)
#define FB_01_EMPTY(np)	np->fbstate &= \
					~(NODE_FB_0_FULL|NODE_FB_1_FULL)
#define FB_23_FULL(np)		np->fbstate |= \
					(NODE_FB_2_FULL|NODE_FB_3_FULL)
#define FB_23_EMPTY(np)	np->fbstate &= \
					~(NODE_FB_2_FULL|NODE_FB_3_FULL)
#define FB_0123_FULL(np)		np->fbstate |= \
					(NODE_FB_2_FULL|NODE_FB_3_FULL| \
					 NODE_FB_0_FULL|NODE_FB_1_FULL)
#define FB_0123_EMPTY(np)	np->fbstate &= \
					~(NODE_FB_2_FULL|NODE_FB_3_FULL| \
					  NODE_FB_0_FULL|NODE_FB_1_FULL)

#define FB_0_WANT(np)		np->fbstate |= NODE_FB_0_WANT
#define FB_0_UNWANT(np)	np->fbstate &= ~NODE_FB_0_WANT
#define FB_1_WANT(np)		np->fbstate |= NODE_FB_1_WANT
#define FB_1_UNWANT(np)	np->fbstate &= ~NODE_FB_1_WANT
#define FB_2_WANT(np)		np->fbstate |= NODE_FB_2_WANT
#define FB_2_UNWANT(np)	np->fbstate &= ~NODE_FB_2_WANT
#define FB_3_WANT(np)		np->fbstate |= NODE_FB_3_WANT
#define FB_3_UNWANT(np)	np->fbstate &= ~NODE_FB_3_WANT
#define FB_01_WANT(np)		np->fbstate |= \
					(NODE_FB_0_WANT|NODE_FB_1_WANT)
#define FB_01_UNWANT(np)	np->fbstate &= \
					~(NODE_FB_0_WANT|NODE_FB_1_WANT)
#define FB_23_WANT(np)		np->fbstate |= \
					(NODE_FB_2_WANT|NODE_FB_3_WANT)
#define FB_23_UNWANT(np)	np->fbstate &= \
					~(NODE_FB_2_WANT|NODE_FB_3_WANT)
#define FB_0123_WANT(np)		np->fbstate |= \
					(NODE_FB_2_WANT|NODE_FB_3_WANT| \
					 NODE_FB_0_WANT|NODE_FB_1_WANT)
#define FB_0123_UNWANT(np)	np->fbstate &= \
					~(NODE_FB_2_WANT|NODE_FB_3_WANT| \
					  NODE_FB_0_WANT|NODE_FB_1_WANT)


#define IS_FB_12_WANT(np)	(np->fbstate & (NODE_FB_0_WANT|NODE_FB_1_WANT) )
#define IS_FB_23_WANT(np)	(np->fbstate & (NODE_FB_2_WANT|NODE_FB_3_WANT) )


#define FB_01_USE(np)		(np->fbstate |= (NODE_FB_0_USE|NODE_FB_1_USE))
#define FB_23_USE(np)		(np->fbstate |= (NODE_FB_2_USE|NODE_FB_3_USE))
#define FB_01_UNUSE(np)		(np->fbstate &= ~(NODE_FB_0_USE|NODE_FB_1_USE))
#define FB_23_UNUSE(np)		(np->fbstate &= ~(NODE_FB_2_USE|NODE_FB_3_USE))

#define FBNEXT_FULL(np)	(np->fbstate & (1<<np->fbnext))


struct reg_set {
        int reg;
        int val;
};

#define MODE_PIXEL(x)		(x->CL560_mode)	
#define MODE_DECOMPRESS(x)	(x->CL560Direction)	

/*
 * There are a few write only registers in this master piece design.
 * Keep a shadow of these registers
 */
struct shadow_ptr_t {
        int     addr;
        char *shad_regs;
};
#define SHADOW_SIZE                     32              /* size of array */

struct JPEG_desc  {
                unsigned        addr;
                unsigned        state;
                char   shadowI2C[SHADOW_SIZE];
                struct shadow_ptr_t shadow_ptr[1];
        };
typedef struct JPEG_desc  jpeg_ptr;



#define	MARK_BUSY	0x01
#define	MARK_FULL	0x02
/*
 * y :: buffer flag
 * x :: field buffer number, 1,2,3 or 4 
 */ 
#define	BUSY_BUFF(x, y)		(y |= (MARK_BUSY << x*8) )
#define	FREE_BUFF(x, y)		(y &= (~MARK_BUSY << x*8) )

#define	BUFF_FULL(x, y)		(y |= (MARK_FULL << x*8) )
#define	BUFF_EMPLTY(x, y)	(y &= (~MARK_FULL << x*8) )

#define NOT_PENDING	0
#define PENDING		(~NOT_PENDING)

/*
 * JPEG device structure. One of these for each board GIO-0 and GIO-1
 */
static struct jpeg_board { 
	JPEG_Regs	*jpeg_regs; 		/* registers of this board */
	JInfo		*jpeg_info;		/* board slot/rev info */	
	struct jpeg_open *jpeg_owner;
	int		 jpeg_number;  		/* board number, index */
	struct jpeg_fncs *jpeg_fncs;
	char		*jpeg_local;		/* dev dependant bd data */

	unsigned 	jpeg_ddbusy;		/* dev dependent busy flag */

	/* dma control */
	unsigned 	jpeg_piobusy;		/* pio interrupt wanted */
	unsigned 	jpeg_intrcomm;		/* intr communications word */
	unsigned 	jpeg_560_irq1;		/* 560 IRQ1 interrupt mask */
	unsigned 	jpeg_560_irq2;		/* 560 IRQ2 interrupt mask */
	unsigned 	jpeg_560_drq;		/* 560 DMA  (DRQ\) interrupt mask */
	unsigned 	jpeg_dma_int;		/* dma chip interrupt mask */
	unsigned 	jpeg_vfc_intr;		/* VFC chip interrupt */

	/* 560 control */ 
	unsigned 	jpeg_doreinit;		/* must do re-init of regs */
	unsigned 	jpeg_end_frame;		/* end of frame was reached */
	unsigned 	jpeg_560_started;	/* 560 has been started */
	unsigned 	jpeg_error;		/* error has occured, contains code */

	/* frame control */
	unsigned 	jpeg_rep_pending; 	/* post a 'repeat' to this board */
	unsigned 	jpeg_adv_pending; 	/* post a 'avd' for this board */
	unsigned 	jpeg_skip_pending; 	/* post a 'skip' to this board  */
	unsigned 	jpeg_last_frame; 	/* processing last frame, flush the board */
	unsigned 	jpeg_field_buf_stat; 	/* filed buffer status, see macro above */
	unsigned	jpeg_app_fields;
	unsigned 	jpeg_D1_fields;		/* valid D1 EOF's */
#ifdef SINGLEWIDE
        unsigned        jpeg_D1_EVEN_fields;    /* valid D1 even EOF's */
        unsigned        jpeg_D1_ODD_fields;     /* valid D1 odd EOF's */
#endif /* SINGLEWIDE */
	unsigned 	jpeg_D1_direction;	/* D1 dir (B_READ/BWRITE) */
	unsigned	jpeg_last_rfb;		/* last known read fb */
	unsigned	jpeg_last_wfb;		/* last known write fb */


	/* Interrupt statistics */
	unsigned	jpeg_CC_intr_cnt;	/* CC intr count */
	unsigned	jpeg_UCC_intr_cnt;	/* UCC intr count */
	unsigned	jpeg_FUR_intr_cnt;	/* UnUsed  */
	unsigned	jpeg_FXI_intr_cnt;	/* ALL VFC intr count */
	unsigned	jpeg_IRQ1_intr_cnt;	/* IRQ1 intr count */
	unsigned	jpeg_IRQ2_intr_cnt;	/* IRQ2 intr count */
	unsigned	jpeg_FRMEND_intr_cnt;	/* 560 FE from dma intr count */
	unsigned	jpeg_FRMEND_flag_cnt;	/* 560 FE found in flag count */
	unsigned	jpeg_VFC_intr_cnt;	/* VFC intr count */
						/* same as */
						/* jpeg_FXI_intr_cnt? */
#ifndef SINGLEWIDE
        unsigned        jpeg_D1_EOF_intr_cnt;   /* D1 end-of-field  */
#else /* SINGLEWIDE */
        unsigned        jpeg_D1_EVENEOF_intr_cnt; /* D1 evenend-of-field  */
        unsigned        jpeg_D1_ODDEOF_intr_cnt; /* D1 odd end-of-field  */
#endif /* SINGLEWIDE */
	unsigned	jpeg_READ_ADV_intr_cnt;	/* read advance  */
	unsigned	jpeg_WRITE_ADV_intr_cnt;/* write advance */
	unsigned	jpeg_DECMP_EOF_intr_cnt;/* Decomp end-of-field  */
	unsigned	jpeg_CC_wakeup_cnt;	/* CC wakeup when sleeping */
	unsigned	jpeg_UCC_wakeup_cnt;	/* UCC wakeup when sleeping */
	unsigned	jpeg_VFC_CC_wakeup_cnt;	/* CC wakeup when sleeping */
	unsigned	jpeg_VFC_UCC_wakeup_cnt;/* UCC wakeup when sleeping */

	JPATH	*cc_path;	/* active CC path pointer */
	JPATH	*ucc_path;	/* active UCC path pointer */
	JPATH	*to_path;	/* timeout path */
};
typedef struct jpeg_board  JBoard;


/*
 * jpeg driver structure. One per each open. The plan is to support
 * multiple opens to the JPEG board.
 */
enum jpeg_minor_dir {
	jpeg_dir_comp = 0,
	jpeg_dir_decomp = 1 };
	

enum jpeg_minor_mode {
	jpeg_mode_unspecified = 0,
	jpeg_mode_in = 1,
	jpeg_mode_out = 2,
	jpeg_mode_loop = 3 };


#if NEVER
typedef struct jpeg_open {
	JBoard			*jpeg_board;	/* pointer to board for this open*/
				/* dev independant per open private data */
	char			*vx_bdata;

	struct proc		*jpeg_pp;     /* proc pointer */
	struct jpeg_open	*jpeg_next;   /* ptr to next open jpeg */

	/*  board driver stuff	*/
	struct jpeg_fncs	*jpeg_fncs;
	struct pregion		*jpeg_preg;
	unsigned int	jpeg_xtra_cnt;	/* xtra bytes from last write */
	unsigned char	jpeg_xtra_data[4];	/* xtra bytes from last write */

	sema_t vx_sema;			/* semaphore for multiple sg procs */

	enum jpeg_minor_dir	jpeg_default_direction;
	enum jpeg_minor_dir	jpeg_current_direction;
	enum jpeg_minor_mode	jpeg_default_mode;
	enum jpeg_minor_mode	jpeg_current_mode;
	char			jpeg_current_set;
	/* we support one path through the board for a particulare open */
	JPATH	*j_path;

	/* open status */
	unsigned 		*OpenDrvStatus;
}JPEGOpen;

typedef struct jpeg_fncs {
	int (*jpeg_Info)	(JInfo *, void *, unsigned int); /* check param */   
	int (*jpeg_Attach)	(JPEGOpen *, unsigned long int, dev_t); 
	int (*jpeg_Detach)	(JPEGOpen *, unsigned long int, dev_t);
	int (*jpeg_Initialize)	(JPEGOpen *);
	int (*jpeg_Start)	(JPEGOpen *);
	int (*jpeg_SetParams)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_GetParams)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_LoadQTable)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_GetQTable)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_LoadHTable)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_GetHTable)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_GetStatus)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_Dma)		(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_SetupDma)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_GetFeature)	(JPEGOpen *, caddr_t, int *, dev_t);
	int (*jpeg_Mmap)	(int, vhandl_t *, int, int, int);
	int (*jpeg_UnMmap)	(int, vhandl_t *);
	int (*jpeg_Poll)	(int);
	int (*jpeg_pass)	(caddr_t, int *);
}JFuncs;
#endif

#endif /* __JPEG_STRUCT_H_ */


