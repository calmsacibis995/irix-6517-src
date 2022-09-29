/* -*- C++ -*- */

#ifndef _INV_APP_H
#define _INV_APP_H

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

#ident "$Id: App.h,v 1.1 1997/09/04 02:43:37 markgw Exp $"

#include <Vk/VkApp.h>
#include "Inv.h"

class INV_App: public VkApp
{
private:

    INV_TermCB	_termCB;
    
public:

    INV_App(char *appClassName,
	    int *arg_c,
	    char **arg_v,
	    XrmOptionDescRec *optionList = NULL,
	    int sizeofOptionList = 0,
	    INV_TermCB termCB = NULL);

    virtual void terminate(int status);
};

#endif /* _INV_APP_H_ */
