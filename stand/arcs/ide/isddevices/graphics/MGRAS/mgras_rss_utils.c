/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sgidefs.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include <sys/mgrashw.h>
#include <sys/mgras.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#include "mgras_tex_1d_in.h"
#include "ide_msg.h"

__uint32_t dith_off = 0; /* on by default */
__uint32_t is_old_pp = 0;

#define DO_DEBUG 	0

__uint32_t exec = 1;
__uint32_t clean_disp;
__uint32_t dac_pat = 0x99999999;
__uint32_t num_tram_comps;

void            MgrasInitCCR(mgras_hw *base_in, mgras_info *info);
void MgrasSyncPPRDRAM_repair(register mgras_hw *, int, int, int);
void MgrasSyncPPRDRAMs_repair(register mgras_hw *, mgras_info *);
extern __uint32_t _mgras_disp_ctrl(int, int, int, int, int, int);
extern __uint32_t _mgras_crc_test(__uint32_t, __uint32_t, __uint32_t);

void
__glMgrRE4PP1_diag(__uint32_t reg, __uint32_t val)
{
	__uint32_t cmd;
	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + reg), 4);    
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);                
        HQ3_PIO_WR(CFIFO1_ADDR, val, ~0x0); 
}

void
__glMgrRE4Int_diag(__uint32_t reg, __uint32_t val)
{
	WRITE_RSS_REG(4, reg, val, ~0x0);
}

void
__glMgrRE4IR_diag(__uint32_t val)
{
#if 0
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1c00 + IR_ALIAS), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, val, ~0x0);
#endif
	HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(IR_ALIAS), val, 0xffffffff);
}

void
__glMgrRE4TEIR_diag(__uint32_t val)
{
#if 0
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1c00 + IR), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, val, ~0x0);
#endif
	HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(IR), val, 0xffffffff);
}

void
__glMgrRE4LineSlope_diag(__uint32_t reg, float val0, float val1)
{
	__uint32_t cmd;
    __int32_t num0 = (__int32_t)(val0 * 0x1000000) & 0x3ffffff;
    __int32_t num1 = (__int32_t)(val1 * 0x1000000) & 0x3ffffff;

        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + reg), 4);     
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);                  
        HQ3_PIO_WR(CFIFO1_ADDR, num0, ~0x0);
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + (reg+1)), 4);     
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);                  
        HQ3_PIO_WR(CFIFO1_ADDR, num1, ~0x0);
}

void
__glMgrRE4LineAdjust_diag(__uint32_t reg, float val)
{
	__uint32_t cmd;
    __int32_t num = (__int32_t)(val * 0x1000000) & 0x1ffffff;

        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + reg), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, num, ~0x0);
}

void __glMgrRE4Position2_diag(__uint32_t reg, float val0, float val1)
{
	__uint32_t cmd;
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + reg), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, *(__uint32_t *)&val0, ~0x0);

        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + (reg+1)), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, *(__uint32_t *)&val1, ~0x0);

#if DO_DEBUG
	fprintf(stderr, "val0: 0x%x, val1: 0x%x\n", *(__uint32_t *)&val0,
		*(__uint32_t *)&val1);
#endif
}

void
__glMgrRE4ColorSgl_diag(__uint32_t reg, float val)
{
	__uint32_t cmd;
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + reg), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, (__int32_t )(val * 0xfff000), ~0x0);
#if DO_DEBUG
	fprintf(stderr, "val: %f, regval: 0x%x\n", val, (__int32_t )(val * 0xfff000));
#endif
}

float
fabs_ide(float num)
{
	if (num < 0.0)
		return (num * (-1.0));
	else
		return (num);
}

/* returns (log(num)/log 2) -- which power of 2 gives you "num" */
__uint32_t
ide_log(__uint32_t num)
{
	__uint32_t i, power;

	if (num == 1)
		return (0);

	power = 1;
	for (i = 1; i < 256; i++) {
		power *= 2;	
		if (power == num)
			return (i);
	}

	return(0);
}

/***************************************************************************
 * mg0_clear_color -r[red] -g[green] -b[blue] -a[alpha] 		   *
 *	-t [optional xstart] [optional ystart] 				   *
 *	-d [optional xend] [optional yend]				   *
 *	-s [optional rss num]						   *
 *	-f [optional buffer, default = 1 (color buf), 0x41 = overlay]	   *
 *									   *
 * Color params (red, green, blue, alpha) specified are on a 0-100 integer *
 * scale and will be scaled  						   *
 *	to be floats ranging from 0-1.0  We do the scaling internally to   *
 * the function since ide does not have atof support and it's easier to    *
 * cast than to add atof support.					   *
 *									   *
 ***************************************************************************/

__uint32_t
mg0_clear_color(int argc, char ** argv)
{

	__uint32_t buf, bad_arg = 0, xstart, ystart, xend, yend;
	__uint32_t red, green, blue, alpha, rssnum, color_max;
	float red_float, green_float, blue_float, alpha_float;

	/* default */
	fpusetup();
	red = green = blue = alpha = 0;
	buf = PP_FILLMODE_DBUFA;
	xstart = ystart = 0;
	xend = 1344; yend = 1024;
	rssnum = 4; /* all rss's */
	color_max = 100;

#ifdef MGRAS_DIAG_SIM
	xend = 100; yend = 100;
	xstart = 50; ystart = 50;
	red = 55; green = 12; blue = 32; alpha = 27;
#endif

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&red);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&red);
                                break;
                        case 'g':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&green);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&green);
                                break;
                        case 'b':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&blue);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&blue);
                                break;
                        case 'a':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&alpha);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&alpha);
                                break;
                        case 'c':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&color_max);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&color_max);
                                break;
                        case 'i':
                                dith_off = 1;
                                break;
                        case 'f':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&buf);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&buf);
                                break;
                        case 's':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rssnum);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rssnum);
                                break;
                        case 'x':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&exec);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&exec);
                                break;
			case 't':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&xstart);
                                        argc--; argv++;
					atob(&argv[1][0], (int*)&ystart);
                                        argc--; argv++;
				}
				else {
				   msg_printf(SUM, "Need a space after -%c\n",
					argv[0][1]);
				   bad_arg++;
				};
				break;
			case 'd':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&xend);
                                        argc--; argv++;
					atob(&argv[1][0], (int*)&yend);
                                        argc--; argv++;
				}
				else {
				   msg_printf(SUM, "Need a space after -%c\n",
					argv[0][1]);
				   bad_arg++;
				};
				break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
        }

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, 
		  "Usage -r[red] -g[green] -b[blue] -a[alpha] -f[optional buffer # (default=1 color buf, 0x41 = overlay)] -s[optional rss num] -t [optional xstart] [optional ystart] -d [optional xend] [optional yend] -c [optional max color, default = 100]\n");
		msg_printf(SUM,"The default is to clear the whole screen with black.\n");
		msg_printf(SUM, "red, green, blue, and alpha are on a 0-100 scale\n");
		return (0);
	}
#if DO_DEBUG
	red = 55; green = 12; blue = 32; alpha = 27;
	fprintf(stdout, "buf: %d, red: %d, green: %d, blue: %d, alpha: %d\n",
		buf, red, green, blue, alpha);
#endif

	/* increment x,y since _mg0_block uses offsets */
	/* Increment start coords if not 0,0 */
#if 0
	if (xstart)
		xstart++;
	if (ystart)
		ystart++;	
	xend++; yend++;
#endif

	msg_printf(DBG, "buf: %d, red: %d, green: %d, blue: %d, alpha: %d\n",
		buf, red, green, blue, alpha);

	red_float = (float)(red * 1.0)/color_max;	
	green_float = (float)(green * 1.0)/color_max;	
	blue_float = (float)(blue * 1.0)/color_max;	
	alpha_float = (float)(alpha * 1.0)/color_max;	

#if DO_DEBUG
	fprintf(stdout, "buf: %d, red: %f, green: %f, blue: %f, alpha: %f\n",
		buf, red_float, green_float, blue_float, alpha_float);
#endif
	msg_printf(DBG, "buf: %d, red: %f, green: %f, blue: %f, alpha: %f\n",
		buf, red_float, green_float, blue_float, alpha_float);

	
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum,EIGHTEENBIT_MASK);
	_mgras_rss_init(rssnum);
	_draw_setup(0, 0);
	_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
		xstart, ystart, xend, yend);
	return(0);
}	

/***************************************************************************
 * mg0_bars -h -w[width]						   *
 *									   *
 * Draw black and white alternating bars. The default is to draw vertical  *
 * lines of 1-pixel wide bars.	 					   *
 *									   *
 * -h = draw horizontal lines						   *
 * -w[width] = draw width-pixel wide bars				   *
 *									   *
 ***************************************************************************/

__uint32_t
mg0_bars(int argc, char ** argv)
{
	__uint32_t x, y, buf, bad_arg = 0, xstart, ystart, xend, yend;
	__uint32_t rssnum, horz = 0, width = 1;
	float red_float, green_float, blue_float, alpha_float;

	/* default */
	fpusetup();
	buf = PP_FILLMODE_DBUFA;
	xstart = ystart = 0;
	xend = 1344; yend = 1024;
	rssnum = 4; /* all rss's */

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'h':
				horz = 1;
                                break;
                        case 'w':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&width);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&width);
                                break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
        }

	if ((bad_arg) || (argc)) {
#if 0
		msg_printf(SUM, "Usage (optional)-h -w[optional width]\n");
#endif
		msg_printf(SUM, "Usage (optional)-h\n");
		return (0);
	}

#ifdef MGRAS_DIAG_SIM
	xend = 50; yend = 50;
	xstart = 0; ystart = 0;
#endif

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum,EIGHTEENBIT_MASK);
	_mgras_rss_init(rssnum);
	_draw_setup(0,0);
	if (horz) {
	   for (y = 0; y < yend; y+=width) {
		red_float = green_float = blue_float = alpha_float = (y % 2);
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xstart, y, xend, y+width-1);
	   }
	}
	else { /* vertical bars */
	   for (x = 0; x < xend; x+=width) {
		red_float = green_float = blue_float = alpha_float = (x % 2);
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			x, ystart, x+width-1, yend);
	    }
	}

	return(0);
}

/**************************************************************************
 * Clears the screen colors using only the rss. 			  *
 *									  *
 * 	Does this equivalent:						  *
 *		glClearColor(red, green, blue, alpha)			  *
 *		glClear(buffer)						  *
 *									  *
 *	Assumptions:							  *
 *		rgba 8888 - ppfillmode.bits.PixType = 0x2;		  *
 *		dither is on - ppfillmode.bits.DitherEnab = 1		  *
 *                                                                        *
 * 	Based on Clear() from MG0/mg0_rgb.c				  *
 **************************************************************************/
int
_mg0_block(
	__uint32_t buffer, 
	float red_f,
	float green_f,
	float blue_f,
	float alpha_f,
	__uint32_t xstart, __uint32_t ystart,
	__uint32_t xend, __uint32_t yend)
{
	__uint32_t dodither = 1, tmp, cfifo_status;
	__uint32_t red, red_scaled, red_inc, red_frac;
	__uint32_t green, green_scaled, green_inc, green_frac;
	__uint32_t blue, blue_scaled, blue_inc, blue_frac;
	__uint32_t alpha, alpha_scaled, alpha_inc, alpha_frac;
	fillmodeu    fillmode = {0};
	ppfillmodeu	ppfillmode = {0};
	iru		ir = {0};
	pptagdata_ru	pptagdata_r = {0};
	pptagdata_gu	pptagdata_g = {0};
	pptagdata_bu	pptagdata_b = {0};
	pptagdata_au	pptagdata_a = {0};

	if ((clean_disp) || (dith_off))
		dodither = 0;

#if DO_DEBUG
	fprintf(stdout, "red_f: %f, green_f: %f, blue_f: %f, alpha_f: %f\n",
		red_f, green_f, blue_f, alpha_f);
	fprintf(stdout, "buffer val: %d\n", buffer);
#endif

	if (red_f > 1.0)
		red_f = 1.0; /* clamp */
	if (green_f > 1.0)
		green_f = 1.0; /* clamp */
	if (blue_f > 1.0)
		blue_f = 1.0; /* clamp */
	if (alpha_f > 1.0)
		alpha_f = 1.0; /* clamp */

	red = (__uint32_t) (red_f * PIX_COLOR_SCALE);
	green = (__uint32_t) (green_f * PIX_COLOR_SCALE);
	blue = (__uint32_t) (blue_f * PIX_COLOR_SCALE);
	alpha = (__uint32_t) (alpha_f * PIX_COLOR_SCALE);
	
#if DO_DEBUG
	fprintf(stdout, "red: 0x%x, green: 0x%x, blue: 0x%x, alpha: 0x%x\n",
		red, green, blue, alpha);
#endif

	/* do adjustment for dac pattern 0,3,4,5 since we pass float values, but
	 * we really want exact hex values */
	switch (dac_pat) {
		case 0:
		   red = green = blue = (xstart)*16; /* on PIX_COLOR_SCALE */
		   msg_printf(DBG, "xstart: %d, red: %d\n", xstart, red);
		   break;
		case 3:
		   red = (xstart)*16; /* on PIX_COLOR_SCALE */
		   green = blue = 0;
		   break;
		case 4:
		   green = (xstart)*16; /* on PIX_COLOR_SCALE */
		   red = blue = 0;
		   break;
		case 5:
		   blue = (xstart)*16; /* on PIX_COLOR_SCALE */
		   red = green = 0;
		   break;
		default: break;
	}

	tmp = red - (red >> 8);
	red_scaled = tmp >> 4;
	red_frac = tmp & 0xf;
	red_inc = red_scaled + 1;
	if (dodither)
		red = (red_inc << 20) | (red_frac << 12) | (red_scaled << 4);
	pptagdata_r.bits.RedNoIncr = red_scaled;
	pptagdata_r.bits.RedIncr = red_inc;
	pptagdata_r.bits.RedFrac = red_frac;
	
	tmp = green - (green >> 8);
	green_scaled = tmp >> 4;
	green_frac = tmp & 0xf;
	green_inc = green_scaled + 1;
	pptagdata_g.bits.GreenNoIncr = green_scaled;
	pptagdata_g.bits.GreenIncr = green_inc;
	pptagdata_g.bits.GreenFrac = green_frac;
	if (dodither)
		green = (green_inc << 12) | (green_scaled << 4) | green_frac;
	
	tmp = blue - (blue >> 8);
	blue_scaled = tmp >> 4;
	blue_frac = tmp & 0xf;
	blue_inc = blue_scaled + 1;
	pptagdata_b.bits.BlueNoIncr = blue_scaled;
	pptagdata_b.bits.BlueIncr = blue_inc;
	pptagdata_b.bits.BlueFrac = blue_frac;
	if (dodither)
		blue = (blue_inc << 12) | (blue_scaled << 4) | blue_frac;
	
	tmp = alpha - (alpha >> 8);
	alpha_scaled = tmp >> 4;
	alpha_frac = tmp & 0xf;
	alpha_inc = alpha_scaled + 1;
	pptagdata_a.bits.AlphaNoIncr = alpha_scaled;
	pptagdata_a.bits.AlphaIncr = alpha_inc;
	pptagdata_a.bits.AlphaFrac = alpha_frac;
	if (dodither)
		alpha = (alpha << 20) | (alpha_inc << 12) | (alpha_scaled << 4) 
			| alpha_frac;
	
	fillmode.bits.FastFill = 1;
	fillmode.bits.RoundEN = 0;
	fillmode.bits.ZiterEN = 0;
	fillmode.bits.ShadeEN = 0;

	if (buffer == PP_FILLMODE_DBUFA) {
		ppfillmode.bits.PixType = PP_FILLMODE_RGBA8888;
	}
        else if (buffer == PP_FILLMODE_DOVERA_OR_B) {
		ppfillmode.bits.PixType = PP_FILLMODE_CI8;
	}
        ppfillmode.bits.DrawBuffer = buffer;

	if ((clean_disp) || (dith_off)) {
		ppfillmode.bits.DitherEnab = 0;
		ppfillmode.bits.DitherEnabA = 0;
		if (clean_disp == 1) {
			ppfillmode.bits.LogicOP = PP_FILLMODE_LO_OR;
			ppfillmode.bits.LogicOpEnab = 1;
		}
	}
	else {
		ppfillmode.bits.DitherEnab = 1;
		ppfillmode.bits.DitherEnabA = 1;
		ppfillmode.bits.ZEnab = 0;
	}

	ir.bits.Opcode = IR_OP_BLOCK; /* block */
	ir.bits.Setup = 1;

	WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    	if (cfifo_status == CFIFO_TIME_OUT) {
        	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
        	return (-1);
    	}

#if 0
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_PIXCOLORR & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, red, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_PIXCOLORG & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, green, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_PIXCOLORB & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, blue, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_PIXCOLORA & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, alpha, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_TAGDATA_R & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, pptagdata_r.val, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_TAGDATA_G & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, pptagdata_g.val, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_TAGDATA_B & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, pptagdata_b.val, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_TAGDATA_A & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, pptagdata_a.val, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_FILLMODE & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, ppfillmode.val, ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + BLOCK_XYSTARTI),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, ((xstart << 16) | ystart), ~0x0);

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + BLOCK_XYENDI),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, ((xend << 16) | yend), ~0x0);

	/* now do it! */
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1c00 + IR_ALIAS),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, ir.val, ~0x0);

#endif
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_PIXCOLORR & 0x1ff)), red, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_PIXCOLORG& 0x1ff)), green, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_PIXCOLORB& 0x1ff)), blue, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_PIXCOLORA& 0x1ff)), alpha, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_TAGDATA_R& 0x1ff)), pptagdata_r.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_TAGDATA_G& 0x1ff)), pptagdata_g.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_TAGDATA_B& 0x1ff)), pptagdata_b.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_TAGDATA_A& 0x1ff)), pptagdata_a.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(FILLMODE), fillmode.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)), ppfillmode.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(BLOCK_XYSTARTI),((xstart << 16) | ystart), 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(BLOCK_XYENDI),((xend << 16) | yend), 0xffffffff);

	if (exec) {
	HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(IR_ALIAS),ir.val, 0xffffffff);

	/* Do some clean up */
	/* reset fillmode to turn off fast fill since not everyone wants it */
	fillmode.bits.FastFill = 0;
	fillmode.bits.RoundEN = 1;
	fillmode.bits.ZiterEN = 0;
	fillmode.bits.ShadeEN = 1;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(FILLMODE),fillmode.val, 0xffffffff);
#if 0
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

#endif
	/* turn z enable on */
	ppfillmode.bits.DitherEnab = 1;
	ppfillmode.bits.DitherEnabA = 1;
	ppfillmode.bits.ZEnab = 0;
	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
	ppfillmode.bits.DrawBuffer = buffer; /* buffer A */
	ppfillmode.bits.LogicOpEnab = 0;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)),ppfillmode.val, 0xffffffff);
#if 0
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_FILLMODE & 0x1ff)),4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, ppfillmode.val, ~0x0);
#endif
	}

	return 0;
}

/**************************************************************************
 * Clears the zbuffer using only the rss. 				  *
 *									  *
 * 	Does this equivalent:						  *
 *		glClearColor(red, green, blue, alpha)			  *
 *		glClearDepth(1)	 1 is max				  *
 *									  *
 *	Assumptions:							  *
 *                                                                        *
 * 	Based on Clear() from MG0/mg0_depth.c				  *
 **************************************************************************/
void
_mg0_clear_depth(__uint32_t clear_z_val)
{
	__uint32_t Stencilmask, xend, yend;
	fillmodeu	fillmode = {0};
	ppfillmodeu	ppfillmode = {0};
	iru		ir = {0};

	/* eventually get these vals from a video routine */
#ifdef NOTYET
	xend = 1344;
	yend = 1024; 
#endif /* NOTYET */
	xend = XMAX;
	yend = YMAX; 

#ifdef MGRAS_DIAG_SIM
	/* for now just clear a 100x100 area */
	xend = 100;
	yend = 100;
#endif
	/* ?? */
	Stencilmask = clear_z_val;

	fillmode.bits.FastFill = 0x1;
	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DZBUF;

	ir.bits.Setup = 0x1;
	ir.bits.Opcode = IR_OP_BLOCK;

#if 1
    __glMgrRE4PP1_diag((PP_STENCILMASK & 0x1ff), Stencilmask & 0xff);

    __glMgrRE4PP1_diag((PP_PIXCOLORR & 0x1ff), clear_z_val & 0xfff);
    __glMgrRE4PP1_diag((PP_PIXCOLORG & 0x1ff), clear_z_val >> 12);

#endif
    __glMgrRE4PP1_diag((PP_TAGDATA_Z & 0x1ff), clear_z_val);

    __glMgrRE4Int_diag(FILLMODE, fillmode.val);
    __glMgrRE4PP1_diag((PP_FILLMODE & 0x1ff), ppfillmode.val);

    __glMgrRE4Int_diag(BLOCK_XYSTARTI, 0); /* start at 0,0 */
    __glMgrRE4Int_diag(BLOCK_XYENDI, ((xend << 16) | yend));
    __glMgrRE4IR_diag(ir.val);

	/* clean up */
	/* reset fillmode to turn off fast fill since not everyone wants it */
	fillmode.bits.FastFill = 0;
	fillmode.bits.RoundEN = 1;
	fillmode.bits.ZiterEN = 0;
	fillmode.bits.ShadeEN = 1;

	/* turn z enable on */
	ppfillmode.bits.DitherEnab = 1;
	ppfillmode.bits.DitherEnabA = 1;
	ppfillmode.bits.ZEnab = 0;
	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DBUFA; /* buffer A */

    __glMgrRE4Int_diag(FILLMODE, fillmode.val);
    __glMgrRE4PP1_diag((PP_FILLMODE & 0x1ff), ppfillmode.val);

#if 1
    __glMgrRE4PP1_diag((PP_STENCILMASK & 0x1ff), Stencilmask);
#endif

}

void
__glMgrRE4Texture_diag (__uint32_t texmode2_bits, __uint32_t reg, double val)
{                              
	__int64_t longnum;

    if (!(texmode2_bits & TEXMODE2_WARP_ENABLE))                        
    {
        switch (reg) {                                                  
            case DWIE_U:                                                
            case DWIX_U:                                                
                longnum = (__int64_t)(val * 0x1000000); 
                break;                                                  
            default:                                                    
                longnum = (__int64_t)(val * 0x1000); 
                break;                                                  
    	}                                                           
    }
    else /* Warp mode! */                                               
    {
        switch(reg) {                                                   
            case SWARP_U:                                               
            case TWARP_U:                                               
                longnum = (__int64_t)(val * 0x20000);                  
                break;                                                  
            case DSE_U:                                                 
            case DTE_U:                                                 
            case DSEX_U:                                                
            case DTEX_U:                                                
                longnum = (__int64_t)(val * 0x20000000);               
                break;                                                  
            default:                                                    
                longnum = (__int64_t)(val * ((__int64_t)1 << 37));    
                break;                                                  
            };                                                          
    }

	msg_printf(DBG, "dbl xt: reg=0x%x, valhi: 0x%x, vallo: 0x%x\n",
	reg, (__int32_t)(longnum >> 0x20), (__uint32_t)(longnum & 0xffffffff));

#if HQ4
	/* AND the reg with 0x1ff in case we're passing in a double address
	   already - WRITE_RSS_REG_DBL wants a single reg addr */
	WRITE_RSS_REG_DBL(CONFIG_RE4_ALL, (reg & 0x1ff), (__int32_t)(longnum >> 0x20), (__uint32_t)(longnum & 0xffffffff), ~0x0);

#else
	HQ3_PIO_WR64_RE_XT((RSS_BASE + HQ_RSS_SPACE(reg)), (__int32_t)(longnum >> 0x20), (__uint32_t)(longnum & 0xffffffff), ~0x0);
#endif
	
}

void
__glMgrRE4Z_diag(__uint32_t reg, __uint32_t val0, __uint32_t val1, __uint32_t kludge)
{
	__int64_t longnum;

	if (kludge == 0) {
    		longnum = (__int64_t)(val0 * 0x1000);
#if HQ4
		/* AND the reg with 0x1ff in case we're passing in a double 
		address already - WRITE_RSS_REG_DBL wants a single reg addr */

		WRITE_RSS_REG_DBL(CONFIG_RE4_ALL, (reg & 0x1ff), (longnum >> 0x20), (longnum & 0xffffffff), ~0x0);

#else
		HQ3_PIO_WR64_RE_XT((RSS_BASE + HQ_RSS_SPACE(reg)), (longnum >> 0x20), (longnum & 0xffffffff), ~0x0);
#endif
	}
	else {	/* kludge is true */
#if HQ4
		/* AND the reg with 0x1ff in case we're passing in a double 
		address already - WRITE_RSS_REG_DBL wants a single reg addr */

		WRITE_RSS_REG_DBL(CONFIG_RE4_ALL, (reg & 0x1ff), val0, val1, ~0x0);

#else
		HQ3_PIO_WR64_RE_XT((RSS_BASE + HQ_RSS_SPACE(reg)), val0, val1, ~0x0);
#endif
	}
}


void
_mgras_re4_ir_write(
	__uint32_t Opcode,
	__uint32_t Setup, 
	__uint32_t EPF_First, 
	__uint32_t EPF_Last)
{
	iru          ir = {0};
	__uint32_t cmd;

	ir.bits.Opcode = Opcode;
	ir.bits.Setup = Setup;
	ir.bits.EPF_First = EPF_First;
	ir.bits.EPF_Last = EPF_Last;

	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1c00 + IR_ALIAS), 4);
    	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    	HQ3_PIO_WR(CFIFO1_ADDR, ir.val, ~0x0);

}

/* convert 32-bit 8888 from indirect data to rgba data for frame buffer
 * readbacks
 */
#define indirect_to_32_rgba(numlo, numhi, rgba) {                       \
   __uint32_t r, g, b, a;                                               \
   r = ((((numlo&0x2)>>1)<<7) | (((numlo&0x10)>>4)<<6) |           	\
        (((numlo&0x80)>>7)<<5) | (((numlo&0x400)>>10)<<4) |       	\
        (((numlo&0x2000)>>13)<<3) | (((numlo&0x10000)>>16)<<2)|		\
        (((numhi&0x2)>>1)<<1) | ((numhi&0x10)>>4));                	\
   g = (((numlo&0x1)<<7) | (((numlo&0x8)>>3)<<6) |                    	\
        (((numlo&0x40)>>6)<<5) | (((numlo&0x200)>>9)<<4) |       	\
        (((numlo&0x1000)>>12)<<3) | (((numlo&0x8000)>>15)<<2)|  	\
        ((numhi&0x1)<<1) | ((numhi&0x8)>>3));                         	\
   b = ((((numlo&0x4)>>2)<<7) | (((numlo&0x20)>>5)<<6) |           	\
        (((numlo&0x100)>>8)<<5) | (((numlo&0x800)>>11)<<4) |     	\
        (((numlo&0x4000)>>14)<<3) | (((numlo&0x20000)>>21)<<2)|		\
        (((numhi&0x4)>>2)<<1) | ((numhi&0x20)>>5));                	\
   a = ((((numhi&0x40)>>6)<<7) | (((numhi&0x80)>>7)<<6) |         	\
        (((numhi&0x100)>>8)<<5) | (((numhi&0x200)>>9)<<4) |     	\
        (((numhi&0x400)>>10)<<3) | (((numhi&0x800)>>11)<<2) |     	\
        (((numhi&0x1000)>>12)<<1) | ((numhi&0x2000)>>13));      	\
   rgba = ((r << 24) | (g << 16) | (b << 8) | a);                       \
};

/* read back from the frame buffer */
/* Pretty much the same as _mgras_read_indirect() but with a flexible 
 * read size instead of hardcoded to 192.
 */

__uint32_t
_read_indirect_general(
        __uint32_t xstart,
	__uint32_t ystart,
        __uint32_t *expected_data,
	__uint32_t tile_size)
{
        __uint32_t subtile, exp, mask;
	__uint32_t ind36_1_lo, ind36_1_hi, ind36_2_lo, ind36_2_hi;
        __uint32_t ppnum, rdram, addr, page, actual, errors = 0;
#if DO_DEBUG
	FILE *fp;

	fp = fopen("data.out", "a+");
#endif

	PIXEL_TILE_SIZE = 192; 

        /*
         * Each rdram holds 32 pixels per scan line, 2 pixels per repeating
         * 12-pixel subtile. In a "tile_size" tile, this is repeated 
	 * (tile_size)/12 times.
         * Since each pixel requires 2 reads, we get 4 reads per 12-pixel
         * chunk per rdram for a total of (4 * (tile_size/12)) reads per rdram.
         */

        /* reset drb pointers for the read - reg: RSS_DRBPTRS_PP1 */
        HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(0x16d))), 0,
                EIGHTEENBIT_MASK);

        mask = EIGHTEENBIT_MASK;

	/* 1 page holds the pixels from a 192wide x16high area */
        page = (xstart / PIXEL_TILE_SIZE) + (7 * (ystart / 16));
        /* 7 is from 1344/192 */

        /* addr = 0 since we always start reading at the start of a page */
        for (ppnum = 0; ppnum < PP_PER_RSS; ppnum++)
           for (rdram = 0; rdram < RDRAM_PER_PP; rdram++)
                for (subtile = 0; subtile < tile_size/PIXEL_SUBTILE_SIZE;
                         subtile++)
                {
		   ind36_1_lo = ind36_1_hi = ind36_2_lo = ind36_2_hi = 0;

                   /* read the 2 pixels in each subtile */
                   for (addr = 0; addr <= 6; addr+=2)
                   {
                        exp = *(expected_data + (addr/2)+(4*rdram));
                        if (((addr % 4) == 0) && (ppnum))  /* data for PP-B */
                                exp += 0x10000;

                        READ_RE_INDIRECT_REG(rssnum,
                                ((ppnum << 28) |
                                ((RDRAM0_MEM + rdram) << 24) |
                                (8*subtile) |
                                ((ystart % 16) * 128) |
                                        /* +(8*16) per y-line within a page*/
                                addr |
                                (page << 11) |
                                (PP_1_RSS_DRB_AB << 11)),
                                mask, exp);
			switch (addr) {
				case 0: ind36_1_lo = actual; break;
				case 2: ind36_1_hi = actual; break;
				case 4: ind36_2_lo = actual; break;
				case 6: ind36_2_hi = actual; break;
			}
#if DO_DEBUG
                           fprintf(stderr,
                                "Addr: 0x%x, Read: 0x%x,       ",
                                (ppnum << 28) | ((RDRAM0_MEM+rdram)<<24) |
                                (8*subtile) | ((ystart % 16)* 128) |
                                (page << 11) | addr | (PP_1_RSS_DRB_AB << 11),
				actual);
			exp = actual;
#endif
                        if ((actual & EIGHTEENBIT_MASK) != exp)
                        {
                           msg_printf(ERR,
                              "ERROR: Addr: 0x%x, Read: 0x%x, Expected: 0x%x\n",                                (ppnum << 28) | ((RDRAM0_MEM+rdram)<<24) |
                                (8*subtile) | ((ystart % 16) * 128) |
                                (page << 11) | addr | (PP_1_RSS_DRB_AB << 11),
				actual, exp);
                           msg_printf(DBG,
                                "192 pixel block started at x: %d, y: %d\n",
                                        xstart, ystart);
                           errors++;
#if DO_DEBUG
                           fprintf(stderr,
                                "Addr: 0x%x, Read: 0x%x, Expected: 0x%x\n",
                                (ppnum << 28) | ((RDRAM0_MEM+rdram)<<24) |
                                (8*subtile) | ((ystart % 16)* 128) |
                                (page << 11) | addr | (PP_1_RSS_DRB_AB << 11),
				actual, exp);
                           fprintf(stderr,
                                "pixel block started at x: %d, y: %d\n",
                                        xstart, ystart);
#endif
                           return (errors);
                        } /* if error */
                   } /* for addr */
#if DO_DEBUG
		   fprintf(fp, "\n");
		   indirect_to_32_rgba(ind36_1_lo, ind36_1_hi, rgba);
		   fprintf(stderr, "lo: 0x%x, hi: 0x%x, rgba-1: 0x%x,  "
			, ind36_1_lo, ind36_1_hi, rgba);
		   indirect_to_32_rgba(ind36_2_lo, ind36_2_hi, rgba);
		   fprintf(stderr, "(0x%x) lo: 0x%x, hi: 0x%x, rgba-2: 0x%x\n", 
				((ppnum << 28) |
                                ((RDRAM0_MEM + rdram) << 24) |
                                (8*subtile) |
                                ((ystart % 16) * 128) |
                                        /* +(8*16) per y-line within a page*/
                                addr |
                                (page << 11) |
                                (PP_1_RSS_DRB_AB << 11)), 
				ind36_2_lo, ind36_2_hi,
				rgba);
#endif
                } /* for subtile */

        return (errors);
}

/***************************************************************************
 * mg0_point								   *
 * 	-x [x-coord]							   *
 *	-y [y-coord]							   *
 *	-r [red]							   *
 *	-g [green]							   *
 *	-b [blue]							   *
 * 	-a [alpha]							   *
 *	-t (enables anti-aliased points)				   *
 *	-X (enables X-points)						   *
 *	-i (enables color index)					   *
 *	-c [color index]						   *	
 *      -s [optional rss number]					   *
 *	-o [optional max color]						   *
 *	-d (optional dither off, default is on)				   *
 *	-f [optional buffer, default = 1 color buf, 0x41 = overlay	   *
 *									   *
 ***************************************************************************/
__uint32_t
mg0_point(int argc, char ** argv)
{
	__uint32_t bad_arg = 0, xstart, ystart, aa_en, x_en, ci_en, max_color;
	__uint32_t red, green, blue, alpha, color_index, rssnum, cmd, dith_en;
	__uint32_t dbuffer = 1;
	float red_float, green_float, blue_float, alpha_float;
	ppfillmodeu	ppfillmode = {0};

	/* default */
	fpusetup();
	red = green = blue = alpha = 0;
	aa_en = x_en = ci_en = 0;
	color_index = 0;
	rssnum = 4; /* all rss's participate */
	max_color = 100;
	dith_en = 1;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&red);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&red);
                                break;
                        case 'g':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&green);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&green);
                                break;
                        case 'b':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&blue);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&blue);
                                break;
                        case 'a':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&alpha);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&alpha);
                                break;
                        case 't':
                                aa_en = 1; break;
			case 'X':
				x_en = 1; break;
			case 'i':
				ci_en = 1; break;
			case 'd':
				dith_en = 0; break;
			case 'x':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&xstart);
                                        argc--; argv++;
				}
				else {
                                        atob(&argv[0][2], (int*)&xstart);
				};
				break;
			case 'y':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ystart);
                                        argc--; argv++;
				}
				else {
                                        atob(&argv[0][2], (int*)&ystart);
				};
				break;
			case 'c':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&color_index);
                                        argc--; argv++;
				}
				else {
                                        atob(&argv[0][2], (int*)&color_index);
				};
				break;
			case 'o':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&max_color);
                                        argc--; argv++;
				}
				else {
                                        atob(&argv[0][2], (int*)&max_color);
				};
			case 's':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum);
                                        argc--; argv++;
				}
				else {
                                        atob(&argv[0][2], (int*)&rssnum);
				};
				break;
			case 'f':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&dbuffer);
                                        argc--; argv++;
				}
				else {
                                        atob(&argv[0][2], (int*)&dbuffer);
				};
				break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
        }

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, 
		  "Usage -x[x-coord] -y[y-coord] -r[red] -g[green] -b[blue] -a[alpha]\n");
		msg_printf(SUM, 
		  " -t (anti-alias enable) -X (X-enable) -i (CI mode) -c[color-index] -s[optional RSS #] -o[optional max color, default=100] -d(optional dither off, default is on), -f[optional buffer, default = 1 color buf, 0x41 = overlay]\n");
		msg_printf(SUM, "red, blue, green, and alpha are on a scale of 0-100.\n");
		return (0);
	}

#ifdef MGRAS_DIAG_SIM
	xstart = 10; ystart = 10;
	red = 100; green = 0; blue = 100; alpha = 100;
	fprintf(stdout, "red: %d, green: %d, blue: %d, alpha: %d\n",
		red, green, blue, alpha);
	aa_en = 0; x_en = 0; ci_en = 0; color_index = 0x3f;
#endif

	red_float = (float)(red)/max_color;	
	green_float = (float)(green)/max_color;	
	blue_float = (float)(blue)/max_color;	
	alpha_float = (float)(alpha)/max_color;	

#if 1
	msg_printf(DBG, "red: %f, green: %f, blue: %f, alpha: %f\n",
		red_float, green_float, blue_float, alpha_float);
#endif
	
	HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(CONFIG))), rssnum,
		EIGHTEENBIT_MASK);
	_mgras_rss_init(rssnum);
	_draw_setup(0, 0);
	
	if (dith_en) {
		ppfillmode.bits.DitherEnab = 1;
        	ppfillmode.bits.DitherEnabA = 1;
	}
	else {
		ppfillmode.bits.DitherEnab = 0;
        	ppfillmode.bits.DitherEnabA = 0;
	}
        ppfillmode.bits.DrawBuffer = PP_FILLMODE_DBUFA;

	if (ci_en) {
                ppfillmode.bits.LogicOpEnab = 1;
                ppfillmode.bits.PixType = PP_FILLMODE_CI12;
                ppfillmode.bits.CIclamp = 1;
                ppfillmode.bits.LogicOP = PP_FILLMODE_LO_SRC;

	}
	else {
                ppfillmode.bits.LogicOpEnab = 0;
                ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */;
	}

	if (dbuffer == PP_FILLMODE_DBUFA) {
		ppfillmode.bits.PixType = PP_FILLMODE_RGBA8888;
	}	
	else if (dbuffer == PP_FILLMODE_DOVERA_OR_B) {
		ppfillmode.bits.PixType = PP_FILLMODE_CI8;
	}		

	ppfillmode.bits.DrawBuffer = dbuffer;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)), ppfillmode.val, 0xffffffff);

	_mgras_pt(xstart, ystart, red_float, green_float, blue_float, 
		alpha_float, aa_en, x_en, ci_en, color_index);
	return(0);
}

/***************************************************************************
 * _mgras_pt								   *
 *									   *
 *									   *
 ***************************************************************************/
__uint32_t
_mgras_pt(
	float xstart,
	float ystart,
	float red,
	float green,
	float blue,
	float alpha,
	__uint32_t aa_enable,
	__uint32_t x_enable,
	__uint32_t ci_en,
	__uint32_t color_index)
{
	__uint32_t cmd, point_op, ir_setup, EPF_First, EPF_Last;
	__uint32_t x, y;
	float tmp3, tmp2;
	fillmodeu       fillmode = {0};

	fillmode.bits.ZiterEN = 1;
        fillmode.bits.ShadeEN = 1;
        fillmode.bits.RoundEN = 1;

	x = (xstart)/1;
	y = (ystart)/1;

	ir_setup = 0;
	EPF_First = EPF_Last = 0;
	if (x_enable) {
		point_op = IR_OP_X_POINT;

        	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 +XLINE_XYSTARTI),4);
        	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR,((x << 16)+y), ~0x0);

        	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + XLINE_XYENDI), 4);
        	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR, (((x+1) << 16)+y), ~0x0);
	}
	else { /* GL-line */
		point_op = IR_OP_GL_POINT;
		__glMgrRE4Position2_diag(GLINE_XSTARTF, xstart+ 49151.5, 
		ystart+ 49151.5);
	}

	if (ci_en) {
		cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + RE_RED), 4);
                HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
                HQ3_PIO_WR(CFIFO1_ADDR, (color_index << 12), ~0x0);	
	}
	else {
		__glMgrRE4ColorExt_diag(RE_RED, red, green);
		__glMgrRE4ColorExt_diag(RE_BLUE, blue, alpha);
	}

	if (aa_enable) {
	/* 	_load_aa_tables(); */
		fillmode.bits.PointAA = 1;
        	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE), 4);
        	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

		EPF_First = EPF_Last = 1;
		tmp3 = xstart + 1.0 + 49151.5;
		tmp2 = ystart + 49151.5;
		__glMgrRE4LineSlope_diag(GLINE_DX, 1.0, 0.0);
		__glMgrRE4Position2_diag(GLINE_XENDF, tmp3, tmp2);

#if 0
	/* also need to turn on blending */
        ppblendfactor.bits.BlendSfactor = PP_SBF_SA; /* source alpha */
        ppblendfactor.bits.BlendDfactor = PP_DBF_MDA; /* 1-dest alpha */

        cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_BLENDFACTOR & 0x1ff)),4);        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, (ppblendfactor.val | 0x80000000),~0x0,HQ_MOD,0);

        cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_BLENDFACTOR & 0x1ff)),4);        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, 0x7880c8c4, ~0x0);

        ppfillmode.bits.DitherEnab = 1;
        ppfillmode.bits.DitherEnabA = 1;
        ppfillmode.bits.ZEnab = 0;
        ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
        ppfillmode.bits.DrawBuffer = 0x1; /* buffer A */
        ppfillmode.bits.BlendEnab = 1; /* turn on blending */

        cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_FILLMODE & 0x1ff)),4);        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, (ppfillmode.val | 0x0c000000), ~0x0);

#endif
	}

	__glMgrRE4LineAdjust_diag(GLINE_ADJUST, 0.0);

	_mgras_re4_ir_write(point_op, ir_setup, EPF_First, EPF_Last);

	return(0);
}

void _draw_char(
	__uint32_t rssnum,
	ushort_t *bitmap,
	__uint32_t xstart, __uint32_t ystart,
	__uint32_t xend, __uint32_t yend,
	float red, float green, float blue, float alpha)
{
	__uint32_t i, cmd;
	fillmodeu	fillmode = {0};
	iru		ir = {0};

	fillmode.bits.ZiterEN = 1;
	fillmode.bits.ShadeEN = 1;
	fillmode.bits.RoundEN = 1;
	fillmode.bits.CharStipEn = 1;
	fillmode.bits.BlockType = FILLMODE_DRAW_BLOCK_CHAR;

	ir.bits.Opcode = IR_OP_BLOCK; /* block */
        ir.bits.Setup = 1;

	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + FILLMODE), 4);    
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);                
        HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0); 

	__glMgrRE4ColorExt_diag(RE_RED, red, green);
	__glMgrRE4ColorExt_diag(RE_BLUE, blue, alpha);

	__glMgrRE4Int_diag(BLOCK_XYSTARTI, ((xstart  << 16) | ystart)); 
    	__glMgrRE4Int_diag(BLOCK_XYENDI, ((xend << 16) | yend));

	__glMgrRE4IR_diag(ir.val);

	/* write the 8 shorts which make up the bitmap */
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1c00 + CHAR_H),4);
	for (i = 0; i < 8; i++) {
        	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR, ((*bitmap)<<24), ~0x0);
		bitmap++;
	}
}	

/***************************************************************************
 * mgras_do_indirect 							   *
 *	-w(for write) or -r(for read)					   *	
 *	-a[address] or -d[data]					   	   *
 *	-l[optional loopcount]				   		   *
 *									   *
 ***************************************************************************/
__uint32_t
mgras_do_indirect(int argc, char ** argv)
{
	__uint32_t is_addr, data,do_rd,bad_arg = 0;
	__int32_t loopcount = 1;
	__uint32_t device, addr, actual = 0xdead, mask = 0xffffffff;
	__uint32_t wr_val;

	is_addr =  do_rd = 0;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'w':
                                break;
                        case 'r':
				do_rd = 1; 
                                break;
                        case 'd':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&data);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&data);
                                break;
                        case 'a':
				is_addr = 1;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&addr);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&addr);
                                break;
			case 'l':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atob(&argv[1][0], (int*)&loopcount);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atob(&argv[0][2], (int*)&loopcount);
                                }
                                if (loopcount < 0) {
                                        msg_printf(SUM,
                                           "Error. Loop must be > or = to 0\n");                                        return (0);
                                }
                                break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
           msg_printf(SUM,	
		"Usage: -w|-r (for write or read) -a|-d [address or data] -l[optional loopcount]. -w and -r are mutually exclusive, and -a and -d are mutually exclusive. \nFor a read from DEVICE_DATA, just put in dummy data, e.g. -d0x0.\n For a read from DEVICE_ADDR, put in dummy data, e.g. -a0x0\n");
	   return (0);
	}

#ifdef MGRAS_DIAG_SIM
	for (i = 0; i < 3; i++) {	
		switch (i) {
			case 0: do_rd = 1; break;
			case 1: do_rd = 0; is_addr = 1; 
				addr =PP_FILLMODE;break;
			case 2: do_rd = 0; is_addr = 0; 
				data = 0xbaadcafe;break;
		}
		loopcount = 1;
#endif
	if (is_addr)
		device = DEVICE_ADDR;
	else
		device = DEVICE_DATA;

	if (loopcount)
                loopcount++; /* bump it up since we decrement it next */

        while (--loopcount) {
		if (do_rd) { 
			HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(device))),
 				mask, actual, 0);	
#ifdef MGRAS_DIAG_SIM
			fprintf(stderr,"Read Indirect Addr: 0x%x, Data: 0x%x\n",
				device, actual);
#else
			msg_printf(SUM,"Read Indirect Addr: 0x%x, Data: 0x%x\n",
				device, actual);
#endif
		} 
		else { /* do a write */
#ifdef MGRAS_DIAG_SIM
		   fprintf(stderr,"Write Indirect Address: 0x%x, Data: 0x%x\n",
				device, (is_addr ? addr:data));
#else
		   msg_printf(DBG,"Write Indirect Address: 0x%x, Data: 0x%x\n",
				device, (is_addr ? addr:data));
#endif
		   if (is_addr) {
			wr_val = addr;
		   } else {
			wr_val = data;
		   }

		   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(device))),
			 wr_val, mask);
		}
        }
#ifdef MGRAS_DIAG_SIM
	}
#endif
	return(0);
}

/***************************************************************************
 * mg_sync_repp()							   *
 *									   *
 * Calls MgrasSyncREPP() from libsk. Does the synchronization between 	   *
 *	the RE and PP Rambus channels.					   *
 *									   *
 * 	-r[RSS #] (not curretly implemented)				   *
 ***************************************************************************/

__uint32_t
mg_sync_repp(int argc, char ** argv)
{
	__uint32_t actual,rflag, rssnum, ppnum = 0, bad_arg;
	__uint32_t re4start, re4end, ppstart, ppend;

	bad_arg = rflag = 0;

	/* initialize */
	re4start = 0; re4end = numRE4s;
	ppstart = 0; ppend = 2;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				rflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&re4start);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&re4start);
				re4end = re4start + 1;
                                break;
                        case 'p':
				rflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&ppstart);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&ppstart);
				ppend = ppstart + 1;
				break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
	}

	/* -r switch and args not currently accepted. Maybe in a future 
	 * implementation
	 */
	if ((bad_arg) || (argc)) {
		msg_printf(SUM,"Usage: mg_sync_repp\n");
		msg_printf(SUM,"Usage: -r[optional RSS #] -p[optional PP #]\n");
		return (0);
	}

	READ_RSS_REG(0, STATUS, 0, ~0x0);
	if ( (actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) {
		msg_printf(ERR, "RE4 status indicates system is busy. Skipping sync attempt.\n");
		return (1);
	}	

	/* MgrasSyncREPP only cares about these 2 fields */
	NUMBER_OF_RE4S();

#ifndef MGRAS_DIAG_SIM
	_mg_set_ZeroGE();
	for (rssnum = re4start; rssnum < re4end; rssnum++)
		for (ppnum = ppstart; ppnum < ppend; ppnum++) {
			msg_printf(SUM, " mgbase before MgrasSyncREPP 0x%llx \n", mgbase);
			MgrasSyncREPP(mgbase, numRE4s, rssnum, ppnum);
			msg_printf(SUM, " mgbase after MgrasSyncREPP 0x%llx \n", mgbase);
		}
#endif
	return(0);
}

/***************************************************************************
 * mg_sync_pprdram()							   *
 *									   *
 * Calls MgrasSyncPPRDRAM() from libsk. Does the synchronization between   *
 *	the PP and RDRAMs.					   	   *
 *									   *
 * 	-r[RSS #] -p[optional PP #] (not curretly implemented)		   *
 ***************************************************************************/

__uint32_t
mg_sync_pprdram(int argc, char ** argv)
{
	__uint32_t actual, rflag, rssnum, bad_arg;
	ushort_t BdVer1_Reg;
	mgras_info info;

	bad_arg = rflag = 0;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				rflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rssnum);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rssnum);
                                break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
	}

	/* -r switch and args not currently accepted. Maybe in a future 
	 * implementation
	 */
	if ((bad_arg) || (argc)) {
		msg_printf(SUM,"Usage: mg_sync_pprdram\n");
#if 0
		msg_printf(SUM,"Usage -r[RSS #]. The -r switch is required.\n");
#endif
		return (0);
	}

	READ_RSS_REG(0, STATUS, 0, ~0x0);
	if ( (actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) {
		msg_printf(ERR, "RE4 status indicates system is busy. Skipping sync attempt.\n");
		return (1);
	}	

	/* MgrasSyncREPP only cares about these 2 fields */
	NUMBER_OF_RE4S();
	BdVer1_Reg = mgbase->bdvers.bdvers1;
	
	info.NumREs = numRE4s;
	info.NumGEs = BdVer1_Reg & 0x3;

#ifndef MGRAS_DIAG_SIM
	_mg_set_ZeroGE();
	MgrasSyncPPRDRAMs(mgbase, &info);
#endif

	return(0);
}

/***************************************************************************
 * mg_sync_pprdram()							   *
 *									   *
 * Calls MgrasSyncPPRDRAM() from libsk. Does the synchronization between   *
 *	the PP and RDRAMs.					   	   *
 *									   *
 * 	-r[RSS #] -p[optional PP #] 					   *
 ***************************************************************************/

__uint32_t
mg_sync_pprdram_repair(int argc, char ** argv)
{
	__uint32_t actual, pflag, rflag, dflag, rdstart, rdend, rssnum, bad_arg;
	__uint32_t ppnum, rdnum;

	/* initialize */
	bad_arg = rflag = pflag = dflag = 0;
	rssstart = 0; rssend = numRE4s;
	ppstart = 0; ppend = 2;
	rdstart = 0; rdend = 3;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				rflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rssstart);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rssstart);
				rssend = rssstart + 1;
                                break;
                        case 'p':
				pflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&ppstart);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&ppstart);
				ppend = ppstart + 1;
				break;
                        case 'd':
				dflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rdstart);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rdstart);
				rdend = rdstart + 1;
				break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
		msg_printf(SUM,"Usage: mg_sync_pprdram -r[optional RSS #] -p[optional PP #] -d[optional RDRAM #, 0-2]\n");
		return (0);
	}

	READ_RSS_REG(0, STATUS, 0, ~0x0);
	if ( (actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) {
		msg_printf(ERR, "RE4 status indicates system is busy. Skipping sync attempt.\n");
		return (1);
	}	

	/* MgrasSyncREPP only cares about these 2 fields */
	NUMBER_OF_RE4S();
	_mg_set_ZeroGE();

	for (rssnum = rssstart; rssnum < rssend; rssnum++) {
    	   for (ppnum = ppstart; ppnum < ppend; ppnum++) {
     		for (rdnum = rdstart; rdnum < rdend; rdnum++) {
      		   MgrasSyncPPRDRAM_repair(mgbase,rssnum,ppnum,rdnum);
    		}
  	   }
	}

	return(0);
}

/***************************************************************************
 * mg_rdram_ccsearch()							   *
 *									   *
 * Calls MgrasInitCCR from libsk. Searches rdrams and sets current vals.   *
 *									   *
 ***************************************************************************/

__uint32_t
mg_rdram_ccsearch(int argc, char ** argv)
{
	__uint32_t actual, rflag, rssnum, bad_arg;
	ushort_t BdVer1_Reg;
	mgras_info info;

	bad_arg = rflag = 0;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				rflag++;
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rssnum);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rssnum);
                                break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
	}

	/* -r switch and args not currently accepted. Maybe in a future 
	 * implementation
	 */
	if ((bad_arg) || (argc)) {
		msg_printf(SUM,"Usage: mg_rdram_ccsearch\n");
		return (0);
	}

	READ_RSS_REG(0, STATUS, 0, ~0x0);
	if ( (actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) {
		msg_printf(ERR, "RE4 status indicates system is busy. Skipping sync attempt.\n");
		return (1);
	}	

	/* MgrasSyncREPP only cares about these 2 fields */
	NUMBER_OF_RE4S();
	BdVer1_Reg = mgbase->bdvers.bdvers1;
	
	info.NumREs = numRE4s;
	info.NumGEs = BdVer1_Reg & 0x3;

#ifndef MGRAS_DIAG_SIM
	_mg_set_ZeroGE();
	MgrasInitCCR(mgbase, &info);
#endif
	return(0);
}


void
rss_GetComputeSig(__uint32_t WhichCmap, __uint32_t testPattern)
{
	input   *ExpSign;

#ifndef MGRAS_DIAG_SIM
	ExpSign = &Signature;

	switch (WhichCmap) {
		case CmapAll:
			/* =============    Program both Cmap0 & Cmap1  =============   */ 
			switch (testPattern) {
			/* ====== Color Triangle Test ====== */ 
				/* ====== Red Triangle ====== */ 
				case	0x54005595:
					/* Blue = 0x0	Green = 0x0	Red = 0x11	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0x11	*/ 
					ExpSign->red = 0x33 ; 	 ExpSign->green = 0x50 ; 	 ExpSign->blue = 0x22e ;
				break;
				/* ====== Green Triangle ====== */ 
				case	0x5455be00:
					/* Blue = 0x0	Green = 0xee	Red = 0x0	*/ 
					/* Blue = 0x0	Green = 0xee	Red = 0x0	*/ 
					ExpSign->red = 0x330 ; 	 ExpSign->green = 0x267 ; 	 ExpSign->blue = 0x293 ;
				break;
				/* ====== Blue Triangle ====== */ 
				case	0xa7180000:
					/* Blue = 0x9	Green = 0x0	Red = 0x0	*/ 
					/* Blue = 0x11	Green = 0x0	Red = 0x0	*/ 
					ExpSign->red = 0x115 ; 	 ExpSign->green = 0x220 ; 	 ExpSign->blue = 0x34d ;
				break;
				/* ====== Alpha Triangle ====== */ 
				case	0xf1563e81:
					/* Blue = 0x0	Green = 0xee	Red = 0xbf	*/ 
					/* Blue = 0x0	Green = 0xee	Red = 0xbf	*/ 
					ExpSign->red = 0x2a3 ; 	 ExpSign->green = 0x33f ; 	 ExpSign->blue = 0x3fa ;
				break;
			/* ====== Z-Buffer Test ====== */ 
				/* ====== s1t0 ====== */ 
				case	0x80007f80:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					ExpSign->red = 0x214 ; 	 ExpSign->green = 0xbc ; 	 ExpSign->blue = 0x293 ;
				break;
				/* ====== s1t1 ====== */ 
				case	0xa05fffa0:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					ExpSign->red = 0x214 ; 	 ExpSign->green = 0xbc ; 	 ExpSign->blue = 0x293 ;
				break;
				/* ====== s1t4 ====== */ 
				case	0xa0203fa0:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					ExpSign->red = 0x369 ; 	 ExpSign->green = 0x1ef ; 	 ExpSign->blue = 0x5a ;
				break;
				/* ====== s1t5 ====== */ 
				case	0xc07fbfc0:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					ExpSign->red = 0x5e ; 	 ExpSign->green = 0x25 ; 	 ExpSign->blue = 0x346 ;
				break;
				/* ====== s2t1 ====== */ 
				case	0x80403f80:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					ExpSign->red = 0x314 ; 	 ExpSign->green = 0x228 ; 	 ExpSign->blue = 0x61 ;
				break;
				/* ====== s2t2 ====== */ 
				case	0xc03fffc0:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					ExpSign->red = 0x15e ; 	 ExpSign->green = 0x2b1 ; 	 ExpSign->blue = 0x1b4 ;
				break;
			/* ====== Points Test ====== */ 
				case	0xfd4fbe7c:
					/* Blue = 0x80	Green = 0x80	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0x0	*/ 
					ExpSign->red = 0xd8 ; 	 ExpSign->green = 0x227 ; 	 ExpSign->blue = 0x229 ;
				break;
			/* ====== Line Test ====== */ 
				case	0xc684ec6c:
					/* Blue = 0x0	Green = 0x0	Red = 0x0	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0x0	*/ 
					ExpSign->red = 0x1c8 ; 	 ExpSign->green = 0x2c ; 	 ExpSign->blue = 0x36a ;
				break;
			/* ====== Stippled Triangle ====== */ 
				case	0xaa0055aa:
					/* Blue = 0x0	Green = 0x0	Red = 0xff	*/ 
					/* Blue = 0x0	Green = 0x0	Red = 0x0	*/ 
					ExpSign->red = 0x1bc ; 	 ExpSign->green = 0x2af ; 	 ExpSign->blue = 0x19e ;
				break;
			/* ====== Block Test ====== */ 
				case	0x3fffff40:
					/* Blue = 0x80	Green = 0x80	Red = 0x80	*/ 
					/* Blue = 0x7f	Green = 0x7f	Red = 0x7f	*/ 
					ExpSign->red = 0x3d7 ; 	 ExpSign->green = 0x1a3 ; 	 ExpSign->blue = 0x284 ;
				break;
			/* ====== Character Test ====== */ 
				case	0x920e01d2:
					/* Blue = 0x40	Green = 0x80	Red = 0xff	*/ 
					/* Blue = 0x40	Green = 0x7f	Red = 0xff	*/ 
					ExpSign->red = 0xb8 ; 	 ExpSign->green = 0x17a ; 	 ExpSign->blue = 0xc ;
				break;
			/* ====== Tex Poly Test ====== */ 
				case	0x21247dbc:
					/* Blue = 0xaa	Green = 0xaa	Red = 0xaa	*/ 
					/* Blue = 0xaa	Green = 0xff	Red = 0xff	*/ 
					ExpSign->red = 0x72 ; 	 ExpSign->green = 0xf7 ; 	 ExpSign->blue = 0x7b ;
				break;
			/* ====== No Tex Poly Test ====== */ 
				case	0xffffffc0:
					/* Blue = 0xff	Green = 0xff	Red = 0xff	*/ 
					/* Blue = 0xff	Green = 0xff	Red = 0xff	*/ 
					ExpSign->red = 0x21b ; 	 ExpSign->green = 0x1a3 ; 	 ExpSign->blue = 0x267 ;
				break;
			/* ====== Logic Op Test ====== */ 
				case	0x188eecb0:
					/* Blue = 0x88	Green = 0xee	Red = 0xcc	*/ 
					/* Blue = 0x88	Green = 0xee	Red = 0xcc	*/ 
					ExpSign->red = 0xc3 ; 	 ExpSign->green = 0x1ba ; 	 ExpSign->blue = 0x252 ;
				break;
			/* ====== TE LOD Test - 32pixels ====== */ 
				case	0x1111107c:
					/* Blue = 0xaa	Green = 0xaa	Red = 0xaa	*/ 
					/* Blue = 0xaa	Green = 0xaa	Red = 0xaa	*/ 
					ExpSign->red = 0x14b ; 	 ExpSign->green = 0x218 ; 	 ExpSign->blue = 0x16a ;
				break;
			/* ====== TE LOD Test - 128 Pixels====== */ 
				case	0x979f2ecc:
					/* Blue = 0xaa	Green = 0xaa	Red = 0xaa	*/ 
					/* Blue = 0xaa	Green = 0xdd	Red = 0xdd	*/ 
					ExpSign->red = 0x207 ; 	 ExpSign->green = 0x394 ; 	 ExpSign->blue = 0x136 ;
				break;
			/* ====== Load/Resample Test ====== */ 
				case	0xffffff80:
					/* Blue = 0x55	Green = 0x55	Red = 0x55	*/ 
					/* Blue = 0x55	Green = 0x0	Red = 0x0	*/ 
					ExpSign->red = 0x23f ; 	 ExpSign->green = 0x13f ; 	 ExpSign->blue = 0x1c7 ;
				break;
			/* ====== TE LOD Test - 4tram, 128 pixels ====== */ 
				case	0x424b0d7f:
					/* Blue = 0xaa	Green = 0xaa	Red = 0xaa	*/ 
					/* Blue = 0xaa	Green = 0xd5	Red = 0xd5	*/ 
					ExpSign->red = 0x27b ; 	 ExpSign->green = 0x27e ; 	 ExpSign->blue = 0x330 ;
				break;
			};
		break;
	}
#endif
}

/* 
 * Enable coprocessor 1 for fpu. (from the IP22/fpu tests)
 *
 */
__uint32_t
fpusetup(void)
{
	__uint32_t status;

#ifndef MGRAS_DIAG_SIM
	/* enable cache and fpu - cache ecc errors still enabled */
	status = GetSR();
	status |= SR_CU0|SR_CU1;
	SetSR(status);

	/* clear cause register */
	set_cause(0);

	/* clear fpu status register */
	SetFPSR(0);
#endif
	return 0;
}


/* -c == set the number of RE4s
 * -q == don't output any messages -- used in scripts 
 */
__uint32_t mg_setnumre4s(int argc, char ** argv)
{
	__uint32_t num = 999999, bad_arg = 0, quiet = 0;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'c':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&num);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&num);
                                break;
                        case 'q': quiet = 1; break;
			default: bad_arg++; break;
		}
                argc--; argv++;
	}

	if (bad_arg || argc) {
		msg_printf(SUM, "Usage: -c [number]\n");
		return (0);
	}

	if (num != 999999)
		numRE4s = num;

	if (quiet == 0)
		msg_printf(VRB, "Number of RSS in this system: %d\n", numRE4s);
		
	return(numRE4s);
}

#ifdef MFG_USED
__uint32_t
mg_setnumtrams(int argc, char ** argv)
{
	__uint32_t num = 999999, bad_arg = 0;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'c':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&num);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&num);
                                break;
			default: bad_arg++; break;
		}
                argc--; argv++;
	}

	if (bad_arg || argc) {
		msg_printf(SUM, "Usage: -c [number]\n");
		return (0);
	}

	if (num != 999999)
		numTRAMs = num;
	msg_printf(VRB, "numTRAMs: %d\n", numTRAMs);
	return(numTRAMs);
}
#endif /* MFG_USED */

/* Given a rgb-12,12,12 value, return the expected indirect pio read of a frame buffer
 * The val returned is the 18-bit half word
 */

__uint32_t 
_rgb12_to_fb36(__uint32_t red, __uint32_t green, __uint32_t blue, __uint32_t hi_half)
{
	__uint32_t bit, val, n, start, end, g_ref, r_ref, b_ref;

#ifdef MGRAS_DIAG_SIM
	fprintf(stderr, "red: %x, green: %x, blue: %x\n", red, green, blue);
#endif

	g_ref = 15;
	r_ref = 16;
	b_ref = 17;

	if (hi_half) {
		start = 0;
		end = 5;
	}
	else {
		start = 6;
		end = 11;
	}

	val = 0x0;
	for (n = start; n <= end; n++) {
		bit = (green & (1 << n)) >> n;
		val |= (bit << (g_ref - (3 * (n-start))));	
		bit = (red & (1 << n)) >> n;
		val |= (bit << (r_ref - (3 * (n-start))));	
		bit = (blue & (1 << n)) >> n;
		val |= (bit << (b_ref - (3 * (n-start))));	
	}

	return (val);
}

/* converts 36-bit rgb121212 frame buffer data to their red, green, blue 
 * component.
 */

void
_fb_rgb121212_to_comp(__uint32_t val_hi, __uint32_t val_lo, __uint32_t *red,
__uint32_t *green, __uint32_t *blue)
{
__uint32_t i, j, lr, lg, lb, val, shift, g_ref, r_ref,b_ref;
__uint32_t n;

   g_ref = 0;
   r_ref = 1;
   b_ref = 2;

   lr = lg = lb = 0;

   for (j = 0; j < 2; j++) {
	if (j == 0) {
		val = val_lo;
		shift = 11;
	}
	else {
		val = val_hi;
		shift = 5;
	}
	for (i = 0, n = 0; n < 18; i++, n+=3) {	
	   lg |= ((val & (1 << (n + g_ref))) >> (n + g_ref)) << (shift -i);
	   lr |= ((val & (1 << (n + r_ref))) >> (n + r_ref)) << (shift -i);
	   lb |= ((val & (1 << (n + b_ref))) >> (n + b_ref)) << (shift -i);
	}
   }
	
   *red = lr;
   *green = lg;
   *blue = lb;
}

void
_fb_rgba888_to_comp(
	__uint32_t val_hi, 
	__uint32_t val_lo, 
	__uint32_t *red,
	__uint32_t *green, 
	__uint32_t *blue,
	__uint32_t *alpha)
{
   __uint32_t i, j, lr, lg, lb, la, val, shift, g_ref, r_ref,b_ref;
   __uint32_t n, a_ref;

   g_ref = 0;
   r_ref = 1;
   b_ref = 2;
   a_ref = 6;

   lr = lg = lb = la = 0;

   for (j = 0; j < 2; j++) {
	if (j == 0) {
	   val = val_lo;
	   shift = 7;
	   for (i = 0, n = 0; n < 18; i++, n+=3) {	
	      lg |= ((val & (1 << (n + g_ref))) >> (n + g_ref)) << (shift -i);
	      lr |= ((val & (1 << (n + r_ref))) >> (n + r_ref)) << (shift -i);
	      lb |= ((val & (1 << (n + b_ref))) >> (n + b_ref)) << (shift -i);
	   }
	}
	else {
	   val = val_hi;
	   shift = 1;
	   for (i = 0, n = 0; i < 2; i++, n+=3) {	
	      lg |= ((val & (1 << (n + g_ref))) >> (n + g_ref)) << (shift -i);
	      lr |= ((val & (1 << (n + r_ref))) >> (n + r_ref)) << (shift -i);
	      lb |= ((val & (1 << (n + b_ref))) >> (n + b_ref)) << (shift -i);
	   }
	   shift = 7;
	   for (i = 0; i < 8; i++) {
	   	la |= ((val & (1 << (a_ref + i))) >> (a_ref + i)) << (shift -i);
	   }
	}
   }
	
   *red = lr;
   *green = lg;
   *blue = lb;
   *alpha = la;
}

/**************************************************************************
 *                                                                        *
 *  _convert_fb_to_comp							  *
 * Converts fb contents into their rgba or ci or zst components.    	  *	
 *                                                                        *
 *		
 *                                                                        *
 **************************************************************************/
void
_convert_fb_to_comp( __uint32_t mode, __uint32_t val_hi, __uint32_t val_lo)
{ 
	__uint32_t red, green, blue, alpha; 

	red = green = blue = alpha = 0;

	switch (mode) {
	   case RGB121212: 
		_fb_rgb121212_to_comp(val_hi, val_lo,&red,&green,&blue);
#ifdef MGRAS_DIAG_SIM
		fprintf(stderr,
			"RGB,12bit: (Red:0x%x, Green:0x%x, Blue:0x%x)\n",
				red, green, blue);
#else
		msg_printf(VRB,
			"RGB,12bit: (Red:0x%x, Green:0x%x, Blue:0x%x)\n",
				red, green, blue);
#endif
		break;
	   case RGBA8888: 
		_fb_rgba888_to_comp(val_hi, val_lo,&red,&green,&blue,&alpha);
#ifdef MGRAS_DIAG_SIM
		fprintf(stderr,
		   "RGBA,8bit: (Red:0x%x, Green:0x%x, Blue:0x%x, Alpha:0x%x)\n",
				red, green, blue, alpha);
#else
		msg_printf(VRB,
		   "RGBA,8bit: (Red:0x%x, Green:0x%x, Blue:0x%x, Alpha:0x%x)\n",
				red, green, blue, alpha);
#endif
		break;
	}		

#ifdef MGRAS_DIAG_SIM
	fprintf(stderr, "\n");
#else
	msg_printf(VRB, "\n");
#endif
}

/*
 * Print out the xy to rdram address mapping.
 */
int
mg_xy_to_rdram(int argc, char ** argv)
{
	__uint32_t x, y, algorithm = 0, mode, tmp1, tmp2, bad_arg = 0;
	__uint32_t drb_ptr = mg_tinfo->DRBptrs.main;

#ifndef MGRAS_DIAG_SIM
	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'x':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&x);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&x);
                                break;
                        case 'y':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&y);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&y);
                                break;
                        case 'a':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&algorithm);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&algorithm);
                                break;
                        case 'd':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&drb_ptr);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&drb_ptr);
                                break;
			default: bad_arg++; break;
		}
                argc--; argv++;
	}

	if (bad_arg || argc) {
		msg_printf(SUM, "Usage: -x[X coord] -y[Y coord]\n");
		return (0);
	}
#else 
	x = 0; y = 1023;
#endif

	mode = 0;
	_xy_to_rdram(x, y, algorithm, mode, 0, 0, &tmp1, &tmp2, drb_ptr);
	return(0);
}

/**************************************************************************
 *                                                                        *
 *  _xy_to_rdram							  *
 * Converts x,y coord to rdram location. 				  *
 *                                                                        *
 * algorithm: 0 ==> from the PP spec.                                     *
 *                                                                        *
 * mode: 0 ==>  just print out the xy-to-rdram mapping			  *
 * 	 1 ==> Take the xy-to-rdram mapping and do a compare of the       *
 *		 expected values (exp_hi, exp_lo)			  * 	
 *	 2 ==> Take the xy-to-rdram mapping and just read out vals with   *
 *		no comparison.						  *
 *                                                                        *
 **************************************************************************/

__uint32_t
_xy_to_rdram(
	__uint32_t x, 
	__uint32_t y, 
	__uint32_t algorithm, 
	__uint32_t mode, 
	__uint32_t exp_hi, 
	__uint32_t exp_lo,
	__uint32_t *actual_hi,
	__uint32_t *actual_lo,
	__uint32_t drb_ptr)
{
	__uint32_t rssnum, line_num, x_relative, num_line_types, ppnum, rdnum;
	__uint32_t div, x_hat, y_hat, pix_addr, page_num, rd_addr;
	__uint32_t actual;
	__uint32_t xtiles36, errors = 0;;

	if (XMAX == 1344) {
		xtiles36 = 7;
	}
	else if (XMAX == 2112) {
		xtiles36 = 11;
	}
	else {
		xtiles36 = XMAX/192;
		if ((xtiles36 * 192) < XMAX) {
			xtiles36 += 1;
		}
	}

	div = 4;
	num_line_types = 3; /* 3 patterns of lines */

	rssnum = y % numRE4s;
	line_num = (y/numRE4s) % num_line_types;
	x_relative = x % PIXEL_SUBTILE_SIZE;

	ppnum = x % 2;

	if (algorithm == 1) {
	switch (line_num) {
		case 0:
			switch (x_relative) {
				case 0:
				case 2: 
				case 1:
				case 3: rdnum = 0;
					break;
				case 4:
				case 6:
				case 5:
				case 7: rdnum = 1;
					break;
				case 8:
				case 10:
				case 9:
				case 11: rdnum = 2;
					break;
			}
			break;
		case 1:
			switch (x_relative) {
				case 0:
				case 2:
				case 1:
				case 3: rdnum = 1;
					break;
				case 4:
				case 6:
				case 5:
				case 7: rdnum = 2;
					break;
				case 8:
				case 10: 
				case 9:
				case 11: rdnum = 0;
					break;
			}
			break;
		case 2:
			switch (x_relative) {
				case 0:
				case 2: 
				case 1:
				case 3: rdnum = 2;
					break;
				case 4:
				case 6: 
				case 5:
				case 7: rdnum = 0;
					break;
				case 8:
				case 10: 
				case 9:
				case 11: rdnum = 1;
					break;
			}
			break;
	}

	}
	else 
	{

	if (((line_num == 0) && (((x/div) % num_line_types) == 0)) ||
	    ((line_num == 1) && (((x/div) % num_line_types) == 2)) ||
	    ((line_num == 2) && (((x/div) % num_line_types) == 1))) 
		rdnum = 0;

	if (((line_num == 0) && (((x/div) % num_line_types) == 1)) ||
	    ((line_num == 1) && (((x/div) % num_line_types) == 0)) ||
	    ((line_num == 2) && (((x/div) % num_line_types) == 2)))
		rdnum = 1;

	if (((line_num == 0) && (((x/div) % num_line_types) == 2)) ||
	    ((line_num == 1) && (((x/div) % num_line_types) == 1)) ||
	    ((line_num == 2) && (((x/div) % num_line_types) == 0)))
		rdnum = 2;

	}

	x_hat = ((x /12) << 1) | ((x & 0x2) >>1);
	y_hat = y / numRE4s;
	page_num = ((y_hat / 16) * xtiles36) + (x_hat / 32);
	/* pix_addr = ((subtile #)*2 + (1st or 2nd pixel in subtile) ) * 2 */
#if 0
	subtile_num = ((y * 16) + ((x % 192) / 12));
	if ((( x % 4) == 0) || ((x % 4) == 1))
		first_or_sec = 0;
	else
		first_or_sec = 1;
#endif
	pix_addr = (((y_hat & 0xf)<< 5) | (x_hat & 0x1f)) * 4;

	rd_addr = ((ppnum << 28) |
	   ((RDRAM0_MEM + rdnum) << 24) |
	   (page_num << 11)  |	
	   (pix_addr));

	if (mode == 0) {
	  switch (rssnum) {
		case 0:
			msg_printf(DBG, "RA BOARD\n"); break;
		case 1:
			msg_printf(DBG, "RB BOARD\n"); break;
		default:
			msg_printf(DBG, "Unsupported RSS number: %d\n", rssnum); break;
	  }
	  msg_printf(DBG, 
	 "X: %d, Y: %d ==> RSS-%d, PP-%d, RD-%d (RDRAM Addr: 0x%x)\n",
		x, y, rssnum, ppnum, rdnum, rd_addr);
	  msg_printf(DBG, "\n");
	  /*
	   * store the rdram error info in a structure for later analysis
	   */
	  _mgras_store_rdram_error_info(rssnum, ppnum, rdnum, rd_addr,
		x, y, exp_lo, *actual_lo);
	}

	if ((mode == 1) || (mode == 2)) {

		/* tell which RSS to read from */
        	rssnum = y % numRE4s;
        	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), rssnum,
                	THIRTEENBIT_MASK);

        	/* reset drb pointers for the read */
        	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(RSS_DRBPTRS_PP1))), 
			drb_ptr, EIGHTEENBIT_MASK);

		actual = 0xdeadbeef;
		READ_RE_INDIRECT_REG(0, rd_addr, EIGHTEENBIT_MASK, 0);
		*actual_lo = actual;
#ifdef MGRAS_DIAG_SIM

		fprintf(stderr, "X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, actual);
#else
		msg_printf(VRB, "X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, actual);
#endif
		if (mode == 2) { /* just print out */
		}
		else if (mode == 1) {
		
		   if (exp_lo != actual) {
#ifdef MGRAS_DIAG_SIM
		   	fprintf(stderr, "ERROR==> X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Exp: 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, exp_hi, actual);
#else
		   	msg_printf(ERR, "ERROR==> X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Exp: 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, exp_hi, actual);
			errors++;
#endif
		   }
		}

		actual = 0xdeadbeef;
		rd_addr += 2;
		READ_RE_INDIRECT_REG(0, rd_addr, EIGHTEENBIT_MASK, 0);
		*actual_hi = actual;
#ifdef MGRAS_DIAG_SIM
		fprintf(stderr, "X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, actual);
#else
		msg_printf(VRB, "X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, actual);
#endif
		if (mode == 2) { /* just print out */
		}
		else {
		   if (exp_hi != actual) {
#ifdef MGRAS_DIAG_SIM
		   	fprintf(stderr, "ERROR==> X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Exp: 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, exp_lo, actual);
#else
		   	msg_printf(ERR, "ERROR==> X: %d, Y: %d, RDRAM Addr (drb ptr = 0x%x): 0x%x, Exp: 0x%x, Read: 0x%x\n", x, y, drb_ptr, rd_addr, exp_lo, actual);
			errors++;
#endif
		   }
		}
	}

	return(errors);
}

void 
mg_wr_re_cmd(int argc, char **argv)
{
	__uint32_t pix_cmd, count;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'c':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&count);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&count);
                                break;
		}
		argc--; argv++;
	}

	pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 2, count);

	HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
}
void 
mg_wr_re_data(int argc, char **argv)
{
	__uint32_t low, high, count;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'l':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&low);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&low);
                                break;
                        case 'h':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&high);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&high);
                                break;
                        case 'c':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&count);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&count);
                                break;
		}
		argc--; argv++;
	}

	HQ3_PIO_WR64(CFIFO1_ADDR, high, low, ~0x0);
}

/* called by mg0_rect below */
void
_mg0_rect(
	__uint32_t llx,
	__uint32_t lly,
	__uint32_t urx,
	__uint32_t ury,
	__uint32_t max_r,
	__uint32_t max_g,
	__uint32_t max_b,
	__uint32_t max_a,
	__uint32_t do_compare,
	__uint32_t rgb_mode)
{
	__uint32_t ulx, uly, lrx, lry;
	__uint32_t x, y, algorithm, mode;
	__uint32_t max_z, mid_z, min_z, rssnum=4;
	__uint32_t drb_ptr = mg_tinfo->DRBptrs.main;
	__uint32_t exp_hi_half, exp_lo_half, actual_hi, actual_lo;
	float fl_max_r, fl_max_g, fl_max_b, fl_max_a;
        float fl_mid_r, fl_mid_g, fl_mid_b, fl_mid_a;
        float fl_min_r, fl_min_g, fl_min_b, fl_min_a;
        Vertex max_pt, mid_pt, min_pt;
        ppfillmodeu     ppfillmode = {0};
        ppzmodeu        ppzmode = {0};
        fillmodeu    fillmode = {0};

	fpusetup();

	/* increment by 1 since the tri routine views the coords as outside the tri */
	if (llx)
		llx++; 
	if (lly)
		lly++; 
	/* needs 2 increments for some reason */
	urx++; ury++;
	urx++; ury++;

	ulx = llx;
	uly = ury;
	lrx = urx;
	lry = lly;

	/* tri left coords: max_x= ulx, max_y = uly 
			    mid_x= llx, mid_y = lly
			    min_x= lrx, min_y = lry

	   tri right coords: max_x=ulx, max_y = uly
			     mid_x=urx, mid_y = ury
			     min_x=lrx, min_y = lry
	*/

	msg_printf(DBG, 
		"ulx: %d, llx: %d, uly: %d, ury: %d, lrx: %d, urx: %d, lry: %d, lly:%d\n",
		ulx, llx, uly, ury, lrx, urx, lry, lly);

	msg_printf(DBG, "max_r: %d, max_g: %d, max_b: %d, max_a: %d\n",
		max_r, max_g, max_b, max_a);
 
	max_z = mid_z = min_z = max_a = 0;

	fl_max_r = (float)max_r/100; fl_max_g = (float)max_g/100;
        fl_max_b = (float)max_b/100; fl_max_a = (float)max_a/100;

        fl_mid_r = fl_max_r; fl_mid_g = fl_max_g;
        fl_mid_b = fl_max_b; fl_mid_a = fl_max_a;

        fl_min_r = fl_max_r; fl_min_g = fl_max_g;
        fl_min_b = fl_max_b; fl_min_a = fl_max_a;

	max_pt.coords.x = (float)ulx; max_pt.coords.y = (float)uly;
        max_pt.coords.z = (float)max_z; max_pt.coords.w = 1.0;

	max_pt.texture.x = max_pt.coords.x; 
	max_pt.texture.y = max_pt.coords.y; 
	max_pt.texture.z = max_pt.coords.z; 
	max_pt.texture.w = max_pt.coords.w; 

        max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = fl_max_r;
        max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = fl_max_g;
        max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = fl_max_b;
        max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = fl_max_a;

        mid_pt.coords.x = (float)llx; mid_pt.coords.y = (float)lly;
        mid_pt.coords.z = (float)mid_z; mid_pt.coords.w = 1.0;

	mid_pt.texture.x = mid_pt.coords.x; 
	mid_pt.texture.y = mid_pt.coords.y; 
	mid_pt.texture.z = mid_pt.coords.z; 
	mid_pt.texture.w = mid_pt.coords.w; 

        mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = fl_mid_r;
        mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = fl_mid_g;
        mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = fl_mid_b;
        mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = fl_mid_a;

        min_pt.coords.x = (float)lrx; min_pt.coords.y = (float)lry;
        min_pt.coords.z = (float)min_z; min_pt.coords.w = 1.0;

	min_pt.texture.x = min_pt.coords.x; 
	min_pt.texture.y = min_pt.coords.y; 
	min_pt.texture.z = min_pt.coords.z; 
	min_pt.texture.w = min_pt.coords.w; 

        min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = fl_min_r;
        min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = fl_min_g;
        min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = fl_min_b;
        min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = fl_min_a;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum,EIGHTEENBIT_MASK);
        /* clear regs */
        /* _mgras_rss_init(rssnum); */
        _draw_setup(0,0);

        /* set up needed registers */
        ppfillmode.bits.DitherEnab = 0;
        ppfillmode.bits.DitherEnabA = 0;
        ppfillmode.bits.ZEnab = 0;
        ppfillmode.bits.DrawBuffer = 0x1; /* buffer A */

        ppzmode.bits.Zfunc = PP_ZF_LESS;

        /* reset fillmode to turn off fast fill since not everyone wants it */
        fillmode.bits.FastFill = 0;
        fillmode.bits.RoundEN = 0;
        fillmode.bits.ZiterEN = 0;
        fillmode.bits.ShadeEN = 0;

        HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_ZMODE & 0x1ff)), 
		(ppzmode.val | 0x0ffffff0), 0xffffffff);

	if (rgb_mode == RGB121212) {
           HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)), 
		PP_CM_RGBA121212, 0xffffffff); 
           ppfillmode.bits.PixType = PP_FILLMODE_RGB121212;
	}
	else {
           HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)), 
		PP_CM_RGBA8888, 0xffffffff); 
           ppfillmode.bits.PixType = PP_FILLMODE_RGBA8888;
	}

	if (global_dbuf == PP_FILLMODE_DOVERA_OR_B) {
		ppfillmode.bits.PixType = PP_FILLMODE_CI8;
		ppfillmode.bits.DrawBuffer = PP_FILLMODE_DOVERA_OR_B;
	}

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE& 0x1ff)), 
		(ppfillmode.val | 0x0c000000), 0xffffffff);
        HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(FILLMODE), fillmode.val, 
		0xffffffff);

	/* tri_shade_en is written inside of FillTri, so reset it */
	tri_shade_en = 0;

	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, 0);

	/* right tri */
	max_pt.coords.x = (float)ulx; max_pt.coords.y = (float)uly;
        max_pt.coords.z = (float)max_z; max_pt.coords.w = 1.0;

	mid_pt.coords.x = (float)urx; mid_pt.coords.y = (float)ury;
        mid_pt.coords.z = (float)mid_z; mid_pt.coords.w = 1.0;

	min_pt.coords.x = (float)lrx; min_pt.coords.y = (float)lry;
        min_pt.coords.z = (float)min_z; min_pt.coords.w = 1.0;

	/* tri_shade_en is written inside of FillTri, so reset it */
	tri_shade_en = 0;

	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, 0);

	algorithm = 0;
	mode = 1;

#if 0
	fl_max_r = 0.0; fl_max_g = 1.0; fl_max_b = 0.0;
	exp_lo_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 0); 
	exp_hi_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 1); 

	fprintf(stderr, "exp_hi_half: 0x%x, exp_lo_half 0x%x\n", 
		exp_hi_half, exp_lo_half);

	fl_max_r = 0.0; fl_max_g = 0.0; fl_max_b = 1.0;
	exp_lo_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 0); 
	exp_hi_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 1); 

	fprintf(stderr, "exp_hi_half: 0x%x, exp_lo_half 0x%x\n", 
		exp_hi_half, exp_lo_half);

	fl_max_r = 0.5; fl_max_g = 0.5; fl_max_b = 0.5;
	exp_lo_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 0); 
	exp_hi_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 1); 

	fprintf(stderr, "exp_hi_half: 0x%x, exp_lo_half 0x%x\n", 
		exp_hi_half, exp_lo_half);

	fl_max_r = 0.1; fl_max_g = 0.3; fl_max_b = 0.4;
	exp_hi_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 1); 
	exp_lo_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 0); 

	
	fprintf(stderr, "exp_hi_half: 0x%x, exp_lo_half 0x%x\n", 
		exp_hi_half, exp_lo_half);

#endif

	if (do_compare) {
	exp_hi_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 1); 
	exp_lo_half = _rgb12_to_fb36((__uint32_t)(fl_max_r * 0xfff), (__uint32_t)(fl_max_g * 0xfff), (__uint32_t)(fl_max_b * 0xfff), 0); 


#ifdef MGRAS_DIAG_SIM
	fprintf(stderr, "exp_hi_half: 0x%x, exp_lo_half 0x%x\n", 
		exp_hi_half, exp_lo_half);
#endif

	for (y = lly; y < uly; y++) {
	   for (x = llx; x < lrx; x++) {
#ifdef NOTYET
		_xy_to_rdram(x, y, algorithm, mode, exp_hi_half, exp_lo_half,
			&actual_hi, &actual_lo, 0x240);
#endif /* NOTYET */
		_xy_to_rdram(x, y, algorithm, mode, exp_hi_half, exp_lo_half,
			&actual_hi, &actual_lo, drb_ptr);
		_convert_fb_to_comp(rgb_mode, actual_hi, actual_lo);
	   }
	}

	} /* do_compare */

}

/*
 * Draws a rectangle and can read the values back via pio
 *
 * Usage: -t [xstart ystart] -d [xend yend] -r[red] -g[green] -b[blue]
 *	-c (do the compare) -x (draw a textured rect with data  
 *		already in the tram -- decal mode for now)
 *	-m [optional mode, default = 1 (rgb12), 0=rgba8888)
 * 	-f [optional buffer, default = 1 color buf, 0x41 = overlay
 */
__uint32_t
mg0_rect(int argc, char ** argv)
{
	__uint32_t urx, ury, llx, lly;
	__uint32_t max_r, max_g, max_b, max_a;
	__uint32_t rssnum, bad_arg=0, rgb_mode = 1;
	__uint32_t do_compare = 0;

	/* set up some initial vals */
	max_r = 100; max_g = 0; max_b = 0; max_a = 0;
	llx = 0; lly = 0; urx = 4; ury = 4;
	global_dbuf = 1;
	
	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 't':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&llx);
                                        argc--; argv++;
                                        atob(&argv[1][0], (int*)&lly);
                                        argc--; argv++;
                                }
                                else {
				   msg_printf(SUM, "Need a space after -%c\n",
                                                argv[0][1]);
                                   bad_arg++;
				}
                                break;
                        case 'd':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&urx);
                                        argc--; argv++;
                                        atob(&argv[1][0], (int*)&ury);
                                        argc--; argv++;
                                }
                                else {
				   msg_printf(SUM, "Need a space after -%c\n",
                                                argv[0][1]);
                                   bad_arg++;
				}
                                break;
                        case 'r':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&max_r);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&max_r);
                                break;
                        case 'g':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&max_g);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&max_g);
                                break;
                        case 'b':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&max_b);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&max_b);
                                break;
                        case 'a':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&max_a);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&max_a);
                                break;
                        case 'x':
				tri_tex_en = 1; break;
			case 'c': do_compare = 1; break;
			case 'm': 
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rgb_mode);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rgb_mode);
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
				bad_arg++;
				break;
		}
        	argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "Usage: -t [xstart ystart] -d [xend yend] -r[red] -g[green] -b[blue] -c (do the compare) -m[optional rgb mode, default=1=rgb12, 0=rgba8888] -f [optional buffer, default = 1 color buf, 0x41 = overlay]\n");
		return (0);
	}

#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
	max_r = 100; max_g = 80; max_b = 70; max_a = 50;	
	urx = 4; ury = 4;
	llx = 0; lly = 0; 
#endif

	if (rgb_mode == 1)
		rgb_mode = RGB121212;
	else
		rgb_mode = RGBA8888;	
	_mg0_rect(llx,lly,urx,ury,max_r,max_g,max_b,max_a,do_compare, rgb_mode);

	tri_tex_en = 0;

	return(0);
}

/* dumps out the contents of the frame buffer in the given rectangular area */
int
mg_read_fb(int argc, char ** argv)
{
	__uint32_t uly, lrx, urx, ury, llx, lly, actual_hi, actual_lo;
	__uint32_t x, y, algorithm, mode, convert_mode;
	__uint32_t bad_arg=0;
	__uint32_t drb_ptr = mg_tinfo->DRBptrs.main;
	ppfillmodeu  ppfillMode = {0};

	/* set up some initial vals */
	llx = 0; lly = 0; urx = 4; ury = 4;
	mode = RGB121212;
	
	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 't':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&llx);
                                        argc--; argv++;
                                        atob(&argv[1][0], (int*)&lly);
                                        argc--; argv++;
                                }
                                else {
				   msg_printf(SUM, "Need a space after -%c\n",
                                                argv[0][1]);
                                   bad_arg++;
				}
                                break;
                        case 'd':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&urx);
                                        argc--; argv++;
                                        atob(&argv[1][0], (int*)&ury);
                                        argc--; argv++;
                                }
                                else {
				   msg_printf(SUM, "Need a space after -%c\n",
                                                argv[0][1]);
                                   bad_arg++;
				}
                                break;
                        case 'm':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&mode);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&mode);
				if ((mode < 1) || (mode > 8))
					bad_arg++;
                                break;
                        case 'r':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&drb_ptr);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&drb_ptr);
				if ((mode < 1) || (mode > 8))
					bad_arg++;
			default:
				bad_arg++;
				break;
		}
        	argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "Usage: -t [xstart ystart] -d [xend yend] -m[optional mode, default = %d]\n", RGB121212);
		msg_printf(SUM, "Mode: %d=RGB121212, %d=RGBA8888\n",
			RGB121212, RGBA8888);
		return (0);
	}

#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
	urx = 4; ury = 4;
	llx = 0; lly = 0; 
	mode = RGBA8888;
#endif

	uly = ury;
	lrx = urx;

	algorithm = 0;
	convert_mode = 2; /* read only */
	
	ppfillMode.bits.PixType = PP_FILLMODE_RGB121212;
	ppfillMode.bits.DrawBuffer = PP_FILLMODE_DBUFA;
	WRITE_RSS_REG(4, RSS_FILLCMD_PP1, ppfillMode.val, 0xffffffff);

	for (y = lly; y <= uly; y++) {
	   for (x = llx; x <= lrx; x++) {
		_xy_to_rdram(x, y, algorithm, convert_mode, 0, 0, 
			&actual_hi, &actual_lo, drb_ptr);
		_convert_fb_to_comp(mode, actual_hi, actual_lo);
	   }
	}

	return(0);
}

#ifndef IP28
/**************************************************************************
 * mg0_clean_disp
 *
 * This function first sets the PP1 to do logiccop=OR and dither disabled:
 * PP1.FillMode <= 0x1c004200. (within _mg0_block)
 *
 * Sets PP1.ColorMaskLSBsA to 0xffffffff.
 * Then a screen clear is done for the full 1344x1024 with Greenx = 2/255 and i
 * Red, Blue, and Alpha set to 0.
 *
 * Finally, the PP1.ColorMaskLSBsA is set to 0xfffbffff and the 
 * PP1.ColorMaskMSBs is set to 0x0.
 **************************************************************************/
 
void
mg0_clean_disp(void)
{
	__uint32_t buffer, xstart, xend, ystart, yend;
	float red_f, green_f, blue_f, alpha_f;

	buffer = PP_FILLMODE_DBUFA;
	red_f = blue_f = alpha_f = 0.0;
	green_f = (float)(2.0)/255;
	xstart = ystart = 0;
#ifdef NOTYET
        xend = 1344; yend = 1024;
#endif /* NOTYET */
        xend = XMAX; yend = YMAX;

	_draw_setup(DMA_PP1, 0);

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)), 
		0xffffffff, 0xffffffff);

	clean_disp = 1;
	_mg0_block(buffer, red_f, green_f, blue_f, alpha_f, xstart, ystart, 
		xend, yend);

#if 0
	/* set up the lines: x=383, x=384 */
	xloc = 383;
        max_pt.coords.x = (float)xloc; max_pt.coords.y = (float)1024.0;
        max_pt.coords.z = 0.0; max_pt.coords.w = 1.0;

        max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = red_f;
        max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = green_f;
        max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = blue_f;
        max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = alpha_f;

        min_pt.coords.x = (float)xloc; min_pt.coords.y = (float)0.0;
        min_pt.coords.z = 0.0; min_pt.coords.w = 1.0;

        min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = red_f;
        min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = green_f;
        min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = blue_f;
        min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = alpha_f;

	_mg0_Line(&min_pt, &max_pt, 0, 0, 0, 0, 0, 0, 0, rssDmaPar);

	xloc = 384;
	max_pt.coords.x = (float)xloc; min_pt.coords.x = (float)xloc;

	_mg0_Line(&min_pt, &max_pt, 0, 0, 0, 0, 0, 0, 0, rssDmaPar);
#endif

	clean_disp = 0;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)), 
		0xfffbffff, 0xffffffff);

}

#endif

#ifndef IP28
/**************************************************************************
 * mg0_clear_fbgone
 *
 * Sets PP1.ColorMaskLSBsA to 0xffffffff.
 * Then a screen clear is done for the full 1344x1024 with Greenx = 2/255 and i
 * Red, Blue, and Alpha set to 0.
 *
 * Finally, the PP1.ColorMaskLSBsA is set to 0xfffbffff and the 
 * PP1.ColorMaskMSBs is set to 0x0.
 **************************************************************************/
 
void
mg0_clear_fbgone()
{
	__uint32_t buffer, xstart, xend, ystart, yend;
	float red_f, green_f, blue_f, alpha_f;

	buffer = PP_FILLMODE_DBUFA;
	red_f = blue_f = alpha_f = 0.0;
	green_f = (float)(2.0)/255;
	xstart = ystart = 0;
#ifdef NOTYET
        xend = 1344; yend = 1024;
#endif /* NOTYET */
        xend = XMAX; yend = YMAX;

	_draw_setup(DMA_PP1, 0);

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)), 
		0xffffffff, 0xffffffff);

	clean_disp = 2;
	_mg0_block(buffer, red_f, green_f, blue_f, alpha_f, xstart, ystart, 
		xend, yend);
	clean_disp = 0;

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)), 
		0xfffbffff, 0xffffffff);
}

#endif

/*
 * Various dac patterns. If no pattern specified, run all patterns.
 *
 * Pattern 0: Gray scale, 1 pixel wide
 * Pattern 1: Checkerboard, white background, black lines
 * Pattern 2: 10-pixel wide color bars of varying rgb values, 0, 0x55, 0xaa
 * Pattern 3: Red scale, 1 pixel wide
 * Pattern 4: Green scale, 1 pixel wide
 * Pattern 5: Blue scale, 1 pixel wide
 */
int
mg_dac_pats(int argc, char ** argv)
{
	__uint32_t i, x, y, buf, bad_arg = 0, xstart, ystart, xend, yend;
	__uint32_t width, rssnum, pat = 100;
	float red_float, green_float, blue_float, alpha_float;

	/* default */
	fpusetup();
	buf = PP_FILLMODE_DBUFA;
	xstart = ystart = 0;
#ifdef NOTYET
	xend = 1280; yend = 1024;
#endif /* NOTYET */
    xend = XMAX; yend = YMAX;

	if (XMAX == 1344)
		xend = 1280;
	else
	if (XMAX == 2112)
		xend = 1600;

	rssnum = 4; /* all rss's */
	dith_off = 1;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'p':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&pat);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&pat);
                                break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
        }

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "Usage -p[optional pattern, 0..5]\n");
		return (0);
	}

#ifdef MGRAS_DIAG_SIM
	xend = 50; yend = 50;
	xstart = 0; ystart = 0;
#endif

	HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum,EIGHTEENBIT_MASK);

	alpha_float = 1.0;

	_draw_setup(0, 0);

#if 0
	/* pattern 0 - gray scale */
	if ((pat == 0) || (pat == 100)) {
	   dac_pat = 0;
	   width = 1;
	   for (x = 0; x < xend; x+=width) {
		red_float=green_float=blue_float = (float)((x/width) * 1.0)/256;
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			x, ystart, x+width-1, yend);
	   }
	   dac_pat = 0x99999999;
	}
#endif
	/* pattern 0, 3, 4, or 5 color scale */
	if ((pat == 0)||(pat == 3)||(pat == 4) || (pat == 5) || (pat == 100)) {
	   dac_pat = pat;
	   width = 1;
	   for (i = 0; i < xend; i+=256) {
	      for (x = 0; x < 256; x+=width) {
		red_float=green_float=blue_float = (float)((x/width) * 1.0)/256;
		switch (pat) {
			case 0: /* gray */ break;
			case 3: /* red */ green_float = blue_float = 0.0;break;
			case 4: /* green */ red_float = blue_float = 0.0;break;
			case 5: /* blue */ green_float = red_float = 0.0;break;
		}
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			x+i+1, ystart, x+i+width-1, yend);
	      }		
	   }

	   /* add borders */
   	   green_float = blue_float = red_float = 1.0; 
	   switch (pat) {
			case 0: /* gray */ break;
			case 3: /* red */ green_float = blue_float = 0.0;break;
			case 4: /* green */ red_float = blue_float = 0.0;break;
			case 5: /* blue */ green_float = red_float = 0.0;break;
	   }
	   dac_pat = 0x99999999;
	   _mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xstart, ystart, xstart, yend);
	   _mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xend-1, ystart, xend-1, yend);
	   _mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xstart, ystart, xend, ystart);
	   _mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xstart, yend-1, xend, yend-1);

	}
	/* pattern 1 - checkerboard */
	if ((pat == 1) || (pat == 100)) {

	   /* clear screen to white */
	   red_float = green_float = blue_float = 1.0;
	   _mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xstart, ystart, xend, yend);

	   /* vertical lines */
	   width = 10;
	   red_float = green_float = blue_float = 0.0;
	   for (x = 0; x < xend; x+=width) {
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			x, ystart, x, yend);
	   }

	   /* horizontal lines */
	   for (y = 0; y < yend; y+=width) {
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			xstart, y, xend, y);
	   }
	}
	/* pattern 2  - 0, 0xaa, 0x55 (adjusted due to no dither to be +1 in
	   the code so the display is correct */
	if ((pat == 2) || (pat == 100)) {
	   width = 10;
	   for (x = 0; x < xend; x+=width) {
		switch (x % 30) {
			case 0: red_float = 0;
				green_float = (float)(0x56 * 1.0)/256;
				blue_float = (float)(0xab * 1.0)/256;
				break;
			case 10:
				green_float = 0;
				blue_float = (float)(0x56 * 1.0)/256;
				red_float = (float)(0xab * 1.0)/256;
				break;
			case 20:
				blue_float = 0;
				red_float = (float)(0x56 * 1.0)/256;
				green_float = (float)(0xab * 1.0)/256;
				break;
		}
		msg_printf(DBG,"x:%d, red:0x%x\n", x, *(__uint32_t*)&red_float);
		_mg0_block(buf, red_float, green_float, blue_float, alpha_float,
			x, ystart, x+width-1, yend);
	   }
	}
	dith_off = 0; /* reset to turn dithering back on */

	return 0;
}

int
_load_aa_tables(void)
{
	__uint32_t i, c, cfifo_status, cfifo_val;
   /*
    ** Load AA tables -- from MG0, mg0_re4.c
    */
    
    cfifo_val = 32;
    WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

    __glMgrRE4Int_diag(DEVICE_ADDR, DEVICE_AA_COVERAGE);
    for (i = 0, c = 0xff; i < 0x40; i++, c -= 4) {
	if (i == 0x20) {
    		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
	}

	WRITE_RSS_REG(4, DEVICE_DATA, c, ~0x0);
    }

    WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

    for (i = 0, c = 0xff; i < 0x40; i++, c -= 4) {
	if (i == 0x20) {
    		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
	}

	WRITE_RSS_REG(4, DEVICE_DATA, c, ~0x0);
    }

    WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

    __glMgrRE4Int_diag(DEVICE_ADDR, DEVICE_AA_ANGLE);
    for (i = 0; i < 0x20; i++) {
	WRITE_RSS_REG(4, DEVICE_DATA, 0x100, ~0x0);
    }

    return 0;
}

/* return number of texels per line given the input parameters */
__uint32_t _do_numtex(__uint32_t bytes_per_line, __uint32_t format, __uint32_t datatype)
{
	__uint32_t tex_per_line, format_val, datatype_val;

	/* tex_per_line = (bytes_per_line) / (bytes_per_tex) 
	 * bytes_per_tex = (format_val x datatype_val) / 8
	*/

	switch (format) { /* # of components */
		case GL_LUMINANCE: 		format_val = 1; break;
		case GL_LUMINANCE_ALPHA: 	format_val = 2; break;
		case GL_RGB: 			format_val = 3; break;
		case GL_RGBA: 			format_val = 4; break;
	}

	switch (datatype) {  /* # of bits per component */
		case GL_UNSIGNED_SHORT:	datatype_val = 16;break;
		case GL_UNSIGNED_BYTE:	datatype_val = 8; break;

		/* 4444 must be dma'd as 8-bit data except for rgba */
		case GL_UNSIGNED_SHORT_4_4_4_4_EXT:	
			if (format == GL_RGBA)
				datatype_val = 4;
			else
				datatype_val = 8;
			break;
	}

	tex_per_line = (bytes_per_line) / ((format_val * datatype_val)/8);

	msg_printf(DBG, "tex_per_line: %d\n", tex_per_line);

	return(tex_per_line);
}

int
_send_data( __uint32_t height, __uint32_t bytes_per_line, __uint32_t test)
{
	__uint32_t tstart, pix_cmd, s, t, word_width, cfifo_status, inc;
	__uint32_t *data, *ogl_data, datatype, format, xsizet, offset;

	data = DMA_WRTile;
	ogl_data = DMA_RDTile;
	inc = 1;
	tstart = 0;

	msg_printf(DBG,"data addr: 0x%x, DMA_WRTile: 0x%x, ogl: 0x%x,RD:0x%x\n",
		data, DMA_WRTile, ogl_data, DMA_RDTile);

	word_width = bytes_per_line/4;
	for (t = 0; t < height; t++) 
		for (s = 0; s < word_width; s++) 
			*(data + s + (t*word_width)) = (s + t*word_width);
			

	/* create the input data */
	switch (test) {
		case TEX_8KHIGH_TEST: 
		case TEX_8KWIDE_TEST: 
		case TE1_LUT_TEST:
			num_tram_comps = 1; 
			format = GL_LUMINANCE;
			datatype = GL_UNSIGNED_BYTE;
			break;  

		case TEX_DETAIL_TEST: 
		case (TEX_DETAIL_TEST | 0x100): 
		case TEX_MAG_TEST: 
		case (TEX_MAG_TEST | 0x100): 
		case (TEX_MAG_TEST | 0x200):
		case TEX_PERSP_TEST: 
		case (TEX_PERSP_TEST | 0x100): 
		case (TEX_PERSP_TEST | 0x200):
		case TEX_LINEGL_TEST: 
		case (TEX_LINEGL_TEST | 0x100): 
		case (TEX_LINEGL_TEST | 0x200):
		case TEX_SCISTRI_TEST: 
		case (TEX_SCISTRI_TEST | 0x100): 
		case (TEX_SCISTRI_TEST | 0x200):
		case (TEX_SCISTRI_TEST | 0x300): 
			num_tram_comps = 2; 
			format = GL_LUMINANCE_ALPHA;
			datatype = GL_UNSIGNED_BYTE;
			break;  

		case TEX_LOAD_TEST: 
		case (TEX_LOAD_TEST | 0x100): 
		case (TEX_LOAD_TEST | 0x200): 
		case (TEX_LOAD_TEST | 0x300): 
			num_tram_comps = 2; 
			format = GL_LUMINANCE_ALPHA;
			datatype = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
			break;  

		case (TEX_LOAD_TEST | 0x400): 
		case (TEX_LOAD_TEST | 0x500): 
		case (TEX_LOAD_TEST | 0x600):
		case (TEX_LOAD_TEST | 0x700): 
			num_tram_comps = 4; 
			format = GL_RGBA;
			datatype = GL_UNSIGNED_BYTE;
			break;  

		case (TEX_LOAD_TEST | 0x800): 
		case (TEX_LOAD_TEST | 0x900):
		case (TEX_LOAD_TEST | 0xa00):
		case (TEX_LOAD_TEST | 0xb00): 
		case (TEX_LOAD_TEST | 0xc00):
			num_tram_comps = 2; 
			format = GL_LUMINANCE_ALPHA;
			datatype = GL_UNSIGNED_BYTE;
			break;  

		case TEX_LUT4D_TEST: 
		case (TEX_LUT4D_TEST | 0x100): 
		case (TEX_LUT4D_TEST | 0x200):
		case (TEX_LUT4D_TEST | 0x300): 
			num_tram_comps = 4; 
			format = GL_RGBA;
			datatype = GL_UNSIGNED_SHORT;
			break;  

		case (TEX_LUT4D_TEST | 0x400):
			/* last dma for 1 RSS, even lines for 2 RSS */
			num_tram_comps = 4; 
			format = GL_RGBA;
			datatype = GL_UNSIGNED_BYTE;
			if (numRE4s == 2)
				inc = 2;
			break;  
		case (TEX_LUT4D_TEST | 0x1400):
			/* odd lines  for 2 RSS */
			num_tram_comps = 4; 
			format = GL_RGBA;
			datatype = GL_UNSIGNED_BYTE;
			inc = 2;
			tstart = 1; /* odd lines */
			break;  

		case TEX_MDDMA_TEST: 
		case (TEX_MDDMA_TEST | 0x100): 
		case (TEX_MDDMA_TEST | 0x200):
		case (TEX_MDDMA_TEST | 0x300):
			num_tram_comps = 4; 
			format = GL_RGBA;
			datatype = GL_UNSIGNED_SHORT;
			break;  

		case (TEX_MDDMA_TEST | 0x400): 
			/* last dma for 1 RSS, even lines for 2 RSS */
			num_tram_comps = 3; 
			format = GL_RGB;
			datatype = GL_UNSIGNED_BYTE;
			if (numRE4s == 2)
				inc = 2;
			break;  
		case (TEX_MDDMA_TEST | 0x1400): 
			/* odd lines  for 2 RSS */
			num_tram_comps = 3; 
			format = GL_RGB;
			datatype = GL_UNSIGNED_BYTE;
			inc = 2;
			tstart = 1; /* odd lines */
			break;  

		case TEX_TEX1D_TEST: 
			/* data = tex1d_array; */
			num_tram_comps = 2;
			format = GL_LUMINANCE_ALPHA;
			datatype = GL_UNSIGNED_BYTE;
			break;

		case TEX_TEX3D_TEST: 
		case (TEX_TEX3D_TEST | 0x100): 
		case (TEX_TEX3D_TEST | 0x200):
		case (TEX_TEX3D_TEST | 0x300): 
			num_tram_comps = 1;
			format = GL_LUMINANCE;
			datatype = GL_UNSIGNED_SHORT;
			break;

		case TEX_TEREV_TEST: 
		case (TEX_TEREV_TEST | 0x100): 
		case (TEX_TEREV_TEST | 0x200):
		case (TEX_TEREV_TEST | 0x300): 
		case (TEX_TEREV_TEST | 0x400): 
			num_tram_comps = 3;
			format = GL_RGB;
			datatype = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
			break;
	}

	/* create an input data array which opengl likes */
	xsizet = _do_numtex(bytes_per_line, format, datatype);	
	offset = 0; 
	msg_printf(DBG, "format: 0x%x, datatype: 0x%x\n", format, datatype);
	mkImage(datatype, format, xsizet, height, ogl_data, offset, test);

#if 0
	for (t = 0; t < 4; t++)
		msg_printf(VRB, "ogl_data[%d]: 0x%x, addr:0x%x\n", 
			t, *(ogl_data + t), (ogl_data + t));
	msg_printf(VRB, "\n");
#endif


	/* take the opengl-based input array and generate a dma-able array */
	mkDMAbuf(datatype, format, xsizet, height, ogl_data, data, test);	

	if ((test & 0xff) == TEX_TEREV_TEST) { /* send all 0x0f000f00 */
		for (t = 0; t < 5700; t++)
			*(data + t) = 0x0f000f00;
	}
#if 0
	msg_printf(VRB, "tstart: %d, inc: %d\n", tstart, inc);

	for (t = 0; t < 8; t++)
		msg_printf(VRB, "data[%d]: 0x%x, addr:0x%x\n", 
			t, *(data + t), (data + t));
	msg_printf(VRB, "\n");
#endif

	pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 2, bytes_per_line);

	for (t = tstart; t < height; t+=inc) {

	   HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);

	   for (s = 0; s < word_width; s+=2) {	
		if ((s % 0x20) == 0) {
	   	   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
    	   	   if (cfifo_status == CFIFO_TIME_OUT) {
        			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
        			return (-1);
    		   }
		}

		HQ3_PIO_WR64(CFIFO1_ADDR, *(data), *(data + 1), ~0x0);
		data++; data++;
	   }
	}

	if ((inc == 1) /* normal case */ || ((inc == 2) && (tstart==1)))
	_mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_BURST);
	
	/* readback ?? */
	return 0;
}

/* common setup code from the verif tex suite */
int
_verif_setup(void)
{
	__uint32_t i, cfifo_val, cfifo_status;
	__paddr_t	pgMask = ~0x0;

	pgMask = PAGE_MASK(pgMask);

DMA_WRTile = (__uint32_t *) 
	((((__paddr_t)DMA_WRTile_notaligned) + 0x1000) & pgMask);
   msg_printf(DBG, "DMA_WRTile 0x%x\n", DMA_WRTile);
DMA_RDTile = (__uint32_t *) 
	((((__paddr_t)DMA_RDTile_notaligned) + 0x1000) & pgMask);
   msg_printf(DBG, "DMA_RDTile 0x%x\n", DMA_RDTile);

        cfifo_val = 32;

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00004000);
hq_HQ_RE_WrReg_0(0x161,0x00000000,0x00000200);
hq_HQ_RE_WrReg_0(0x16a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00005000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x112,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x113,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x114,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x115,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x144,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x145,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x116,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x147,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x148,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x149,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x14a,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x14b,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x14c,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x14d,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x14e,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x14f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x156,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x157,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00040000);
hq_HQ_RE_WrReg_0(0x15a,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x15b,0x00000000,0x000f0000);
hq_HQ_RE_WrReg_0(0x192,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x182,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x187,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x188,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x189,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x160,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x161,0x00000000,0x0c004203);
hq_HQ_RE_WrReg_0(0x162,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x163,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x164,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000001);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x58840000);
hq_HQ_RE_WrReg_0(0x166,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x167,0x00000000,0x0000ffff);
hq_HQ_RE_WrReg_0(0x168,0x00000000,0x0ffffff1);
hq_HQ_RE_WrReg_0(0x169,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x16a,0x00000000,0x00000000);
#if 0
hq_HQ_RE_WrReg_0(0x16d,0x00000000,0x000c8240);
hq_HQ_RE_WrReg_0(0x16e,0x00000000,0x0000031e);
#endif
hq_HQ_RE_WrReg_2(0x268,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x40000000);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x03084111);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x50000000);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004111);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0xb0000000);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000ff);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000fb);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000f7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000f3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000ef);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000eb);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000e7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000e3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000df);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000db);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000d7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000d3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000cf);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000cb);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000c7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000c3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000bf);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000bb);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000b7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000b3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000af);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000ab);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000a7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000a3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000009f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000009b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000097);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000093);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000008f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000008b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000087);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000083);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000007f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000007b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000077);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000073);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000006f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000006b);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000067);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000063);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000005f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000005b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000057);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000053);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000004f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000004b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000047);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000043);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000003b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000037);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000033);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000002f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000002b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000027);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000023);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000001f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000001b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000017);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000013);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000000f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000ff);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000fb);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000f7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000f3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000ef);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000eb);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000e7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000e3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000df);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000db);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000d7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000d3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000cf);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000cb);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000c7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000c3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000bf);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000bb);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000b7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000b3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000af);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000ab);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000a7);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000a3);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000009f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000009b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000097);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000093);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000008f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000008b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000087);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000083);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000007f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000007b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000077);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000073);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000006f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000006b);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000067);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000063);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000005f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000005b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000057);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000053);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000004f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000004b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000047);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000043);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000003b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000037);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000033);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000002f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000002b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000027);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000023);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000001f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000001b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000017);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000013);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000000f);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0xb0000080);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

for (i = 0; i < 28; i++) {
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);
}

if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

	return 0;
}

#ifdef MFG_USED
void
mg_nackflags(void)
{
  __int32_t  re, pp, select, nackflags;

  for (re = 0; re < numRE4s; re++) {
    for (pp = 0; pp < 2; pp++) {
      /* Clean out the BFIFO */
      mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

      /* Setup the PP */
      select = MGRAS_XMAP_SELECT_PP1((re << 1) | pp);
      mgras_xmapSetPP1Select(mgbase, select);

      /* Get the data */
      mgras_xmapSetAddr(mgbase, 0x4);
      mgras_xmapGetRE_RAC(mgbase, nackflags);

      /* print it */
      msg_printf(VRB, "RE%d PP%d: 0x%08x\n", re, pp, nackflags);
    }
  }
}
#endif /* MFG_USED */

void
MgrasSyncPPRDRAMs_repair(register mgras_hw *base, mgras_info *info)
{
    unsigned int re4_id, pp1_id, rdram_id;
  
    for (re4_id = 0; re4_id < info->NumREs; re4_id++) {
	for (pp1_id = 0; pp1_id < 2; pp1_id++) {
	    for (rdram_id = 0; rdram_id < 3; rdram_id++) {
		MgrasSyncPPRDRAM_repair(base, re4_id, pp1_id, rdram_id);
	    }
	}
    }
}

void
MgrasSyncPPRDRAM_repair(register mgras_hw *base, int re_id, int pp_id, int rdram_id)
{
  unsigned int curr_val, curr_field;
  unsigned int which_re, which_pp, which_ram;
  unsigned int row, dummy_data;
  
#define ALL_PP1_RACCONTROLS  (MGRAS_RE4_DEVICE_PP1_BCAST|MGRAS_PP1_RACCONTROL)
#define ALL_RDRAMS      (MGRAS_RE4_DEVICE_PP1_BCAST|MGRAS_PP1_RDRAM_BCAST_REG)
#define ALL_RDRAM_DELAYS (ALL_RDRAMS|MGRAS_RDRAM_DELAY)
#define ALL_RDRAM_DEVIDS (ALL_RDRAMS|MGRAS_RDRAM_DEVICEID)
#define ALL_RDRAM_MODES  (ALL_RDRAMS|MGRAS_RDRAM_MODE)
#define ALL_RDRAM_MEMS   (MGRAS_RE4_DEVICE_PP1_BCAST|MGRAS_PP1_RDRAM_BCAST_MEM)
  
  /* talk to all of the REs at once */
  mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);
  while (!(base->status & 0x2))
	/* spin until RSS's are idle */ ;
  mgras_re4Set(base, config.val, CFG_BCAST); /**/
  
  /* Set up the PP1s for PIO */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, MGRAS_PP1_RACCONTROL_SYNC);
  mgras_re4Set(base, PixCmd, MGRAS_PP1_PIXCMD_SYNC);
  mgras_re4Set(base, pp1fillmode.val, MGRAS_PP1_FILLMODE_SYNC);
  mgras_re4Set(base, pp1winmode.val, MGRAS_PP1_WINMODE_SYNC);
  mgras_re4Set(base, TAGmode.val, MGRAS_PP1_TAGMODE_SYNC);
  mgras_re4Set(base, DRBpointers.val, MGRAS_PP1_DRBPTRS_SYNC);
  mgras_re4Set(base, DRBsize.val, MGRAS_PP1_DRBSIZE_SYNC);
  
  /* Broadcast to all PP1s */
  mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);
  mgras_xmapSetPP1Select(base, MGRAS_XMAP_SELECT_BROADCAST);
  mgras_xmapSetAddr(base, MGRAS_XMAP_RETRYTIME_ADDR);
  mgras_xmapSetRetryTime(base, MGRAS_PP1_RETRYTIME_SYNC);
  
  /* NOTE: there are a LOT of non-symbolic constants in the following
     code.  These are all taken from the PP1 spec.  So are the comments.
     */
  mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);
  DELAY(MGRAS_PP1_TPAUSE);	                            /* core settle */
  DELAY(MGRAS_PP1_TLOCKRESET);				    /* clocks settle */
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x80244);   /* ForceBE=1 */
  mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);
  DELAY(MGRAS_PP1_TMODEARMAX);				    /* reset RAMs */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* ForceBE=0 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x40244);   /* preinit=1 */
  mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);
  DELAY(MGRAS_PP1_TINTRESET);				    /* rst internal */
  DELAY(MGRAS_PP1_TLOCKRESET);
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x40344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x40244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_RDRAM_DELAYS, 0x00000800);   /* preinit delay */
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_RDRAM_DELAYS, 0x08183828);   /* postinit delay */
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_RDRAM_DEVIDS,    0x0    );
  
  if (MGRAS_PP1_BUSFREQ == MGRAS_450MHz)
    curr_val = MGRAS_PP1_CURRVAL450;
  else
    curr_val = MGRAS_PP1_CURRVAL500;
  
  curr_field = (((curr_val & 0x20) ? (0x1 << 15) : 0) |
		((curr_val & 0x10) ? (0x1 << 23) : 0) |
		((curr_val & 0x08) ? (0x1 << 31) : 0) |
		((curr_val & 0x04) ? (0x1 << 14) : 0) |
		((curr_val & 0x02) ? (0x1 << 22) : 0) |
		((curr_val & 0x01) ? (0x1 << 30) : 0) |
		0x000000c2);
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);
  for (row = 0; row < 64; row++) { /* XXX Hack for PP1 HW problem */
    mgras_re4SetDevice(base, ALL_RDRAM_MODES, curr_field);
    DELAY(1000);
  }
  
  /* individually set RasInterval for each RDRAM */
  mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);
  which_re = re_id;
  { 
    mgras_re4Set(base, config.val, which_re);
    which_pp = pp_id;
    { 
      unsigned int pp_addr = ((which_pp == 0) ? MGRAS_RE4_DEVICE_PP1_A :
			      MGRAS_RE4_DEVICE_PP1_B);
      which_ram = rdram_id;
      { 
	unsigned int rdram_addr = pp_addr | MGRAS_PP1_RDRAM_N_REG(which_ram);
	unsigned int rdram_mem_addr =pp_addr | MGRAS_PP1_RDRAM_N_MEM(which_ram);
	unsigned int rdram_mfr_id;
	mgras_re4SetDevice(base, pp_addr | MGRAS_PP1_RACCONTROL, 0x00344);
	mgras_re4SetDevice(base, pp_addr | MGRAS_PP1_RACCONTROL, 0x00244);
	mgras_re4GetDevice(base, rdram_addr | MGRAS_RDRAM_MANUFACTURER,
			   rdram_mfr_id);
	if (EXTRACT_BITS(rdram_mfr_id, 31, 16) == MGRAS_PP1_NEC) {
	  mgras_re4SetDevice(base, rdram_addr | MGRAS_RDRAM_RASINTERVAL,
			     MGRAS_RASINTERVAL_NEC);
	} 
	else {
	  mgras_re4SetDevice(base, rdram_addr | MGRAS_RDRAM_RASINTERVAL,
			     MGRAS_RASINTERVAL_TOSHIBA);
	}
	/* init 1 core at a time */
  	for (row = 0; row < 8; row++) {
    	   mgras_re4GetDevice(base, rdram_mem_addr + (2048*row), dummy_data);
  	}
      }
    }
  }

  /* talk to all REs again */
  mgras_re4Set(base, config.val, CFG_BCAST);
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x30244);

#if 0
  /* to init the RDRAM cores, read first 8 lines of each */
  for (row = 0; row < 8; row++) {
    mgras_re4GetDevice(base, ALL_RDRAM_MEMS + (2048*row), dummy_data);
  }
#endif
}

/* Replaces the dac_crc_rssX_ppX scripts 
 * Input: rss #, pp #, red, green, blue for 1rss and 2rss systems.
 * If we're checking a 1rss system, just enter dummy values for 2rss.
 * If we're checking a 2rss system, just enter dummy values for 1rss.
 */
__uint32_t
_mg_dac_crc_rss_pp(int rssnum, int ppnum, 
	int red_1rss, int green_1rss, int blue_1rss, 
	int red_2rss, int green_2rss, int blue_2rss)
{
	int r, p, errors = 0;

	msg_printf(VRB, "Checking RSS-%d, PP-%d\n", rssnum, ppnum);

	for (r = 0; r < numRE4s; r++) {
		for (p = 0; p < 2; p++) {
			if ((r == rssnum) && (p == ppnum)) {
				_mgras_disp_ctrl(1, ppnum, 1, rssnum, 1, 1);
			}
			else {
				_mgras_disp_ctrl(1, p, 1, r, 1, 0);
			} 
			us_delay(1000000);
		}
	}

	/* If we're checking RSS-1, we also need to turn on RSS-0 */
	if (rssnum == 1) {
		_mgras_disp_ctrl(1, ppnum, 1, (rssnum - 1), 1, 1);
	}


	if (numRE4s == 2) {
		errors = _mgras_crc_test(red_2rss, green_2rss, blue_2rss);	
	}
	else {
		errors = _mgras_crc_test(red_1rss, green_1rss, blue_1rss);	
	}
	
	msg_printf(VRB, "\nRSS-%d, PP-%d ", rssnum, ppnum);
	if (errors)
		msg_printf(VRB, "FAIL\n\n\n");
	else
		msg_printf(VRB, "PASS\n\n\n");

	return (errors);
}

mg_dac_crc_rss_pp(int argc, char ** argv)
{

	__uint32_t bad_arg, red_2rss, green_2rss, blue_2rss, red_1rss;
	__uint32_t green_1rss, blue_1rss, errors, rssnum, ppnum;

	errors = bad_arg = red_2rss = green_2rss = blue_2rss = red_1rss = green_1rss = blue_1rss = 0;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
                                if (argv[0][2]=='\0') {
                                        atob(&argv[1][0], (int*)&rssnum);
                                        argc--; argv++;
                                }
                                else
                                        atob(&argv[0][2], (int*)&rssnum);
                                break;
			case 'p':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum);
				break;
			case 'a':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&red_1rss);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&red_1rss);
				break;
			case 'b':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&green_1rss);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&green_1rss);
				break;
			case 'c':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&blue_1rss);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&blue_1rss);
				break;
			case 'x':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&red_2rss);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&red_2rss);
				break;
			case 'y':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&green_2rss);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&green_2rss);
				break;
			case 'z':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&blue_2rss);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&blue_2rss);
				break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,"Usage: -r[RSS number] -p[PP number] -a[Red-1RSS] -b[Green-1RSS] -c[Blue-1RSS] -x[Red-2RSS] -y[Green-2RSS] -z[Blue-2RSS]\n");
                return (0);
        }

	errors = _mg_dac_crc_rss_pp(rssnum, ppnum,red_1rss,green_1rss,blue_1rss,
		red_2rss, green_2rss, blue_2rss);

	return (errors);
}

void
mg_restore_pps()
{
	int ppnum, rssnum;

	for (rssnum = 0; rssnum < numRE4s; rssnum++) 
		for (ppnum = 0; ppnum < 2; ppnum++) {
			us_delay(1000000);
			_mgras_disp_ctrl(1, ppnum, 1, rssnum, 1, 1);
		}
}
