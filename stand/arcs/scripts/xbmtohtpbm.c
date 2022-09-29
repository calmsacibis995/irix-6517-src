#include <stdio.h>

/*
 * returns the hex digit with the bits reversed
 */
int
hextoi(char c)
{
	switch (c) {
	case '0': return 0x0;
	case '1': return 0x8;
	case '2': return 0x4;
	case '3': return 0xc;
	case '4': return 0x2;
	case '5': return 0xa;
	case '6': return 0x6;
	case '7': return 0xe;
	case '8': return 0x1;
	case '9': return 0x9;
	case 'a': return 0x5;
	case 'b': return 0xd;
	case 'c': return 0x3;
	case 'd': return 0xb;
	case 'e': return 0x7;
	case 'f': return 0xf;
	default:
		fprintf(stderr, "xbmtohtpbm: bad hex number: %c\n", c);
		return 0x0;
	}		
}

char
getbyte(FILE *in)
{
	int c;

	/* go to next zero (beginning of '0x') */
	while ((c = fgetc(in)) != '0');

	/* skip the x in '0x' */
	fgetc(in);

	c = hextoi(fgetc(in));
	c |= hextoi(fgetc(in)) << 4;
	return c;
}

main(int argc, char **argv)
{
	FILE *in;
	char line[80];
	char name[32];
	int width, height, shorts;
	int sper, bper;
	int x, y, i;
	unsigned short *a;

	if (argc == 2)
		in = fopen(argv[1], "r");
	else
		in = stdin;

	/* print #defines for width and height */
	fputs(fgets(line, 80, in), stdout);
	sscanf(line, "%*s%*s%d", &width);
	fputs(fgets(line, 80, in), stdout);
	sscanf(line, "%*s%*s%d", &height);

	/* array definition */
	fscanf(in, "%*s%*s%s%*s%*s", name);
	printf("static unsigned short %s = {", name);

	bper = (width + 7) / 8;
	sper = (width + 15) / 16;
	shorts = sper * height;

	a = (unsigned short*)malloc(shorts*sizeof(short));

	/* get data */
	for (i = 0, y = 0; y < height; y++) {
		for (x = bper; x > 0; x -= 2, i++) {
			a[i] = getbyte(in) << 8;
			if (x != 1)
				a[i] |= getbyte(in);
		}
	}

	/* print data */
	printf("\n   0x%04x", a[(height-1)*sper]);
	for (i = 1; i < sper; i++)
		printf(", 0x%04x", a[(height-1)*sper+i]);

	for (y = height-2; y >= 0; y--) {
		for (x = 0; x < sper; x++, i++) {
			printf(",");
			printf((i % 8) ? " " : "\n   ");
			printf("0x%04x", a[y*sper+x]);
		}
	}
	printf("};\n");
}
