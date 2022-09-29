// -*- C++ -*-

#ifndef _SPROUT_H_
#define _SPROUT_H_

/*
 * Copyright 1995, Silicon Graphics, Inc.
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

#ident "$Id: Sprout.h,v 1.1 1997/08/20 05:28:10 markgw Exp $"

#include <iostream.h>

#include "oview.h"
#include "Vector.h"
#include "List.h"
#include "String.h"

class SoSeparator;
class SoNode;
class SoPath;
class SoSwitch;

class VkSubMenu;
class VkMenuItem;

typedef OMC_List<SoPath *>	PathList;

struct SproutCpu
{
    uint_t	_id;
    uint_t	_node;
    SoSwitch	*_switch;
    PathList	_selPaths;

    ~SproutCpu();
    SproutCpu(uint_t node);

    const char *name() const;
};

typedef OMC_List<SproutCpu *> CpuList;

struct SproutNode
{
    uint_t	_id;
    uint_t	_router;
    SoSwitch	*_switch;
    CpuList	_cpus;
    PathList	_selPaths;

    ~SproutNode();
    SproutNode(uint_t router);

    const char *name() const;

    SproutCpu *addCpu();
};

typedef OMC_List<SproutNode *> NodeList;

struct SproutRouter
{
    uint_t	_id;
    NodeList	_nodes;

    ~SproutRouter();
    SproutRouter();

    const char *name() const;

    SproutNode *addNode();
};

typedef OMC_List<SproutRouter *> RouterList;

class Sprout
{

friend class SproutCpu;
friend class SproutNode;
friend class SproutRouter;

private:

    CpuList	_cpus;
    NodeList	_nodes;
    RouterList	_routers;

    OMC_Bool	_alwaysShowNodes;

    VkMenuItem	*_showNodeButton;
    VkMenuItem	*_hideNodeButton;
    VkMenuItem	*_showCpuButton;
    VkMenuItem	*_hideCpuButton;

    static OMC_String	theShowAllNodesStr;
    static OMC_String	theHideAllNodesStr;
    static OMC_String	theShowNodesStr;
    static OMC_String	theHideNodesStr;
    static OMC_String	theShowNodeStr;
    static OMC_String	theHideNodeStr;
    static OMC_String	theShowAllCpusStr;
    static OMC_String	theHideAllCpusStr;
    static OMC_String	theShowCpusStr;
    static OMC_String	theHideCpusStr;
    static OMC_String	theShowCpuStr;
    static OMC_String	theHideCpuStr;

public:

    static const char	theCpuId;
    static const char	theNodeId;
    static const char	theRouterId;
    static const char	theUnknownId;

    ~Sprout();

    Sprout();

    uint_t numRouters() const
	{ return _routers.length(); }
    uint_t numNodes() const
	{ return _nodes.length(); }
    uint_t numCpus() const
	{ return _cpus.length(); }

    void addMenu(VkSubMenu *menu);
    void updateMenu();

    void alwaysShowNodes()
	{ _alwaysShowNodes = OMC_true; }

    void updateState();
    static void selCB(INV_ModList *modList, SoPath *path);
    static void deselCB(INV_ModList *modList, SoPath *path);

    static void showNodeCB(Widget, XtPointer, XtPointer);
    static void hideNodeCB(Widget, XtPointer, XtPointer);
    static void showCpuCB(Widget, XtPointer, XtPointer);
    static void hideCpuCB(Widget, XtPointer, XtPointer);

    friend ostream& operator<<(ostream &os, const Sprout &rhs);

private:

    Sprout(const Sprout &);
    const Sprout &operator=(const Sprout &);
    // Never defined

    static uint_t findToken(const SoPath *path, char &type);
    static void deselect(SproutCpu *cpu);
    static void deselect(SproutNode *node);
};

extern Sprout *theSprout;

#endif /* _SPROUT_H_ */
