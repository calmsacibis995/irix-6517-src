#ifndef __SYS_CRIME_GBE_H__
#define __SYS_CRIME_GBE_H__

/**************************************************************************
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 **************************************************************************/

/*
 * $Revision: 1.1 $
 */

typedef volatile unsigned int vui;
#define GBECHIP_ADDR	0x16000000
#define GBE_ID  	0x666



#define GBE_GUTS	\
	vui ctrlstat;		/* control and status 		0x0000 */\
	vui dotclock;		/* dot clock parameters		0x0004 */\
	vui i2c;		/* i2c interface		0x0008 */\
	vui sysclk;		/* system clock PLL control	0x000c */\
	vui i2cfp;		/* i2c flat panel control	0x0010 */\
	vui id;			/* device id and revision	0x0014 */\
	char _pad0[ padbytes(0x10000,id) ];\
\
	vui vt_xy;		/* current dot coords (inc. blank) 0x10000 */\
	vui vt_xymax;		/* maximum dot coords (inc. blank) 0x10004 */\
	vui vt_vsync;		/* vsync on/off 		   0x10008 */\
	vui vt_hsync;		/* hsync on/off 		   0x1000c */\
	vui vt_vblank;		/* vblank on/off 		   0x10010 */\
	vui vt_hblank;		/* hblank on/off 		   0x10014 */\
	vui vt_flags;		/* polarity of various vt signals  0x10018 */\
	vui vt_f2rf_lock;	/* f2rf and framelock y coords	   0x1001c */\
	vui vt_intr01;		/* intr 0,1 y coords		   0x10020 */\
	vui vt_intr23;		/* intr 2,3 y coords		   0x10024 */\
\
	vui fp_hdrv;		/* flat panel hdrv on/off	   0x10028 */\
	vui fp_vdrv;		/* flat panel vdrv on/off	   0x1002c */\
	vui fp_de;		/* flat panel de on/off		   0x10030 */\
\
	vui vt_hpixen;		/* internal horiz pixel on/off     0x10034 */\
	vui vt_vpixen;		/* internal vert pixel on/off      0x10038 */\
	vui vt_hcmap;		/* cmap write enable (horiz)	   0x1003c */\
	vui vt_vcmap;		/* cmap write enable (vert)	   0x10040 */\
		\
	vui did_start_xy;	/* eol/f reset values for did_xy   0x10044 */\
	vui crs_start_xy;	/* eol/f reset values for crs_xy   0x10048 */\
	vui vc_start_xy;	/* eol/f reset values for vc_xy	   0x1004c */\
	\
	char _pad1[ padbytes(0x20000,vc_start_xy) ];\
\
	vui ovr_width_tile;	/* overlay buffer width in tiles 0x20000 */\
	vui ovr_control;	/* tile list ptr and dma enable  0x20004 */\
	char _pad2[ padbytes(0x30000,ovr_control) ];\
\
	vui frm_size_tile;	/* frame buffer h/w in tiles	 0x30000 */\
	vui frm_size_pixel;	/* frame buffer h/w in pixels	 0x30004 */\
	vui frm_control;	/* tile list ptr and dma enable  0x30008 */\
	char _pad3[ padbytes(0x40000,frm_control) ];\
\
	vui did_control;	/* DID table ptr and dma enable  0x40000 */\
	char _pad4[ padbytes(0x48000,did_control) ];\
\
	vui mode_regs[32];	/* xmap mode registers		 0x48000 */\
	char _pad5[ padbytes(0x50000,mode_regs) ];\
\
	vui cmap[4608];		/* color palette 		 0x50000 */\
	char _pad6[ padbytes(0x58000,cmap) ];\
\
	vui cm_fifo;		/* # empty slots in cmap fifo    0x58000 */\
	char _pad7[ padbytes(0x60000,cm_fifo) ];\
\
	vui gmap[256];		/* gamma ramp			 0x60000 */\
	char _pad8[ padbytes(0x70000,gmap) ];\
\
	vui crs_pos;		/* cursor position		 0x70000 */\
	vui crs_ctl;		/* cursor enable, xhair enable   0x70004 */\
	vui crs_cmap[3];	/* cursor colors 1-3		 0x70008 */\
	char _pad9[ padbytes(0x78000,crs_cmap) ];\
\
	vui crs_glyph[64];	/* cursor glyph			 0x78000 */\
	char _pad10[ padbytes(0x80000, crs_glyph) ];\
\
	/* video capture stuff 		 0x80000 */\
	vui vc_lr;		/* x coords of capture window	 0x80000 */\
	vui vc_tb;		/* y coords of capture window	 0x80004 */\
	vui vc_filters;		/* capture filter stuff 	 0x80008 */\
	vui vc_control;		/* misc video capture stuff	 0x8000c */

#define FOO ((struct gbechip *)(0))

/*
 * physical gbe chip address map, 
 * contains large gaps in address space,
 * about 512kb total.
 */
struct gbechip {
#define padbytes(n,h) \
		(n - ((int)&(FOO->h) - (int)FOO) - sizeof(FOO->h))
	GBE_GUTS
};

/*
 * gbe chip virtual address map.  The
 * 9 physical pages that contain gbe registers are
 * mapped contiguously into Xsgi virtual address space.
 */
struct gbechip_nopad {
	/* Pad to next page boundary */
#undef padbytes
#define padbytes(n,h) \
		((n - ((int)&(FOO->h) - (int)FOO) - sizeof(FOO->h)) % _PAGESZ)
	GBE_GUTS
};

#undef padbytes
#undef FOO


typedef struct _gbe_videotiming {

	int framerate;		/* frames/sec */
	int dottime;		/* picoseconds/pixel */

	/*
	 * Assume each line looks like
	 * horizontal {front porch, sync, back porch, visible}
	 */
	int hsync_start, hsync_length;	/* in pixels */
	int hvis_start, hvis_length;	/* in pixels */

	/*
	 * dot clock stuff
	 */

	int m, n, l;			/* clock generation parameters */

}gbevt_t;

/*
 * The board manager and kernel manage some gbe operations
 * via shared memory, laid out as follows.
 */

#define N_GBE_DID_TILES 3

typedef struct {
	unsigned int age;	/* 'when' this table became PENDING */
	int status;		/* current status of this table -
				 * GBE_RS_{FREE, PENDING, ACTIVE} */
	paddr_t paddr;		/* physical address of memory 
				 * associated with resource (fb/ov
				 * tile list, DID tile) */
} gbe_resource_status;

#define GBE_RS_FREE	0
#define GBE_RS_PENDING	1	/* waiting to be loaded into gbe */
#define GBE_RS_ACTIVE	2	/* currently loaded in gbe */

/*
 * This data structure is part of the Xsgi context shadow area.
 * The normal and overlay tile lists referenced by the gbe hw
 * are stored here, and must be aligned on a 32 byte boundary.
 * The tiles used to triple buffer the DID table are
 * stored in struct crimeChunk crmGbeTables, and are mapped
 * into the server address space.  The server virtual address
 * of the did tables is stored in did_tile_vaddr[].
 */

typedef struct _gbedata {
	tilenum_t fblist[80];
	tilenum_t ovlist[32];
	paddr_t fblistaddr;	/* physical address of fblist[] */
	paddr_t ovlistaddr;	/* physical address of ovlist[] */
	caddr_t did_tile_vaddr[N_GBE_DID_TILES];
	gbe_resource_status did_tables[N_GBE_DID_TILES];
} gbedata_t;


/*
 * XXX miscellaneous gbe stuff.  Use the
 * x gbe headers when they move to gfx/kern/sys
 */

/*
 * gbe mode register bits
 */
#define GBEWT_BUF_BOT			0x1
#define GBEWT_BUF_TOP			0x2
#define GBE_OVR_CMAP_OFFSET             0x1100
#define GBE_WID_TYPE_MASK               0x1C
#define GBE_WID_TYPE_SHIFT              2
#define GBE_TYPE_I12			(1 << GBE_WID_TYPE_SHIFT)
#define GBE_TYPE_RGB5			(4 << GBE_WID_TYPE_SHIFT)
#define GBE_TYPE_RGB8			(5 << GBE_WID_TYPE_SHIFT)

#define gbeSetReg(hwpage, reg, val ) \
    (hwpage)->reg = (val);

#define gbeSetTable(hwpage, reg, index, val) \
    (hwpage)->reg[index] = (val);

#define GBE_CMFIFO_MASK 0x3f

#ifdef REAL_HARDWARE
#define gbeWaitCmapFifo(hwpage, n) \
        while (((hwpage)->cmfifo & GBE_CMFIFO_MASK) < (n));
#else
#define gbeWaitCmapFifo(hwpage, n)
#endif


#endif 	/* __SYS_CRIME_GBE_H__ */
