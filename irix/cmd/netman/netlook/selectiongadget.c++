/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Selection Gadget
 *
 *	$Revision: 1.1 $
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

#include <stdio.h>
#include <string.h>
#include <tuGadget.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include "selectiongadget.h"
#include "selectiongadgetlayout.h"

SelectionGadget *selectiongadget;

SelectionGadget::SelectionGadget(NetLook *nl, tuGadget *parent,
				 const char *instanceName)
{
    // Save name
    name = instanceName;

    // Unpickle UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(parent, layoutstr);

    textfield = (tuTextField *) ui->findGadget("Selection_textfield");
    if (textfield == 0) {
	fprintf(stderr, "No Selection_textfield\n");
	exit(-1);
    }

    // Save pointer to netlook
    netlook = nl;

    selectiongadget = this;
}

void
SelectionGadget::setSelection(const char *c)
{
    if (c == 0)
	textfield->setText("");
    else {
	textfield->setText(c);
	textfield->selectAll();
    }
}
