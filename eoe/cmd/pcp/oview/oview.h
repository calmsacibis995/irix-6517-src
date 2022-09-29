/* -*- C++ -*- */

#ifndef _OVIEW_H_
#define _OVIEW_H_

/*
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident "$Id: oview.h,v 1.1 1997/08/20 05:32:01 markgw Exp $"

#include "String.h"
#include "Bool.h"
#include "Args.h"
#include "Inv.h"

class Scene;
class Sprout;

//
// Globals
//

// Configuration file name
extern OMC_String	theConfigFile;

// Level of detail
extern uint_t		theDetailLevel;

// The Scene Representation
extern Scene		*theScene;

// Sprouting control
extern Sprout		*theSprout;

//
// Types
//

typedef float           RealPos[3];
typedef int             IntPos[3];

//
// Enumerations
//

enum Axis { xAxis, yAxis, zAxis };

//
// Constants
//

const float theError = 1e-6;

//
// Inline methods
//

inline int
geErr(float a, float b)
{
    return (a > (b - theError));
}

inline int
leErr(float a, float b)
{
    return (a < (b + theError));
}

inline int
eqErr(float a, float b)
{
    return (geErr(a, b) && leErr(a, b));
}

inline int
zyx(int a)
{
    return ((a + 2) % 3);
}

inline int
xor(int a, int b)
{
    return ((a && !b) || (!a && b));
}

inline float
sqr(float a)
{
    return a*a;
}

inline void *
smalloc(size_t size)
{
    void *tmp = malloc(size);
    if (tmp == NULL)
        INV_fatalMsg(_POS_, "Unable to allocate %d bytes", size);
    return tmp;
}

inline void *
srealloc(void *memptr, size_t size)
{
    void *tmp = realloc(memptr, size);
    if (tmp == NULL) 
        INV_fatalMsg(_POS_, "Unable to allocate %d bytes", size);
    return tmp;
}

inline float
round(float x)
{
    return (float)(((int)(2*x)+1)/2); // ANSI sez integer conversion truncates
}

inline float 
min(float a, float b)
{
    return (a < b) ? a : b;
}

inline int 
min(int a, int b)
{
    return (a < b) ? a : b;
}

inline float 
max(float a, float b)
{
    return (a > b) ? a : b;
}

inline int 
max(int a, int b)
{
    return (a > b) ? a : b;
}

inline float 
deg2rad(float deg)
{
    return M_PI*deg/180;
}

inline float 
rad2deg(float rad)
{
    return 180*rad/M_PI;
}

inline void 
lbound(float bound, float &val)
{
    if (val < bound) val = bound;
}

inline void 
ubound(float &val, float bound)
{
    if (val > bound) val = bound;
}

inline void 
fence(float lbound, float &val, float ubound)
{
    if (val < lbound)
        val = lbound;
    else if (val > ubound)
        val = ubound;
}

#endif /* _OVIEW_H_ */
