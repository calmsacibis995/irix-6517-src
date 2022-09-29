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

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/utsname.h>

#include "pmapi.h"
#include "impl.h"

static struct {
    int style;
    char *name;
} known_styles[] = {
	{PM_STYLE_O32,	"mips_o32"},
	{PM_STYLE_N32,	"mips_n32"},
	{PM_STYLE_64,	"mips_64"},
	{PM_STYLE_IA32,	"ia_32"},
	{PM_STYLE_UNKNOWN,	"unknown"} /* must be last */
};

extern int	errno;
extern int	pmDebug;

#ifndef _SC_KERN_POINTERS /* for 5.3 */
#define _SC_KERN_POINTERS 0
#endif

static int
UserStyle(void)
{
    int caller_style;

#if defined(_M_IX86) /* defined by MSVC++ on Intel architecture */
    caller_style = PM_STYLE_IA32;
#elif _MIPS_SIM == _MIPS_SIM_ABI32
    caller_style = PM_STYLE_O32;
#elif _MIPS_SIM == _MIPS_SIM_NABI32
    caller_style = PM_STYLE_N32;
#elif _MIPS_SIM == _MIPS_SIM_ABI64
    caller_style = PM_STYLE_64;
#else
    caller_style = PM_STYLE_UNKNOWN;
#endif
    return caller_style;

}

static int
KernelStyle(void)
{
    struct utsname uname_info;
    long kern_pointers;
    int kern_style;

#if defined(_M_IX86)
    kern_style = PM_STYLE_IA32;
#elif defined(_MIPS_SIM)

    if (uname(&uname_info) < 0) {
	return -errno;
    }

    if (strcmp(uname_info.release, "5.3") == 0) {
	/*
	 * 5.3 is always o32
	 */
	kern_pointers = 32;
	kern_style = PM_STYLE_O32;
    }
    else {
	if ((kern_pointers = sysconf(_SC_KERN_POINTERS)) < 0) {
	    return -errno;
	}

	if (strcmp(uname_info.release, "6.2") == 0 ||
	    strcmp(uname_info.release, "6.3") == 0) {
	    /*
	     * 6.2 or 6.3.
	     * May be o32 or 64 depending on kern_pointers
	     */
	    kern_style = (kern_pointers == 32) ? PM_STYLE_O32 : PM_STYLE_64;
	}
	else {
	    /*
	     * 6.4 and later.
	     * May be n32 or 64 depending on kern_pointers.
	     */
	    kern_style = (kern_pointers == 32) ? PM_STYLE_N32 : PM_STYLE_64;
	}
    }
#else
!bozo!
#endif

    return kern_style;
}

const char *
__pmNameObjectStyle(int style)
{
    int i;

    for (i=0; known_styles[i].style != PM_STYLE_UNKNOWN; i++) {
	if (style == known_styles[i].style)
	    break;
    }

    return known_styles[i].name;
}

int
__pmGetObjectStyle(int *user, int *kernel)
{
    int k = KernelStyle();
    int u = UserStyle();
    int e = 0;

    if (u < 0)
	e = u;
    else
    if (k < 0)
	e = k;
    else {
	*user = u;
	*kernel = k;
    }

    return e;
}

int
__pmCheckObjectStyle(void)
{
    int k;
    int u;
    int e;

    if ((e = __pmGetObjectStyle(&u, &k)) < 0)
	return e;
    if (u == k)
	return 0;
    return PM_ERR_OBJSTYLE;
}
