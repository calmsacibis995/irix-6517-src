/*
 * main.c++
 *
 *	main program for regview
 *
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
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

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/capability.h>
#include <unistd.h>

#include <Vk/VkApp.h>
#include "MainWindow.h"

/*
 *  static void
 *  InitCapabilities(void)
 *
 *  Description:
 *      Set up our set of permitted capabilities, and then surrender
 *      our setuid root status.  The capabilities we set in our
 *      permitted set correspond to those used by the procfs code.
 *      They do not get moved into our effective set until we need
 *      them (see cap_open in RegionInfo.c++).
 *      
 *      Setting CAP_FLAG_PURE_RECALC in our inheritable set causes
 *      programs that we exec to inherit no capabilities from us.
 */
static void
InitCapabilities(void)
{
    cap_t cap = cap_from_text("all= "
		      "CAP_DAC_WRITE,CAP_DAC_READ_SEARCH,CAP_FOWNER+p");
    cap_set_proc(cap);
    cap_free(cap);
    cap_set_proc_flags(CAP_FLAG_PURE_RECALC);
    setuid(getuid());
}

void main(int argc, char **argv)
{
    InitCapabilities();
    VkApp *app = new VkApp("Regview", &argc, argv);
    MainWindow *mainWindow = new MainWindow("mainWindow");
    mainWindow->show();
    app->run();
}
