/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	3D moving color bar module.
 *	J. M. Barton  03/19/88
 */

# include	<gl.h>
# include	<gl/device.h>
# include	<stdlib.h>
# include	<math.h>
# include	<fmclient.h>
# include	<malloc.h>
# include	<memory.h>
# include	<stdarg.h>
# include	<string.h>

# include	"grosview.h"

/*
 * DEFXPERY basically defined how long a bar is
 */
# define	DEFXPERY	9.5		/* aspect ratio */
# define	PNTINCH		72.0		/* points per inch */
# define	DEFFONT		"TimesBoldItalic"

# define	J_LEFT		0	/* left justify number */
# define	J_RIGHT		1	/* right justify number */

# define XSHRINK	((float)0.97)	/* to provide border around bar */
# define YBAROFF	((float)0.05)	/* bottom of slot to bar bottom */
# define YBARSCALE	((float)0.55)	/* size of bar space */
/*
 * define text base above base of bar
 * An entire bar is 1.0 units high - so we center the baseline of the
 * text in the space remaining after the real bar portion
 */
# define YTEXTBASE	((((1.0 - (YBAROFF + YBARSCALE)) - cheight)/2) + (YBAROFF+YBARSCALE))
# define CHEIGHT	0.25	/* size of a canonical character */
# define BORDER		(YBARSCALE*0.2)	/* size of bar and window border */
# define YBARMID	(YBAROFF+(YBARSCALE/2))
# define YBARTOP	(YBAROFF+YBARSCALE)
# define YBARBOT	(YBAROFF)

# define MINYSIZE	45			/* minimum pixels in Y */
# define MINXSIZE	((long)(DEFXPERY*MINYSIZE))/* minimum pixels in X */

# define NDIVS		10	/* number of ticks in bar */
# define YSIZE		((nbars + (width - 1)) / width)

# define AVGSTR		"      Avg "
# define TICKPER	0.2

# define GETCPOS(x,y)		getcpos(x, y)
# define scrmask(r,l,b,t)		
# define TALPHA		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ 1234567890"

Colorindex	frontcolor = BLACK;
Colorindex	backcolor = DEFBACKCOLOR;
Colorindex	palecolor = DEFPALECOLOR;
char		*fontface;
int		arbsize = 0;
int		width = 1;

static colorblock_t	*bars[MAXBARS];
static int		nbars = 0;
static int		windes;
static char		doublebuffered = 0;
static char		*tfont;
static fmfonthandle	rtfont;
static fmfontinfo	rtfontinfo;
static int		inredraw = 0;
static float		cheight = CHEIGHT;
static float		bheight = 0.0;		/* baseline height */
static fmfonthandle	deffontfh;		/* handle for basic dft font */

/*
 * Window screen coordinates for other modules to use.
 */
static long		winposx;	/* origin in x */
static long		winposy;	/* origin in y */
static long		winsizex;	/* size in x */
static long		winsizey;	/* size in y */

static int	newwin = 1;

/*
 * Sliding scale to use when autosizing the bar under varying conditions.
 */
# define	MAXSCALE	9
static struct {
	int	disval;
	int	decay;		/* scale decay (in units) */
	int 	advance;	/* advance hysterisis (in units) */
} sscale[MAXSCALE] = {
	10,	20,	5,
	50,	75,	5,
	100,	100,	5,
	200,	70,	10,
	1000,	50,	10,
	5000,	50,	15,
	20000,	50,	15,
	100000,	50,	20,
	500000,	50,	30,
};

static void		cbwinsetup(int);
static void		cbredrawstrip(colorblock_t *);
static void		cbredraw1(colorblock_t *);
static void		cbstredraw(colorblock_t *);
static void		cbstquickdraw(colorblock_t *);
static void		cbdrawmax(colorblock_t *);
static void		cbredrawnum(colorblock_t *);
static void		cbonebar(colorblock_t *);
static void		cbnumdraw(colorblock_t *, int, int, float);
static void		barlimit(colorblock_t *, double *);
static void		barclamp(colorblock_t *, double *);
static void		barmax(colorblock_t *, double *);
static void		cbstalloc(colorblock_t *, int);
static int		cbydist(Screencoord, Screencoord);

# define doscrmask(x)	\
		scrmask((x)->cb_scrmask[0], (x)->cb_scrmask[1], \
			(x)->cb_scrmask[2], (x)->cb_scrmask[3]);
# define	_drawlimit(cbp)	\
	cbnumdraw(cbp, 2, J_RIGHT, cbp->cb_tlimit)
# define	_drawmax(cbp) \
	cbnumdraw(cbp, 1, J_RIGHT, cbp->cb_dispmax)
# define	_drawsum(cbp) \
	cbnumdraw(cbp, 0, J_RIGHT, cbp->cb_lastavg1)
# define	halfpix(tp)	(tp->cb_ftwid/2)

void
rtbar_text_start(void) {}	/* beginning of text to lock down */

void
FMPRSTR(char *s) {
	if (nofont)
		return;
	else
		fmprstr(s);
}

static char *
displaynum(float pstr, int scale)
{
   static char	tstr[10];
   char		*str = tstr;

	if (pstr < 0) {
		*str++ = '-';
		pstr = fabs(pstr);
	}
	if (scale) {
		if (pstr < 1024)
			sprintf(str, "%.1fm", pstr);
		else
			sprintf(str, "%.2fg", pstr/1024.0);
	}
	else if (pstr < 64*1024)
		sprintf(str, "%d", (int) pstr);
	else if (pstr < 1024*1024)
		sprintf(str, "%.1fk", pstr / 1024.0);
	else if (pstr < 1024*1024*1024)
		sprintf(str, "%.1fm", pstr / (1024.0*1024));
	else
		sprintf(str, "%.2fg", pstr / (1024.0*1024*1024));
	return(str);
}

float
masksum(colorblock_t *cbp, float *data) {
   int		i;
   float	rval;

	rval = 0;
	for (i = 0; i < cbp->cb_nsects; i++) {
		if (!(mask(i)&cbp->cb_nmask))
			rval += data[i];
	}
	return(rval);
}

float
unmasksum(colorblock_t *cbp, float *data) {
   int		i;
   float	rval;

	rval = 0;
	for (i = 0; i < cbp->cb_nsects; i++) {
		rval += data[i];
	}
	return(rval);
}

/*
 * Swap the buffers.
 */
void
cbswapbuf(void)
{
	if (doublebuffered) {
		swapbuffers();
		cbredraw();
	}
}

/*
 * Redraw the bars and names.
 */

void
cbredraw(void)
{
   colorblock_t	*tp;

	inredraw = 1;
	tp = nextbar(1);
	while (tp != 0) {
		if (tp->cb_type == T_NUM)
			tp->cb_flags |= TF_REMAX;
		cbredraw1(tp);
		tp = nextbar(0);
	}
	inredraw = 0;
}

/*
 * cbdrawlegend - draw the character string legend for a particular
 *	bar.
 */
void
cbdrawlegend(colorblock_t *tp)
{
   int			j;

	color(backcolor);
	rectf(tp->cb_cposx, tp->cb_cposy, tp->cb_cposx + tp->cb_strwid,
		tp->cb_cposy + (rtfontinfo.ysize * tp->cb_ftsywid));

	color(frontcolor);
	cmov2(tp->cb_cposx, tp->cb_cposy + (rtfontinfo.yorig * tp->cb_ftsywid));
	FMPRSTR(tp->cb_header);

	for (j = 0; j < tp->cb_nsects; j++) {
		color(tp->cb_colors[j]);
		FMPRSTR(tp->cb_legend[j]);
	}
	if (tp->cb_type == T_NUM) {
		if (tp->cb_flags & TF_AVERAGE) {
			color(frontcolor);
			FMPRSTR(AVGSTR);
		}
	}

	cbdrawabs(tp);
}

/*
 * cbdraw - draw the given color bar at the given place in the given
 *	colors.  calls the clamping function if necessary.
 */
void
cbdraw(colorblock_t *cbp, ...)
{
   va_list	ap;
   double	nvals[MAXSECTS];
   int		i;

#ifdef JEFF
	logcbdraw(&va_alist);
#endif
	if (cbp->cb_flags & TF_ONETRIP) {
		cbp->cb_flags &= ~TF_ONETRIP;
		return;
	}
	va_start(ap, cbp);
	for (i = 0; i < cbp->cb_nsects; i++) {
		nvals[i] = va_arg(ap, double);
	}
	va_end(ap);
	switch (cbp->cb_type) {
	case T_SABS:
	case T_ABS:
		barlimit(cbp, nvals);
		break;
	case T_SREL:
	case T_REL:
		barclamp(cbp, nvals);
		break;
	case T_NUM:
		barmax(cbp, nvals);
		break;
	}
	cbredraw1(cbp);
}

/*
 * cbdrawabs - draw the limit value for a bar.
 */
void
cbdrawabs(colorblock_t *tp)
{
	if (tp->cb_type == T_ABS || tp->cb_type == T_SABS)
		_drawlimit(tp);
}

/*
 * cbredraw1 - redraw the bar as passed in.
 */

static void
cbredraw1(colorblock_t *cbp)
{
   int			i;
   float		offset;
   float		psize;

	if (cbp->cb_mess) {
		cbmessage(cbp, cbp->cb_mess);
		return;
	}
	if (cbp->cb_type == T_SABS || cbp->cb_type == T_SREL)
		cbredrawstrip(cbp);
	else if (cbp->cb_type == T_NUM)
		cbredrawnum(cbp);
	else {
		pushmatrix();
		loadmatrix(cbp->cb_matrix);
		offset = 0;
		for (i = 0; i < cbp->cb_nsects; i++) {
			psize = cbp->cb_aspect * cbp->cb_last[i];
			color(cbp->cb_colors[i]);
			rectf(offset, 0.0, offset + psize, 1.0);
			offset += psize;
		}
		popmatrix();
	}
	if ((cbp->cb_type == T_ABS || cbp->cb_type == T_SABS))
		if (cbp->cb_flags & (TF_MAX|TF_AVERAGE)) {
			cbdrawmax(cbp);
		}
}

static void
cbredrawstrip(colorblock_t *cbp)
{
   int			i;
   strip_t		*sp = cbp->cb_stp;
   float		*fp;

	sp->st_cursamp = (sp->st_cursamp + 1) % sp->st_nsamples;
	fp = sp->st_samples + (sp->st_cursamp * cbp->cb_nsects);
	for (i = 0; i < cbp->cb_nsects; i++, fp++)
			*fp = cbp->cb_last[i];
	pushmatrix();
	loadmatrix(cbp->cb_matrix);
	if (inredraw || (cbp->cb_flags & TF_STRDR))
		cbstredraw(cbp);
	else
		cbstquickdraw(cbp);
	popmatrix();
}

static void
cbstredraw(colorblock_t *cbp)
{
   int			i;
   int			j;
   strip_t		*sp = cbp->cb_stp;
   float		offset;
   float		incr;
   float		height;
   int			start;
   float		*wf;
   int			whereseen = 0;
   int			toff;
   int			btoff;

	incr = cbp->cb_aspect / sp->st_nsamples;
	offset = 0;
	cbp->cb_pixcross = 0;
	if ((cbp->cb_flags & TF_WHERE) && sp->st_abswhere == sp->st_cursamp)
		cbp->cb_flags &= ~TF_WHERE;
	if (cbp->cb_flags & TF_TICK) {
		toff = cbp->cb_tick - sp->st_tickcnt;
		btoff = cbp->cb_bigtick - sp->st_bigtickcnt;
	}
	start = (sp->st_cursamp + 1) % sp->st_nsamples;
	for (i = start; ; i = (i + 1) % sp->st_nsamples) {
		height = 0;
		wf = sp->st_samples + (i * cbp->cb_nsects);
		for (j = 0; j < cbp->cb_nsects; j++) {
			if (((cbp->cb_flags & TF_WHERE) && !whereseen) &&
			    j != cbp->cb_nsects - 1)
				color(palecolor);
			else
				color(cbp->cb_colors[j]);
			rectf(offset, height, offset + incr, (height + *wf));
			height += *wf++;
		}
		if ((cbp->cb_flags & TF_WHERE) && sp->st_abswhere == i) {
			color(RED);
			move(offset+incr-cbp->cb_ftxwid, 0.0, 0.0);
			draw(offset+incr-cbp->cb_ftxwid, 1.0, 0.0);
			whereseen++;
		}
		if (cbp->cb_flags & TF_TICK) {
		   float	npix;

			toff--;
			if (toff < 0) {
				toff = cbp->cb_tick;
				if (cbp->cb_flags & TF_BIGTICK) {
					btoff--;
					if (btoff < 0) {
						btoff = cbp->cb_bigtick;
						npix = TICKPER*2;
					}
					else
						npix = TICKPER;
				}
				else
					npix = TICKPER;

				color(frontcolor);
				move(offset, 1.0, 0.0);
				draw(offset, 1.0-npix, 0.0);
			}
		}
		offset += incr;
		if (i == sp->st_cursamp)
			break;
	}
}

static void
cbstquickdraw(colorblock_t *cbp)
{
   strip_t		*sp = cbp->cb_stp;
   float		incr;
   float		*wf;
   float		fx;
   float		height;
   float		npix;
   int			i;

	if (cbp->cb_flags & TF_SCHANGE) {
		cbp->cb_flags &= ~TF_SCHANGE;
		cbstredraw(cbp);
		return;
	}
	incr = cbp->cb_aspect/sp->st_nsamples;
	wf = sp->st_samples+(sp->st_cursamp*cbp->cb_nsects);
	fx = cbp->cb_aspect - incr;

	if (incr < cbp->cb_ftwid) {
		cbp->cb_pixcross += incr;
		if (cbp->cb_pixcross >= cbp->cb_ftwid) {
		   float	tavg;
		   float	nht;
		   float	nfx;

			for (tavg = 0, i = 0; i < cbp->cb_nsects; i++)
				tavg += sp->st_avg[i]/sp->st_avgcnt;
			tavg = 1.0 / tavg;

			height = 0;
			nfx = cbp->cb_aspect - cbp->cb_ftwid;
			for (i = 0; i < cbp->cb_nsects; i++) {
				color(cbp->cb_colors[i]);
				nht = (sp->st_avg[i]/sp->st_avgcnt)*tavg;
				rectf(nfx, height, nfx + cbp->cb_ftwid,
					(height + nht));
				height += nht;
				sp->st_avg[i] = 0;
			}
			sp->st_avgcnt = 0;

			if (sp->st_forcetick) {
			    float	nfx;

				if (sp->st_forcetick == 1)
					npix = TICKPER;
				else 
					npix = TICKPER*2;
				sp->st_forcetick = 0;
				nfx = cbp->cb_aspect - cbp->cb_ftwid;
				color(frontcolor);
				move(nfx, 1.0, 0.0);
				draw(nfx, 1.0-npix, 0.0);
			}
			rectcopy(sp->st_irect[0], sp->st_irect[1],
				sp->st_irect[2], sp->st_irect[3],
				sp->st_fpos[0], sp->st_fpos[1]);
			cbp->cb_pixcross = 0;
		}
		else {
		   float	*twf = wf;

			for (i = 0; i < cbp->cb_nsects; i++)
				sp->st_avg[i] += *twf++;
			sp->st_avgcnt++;
		}
	}
	else {
		rectcopy(sp->st_irect[0], sp->st_irect[1], sp->st_irect[2],
			sp->st_irect[3], sp->st_fpos[0], sp->st_fpos[1]);
	}

	height = 0;
	for (i = 0; i < cbp->cb_nsects; i++) {
		color(cbp->cb_colors[i]);
		rectf(fx, height, fx + incr, (height + *wf));
		height += *wf++;
	}

	if (cbp->cb_flags & TF_TICK) {
		sp->st_tickcnt++;
		if (sp->st_tickcnt >= cbp->cb_tick) {
			if (incr < cbp->cb_ftwid)
				sp->st_forcetick = 1;
			if (cbp->cb_flags & TF_BIGTICK) {
				sp->st_bigtickcnt++;
				if (sp->st_bigtickcnt >=
				    cbp->cb_bigtick) {
					npix = TICKPER*2;
					sp->st_bigtickcnt = 0;
					if (incr < cbp->cb_ftwid)
						sp->st_forcetick = 2;
				}
				else
					npix = TICKPER;
			}
			else
				npix = TICKPER;
			sp->st_tickcnt = 0;

			if (!sp->st_forcetick) {
				color(frontcolor);
				move(fx, 1.0, 0.0);
				draw(fx, 1.0-npix, 0.0);
			}
		}
	}
}

static void
cbdrawmax(colorblock_t *cbp)
{
	if (cbp->cb_flags & TF_MAX) {
		if (!inredraw) {
			if (cbp->cb_max1 == 0)
				goto checksum;
			if (!(cbp->cb_flags & TF_REMAX))
				goto checksum;
		}
		_drawmax(cbp);
	}
    checksum:
	if (cbp->cb_flags & TF_AVERAGE) {
		if (!inredraw) {
			if (cbp->cb_lastavg1 == cbp->cb_dispavg1)
				return;
			cbp->cb_lastavg1 = cbp->cb_dispavg1;
		}
		_drawsum(cbp);
	}
}

static void
cbredrawnum(colorblock_t *cbp)
{
   char			*dbuf;
   float		swid;
   int			i;
   float		ysc;
   float		sum;
   int			rv;

	if (!(cbp->cb_flags & TF_NUMDISP))
		if (!inredraw)
			goto checkmax;
	cbp->cb_flags &= ~TF_NUMDISP;

	color(backcolor);
	swid = cbp->cb_hdstart;
	rectf(swid, cbp->cb_cposmid+halfpix(cbp), cbp->cb_cposx+cbp->cb_hdlen,
		cbp->cb_cpostop);
	color(frontcolor);
	swid += rtfontinfo.xorig * cbp->cb_ftwid;

	ysc = cbp->cb_cposmid + (rtfontinfo.yorig*cbp->cb_ftsywid);
	sum = 0;
	for (i = 0; i < cbp->cb_nsects - 1; i++) {
		cmov2(swid, ysc);
		dbuf = displaynum(cbp->cb_results[i], 0);
		sum += cbp->cb_avg[i];
		swid += (fmgetstrwidth(rtfont, cbp->cb_legend[i])*
			cbp->cb_ftwid);
		color(cbp->cb_colors[i]);
		FMPRSTR(dbuf);
	}
	if ((rv = strlen(cbp->cb_legend[cbp->cb_nsects-1])) != 0) {
		cmov2(swid, ysc);
		dbuf = displaynum(cbp->cb_results[cbp->cb_nsects-1], 0);
		color(cbp->cb_colors[i]);
		FMPRSTR(dbuf);
	}
	if (cbp->cb_flags & TF_AVERAGE) {
		if (rv) {
			swid += (fmgetstrwidth(rtfont,
				 cbp->cb_legend[cbp->cb_nsects-1])*
				 cbp->cb_ftwid);
		}
		else
			sum -= cbp->cb_avg[cbp->cb_nsects-2];
		if (cbp->cb_flags & TF_MBSCALE)
			dbuf = displaynum(sum, 1);
		else
			dbuf = displaynum(sum, 0);
		strcat(dbuf, " ");
		swid = swid + fmgetstrwidth(rtfont, AVGSTR)*cbp->cb_ftwid
			- fmgetstrwidth(rtfont, dbuf)*cbp->cb_ftwid;
		cmov2(swid, ysc);
		color(frontcolor);
		FMPRSTR(dbuf);
	}

    checkmax:
	if (cbp->cb_flags & TF_MAX) {
		if (!(cbp->cb_flags & TF_REMAX))
			if (!inredraw)
				return;
		cbp->cb_flags &= ~TF_REMAX;

		swid = cbp->cb_hdstart;
		color(backcolor);
		rectf(swid, cbp->cb_cposbot, cbp->cb_cposx+cbp->cb_hdlen,
			cbp->cb_cposmid-halfpix(cbp));
		ysc = cbp->cb_cposbot + (rtfontinfo.yorig * cbp->cb_ftsywid);
		swid += rtfontinfo.xorig * cbp->cb_ftwid;
		sum = 0;
		color(cbp->cb_maxcol.front);
		for (i = 0; i < cbp->cb_nsects - 1; i++) {
			cmov2(swid, ysc);
			dbuf = displaynum(cbp->cb_max[i], 0);
			sum += cbp->cb_max[i];
			swid += (fmgetstrwidth(rtfont, cbp->cb_legend[i])*
				cbp->cb_ftwid);
			FMPRSTR(dbuf);
		}
		if ((rv = strlen(cbp->cb_legend[cbp->cb_nsects-1])) != 0) {
			cmov2(swid, ysc);
			dbuf = displaynum(cbp->cb_max[i], 0);
			FMPRSTR(dbuf);
		}
		if (cbp->cb_flags & TF_AVERAGE) {
			if (rv) {
				swid += (fmgetstrwidth(rtfont,
					 cbp->cb_legend[cbp->cb_nsects-1])*
					 cbp->cb_ftwid);
			}
			else
				sum -= cbp->cb_avg[cbp->cb_nsects-2];
			if (cbp->cb_flags & TF_MBSCALE)
				dbuf = displaynum(sum, 1);
			else
				dbuf = displaynum(sum, 0);
			strcat(dbuf, " ");
			swid = swid +
				fmgetstrwidth(rtfont, AVGSTR)*cbp->cb_ftwid
				- fmgetstrwidth(rtfont, dbuf)*cbp->cb_ftwid;
			cmov2(swid, ysc);
			FMPRSTR(dbuf);
		}
	}
}

/*
 * Put a message in the window for the user.
 */
void
cbmessage(colorblock_t *cbp, char *t)
{

	if (t == 0) {
		cbredraw1(cbp);
		cbp->cb_mess = 0;
	}
	pushmatrix();
	loadmatrix(cbp->cb_matrix);
	color(WHITE);
	rectf(0.0, 0.0, cbp->cb_aspect, 1.0);
	cmov2(0.1, 0.3);
	color(RED);
	FMPRSTR(t);
	popmatrix();
	cbp->cb_mess = t;
}


static void
cbnumdraw(colorblock_t *cbp, int which, int just, float pstr) {
   float	y;
   int		back;
   int		front;
   long		swid;
   float	xcoff;
   float	ycoff;
   char		*str;
   int		scale = 0;

	if (cbp->cb_flags & TF_MBSCALE)
		scale = 1;
	switch (which) {
	case 0:
		y = cbp->cb_cposbot;
		back = cbp->cb_sumcol.back;
		front = cbp->cb_sumcol.front;
		break;
	case 1:
		y = cbp->cb_cposmid;
		back = cbp->cb_maxcol.back;
		front = cbp->cb_maxcol.front;
		break;
	case 2:
		y = cbp->cb_cposy;
		if (cbp->cb_flags & TF_NOSCALE) {
			front = cbp->cb_limcol.back;
			back = cbp->cb_limcol.front;
		}
		else {
			back = cbp->cb_limcol.back;
			front = cbp->cb_limcol.front;
		}
		break;
	}
	str = displaynum(pstr, scale);

	xcoff = (rtfontinfo.xorig * cbp->cb_ftwid);
	ycoff = (rtfontinfo.yorig * cbp->cb_ftsywid);

	color(back);
	rectf(cbp->cb_cmaxx, y, cbp->cb_cmaxx+cbp->cb_cmwid, y+cheight);
	color(front);

	swid = fmgetstrwidth(rtfont, str);
	switch (just) {
	case J_LEFT:
		cmov2(cbp->cb_cmaxx+xcoff, y+ycoff);
		FMPRSTR(str);
		break;
	case J_RIGHT:
		cmov2((cbp->cb_cmaxx+cbp->cb_cmwid)-(swid*cbp->cb_ftwid),
			y+ycoff);
		FMPRSTR(str);
		break;
	}

	if ((which == 2) && (cbp->cb_flags & TF_EXCEED)) {
		color(RED);
		rect(cbp->cb_cmaxx, y, cbp->cb_cmaxx+cbp->cb_cmwid, y+cheight);
		color(RED);
	}
}

/*
 * Normalize a set of data points.
 */

static void
normalize(int size, float norm, float *array)
{
   int		i;
	if (norm <= 0)
		norm = 1.0;
	else
		norm = 1.0 / norm;
	for (i = 0; i < size; i++)
		array[i] *= norm;
}
		
static int
rollcopy(int size, float scale, float *old, double *new)
{
   int		i;
   int		ifc = 0;
   float	tmp;

	for (i = 0; i < size; i++) {
		tmp = fabs((scale*new[i])+((1-scale)*old[i]));
		if (tmp != old[i]) {
			ifc++;
			old[i] = tmp;
		}
	}
	return(ifc);
}

static int
frollcopy(int size, float scale, float *old, float *new)
{
   int		i;
   int		ifc = 0;
   float	tmp;

	for (i = 0; i < size; i++) {
		tmp = fabs((scale*new[i])+((1-scale)*old[i]));
		if (tmp != old[i]) {
			ifc++;
			old[i] = tmp;
		}
	}
	return(ifc);
}

/*
 * barclamp - clamp the values to get a smooth movement out of the bar.
 */

static void
barclamp(colorblock_t *cbp, double *new)
{
	(void) rollcopy(cbp->cb_nsects, cbp->cb_upmove, cbp->cb_results, new);
	(void) frollcopy(cbp->cb_nsects, 1.0, cbp->cb_last, cbp->cb_results);
	normalize(cbp->cb_nsects,unmasksum(cbp, cbp->cb_results), cbp->cb_last);
}

/*
 * barmax - keep maximum value information before calling
 *		clamping function.
 */

static void
barmax(colorblock_t *cbp, double *new)
{
   int		i;

	if (rollcopy(cbp->cb_nsects, 1.0, cbp->cb_results, new))
		cbp->cb_flags  |= TF_NUMDISP;

	if (cbp->cb_flags & TF_AVERAGE) {
		if (frollcopy(cbp->cb_nsects, 1.0/cbp->cb_avgtick, cbp->cb_avg,
			cbp->cb_results)) {
			cbp->cb_flags |= TF_REMAX;
			cbp->cb_flags |= TF_NUMDISP;
		}
	}
	if (cbp->cb_flags & TF_MAX) {
		if (cbp->cb_flags & TF_MAXRESET) {
			if (cbp->cb_maxcnt-- <= 0) {
				for (i = 0; i < cbp->cb_nsects; i++)
					cbp->cb_max[i] = 0;
				cbp->cb_maxcnt = cbp->cb_maxtick;
				cbp->cb_flags &= ~TF_MAXRESET;
			}
		}
		for (i = 0; i < cbp->cb_nsects; i++) {
			if (cbp->cb_results[i] > cbp->cb_max[i]) {
				cbp->cb_flags |= TF_REMAX;
				cbp->cb_max[i] = cbp->cb_results[i];
			}
			else if (!(cbp->cb_flags & TF_MAXRESET)) {
				if (cbp->cb_maxtick <= 0)
					continue;
				if (cbp->cb_results[i] < cbp->cb_max[i]) {
					if (cbp->cb_amax[i]-- <= 0) {
						cbp->cb_max[i] = 0;
						cbp->cb_amax[i] =
							cbp->cb_maxtick;
					}
				}
				else
					cbp->cb_amax[i] = cbp->cb_maxtick;
			}
		}
	}
}

/*
 * barlimit - if a counter value exceeds the current maximum, re-scale.
 */

static void
barlimit(colorblock_t *cbp, double *new)
{
   int		i;
   int		tchg;
   double	sum;
   double	fullsum;
   float	cmax;
   int		nreal;
   float	scaler;

	/*
	 * Normalize the data with the new numbers.
	 */
	if (strlen(cbp->cb_legend[cbp->cb_nsects-1]) == 0)
		nreal = cbp->cb_nsects - 1;
	else
		nreal = cbp->cb_nsects;

	rollcopy(cbp->cb_nsects, cbp->cb_upmove, cbp->cb_results, new);

	if (cbp->cb_flags & TF_AVERAGE) {
		rollcopy(nreal,1.0/cbp->cb_avgtick,cbp->cb_avg, new);
		if (--cbp->cb_avgcnt <= 0) {
			cmax = masksum(cbp, cbp->cb_avg);
			cbp->cb_dispavg1 = cmax;
			cbp->cb_avgcnt = cbp->cb_avgtick;
		}
	}

	if (cbp->cb_flags & TF_MAX) {
		if (cbp->cb_flags & TF_MAXRESET) {
			rollcopy(nreal, 1.0, cbp->cb_max, new);
			cmax = masksum(cbp, cbp->cb_max);
			if (cbp->cb_maxcnt-- <= 0) {
				cbp->cb_max1 = cbp->cb_dispmax = 0;
				cbp->cb_flags &= ~TF_MAXRESET;
			}
		}
		else
			cmax = masksum(cbp, cbp->cb_results);
		if (cmax < 0)
			cmax = 0;
		if (cmax > cbp->cb_max1) cbp->cb_max1 = cmax;
		if (!(cbp->cb_flags & TF_MAXRESET) &&
		    cbp->cb_maxtick > 0) {
			if (cmax < cbp->cb_dispmax) {
				if (cbp->cb_maxcnt-- <= 0) {
					cbp->cb_maxcnt = cbp->cb_maxtick;
					cbp->cb_dispmax = cbp->cb_max1 = cmax;
					cbp->cb_flags |= TF_REMAX;
				}
			}
			else
				cbp->cb_maxcnt = cbp->cb_maxtick;
		}
		if (cbp->cb_dispmax < cbp->cb_max1) {
			cbp->cb_dispmax = cbp->cb_max1;
			cbp->cb_flags |= TF_REMAX;
		}
	}


	/*
	 * If a scale change is required, do so.
	 */
	sum = masksum(cbp, cbp->cb_results);
	if (!(cbp->cb_flags & TF_NOSCALE)) {
		tchg = 0;
		if (cbp->cb_tlimit == 0 && sum <= cbp->cb_tlimit) {
			cbp->cb_tlimit = sscale[0].disval;
			cbdrawabs(cbp);
			tchg++;
		}
		else if (sum > cbp->cb_tlimit) {
			if (--cbp->cb_nabove <= 0) {
				for (i = 0; i < MAXSCALE - 1 &&
				    sscale[i].disval < sum; i++);
				cbp->cb_tlimit = sscale[i].disval;
				cbdrawabs(cbp);
				cbp->cb_nbelow = sscale[i].decay;
				cbp->cb_nabove = sscale[i].advance;
				cbp->cb_curscale = i;
				tchg++;
			}
		}
		else if (!(cbp->cb_flags & TF_CREEP)) {
			if (cbp->cb_nbelow <= 0) {
				for (i = 0; i < MAXSCALE - 1 &&
				    sscale[i].disval < sum; i++);
				if (i < cbp->cb_curscale) {
					cbp->cb_curscale--;
					cbp->cb_tlimit =
						sscale[cbp->cb_curscale].disval;
					cbp->cb_nabove = sscale[i].advance;
					cbdrawabs(cbp);
					tchg++;
				}
				cbp->cb_nbelow = sscale[cbp->cb_curscale].decay;
			}
			else
				cbp->cb_nbelow--;
		}
		if (tchg) {
			if (cbp->cb_stp) {
				cbp->cb_stp->st_abswhere =
					cbp->cb_stp->st_cursamp;
				cbp->cb_flags |= (TF_WHERE|TF_SCHANGE);
			}
		}
	}

	/*
	 * Finally, scale the actual values to match the scale factor.
	 */
	fullsum = 0;
	for (i = 0; i < nreal; i++)
		fullsum += cbp->cb_last[i] = cbp->cb_results[i];
	if (nreal == cbp->cb_nsects - 1) {
		if (fullsum > cbp->cb_tlimit) {
			scaler = fullsum;
			cbp->cb_last[nreal] = 0;
		}
		else {
			scaler = cbp->cb_tlimit;
			cbp->cb_last[nreal] = cbp->cb_tlimit - fullsum;
		}
	}
	else {
		fullsum += cbp->cb_last[cbp->cb_nsects]
			= cbp->cb_results[cbp->cb_nsects];
		scaler = fullsum;
	}
	if (sum > (cbp->cb_tlimit+(cbp->cb_tlimit*0.01))) {
		if (!(cbp->cb_flags & TF_EXCEED)) {
			cbp->cb_flags |= TF_EXCEED;
			cbdrawabs(cbp);
		}
	}
	else if (cbp->cb_flags & TF_EXCEED) {
		cbp->cb_flags &= ~TF_EXCEED;
		cbdrawabs(cbp);
	}
	normalize(cbp->cb_nsects, scaler, cbp->cb_last);
}

void
rtbar_text_end(void) {}	/* end of text to lock down */

/*
 * cbinit - create and initialize a new color bar.  all you get from this
 *	call is a control block, not mapped to any screen position.  the
 *	caller is responsible for initializing the transformation matrix and
 *	the color list.
 */
colorblock_t *
cbinit(void)
{
   colorblock_t		*tp;

	if ((nbars % MAXBARS) == 0)
		bars[nbars/MAXBARS] =
			(colorblock_t *) calloc(sizeof(colorblock_t), MAXBARS);
	tp = bars[nbars/MAXBARS] + (nbars % MAXBARS);
	memset((void *) tp, 0, sizeof(*tp));
	nbars++;
	tp->cb_valid = 1;
	tp->cb_rb = cbindex(tp);
	return(tp);
}

/*
 * cbindex - given a color bar pointer, return it's index
 */
int
cbindex(colorblock_t *cbp)
{
   int		i;

	for (i = 0; i < MAXBARS; i++)
		if (((long)cbp >= (long)bars[i]) &&
		    ((long)cbp<((long)bars[i]+(MAXBARS*sizeof(colorblock_t)))))
		return((i*MAXBARS) + (cbp - bars[i]));
	return(-1);
}

/*
 * cbptr - given an index, return a pointer to a color bar
 */
colorblock_t *
cbptr(int index) { return((bars[index/MAXBARS]) + (index % MAXBARS)); }

/*
 * cbinitindex - initialize a color bar at a specific index
 */
colorblock_t *
cbinitindex(int index)
{
   colorblock_t		*tp;

	if (bars[index/MAXBARS] == 0)
		bars[index/MAXBARS] =
			(colorblock_t *) calloc(sizeof(colorblock_t), MAXBARS);
	tp = (bars[index/MAXBARS]) + (index % MAXBARS);
	if (!tp->cb_valid) {
		memset((void *) tp, 0, sizeof(colorblock_t));
		tp->cb_valid = 1;
	}
	nbars++;
	tp->cb_rb = index;
	return(tp);
}

colorblock_t *
nextbar(int begin)
{
   static int	pos;
   colorblock_t	*tp;

	if (begin) pos = 0;
	for (;;) {
		if (bars[pos/MAXBARS] == 0)
			return(0);
		tp = (bars[pos/MAXBARS]) + (pos % MAXBARS);
		pos++;
		if (tp->cb_valid)
			return(tp);
	}
}

/*
 * cbstalloc - allocate space for the strip chart information.
 */
static void
cbstalloc(colorblock_t *cp, int n)
{
   strip_t	*sp;
   float	*wf;
   int		j;
   int		i;

	if (cp->cb_stp == 0) {
		cp->cb_stp = sp = (strip_t *) calloc(1, sizeof(strip_t));
		cp->cb_pixcross = 0;
		sp->st_nsamples = n;
		sp->st_cursamp = n - 1;
		sp->st_abswhere = 0;
		sp->st_samples =
			(float *) calloc(n, sizeof(float)*cp->cb_nsects);
		for (i = 0; i < n; i++) {
			wf = sp->st_samples + (i * cp->cb_nsects);
			for (j = 0; j < cp->cb_nsects - 1; j++)
				*wf++ = 0;
			*wf = 1.0;
		}
	}
}

/*
 * cblayout - once all bars are initialized, this function figures out
 *	all the display parameters for the window and bars and sets
 *	up the window.
 */

void
cblayout(char *title)
{
   static int	fontinit = 0;
   int		i;
   int		j;
   colorblock_t	*tp;
   Screencoord	ix;
   Screencoord	iy;
   Screencoord	six;
   Screencoord	siy;
   Coord	ywhere;
   int		wrow;
   Coord	offset;
   int		truncw = YSIZE;
   int		wcfontht;
   static int	old_wcfontht;

	/*
	 * Open up the window.
	 */
	if (newwin) {
		newwin = 0;
		old_wcfontht = -9999;
		/*
		 * First make sure we can find the font we wish to use.
		 */
		if (!fontinit) {
			fminit();
			fontinit = 1;
		}
		if (fontface == 0)
			tfont = DEFFONT;
		else
			tfont = fontface;
		if ((rtfont = fmfindfont(tfont)) == 0) {
			fprintf(stderr, "%s: unable to find font \"%s\"\n",
				pname, tfont);
			exit(1);
		}

		if ((deffontfh = fmfindfont(DEFFONT)) == 0) {
			/* default font not found?? not a disaster */
			deffontfh = fmfindfont(tfont);
		}

		/*
		 * Now open up the window.
		 */
		if (!(title != 0 || do_border))
			noborder();
		if (debugger)
			foreground();
		wrow = (int)(((float) nbars / width) + 0.99);

		/*
		 * Now deal with what the user said in laying it out.
		 * The rule is that arbsize is overruled by specifying
		 * the origin or preferred size.
		 */
		if (prefpos) {
			if (prefwsize)
				prefposition(prefxpos, prefxpos + prefxsize,
					prefypos, prefypos + prefysize);
			else
				prefposition(prefxpos,
					prefxpos + (MINXSIZE * width),
					prefypos,
					prefypos + (MINYSIZE * wrow));
		}
		else if (prefwsize)
			prefsize(prefxsize, prefysize);
		else if (!arbsize) {
			if (debug)
				fprintf(stderr, "width %d wrow %d aspect(%.2f,%d),size(%d,%d)\n",
					width, wrow,
					DEFXPERY*width, wrow,
					MINXSIZE*width, MINYSIZE*wrow);
			keepaspect((long)(DEFXPERY*100) * width, wrow * 100);
			minsize(MINXSIZE * width, MINYSIZE * wrow);
		}

		fflush(logfd);
		if (title != 0)
			windes = winopen(title);
		else
			windes = winopen("gr_osview");
		
		qdevice(LEFTMOUSE);
		qdevice(WINQUIT);
		qdevice(WINSHUT);
		qdevice(WINFREEZE);
		qdevice(WINTHAW);
		qdevice(ESCKEY);
		shademodel(FLAT);
	} else {
		winset(windes);

		/*
		 * If not the first pass, we will need to re-scale the
		 * font, so discard what we had and start over.
		 */
	}

	/*
	 * Shape the viewport and set up the base transformation.
	 */
	inredraw = 1;
	cbwinsetup(0);

	/*
	 * Determine the font size to use.  The first step of this is to 
	 * figure out where our window is on the screen.
	 */
	cmov2(0.0, 0.0);
	GETCPOS(&ix, &iy);
	cmov2(0.0, CHEIGHT);
	GETCPOS(&six, &siy);
	/*
	if (debug || printpos)
		printf("cp <%d,%d>, ch <%d,%d>, pixht <%d,%d>\n",
			ix, iy, six, siy, six - ix, siy - iy);
	*/

	wcfontht = cbydist(iy, siy);
	if (wcfontht <= 0)
		wcfontht = 1;
	{
	   fmfonthandle 	nfont = NULL;
	   double		pntsz;
	   fmfonthandle 	ndeffont = NULL;
	   int			alphasz;

	   if (wcfontht!=old_wcfontht) {
		float pixinch;
		fmfreefont(rtfont);
		if ((rtfont = fmfindfont(tfont)) == 0) {
			fprintf(stderr, "%s: unable to find font \"%s\"\n",
				pname, tfont);
			exit(1);
		}

		pixinch = (float)getgdesc(GD_YPMAX)/
			                ((float)getgdesc(GD_YMMAX)*.03937);
		pntsz = (wcfontht / pixinch) * PNTINCH;

		if ((nfont = fmscalefont(rtfont, pntsz)) == 0) {
			fprintf(stderr, "%s: unable to scale font \"%s\" to point size %f\n",
				pname, tfont, pntsz);
			exit(1);
		}
		alphasz = fmgetstrwidth(nfont, TALPHA);

		if (strcmp(tfont, DEFFONT)) {
			int defalphasz;
			/*
			 * Since all 'tuning' for fit is done on the default
			 * font - we must tweak other fonts to fit
			 */
			if ((ndeffont = fmscalefont(deffontfh, pntsz)) == 0) {
				fprintf(stderr, "%s: unable to scale font \"%s\" to point size %f\n",
					pname, DEFFONT, pntsz);
				exit(1);
			}
			defalphasz = fmgetstrwidth(ndeffont, TALPHA);
			fmfreefont(ndeffont);
			if (debug)
				printf("pntsz %f defalphasz %d alphasz %d\n",
					pntsz, defalphasz, alphasz);
			while (alphasz > defalphasz) {
				pntsz -= 0.25;
				fmfreefont(nfont);
				if ((nfont = fmscalefont(rtfont, pntsz)) == 0) {
					fprintf(stderr, "%s: unable to scale font \"%s\" to pointsize %f\n",
						pname, tfont, pntsz);
					exit(1);
				}
				alphasz = fmgetstrwidth(nfont, TALPHA);
				if (debug)
					printf("pntsz %f defalphasz %d alphasz %d\n",
						pntsz, defalphasz, alphasz);
			}
		}
		fmgetfontinfo(nfont, &rtfontinfo);
		fmfreefont(rtfont);
		rtfont = nfont;
		old_wcfontht=wcfontht;
		/*
		 * alter real cheight based on font we really got
		 * oldC is to newC as old ht is to new ht
		 */
		cheight = ((float)rtfontinfo.ysize * CHEIGHT) / wcfontht;
		bheight = ((float)rtfontinfo.yorig * CHEIGHT) / wcfontht;
		if (debug) {
			printf("wcfontht %d f.ysize %d f.yorig %d cheight %.2f, bheight %.2f\n",
				wcfontht, rtfontinfo.ysize,
				rtfontinfo.yorig, cheight, bheight);
			printf("f.xsize %d f.xorig %d\n",
				rtfontinfo.xsize, rtfontinfo.xorig);
		}
	   }
	}
	fmsetfont(rtfont);

	/*
	 * Now layout the bars in it.
	 */
	wrow = 0;
	tp = nextbar(1);
	while (tp != 0) {
		/*
		 * Set up the bar space.
		 */
		ywhere = (Coord) (truncw - 1) - wrow + YBAROFF;
		for (j = 0; j < width && tp != 0; j++, i++) {
			/*
			 * Determine our position and set initial scaling.
			 */
			tp->cb_aspect = DEFXPERY;
			offset = (DEFXPERY * (1.0-XSHRINK)) / 2.0;
			tp->cb_xwhere = (DEFXPERY * j) + offset;
			tp->cb_ywhere = ywhere;

			pushmatrix();
			translate(tp->cb_xwhere, tp->cb_ywhere, 0.0);
			scale(XSHRINK, 1.0, 1.0);

			cmov2(0.0, 0.0);
			GETCPOS(&tp->cb_scrmask[0], &tp->cb_scrmask[2]);
			cmov2(tp->cb_aspect, 1.0);
			GETCPOS(&tp->cb_scrmask[1], &tp->cb_scrmask[3]);

			getmatrix(tp->cb_matrix);
			popmatrix();

			cbonebar(tp);

			if ((tp = nextbar(0)) == 0)
				break;
		}
		wrow++;
	}

	/*
	 * Finalize the window.
	 */
	cbwinsetup(2);
	inredraw = 0;
}

/*
 * cbtakedown - destroy all window information and color bars.
 */
void
cbtakedown(void)
{
	nbars = 0;
	winclose(windes);
	newwin = 1;
	fmfreefont(rtfont);
}

static void
cbonebar(colorblock_t *tp)
{
   Screencoord	ix;
   Screencoord	iy;
   Screencoord	six;
   Screencoord	siy;
   float	xwhere = tp->cb_xwhere;
   float	ywhere = tp->cb_ywhere;
   int		border;
   int		j;
   float	psize;
   long		slen;
   strip_t	*sp;
   float	minx;
   static int	maxscreenx = -1;

	/*
	 * If text supression was requested, then also suppress the
	 * max and average counters. For the scale, if it is locked,
	 * set the foreground and background to the bar background,
	 * making it invisible.
	 */
	if (nofont) {
		tp->cb_flags &= ~(TF_MAX|TF_AVERAGE);
		tp->cb_limcol.back = tp->cb_limcol.front = backcolor;
	}

	/*
	 * Now set up the character drawing space.
	 * Text is drawn in original world space, while bar drawing is
	 * done in a custom coordinate system.
	 */
	tp->cb_strwid = tp->cb_aspect * XSHRINK;
	tp->cb_cposx = xwhere;
	tp->cb_cposy = ywhere + YTEXTBASE;
	tp->cb_cposz = 0.0;
	psize = (YBARSCALE - (2 * cheight)) / 3;
	tp->cb_cposbot = ywhere + psize;
	tp->cb_cposmid = tp->cb_cposbot + cheight + psize;
	tp->cb_cpostop = ywhere + YBARSCALE;

	/*
	 * Setting this up is also affected by the brain-damaged rules
	 * in the GL for reflective window coordinates.
	 */
	cmov2(tp->cb_cposx, 0.0);
	GETCPOS(&ix, &iy);

	cmov2(tp->cb_cposx + tp->cb_strwid, 1.0);
	GETCPOS(&six, &siy);

	tp->cb_twid = six - ix;
	tp->cb_ftwid = tp->cb_strwid / tp->cb_twid;
	tp->cb_ftsywid = 1.0 / cbydist(iy, siy);

	/*
	 * Figure out the maximum length of the text string area.
	 */
	slen = fmgetstrwidth(rtfont, tp->cb_header);
	tp->cb_hdstart = tp->cb_cposx + (slen * tp->cb_ftwid);
	for (j = 0; j < tp->cb_nsects; j++)
		slen += fmgetstrwidth(rtfont, tp->cb_legend[j]);
	if (tp->cb_type == T_NUM && (tp->cb_flags & TF_AVERAGE))
		slen += fmgetstrwidth(rtfont, AVGSTR);
	tp->cb_hdlen = (slen * tp->cb_ftwid);

	/*
	 * Move drawing of bar to appropriate place.
	 */
	color(frontcolor);
	linewidth((short) 1);
	doscrmask(tp);

	/*
	 * If an absolute limit display is in use, then calculate out
	 * the character positions.
	 */
	if (tp->cb_type == T_ABS || tp->cb_type == T_SABS) {
	   long		swid;
	   float	xv;

		/*
		 * If the bar contains the side fields, then draw a rectangle
		 * around them now.
		 */
		swid = fmgetstrwidth(rtfont, "2222222");
		tp->cb_cmaxx = tp->cb_cposx+tp->cb_strwid-
			(tp->cb_cmwid = (swid*tp->cb_ftwid));

		/*
		rect(tp->cb_cmaxx-tp->cb_ftwid,
		     tp->cb_ywhere-tp->cb_ftsywid,
		     tp->cb_cposx+tp->cb_strwid+tp->cb_ftwid,
		     tp->cb_cposy+cheight+(2*tp->cb_ftsywid));
		*/

		pushmatrix();
		loadmatrix(tp->cb_matrix);

		if (tp->cb_flags & (TF_MAX|TF_AVERAGE)) {
			/*
			 * A max value display requires that the bar be
			 * scaled down in size for room to display.
			 */
			xv = (tp->cb_strwid - tp->cb_cmwid) / tp->cb_aspect;
			scale(xv, YBARSCALE, 1.0);
		}
		else
			scale(1.0, YBARSCALE, 1.0);
	}
	else {
		pushmatrix();
		loadmatrix(tp->cb_matrix);
		scale(1.0, YBARSCALE, 1.0);
	}

	/*
	 * Determine if a border is desired and set the clamping
	 * function.
	 */
	switch (tp->cb_type) {
	case T_SABS:
		if (tp->cb_flags & TF_NOBORD)
			border = 0;
		else
			border = 1;
		break;
	case T_SREL:
		if (tp->cb_flags & TF_NOBORD)
			border = 0;
		else
			border = 1;
		break;
	case T_NUM:
		border = 0;	/* no lines at all */
		popmatrix();

		move(tp->cb_hdstart, tp->cb_cpostop + (2*tp->cb_ftsywid), 0.0);
		draw(tp->cb_cposx + tp->cb_hdlen,
			tp->cb_cpostop + (2*tp->cb_ftsywid),
			0.0);
		cmov2(tp->cb_cposx,
			tp->cb_cposmid+(rtfontinfo.yorig*tp->cb_ftsywid));
		color(frontcolor);
		FMPRSTR("Current");

		if (tp->cb_flags & TF_MAX) {
			cmov2(tp->cb_cposx,
			     tp->cb_cposbot+(rtfontinfo.yorig*tp->cb_ftsywid));
			color(tp->cb_maxcol.front);
			FMPRSTR("Maximum");
		}

		pushmatrix();
		loadmatrix(tp->cb_matrix);
		break;
	case T_ABS:
		goto checkborder;
	case T_REL:
	default:
	checkborder:
		if (tp->cb_flags & TF_NOBORD)
			border = 0;	/* no border */
		else
			border = 2;	/* real live hash marks */
		break;
	}

	/*
	 * In bar space at this point.  Get initial world/pixel ratio in Y.
	 */
	cmov2(0.0, 0.0);
	GETCPOS(&ix, &iy);
	cmov2(1.0, 1.0);
	GETCPOS(&six, &siy);
	tp->cb_ftywid = 1.0 / cbydist(iy, siy);	/* one pixel in Y */
	tp->cb_ftxwid = 1.0 / (six - ix);	/* one pixel in X */


	if (tp->cb_type == T_SREL || tp->cb_type == T_SABS) {
	    cbstalloc(tp, tp->cb_nsamp);
	    sp = tp->cb_stp;
	    cmov2(0.0, 0.0);
	    GETCPOS(&sp->st_fpos[0], &sp->st_fpos[1]);
	    sp->st_fpos[0] -= winposx;
	    sp->st_fpos[1] = cbydist(winposy, sp->st_fpos[1]);
	}

	/*
	 * Create the border around the bar.
	 */
	if (border == 2) {
	   float	bmin;
	   float	v[2];

		/*
		 * Wide border with hash marks.
		 */
		rect(0.0, 0.0, tp->cb_aspect, 1.0);
		bmin = 2.5 * tp->cb_ftywid;
		if (bmin < BORDER) {
			/*
			 * Largish window, easy to do.
			 */
			bmin = BORDER;
		}
		translate(tp->cb_ftxwid*2, bmin, 0.0);
		scale(1-((tp->cb_ftxwid*3)/DEFXPERY), 1-(bmin+(tp->cb_ftywid)),
			1.0);

		/*
		 * Now go down the bar and add ticks marks in
		 * WHITE over the lower black border.
		 */
		color(frontcolor);
		rectf(0.0, 0.0, tp->cb_aspect, -bmin);
		color(WHITE);
		psize = tp->cb_aspect / NDIVS;
		for (j = 0; j <= NDIVS; j++) {
			v[0] = j * psize;
			bgnline();
			v[1] = 0.0;
			v2f(v);
			v[1] = -bmin;
			v2f(v);
			endline();

		}
		if (tp->cb_type == T_SREL || tp->cb_type == T_SABS) {
		    sp->st_fpos[0]++;
		    sp->st_fpos[1] += 2;
		}
	}
	else if (border == 1) {
	    /*
	     * Narrow border.
	     */
	    rect(0.0, 0.0, tp->cb_aspect, 1.0);
	    /*
	     * Want exactly enough room for a one-pixel wide line
	     * around the bar.
	     */
	    translate(tp->cb_ftxwid, tp->cb_ftywid, 0.0);
	    scale(1.0-((tp->cb_ftxwid*2)/DEFXPERY),
	    	1.0-(tp->cb_ftywid*2), 1.0);

	    if (tp->cb_type == T_SREL || tp->cb_type == T_SABS) {
		sp->st_fpos[0]++;
		sp->st_fpos[1]++;
	    }
	}

	/*
	 * The bar was rescaled by drawing the border.  Recalculate
	 * the pixel ratios.
	 */
	cmov2(0.0, 0.0);
	GETCPOS(&ix, &iy);
	cmov2(1.0, 1.0);
	GETCPOS(&six, &siy);
	tp->cb_ftywid = 1.0 / cbydist(iy, siy);	/* one pixel in Y */
	tp->cb_ftxwid = 1.0 / (six - ix);	/* one pixel in X */

	/*
	 * Now, if drawing a strip chart, figure out where the rectangles
	 * are in screen coordinates.
	 */
	if (tp->cb_type == T_SREL || tp->cb_type == T_SABS) {
		/*
		 * Insure that the rectangle always copies at least one
		 * pixel's worth of stuff.
		 */
		minx = (tp->cb_ftwid > (tp->cb_aspect/sp->st_nsamples) ?
			tp->cb_ftwid : (tp->cb_aspect/sp->st_nsamples));
		cmov2(0.0+minx, 0.0);
		GETCPOS(&sp->st_irect[0], &sp->st_irect[1]);
		sp->st_irect[0] -= winposx;
		sp->st_irect[1] = sp->st_fpos[1];

		cmov2(tp->cb_aspect, 1.0);
		GETCPOS(&sp->st_irect[2], &sp->st_irect[3]);
		sp->st_irect[2] -= winposx;
		sp->st_irect[3] = cbydist(winposy, sp->st_irect[3]);

		if (printpos)
			printf("irect[%d,%d,%d,%d], fpos[%d,%d]\n",
				sp->st_irect[0], sp->st_irect[1],
				sp->st_irect[2], sp->st_irect[3],
				sp->st_fpos[0], sp->st_fpos[1]);

		/*
		 * Disable the rectcopy if there's anything weird about
		 * where the window is.
		 */
		if (maxscreenx == -1)
		    maxscreenx = getgdesc(GD_XPMAX);
		if (sp->st_irect[0] + winposx > maxscreenx ||
		    sp->st_irect[2] + winposx > maxscreenx ||
		    sp->st_fpos[0] + winposx > maxscreenx ||
		    sp->st_irect[0] < 0 ||
		    sp->st_irect[2] < 0 ||
		    sp->st_fpos[0] < 0)
			tp->cb_flags |= TF_STRDR;
		else
			tp->cb_flags &= ~TF_STRDR;
	}
	getmatrix(tp->cb_matrix);
	popmatrix();

	/*
	 * Finally, perform any other random setup that needs to be
	 * done.
	 */
}

/*
 * cbwinsetup - responsible for initializing the window and laying out
 *	the bars and wording in a reasonable manner within it.
 */

static void
cbwinsetup(int x)
{
	if (x < 2) {
		/*
		 * Re-initialize the window in case it was moved.
		 */
		reshapeviewport();
		getsize(&winsizex, &winsizey);
		getorigin(&winposx, &winposy);
		if (debug || printpos)
			printf("Origin <%ld,%ld>, Size <%ld,%ld>\n",
				winposx, winposy, winsizex, winsizey);

		if (doublebuffered) {
			backbuffer(TRUE);
			frontbuffer(TRUE);
		}
	
		/*
		 * Set up world space matrix.
		 */

		viewport(0, winsizex-1, 0, winsizey-1);
		color(backcolor);
		clear();
		ortho(0.0, (Coord) (DEFXPERY*width), 0.0, 
		    (Coord) YSIZE, -1.0, 1.0);
		if (debug)
			printf("ortho 0,%f X 0,%f\n",
				DEFXPERY*width, (double)YSIZE);
	}
	if (x) {
	   colorblock_t	*tp;

		/*
		 * Figure out where all 
		 * the words should go and draw them.
		 */
		tp = nextbar(1);
		while (tp != 0) {
			doscrmask(tp);
			cbdrawlegend(tp);
			tp = nextbar(0);
		}
		scrmask(winposx, winposx+winsizex, winposy, winposy+winsizey);

		/*
		 * Finally, if a message is present in any of the bars,
		 * then slam it out.
		 */
		tp = nextbar(1);
		while (tp != 0) {
			if (tp->cb_mess)
				cbmessage(tp, tp->cb_mess);
			tp = nextbar(0);
		}
	}
}

/*
 * cbpreops - perform any pre-setup initialization needed.
 */
void
cbpreops(void)
{
	frontcolor = BLACK;
	backcolor = DEFBACKCOLOR;
	palecolor = DEFPALECOLOR;

	/*
	 * Make sure no color bars are allocated.  If we ever go to dynamic
	 * allocation, this must free them all.
	 */
	nbars = 0;
	/* we WILL lose memory here, unless you want to fix it ... */
}

/*
 * cbresetbar - reset a colorbar for data
 */
void
cbresetbar(colorblock_t *cb)
{
   int		i;

	cb->cb_flags |= TF_ONETRIP;
	for (i = 0; i < MAXSECTS; i++) {
		cb->cb_mess = 0;
		cb->cb_max[i] = 0;
		cb->cb_last[i] = 0;
		cb->cb_avg[i] = 0;
		cb->cb_results[i] = 0;
		cb->cb_dispavg1 = cb->cb_lastavg1 = 0;
		cb->cb_max1 = cb->cb_dispmax = 0;
		cb->cb_nabove = 0;
		cb->cb_nbelow = 0;
	}
}

/*
 * Routine to figure out the difference between two character positions,
 * one of which may be in the GL's brain-damaged no-man's land.
 */
static int
cbydist(Screencoord iy, Screencoord siy)
{
   int j;
   static int ymax = 0;

	if (ymax == 0)
		ymax = getgdesc(GD_YPMAX);
	if (siy < 0 && iy < 0) {
		j = -iy + siy;
	} else if (siy < iy) {
		j = (ymax + siy) - iy;
	} else {
		j = siy - iy;
	}
	return(j);
}

