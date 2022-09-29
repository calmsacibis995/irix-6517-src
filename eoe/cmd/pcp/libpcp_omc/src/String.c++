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

#ident "$Id: String.c++,v 1.3 1997/09/05 00:53:28 markgw Exp $"

#include <stdio.h>
#include "String.h"
#include "Bool.h"

OMC_String::OMC_String(const char *str)
: _len((str == NULL ? 0 : strlen(str))), _str(_len + 1)
{
    if (_len > 0)
	strcpy(_str.ptr(), str);
    else
        _str.ptr()[0] = '\0';
}

const OMC_String &
OMC_String::operator=(const char *ptr)
{
    if (ptr == NULL) {
	_len = 0;
	_str[0] = '\0';
    }
    else if (strcmp(_str.ptr(), ptr) != 0) {
	uint_t len = strlen(ptr);
	
	if (len + 1 > size()) {
	    char *oldPtr = _str.resizeNoCopy(len + 1);
	    delete [] oldPtr;
	}
	
	strcpy(_str.ptr(), ptr);
	_len = len;
    }

    return *this;
}

const OMC_String &
OMC_String::operator=(const OMC_String &rhs)
{
    if (this != &rhs && strcmp(_str.ptr(), rhs._str.ptr()) != 0) {
	if (rhs.length() + 1 > size()) {
	    char *oldPtr = _str.resizeNoCopy(rhs.length() + 1);
	    delete [] oldPtr;
	}
	
	strcpy(_str.ptr(), rhs._str.ptr());
	_len = rhs.length();
    }
    return *this;
}

void
OMC_String::push(uint_t len)
{
    char	*oldPtr;
    OMC_Bool	deleteFlag = OMC_false;
    uint_t	newLen = length() + len;
    uint_t	newSize;
    uint_t	i;

    if (newLen + 1 >= size()) {
	if (length() > len)
	    newSize = length() * 2;
	else
	    newSize = newLen + 1;
	oldPtr = _str.resizeNoCopy(newSize);
	deleteFlag = OMC_true;
    }
    else
	oldPtr = _str.ptr();

    for (i = newLen; i >= len; i--)
	_str.ptr()[i] = oldPtr[i - len];

    if (deleteFlag)
	delete [] oldPtr;

    _len = newLen;

}

OMC_String&
OMC_String::prepend(const char *str)
{
    uint_t i;
    if (str == NULL)
	return *this;

    uint_t len = strlen(str);
    push(len);
    for (i = 0; i < len; i++)
	_str.ptr()[i] = str[i];
    return *this;
}

OMC_String&
OMC_String::prepend(const OMC_String &str)
{
    uint_t i;
    if (str.length() == 0)
	return *this;

    push(str.length());
    for (i = 0; i < str.length(); i++)
	_str.ptr()[i] = str.ptr()[i];

    return *this;
}

void
OMC_String::extend(uint_t len)
{
    uint_t newLen = length() + len;
    uint_t newSize = length();

    if (newLen + 1 >= size()) {
	if (length() > len)
	    newSize += length();
	else
	    newSize += len + 1;
	_str.resize(newSize);
    }
    _str[newLen] = '\0';
    _len = newLen;
}

OMC_String&
OMC_String::append(const char *ptr)
{
    if (ptr == NULL)
    	return *this;

    uint_t i;
    uint_t len = strlen(ptr);
    uint_t oldLen = length();

    extend(len);
    for (i = 0; i < len; i++)
	_str.ptr()[i + oldLen] = ptr[i];
    return *this;
}

OMC_String&
OMC_String::append(const OMC_String &str)
{
    uint_t i;
    uint_t oldLen = length();
    
    extend(str.length());
    for (i = 0; i < str.length(); i++)
	_str.ptr()[i + oldLen] = str.ptr()[i];
    return *this;
}

OMC_String&
OMC_String::appendInt(int value, uint_t width)
{
    char buf[32];

    sprintf(buf, "%*d", width, value);
    return append(buf);
}

OMC_String&
OMC_String::appendReal(double value, uint_t precision)
{
    char buf[32];

    sprintf(buf, "%.*g", precision, value);
    return append(buf);
}

OMC_String& 
OMC_String::truncate(uint_t len)
{ 
    assert(len <= _len); 
    _len = len; \
    _str[len] = '\0'; 
    return *this;
}

OMC_String
OMC_String::substr(uint_t pos, uint_t len) const
{
    uint_t i;

    assert(pos + len < length());
    OMC_String newStr(len + 1);
    for (i = 0; i < len; i++)
	newStr._str.ptr()[i] = _str[i + pos];
    newStr._len = len;
    newStr._str.last() = '\0'; 
    return newStr;
}

OMC_String&
OMC_String::remove(uint_t pos, uint_t len)
{
    uint_t i;
    uint_t last = pos + len;

    assert(last <= length());
    for (i = last; i < length(); i++)
	_str.ptr()[i - len] = _str.ptr()[i];
    _len -= len;
    _str[length()] = '\0';
    return *this;
}

OMC_String&
OMC_String::resize(uint_t size)
{
    _str.resize(size);
    if (size <= length()) {
	_len = size - 1;
	_str[_len] = '\0';
    }
    return *this;
}

ostream&
operator<<(ostream &os, const OMC_String &rhs)
{
    os << rhs._str.ptr();
    return os;
}
