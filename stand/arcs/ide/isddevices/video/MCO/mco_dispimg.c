/*
 *	dispimg -
 *		General package for the display of images on all machines.
 *	This package deals with the problems of dithering RGB images on
 *	to the screens of all IRISes from the very low end to the 
 *	high end.  This is NOT what you need if you want to display
 *	iris images on a machine with RGBmode().  Take a look at
 *	imgread.c
 *
 *				Paul Haeberli - 1988
 */
#include <sys/types.h>
#include "gl.h"
#include "get.h"
#include "dispimg.h"
#include "image.h"
#include "gfxmach.h"
#include <ide_msg.h>

extern unsigned char red_map[256];
extern unsigned char green_map[256];
extern unsigned char blue_map[256];
extern unsigned char red_inverse[256];
extern unsigned char green_inverse[256];
extern unsigned char blue_inverse[256];
extern unsigned char grey_inverse[256];

/* kinds of image files */

#define BWIMG		0
#define RGBIMG		1
#define SCREENIMG	2

static prefmode[10][3] = {
/* 	     BW		     RGB	   SCREEN		 	*/
	DI_WRITEPIXELS,	DI_WRITEPIXELS, DI_WRITEPIXELS, /* MACH3D 	*/
	DI_WRITEPIXELS,	DI_WRITERGB,	DI_WRITEPIXELS, /* MACH4D 	*/
	DI_LRECTWRITE,	DI_LRECTWRITE,	DI_RECTWRITE, 	/* MACH4DGT 	*/
	DI_WRITEPIXELS,	DI_LRECTWRITE,	DI_WRITEPIXELS,	/* MACH4DPI 	*/
	DI_WRITEPIXELS,	DI_WRITEPIXELS,	DI_WRITEPIXELS,	/* MACH4D8 	*/
	DI_LRECTWRITE,	DI_LRECTWRITE,	DI_RECTWRITE, 	/* MACH4DVGX 	*/
	DI_LRECTWRITE,	DI_LRECTWRITE,	DI_RECTWRITE, 	/* MACH4DLIGHT 	*/
        DI_LRECTWRITE,  DI_LRECTWRITE,  DI_RECTWRITE,   /* MACH4DXG     */
        DI_LRECTWRITE,  DI_LRECTWRITE,  DI_RECTWRITE,   /* MACH4DRE     */
	DI_LRECTWRITE,	DI_LRECTWRITE,	DI_RECTWRITE, 	/* MACH4DNP 	*/
};

#define XSIZE	4
#define YSIZE	4
#define TOTAL		(XSIZE*YSIZE)
#define WRAPY(y)	((y)%YSIZE)
#define WRAPX(x)	((x)%XSIZE)

static short dithmat[YSIZE][XSIZE] = {
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5,
};

static int imgtype;
static short **bwtab;
static short **rtab;
static short **gtab;
static short **btab;

#ifdef NOTYET
static inchack()	/* just to get externs defined */
{
     rgbi(0,0,0);
}
#endif /* NOTYET */

short **makedittab(int levels,int mult,int add)
{
    register int val;
    register int nshades;
    register int i, j, k;
    register int matval, tabval;
    short **tab;

    nshades = XSIZE*YSIZE*(levels-1)+1;
    tab = (short **)malloc(YSIZE*sizeof(short *));
    for(j=0; j<YSIZE; j++) {
	tab[j] = (short *)malloc(XSIZE*256*sizeof(short));
	for(i=0; i<XSIZE; i++ ) {
	    matval = dithmat[i][j]; 
	    for(k=0; k<256; k++) {
		val = (nshades*k)/255;
		if(val==nshades)
		    val = nshades-1;
		if((val%TOTAL)>matval) 
		    tabval =  (val/TOTAL)+1;
		else 
		    tabval = (val/TOTAL);
		tabval *= mult;
		tabval += add;
		tab[j][256*i+k] = tabval;
	    }
	}
    }
    return tab;
}

int
imagetype(IMAGE *image)
{
    if(image->zsize>=3)
	return RGBIMG;
    if(image->colormap == CM_SCREEN)
	return SCREENIMG;
    else
	return BWIMG;
}

setimagemode(IMAGE *image)
{
    int type, dispmode, curdispmode;

    type = imagetype(image);
    dispmode = prefdrawmode(type);
#ifdef ORIG
    curdispmode = getdisplaymode();
#endif /* ORIG */
    curdispmode = DMRGBDOUBLE;
#ifdef ORIG
    switch(dispmode) {
   	case DI_WRITERGB:
   	case DI_LRECTWRITE:
	    if(curdispmode != DMRGB && curdispmode != DMRGBDOUBLE)
		pseudorgb();
	    break;
   	case DI_WRITEPIXELS:
   	case DI_RECTWRITE:
	    if(curdispmode != DMSINGLE && curdispmode != DMDOUBLE) {
		singlebuffer();
		gconfig();
	    }
	    break;
    }
#endif /* ORIG */
}

prefdrawmode(type)
int type;
{
    int val, mach; 
    
#ifdef ORIG
    mach = gfxmachine();
#endif /* ORIG */
    mach = MACH4DNP;
    if(mach>MACH4DNP)
	mach = MACH4DNP;
    val = prefmode[mach][type];
    return val;
}

drawimage(di,xorg,yorg)
DISPIMAGE *di;
int xorg, yorg;
{
    register int y, step;
    register unsigned short *sptr;
    register unsigned char *rptr, *gptr, *bptr;
#ifdef ORIG
    short vpx1, vpx2, vpy1, vpy2;
#endif /* NOTYET */
    int vpx1, vpx2, vpy1, vpy2;

#ifdef ORIG
    getviewport(&vpx1,&vpx2,&vpy1,&vpy2);
#endif /* ORIG */
    vpx1 = 0;
    vpx2 = di->xsize;
    vpy1 = 0;
    vpy2 = di->ysize;

    switch(di->type) {
	case DI_WRITEPIXELS:
	    sptr = (unsigned short *)di->data;
#ifdef ORIG
            pushmatrix();
	    ortho2(-0.5,(vpx2-vpx1)+0.5,-0.5,(vpy2-vpy1)+0.5);
	    for(y=0; y<di->ysize; y++) {
		cmov2i(xorg,yorg+y);
		writepixels(di->xsize,sptr);
		sptr += di->xsize;
	    }
            popmatrix();
#endif /* ORIG */
	    break;
	case DI_WRITERGB:
	    rptr = di->data;
	    gptr = rptr+di->xsize;
	    bptr = gptr+di->xsize;
	    step = 3*di->xsize;
#ifdef ORIG
            pushmatrix();
	    ortho2(-0.5,(vpx2-vpx1)+0.5,-0.5,(vpy2-vpy1)+0.5);
	    for(y=0; y<di->ysize; y++) {
		cmov2i(xorg,yorg+y);
		writeRGB(di->xsize,rptr,gptr,bptr);
		rptr += step;
		gptr += step;
		bptr += step;
	    }
            popmatrix();
#endif /* ORIG */
	    break;
	case DI_RECTWRITE:
#ifdef ORIG
	    rectwrite(xorg+vpx1,yorg+vpy1,
		      	xorg+di->xsize-1,yorg+di->ysize-1,(unsigned short *)di->data);
#endif /* ORIG */
	    break;
	case DI_LRECTWRITE:
#ifdef ORIG
	    lrectwrite(xorg+vpx1,yorg+vpy1,
		   	xorg+di->xsize-1,yorg+di->ysize-1,(unsigned long *)di->data);
#endif /* ORIG */
	    mco_lrectwrite(xorg+vpx1,yorg+vpy1,
		   	xorg+di->xsize-1,yorg+di->ysize-1,(__uint32_t *)di->data);
	    break;
    }
}

DISPIMAGE *makedisprgn(image,x1,x2,y1,y2,show,xorg,yorg)
IMAGE *image;
unsigned int x1, x2, y1, y2;
int show, xorg, yorg;
{
    DISPIMAGE *di;
    int xsize, ysize, y;
    int imgtype;
    register unsigned char *cptr;
    register unsigned short *sptr;
    register __uint32_t *lptr;
    register short *sbuf, *rbuf, *gbuf, *bbuf;
    short vpx1, vpx2, vpy1, vpy2;

    msg_printf(VRB, "makedisprgn: allocating %d bytes for di\n", sizeof(DISPIMAGE));
    di = (DISPIMAGE *)malloc(sizeof(DISPIMAGE));
    xsize = x2-x1+1;
    ysize = y2-y1+1;
    di->xsize = xsize;
    di->ysize = ysize;
    imgtype = imagetype(image);
    di->type = prefdrawmode(imgtype);
    switch(di->type) {
	case DI_WRITEPIXELS:
	case DI_RECTWRITE:
	    msg_printf(VRB, "makedisprgn: allocating %d bytes for di->data\n", 
	    xsize*ysize*sizeof(short));
	    di->data = (unsigned char *)malloc(xsize*ysize*sizeof(short));
	    break;
	case DI_WRITERGB:
	    msg_printf(VRB, "makedisprgn: allocating %d bytes for di->data\n", 
	    xsize*ysize*sizeof(short));
	    di->data = (unsigned char *)malloc(xsize*ysize*3);
	    break;
	case DI_LRECTWRITE:
#ifdef NOTYET
	    di->data = (unsigned char *)malloc(xsize*ysize*sizeof(long));
#endif /* NOTYET */
	    msg_printf(VRB, "makedisprgn: get_chunk %d bytes for di->data\n", 
	    xsize*ysize*sizeof(__uint32_t));
	    di->data = (unsigned char *)get_chunk(xsize*ysize*sizeof(__uint32_t));
	    break;
    }
    if(!di->data) {
#ifdef ORIG
	 fprintf(stderr,"makedisprgn: out of memory\n");
#endif /* ORIG */
	 msg_printf(SUM, "makedisprgn: out of memory\n");
	 return 0;
    }

#ifdef ORIG
    if(show) {
        getviewport(&vpx1,&vpx2,&vpy1,&vpy2);
	pushmatrix();
	ortho2(-0.5,(vpx2-vpx1)+0.5,-0.5,(vpy2-vpy1)+0.5);
    }
#endif /* ORIG */
    if(show) {
	vpx1 = x1;
	vpx2 = x2;
	vpy1 = y1;
	vpy2 = y2;
    }
    switch(imgtype) {
	case BWIMG:
	    switch(di->type) {
	 	case  DI_WRITEPIXELS:
		    sptr = (unsigned short *)di->data;
#ifdef ORIG
		    sbuf = (short *)malloc(image->xsize*sizeof(short));
		    for(y=0; y<ysize; y++) {
			getrow(image,sbuf,y+y1,0);
			bwtowp(sbuf+x1,sptr,xsize,y);
			if(show) {
			    cmov2i(xorg,yorg+y);
			    writepixels(xsize,sptr);
			}
			sptr += xsize;
		    }
		    free(sbuf);
#endif /* ORIG */
		    break;
		case  DI_LRECTWRITE:
		    lptr = (__uint32_t *)di->data;
	            rbuf = (short *)malloc(image->xsize*sizeof(short));
		    for(y=0; y<ysize; y++) {
			getrow(image,rbuf,y+y1,0);
			bwtocpack(rbuf+x1,lptr,xsize);
		        if(show) {
			    stoc(rbuf,rbuf,xsize);
#ifdef ORIG
		            cmov2i(xorg,yorg+y);
		            writeRGB(xsize,((unsigned char*)rbuf)+x1,
				    	   ((unsigned char*)rbuf)+x1,
				    	   ((unsigned char*)rbuf)+x1);
#endif /* ORIG */
		        }
			lptr += xsize;
		    }
		    free(rbuf);
		    break;
	    }
	    break;
	case RGBIMG:
	    switch(di->type) {
#ifdef ORIG
		case DI_WRITEPIXELS:
		case DI_RECTWRITE:
		    sptr = (unsigned short *)di->data;
	            rbuf = (short *)malloc(image->xsize*sizeof(short));
	            gbuf = (short *)malloc(image->xsize*sizeof(short));
	            bbuf = (short *)malloc(image->xsize*sizeof(short));
		    for(y=0; y<ysize; y++) {
			getrow(image,rbuf,y+y1,0);
			getrow(image,gbuf,y+y1,1);
			getrow(image,bbuf,y+y1,2);
			rgbtowp(rbuf+x1,gbuf+x1,bbuf+x1,sptr,xsize,y);
		        if(show) {
		            cmov2i(xorg,yorg+y);
		            writepixels(xsize,sptr);
		        }
			sptr += xsize;
		    }
		    free(rbuf);
		    free(gbuf);
		    free(bbuf);
		    break;
#endif /* ORIG */
		case DI_WRITERGB:
		    cptr = di->data;
		    sbuf = (short *)malloc(image->xsize*sizeof(short));
		    for(y=0; y<ysize; y++) {
			getrow(image,sbuf,y+y1,0);
			stoc(sbuf+x1,cptr,xsize);
			cptr += xsize;
			getrow(image,sbuf,y+y1,1);
			stoc(sbuf+x1,cptr,xsize);
			cptr += xsize;
			getrow(image,sbuf,y+y1,2);
			stoc(sbuf+x1,cptr,xsize);
			cptr += xsize;
#ifdef ORIG
		        if(show) {
		            cmov2i(xorg,yorg+y);
		            writeRGB(xsize,cptr-3*xsize,
						 cptr-2*xsize,cptr-1*xsize);
		        }
#endif /* ORIG */
		    }
		    free(sbuf);
		    break;
		case DI_LRECTWRITE:
		    lptr = (__uint32_t *)di->data;
	            rbuf = (short *)malloc(image->xsize*sizeof(short));
	            gbuf = (short *)malloc(image->xsize*sizeof(short));
	            bbuf = (short *)malloc(image->xsize*sizeof(short));
		    for(y=0; y<ysize; y++) {
			getrow(image,rbuf,y+y1,0);
			getrow(image,gbuf,y+y1,1);
			getrow(image,bbuf,y+y1,2);
			rgbtocpack(rbuf+x1,gbuf+x1,bbuf+x1,lptr,xsize);
		        if(show) {
#ifdef ORIG
			    stoc(rbuf+x1,rbuf,xsize);
			    stoc(gbuf+x1,gbuf,xsize);
			    stoc(bbuf+x1,bbuf,xsize);
		            cmov2i(xorg,yorg+y);
		            writeRGB(xsize,(unsigned char*)rbuf,
			       (unsigned char *)gbuf,(unsigned char *)bbuf);
#endif /* ORIG */
			    msg_printf(VRB,"makedisprgn: calling mco_lrectwrite(0x%x, 0x%x, 0x%x, 0x%x, lptr)\n",
			    x1,y+y1,xsize,y+y1);
			    mco_lrectwrite(x1,y+y1,xsize,y+y1,lptr);
		        }
			lptr += xsize;
		    }
		    free(rbuf);
		    free(gbuf);
		    free(bbuf);
		    break;
	    }
	    break;
#ifdef ORIG
	case SCREENIMG:
	    sptr = (unsigned short *)di->data;
	    sbuf = (short *)malloc(image->xsize*sizeof(short));
	    for(y=0; y<ysize; y++) {
		getrow(image,sbuf,y+y1,0);
		wptowp(sbuf+x1,sptr,xsize);
		if(show) {
		    cmov2i(xorg,yorg+y);
		    writepixels(xsize,(unsigned short *)sbuf+x1);
		}
		sptr += xsize;
	    }
	    free(sbuf);
	    break;
#endif /* ORIG */
    }
#ifdef ORIG
    if(show) 
	popmatrix();
#endif /* ORIG */
    return di;
}

DISPIMAGE *makedisp(image)
IMAGE *image;
{
    return makedisprgn(image,0,image->xsize-1,0,image->ysize-1,0,0,0);
}

#ifdef NOTYET
bwtowp(bw,wp,n,y)
register unsigned short *bw, *wp;
register int n, y;
{
    register short *bwbase;
    register int *maptab;
    int i, cur, nshades;
    int ix, iy;

    if(!bwtab) {
#ifdef ORIG
	if(gfxmachine() == MACH3D) {
	    bwtab = makedittab(128,1,128);
	} else {
#endif /* ORIG */
	    maptab = (int *)malloc(256*sizeof(int));
	    cur = 100000;
	    nshades = 0;
	    for(i=0; i<256; i++) {
		if(grey_inverse[i] != cur) {
		    cur = grey_inverse[i];
		    maptab[nshades] = cur;
		    nshades++;
		}
	    }
	    bwtab= makedittab(nshades,1,0);
	    for(iy=0; iy<YSIZE; iy++) {
		for(ix=0; ix<XSIZE; ix++) {
		    bwbase = bwtab[iy]+256*ix;
		    for(i=0; i<256; i++) 
			bwbase[i] = maptab[bwbase[i]];
		}
	    }
	    free(maptab);
#ifdef ORIG
	}
#endif /* ORIG */
    }
    bwbase = bwtab[WRAPY(y)];
    while(n) {
	if(n>=XSIZE) {
	    wp[0] = bwbase[bw[0] + 0];
	    wp[1] = bwbase[bw[1] + 256];
	    wp[2] = bwbase[bw[2] + 512];
	    wp[3] = bwbase[bw[3] + 768];
	    wp+=4;
	    bw+=4;
	    n -= XSIZE;
	} else {
	    *wp++ = bwbase[*bw++];
	    bwbase += 256;
	    n--;
	}
    }
}
#endif /* NOTYET */

makedittabs()
{
#ifdef ORIG
    if(gfxmachine() == MACH3D) {
	rtab = makedittab(8,1,256);
	gtab = makedittab(8,8,0);
	btab = makedittab(4,64,0);
    } else {
#endif /* ORIG */
	rtab = makedittab(5,8,0);
	btab = makedittab(5,40,0);
	gtab = makedittab(8,1,56);
#ifdef ORIG
    }
#endif /* ORIG */
}

#ifdef NOTYET
rgbtowp(r,g,b,wp,n,y)
register unsigned short *r, *g, *b;
register short *wp;
register int n, y;
{
    register short *rbase;
    register short *gbase;
    register short *bbase;

    if(!rtab) 
	makedittabs();
    rbase = rtab[WRAPY(y)];
    gbase = gtab[WRAPY(y)];
    bbase = btab[WRAPY(y)];
    while(n) {
	if(n>=XSIZE) {
	    wp[0] = rbase[r[0] +   0] + gbase[g[0] +   0] + bbase[b[0] +   0];
	    wp[1] = rbase[r[1] + 256] + gbase[g[1] + 256] + bbase[b[1] + 256];
	    wp[2] = rbase[r[2] + 512] + gbase[g[2] + 512] + bbase[b[2] + 512];
	    wp[3] = rbase[r[3] + 768] + gbase[g[3] + 768] + bbase[b[3] + 768];
	    wp += 4;
	    r += 4;
	    g += 4;
	    b += 4;
	    n -= XSIZE;
	} else {
	    *wp++ = rbase[*r++] + gbase[*g++] + bbase[*b++];
	    rbase += 256;
	    gbase += 256;
	    bbase += 256;
	    n--;
	}
    }
}
#endif /* NOTYET */

#ifdef NOTYET
wptowp(sptr,dptr,n)
short *sptr, *dptr; 
int n;
{
    bcopy(sptr,dptr,n*sizeof(short));
}
#endif /* NOTYET */

DISPIMAGE *readdispimage(name)
char *name;
{
    char fullname[100];
    register IMAGE *image;

#ifdef ORIG
    findname(name,fullname,"GFXPATH");
    image = iopen(fullname,"r");
#endif /* ORIG */
    image = iopen(name,"r");
    if(!image) {
	msg_printf(SUM,"readdispimage: can't find image %s along GFXPATH\n",name);
#ifdef ORIG
	exit(1);
#endif /* ORIG */
	return 0;
    }
    return makedisp(image);
}

#ifdef NOTYET
static unsigned short *sbuf; 
static unsigned long *lbuf; 
static int ditxzoom = 1;
static int dityzoom = 1;

ditrectzoom(xzoom,yzoom)
float xzoom,yzoom;
{
    ditxzoom = xzoom+0.49;
    dityzoom = yzoom+0.49;
}
#endif /* NOTYET */

#ifdef NOTYET
ditlrectwrite(x1,y1,x2,y2,buf)
int x1, y1, x2, y2;
unsigned long *buf;
{
    int y, ry, my, n;
    int scrxsize;

    if(!sbuf) {
	scrxsize = getgdesc(GD_XPMAX);
	sbuf = (unsigned short *)malloc(scrxsize*sizeof(short));
	lbuf = (unsigned long *)malloc(scrxsize*sizeof(long));
	makedittabs();
    }
    n = x2-x1+1;
    if(ditxzoom == 1 && dityzoom == 1) {
	for(y=y1; y<=y2; y++) {
	    ditlrow(x1,y,buf,sbuf,n);
	    rectwrite(x1,y,x2,y,sbuf);
	    buf+=n;
	}
    } else {
	x2 = x1+(ditxzoom*n)-1;
	my = y1;
	for(y=y1; y<=y2; y++) {
	    replrow(buf,lbuf,n,ditxzoom);
	    for(ry=0; ry<dityzoom; ry++) {
		ditlrow(x1,my,lbuf,sbuf,ditxzoom*n);
		rectwrite(x1,my,x2,my,sbuf);
		my++;
	    }
	    buf+=n;
	}
    }
}
#endif /* NOTYET */

#ifdef NOTYET
ditlrow(x,y,buf,sbuf,n)
int x, y;
unsigned char *buf;
unsigned short *sbuf;
int n;
{
    short offset;
    int s;
    register short *rbase;
    register short *gbase;
    register short *bbase;

    rbase = rtab[WRAPY(y)];
    gbase = gtab[WRAPY(y)];
    bbase = btab[WRAPY(y)];

/* do the first few pixels */
    s = WRAPX(x);
    if(s) {
	offset = 256*s;
	s = 4-s;
	while(n && s) {
	    *sbuf++ = rbase[buf[3] + offset] + 
			    gbase[buf[2] + offset] + 
				    bbase[buf[1] + offset];
	    buf += 4;
	    offset += 256;
	    n--;
	    s--;
	}
    }

/* do the rest */
    offset = 0;
    while(n) {
	if(n>=4) {
	    *sbuf++ = rbase[buf[3+0] +   0] + 
			    gbase[buf[2+0] +   0] + 
				    bbase[buf[1+0] +   0];
	    *sbuf++ = rbase[buf[3+4] + 256] + 
			    gbase[buf[2+4] + 256] + 
				    bbase[buf[1+4] + 256];
	    *sbuf++ = rbase[buf[3+8] + 512] + 
			    gbase[buf[2+8] + 512] + 
				    bbase[buf[1+8] + 512];
	    *sbuf++ = rbase[buf[3+12] + 768] + 
			    gbase[buf[2+12] + 768] + 
				    bbase[buf[1+12] + 768];
	    buf += 16;
	    n -= 4;
	} else {
	    *sbuf++ = rbase[buf[3] + offset] + 
			    gbase[buf[2] + offset] + 
				    bbase[buf[1] + offset];
	    buf += 4;
	    offset += 256;
	    n--;
	}
    }
}
#endif /* NOTYET */

#ifdef NOTYET
ditpnt2i(x,y,c)
int x, y;
long c;
{
    register short *rbase;
    register short *gbase;
    register short *bbase;
    short r, g, b, offset;

    if(!rtab) 
	makedittabs();
    rbase = rtab[WRAPY(y)];
    gbase = gtab[WRAPY(y)];
    bbase = btab[WRAPY(y)];
    offset = 256*WRAPX(x);
    r = offset+((c>>0)&0xff);
    g = offset+((c>>8)&0xff);
    b = offset+((c>>16)&0xff);
    color(rbase[r] + gbase[g] + bbase[b]);
    pnt2i(x,y);
}
#endif /* NOTYET */

replrow(buf,mbuf,n,mag)
__uint32_t *buf, *mbuf;
int n, mag;
{
    int m;
    __uint32_t val;

    switch(mag) {
	case 1:
	    bcopy(buf,mbuf,n*sizeof(__uint32_t)); 
	    break;
	case 2:
	    while(n--) {
		*mbuf++ = *buf;
		*mbuf++ = *buf++;
	    }
	    break;
	case 3:
	    while(n--) {
		*mbuf++ = *buf;
		*mbuf++ = *buf;
		*mbuf++ = *buf++;
	    }
	    break;
	case 4:
	    while(n--) {
		*mbuf++ = *buf;
		*mbuf++ = *buf;
		*mbuf++ = *buf;
		*mbuf++ = *buf++;
	    }
	    break;
	default:
	    while(n--) {
		m = mag;
		val = *buf++;
		while(m--)
		    *mbuf++ = val;
	    }
	    break;
    }
}
