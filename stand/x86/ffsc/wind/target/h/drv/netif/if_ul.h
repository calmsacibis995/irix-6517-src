/* if_ul.h - User Level IP header  */

/* Copyright 1992-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,20jun91,gae  written.
*/

#ifndef	INCif_ulh
#define	INCif_ulh

#define M_if_ul	91		/* XXX should be in vwModNum.h */

#define S_if_ul_INVALID_UNIT_NUMBER			(M_if_ul | 1)
#define S_if_ul_UNIT_UNINITIALIZED			(M_if_ul | 2)
#define S_if_ul_UNIT_ALREADY_INITIALIZED		(M_if_ul | 3)
#define S_if_ul_NO_UNIX_DEVICE				(M_if_ul | 4)

#endif	/* INCif_ulh */
