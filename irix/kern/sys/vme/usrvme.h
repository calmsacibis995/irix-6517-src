/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
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

#ident "$Revision: 1.2 $"

/*
 * Since the VME subsystem is in use by two different set of users, 
 * the device driver writers and the user level VME users, the
 * VME support files should try and avoid as much kernel data structure
 * dependency as possible.
 * XXX Try to hide all the data member details in this file
 * This file is to be used only by the kernel and internal user libraries,
 * namely, user level dma engine library.
 */

#ifndef __SYS_VME_USRVME_H__
#define __SYS_VME_USRVME_H__

#define UVIOC ('u' << 16 | 'v' << 8)

#define UVIOCREGISTERULI	(UVIOC|0)	/* register ULI */

#endif /* __SYS_VME_USRVME_H__ */




