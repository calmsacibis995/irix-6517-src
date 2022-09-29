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

#ident "$Id: Vector.C,v 1.3 1999/05/11 00:28:03 kenmcd Exp $"

#include <sys/time.h>
#include <unistd.h>

#include "Vector.h"
#include "String.h"
#include "Context.h"
#include "Metric.h"

template<class T>
OMC_Vector<T>::~OMC_Vector()
{
    delete [] _ptr;
}

template<class T>
OMC_Vector<T>::OMC_Vector(uint_t len)
: _ptr(0), _len(len)
{
    assert(_len > 0);
    _ptr = new T[_len];
}

template<class T>
OMC_Vector<T>::OMC_Vector(uint_t len, T item)
: _ptr(0), _len(len)
{
    uint_t i;

    assert(_len > 0);
    _ptr = new T[_len];
    for (i = 0; i < _len; i++)
	_ptr[i] = item;
}

template<class T>
OMC_Vector<T>::OMC_Vector(uint_t len, const T *ptr)
: _ptr(0), _len(len)
{
    uint_t i;

    assert(_len > 0);
    _ptr = new T[_len];
    for (i = 0; i < _len; i++)
	_ptr[i] = ptr[i];
}

template<class T>
OMC_Vector<T>::OMC_Vector(const OMC_Vector<T> &rhs)
: _ptr(0), _len(rhs._len)
{
    uint_t i;

    _ptr = new T[_len];
    for (i = 0; i < _len; i++)
	_ptr[i] = rhs._ptr[i];
}

template<class T>
const OMC_Vector<T> &
OMC_Vector<T>::operator=(const OMC_Vector<T> &rhs)
{
    uint_t i;

    if (this != &rhs) {
	if (_len != rhs._len) {
	    delete [] _ptr;
	    _len = rhs._len;
	    _ptr = new T[_len];
	}
	for (i = 0; i < _len; i++)
	    _ptr[i] = rhs._ptr[i];
    }
    return *this;
}

template<class T>
void
OMC_Vector<T>::resize(uint_t len)
{
    uint_t i;

    assert(len > 0);
    if (_len != len) {
	T* oldPtr = _ptr;
	_ptr = new T[len];
	for (i = 0; i < len && i < _len; i++)
	    _ptr[i] = oldPtr[i];
	delete [] oldPtr;
	_len = len;
    }
}

template<class T>
void
OMC_Vector<T>::resize(uint_t len, const T& item)
{
    uint_t i;
    uint_t oldLen = _len;

    resize(len);
    for (i = oldLen; i < _len; i++)
	_ptr[i] = item;
}

template<class T>
T*
OMC_Vector<T>::resizeNoCopy(uint_t len)
{
    assert(len > 0);

    T* oldPtr = _ptr;
    _ptr = new T[len];
    _len = len;
    return oldPtr;
}

#if defined(__GNUC__) && defined(EXPLICIT_TEMPLATES)
#include "vector-templates.h"
#endif
