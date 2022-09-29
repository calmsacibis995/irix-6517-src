/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/param.h>
#include <sys/kona.h>
#include <libsc.h>
#include <stand_htport.h>
#include <ktport.h>


#define FIFO_EMPTY(hw) {				   \
  int count, i = 0;					   \
  gfxvaddr = (unsigned long *) hw;			   \
  do {							   \
    count = hh_read(HH_GFIFO_LEVEL_REG) & GFIFO_LEVEL_MASK;\
  } while (i++ < 5000 && count);			   \
}

#if 0
  do {
    count = hh_read(HH_GFIFO_LEVEL_REG) & GFIFO_LEVEL_MASK;
  } while (i++ < 20000 && count);
    ttyprintf ("FIFO_EMPTY: count = %d\n", count);
#endif

static int xsize = 1280;	/* Default screen size */
static int ysize = 1024;

/* 
 * Textport ucode entry-points
 */

/*
 * Set the current drawing color
 */
static void kona_setcolor(void *hw, int color)
{
#if 0
    ttyprintf("kona_setcolor(%d)\n", color);
    verbose("kona_setcolor(%d)\n", color);
#endif

  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_SETCOLOR);
  kfifo(color);
}


/*
 * Add an entry to the color-map
 */
static void kona_mapcolor(void *hw, int index, int r, int g, int b)
{
#if 0
    verbose("map_color(%d) %d %d %d\n", index, r, g, b);
#endif

  r <<= 2; g <<= 2; b <<= 2;	/* ucode routine expects 10-bit */
				/* color. textport has 8-bit color */
  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_MAPCOLOR);
  kfifo(index);
  kfifo(r); kfifo(g); kfifo(b);
  
  return;
}


/*
 * Draw a point in the current drawing color
 */

static void kona_point(void *hw, int x, int y)
{
#if 0
  verbose("kona_point( %x, %x)\n", x, y);
#endif

  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_POINT);
  kfifo(x);
  kfifo(y);
  
  return;
}


/*
 * Draw a filled rectangle in screen coordinates
 */
static void kona_box(void *hw, int x1, int y1, int x2, int y2)
{
#if 0
  ttyprintf("box(): %d %d %d %d\n", x1, y1, x2, y2);
#endif
  /*
   * Take care of alignments. PROM dirver expects all rectangles to be
   * inclusive in both direcftions; whereas in ucode, the left and
   * bottom co-ods are not included. Compensate for this.
   */
  if (--x1 < 0) x1 = 0;
  if (--y1 < 0) y1 = 0;

  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_BOX);
  kfifo(x1); kfifo(y1);
  kfifo(x2); kfifo(y2);

  return;
}


/*
 * Blank/Unblank the screen (0: unblank, 1: blank)
 */
static void kona_blankscreen(void *hw, int blank)
{
#if 0
  verbose("In kona_blankscreen(%d)\n", blank);
#endif

  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_BLANK_SCREEN);
  kfifo(blank);
}

/*
 * Set cursor position
 */
static void kona_setcursor(void *hw, int x, int y)
{
  FIFO_EMPTY(hw);

#ifdef DG_CURSOR_WAR
  x &= ~3;
  y &= ~3;
#endif

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_SETCURSOR);
  kfifo(x);
  kfifo(y);

  return;
}


/*
 * Move current character-position (all bitmaps are drawn relative to
 * this drawing position)
 */
static void kona_movechar(void *hw, int x, int y)
{
  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_MOVECHAR);
  kfifo(x);
  kfifo(y);
  
  return;
}

/*
 * Draw a bitmap
 */
static void kona_bitmap(void *hw, struct htp_bitmap *b)
{
  int x, y, w, off, r, i;
  int nwords = 0;
  unsigned short *s = (unsigned short *) b->buf;
  unsigned int d32[100], *buf;

  for (i = 0; i < 100; i++) 
    d32[i] = 0;

  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_BITMAP);
  kfifo(b->xsize);
  kfifo(b->ysize);
  kfifo(b->xorig);
  kfifo(b->yorig);
  kfifo(b->xmove);
  kfifo(b->ymove);

  /* Ucode reads the bitmap buffer 1 word at a time. Hence, 
   * total #words sent out to the FIFO = ((w*h +7)/8 + 3)/4
   */

  nwords = ((b->xsize * b->ysize +7)/8 + 3)/4; 

  /*
   * Pack the bits into compact 32-bit words
   */

  y = 0;
  w = b->xsize;
  off = 32;
  buf = &(d32[0]);		/* Initialize destination ptr */
  while (y < b->ysize) {
    x = 0;
    while (x < w) {
      if ((r = (w - x)) > 16)
	r = 16;
      
      if (off > 16)
	*buf |= (*s) << (off - 16);
      else 
	*buf |= (*s) >> (16 - off);
      off -= r;
      
      if (off < 0) {
	buf++;
	*buf |= (*s) << (16 + off + r);
	off = 32 + off;
      } else if (off == 0) {
	buf++;
	off = 32;
      }
      s++;
      
      x += 16;  
    }
    y++;
  }

  /*
   * Send down the packed data through the FIFO
   */

  for (i = 0; i < nwords; i++) {
    kfifo(d32[i]);
  }

  return;
}


/*
 * Initialize textport graphics. This assumes that ucode has already
 * been loaded and the HIP is ready to accept commands.
 */
static void kona_tportinit(void *hw, int *x, int *y) 
{
#if 0
  ttyprintf("In kona_tportinit(xs, ys = %d, %d)\n", xsize, ysize);
  verbose("In kona_tportinit(xs, ys = %d, %d)\n", xsize, ysize);
#endif

  *x = xsize;
  *y = ysize;

  FIFO_EMPTY(hw);
  kfifo(HP_TPORT_MAIN);
  kfifo(TP_INIT);
  kfifo(xsize);
  kfifo(ysize);

  return;
}


/*
 * Host interface to graphics hardware
 */

struct htp_fncs kona_tpfuncs = {
  kona_blankscreen,
  kona_setcolor,
  kona_mapcolor,
  kona_box,
  kona_point,
  kona_movechar,
  kona_bitmap,
  0,				/* kona_bmove not implemented yet */
  kona_tportinit,
  kona_setcursor
};


/*
 * Poll driven routine to accept data from the HIP. After putting data
 * in the mailbox register, the HIP generates ARM_INTR. This sets a
 * bit in the INTR_STATUS register which is polled here.
 *
 * Returns -1 on a timeout.
 */

int receive_armdata(int n, unsigned int *buf, int delay)
{
  int i;
  int ret = 0;

  if (!delay)
    delay = 5000;		/* default #tries. */ 

  for (i = 0; i < n; i++) {
    int status, j;
    
    /*
     * Read one word of data from the arm
     */
    status = j = 0;
    do {
      status = hh_read(HH_INTR_STATUS_REG) & INTR_BIT_ARM_MASK;

	sginap(2);

	j++;
    } while (j < delay && !status);

    if (status) {
      *buf++ = hh_read(HH_ARM_MBOX_REG);
      ret++;
      
      hh_write(HH_ARM_MBOX_REG, 0x0);
      
      hh_write(HH_INTR_STATUS_REG, INTR_BIT_ARM_MASK);
    } else {
#if 0
      verbose("Timeout reading ARM data\n");
#endif
      return -1;
    }
  }
  
  return ret;
}

/*
 * Check if the graphics pipe is alive. Send check_pipe_sync command
 * and poll the mailbox to look for the success code/
 */
int kona_alive(void *gfxbase, int delay)
{
  unsigned int status = 0;
  void *hw = (void *) gfxbase;

#if 0
  verbose("In kona_alive()\n");
#endif

  FIFO_EMPTY(hw);
#if 0
   kona_debug_enable();
#endif


  hh_write(HH_ARM_MBOX_REG, 0x0);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_CHKPIPESYNC);
  kfifo(0);

  sginap(10);
  if (receive_armdata(1, &status, delay) < 0) {

#if 0
    verbose("pipe not alive\n");
#endif
    return -1;
  }
  else if (status == TP_SUCCESS) {

#if 0
    verbose("pipe alive\n");
#endif
    return 0;
  }
  else 
    return -1;
  
}


/*
 * Communicate with the pipe to get graphics information. Store screen
 * size, and call the textport initialization routine in ucode
 */
int kona_initpipe(void *hw)
{
  struct {
    int numges, numrms, nchannels;
    int xsize, ysize;
  } info_arg;

  FIFO_EMPTY(hw);

  kfifo(HP_TPORT_MAIN);
  kfifo(TP_GFXINFO);
  kfifo(0);
  
  sginap(200);
  if (receive_armdata(5, (unsigned int *) &info_arg, 0) <= 0)
    return -1;			/* pipe dead */

  xsize = info_arg.xsize;
  ysize = info_arg.ysize;

  if (xsize < 0)		/* Discard outrageous values */
    xsize = 1280;
  if (ysize < 0)
    ysize = 1024;

  return 0;
}

void kona_debug_enable(void)
{
  kfifo(HP_TPORT_MAIN);
  kfifo(TP_DEBUG);
  kfifo(1);
}

