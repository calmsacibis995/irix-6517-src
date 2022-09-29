/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: procmem.h,v 1.1 1997/03/21 05:01:54 markgw Exp $"

/*
 * process memory interface
 * Fields are Kbytes.
 * vxxx is virtual memory usage.
 * pxxx is physical mem, pro-rated for sharing.
 */
typedef struct {
	unsigned long	ptxt;
	unsigned long	vtxt;
	unsigned long	pdat;
	unsigned long	vdat;
	unsigned long	pbss;
	unsigned long	vbss;
	unsigned long	pstk;
	unsigned long	vstk;
	unsigned long	pshm;
	unsigned long	vshm;
} _pmProcMem_t;

extern int _pmProcMem(int /*fd*/, _pmProcMem_t *);
