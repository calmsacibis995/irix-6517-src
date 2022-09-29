/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetLook Gadget
 *
 *	$Revision: 1.3 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

class NetLook;
class tuScrollBar;
class ViewGadget;
class tuGLGadget;
class tuResourceDB;
class tuResourcePath;
class tuLayoutHints;
class Network;
class Node;
class CfNode;
class GLMapGadget;

#include "position.h"

enum AdjustObjects { aoNone, aoNetwork, aoSegment, aoNode };

class NetLookGadget : public tuGLGadget {
public:
	NetLookGadget(NetLook *, tuGadget *, const char *);

	virtual void bindResources(tuResourceDB * = NULL,
				   tuResourcePath * = NULL);
	virtual void open(void);
	virtual void render(void);
	virtual void getLayoutHints(tuLayoutHints *);
	virtual void resize(int, int);
	virtual void processEvent(XEvent *);

	// Render only one network in front buffer
	void render(Network *, Node * = 0);

	void setUniverse(void);
	void setUniverse(const Rectf &);
	void setUniverse(float, float, float, float);
	inline Rectf &getUniverse(void);

	void position(void);

	// Class handling scolling of window
	inline void setEnclosingView(ViewGadget *);

	// Map
	inline void setMapGadget(GLMapGadget *);

	// Bitplanes
	unsigned int getAvailableBitplanes(void);

protected:
	void growUniverse(void);
	unsigned int extentOverlap(Position *, Position *);

	// Dragging
	void startDrag(int, int);
	void drag(int, int);
	void stopDrag(int, int);

	// Sweeping
	void startSweep(int, int);
	void sweep(int, int);
	void stopSweep(int, int);

	// Zooming
	void startZoom(int, int);
	void zoom(int, int);
	void stopZoom(int, int);

	// Picking
	void pickIt(int, int);

	// Adjusting
	void startAdjust(int, int, unsigned int);
	void adjust(int, int);
	void stopAdjust(int, int);

	// Adjusting functions
	Node *getNextUnhiddenNode(Node *);
	Node *getPrevUnhiddenNode(Node *);
	Network *getNextUnhiddenNetwork(Network *);
	Network *getPrevUnhiddenNetwork(Network *);
	Network *getNextUnhiddenSegment(Network *);
	Network *getPrevUnhiddenSegment(Network *);
	void swapNode(Node *, Node *);
	void swapNetwork(Network *, Network *);
	void swapSegment(Network *, Network *);
	void swap(CfNode *, CfNode *);

private:
	NetLook *netlook;
	ViewGadget *ev;
	GLMapGadget *mg;
	long zmax;
	Rectf universe;
	unsigned int selectButton;
	unsigned int adjustButton;
	unsigned int dragging;
	int dragx;
	int dragy;
	unsigned int sweeping;
	Rectf sweepBox;
	int sweepx;
	int sweepy;
	unsigned int zooming;
	unsigned int adjusting;
	enum AdjustObjects adjustType;
	void *adjustPrev;
	void *adjustPick;
	void *adjustNext;
};

Rectf &
NetLookGadget::getUniverse(void)
{
    return universe;
}

void
NetLookGadget::setEnclosingView(ViewGadget *v)
{
    ev = v;
}

void
NetLookGadget::setMapGadget(GLMapGadget *g)
{
    mg = g;
}
