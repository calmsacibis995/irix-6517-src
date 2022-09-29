/* -*- C++ -*- */

#ifndef _OMC_STRING_H_
#define _OMC_STRING_H_

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

#ident "$Id: String.h,v 1.4 1997/09/05 00:53:48 markgw Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <iostream.h>
#include "Vector.h"
#include "List.h"

typedef OMC_Vector<char> OMC_CharVector;

class OMC_String
{
private:

    uint_t		_len;
    OMC_CharVector	_str;

    void push(uint_t len);
    void extend(uint_t len);

public:

    ~OMC_String()
	{}

    OMC_String()
	: _len(0), _str(16) { _str[0] = '\0'; }
    OMC_String(uint_t size)
	: _len(0), _str(size) { _str[0] = '\0'; }
    OMC_String(const char *str);
    OMC_String(const OMC_String &rhs)
	: _len(rhs._len), _str(rhs._str) {}

    const OMC_String& operator=(const char *ptr);
    const OMC_String& operator=(const OMC_String& rhs);

    uint_t length() const
	{ return _len; }
    uint_t size() const
	{ return _str.length(); }
	    
    const char* ptr() const
	{ return _str.ptr(); }
    char* ptr()
	{ return _str.ptr(); }

    const char &operator[](uint_t pos) const
	{ assert(pos <= length()); return _str[pos]; }
    char &operator[](uint_t pos)
	{ assert(pos <= length()); return _str[pos]; }
    
    int operator==(const OMC_String &rhs) const
	{ return (strcmp(ptr(), rhs.ptr()) == 0); }
    int operator==(const char *rhs) const
	{ return (rhs != NULL ? (strcmp(ptr(), rhs) == 0) : length() == 0); }
    
    int operator!=(const OMC_String &rhs) const
	{ return (strcmp(ptr(), rhs.ptr()) != 0); }
    int operator!=(const char *rhs) const
	{ return (rhs != NULL ? (strcmp(ptr(), rhs) != 0) : length() != 0); }
    
    OMC_String& prepend(char item)
	{ push(1); _str[0] = item; return *this; }
    OMC_String& prepend(const char *ptr);
    OMC_String& prepend(const OMC_String& str);

    OMC_String& append(const char *ptr);
    OMC_String& append(const OMC_String& str);

    // Cannot overload append method as results may be ambiguous
    OMC_String& appendChar(char item)
	{ _str[length()] = item; extend(1); return *this; }
    OMC_String& appendInt(int value, uint_t width = 1);
    OMC_String& appendReal(double value, uint_t precision = 3);

    OMC_String& truncate(uint_t len);

    OMC_String substr(uint_t pos, uint_t len) const;
    OMC_String& remove(uint_t pos, uint_t len);

    OMC_String& resize(uint_t size);

    OMC_String& sync()
	{ _str.resize(_len + 1); return *this; }

    friend ostream& operator<<(ostream &os, const OMC_String &rhs);
};

typedef OMC_List<OMC_String> OMC_StrList;

#endif /* _OMC_STRING_H_ */
