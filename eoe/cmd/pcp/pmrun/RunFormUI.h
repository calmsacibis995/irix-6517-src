//
// Copyright 1997, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//

#ifndef RUNFORMUI_H
#define RUNFORMUI_H

#include <Vk/VkComponent.h>
#include "FlagList.h"

class VkOptionMenu;
class VkMenuAction;
class VkMenuToggle;
class VkMenuItem;
class ComboHostFinder;	// black magic here - gets around two problems:
class ComboPathFinder;	// IMD vs. oz libs & overlapped drop sites (see below)


class RunFormUI : public VkComponent
{ 
  public:

    RunFormUI(const char	*name,
	      Widget		parent,
	      char		*host,
	      char		*archive,
	      double		delta,
	      const char	*start,
	      const char	*end,
	      int		numspecs,
	      const char	**specs);

    virtual ~RunFormUI();

    void create(Widget);
    const char *className();

  protected:

    ComboHostFinder *_host;
    ComboPathFinder *_path;

    // Widgets created by this class
    Widget  _runForm;
    Widget  _archiveToggle;
    Widget  _hostToggle;
    Widget  _intervalText;
    Widget  _startText;
    Widget  _endText;
    Widget  _cmdText;
    Widget  _argsText;
    Widget  _separator;

    VkOptionMenu  *_timeMenu;

    VkMenuItem *_msec;
    VkMenuItem *_sec;
    VkMenuItem *_min;
    VkMenuItem *_hour;
    VkMenuItem *_day;
    VkMenuItem *_week;

    Boolean _hostflag;
    Boolean _archiveflag;
    Boolean _deltaflag;
    Boolean _startflag;
    Boolean _endflag;

    double _delta;

    char *_runProgram;
    int  _speccount;
    char **_specs;
    FlagList _flags;

#define BUF_SIZE 2048
    char buf[BUF_SIZE];
    int  hostset;

    void showInterval();
    void setInterval();

    char *stripwhite(char *);

    virtual void intervalUnitsActivate(Widget, XtPointer);
    virtual void intervalTextLosingFocus(Widget, XtPointer);
    virtual void toggleValueChanged(Widget, XtPointer);

    // updates start & end times if required ...
    void archiveNameChanged(VkComponent *, void *, void *);

  private: 

    // Array of default resources
    static String      _defaultRunFormUIResources[];

    void createSpecs();
    char *getSpecToken(const char *, int *);

    static void intervalUnitsActivateCallback(Widget, XtPointer, XtPointer);
    static void intervalTextLosingFocusCallback(Widget, XtPointer, XtPointer);
    static void toggleValueChangedCallback(Widget, XtPointer, XtPointer);
};


//
// ComboHostFinder & ComboIconFinder justification ...
//
// All code differences between IMD & oz desktop libraries are hidden
// away in these two classes (conditional compilation).
//
// Because we are using the same screen real estate for BOTH the host
// and the archive drop sites, only one is successfully registered
// as a drop site (host).  This means that whenever the toggle button
// is switched, we have to unregister one of the drop sites & then
// register the other for the one which wasn't successfully registered
// to allow drops at all on this second site.
// This has to be done mainly through Motif, since the desktop stuff
// doesn't really support what we're doing here.  They do give access
// to the drop site widgets, so we get away with this because we know
// what happens in registerDropSite() in both of the desktop libraries.
//

#include <Xm/DragDrop.h>

#ifdef PMRUN_NO_IMD	// PMRUN_NO_IMD comes in via the command line
#include <oz/HostFinder.h>
#include <oz/PathFinder.h>

class ComboHostFinder : public HostFinder
{
  public:
    ComboHostFinder(const char *name, Widget base)
	: HostFinder(name, base) { }
    void enableDrop()
	{ _dropPocket->registerDropSite(); }
    void disableDrop()
	{ XmDropSiteUnregister(_dropPocket->getIconWidget()); }
    void updateContent(char *contents)
	{ setName(contents); }
    char *fetchContent()
	{ return strdup(getName()); }
    void giveMeFocus()
	{ traverseCurrent(); }
};

class ComboPathFinder : public PathFinder
{
  public:
    ComboPathFinder(const char *name, Widget base)
	: PathFinder(name, base) { }
    void enableDrop()
	{ _dropPocket->registerDropSite(); }
    void disableDrop()
	{ XmDropSiteUnregister(_dropPocket->getIconWidget()); }
    void updateContent(char *contents)
	{ setName(contents); }
    char *fetchContent()
	{ return strdup(getName()); }
    void giveMeFocus()
	{ traverseCurrent(); }
    void drop(IconData *);
};

#else
#include <imd/HostFinder.h>
#include <imd/ModelFinder.h>

enum DropState {PMRUN_ENABLE_DROP, PMRUN_DISABLE_DROP};
void updateDropSite(Widget, enum DropState);

class ComboHostFinder : public ImdHostFinder
{
  public:
    ComboHostFinder(const char *name, Widget base)
	: ImdHostFinder(name, base) { }
    void enableDrop()
	{ updateDropSite(baseWidget(), PMRUN_ENABLE_DROP); }
    void disableDrop()
	{ updateDropSite(baseWidget(), PMRUN_DISABLE_DROP); }
    void updateContent(char *contents)
	{ setContent((ImdString)contents); }
    char *fetchContent()
	{ return strdup(getContent().writableCharPtr()); }
    void giveMeFocus() { }
};

class ComboPathFinder : public ImdViewerFinder
{
  public:
    ComboPathFinder(const char *name, Widget base)
	: ImdViewerFinder((ImdString)name, base) { }
    void enableDrop()
	{ updateDropSite(baseWidget(), PMRUN_ENABLE_DROP); }
    void disableDrop()
	{ updateDropSite(baseWidget(), PMRUN_DISABLE_DROP); }
    void updateContent(char *contents)
	{ setContent((ImdString)contents); }
    char *fetchContent()
	{ return strdup(getContent().writableCharPtr()); }
    void giveMeFocus() { }
    void handleDropDNA(const ImdDNA& droppedDNA);
};
#endif	// ifndef PMRUN_NO_IMD


#endif	// ifndef RUNFORMUI_H
