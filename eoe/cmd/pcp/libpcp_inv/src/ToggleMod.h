/* -*- C++ -*- */

#ifndef _INV_TOGGLEMOD_H_
#define _INV_TOGGLEMOD_H_

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

#ident "$Id: ToggleMod.h,v 1.1 1997/09/04 02:43:45 markgw Exp $"

#include <Inventor/SbString.h>
#include "Modulate.h"
#include "ModList.h"

class SoSeparator;
class SoPath;
class INV_Launch;
class INV_Record;

class INV_ToggleMod : public INV_Modulate
{
private:

    INV_ModulateList	_list;
    OMC_String		_label;

public:

    virtual ~INV_ToggleMod();

    INV_ToggleMod(SoNode *obj, const char *label);

    void addMod(INV_Modulate *mod)
    	{ _list.append(mod); }

    virtual void selectAll();
    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void selectInfo(SoPath *)
	{}
    virtual void removeInfo(SoPath *)
	{}

    virtual void infoText(OMC_String &str, OMC_Bool) const
	{ str = _label; }

    virtual void refresh(OMC_Bool)
    	{}
    virtual void launch(INV_Launch &, OMC_Bool) const
    	{}
    virtual void record(INV_Record &) const
    	{}

    virtual void dump(ostream &) const;
    void dumpState(ostream &os, INV_Modulate::State state) const;

    friend ostream &operator<<(ostream &os, const INV_ToggleMod &rhs);

private:

    INV_ToggleMod();
    INV_ToggleMod(const INV_ToggleMod &);
    const INV_ToggleMod &operator=(const INV_ToggleMod &);
    // Never defined
};

#endif /* _INV_TOGGLEMOD_H_ */
