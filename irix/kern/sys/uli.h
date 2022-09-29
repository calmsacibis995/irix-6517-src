/*
 * sys/uli.h
 *
 *      User Level Interrupt definitions
 *
 * Copyright 1995, 1999, Silicon Graphics, Inc.
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


#ifndef __SYS_ULI_H
#define __SYS_ULI_H

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.22 $"

#include <sys/types.h>

struct uliargs {

    /* device independent fields */
    caddr_t pc;		/* parameter */
    caddr_t sp;		/* parameter */
    void *funcarg;	/* parameter */
    int id;		/* return */
    int nsemas;		/* parameter */

    /* device dependent fields */
    union {
	struct {
	    int ipl;	/* parameter */
	    int vec;	/* return */
	} vme;
    } dd; 
};

#if _MIPS_SIM == _ABI64
struct uliargs32 {
    uint pc;
    uint sp;
    uint funcarg;
    int id;
    int nsemas;
    union {
	struct {
	    int ipl;
	    int vec;
	} vme;
    } dd;
};
#endif

#define VMEVEC_ANY -1

extern void  ULI_block_intr	(void*);
extern void  ULI_unblock_intr	(void*);
extern void *ULI_register_vme	(int, void (*)(void*), void *, int, 
				 char *, int, int, int *);
extern void *ULI_register_ei	(int, void (*)(void*), void *, int,
				 char *, int);
extern void *ULI_register_pci	(int, void (*)(void*), void *, int,
				 char *, int, int);
extern int   ULI_sleep		(void*, int);
extern int   ULI_wakeup		(void*, int);

#ifdef __cplusplus
}
#endif
#endif /* __SYS_ULI_H */
