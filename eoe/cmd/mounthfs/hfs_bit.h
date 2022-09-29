/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_BIT_H__
#define __HFS_BIT_H__

#ident "$Revision: 1.2 $"

/* Bitmap routines */

#define ISSET(m,p)  ((m)[p>>3] &   (0x80>>(p%8)))
#define ISCLR(m,p) (((m)[p>>3] &   (0x80>>(p%8)))==0)
#define SET(m,p)     (m)[p>>3] |=   0x80>>(p%8)
#define CLR(m,p)     (m)[p>>3] &= ~(0x80>>(p%8))
#define XOR(m,p)     (m)[p>>3] ^=  (0x80>>(p%8))

#define bit_set(p,m) SET(m,p)
#define bit_clear(p,m) CLR(m,p)


/* Well known global bit_ routines */

#ifdef BIT
void bit_clear(u_int position, char *map);
void bit_set(u_int position, char *map);
#endif

int bit_ffz(char *map, u_int max);
int bit_run(u_int position, char *map, u_int mapsize, u_int *runsize);
void bit_run_set(u_int position, char*map, u_int runsize);
void bit_run_clr(u_int position, char*map, u_int runsize);
int bit_run_isset(u_int position, char *map, u_int runsize);
int bit_run_isclr(u_int position, char *map, u_int runsize);
#endif
