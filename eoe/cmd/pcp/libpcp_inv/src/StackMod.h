/* -*- C++ -*- */

#ifndef _INV_STACKMOD_H_
#define _INV_STACKMOD_H_

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

#ident "$Id: StackMod.h,v 1.2 1997/09/05 00:48:59 markgw Exp $"

#include <iostream.h>
#include "Bool.h"
#include "Vector.h"
#include "Modulate.h"

class SoBaseColor;
class SoTranslation;
class SoScale;
class SoNode;
class SoSwitch;
class INV_Launch;

struct INV_StackBlock {
    SoSeparator		*_sep;
    SoBaseColor		*_color;
    SoScale		*_scale;
    SoTranslation	*_tran;
    INV_Modulate::State	_state;
    OMC_Bool		_selected;
};

typedef OMC_Vector<INV_StackBlock> INV_BlockList;

class INV_StackMod : public INV_Modulate
{
public:

    enum Height { unfixed, fixed, util };

private:

    static const float	theDefFillColor[];
    static const char	theStackId;

    INV_BlockList	_blocks;
    SoSwitch		*_switch;
    Height		_height;
    OMC_String		_text;
    uint_t		_selectCount;
    uint_t		_infoValue;
    uint_t		_infoMetric;
    uint_t		_infoInst;

public:

    virtual ~INV_StackMod();

    INV_StackMod(INV_MetricList *metrics,
		 SoNode *obj, 
		 Height height = unfixed);

    void setFillColor(const SbColor &col);
    void setFillColor(uint32_t packedcol);
    void setFillText(const char *str)
	{ _text = str; }

    virtual void refresh(OMC_Bool fetchFlag);

    virtual void selectAll();
    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void selectInfo(SoPath *);
    virtual void removeInfo(SoPath *);

    virtual void infoText(OMC_String &str, OMC_Bool) const;

    virtual void launch(INV_Launch &launch, OMC_Bool all) const;

    virtual void dump(ostream &) const;

private:

    INV_StackMod();
    INV_StackMod(const INV_StackMod &);
    const INV_StackMod &operator=(const INV_StackMod &);
    // Never defined

    void findBlock(SoPath *path, uint_t &metric, uint_t &inst, 
		   uint_t &value, OMC_Bool idMetric = OMC_true);
};

#endif /* _INV_STACKMOD_H_ */

