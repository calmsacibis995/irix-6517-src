#include <sys/types.h>
#include "mgras_color_z.h"
#include "mgras_z.h"

#ifndef _STANDALONE
#include <stdio.h>
#endif

unsigned int my_points_array[30] = {
                0xff8080ff, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x7e33ff7f, 0x33ff80, 0x0, 0x0,
                0x0, 0x0, 0x7f33ff80, 0x33ff7f, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0xffff3f7f, 0x0};

unsigned int my_tex_poly[64] = {
        0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff,
0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee,
0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33};

unsigned int my_logicop_array[48] = {
        0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc,
        0x0, 0x0, 0xcccccc, 0xcccccc, 0x0, 0x0, 0xcccccc, 0xcccccc, 0x333333, 0x333333, 0xffffff, 0xffffff, 0x333333, 0x333333, 0xffffff, 0xffffff,
        0x0, 0x88cccc, 0x440000,0xcccccc, 0x2200, 0x88eecc, 0x442200, 0xcceecc,
0x331133, 0xbbddff, 0x771133, 0xffddff, 0x333333, 0xbbffff, 0x773333, 0xffffff};

unsigned int my_dither_array[32] = {
	0x0, 0x9000009, 0x1a00001a, 0x2a00002a, 0x3c00003c, 0x4d00004d, 0x5e00005e, 0x6e00006e, 0x80000080, 0x90000090, 0xa20000a2, 0xb20000b2, 0xc40000c4, 0xd40000d4, 0xe60000e6, 0x0,
0x0, 0x8000008, 0x19000019, 0x2a00002a, 0x3b00003b, 0x4c00004c, 0x5d00005d, 0x6e00006e, 0x80000080, 0x91000091, 0xa20000a2, 0xb30000b3, 0xc40000c4, 0xd50000d5, 0xe60000e6, 0x0 };

unsigned int my_telod[128] = {
0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955,
0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x6688dd99, 0xbbffbbee, 0xbbeeddff, 0xbbddeeff, 0xbbeeddff, 0xbbffbbee, 0xbbeedddd, 0xbbddeebb, 0xbbaadd99, 0xbb77bb66, 0xbb66ddbb, 0xbb55eeff, 0xbb66ddbb, 0xbb77bb66, 0xbb66dd99, 0xbb55eebb, 0xbb88ccbb, 0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33,
0x6688dd99, 0xbbffbbee, 0xbbeeddff, 0xbbddeeff, 0xbbeeddff, 0xbbffbbee, 0xbbeedddd, 0xbbddeebb, 0xbbaadd99, 0xbb77bb66, 0xbb66ddbb, 0xbb55eeff, 0xbb66ddbb, 0xbb77bb66, 0xbb66dd99, 0xbb55eebb, 0xbb88ccbb, 0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff,
0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33,
0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x5555dd77};

unsigned int my_detail_tex[32] = {
0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333, 0x33333333, 0x0, 0x0, 0xbbffbbee, 0xbbffbbee, 0xbb77bb66, 0xbb77bb66, 0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333, 0x33333333, 0x0, 0x0, 0xbbffbbee, 0xbbffbbee, 0xbb77bb66, 0xbb77bb66, 0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333,
0x33333333, 0x0, 0x0};

unsigned int my_ldrsmpl_array[256] = {
0x55555555, 0x55550000, 0x55555555, 0x55550044, 0xaaaaaaaa, 0xaaaa0000, 0xaaaaaaaa, 0xaaaa0044, 0xcccccccc, 0xcccc0088, 0xcccccccc, 0xcccc00cc, 0xffffffff, 0xffff0088, 0xffffffff, 0xffff00cc, 0x44004411, 0x44221100, 0x44004411, 0x44221144,
0x44884499, 0x44aa1100, 0x44884499, 0x44aa1144, 0x55555555, 0x55550000, 0x55555555, 0x55550044, 0xaaaaaaaa, 0xaaaa0000, 0xaaaaaaaa, 0xaaaa0044, 0xcccccccc, 0xcccc0088, 0xcccccccc, 0xcccc00cc, 0xffffffff, 0xffff0088, 0xffffffff, 0xffff00cc,
0x44004411, 0x44221100, 0x44004411, 0x44221144, 0x44884499, 0x44aa1100, 0x44884499, 0x44aa1144, 0x55555555, 0x55550000, 0x55555555, 0x55550044, 0xaaaaaaaa, 0xaaaa0000, 0xaaaaaaaa, 0xaaaa0044, 0xcccccccc, 0xcccc0088, 0xcccccccc, 0xcccc00cc,
0xffffffff, 0xffff0088, 0xffffffff, 0xffff00cc,
0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333,
0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33,
0x55555555, 0x55550000, 0x55555555, 0x55550044, 0xaaaaaaaa, 0xaaaa0000, 0xaaaaaaaa, 0xaaaa0044, 0xcccccccc, 0xcccc0088, 0xcccccccc, 0xcccc00cc, 0xffffffff, 0xffff0088, 0xffffffff, 0xffff00cc, 0x44004411, 0x44221100, 0x44004411, 0x44221144,
0x44884499, 0x44aa1100, 0x44884499, 0x44aa1144, 0x55555555, 0x55550000, 0x55555555, 0x55550044, 0xaaaaaaaa, 0xaaaa0000, 0xaaaaaaaa, 0xaaaa0044, 0xcccccccc, 0xcccc0088, 0xcccccccc, 0xcccc00cc, 0xffffffff, 0xffff0088, 0xffffffff, 0xffff00cc,
0x44004411, 0x44221100, 0x44004411, 0x44221144, 0x44884499, 0x44aa1100, 0x44884499, 0x44aa1144, 0x55555555, 0x55550000, 0x55555555, 0x55550044, 0xaaaaaaaa, 0xaaaa0000, 0xaaaaaaaa, 0xaaaa0044, 0xcccccccc, 0xcccc0088, 0xcccccccc, 0xcccc00cc,
0xffffffff, 0xffff0088, 0xffffffff, 0xffff00cc,
0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333,
0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33
};

unsigned int my_telod_128_4tram[128] = {
0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff, 0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955,
0x33333333, 0x33339933, 0x3333ff33, 0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33, 0x5d80dd91, 0xbbffbbee, 0xbbeed5f7, 0xbbddeeff, 0xbbeed5f7, 0xbbffbbee, 0xbbeed5d5, 0xbbddeebb, 0xbbaad591, 0xbb77bb66, 0xbb66d5b3, 0xbb55eeff, 0xbb66d5b3, 0xbb77bb66, 0xbb66d591, 0xbb55eebb, 0xb380ccb3, 0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff, 0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33,
0x5d80dd91, 0xbbffbbee, 0xbbeed5f7, 0xbbddeeff, 0xbbeed5f7, 0xbbffbbee, 0xbbeed5d5, 0xbbddeebb, 0xbbaad591, 0xbb77bb66, 0xbb66d5b3, 0xbb55eeff, 0xbb66d5b3, 0xbb77bb66, 0xbb66d591, 0xbb55eebb, 0xb380ccb3, 0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff,
0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33,
0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33, 0x5555d56e};

my_log(unsigned int num)
{
        unsigned int i, x, power;

        if (num == 1)
                return (0);

        power = 1;
        for (i = 1; i < 256; i++) {
                power *= 2;     
                if (power == num)
        		return (i);
        }
};

float
fabs(float num)
{
        if (num < 0.0)
                return (num * (-1.0));
        else
                return (num);
}

float
floorf(float orignum)
{
        int int_num;
        float floornum;

        printf("floor orignum: %f, ", orignum);
        int_num = (int)(orignum * 1);
        floornum = (float)(int_num * 1.0);;
        printf("int_num: %d, floornum: %f\n", int_num, floornum);

        if ((orignum < 0.0) && (orignum < floornum))
                floornum -= 1.0;

        return (floornum);
}


float
ceilf(float orignum)
{
        int int_num;
        float ceilnum;

        printf("ceil orignum: %f, ", orignum);
        int_num = (int)(orignum * 1);
        ceilnum = (float)(int_num * 1.0);

        if ((orignum < 0.0) && (orignum < ceilnum))
                ceilnum -= 1.0;                             

        if (orignum > ceilnum)
                ceilnum += 1.0;

        return (ceilnum);
}

#define PARSE_TRI_ARGS_C(r, g, b, a) {  \
        atob(&argv[1][0], &r);           \
        argc--; argv++;                 \
        atob(&argv[1][0], &g);           \
        argc--; argv++;                 \
        atob(&argv[1][0], &b);           \
        argc--; argv++;                 \
        atob(&argv[1][0], &a);           \
        argc--; argv++;                 \
}

#define PARSE_TRI_ARGS(x, y, z, r, g, b, a) {   \
        atob(&argv[1][0], &x);                   \
        argc--; argv++;                         \
        atob(&argv[1][0], &y);                   \
        argc--; argv++;                         \
        atob(&argv[1][0], &z);                   \
        argc--; argv++;                         \
        PARSE_TRI_ARGS_C(r, g, b, a);           \
}

struct range {
	int ra_base;
	int ra_count;
	int ra_size;
};

/*
 * _digit -- convert the ascii representation of a digit to its
 * binary representation
 */
unsigned
_digit(char c)
{
	unsigned d;

	if (isdigit(c))
		d = c - '0';
	else if (isalpha(c)) {
		if (isupper(c))
			c = _tolower(c);
		d = c - 'a' + 10;
	} else
		d = 999999; /* larger than any base to break callers loop */

	return(d);
}

/*
 * atob -- convert ascii to binary.  Accepts all C numeric formats.
 */
char *
atob(const char *cp, int *iptr)
{
	register unsigned base = 10;
	register int value = 0;
	register unsigned d;
	int minus = 0;
	int overflow = 0;

	*iptr = 0;
	if (!cp)
		return(0);

	while (isspace(*cp))
		cp++;

	while (*cp == '-') {
		cp++;
		minus = !minus;
	}

	/*
	 * Determine base by looking at first 2 characters
	 */
	if (*cp == '0') {
		switch (*++cp) {
		case 'X':
		case 'x':
			base = 16;
			cp++;
			break;

		case 'B':	/* a frill: allow binary base */
		case 'b':
			base = 2;
			cp++;
			break;
		
		default:
			base = 8;
			break;
		}
	}

	while ((d = _digit(*cp)) < base) {
		if ((unsigned)value > ((unsigned)-1/base))
			overflow++;
		value *= base;
		if ((unsigned)value > ((unsigned)-1 - d))
			overflow++;
		value += d;
		cp++;
	}

	if (overflow || (value == 0x80000000 && minus))
		printf("WARNING: conversion overflow\n");
	if (minus)
		value = -value;

	*iptr = value;
	return((char*)cp);
}

char *
atobu(char *cp, unsigned *intp)
{
	cp = atob(cp, (int *)intp);
	switch (*cp) {
	  default:			/* unknown */
		return (char*)cp;
	  case 'k':			/* kilobytes */
	  case 'K':
		*intp *= 0x400;
		break;
	  case 'm':			/* megabytes */
	  case 'M':
		*intp *= 0x100000;
		break;
	  case 'p':			/* pages */
	  case 'P':
		*intp *= 4096;
		break;
	}
	return ((char*)cp + 1);
}
/*
 * Parse a range expression having one of the following forms:
 *
 *	base:limit	base and limit are byte-addresses
 *	base#count	base is a byte-address, count is in size-units
 *	base		byte-address, implicit count of 1 size-unit
 *
 * Return 0 on syntax error, 1 otherwise.  Return in ra->ra_base the base
 * address, truncated to be size-aligned.  Return in ra->ra_count a size-unit
 * count, and the size in bytes in ra->ra_size.
 *
 * NB: size must be a power of 2.
 */
int
parse_range(cp, size, ra)
	register char *cp;
	register int size;
	register struct range *ra;
{
	register unsigned int sizemask;
	unsigned temp;

	/* ASSERT((size & (size - 1)) == 0); */
	cp = atobu(cp, (unsigned *) &temp);
	ra->ra_base = (unsigned long long) temp;
	sizemask = ~(size - 1);
	ra->ra_base &= sizemask;
	switch (*cp) {
	  case ':':
		if (*atobu(++cp, (unsigned *)&ra->ra_count))
			return 0;
		/*
		 * ra_count holds the range's limit address, so turn it into
		 * a size-unit count.
		 */
		ra->ra_count = ((ra->ra_count&sizemask) - ra->ra_base) / size;
		break;

	  case '#':
		if (*atobu(++cp, (unsigned *)&ra->ra_count))
			return 0;
		break;

	  case '\0':
		ra->ra_count = 1;
		break;

	  default:
		return 0;
	}
	if (ra->ra_count == 0)		/* MIPS allowed count < 0 */
		ra->ra_count = 1;
	ra->ra_size = size;		/* XXX needed? */
	return 1;
}

int
check_float(float tom)
{
	fprintf(stdout, "Tom is %f\n", tom);
        return 1;
}


main(int argc, char * argv[])
{
	unsigned int i, actual, rssnum, ppnum, status, rdend_addr, zoom=0;
        unsigned int addr, errors = 0, bad_arg = 0, cnt, data, rdstart_addr;
        unsigned int x, y,rflag = 0, aflag = 0, pflag =0, dflag = 0, start, end;
	unsigned long long _val;
	unsigned int testnum, test2, test3, test4, t5, t6, t7;
	int loopcount = 1;
	struct range range;
	float hello, fl_test, fl_test4;
	double d_test4;
	char *str[50];
	unsigned int block_array[384];

	strcpy(str, argv[0]);
	printf("argv: %s\n", str);
	hello = 1.75;

               argc--; argv++;
	printf("argc: %d, argv: %s\n", argc, *argv);
	/* get the args */
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') { 
#if 0
	printf("argv[0][0]: %s, argv[1][0]: %s\n", &argv[0][0], &argv[1][0]);
	printf("argv[0][1]: %s\n", &argv[0][1]);
#endif
           switch (argv[0][1]) { 
                case 'r':
                        /* RSS select  - required */
			printf(" Case r\n");
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], &rssnum); argc--; argv++;
#if 0
				printf("1:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("Has space, rssnum: %d\n", rssnum);
			}
			else {
                        	atob(&argv[0][2], &rssnum);/* argc--; argv++; */
#if 0
				printf("2:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("No space, rssnum: %d\n", rssnum);
			}
                        rflag++; break;
                case 'a':
                     /* RDRAM reg address select  - required */
			printf(" Case a\n");
                     aflag++;
			if (argv[0][2]=='\0') {
                        	parse_range(&argv[1][0], sizeof(short), &range); 
				argc--; argv++;
#if 0
				printf("1:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("Has space, base: 0x%x, count: %d, size %d\n", range.ra_base, range.ra_count, range.ra_size);
			}
			else {
                        	parse_range(&argv[0][2], sizeof(short), &range); 
#if 0
				printf("2:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("No space, base: 0x%x, count: %d, size %d\n", range.ra_base, range.ra_count, range.ra_size);
			}

                     /* we're reading 2 bytes at a time, so use a size of 16.
                      * We're really using 9-bit bytes but that doesn't matter
                      */
                     break;
                case 'p':
                        /* PP select  - required */
			printf(" Case p\n");
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], &ppnum); argc--; argv++;
#if 0
				printf("1:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("Has space, ppnum: %d\n", ppnum);
			}
			else {
                        	atob(&argv[0][2],&ppnum); /* argc--; argv++; */
#if 0
				printf("2:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("No space, ppnum: %d\n", ppnum);
			}
                        pflag++; break;
                case 'd':
                        /* data - required */
			printf(" Case d\n");
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], &data); argc--; argv++;
#if 0
				printf("1:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("Has space, data: %d\n", data);
			}
			else {
                        	atob(&argv[0][2], &data); /* argc--; argv++; */
#if 0
				printf("2:argv[0][0]: %s\n", &argv[0][0]);
#endif
				printf("No space, data: %d\n", data);
			}
                        dflag++; break;
		case 'l':
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], &loopcount); argc--; argv++;
				printf("Has space, loopcount: %d\n", loopcount);
			}
			else {
                        	atob(&argv[0][2], &loopcount);
				printf("No space, loopcount: %d\n", loopcount);
			}
			if (loopcount < 0) {
				printf("Error. Loop must be > or = to 0\n");
				return (0); /* bad_arg++; */
			}
			break;
		case 'z':
			zoom = 1;
			if (zoom)
				printf("Zoom is %d\n", zoom);
			break;
		case 't':
			if (argv[0][2]=='\0') {
				PARSE_TRI_ARGS(testnum, test2, test3, test4, t5, t6, t7);
			}
			else
				printf("Need a space after -%c\n", argv[0][1]);
			break;
                default: printf(" Case bad\n");bad_arg++; break;
           }
#if 0
			printf("argv[0][2]: %s\n", &argv[0][2]);
			printf("argv[1][0]: %s\n", &argv[1][0]);
			printf("argv[1][1]: %s\n", &argv[1][1]);
#endif
                argc--; argv++;
        }

	fl_test = (float)testnum;
	printf("testnum: %d, test2: %d, test3: %d, test4: %d, t5:%d, t6:%d, t7:%d\n",
		testnum, test2, test3, test4, t5, t6, t7);
	printf("zoom is %d\n", zoom);
	printf("bad_arg: %d, argc: %d\n", bad_arg, argc);
	if ((bad_arg) || (argc)) { /* no arguments specified, but something is there */
		printf("Usage: foof foo \n");
		return (0);
	}

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
		printf("loopcount is : %d\n", loopcount);
	}
	
	/* check_float(hello); */
	_val = 0x8000000000000000;
	_val |= ((unsigned long long)(rssnum & 0x3ffffff) << 26)|(ppnum & 0x3ffffff);


        _val |= ((unsigned long long)(rssnum) << 62);
	_val |= ((unsigned long long)(data) << 52);
        printf("val: 0x%llx, 0x%llx\n", _val, (1 << 63));

	printf("fabs(5.0): %f, fabs(-5.0): %f\n", fabs(5.0), fabs(5.0 * -1.0));

	i = my_log(1); printf("i: %d, 1\n", i);
	i = my_log(2); printf("i: %d, 2\n", i);
	i = my_log(4); printf("i: %d, 4\n", i);
	i = my_log(64); printf("i: %d, 64\n", i);
	i = my_log(256); printf("i: %d, 256\n", i);
	i = my_log(1024); printf("i: %d, 1024\n", i);
	i = my_log(4096); printf("i: %d, 4096\n", i);

	_val = 0;
	for (i = 0; i < 30; i++)
		_val += my_points_array[i];	
	printf("points: %llx\n", _val);
	
	_val = 0;
	for (i = 0; i < 64; i++)
		_val += my_tex_poly[i];	
	printf("tex: %llx\n", _val);
	
	_val = 0;
	for (i = 0; i < 48; i++)
		_val += my_logicop_array[i];	
	printf("logic: %llx\n", _val);
	
	_val = 0;
	for (i = 0; i < 384; i++)
		_val += red_tri[i];	
	printf("red_tri: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 384; i++)
		_val += green_tri[i];	
	printf("green_tri: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 384; i++)
		_val += blue_tri[i];	
	printf("blue_tri: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 384; i++)
		_val += alpha_tri[i];	
	printf("alpha_tri: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 384; i++)
		_val += stip_tri[i];	
	printf("stip_tri: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 640; i++)
		_val += char_array[i];	
	printf("char_array: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 221; i++)
		_val += lines_array[i];	
	printf("lines_array: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t0[i];	
	printf("s1t0: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t1[i];	
	printf("s1t1: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t2[i];	
	printf("s1t2: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t3[i];	
	printf("s1t3: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t4[i];	
	printf("s1t4: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t5[i];	
	printf("s1t5: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t6[i];	
	printf("s1t6: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t0[i];	
	printf("s2t0: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t1[i];	
	printf("s2t1: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t2[i];	
	printf("s2t2: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t3[i];	
	printf("s2t3: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t4[i];	
	printf("s2t4: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t5[i];	
	printf("s2t5: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t6[i];	
	printf("s2t6: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s1t7[i];	
	printf("s1t7: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += s2t7[i];	
	printf("s2t7: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 128; i++)
		_val += my_telod[i];	
	printf("my_telod: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 128; i++)
		_val += my_telod_128_4tram[i];	
	printf("my_telod_128_4tram: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 32; i++)
		_val += my_detail_tex[i];	
	printf("my_detail_tex: %llx\n", _val);

	_val = 0;
	for (i = 0; i < 256; i++)
		_val += my_ldrsmpl_array[i];	
	printf("my_ldrsmpl_array: %llx\n", _val);
	
	_val = 0;
	for (i = 0; i < 32; i++) {
		_val += my_dither_array[i];	
		printf("tot: %llx, item: %x\n", _val, my_dither_array[i]);
  	}	
	printf("my_dither_array: %llx\n", _val);
	
	 for (y = 0; y < 16; y++) {
                for (x = 0; x < 24; x+=2) {
                   if (y % 2) { /* odd rows */
                        block_array[x + 24*y] = 0xff7f7f7f;
                        block_array[x+1 + 24*y] = 0xff808080;
                   }
                   else { /* even rows */
                        block_array[x + 24*y] = 0xff808080;
                        block_array[x+1 + 24*y] = 0xff7f7f7f;
                   }
                }
        }
	_val = 0;
	for (i = 0; i < 384; i++)
		_val += block_array[i];	
	printf("block_array : %llx\n", _val);
	



#if 0
	for (rssnum = 1; rssnum <= 4; rssnum++) {
		start = 0; end = rssnum;

		printf("numRE4s: %d\n", rssnum);
		for (i = start; i < end; i++) {
			printf("\t i: %d\n", i);		
			printf("\t Doing stuff... \n");
			if ((i == (end -1)) && (i) && !(i == 4)) {	
				i = 3; end = 5;
				printf("\t\t i: %d, end: %d\n", i, end);
			}
		}
		printf("Done\n\n");
	}
#endif

	i = 255;
	x = 127;
	hello = 0.0039215686 * x;
	printf("float mult: %f\n", hello);
	x = 1;
	hello = (0.0039215686 * x)/i;
	printf("float mult: %f\n", hello);

	hello = ceilf(23.4);
	printf("float mult: %f\n", hello);
	hello = floorf(23.4);
	printf("float mult: %f\n", hello);

	printf("float-as_int: %d\n", hello);
}
