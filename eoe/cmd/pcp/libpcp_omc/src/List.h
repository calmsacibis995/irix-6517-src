/* -*- C++ -*- */

#ifndef _OMC_LIST_H_
#define _OMC_LIST_H_

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

#ident "$Id: List.h,v 1.1 1997/08/20 05:18:25 markgw Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include "Vector.h"

template<class T>
class OMC_List
{
private:

    OMC_Vector<T>	_list;
    uint_t		_len;

public:

    virtual ~OMC_List();

    OMC_List(uint_t size = 4)
	: _list(size), _len(0) {}
    OMC_List(uint_t len, T item)
	: _list(len, item), _len(len) {}
    OMC_List(uint_t len, const T* ptr)
	: _list(len, ptr), _len(len) {}
    OMC_List(uint_t newSize, uint_t len, const T* ptr);
    OMC_List(const OMC_List<T> &rhs)
	: _list(rhs._list), _len(rhs._len) {}

    const OMC_List<T>& operator=(const OMC_List<T>& rhs);

    uint_t length() const
	{ return _len; }
    uint_t size() const
	{ return _list.length(); }

    const T* ptr() const
	{ return _list.ptr(); }
    T* ptr()
	{ return _list.ptr(); }

    const T& operator[](uint_t pos) const
	{ assert(pos < _len); return _list[pos]; }
    T &operator[](uint_t pos)
	{ assert(pos < _len); return _list[pos]; }

    const T &head() const
	{ assert(_len > 0); return _list[0]; }
    T &head()
	{ assert(_len > 0); return _list[0]; }

    const T &tail() const
	{ assert(_len > 0); return _list[_len - 1]; }
    T &tail()
	{ assert(_len > 0); return _list[_len - 1]; }

    OMC_List<T>& insert(const T& item, uint_t pos = 0);
    OMC_List<T>& insert(uint_t len, const T* ptr, uint_t pos = 0);
    OMC_List<T>& insert(const OMC_List<T>& list, uint_t pos = 0)
	{ return insert(list.length(), list.ptr(), pos); }
    OMC_List<T>& insertCopy(T item, uint_t pos = 0)
	{ return insert(item, pos); }

    OMC_List<T>& append(const T& item);
    OMC_List<T>& append(uint_t len, const T* ptr);
    OMC_List<T>& append(const OMC_List<T>& list)
	{ return append(list.length(), list.ptr()); }
    OMC_List<T>& appendCopy(T item)
	{ return append(item); }

    OMC_List<T>& remove(uint_t pos, uint_t len = 1);
    OMC_List<T>& removeAll()
	{ _len = 0; return *this; }

    OMC_List<T>& destroy(uint_t pos, uint_t len = 1);
    OMC_List<T>& destroyAll(uint_t newSize = 4);

    OMC_List<T>& resize(uint_t size);

    // Resize to current list length
    void sync()
	{ _list.resize(_len); }

private:

    void push(uint_t pos, uint_t len);

};

typedef OMC_List<int> OMC_IntList;
typedef OMC_List<double> OMC_RealList;

#endif /* _OMC_LIST_H_ */
