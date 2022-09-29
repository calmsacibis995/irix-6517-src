#ifndef	_xfsUtil_h
#define	_xfsUtil_h

/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.1 $"

/*
 *	The structure, tuple_s holds the value pair (a,b) where a is the
 *	lvalue and b is the rvalue.
 */
typedef struct tuple_s {
	int	id;
	double	lvalue;
	double	rvalue;
	int	type;
} tuple_t;

#endif	/* _xfsUtil_h */
