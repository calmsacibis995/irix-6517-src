/*  promgl gets/gives requests from the prom via some pipes.  This is
 * seperate from the IPXX prom since linking gets all hozed when we
 * link with -lc_s and -lgl_s.
 *
 *  File descriptors:
 *
 *	0	read GL requests.
 *	1	standard output.
 *	2	standard error.
 *	3	report keyboard output.
 *	4	report mouse status.
 */

/* #define DEBUG /* */

#define GLDRAW	0
#define KB	3
#define MS	4

/* #define _TP_OLDFONT			/* for testing iris10i */

static int glq = 0;
static short prevx,prevy;
static int blank;

#include <sys/types.h>
#include <gl/gl.h>
#include <gl/device.h>

#include <stand_htport.h>
#include <style.h>
#ifdef _TP_OLDFONT
#include <fonts/iris10i.h>
#else
#include <fonts/scrB18.h>
#endif
#include <fonts/special.h>
#include <fonts/ncenB18.h>
#include <fonts/helvR10.h>

#define FONTBASE	2

int blkread(int fd, char *buf, int len);
int isempty(int fd);

main(int argc, char **argv)
{
	fd_set rfd,wfd,efd;
	int rc;

	while (1) {
		if (!isempty(GLDRAW))
			goto dogldraw;
			
		FD_ZERO(&rfd);
		FD_SET(GLDRAW,&rfd);
		if (glq)
			FD_SET(glq,&rfd);
		FD_ZERO(&wfd);
		FD_ZERO(&efd);
		FD_SET(GLDRAW,&efd);
		
		rc=select(32,&rfd,&wfd,&efd,0);

		if (rc <= 0) {
			perror("select");
			exit(1);
		}

		if (FD_ISSET(GLDRAW,&efd)) {
			/* assume parent is gone + exit */
			exit(0);
		}

		if (FD_ISSET(GLDRAW,&rfd)) {
dogldraw:
			handle_glreq();
		}

		if (glq && FD_ISSET(glq,&rfd)) {
			handle_gl();
		}
	}
}

#define MAXBUF	1024
char buf[MAXBUF];
int bufsize;
int cposx,cposy;

fillbuf(int count)
{
	char *p = buf;
	char c = 0;
	int rc;

	bufsize = 0;

	if (count) {
		while (count) {
			rc = blkread(GLDRAW,p,count);
			p += rc;
			count -= rc;
		}
		bufsize = count;
	}
	else {
		while (c != ':') {
			rc = blkread(GLDRAW,&c,1);
			if (rc <= 0) {
				c = ':';	/* fake it to keep in sync */
			}
			*p++ = c;
			bufsize++;
		}
		*(p-1) = '\0';
		bufsize--;
	}
}

struct _table {
	char *name;
	int argtype;
#define	STRING_1	1
#define BYTE_1		2
#define BYTE_2		3
#define BYTE_4		5
#define SHORT_2		6
#define SHORT_3		7
#define SHORT_4		8
#define INT_8_DATA	9
#define NONE		10
	int sw;
} table[] = {
	"h",	SHORT_3,	6,		/* horizontal line */
	"c",	BYTE_1,		4,		/* color */
	"b",	SHORT_4,	5,		/* sboxfi */
	"d",	INT_8_DATA,	9,		/* drawbit */
	"f",	BYTE_2,		11,		/* font X char Y */
	"m",	SHORT_2,	8,		/* cmov2i */
	"C",	NONE,		2,		/* clear */
	"p",	SHORT_2,	3,		/* prefsize */
	"S",	SHORT_4,	14,		/* prefposition */
	"w",	STRING_1,	1,		/* winopen */
	"M",	BYTE_4,		10,		/* mapcolor */
	"B",	BYTE_1,		12,		/* blankscreen */
	"P",	SHORT_2,	7,		/* pnt2i */
	0,0,0
};

static struct {
	long r;
	long g;
	long b;
} cmap[256];


handle_glreq()
{
	int iarg1,iarg2,iarg3,iarg4,iarg5,iarg6,iarg7,iarg8;
	struct _table *t;
	char *sarg1;
	char cmd;
	int rc;

	rc = blkread(GLDRAW,&cmd,1);
	if (rc != 1) {
#ifdef DEBUG
		printf("token not ready!\n");
#endif
		return;
	}

	for (t=table;t->name; t++) {
		if (cmd == *t->name)
			break;
	}
	if (!t->name) {
		printf("promgl: unknown cmd(%d)!\n",cmd);
		return;
	}

	switch(t->argtype) {
	case STRING_1:
		fillbuf(0);
		sarg1 = buf;
		break;
	case BYTE_1:
		blkread(GLDRAW,buf,1);
		iarg1 = (unsigned int)buf[0];
		break;
	case BYTE_2:
		blkread(GLDRAW,buf,2);
		iarg1 = (unsigned int)buf[0];
		iarg2 = (unsigned int)buf[1];
		break;
	case BYTE_4:
		blkread(GLDRAW,buf,4);
		iarg1 = (unsigned int)buf[0];
		iarg2 = (unsigned int)buf[1];
		iarg3 = (unsigned int)buf[2];
		iarg4 = (unsigned int)buf[3];
		break;
	case SHORT_2:
		blkread(GLDRAW,buf,4);
		iarg1 = (unsigned int)((buf[0] << 8) | buf[1]);
		iarg2 = (unsigned int)((buf[2] << 8) | buf[3]);
		break;
	case SHORT_3:
		blkread(GLDRAW,buf,6);
		iarg1 = (unsigned int)((buf[0] << 8) | buf[1]);
		iarg2 = (unsigned int)((buf[2] << 8) | buf[3]);
		iarg3 = (unsigned int)((buf[4] << 8) | buf[5]);
		break;
	case SHORT_4:
		blkread(GLDRAW,buf,8);
		iarg1 = (unsigned int)((buf[0] << 8) | buf[1]);
		iarg2 = (unsigned int)((buf[2] << 8) | buf[3]);
		iarg3 = (unsigned int)((buf[4] << 8) | buf[5]);
		iarg4 = (unsigned int)((buf[6] << 8) | buf[7]);
		break;
	case INT_8_DATA:
		fillbuf(0);
		sscanf(buf,"%d,%d,%d,%d,%d,%d,%d,%d",
		       &iarg1,&iarg2,&iarg3,&iarg4,&iarg5,&iarg6,&iarg7,&iarg8);
		fillbuf(iarg8);
		break;
	}

	switch(t->sw) {
	case 1:		/* winopen */
#ifdef DEBUG
		printf("winopen(%s)\n",sarg1);
#endif
		foreground();
		winopen(sarg1);
		RGBmode();
		gconfig();
		qdevice(REDRAW);
		qdevice(KEYBD);
		qdevice(MOUSEX);
		qdevice(MOUSEY);
		qdevice(MOUSE1);
		qdevice(MOUSE2);
		qdevice(MOUSE3);
		glq = qgetfd();
		prevx = prevy = -1;

		blank = 1;
		drawmode(OVERDRAW);
		mapcolor(1,0,0,0);
		color(1);
		clear();
		drawmode(NORMALDRAW);
		
		c3i((long *)&cmap[0]);
		clear();

#ifdef _TP_OLDFONT
		defrasterfont(FONTBASE+ScrB18,iris10i_ht,iris10i_nc,
			      (Fontchar *)iris10iInfo,iris10i_nr,
			      iris10iData);
#else
		defrasterfont(FONTBASE+ScrB18,ScrB18_ht,ScrB18_nc,
			      (Fontchar *)ScrB18Info,ScrB18_nr,
			      ScrB18Data);
#endif
		defrasterfont(FONTBASE+Special,special_ht,special_nc,
			      (Fontchar *)tpspecial,special_nr,
			      tpsdata);
		defrasterfont(FONTBASE+ncenB18,ncenB18_ht,ncenB18_nc,
			      (Fontchar *)ncenB18Info,ncenB18_nr,
			      ncenB18Data);
		defrasterfont(FONTBASE+helvR10,helvR10_ht,helvR10_nc,
			      (Fontchar *)helvR10Info,helvR10_nr,
			      helvR10Data);
		break;
	case 2:		/* clear */
#ifdef DEBUG
		printf("clear()\n");
#endif
		clear();
		break;
	case 3:		/* prfsize */
#ifdef DEBUG
		printf("prefsize(%d,%d)\n",iarg1,iarg2);
#endif
		prefsize(iarg1,iarg2);
		break;
	case 4:		/* color */
#ifdef DEBUG
		printf("color(%d)\n",iarg1);
#endif
		c3i((long *)&cmap[iarg1]);
		break;
	case 5:		/* sboxfi */
#ifdef DEBUG
		printf("sboxfi(%d,%d,%d,%d)\n",iarg1,iarg2,iarg3,iarg4);
#endif
		sboxfi(iarg1,iarg2,iarg3,iarg4);
		break;
	case 6:		/* horizontal line sboxfi */
#ifdef DEBUG
		printf("horizontal sboxfi(%d,%d,%d)\n",iarg1,iarg2,iarg3);
#endif
		sboxfi(iarg1,iarg2,iarg1+iarg3,iarg2);
		break;
	case 7:
#ifdef DEBUG
		printf("pnt2i(%d,%d)\n",iarg1,iarg2);
#endif
		pnt2i(iarg1,iarg2);
		break;
	case 8:		/* cmove2i */
		cposx = iarg1;
		cposy = iarg2;
		cmov2i(cposx,cposy);
		break;
	case 9:		/* bitmap */
#ifdef DEBUG
		printf("bitmap(%dx%d)\n",iarg1,iarg2);
#endif
		{
			int xleft,xright,ytop;
			Fontchar fc[2];

			bzero(&fc[0],sizeof(Fontchar));
			fc[1].offset = 0;
			fc[1].w = iarg1;
			fc[1].h = iarg2;
 			fc[1].xoff = -iarg3;
			fc[1].yoff = -iarg4;
			fc[1].width = iarg5;

			defrasterfont(1,iarg2,2,fc,bufsize/2,
				      (unsigned short *)buf);

			font(1);
			charstr("\001");

			cposx += iarg5;
			cposy += iarg6;
		}
		break;
	case 10:	/* mapcolor */
#ifdef DEBUG
		printf("mapcolor: %d = %d %d %d\n",iarg1,iarg2,iarg3,iarg4);
#endif
		cmap[iarg1].r = iarg2;
		cmap[iarg1].g = iarg3;
		cmap[iarg1].b = iarg4;
		break;
	case 11:	/* font/char */
#ifdef DEBUG
		printf("font %d char %c\n",iarg1,iarg2);
#endif
		font(FONTBASE+iarg1);
		buf[0] = iarg2;
		buf[1] = '\0';
		charstr(buf);
		break;
	case 12:	/* blankscreen */
		if (iarg1 == blank)
			break;
		blank = iarg1;
		drawmode(OVERDRAW);
		color(blank);
		clear();
		drawmode(NORMALDRAW);
		break;
	case 14:	/* prefposition() */
		prefposition(iarg1,iarg2,iarg3,iarg4);
		break;
	}

	return;
}

handle_gl()
{
	long wx,wy,val;
	short data;
	char buf[3];
	char c;

	while (qtest()) {
		switch(qread(&data)) {
		      case REDRAW:
			printf("redraw!\n");
			break;
		      case KEYBD:
			c = data;
			write(KB,&c,1);
			break;
		      case MOUSEX:
			val = getvaluator(MOUSEX);
			getorigin(&wx,&wy);
			val -= wx;
			if (val < 0) val = 0;
			if (val != prevx) {
				buf[0] = 'x';
				buf[1] = (val & 0xff00) >> 8;
				buf[2] = val & 0xff;
				write(MS,buf,3);
				prevx = val;
			}
			break;
		      case MOUSEY:
			val = getvaluator(MOUSEY);
			getorigin(&wx,&wy);
			val -= wy;
			if (val < 0) val = 0;
			if (val != prevy) {
				buf[0] = 'y';
				buf[1] = (val & 0xff00) >> 8;
				buf[2] = val & 0xff;
				write(MS,buf,3);
				prevy = val;
			}
			break;
		      case MOUSE1:
			buf[0] = 'b';
			buf[1] = 0;
			buf[2] = data;
			write(MS,buf,3);
			break;
		      case MOUSE2:
			buf[0] = 'b';
			buf[1] = 1;
			buf[2] = data;
			write(MS,buf,3);
			break;
		      case MOUSE3:
			buf[0] = 'b';
			buf[1] = 2;
			buf[2] = data;
			write(MS,buf,3);
			break;
		}
	}
}

#define BLKSIZE 8192
static char blkbuf[BLKSIZE];
static int blks,blke;

int
blkread(int fd, char *buf, int len)
{
	int in, cp, off;
	int olen = len;

	off = 0;
	while (len > 0) {
		if (blks == blke) {
			blks = 0;
			in = read(fd,blkbuf,BLKSIZE);
			if (in < 0)
				return(in);
			blke = in;
		}
		else
			in = blke - blks;

		if (len < in) {
			cp = len;
		}
		else {
			cp = in;
			blke = blks + cp;
		}

		bcopy(blkbuf+blks,buf+off,cp);

		len -= cp;
		off += cp;
		blks += cp;
	}
	return(olen);
}

int
isempty(int fd)
{
	return(blks == blke);
}
