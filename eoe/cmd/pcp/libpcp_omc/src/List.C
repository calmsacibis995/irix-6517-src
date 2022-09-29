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

#ident "$Id: List.C,v 1.3 1999/05/11 00:28:03 kenmcd Exp $"

#include "List.h"
#include "Bool.h"
#include "String.h"
#include "Context.h"
#include "Metric.h"

template<class T>
OMC_List<T>::~OMC_List()
{
}

template<class T>
OMC_List<T>::OMC_List(uint_t newSize, uint_t len, const T *ptr)
: _list(newSize), _len(len)
{
    uint_t i;

    assert(size() >= length());
    for (i = 0; i < length(); i++)
	_list[i] = ptr[i];
}

template<class T>
const OMC_List<T> &
OMC_List<T>::operator=(const OMC_List<T> &rhs)
{
    if (this != &rhs) {
	_list = rhs._list;
	_len = rhs._len;
    }
    return *this;
}

template<class T>
void
OMC_List<T>::push(uint_t pos, uint_t len)
{
    T		*oldPtr;
    OMC_Bool	deleteFlag = OMC_false;
    uint_t	newLen = length() + len;
    uint_t	newSize = length();
    uint_t	last = pos + len;
    uint_t	i;

    assert(pos <= length());

    if (newLen > size()) {
	if (length() > len)
	    newSize += length();
	else
	    newSize += len;
	oldPtr = _list.resizeNoCopy(newSize);
	for (i = 0; i < pos; i++)
	    _list.ptr()[i] = oldPtr[i];
	deleteFlag = OMC_true;
    }
    else
	oldPtr = _list.ptr();
    
    for (i = newLen - 1; i >= last; i--)
	_list.ptr()[i] = oldPtr[i - len];

    if (deleteFlag) 
	delete [] oldPtr;

    _len = newLen;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::insert(const T& item, uint_t pos)
{
    push(pos, 1);
    _list[pos] = item;
    return *this;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::insert(uint_t len, const T* ptr, uint_t pos)
{
    uint_t i;

    assert(_list.ptr() != ptr);

    if (len == 0)
	return *this;

    push(pos, len);
    for (i = 0; i < len; i++)
	_list[i + pos] = ptr[i];
    return *this;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::append(const T& item)
{
    if (length() >= size())
	_list.resize(length() * 2);
    _list[_len++] = item;
    return *this;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::append(uint_t len, const T* ptr)
{
    uint_t newLen = length() + len;
    uint_t oldLen = length();
    uint_t newSize;
    uint_t i;

    if (len == 0)
	return *this;

    if (newLen > size()) {
	if (length() > len)
	    newSize = length() * 2;
	else
	    newSize = newLen;
	_list.resize(newSize);
    }

    for (i = 0; i < len; i++)
	_list[i + oldLen] = ptr[i];
    _len = newLen;

    return *this;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::remove(uint_t pos, uint_t len)
{
    uint_t i;
    uint_t last = _len - len;

    assert(last <= length());

    for (i = pos; i < last; i++)
	_list[i] = _list[i + len];

    _len -= len;

    return *this;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::destroy(uint_t pos, uint_t len)
{
    uint_t i;
    uint_t last = pos + len;

    assert(last <= length());

    T* oldPtr = _list.resizeNoCopy(length() - len);

    for (i = 0; i < pos; i++)
	_list[i] = oldPtr[i];
    for (i = last; i < length(); i++)
	_list[i - len] = oldPtr[i];

    _len -= len;
    
    delete [] oldPtr;

    return *this;
}

template<class T>
OMC_List<T>& 
OMC_List<T>::destroyAll(uint newSize)
{
    T* oldPtr = _list.resizeNoCopy(newSize);
    delete [] oldPtr;
    _len = 0;
    return *this;
}

template<class T>
OMC_List<T>&
OMC_List<T>::resize(uint_t newSize)
{
    _list.resize(newSize);
    if (size() < length())
	_len = size();
    return *this;
}

#if defined(__GNUC__) && defined(EXPLICIT_TEMPLATES)
#include "list-templates.h"
#endif
