/*
** Copyright 1993, 1994, 1995  Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
** Based on: /proj/future/isms/gfx/lib/opengl/MG0/RCS/mg0_linedraw.c,v 1.29 1995/01/30 22:23:11 milt 
*/

#include <sys/types.h>
#include <sgidefs.h>
#include <libsc.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#include "ide_msg.h"

#define DO_DEBUG 	0


/**************************************************************************
 *                                                                        *
 * UI to the _mg0_line routine 		                                  *
 *                                                                        *
 * Expects the following:                                                 *
 *      -x (max point) x-coord y-coord z-coord red green blue alpha       *
 *      -n (min point) x-coord y-coord z-coord red green blue alpha       *
 *      colors are on a 0-100 int scale and will be converted to floats   *
 *      ranging from 0-1.0                                                *
 *	-s (stipple enable)						  *
 *	-l [line stipple pattern]					  *
 *	-a (anti-alias enable)						  *
 *	-X (X-line enable)						  *	
 *      -r [optional rss number]                                          *
 *	-t (optional texture enable) - but no tram data supplied right now*
 *	-c [optional maximum color value - default is 100]		  *
 *	-f [optional buffer, default = 1 color buf, 0x41 = overlay	  *
 *                                                                        *
 * Textured lines not yet supported via this routine(see the texture tests*
 * in mgras_re4func.c)							  *
 *                                                                        *
 * Lines with different colors don't seem to work right -- it's probably  *
 * in my port. Leave it for now. Single colors work fine.		  *
 *                                                                        *
 **************************************************************************/
mg0_line(int argc, char ** argv)
{
        __uint32_t bad_arg= 0, max_x, max_y, max_z, max_r, max_g, max_b, max_a;
        __uint32_t min_x, min_y, min_z, min_r, min_g, min_b, min_a;
        __uint32_t rssnum, stip_en, aa_en, x_en, tex_en, lspat_val;
	__uint32_t max_color = 100;
        float fl_max_r, fl_max_g, fl_max_b, fl_max_a;
        float fl_min_r, fl_min_g, fl_min_b, fl_min_a;
        __uint32_t xflag=0, nflag = 0;
        Vertex max_pt, min_pt;
	lscrlu          lscrl = {0};
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams; /* not used here */

        /* default */
	fpusetup();
        rssnum = 4; /* all rss's */
	tex_en = stip_en = aa_en = x_en = 0;
	lscrl.bits.LineStipLength = 0xf; /* go forever */
        lscrl.bits.LineStipRepeat = 0;  /* 0 = repeat just 1 time */
	global_dbuf = 1;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	   msg_printf(DBG, "arg: %c, ", argv[0][1]);
           switch (argv[0][1]) {
                case 'x':
                        if (argv[0][2]=='\0') {
                            PARSE_VERT_ARGS(max_x, max_y, max_z, max_r, max_g, max_b, max_a);
                            msg_printf(DBG, "max_x:%d, max_y:%d, max_z:%d, max_r:%d, max_g:%d, max_b:%d, max_a:%d\n", max_x, max_y, max_z, max_r, max_g, max_b, max_a);
                        }
                        else {
                                msg_printf(SUM, "Need a space after -%c\n",
                                                argv[0][1]);
                                bad_arg++;
			}
                        xflag++; break;
                case 'n':
                        if (argv[0][2]=='\0') {
                            PARSE_VERT_ARGS(min_x, min_y, min_z, min_r, min_g, min_b, min_a);
                            msg_printf(DBG, "min_x:%d, min_y:%d, min_z:%d, min_r:%d, min_g:%d, min_b:%d, min_a:%d\n", min_x, min_y, min_z, min_r, min_g, min_b, min_a);
                        }
                        else {
                                msg_printf(SUM, "Need a space after -%c\n",
                                                argv[0][1]);
                                bad_arg++;
			}
                        nflag++; break;
		case 'r':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], (int*)&rssnum);
                                argc--; argv++;
                        }
                        else
                                atob(&argv[0][2], (int*)&rssnum);
                        break;
		case 't':
                        tex_en = 1; break;
		case 's':
                        stip_en = 1; break;
		case 'a':
                        aa_en = 1; break;
		case 'X':
                        x_en = 1; break;
		case 'l':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], (int*)&lspat_val);
                                argc--; argv++;
                        }
                        else
                                atob(&argv[0][2], (int*)&lspat_val);
                        break;
		case 'c':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], (int*)&max_color);
                                argc--; argv++;
                        }
                        else
                                atob(&argv[0][2], (int*)&max_color);
                        break;
		case 'f':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], (int*)&global_dbuf);
                                argc--; argv++;
                        }
                        else
                                atob(&argv[0][2], (int*)&global_dbuf);
                        break;
                default:
			msg_printf(DBG, "unknown arg: %c\n", argv[0][1]);
                        bad_arg++; break;
           }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
        xflag++; nflag++;
#endif

        if ((xflag == 0) || (nflag == 0))
                bad_arg++;

        if ((bad_arg) || (argc)) {
		msg_printf(DBG, "bad_arg: %d, argc: %d\n", bad_arg, argc);
msg_printf(SUM, "Usage: \n");
                msg_printf(SUM,
                  "-x (max point) x-coord y-coord z-coord red green blue alpha\n");
                msg_printf(SUM,
                  "-n (min point) x-coord y-coord z-coord red green blue alpha\n");
                msg_printf(SUM, "-r [optional rss number]\n");
                msg_printf(SUM, "-s (for stipple enable)\n");
                msg_printf(SUM, "-l [line stipple pattern]\n");
                msg_printf(SUM, "-a (for anti-alias enable)\n");
                msg_printf(SUM, "-t (for texture enable)\n");
                msg_printf(SUM, "-X <cap-X>(for X enable)\n");
                msg_printf(SUM, "-c [optional max color, default=100]\n");
		msg_printf(SUM, "-f [optional buffer, default = 1 color buf, 0x41 = overlay]\n");
                msg_printf(SUM, "Arguments x and n are required.\n");
                msg_printf(SUM, "red, green, blue, and alpha are on a 0-100 scale\n");
                return (0);
        }

#ifdef MGRAS_DIAG_SIM
        xflag++; nflag++;
        max_x = 64;max_y=0;max_z = 0;max_r =255;max_g = 0;max_b=0;max_a=255;
        min_x = 0;min_y =0;min_z = 0;min_r =0;min_g = 255;min_b=0;min_a=255;
	stip_en = 0; aa_en = 0; x_en = 0; lspat_val = 0xa9876543;
	max_color = 255;
	tex_en = 1; rssDmaPar->numCols = 64;
#endif


	/* increment x,y since the vals are offsets while the user wants
	 * actual x,y locations.
	 */
	max_x++; max_y++; /* min_x++; min_y++; */

	fl_max_r = (float)max_r/max_color; fl_max_g = (float)max_g/max_color;
        fl_max_b = (float)max_b/max_color; fl_max_a = (float)max_a/max_color;

        fl_min_r = (float)min_r/max_color; fl_min_g = (float)min_g/max_color;
        fl_min_b = (float)min_b/max_color; fl_min_a = (float)min_a/max_color;

        max_pt.coords.x = (float)max_x; max_pt.coords.y = (float)max_y;
        max_pt.coords.z = (float)max_z; max_pt.coords.w = 1.0;

        max_pt.texture.x = 1.0;
        max_pt.texture.y = max_pt.coords.y;
        max_pt.texture.z = max_pt.coords.z;
        max_pt.texture.w = max_pt.coords.w;

        max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = fl_max_r;
        max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = fl_max_g;
        max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = fl_max_b;
        max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = fl_max_a;

        min_pt.coords.x = (float)min_x; min_pt.coords.y = (float)min_y;
        min_pt.coords.z = (float)min_z; min_pt.coords.w = 1.0;

        min_pt.texture.x = 0.0;
        min_pt.texture.y = min_pt.coords.y;
        min_pt.texture.z = min_pt.coords.z;
        min_pt.texture.w = min_pt.coords.w;

        min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = fl_min_r;
        min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = fl_min_g;
        min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = fl_min_b;
        min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = fl_min_a;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum,EIGHTEENBIT_MASK);
	_mgras_rss_init(rssnum);
	_draw_setup(0, 0);

	_mg0_Line(&min_pt, &max_pt, stip_en, lscrl.val, lspat_val, aa_en, x_en,
		tex_en, 0, rssDmaPar);
	return(0);
}

int
_mg0_Line(
	Vertex *v0, Vertex *v1, 
	__uint32_t stip_en, 
	__uint32_t lscrl_val, 
	__uint32_t lspat_val, 
	__uint32_t aa_en,
	__uint32_t x_en,
	__uint32_t tex_en,
	__uint32_t db_enable,
	RSS_DMA_Params *rssDmaPar)
{
    Coords vw0, vw1;
    float invDelta, tmpx, tmpy;
    Colors *cp, color0, color1;
    float offset;
    __uint32_t modeFlags, state_enables_general, source, dest, x, y;
    __uint32_t errors = 0, cfifo_status;
    float dx, dy, adjust, maj_frac;
    float wideOffset = 0.0;
    float dre, dge, dbe, dae;
    __uint32_t cmd, oneOverScale, line_options_axis, line_options_length;
    __uint32_t texmode2_tacp, texmode2_sacp, txsize_us_ssize, txsize_us_tsize;
    txscaleu		txscale = {0};
    iru			ir = {0};
    fillmodeu		fillmode = {0};
    ppblendfactoru	ppblendfactor = {0};
    ppfillmodeu		ppfillmode = {0};
    glineconfigu	glineconfig = {0};

#if DO_DEBUG
	fprintf(stderr, "v0->color->r: %f\n", v0->color.r);
        fprintf(stderr, "v0->color->g: %f\n", v0->color.g);
        fprintf(stderr, "v0->color->b: %f\n", v0->color.b);
        fprintf(stderr, "v0->color->a: %f\n", v0->color.a);

	fprintf(stderr, "v1->color->r: %f\n", v1->color.r);
        fprintf(stderr, "v1->color->g: %f\n", v1->color.g);
        fprintf(stderr, "v1->color->b: %f\n", v1->color.b);
        fprintf(stderr, "v1->color->a: %f\n", v1->color.a);

	fprintf(stderr, "v0->coords.x: %f\n", v0->coords.x);
        fprintf(stderr, "v0->coords.y: %f\n", v0->coords.y);
        fprintf(stderr, "v0->coords.z: %f\n", v0->coords.z);
        fprintf(stderr, "v0->coords.w: %f\n", v0->coords.w);

	fprintf(stderr, "v1->coords.x: %f\n", v1->coords.x);
        fprintf(stderr, "v1->coords.y: %f\n", v1->coords.y);
        fprintf(stderr, "v1->coords.z: %f\n", v1->coords.z);
        fprintf(stderr, "v1->coords.w: %f\n", v1->coords.w);
#endif


    WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    if (cfifo_status == CFIFO_TIME_OUT) {
	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
	return (-1);
    }

    /* initial values to try */
    line_options_axis = 1;
    line_options_length = 1;
    oneOverScale = 1;
    offset = 1.0;
#if 0
    modeFlags = (__GL_SHADE_SMOOTH | __GL_SHADE_RGB | __GL_SHADE_DEPTH_ITER);
#else
    modeFlags = (__GL_SHADE_SMOOTH | __GL_SHADE_RGB);
#endif
    state_enables_general = 0;

    if (tex_en)
	modeFlags |= __GL_SHADE_TEXTURE;

    if (stip_en)
	modeFlags |= __GL_SHADE_LINE_STIPPLE;
    
    vw0 = v0->coords;
    vw1 = v1->coords;

	/* try a scale of 1 */
    color0.r = v0->color.r * oneOverScale;
    color0.g = v0->color.g * oneOverScale;
    color0.b = v0->color.b * oneOverScale;
    color0.a = v0->color.a * oneOverScale;
    
    color1.r = v1->color.r * oneOverScale;
    color1.g = v1->color.g * oneOverScale;
    color1.b = v1->color.b * oneOverScale;
    color1.a = v1->color.a * oneOverScale;

    dx = vw1.x - vw0.x;
    dy = vw1.y - vw0.y;


    if (fabs_ide(dx) == fabs_ide(dy))
 	line_options_axis = __GL_X_MAJOR;
    else
    if (dy == 0)
	line_options_axis = __GL_X_MAJOR;
    else
    if (dx == 0)
	line_options_axis = __GL_X_MAJOR + 1;
    else
    if (fabs_ide(dx) > fabs_ide(dy))
	line_options_axis = __GL_X_MAJOR;
    
    if (line_options_axis == __GL_X_MAJOR) 
	line_options_length = fabs_ide(dx);	
    else
	line_options_length = fabs_ide(dy);	

#if DO_DEBUG
	fprintf(stderr, "dx %f, dy %f, axis: %x, length: %d\n", dx, dy, line_options_axis, line_options_length);
	fprintf(stderr, "fabs_ide(dx): %f, fabs_ide(dy): %f\n", fabs_ide(dx), fabs_ide(dy));
#endif
    if (wideOffset < 0.0)
	wideOffset = 0.0;

    if (line_options_axis == __GL_X_MAJOR) {
	vw0.y -= wideOffset;
	vw1.y -= wideOffset;

	dy /= fabs_ide(dx);
	if (dx > 0.0) {
	    dx = 1.0;
	    maj_frac = ceilf(vw0.x - 0.5) - vw0.x - 0.5;
	} else {
	    dx = -1.0;
	    maj_frac = vw0.x - 0.5 - floorf(vw0.x - 0.5);
	}
	adjust = dy * dx * (floorf(vw0.x + 0.5) - vw0.x);
    } else {
	vw0.x -= wideOffset;
	vw1.x -= wideOffset;

	dx /= fabs_ide(dy);
	if (dy > 0.0) {
	    dy = 1.0;
	    maj_frac = ceilf(vw0.y - 0.5) - vw0.y - 0.5;
	} else {
	    dy = -1.0;
	    maj_frac = vw0.y - 0.5 - floorf(vw0.y - 0.5);
	}
	adjust = dx * dy * (floorf(vw0.y + 0.5) - vw0.y);
    }

#if DO_DEBUG
	fprintf(stderr, "vw0.x: %f, vw0.y: %f\n", vw0.x, vw0.y);
	fprintf(stderr, "vw1.x: %f, vw1.y: %f\n", vw1.x, vw1.y);
	fprintf(stderr, "dx: %f, dy: %f, adjust: %f, fl: %f\n", 
		dx,dy,adjust,floorf(vw0.y + 0.5));
#endif

    if (x_en) {
	x = vw0.x * 1;
	y = vw0.y * 1;
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 +XLINE_XYSTARTI),4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR,((x << 16)+y), ~0x0);

	x = vw1.x * 1;
	y = vw1.y * 1;
        cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + XLINE_XYENDI), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, ((x << 16)+y), ~0x0);
    }
    else {
    	tmpx = vw0.x + 49151.5; tmpy = vw0.y + 49151.5;
    	__glMgrRE4Position2_diag(GLINE_XSTARTF, tmpx, tmpy);

    	tmpx = vw1.x + 49151.5; tmpy = vw1.y + 49151.5;
    	__glMgrRE4Position2_diag(GLINE_XENDF, tmpx, tmpy);

    	__glMgrRE4LineSlope_diag(GLINE_DX, dx, dy);
    	__glMgrRE4LineAdjust_diag(GLINE_ADJUST, adjust);
    }

    /* offset = gc->line.options.offset; */
    offset = 1.0;

    /*
    ** Set up increments for any enabled line options.
    */
    /* invDelta = __glOne / gc->line.options.length; */
    /* gc->line.options.length =  length of the major axis in pixels */
    invDelta = (float)1.0/line_options_length;

#if DO_DEBUG
    fprintf(stderr, "invD: %f, length: %d\n", invDelta, line_options_length);
#endif

    /*
    ** Window z coordinate starting position and increment.
    */
    if ((modeFlags & __GL_SHADE_DEPTH_ITER) ||
	(state_enables_general & __GL_FOG_ENABLE)) {
	__glMgrRE4Z_diag(Z_HI, v0->coords.z, 0, 0);
	__glMgrRE4Z_diag(DZE_HI, (v1->coords.z - v0->coords.z) * invDelta, 0,0);
    }
    
	fillmode.bits.ZiterEN = 1;
        fillmode.bits.ShadeEN = 1;
        fillmode.bits.RoundEN = 1;

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

    if (modeFlags & __GL_SHADE_LINE_STIPPLE) {
	/* if (!gc->line.notResetStipple) { */
	/*     gc->line.stipplePosition = 0; */

        fillmode.bits.LineStipEn = 1;
        fillmode.bits.LineStipAdv = 1;

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

	    __glMgrRE4Int_diag(LSCRL, lscrl_val);
	    __glMgrRE4Int_diag(LSPAT, lspat_val);

	    /* gc->line.repeat = 0; */
	    /* gc->line.notResetStipple = GL_TRUE; */
	/* } */
    }

#if 0
    if (modeFlags & __GL_SHADE_SLOW_FOG) {
	float f1, f0;
	float dfdx;

	/*
	** Calculate eye z coordinate increment.
	*/
	if (gc->state.hints.fog == GL_NICEST) {
	    gc->line.options.f0 = f0 = v0->eye.z;
	    gc->polygon.shader.dfdx = dfdx = 
		    (v1->eye.z - v0->eye.z) * invDelta;
	} else {
	    f0 = (*gc->procs.fogVertex)(gc, v0);
	    f1 = (*gc->procs.fogVertex)(gc, v1);
	    gc->line.options.f0 = f0;
	    gc->polygon.shader.dfdx = dfdx = (f1 - f0) * invDelta;
	}
	gc->polygon.shader.frag.f = f0 + dfdx * offset;
    }
#endif

    if (modeFlags & __GL_SHADE_TEXTURE) { /* do texture slopes */
	double inv_dx, inv_dy, inv_maj;
	Coords vt0 = v0->texture;
	Coords vt1 = v1->texture;
	double maxw = 1.0, dsw, dtw, dwi;
	__uint32_t dsw_hi, dsw_lo, dswx_lo, swu_hi, swu_lo;
	long mincoord, tmplong;
	long swrap, twrap, fs, ft;
	long scale_s, scale_t;
	double maxs, maxt;
	struct {
	    double s, t, w;
	} vt0d, vt1d;

#if 1
	vt0d.w = (double)vw0.w;
	vt1d.w = (double)vw1.w;

	/* Find maximum w inverse */
	maxw = vt0d.w > vt1d.w ? vt0d.w : vt1d.w;
	maxw = 1.0/maxw * ((1<<18) - 1);

	/* Scale all w inverses according to the maximum */
	vt0d.w *= maxw;
	vt1d.w *= maxw;

	/* for textured lines, we cannot read these registers, so only support
	 * a 1x64 line as used in the mgras_tex_poly() test in mgras_re4func.c 
 	 */
	texmode2_sacp = 0;
	texmode2_tacp = 0;
	/* rssDmaPar->numCols = line length */
	txsize_us_ssize = ide_log(rssDmaPar->numCols);
	txsize_us_tsize = 0; /* 1 */

	/* If wrapping, find the minimum of S and T and translate */
	if (!texmode2_sacp) {
	    mincoord = (long)floorf(vt0.x);
	    if ((tmplong = (long)floorf(vt1.x)) < mincoord)
		mincoord = tmplong;
	    vt0.x -= mincoord;
	    vt1.x -= mincoord;
	}
	if (!texmode2_tacp) {
	    mincoord = (long)floorf(vt0.y);
	    if ((tmplong = (long)floorf(vt1.y)) < mincoord)
		mincoord = tmplong;
	    vt0.y -= mincoord;
	    vt1.y -= mincoord;
	}
		
	/* Find maximum s and t */
	maxs = fabs_ide(vt0.x) > fabs_ide(vt1.x) ? fabs_ide(vt0.x) : fabs_ide(vt1.x);
	maxt = fabs_ide(vt0.y) > fabs_ide(vt1.y) ? fabs_ide(vt0.y) : fabs_ide(vt1.y);

	swrap = 0;
	while (maxs >= 1) {maxs /= 2; swrap++;}
	twrap = 0;
	while (maxt >= 1) {maxt /= 2; twrap++;}

	fs = 20 - txsize_us_ssize - swrap;
	ft = 20 - txsize_us_tsize - twrap;
	if (fs > 7) fs = 7;
	else if (fs < 0) fs = 0;
	if (ft > 7) ft = 7;
	else if (ft < 0) ft = 0;

#if 0	
	if(hwcx->num_sfrac >= 0) fs = hwcx->num_sfrac;
	if(hwcx->num_tfrac >= 0) ft = hwcx->num_tfrac;
#endif
	
	txscale.bits.fs = fs;
	txscale.bits.ft = ft;

	scale_s = 1 << txsize_us_ssize << fs;
	scale_t = 1 << txsize_us_tsize << ft;

	vt0d.s = vt0.x * scale_s * vt0d.w;
	vt0d.t = vt0.y * scale_t * vt0d.w;
	vt1d.s = vt1.x * scale_s * vt1d.w;
	vt1d.t = vt1.y * scale_t * vt1d.w;

	dx = vw1.x - vw0.x;
	inv_dx = (double)0.0;
	if(dx != 0) inv_dx = (double)1.0/fabs_ide(dx);

	dy = vw1.y - vw0.y;
	inv_dy = (double)0.0;
	if(dy != 0) inv_dy = (double)1.0/fabs_ide(dy);

	inv_maj = line_options_axis == __GL_X_MAJOR ? inv_dx : inv_dy;
	dsw = (vt1d.s - vt0d.s) * inv_maj;
	dtw = (vt1d.t - vt0d.t) * inv_maj;
	dwi = (vt1d.w - vt0d.w) * inv_maj;

	msg_printf(DBG, "dx: 0x%llx, dy: 0x%llx, inv_dx: 0x%llx, inv_dy: 0x%llx\n",
	*(__uint64_t*)&dx, *(__uint64_t*)&dy, *(__uint64_t*)&inv_dx, *(__uint64_t*)&inv_dy);

	msg_printf(DBG, "dsw: 0x%llx, scale_s: %d, txsize_us_ssize: %d, inv_maj: %llx, vt1d.s: 0x%llx, vt0d.s: 0x%llx, vt0d.w: 0x%llx, vt0.x: 0x%llx, vt0.y: 0x%llx, vt1.x: 0x%llx, vt1.y: 0x%llx \n", *(__uint64_t*)&dsw, scale_s, txsize_us_ssize, *(__uint64_t*)&inv_maj, *(__uint64_t*)&vt1d.s, *(__uint64_t*)&vt0d.s, *(__uint64_t*)&vt0d.w, *(__uint64_t*)&vt0.x, *(__uint64_t*)&vt0.y, *(__uint64_t*)&vt1.x, *(__uint64_t*)&vt1.y);

	msg_printf(DBG, "scale_t: %d, txsize_us_tsize: %d, vt1d.t: 0x%llx, vt0d.t: 0x%llx, vt1d.w: 0x%llx\n", scale_t, txsize_us_tsize, *(__uint64_t*)&vt1d.t, *(__uint64_t*)&vt0d.t, *(__uint64_t*)&vt1d.w);

#if DO_DEBUG
	fprintf(stderr, "\ndx: %f, dy: %f, inv_dx: %f, inv_dy: %f\n",
	dx, dy, inv_dx, inv_dy);

	fprintf(stderr, "dsw: %f, scale_s: %d, txsize_us_ssize: %d, inv_maj: %f, vt1d.s: %f, vt0d.s: %f, vt0d.w: %f, vt0.x: %f, vt0.y: %f, vt1.x: %f, vt1.y: %f \n", dsw, scale_s, txsize_us_ssize, inv_maj, vt1d.s, vt0d.s, vt0d.w, vt0.x, vt0.y, vt1.x, vt1.y);

	fprintf(stderr, "scale_t: %d, txsize_us_tsize: %d, vt1d.t: %f, vt0d.t: %f, vt1d.w: %f\n", scale_t, txsize_us_tsize, vt1d.t, vt0d.t, vt1d.w);

#endif
	/* hacks */	
	switch (tex_en) {
		case 64: dsw_hi = 0x1f;	
			dsw_lo = 0xfff7f600;	
			dswx_lo =0xfff7f000;	
			dtw = 0x0;
			dwi = 0x0;
			swu_hi = 0x8;;
			swu_lo = 0xf5c04c8f;
			break;
		case 128: dsw_hi = 0xf;	
			dsw_lo = 0xfffbfd80;	
			dswx_lo =0xfffbf000;	
			dtw = 0x0;
			dwi = 0x0;
			swu_hi = 0x4;
			swu_lo = 0x7ae023c7;
			break;
		case 32: dsw_hi = 0x3f;	
			dsw_lo = 0xffefd800;	
			dswx_lo =0xffefd000;	
			dtw = 0x0;
			dwi = 0x0;
			swu_hi = 0x11;
			swu_lo = 0xeb80ad1e;
			break;
		default: break;
	}

	HQ3_PIO_WR64_RE_XT((RSS_BASE+ HQ_RSS_SPACE(DSWE_U)), (dsw_hi) , 
		(dsw_lo),~0x0);

	__glMgrRE4Texture_diag(0, DTWE_U, dtw);
	__glMgrRE4Texture_diag(0, DWIE_U, dwi);

	HQ3_PIO_WR64_RE_XT((RSS_BASE+HQ_RSS_SPACE(SW_U)),swu_hi,swu_lo,~0x0);
	__glMgrRE4Texture_diag(0, TW_U, vt0d.t + dtw * maj_frac);


	__glMgrRE4Texture_diag(0, WI_U, vt0d.w + dwi * maj_frac);

	HQ3_PIO_WR64_RE_XT((RSS_BASE+HQ_RSS_SPACE(DSWX_U)),dsw_hi,dswx_lo,~0x0);
	HQ3_PIO_WR64_RE_XT((RSS_BASE+HQ_RSS_SPACE(DTWX_U)),dsw_hi,dswx_lo,~0x0);

	__glMgrRE4Texture_diag(0, DWIX_U, dwi);
	HQ3_PIO_WR64_RE_XT((RSS_BASE+HQ_RSS_SPACE(DSWY_U)),dsw_hi,dswx_lo,~0x0);
	HQ3_PIO_WR64_RE_XT((RSS_BASE+HQ_RSS_SPACE(DTWY_U)),dsw_hi,dswx_lo,~0x0);
	__glMgrRE4Texture_diag(0, DWIY_U, dwi);

	/*
	fprintf(stderr, "sw:%18.4lf tw: %18.4lf wi: %18.4lf\n", dsw, dtw, dwi);
	*/

	__glMgrRE4Int_diag(TXSCALE, txscale.val);

#endif
#if 0
	/* setup the texmode1 for modulate */
	NUMBER_OF_TRAMS();
	texmode1.bits.TexEn = 1;             /* enable */
   	texmode1.bits.TexEnvMode = 0;        /* MODULATE */
   	texmode1.bits.NumTram = numTRAMs/2;  /* from NUMBER_OF_TRAMS() */
   	texmode1.bits.TexReadSelect = 0;     /* location 0 */
   	texmode1.bits.TexLUTmode = 4;        /* RGBA -> RGBA */
   	texmode1.bits.TexLUTbypass = 0;      /* bypass alpha LUT */
   	texmode1.bits.BlendUnitCount = 4;    /* 5 pairs */
   	texmode1.bits.IAMode = 0;            /* not enabled */

	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + TEXMODE1), 4);
   	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   	HQ3_PIO_WR(CFIFO1_ADDR, texmode1.val, ~0x0);

#endif
#if 0
	/* set up a dma read, but don't do it. We want the s, t parameters */
	/* The size of the texture was passed via the tex_en variable */
	tmp = rssDmaPar->numCols;
	rssDmaPar->numCols = tex_en;
	errors += _mgras_DMA_TexReadSetup(rssDmaPar,DMA_BURST,DMA_READ,DMA_TRAM,0,tmp,
		db_enable);
	rssDmaPar->numCols = tmp;
#endif
    	WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
    	}

    }

    if (global_dbuf == PP_FILLMODE_DOVERA_OR_B) {
        ppfillmode.bits.PixType = PP_FILLMODE_CI8;
    	ppfillmode.bits.DrawBuffer = global_dbuf;
    	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)), ppfillmode.val, 0xffffffff);
    }

    if (modeFlags & __GL_SHADE_SMOOTH) {
	Colors *c0 = &color0;
	Colors *c1 = &color1;

	/*
	** Calculate red, green, blue and alpha value increments.
	*/
	if (modeFlags & __GL_SHADE_RGB) {
	    dre = (c1->r - c0->r) * invDelta;
	    __glMgrRE4ColorSgl_diag(DRE, dre);
	    dge = (c1->g - c0->g) * invDelta;
	    __glMgrRE4ColorSgl_diag(DGE, dge);
	    dbe = (c1->b - c0->b) * invDelta;
	    __glMgrRE4ColorSgl_diag(RE_DBE, dbe);
	    dae = (c1->a - c0->a) * invDelta;
	    __glMgrRE4ColorSgl_diag(DAE, dae);
	} else {
	    dre = dge = dbe = dae = 0.0;
	    __glMgrRE4Index_diag(DRE, (c1->r - c0->r) * invDelta);
	}
	cp = &color0;
    } else {
	cp = &color1;
	dre = dge = dbe = dae = 0.0;
    }
    
    if (modeFlags & __GL_SHADE_RGB) {
	__glMgrRE4ColorExt_diag(RE_RED, (cp->r + dre * maj_frac), 
		(cp->g + dge * maj_frac));
	__glMgrRE4ColorExt_diag(RE_BLUE, (cp->b + dbe * maj_frac), 
		(cp->a + dae * maj_frac));
    } else
	__glMgrRE4Index_diag(RE_RED, cp->r);

    if (x_en) {
    	ir.bits.Setup = 0x1;
    	ir.bits.Opcode = IR_OP_X_LINE;
    }
    else {
    	ir.bits.Setup = 0x0;
    	ir.bits.Opcode = IR_OP_GL_LINE;
    }

	/* set gllineconfig */
	glineconfig.bits.glinewidth = 0;
	
	WRITE_RSS_REG(4, GLINECONFIG_ALIAS, glineconfig.val, ~0x0);

    if (aa_en) {
	_load_aa_tables();
	state_enables_general |= __GL_LINE_SMOOTH_ENABLE;
	fillmode.bits.ZiterEN = 1;
        fillmode.bits.ShadeEN = 1;
        fillmode.bits.RoundEN = 1;
	fillmode.bits.LineAA  = 1;

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

	/* also need to turn on blending */
	source = PP_SBF_SA; dest = PP_DBF_MDA;
        ppblendfactor.bits.BlendSfactor = source; /* source alpha */
        ppblendfactor.bits.BlendDfactor = dest; /* 1-dest alpha */

        cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_BLENDFACTOR & 0x1ff)),4);        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, (ppblendfactor.val | 0x80000000),~0x0);

        cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_BLENDFACTOR & 0x1ff)),4);        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, 0x7880c8c4, ~0x0);

	ppfillmode.bits.DitherEnab = 1;
        ppfillmode.bits.DitherEnabA = 1;
        ppfillmode.bits.ZEnab = 0;
        ppfillmode.bits.BlendEnab = 1; /* turn on blending */

	if (global_dbuf == PP_FILLMODE_DBUFA) {
        	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
	}
	else if (global_dbuf == PP_FILLMODE_DOVERA_OR_B) {
        	ppfillmode.bits.PixType = PP_FILLMODE_CI8;
	}

        ppfillmode.bits.DrawBuffer = global_dbuf;

        cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_FILLMODE & 0x1ff)),4);        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, (ppfillmode.val | 0x0c000000), ~0x0);

	/* set gllineconfig */
	glineconfig.bits.glinewidth = 1;
	
	WRITE_RSS_REG(4, GLINECONFIG_ALIAS, glineconfig.val, ~0x0);
    }

    if (clean_disp) {
        ppfillmode.bits.ZEnab = 0;
	ppfillmode.bits.DitherEnab = 0;
        ppfillmode.bits.DitherEnabA = 0;
	ppfillmode.bits.LogicOP = PP_FILLMODE_LO_OR;
        ppfillmode.bits.LogicOpEnab = 1;

	if (global_dbuf == PP_FILLMODE_DBUFA) {
        	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
	}
	else if (global_dbuf == PP_FILLMODE_DOVERA_OR_B) {
        	ppfillmode.bits.PixType = PP_FILLMODE_CI8;
	}

        ppfillmode.bits.DrawBuffer = global_dbuf;
	
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)), ppfillmode.val, 0xffffffff);
    }

    if (state_enables_general & __GL_LINE_SMOOTH_ENABLE) {
	ir.bits.EPF_First = 0x1;
	ir.bits.EPF_Last = 0x1;
    } else {
	ir.bits.EPF_First = 0x0;
	ir.bits.EPF_Last = 0x0;
    }
#if 1
    if (tex_en) {
    	__glMgrRE4TEIR_diag(ir.val);
    }
    else {
    	__glMgrRE4IR_diag(ir.val);
    }
#endif

    return errors;
}
