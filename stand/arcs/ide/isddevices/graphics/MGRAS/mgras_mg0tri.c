/*
** Copyright 1993, Silicon Graphics, Inc.
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
** Based on /proj/future/isms/gfx/lib/opengl/MG0/RCS/mg0_polydraw.c,v 1.28 1994/11/24 02:32:54 milt 
*/

#include <sys/types.h>
#include <sgidefs.h>
#include "uif.h"
#include "libsc.h"
#include <math.h>
#include <sys/mgrashw.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

__uint32_t tri_shade_en = 1;
__uint32_t tri_dith_en = 1;
__uint32_t tri_tex_en = 0;
__uint32_t rgb121212 = 0;

/**************************************************************************
 *                                                                        *
 * UI to the mg0_FillTriangle routine 					  *
 *                                                                        *
 * Expects the following:						  *
 *	-x (max point) x-coord y-coord z-coord red green blue alpha	  *
 *	-d (mid point) x-coord y-coord z-coord red green blue alpha	  *
 *	-n (min point) x-coord y-coord z-coord red green blue alpha	  *
 * 	-c (constant color) red geen blue alpha.			  *
 *		-c will ignore any other specified color. So if you use   *
 *		-c you don't need to specify individual vertex colors.	  *
 * 	colors are on a 0-100 int scale and will be converted to floats   *
 *	ranging from 0-1.0						  *
 *	-r [optional rss number]					  *
 * 	-o [optional max color]						  *
 *	-b [optional buffer, default is 1=color buffer A]		  *
 *		0x41 = overlay buffer AB				  *
 *									  *
 **************************************************************************/
int
mg0_tri(int argc, char ** argv)
{
	__uint32_t bad_arg= 0, max_x, max_y, max_z, max_r, max_g, max_b, max_a;
	__uint32_t mid_x, mid_y, mid_z, mid_r, mid_g, mid_b, mid_a;
	__uint32_t min_x, min_y, min_z, min_r, min_g, min_b, min_a;
	__uint32_t con_r, con_g, con_b, con_a, cmd, rssnum, max_color;
	float fl_max_r, fl_max_g, fl_max_b, fl_max_a;
	float fl_mid_r, fl_mid_g, fl_mid_b, fl_mid_a;
	float fl_min_r, fl_min_g, fl_min_b, fl_min_a;
	float fl_con_r, fl_con_g, fl_con_b, fl_con_a;
	__uint32_t xflag=0, dflag = 0, nflag = 0, cflag = 0, dbuffer = 1;
	Vertex max_pt, mid_pt, min_pt;
	ppfillmodeu     ppfillmode = {0};
        ppzmodeu        ppzmode = {0};
        ppblendfactoru  ppblendfactor = {0};
	fillmodeu    fillmode = {0};

	/* default */
	rssnum = 4; /* all rss's */
	max_color = 100;
	tri_shade_en = 1;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
           switch (argv[0][1]) {
                case 'x':
                        if (argv[0][2]=='\0') {
                            PARSE_TRI_ARGS(max_x, max_y, max_z, max_r, max_g, max_b, max_a); 
                        }
                        else {
                                msg_printf(SUM, "Need a space after -%c\n",
						argv[0][1]);
				bad_arg++;
			}
                        xflag++; break;
                case 'd':
                        if (argv[0][2]=='\0') {
                            PARSE_TRI_ARGS(mid_x, mid_y, mid_z, mid_r, mid_g, mid_b, mid_a); 
                        }
                        else {
                                msg_printf(SUM, "Need a space after -%c\n",
						argv[0][1]);
				bad_arg++;
			}
                        dflag++; break;
                case 'n':
                        if (argv[0][2]=='\0') {
                            PARSE_TRI_ARGS(min_x, min_y, min_z, min_r, min_g, min_b, min_a); 
                        }
                        else {
                                msg_printf(SUM, "Need a space after -%c\n",
						argv[0][1]);
				bad_arg++;
			}
                        nflag++; break;
                case 'c':
                        if (argv[0][2]=='\0') {
                            PARSE_TRI_ARGS_C(con_r, con_g, con_b, con_a); 
                        }
                        else {
                                msg_printf(SUM, "Need a space after -%c\n",
						argv[0][1]);
				bad_arg++;
			}
                        break;
                case 'r':
                        if (argv[0][2]=='\0') {
                           	atob(&argv[1][0], (int*)&rssnum);
				argc--; argv++;
                        }
                        else
				atob(&argv[0][2], (int*)&rssnum);
                        break;
                case 'o':
                        if (argv[0][2]=='\0') {
                           	atob(&argv[1][0], (int*)&max_color);
				argc--; argv++;
                        }
                        else
				atob(&argv[0][2], (int*)&max_color);
                        break;
                case 'b':
                        if (argv[0][2]=='\0') {
                           	atob(&argv[1][0], (int*)&dbuffer);
				argc--; argv++;
                        }
                        else
				atob(&argv[0][2], (int*)&dbuffer);
                        break;
		default: 
			bad_arg++; break;
           }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
	xflag++; dflag++; nflag++;
#endif

	if ((xflag == 0) || (dflag == 0) || (nflag == 0))
		bad_arg++;

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "Usage: \n");
		msg_printf(SUM, 
		  "-x (max point) x-coord y-coord z-coord red green blue alpha\n");
		msg_printf(SUM, 
		  "-d (mid point) x-coord y-coord z-coord red green blue alpha\n");
		msg_printf(SUM, 
		  "-n (min point) x-coord y-coord z-coord red green blue alpha\n");
        	msg_printf(SUM, 
		  "-c red green blue alpha. -c will ignore any other \n");
		msg_printf(SUM, "specified color. So if you use -c you don't need to specify individual \n");
		msg_printf(SUM, "vertex colors.\n"); 
		msg_printf(SUM, "-r [optional rss number]\n");
		msg_printf(SUM, "-b [optional buffer, default =1 color buf, 0x41 = overlay]\n");
		msg_printf(SUM, "Arguments x, d, and n are required. c is optional.\n");
		msg_printf(SUM, "red, green, blue, and alpha are on a 0-100 scale\n");
		return (0);
	}

#ifdef MGRAS_DIAG_SIM
	xflag++; dflag++; nflag++;
	max_x = 10;max_y=60;max_z = 0;max_r = 100;max_g = 100;max_b=0;max_a=0;
	mid_x = 60;mid_y=35;mid_z = 0;mid_r = 100;mid_g = 0;mid_b=100;mid_a=0;
	min_x = 10;min_y =10;min_z = 0;min_r = 100;min_g = 0;min_b=0;min_a=0;
	cflag = 0;
	con_r = 23; con_g = 88; con_b = 33; con_a =11;	
#endif
	if (cflag) {
		max_r = mid_r = min_r = con_r;
		max_g = mid_g = min_g = con_g;
		max_b = mid_b = min_b = con_b;
		max_a = mid_a = min_a = con_a;
	}

	/* increment the x,y,z coords by 1 since the _mg0_FillTriangle()
 	 * routine uses these as offsets not as actual pixel locations.
	 */
	max_y++; max_z++;
	mid_y++; mid_z++;
	min_y++; min_z++;

	fl_max_r = (float)max_r/max_color; fl_max_g = (float)max_g/max_color;
	fl_max_b = (float)max_b/max_color; fl_max_a = (float)max_a/max_color;

	fl_mid_r = (float)mid_r/max_color; fl_mid_g = (float)mid_g/max_color;
	fl_mid_b = (float)mid_b/max_color; fl_mid_a = (float)mid_a/max_color;

	fl_min_r = (float)min_r/max_color; fl_min_g = (float)min_g/max_color;
	fl_min_b = (float)min_b/max_color; fl_min_a = (float)min_a/max_color;

	max_pt.coords.x = (float)max_x; max_pt.coords.y = (float)max_y;
        max_pt.coords.z = (float)max_z; max_pt.coords.w = 1.0;

        max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = fl_max_r;
        max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = fl_max_g;
        max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = fl_max_b;
        max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = fl_max_a;

        mid_pt.coords.x = (float)mid_x; mid_pt.coords.y = (float)mid_y;
        mid_pt.coords.z = (float)mid_z; mid_pt.coords.w = 1.0;

        mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = fl_mid_r;
        mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = fl_mid_g;
        mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = fl_mid_b;
        mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = fl_mid_a;

        min_pt.coords.x = (float)min_x; min_pt.coords.y = (float)min_y;
        min_pt.coords.z = (float)min_z; min_pt.coords.w = 1.0;

        min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = fl_min_r;
        min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = fl_min_g;
        min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = fl_min_b;
        min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = fl_min_a;

#if 1
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum,EIGHTEENBIT_MASK);
#endif
	/* clear regs */
        _mgras_rss_init(rssnum);
	_draw_setup(0,0);

        /* set up needed registers */
	if (tri_dith_en) {
        	ppfillmode.bits.DitherEnab = 1;
        	ppfillmode.bits.DitherEnabA = 1;
	}
	else {
        	ppfillmode.bits.DitherEnab = 0;
        	ppfillmode.bits.DitherEnabA = 0;
		tri_dith_en = 1;
	}
        ppfillmode.bits.ZEnab = 0;

	if (dbuffer == PP_FILLMODE_DBUFA)
        	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DBUFA; /* 0x1 */
	else if (dbuffer == PP_FILLMODE_DOVERA_OR_B)
        	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DOVERA_OR_B; /* 0x41 */

        ppzmode.bits.Zfunc = PP_ZF_LESS;

	/* reset fillmode to turn off fast fill since not everyone wants it */
        fillmode.bits.FastFill = 0;
        fillmode.bits.RoundEN = 1;
        fillmode.bits.ZiterEN = 0;
	if (tri_shade_en)
        	fillmode.bits.ShadeEN = 1;
	else {
        	fillmode.bits.ShadeEN = 0;
		/* reset shade_en for the default */
		tri_shade_en = 1;
	}

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_ZMODE & 0x1ff)), (ppzmode.val | 0x0ffffff0), 0xffffffff);

	if (rgb121212) {
	   if (is_old_pp) {
		HQ3_PIO_WR_RE(RSS_BASE +HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)),
		0xfffbffff, 0xffffffff);
	   }
	   else {
		HQ3_PIO_WR_RE(RSS_BASE +HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)),
		0xffffffff, 0xffffffff);
	   }
           ppfillmode.bits.PixType = PP_FILLMODE_RGB121212;
	   rgb121212 = 0;
	}
	else {
	   if (is_old_pp) {
		HQ3_PIO_WR_RE(RSS_BASE+ HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)),
			 0xfffbffff, 0xffffffff);
	   }
	   else {
		HQ3_PIO_WR_RE(RSS_BASE+ HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)),
			 0xffffffff, 0xffffffff);
	   }
           ppfillmode.bits.PixType = PP_FILLMODE_RGBA8888;
	}

	if (dbuffer == PP_FILLMODE_DOVERA_OR_B)
        	ppfillmode.bits.PixType = PP_FILLMODE_CI8;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE& 0x1ff)), (ppfillmode.val | 0x0c000000), 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(FILLMODE), fillmode.val, 0xffffffff);

#if 0
        cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_ZMODE & 0x1ff)),4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
        HQ3_PIO_WR(CFIFO1_ADDR, (ppzmode.val | 0x0ffffff0), ~0x0, HQ_MOD, 0);

        /* set the color mask registers */

        cmd=BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_COLORMASKLSBA& 0x1ff)),4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
        HQ3_PIO_WR(CFIFO1_ADDR, PP_CM_RGBA8888, ~0x0, HQ_MOD, 0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_FILLMODE & 0x1ff)),4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
        HQ3_PIO_WR(CFIFO1_ADDR, (ppfillmode.val | 0x0c000000), ~0x0, HQ_MOD, 0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE),4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
        HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0, HQ_MOD, 0);

#endif
#if 1
	msg_printf(DBG, "max_pt->color->r: %d\n", max_r);
	msg_printf(DBG, "max_pt->color->g: %d\n", max_g);
	msg_printf(DBG, "max_pt->color->b: %d\n", max_b);
	msg_printf(DBG, "max_pt->color->a: %d\n", max_a);

	msg_printf(DBG, "mid_pt->color->r: %d\n", mid_r);
	msg_printf(DBG, "mid_pt->color->g: %d\n", mid_g);
	msg_printf(DBG, "mid_pt->color->b: %d\n", mid_b);
	msg_printf(DBG, "mid_pt->color->a: %d\n", mid_a);

	msg_printf(DBG, "min_pt->color->r: %d\n", min_r);
	msg_printf(DBG, "min_pt->color->g: %d\n", min_g);
	msg_printf(DBG, "min_pt->color->b: %d\n", min_b);
	msg_printf(DBG, "min_pt->color->a: %d\n", min_a);

	msg_printf(DBG, "max_pt->coords.x: %d\n", max_x);
	msg_printf(DBG, "max_pt->coords.y: %d\n", max_y);
	msg_printf(DBG, "max_pt->coords.z: %d\n", max_z);

	msg_printf(DBG, "mid_pt->coords.x: %d\n", mid_x);
	msg_printf(DBG, "mid_pt->coords.y: %d\n", mid_y);
	msg_printf(DBG, "mid_pt->coords.z: %d\n", mid_z);

	msg_printf(DBG, "min_pt->coords.x: %d\n", min_x);
	msg_printf(DBG, "min_pt->coords.y: %d\n", min_y);
	msg_printf(DBG, "min_pt->coords.z: %d\n", min_z);
#endif
	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, 0);
}


/* hacked up defs for floorf and ceilf. They seem to basically work for 
 * most common numbers, but I haven't exhaustively test them
 */
#if 1
float
floorf(float orignum)
{
        __int32_t int_num;
        float floornum;

	msg_printf(DBG, "floor orignum: %d, ", (__int32_t)orignum);
        int_num = (__int32_t)(orignum * 1);
        floornum = (float)(int_num * 1.0);;
	msg_printf(DBG, "int_num: %d, floornum: %d\n", 
		int_num, (__int32_t)floornum);

        if ((orignum < 0.0) && (orignum < floornum))
                floornum -= 1.0;

        return (floornum);
}

float
ceilf(float orignum)
{
        __int32_t int_num;
	float ceilnum;

	msg_printf(DBG, "ceil orignum: %d, ", (__int32_t)orignum);
        int_num = (__int32_t)(orignum * 1);
        ceilnum = (float)(int_num * 1.0);
	msg_printf(DBG, "int_num: %d, ceilnum: %d\n", 
		int_num, (__int32_t)ceilnum);

        if ((orignum < 0.0) && (orignum < ceilnum))
                ceilnum -= 1.0;

        if (orignum > ceilnum)
                ceilnum += 1.0;

        return (ceilnum);
}

#else

float
floorf(float orignum)
{
	return (orignum);
}

float
ceilf(float orignum)
{
	return (orignum);
}
#endif


int
_mg0_FillTriangle(Vertex *max_pt, Vertex *mid_pt, Vertex *min_pt, __uint32_t depthkludge)
{
    Coords aw, bw, cw;
    Colors ac, bc, cc;
    __uint32_t actual, modeFlags, oneOverScale, state_enables_general = 0;
    __uint32_t texmode2_bits, txsize_bits, cfifo_status;
    float x0, x1, x2;
    float ymax, ymid;
    float dxdy0, dxdy1, dxdy2;
    float red, green, blue, alpha;
    float dre, dge, dbe, dae;
    float drx, dgx, dbx, dax;
    float major_y, major_x, minor_y, perc, tmpflt;
    float xmid, span_x, deltY, deltX, zmid;
    float z, dzx, dze, dzdy;
    float rmid, gmid, bmid, amid;
    float drdy, dgdy, dbdy, dady;
    float warpfactor;
    float tmp0, tmp1;
    __uint64_t n1, direction, longnum;
    iru		    ir = {0};
    txscaleu        txscale = {0};

#if 0
	fprintf(stderr, "max_pt->color->r: %f\n", __tmp = max_pt->color.r);
	fprintf(stderr, "max_pt->color->g: %f\n", __tmp = max_pt->color.g);
	fprintf(stderr, "max_pt->color->b: %f\n", __tmp = max_pt->color.b);
	fprintf(stderr, "max_pt->color->a: %f\n", __tmp = max_pt->color.a);

	fprintf(stderr, "mid_pt->color->r: %f\n", __tmp = mid_pt->color.r);
	fprintf(stderr, "mid_pt->color->g: %f\n", __tmp = mid_pt->color.g);
	fprintf(stderr, "mid_pt->color->b: %f\n", __tmp = mid_pt->color.b);
	fprintf(stderr, "mid_pt->color->a: %f\n", __tmp = mid_pt->color.a);

	fprintf(stderr, "min_pt->color->r: %f\n", __tmp = min_pt->color.r);
	fprintf(stderr, "min_pt->color->g: %f\n", __tmp = min_pt->color.g);
	fprintf(stderr, "min_pt->color->b: %f\n", __tmp = min_pt->color.b);
	fprintf(stderr, "min_pt->color->a: %f\n", __tmp = min_pt->color.a);

	fprintf(stderr, "max_pt->coords.x: %f\n", __tmp = max_pt->coords.x);
	fprintf(stderr, "max_pt->coords.y: %f\n", __tmp = max_pt->coords.y);
	fprintf(stderr, "max_pt->coords.z: %f\n", __tmp = max_pt->coords.z);
	fprintf(stderr, "max_pt->coords.w: %f\n", __tmp = max_pt->coords.w);

	fprintf(stderr, "mid_pt->coords.x: %f\n", __tmp = mid_pt->coords.x);
	fprintf(stderr, "mid_pt->coords.y: %f\n", __tmp = mid_pt->coords.y);
	fprintf(stderr, "mid_pt->coords.z: %f\n", __tmp = mid_pt->coords.z);
	fprintf(stderr, "mid_pt->coords.w: %f\n", __tmp = mid_pt->coords.w);

	fprintf(stderr, "min_pt->coords.x: %f\n", __tmp = min_pt->coords.x);
	fprintf(stderr, "min_pt->coords.y: %f\n", __tmp = min_pt->coords.y);
	fprintf(stderr, "min_pt->coords.z: %f\n", __tmp = min_pt->coords.z);
	fprintf(stderr, "min_pt->coords.w: %f\n", __tmp = min_pt->coords.w);
#endif
    
#if COMPILER_62_ALLOWS_THIS
    (__uint32_t)__tmp = max_pt->color.r; msg_printf(DBG, "max_pt->color->r: 0x%x\n", __tmp);
    (__uint32_t)__tmp = max_pt->color.g; msg_printf(DBG, "max_pt->color->g: 0x%x\n", __tmp);
    (__uint32_t)__tmp = max_pt->color.b; msg_printf(DBG, "max_pt->color->b: 0x%x\n", __tmp);
    (__uint32_t)__tmp = max_pt->color.a; msg_printf(DBG, "max_pt->color->a: 0x%x\n", __tmp);

    (__uint32_t)__tmp = mid_pt->color.r; msg_printf(DBG, "mid_pt->color->r: 0x%x\n", __tmp);
    (__uint32_t)__tmp = mid_pt->color.g; msg_printf(DBG, "mid_pt->color->g: 0x%x\n", __tmp);
    (__uint32_t)__tmp = mid_pt->color.b; msg_printf(DBG, "mid_pt->color->b: 0x%x\n", __tmp);
    (__uint32_t)__tmp = mid_pt->color.a; msg_printf(DBG, "mid_pt->color->a: 0x%x\n", __tmp);

    (__uint32_t)__tmp = min_pt->color.r; msg_printf(DBG, "min_pt->color->r: 0x%x\n", __tmp);
    (__uint32_t)__tmp = min_pt->color.g; msg_printf(DBG, "min_pt->color->g: 0x%x\n", __tmp);
    (__uint32_t)__tmp = min_pt->color.b; msg_printf(DBG, "min_pt->color->b: 0x%x\n", __tmp);
    (__uint32_t)__tmp = min_pt->color.a; msg_printf(DBG, "min_pt->color->a: 0x%x\n", __tmp);

    (__uint32_t)__tmp = max_pt->coords.x; msg_printf(DBG, "max_pt->coords.x: 0x%x\n", __tmp);
    (__uint32_t)__tmp = max_pt->coords.y; msg_printf(DBG, "max_pt->coords.y: 0x%x\n", __tmp);
    (__uint32_t)__tmp = max_pt->coords.z; msg_printf(DBG, "max_pt->coords.z: 0x%x\n", __tmp);
    (__uint32_t)__tmp = max_pt->coords.w; msg_printf(DBG, "max_pt->coords.w: 0x%x\n", __tmp);

    (__uint32_t)__tmp = mid_pt->coords.x; msg_printf(DBG, "mid_pt->coords.x: 0x%x\n", __tmp);
    (__uint32_t)__tmp = mid_pt->coords.y; msg_printf(DBG, "mid_pt->coords.y: 0x%x\n", __tmp);
    (__uint32_t)__tmp = mid_pt->coords.z; msg_printf(DBG, "mid_pt->coords.z: 0x%x\n", __tmp);
    (__uint32_t)__tmp = mid_pt->coords.w; msg_printf(DBG, "mid_pt->coords.w: 0x%x\n", __tmp);

    (__uint32_t)__tmp = min_pt->coords.x; msg_printf(DBG, "min_pt->coords.x: 0x%x\n", __tmp);
    (__uint32_t)__tmp = min_pt->coords.y; msg_printf(DBG, "min_pt->coords.y: 0x%x\n", __tmp);
    (__uint32_t)__tmp = min_pt->coords.z; msg_printf(DBG, "min_pt->coords.z: 0x%x\n", __tmp);
    (__uint32_t)__tmp = min_pt->coords.w; msg_printf(DBG, "min_pt->coords.w: 0x%x\n", __tmp);
#endif

    warpfactor = 0.0;
    oneOverScale = 1;
    aw = min_pt->coords;
    bw = mid_pt->coords;
    cw = max_pt->coords;

    /* find source for these */
    ymid = floorf(bw.y);
    ymax = floorf(cw.y);

#if 0
    modeFlags =(__GL_SHADE_SMOOTH | __GL_SHADE_DEPTH_ITER | __GL_SHADE_TEXTURE);
#endif

    modeFlags =(__GL_SHADE_SMOOTH | __GL_SHADE_RGB | __GL_SHADE_DEPTH_ITER);

    if (tri_tex_en)
	modeFlags |= __GL_SHADE_TEXTURE;

    actual = 0;
    texmode2_bits = 0; /* no texture */

#if 0 
	fprintf(stdout, "bw.y: %f, aw.y: %f\n", bw.y, aw.y);
#endif

    WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    if (cfifo_status == CFIFO_TIME_OUT) {
	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
	return (-1);
    }

    __glMgrRE4Position_diag(TRI_YMID, bw.y + 49151.5, aw.y + 49151.5, 1);

    /* Slopes along major edge */
    major_y = cw.y - aw.y;
    major_x = aw.x - cw.x;
    minor_y = cw.y - bw.y;
    if (major_y == 0.0) 
dxdy0 = perc = 0.0; 
    else {
	dxdy0 = major_x / major_y; 
	perc = minor_y / major_y;
    }

#if 0
	fprintf(stdout, "cw.y: %f, aw.y: %f, aw.x: %f, cw.x: %f, bw.y: %f\n",
		cw.y, aw.y, aw.x, cw.x, bw.y);
	fprintf(stdout, "major_y: %f, maj_x: %f, min_y: %f, dxdy0: %f\n",
		major_y, major_x, minor_y, dxdy0);
#endif

    __glMgrRE4TriSlope_diag(TRI_DXDY0_HI, dxdy0);

    /* XY slopes along minor edges */
    if (minor_y == 0.0) 
	dxdy1 = 0.0;
    else {
	dxdy1 = (bw.x - cw.x) / minor_y;
    }

    __glMgrRE4TriSlope_diag(TRI_DXDY1_HI, dxdy1);

    if (bw.y - aw.y == 0.0) 
	dxdy2 = 0.0;
    else {
	tmpflt = bw.y - aw.y;
	dxdy2 = (aw.x - bw.x) / tmpflt;
    }

    __glMgrRE4TriSlope_diag(TRI_DXDY2_HI_IR, dxdy2);

    /* now find the x coord of a point on the major edge with y == ymid */
    xmid = cw.x + perc * major_x; 
    direction = xmid <= bw.x ? IR_OP_AREA_LTOR : IR_OP_AREA_RTOL;
    span_x = bw.x - xmid;
    deltY = cw.y - ymax;
    x0 = cw.x + dxdy0 * deltY;
    x1 = cw.x + dxdy1 * deltY;
    x2 = bw.x + dxdy2 * (bw.y - ymid);

#if 0
	fprintf(stdout, "x0: %f, x1: %f, dxdy0: %f, dxdy1: %f, deltY: %f, ymax: %f\n", x0, x1, dxdy0, dxdy1, deltY, ymax);
#endif
	
    __glMgrRE4Position_diag(TRI_X0, x0 + 49151.5, x1 + 49151.5, 1);
    __glMgrRE4Position_diag(TRI_X2, x2 + 49151.5, cw.y + 49151.5, 1);

    /* now prepare for color and z specification */
    deltX = (direction == IR_OP_AREA_LTOR ? ceilf(x0) : floorf(x0)) - cw.x;
    n1 = (long)dxdy0;

    drx = dgx = dbx = dax = 0.0;
    drdy = dgdy = dbdy = dady = 0.0;
    dre = dge = dbe = dae = 0.0;

/* try a scale of 1 */
    if (modeFlags & __GL_SHADE_SMOOTH) {
	ac.r = min_pt->colors[__GL_FRONTFACE].r * oneOverScale;
	ac.g = min_pt->colors[__GL_FRONTFACE].g * oneOverScale;
	ac.b = min_pt->colors[__GL_FRONTFACE].b * oneOverScale;
	ac.a = min_pt->colors[__GL_FRONTFACE].a * oneOverScale;
	
	bc.r = mid_pt->colors[__GL_FRONTFACE].r * oneOverScale;
	bc.g = mid_pt->colors[__GL_FRONTFACE].g * oneOverScale;
	bc.b = mid_pt->colors[__GL_FRONTFACE].b * oneOverScale;
	bc.a = mid_pt->colors[__GL_FRONTFACE].a * oneOverScale;
	
	cc.r = max_pt->colors[__GL_FRONTFACE].r * oneOverScale;
	cc.g = max_pt->colors[__GL_FRONTFACE].g * oneOverScale;
	cc.b = max_pt->colors[__GL_FRONTFACE].b * oneOverScale;
	cc.a = max_pt->colors[__GL_FRONTFACE].a * oneOverScale;
	
	rmid = cc.r + perc * (ac.r - cc.r);
	if (modeFlags & __GL_SHADE_RGB) {
	    gmid = cc.g + perc * (ac.g - cc.g);
	    bmid = cc.b + perc * (ac.b - cc.b);
	    amid = cc.a + perc * (ac.a - cc.a);
	}
#if 0
	fprintf(stdout," span_x: %f\n", span_x);
#endif
	if (span_x != 0.0) {
	    drx = (bc.r - rmid) / span_x;
#if 0
		fprintf(stdout," span_x: %f, drx: %f, bc.r : %f, rmid: %f, perc: %f\n", span_x, drx, bc.r, rmid, perc);
#endif
	    if (modeFlags & __GL_SHADE_RGB) {
		dgx = (bc.g - gmid) / span_x;
		dbx = (bc.b - bmid) / span_x;
		dax = (bc.a - amid) / span_x;
	    }
	}
	if (major_y != 0.0) {
	    drdy = (ac.r - cc.r - drx * major_x) / major_y;
	    if (modeFlags & __GL_SHADE_RGB) {
		dgdy = (ac.g - cc.g - dgx * major_x) / major_y;
		dbdy = (ac.b - cc.b - dbx * major_x) / major_y;
		dady = (ac.a - cc.a - dax * major_x) / major_y;
	    }
	}
	
	red = cc.r + drdy * deltY + drx * deltX;

	if (modeFlags & __GL_SHADE_RGB) {
	    green = cc.g + dgdy * deltY + dgx * deltX;
#if 0
		fprintf(stdout, "green being calculated...\n");
	fprintf(stdout, "green at loc x is %f, cc.g: %f\n", green, cc.g);
#endif
	    blue = cc.b + dbdy * deltY + dbx * deltX;
	    alpha = cc.a + dady * deltY + dax * deltX;
	}

#if 0
	fprintf(stdout, "red at loc 1 is %f\n", red);
	fprintf(stdout, "green at loc 1 is %f, cc.g: %f\n", green, cc.g);
#endif
	dre = drdy + n1 * drx;

#if 0
	fprintf(stdout, "dre: %f, drdy: %f, n1: %f, drx: %f\n",
		dre, drdy, n1, drx);
#endif
	if (modeFlags & __GL_SHADE_RGB) {
	    dge = dgdy + n1 * dgx;
	    dbe = dbdy + n1 * dbx;
	    dae = dady + n1 * dax;
	}

	if (direction == IR_OP_AREA_RTOL) {
	    drx = -drx;
	    if (modeFlags & __GL_SHADE_RGB) {
		dgx = -dgx;
		dbx = -dbx;
		dax = -dax;
	    }
	}

	if ((direction == IR_OP_AREA_LTOR) ^ (dxdy0 >= 0)) {
	    dre -= drx;
#if 0
	fprintf(stdout, "dre at loc 2: %f, drx: %f\n", dre, drx);
#endif
	    if (modeFlags & __GL_SHADE_RGB) {
		dge -= dgx;
		dbe -= dbx;
		dae -= dax;
	    }
	}
    } else {
	Colors flatColor =  cc; /* *gc->vertex.provoking->color */

#if 0
	flatColor.r *= gc->frontBuffer.oneOverRedScale;
	flatColor.g *= gc->frontBuffer.oneOverGreenScale;
	flatColor.b *= gc->frontBuffer.oneOverBlueScale;
	flatColor.a *= gc->frontBuffer.oneOverAlphaScale;
#endif

	flatColor.r *= oneOverScale;
	flatColor.g *= oneOverScale;
	flatColor.b *= oneOverScale;
	flatColor.a *= oneOverScale;

	red = flatColor.r;
	if (modeFlags & __GL_SHADE_RGB) {
	    green = flatColor.g;
	    blue = flatColor.b;
	    alpha = flatColor.a;
	}
#if 0
	fprintf(stdout, "red at loc 2 %f\n", red);
	fprintf(stdout, "green at loc 3 is %f\n", green);
#endif
    }

#if 0
	fprintf(stdout, "red at loc 3 is %f\n", red);
	fprintf(stdout, "green at loc 3 is %f\n", green);
#endif

    if (modeFlags & __GL_SHADE_RGB) {
	__glMgrRE4ColorExt_diag(RE_RED, red, green);

#if 0
	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + RE_RED), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
        HQ3_PIO_WR(CFIFO1_ADDR, 0x00f98a00, ~0x0, HQ_MOD, 0);
#endif

	__glMgrRE4ColorExt_diag(RE_BLUE, blue, alpha);

	__glMgrRE4ColorExt_diag(DRE, dre, drx);

#if 0
	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + DRE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
        HQ3_PIO_WR(CFIFO1_ADDR, 0xfff33400, ~0x0, HQ_MOD, 0);

#endif
	__glMgrRE4ColorExt_diag(DGE, dge, dgx);
	__glMgrRE4ColorExt_diag(RE_DBE, dbe, dbx);
	__glMgrRE4ColorExt_diag(DAE, dae, dax);
    } else {
	__glMgrRE4Index_diag(RE_RED, red);

	__glMgrRE4Index_diag(DRE, dre);

	__glMgrRE4Index_diag(DRX, drx);
    }

    if (((modeFlags & __GL_SHADE_DEPTH_ITER) && depthkludge) ||
	(state_enables_general & __GL_FOG_ENABLE)) {
	zmid = cw.z + perc * (aw.z - cw.z);
	if (span_x == 0.0)
	    dzx = 0.0;
	else
	    dzx = (bw.z - zmid) / span_x;
	if (major_y == 0.0)
	    dzdy = 0.0;
	else
	    dzdy = ((aw.z - cw.z) - dzx * major_x) / major_y;
	z = cw.z + dzdy * deltY + dzx * deltX;
	dze = dzdy + n1 * dzx;
	
	if (direction == IR_OP_AREA_RTOL)
	    dzx = -dzx;

	if ((direction == IR_OP_AREA_LTOR) ^ (dxdy0 >= 0))
	    dze -= dzx;

	/* there is some weird stuff going on here. z values are scaled
	 *
	 * Let's be gross and hard code this stuff since we know what we
	 * want to do in our tests!
	 */	 
	if (depthkludge) { /* if we have the varied z triangle of scene2) */
		if (z == 16.0)	{ /* non-varied z red triangle of 50*/
			__glMgrRE4Z_diag(Z_HI, 0x8, 0x51eb8000, depthkludge);
			__glMgrRE4Z_diag(DZX_HI, 0, 0, depthkludge ); /* 0 */
			__glMgrRE4Z_diag(DZE_HI, 0, 0, depthkludge); /* 0 */
		}
		else if (z == 0.0) { /* green triangle, min=max=0, mid=100 */
			__glMgrRE4Z_diag(Z_HI, 0x8, 0x051eb000, depthkludge);
			__glMgrRE4Z_diag(DZX_HI, 0, 0x0a3d7080, depthkludge); 
			__glMgrRE4Z_diag(DZE_HI, 0xffffffff, 0xf5c28f80, 
				depthkludge);
		}
		else if (z == 32.0) { /* constant z of 32, black tri */
			__glMgrRE4Z_diag(Z_HI, 0x8, 0xa3d70000, depthkludge);
			__glMgrRE4Z_diag(DZX_HI, 0, 0, depthkludge ); /* 0 */
			__glMgrRE4Z_diag(DZE_HI, 0, 0, depthkludge); /* 0 */
		}
	}
	else {
		__glMgrRE4Z_diag(Z_HI, z, 0, 0);
		__glMgrRE4Z_diag(DZX_HI, dzx, 0, 0);
		__glMgrRE4Z_diag(DZE_HI, dze, 0, 0);
	}
    }

    if (modeFlags & __GL_SHADE_TEXTURE) { /* do texture slopes */
	Coords at = min_pt->texture;
	Coords bt = mid_pt->texture;
	Coords ct = max_pt->texture;
	__uint64_t maxw = 1.0;
	__uint32_t mincoord, tmplong;
	__uint64_t scale_r, scale_s, scale_t;
	__uint64_t fs, ft;
	__uint64_t dsx, dtx;
	__uint64_t dsy, dty;
	__uint64_t dse, dte;
	__uint64_t smid, tmid;
	__uint64_t s, t;
	struct {
	    __uint64_t s, t, w;
	} atd, btd, ctd;

	atd.w = (__uint64_t)aw.w;
	btd.w = (__uint64_t)bw.w;
	ctd.w = (__uint64_t)cw.w;
	msg_printf(DBG, "v1 (s,t,w): %x %x \n", 
		*(__uint32_t*)&at.x, *(__uint32_t*)&at.y);
	msg_printf(DBG, "v2 (s,t,w): %x %x \n", 
		*(__uint32_t*)&bt.x, *(__uint32_t*)&bt.y);
	msg_printf(DBG, "v3 (s,t,w): %x %x \n", 
		*(__uint32_t*)&ct.x, *(__uint32_t*)&ct.y);

	texmode2_bits = actual;
	if (!(texmode2_bits & TEXMODE2_3DITER_EN)) {
	    if (!(texmode2_bits & TEXMODE2_WARP_ENABLE)) {
	    	/* Find maximum w inverse */
	    	maxw = ctd.w > btd.w ? ctd.w : btd.w;
	    	maxw = maxw > atd.w ? maxw : atd.w;
	    	maxw = 1.0/maxw * ((1<<18) - 1);

	    	/* Scale all w inverses according to the maximum */
	    	atd.w *= maxw;
	    	btd.w *= maxw;
	    	ctd.w *= maxw;
	    	}
	    else  /* Perspective not allowed in warp mode */ 
	    	atd.w = btd.w = ctd.w = 1.0;
	}

	else { /* Perspective not allowed in 3D mapping */ 
	    atd.w = btd.w = ctd.w = 1.0;
	    if (!(texmode2_bits & TEXMODE2_RACP)) { /* no R clamping */
	    	mincoord = (__uint32_t)floorf(at.z);
	    	if ((tmplong = (__uint32_t)floorf(bt.z)) < mincoord)
		mincoord = tmplong;
	    	if((tmplong = (__uint32_t)floorf(ct.z)) < mincoord)
		mincoord = tmplong;
	    	at.z -= mincoord;
	    	bt.z -= mincoord;
	    	ct.z -= mincoord;
		}
	    }

	/* If wrapping, find the minimum of S and T and translate */
	if (!(texmode2_bits & TEXMODE2_SACP)) {
	    mincoord = (__uint32_t)floorf(at.x);
	    if ((tmplong = (__uint32_t)floorf(bt.x)) < mincoord)
		mincoord = tmplong;
	    if ((tmplong = (__uint32_t)floorf(ct.x)) < mincoord)
		mincoord = tmplong;
	    at.x -= mincoord;
	    bt.x -= mincoord;
	    ct.x -= mincoord;
	    }

	if (!(texmode2_bits & TEXMODE2_TACP )) {
	    mincoord = (__uint32_t)floorf(at.y);
	    if ((tmplong = (__uint32_t)floorf(bt.y)) < mincoord)
		mincoord = tmplong;
	    if((tmplong = (__uint32_t)floorf(ct.y)) < mincoord)
		mincoord = tmplong;
	    at.y -= mincoord;
	    bt.y -= mincoord;
	    ct.y -= mincoord;
	    }
		
	if (!(texmode2_bits & TEXMODE2_WARP_ENABLE)) { 
	    __uint64_t dwx, dwy, dwe, wmid, w;
	    __uint64_t maxs, maxt;
	    __uint64_t swrap, twrap;

	    /* Find maximum s and t */
	    maxs = fabs(ct.x) > fabs(bt.x) ? fabs(ct.x) : fabs(bt.x);
	    maxs = maxs > fabs(at.x) ? maxs : fabs(at.x);

	    maxt = fabs(ct.y) > fabs(bt.y) ? fabs(ct.y) : fabs(bt.y);
	    maxt = maxt > fabs(at.y) ? maxt : fabs(at.y);

	    swrap = 0; while (maxs >= 1) {maxs /= 2; swrap++;}
	    twrap = 0; while (maxt >= 1) {maxt /= 2; twrap++;}

#if 0
	    READ_RSS_REG(rssnum, TXSIZE, 0, NINETEENBIT_MASK);
#endif
	    txsize_bits = actual;
	    fs = 20 - (txsize_bits & TXSIZE_US_SSIZE) -swrap; 
	    fs = 20 - (txsize_bits & TXSIZE_US_TSIZE) -twrap; 

	    if (fs > 7) fs = 7;
	    else if (!fs) fs = 0;
	    if (ft > 7) ft = 7;
	    else if (!ft) ft = 0;

#if 0
	    if(hwcx->num_sfrac >= 0) fs = hwcx->num_sfrac;
	    if(hwcx->num_tfrac >= 0) ft = hwcx->num_tfrac;
#endif
	
	    if (texmode2_bits & TEXMODE2_3DITER_EN)
	    	fs = ft = 7;
#if 0 
	    scale_s = 1 <<hwcx->rss.n.s.txsize.bits.us_ssize << fs
			>>hwcx->rss.n.s.texmode2.bits.threediter_en;
#endif
	    scale_s = 1 << (txsize_bits & TXSIZE_US_SSIZE) << fs
			>> (texmode2_bits & TEXMODE2_3DITER_EN);
	    atd.s = at.x * scale_s * atd.w;
	    btd.s = bt.x * scale_s * btd.w;
	    ctd.s = ct.x * scale_s * ctd.w;

	    scale_t = 1 << (txsize_bits & TXSIZE_US_TSIZE) < ft;
	    atd.t = at.y * scale_t * atd.w;
	    btd.t = bt.y * scale_t * btd.w;
	    ctd.t = ct.y * scale_t * ctd.w;

	    if (texmode2_bits & TEXMODE2_3DITER_EN) {
	    	scale_r = 1 << (txsize_bits & TXSIZE_US_RSIZE) << 6;
	    	atd.w = (__uint64_t)at.z * scale_r; /* For 3D, w is now */
	    	btd.w = (__uint64_t)bt.z * scale_r; /* replaced by r    */
	    	ctd.w = (__uint64_t)ct.z * scale_r;
	    	atd.s *= 0x80000; btd.s *= 0x80000; ctd.s *= 0x80000;
	    	atd.t *= 0x80000; btd.t *= 0x80000; ctd.t *= 0x80000;
	    	}

	    smid = ctd.s + perc * (atd.s - ctd.s);
	    tmid = ctd.t + perc * (atd.t - ctd.t);
	    wmid = ctd.w + perc * (atd.w - ctd.w);

	    if (span_x == 0.0)
	    	dsx = dtx = dwx = 0.0;
	    else {
	    	dsx = (btd.s - smid) / span_x;
	    	dtx = (btd.t - tmid) / span_x;
	    	dwx = (btd.w - wmid) / span_x;
		}

	    if (major_y == 0.0)
	    	dsy = dty = dwy = 0.0;
	    else {
	    	dsy = (atd.s - ctd.s - dsx * major_x) / major_y;
	    	dty = (atd.t - ctd.t - dtx * major_x) / major_y;
	    	dwy = (atd.w - ctd.w - dwx * major_x) / major_y;
		}

    		WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
    		if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
			return (-1);
    		}

	    __glMgrRE4Texture_diag(texmode2_bits, DSWY_U, dsy);
	    __glMgrRE4Texture_diag(texmode2_bits, DTWY_U, dty);
	    __glMgrRE4Texture_diag(texmode2_bits, DWIY_U, dwy);

	    s = ctd.s + dsy * deltY + dsx * deltX;
	    t = ctd.t + dty * deltY + dtx * deltX;
	    w = ctd.w + dwy * deltY + dwx * deltX;

	    __glMgrRE4Texture_diag(texmode2_bits, SW_U, s);
	    __glMgrRE4Texture_diag(texmode2_bits, TW_U, t);
	    __glMgrRE4Texture_diag(texmode2_bits, WI_U, w);


	    dse = dsy + n1 * dsx;
	    dte = dty + n1 * dtx;
	    dwe = dwy + n1 * dwx;

	    if (direction == IR_OP_AREA_RTOL) {
	    	dsx = -dsx;
	    	dtx = -dtx;
	    	dwx = -dwx;
		}
	    __glMgrRE4Texture_diag(texmode2_bits, DSWX_U, dsx);
	    __glMgrRE4Texture_diag(texmode2_bits, DTWX_U, dtx);
	    __glMgrRE4Texture_diag(texmode2_bits, DWIX_U, dwx);

	    if ((direction == IR_OP_AREA_LTOR) ^ (dxdy0 >= 0)) {
	    	dse -= dsx;
	    	dte -= dtx;
	    	dwe -= dwx;
		}
	    __glMgrRE4Texture_diag(texmode2_bits, DSWE_U, dse);
	    __glMgrRE4Texture_diag(texmode2_bits, DTWE_U, dte);
	    __glMgrRE4Texture_diag(texmode2_bits, DWIE_U, dwe);
	    }

	else { /* WARP MODE! */
	    __uint64_t dsxx, dsxy, dsyx, dsyy, dsxe, dsex, dsey, dsee;
	    __uint64_t dtxx, dtxy, dtyx, dtyy, dtxe, dtex, dtey, dtee;
	    __uint64_t mid2_dsx, mid2_dsy, mid2_dtx, mid2_dty, mid2_dse, mid2_dte;
	    struct {
	    	__uint64_t dsx, dsy, dtx, dty, dse, dte;
	    } at2d, bt2d, ct2d;

	    fs = 18 - (txsize_bits & TXSIZE_US_SSIZE);
	    if(fs > 7) fs = 7;
	    if(fs < 5) fs = 5;
	    scale_s = 1 << (txsize_bits & TXSIZE_US_SSIZE) << (fs-5);
	
	    atd.s = at.x * scale_s;
	    btd.s = bt.x * scale_s;
	    ctd.s = ct.x * scale_s;

	    ft = 18 - (txsize_bits & TXSIZE_US_TSIZE);
	    if(ft > 7) ft = 7;
	    if(ft < 5) ft = 5;
	    scale_t = 1 << (txsize_bits & TXSIZE_US_TSIZE) << (ft-5);

	    atd.t = at.y * scale_t;
	    btd.t = bt.y * scale_t;
	    ctd.t = ct.y * scale_t;

	    smid = ctd.s + perc * (atd.s - ctd.s);
	    tmid = ctd.t + perc * (atd.t - ctd.t);

	    if(direction == IR_OP_AREA_RTOL) {
		span_x = -span_x;
		major_x = -major_x;
		deltX = -deltX;
	    	n1 = -n1;
		}

	    /* Slopes at the top vertex (ct) */
	    if(span_x == 0.0)
	    	ct2d.dsx = ct2d.dtx = 0.0;
	    else {
	    	ct2d.dsx = (btd.s - smid) / span_x;
	    	ct2d.dtx = (btd.t - tmid) / span_x;
		}

	    if(major_y == 0.0)
	    	ct2d.dsy = ct2d.dty = 0.0;
	    else {
	    	ct2d.dsy = (atd.s - ct2d.dsx * major_x - ctd.s) / major_y;
	    	ct2d.dty = (atd.t - ct2d.dtx * major_x - ctd.t) / major_y;
		}

	    if(at.x == 0.5) {
	    	at2d.dsx = ct2d.dsx * (1 - warpfactor);
	    	at2d.dsy = ct2d.dsy * (1 - warpfactor);
		}
	    else {
	    	at2d.dsx = ct2d.dsx * (1 + warpfactor);
	    	at2d.dsy = ct2d.dsy * (1 + warpfactor);
		}
	    if(at.y == 0.5) {
	    	at2d.dtx = ct2d.dtx * (1 - warpfactor);
	    	at2d.dty = ct2d.dty * (1 - warpfactor);
		}
	    else {
	    	at2d.dtx = ct2d.dtx * (1 + warpfactor);
	    	at2d.dty = ct2d.dty * (1 + warpfactor);
		}
	    if(bt.x == 0.5) {
	    	bt2d.dsx = ct2d.dsx * (1 - warpfactor);
	    	bt2d.dsy = ct2d.dsy * (1 - warpfactor);
		}
	    else {
	    	bt2d.dsx = ct2d.dsx * (1 + warpfactor);
	    	bt2d.dsy = ct2d.dsy * (1 + warpfactor);
		}
	    if(bt.y == 0.5) {
	    	bt2d.dtx = ct2d.dtx * (1 - warpfactor);
	    	bt2d.dty = ct2d.dty * (1 - warpfactor);
		}
	    else {
	    	bt2d.dtx = ct2d.dtx * (1 + warpfactor);
	    	bt2d.dty = ct2d.dty * (1 + warpfactor);
		}
	    if(ct.x == 0.5) {
	    	dsx = ct2d.dsx * (1 - warpfactor);
	    	dsy = ct2d.dsy * (1 - warpfactor);
		}
	    else {
	    	dsx = ct2d.dsx * (1 + warpfactor);
	    	dsy = ct2d.dsy * (1 + warpfactor);
		}
	    if(ct.y == 0.5) {
	    	dtx = ct2d.dtx * (1 - warpfactor);
	    	dty = ct2d.dty * (1 - warpfactor);
		}
	    else {
	    	dtx = ct2d.dtx * (1 + warpfactor);
	    	dty = ct2d.dty * (1 + warpfactor);
		}

	    s = ctd.s + dsy * deltY + dsx * deltX;
	    t = ctd.t + dty * deltY + dtx * deltX;

	    dse = dsy + n1 * dsx;
	    dte = dty + n1 * dtx;
	    bt2d.dse = bt2d.dsy + n1 * bt2d.dsx;
	    bt2d.dte = bt2d.dty + n1 * bt2d.dtx;
	    at2d.dse = at2d.dsy + n1 * at2d.dsx;
	    at2d.dte = at2d.dty + n1 * at2d.dtx;

	    mid2_dsx = dsx + perc * (at2d.dsx - dsx);
	    mid2_dsy = dsy + perc * (at2d.dsy - dsy);
	    mid2_dtx = dtx + perc * (at2d.dtx - dtx);
	    mid2_dty = dty + perc * (at2d.dty - dty);
	    mid2_dse = dse + perc * (at2d.dse - dse);
	    mid2_dte = dte + perc * (at2d.dte - dte);

	    /* 2nd order slopes at the top vertex (ct) */
	    if(span_x == 0.0)
	    	dsxx = dsyx = dtxx = dtyx = dsex = dtex = 0.0;
	    else {
		dsxx = (bt2d.dsx - mid2_dsx) / span_x;
		dsyx = (bt2d.dsy - mid2_dsy) / span_x;
		dtxx = (bt2d.dtx - mid2_dtx) / span_x;
		dtyx = (bt2d.dty - mid2_dty) / span_x;
		dsex = (bt2d.dse - mid2_dse) / span_x;
		dtex = (bt2d.dte - mid2_dte) / span_x;
		}

	    if(major_y == 0.0)
	    	dsxy = dsyy = dtxy = dtyy = dsey = dtey = 0.0;
	    else {
	    	dsxy = (at2d.dsx - dsx - dsxx * major_x) / major_y;
	    	dsyy = (at2d.dsy - dsy - dsyx * major_x) / major_y;
	    	dtxy = (at2d.dtx - dtx - dtxx * major_x) / major_y;
	    	dtyy = (at2d.dty - dty - dtyx * major_x) / major_y;
	    	dsey = (at2d.dse - dse - dsex * major_x) / major_y;
	    	dtey = (at2d.dte - dte - dtex * major_x) / major_y;
		}

	    dsx += dsxy * deltY + dsxx * deltX;
	    dsy += dsyy * deltY + dsyx * deltX;
	    dtx += dtxy * deltY + dtxx * deltX;
	    dty += dtyy * deltY + dtyx * deltX;
	    dse += dsey * deltY + dsex * deltX;
	    dte += dtey * deltY + dtex * deltX;

	    dsxe = dsxy + n1 * dsxx;
	    dsee = dsey + n1 * dsex;
	    dtxe = dtxy + n1 * dtxx;
	    dtee = dtey + n1 * dtex;

	    __glMgrRE4Texture_diag(texmode2_bits, SWARP_U, s);
	    __glMgrRE4Texture_diag(texmode2_bits, TWARP_U, t);

	    __glMgrRE4Texture_diag(texmode2_bits, DSX_U, dsx);
	    __glMgrRE4Texture_diag(texmode2_bits, DTX_U, dtx);
	    __glMgrRE4Texture_diag(texmode2_bits, DSXX_U, dsxx);
	    __glMgrRE4Texture_diag(texmode2_bits, DTXX_U, dtxx);
	    __glMgrRE4Texture_diag(texmode2_bits, DTEX_U, dtex);

	    if((direction == IR_OP_AREA_LTOR) ^ (dxdy0 >= 0)) {
	    	dse -= dsx;
	    	dte -= dtx;
		dsee -= dsex;
		dtee -= dtex;
		dsxe -= dsxx;
		dtxe -= dtxx;
		}

	    __glMgrRE4Texture_diag(texmode2_bits, DSEX_U, dsex);
	    __glMgrRE4Texture_diag(texmode2_bits, DSE_U, dse);
	    __glMgrRE4Texture_diag(texmode2_bits, DTE_U, dte);
	    __glMgrRE4Texture_diag(texmode2_bits, DSXE_U, dsxe);
	    __glMgrRE4Texture_diag(texmode2_bits, DTXE_U, dtxe);
	    __glMgrRE4Texture_diag(texmode2_bits, DSEE_U, dsee);
	    __glMgrRE4Texture_diag(texmode2_bits, DTEE_U, dtee);
	    }
    txscale.bits.fs = fs;
    txscale.bits.ft = ft;
  
#if 0
    cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + TXSCALE), 4);
    HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0, HQ_MOD, 0);
    HQ3_PIO_WR(CFIFO1_ADDR, txscale.val, ~0x0, HQ_MOD, 0);
#endif
    WRITE_RSS_REG(4, TXSCALE, txscale.val, 0xffffffff);

    }

    ir.bits.Opcode = direction;
    ir.bits.Setup = 0x0;

    if (modeFlags & __GL_SHADE_TEXTURE) { /* do texture */
    	HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(IR),ir.val, 0xffffffff);
    }
    else {
    	HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(IR_ALIAS),ir.val, 0xffffffff);
    }

    return 0;
}
