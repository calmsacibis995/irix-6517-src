#ifndef __SYS_CRIME_GFX_H__
#define __SYS_CRIME_GFX_H__

/*
 * sys/crime_gfx.h
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.1 $"

/*
 * Max number of crime boards in a system
 */
#define MAXCRIME 1
#define CRM_TEX_MAX_RESIDENT	128

typedef unsigned short	tilenum_t;	/* physical tile address >> 16 */

#define GFX_NAME_CRIME	"CRM"	/* Moosehead/Crime graphics board */

typedef struct {
    	int flags;
        short fields_sec;	/* fields/sec */
} crime_vof_info_t;

/*
 * Definitions for crime_vof_info_t
 * and crime_timing_info flags
 */

#define CRIME_VOF_UNKNOWNMON 1
#define CRIME_VOF_STEREO     2
#define CRIME_VOF_DO_GENSYNC 4          /* enable incoming sync */

struct crime_info {
			/* device independent information */
	struct gfx_info gfx_info; 
			/* crime specific information */
	unsigned char boardrev;
	unsigned char crimerev;
	unsigned char gberev;
	unsigned char monitortype;
	unsigned char bitplanes;	/* 8, 16, 32 */
	crime_vof_info_t crime_vof_info;
};

#define CRIME_SYNC_VALUE 0

#define CRIME_N_FB_TILES 	80

/*
 * CRIME_VALIDATECLIP ioctl.
 * Updates clipping information in the ddrnp/shadow area
 * for software double-buffered windows.
 */

typedef struct _crimeBox {
	short x1, y1, x2, y2;
} crimeBox_t;

typedef struct _crime_ValidateClip {
	int             rnid;               /* rn ID, not for MGR Swapbuf */
	int             wid;                /* for MGR Swapbuf */
	unsigned char	numtiles;	    /* Number of tiles */
	unsigned int	numboxes;	    /* Number of boxes. */
	unsigned int	num_alloc_boxes;    /* Number of allocated boxes. */
	unsigned char	tiles[256];    	    /* Ptr to array of tile indices
					     * (CRIME_N_FB_TILES max.)
					     */
	crimeBox_t 	*boxes;		    /* Ptr to array of boxes */
} crime_ValidateClip_t;

typedef struct _crime_UpdateBackTiles {
	unsigned char numadds; 
	unsigned char numdels; 
	unsigned char tiles[256];
} crime_UpdateBackTiles_t;

typedef struct _crime_UnSchedMGRSwapBuf {
	int wid;
} crime_UnSchedMGRSwapBuf_t;

/*
 *   crime private ioctls
 */
#define CRIME_BASE			22000
#define CRIME_SET_CURSOR_HOTSPOT	(CRIME_BASE+1)
#define CRIME_PIXELDMA			(CRIME_BASE+2)
#define CRIME_MAP_SHADOW		(CRIME_BASE+3)
#define CRIME_SETDISPLAYMODE            (CRIME_BASE+4)
#define CRIME_SETGAMMARAMP		(CRIME_BASE+5)
#define CRIME_SETVIDEOTIMING		(CRIME_BASE+6)
/* 7,8,9 available */
#define CRIME_GET_TILES			(CRIME_BASE+10)
#define CRIME_UNGET_TILES		(CRIME_BASE+11)
#define CRIME_MAP_GBEMEM		(CRIME_BASE+12)
#define CRIME_GET_GBE_RESOURCE		(CRIME_BASE+13)
#define CRIME_OPEN_PBUF			(CRIME_BASE+14)
#define CRIME_VALIDATECLIP		(CRIME_BASE+15)
#define CRIME_CLOSE_PBUF		(CRIME_BASE+16)
#define CRIME_TEX_ALLOC			(CRIME_BASE+17)
#define CRIME_TEX_DEALLOC		(CRIME_BASE+18)
#define CRIME_TEX_DEFINE		(CRIME_BASE+19)
#define CRIME_SWAPBUF			(CRIME_BASE+20)
#define CRIME_LOAD_LINEAR_TLB		(CRIME_BASE+21)
#define CRIME_CLEAR_BUFFER		(CRIME_BASE+22)
#define CRIME_LOAD_FB_TLB		(CRIME_BASE+23)
#define CRIME_UPDATE_BACK_TILES		(CRIME_BASE+24)
#define CRIME_MAKECURRENT		(CRIME_BASE+25)
#define CRIME_MGR_UNSCHEDSWAPBUF	(CRIME_BASE+26)

#ifdef CRMSIM
#define CRIME_FAKE_RETR          	(CRIME_BASE+94)	
#define CRIME_GETLOG_TILES          	(CRIME_BASE+95)	
#define CRIME_LOAD_TEXTLB          	(CRIME_BASE+96)	
#define CRIME_GETLOG          		(CRIME_BASE+97)	
#define CRIME_READREG          		(CRIME_BASE+98)
#define CRIME_WRITEREG          	(CRIME_BASE+99)

struct crime_getlog_tiles {
	unsigned int what;	/* chunk to get tiles of */
	unsigned int flags;	/* map or not */
	void *addr;		/* user space addr they were mapped to */
	int ntiles;		/* how many tiles returned below */
	tilenum_t tiles[256];	/* returned phys addr of tiles */
};

/*
 * Struct describing the buffer transferred
 * by the CRIME_GETLOG ioctl.  'addr' points to
 * the user buffer the kernel data is copied to.
 * 'size' is the size in words of the user buffer,
 * and on return is the number of words the kernel
 * copied to the buffer. 'minsize' is the minimum
 * number of words the user wants per transfer; if
 * this amount is not available, sleep.  If 'minsize'
 * is 0, then whatever amount of data is available
 * (if any) is returned immediately.
 */

struct crime_getlog {
	unsigned int *addr;
	int size;	/* user bufsize in, amount returned out */
	int minsize;	/* sleep if we don't have this much buffered yet */
};

/*
 * The returned data is an array of variable size
 * records.  The first word in each record describes
 * the size and content of the data that follows.
	struct crime_getlog_desc {
		char flags;
		char opcode;
		short operand;
	};
 * This is followed by 0 or more words of data.
 */
	/* opcode
	 *	write to crime register
	 *	read from ""
	 *	begin polling status register
	 *	done polling
	 *	message from the driver
	 *	linear buffer info follows
	 */
#define CRMLOG_WRITE		0
#define CRMLOG_READ		1
#define CRMLOG_BEGINPOLL	2
#define CRMLOG_ENDPOLL		3
#define CRMLOG_LINBUF		4
#define CRMLOG_CMD_MASK		0xff

	/*
	 * flags :
	 *	64 bit read/write
	 *	kernel mode read/write (context switch etc)
	 *	cmodel reported error
	 */

#define CRMLOG_64	(1 << 8)
#define CRMLOG_KERN	(2 << 8)
#define CRMLOG_ERROR	(4 << 8)


/*
 * Structure used by CRIME_READREG and CRIME_WRITEREG ioctls
 */
struct crime_writereg {
	int size, addr, data[2];
};

#endif	/* CRMSIM */


struct crime_get_gbe_resource {
	int type;
	int rval;
};
/*
 * Valid types for CRIME_GET_GBE_RESOURCE ioctl
 */
#define GBE_DID_RESOURCE 1


struct crime_set_cursor_hotspot {
	unsigned short xhot;
	unsigned short yhot;
};

/*
 * CRIME_OPEN_PBUF
 * Issued by Xsgi, to inform driver that
 * a given clipid refers to a pbuffer.
 */

struct crime_open_pbuf {
	unsigned int clipid;
	int xsize, ysize;	/* pixels */
	int depth;		/* bytes */
	int flags;
};

/*
 * CRIME_CLOSE_PBUF ioctl.
 * Free all resources associated with a pbuffer.
 * Usable by Xsgi only.
 */

struct crime_close_pbuf {
	unsigned int clipid;	/* the drawable id */
	int flags;
};


/*
 * CRIME_MAKECURRENT ioctl.
 * Process must RRM_OPENRN,
 * RRM_BINDPROCTORN and RRM_BINDRNTOCLIP before 
 * issuing this ioctl.  The purpose is to inform the
 * driver of the type of the read and draw buffers,
 * so it can allocate tiles or whatever, before
 * the buffers are used.
 */
struct crime_makecurrent {	
	int drawtype;
	int readtype;
	int flags;	/* 1 = enable z buffer */
};

/* crime_makecurrent types */
#define CRM_GL_WINDOW             1
#define CRM_GL_PIXMAP             2
#define CRM_GL_VIDEO_SOURCE       3
#define CRM_GL_PBUFFER            4

/* crime_makecurrent flags */
#define CRM_DRAW_HAS_Z		1
#define CRM_READ_HAS_Z		2


struct crime_pixeldma_args {
        char    *buf;                   /* low byte address of buffer */
        int     stride;                 /* bytes from buf(y,x) to buf(y+1,x) */
        int     width;                  /* bytes transferred per row of buf */
        int     height;                 /* number of rows of buf tranferred */
        int     x0, y0, x1, y1;         /* coords, inclusive */
        int     pixsize;                /* sort of pixel size, in bytes */
        char    yzoom;                  /* (PixelXfer.src.yStep31:28) */
        char    flags;
};

	/* Flags for graphics DMA */

/* higher address rows of linear buf are transferred first */
#define PIXELXFER_LAST_LINE_FIRST         1

/* set if higher address pixels within a linear buf row are transferred first
 * (linear src only) */
#define PIXELXFER_END_OF_LINE_FIRST       2

/* linear tlb the driver must use to map the linear buffer */
#define PIXELXFER_TLBA  4

/* direction of transfer, linear src to tiled dst or the reverse */
#define PIXELXFER_LINEAR_SRC 8


	/* pixel dma constraints.  xlen and ylen are constrained to
	 * the coordinate range adressable by crime.  MAXSIZE is
	 * bounded because we don't want to hog graphics or system
	 * memory; 1.5 mb is a good number mainly because a vino
	 * frame is almost that big, in other words, for historical
	 * reasons. */

#define CRIME_DMA_MAXSIZE (512*512*6) 	/* 1.5Mb limit (pick a number) */
#define CRIME_DMA_MAXXLEN	(2048 * 4)
#define CRIME_DMA_MAXYLEN	(2048)

/*
 * Used by gl only.  This ioctl pins and cache-flushes a
 * user buffer, and maps the buffer using the selected
 * crime linear tlb for subsequent pixelxfer operations.  
 * Up to two buffers can be pinned simultaneously, one
 * in linearA and one in linearB.
 * Note that a buffer can be pinned for read or write.
 * This driver doesn't care, it just passes this bit of info
 * to userdma() which will either flush (if write) or just invalidate
 * the cache.  Probably a check is done to verify write permission
 * on the affected memory is done too.
 * A buffer remains pinned to a tlb until
 * the ioctl is called again, or the associated rn is
 * destroyed.  Passing NULL in crime_loadlineartlb.addr
 * is allowed; in this case we simply undo the last
 * pin on the selected tlb.
 */

struct crime_loadlineartlb {
	void *addr;		/* user virtual address to pin */
	int nbytes;		/* size of the buffer */
	int flags;		/* PIXELXFER_{LINEAR_SRC, TLBA} */
};

/*
 * Used by gl only.  Fill the selected buffer
 * with a constant; uses mte, so this ioctl
 * only does something if the destination
 * is not cid clipped.  If cid clipping is enabled,
 * we return a code that means 'do it yourself'.
 *
 * XXX We write the users mte shadowregs,
 * in case of an isr using mte to do something.
 * This reduces the number of per visible-box mte writes
 * from 6 to 2 for each box after the first.
 * We could have up to 16 or so boxes ...
 */
struct crime_clear_buffer {
	int color;		/* value to write to buffer */
	int bytemask;		/* mte bytemask */ 
	char bufdepth;		/* 1, 2, or 4 bytes/pixel */
	char bufsel;		/* select which tlb maps destination;
				   one of MBUF_FB_[ABC] */
	char flags;		/* see below */
};
#define CRIME_CLEAR_BUFFER_SCISSOR 1	/* clip clear to scissor box */

struct crime_loadfbtlb {
	int id;
	char idtype;
	char tlb;
	char flags;
};
/* crime_loadfbtlb.idtype */
#define CRIME_DRAWID 	0
#define CRIME_READID	1
#define CRIME_TEXID	2
#define CRIME_DMSID	3

/* crime_loadfbtlb.id */
#define CRIME_FRONTBUF       0
#define CRIME_BACKBUF        1
#define CRIME_SZBUF          2


struct crime_map_gbemem {
	void *gbedata;	/* user address, returned */
};

struct crime_map_shadow_cx {
	void *uscx;	/* user address, returned */
};

struct crime_setdisplaymode_args {
	long wid;
	unsigned long mode;
};

struct crime_setgammaramp_args {
	unsigned char red[256];
	unsigned char green[256];
	unsigned char blue[256];
	int rampid;
};


struct crime_timing_info
{
	int flags;          	/* see #defines above */
        short w, h;		/* monitor resolution */
        short fields_sec;	/* fields/sec */
        short cfreq;		/* pixel clock frequency (MHz) */
        unsigned short *ftab;	/* video timing frame table */
        short ftab_len;		/* length (in shorts) of *ftab */
        unsigned short *ltab;	/* video timing line table */
        short ltab_len;		/* length (in shorts) of *ltab */
};

/*
 * Texture memory management.
 */
#define CRM_NUM_TEX_MIPMAPS             6

#define CRM_TEX_ALLOC_BANK_0            0x0
#define CRM_TEX_ALLOC_BANK_1            0x1

typedef struct {
	unsigned long	texId;
	int         	nTiles;
	unsigned long	flag;
	void		*tile0;   /* user virtual address of texture tiles. */
} crmTexAlloc_t;

typedef struct {
	int         slotId;
	int         w, h;
	unsigned long      texIds[CRM_NUM_TEX_MIPMAPS];
} crmTexDefine_t;

/*
 * Structure used to pass chunk mapping request to
 * kernel, or to get tile list for a chunk.
 * XXX This badly needs cleaning up.
 */

struct CRIME_TLB_args {
	short ntiles;
	short what;
	short flags;
	char *vaddr;	/* returns user address tiles are mapped at */
	tilenum_t *buf;  /* 256 entry max */
};

/* CRIME_TLB_args.what */
#define CRM_ALLOC_FB 1
#define CRM_ALLOC_OV 2
#define CRM_ALLOC_GBE 3
#define CRM_ALLOC_CID 4
#define CRM_ALLOC_Z 5
#define CRM_ALLOC_TEX 6
#define CRM_ALLOC_BB 7

/* CRIME_TLB_args.flags */

#define CRM_ALLOC_MAPUSER 1



#endif /* __SYS_CRIME_GFX_H__ */

