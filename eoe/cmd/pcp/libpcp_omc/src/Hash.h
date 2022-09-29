/* -*- C++ -*- */

#ifndef _OMC_HASH_H_
#define _OMC_HASH_H_

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

#ident "$Id: Hash.h,v 1.1 1997/08/20 05:17:26 markgw Exp $"

#include "pmapi.h"
#include "impl.h"

template<class T>
class OMC_Hash
{
private:

    __pmHashCtl	*_table;

public:

    ~OMC_Hash();

    OMC_Hash();

    int add(int key, const T &item);
    // Add copy of <item> into hash table

    int add(int key, T *item);
    // Add <item> into hash table, ~OMC_Hash() will destroy it.

    int add_cp(int key, T item)
	{ return add(key, item); }
    // Add copy <item> into hash table

    T *search(int);

private:

    OMC_Hash(const OMC_Hash &);
    const OMC_Hash &operator=(const OMC_Hash &);
    // Never defined
};

typedef OMC_Hash<int> OMC_IntHash;

#endif /* _OMC_HASH_H_ */
