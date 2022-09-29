/*
 *	graytoxbm - convert a .bw image to xbitmap
 */

#include "image.h"
#include "assert.h"

short buf[4096]; 

void
printbits(short l, short c)
{
	static char digits[] = "0123456789abcdef";
	static int rowcount = 0;
	static int leftover = 0;
	static char leftbits = 0;
	int i;
	
	assert(l >= 0 && l <= 255);
	if(leftover){
	    if(leftover + l < 8){
		if(c){
		    for(i=leftover; i<leftover+l; i++){
			leftbits |= 1 << i; 
		    }
		}
		leftover += l;
		return;
	    } else {
		if(c){
		    for(i=leftover; i<8; i++){
			leftbits |= 1 << i;
		    }
		}
		l = l - (8 - leftover);
		leftover = 0;
		fprintf(stdout, "0x%x, ", leftbits);
		leftbits = 0;
		rowcount++;
		if(rowcount == 8){
		    fprintf(stdout, "\n\t");
		    rowcount= 0;
		}
	    }
	}
	while(l >= 8){
	    fprintf(stdout, "%s, ", c ? "0xff" : "0x00"); 
	    rowcount ++;
	    if (rowcount >= 8) {
		fputs("\n\t", stdout);
		rowcount = 0;
	    }
	    l -= 8;
	}
	if(l){
	    leftover = l;
	    if(c){
		for(i=0; i<l; i++){
		    leftbits |= 1 << i;
		}
	    }
	}
}

main(int argc, char **argv)
{
	register IMAGE *image;
	register int x, xsize;
	register int y, ysize;
	register int zsize;
	
	if (argc < 2) {
		fprintf(stderr, "usage: %s inimage.rgb\n", argv[0]);
		exit(1);
	} 
	if ((image = iopen(argv[1], "r")) == NULL ) {
		fprintf(stderr, "readimg: can't open input file %s\n", argv[1]);
		exit(1);
	}
	xsize = image->xsize;
	ysize = image->ysize;
	printf("#define clogo_width  %d\n", xsize);
	printf("#define clogo_height  %d\n", ysize);
	printf("static char clogo_bits[] =\n{\n\t");
	for (y = ysize-1; y >= 0; y--) {
		short ccolor = 1;
		short currentlength = 0;
		getrow(image, buf, y, 0);
		for (x = 0; x < xsize; x++) {
			short color = (buf[x] > 128) ? 1 : 0;
			if (color != ccolor) {
				printbits(currentlength, ccolor);
				currentlength = 1;
				ccolor = color;
			} else if (currentlength == 255) {
					printbits(currentlength, ccolor);
					currentlength = 1;
				} else
					currentlength++;
		}
		if (currentlength)
			printbits(currentlength, ccolor);
	}
	printf("};\n");
}
