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

#ident "$Id: Args.c++,v 1.5 1997/12/18 06:23:26 markgw Exp $"

#include <string.h>
#include <stdlib.h>
#include "Args.h"

OMC_Args::~OMC_Args()
{
    uint_t	i;

    for (i = 0; i < _args.length(); i++) {
	if (_args[i] != NULL)
	    free(_args[i]);
    }
    _args.removeAll();
}

OMC_Args::OMC_Args()
: _args()
{
    char *nullPtr = NULL;
    _args.append(nullPtr);
}

OMC_Args::OMC_Args(char const* str)
: _args()
{
// TODO EXCEPTION PCP 2.0: This method should not ignore quotes which 
// combine args

    char *p = NULL;
    char *copy = strdup(str);
    char *old = copy;

    _args.append(p);

    // Skip leading spaces
    for (p = copy; *p == ' ' || *p == '\t' || *p == '\n'; old = ++p);
    // Find first word
    if (*p != '\0')
	for (p++; *p != ' ' && *p != '\t' && *p != '\n' && *p != '\0'; p++);
    else {
	free(copy);
	return;
    }

    while (*p != '\0') {
	*p = '\0';
	addFlag(old);
	// Find start of next word
	for (p++; *p == ' ' || *p == '\t' || *p == '\n'; p++);
	old = p;
	if (*p == '\0')
	    break;
	for (p++; *p != ' ' && *p != '\t' && *p != '\n' && *p != '\0'; p++);
    }

    if (*old != '\0') {
	addFlag(old);
    }

    free(copy);
}

OMC_Args::OMC_Args(const OMC_Args &rhs)
: _args(rhs._args.length())
{
    uint_t	i;
    for (i = 0; i < rhs._args.length(); i++)
    	addFlag(rhs[i]);
}

const OMC_Args &
OMC_Args::operator=(const OMC_Args &rhs)
{
    uint_t	i;

    if (this !=&rhs)
	for (i = 0; i < _args.length(); i++)
	    free(_args[i]);
	_args.removeAll();
	for (i = 0; i < rhs._args.length(); i++)
	    addFlag(rhs[i]);
    return *this;
}

void
OMC_Args::addFlag(const char *str)
{
    char *ptr = NULL;
    char *nullPtr = NULL;

    if (str != NULL) {
        ptr = strdup(str);
	_args.tail() = ptr;
	_args.append(nullPtr);
    }
    else if (_args.tail() != nullPtr)
    	_args.append(nullPtr);
}

void
OMC_Args::append(OMC_Args const& rhs)
{
    uint_t	i;

    for (i = 0; i < rhs._args.length(); i++)
	addFlag(rhs[i]);
}

void
OMC_Args::replace(int i, char const *str)
{
    free(_args[i]);
    _args[i] = strdup(str);
}

void
OMC_Args::combinedStr(OMC_String &str) const
{
    uint_t	i;

    if (_args.length()) {
        if (_args[0] != NULL)
	    str.append(_args[0]);
	else
	    str.append("(NULL)");
	for (i = 1; i < _args.length(); i++) {
	    str.appendChar(' ');
	    if (_args[i] != NULL)
		str.append(_args[i]);
	    else
	    	str.append("(NULL)");
	}
    }
}

ostream&
operator<<(ostream& os, const OMC_Args &rhs)
{
    OMC_String	str;

    rhs.combinedStr(str);
    os << str;
    return os;
}
