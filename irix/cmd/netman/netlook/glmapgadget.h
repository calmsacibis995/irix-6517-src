/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	GL Map Gadget
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
class tuGLGadget;
class tuResourceDB;
class tuResourcePath;
class tuLayoutHints;

#include "position.h"

class GLMapGadget : public tuGLGadget {
public:
	GLMapGadget(NetLook *, tuGadget *, const char *);

	virtual void bindResources(tuResourceDB * = NULL,
				   tuResourcePath * = NULL);
	virtual void open(void);
	virtual void render(void);
	virtual void getLayoutHints(tuLayoutHints *);
	virtual void processEvent(XEvent *);

	void setUniverse(void);
	void setUniverse(const Rectf &);
	void setUniverse(float, float, float, float);

	void iconify(tuGadget *);

protected:
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

private:
	NetLook *netlook;
	Rectf universe;
	Rectf sweepBox;
	unsigned int dragging;
	int dragx;
	int dragy;
	unsigned int sweeping;
	int sweepx;
	int sweepy;
	unsigned int zooming;
	unsigned int selectButton;
	unsigned int adjustButton;
	unsigned int iconic;
};
