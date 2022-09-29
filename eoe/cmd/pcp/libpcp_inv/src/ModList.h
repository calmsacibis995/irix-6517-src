/* -*- C++ -*- */

#ifndef _INV_MODLIST_H_
#define _INV_MODLIST_H_

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

#ident "$Id: ModList.h,v 1.3 1997/11/28 03:10:28 markgw Exp $"

#include <iostream.h>
#include "List.h"
#include "Modulate.h"

class SoXtViewer;
class SoSelection;
class SoEventCallback;
class SoBoxHighlightRenderAction;
class SoPath;
class SoPickedPoint;
class SoSeparator;
class INV_ModList;
class INV_Launch;
class INV_Record;

typedef void (*SelCallBack)(INV_ModList *, OMC_Bool);
typedef void (*SelInvCallBack)(INV_ModList *, SoPath *);

typedef OMC_List<INV_Modulate *> INV_ModulateList;

class INV_ModList
{
private:

    static const OMC_String	theBogusId;
    static const char		theModListId;

    SoXtViewer			*_viewer;
    SelCallBack			_selCB;
    SelInvCallBack		_selInvCB;
    SelInvCallBack		_deselInvCB;
    SoSeparator                 *_root;
    SoSelection                 *_selection;
    SoEventCallback             *_motion;
    SoBoxHighlightRenderAction  *_rendAct;

    INV_ModulateList		_list;
    OMC_IntList			_selList;
    uint_t			_current;
    uint_t			_numSel;
    uint_t			_oneSel;

    OMC_Bool			_allFlag;
    int				_allId;

public:

    ~INV_ModList();

    INV_ModList(SoXtViewer *viewer, 
		SelCallBack selCB, 
		SelInvCallBack selInvCB = NULL,
		SelInvCallBack deselInvCB = NULL);

    uint_t length() const
	{ return _list.length(); }
    uint_t numSelected() const
	{ return _numSel; }

    SoSeparator *root()
	{ return _root; }
    void setRoot(SoSeparator *root);

    SoSelection	*selector() const
	{ return _selection; }

    const char *add(INV_Modulate *obj);

    const INV_Modulate &operator[](uint_t i) const
	{ return *(_list[i]); }
    INV_Modulate &operator[](uint_t i)
	{ return *(_list[i]); }
    
    const INV_Modulate &current() const
	{ return *(_list[_current]); }
    INV_Modulate &current()
	{ return *(_list[_current]); }

    void refresh(OMC_Bool fetchFlag);

    void infoText(OMC_String &str) const;

    void launch(INV_Launch &launch, OMC_Bool all = OMC_false) const;
    void record(INV_Record &rec) const;

    void dumpSelections(ostream &os) const;
    OMC_Bool selections() const;

    void selectAllOn()
	{ _allFlag = OMC_true; _allId = _list.length(); }
    void selectAllId(SoNode *node, uint_t count);
    void selectSingle(SoNode *node);
    void selectAllOff();

    friend ostream &operator<<(ostream &os, const INV_ModList &rhs);

    static void deselectCB(void *me, SoPath *path);

    // Sprouting support

    // Get path to single selection
    const SoPath *oneSelPath() const;

    void deselectPath(SoPath *path);

private:

    static void selCB(void *me, SoPath *path);
    static void motionCB(void *me, SoEventCallback *event);
    static int findToken(const SoPath *path);

    INV_ModList(const INV_ModList &);
    const INV_ModList &operator=(const INV_ModList &);
    // Never defined
};

extern INV_ModList *theModList;

#endif /* _INV_MODLIST_H_ */
