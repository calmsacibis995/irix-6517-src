/*
 * venice_tp.c
 */

#ident "$Revision: 1.1 $"

#include "sys/types.h"
#include "sys/sema.h"
#include "arcs/hinv.h"
#include "arcs/folder.h"
#include "arcs/cfgdata.h"
#include "arcs/cfgtree.h"
#include "sys/gfx.h"
#include "sys/rrm.h"
#include "sys/venice.h"
#include "stand_htport.h"
#include "VENICE/gl/cpcmds.h"
#include "VENICE/gl/cpcmdstr.h"
#include "sys/mips_addrspace.h"
#if       defined(IP19)
#include "sys/EVEREST/IP19.h"
#elif defined(IP21)
#include "sys/EVEREST/IP21.h"
#elif defined(IP25)
#include "sys/EVEREST/IP25.h"
#endif
#include "wg.h"

static void veniceTpBlankscreen(void *,int);
static void veniceTpColor(void *,int);
static void veniceTpMapcolor(void *, int, int, int, int);
static void veniceTpSboxfi(void *, int, int, int, int);
static void veniceTpPnt2i(void *, int, int);
static void veniceTpCmov2i(void *, int, int);
void veniceTpDrawBitmap(void *, struct htp_bitmap *);
static void veniceTpInit(void *hw, int *, int *);
static void veniceTpMovec(void *hw, int, int);


extern volatile struct fcg *fcg_base_va;


#ifdef IP19
#define gecmdstore(cmd, val) { SETUP_GE; GE[cmd].i = val; } 
#define gestore(cmd, val)    { SETUP_GE; GE[cmd].i = val; } 
#define wg_flush()           { SETUP_GE; GE[CP_WG_FLUSH_0].i = 0; }
#endif

#ifndef DEBUG
#define xprintf (void)
#else
#define xprintf ttyprintf
#endif

extern void sginap(int ticks);
extern void venice_move_cursor(int x, int y);
extern void venice_set_vcreg(int addr, int data, int fifo);
extern void ttyprintf(char *fmt, ...);

#define SETUP_REGS	/* Assume pipe 0 */	\
			volatile struct fcg *fcg = fcg_base_va

/*
 * Normally checking for FIFO not full is adequate, but
 * there is a chip bug in that only the low eight bits
 * of the gfxword count is readable, so the easiest thing
 * is just wait for fifo to empty.
 */
#define FIFO_WAIT	{ SETUP_REGS; int i=0; \
			  while((fcg->gfxwords > 0xff0) && (i<200)) { sginap(1); i++;}\
			  if (i >= 200) xprintf("FIFO_WAIT timed out\n"); }

#define FIFO_EMPTY	FIFO_WAIT

/*
 * Turn on/off video.
 */
/* ARGSUSED */
static void veniceTpBlankscreen(void *hw, int blank)
{
	extern int vof_control_reg_value;
	
	if (blank) {
	  	/* Blank the screen */
	  	vof_control_reg_value |= DG_VOF_BLANK_SCREEN;
	}
	else {
		/* unblank the screen */
		vof_control_reg_value &= ~DG_VOF_BLANK_SCREEN;
	}

	venice_set_vcreg(V_DG_VOF_CNTRL, (vof_control_reg_value & 0xfe), 0);

}

/*
 * Set the current colour.
 */
/* ARGSUSED */
static void veniceTpColor(void *hw, int color)
{

	gecmdstore( CP_ARCS_COLOR, color)  ;

	wg_flush();
}

/*
 * Create colour map in the XMAP.
 */
/* ARGSUSED */
static void veniceTpMapcolor(void *hw, int index, int red, int green, int blue)
{
	FIFO_EMPTY;
	gecmdstore( CP_ARCS_MAPCOLOR, index)  ;
	gestore( CP_DATA, red)  ;
	gestore( CP_DATA, green)  ;
	gestore( CP_DATA, blue)  ;

	wg_flush();
}

/*
 * Draw a filled, screen aligned rectangle.
 */
/* ARGSUSED */
static void veniceTpSboxfi(void *hw, int x1, int y1, int x2, int y2)
{
/* xprintf("veniceTpSboxfi(%d %d %d %d)\n",x1,y1, x2,y2);  */
	FIFO_EMPTY;
	gecmdstore( CP_ARCS_SBOXFI, x1)  ;
	gestore( CP_DATA, y1)  ;
	gestore( CP_DATA, x2)  ;
	gestore( CP_DATA, y2)  ;
	wg_flush();



}

/*
 * Move the character position.
 */
/* ARGSUSED */
static void veniceTpCmov2i(void *hw, int x, int y)
{
/* xprintf("veniceTpCmov2i(%d %d)\n",x,y); */
	FIFO_EMPTY;
	gecmdstore( CP_ARCS_CMOV2I, x)  ;
	gestore( CP_DATA, y)  ;

	wg_flush();
}

/*
 * Draw a point.
 */
/* ARGSUSED */
static void veniceTpPnt2i(void *hw, int x, int y)
{
/* xprintf("veniceTpPnt2i(%d %d)\n",x,y); */
	FIFO_EMPTY;
	gecmdstore( CP_ARCS_PNT2I, x)  ;
	gestore( CP_DATA, y)  ;
	wg_flush();

}

/*
 * Draw a character bitmap.
 */
/* ARGSUSED */
void
veniceTpDrawBitmap(void *hw, struct htp_bitmap *bitmap)
{

    unsigned short *sptr = bitmap->buf;
    int xpos, ypos;
    int x, y;
    int i, sw;
    int sper = bitmap->sper;

    FIFO_EMPTY;

/* xprintf("veniceTpDrawBitmap()\n"); */
#define	XMAXCHAR	32
#define	XMAXCHAROVER2	(XMAXCHAR>>1)
#define	YMAXCHAR	28
#define	YMAXCHAROVER2	(YMAXCHAR>>1)

#if defined(IP19) || defined(IP21) || defined(IP25)
#define	CP_CHAR_CMD(wide,xs,ys,xp,yp,cnt) \
	gecmdstore( CP_CHARACTER, wide)  ; \
	gestore( CP_DATA, xs)  ; \
	gestore( CP_DATA, ys)  ; \
	gestore( CP_DATA, xp)  ; \
	gestore( CP_DATA, yp)  ; \
	gecmdstore( CP_SET_COUNT, cnt)  

	wg_flush();   /* XXX temporary */
#else
#define	CP_CHAR_CMD(wide,xs,ys,xp,yp,cnt) \
	gecmdstore( CP_CHARACTER, wide)  ; \
	gestore( CP_DATA, xs)  ; \
	gestore( CP_DATA, ys)  ; \
	gestore( CP_DATA, xp)  ; \
	gestore( CP_DATA, yp)  
#endif

    /* send a token down to indicate start drawing the bitmap */
    gecmdstore( CP_BEGINSTRING, 0)  ;

    /* this loop draws the portion whose width exceeds one short word */
    for ( x=bitmap->xsize,xpos=-bitmap->xorig,sw=0 ;
	  x>XMAXCHAROVER2 ;
	  x-=XMAXCHAR,xpos+=XMAXCHAR,sw+=2 )  {
	for ( y=bitmap->ysize,ypos=-bitmap->yorig,sptr=bitmap->buf+sw ;
		y>=YMAXCHAR ;
		y-=YMAXCHAR,ypos+=YMAXCHAR )  {
	    /* output a char command */
	    CP_CHAR_CMD(1, XMAXCHAR, YMAXCHAR, xpos, ypos, YMAXCHAR);

	    /* output data */
	    FIFO_EMPTY;
	    for ( i=YMAXCHAR ; i-- ; sptr+=sper )  {
	        int tmp;

		tmp = (*sptr << 16) | *(sptr+1);
		gestore( CP_DATA, tmp);
		if ((i % 8) == 0)   /* XXX temporary */
		  wg_flush(); 
	    }
	}

	/* check if there is an extra character here */
	if  (y > 0) {
	    /* output a char command */
	    CP_CHAR_CMD(1, XMAXCHAR, y, xpos, ypos, YMAXCHAR);
	    FIFO_EMPTY;
	    /* output data */
	    for ( i=y ; i-- ; sptr+=sper )  {
	        int tmp;

		tmp = (*sptr << 16) | *(sptr+1);
		gestore( CP_DATA, tmp);
		if ((i % 8) == 0)   /* XXX temporary */
		  wg_flush(); 
	    }
	    for ( i=YMAXCHAR-y ; i-- ; )  {
		gestore( CP_DATA, 0)  ;
		if ((i % 8) == 0)   /* XXX temporary */
		  wg_flush(); 
	    }
	}
    }

    /*
     * check if there is an extra column of characters here.
     * if there is, it draws the portion whose width is under
     * one short word.
     */
    if  (x > 0) {
	for ( y=bitmap->ysize,ypos=-bitmap->yorig,sptr=bitmap->buf+sw ;
		y>=YMAXCHAR ;
		y-=YMAXCHAR,ypos+=YMAXCHAR )  {
	    /* output a char command */
	    CP_CHAR_CMD(0, x, YMAXCHAR, xpos, ypos, YMAXCHAROVER2);
	    FIFO_EMPTY;
	    /* output data */
	    for ( i=YMAXCHAROVER2 ; i-- ; sptr+=(sper<<1) )  {
	        int tmp;

		tmp = (*sptr << 16) | *(sptr+sper);
		gestore( CP_DATA, tmp);
		if ((i % 8) == 0)      /* XXX temporary */
		  wg_flush();      
	    }
	}

	/* check if there is an extra character here */
	if  (y > 0) {
	    /* output a char command */
	    CP_CHAR_CMD(0, x, y, xpos, ypos, YMAXCHAROVER2);
	    FIFO_EMPTY;
	    /* output data */
	    for ( i=y ; i>=2 ; i-=2,sptr+=(sper<<1) )  {
	        int tmp;

		tmp = (*sptr << 16) | *(sptr+sper);
		gestore( CP_DATA, tmp);
		if ((i % 8) == 0)   /* XXX temporary */
		  wg_flush();
	    }

	    if  (i > 0)  {
	        int tmp;

		tmp = *sptr << 16;
		gestore( CP_DATA, tmp);
		i = (YMAXCHAR - y - 1) >> 1;
	    }
	    else
		i = (YMAXCHAR - y) >> 1;

	    for ( ; i-- ; )  {
		gestore( CP_DATA, 0)  ;
	    }
	}
    }

    /* send a token down to signal end of the bitmap */
    gecmdstore( CP_ENDSTRING, bitmap->xmove)  ;
    gestore( CP_DATA, bitmap->ymove)  ;
    wg_flush();

}

/*
 * Initialize the textport.
 */
/* ARGSUSED */
static void veniceTpInit(void *hw, int *x, int *y)
{
	extern int xsize, ysize;

/* xprintf("veniceTpInit()\n"); */
	*x = xsize;
	*y = ysize;
}

/*
 * Move cursor.
 */
/* ARGSUSED */
static void veniceTpMovec(void *hw, int x, int y)
{
/* xprintf("veniceTpMovec(%d, %d)\n",x,y); */
	venice_move_cursor(x, y);
	wg_flush();
}

/*
 * Rectcopy.
 */
/* ARGSUSED */
#if 0
static void veniceTpBmove(void *hw, int oldx, int oldy, int newx, int newy, int w, int h)
{

/* xprintf("veniceTpBmove(%d %d %d %d %d %d)\n",oldx,oldy,newx,newy,w,h); */
}
#endif


/*****************************************************************************
 * Host Textport routines.  This is the interface with htport.
 *****************************************************************************/
struct htp_fncs venice_tp_fncs = {
	veniceTpBlankscreen,
	veniceTpColor,
	veniceTpMapcolor,
	veniceTpSboxfi,
	veniceTpPnt2i,
	veniceTpCmov2i,
	veniceTpDrawBitmap,
	/*veniceTpBmove,*/0,
	veniceTpInit,
	veniceTpMovec
};
