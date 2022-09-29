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

#ident "$Id: ToggleMod.c++,v 1.3 1997/11/28 03:12:48 markgw Exp $"

#include <iostream.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/SoPath.h>
#include "Inv.h"
#include "ToggleMod.h"

INV_ToggleMod::~INV_ToggleMod()
{
}

INV_ToggleMod::INV_ToggleMod(SoNode *obj, const char *label)
: INV_Modulate(NULL),
  _label(label)
{
    _root = new SoSeparator;
    _root->addChild(obj);

    if (_label.length() == 0)
	_label = "\n";

    add();
}

void
INV_ToggleMod::dump(ostream &os) const
{
    os << "INV_ToggleMod: \"" << _label << "\" has " << _list.length() 
       << " objects" << endl;
}

void
INV_ToggleMod::selectAll()
{
    uint_t	i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ToggleMod::selectAll: \"" << _label << '"' << endl;
#endif

    for (i = 0; i < _list.length(); i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ToggleMod::selectAll: Selecting [" << i << ']' 
		 << endl;
#endif

    	_list[i]->selectAll();
    }
}

uint_t
INV_ToggleMod::select(SoPath *path)
{
    uint_t	i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ToggleMod::select: \"" << _label << '"' << endl;
#endif

    theModList->selectAllOn();

    for (i = 0; i < _list.length(); i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ToggleMod::select: Selecting [" << i << ']' << endl;
#endif

	_list[i]->selectAll();
    }

    theModList->selectAllOff();

    theModList->selector()->deselect(path->getNodeFromTail(0));

    return 0;
}

uint_t
INV_ToggleMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ToggleMod::remove: " << _label << endl;
#endif
    return 0;
}

