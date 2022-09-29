#ifndef __PIPEIOCTL_H__
#define __PIPEIOCTL_H__
/*
 * pipeioctl.h
 *
 *	Contains defs for use with pipefs ioctl interface.
 *
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

#ident "$Revision: 1.1 $"


#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#define	PFSIOC	('p' << 24 | 'f' << 16 | 's' << 8)

#define I_PPEEK (PFSIOC|1) 	/* peek data from pipe */


typedef struct pipepeek {
	long	pp_size;	/* size of buffer */
	caddr_t	pp_buf;		/* address of buffer */
} pipepeek_t;

#endif /* C || C++ */
#endif /* !__PIPEIOCTL_H__ */

