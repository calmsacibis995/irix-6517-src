/*
 *	xbmtospans - xbitmap to spans converter	
 */
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xmu/Drawing.h>

#include "image.h"
#include "assert.h"

char buf[4096]; 

static XImage image = {
   0, 0,                        /* width, height */
   0, XYBitmap, NULL,           /* xoffset, format, data */
   LSBFirst, 8,                 /* byte-order, bitmap-unit */
   LSBFirst, 8, 1               /* bitmap-bit-order, bitmap-pad, depth */
   };


void
getrowbits(buffer, yoff)
char *buffer;
int yoff;
{
	char *bptr; 
	int ind = 0;
	int i;

	bptr = image.data + ((image.height - yoff - 1) * image.bytes_per_line); 
	bzero(buffer, image.width);
	for(i=0; i<image.bytes_per_line; i++){
	    if((*bptr) & 0x1){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x2){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x4){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x8){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x10){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x20){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x40){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    if((*bptr) & 0x80){
		buffer[ind] = 0xff;
	    }
	    ind++;
	    bptr++;
	}	    
}

	

void
printone(short l)
{
	static char digits[] = "0123456789abcdef";
	static int rowcount = 0;
	
	assert(l >= 0 && l <= 255);
	fputs("0x", stdout);
	putchar(digits[l >> 4]);
	putchar(digits[l & 0xf]);
	fputs(", ", stdout);
	
	rowcount ++;
	if (rowcount >= 8) {
		fputs("\n\t", stdout);
		rowcount = 0;
	}
}

int ReadBitmapFile (filename)
    char *filename;
{
    unsigned int width, height;
    int x_hot, y_hot;
    unsigned char *data;
    int status;

    status = XmuReadBitmapDataFromFile (filename, &width, &height, &data,
                                        &x_hot, &y_hot);
    if (status != BitmapSuccess) return status;

    image.width = width;
    image.height = height;
    image.data = (char *) data;
    image.bytes_per_line = (image.width + 7) / 8;

    return BitmapSuccess;
}

main(int argc, char **argv)
{
	register int x;
	register int y;
	int status;
	
	if(argc != 2){
	    fprintf(stderr, "Usage: xbmtospans <xbitmap file>\n");
	    exit(0);
	}

	status = ReadBitmapFile(argv[1]);
	if (status != BitmapSuccess) {
	    fprintf (stderr, "%s:  error reading input file \"%s\"\n",
               argv[0], argv[1]);
	    exit (1);
	}
        printf("unsigned char clogo_data[] =\n{\n\t");
	for (y = 0; y < image.height; y++) {
		short ccolor = 1;
		short currentlength = 0;
	
		getrowbits(buf, y);
		for (x = 0; x < image.width; x++) {
			short color = buf[x] ? 1 : 0;
			if (color != ccolor) {
				printone(currentlength);
				currentlength = 1;
				ccolor = color;
			} else if (currentlength == 255) {
					printone(currentlength);
					printone(0);
					currentlength = 1;
				} else
					currentlength++;
		}
		if (currentlength)
			printone(currentlength);
	}
	printf("};\n");
	printf("int clogo_w = %d;\n", image.width);
	printf("int clogo_h = %d;\n", image.height);
}
