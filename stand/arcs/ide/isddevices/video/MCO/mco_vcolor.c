/*
 *	vcolor -
 *		Provide virtual rgb and grey scale support.
 *
 *				Paul Haeberli - 1985
 */
#include "gl.h"
#include "get.h"
#define NOGREYPATTERNS
#include "colortbl.inc"
#include "vect.h"

long rgb();
long rgbpack();
long rgb3f();
long rgbi();
long grey();
long greyi();
long hvs();
long hvsi();

#define MAPNEWS		0
#define MAP332		1
#define REALRGB		2

#define MKRGB(r,g,b) ((((r)&0xff)<<0) | (((g)&0xff)<<8) | (((b)&0xff)<<16))

static int colormode = MAPNEWS;
static int colorinited;

long rgb(fr,fg,fb)
float fr,fg,fb;
{
    int r, g, b;

    r = fr*255;
    g = fg*255;
    b = fb*255;
    return rgbi(r,g,b);
}

long rgbpack(c)
unsigned long c;
{
    int r, g, b;

    r = (c>>0)&0xff;
    g = (c>>8)&0xff;
    b = (c>>16)&0xff;
    return rgbi(r,g,b);
}

long rgb3f(c)
float c[3];
{
    return rgb(c[0],c[1],c[2]);
}

cpacktovect(c,v)
unsigned long c;
vect *v;
{
    v->x = ((c>>0)&0xff)/255.0;
    v->y = ((c>>8)&0xff)/255.0;
    v->z = ((c>>16)&0xff)/255.0;
    v->w = ((c>>24)&0xff)/255.0;
}

long rgbi(r,g,b)
int r,g,b;
{
    long c;

    if(!colorinited) 
	colorinit();
    switch(colormode) {
	case MAPNEWS:
	    if((r == g) && (g == b)) 
	        return greyi(r);
	    c = red_inverse[r]+green_inverse[g]+blue_inverse[b];
	    color(c);
	    break;
	case REALRGB:
	    c = MKRGB(r,g,b);
#ifdef ORIG
	    cpack(c);
#endif /* ORIG */
    }
    return c;
}

#ifdef NOTYET
long hsv(h,s,v)
float h,s,v;
{
    float r, g, b;

    hsv_to_rgb(h,s,v,&r,&g,&b);
    return rgb(r,g,b);
}

long hsvi(h,s,v)
int h,s,v;
{
    float fh,fs,fv;

    fh = h/255.0;
    fs = s/255.0;
    fv = v/255.0;
    return hsv(fh,fs,fv);
}
#endif /* NOTYET */

long grey(fg)
float fg;
{
    return greyi((int)(255*fg));
}

long greyi(g)
int g;
{
    long c;

    if(!colorinited) 
	colorinit();
    switch(colormode) {
	case MAPNEWS:
	    c = grey_inverse[g];
	    color(c);
	    break;
	case REALRGB:
	    c = MKRGB(g,g,g);
#ifdef ORIG
	    cpack(c);
#endif /* ORIG */
	    break;
    }
    return c;
}

usecolor(c)
long c;
{
    switch(colormode) {
	case MAPNEWS:
	    color(c);
	    break;
	case REALRGB:
#ifdef ORIG
	    cpack(c);
#endif /* ORIG */
	    break;
    }
}

pseudorgb()
{
#ifdef NOTYET
    if(getgdesc(GD_BITS_NORM_SNG_RED)>0) {
	RGBmode();
	gconfig();
	colormode = REALRGB;
    } else 
	colormode = MAPNEWS;
#endif /* NOTYET */

    colormode = REALRGB;
    colorinited = 1;
}

colorinit()
{
    int mode;

#ifdef ORIG
    mode = getdisplaymode();
#endif /* ORIG */
    mode = DMRGBDOUBLE;
    if(mode == DMRGB || mode == DMRGBDOUBLE) 
	colormode = REALRGB;
    else
	colormode = MAPNEWS;
    colorinited = 1;
}
