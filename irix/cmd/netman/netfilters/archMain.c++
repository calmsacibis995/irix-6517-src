// $Revision: 1.8 $
// $Date: 1996/02/26 01:27:45 $

#include "archiver.h"
#include "dialog.h"

#include <stdio.h>
#include <strings.h>

#include <tuScreen.h>
#include <tuTopLevel.h>
#include <tuWindow.h>
#include <tuVisual.h>
#include <tuXExec.h>

#include <helpapi/HelpBroker.h>

#define X_ORIGIN	9
#define Y_ORIGIN	320

// Define the command line arguments that we understand.  For example,
// saying "-fn foo" is the same as putting "NetFilters*font: foo" in a
// .Xdefaults file. 
// xxx need better arg names for these (i.e "-foo" names); xxx
static XrmOptionDescRec args[] = {
    {"-f",	"*filterFile",		XrmoptionSepArg, NULL},
    {"-help",	"*help", 		XrmoptionNoArg, "Yes"},
    {"-stdout",	"*stdout", 		XrmoptionNoArg, "Yes"},
};

static const int nargs = sizeof(args) / sizeof(XrmOptionDescRec);

// Define some application specific resources.  Right now, they all
// need to be placed into one structure.  Note that most of these
// names match up to the command line arguments above.
struct AppResources {
    char* filterFile;
    tuBool help;
    tuBool stdOut;
};

static tuResourceItem opts[] = {
    { "filterFile", 0, tuResString, INITIAL_FILE, offsetof(AppResources, filterFile), },
    { "help", 0, tuResBool, "False", offsetof(AppResources, help), },
    { "stdout", 0, tuResBool, "False", offsetof(AppResources, stdOut), },
    0,
};

void main(int argc, char** argv)
{
    
    tuResourceDB* rdb = new tuResourceDB;
    AppResources ar;
    
    char* instanceName = 0;
    char* className = "NetFilters";
    tuScreen* screen = rdb->getAppResources(&ar, args, nargs, opts,
					   &instanceName, className,
					   &argc, argv);
    // note: declaration is:
    /***
	tuScreen* tuResourceDB::getAppResources(void* app,
	XrmOptionDescList commandLineOptions, int numberOfCommandLineOptions,
	tuResourceItem* optionsWanted,
	char** programNameAddr, char const* className,
	int* argc, char** argv)
    ***/

    Display* dpy = rdb->getDpy();

    // Initilize SGIHelp
    SGIHelpInit (dpy, className, ".");

    tuColorMap* cmap = screen->getDefaultColorMap();

    char* fileName = ar.filterFile;
    
    Archiver* archiver = new Archiver("NetFilters", cmap, rdb,
				      instanceName, className,
				      fileName, ar.stdOut);

    if (ar.help) {
	archiver->usage();
    } else {
	archiver->bind();
   	archiver->resizeToFit();
	archiver->setInitialOrigin(X_ORIGIN, Y_ORIGIN);
    	archiver->map();
    }
    
    tuXExec exec(dpy);

    exec.loop();
} // main

