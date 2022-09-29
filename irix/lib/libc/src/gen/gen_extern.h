/**************************************************************************
 *									  *
 * Copyright (C) 1986-1992 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __GEN_EXTERN_H__
#define __GEN_EXTERN_H__

#include <utmpx.h>	/* for updutmp prototype */

/* abort.c */
extern void _exithandle(void);
extern void _cleanup(void);

/* from proc/sproc.s used in cuexit.c */
/* 
 * This symbol is not really a function, but rather a loader defined
 * symbol with an absolute value.  This value will be defined in
 * an a.out to be 1 if dynamic linking is used, 0 if the a.out was
 * statically linked.  It is declared here as a function simply to
 * avoid having the compiler expect storage to be reserved for it.
 * jlg 9/29/92
 */
extern void _DYNAMIC_LINK();

/* gethz.c */
extern int gethz(void);

/*  isastream.c - is "public", but not defined in any other header files */
extern int isastream(int);

/* time_data.c */
#include <tzfile.h>
extern const short __month_size[];
extern const int __lyday_to_month[];
extern const int __yday_to_month[];
extern const int __mon_lengths[2][MONS_PER_YEAR];
extern const int __year_lengths[2];
extern const struct {
	int     yrbgn;
	int     daylb;
	int     dayle;
} __daytab[];

/* getdate_data.c */
#include <nl_types.h>
extern const nl_item _time_item[];

/* getut.c */
extern int synchutmp(const char *, const char *);

/* malloc.c */
extern void __free(void *);

/* getutx.c */
extern int updutmp(const struct utmpx *);

/* atexit.c */
extern void __eachexithandle(void);

#endif
