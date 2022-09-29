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
 * File: buttons.c
 *
 * Description: Creates and handles the text browsing control buttons.
 *
 **************************************************************************/


#ident "$Revision: 1.1 $"


#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>

#include "grelnotes.h"


static Widget chapter_next, chapter_prev;	/* Chapter motion buttons */
static Widget product_next, product_prev;	/* Product motion buttons */


/**************************************************************************
 *
 * Function: create_buttons
 *
 * Description: Supervises the creation of the text browsing control
 *	buttons. These buttons control chapter and product selection
 *	in a sequential manner.
 *
 * Parameters: 
 *	parent (I) - parent widget
 *
 * Return: The created button RowColumn widget is returned.
 *
 **************************************************************************/

Widget create_buttons(Widget parent)
{
    Widget button_rc, left_rc, right_rc;
    DEFARGS(10);

    /*
     * Create a RowColumns to hold the buttons
     */
    STARTARGS;
    SETARG(XmNorientation, XmHORIZONTAL);
    SETARG(XmNpacking, XmPACK_COLUMN);
    SETARG(XmNnumColumns, 1);
    button_rc = XtCreateManagedWidget("buttonRc", xmRowColumnWidgetClass,
				      parent, ARGLIST);
#ifdef PRODBUTTONS
    left_rc = XtCreateManagedWidget("leftButtonRc", xmRowColumnWidgetClass,
				      button_rc, ARGLIST);
#endif
    right_rc = XtCreateManagedWidget("rightButtonRc", xmRowColumnWidgetClass,
				      button_rc, ARGLIST);

#ifdef PRODBUTTONS
    /*
     * Create product browsing buttons
     */
    product_prev = XtCreateManagedWidget("prevProduct",
					 xmPushButtonWidgetClass,
					 left_rc, NULL, 0);
    product_next = XtCreateManagedWidget("nextProduct",
					 xmPushButtonWidgetClass,
					 left_rc, NULL, 0);
    XtAddCallback(product_prev, XmNactivateCallback, next_product,
		  (XtPointer)MOVE_PREV);
    XtAddCallback(product_next, XmNactivateCallback, next_product,
		  (XtPointer)MOVE_NEXT);
#endif

    /*
     * Create chapter browsing buttons
     */
    chapter_prev = XtCreateManagedWidget("prevChapter",
					 xmPushButtonWidgetClass,
					 right_rc, NULL, 0);
    chapter_next = XtCreateManagedWidget("nextChapter",
					 xmPushButtonWidgetClass,
					 right_rc, NULL, 0);
    XtAddCallback(chapter_prev, XmNactivateCallback, next_chapter,
		  (XtPointer)MOVE_PREV);
    XtAddCallback(chapter_next, XmNactivateCallback, next_chapter,
		  (XtPointer)MOVE_NEXT);

    /*
     * Start with all buttons disabled
     */
    set_button_state(CHAPTER_BUTTONS, DISABLED);
    set_button_state(PRODUCT_BUTTONS, DISABLED);

    return button_rc;
}


/**************************************************************************
 *
 * Function: set_button_state
 *
 * Description: Sets the product or chapter movement button sensitivity
 *
 * Parameters: 
 *	buttons (I) - button set on which to set state. One of
 *		      PRODUCT_BUTTONS, CHAPTER_BUTTONS.
 *	state (I) - ENABLED or DISABLED
 *
 * Return: none
 *
 **************************************************************************/

void set_button_state(int buttons, int state)
{
    DEFARGS(5);

    STARTARGS;
    SETARG(XmNsensitive, state);

    switch (buttons) {
	case PRODUCT_BUTTONS:
#ifdef PRODBUTTONS
	    XtSetValues(product_prev, ARGLIST);
	    XtSetValues(product_next, ARGLIST);
#endif
	    break;
	case CHAPTER_BUTTONS:
	    XtSetValues(chapter_prev, ARGLIST);
	    XtSetValues(chapter_next, ARGLIST);
	    break;
    }
}
