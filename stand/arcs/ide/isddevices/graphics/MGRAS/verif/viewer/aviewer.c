#if 0
#include <GL/gl.h>
#include <GL/glx.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#define WSIZEX 1280
#define WSIZEY 512
#define RGBA32SB		2
#define MAX_HEIGHT	1024
#define MAX_WIDTH	1344
#define BIT(data, bn, fp)         (((data >> bn) & 1) << fp)
#define BITX(data, bn, fp)         (((data[bn/9] >> (bn % 9)) & 1) << fp)
#define STXPIXS 192
#define STYPIXS 16
#define STPIXS (STXPIXS * STYPIXS)

int xs;
int rdmp[6];
ushort rdm[6][1 << 21]; /* rdram storage */
ushort pixs[MAX_HEIGHT*MAX_WIDTH][4]; /* pixel storage */


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

                case 'B':       /* a frill: allow binary base */
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
          default:                      /* unknown */
                return (char*)cp;
          case 'k':                     /* kilobytes */
          case 'K':
                *intp *= 0x400;
                break;
          case 'm':                     /* megabytes */
          case 'M':
                *intp *= 0x100000;
                break;
          case 'p':                     /* pages */
          case 'P':
                *intp *= 4096;
                break;
        }
        return ((char*)cp + 1);
}

void pp_format_from_32_sb_rgba(
			       unsigned short *s,
			       unsigned short *r,
			       unsigned short *g,
			       unsigned short *b,
			       unsigned short *a)
{
 *r = *g = *b = *a =0;
    *r=	BITX(s, 10,  4) |
	BITX(s,  7,  5) |
	BITX(s,  4,  6) |
	BITX(s,  1,  7) |
	BITX(s, 22,  8) |
	BITX(s, 19,  9) |
	BITX(s, 16, 10) |
	BITX(s, 13, 11) |
	BITX(s, 10, 12) |
	BITX(s,  7, 13) |
	BITX(s,  4, 14) |
	BITX(s,  1, 15);
    *g=	BITX(s,  9,  4) |
	BITX(s,  6,  5) |
	BITX(s,  3,  6) |
	BITX(s,  0,  7) |
	BITX(s, 21,  8) |
	BITX(s, 18,  9) |
	BITX(s, 15, 10) |
	BITX(s, 12, 11) |
	BITX(s,  9, 12) |
	BITX(s,  6, 13) |
	BITX(s,  3, 14) |
	BITX(s,  0, 15);
    *b=	BITX(s, 11,  4) |
	BITX(s,  8,  5) |
	BITX(s,  5,  6) |
	BITX(s,  2,  7) |
	BITX(s, 23,  8) |
	BITX(s, 20,  9) |
	BITX(s, 17, 10) |
	BITX(s, 14, 11) |
	BITX(s, 11, 12) |
	BITX(s,  8, 13) |
	BITX(s,  5, 14) |
	BITX(s,  2, 15);
    *a= BITX(s, 27,  4) |
	BITX(s, 26,  5) |
	BITX(s, 25,  6) |
	BITX(s, 24,  7) |
	BITX(s, 31,  8) |
	BITX(s, 30,  9) |
	BITX(s, 29, 10) |
	BITX(s, 28, 11) |
	BITX(s, 27, 12) |
	BITX(s, 26, 13) |
	BITX(s, 25, 14) |
	BITX(s, 24, 15);
}

void wrLoc(char *zeile, ushort *dat, ushort *rdm, char *fname, ulong *nadr) {
	int page;
	int byte;
	int i;
	short hd;
	char t[4] = "xxx";

	if (sscanf(zeile, "%3x.%3x ", &page, &byte) == 2) {
		*nadr = (page << 11) + byte;
		for (i = 3; i >= 0; i--) {
			strncpy(t, zeile + 17 - (3 * i), 3);
			if ((sscanf(t, "%3ho", &hd)) == 1) {
				dat[i] = rdm[(*nadr) + i] = hd | 0x8000;
			} else {
				dat[i] = rdm[(*nadr) + i] = 0;
			}
		}
	} else {
		fprintf(stderr, "Bad address format in input file = %s\n", fname);
		exit(__LINE__);
	}
}

void rdFile(register ushort *rdm, register char *fname) {
	FILE *inf;
	int usw = 0;
	char zeile[22];
	ushort dat[4];
	ushort odat[4];
	register int i;
	register ulong oadr;
	ulong nadr;

	if ((inf = fopen(fname, "r")) == NULL) {
		perror(fname);
		exit(__LINE__);
	}
	while (fgets(zeile, 22, inf) != NULL) {
		if (usw) {
			odat[0] = dat[0];
			odat[1] = dat[1];
			odat[2] = dat[2];
			odat[3] = dat[3];
			oadr = nadr + 4;
			wrLoc(zeile, dat, rdm, fname, &nadr);
			while (oadr < nadr) {
				for (i = 3; i >= 0; i--) {
					rdm[oadr + i] = odat[i];
				}
				oadr += 4;
			}
			usw = 0;
		} else {
			if (strncmp(zeile, "...", 3)) {
				wrLoc(zeile, dat, rdm, fname, &nadr);
			} else {
				usw = 1;
			}
		}
	}
	oadr = nadr + 4;
	nadr = 1 << 21;
	while (oadr < nadr) {
		for (i = 3; i >= 0; i--) {
			rdm[oadr + i] = dat[i];
		}
		oadr += 4;
	}
}

void
viewer(int rss, int w, int h, int fmt, char *vc) {

	char fn[] = "RE0_PPp_RDr.xmou";
	int pp;
	int rd;
	int imp;
	register int x;
	register int y;
	int ys;
	int sb;
	ushort *rgb;
	int pprd;
	int rp;
	register int xsv;
	register int ysv;
	register int ytmp;
	register int tmp;

	switch (fmt) {
		case RGBA32SB:
			break;
		default:
			fprintf(stderr, "Unknown pixel format at line %d file %s\n",
				__LINE__, __FILE__);
			exit(__LINE__);
			break;
	}

	switch (w) {
		case 1280:
			switch (h) {
				case 1024:
					sb = 0x240 << 11;
					xs = 7;
					ys = 64;
					break;
				default:
					fprintf(stderr, "Error in screen parameters. Exiting from file %s at line %d.\n", __FILE__, __LINE__);
					exit(__LINE__);
					break;
			}
			break;
		default:
			fprintf(stderr, "Unknown width %d at line %d file %s\n",
				w, __LINE__, __FILE__);
			exit(__LINE__);
			break;
	}

	fprintf(stderr, "Start rdFile.\n");

	for (pp = 0; pp <= 1; pp++) {
		for (rd = 0; rd <= 2; rd++) {
			sprintf(fn, "PP%d_RD%d.%s", pp, rd, vc);
			pprd = (pp * 3) + rd;
			rdmp[pprd] = sb;
			rdFile(rdm[pprd], fn);
		}
	}

	fprintf(stderr, "Finish rdFile. Start transforming.\n");

	for (ysv = 0; ysv < ys; ysv++) {
		for (xsv = 0; xsv < xs; xsv++) {
			for (y = 0; y < 16; y++) {
				for (x = 0; x < 192; x++) {
					ytmp = (ysv * 16) + y;
					imp = ((x >> 2) + (ytmp >> (rss >> 1))) % 3;
					pprd = ((x & 1) * 3) + imp;
					rp = rdmp[pprd];
					tmp = (ytmp * xs * 192) + (xsv * 192) + x;
					rgb = pixs[tmp];
					if (y < 20)
					pp_format_from_32_sb_rgba(rdm[pprd]+rp, rgb, rgb+1, rgb+2, rgb+3);
					rdmp[pprd] = rp + 4;
				}
			}
		}
	}

	fprintf(stderr, "Finish transforming.\n");
}

int
main(int argc, char *argv[]) {

	int x, y, xy ,i, ystart, yend, xstart, xend, width, height, do_hdr;
	int do_zeroes, first_x, first_y, first_nonzero, first_data;
	int max_x, max_y, min_x, min_y;
	FILE *fp;

	/* initial vals */
	ystart = 0;
	yend = MAX_HEIGHT;
	xstart = 0;
	xend = MAX_WIDTH;
	do_zeroes = 0;
	do_hdr = 0;
	first_nonzero = 1;
	first_data = 1;
	max_x = max_y = 0;
	min_x = 1280;
	min_y = 1024;

	fprintf(stderr, "Start\n");
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	   switch (argv[0][1]) {
		case 'x':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &xstart); argc--; argv++;
			}
			else {
				atob(&argv[0][2], &xstart);
			};
			break;
		case 'y':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &ystart); argc--; argv++;
			}
			else {
				atob(&argv[0][2], &ystart);
			};
			break;
		case 'w':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &width); argc--; argv++;
			}
			else {
				atob(&argv[0][2], &width);
			};
			xend = xstart + width;
			break;
		case 'h':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &height); argc--; argv++;
			}
			else {
				atob(&argv[0][2], &height);
			};
			yend = ystart + height;
			break;
		case 'z':
			do_zeroes = 1; break;
		case 'o':
			do_hdr = 1; break;
 	   }
	   argc--; argv++;
	}
	
	
#if 0
	openwindow();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, WSIZEX, WSIZEY);
	glOrtho(0, WSIZEX, 0, WSIZEY, -1.0, 0.0);
	glRasterPos2d(0, 0);
#endif
	viewer(1, 1280, 1024, RGBA32SB, "cmou");

#if 0
	glPixelStorei(GL_UNPACK_ROW_LENGTH, xs * STXPIXS);
	glDrawPixels(WSIZEX, WSIZEY, GL_RGBA, GL_UNSIGNED_SHORT, pixs);
#endif

	if (do_hdr) {
		fp = fopen("out.h", "w");
		fprintf(fp, "__uint32_t out_array[%d] = {\n", width*height);
	}

	for (y = ystart; y < yend; y++) {
	   for (x = xstart; x < xend; x++) {
		xy = (y * MAX_WIDTH) + x;
		if ((do_zeroes == 1) || pixs[xy][0] || pixs[xy][1] || 
			pixs[xy][2]) 
		{
		   printf("%4.4d %4.4d %4.4x %4.4x %4.4x %4.4x\n", 
			x, y, pixs[xy][0], pixs[xy][1], pixs[xy][2], 
			pixs[xy][3]);

		   if (do_hdr) {
			if (first_data) {
				fprintf(fp, "0x%2.2x%2.2x%2.2x%2.2x",
				   (pixs[xy][3]&0xff00)>>8, (pixs[xy][2]&0xff00)>>8, 
				   (pixs[xy][1]&0xff00)>>8, (pixs[xy][0]&0xff00)>>8);
				first_data = 0;
			}
			else {
				fprintf(fp, ",0x%2.2x%2.2x%2.2x%2.2x",
				   (pixs[xy][3]&0xff00)>>8, (pixs[xy][2]&0xff00)>>8, 
				   (pixs[xy][1]&0xff00)>>8, (pixs[xy][0]&0xff00)>>8);
			}
		   }

		   if (pixs[xy][0] || pixs[xy][1] || pixs[xy][2]) {
			if (first_nonzero == 1) {
				first_x = x;
				first_y = y;
			}
			first_nonzero = 0;	
			if (x < min_x)
				min_x = x;
			if (y < min_y)
				min_y = y;
			if (x > max_x)
				max_x = x;
			if (y > max_y)
				max_y = y;
		   }
		}
	   }
	   if (do_hdr)
		   fprintf(fp, "\n");
	};
#if 0
	glFinish();
	fprintf(stderr, "End\n");
	(void)getchar();
#endif
	if (do_hdr) {
		fprintf(fp, "};");
		fclose(fp);
	}

	fprintf(stderr, "xstart: %d, xend: %d, ystart: %d, yend: %d\n",
		xstart,xend,ystart,yend);

	fprintf(stderr, "first_x: %d, first_y: %d\n", first_x,first_y);
	fprintf(stderr, "max_x: %d, max_y: %d, min_x: %d, min_y: %d\n",
		max_x, max_y, min_x, min_y);
}


#if 0
static Bool WaitForNotify(Display *d, XEvent *e, char *arg) {
    return(e->type == MapNotify) && (e->xmap.window == (Window)arg);
}

openwindow()

{

int vinfo[] = {
    GLX_RGBA, 
    GLX_RED_SIZE, 8, 
    GLX_GREEN_SIZE, 8, 
    GLX_BLUE_SIZE, 8, 
    None
};

    Display *d;
    Window w;
    XVisualInfo	*v;
    GLXContext	c;
    XSetWindowAttributes wa;
    Colormap cmap;
    XEvent	event;
    

    d = XOpenDisplay(0);
    if (d == NULL) {
       fprintf(stderr, "Cannot open display\n");
       exit(2);
    }
    v = glXChooseVisual(d, DefaultScreen(d), vinfo);
    if (v == NULL) {
       fprintf(stderr, "Cannot find X visual conforming to requested pixel format\n");
       exit(2);
    }
    c = glXCreateContext(d, v, 0, GL_TRUE);
    if (c == NULL) {
       fprintf(stderr, "Cannot create GLX rendering context\n");
       exit(2);
     }

    cmap = XCreateColormap(d, RootWindow(d,v->screen), v->visual, AllocNone);
    
    wa.override_redirect = 0;
    wa.colormap = cmap;
    wa.border_pixel = 1;
    wa.event_mask = StructureNotifyMask | KeyPressMask | ButtonPressMask;
    w = XCreateWindow(d, RootWindow(d, v->screen), 100, 624, WSIZEX, WSIZEY, 
		    0, v->depth, InputOutput, v->visual, 
		    CWOverrideRedirect|CWBorderPixel|CWEventMask|CWColormap, &wa);
    XMapWindow(d, w);
    XIfEvent(d, &event, WaitForNotify, (char *)w);
    
    if (!glXMakeCurrent(d, w, c)) {
	printf ("error: gfxMakeCurrent failed\n");
	exit(1);
    }
}
#endif

