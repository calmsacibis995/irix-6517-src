#ifndef _OMC_ARGS_H_
#define _OMC_ARGS_H_

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

#ident "$Id: Args.h,v 1.2 1997/10/09 00:27:47 markgw Exp $"

#include "List.h"
#include "String.h"

typedef OMC_List<char *> OMC_ArgList;

class OMC_Args
{
 private:

    OMC_ArgList	_args;

 public:

    ~OMC_Args();

    OMC_Args();
    OMC_Args(char const *str);
    OMC_Args(OMC_Args const& rhs);
    const OMC_Args &operator=(const OMC_Args &rhs);

    int argc() const
	{ return _args.length(); }
    char **argv() const
	{ return (char **)(_args.ptr()); }
    char **argv()
	{ return _args.ptr(); }

    const char *operator[](int i) const
	{ return _args[i]; }
    char *operator[](int i)
	{ return _args[i]; }

    void addFlag(const char *flag);
    void addFlag(const char *flag, const char *arg)
	{ addFlag(flag); addFlag(arg); }
    void append(OMC_Args const& rhs);

    void replace(int i, char const *str);

    void combinedStr(OMC_String &str) const;

    friend ostream& operator<<(ostream& os, OMC_Args const& rhs);
};

#endif
