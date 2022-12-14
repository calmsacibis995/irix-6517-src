/*
 *	putrow, getrow -
 *
 *				Paul Haeberli - 1984
 *
 */
#include	<stdio.h>
#include	"image.h"

#include	<ide_msg.h>

int putrow(IMAGE *image, unsigned short *buffer,
		unsigned int y, unsigned int z) 
{
    register unsigned short 	*sptr;
    register unsigned char      *cptr;
    register unsigned int x;
    register unsigned int min, max;
    register int cnt;

    if( !(image->flags & (_IORW|_IOWRT)) )
	return -1;
    if(image->dim<3)
	z = 0;
    if(image->dim<2)
	y = 0;
    if(ISVERBATIM(image->type)) {
	switch(BPP(image->type)) {
	    case 1: 
		min = image->min;
		max = image->max;
		cptr = (unsigned char *)image->tmpbuf;
		sptr = buffer;
		for(x=image->xsize; x--;) { 
		    *cptr = *sptr++;
		    if (*cptr > max) max = *cptr;
		    if (*cptr < min) min = *cptr;
		    cptr++;
		}
		image->min = min;
		image->max = max;
		img_seek(image,y,z);
		cnt = image->xsize;
		if (img_write(image,(char *)(image->tmpbuf),cnt) != cnt) {
		    msg_printf(SUM, "putrow: error on write of row\n");
		    return -1;
		} else
		    return cnt;
		/* NOTREACHED */

	    case 2: 
		min = image->min;
		max = image->max;
		sptr = buffer;
		for(x=image->xsize; x--;) { 
		    if (*sptr > max) max = *sptr;
		    if (*sptr < min) min = *sptr;
		    sptr++;
		}
		image->min = min;
		image->max = max;
		img_seek(image,y,z);
		cnt = image->xsize<<1;
		if(image->dorev)	
		    cvtshorts(buffer,cnt);
		if (img_write(image,(char *)(buffer),cnt) != cnt) {
		    if(image->dorev) {
			cvtshorts(buffer,cnt);
		    }
		    msg_printf(SUM,"putrow: error on write of row\n");
		    return -1;
		} else {
		    if(image->dorev)	
			cvtshorts(buffer,cnt);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		msg_printf(SUM,"putrow: weird bpp\n");
	}
    } else if(ISRLE(image->type)) {
	switch(BPP(image->type)) {
	    case 1: 
		min = image->min;
		max = image->max;
		sptr = buffer;
		for(x=image->xsize; x--;) { 
		    if (*sptr > max) max = *sptr;
		    if (*sptr < min) min = *sptr;
		    sptr++;
		}
		image->min = min;
		image->max = max;
		cnt = img_rle_compact(buffer,2,image->tmpbuf,1,image->xsize);
		img_setrowsize(image,cnt,y,z);
		img_seek(image,y,z);
		if (img_write(image,(char *)(image->tmpbuf),cnt) != cnt) {
	    	    msg_printf(SUM,"putrow: error on write of row\n");
		    return -1;
		} else
		    return image->xsize;
		/* NOTREACHED */

	    case 2: 
		min = image->min;
		max = image->max;
		sptr = buffer;
		for(x=image->xsize; x--;) { 
		    if (*sptr > max) max = *sptr;
		    if (*sptr < min) min = *sptr;
		    sptr++;
		}
		image->min = min;
		image->max = max;
		cnt = img_rle_compact(buffer,2,image->tmpbuf,2,image->xsize);
		cnt <<= 1;
		img_setrowsize(image,cnt,y,z);
		img_seek(image,y,z);
		if(image->dorev)
		    cvtshorts(image->tmpbuf,cnt);
		if (img_write(image,(char *)(image->tmpbuf),cnt) != cnt) {
		    if(image->dorev)
			cvtshorts(image->tmpbuf,cnt);
		    msg_printf(SUM,"putrow: error on write of row\n");
		    return -1;
		} else {
		    if(image->dorev)
			cvtshorts(image->tmpbuf,cnt);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		msg_printf(SUM,"putrow: weird bpp\n");
	}
    } else 
	msg_printf(SUM,"putrow: weird image type\n");
    return(-1);
}

int getrow(IMAGE *image, unsigned short *buffer,
		unsigned int y, unsigned int z) 
{
    register short i;
    register unsigned char *cptr;
    register unsigned short *sptr;
    register short cnt; 

    if( !(image->flags & (_IORW|_IOREAD)) )
	return -1;
    if(image->dim<3)
	z = 0;
    if(image->dim<2)
	y = 0;
    img_seek(image, y, z);
    if(ISVERBATIM(image->type)) {
	switch(BPP(image->type)) {
	    case 1: 
		if (img_read(image,(char *)image->tmpbuf,image->xsize) 
							    != image->xsize) {
		    msg_printf(SUM,"getrow: error on read of row\n");
		    return -1;
		} else {
		    cptr = (unsigned char *)image->tmpbuf;
		    sptr = buffer;
		    for(i=image->xsize; i--;)
			*sptr++ = *cptr++;
		}
		return image->xsize;
		/* NOTREACHED */

	    case 2: 
		cnt = image->xsize<<1; 
		if (img_read(image,(char *)(buffer),cnt) != cnt) {
		    msg_printf(SUM,"getrow: error on read of row\n");
		    return -1;
		} else {
		    if(image->dorev)
			cvtshorts(buffer,cnt);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		msg_printf(SUM,"getrow: weird bpp\n");
		break;
	}
    } else if(ISRLE(image->type)) {
	switch(BPP(image->type)) {
	    case 1: 
		if( (cnt = img_getrowsize(image)) == -1 )
		    return -1;
		if( img_read(image,(char *)(image->tmpbuf),cnt) != cnt ) {
		    msg_printf(SUM,"getrow: error on read of row\n");
		    return -1;
		} else {
		    img_rle_expand(image->tmpbuf,1,buffer,2);
		    return image->xsize;
		}
		/* NOTREACHED */

	    case 2: 
		if( (cnt = img_getrowsize(image)) == -1 )
		    return -1;
		if( cnt != img_read(image,(char *)(image->tmpbuf),cnt) ) {
		    msg_printf(SUM,"getrow: error on read of row\n");
		    return -1;
		} else {
		    if(image->dorev)
			cvtshorts(image->tmpbuf,cnt);
		    img_rle_expand(image->tmpbuf,2,buffer,2);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		msg_printf(SUM,"getrow: weird bpp\n");
		break;
	}
    } else 
	msg_printf(SUM,"getrow: weird image type\n");
}
