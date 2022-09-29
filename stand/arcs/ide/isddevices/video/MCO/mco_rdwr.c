/*
 *	img_seek, img_write, img_read, img_optseek -
 *
 *				Paul Haeberli - 1984
 *
 */
#include	<stdio.h>
#include	"image.h"

#include	<arcs/io.h>
#include	<arcs/errno.h>
#include	<ide_msg.h>

extern unsigned int img_optseek(IMAGE *image, unsigned int offset);

ULONG readfile(ULONG fd, unsigned char *filedata, int filesize) {
    
    int i, bytecount;
    ULONG cc, filelength;
    long rc;

    msg_printf(VRB, "readfile: Reading %d bytes from file\n", filesize);
    i = 0;
    bytecount=512;
    cc = 1;
    while (bytecount && cc) {
	if (((rc = Read(fd, &filedata[i], bytecount, &cc))==ESUCCESS) && cc) {
	    i = i+cc;
	    if (filesize < (i+bytecount)) {
		bytecount = filesize - i;
	    }
	}
    }
    if (rc != ESUCCESS) {
        msg_printf(SUM,"readfile: error on read of file\n");
        return NULL;
    }
    filelength = i;
    msg_printf(VRB, "imgopen: (ISRLE) Done reading %d bytes for rowstart\n", i);
    return filelength;
}

unsigned int img_seek(IMAGE *image, unsigned int y, unsigned int z)
{
    if(img_badrow(image,y,z)) {
	msg_printf(SUM, "img_seek: row number out of range\n");
	return EOF;
    }
    image->x = 0;
    image->y = y;
    image->z = z;
    if(ISVERBATIM(image->type)) {
	switch(image->dim) {
	    case 1:
		return img_optseek(image, 512);
	    case 2: 
		return img_optseek(image,512+(y*image->xsize)*BPP(image->type));
	    case 3: 
		return img_optseek(image,
		    512+(y*image->xsize+z*image->xsize*image->ysize)*
							BPP(image->type));
	    default:
		msg_printf(SUM, "img_seek: weird dim\n");
		break;
	}
    } else if(ISRLE(image->type)) {
	switch(image->dim) {
	    case 1:
		return img_optseek(image, image->rowstart[0]);
	    case 2: 
		return img_optseek(image, image->rowstart[y]);
	    case 3: 
		return img_optseek(image, image->rowstart[y+z*image->ysize]);
	    default:
		msg_printf(SUM, "img_seek: weird dim\n");
		break;
	}
    } else 
	msg_printf(SUM, "img_seek: weird image type\n");
    return((unsigned int)-1);
}

int img_badrow(IMAGE *image, unsigned int y, unsigned int z)
{
    if(y>=image->ysize || z>=image->zsize)
	return 1;
    else
        return 0;
}

int img_write(IMAGE *image, char *buffer,int count)
{
    unsigned long retval;

#ifdef ORIG
    retval =  write(image->file,buffer,count);
#endif /* ORIG */
    Write(image->file, buffer, count, &retval);
    if(retval == count) 
	image->offset += count;
    else
	image->offset = -1;
    return retval;
}

int img_read(IMAGE *image, char *buffer, int count)
{
    unsigned long retval;
    long rc;

#ifdef ORIG
    retval =  read(image->file,buffer,count);
#endif /* ORIG */
    msg_printf(VRB, "img_read: reading %d bytes from file\n", count);
    if ((rc = Read(image->file, buffer, count, &retval)) != ESUCCESS) {
	msg_printf(SUM,"img_read: error on read of image file\n");
	image->offset = -1;
	return rc;
    }
    if(rc == count) 
	image->offset += count;
    else
	image->offset = -1;
    return rc;
}

unsigned int img_optseek(IMAGE *image, unsigned int offset)
{
    LARGEINTEGER loffset;

    if (image->offset != offset) {
	image->offset = offset;
	loffset.lo = (unsigned long)offset;
#ifdef ORIG
	return ((unsigned int) lseek(image->file, (long) offset, 0));
#endif /* NOTYET */
	Seek(image->file, &loffset, SeekAbsolute);
    }
    return offset;
}

