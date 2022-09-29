/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *      FileName: test_allstds.c
 *		  This is to generate 7 different test patterns in 8-bit and 
 *		  10-bit YUV 4:2:2, YUVA 4:4:4:4 and YUVA 4:2:2:4 formats in
 *		  NTSC or PAL timing.
 *		  NOTE: This is a compilation of 4 programs by Bob Williams:
 *			test_d525.c, test_d625.c, test_s525.c, test_s625.c
 *		    The top-level function was made callable, all file operations
 *			are replaced by array pointer operations. The pixel array
 *			size was reduced from full frame to conform to ide memory 
 *			size constraints.
 *
 *      $Revision: 1.4 $
 */

#include <sys/types.h>
#include "mgv_diag.h"
#include <math.h>

/*								Parameters for each tv standard          */
/*								CCIR_525, CCIR_625, SQ_525,   SQ_625   */
static double	samp_clk[]	= { 13.5, 13.5, 12.272727, 14.75 };
static int		blank_lvl[]	= { 0, 16, 0, 16};
static float	max_u[]		= { 54.0, 27.0, 54.0, 27.0 };
static float	max_v[]		= { 0.0, 34.0, 0.0, 34.0 };

/*  time from 50% falling sync to first pixel is 9.7/8.95/9.7/10.25 uS on RGB out */

static int		alignment = 0;
static float	multiply = 1.0;
static int		convert = 0;

extern int xsize[], ysize[];

/* luminance only circular zoneplate. OK for d525,d625,s525,s625 */

void mod_make_y_zoneplate(float *pyx, float *pux, float *pvx,unsigned char std)
{
        float *ppy, *ppu, *ppv;
        short x, y;

        float x_frac, y_frac, t, ty;
        float x_phase, x_pos, x_velo, y_phase, y_pos, y_velo, x_y;
        x_phase = y_phase = 0.0;
        x_pos = -(float)(xsize[std]/2.0);
        y_pos = -(float)(ysize[std]/2.0);
        x_velo = (float)xsize[std];
        y_velo = (float)ysize[std];
        x_y = 0.0;

        for (y = 0; y < ysize[std]; y++){
                ppy = pyx + (y * xsize[std]);
                ppu = pux + (y * xsize[std]);
                ppv = pvx + (y * xsize[std]);
                y_frac = (float)y / (float)ysize[std];
                ty = y_phase + y_frac * (y_pos + 0.5 * y_frac * y_velo);
                for(x = 0; x < xsize[std]; x++){
                        x_frac = (double)x / (double)(xsize[std]);
                        t = ty + x_phase +
                                x_frac * (x_pos + 0.5 * x_frac * x_velo) +
                                x_y * x_frac * y_frac;
                        *(ppy + x) = 109.5 * (1.0 + cos((2.0 * M_PI) * t)) +
                                16.0;
                        *(ppu + x) = 128.0;
                        *(ppv + x) = 128.0;
                }
        }
}

/* luminance and chrominance circular zoneplate . OK for d525,d625,s525,s625 */

void mod_make_yc_zoneplate(float *pyx, float *pux, float *pvx,unsigned char std)
{
        float *ppy, *ppu, *ppv;
        short x, y;

        float x_frac, y_frac, t, ty;
        float x_phase, x_pos, x_velo, y_phase, y_pos, y_velo, x_y;
        x_phase = y_phase = 0.0;
        x_pos = -(float)(xsize[std]/2.0);
        y_pos = -(float)(ysize[std]/2.0);
        x_velo = (float)xsize[std];
        y_velo = (float)ysize[std];
        x_y = 0.0;

        for (y = 0; y < ysize[std]; y++){
                ppy = pyx + (y * xsize[std]);
                y_frac = (float)y / (float)ysize[std];
                ty = y_phase + y_frac * (y_pos + 0.5 * y_frac * y_velo);
                for(x = 0; x < xsize[std]; x++){
                        x_frac = (double)x / (double)(xsize[std]);
                        t = ty + x_phase +
                                x_frac * (x_pos + 0.5 * x_frac * x_velo) +
                                x_y * x_frac * y_frac;
                        *(ppy + x) = 109.5 * (1.0 + cos((2.0 * M_PI) * t)) +
                                16.0;
                }
        }

        x_phase = y_phase = M_PI/2.0;
        x_pos = -(float)(xsize[std]/4.0);
        y_pos = -(float)(ysize[std]/4.0);
        x_velo = (float)(xsize[std]/2.0);
        y_velo = (float)(ysize[std]/2.0);
        for (y = 0; y < ysize[std]; y++){
                ppu = pux + (y * xsize[std]);
                y_frac = (float)y / (float)ysize[std];
                ty = y_phase + y_frac * (y_pos + 0.5 * y_frac * y_velo);
                for(x = 0; x < xsize[std]; x++){
                        x_frac = (double)x / (double)(xsize[std]);
                        t = ty + x_phase +
                                x_frac * (x_pos + 0.5 * x_frac * x_velo) +
                                x_y * x_frac * y_frac;
                        *(ppu + x) = 112.0 * cos((2.0 * M_PI) * t) + 128.0;
                }
        }

        x_phase = y_phase = -M_PI/2.0;
        for (y = 0; y < ysize[std]; y++){
                ppv = pvx + (y * xsize[std]);
                y_frac = (float)y / (float)ysize[std];
                ty = y_phase + y_frac * (y_pos + 0.5 * y_frac * y_velo);
                for(x = 0; x < xsize[std];x++){
                        x_frac = (double)x / (double)(xsize[std]);
                        t = ty + x_phase +
                                x_frac * (x_pos + 0.5 * x_frac * x_velo) +
                                x_y * x_frac * y_frac;
                        *(ppv + x) = 112.0 * cos((2.0 * M_PI) * t) + 128.0;
                }
        }
}


/* modulated ramp for d525 */

void make_mod_ramp0(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 22){
				*(ppy + x) = 0.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 29){ /* start of modulation */
				t = sin(M_PI * (float)(x-22.0) / 14.0);
				t = t * t;

				*(ppy + x) = 0.0;
				*(ppu + x) = 128.0 - max_u[std]* t;
				*(ppv + x) = 128.0;
			}
			else if (x < 45){

				*(ppy + x) = 0.0;
				*(ppu + x) = 128.0 - max_u[std];
				*(ppv + x) = 128.0;
			}
			else if (x < 670){ /* ramp */

				*(ppy + x) = ((x-45.0)*235.0)/625.0;
				*(ppu + x) = 128.0 - max_u[std];
				*(ppv + x) = 128.0;
			}
			else if (x < 688){ /* top */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 - max_u[std];
				*(ppv + x) = 128.0;
			}
			else if (x < 695){ /* modulation off */
				t = sin(M_PI * (float)(x-688.0) / 14.0);
				t = 1.0 - (t * t);

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 - max_u[std]* t;
				*(ppv + x) = 128.0;
			}
			else if (x < 714){ /*  white  */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 718){
				t = sin(M_PI * (float)(x-714.0) / 8.0);
				t = 1.0 - (t * t);

				*(ppy + x) = t * 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else { /* end */

				*(ppy + x) = 0.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}



/* modulated ramp for d625 */

void make_mod_ramp1(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 3){
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 9){ /* start of modulation */
				t = sin(M_PI * (float)(x-2) / 14.0);
				t = t * t;

				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0 + max_u[std]* t;
				*(ppv + x) = 128.0 + max_v[std]* t;
			}
			else if (x < 116){

				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0 + max_u[std];
				*(ppv + x) = 128.0 + max_v[std];
			}
			else if (x < 608){ /* ramp */

				*(ppy + x) = 16.0 + ((x-115.0)*219.0)/492.0;
				*(ppu + x) = 128.0 + max_u[std];
				*(ppv + x) = 128.0 + max_v[std];
			}
			else if (x < 680){ /* top */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 + max_u[std];
				*(ppv + x) = 128.0 + max_v[std];
			}
			else if (x < 686){ /* modulation off */
				t = sin(M_PI * (float)(x-679.0) / 14.0);
				t = 1.0 - (t * t);

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 + max_u[std]* t;
				*(ppv + x) = 128.0 + max_v[std]* t;
			}
			else if (x < 716){ /*  white  */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 719){
				t = sin(M_PI * (float)(x-715.0) / 8.0);
				t = 1.0 - (t * t);

				*(ppy + x) = 16.0 + t * 219.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else { /* end */

				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}


/* modulated ramp for s525 */

void make_mod_ramp2(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 19){
				*(ppy + x) = 0.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 26){ /* start of modulation */
				t = sin(M_PI * (float)(x-19) / 14.0);
				t = t * t;

				*(ppy + x) = 0.0;
				*(ppu + x) = 128.0 - max_u[std]* t;
				*(ppv + x) = 128.0;
			}
			else if (x < 40){

				*(ppy + x) = 0;
				*(ppu + x) = 128.0 - max_u[std];
				*(ppv + x) = 128.0; 
			}
			else if (x < 596){ /* ramp */

				*(ppy + x) = ((x-40.0)*235.0)/556.0;
				*(ppu + x) = 128.0 - max_u[std];
				*(ppv + x) = 128.0; 
			}
			else if (x < 612){ /* top */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 - max_u[std];
				*(ppv + x) = 128.0;
			}
			else if (x < 619){ /* modulation off */
				t = sin(M_PI * (float)(x-612) / 14.0);
				t = 1.0 - (t * t);

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 - max_u[std]* t;
				*(ppv + x) = 128.0;
			}
			else if (x < 635){ /*  white  */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 639){
				t = sin(M_PI * (float)(x-635) / 8.0);
				t = 1.0 - (t * t);

				*(ppy + x) = t * 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else { /* end */

				*(ppy + x) = 0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}



/* modulated ramp for d625 */

void make_mod_ramp3(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 3){
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 9){ /* start of modulation */
				t = sin(M_PI * (float)(x-2) / 14.0);
				t = t * t;

				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0 + max_u[std]* t;
				*(ppv + x) = 128.0 + max_v[std]* t;
			}
			else if (x < 124){

				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0 + max_u[std];
				*(ppv + x) = 128.0 + max_v[std];
			}
			else if (x < 648){ /* ramp */

				*(ppy + x) = 16.0 + ((x-123)*219.0)/524.0;
				*(ppu + x) = 128.0 + max_u[std];
				*(ppv + x) = 128.0 + max_v[std];
			}
			else if (x < 725){ /* top */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 + max_u[std];
				*(ppv + x) = 128.0 + max_v[std];
			}
			else if (x < 731){ /* modulation off */
				t = sin(M_PI * (float)(x-724) / 14.0);
				t = 1.0 - (t * t);

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0 + max_u[std]* t;
				*(ppv + x) = 128.0 + max_v[std]* t;
			}
			else if (x < 764){ /*  white  */

				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 767){
				t = sin(M_PI * (float)(x-763) / 8.0);
				t = 1.0 - (t * t);

				*(ppy + x) = 16.0 + t * 219.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else { /* end */

				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}



/* color bars */

void make_cb75_0(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 3){
				t = sin(M_PI * (float)(x+1) / 8.0);
				t = t * t;
				*(ppy + x) = t * 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 114){ /* 100% white */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 117){
				t = sin(M_PI * (float)(x-113) / 8.0);
				t = t * t;
				*(ppy + x) = 235.0 - t * 73.47;
				*(ppu + x) = 128.0 - t * 84.0;
				*(ppv + x) = 128.0 + t * 13.66;
			}
			else if (x < 214){ /* 75% yellow */
				*(ppy + x) = 161.53;
				*(ppu + x) = 44.0;
				*(ppv + x) = 141.66;
			}
			else if (x < 217){
				t = sin(M_PI * (float)(x-213) / 8.0);
				t = t * t;
				*(ppy + x) = 161.53 - t * 30.39;
				*(ppu + x) = 44.0 + t * 112.35;
				*(ppv + x) = 141.66 - t * 97.66;
			}
			else if (x < 316){ /* 75% cyan */
				*(ppy + x) = 131.14;
				*(ppu + x) = 156.35;
				*(ppv + x) = 44.0;
			}
			else if (x < 319){
				t = sin(M_PI * (float)(x-315) / 8.0);
				t = t * t;
				*(ppy + x) = 131.14 - t * 18.73;
				*(ppu + x) = 156.35 - t * 84.0;
				*(ppv + x) = 44.0 + t * 13.66;
			}
			else if (x < 417){ /* 75% green */
				*(ppy + x) = 112.41;
				*(ppu + x) = 72.35;
				*(ppv + x) = 57.66;
			}
			else if (x < 420){
				t = sin(M_PI * (float)(x-416) / 8.0);
				t = t * t;
				*(ppy + x) = 112.41 - t * 28.57;
				*(ppu + x) = 72.35 + t * 111.30;
				*(ppv + x) = 57.66 + t * 140.68;
			}
			else if (x < 518){ /* 75% magenta */
				*(ppy + x) = 83.84;
				*(ppu + x) = 183.65;
				*(ppv + x) = 198.34;
			}
			else if (x < 521){
				t = sin(M_PI * (float)(x-517) / 8.0);
				t = t * t;
				*(ppy + x) = 83.84 - t * 18.73;
				*(ppu + x) = 183.65 - t * 84.0;
				*(ppv + x) = 198.34 + t * 13.66;
			}
			else if (x < 619){ /* 75% red */
				*(ppy + x) = 65.11;
				*(ppu + x) = 99.65;
				*(ppv + x) = 212.0;
			}
			else if (x < 622){
				t = sin(M_PI * (float)(x-618) / 8.0);
				t = t * t;
				*(ppy + x) = 65.11 - t * 30.39;
				*(ppu + x) = 99.65 + t * 112.35;
				*(ppv + x) = 212.0 - t * 97.66;
			}
			else if (x < 716){ /* 75% blue */
				*(ppy + x) = 34.72;
				*(ppu + x) = 212.0;
				*(ppv + x) = 114.34;
			}
			else if (x < 719){
				t = sin(M_PI * (float)(x-715) / 8.0);
				t = t * t;
				*(ppy + x) = 34.72 - t * 18.72;
				*(ppu + x) = 212.0 - t * 84.0;
				*(ppv + x) = 114.34 + t * 13.66;
			}
			else { /* black */
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}



/* color bars */

void make_cb75_1(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 7){
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 10) {
				t = sin(M_PI * (float)(x-6.0) / 8.0);
				t = t * t;
				*(ppy + x) = 16.0 + t * 219.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 115){ /* 100% white */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 118){
				t = sin(M_PI * (float)(x-114.0) / 8.0);
				t = t * t;
				*(ppy + x) = 235.0 - t * 73.47;
				*(ppu + x) = 128.0 - t * 84.0;
				*(ppv + x) = 128.0 + t * 13.66;
			}
			else if (x < 204){ /* 75% yellow */
				t = sin(M_PI * (float)(x-200.0) / 8.0);
				t = t * t;
				*(ppy + x) = 161.53 - t * 30.39;
				*(ppu + x) = 44.0 + t * 112.35;
				*(ppv + x) = 141.66 - t * 97.66;
			}
			else if (x < 286){ /* 75% cyan */
				*(ppy + x) = 131.14;
				*(ppu + x) = 156.35;
				*(ppv + x) = 44.0;
			}
			else if (x < 289){
				t = sin(M_PI * (float)(x-285.0) / 8.0);
				t = t * t;
				*(ppy + x) = 131.14 - t * 18.73;
				*(ppu + x) = 156.35 - t * 84.0;
				*(ppv + x) = 44.0 + t * 13.66;
			}
			else if (x < 371){ /* 75% green */
				*(ppy + x) = 112.41;
				*(ppu + x) = 72.35;
				*(ppv + x) = 57.66;
			}
			else if (x < 374){
				t = sin(M_PI * (float)(x-370.0) / 8.0);
				t = t * t;
				*(ppy + x) = 112.41 - t * 28.57;
				*(ppu + x) = 72.35 + t * 111.30;
				*(ppv + x) = 57.66 + t * 140.68;
			}
			else if (x < 455){ /* 75% magenta */
				*(ppy + x) = 83.84;
				*(ppu + x) = 183.65;
				*(ppv + x) = 198.34;
			}
			else if (x < 458){
				t = sin(M_PI * (float)(x-454.0) / 8.0);
				t = t * t;
				*(ppy + x) = 83.84 - t * 18.73;
				*(ppu + x) = 183.65 - t * 84.0;
				*(ppv + x) = 198.34 + t * 13.66;
			}
			else if (x < 541){ /* 75% red */
				*(ppy + x) = 65.11;
				*(ppu + x) = 99.65;
				*(ppv + x) = 212.0;
			}
			else if (x < 544){
				t = sin(M_PI * (float)(x-540) / 8.0);
				t = t * t;
				*(ppy + x) = 65.11 - t * 30.39;
				*(ppu + x) = 99.65 + t * 112.35;
				*(ppv + x) = 212.0 - t * 97.66;
			}
			else if (x < 627){ /* 75% blue */
				*(ppy + x) = 34.72;
				*(ppu + x) = 212.0;
				*(ppv + x) = 114.34;
			}
			else if (x < 630){
				t = sin(M_PI * (float)(x-626) / 8.0);
				t = t * t;
				*(ppy + x) = 34.72 - t * 18.72;
				*(ppu + x) = 212.0 - t * 84.0;
				*(ppv + x) = 114.34 + t * 13.66;
			}
			else { /* black */
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}


/* color bars */

void make_cb75_2(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 3) {
				t = sin(M_PI * (float)(x+1) / 8.0);
				t = t * t;
				*(ppy + x) = t * 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 88){ /* 100% white */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 91){
				t = sin(M_PI * (float)(x-87) / 8.0);
				t = t * t;
				*(ppy + x) = 235.0 - t * 73.47;
				*(ppu + x) = 128.0 - t * 84.0;
				*(ppv + x) = 128.0 + t * 13.66;
			}
			else if (x < 180){ /* 75% yellow */
				*(ppy + x) = 161.53 -t *  30.39;
				*(ppu + x) = 44.0 + t * 112.35;
				*(ppv + x) = 141.66 - t * 97.66;
			}
			else if (x < 183){ 
				t = sin(M_PI * (float)(x-179.0) / 8.0);
				t = t * t;
				*(ppy + x) = 161.53 - t * 30.39;
				*(ppu + x) = 44.0 + t * 112.35;
				*(ppv + x) = 141.66 - t * 97.66;
			}
			else if (x < 272){ /* 75% cyan */
				*(ppy + x) = 131.14;
				*(ppu + x) = 156.35;
				*(ppv + x) = 44.0;
			}
			else if (x < 275){
				t = sin(M_PI * (float)(x-271.0) / 8.0);
				t = t * t;
				*(ppy + x) = 131.14 - t * 18.73;
				*(ppu + x) = 156.35 - t * 84.0;
				*(ppv + x) = 44.0 + t * 13.66;
			}
			else if (x < 364){ /* 75% green */
				*(ppy + x) = 112.41;
				*(ppu + x) = 72.35;
				*(ppv + x) = 57.66;
			}
			else if (x < 367){
				t = sin(M_PI * (float)(x-363) / 8.0);
				t = t * t;
				*(ppy + x) = 112.41 - t * 28.57;
				*(ppu + x) = 72.35 + t * 111.30;
				*(ppv + x) = 57.66 + t * 140.68;
			}
			else if (x < 456){ /* 75% magenta */
				*(ppy + x) = 83.84;
				*(ppu + x) = 183.65;
				*(ppv + x) = 198.34;
			}
			else if (x < 459){
				t = sin(M_PI * (float)(x-455.0) / 8.0);
				t = t * t;
				*(ppy + x) = 83.84 - t * 18.73;
				*(ppu + x) = 183.65 - t * 84.0;
				*(ppv + x) = 198.34 + t * 13.66;
			}
			else if (x < 548){ /* 75% red */
				*(ppy + x) = 65.11;
				*(ppu + x) = 99.65;
				*(ppv + x) = 212.0;
			}
			else if (x < 551){
				t = sin(M_PI * (float)(x-547) / 8.0);
				t = t * t;
				*(ppy + x) = 65.11 - t * 30.39;
				*(ppu + x) = 99.65 + t * 112.35;
				*(ppv + x) = 212.0 - t * 97.66;
			}
			else if (x < 636){ /* 75% blue */
				*(ppy + x) = 34.72;
				*(ppu + x) = 212.0;
				*(ppv + x) = 114.34;
			}
			else if (x < 639){
				t = sin(M_PI * (float)(x-635) / 8.0);
				t = t * t;
				*(ppy + x) = 34.72 - t * 18.72;
				*(ppu + x) = 212.0 - t * 84.0;
				*(ppv + x) = 114.34 + t * 13.66;
			}
			else { /* black */
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}




/* color bars */

void make_cb75_3(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x < 3){
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 6){
				t = sin(M_PI * (float)(x-2) / 8.0);
				t = t * t;
				*(ppy + x) = 16.0 - t * 219.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 104){ /* 100% white */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x < 107){
				t = sin(M_PI * (float)(x-103) / 8.0);
				t = t * t;
				*(ppy + x) = 235.0 - t * 73.47;
				*(ppu + x) = 128.0 - t * 84.0;
				*(ppv + x) = 128.0 + t * 13.66;
			}
			else if (x < 197){ /* 75% yellow */
				*(ppy + x) = 161.53;
				*(ppu + x) = 44.0;
				*(ppv + x) = 141.66;
			}
			else if (x < 200){
				t = sin(M_PI * (float)(x-196) / 8.0);
				t = t * t;
				*(ppy + x) = 161.53 - t * 30.39;
				*(ppu + x) = 44.0 + t * 112.35;
				*(ppv + x) = 141.66 - t * 97.66;
			}
			else if (x < 290){ /* 75% cyan */
				*(ppy + x) = 131.14;
				*(ppu + x) = 156.35;
				*(ppv + x) = 44.0;
			}
			else if (x < 293){
				t = sin(M_PI * (float)(x-289) / 8.0);
				t = t * t;
				*(ppy + x) = 131.14 - t * 18.73;
				*(ppu + x) = 156.35 - t * 84.0;
				*(ppv + x) = 44.0 + t * 13.66;
			}
			else if (x < 383){ /* 75% green */
				*(ppy + x) = 112.41;
				*(ppu + x) = 72.35;
				*(ppv + x) = 57.66;
			}
			else if (x < 386){
				t = sin(M_PI * (float)(x-382) / 8.0);
				t = t * t;
				*(ppy + x) = 112.41 - t * 28.57;
				*(ppu + x) = 72.35 + t * 111.30;
				*(ppv + x) = 57.66 + t * 140.68;
			}
			else if (x < 475){ /* 75% magenta */
				*(ppy + x) = 83.84;
				*(ppu + x) = 183.65;
				*(ppv + x) = 198.34;
			}
			else if (x < 478){
				t = sin(M_PI * (float)(x-474) / 8.0);
				t = t * t;
				*(ppy + x) = 83.84 - t * 18.73;
				*(ppu + x) = 183.65 - t * 84.0;
				*(ppv + x) = 198.34 + t * 13.66;
			}
			else if (x < 568){ /* 75% red */
				*(ppy + x) = 65.11;
				*(ppu + x) = 99.65;
				*(ppv + x) = 212.0;
			}
			else if (x < 571){
				t = sin(M_PI * (float)(x-618) / 8.0);
				t = t * t;
				*(ppy + x) = 65.11 - t * 30.39;
				*(ppu + x) = 99.65 + t * 112.35;
				*(ppv + x) = 212.0 - t * 97.66;
			}
			else if (x < 661){ /* 75% blue */
				*(ppy + x) = 34.72;
				*(ppu + x) = 212.0;
				*(ppv + x) = 114.34;
			}
			else if (x < 664){
				t = sin(M_PI * (float)(x-660) / 8.0);
				t = t * t;
				*(ppy + x) = 34.72 - t * 18.72;
				*(ppu + x) = 212.0 - t * 84.0;
				*(ppv + x) = 114.34 + t * 13.66;
			}
			else { /* black */
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}

/* 12.5T, 2T pulse and bar */

void make_pulse_bar0(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x > 40 && x < 84){
				/*  12.5T pulse  */
				t = sin(M_PI * ((float)(x-62)/(3.125 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = (float)(117.0-blank_lvl[std])*t
					+ (float)blank_lvl[std];
				*(ppu + x) = (67.0 * t) + 128.0;
				*(ppv + x) = (85.0 * t) + 128.0;
			}
			else if (x > 152 && x < 160){ /*  2T pulse  */
				t = sin(M_PI * ((float)(x-156) /
					(0.5 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = (float)(235.0-blank_lvl[std])*t
					+ (float)blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 224 && x < 232){ /*  +bar edge  */
				t = M_PI * ((float)(x-228) /
					(0.5 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = (float)(235-blank_lvl[std])*t
					+ (float)blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 381 && x < 389){  /*  -2T pulse  */
				t = sin(M_PI * ((float)(x-385) /
					(0.5 * samp_clk[std]) + 0.5));
				t = 1.0 - (t * t);
				*(ppy + x) = (float)(235-blank_lvl[std])*t
					+ (float)blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x >= 232 && x <= 555){ /*  white  */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 555 && x < 563){ /*  -bar edge  */
				t = M_PI * ((float)(x-559) /
					(0.5 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = (float)(235-blank_lvl[std]) * 
					(1.0 - t) + (float)(blank_lvl[std]);
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else {
				*(ppy + x) = blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}


/*  standard multiburst  */
/* 12.5T, 2T pulse and bar */

void make_pulse_bar1(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x > 78 && x < 132){
				/*  20T pulse  */
				t = sin(M_PI * ((float)(x-105)/(4.0 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = 109.5 * t + 16.0;
				*(ppu + x) = (63.0 * t) + 128.0;
				*(ppv + x) = (79.0 * t) + 128.0;
			}
			else if (x > 175 && x < 181){ /*  2T pulse  */
				t = sin(M_PI * ((float)(x-178) /
					(0.4 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = 219.0 * t + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 245 && x < 251){ /*  +bar edge  */
				t = M_PI * ((float)(x-248) /
					(0.4 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = 219.0 * t + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 419 && x < 425){  /*  -2T pulse  */
				t = sin(M_PI * ((float)(x-422) /
					(0.5 * samp_clk[std]) + 0.5));
				t = 1.0 - (t * t);
				*(ppy + x) = 219.0 * t + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x >= 251 && x <= 595){ /*  white  */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 595 && x < 601){ /*  -bar edge  */
				t = M_PI * ((float)(x-598) /
					(0.4 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = 219.0 * (1.0 - t) + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else {
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}



/*  standard multiburst  */
/* 12.5T, 2T pulse and bar */

void make_pulse_bar2(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x > 20 && x < 60){
				/*  12.5T pulse  */
				t = sin(M_PI * ((float)(x-40)/(3.125 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = (float) (117-blank_lvl[std]) * t 
							 + (float)blank_lvl[std];
				*(ppu + x) = (63.0 * t) + 128.0;
				*(ppv + x) = (79.0 * t) + 128.0;
			}
			else if (x > 122 && x < 130){ /*  2T pulse  */
				t = sin(M_PI * ((float)(x-126) /
					(0.5 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = (float)(235 - blank_lvl[std]) * t
							+ (float)blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 187 && x < 195){ /*  +bar edge  */
				t = sin(M_PI * ((float)(x-191) /
					(0.5 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = (float)(235 - blank_lvl[std]) * t
							+ (float)blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 330 && x < 338){  /*  -2T pulse  */
				t = sin(M_PI * ((float)(x-334) /
					(0.5 * samp_clk[std]) + 0.5));
				t = 1.0 - (t * t);
				*(ppy + x) = (float)(235-blank_lvl[std])* t
							+ (float)blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x >= 195 && x <= 489){ /*  white  */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 489 && x < 497){ /*  -bar edge  */
				t = M_PI * ((float)(x-493) /
					(0.5 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = (float) (235-blank_lvl[std]) *
							 (1.0 - t) + (float) blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else {
				*(ppy + x) = blank_lvl[std];
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}



/* 12.5T, 2T pulse and bar */

void make_pulse_bar3(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	float t;

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x++){
			if (x > 63 && x < 123){
				/*  20T pulse  */
				t = sin(M_PI * ((float)(x-93)/(4.0 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = 109.5 * t + 16.0;
				*(ppu + x) = 63.0 * t + 128.0;
				*(ppv + x) = 79.0 * t + 128.0;
			}
			else if (x > 172 && x < 178){ /*  2T pulse  */
				t = sin(M_PI * ((float)(x-175) /
					(0.4 * samp_clk[std]) + 0.5));
				t = t * t;
				*(ppy + x) = 219.0 * t + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 228 && x < 254){ /*  +bar edge  */
				t = M_PI * ((float)(x-251) /
					(0.4 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = 219.0 * t + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 439 && x < 445){  /*  -2T pulse  */
				t = sin(M_PI * ((float)(x-442) /
					(0.4 * samp_clk[std]) + 0.5));
				t = 1.0 - (t * t);
				*(ppy + x) = 219.0 * t + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x >= 254 && x <= 632){ /*  white  */
				*(ppy + x) = 235.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else if (x > 632 && x < 638){ /*  -bar edge  */
				t = M_PI * ((float)(x-635) /
					(0.4 * samp_clk[std]) + 0.5);
				t = (0.5 * t - 0.25 * sin(2.0 * t)) *
					(2.0 / M_PI);
				*(ppy + x) = 219.0 * (1.0 - t) + 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
			else {
				*(ppy + x) = 16.0;
				*(ppu + x) = 128.0;
				*(ppv + x) = 128.0;
			}
		}
	}
}


/*  standard multiburst  */

void make_multiburst0(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float a, t, w;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 38)		yp[x] = 0;
		else if (x < 43){
			t = sin(M_PI * (float)(x-37) / 12.0);
			t = t * t;
			yp[x] = t * 164.0;
		}
		else if (x < 92)	yp[x] = 164.0; /* upper */
		else if (x < 97){
			t = sin(M_PI * (float)(x-91.0) / 12.0);
			t = t * t;
			yp[x] = 164.0 - t * 142.0;
		}
		else if (x < 147)	yp[x] = 22.0; /* lower */
		else if (x < 152){
			t = sin(M_PI * (float)(x-146.0) / 12.0);
			t = t * t;
			yp[x] = 22.0 + t * 71.0;
		}
		else if (x < 200)	yp[x] = 93.0;
		else if (x < 270){ /* 500 kHz */
			t = (float)(x - 199.0);
			w = 2.0 * M_PI * 0.5 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 205){
				t = sin(M_PI * (float)(x-199.0) / 12.0);
				a = a * t * t;
			}
			else if (x >= 265){
				t = sin(M_PI * (float)(x-264.0) / 12.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 279)	yp[x] = 93.0;
		else if (x < 351){ /* 1 MHz */
			t = (float)(x - 278.0);
			w = 2.0 * M_PI * 1.0 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 286){
				t = sin(M_PI * (float)(x-278.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 344){
				t = sin(M_PI * (float)(x-343.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 360)	yp[x] = 93.0;
		else if (x < 430){ /* 2 MHz */
			t = (float)(x - 359.0);
			w = 2.0 * M_PI * 2.0 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 367){
				t = sin(M_PI * (float)(x-359.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 423){
				t = sin(M_PI * (float)(x-422.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 441)	yp[x] = 93.0;
		else if (x < 511){ /* 3 MHz */
			t = (float)(x - 440.0);
			w = 2.0 * M_PI * 3.0 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 448){
				t = sin(M_PI * (float)(x-440.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 504){
				t = sin(M_PI * (float)(x-503.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 522)	yp[x] = 93;
		else if (x < 591){ /* 3.58 MHz */
			t = (float)(x - 521.0);
			w = 2.0 * M_PI * 3.57954545 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 529){
				t = sin(M_PI * (float)(x-521.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 584){
				t = sin(M_PI * (float)(x-583.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 603)	yp[x] = 93.0;
		else if (x < 672){ /* 4.2 MHz */
			t = (float)(x - 602.0);
			w = 2.0 * M_PI * 4.2 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 610){
				t = sin(M_PI * (float)(x-602.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 665){
				t = sin(M_PI * (float)(x-664.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 710)	yp[x] = 93.0;
		else if (x < 715){
			t = sin(M_PI * (float)(x-709.0) / 12.0);
			t = t * t;
			yp[x] = 93.0 - t * 93.0;
		}
		else	yp[x] = 0.0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}

/*  standard multiburst  */

void make_multiburst1(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float a, t, w;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 38)		yp[x] = 0;
		else if (x < 43){
			t = sin(M_PI * (float)(x-37) / 12.0);
			t = t * t;
			yp[x] = 16.0 + t * 164.0;
		}
		else if (x < 92)	yp[x] = 191.0; /* upper */
		else if (x < 97){
			t = sin(M_PI * (float)(x-91) / 12.0);
			t = t * t;
			yp[x] = 191.0 - t * 132.0;
		}
		else if (x < 147)	yp[x] = 59.0; /* lower */
		else if (x < 152){
			t = sin(M_PI * (float)(x-146) / 12.0);
			t = t * t;
			yp[x] = 59.0 + t * 66.0;
		}
		else if (x < 200)	yp[x] = 125.0;
		else if (x < 270){ /* 500 kHz */
			t = (float)(x - 199.0);
			w = 2.0 * M_PI * 0.5 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 205){
				t = sin(M_PI * (float)(x-199.0) / 12.0);
				a = a * t * t;
			}
			else if (x >= 265){
				t = sin(M_PI * (float)(x-264.0) / 12.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 279)	yp[x] = 125.0;
		else if (x < 351){ /* 1 MHz */
			t = (float)(x - 278.0);
			w = 2.0 * M_PI * 1.0 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 286){
				t = sin(M_PI * (float)(x-278.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 344){
				t = sin(M_PI * (float)(x-343.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 360)	yp[x] = 125.0;
		else if (x < 430){ /* 2 MHz */
			t = (float)(x - 359.0);
			w = 2.0 * M_PI * 2.0 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 367){
				t = sin(M_PI * (float)(x-359.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 423){
				t = sin(M_PI * (float)(x-422.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 441)	yp[x] = 125.0;
		else if (x < 511){ /* 4 MHz */
			t = (float)(x - 440.0);
			w = 2.0 * M_PI * 3.0 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 448){
				t = sin(M_PI * (float)(x-440.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 504){
				t = sin(M_PI * (float)(x-503.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 522)	yp[x] = 125.0;
		else if (x < 591){ /* 4.8 MHz */
			t = (float)(x - 521.0);
			w = 2.0 * M_PI * 4.8 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 529){
				t = sin(M_PI * (float)(x-521.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 584){
				t = sin(M_PI * (float)(x-583.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 603)	yp[x] = 125.0;
		else if (x < 672){ /* 5.8 MHz */
			t = (float)(x - 602.0);
			w = 2.0 * M_PI * 5.8 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 610){
				t = sin(M_PI * (float)(x-602.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 665){
				t = sin(M_PI * (float)(x-664.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 710)	yp[x] = 125.0;
		else if (x < 715){
			t = sin(M_PI * (float)(x-709.0) / 12.0);
			t = t * t;
			yp[x] = 125.0 - t * 109.0;
		}
		else	yp[x] = 16.0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}


/*  standard multiburst  */

void make_multiburst2(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float a, t, w;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 5)		yp[x] = 0;
		else if (x < 43){
			t = sin(M_PI * (float)(x+1) / 12.0);
			t = t * t;
			yp[x] = t * 164.0;
		}
		else if (x < 48)	yp[x] = 164.0; /* upper */
		else if (x < 53){
			t = sin(M_PI * (float)(x-47) / 12.0);
			t = t * t;
			yp[x] = 164.0 - t * 142.0;
		}
		else if (x < 100)	yp[x] = 22.0; /* lower */
		else if (x < 105){
			t = sin(M_PI * (float)(x-99) / 12.0);
			t = t * t;
			yp[x] = 22.0 + t * 71.0;
		}
		else if (x < 127)	yp[x] = 93.0;
		else if (x < 200){ /* 500 kHz */
			t = (float)(x - 127);
			w = 2.0 * M_PI * 0.5 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 132){
				t = sin(M_PI * (float)(x-126) / 12.0);
				a = a * t * t;
			}
			else if (x >= 195){
				t = sin(M_PI * (float)(x-194.0) / 12.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 214)	yp[x] = 93.0;
		else if (x < 286){ /* 1 MHz */
			t = (float)(x - 216.0);
			w = 2.0 * M_PI * 1.0 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 221){
				t = sin(M_PI * (float)(x-213.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 279){
				t = sin(M_PI * (float)(x-278.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 300)	yp[x] = 93.0;
		else if (x < 370){ /* 2 MHz */
			t = (float)(x - 302);
			w = 2.0 * M_PI * 2.0 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 307){
				t = sin(M_PI * (float)(x-299.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 363){
				t = sin(M_PI * (float)(x-362.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 386)	yp[x] = 93.0;
		else if (x < 456){ /* 3 MHz */
			t = (float)(x - 388);
			w = 2.0 * M_PI * 3.0 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 393){
				t = sin(M_PI * (float)(x-385) / 16.0);
				a = a * t * t;
			}
			else if (x >= 449){
				t = sin(M_PI * (float)(x-448) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 472)	yp[x] = 93.0;
		else if (x < 543){ /* 3.58 MHz */
			t = (float)(x - 474.0);
			w = 2.0 * M_PI * 3.5794545 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 479){
				t = sin(M_PI * (float)(x-471) / 16.0);
				a = a * t * t;
			}
			else if (x >= 536){
				t = sin(M_PI * (float)(x-535.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 558)	yp[x] = 93.0;
		else if (x < 624){ /* 4.2 MHz */
			t = (float)(x - 560);
			w = 2.0 * M_PI * 4.2 / samp_clk[std];
			a = 71.0 * sin(w * t);
			if (x < 565){
				t = sin(M_PI * (float)(x-557) / 16.0);
				a = a * t * t;
			}
			else if (x >= 617){
				t = sin(M_PI * (float)(x-616) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 93.0 + a;
		}
		else if (x < 635)	yp[x] = 93.0;
		else if (x < 640){
			t = sin(M_PI * (float)(x-634) / 12.0);
			t = t * t;
			yp[x] = 93.0 - t * 93.0;
		}
		else	yp[x] = 0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}



/*  standard multiburst  */

void make_multiburst3(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float a, t, w;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 24)		yp[x] = 16.0;
		else if (x < 29){
			t = sin(M_PI * (float)(x+1) / 12.0);
			t = t * t;
			yp[x] = 16.0 + t * 175.0;
		}
		else if (x < 83)	yp[x] = 191.0; /* upper */
		else if (x < 88){
			t = sin(M_PI * (float)(x-82) / 12.0);
			t = t * t;
			yp[x] = 191.0 - t * 132.0;
		}
		else if (x < 142)	yp[x] = 59.0; /* lower */
		else if (x < 147){
			t = sin(M_PI * (float)(x-141) / 12.0);
			t = t * t;
			yp[x] = 59.0 + t * 66.0;
		}
		else if (x < 201)	yp[x] = 125.0;
		else if (x < 275){ /* 500 kHz */
			t = (float)(x - 200.0);
			w = 2.0 * M_PI * 0.5 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 206){
				t = sin(M_PI * (float)(x-200) / 12.0);
				a = a * t * t;
			}
			else if (x >= 270){
				t = sin(M_PI * (float)(x-269) / 12.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 287)	yp[x] = 125.0;
		else if (x < 365){ /* 1 MHz */
			t = (float)(x - 286.0);
			w = 2.0 * M_PI * 1.0 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 294){
				t = sin(M_PI * (float)(x-286) / 16.0);
				a = a * t * t;
			}
			else if (x >= 358){
				t = sin(M_PI * (float)(x-357.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 376)	yp[x] = 125.0;
		else if (x < 452){ /* 2 MHz */
			t = (float)(x - 375.0);
			w = 2.0 * M_PI * 2.0 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 383){
				t = sin(M_PI * (float)(x-375) / 16.0);
				a = a * t * t;
			}
			else if (x >= 445){
				t = sin(M_PI * (float)(x-444) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 465)	yp[x] = 125.0;
		else if (x < 542){ /* 4 MHz */
			t = (float)(x - 464.0);
			w = 2.0 * M_PI * 4.0 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 472){
				t = sin(M_PI * (float)(x-464.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 535){
				t = sin(M_PI * (float)(x-534) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 552)	yp[x] = 125;
		else if (x < 631){ /* 4.8 MHz */
			t = (float)(x - 551);
			w = 2.0 * M_PI * 4.8 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 559){
				t = sin(M_PI * (float)(x-551.0) / 16.0);
				a = a * t * t;
			}
			else if (x >= 624){
				t = sin(M_PI * (float)(x-623.0) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 641)	yp[x] = 125.0;
		else if (x < 721){ /* 5.8 MHz */
			t = (float)(x - 640);
			w = 2.0 * M_PI * 5.8 / samp_clk[std];
			a = 66.0 * sin(w * t);
			if (x < 648){
				t = sin(M_PI * (float)(x-640) / 16.0);
				a = a * t * t;
			}
			else if (x >= 714){
				t = sin(M_PI * (float)(x-713) / 16.0);
				a = a * (1.0 - (t * t));
			}
			yp[x] = 125.0 + a;
		}
		else if (x < 761)	yp[x] = 125.0;
		else if (x < 766){
			t = sin(M_PI * (float)(x-760) / 12.0);
			t = t * t;
			yp[x] = 125.0 - t * 109.0;
		}
		else	yp[x] = 16.0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}



/*  Y-only 5-step staircase  */

void make_5step0(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float t;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 129)		yp[x] = 0.0;
		else if (x < 134){
			t = sin(M_PI * (float)(x-128.0) / 12.0);
			t = t * t;
			yp[x] = t * 47.0;
		}
		else if (x < 241)	yp[x] = 47.0;
		else if (x < 246){
			t = sin(M_PI * (float)(x-240.0) / 12.0);
			t = t * t;
			yp[x] = 47.0 + t * 47.0;
		}
		else if (x < 351)	yp[x] = 94.0;
		else if (x < 356){
			t = sin(M_PI * (float)(x-350.0) / 12.0);
			t = t * t;
			yp[x] = 94.0 + t * 47.0;
		}
		else if (x < 463)	yp[x] = 141.0;
		else if (x < 468){
			t = sin(M_PI * (float)(x-462.0) / 12.0);
			t = t * t;
			yp[x] = 141.0 + t * 47.0;
		}
		else if (x < 574)	yp[x] = 188.0;
		else if (x < 579){
			t = sin(M_PI * (float)(x-573.0) / 12.0);
			t = t * t;
			yp[x] = 188.0 + t * 47.0;
		}
		else if (x < 713)	yp[x] = 235.0;
		else if (x < 718){
			t = sin(M_PI * (float)(x-712.0) / 12.0);
			t = t * t;
			yp[x] = 235.0 - t * 235.0;
		}
		else	yp[x] = 0.0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}



/*  Y-only 5-step staircase  */

void make_5step1(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float t;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 112)		yp[x] = 15.0;
		else if (x < 134){
			t = sin(M_PI * (float)(x-111.0) / 12.0);
			t = t * t;
			yp[x] = 15.0 + t * 44.0;
		}
		else if (x < 220)	yp[x] = 59.0;
		else if (x < 225){
			t = sin(M_PI * (float)(x-219.0) / 12.0);
			t = t * t;
			yp[x] = 59.0 + t * 44.0;
		}
		else if (x < 333)	yp[x] = 103.0;
		else if (x < 338){
			t = sin(M_PI * (float)(x-332.0) / 12.0);
			t = t * t;
			yp[x] = 103.0 + t * 44.0;
		}
		else if (x < 445)	yp[x] = 147.0;
		else if (x < 450){
			t = sin(M_PI * (float)(x-444.0) / 12.0);
			t = t * t;
			yp[x] = 147.0 + t * 44.0;
		}
		else if (x < 558)	yp[x] = 191.0;
		else if (x < 563){
			t = sin(M_PI * (float)(x-557.0) / 12.0);
			t = t * t;
			yp[x] = 191.0 + t * 44.0;
		}
		else if (x < 713)	yp[x] = 235.0;
		else if (x < 718){
			t = sin(M_PI * (float)(x-712.0) / 12.0);
			t = t * t;
			yp[x] = 235.0 - t * 220.0;
		}
		else	yp[x] = 15.0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}


/*  Y-only 5-step staircase  */

void make_5step2(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float t;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 115)		yp[x] = 0;
		else if (x < 120){
			t = sin(M_PI * (float)(x-114) / 12.0);
			t = t * t;
			yp[x] = t * 47.0;
		}
		else if (x < 214)	yp[x] = 47.0;
		else if (x < 219){
			t = sin(M_PI * (float)(x-213) / 12.0);
			t = t * t;
			yp[x] = 47.0 + t * 47.0;
		}
		else if (x < 312)	yp[x] = 94.0;
		else if (x < 317){
			t = sin(M_PI * (float)(x-311) / 12.0);
			t = t * t;
			yp[x] = 94.0 + t * 47.0;
		}
		else if (x < 411)	yp[x] = 141.0;
		else if (x < 416){
			t = sin(M_PI * (float)(x-410) / 12.0);
			t = t * t;
			yp[x] = 141.0 + t * 47.0;
		}
		else if (x < 510)	yp[x] = 188.0;
		else if (x < 515){
			t = sin(M_PI * (float)(x-509) / 12.0);
			t = t * t;
			yp[x] = 188.0 + t * 47.0;
		}
		else if (x < 633)	yp[x] = 235.0;
		else if (x < 638){
			t = sin(M_PI * (float)(x-632.0) / 12.0);
			t = t * t;
			yp[x] = 235.0 - t * 235.0;
		}
		else	yp[x] = 0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}



/*  Y-only 5-step staircase  */

void make_5step3(float *pyx, float *pux, float *pvx,unsigned char std)
{
	float *ppy, *ppu, *ppv, yp[ MAX_HOR/XDIV ], uv[ MAX_HOR/XDIV ];
	short x, y;
	float t;

	for (x = 0; x < xsize[std]; x++){
		uv[x] = 128.0;
		if (x < 120)		yp[x] = 15.0;
		else if (x < 125){
			t = sin(M_PI * (float)(x-119) / 12.0);
			t = t * t;
			yp[x] = 15.0 + t * 44.0;
		}
		else if (x < 235)	yp[x] = 59.0;
		else if (x < 240){
			t = sin(M_PI * (float)(x-234) / 12.0);
			t = t * t;
			yp[x] = 59.0 + t * 44.0;
		}
		else if (x < 355)	yp[x] = 103.0;
		else if (x < 360){
			t = sin(M_PI * (float)(x-354) / 12.0);
			t = t * t;
			yp[x] = 103.0 + t * 44.0;
		}
		else if (x < 475)	yp[x] = 147.0;
		else if (x < 480){
			t = sin(M_PI * (float)(x-474) / 12.0);
			t = t * t;
			yp[x] = 147.0 + t * 44.0;
		}
		else if (x < 595)	yp[x] = 191.0;
		else if (x < 600){
			t = sin(M_PI * (float)(x-594) / 12.0);
			t = t * t;
			yp[x] = 191.0 + t * 44.0;
		}
		else if (x < 761)	yp[x] = 235.0;
		else if (x < 766){
			t = sin(M_PI * (float)(x-760) / 12.0);
			t = t * t;
			yp[x] = 235.0 - t * 220.0;
		}
		else	yp[x] = 15.0;
	}

	for (y = 0; y < ysize[std]; y++){
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);
		for (x = 0; x < xsize[std]; x += 2){
			*(ppu + x) = uv[x];
			*(ppy + x) = yp[x];
			*(ppv + x) = uv[x];
			*(ppy + x + 1) = yp[x+1];
		}
	}
}


/* Generate a 8-bit alpha pattern */
void mod_make_8bit_alpha(unsigned char std)
{
	short *ppa;
	short x, y, i;

	/* To generate a pattern of 0,0,...,1,2,3,...,255,255,255... in each line */
	for (y = 0; y < ysize[std]; y++)
	{
	    ppa = &afgm[y][0];

	    for (x = 0; x < xsize[std]; x++)
	    {
		i = x - 192;
		*ppa = i < 0 ? 0 : (i > 255 ? 255 : i);
		ppa++;
	    }
	}
}

/* Generate a 10-bit alpha pattern */

void mod_make_10bit_alpha(unsigned char std)
{
        short *ppa;
        short x, y, i;

	/* To generate a pattern of 0,0,..,0,2,4,6,8,..,1022,1023,1023,.. in each line */
        for (y = 0; y < ysize[std]; y++)
        {
	    	ppa = &afgm[y][0];

            for (x = 0; x < xsize[std]; x++)
            {
		i = x - 103;
		if (i < 0)
		    *ppa = 0;
		else
		{
		    if (*(ppa-1) >= 1022)
		    	*ppa = 1023;
		    else
		        *ppa = *(ppa -1) + 2;
		}

		ppa++;
	    }
	}
}

/* Write YUVA data to an output file in RP175 4x4 format */

void pix_write_4444m(unsigned long *pline, float *pyx, float *pux, float *pvx, short *pax, unsigned char std, unsigned char bit8_10)
{
        float *ppy, *ppu, *ppv;
		short *ppa;
        short x, y;
		unsigned char 	*pline8;
		unsigned short 	*pline10;
		pline8 	= (unsigned char *)pline;
		pline10 = (unsigned short *)pline;

        for (y = 0; y < ysize[std]; y++)
        {
                ppy = pyx + (y * xsize[std]);
                ppu = pux + (y * xsize[std]);
                ppv = pvx + (y * xsize[std]);
				ppa = pax + (y * xsize[std]);

                if (bit8_10 == 8)
                {
                    for (x = 0; x < xsize[std]; x++)
                    {
                        *(pline8++) = (unsigned char)(*(ppv + x) + 0.5);
                        *(pline8++) = (unsigned char)(*(ppy + x) + 0.5);
                        *(pline8++) = (unsigned char)(*(ppu + x) + 0.5);
						*(pline8++) = (unsigned char)(*(ppa + x) + 0.5);
                    }
		}
                else /* 10-bit */
                {
                    for (x = 0; x < xsize[std]; x++)
                    {
                        *(pline10++) = (unsigned short)(*(ppv + x) * multiply + 0.5)
				       << alignment;

                        *(pline10++) = (unsigned short)(*(ppy + x) * multiply + 0.5)
				       << alignment;

                        *(pline10++) = (unsigned short)(*(ppu + x) * multiply + 0.5)
				       << alignment;

                        *(pline10++) = (unsigned short)(*(ppa + x) * multiply + 0.5)
				       << alignment;
		    } 
                }
        }
}

/* Write YUVA data to an output file in packed YUVA 4:2:2:4 format */

void pix_write_4224m(unsigned long *pline, float *pyx, float *pux, float *pvx, short *pax, unsigned char std)
{
        float *ppy, *ppu, *ppv;
        short *ppa;
        short x, y;

        for (y = 0; y < ysize[std]; y++)
        {
                ppy = pyx + (y * xsize[std]);
                ppu = pux + (y * xsize[std]);
                ppv = pvx + (y * xsize[std]);
                ppa = pax + (y * xsize[std]);

	    for (x = 0; x < xsize[std]; x += 2)
	    {
		*(pline++) = ((unsigned long)(*(ppu + x) * multiply + 0.5) << 22) |
			     ((unsigned long)(*(ppy + x) * multiply + 0.5) << 12) |
			     ((unsigned long)(*(ppa + x) * multiply + 0.5) << 2);

		*(pline++) = ((unsigned long)(*(ppv + x)   * multiply + 0.5) << 22) |
                             ((unsigned long)(*(ppy + x+1) * multiply + 0.5) << 12) |
                             ((unsigned long)(*(ppa + x+1) * multiply + 0.5) << 2);
	    }
	}
}

/* Write YUV data to an output file in YUV 4:2:2 format */

void pix_write_422m(unsigned long *pline, float *pyx, float *pux, float *pvx, unsigned char std, unsigned char bit8_10)
{
	float *ppy, *ppu, *ppv;
	short x, y;
	int i;
	unsigned char 	*pline8;
	unsigned short *pline10;
	pline8 	= (unsigned char  *)pline;
	pline10 = (unsigned short *)pline;

	for (y = 0; y < ysize[std]; y++)
	{
		ppy = pyx + (y * xsize[std]);
		ppu = pux + (y * xsize[std]);
		ppv = pvx + (y * xsize[std]);

		if (bit8_10 == 8)
		{
		    for (x = 0; x < xsize[std]; x += 2)
		    {
		        *(pline8++) = (unsigned char)(*(ppu + x) + 0.5);
		        *(pline8++) = (unsigned char)(*(ppy + x) + 0.5);
		        *(pline8++) = (unsigned char)(*(ppv + x) + 0.5);
		        *(pline8++) = (unsigned char)(*(ppy + x + 1) + 0.5);
		    }
	    }
		else /* 10-bit */
		{
            for (x = 0; x < xsize[std]; x += 2)
            {
            	*(pline10++) = (unsigned short)(*(ppu + x) * multiply + 0.5)
				       << alignment;

                *(pline10++) = (unsigned short)(*(ppy + x) * multiply + 0.5)
				       << alignment;

                *(pline10++) = (unsigned short)(*(ppv + x) * multiply + 0.5)
				       << alignment;

               *(pline10++) = (unsigned short)(*(ppy + x + 1) * multiply + 0.5)
				       << alignment;

			/* Convert the data to 8-bit for display on Galileo 1.0 */
			if (convert)
			{
			    for (i = 4; i > 0; i--)
			    {
			         *(pline8++) = (unsigned char)(((*(pline10-i) >> alignment) + 0.5) / multiply) & 0xff;
			    }
			}
			}
		}
	}
}

void test_allstds(unsigned long *pixels, int format, unsigned char tv_std, int bits, int test_num)
{

	/* set up a number of left alignment with zero for 10-bit data 
         * in 16-bit packs
	 */ 
	if (format == 4224)
	    alignment = 2;
	else if (bits == 10)
 	{
	    alignment = 6;
	    multiply = 4.0;
	}

	switch(test_num)
	{
		case 5: mod_make_y_zoneplate(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
		case 6: mod_make_yc_zoneplate(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	}

	switch(tv_std)
	{
		case 0:		
			switch(test_num)
			{
	    		case 0: make_cb75_0(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 1: make_pulse_bar0(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 2: make_mod_ramp0(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 3: make_multiburst0(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 4: make_5step0(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		default:
						make_cb75_0(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
			}
		break;

		case 1:
			switch(test_num)
			{
	    		case 0: make_cb75_1(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 1: make_pulse_bar1(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 2: make_mod_ramp1(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 3: make_multiburst1(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 4: make_5step1(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		default:
						make_cb75_1(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
			}
		break;

		case 2:
			switch(test_num)
			{
	    		case 0: make_cb75_2(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 1: make_pulse_bar2(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 2: make_mod_ramp2(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 3: make_multiburst2(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 4: make_5step2(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		default:
						make_cb75_2(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
			}
		break;

		case 3:
			switch(test_num)
			{
	    		case 0: make_cb75_3(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 1: make_pulse_bar3(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 2: make_mod_ramp3(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 3: make_multiburst3(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		case 4: make_5step3(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
	    		default:
						make_cb75_3(yfgm[0], ufgm[0], vfgm[0],tv_std); break;
			}
		break;

		default:
			break;
	}
	

	switch(format)
	{
	    case 4444:
		if (bits == 8)
		    mod_make_8bit_alpha(tv_std);
		else 	
			mod_make_10bit_alpha(tv_std);
		pix_write_4444m(pixels, yfgm[0], ufgm[0], vfgm[0], afgm[0], tv_std, bits);
		break;

	    case 422:
		pix_write_422m(pixels, yfgm[0], ufgm[0], vfgm[0], tv_std, bits);
		break;

	    case 4224:
                mod_make_10bit_alpha(tv_std);
		pix_write_4224m(pixels, yfgm[0], ufgm[0], vfgm[0], afgm[0], tv_std);
		break;

	    default:
			break;
	}
}
