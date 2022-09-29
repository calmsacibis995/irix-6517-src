/* -*- C++ -*- */

#ifndef _INV_LAUNCHMENU_H_
#define _INV_LAUNCHMENU_H_

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

#include "Menu.h"

class INV_LaunchMenu
{
private:

    INV_Menu	_menuItems;
    VkSubMenu	*_menu;

public:

    ~INV_LaunchMenu();

    INV_LaunchMenu(VkSubMenu *menu);

    uint_t numItems() const
	{ return _menuItems.length(); }

    const INV_MenuItem &operator[](int i) const
	{ return _menuItems[i]; }
    INV_MenuItem &operator[](int i)
	{ return _menuItems[i]; }

    static void callback(Widget, XtPointer, XtPointer);
    
private:

    void parseConfig(char const* path);

    INV_LaunchMenu(INV_LaunchMenu const&);
    INV_LaunchMenu const& operator=(INV_LaunchMenu const&);
    // Never defined
};

#endif /* _INV_LAUNCHMENU_H_ */
