/* -*- C++ -*- */

#ifndef _OMC_VECTOR_H_
#define _OMC_VECTOR_H_

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

#ident "$Id: Vector.h,v 1.3 1999/05/11 00:28:03 kenmcd Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include "platform_defs.h"

template<class T>
class OMC_Vector
{
private:

    T		*_ptr;
    uint_t	_len;

public:

    virtual ~OMC_Vector();

    OMC_Vector(uint_t len = 4);
    OMC_Vector(uint_t len, T item);
    OMC_Vector(uint_t len, const T* ptr);
    OMC_Vector(const OMC_Vector<T>& rhs);

    const OMC_Vector<T>& operator=(const OMC_Vector<T> &rhs);

    uint_t length() const
	{ return _len; }

    const T* ptr() const
	{ return _ptr; }
    T* ptr()
	{ return _ptr; }

    const T& operator[](uint_t pos) const
	{ assert(pos < _len); return _ptr[pos]; }
    T &operator[](uint_t pos)
	{ assert(pos < _len); return _ptr[pos]; }

    const T &last() const
	{ return _ptr[_len-1]; }
    T &last()
	{ return _ptr[_len-1]; }

    // Resize and copy old vector into new vector
    void resize(uint_t len);
    void resize(uint_t len, const T& item);
    void resizeCopy(uint_t len, T item)
	{ resize(len, item); }

    // Resize vector but do not copy or destroy old vector
    // The old vector is returned, called should delete [] it
    T* resizeNoCopy(uint_t len);
};

typedef OMC_Vector<int> OMC_IntVector;
typedef OMC_Vector<double> OMC_RealVector;

#endif /* _OMC_VECTOR_H_ */
