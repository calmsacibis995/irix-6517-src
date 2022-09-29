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
 * File:	kf_colors.c						  *
 *									  *
 * Description:	This file contains the routines that handle      	  *
 *              the colors data base					  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1995 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#define GRMAIN extern
#include "graph.h"

#include "kf_main.h"
#include "kf_colors.h"

/*
 * Global variables
 */

/* default values are used if color data base file is not found */

struct color_db default_color_db[] = {
{{0.17, 0.00, 0.00}, 0,},
{{0.69, 0.25, 0.00}, 0,},
{{0.94, 0.46, 0.17}, 0,},
{{0.98, 0.12, 0.17}, 0,},
{{0.98, 0.21, 0.62}, 0,},
{{0.53, 0.21, 0.87}, 0,},
{{0.01, 0.37, 0.87}, 0,},
};

int ndefault_color_db_entries = 7;
/* NOTE: CHANGE default_color_db and ndefault_color_db_entries together */


struct color_db color_db[MAXCOLORS];

int nallocated_color_entries;    /* # of entries already allocated */
int ncolors;                 /* total number of colors in d b */

struct funcname_color *funcname_color;
int funcname_color_nentries = 0;


/*
 * Functions
 */

/*
 * get_default_color_db - get colors db from default
 */

void get_default_color_db()
{
	int i;

	for(i=0; i< ndefault_color_db_entries; i++)
		color_db[i] = default_color_db[i];  /* copy struct */

	nallocated_color_entries = ndefault_color_db_entries;
}

/*
 * read_color_db - read colors data base file. If there is any error, the
 *	default data base is used
 */

void read_color_db()
{
	FILE *fp;
	int lineno;
	int retval;
	char *filename = COLOR_DB_FILE_NAME;
	int c;
	char str[256];
	char msg_str[64];

	get_default_color_db();

	if((fp = fopen(filename, "r")) == NULL){
		perror(filename);
		sprintf(str,"%s/%s", INSTALL_DIR, filename);
		sprintf(msg_str, "Trying %s", str);
		grtmon_message(msg_str, DEBUG_MSG, 100);
		if((fp = fopen(str, "r")) == NULL){
			perror(str);
			sprintf(msg_str, "Using default colors");
			grtmon_message(msg_str, DEBUG_MSG, 100);
			return;
		}
	}

	nallocated_color_entries = 0;
	lineno = 1;
	while(1) {
		color_db[nallocated_color_entries].nalloc = 0;

		retval = fscanf(fp, " R %f G %f B %f", 
			&color_db[nallocated_color_entries].rgb[0],
			&color_db[nallocated_color_entries].rgb[1],
			&color_db[nallocated_color_entries].rgb[2]);

#ifdef DEBUG
sprintf(msg_str, "read_color_db: line lineno %d fscanf ret %d R %4.2f G %4.2f B %4.2f",
lineno, retval,color_db[nallocated_color_entries].rgb[0],
color_db[nallocated_color_entries].rgb[1],
color_db[nallocated_color_entries].rgb[2]);
		grtmon_message(msg_str, DEBUG_MSG, 100);
sleep(1);
#endif
		if(lineno > 1000){
			sprintf(msg_str, "Color file has invalid lines?");
			grtmon_message(msg_str, DEBUG_MSG, 100);
			break;
		}

		if(retval == EOF)   /* EOF */
			break;

		if(retval != 3){
			fprintf(stderr,"%s, line %d: invalid format\n",
				filename, lineno);
			while(((c = getc(fp)) != EOF) && (c != '\n'))
				; /* null command; eat rest of line */
#ifdef XXX
			while((retval = fscanf(fp, "%[\n]",str)) == 0)
				; /* null command; eat rest of line */
#endif
		}
		else {
			nallocated_color_entries++;
		}
		lineno++;
	}

	if(nallocated_color_entries == 0){
		fprintf(stderr,
			 "Could not get any colors, using default colors\n");
		get_default_color_db();
	}
}


/*
 * insert_color_in_db - insert a new color in data base (in the memory version)
 *	Returns the color's index
 */

int insert_color_in_db(float rgb[3])
{
	int i;
	int ind;

	/* first see if color already in db */
	for(ind=0; ind < nallocated_color_entries; ind++)
		if(rgbcmp(rgb, color_db[ind].rgb) == 0) {
			return ind;
		}

	/* allocate new entry */
	if(nallocated_color_entries >= MAXCOLORS){
		fprintf(stderr,"Colors data base is full\n");
		return 0;
	}

	for(i=0; i< 3; i++)
		color_db[nallocated_color_entries].rgb[i] = rgb[i];
	color_db[nallocated_color_entries].nalloc = 0;

	return nallocated_color_entries++;
}

/*
 * get_color_by_index - given index in color_db, return color vector
 */

float *get_color_by_index(int index)
{
	float *ptr;
	char msg_str[64];

	if(index >= nallocated_color_entries){
		sprintf(msg_str, "get_color_by_index: invalid index: %d",index);
		grtmon_message(msg_str, DEBUG_MSG, 100);
		ptr = color_db[0].rgb;
	}
	else
		if(index < 0){
			sprintf(msg_str, "get_color_by_index: invalid index: %d",index);
			grtmon_message(msg_str, DEBUG_MSG, 100);
			ptr = color_db[0].rgb;
		}
		else
			ptr = color_db[index].rgb;

	return ptr;
}

/*
 * color_alloc - allocate colors; return index in db
 */

int color_alloc()
{
	static int alloc_index = 0;
	int index;

	/* for now just circular allocation */
	index = alloc_index;
	if(++alloc_index >= nallocated_color_entries)
		alloc_index = 0;

	return index;
}

/*
 * rgbcmp - compare 2 rgb colors; return 0 if equal
 */

int rgbcmp(float rgb1[3], float rgb2[3])
{
	if((rgb1[0] == rgb2[0]) &&
	   (rgb1[1] == rgb2[1]) &&
	   (rgb1[2] == rgb2[2]))
		return 0;
	else
		return -1;
}

/*
 * read_funcname_color_db - read function name - color association file
 */

void read_funcname_color_db()
{
	FILE *fp;
	char filename[256];
	char *s;
	int c;
	int nlines;
	int lineno;
	int retval;
	char str[256];
    char msg_str[64];

	s = getenv("HOME");
	if(s == NULL){
		sprintf(msg_str, "read_funcname_color_db: no HOME variable");
		grtmon_message(msg_str, DEBUG_MSG, 100);
		return;
	}
	sprintf(filename,"%s/%s",s,FUNCNAME_COLOR_DB_FILE_NAME);

sprintf(msg_str, "read_funcname_color_db: Reading %s",filename);
	grtmon_message(msg_str, DEBUG_MSG, 100);

	if((fp = fopen(filename, "r")) == NULL){
		perror(filename);
		sprintf(msg_str, "Allocating colors in color data base");
		grtmon_message(msg_str, DEBUG_MSG, 100);
		return;
	}

	/* first get number of lines to know how many entries in
	   'funcname_color' to allocate */
	for(nlines = 1; (c = getc(fp)) != EOF;)
		if(c == '\n')
			nlines++;

	rewind(fp);

	funcname_color = (struct funcname_color *)
			malloc(sizeof(struct funcname_color)*(nlines+1));
	if(funcname_color == NULL){
		sprintf(msg_str, "No memory for funcname_color");
		grtmon_message(msg_str, DEBUG_MSG, 100);
		fclose(fp);
		return;
	}


	lineno = 1;
	funcname_color_nentries = 0;

	while(1) {
		retval = fscanf(fp, " %s R %f G %f B %f", 
			funcname_color[funcname_color_nentries].name,
			&funcname_color[funcname_color_nentries].rgb[0],
			&funcname_color[funcname_color_nentries].rgb[1],
			&funcname_color[funcname_color_nentries].rgb[2]);

#ifdef DEBUG
sprintf(msg_str, "read_funcname_color_db: l %d fscanf:%d [%s] R %4.2f G %4.2f B %4.2f",
lineno, retval,
funcname_color[funcname_color_nentries].name,
funcname_color[funcname_color_nentries].rgb[0],
funcname_color[funcname_color_nentries].rgb[1],
funcname_color[funcname_color_nentries].rgb[2]);
		grtmon_message(msg_str, DEBUG_MSG, 100);
sleep(1);
#endif
		if(lineno > 2000){
			sprintf(msg_str, "Funct name-Color file has invalid lines?");
			grtmon_message(msg_str, DEBUG_MSG, 100);
			break;
		}

		if(retval == EOF)   /* EOF */
			break;

		if(retval != 4){
			fprintf(stderr,"%s, line %d: invalid format\n",
				filename, lineno);
			while(((c = getc(fp)) != EOF) && (c != '\n'))
				; /* null command; eat rest of line */
#ifdef XXX
			while((retval = fscanf(fp, "%[\n]",str)) == 0)
				; /* null command; eat rest of line */
#endif
		}
		else {
			funcname_color[funcname_color_nentries].used = 0;
			funcname_color_nentries++;
		}
		lineno++;
	}
	fclose(fp);

sprintf(msg_str, "read_funcname_color_db: nlines %d funcname_color_nentries %d",
nlines,funcname_color_nentries);
	grtmon_message(msg_str, DEBUG_MSG, 100);

}


/*
 * write_funcname_color_db - write function name - color association file
 *	based on what we have in memory
 */

void write_funcname_color_db()
{
	FILE *fp;
	char filename[256];
	char *s;
	int retval;
	int i;
	float *rgb_ptr;
    char msg_str[64];

	s = getenv("HOME");
	if(s == NULL){
		sprintf(msg_str, "write_funcname_color_db: no HOME variable");
		grtmon_message(msg_str, DEBUG_MSG, 100);
		return;
	}
	sprintf(filename,"%s/%s",s,FUNCNAME_COLOR_DB_FILE_NAME);

sprintf(msg_str, "write_funcname_color_db: Writing %s",filename);
	grtmon_message(msg_str, DEBUG_MSG, 100);

	if((fp = fopen(filename, "w")) == NULL){
		perror(filename);
		return;
	}

	for(i=0; i <= funct_info_last_index; i++)
		if((funct_info[i].name != NULL) &&
		   (funct_info[i].color_index != -1)){
			rgb_ptr = get_function_color(i);
			fprintf(fp, "%s R %4.2f G %4.2f B %4.2f\n",
				funct_info[i].name,
				rgb_ptr[0], rgb_ptr[1], rgb_ptr[2]);
		}

	/* write also colors for functions (in the function name - color
	   association data base) that were not present in the event stream */
	for(i=0; i < funcname_color_nentries; i++)
		if(funcname_color[i].used == 0)
			fprintf(fp, "%s R %4.2f G %4.2f B %4.2f\n",
				funcname_color[i].name,
				funcname_color[i].rgb[0],
				funcname_color[i].rgb[1],
				funcname_color[i].rgb[2]);

	fclose(fp);
}


/*
 * get_color_by_funcname - looks in funcname color db for a color
 *	given the function name; return -1 if not found
 */

float *get_color_by_funcname(char *name)
{
	int i;

	for(i=0; i < funcname_color_nentries; i++)
		if(!strcmp(name, funcname_color[i].name)){
			funcname_color[i].used = 1;
			return funcname_color[i].rgb;
		}
	return (float *)-1;
}
