/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	nvlicense.c++
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

#include <tuSimpleApp.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "help.h"
#include <helpapi/HelpBroker.h>

#include "nvlicenselayout.h"
#include "dialog.h"

static char lockDir[] = "/var/netls";
static char lockFile[] = "/var/netls/nodelock";
static char vendorID[] = "546fb4684914.02.c0.1a.3d.52.00.00.00";

static tuTopLevel *toplevel;
static DialogBox *dialog;
static tuGadget *password;
static tuGadget *datastations;

extern "C" {
unsigned int sysid(char *);
};

void
accept(tuGadget *, void *)
{
    int ds = atoi(datastations->getText());
    if (ds <= 0) {
	dialog->warning("The value for Data Stations must be greater than 0");
	if (dialog->isMapped())
	    dialog->unmap();
	dialog->map();
	return;
    }

    FILE *fp = fopen(lockFile, "a");
    if (fp == 0) {
	if (errno == ENOENT) {
	    if (mkdir(lockDir, 0755) != 0) {
		dialog->warning(errno, "Could not create directory %s",
					lockDir);
		if (dialog->isMapped())
		    dialog->unmap();
		dialog->map();
		return;
	    }
	    fp = fopen(lockFile, "a");
	}
	if (fp == 0) {
	    dialog->warning(errno, "Could not open %s", lockFile);
	    if (dialog->isMapped())
		dialog->unmap();
	    dialog->map();
	    return;
	}
    }

    if (fprintf(fp, "\n# License for NetVisualyzer 2.0 (%d Data Station%s)\
		    \n%s %s datastations=%d\n",
		    ds, (ds > 1) ? "s" : "",
		    vendorID, password->getText(), ds) < 0) {
	dialog->warning(errno, "Could not write %s", lockFile);
	if (dialog->isMapped())
	    dialog->unmap();
	dialog->map();
	fclose(fp);
	return;
    }

    fclose(fp);
    dialog->information("License data created");
    if (dialog->isMapped())
	dialog->unmap();
    dialog->map();
}

void
cancel(tuGadget *, void *)
{
    HELPclose();
    exit(0);
}

void
help(tuGadget *, void *)
{
    HELPdisplay("/usr/lib/HelpCards/NVLicense.general.help");
}

main(int argc, char *argv[])
{
    TUREGISTERGADGETS;

    (new tuFunctionCallBack(accept, NULL))->registerName("_accept");
    (new tuFunctionCallBack(cancel, NULL))->registerName("_cancel");
    (new tuFunctionCallBack(help, NULL))->registerName("_help");

    toplevel = tuCreateSimpleApp("nvlicense", &argc, argv,
				 layoutstr, NULL, NULL, 0, NULL, False);
    toplevel->catchDeleteWindow(new tuFunctionCallBack(cancel, NULL));

    // Initialize SGIHelp
    SGIHelpInit (toplevel->getDpy(), "nvlicense", ".");

    tuGadget *g = toplevel->findGadget("_vendorid");
    g->setText(vendorID);

    g = toplevel->findGadget("_systemid");
    char buf[32];
    sprintf(buf, "%x", sysid(0));
    g->setText(buf);

    password = toplevel->findGadget("_password");
    password->setText("");

    datastations = toplevel->findGadget("_datastations");
    datastations->setText("1");

    toplevel->bind();
    toplevel->getExternalChild(0)->map(True);
    toplevel->resizeToFit();
    toplevel->setInitialOrigin(200, 200);
    toplevel->map(True);

    dialog = new DialogBox("nvlicense", toplevel, True);

    tuXExec *exec = new tuXExec(toplevel->getDpy());
    exec->loop();
    return 0;
}
