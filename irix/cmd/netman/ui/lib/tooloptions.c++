/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Tool Options
 *
 *	$Revision: 1.7 $
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
#include <bstring.h>
#include <tu/tuGadget.h>
#include <tu/tuCallBack.h>
#include <tu/tuTextField.h>
#include "tooloptions.h"
#include "prompt.h"

tuDeclareCallBackClass(ToolOptionsCallBack, ToolOptions);
tuImplementCallBackClass(ToolOptionsCallBack, ToolOptions);

ToolOptions::ToolOptions(tuTopLevel *t)
{
    interface = 0;
    filter = 0;
    prompt = 0;

    // Set up callbacks
    (new ToolOptionsCallBack(this, ToolOptions::analyzer))->
				registerName("Tool_analyzer");
    (new ToolOptionsCallBack(this, ToolOptions::browser))->
				registerName("Tool_browser");
    (new ToolOptionsCallBack(this, ToolOptions::netgraph))->
				registerName("Tool_netgraph");
    (new ToolOptionsCallBack(this, ToolOptions::netlook))->
				registerName("Tool_netlook");
    (new ToolOptionsCallBack(this, ToolOptions::nettop))->
				registerName("Tool_nettop");

    toplevel = t;
}

void
ToolOptions::setInterface(const char *c)
{
    if (interface != 0)
	delete interface;
    if (c == 0) {
	interface = 0;
	return;
    }
    unsigned int len = strlen(c) + 1;
    interface = new char[len];
    bcopy(c, interface, len);
}

void
ToolOptions::setFilter(const char *c)
{
    if (filter != 0)
	delete filter;
    if (c == 0) {
	filter = 0;
	return;
    }
    unsigned int len = strlen(c) + 1;
    filter = new char[len];
    bcopy(c, filter, len);
}

void
ToolOptions::doPrompt(const char *c)
{
    if (prompt == 0) {
	prompt = new PromptBox("prompt", toplevel, True);
	prompt->bind();
	prompt->setText("Complete the command:");
	prompt->setName("Tools Prompt");
	prompt->setCallBack(new ToolOptionsCallBack(this, ToolOptions::run));
    } else if (prompt->isMapped())
	prompt->unmap();
    prompt->getTextField()->setText(c);
    prompt->getTextField()->movePointToHome(False);
    prompt->resizeToFit();
    prompt->mapWithCancelUnderMouse();
}

void
ToolOptions::run(tuGadget *)
{
    if (prompt->getHitCode() == tuCancel) {
        prompt->unmap();
        return;
    }
    const char *c = prompt->getTextField()->getText();
    system(c);
    prompt->unmap();
    prompt->getTextField()->setText("");
}

void
ToolOptions::analyzer(tuGadget *)
{
    char buf[256];
    char *b = buf;
    strcpy(b, "analyzer");
    b += strlen(b);
    if (interface != 0)
	b += sprintf(b, " -i %s", interface);
    if (filter != 0) {
	*b++ = ' ';
	*b++ = '"';
	strcpy(b, filter);
	b += strlen(b);
	*b++ = '"';
    }
    *b++ = ' ';
    *b++ = '&';
    *b = '\0';
    doPrompt(buf);
}

void
ToolOptions::browser(tuGadget *)
{
    doPrompt("browser &");
}

void
ToolOptions::netgraph(tuGadget *)
{
    char buf[256];
    char *b = buf;
    strcpy(b, "netgraph");
    b += strlen(b);
    if (interface != 0)
	b += sprintf(b, " -i %s", interface);
    if (filter != 0) {
	*b++ = ' ';
	*b++ = '"';
	strcpy(b, filter);
	b += strlen(b);
	*b++ = '"';
    }
    *b++ = ' ';
    *b++ = '&';
    *b = '\0';
    doPrompt(buf);
}

void
ToolOptions::netlook(tuGadget *)
{
    char buf[256];
    char *b = buf;
    strcpy(b, "netlook");
    b += strlen(b);
    if (interface != 0)
	b += sprintf(b, " -i %s", interface);
    if (filter != 0) {
	*b++ = ' ';
	*b++ = '"';
	strcpy(b, filter);
	b += strlen(b);
	*b++ = '"';
    }
    *b++ = ' ';
    *b++ = '&';
    *b = '\0';
    doPrompt(buf);
}

void
ToolOptions::nettop(tuGadget *)
{
    char buf[256];
    char *b = buf;
    strcpy(b, "nettop");
    b += strlen(b);
    if (interface != 0)
	b += sprintf(b, " -i %s", interface);
    if (filter != 0) {
	*b++ = ' ';
	*b++ = '"';
	strcpy(b, filter);
	b += strlen(b);
	*b++ = '"';
    }
    *b++ = ' ';
    *b++ = '&';
    *b = '\0';
    doPrompt(buf);
}
