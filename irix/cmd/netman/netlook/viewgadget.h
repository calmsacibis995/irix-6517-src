/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Viewing Gadget
 *
 *	$Revision: 1.2 $
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

#include "position.h"

class tuGadget;
class tuScrollBar;
class NetLook;
class NetLookGadget;
class MenuGadget;

class ViewGadget {
public:
	ViewGadget(NetLook *nl, tuGadget *, const char *instanceName);
	//~ViewGadget(void);

	// Viewport resizing
	void setViewPort(void);
	void setViewPort(const Rectf &);
	void setViewPort(tuRect &);
	void setViewPort(float, float, float, float);
	inline Rectf &getViewPort(void);

	void recalculateScrollBar(void);

	float getMinScaleValue(void);
	inline float getScaleValue(void);
	void setScaleValue(float);

protected:
	void adjustViewPort(void);
	void adjustScrollBar(void);

	// Scroll bar movement routines
	void moveX(tuGadget *);
	void pageIncX(tuGadget *);
	void pageDecX(tuGadget *);
	void incX(tuGadget *);
	void decX(tuGadget *);
	void moveY(tuGadget *);
	void pageIncY(tuGadget *);
	void pageDecY(tuGadget *);
	void incY(tuGadget *);
	void decY(tuGadget *);
	void moveZ(tuGadget *);
	void pageIncZ(tuGadget *);
	void pageDecZ(tuGadget *);
	void incZ(tuGadget *);
	void decZ(tuGadget *);

private:
	const char *name;
	tuGadget *gadget;
	NetLookGadget *nlg;
	MenuGadget *menu;

	Rectf view;
	float scaleValue;

	tuScrollBar *scrollX;
	tuScrollBar *scrollY;
	tuScrollBar *scrollZ;
};

// Inline functions
Rectf &
ViewGadget::getViewPort(void)
{
    return view;
}

float
ViewGadget::getScaleValue(void)
{
    return scaleValue;
}
