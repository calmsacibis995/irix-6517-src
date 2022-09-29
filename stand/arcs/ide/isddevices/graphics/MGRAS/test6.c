#include <sys/types.h>
#include "mgras_color_z.h"
#include "mgras_z.h"
#include "mgras_tetr.h"
#include "mgras_tex_1d_out.h"
#include "mgras_tex_3d_out.h"
#include "mgras_tex_scistri_out.h"
#include "mgras_tex_linegl_out.h"
#include "mgras_tex_load_out.h"
#include "mgras_tex_lut4d_out.h"
#include "mgras_tex_mddma_out.h"
#include "mgras_tex_persp_out.h"
#include "mgras_tex_mag_out.h"
#include "mgras_tex_detail_out.h"
#include "mgras_tex_bordertall_out.h"
#include "mgras_tex_borderwide_out.h"
#include "mgras_tex_poly.h"
#include "mgras_tex_terev_revB_out.h"
#include "mgras_tex_terev_revD_out.h"
/* #include "out.new.h" */

#ifndef _STANDALONE
#include <stdio.h>
#endif


struct range {
        int ra_base;
        int ra_count;
        int ra_size;
};

__uint32_t opt43_array[1024] = {0xff88fff4, 0xffffffff,
0xffffffff, 0xffffffff,
0xfffffff6, 0xffd9ffb8,
0xff9fff7d, 0xff69ff67,
0xff68ff68, 0xff6bff73,
0xff7bff88, 0xffafffe0,
0xfffeffff, 0xffffffff,
0xfffeffe6, 0xffa8ff3d,

0xff16ff32, 0xff8affd0,
0xfffdffff, 0xfffffffe,
0xffd8ffb0, 0xff9aff80,
0xff6fff61, 0xff5cff5b,
0xff5aff5b, 0xff5bff64,
0xff69ff67, 0xff64ff66,
0xff7cffc0, 0xfff5ffe2,
0xff9dff62, 0xff26ff11,

0xff13ff1a, 0xff34ff51,
0xff50ffb6, 0xfffdffc1,
0xffa1ff8f, 0xff7fff6d,
0xff6cff69, 0xff62ff5d,
0xff5fff5e, 0xff5cff5e,
0xff56ff49, 0xff3aff2f,
0xff26ff24, 0xff49ff5e,
0xff52ff30, 0xff16ff0d,

0xff0fff13, 0xff1aff53,
0xff3bff27, 0xff54ff45,
0xff43ff5b, 0xff73ff72,
0xff79ff6e, 0xff60ff5f,
0xff5fff5d, 0xff5eff5e,
0xff46ff28, 0xff1cff1c,
0xff1aff1a, 0xff19ff33,
0xff3bff1b, 0xff0fff0d,

0xff0fff10, 0xff16ff3f,
0xff3dff34, 0xff20ff1d,
0xff13ff1a, 0xff65ff72,
0xff7aff7d, 0xff74ff75,
0xff7dff6c, 0xff5bff5c,
0xff3cff12, 0xff14ff1e,
0xff24ff1b, 0xff1dff32,
0xff2cff17, 0xff1fff10,

0xff0fff10, 0xff10ff2a,
0xff3aff36, 0xff37ff1b,
0xff18ff11, 0xff5bff74,
0xff88ff8c, 0xff81ff82,
0xff85ff7f, 0xff6fff5d,
0xff34ff0e, 0xff13ff20,
0xff20ff19, 0xff1cff30,
0xff1cff11, 0xff1aff12,

0xff0cff10, 0xff0fff24,
0xff30ff33, 0xff39ff1f,
0xff1aff13, 0xff5fff72,
0xff96ff96, 0xff7bff73,
0xff7dff83, 0xff78ff6a,
0xff3fff0e, 0xff12ff23,
0xff22ff19, 0xff1cff2f,
0xff18ff11, 0xff10ff0f,

0xff0aff0f, 0xff0fff1d,
0xff2cff2f, 0xff30ff1d,
0xff18ff45, 0xff68ff74,
0xff8bff6c, 0xff62ff61,
0xff68ff75, 0xff6bff6c,
0xff67ff25, 0xff12ff27,
0xff2bff18, 0xff21ff2c,
0xff14ff11, 0xff10ff0e,

0xff0aff0d, 0xff0eff1a,
0xff26ff29, 0xff2bff1d,
0xff2cff69, 0xff6aff62,
0xff54ff61, 0xff63ff58,
0xff53ff4a, 0xff50ff66,
0xff68ff55, 0xff15ff29,
0xff32ff18, 0xff29ff26,
0xff0fff0f, 0xff0eff0d,

0xff0aff0c, 0xff0cff19,
0xff22ff21, 0xff26ff1e,
0xff4fff64, 0xff58ff49,
0xff67ff7d, 0xff6dff68,
0xff6fff6c, 0xff50ff53,
0xff63ff63, 0xff25ff2a,
0xff2bff19, 0xff31ff20,
0xff0eff0e, 0xff0eff0e,

0xff0bff0b, 0xff0bff12,
0xff20ff1d, 0xff1eff1b,
0xff64ff6d, 0xff66ff5b,
0xff6cff61, 0xff54ff58,
0xff58ff65, 0xff63ff70,
0xff6fff68, 0xff32ff24,
0xff20ff1a, 0xff36ff18,
0xff0dff0e, 0xff0fff0e,

0xff0aff0c, 0xff0cff0e,
0xff1bff1b, 0xff16ff17,
0xff75ff95, 0xff92ff78,
0xff68ffac, 0xff9dff97,
0xff8dff6b, 0xff7dff8b,
0xff89ff78, 0xff50ff2a,
0xff19ff1b, 0xff35ff12,
0xff0dff0e, 0xff10ff0f,

0xff0aff0b, 0xff0cff0b,
0xff18ff1a, 0xff12ff49,
0xff92ffb3, 0xffb7ffa1,
0xff90ffa3, 0xffbfffac,
0xff7fff80, 0xff8dff91,
0xff8eff7d, 0xff66ff52,
0xff15ff1a, 0xff2cff10,
0xff0eff0e, 0xff0eff0e,

0xff0bff0c, 0xff0bff0b,
0xff15ff1a, 0xff15ff68,
0xff92ffb0, 0xffa6ff81,
0xff75ff87, 0xffa5ff97,
0xff6bff60, 0xff61ff73,
0xff83ff7b, 0xff64ff40,
0xff18ff1a, 0xff20ff10,
0xff0eff0d, 0xff0dff0d,

0xff0bff0a, 0xff0bff0a,
0xff15ff1b, 0xff15ff40,
0xff98ffab, 0xff7bff4c,
0xff38ff44, 0xff7eff81,
0xff46ff37, 0xff39ff4f,
0xff6cff7c, 0xff5dff2c,
0xff1aff19, 0xff18ff0e,
0xff0dff0d, 0xff0cff0d,

0xff0aff0a, 0xff0aff0a,
0xff13ff1b, 0xff15ff31,
0xff8cff69, 0xff39ff1d,
0xff1aff2d, 0xff66ff6a,
0xff41ff23, 0xff1dff26,
0xff3cff63, 0xff4bff1e,
0xff19ff16, 0xff14ff0d,
0xff0cff0c, 0xff0cff0d,

0xff09ff0a, 0xff0aff0a,
0xff10ff1c, 0xff16ff27,
0xff67ff51, 0xff37ff2c,
0xff28ff3d, 0xff5bff63,
0xff4dff3a, 0xff2dff33,
0xff3cff40, 0xff2bff1d,
0xff27ff16, 0xff10ff0c,
0xff0bff0b, 0xff0bff0d,

0xff08ff08, 0xff09ff09,
0xff0aff1d, 0xff20ff2b,
0xff48ff5c, 0xff54ff43,
0xff37ff50, 0xff65ff69,
0xff63ff5a, 0xff4bff35,
0xff30ff2e, 0xff21ff1b,
0xff10ff16, 0xff0eff0c,
0xff0cff0b, 0xff0aff0c,

0xff08ff07, 0xff08ff09,
0xff0aff1a, 0xff18ff32,
0xff3bff5e, 0xff73ff56,
0xff51ff3e, 0xff5dff63,
0xff59ff4f, 0xff3eff35,
0xff2eff28, 0xff1dff15,
0xff14ff17, 0xff0cff0b,
0xff0cff0b, 0xff0bff0a,

0xff09ff09, 0xff09ff0a,
0xff0bff16, 0xff16ff23,
0xff30ff4f, 0xff5eff4f,
0xff45ff43, 0xff41ff56,
0xff4cff3d, 0xff35ff34,
0xff2aff1e, 0xff15ff19,
0xff19ff17, 0xff0dff0c,
0xff0cff0c, 0xff0aff0a,

0xff09ff09, 0xff09ff09,
0xff0aff11, 0xff1aff1b,
0xff26ff42, 0xff58ff4c,
0xff46ff4b, 0xff3dff47,
0xff46ff33, 0xff38ff32,
0xff2aff1c, 0xff18ff1b,
0xff1eff13, 0xff0dff0c,
0xff0cff0c, 0xff0bff0a,

0xff09ff08, 0xff08ff09,
0xff09ff0b, 0xff1eff13,
0xff1cff2b, 0xff47ff4c,
0xff47ff4c, 0xff45ff38,
0xff3cff33, 0xff33ff2e,
0xff23ff19, 0xff18ff1e,
0xff1dff0f, 0xff0dff0c,
0xff0dff0c, 0xff0bff0a,

0xff0bff09, 0xff09ff08,
0xff08ff09, 0xff19ff15,
0xff15ff21, 0xff32ff2d,
0xff38ff47, 0xff3cff28,
0xff33ff32, 0xff24ff1e,
0xff1bff18, 0xff16ff22,
0xff1bff0c, 0xff0dff0c,
0xff0cff0c, 0xff0bff0a,

0xff0aff09, 0xff09ff09,
0xff08ff08, 0xff0eff1c,
0xff11ff11, 0xff1fff23,
0xff1bff2d, 0xff2cff24,
0xff1eff19, 0xff1eff1d,
0xff1bff18, 0xff18ff22,
0xff14ff0d, 0xff0eff0d,
0xff0cff0c, 0xff0bff0b,

0xff0cff0a, 0xff09ff0a,
0xff08ff08, 0xff0aff18,
0xff18ff11, 0xff19ff20,
0xff19ff2c, 0xff29ff1e,
0xff1cff1c, 0xff1bff1f,
0xff1cff17, 0xff21ff20,
0xff0cff0c, 0xff0dff0d,
0xff0eff0b, 0xff0aff0b,

0xff0aff0a, 0xff09ff0a,
0xff09ff09, 0xff09ff0f,
0xff1fff18, 0xff1cff20,
0xff17ff29, 0xff31ff1e,
0xff18ff1a, 0xff1fff20,
0xff1cff22, 0xff24ff14,
0xff0dff0d, 0xff0dff0d,
0xff0cff0b, 0xff0aff0a,

0xff0aff0a, 0xff0aff09,
0xff08ff09, 0xff09ff0a,
0xff1aff27, 0xff25ff28,
0xff1cff1e, 0xff2bff24,
0xff24ff24, 0xff23ff21,
0xff28ff28, 0xff17ff0d,
0xff0dff0c, 0xff0cff0b,
0xff0bff0c, 0xff0cff0b,

0xff0aff0a, 0xff0aff0a,
0xff09ff0a, 0xff0aff0a,
0xff0cff22, 0xff2dff2e,
0xff23ff1a, 0xff1eff26,
0xff29ff25, 0xff26ff2b,
0xff2bff1b, 0xff0dff0c,
0xff0cff0b, 0xff0bff0c,
0xff0bff0d, 0xff10ff0c,

0xff09ff0a, 0xff0dff0b,
0xff09ff09, 0xff0aff0a,
0xff0aff0e, 0xff26ff38,
0xff30ff1f, 0xff20ff25,
0xff29ff2e, 0xff2fff29,
0xff1aff0d, 0xff0cff0c,
0xff0bff0b, 0xff0bff0b,
0xff0aff0b, 0xff0dff0d,

0xff09ff0b, 0xff10ff12,
0xff0aff0a, 0xff0bff0a,
0xff0bff0b, 0xff0fff25,
0xff33ff24, 0xff1dff28,
0xff35ff2d, 0xff24ff15,
0xff0cff0c, 0xff0bff0c,
0xff0cff0c, 0xff0cff0b,
0xff0bff0b, 0xff0cff0d,

0xff09ff0b, 0xff0cff0e,
0xff0aff0c, 0xff0bff0b,
0xff0cff0b, 0xff0bff0d,
0xff17ff22, 0xff1dff13,
0xff19ff12, 0xff0eff0c,
0xff0cff0b, 0xff0bff0b,
0xff0bff0d, 0xff0cff0b,
0xff0bff0b, 0xff0bff0d,

0xff0bff0d, 0xff0dff0c,
0xff0cff0c, 0xff0cff0d,
0xff0fff0e, 0xff0dff0c,
0xff0dff10, 0xff12ff0f,
0xff0eff0e, 0xff0dff0c,
0xff0dff0d, 0xff0cff0c,
0xff0dff0d, 0xff0dff0d,
0xff0cff0c, 0xff0cff0d};


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

int
do_checksum(unsigned int val, unsigned long long *csum)
{
	unsigned int i;

	if (*csum & 0x1) 
        	*csum = (*csum >>1) + 0x8000;
        else
        	*csum >= 1;
        *csum += val;
}

main(int argc, char * argv[])
{
	unsigned int i, actual, rssnum, ppnum, status, rdend_addr, zoom=0;
        unsigned int addr, errors = 0, bad_arg = 0, cnt, data, rdstart_addr;
        unsigned int x, y,rflag = 0, aflag = 0, pflag =0, dflag = 0, start, end;
	unsigned long long _val, _checksum;
	unsigned int j, tmp, tmp_hi, tmp_lo, tmp_hi_lsByte, tmp_lo_lsByte;
	unsigned int word_hi, word_lo;
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

#if 0
	/* create the expected data array for opt43 */
	for (i = 0; i < 32; i++) {

	   /* create the 1st 4 pixels */
	   for (j = 14; j < 16; j++) {
		tmp = opt43_array[i*16 + j];
		tmp_hi = (tmp & 0xffff0000) >> 16;
		tmp_hi_lsByte = (tmp_hi & 0xff);
		tmp_lo = (tmp & 0xffff);
		tmp_lo_lsByte = (tmp_lo & 0xff);
		word_hi = (tmp_hi << 16) | (tmp_hi_lsByte << 8) | tmp_hi_lsByte;
		word_lo = (tmp_lo << 16) | (tmp_lo_lsByte << 8) | tmp_lo_lsByte;
		printf("0x%x, 0x%x, ", word_hi, word_lo);
	   }
		
	   /* create the 32 pixel tile */
	   for (j = 0; j < 16; j++) {
		tmp = opt43_array[i*16 + j];
		tmp_hi = (tmp & 0xffff0000) >> 16;
		tmp_hi_lsByte = (tmp_hi & 0xff);
		tmp_lo = (tmp & 0xffff);
		tmp_lo_lsByte = (tmp_lo & 0xff);
		word_hi = (tmp_hi << 16) | (tmp_hi_lsByte << 8) | tmp_hi_lsByte;
		word_lo = (tmp_lo << 16) | (tmp_lo_lsByte << 8) | tmp_lo_lsByte;
		printf("0x%x, 0x%x, ", word_hi, word_lo);
	   }

	   /* create the last 4 pixels */
	   for (j = 0; j < 2; j++) {
		tmp = opt43_array[i*16 + j];
		tmp_hi = (tmp & 0xffff0000) >> 16;
		tmp_hi_lsByte = (tmp_hi & 0xff);
		tmp_lo = (tmp & 0xffff);
		tmp_lo_lsByte = (tmp_lo & 0xff);
		word_hi = (tmp_hi << 16) | (tmp_hi_lsByte << 8) | tmp_hi_lsByte;
		word_lo = (tmp_lo << 16) | (tmp_lo_lsByte << 8) | tmp_lo_lsByte;
		printf("0x%x, 0x%x, ", word_hi, word_lo);
	   }

	   printf("\n");
	}
#endif

	
	_val = 0;
	for (i = 0; i < 128; i++) {
		_val += ldrsmpl_1tram_array[i];
	}

	printf("0x%llx is checksum for ldrsmpl_1tram_array\n", _val);

	_val = _checksum = 0;
	for (i = 0; i < 205332; i++) {
		_val += tex_1d_out_array[i];
		do_checksum(tex_1d_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_1d_out_array\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 10000; i++) {
		_val += tex_3d_out_array[i];
		do_checksum(tex_3d_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_3d_out_array\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 12544; i++) {
		_val += tex_load_out_array[i];
		do_checksum(tex_load_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_load\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 10000; i++) {
		_val += tex_scistri_out_array[i];
		do_checksum(tex_scistri_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_scistri\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 5184; i++) {
		_val += tex_linegl_out_array[i];
		do_checksum(tex_linegl_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_linegl\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 12152; i++) {
		_val += tex_persp_out_array[i];
		do_checksum(tex_persp_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_persp\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 35616; i++) {
		_val += tex_mag_out_array[i];
		do_checksum(tex_mag_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_mag\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 123824; i++) {
		_val += tex_detail_out_array[i];
		do_checksum(tex_detail_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_detail\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 28518; i++) {
		_val += tex_bordertall_out_array[i];
		do_checksum(tex_bordertall_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for bordertall\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 28616; i++) {
		_val += tex_borderwide_out_array[i];
		do_checksum(tex_borderwide_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for borderwide\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 65536; i++) {
		_val += tex_lut4d_out_array[i];
		do_checksum(tex_lut4d_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for 4dlut\n", _checksum);
	
	_val = _checksum = 0;
	for (i = 0; i < 65536; i++) {
		_val += tex_mddma_out_array[i];
		do_checksum(tex_mddma_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for tex_mddma_out_array\n", _checksum);

	_val = _checksum = 0;
	for (i = 0; i < 5700; i++) {
		_val += terevB_out_array[i];
		do_checksum(terevB_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for terev_B_array\n", _checksum);

	_val = _checksum = 0;
	for (i = 0; i < 5700; i++) {
		_val += terevD_out_array[i];
		do_checksum(terevD_out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for terev_D_array\n", _checksum);

/*
	_val = _checksum = 0;
	for (i = 0; i < 28518; i++) {
		_val += out_array[i];
		do_checksum(out_array[i], &_checksum);
	}
	printf("0x%llx is checksum for out.new\n", _checksum);
*/	
}
