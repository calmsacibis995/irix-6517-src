/*
 *	iopen -
 *
 *				Paul Haeberli - 1984
 *
 */
#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <ide_msg.h>

#ifdef NOTYET
#include <unistd.h>
#endif /* NOTYET */

#include "gl.h"
#include "image.h"

/* Stuff from <stdio.h> for compatibility */
#define _IOFBF          0000    /* full buffered */
#define _IOLBF          0100    /* line buffered */
#define _IONBF          0004    /* not buffered */
#define _IOEOF          0020    /* EOF reached on read */
#define _IOERR          0040    /* I/O error from system */

#define _IOREAD         0001    /* currently reading */
#define _IOWRT          0002    /* currently writing */
#define _IORW           0200    /* opened for reading and writing */
#define _IOMYBUF        0010    /* stdio malloc()'d buffer */

#define IPASTEOFFSET	512L
#define XMAX 	(XMAXSCREEN+1)
#define YMAX 	(YMAXSCREEN+1)
#define IPASTEMAXSIZE	(XMAX * YMAX * sizeof(__uint32_t))
#ifdef NOTYET
IMAGE mcoImage;
#endif /* NOTYET */

unsigned char *ipaste_data;
char ipaste_filename[256];

IMAGE *imgopen(long, char *, char *,unsigned int, unsigned int,
		unsigned int, unsigned int, unsigned int);


IMAGE *iopen(char *file, char *mode, unsigned int type, unsigned int dim,
		unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
    return(imgopen(0, file, mode, type, dim, xsize, ysize, zsize));
}

IMAGE *fiopen(long f, char *mode, unsigned int type, unsigned int dim,
		unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
    return(imgopen(f, 0, mode, type, dim, xsize, ysize, zsize));
}

IMAGE *imgopen(long f, char *file, char *mode,
		unsigned int type, unsigned int dim,
		unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
	register IMAGE 	*image, *tmpimage;
	register rw;
	int tablesize;
	register int i, max;
	ULONG fd, cc;
	long rc;
	LARGEINTEGER offs;
	unsigned char *junkmem;
	int junksize;
	int bytecount;

	image = (IMAGE*)calloc(1,sizeof(IMAGE));
#ifdef NOTYET
	image = (IMAGE*)get_chunk(sizeof(IMAGE));
	image = &mcoImage;
#endif /* NOTYET */
	if(!image ) {
	    msg_printf(SUM,"iopen: error on image struct alloc\n");
	    return NULL;
	}
	rw = mode[1] == '+';
	if(rw) {
	    msg_printf(SUM,"iopen: read/write mode not supported\n");
		return NULL;
	}
	if (*mode=='w') {
	    msg_printf(SUM,"iopen: write mode not supported\n");
		return NULL;
#ifdef NOTYET
		if (file) {
		    f = creat(file, 0666);
		    if (rw && f>=0) {
			    close(f);
			    f = open(file, 2);
		    }
		}
		if (f < 0) {
		    msg_printf(SUM,"iopen: can't open output file %s\n",file);
		    return NULL;
		}
		image->imagic = IMAGIC;
		image->type = type;
		image->xsize = xsize;
		image->ysize = 1;
		image->zsize = 1;
		if (dim>1)
		    image->ysize = ysize;
		if (dim>2)
		    image->zsize = zsize;
		if(image->zsize == 1) {
		    image->dim = 2;
		    if(image->ysize == 1)
			image->dim = 1;
		} else {
		    image->dim = 3;
		}
		image->min = 10000000;
		image->max = 0;
		isetname(image,"no name"); 
		image->wastebytes = 0;
		image->dorev = 0;
		if (write(f,image,sizeof(IMAGE)) != sizeof(IMAGE)) {
		    msg_printf(SUM,"iopen: error on write of image header\n");
		    return NULL;
		}
#endif /* NOTYET */
	} else {
		if (file) {
#ifdef ORIG
		    f = open(file, rw? 2: 0);
#endif /* ORIG */
		    msg_printf(VRB, "imgopen: Opening file %s\n", file);
		    if ( (f = Open(file, OpenReadOnly, &fd)) != ESUCCESS) {
			return(NULL);
		    }
		}
		/* Now that we know that the file is open, let's allocate
		 * memory for the file.  We only read up to MAXIPASTESIZE,
		 * as that is all we can display anyway
		 */
		ipaste_data = (unsigned char *)get_chunk(IPASTEMAXSIZE);

		/* 
		 * Now read the file
		 */
		msg_printf(VRB, "imgopen: Reading file %s\n", file);
		if (rc=readfile(fd,ipaste_data,IPASTEMAXSIZE)) {
		}
		tmpimage = (IMAGE *)ipaste_data;
		/* Copy tmpimage to image */
		image->imagic = tmpimage->imagic;
		image->type = tmpimage->type;
		image->dim   = tmpimage->dim;
		image->xsize = tmpimage->xsize;
		image->ysize = tmpimage->ysize;
		image->zsize = tmpimage->zsize;
		image->min   = tmpimage->min;
		image->max   = tmpimage->max;
		image->wastebytes = tmpimage->wastebytes;
		sprintf(image->name,"%s",tmpimage->name);
		image->colormap = tmpimage->colormap;
		image->file = tmpimage->file;
		image->flags = tmpimage->flags;
		image->dorev = tmpimage->dorev;
		image->x = tmpimage->x;
		image->y = tmpimage->y;
		image->z = tmpimage->z;
		image->cnt = tmpimage->cnt;
		image->ptr = tmpimage->ptr;
		image->base = tmpimage->base;
		image->tmpbuf = tmpimage->tmpbuf;
		image->offset = tmpimage->offset;
		image->rleend = tmpimage->rleend;
		image->rowstart = tmpimage->rowstart;
		image->rowsize = tmpimage->rowsize;

#ifdef NOTYET1
		if ((rc=Read(fd,image,sizeof(IMAGE), &cc)) != ESUCCESS) {
		    msg_printf(SUM,"iopen: error on read of image header\n");
		    return NULL;
		}
#endif /* NOTYET1 */
		if( ((image->imagic>>8) | ((image->imagic&0xff)<<8)) 
							     == IMAGIC ) {
		    image->dorev = 1;
		    cvtimage(image);
		} else {
		    image->dorev = 0;
		}
		if (image->imagic != IMAGIC) {
			msg_printf(SUM,"iopen: bad magic in image file %x\n",image->imagic);
		    return NULL;
		}
		msg_printf(VRB, "imgopen: Magic # of file %s is %d (exp %d)\n", 
		file, image->imagic, IMAGIC);
	}
	if (rw) {
	    image->flags = _IORW;
	}
	else if (*mode != 'r') {
	    image->flags = _IOWRT;
	}
	else {
	    image->flags = _IOREAD;
	}
	if(ISRLE(image->type)) {
	    tablesize = image->ysize*image->zsize*sizeof(int);
	    msg_printf(VRB,"imgopen: (ISRLE) calling malloc(%d) for image->rowstart\n", tablesize);
	    image->rowstart = (unsigned long *)malloc(tablesize);
	    msg_printf(VRB,"imgopen: (ISRLE) calling malloc(%d) for image->rowsize\n", tablesize);
	    image->rowsize = (long *)malloc(tablesize);
	    if( image->rowstart == 0 || image->rowsize == 0 ) {
		msg_printf(SUM,"iopen: error on table alloc\n");
		return NULL;
	    }
	    image->rleend = 512 + 2*tablesize;
	    if (*mode=='w') {
		max = image->ysize*image->zsize;
		for(i=0; i<max; i++) {
		    image->rowstart[i] = 0;
		    image->rowsize[i] = -1;
		}
	    } else {
		tablesize = image->ysize*image->zsize*sizeof(int);
#ifdef ORIG
		ipaste_lseek(f, 512L, 0);
#endif /* ORIG */
		/* Originally, imgopen() used lseek() to set the file pointer
		 *     lseek(image->file, 512L, 0);
		 * but Seek() doesn't work right on files Open'd over the net.
		 * 
		 * We'll try simulating lseek by using Read() to read in 
		 * 512L bytes into a throwaway buffer, then continue to 
		 * Read the file as usual.
		 */
		junkmem = (unsigned char *)get_chunk(2 * IPASTEOFFSET);
		junksize = 512L - sizeof(IMAGE);
		msg_printf(VRB,"imgopen: (ISRLE) Reading %d bytes (junk) from file\n", junksize);
		if (((rc = Read(fd,junkmem, junksize, &cc))!=ESUCCESS) || (cc != junksize)) {
		    msg_printf(SUM,"imgopen: error on read of %d bytes (expected %d bytes)\n", cc, junksize);
		    return NULL;
		}


		msg_printf(VRB, "imgopen: (ISRLE) Reading %d bytes for rowstart\n", tablesize);
		i = 0;
		bytecount=512;
		while (bytecount) {
		    if (((rc = Read(fd,&image->rowstart[i], bytecount, &cc))==ESUCCESS) && (cc == bytecount)) {

			i = i+cc;
			if ((tablesize-bytecount) < (i+cc)) {
			    bytecount = tablesize - i;
			}
		    }
		}
		if (rc != ESUCCESS) {
		    msg_printf(SUM,"iopen: error on read of rowstart\n");
		    return NULL;
		}
		msg_printf(VRB, "imgopen: (ISRLE) Done reading %d bytes for rowstart\n", tablesize);
		if(image->dorev) {
		    cvtlongs(image->rowstart,tablesize);
		}
		msg_printf(VRB, "imgopen: (ISRLE) Reading %d bytes for rowsize\n", tablesize);
		i = 0;
		bytecount=512;
		while (bytecount) {
		    if (((rc = Read(fd,&image->rowsize[i], bytecount, &cc))==ESUCCESS) && (cc == bytecount)) {

			i = i+cc;
			if ((tablesize-bytecount) < (i+cc)) {
			    bytecount = tablesize - i;
			}
		    }
		}
		if (rc != ESUCCESS) {
		    msg_printf(SUM,"iopen: error on read of rowsize\n");
		    return NULL;
		}
		msg_printf(VRB, "imgopen: (ISRLE) Done reading %d bytes for rowsize\n", tablesize);
		if(image->dorev)
		    cvtlongs(image->rowsize,tablesize);
	    }
	}
	image->cnt = 0;
	image->ptr = 0;
	image->base = 0;
	if( (image->tmpbuf = ibufalloc(image)) == 0 ) {	
	    msg_printf(SUM,"imgopen: error on tmpbuf alloc %d\n",image->xsize);
	    return NULL;
	}
	image->x = image->y = image->z = 0;
#ifdef ORIG
	image->file = f;
	image->offset = 512L;			/* set up for img_optseek */
	lseek(image->file, 512L, 0);
#endif /* ORIG */
	/*
	 * Close the file, then re-open it, so we can seek to the 
	 * right offset
	 */
	if ((Close(fd)) != ESUCCESS) {
		return(NULL);
	}
	msg_printf(VRB, "imgopen: Re-opening file %s\n", file);
	if ( (f = Open(file, OpenReadOnly, &fd)) != ESUCCESS) {
	    msg_printf(SUM,"imgopen: Error re-opening file %s\n", file);
	    return(NULL);
	}
	/* Originally, imgopen() used lseek() to set the file pointer
	 *     lseek(image->file, 512L, 0);
	 * but Seek() doesn't work right on files Open'd over the net.
	 * 
	 * We'll try simulating lseek by using Read() to read in 
	 * 512L bytes into a throwaway buffer
	 */
	junkmem = (unsigned char *)get_chunk(2 * IPASTEOFFSET);
	if (((rc = Read(fd,junkmem, IPASTEOFFSET, &cc))!=ESUCCESS) || (cc != 512)) {
	    msg_printf(SUM,"imgopen: error on read of 512L (%d bytes)\n",cc);
	    return NULL;
	}
	image->file = fd;
	image->offset = 512;			/* set up for img_optseek */

	msg_printf(VRB,"imgopen: returning\n");
	return(image);
}

unsigned short *ibufalloc(IMAGE *image)
{
    msg_printf(VRB,"ibufalloc: calling malloc(%d)\n", IBUFSIZE(image->xsize));
    return (unsigned short *)malloc(IBUFSIZE(image->xsize));
}

reverse(lwrd) 
register unsigned int lwrd;
{
    return ((lwrd>>24) 		| 
	   (lwrd>>8 & 0xff00) 	| 
	   (lwrd<<8 & 0xff0000) | 
	   (lwrd<<24) 		);
}

cvtshorts( buffer, n)
register unsigned short buffer[];
register int n;
{
    register short i;
    register int nshorts = n>>1;
    register unsigned short swrd;

    for(i=0; i<nshorts; i++) {
	swrd = *buffer;
	*buffer++ = (swrd>>8) | (swrd<<8);
    }
}

cvtlongs( buffer, n)
register int buffer[];
register int n;
{
    register short i;
    register int nlongs = n>>2;
    register unsigned int lwrd;

    for(i=0; i<nlongs; i++) {
	lwrd = buffer[i];
	buffer[i] =     ((lwrd>>24) 		| 
	   		(lwrd>>8 & 0xff00) 	| 
	   		(lwrd<<8 & 0xff0000) 	| 
	   		(lwrd<<24) 		);
    }
}

cvtimage( register int buffer[] )
{
    cvtshorts(buffer,12);
    cvtlongs(buffer+3,12);
    cvtlongs(buffer+26,4);
}

#ifdef NOTYET
static void (*i_errfunc)();

/*	error handler for the image library.  If the iseterror() routine
	has been called, sprintf's the args into a string and calls the
	error function.  Otherwise calls fprintf with the args and then
	exit.  This allows 'old' programs to assume that no errors
	ever need be worried about, while programs that know how and
	want to can handle the errors themselves.  Olson, 11/88
*/
i_errhdlr(char *fmt, a1, a2, a3, a4)	/* most args currently used is 2 */
{
	if(i_errfunc) {
		char ebuf[2048];	/* be generous; if an error includes a
			pathname, the maxlen is 1024, so we shouldn't ever 
			overflow this! */
		sprintf(ebuf, fmt, a1, a2, a3, a4);
		(*i_errfunc)(ebuf);
		return;
	}
	msg_printf(SUM, fmt, a1, a2, a3, a4);
	return(1);
}

/* this function sets the error handler for i_errhdlr */
i_seterror(func)
void (*func)();
{
	i_errfunc = func;
}
#endif /* NOT YET */
