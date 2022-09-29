/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_CXFS_ARRAY_H_
#define	_CXFS_ARRAY_H_

#ident	"$Id: cxfs_array.h,v 1.1 1997/08/26 18:21:38 dnoveck Exp $"

/*
 * fs/xfs/cell/v1/cxfs_array.h -- Miscellaneous stuff for cxfs array code
 *
 * This header file provides miscellaneous definitions for the cxfs
 * array code.
 * 
 * It should only be included in code within the cxfs (fs/xfs/cell/v1)
 * directory and should never be included in non CELL_ARRAY configurations.
 */
 
#ifndef CELL_ARRAY
#error included by non-CELL_ARRAY configuration
#endif

#define CXFS_CTIMEOUT_DFLT	30000	/* Default value for client timeout */
                                        /* (in milliseconds). */
#define CXFS_STIMEOUT_DFLT	5000	/* Default value for server timeout */
                                        /* (in milliseconds). */

extern void cxfs_array_wpinit(void);    /* Special initialization for the
                                           first array mount */
extern void cxfs_array_setup(void);     /* Do any delayed initialization. */

#endif /* _CXFS_ARRAY_H_ */
