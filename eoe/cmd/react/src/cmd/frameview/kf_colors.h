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
 * File:	kf_colors.h						  *
 *									  *
 * Description:	This file contains data structures for the		  *
 *              colors data base					  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1995 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define MAXCOLORS	500
#define COLOR_DB_FILE_NAME	"color_db"
#define FUNCNAME_COLOR_DB_FILE_NAME	".funcname_color"

/*
 * struct color_db: the colors data base
 */

struct color_db {
	float rgb[3];
	int nalloc;		/* # of allocations */
};

extern struct color_db color_db[MAXCOLORS];
extern struct color_db default_color_db[];

extern int nallocated_color_entries;	/* # of entries already allocated */
extern int ncolors;			/* total number of colors in d b */

/*
 * struct funcname_color - function name - color association
 */

struct funcname_color {
	char name[MAXFUNCTNAME];
	float rgb[3];
	int used;		/* has this color been used? */
};

extern struct funcname_color *funcname_color;
extern int funcname_color_nentries;


/*
 * Function prototypes
 */

void read_color_db();
int insert_color_in_db(float rgb[3]);
float *get_color_by_index(int index);
int color_alloc();
int rgbcmp(float rgb1[3], float rgb2[3]);
void read_funcname_color_db();
void write_funcname_color_db();
float *get_color_by_funcname(char *name);

