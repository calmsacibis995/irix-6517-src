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
 * File: place.c
 *
 * Description: Provides widget creation, layout and access methods for
 *	the current product and chapter place indication labels.
 *
 **************************************************************************/


#ident "$Revision: 1.1 $"


#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#include "grelnotes.h"


static Widget product_name, chapter_name;


/**************************************************************************
 *
 * Function: create_place
 *
 * Description: Creates the place indication widgets and arranges them.
 *
 * Parameters: 
 *	parent (I) - parent widget
 *
 * Return: The created place indication widget grouping form is returned.
 *
 **************************************************************************/

Widget create_place(Widget parent)
{
    Widget form;
    Widget product_label, chapter_label;
    DEFARGS(15);

    /*
     * Create a form to hold everything
     */
    form = XtCreateManagedWidget("placeForm", xmFormWidgetClass,
				 parent, NULL, 0);

    /*
     * Create the place component labels
     */
    STARTARGS;
    SETARG(XmNalignment, XmALIGNMENT_BEGINNING);
    SETARG(XmNtopAttachment, XmATTACH_FORM);
    SETARG(XmNleftAttachment, XmATTACH_FORM);
    SETARG(XmNleftOffset, 5);
    SETARG(XmNbottomAttachment, XmATTACH_FORM);
    product_label = XtCreateManagedWidget("productLabel", xmLabelWidgetClass,
			 	    form, ARGLIST);

    STARTARGS;
    SETARG(XmNalignment, XmALIGNMENT_BEGINNING);
    SETARG(XmNtopAttachment, XmATTACH_FORM);
    SETARG(XmNleftAttachment, XmATTACH_WIDGET);
    SETARG(XmNleftWidget, product_label);
    SETARG(XmNbottomAttachment, XmATTACH_FORM);
    product_name = XtCreateManagedWidget("product", xmLabelWidgetClass,
			 	    form, ARGLIST);

    STARTARGS;
    SETARG(XmNalignment, XmALIGNMENT_BEGINNING);
    SETARG(XmNtopAttachment, XmATTACH_FORM);
    SETARG(XmNleftAttachment, XmATTACH_WIDGET);
    SETARG(XmNleftWidget, product_name);
    SETARG(XmNbottomAttachment, XmATTACH_FORM);
    chapter_label = XtCreateManagedWidget("chapterLabel", xmLabelWidgetClass,
			 	    form, ARGLIST);

    STARTARGS;
    SETARG(XmNalignment, XmALIGNMENT_BEGINNING);
    SETARG(XmNtopAttachment, XmATTACH_FORM);
    SETARG(XmNleftAttachment, XmATTACH_WIDGET);
    SETARG(XmNleftWidget, chapter_label);
    SETARG(XmNbottomAttachment, XmATTACH_FORM);
    SETARG(XmNrightAttachment, XmATTACH_FORM);
    chapter_name = XtCreateManagedWidget("chapter", xmLabelWidgetClass,
			 	    form, ARGLIST);

    return form;
}


/**************************************************************************
 *
 * Function: set_product_name, set_chapter_name
 *
 * Description: Sets the string for the product and chapter names
 *	respectively.
 *
 * Parameters: 
 *	name (I) - string to display
 *
 * Return: none
 *
 **************************************************************************/

void set_product_name(char *name)
{
    char buffer[256];
    XmString text;
    DEFARGS(5);

    (void)sprintf(buffer, "\"%s\"", name);
    text = XmStringCreateSimple(buffer);
    STARTARGS;
    SETARG(XmNlabelString, text);
    XtSetValues(product_name, ARGLIST);
    XmStringFree(text);

    /*
     * Required to force geometry recomputation (bug in Motif?)
     */
    XtUnmanageChild(product_name);
    XtManageChild(product_name);
}

void set_chapter_name(char *name)
{
    DEFARGS(5);

    XmString text = XmStringCreateSimple(name);
    STARTARGS;
    SETARG(XmNlabelString, text);
    XtSetValues(chapter_name, ARGLIST);
    XmStringFree(text);

    /*
     * Required to force geometry recomputation (bug in Motif?)
     */
    XtUnmanageChild(chapter_name);
    XtManageChild(chapter_name);
}
