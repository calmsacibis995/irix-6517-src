/*
 *	graytospans - convert a .bw image to spans
 */

#include "image.h"
#include "assert.h"

short buf[4096]; 

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
	zsize = image->zsize;
	if (zsize != 1) {
		printf("Warning: Expecting a greyscale image in the 0-15 range\n");
	}
	printf("unsigned char clogo_data[] =\n{\n\t");
	for (y = 0; y < ysize; y++) {
		short ccolor = 1;
		short currentlength = 0;
		getrow(image, buf, y, 0);
		for (x = 0; x < xsize; x++) {
			short color = (buf[x] > 128) ? 1 : 0;
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
	printf("int clogo_w = %d;\n", xsize);
	printf("int clogo_h = %d;\n", ysize);
}
