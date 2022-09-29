#ifndef _INV_MENU_H_
#define _INV_MENU_H_

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

#include <iostream.h>
#include "pmapi.h"
#include "String.h"
#include "Args.h"
#include "Window.h"

class VkMenuItem;

class INV_MenuItem
{
private:

    OMC_String	_name;
    OMC_Args	_script;
    VkMenuItem	*_button;

 public:

    ~INV_MenuItem();

    INV_MenuItem(char const* name, char const* script, OMC_Bool createButton);
    INV_MenuItem(char const* name, char const* script);
    INV_MenuItem(char const* name, const OMC_Args &args);
    INV_MenuItem(const INV_MenuItem &rhs);

    const OMC_String &name() const
    	{ return _name; }

    const OMC_Args &script() const
    	{ return _script; }

    const VkMenuItem &button() const
    	{ return *(_button); }

    friend ostream& operator<<(ostream& os, INV_MenuItem const& rhs);

    void buildButton();

 private:

    INV_MenuItem();
    const INV_MenuItem &operator=(const INV_MenuItem &rhs);
    // Never defined
};

typedef OMC_List<INV_MenuItem *> INV_MenuItemList;

class INV_Menu
{
 private:

    INV_MenuItemList	_items;

 public:

    ~INV_Menu();

    INV_Menu(uint_t size = 4)
	: _items(size) {}

    INV_Menu(INV_Menu const& rhs);

    uint_t length() const
	{ return _items.length(); }

    uint_t size() const
	{ return _items.size(); }

    int append(INV_MenuItem *item);
    int append(const INV_Menu &menu);
    void addVkMenuItems(VkSubMenu *menu);

    void remove(int i);

    void removeAll();

    INV_MenuItem const& operator[](int i) const
	{ return *(_items[i]); }
    INV_MenuItem& operator[](int i)
	{ return *(_items[i]); }

    INV_MenuItem const& last() const
	{ return *(_items.tail()); }
    INV_MenuItem& last()
	{ return *(_items.tail()); }

    friend ostream& operator<<(ostream& os, INV_Menu const& rhs);

 private:

    INV_Menu const& operator=(INV_Menu const &);
};

#endif /* _INV_MENU_H_ */
