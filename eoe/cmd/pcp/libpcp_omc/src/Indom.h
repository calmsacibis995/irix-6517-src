/* -*- C++ -*- */

#ifndef _OMC_INDOM_H_
#define _OMC_INDOM_H_

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

#ident "$Id: Indom.h,v 1.3 1999/05/11 00:28:03 kenmcd Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <iostream.h>
#include <assert.h>
#include "pmapi.h"
#include "impl.h"
#include "List.h"
#include "String.h"

class OMC_Desc;

class OMC_Indom
{
private:

    int		_sts;
    pmInDom	_id;
    int		_numInsts;
    OMC_IntList	_instlist;
    OMC_StrList	_namelist;
    
public:

    ~OMC_Indom();

    OMC_Indom(int type, OMC_Desc &desc);

    int status() const
	{ return _sts; }
    int id() const
	{ return _id; }
    int numInsts() const
	{ return _numInsts; }
    int inst(uint_t index) const
	{ return _instlist[index]; }
    const OMC_String &name(uint_t index) const
	{ return _namelist[index]; }

    friend ostream &operator<<(ostream &os, const OMC_Indom &indom);

    OMC_Indom();
    // OMC_Indom(const OMC_Indom &);
    const OMC_Indom &operator=(const OMC_Indom &);
    // Never defined
};

#endif /* _OMC_INDOM_H_ */
