/*
 *	row -
 *		Support for operations on image rows.
 *
 *			Paul Haeberli - 1989
 */
#include "image.h"
#include "lum.h"

zerorow(sptr,n)
short *sptr;
int n;
{
    bzero(sptr,n*sizeof(short));
}

copyrow(s,d,n)
short *s, *d;
int n;
{
    bcopy(s,d,n*sizeof(short));
}

setrow(sptr,val,n)
short *sptr;
int val, n;
{
    if(val==0)
	zerorow(sptr,n);
    else {
	while(n>=8) {
	    sptr[0] = val;
	    sptr[1] = val;
	    sptr[2] = val;
	    sptr[3] = val;
	    sptr[4] = val;
	    sptr[5] = val;
	    sptr[6] = val;
	    sptr[7] = val;
	    sptr += 8;
	    n -= 8;
	}
	while(n--) 
	    *sptr++ = val;
    }
}

#define DOCLAMP(iptr,optr)	*(optr) = ((*(iptr)<0) ? 0 : (*(iptr)>255) ? 255 : *(iptr))

clamprow(iptr,optr,n)
short *iptr, *optr;
int n;
{
    short  val;

    while(n>=8) {
	DOCLAMP(iptr+0,optr+0);
	DOCLAMP(iptr+1,optr+1);
	DOCLAMP(iptr+2,optr+2);
	DOCLAMP(iptr+3,optr+3);
	DOCLAMP(iptr+4,optr+4);
	DOCLAMP(iptr+5,optr+5);
	DOCLAMP(iptr+6,optr+6);
	DOCLAMP(iptr+7,optr+7);
	iptr += 8;
	optr += 8;
	n -= 8;
    }
    while(n--) {
	DOCLAMP(iptr,optr);
	iptr++;
	optr++;
    }
}

accrow(iptr,sptr,w,n)
short *iptr;
short *sptr;
int w, n;
{
    if(w == 1) {
	addsrow(iptr,sptr,n);
    } else if(w == -1) {
	subsrow(iptr,sptr,n);
    } else {
	while(n>=8) {
	    iptr[0] += w*sptr[0];
	    iptr[1] += w*sptr[1];
	    iptr[2] += w*sptr[2];
	    iptr[3] += w*sptr[3];
	    iptr[4] += w*sptr[4];
	    iptr[5] += w*sptr[5];
	    iptr[6] += w*sptr[6];
	    iptr[7] += w*sptr[7];
	    iptr += 8;
	    sptr += 8;
	    n -= 8;
	}
	while(n--) 
	    *iptr++ += (w * *sptr++);
    }
}

divrow(iptr,optr,tot,n)
short *iptr;
short *optr;
int tot, n;
{
    if(iptr == optr) {
	while(n>=8) {
	    optr[0] = optr[0]/tot;
	    optr[1] = optr[1]/tot;
	    optr[2] = optr[2]/tot;
	    optr[3] = optr[3]/tot;
	    optr[4] = optr[4]/tot;
	    optr[5] = optr[5]/tot;
	    optr[6] = optr[6]/tot;
	    optr[7] = optr[7]/tot;
	    optr += 8;
	    n -= 8;
	}
	while(n--) {
	    *optr = (*optr)/tot;
	    optr++;
	}
    } else {
	while(n>=8) {
	    optr[0] = iptr[0]/tot;
	    optr[1] = iptr[1]/tot;
	    optr[2] = iptr[2]/tot;
	    optr[3] = iptr[3]/tot;
	    optr[4] = iptr[4]/tot;
	    optr[5] = iptr[5]/tot;
	    optr[6] = iptr[6]/tot;
	    optr[7] = iptr[7]/tot;
	    optr += 8;
	    iptr += 8;
	    n -= 8;
	}
	while(n--) 
	    *optr++ = (*iptr++)/tot;
    }
}

#define DOTOBW(optr,rptr,gptr,bptr)	*(optr) = ILUM(*(rptr),*(gptr),*(bptr))

rgbrowtobw(rbuf,gbuf,bbuf,obuf,n)
unsigned short *rbuf, *gbuf, *bbuf, *obuf;
int n;
{
    while(n>=8) {
	DOTOBW(obuf+0,rbuf+0,gbuf+0,bbuf+0);
	DOTOBW(obuf+1,rbuf+1,gbuf+1,bbuf+1);
	DOTOBW(obuf+2,rbuf+2,gbuf+2,bbuf+2);
	DOTOBW(obuf+3,rbuf+3,gbuf+3,bbuf+3);
	DOTOBW(obuf+4,rbuf+4,gbuf+4,bbuf+4);
	DOTOBW(obuf+5,rbuf+5,gbuf+5,bbuf+5);
	DOTOBW(obuf+6,rbuf+6,gbuf+6,bbuf+6);
	DOTOBW(obuf+7,rbuf+7,gbuf+7,bbuf+7);
	rbuf += 8;
	gbuf += 8;
	bbuf += 8;
	obuf += 8;
	n -= 8;
    } 
    while(n--) {
	DOTOBW(obuf,rbuf,gbuf,bbuf);
	rbuf++;
	gbuf++;
	bbuf++;
	obuf++;
    }
}

/*
 *	addsrow -
 *		Add two rows together
 *
 */
addsrow(dptr,sptr,n)
short *dptr, *sptr;
int n;
{
    while(n>=8) {
	dptr[0] += sptr[0];
	dptr[1] += sptr[1];
	dptr[2] += sptr[2];
	dptr[3] += sptr[3];
	dptr[4] += sptr[4];
	dptr[5] += sptr[5];
	dptr[6] += sptr[6];
	dptr[7] += sptr[7];
	dptr += 8;
	sptr += 8;
	n -= 8;
    }
    while(n--) 
	*dptr++ += *sptr++;
}

/*
 *	subsrow -
 *		Subtract two rows
 *
 */
subsrow(dptr,sptr,n)
short *dptr, *sptr;
int n;
{
    while(n>=8) {
	dptr[0] -= sptr[0];
	dptr[1] -= sptr[1];
	dptr[2] -= sptr[2];
	dptr[3] -= sptr[3];
	dptr[4] -= sptr[4];
	dptr[5] -= sptr[5];
	dptr[6] -= sptr[6];
	dptr[7] -= sptr[7];
	dptr += 8;
	sptr += 8;
	n -= 8;
    }
    while(n--) 
	*dptr++ -= *sptr++;
}

bitstorow(bits,sbuf,n)
unsigned char *bits;
short *sbuf;
int n;
{
    int i, val, nbytes;

    nbytes = ((n-1)/8)+1;
    for(i = 0; i<nbytes; i++ ) {
	val = *bits++;
	if(val&0x80)
	    sbuf[0] = 0;
	else
	    sbuf[0] = 255;
	if(val&0x40)
	    sbuf[1] = 0;
	else
	    sbuf[1] = 255;
	if(val&0x20)
	    sbuf[2] = 0;
	else
	    sbuf[2] = 255;
	if(val&0x10)
	    sbuf[3] = 0;
	else
	    sbuf[3] = 255;
	if(val&0x08)
	    sbuf[4] = 0;
	else
	    sbuf[4] = 255;
	if(val&0x04)
	    sbuf[5] = 0;
	else
	    sbuf[5] = 255;
	if(val&0x02)
	    sbuf[6] = 0;
	else
	    sbuf[6] = 255;
	if(val&0x01)
	    sbuf[7] = 0;
	else
	    sbuf[7] = 255;
	sbuf += 8;
    }
}

rowtobits(sbuf,bits,n)
short *sbuf;
unsigned char *bits;
int n;
{
    int i, val, nbytes, thresh;

    nbytes = ((n-1)/8)+1;
    thresh = 128;
    for(i = 0; i<nbytes; i++) {
	val = 0;
	if(sbuf[0]<thresh)
	    val |= 0x80;
	if(sbuf[1]<thresh)
	    val |= 0x40;
	if(sbuf[2]<thresh)
	    val |= 0x20;
	if(sbuf[3]<thresh)
	    val |= 0x10;
	if(sbuf[4]<thresh)
	    val |= 0x08;
	if(sbuf[5]<thresh)
	    val |= 0x04;
	if(sbuf[6]<thresh)
	    val |= 0x02;
	if(sbuf[7]<thresh)
	    val |= 0x01;
	sbuf += 8;
	*bits++ = val;
    }
}

/* 
 *	bit reverse a stream of bytes
 *
 */
bitrevbytes(buf,n)
unsigned char *buf;
int n;
{
    int i, x, br;
    static unsigned char *bitrev;

    if(!bitrev) {
	bitrev = (unsigned char *)malloc(256);
	for(i=0; i<256; i++) {
	    br = 0;
	    for(x=0; x<8; x++) {
		br = br<<1;
		if(i&(1<<x))
		    br |= 1;
	    }
	    bitrev[i] = br;
	}
    }
    while(n>=8) {
	buf[0] = bitrev[buf[0]];
	buf[1] = bitrev[buf[1]];
	buf[2] = bitrev[buf[2]];
	buf[3] = bitrev[buf[3]];
	buf[4] = bitrev[buf[4]];
	buf[5] = bitrev[buf[5]];
	buf[6] = bitrev[buf[6]];
	buf[7] = bitrev[buf[7]];
	buf += 8;
	n -= 8;
    }
    while(n--) {
	buf[0] = bitrev[buf[0]];
	*buf++;
    }
}

/* 
 *	flip a row of shorts
 *
 */
flipsrow(sptr,n) 
register short *sptr;
register int n;
{
    register short temp, *p1, *p2;

    p1 = sptr;
    p2 = sptr+n-1;
    n = n/2;
    while(n--) {
	temp = *p1;
	*p1++ = *p2;
	*p2-- = temp;
    }
}

/* 	cpack -
 *		Convert from and to cpack format.
 *	
 */
bwtocpack(b,l,n)
register unsigned short *b;
register unsigned long *l;
register int n;
{
    while(n>=8) {
	l[0] = 0xff000000+0x00010101*b[0];
	l[1] = 0xff000000+0x00010101*b[1];
	l[2] = 0xff000000+0x00010101*b[2];
	l[3] = 0xff000000+0x00010101*b[3];
	l[4] = 0xff000000+0x00010101*b[4];
	l[5] = 0xff000000+0x00010101*b[5];
	l[6] = 0xff000000+0x00010101*b[6];
	l[7] = 0xff000000+0x00010101*b[7];
	l += 8;
	b += 8;
	n -= 8;
    }
    while(n--) 
	*l++ = 0xff000000+0x00010101*(*b++);
}

rgbtocpack(r,g,b,l,n)
register unsigned short *r, *g, *b;
register unsigned long *l;
register int n;
{
    while(n>=8) {
	l[0] = r[0] | (g[0]<<8) | (b[0]<<16) | (0xff<<24);
	l[1] = r[1] | (g[1]<<8) | (b[1]<<16) | (0xff<<24);
	l[2] = r[2] | (g[2]<<8) | (b[2]<<16) | (0xff<<24);
	l[3] = r[3] | (g[3]<<8) | (b[3]<<16) | (0xff<<24);
	l[4] = r[4] | (g[4]<<8) | (b[4]<<16) | (0xff<<24);
	l[5] = r[5] | (g[5]<<8) | (b[5]<<16) | (0xff<<24);
	l[6] = r[6] | (g[6]<<8) | (b[6]<<16) | (0xff<<24);
	l[7] = r[7] | (g[7]<<8) | (b[7]<<16) | (0xff<<24);
	l += 8;
	r += 8;
	g += 8;
	b += 8;
	n -= 8;
    }
    while(n--) 
        *l++ = *r++ | ((*g++)<<8) | ((*b++)<<16) | (0xff<<24);
}

rgbatocpack(r,g,b,a,l,n)
register unsigned short *r, *g, *b, *a;
register unsigned long *l;
register int n;
{
    while(n>=8) {
	l[0] = r[0] | (g[0]<<8) | (b[0]<<16) | (a[0]<<24);
	l[1] = r[1] | (g[1]<<8) | (b[1]<<16) | (a[1]<<24);
	l[2] = r[2] | (g[2]<<8) | (b[2]<<16) | (a[2]<<24);
	l[3] = r[3] | (g[3]<<8) | (b[3]<<16) | (a[3]<<24);
	l[4] = r[4] | (g[4]<<8) | (b[4]<<16) | (a[4]<<24);
	l[5] = r[5] | (g[5]<<8) | (b[5]<<16) | (a[5]<<24);
	l[6] = r[6] | (g[6]<<8) | (b[6]<<16) | (a[6]<<24);
	l[7] = r[7] | (g[7]<<8) | (b[7]<<16) | (a[7]<<24);
	l += 8;
	r += 8;
	g += 8;
	b += 8;
	a += 8;
	n -= 8;
    }
    while(n--) 
        *l++ = *r++ | ((*g++)<<8) | ((*b++)<<16) | ((*a++)<<24);
}

#define CPACKTORGB(l,r,g,b)			\
	val = (l);				\
	(r) = (val>>0) & 0xff;			\
	(g) = (val>>8) & 0xff;			\
	(b) = (val>>16) & 0xff;			

cpacktorgb(l,r,g,b,n)
register unsigned long *l;
register unsigned short *r, *g, *b;
register int n;
{
    unsigned long val;

    while(n>=8) {
	CPACKTORGB(l[0],r[0],g[0],b[0]);
	CPACKTORGB(l[1],r[1],g[1],b[1]);
	CPACKTORGB(l[2],r[2],g[2],b[2]);
	CPACKTORGB(l[3],r[3],g[3],b[3]);
	CPACKTORGB(l[4],r[4],g[4],b[4]);
	CPACKTORGB(l[5],r[5],g[5],b[5]);
	CPACKTORGB(l[6],r[6],g[6],b[6]);
	CPACKTORGB(l[7],r[7],g[7],b[7]);
	l += 8;
	r += 8;
	g += 8;
	b += 8;
	n -= 8;
    }
    while(n--) {
	CPACKTORGB(l[0],r[0],g[0],b[0]);
	l++;
	r++;
	g++;
	b++;
    }
}

#define CPACKTORGBA(l,r,g,b,a)			\
	val = (l);				\
	(r) = (val>>0) & 0xff;			\
	(g) = (val>>8) & 0xff;			\
	(b) = (val>>16) & 0xff;			\
	(a) = (val>>24) & 0xff;			

cpacktorgba(l,r,g,b,a,n)
register unsigned long *l;
register unsigned short *r, *g, *b, *a;
register int n;
{
    unsigned long val;

    while(n>=8) {
	CPACKTORGBA(l[0],r[0],g[0],b[0],a[0]);
	CPACKTORGBA(l[1],r[1],g[1],b[1],a[1]);
	CPACKTORGBA(l[2],r[2],g[2],b[2],a[2]);
	CPACKTORGBA(l[3],r[3],g[3],b[3],a[3]);
	CPACKTORGBA(l[4],r[4],g[4],b[4],a[4]);
	CPACKTORGBA(l[5],r[5],g[5],b[5],a[5]);
	CPACKTORGBA(l[6],r[6],g[6],b[6],a[6]);
	CPACKTORGBA(l[7],r[7],g[7],b[7],a[7]);
	l += 8;
	r += 8;
	g += 8;
	b += 8;
	a += 8;
	n -= 8;
    }
    while(n--) {
	CPACKTORGBA(l[0],r[0],g[0],b[0],a[0]);
	l++;
	r++;
	g++;
	b++;
	a++;
    }
}

/*
 *	normrow -
 *		Normalize a row of image data
 *
 */
normrow(image,buf)
IMAGE *image;
short *buf;
{
    int n, max;

    n = image->xsize;
    max = image->max;
    if(max == 255 || max == 0)
	return;
    while(n>=8) {
	buf[0] = (buf[0]*255)/max;
	buf[1] = (buf[1]*255)/max;
	buf[2] = (buf[2]*255)/max;
	buf[3] = (buf[3]*255)/max;
	buf[4] = (buf[4]*255)/max;
	buf[5] = (buf[5]*255)/max;
	buf[6] = (buf[6]*255)/max;
	buf[7] = (buf[7]*255)/max;
	buf += 8;
	n -= 8;
    }
    while(n--) {
	*buf = (*buf*255)/max;
	buf++;
    }
}

static short *rbuf, *gbuf, *bbuf;
static int malloclen = 0;

getbwrow(image,buf,y)
IMAGE *image;
char *buf;
int y;
{
    if(malloclen<image->xsize) {
	if(malloclen) {
	    free(rbuf);
	    free(gbuf);
	    free(bbuf);
	}
	rbuf = (short *)malloc(image->xsize*sizeof(short));
	gbuf = (short *)malloc(image->xsize*sizeof(short));
	bbuf = (short *)malloc(image->xsize*sizeof(short));
	malloclen=image->xsize;;
    }
    if(image->zsize<3) {
	getrow(image,buf,y,0);
    } else {
	getrow(image,rbuf,y,0);
	getrow(image,gbuf,y,1);
	getrow(image,bbuf,y,2);
	rgbrowtobw(rbuf,gbuf,bbuf,buf,image->xsize);
    }
}

putfliprow(image,buf,y,z,flipcode)
IMAGE *image;
short *buf;
int y, z;
int flipcode;
{
    if(flipcode&1)
	flipsrow(buf,image->xsize);
    if(flipcode&2)
	putrow(image,buf,(image->ysize-1)-y,z);
    else
	putrow(image,buf,y,z);
}

getfliprow(image,buf,y,z,flipcode)
IMAGE *image;
short *buf;
int y, z, flipcode;
{
    if(flipcode&2)
	getrow(image,buf,(image->ysize-1)-y,z);
    else
	getrow(image,buf,y,z);
    if(flipcode&1)
	flipsrow(buf,image->xsize);
}

/* 
 *	dithering stuff follows
 *
 */
#define MATSIZE88

#define XSIZE	8
#define YSIZE	8

static short dithmat[YSIZE][XSIZE] = {		/* 8x8 Limb */
	0,	8,	36,	44,	2,	10,	38,	46,
	16,	24,	52,	60,	18,	26,	54,	62,
	32,	40, 	4,	12,	34,	42,	6,	14,
	48,	56,	20,	28,	50,	58,	22,	30,
	3,	11,	39,	47,	1,	9,	37,	45,
	19,	27,	55,	63,	17,	25,	53,	61,
	35,	43,	7,	15,	33,	41,	5,	13,
	51,	59,	23,	31,	49,	57,	21,	29,
};

#ifdef NOTDEF
static short dithmat[YSIZE][XSIZE] = {		/* halftone dots */
	3,	17,	55,     63,	61,     47,	9,	1,
	15, 	29,	39,	51,	49,	35,	25,	13,
	40,	32,	26,	20,	22,	30,	36,	42,
	56,	44,	10,	4,	6,	18,	52,	58,
	60,	46,	8,	0,	2,	16,	54,	62,
	48,	34,	24,	12,	14,	28,	38,	50,
	23,	31,	37,	43,	41,	33,	27,	21,
	7,	19,	53,	59,	57,	45,	11,	5,
};
#endif

#define TOTAL		(XSIZE*YSIZE)

ditherrow(buf,y,n)
short *buf;
int y, n;
{
    int r, val;
    int rshades, rmaxbits;
    short *rdith, *gdith, *bdith;

    rdith = &dithmat[y%YSIZE][0];
    rshades = TOTAL+1;
    rmaxbits = ((rshades-1)/TOTAL);
    while(n--) {
	r = *buf;
	val = (rshades*r)/255;
	if(val>=TOTAL) 
	    *buf++ = 255;
	else if(val>rdith[n%XSIZE])
	    *buf++ = 255;
	else
	    *buf++ = 0;
    }
}
