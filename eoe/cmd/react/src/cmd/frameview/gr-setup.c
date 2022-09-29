/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************

 **************************************************************************
 *	 								  *
 * File:	setup.c						          *
 *									  *
 * Description:	This file sets up the color vectors for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define GRMAIN extern
#include "graph.h"

/* make these global so everybody can use them */
float blackvect[3] =	{0.0, 0.0, 0.0};
float redvect[3] =	{1.0, 0.0, 0.0};
float red1vect[3] =	{1.0, 0.0, 0.0};
float greenvect[3] =	{0.0, 1.0, 0.0};
float green1vect[3] =	{0.0, 0.5, 0.0};
float bluevect[3] =	{0.0, 0.0, 1.0};
float blue1vect[3] =	{0.0, 0.0, 0.5};
float yellowvect[3] =	{1.0, 1.0, 0.0};
float yellow1vect[3] =	{0.5, 0.5, 0.0};
float magentavect[3] =	{1.0, 0.0, 1.0};
float magenta1vect[3] =	{0.5, 0.0, 0.5};
float purplevect[3] =	{0.5, 0.0, 1.0};
float purple1vect[3] =	{0.25, 0.0, 0.5};
float cyanvect[3] =	{0.0, 1.0, 1.0};
float cyan1vect[3] =	{0.0, 0.5, 0.5};
float whitevect[3] =	{1.0, 1.0, 1.0};
float greyvect[3] =	{.588, .588, .588};

float backgnd[3] =	{0.0, 0.0, 0.0};
short bkgnd[3] =	{0, 0, 0};

float kern_color[KERN_COLORS][3] = {
    {1.0, 0.0, 1.0},
    {0.0, 0.0, 1.0},
    {0.0, 1.0, 0.0},
    {1.0, 0.5, 0.5},
    {0.0, 1.0, 1.0},
    {0.5, 0.0, 1.0},
    {0.0, 1.0, 0.5},
    {1.0, 0.75, 0.5},
};

/*float kern_color[KERN_COLORS][3] = {
    {1.0, 0.0, 1.0},
    {0.0, 1.0, 1.0},
    {1.0, 0.5, 0.5},
    {0.0, 1.0, 0.0},
    {0.0, 0.0, 1.0},
    {1.0, 0.75, 0.5},
    {0.5, 0.5, 1.0},
    {0.0, 1.0, 0.5},
};*/

float idlevect[3] =	{0.0, 0.90, 0.70};
float ex_ev_idlevect[3] = {0.0, 0.90, 0.70};
float ex_ev_kernvect[3] = {1.0, 0.0, 0.0};
float ex_ev_intrvect[3] = {1.0, 1.0, 0.0};
float ex_ev_uservect[3] = {0.0, 0.9, 1.0};

/*GLXconfig rgb_mode[] = {
    { GLX_NORMAL, GLX_RGB, TRUE },
    { GLX_NORMAL, GLX_DOUBLE, TRUE },
    { GLX_NORMAL, GLX_ZSIZE, GLX_NOCONFIG },
    { 0,          0,       0,   }
}; 
*/

int dblBuf[] = {
    GLX_DOUBLEBUFFER, GLX_RGBA, GLX_DEPTH_SIZE, 16,
    GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
    None
};

int *snglBuf = &dblBuf[1];

int zBuf[] = {
    GLX_DEPTH_SIZE, 1,
    None
};

/* empirically determined to make the text print nicely 
   centered around a requested value */
int adj_val[16] = {1,400,140,90,60,47,38,32,27,23,20,17,15,13,12,11};



void
init_proc_colors(int numb_colors)
{
    int i;

    i = 0;

    /* purple blue */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 1.0;
    i++;
    /* dim purle blue */
    proc_color[i][0] = 0.25;
    proc_color[i][1] = 0.25;
    proc_color[i][2] = 0.5;
    i++;
    /* magenta */
    proc_color[i][0] = 1.0;
    proc_color[i][1] = 0;
    proc_color[i][2] = 1.0;
    i++;
    /* dim magenta */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 0;
    proc_color[i][2] = 0.5;
    i++;
    /* rose? */
    proc_color[i][0] = 1.0;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 0.5;
    i++;
    /* dim rose? */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 0.25;
    proc_color[i][2] = 0.25;
    i++;
    if (numb_colors <= 4) goto color_end;
    /* blue */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0;
    proc_color[i][2] = 1.0;
    i++;
    /* dim blue */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0;
    proc_color[i][2] = 0.5;
    i++;
   /* light blue */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 1.;
    i++;
    /* dim light blue */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0.25;
    proc_color[i][2] = 0.5;
    i++;
    /* cyan */
    proc_color[i][0] = 0;
    proc_color[i][1] = 1.0;
    proc_color[i][2] = 1.0;
    i++;
    /* dim cyan */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 0.5;
    i++;
    /* green */
    proc_color[i][0] = 0;
    proc_color[i][1] = 1.0;
    proc_color[i][2] = 0;
    i++;
    /* dim green */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 0;
    i++;
    /* purple */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 0;
    proc_color[i][2] = 1.0;
    i++;
    /* dim purple */
    proc_color[i][0] = 0.25;
    proc_color[i][1] = 0;
    proc_color[i][2] = 0.5;
    i++;
    /* turquois? */
    proc_color[i][0] = 0;
    proc_color[i][1] = 1.0;
    proc_color[i][2] = 0.5;
    i++;
    /* dim turquoise? */
    proc_color[i][0] = 0;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 0.25;
    i++;
    /* light green */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 1.0;
    proc_color[i][2] = 0.5;
    i++;
    /* dim light green */
    proc_color[i][0] = 0.25;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 0.25;
    i++;
    /* peach */
    proc_color[i][0] = 1.0;
    proc_color[i][1] = 0.75;
    proc_color[i][2] = 0.5;
    i++;
    /* dim peach */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 0.375;
    proc_color[i][2] = 0.25;
    i++;
    /* orange */
    proc_color[i][0] = 1.0;
    proc_color[i][1] = 0.5;
    proc_color[i][2] = 0.0;
    i++;
    /* dim orannge */
    proc_color[i][0] = 0.5;
    proc_color[i][1] = 0.25;
    proc_color[i][2] = 0.0;
    i++;

  color_end:
    numb_proc_colors = i/2;
    
    /* set kernel startup to be red */
    kern_startup_color[0] = 1.0;
    kern_startup_color[1] = 1.0;
    kern_startup_color[2] = 1.0;

}
