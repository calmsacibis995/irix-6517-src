/* -*- C++ -*- */

#ifndef _INV_H_
#define _INV_H_

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

#ident "$Id: Inv.h,v 1.5 1999/04/30 01:44:04 kenmcd Exp $"

#include "platform_defs.h"
#include "String.h"
#include "Bool.h"
#include "X11/Xlib.h"
#include "X11/Xresource.h"

//
// Typedefs
//

class INV_App;
class INV_View;
class INV_ModList;
class SoXtExaminerViewer;

typedef void (*INV_TermCB)(int);

//
// Globals
//

extern "C" {

extern char			*pmProgname;
// Executable name

extern int			pmDebug;
// Debugging flags;

extern int			errno;
// Error number

}	// extern "C"

extern OMC_String		theAppName;
// The application name, uses for resources

extern float			theScale;
// The scale controls multiplier

extern OMC_Bool			theStderrFlag;
// Use stderr if true

extern INV_ModList		*theModList;
// List of modulated objects

extern INV_View			*theView;
// Viewer coordinator

extern INV_App			*theApp;
// Viewkit application object

extern SoXtExaminerViewer	*theViewer;
// The examiner window

extern const uint_t		theBufferLen;
// Length of theBuffer

extern char			theBuffer[];
// String buffer that can be used for anything

extern const OMC_String		theDefaultFlags;
// Default flags parsed by INV_View::parseArgs

extern OMC_String		theLoggerLaunch;
// Path to pmlogger launch script

//
// Setup routines
//

int INV_setup(const char *appname, int *argc, char **argv, 
	      XrmOptionDescRec *cmdopts, int numOpts, INV_TermCB termCB);

//
// Error message routines
//
// DBG_TRACE_1 is used in setup, launch, selection classes
// DBG_TRACE_2 is used in modulation classes
// 
// If pmDebug is set to anything and PCP_USE_STDERR is not set, these 
// routines below will also dump a message to stderr in addition to a dialog
//

#define _POS_	__FILE__, __LINE__
 
int INV_warningMsg(const char *fileName, int line, const char *msg, ...);
int INV_errorMsg(const char *fileName, int line, const char *msg, ...);
int INV_fatalMsg(const char *fileName, int line, const char *msg, ...);

#endif /* _INV_H_ */
