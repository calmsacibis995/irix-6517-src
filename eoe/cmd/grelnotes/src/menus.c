/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 * 			All Rights Reserved.				  *
 *									  *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.; *
 * the contents of this file may not be disclosed to third parties,	  *
 * copied or duplicated in any form, in whole or in part, without the	  *
 * prior written permission of Silicon Graphics, Inc.			  *
 *									  *
 * RESTRICTED RIGHTS LEGEND:						  *
 *	Use, duplication or disclosure by the Government is subject to    *
 *	restrictions as set forth in subdivision (c)(1)(ii) of the Rights *
 *	in Technical Data and Computer Software clause at DFARS 	  *
 *	252.227-7013, and/or in similar or successor clauses in the FAR,  *
 *	DOD or NASA FAR Supplement. Unpublished - rights reserved under   *
 *	the Copyright Laws of the United States.			  *
 **************************************************************************
 *
 * File: menus.c
 *
 * Description: Creates the application menu bar, and associated menu
 *	widgets. The appripriate callbacks are also registered.
 *
 **************************************************************************/


#ident "$Revision: 1.4 $"


#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeBG.h>

#include "grelnotes.h"


#define MAX_ITEMS_PER_PANE	20


/* Menu item structure */

typedef struct _menu_item {
    char *name;				/* Item name */
    void (*func)();			/* Callback to be invoked */
    caddr_t data;			/* Client data for callback */
    Boolean insensitive;		/* TRUE = insensitive */
    Widget button;			/* Assigned to button holding item */
} menu_item_t;

typedef struct {
    char *name;				/* Cascade button name */
    Boolean insensitive;		/* TRUE = insensitive */
    Boolean help;			/* TRUE = help item */
    int nitems;				/* Number of items in menu */
    menu_item_t *items;			/* List of menu items */
} menu_t;


static void create_menu(Widget, menu_t*, Widget*, Widget*);
static void add_menu_items(Widget, menu_item_t*, int);
static void clear_menu_items(Widget);
static void update_disp(Widget, XtPointer, XtPointer);


/* Menu item initialization */

/* 	File menu pane */
static menu_item_t file_menu_items[] = {
#ifdef notdef
	    { "searchMenuButton", search_handler, NULL, TRUE, NULL },
#endif
	    { "printMenuButton", print_handler, NULL, TRUE, NULL },
	    { "updateMenuButton", scan, NULL, FALSE, NULL },
	    { "quitMenuButton", quit, NULL, FALSE, NULL },
	};

/*	Help menu pane */
static menu_item_t help_menu_items[] = {
	    { "progHelpMenuButton", display_help, (caddr_t)PROGRAM_HELP,
								FALSE, NULL },
	    { "verHelpMenuButton", display_help, (caddr_t)VERSION_HELP,
								FALSE, NULL },
	};

/*	Menu bar items */

static menu_t file_menu_data[] = {
	        { "fileMenuButton", FALSE, FALSE,
		  XtNumber(file_menu_items), file_menu_items
		}
	    };

static menu_t product_menu_data[] = {
	        { "productsMenuButton", TRUE, FALSE, 0, NULL }
	    };

static menu_t chapter_menu_data[] = {
	        { "chaptersMenuButton", TRUE, FALSE, 0, NULL }
	    };

static menu_t more_menu_data[] = {
	        { "moreMenuButton", FALSE, FALSE, 0, NULL }
	    };

static menu_t help_menu_data[] = {
	        { "helpMenuButton", FALSE, TRUE,
		  XtNumber(help_menu_items), help_menu_items
		}
	    };

static Widget product_pane, product_button;
static Widget chapter_pane, chapter_button;


/**************************************************************************
 *
 * Function: create_appmenu
 *
 * Description: Supervises the creation of the application menu bar,
 *	buttons and pulldowns. Callback registration is also performed.
 *
 * Parameters: 
 *	parent (I) - parent widget
 *
 * Return: The created menubar widget is returned.
 *
 **************************************************************************/

Widget create_appmenu(Widget parent)
{
    Widget menubar;
    Widget pane, button;

    /*
     * Create the app menubar
     */
     menubar = XmCreateMenuBar(parent, "menuBar", NULL, 0);
     XtManageChild(menubar);

    /*
     * Add the menu items, also registers callbacks
     */
    create_menu(menubar, file_menu_data, &pane, &button);
    create_menu(menubar, product_menu_data, &product_pane, &product_button);
    create_menu(menubar, chapter_menu_data, &chapter_pane, &chapter_button);
    create_menu(menubar, help_menu_data, &pane, &button);

    return menubar;
}


/**************************************************************************
 *
 * Function: build_prodmenu
 *
 * Description: Builds the product selection menu by searching for
 *	products and building the correponding menu structure.
 *
 * Parameters: none
 *
 * Return: Number of products found
 *
 **************************************************************************/

int build_prodmenu(PRODUCT_LIST *plist)
{
    int i;
    static menu_item_t *prod_menu_items = 0;
    DEFARGS(5);

    /*
     * If there are no products with relnotes gray out the menubar
     * Product item
     */
    if (plist->nproducts == 0) {
	STARTARGS;
	SETARG(XmNsensitive, FALSE);
        XtSetValues(product_button, ARGLIST);
	return 0;
    }

    /*
     * Allocate room for the Product menu items
     */
    if (prod_menu_items)
	XtFree((char*)prod_menu_items);
    prod_menu_items = (menu_item_t*)XtMalloc(plist->nproducts *
						sizeof(menu_item_t));

    /*
     * Put the products in the list
     */
    for (i = 0; i < plist->nproducts; i++) {
	prod_menu_items[i].name = plist->products[i].title;
	prod_menu_items[i].func = display_product;
	prod_menu_items[i].data = (XtPointer)&plist->products[i];
	prod_menu_items[i].insensitive = FALSE;
    }

    /*
     * Clear old Product menu and build new one
     */
    clear_menu_items(product_pane);
    add_menu_items(product_pane, prod_menu_items, plist->nproducts);
    STARTARGS;
    SETARG(XmNsensitive, TRUE);
    XtSetValues(product_button, ARGLIST);

    return plist->nproducts;
}


/**************************************************************************
 *
 * Function: build_chapmenu
 *
 * Description: Builds the chapter selection menu based on the specified
 *	product.
 *
 * Parameters:
 *	product (I) - product from which to build chapter menu
 *
 * Return: Number of chapters found
 *
 **************************************************************************/

int build_chapmenu(PRODUCT *product)
{
    int i;
    static menu_item_t *chap_menu_items = 0;
    DEFARGS(5);

    if (product->nchapters == 0) {
	STARTARGS;
	SETARG(XmNsensitive, FALSE);
        XtSetValues(chapter_button, ARGLIST);
	return 0;
    }
    if (chap_menu_items)
	XtFree((char*)chap_menu_items);
    chap_menu_items = (menu_item_t*)XtMalloc(product->nchapters *
						sizeof(menu_item_t));
    for (i = 0; i < product->nchapters; i++) {
	chap_menu_items[i].name = product->chapters[i].title;
	chap_menu_items[i].func = display_chapter;
	chap_menu_items[i].data = (XtPointer)&product->chapters[i];
	chap_menu_items[i].insensitive = FALSE;
    }

    clear_menu_items(chapter_pane);
    add_menu_items(chapter_pane, chap_menu_items, product->nchapters);
    STARTARGS;
    SETARG(XmNsensitive, TRUE);
    XtSetValues(chapter_button, ARGLIST);

    return product->nchapters;
}


/**************************************************************************
 *
 * Function: set_print_search_state
 *
 * Description: Makes print and search menu items sensitive or insensitive
 *
 * Parameters: 
 *	sensitive (I) - TRUE = make sensitive, FALSE = make insensitive
 *
 * Return: none
 *
 **************************************************************************/

void set_print_search_state(int sensitive)
{
    int i;
    DEFARGS(5);

    STARTARGS;
    SETARG(XmNsensitive, sensitive);
    for (i = 0; i < XtNumber(file_menu_items); i++) {
	if (file_menu_items[i].func == print_handler ||
		file_menu_items[i].func == search_handler) {
	    XtSetValues(file_menu_items[i].button, ARGLIST);
	}
    }
}


/*
 ==========================================================================
			 LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: create_menu
 *
 * Description: Creates the menu bar button and menu pane on the specified
 *	menubar.
 *
 * Parameters: 
 *	menubar (I) - parent menubar widget
 *	menu (I) - pointer to menu data structure
 *	panep (O) - pointer to created menu pane
 *	buttonp (O) - pointer to created menu cascade button
 *
 * Return: Menu bar cascade button widget.
 *
 **************************************************************************/

static void create_menu(Widget menubar, menu_t *menu,
			Widget *panep, Widget *buttonp)
{
    Widget menu_pane, menu_button;
    DEFARGS(5);

    /*
     * Create the menu bar items and cascade buttons
     */
    menu_pane = XmCreatePulldownMenu(menubar, menu->name, NULL, 0);
    STARTARGS;
    SETARG(XmNsubMenuId, menu_pane);
    menu_button = XtCreateManagedWidget(menu->name, xmCascadeButtonGadgetClass,
				    	menubar, ARGLIST);
    if (menu->help) {
	STARTARGS;
	SETARG(XmNmenuHelpWidget, menu_button);
	XtSetValues(menubar, ARGLIST);
    }
    if (menu->insensitive) {
	STARTARGS;
	SETARG(XmNsensitive, FALSE);
	XtSetValues(menu_button, ARGLIST);
    }

    /*
     * Add menu items to panes
     */
    add_menu_items(menu_pane, menu->items, menu->nitems);

    *panep = menu_pane;
    *buttonp = menu_button;
}


/**************************************************************************
 *
 * Function: add_menu_items
 *
 * Description: Adds menu items to a menu pane. Note that for each item
 *	added both the specified callback routine and the update display
 *	callback are added to the item's callback list. The update display
 *	function will always be called first so that pending expose
 *	events are processed before the specified callback function.
 *
 * Parameters: 
 *	pane (I) - parent menu pane
 *	items (I) - list of menu items
 *	nitems (I) - number of menu items
 *
 * Return: none
 *
 **************************************************************************/

static void add_menu_items(Widget pane, menu_item_t *items, int nitems)
{
    Widget cur_pane, moreb;
    WidgetList buttons;
    menu_item_t *iptr;
    int i, bcount = 0, item_count;
    DEFARGS(5);

    if (nitems) {
	cur_pane = pane;
	STARTARGS;
	SETARG(XmNsensitive, FALSE);
        buttons = (WidgetList) XtMalloc(nitems * sizeof(Widget));
        for (i = 0, item_count = 1, iptr = items; i < nitems;
						i++, item_count++, iptr++) {
	    if (item_count % MAX_ITEMS_PER_PANE == 0) {
        	XtManageChildren(buttons, bcount);
		bcount = 0;
    		create_menu(cur_pane, more_menu_data, &cur_pane, &moreb);
		item_count++;
	    }
	    iptr->button = buttons[bcount] = XtCreateWidget(iptr->name,
						xmPushButtonGadgetClass,
						cur_pane, NULL, 0);
	    XtAddCallback(buttons[bcount], XmNactivateCallback,
					update_disp, NULL);
	    XtAddCallback(buttons[bcount], XmNactivateCallback,
					iptr->func, iptr->data);
    	    if (iptr->insensitive)
		XtSetValues(buttons[bcount], ARGLIST);
	    bcount++;
        }
        XtManageChildren(buttons, bcount);
	XtFree((char*)buttons);
    }
}


/**************************************************************************
 *
 * Function: clear_menu_items
 *
 * Description: Removes and destroys all menu items on the specified
 *	menu pane.
 *
 * Parameters: 
 *	pane (I) - pane to have menu items cleared
 *
 * Return: none
 *
 **************************************************************************/

static void clear_menu_items(Widget pane)
{
    Widget sub_pane;
    WidgetList items;
    WidgetClass class;
    int i, num;
    DEFARGS(5);

    STARTARGS;
    SETARG(XmNnumChildren, &num);
    XtGetValues(pane, ARGLIST);
    if (!num)
	return;
    STARTARGS;
    SETARG(XmNchildren, &items);
    XtGetValues(pane, ARGLIST);
    for (i = 0; i < num; i++) {
	if (XtIsSubclass(items[i], xmCascadeButtonGadgetClass) == True) {
	    STARTARGS;
	    SETARG(XmNsubMenuId, &sub_pane);
	    XtGetValues(items[i], ARGLIST);
	    clear_menu_items(sub_pane);
	    XtDestroyWidget(sub_pane);
	    STARTARGS;
	    SETARG(XmNsubMenuId, NULL);
	    XtSetValues(items[i], ARGLIST);
	}
	XtDestroyWidget(items[i]);
    }
}


/**************************************************************************
 *
 * Function: update_disp
 *
 * Description: Processes pending exposure events before the menu pane
 *	item callbacks are invoked.
 *
 * Parameters: 
 *	w (I) - widget that invoked this callback
 *	client_data (I) - not used
 *	call_data (I) - not used
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void update_disp(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmUpdateDisplay(w);
}
