/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	funcal_h
#	define funcal_h " @(#) funcal.h:3.2 5/30/92 20:18:41"
#endif

/*
** funcal.h
**
**      Common include file for Suite 3 benchmark.
**
*/

/*
** HISTORY
**
** 4/25/90 CTN Created
**
*/

#if defined(FUNCAL)
#define EXTERN
#else
#define EXTERN          extern
#endif

EXTERN int     fcal0(), fcal1(), fcal2(), fcal15(), fcalfake();

#undef EXTERN

