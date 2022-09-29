/*
 *	ipaste - 
 *		Display an image on the iris screen.
 *
 *			Paul Haeberli - 1984	
 *
 */
#include <gl/gl.h>
#include <gl/device.h>
#include "image.h"
#include "port.h"
#include "dispimg.h"
#include "sys/mgrashw.h"
#include "mco_diag.h"
#include "ide_msg.h"

#define XBORDER		8
#define TOPBORDER	8
#define BOTBORDER	40
#define MINWINXSIZE     75

/* 1/4 of 1280X1024 24-bit image */
#define NTSC_FBSIZE	(512*640*3)

typedef struct PASTEIMAGE {
    int xsize, ysize, zsize;
    int xorg, yorg;
    int sxmode, dpp;
    int xframe, yframe;
    int xdraw, ydraw;
    DISPIMAGE *di;
} PASTEIMAGE;

PASTEIMAGE *pi;
PASTEIMAGE *readit();
int drawit();
int drawfunc();

int xmaxscreen;
int ymaxscreen;

extern __uint32_t *DMA_WRTile_notaligned;
extern __uint32_t *DMA_WRTile;

void foreground(void)
{
    msg_printf(DBG, "foreground() dummy called.\n");
}

void noborder(void)
{
    msg_printf(DBG, "noborder() dummy called.\n");
}

__uint32_t
mco_ipaste(__uint32_t argc, char **argv)
{
    register int i;
    short val;
    int wx, wy, preforg, sxmode;
    char *inimg;

    if( argc<2 ) {
	msg_printf(SUM,"Usage: mco_ipaste [-sx] [-o xorg yorg] inimage\n");
	return(1);
    } 
    xmaxscreen = 1279;
    ymaxscreen = 1023;
    sxmode = 0;
    preforg = 0;
    wx =0; wy=0;
    for(i=1; i<argc; i++) {
	if (argv[i][0] == '-') {
	    if(strcmp(argv[i],"-sx") == 0)
		sxmode++;
#ifdef ORIG
	    if(strcmp(argv[i],"-f") == 0)
		foreground();
	    if(strcmp(argv[i],"-n") == 0)
		noborder();
#endif /* ORIG */
	    if(strcmp(argv[i],"-o") == 0) {
		i++;
		wx = atoi(argv[i]);
		i++;
		wy = atoi(argv[i]);
		preforg = 1;
	    }
	} else {      /* azzume we've got a legit image file at this point */
	    inimg = argv[i];
	    sprintf(ipaste_filename, "%s", &argv[i]);
	}
    }
    pi = readit(ipaste_filename,sxmode,wx,wy,preforg);
    drawfunc(drawit);
}

/* readit() is based on readit from gfx/lib/XXX.c
 * 
 * readit does the following:
 * 	open the file
 *	read the header
 * 	check the header for file type (Magic number)
 * 	
 */
PASTEIMAGE *readit(filename,sxmode,wx,wy,preforg)
char *filename;
int sxmode, wx, wy, preforg;
{
    register IMAGE *image;
    register PASTEIMAGE *pi;
    register int y;
    register short *srow;
    unsigned char *rrow, *grow, *brow;

/* allocate the image struct, and open the input file */
    pi = (PASTEIMAGE *)malloc(sizeof(PASTEIMAGE));
    msg_printf(VRB, "readit: calling iopen()\n");
    if( (image=iopen(filename,"r")) == NULL ) {
	msg_printf(SUM,"mco_ipaste: can't open input file %s\n",filename);
	return(0);
    }

/* calculate the window size */
    pi->xsize = image->xsize;
    pi->ysize = image->ysize;
    msg_printf(VRB, "readit: pi->xsize = %d\tpi->ysize = %d\n", pi->xsize, pi->ysize);
    if(sxmode) {
	pi->sxmode = 1;
	pi->xframe = pi->xsize+XBORDER+XBORDER;
	pi->yframe = pi->ysize+BOTBORDER+TOPBORDER;
	if(pi->xframe>xmaxscreen+1)
	    pi->xframe = xmaxscreen+1;
	if(pi->yframe>ymaxscreen+1)
	    pi->yframe = ymaxscreen+1;
 	pi->xdraw = pi->xframe-XBORDER-XBORDER;
 	pi->ydraw = pi->yframe-BOTBORDER-TOPBORDER;
	pi->xorg = XBORDER;
	pi->yorg = BOTBORDER;
	noborder();
    } else if (pi->xsize < MINWINXSIZE) {
	pi->sxmode = 0;
	pi->xframe = MINWINXSIZE;
	pi->yframe = pi->ysize;
	if(pi->yframe>ymaxscreen+1)
	    pi->yframe = ymaxscreen+1;
 	pi->xdraw = pi->xsize;
 	pi->ydraw = pi->ysize;
	pi->xorg = (MINWINXSIZE - pi->xdraw) / 2;
	pi->yorg = 0;
    } else {
	pi->sxmode = 0;
	pi->xframe = pi->xsize;
	pi->yframe = pi->ysize;
	if(pi->xframe>xmaxscreen+1)
	    pi->xframe = xmaxscreen+1;
	if(pi->yframe>ymaxscreen+1)
	    pi->yframe = ymaxscreen+1;
 	pi->xdraw = pi->xframe;
 	pi->ydraw = pi->yframe;
#ifdef ORIG
	pi->xorg = 0;
	pi->yorg = 0;
#endif /* ORIG */
	pi->xorg = wx;
	pi->yorg = wy;
    }
    msg_printf(VRB, "readit: pi->sxmode = %d\n", pi->sxmode);
    msg_printf(VRB, "readit: pi->xframe = %d\n", pi->xframe);
    msg_printf(VRB, "readit: pi->yframe = %d\n", pi->yframe);
    msg_printf(VRB, "readit: pi->xdraw = %d\n", pi->xdraw);
    msg_printf(VRB, "readit: pi->ydraw = %d\n", pi->ydraw);
    msg_printf(VRB, "readit: pi->xorg = %d\n", pi->xorg);
    msg_printf(VRB, "readit: pi->yorg = %d\n", pi->yorg);

#ifdef NOTYET
/* open the window */
    if(preforg) {
	prefposition(wx,wx+pi->xframe-1,wy,wy+pi->yframe-1);
	prefsize(pi->xframe,pi->yframe);
	winopen("ipaste");
	wintitle(filename);
    } else {
	prefsize(pi->xframe,pi->yframe);
	winopen("ipaste");
	wintitle(filename);
    }
#endif /* NOTYET */

/* set the display mode for the image */

    setimagemode(image);
    grey(0.5);
#ifdef NOTYET
    clear();
#endif /* NOTYET */
    makeframe(pi);
    pi->di = makedisprgn(image,0,pi->xdraw-1,0,pi->ydraw-1,1,pi->xorg,pi->yorg);
    iclose(image);
    return pi;
}

drawit()
{
#ifdef NOTYET
    if(pi->xorg>0) {
        grey(0.5);
        clear();
    }
#endif /* NOTYET */
    makeframe(pi);
    drawimage(pi->di,pi->xorg,pi->yorg);
}

makeframe(pi)
PASTEIMAGE *pi;
{
    long currentcolor = 0;
#ifdef NOTYET
    reshapeviewport();
    viewport(0,pi->xframe-1,0,pi->yframe-1);
    ortho2(-0.5,pi->xframe-0.5,-0.5,pi->yframe-0.5);
#endif /* NOTYET */
    if(pi->sxmode) {
#ifdef ORIG
	rectfi(0,0,pi->xframe-1,BOTBORDER-1);
	rectfi(0,0,XBORDER-1,pi->yframe-1);
	rectfi(pi->xframe-XBORDER,0,pi->xframe-1,pi->yframe-1);
	rectfi(0,pi->yframe-TOPBORDER,pi->xframe-1,pi->yframe-1);
#endif /* ORIG */

#ifdef ORIG
	recti(XBORDER-1,BOTBORDER-1,pi->xframe-XBORDER,pi->yframe-TOPBORDER);
#endif /* ORIG */
	currentcolor = grey(0.0);
#ifdef ORIG
        recti(0,0,pi->xframe-1,pi->yframe-1);
#endif /* ORIG */
    }
}
