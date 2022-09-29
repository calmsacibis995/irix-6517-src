
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

#ident "$Id: Sprout.c++,v 1.3 1997/12/17 07:08:45 markgw Exp $"

#include <iostream.h>
#include <Inventor/SoPath.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkMenuItem.h>

#include "Inv.h"
#include "ModList.h"
#include "Sprout.h"
#include "Bool.h"

static char	theSproutName[16];

const char	Sprout::theCpuId = 'c';
const char	Sprout::theNodeId = 'n';
const char	Sprout::theRouterId = 'r';
const char	Sprout::theUnknownId = '\0';

OMC_String	Sprout::theShowAllNodesStr = "Show All Nodes";
OMC_String	Sprout::theHideAllNodesStr = "Hide All Nodes";
OMC_String	Sprout::theShowNodesStr = "Show Nodes";
OMC_String	Sprout::theHideNodesStr = "Hide Nodes";
OMC_String	Sprout::theShowNodeStr = "Show Node";
OMC_String	Sprout::theHideNodeStr = "Hide Node";
OMC_String	Sprout::theShowAllCpusStr = "Show All CPUs";
OMC_String	Sprout::theHideAllCpusStr = "Hide All CPUs";
OMC_String	Sprout::theShowCpusStr = "Show CPUs";
OMC_String	Sprout::theHideCpusStr = "Hide CPUs";
OMC_String	Sprout::theShowCpuStr = "Show CPU";
OMC_String	Sprout::theHideCpuStr = "Hide CPU";

static uint_t	_shownCpusTotal = 0;
static uint_t	_shownNodesTotal = 0;
static uint_t	_shownCpus = 0;
static uint_t	_hiddenCpus = 0;
static uint_t	_shownNodes = 0;
static uint_t	_hiddenNodes = 0;

static uint_t	_selId = 0;
static char	_selType = Sprout::theUnknownId;

Sprout		*theSprout;

SproutCpu::~SproutCpu()
{
    uint_t	i;

    _switch->unref();
    for (i = 0; i < _selPaths.length(); i++)
	_selPaths[i]->unref();
    _selPaths.removeAll();
}

SproutCpu::SproutCpu(uint_t node)
: _id(0),
  _node(node),
  _switch(new SoSwitch),
  _selPaths(4)
{
    SproutCpu *me = this;
    _switch->whichChild.setValue(SO_SWITCH_NONE);
    theSprout->_cpus.append(me);
    _id = theSprout->_cpus.length() - 1;
}

const char *
SproutCpu::name() const
{
    sprintf(theSproutName, "%c%d", Sprout::theCpuId, _id);
    return theSproutName;
}

SproutNode::~SproutNode()
{
    uint_t	i;

    _switch->unref();
    for (i = 0; i < _cpus.length(); i++)
	delete _cpus[i];
    for (i = 0; i < _selPaths.length(); i++)
	_selPaths[i]->unref();
    _selPaths.removeAll();
}

SproutNode::SproutNode(uint_t router)
: _id(0),
  _router(router),
  _switch(new SoSwitch),
  _cpus(2),
  _selPaths(1)
{
    SproutNode *me = this;
    _switch->whichChild.setValue(SO_SWITCH_NONE);
    theSprout->_nodes.append(me);
    _id = theSprout->_nodes.length() - 1;
}

const char *
SproutNode::name() const
{
    sprintf(theSproutName, "%c%d", Sprout::theNodeId, _id);
    return theSproutName;
}

SproutCpu *
SproutNode::addCpu()
{
    SproutCpu *cpu = new SproutCpu(_id);
    _cpus.append(cpu);
    return cpu;
}

SproutRouter::~SproutRouter()
{
    uint_t	i;
    for (i = 0; i < _nodes.length(); i++)
	delete _nodes[i];
}

SproutRouter::SproutRouter()
: _id(0),
  _nodes(4)
{
    SproutRouter *me = this;
    theSprout->_routers.append(me);
    _id = theSprout->_routers.length() - 1;
}

const char *
SproutRouter::name() const
{
    sprintf(theSproutName, "%c%d", Sprout::theRouterId, _id);
    return theSproutName;
}

SproutNode *
SproutRouter::addNode()
{
    SproutNode *node = new SproutNode(_id);
    _nodes.append(node);
    return node;
}

Sprout::~Sprout()
{
    uint_t	i;

    for (i = 0; i < _routers.length(); i++)
	delete _routers[i];
}

Sprout::Sprout()
: _cpus(8),
  _nodes(4),
  _routers(2),
  _alwaysShowNodes(OMC_false),
  _showNodeButton(0),
  _hideNodeButton(0),
  _showCpuButton(0),
  _hideCpuButton(0)
{
}

void
Sprout::addMenu(VkSubMenu *menu)
{
    menu->addSeparator();

    _showNodeButton = menu->addAction(theShowAllNodesStr.ptr(),
				      &Sprout::showNodeCB);
    _hideNodeButton = menu->addAction(theHideAllNodesStr.ptr(),
				      &Sprout::hideNodeCB);
    _showCpuButton = menu->addAction(theShowAllCpusStr.ptr(),
				     &Sprout::showCpuCB);
    _hideCpuButton = menu->addAction(theHideAllCpusStr.ptr(),
				     &Sprout::hideCpuCB);
}

void
Sprout::updateMenu()
{
    if (theModList->numSelected() == 1) {

	if (_selType == Sprout::theCpuId) {
	    _showNodeButton->setLabel(theShowNodeStr.ptr());
	    _showNodeButton->deactivate();
	    _hideNodeButton->setLabel(theHideNodeStr.ptr());
	    if (_alwaysShowNodes)
		_hideNodeButton->deactivate();
	    else
		_hideNodeButton->activate();
	    _showCpuButton->setLabel(theShowCpuStr.ptr());
	    _showCpuButton->deactivate();
	    _hideCpuButton->setLabel(theHideCpuStr.ptr());
	    _hideCpuButton->activate();
	}
	else if (_selType == Sprout::theNodeId) {
	    _showNodeButton->setLabel(theShowNodeStr.ptr());
	    _hideNodeButton->setLabel(theHideNodeStr.ptr());
	    _showNodeButton->deactivate();
	    if (_alwaysShowNodes)
		_hideNodeButton->deactivate();
	    else
		_hideNodeButton->activate();
	}
	else if (_selType == Sprout::theRouterId) {
	    if (_hiddenNodes == 1) {
		_showNodeButton->setLabel(theShowNodeStr.ptr());
		_showNodeButton->activate();
	    }
	    else if (_hiddenNodes > 1) {
		_showNodeButton->setLabel(theShowNodesStr.ptr());
		_showNodeButton->activate();
	    }
	    else {
		_showNodeButton->setLabel(theShowNodeStr.ptr());
		_showNodeButton->deactivate();
	    }

	    if (_shownNodes == 1) {
		_hideNodeButton->setLabel(theHideNodeStr.ptr());
		if (_alwaysShowNodes)
		    _hideNodeButton->deactivate();
		else
		    _hideNodeButton->activate();
	    }
	    else if (_shownNodes > 1) {
		_hideNodeButton->setLabel(theHideNodesStr.ptr());
		if (_alwaysShowNodes)
		    _hideNodeButton->deactivate();
		else
		    _hideNodeButton->activate();
	    }
	    else {
		_hideNodeButton->setLabel(theHideNodeStr.ptr());
		_hideNodeButton->deactivate();
	    }
	}

	if (_selType == Sprout::theNodeId || _selType == Sprout::theRouterId) {
	    if (_hiddenCpus == 1) {
		_showCpuButton->setLabel(theShowCpuStr.ptr());
		_showCpuButton->activate();
	    }
	    else if (_hiddenCpus > 1) {
		_showCpuButton->setLabel(theShowCpusStr.ptr());
		_showCpuButton->activate();
	    }
	    else {
		_showCpuButton->setLabel(theShowCpuStr.ptr());
		_showCpuButton->deactivate();
	    }

	    if (_shownCpus == 1) {
		_hideCpuButton->setLabel(theHideCpuStr.ptr());		
		_hideCpuButton->activate();
	    }
	    else if (_shownCpus > 1) {
		_hideCpuButton->setLabel(theHideCpusStr.ptr());
		_hideCpuButton->activate();
	    }
	    else {
		_hideCpuButton->setLabel(theHideCpuStr.ptr());		
		_hideCpuButton->deactivate();
	    }
		
	}
    }

    if (theModList->numSelected() != 1 || _selType == theUnknownId) {
	_showNodeButton->setLabel(theShowAllNodesStr.ptr());
	_hideNodeButton->setLabel(theHideAllNodesStr.ptr());
	_showCpuButton->setLabel(theShowAllCpusStr.ptr());
	_hideCpuButton->setLabel(theHideAllCpusStr.ptr());

	if (_shownNodesTotal == _nodes.length())
	    _showNodeButton->deactivate();
	else
	    _showNodeButton->activate();

	if (_shownNodesTotal == 0 || _alwaysShowNodes)
	    _hideNodeButton->deactivate();
	else
	    _hideNodeButton->activate();

	if (_shownCpusTotal == _cpus.length())
	    _showCpuButton->deactivate();
	else
	    _showCpuButton->activate();

	if (_shownCpusTotal == 0)
	    _hideCpuButton->deactivate();
	else
	    _hideCpuButton->activate();
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::updateMenu: " << endl << *this << endl;
#endif
}

uint_t
Sprout::findToken(const SoPath *path, char &type)
{
    SoNode	*node = NULL;
    char	*str = NULL;
    int		id = 0;
    int		i;

    type = Sprout::theUnknownId;

    for (i = path->getLength() - 1; i >= 0; --i) {
        node = path->getNode(i);
        str = (char *)(node->getName().getString());
        if (strlen(str) && 
	    (str[0] == Sprout::theCpuId ||
	     str[0] == Sprout::theNodeId ||
	     str[0] == Sprout::theRouterId)) {
	    sscanf(str, "%c%u", &type, &id);
	    break;
	}
    }

    return id;
}

void
Sprout::updateState()
{
    SproutNode		*node;
    SproutRouter	*router;
    uint_t		i;
    uint_t		j;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::updateState: num selections = " 
	     << theModList->numSelected() << endl;
#endif

    if (theModList->numSelected() == 1 && _selType != Sprout::theUnknownId) {
	_shownCpus = 0;
	_hiddenCpus = 0;
	_shownNodes = 0;
	_hiddenNodes = 0;

	// Determine number of cpus
	if (_selType == Sprout::theNodeId) {
	    node = _nodes[_selId];
	    for (i = 0; i < node->_cpus.length(); i++) {
		if (node->_cpus[i]->_switch->whichChild.getValue() == SO_SWITCH_ALL)
		    _shownCpus++;
		else
		    _hiddenCpus++;
	    }
	}

	// Determine number of cpus and nodes
	else if (_selType == Sprout::theRouterId) {
	    router = _routers[_selId];
	    for (i = 0; i < router->_nodes.length(); i++) {
		node = router->_nodes[i];
		if (node->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
		    _shownNodes++;
		    for (j = 0; j < node->_cpus.length(); j++) {
			if (node->_cpus[j]->_switch->whichChild.getValue() == SO_SWITCH_ALL)
			    _shownCpus++;
			else
			    _hiddenCpus++;
		    }
		}
		else {
		    _hiddenNodes++;
		    _hiddenCpus += node->_cpus.length();
		}
	    }
	}
    }

    updateMenu();
}

void
Sprout::selCB(INV_ModList *modList, SoPath *path)
{
    SoPath	*newPath;
    int		id;
    char	type;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::selCB:" << endl;
#endif

    id = findToken(path, type);
    if (type != Sprout::theUnknownId) {
	if (modList->numSelected() == 1) {
	    _selId = id;
	    _selType = type;
	}
	if (type == Sprout::theCpuId) {
	    newPath = path->copy(0,0);
	    newPath->ref();
	    theSprout->_cpus[id]->_selPaths.append(newPath);
	}
	else if (type == Sprout::theNodeId) {
	    newPath = path->copy(0,0);
	    newPath->ref();
	    theSprout->_nodes[id]->_selPaths.append(newPath);
	}

	theSprout->updateState();
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::selCB: Done" << endl;
#endif
}

void
Sprout::deselCB(INV_ModList *modList, SoPath *path)
{
    SproutNode	*node;
    SproutCpu	*cpu;
    int		id;
    char	type;
    uint_t	i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::deselCB:" << endl;
#endif

    id = findToken(path, type);
    if (type != Sprout::theUnknownId) {

	if (type == Sprout::theCpuId) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "Sprout::deselCB: Removing path from cpu[" << id
		     << ']' << endl;
#endif

	    cpu = theSprout->_cpus[id];
	    for (i = 0; i < cpu->_selPaths.length(); i++) {
		if (cpu->_selPaths[i] == path) {
		    cpu->_selPaths[i]->unref();
		    cpu->_selPaths.remove(i);

#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			cerr << "Sprout::deselCB: path " << i << " removed" 
			     << endl;
#endif

		    break;
		}
	    }
	}
	else if (type == Sprout::theNodeId) {

	    node = theSprout->_nodes[id];

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "Sprout::deselCB: Removing path from node[" << id
		     << "] from " << node->_selPaths.length() << " paths"
		     << endl;
#endif
	    for (i = 0; i < node->_selPaths.length(); i++) {
		if (node->_selPaths[i] == path) {
		    node->_selPaths[i]->unref();
		    node->_selPaths.remove(i);

#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			cerr << "Sprout::deselCB: path " << i << " removed" 
			     << endl;
#endif

		    break;
		}
	    }
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "Sprout::deselCB: No paths to remove for type " << type 
		 << endl;
#endif

	// Find only selected object
	if (modList->numSelected() == 1) {
	    const SoPath *onePath = modList->oneSelPath();
	    id = findToken(onePath, type);
	    if (type != theUnknownId) {
		_selId = id;
		_selType = type;
	    }
	}

	theSprout->updateState();
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::deselCB: Done" << endl;
#endif
}

void
Sprout::showNodeCB(Widget, XtPointer, XtPointer)
{
    SproutRouter	*router;
    SproutNode		*node;
    uint_t		i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::showNodeCB:" << endl;
#endif

    if (theModList->numSelected() == 1 && _selType != theUnknownId) {
	if (_selType == Sprout::theRouterId) {
	    router = theSprout->_routers[_selId];
	    for (i = 0; i < router->_nodes.length(); i++) {
		node = router->_nodes[i];
		if (node->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
		    node->_switch->whichChild.setValue(SO_SWITCH_ALL);
		    _shownNodesTotal++;
		    _shownNodes++;
		    _hiddenNodes--;
		}
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_selType == Sprout::theCpuId || 
		_selType == Sprout::theNodeId)
		cerr << "Sprout::showNodeCB: State failure: _selType == "
		     << _selType << " != " << Sprout::theRouterId << endl;
	    else if (_selType == Sprout::theRouterId && _hiddenNodes != 0)
		cerr << "Sprout::showNodeCB: State failure: hiddenNodes = "
		     << _hiddenNodes << " != 0" << endl;
	    else if (_selType == Sprout::theRouterId &&
		     _shownNodes != router->_nodes.length())
		cerr << "Sprout::showNodeCB: State failure: shownNodes = "
		     << _shownNodes << " != "
		     << router->_nodes.length() << endl;
	}
#endif

    }
    else {
	for (i = 0; i < theSprout->_nodes.length(); i++) {
	    node = theSprout->_nodes[i];
	    if (node->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
		node->_switch->whichChild.setValue(SO_SWITCH_ALL);
		_shownNodesTotal++;
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_shownNodesTotal != theSprout->_nodes.length())
		cerr << "Sprout::showNodeCB: State failure: shownNodesTotal == "
		     << _shownNodesTotal << " != "
		     << theSprout->_nodes.length() << endl;
	}
#endif
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::showNodeCB: Done" << endl;
#endif

    theSprout->updateState();
}

void
Sprout::hideNodeCB(Widget, XtPointer, XtPointer)
{
    SproutCpu		*cpu;
    SproutRouter	*router;
    SproutNode		*node;
    uint_t		i;
    uint_t		j;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::hideNodeCB:" << endl;
#endif

    if (theModList->numSelected() == 1 && _selType != theUnknownId) {
	if (_selType == Sprout::theNodeId) {
	    node = theSprout->_nodes[_selId];
	    node->_switch->whichChild.setValue(SO_SWITCH_NONE);
	    deselect(node);
	    _shownNodesTotal--;

	    for (i = 0; i < node->_cpus.length(); i++) {
		cpu = node->_cpus[i];
		if (cpu->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
		    cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
		    deselect(cpu);
		    _shownCpusTotal--;
		    _shownCpus--;
		    _hiddenCpus++;
		}
	    }
	}
	else if (_selType == Sprout::theRouterId) {
	    router = theSprout->_routers[_selId];
	    for (i = 0; i < router->_nodes.length(); i++) {
		node = router->_nodes[i];
		if (node->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
		    node->_switch->whichChild.setValue(SO_SWITCH_NONE);
		    deselect(node);
		    _shownNodesTotal--;
		    _shownNodes--;
		    _hiddenNodes++;
		    for (j = 0; j < node->_cpus.length(); j++) {
			cpu = node->_cpus[j];
			if (cpu->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
			    cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
			    deselect(cpu);
			    _shownCpusTotal--;
			    _shownCpus--;
			    _hiddenCpus++;
			}
		    }
		}
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_selType == Sprout::theCpuId)
		cerr << "Sprout::showNodeCB: State failure: _selType == "
		     << _selType << " != " << Sprout::theRouterId 
		     << " || " << Sprout::theNodeId << endl;
	    else if (_shownCpus != 0)
		cerr << "Sprout::hideNodeCB: State failure: shownCpus = "
		     << _shownCpus << " != 0" << endl;
	    else if (_selType == Sprout::theNodeId &&
		     _hiddenCpus != node->_cpus.length())
		cerr << "Sprout::hideNodeCB: State failure: hiddenCpus = "
		     << _hiddenCpus << " != " << node->_cpus.length() << endl;
	    else if (_selType == Sprout::theRouterId &&
		     _hiddenNodes != router->_nodes.length())
		cerr << "Sprout::hideNodeCB: State failure: hiddenNodes = "
		     << _hiddenNodes << " != " << router->_nodes.length()
		     << endl;
	    else if (_shownCpusTotal == 0 && _shownCpus != 0)
		cerr << "Sprout::hideNodeCB: State failure: shownCpus == "
		     << _shownCpus << " != shownCpusTotal == 0" << endl;
	    else if (_shownNodesTotal == 0 && _shownNodes != 0)
		cerr << "Sprout::hideNodeCB: State failure: shownNodes == "
		     << _shownNodes << " != shownNodesTotal == 0" << endl;
	}
#endif

    }
    else {
	for (i = 0; i < theSprout->_nodes.length(); i++) {
	    node = theSprout->_nodes[i];
	    if (node->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
		node->_switch->whichChild.setValue(SO_SWITCH_NONE);
		_shownNodesTotal--;
		deselect(node);
	    }
	}

	if (_shownCpusTotal > 0) {
	    for (i = 0; i < theSprout->_cpus.length(); i++) {
		cpu = theSprout->_cpus[i];
		if (cpu->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
		    cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
		    _shownCpusTotal--;
		    deselect(cpu);
		}
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_shownCpusTotal != 0)
		cerr << "Sprout::hideNodeCB: State failure: shownCpusTotal == "
		     << _shownCpusTotal << " != 0" << endl;
	    else if (_shownNodesTotal != 0)
		cerr << "Sprout::hideNodeCB: State failure: shownNodesTotal == "
		     << _shownNodesTotal << " != 0" << endl;
	}
#endif

    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::hideNodeCB: Done" << endl;
#endif

    theSprout->updateState();
}

void
Sprout::showCpuCB(Widget, XtPointer, XtPointer)
{
    SproutCpu		*cpu;
    SproutRouter	*router;
    SproutNode		*node;
    uint_t		i;
    uint_t		j;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::showCpuCB:" << endl;
#endif

    if (theModList->numSelected() == 1 && _selType != theUnknownId) {
	if (_selType == Sprout::theNodeId) {
	    node = theSprout->_nodes[_selId];
	    for (i = 0; i < node->_cpus.length(); i++)
		if (node->_cpus[i]->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
		    node->_cpus[i]->_switch->whichChild.setValue(SO_SWITCH_ALL);
		    _shownCpusTotal++;
		    _shownCpus++;
		    _hiddenCpus--;
		}
	}
	else if (_selType == Sprout::theRouterId) {
	    router = theSprout->_routers[_selId];
	    for (i = 0; i < router->_nodes.length(); i++) {
		node = router->_nodes[i];
		if (node->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
		    node->_switch->whichChild.setValue(SO_SWITCH_ALL);
		    _shownNodesTotal++;
		    _shownNodes++;
		    _hiddenNodes--;
		}
		for (j = 0; j < node->_cpus.length(); j++) {
		    cpu = node->_cpus[j];
		    if (cpu->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
			cpu->_switch->whichChild.setValue(SO_SWITCH_ALL);
			_shownCpusTotal++;
			_shownCpus++;
			_hiddenCpus--;
		    }
		}
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_selType == Sprout::theCpuId)
		cerr << "Sprout::showCpuCB: State failure: _selType == "
		     << _selType << " != " << Sprout::theNodeId << " || "
		     << Sprout::theRouterId << endl;
	    else if (_selType == Sprout::theNodeId && _hiddenCpus != 0)
		cerr << "Sprout::showCpuCB: State failure: hiddenCpus == "
		     << _hiddenCpus << " != 0" << endl;
	    else if (_selType == Sprout::theNodeId && 
		     _shownCpus != node->_cpus.length())
		cerr << "Sprout::showCpuCB: State failure: shownCpus == "
		     << _shownCpus << " != " << node->_cpus.length() << endl;
	    else if (_selType == Sprout::theRouterId && _hiddenNodes != 0)
		cerr << "Sprout::showCpuCB: State failure: hiddenNodes == "
		     << _hiddenNodes << " != 0" << endl;
	    else if (_selType == Sprout::theRouterId && _hiddenCpus != 0)
		cerr << "Sprout::showCpuCB: State failure: hiddenCpus  == "
		     << _hiddenCpus  << " != 0" << endl;
	    else if (_selType == Sprout::theRouterId && 
		     _shownNodes!= router->_nodes.length())
		cerr << "Sprout::showCpuCB: State failure: shownNodes == "
		     << _shownNodes << " != " << router->_nodes.length()
		     << endl;
	}
#endif

    }
    else {
	for (i = 0; i < theSprout->_cpus.length(); i++) {
	    cpu = theSprout->_cpus[i];
	    if (cpu->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
		cpu->_switch->whichChild.setValue(SO_SWITCH_ALL);
		_shownCpusTotal++;
	    }
	}
	for (i = 0; i < theSprout->_nodes.length(); i++) {
	    node = theSprout->_nodes[i];
	    if (node->_switch->whichChild.getValue() == SO_SWITCH_NONE) {
		node->_switch->whichChild.setValue(SO_SWITCH_ALL);
		_shownNodesTotal++;
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_shownNodesTotal != theSprout->_nodes.length())
		cerr << "Sprout::showCpuCB: State failure: shownNodesTotal == "
		     << _shownNodesTotal << " != "
		     << theSprout->_nodes.length() << endl;
	    else if (_shownCpusTotal != theSprout->_cpus.length())
		cerr << "Sprout::showCpuCB: State failure: shownCpusTotal == "
		     << _shownCpusTotal << " != "
		     << theSprout->_cpus.length() << endl;
	}
#endif

    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::showCpuCB: Done" << endl;
#endif

    theSprout->updateState();
}

void
Sprout::hideCpuCB(Widget, XtPointer, XtPointer)
{
    SproutCpu		*cpu;
    SproutRouter	*router;
    SproutNode		*node;
    uint_t		i;
    uint_t		j;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::hideCpuCB:" << endl;
#endif

    if (theModList->numSelected() == 1 && _selType != theUnknownId) {
	if (_selType == Sprout::theCpuId) {
	    cpu = theSprout->_cpus[_selId];
	    cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
	    deselect(cpu);
	    _shownCpusTotal--;
	}
	else if (_selType == Sprout::theNodeId) {
	    node = theSprout->_nodes[_selId];
	    for (i = 0; i < node->_cpus.length(); i++) {
		cpu = node->_cpus[i];
		if (cpu->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
		    cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
		    deselect(cpu);
		    _shownCpusTotal--;
		    _shownCpus--;
		    _hiddenCpus++;
		}
	    }
	}
	else if (_selType == Sprout::theRouterId) {
	    router = theSprout->_routers[_selId];
	    for (i = 0; i < router->_nodes.length(); i++) {
		node = router->_nodes[i];
		for (j = 0; j < node->_cpus.length(); j++) {
		    cpu = node->_cpus[j];
		    if (cpu->_switch->whichChild.getValue() == SO_SWITCH_ALL) {
			cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
			deselect(cpu);
			_shownCpusTotal--;
			_shownCpus--;
			_hiddenCpus++;
		    }
		}
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if ((_selType == Sprout::theNodeId || _selType == Sprout::theRouterId) && 
		_shownCpus != 0)
		cerr << "Sprout::hideCpuCB: State failure: shownCpus == "
		     << _shownCpus << " != 0" << endl;
	    else if (_selType == Sprout::theNodeId && 
		     _hiddenCpus != node->_cpus.length())
		cerr << "Sprout::hideCpuCB: State failure: hiddenCpus == "
		     << _hiddenCpus << " != " << node->_cpus.length() << endl;
	    else if (_shownCpusTotal == 0 && _shownCpus != 0)
		cerr << "Sprout::hideCpuCB: State failure: shownCpus == "
		     << _shownCpus << " != shownCpusTotal == 0" << endl;
	}
#endif

    }
    else {
	for (i = 0; i < theSprout->_cpus.length(); i++) {
	    cpu = theSprout->_cpus[i];
	    if (cpu->_switch->whichChild.getValue() == SO_SWITCH_ALL) {

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    cerr << "Sprout::hideCpuCB: Hiding cpu[" << i
			 << "], num of paths = " << cpu->_selPaths.length()
			 << endl;
#endif

		cpu->_switch->whichChild.setValue(SO_SWITCH_NONE);
		_shownCpusTotal--;
		deselect(cpu);
	    }
	}

// Verify state	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (_shownCpusTotal != 0)
		cerr << "Sprout::hideCpuCB: State failure: shownCpusTotal == "
		     << _shownCpusTotal << " != 0" << endl;
	}
#endif
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::hideCpuCB: Done" << endl;
#endif

    theSprout->updateState();
}

void
Sprout::deselect(SproutCpu *cpu)
{
    uint_t i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::deselect: deselecting " << cpu->_selPaths.length()
	     << " paths from cpu[" << cpu->_id << ']' << endl;
#endif

    for (i = cpu->_selPaths.length(); i > 0; i--)
	theModList->deselectPath(cpu->_selPaths[i-1]);
}

void
Sprout::deselect(SproutNode *node)
{
    uint_t i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Sprout::deselect: deselecting " << node->_selPaths.length()
	     << " paths from node[" << node->_id << ']' << endl;
#endif

    for (i = node->_selPaths.length(); i > 0; i--)
	theModList->deselectPath(node->_selPaths[i-1]);
}

ostream&
operator<<(ostream &os, const Sprout &rhs)
{
    os << "Sprout is aware of " << rhs._routers.length() << " routers, "
       << rhs._nodes.length() << " nodes and " << rhs._cpus.length()
       << " cpus." << endl;
    if (rhs._alwaysShowNodes)
	os << "Sprout will not remove nodes" << endl;
    os << "Sprout is showing " << _shownNodesTotal << " nodes and "
       << _shownCpusTotal << " cpus." << endl;

    if (theModList->numSelected() == 1) {
	os << "Sprout is aware of a single ";
	if (_selType == Sprout::theCpuId)
	    os << "cpu";
	else if (_selType == Sprout::theNodeId)
	    os << "node";
	else if (_selType == Sprout::theRouterId)
	    os << "router";
	os << "[" << _selId << "] selection" << endl;
	if (_selType == Sprout::theNodeId)
	    os << " with " << _shownCpus << " cpus shown and "
	       << _hiddenCpus << " cpus hidden" << endl;
	else if (_selType == Sprout::theRouterId) {
	    os << " with " << _shownNodes << " nodes shown and "
	       << _hiddenNodes << " nodes hidden and" << endl;
	    os << " with " << _shownCpus << " cpus shown and "
	       << _hiddenCpus << " cpus hidden" << endl;
	}
    }
    return os;
}
