/*
 * odsy_tport.c
 *
 * textport graphics routines for Odyssey
 *
 * Copyright 1998, Silicon Graphics, Inc.
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
 *
 */


#include "sys/types.h"
#include "sys/systm.h"
#include "sys/cmn_err.h"
#include "sys/sbd.h"
#include "sys/sysmacros.h"
#include "sys/debug.h"
#include "sys/cpu.h"
#include "sys/param.h"

#if defined(_STANDALONE)
/*
#include "libsc.h"
#include "libsk.h"
*/
#include "stand_htport.h"
#else	/* !_STANDALONE */
#include "sys/htport.h"
#endif	/* _STANDALONE */

#include "sys/rrm.h"
#include "sys/gfx.h"
#include "sys/odsy.h"
#include "sys/odsyhw.h"
#include "odsy_internals.h"

/* Context info for textport */
static struct
{
  int cpos_x, cpos_y;
  int color;
  int width, height;
} tp_context;

static uint32_t intToOdsyFloat(int i); /* convert integer to odyssey floating point number */ 

/* Shadow area used by buzz.h macros */ 
static odsy_sw_shadow sw_shadow_area;
static odsy_sw_shadow * sw_shadow = &sw_shadow_area;

#define ODSY_FLOAT_ONE	0x3FC00000

#ifdef ODSY_SIM_KERN
#define GRADIENT_CHEAT  1	/* Draw a smooth quad instead of the gradient lines when using true color mode */
#define SKIP_GRADIENT	1	/* Faster yet - Draw a solid box instead of the gradient (should be fast with rect) */
#define USE_LINES	0	/* Draw lines for pixel-wide rectangles */
#define USE_RECT	1	/* Draw rects instead of quads and leave polygon stipple enabled for bitmaps - faster for sim */
#define USE_TRUECOLOR	0	/* Render in RGBA - just for debugging on simulators */ 
#else
#define GRADIENT_CHEAT  0
#define SKIP_GRADIENT   0
#define USE_LINES       0
#define USE_RECT        1
#define USE_TRUECOLOR   0
#endif

#if defined(_STANDALONE )

#if defined(DEBUG)
void pon_puts(char *);
void pon_puthex(__uint32_t);
#define DPRINTF(s)		pon_puts(s)
#define DHEX(x)			pon_puthex(x)
#else	/* !DEBUG */
#define DPRINTF(s)
#define DHEX(x) 
#endif	/* DEBUG */

#else	/* !_STANDALONE */

#if defined(DEBUG)
#define DPRINTF(s)		printf s 
#define DHEX(x)			printf ("0x%x", x)
#else	/* !DEBUG */
#define DPRINTF(s)
#define DHEX(x) 
#endif	/* DEBUG */

#endif	/* _STANDALONE */


#if !( defined(DEBUG) && defined(TEXTPORT_BRINGUP) )
/* debug macros do nothing */
# define PROC_ENTRY(tag) 
# define PROC_DATA(x, y)
/* Make sure there is enough room in the cfifo */
# define TPORT_CFIFO_POLL(n)	ODSY_POLL_CFIFO_LEVEL(hw,ODSY_CFIFO_LEVEL_REQ(n))
# define TPORT_DFIFO_POLL(n)	ODSY_POLL_DFIFO_LEVEL(hw,ODSY_DFIFO_LEVEL_REQ(n))

/*
 *  The hw sync seems to be required for stand alone. If it is not enabled, 
 *  the system panics.
 */
#if defined( _STANDALONE )
# define PROC_SYNC() { ODSY_SEND_HWSYNC(ODSY_HWSYNC_CFIFO_WAIT | ODSY_HWSYNC_PIXPIPE_SRC); }
#else
# define PROC_SYNC()
#endif  /* _STANDALONE */

#else
/*
 * debug macros save the name of the last odsy_tport routine called
 * and some arguments
 */
int textport_nop;	/* don't touch hardware until this flag is cleared */
static char *proc_entry_name;
static char *proc_str;
static long proc_val;
static char proc_buf[256];
static int proc_count;
static __odsyreg_fifo_levels proc_levels;
static __odsyreg_status0 proc_st0;
static drawbitmapCount, sboxfiCount;



#if defined( _STANDALONE )
extern void sprintf( char* fmt, ... );
#define arcs_outstr(x)      DPRINTF((x))

#else
static void
arcs_outstr(char *cp)
{
    extern void arcs_write(long, void *, long, void *);
    long a;

    while (*cp) {
	if (*cp == '\n')
	    arcs_write(1, "\r\n", 2, &a);
	else
	    arcs_write(1, cp, 1, &a);
	cp++;
    }
}
#endif

# define PROC_ENTRY(tag) if (textport_nop) return; proc_entry_name = #tag
# define PROC_DATA(x, y) proc_str = x; proc_val = y
/*
 * cfifo and dfifo macros do not loop forever
 * but rather drop into the debugger after a fixed number of iterations
 */
# define TPORT_CFIFO_POLL(n) \
    for (proc_count = 1000; proc_count >= 0; proc_count--) { \
	ODSY_READ_HW(hw, sys.status0, *(__uint32_t *)&proc_st0); \
        ODSY_READ_HW(hw, sys.fifo_levels, *(__uint32_t *)&proc_levels); \
	if (proc_st0.cfifo_mt && proc_st0.xrfifo_hw == 0 && proc_levels.gfx_flow_fifo_dw_cnt == 0) \
	    break; \
	if (proc_count == 0) { \
	    sprintf(proc_buf, "Hung in %s, '", proc_entry_name); \
	    arcs_outstr(proc_buf); \
	    sprintf(proc_buf, proc_str, proc_val); \
	    arcs_outstr(proc_buf); \
	    sprintf(proc_buf, "', fifo_levels=%d, cfifo_mt=%d, xrfifo_hw=%d, sboxfiCount=%d, drawbitmapCount=%d\n", \
		    proc_levels.gfx_flow_fifo_dw_cnt, \
		    proc_st0.cfifo_mt, proc_st0.xrfifo_hw, \
		    sboxfiCount, drawbitmapCount); \
	    arcs_outstr(proc_buf); \
	    debug("ring"); \
	} \
    }
# define TPORT_DFIFO_POLL(n) \
    for (proc_count = 1000; proc_count >= 0; proc_count--) { \
        ODSY_READ_HW(hw, sys.fifo_levels, *(__uint32_t *)&proc_levels); \
	if (proc_levels.dbe_flow_fifo_dw_cnt == 0) \
	    break; \
	if (proc_count == 0) { \
	    sprintf(proc_buf, "Hung in %s, fifo_levels=%d, '", \
		    proc_entry_name, proc_levels.gfx_flow_fifo_dw_cnt); \
	    arcs_outstr(proc_buf); \
	    sprintf(proc_buf, proc_str, proc_val); \
	    arcs_outstr(proc_buf); \
	    arcs_outstr("'\n"); \
	    debug("ring"); \
	} \
    }
# define PROC_SYNC() { \
    ODSY_SEND_HWSYNC(ODSY_HWSYNC_CFIFO_WAIT | ODSY_HWSYNC_PIXPIPE_SRC); \
    }
#endif

/* Flush the frame-buffer cache after geometry to make sure it reaches the screen */
#define TPORT_GEOM_SUFFIX()	BUZZ_CMD_FB_FLUSH()

#if USE_TRUECOLOR
static int colormap[256][3];
#endif

/* NOTE:   The textport origin is in the bottom left, so this macro flips everything in 
   vertically to match odyssey hardware */

#if USE_TRUECOLOR
#define BUZZ_VTX2D_CI(x,y,c) \
	ODY_STORE_i(BUZZ_RASTER(VTX_C_OPCODE, VTX_C_COUNT)); \
	ODY_STORE_ui((colormap[c][0] << 16) | (colormap[c][1] << 4)); \
	ODY_STORE_ui((colormap[c][2] << 16)); \
	ODY_STORE_ui(ODSY_FLOAT_ONE); /* w inv */ \
	ODY_STORE_ui(0); /* z */ \
	ODY_STORE_ui(intToOdsyFloat(x)); \
	ODY_STORE_ui(intToOdsyFloat(tp_context.height - (y)));
#else
#define BUZZ_VTX2D_CI(x,y,c) \
	ODY_STORE_i(BUZZ_RASTER(VTX_C_OPCODE, VTX_C_COUNT)); \
	ODY_STORE_ui((c) << 12); \
	ODY_STORE_ui(0); \
	ODY_STORE_ui(ODSY_FLOAT_ONE); /* w inv */ \
	ODY_STORE_ui(0); /* z */ \
	ODY_STORE_ui(intToOdsyFloat(x)); \
	ODY_STORE_ui(intToOdsyFloat(tp_context.height - (y)));
#endif


static void odsy_hw_sync(void *hw);
static void odsy_tpinit(void *hw);
void odsy_init_cursor(void*); /* standalone cursor initialization */
static int odsy_getdesc(void *hw, int code);
static void odsy_drawbitmap(void *hw, struct htp_bitmap *bitmap);
static void odsy_cmov2i(void *hw, int x, int y);
static void odsy_pnt2i(void *hw, int x, int y);
static void odsy_sboxfi(void *hw, int x1, int y1, int x2, int y2);
static void odsy_mapcolor(void *hw, int col, int r, int g, int b);
static void odsy_color(void *hw, int col);
static int  odsy_positioncursor(void *hw, int x, int y);
static void odsy_unblank(void *hw);

#if defined( _STANDALONE )
void odsy_stand_tpinit(void *hw, int *x, int *y);
static void odsy_movec(void*, int, int);
static void odsy_blankscreen(void*, int);
static void odsy_blockm(void*, int, int, int, int, int, int);
#endif


/* Convert an integer to an odyssey float */ 
uint32_t
intToOdsyFloat(int i)
{
  uint32_t exp = (127 + 22);

  if (!i)
    return 0;

  /* Rotate int left until the leading 1 hits bit 22 */
  while (!(i & (1 << 22))) {
    i <<= 1;
    exp--; 
  }

  return (exp << 23) | i;
}

/*
 * Function:	odsy_color	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *		col		New current drawing color (CI8 value)
 *
 * Description:	This function sets the current drawing color to col
 *
 */

static void
odsy_color(void *hw, int col)
{
  tp_context.color = col;
}

/*
 * Function:	odsy_mapcolor	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *		col		Index to colormap
 *		r		Red value
 *		g		Green value
 *		b		Blue value
 *
 * Description:	This function sets the color map entry indexed by col to the
 *		given R, G, and B values.
 *
 */

static void
odsy_mapcolor(void *hw, int col, int r, int g, int b)
{
  PROC_ENTRY(odsy_mapcolor);
  PROC_DATA("index=%d", col);
#if !USE_TRUECOLOR

  TPORT_DFIFO_POLL(1);

  /* create a 10-bit rgb value by extending incoming 8-bit value */
  ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(DBE_PACKET_HDR(DBE_UPDATE_IMMED,
							      DBE_BUZZ_ALL,
							      DBE_ADDR(cmap_clut) + col,
							      1),
					       ((((r << 2) | (r >> 6)) << 20) |
						(((g << 2) | (g >> 6)) << 10) |
						(((b << 2) | (b >> 6)) << 00))));
#else
  colormap[col][0] = r;
  colormap[col][1] = g;
  colormap[col][2] = b;
#endif
}


/*
 * Function:	odsy_sboxfi	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *		x1, y1		First set of coordinates
 *		x2, y2		Second set of coordinates (inclusive)
 *
 * Description:	This function draws a filled rectangle (presumably filled
 *		with the current drawing color) via the rendering engine.
 *
 */

static void
odsy_sboxfi(void *hw, int x1, int y1, int x2, int y2)
{

  int i;

  PROC_ENTRY(odsy_sboxfi);
  PROC_DATA("y2=0x%x", y2);
  TPORT_CFIFO_POLL(50);
  PROC_SYNC();

#if defined(DEBUG) && defined(TEXTPORT_BRINGUP)
  sboxfiCount++;
  while (0) {
    static lastx1, lasty1, lastx2, lasty2, repeat;

    if (x1 == lastx1 && x2 == lastx2 &&
	y1 == y2 && lasty1 == lasty2 &&
	y1 == (lasty1 + 1)) {
	lastx1 = x1;
	lasty1 = y1;
	lastx2 = x2;
	lasty2 = y2;
	repeat++;
	break;
    }
    if (repeat) {
        sprintf(proc_buf, "...sboxfi %x %x %x %x\n", lastx1, lasty1, lastx2, lasty2);
        arcs_outstr(proc_buf);
    }
    sprintf(proc_buf, "sboxfi %x %x %x %x\n", x1, y1, x2, y2);
    arcs_outstr(proc_buf);
    lastx1 = x1;
    lasty1 = y1;
    lastx2 = x2;
    lasty2 = y2;
    repeat = 0;
    break;
  }
#endif


  /* Order the coordinates */
  if (x2 < x1) {
    int tmp = x1;
    x1 = x2;
    x2 = tmp;
  }

  if (y2 < y1) {
    int tmp = y1;
    y1 = y2;
    y2 = tmp;
  } 
  
#if GRADIENT_CHEAT && USE_TRUECOLOR
  if ((x1 == 0 && x2 == tp_context.width - 1) && (y1 == y2)) {
    static int first_color;
    if (y1 == 0)
      first_color = tp_context.color;
    else if (y1 == tp_context.height - 1) {

#if SKIP_GRADIENT
      odsy_sboxfi(hw,x1,0,x2,y2);
      { const int i = 1; if (i) return; } /* return without compiler noise */
#endif

#if USE_RECT
      ENABLE_POLY_STIPPLE_PUT(sw_shadow->enable, 0);
#endif
      ENABLE_FLAT_SHADE_PUT(sw_shadow->enable, 0); /* Disable flat shading */
      BUZZ_CMD_ENABLE(); /* Send enable token */

      BUZZ_CMD_BEGIN(BEGIN_PRIM_QUADS);
      BUZZ_VTX2D_CI(x1, 0, first_color);
      BUZZ_VTX2D_CI(x1, y2+1, tp_context.color);
      BUZZ_VTX2D_CI(x2+1, y2+1, tp_context.color);
      BUZZ_VTX2D_CI(x2+1, 0, first_color);
      BUZZ_CMD_END();

#if USE_RECT      
      ENABLE_POLY_STIPPLE_PUT(sw_shadow->enable, 1);
#endif
      ENABLE_FLAT_SHADE_PUT(sw_shadow->enable, 1); /* Re-enable flat shading */
      BUZZ_CMD_ENABLE(); /* Send enable token */
    }
  } else
#endif

#if USE_LINES
  if (y1 == y2) {
    BUZZ_CMD_BEGIN(BEGIN_PRIM_LINES);
    BUZZ_VTX2D_CI(x1, y1, tp_context.color);
    BUZZ_VTX2D_CI(x2+1, y1, tp_context.color);
    BUZZ_CMD_END();
  } else if (x1 == x2) {
    BUZZ_CMD_BEGIN(BEGIN_PRIM_LINES);
    BUZZ_VTX2D_CI(x1+1, y1, tp_context.color);
    BUZZ_VTX2D_CI(x2+1, y2+1, tp_context.color);
    BUZZ_CMD_END();
  } else
#endif
  {    
#if USE_RECT
    BUZZ_CMD_BEGIN(BEGIN_PRIM_RECT);
    BUZZ_VTX2D_CI(x1, y2+1, tp_context.color);
    BUZZ_VTX2D_CI(x2+1, y1, tp_context.color);
    BUZZ_CMD_END();
#else
    BUZZ_CMD_BEGIN(BEGIN_PRIM_QUADS);
    BUZZ_VTX2D_CI(x1, y1, tp_context.color);
    BUZZ_VTX2D_CI(x1, y2+1, tp_context.color);
    BUZZ_VTX2D_CI(x2+1, y2+1, tp_context.color);
    BUZZ_VTX2D_CI(x2+1, y1, tp_context.color);
    BUZZ_CMD_END();
#endif
  }

  TPORT_GEOM_SUFFIX();
}

/*
 * Function:	odsy_pnt2i	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *
 * Description:	Not used?
 *
 */

static void
odsy_pnt2i(void *hw, int x, int y)
{
  PROC_ENTRY(odsy_pnt2i);
  PROC_DATA("x,y=%d,%d", (x<<16)|y);
  TPORT_CFIFO_POLL(10);
#ifdef TEXTPORT_BRINGUP
  arcs_outstr("pnt2i\n");
#endif
  PROC_SYNC();

  BUZZ_CMD_BEGIN(ODSY_PRIM_GL_POINTS);
  BUZZ_VTX2D_CI(x, y+1, tp_context.color);
  BUZZ_CMD_END();

  TPORT_GEOM_SUFFIX();
}


/*
 * Function:	odsy_cmov2i	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *		x, y		New destination coordinates
 *
 * Description:	This function sets the position of the drawing context as
 *		given by the parameters.
 *
 */

static void
odsy_cmov2i(void *hw, int x, int y)
{
    tp_context.cpos_x = x;
    tp_context.cpos_y = y;
}

/*
 * Function:	odsy_drawbitmap	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *		bitmap		Pointer to bitmap
 *
 * Description:	This function takes a bitmap (as described the header file
 *		htport.h) and draws it to the framebuffer.
 */
static void
odsy_drawbitmap(void *hw, struct htp_bitmap *bitmap)
{
  int xsize, ysize, i, j, x, y, width, height;
  unsigned short * bitmap_ptr = bitmap->buf;
  uint16_t * stipple_ptr = (uint16_t *) &sw_shadow->stip_pat_a[0];

  PROC_ENTRY(odsy_drawbitmap);
  PROC_DATA("x,y=0x%x", (tp_context.cpos_x << 16) | tp_context.cpos_y);
  TPORT_CFIFO_POLL(100);
  PROC_SYNC();
#if defined(DEBUG) && defined(TEXTPORT_BRINGUP)
  drawbitmapCount++;
#endif

#if !USE_RECT
  /* Set the poly stipple enable bit */
  ENABLE_POLY_STIPPLE_PUT(sw_shadow->enable, 1);
  BUZZ_CMD_ENABLE();
#endif

  /* Draw the bitmap using 16w x 64h stippled quads since the incoming bitmap is 16-bit aligned */
  ysize = bitmap->ysize;
  y = tp_context.cpos_y - bitmap->yorig;

  while (ysize > 0) {

    xsize = bitmap->xsize;
    x = tp_context.cpos_x - bitmap->xorig;
    height = MIN(ysize, 64);
    i = 0;

    while (xsize > 0) {
      
      width = MIN(xsize, 16);

      /* Copy bitmap to stipple buffer */
      for (j = 0; j < height; j++)
	stipple_ptr[height - j - 1] = *(bitmap_ptr + bitmap->sper * j + i);

#define USE_ONLY_RECTS 1
#if USE_ONLY_RECTS
      {
	int hh, ww, pattern;

        for (hh = 0; hh < height; hh++) {
	  pattern = stipple_ptr[height - 1 - hh];
	  for (ww = 0; ww < width; ww++) {
	    if (pattern & 0x8000) {
	       odsy_sboxfi(hw, x+ww, y+hh, x+ww, y+hh);
	       //BUZZ_CMD_BEGIN(BEGIN_PRIM_RECT);
	       //BUZZ_VTX2D_CI(x+ww, y+hh+1, tp_context.color);
	       //BUZZ_VTX2D_CI(x+ww+1, y+hh, tp_context.color);
	       //BUZZ_CMD_END();
	    }
	    pattern <<= 1;
	  }
        }
      }
#else
      BUZZ_CMD_POLY_STIPPLE(); /* Setup and load polygon stipple and mode */

      BUZZ_CMD_BEGIN(BEGIN_PRIM_QUADS);
      BUZZ_VTX2D_CI(x, y + height, tp_context.color);
      BUZZ_VTX2D_CI(x, y, tp_context.color);
      BUZZ_VTX2D_CI(x + width, y, tp_context.color);
      BUZZ_VTX2D_CI(x + width, y + height, tp_context.color);
      BUZZ_CMD_END();
#endif

      xsize -= width;
      x += width;
      i++;
    }
    ysize -= height;
    y  += height;
    bitmap_ptr += bitmap->sper * 64;
  }

#if !USE_RECT
  /* Reset the poly stipple enable bit if using quads for sboxes */
  ENABLE_POLY_STIPPLE_PUT(sw_shadow->enable, 0);
  BUZZ_CMD_ENABLE();
#endif

  tp_context.cpos_x += bitmap->xmove;
  tp_context.cpos_y += bitmap->ymove;

  TPORT_GEOM_SUFFIX();
}


#if !defined(_STANDALONE)
/* 
 * Function:    odsy_getdesc 
 *
 * Parameters:  hw		Pointer to ODSY hardware
 * 		code
 *
 * Description:
 *
 */
static int
odsy_getdesc(void *hw, int code)
{
  switch(code){
  case HTP_GD_BIGMAPS:
    return 1;
  }
  return 0;
}
#endif	/* _STANDALONE */

/*
 * Function:	odsy_tpinit	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *
 * Description:	setup odsy for textport rendering.
 */
static void
odsy_tpinit(void *hw)
{
  odsy_data * data = (odsy_data *) ODSY_BDATA_FROM_BASE(hw);
  
#if TEXTPORT_BRINGUP
  arcs_outstr("calling odsy_tpinit\n");
#endif
  tp_context.width = data->info.gfx_info.xpmax;
  tp_context.height = data->info.gfx_info.ypmax;

  PROC_ENTRY(odsy_tpinit);

  TPORT_CFIFO_POLL(100);

  /*
  ** Initialize pixel mode and buffer select
  */
  sw_shadow->pixel_mode.origin = 0;
  sw_shadow->pixel_mode.stride = 0;
  sw_shadow->pixel_mode.status = 0;
  sw_shadow->pixel_mode.zcull_origin = 0;

#if !USE_TRUECOLOR 
#ifdef ODSY_TEXTPORT_IN_OVERLAY
  sw_shadow->buffer_select = 0xc; /* back for overlay plane */
  PIXEL_MODE_PIXEL_FMT_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_PIXEL_FMT_B_FMT);
  PIXEL_MODE_WIN_DEPTH_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_WIN_DEPTH_BYTE_2);
  PIXEL_MODE_FB_DEPTH_PUT(sw_shadow->pixel_mode.status,  PIXEL_MODE_FB_DEPTH_BYTE_2);
  PIXEL_MODE_PIXEL_ORIGIN_PUT(sw_shadow->pixel_mode.origin, data->mm.wbfr.base_addr);
  PIXEL_MODE_PIXEL_STRIDE_PUT(sw_shadow->pixel_mode.stride, ODSY_MM_NR_TILES_FROM_PIXELS(data->mm.wbfr.attr.dx));
#else
  sw_shadow->buffer_select = 0x3; /* front normally */
  PIXEL_MODE_PIXEL_FMT_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_PIXEL_FMT_E_FMT);
  PIXEL_MODE_WIN_DEPTH_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_WIN_DEPTH_BYTE_8);
  PIXEL_MODE_FB_DEPTH_PUT(sw_shadow->pixel_mode.status,  PIXEL_MODE_FB_DEPTH_BYTE_8);
  PIXEL_MODE_PIXEL_ORIGIN_PUT(sw_shadow->pixel_mode.origin, data->mm.fbfr.base_addr);
  PIXEL_MODE_PIXEL_STRIDE_PUT(sw_shadow->pixel_mode.stride, ODSY_MM_NR_TILES_FROM_PIXELS(data->mm.fbfr.attr.dx));
#endif
  /* Use 8-bit CI */
  PIXEL_MODE_FIELD_FMT_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_FIELD_FMT_IDX);
  PIXEL_MODE_PIXEL_INDEX_PUT(sw_shadow->pixel_mode.status, 1);
#else
  sw_shadow->buffer_select = 0x3; /* front */
  /* Use 8-bit rgba */
  PIXEL_MODE_FIELD_FMT_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_FIELD_FMT_RGBA);
  PIXEL_MODE_PIXEL_FMT_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_PIXEL_FMT_C_FMT);
  PIXEL_MODE_WIN_DEPTH_PUT(sw_shadow->pixel_mode.status, PIXEL_MODE_WIN_DEPTH_BYTE_8);
  PIXEL_MODE_FB_DEPTH_PUT(sw_shadow->pixel_mode.status,  PIXEL_MODE_FB_DEPTH_BYTE_8);
  PIXEL_MODE_PIXEL_INDEX_PUT(sw_shadow->pixel_mode.status, 0);
  PIXEL_MODE_PIXEL_ORIGIN_PUT(sw_shadow->pixel_mode.origin, data->mm.fbfr.base_addr);
  PIXEL_MODE_PIXEL_STRIDE_PUT(sw_shadow->pixel_mode.stride, ODSY_MM_NR_TILES_FROM_PIXELS(data->mm.fbfr.attr.dx));
#endif
  PIXEL_MODE_USE_CLEAR_PUT(sw_shadow->pixel_mode.status, 1); /* Enable filled rects */
  PIXEL_MODE_SCRATCH_BUFFER_PUT(sw_shadow->pixel_mode.status, 1); /* Disable buf select, screen mask, wid check, window offset */
  BUZZ_CMD_PIXEL_MODE();	/* raw pixel_mode token, no ucode calls please */
  BUZZ_CMD_BUFFER_SELECT();
#ifdef TEXTPORT_BRINGUP
  sprintf(proc_buf, "pixel_mode.status = %x\n", sw_shadow->pixel_mode.status);
  arcs_outstr(proc_buf);
#endif
  /*
   * Even though this is marked as a scratch buffer,
   * send down window offset and screen mask anyway.
   * There are hw bugs in this area.
   */
  sw_shadow->wind_off_x =
  sw_shadow->wind_off_y = 0;
  WINDOW_WIND_OFF_X_PUT(sw_shadow->wind_off_x, 0);
  WINDOW_WIND_OFF_Y_PUT(sw_shadow->wind_off_y, 0);
  BUZZ_CMD_WINDOW();
  sw_shadow->screen_mask[0] =
  sw_shadow->screen_mask[1] = 0;
  SCREEN_MASK_SMASK_XMIN0_PUT(sw_shadow->screen_mask[0], 0);
  SCREEN_MASK_SMASK_YMIN0_PUT(sw_shadow->screen_mask[0], 0);
  sw_shadow->screen_mask[2] =
  sw_shadow->screen_mask[4] =
  sw_shadow->screen_mask[6] = sw_shadow->screen_mask[0];
  SCREEN_MASK_SMASK_XMAX0_PUT(sw_shadow->screen_mask[1], tp_context.width);
  SCREEN_MASK_SMASK_YMAX0_PUT(sw_shadow->screen_mask[1], tp_context.height);
  sw_shadow->screen_mask[3] =
  sw_shadow->screen_mask[5] =
  sw_shadow->screen_mask[7] = sw_shadow->screen_mask[1];
  BUZZ_CMD_SCREEN_MASK();
  /* do we need scissor box ? */
  sw_shadow->scissor_xlimits = sw_shadow->scissor_ylimits = 0;
  SCISSOR_BOX_SCISSOR_XMIN_PUT(sw_shadow->scissor_xlimits, 0);
  SCISSOR_BOX_SCISSOR_XMAX_PUT(sw_shadow->scissor_xlimits, tp_context.width);
  SCISSOR_BOX_SCISSOR_YMIN_PUT(sw_shadow->scissor_ylimits, 0);
  SCISSOR_BOX_SCISSOR_YMAX_PUT(sw_shadow->scissor_ylimits, tp_context.height);
  BUZZ_CMD_SCISSOR_BOX();
  /* 
  ** Initialize enables
  */    
  sw_shadow->enable = 0;
  sw_shadow->en_others = 0;

#if USE_RECT
  // don't enable stipple
  //ENABLE_POLY_STIPPLE_PUT(sw_shadow->enable, 1); /* Enable poly stipple for bitmaps if sboxes are rects */
#endif

  ENABLE_FLAT_SHADE_PUT(sw_shadow->enable, 1); /* Enable flat shading */
  BUZZ_CMD_ENABLE(); /* Send enable token */
#ifdef TEXTPORT_BRINGUP
  sprintf(proc_buf, "sw_shadow->enable=%x\n", sw_shadow->enable);
  arcs_outstr(proc_buf);
#endif

  EN_OTHERS_MP_LGT_ENV_PUT(sw_shadow->en_others, EN_OTHERS_MP_LGT_ENV_LET); /* Pass color through lighting env */
  BUZZ_CMD_ENABLE_OTHERS();
  
  /*
  ** Initialize raster mode
  */
  sw_shadow->raster_mode = 0;
  RASTER_MODE_SGN_PROJ_MAT23_PUT(sw_shadow->raster_mode, 1);
  BUZZ_CMD_RASTER_MODE();

  /*
  ** Enable all the masks
  */

  sw_shadow->writemask = ~0;
  BUZZ_CMD_WRITEMASK();

  sw_shadow->x_writemask_rg = ~0;
  sw_shadow->x_writemask_ba = ~0;
  sw_shadow->x_writemask_depth = ~0;                         
  sw_shadow->x_writemask_stencil = ~0;
  BUZZ_CMD_X_WRITEMASK();

  /*
  ** Disable wid test
  */
  ODY_STORE_i(BUZZ_RASTER(WID_OPCODE, WID_COUNT));
  ODY_STORE_i(0);
  ODY_STORE_i(0);

  /*
  ** Enable polymode fill
  */

  sw_shadow->poly_mode = 0;
  POLY_MODE_FRONT_MODE_PUT(sw_shadow->poly_mode, POLY_MODE_FRONT_MODE_FILL);
  POLY_MODE_BACK_MODE_PUT(sw_shadow->poly_mode, POLY_MODE_BACK_MODE_FILL);
  BUZZ_CMD_POLY_MODE();

  /*
  ** Set up the polygon stipple mode for drawing characters
  */
  POLY_STIPPLE_OPAQUE_PUT(sw_shadow->poly_stipple, 0);
  POLY_STIPPLE_RELATIVE_PUT(sw_shadow->poly_stipple, POLY_STIPPLE_RELATIVE_PRIM_REL);
  POLY_STIPPLE_ASPECT_PUT(sw_shadow->poly_stipple, POLY_STIPPLE_ASPECT_TALL_ASP); /* 16 x 64 */

  /*
  ** Initialize line width and point size
  */
  WIDTH_LINE_WIDTH_PUT(sw_shadow->line_width, BUZZ_FLOAT_TO_WIDTH(1.0));
  WIDTH_POINT_SIZE_PUT(sw_shadow->point_size, BUZZ_FLOAT_TO_WIDTH(1.0));
  BUZZ_CMD_WIDTH();
#if TEXTPORT_BRINGUP
  arcs_outstr("returning from odsy_tpinit\n");
#endif
}

static void
odsy_unblank(void *hw)
{
}


#if defined(_STANDALONE)

/*******************************************************
 *  STANDALONE STUFF
 *******************************************************/

/*
 * Function:	odsy_positioncursor	
 *
 * Parameters:	hw		Pointer to ODSY hardware
 *
 * Description:	display the cursor at the given location
 */

static int
odsy_positioncursor(void *hw, int x, int y)
{
  struct cursor_xy_reg xy;

  xy.x = x;
  xy.y = y;

  TPORT_DFIFO_POLL(1);

  ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
				      cursor_xy,
				      *(__uint32_t *) &xy));

  return 0;
}


void
odsy_init_cursor(void *hw)
{
  extern unsigned char vc1_ptr_bits[];
  extern unsigned char vc1_ptr_ol_bits[];
  int i;

    /*
     * Notice:  this is the correct mapping.  transpose the middle
     *          two columns, then the image is mirrored.
     */
  struct cursor_control_reg control;
  __uint32_t cursor_glyph[ODSY_CURSOR_GLYPH_SIZE] = 
    {
        0x00000022,0x00000000,0x00000000,0x00000000,
        0x00000212,0x00000000,0x00000000,0x00000000,
        0x00002112,0x00000000,0x00000000,0x00000000,
        0x00021112,0x00000000,0x00000000,0x00000000,
        0x00211112,0x00000000,0x00000000,0x00000000,
        0x02111112,0x00000000,0x00000000,0x00000000,
        0x21111112,0x00000000,0x00000000,0x00000000,
        0x11111112,0x00000002,0x00000000,0x00000000,
        0x11111112,0x00000021,0x00000000,0x00000000,
        0x11111112,0x00000021,0x00000000,0x00000000,
        0x21111112,0x00000022,0x00000000,0x00000000,
        0x21112112,0x00000000,0x00000000,0x00000000,
        0x21112222,0x00000000,0x00000000,0x00000000,
        0x11220000,0x00000002,0x00000000,0x00000000,
        0x11200000,0x00000002,0x00000000,0x00000000,
        0x22000000,0x00000000,0x00000000,0x00000000,
    };

  __uint32_t cursor_alpha[ODSY_CURSOR_ALPHA_SIZE] = 
    {
        0x0000000F,0x00000000,
        0x0000003F,0x00000000,
        0x000000FF,0x00000000,
        0x000003FF,0x00000000,
        0x00000FFF,0x00000000,
        0x00003FFF,0x00000000,
        0x0000FFFF,0x00000000,
        0x0003FFFF,0x00000000,
        0x000FFFFF,0x00000000,
        0x000FFFFF,0x00000000,
        0x00003FFF,0x00000000,
        0x0000FF3F,0x00000000,
        0x0000FF0F,0x00000000,
        0x0003FC00,0x00000000,
        0x0003FC00,0x00000000,
        0x0000F000,0x00000000,
    };

  __uint32_t cursor_lut[ODSY_CURSOR_LUT_SIZE][3] = {
    0x00,0xFF,0x00, /* 0 */
    0xFF,0x00,0x00, /* 1 */
    0x00,0xFF,0xFF, /* 2 */
    0x00,0x00,0x00, /* 3 */
    0x00,0x00,0x00, /* 4 */
    0x00,0x00,0x00, /* 5 */
    0x00,0x00,0x00, /* 6 */
    0x00,0x00,0x00, /* 7 */
    0x00,0x00,0x00, /* 8 */
    0x00,0x00,0x00, /* 9 */
    0x00,0x00,0x00, /* 10 */
    0x00,0x00,0x00, /* 11 */
    0x00,0x00,0x00, /* 12 */
    0x00,0x00,0x00, /* 13 */
    0x00,0x00,0x00, /* 14 */
    /* no index 15 - 15 inverts covered pixel */
  };


  /* Double-word alignment depends on even number of entries */
  TPORT_DFIFO_POLL(1);
  ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(DBE_PACKET_HDR(DBE_UPDATE_IMMED,
							      DBE_BUZZ_ALL,
							      DBE_ADDR(cursor_glyph),
							      0),
					       DBE_PACKET_HDR(DBE_UPDATE_IMMED,
							      DBE_BUZZ_ALL,
							      DBE_ADDR(cursor_glyph),
							      ODSY_CURSOR_GLYPH_SIZE)));

  for (i = 0; i < ODSY_CURSOR_GLYPH_SIZE; i += 2) {
    TPORT_DFIFO_POLL(1);
    ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(cursor_glyph[i], cursor_glyph[i+1]));
  }
  
  /* Double-word alignment depends on even number of entries */
  TPORT_DFIFO_POLL(1);
  ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(DBE_PACKET_HDR(DBE_UPDATE_IMMED,
							      DBE_BUZZ_ALL,
							      DBE_ADDR(cursor_alpha),
							      0),
					       DBE_PACKET_HDR(DBE_UPDATE_IMMED,
							      DBE_BUZZ_ALL,
							      DBE_ADDR(cursor_alpha),
							      ODSY_CURSOR_ALPHA_SIZE)));

  for (i = 0; i < ODSY_CURSOR_ALPHA_SIZE; i += 2) {
    TPORT_DFIFO_POLL(1);
    ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(cursor_alpha[i], cursor_alpha[i+1]));
  }

  /* Cursor lut has an odd number of entries unlike above */
  TPORT_DFIFO_POLL(1);

#define ARRAY_TO_ENTRY(a) (((a)[0] << 22) | ((a)[1] << 12) | ((a)[2] << 2))
  
  ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(DBE_PACKET_HDR(DBE_UPDATE_IMMED,
							      DBE_BUZZ_ALL,
							      DBE_ADDR(cursor_LUT),
							      ODSY_CURSOR_LUT_SIZE),
					       ARRAY_TO_ENTRY(cursor_lut[0])));

  for (i = 1; i < ODSY_CURSOR_LUT_SIZE; i += 2) {
    TPORT_DFIFO_POLL(1);
    ODSY_WRITE_HW(hw, dbe, ODSY_DW_CFIFO_PAYLOAD(ARRAY_TO_ENTRY(cursor_lut[i]),
						 ARRAY_TO_ENTRY(cursor_lut[i+1])));
  } 

  /* Enable the cursor */
  control.dual_head_enable = 0;
  control.fq_enable = 0;
  control.xhair_mode = 0;
  control.enable = 1;
  control.delay_enable = 1;
  TPORT_DFIFO_POLL(1);
  ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,cursor_control,*(__uint32_t *) &control));

}

/* This functions purpose is to return void so the jumptable ptf is exact */
static void
odsy_movec(void *hw, int x, int y)
{
    odsy_positioncursor(hw, x, y);
}

void
odsy_blankscreen(void *hw, int doBlanking )
{
    int i;
    extern int odsyBlankScreen( odsy_data *h, int b );
    extern odsy_data OdsyBoards[ODSY_MAXBOARDS];

    DPRINTF(("odsy_blankscreen\r\n"));

    /* find the matching board data */
    for( i=0; i<ODSY_MAXBOARDS; i++ )
    {
        if( OdsyBoards[i].hw == (odsy_hw*) hw )
            break;
    }

    odsyBlankScreen( &OdsyBoards[i], doBlanking );
}

static void
odsy_blockm(void *hw, int oldx, int oldy, int newx, int newy, int w, int h)
{
    /* This is unused, but a p[lace holder is nice to see */
    DPRINTF(("in odsy_blockm\r\n"));
}

void
odsy_stand_tpinit(void *hw, int *x, int *y)
{
    int idx = 0;
    extern int odsy_earlyinit(void);

    DPRINTF("Entering odsy_stand_tpinit\n\r"); 

    odsy_earlyinit();
    odsy_tpinit(hw);
    odsy_init_cursor(hw);

    idx = ODSY_PIPENR((odsy_hw *) hw);

    *x = OdsyBoards[idx].info.gfx_info.xpmax;
    *y = OdsyBoards[idx].info.gfx_info.ypmax;

    /* initialize position offscreen so the cursor does not appear */
    odsy_positioncursor(hw, 1280, 0);

    DPRINTF("Leaving odsy_stand_tpinit\n\r");
}
#endif	/* _STANDALONE */

/******************************************
 * DISPATCH TABLE
 *****************************************/

#if !defined(_STANDALONE)
/*  Structure defined in htport.h  */
struct htp_fncs odsy_htp_fncs = {
    odsy_tpinit,
    odsy_mapcolor,
    odsy_color,
    odsy_sboxfi,
    odsy_pnt2i,
    odsy_drawbitmap,
    odsy_cmov2i,
    odsy_unblank,
    odsy_getdesc,
    NULL
};
#else
/*  Structure defined in stand_htport.h  */
struct htp_fncs odsy_htp_fncs = {
    odsy_blankscreen,       /* blankscreen */
    odsy_color,             /* color       */
    odsy_mapcolor,          /* mapcolor    */
    odsy_sboxfi,            /* sboxfi      */
    odsy_pnt2i,             /* pnt2i       */
    odsy_cmov2i,            /* cmove2i     */
    odsy_drawbitmap,        /* drawbitmap  */
    NULL,                   /* blockm - leave as 0 if not used */
    odsy_stand_tpinit,      /* init        */
    odsy_movec              /* movec - move cursor */
};
#endif	/* _STANDALONE */

