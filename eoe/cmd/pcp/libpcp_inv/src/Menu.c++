
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

#ident "$Id: Menu.c++,v 1.2 1997/10/24 06:00:29 markgw Exp $"

#include <stdlib.h>
#include <string.h>
#include <iostream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkSubMenu.h>

#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "Window.h"
#include "Launch.h"
#include "Menu.h"

INV_MenuItem::~INV_MenuItem()
{
}

INV_MenuItem::INV_MenuItem(char const* theName, char const* theScript, 
			   OMC_Bool createButton)
: _name(theName), _script(theScript), _button(0)
{
    if (createButton == OMC_true) {
	buildButton();
    }
}

INV_MenuItem::INV_MenuItem(char const* theName, char const* theScript)
: _name(theName), _script(theScript), _button(0)
{
    buildButton();
}

INV_MenuItem::INV_MenuItem(char const* theName, const OMC_Args &args)
: _name(theName), _script(args), _button(0)
{
    buildButton();
}

INV_MenuItem::INV_MenuItem(const INV_MenuItem &rhs)
: _name(rhs._name), _script(rhs._script), _button(0)
{
    buildButton();
}

void
INV_MenuItem::buildButton()
{
    if (_script.argc() == 0)
	return;

    _button = new VkMenuAction(_name.ptr(),
			      (XtCallbackProc) &INV_LaunchMenu::callback,
			      (XtPointer) this);

    if (_script[0][0] != '/') {
	char buf[FILENAME_MAX];
	sprintf(buf, "%s/%s", INV_Launch::launchPath(), _script[0]);
	_script.replace(0, buf);
    }
}

ostream&
operator<<(ostream& os, INV_MenuItem const& rhs)
{
    os << rhs.name() << ": " << rhs.script();
    return os;
}

INV_Menu::~INV_Menu()
{
    removeAll();
}

int
INV_Menu::append(INV_MenuItem *item)
{
    uint_t	i;

    // latter items override
    // so need to do lookup
    if (item->name().length() > 0) {
	for (i = 0; i < _items.length(); i++) {
	    if (_items[i]->name() == item->name()) {
		remove(i);
		break;
	    }
	}
	
	_items.append(item);
    }

    return length();
}

void
INV_Menu::addVkMenuItems(VkSubMenu *menu)
{
    uint_t	i;
    INV_MenuItem *item;

    // Go thru our item list and add them
    // to the Vk sub menu
    for (i = 0; i < _items.length(); i++) {
	item = _items[i];
        item->buildButton();
	menu->add((VkMenuItem *)&(item->button()));
    }
}

int 
INV_Menu::append(INV_Menu const& rhs)
{
    uint_t	i;

    for (i = 0; i < rhs._items.length(); i++)
	append(new INV_MenuItem(rhs[i]));

    return length();
}

void
INV_Menu::remove(int i)
{
    delete _items[i];
    _items.remove(i);
}

void
INV_Menu::removeAll()
{
    uint_t	i;

    for (i = 0; i < length(); i++)
    	delete _items[i];
    _items.removeAll();
}

ostream&
operator<<(ostream& os, INV_Menu const& rhs)
{
    uint_t	i;
    for (i = 0; i < rhs._items.length(); i++) {
	INV_MenuItem *item = rhs._items[i];
	os << '[' << item->name() << "]: " << item->script() << endl;
    }
    return os;
}
