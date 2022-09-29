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

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/param.h>
#include <X11/Intrinsic.h>
#include <Xm/Form.h> 
#include <Xm/LabelG.h> 
#include <Xm/SeparatoG.h> 
#include <Xm/RowColumn.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleB.h> 
#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/VkOptionMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkErrorDialog.h>
#include "RunFormUI.h"
#include "pmapi.h"
#include "impl.h"


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  RunFormUI::_defaultRunFormUIResources[] = {
	"*sourceLabel.labelString:  Source:",
	"*hostToggle.labelString:  Host",
	"*archiveToggle.labelString:  Archive",
	"*timeMenu.labelString:  ",
	"*intervalLabel.labelString:  Interval:",
	"*startLabel.labelString:  Start Time:",
	"*endLabel.labelString:  End Time:",
	"*cmdLabel.labelString:  Command:",
	"*argsLabel.labelString:  Arguments:",
	"*intervalUnitsMilliseconds.labelString:  Milliseconds",
	"*intervalUnitsSeconds.labelString:  Seconds",
	"*intervalUnitsMinutes.labelString:  Minutes",
	"*intervalUnitsHours.labelString:  Hours",
	"*intervalUnitsDays.labelString:  Days",
	"*intervalUnitsWeeks.labelString:  Weeks",
	(char*)NULL
};

extern VkApp	*app;
extern char	*pmProgname;
extern char	*trailer;
extern int	pmDebug;
extern int	errno;

Widget		bottom = NULL;


RunFormUI::RunFormUI(
	const char *name,
	Widget parent,
	char *host,
	char *archive,
	double delta,
	const char *start,
	const char *end,
	int numspecs,
	const char **specs)
	: VkComponent(name),
	_flags(),
	_runProgram(NULL),
	_speccount(numspecs),
	_specs((char **)specs)
{
    // For the IMD desktop we need to ensure that a default value
    // is supplied since the desktop no longer does this for us.
    char	defaulthost[MAXHOSTNAMELEN];
    char	defaultpath[MAXPATHLEN];

    if (host) {
	_hostflag = True;
	hostset = 1;
	if (host[0] == '\0') {
	    (void)gethostname(defaulthost, MAXHOSTNAMELEN);
	    host = strdup(defaulthost);	// keep forever (don't free)
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "RunFormUI::RunFormUI host=%s\n", host);
#endif
    }
    else {
	_hostflag = False;
	hostset = 0;
    }

    if (archive) {
	_archiveflag = True;
	if (archive[0] == '\0') {
	    (void)getcwd(defaultpath, MAXPATHLEN);
	    archive = strdup(defaultpath); // keep forever (don't free)
	}
    }
    else _archiveflag = False;

    if (delta >= 0.0) _deltaflag = True;
    else _deltaflag = False;

    if (start) _startflag = True;
    else _startflag = False;

    if (end) _endflag = True;
    else _endflag = False;

    create(parent);

    if (_startflag && start[0] != '\0')
	XmTextFieldSetString(_startText, (char *)start);
    if (_endflag && end[0] != '\0')
	XmTextFieldSetString(_endText, (char *)end);
    if (_archiveflag && archive[0] != '\0') {
	_path->updateContent((char *)archive);
    }
    if (_hostflag && host[0] != '\0') {
	_host->updateContent((char *)host);
    }
    if (_deltaflag) {
	_delta = delta;
	showInterval();
    }
    if (trailer != NULL && trailer[0] != '\0')
	XmTextFieldSetString(_argsText, trailer);
}

RunFormUI::~RunFormUI(void)
{
}


void
RunFormUI::create(Widget parent)
{
    Arg		args[32];
    Cardinal	count;

    // Load any class-defaulted resources for this object
    setDefaultResources(parent, _defaultRunFormUIResources);

    // Create an unmanaged widget as the top of the widget hierarchy
    count = 0;
    XtSetArg(args[count], XmNresizePolicy, XmRESIZE_GROW); count++;
    _baseWidget = _runForm = XtCreateWidget(_name,
		xmFormWidgetClass, parent, args, count);

    // install a callback to guard against unexpected widget destruction
    installDestroyHandler();

    if (_hostflag || _archiveflag) {
	count = 0;
	XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
	XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
	XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
	XtSetArg(args[count], XmNtopOffset, 2); count++;
	XtSetArg(args[count], XmNleftOffset, 2); count++;
	XtSetArg(args[count], XmNwidth, 95); count++;
	XtSetArg(args[count], XmNheight, 20); count++;
	Widget sourceLabel = XmCreateLabelGadget(_baseWidget,
					"sourceLabel", args, count);
	XtManageChild(sourceLabel);

	if (_archiveflag) {
	    count = 0;
	    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
	    XtSetArg(args[count], XmNtopWidget, sourceLabel); count++;
	    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNtopOffset, 6); count++;
	    XtSetArg(args[count], XmNleftOffset, 7); count++;
	    XtSetArg(args[count], XmNrightOffset, 7); count++;
	    _path = new ComboPathFinder("iconFinder", _baseWidget);
	    _path->addCallback(ComboPathFinder::nameChangedCallback, this,
				(VkCallbackMethod) &RunFormUI::archiveNameChanged);
	    XtSetValues(_path->baseWidget(), args, count);
	}
	if (_hostflag) {
	    count = 0;
	    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
	    XtSetArg(args[count], XmNtopWidget, sourceLabel); count++;
	    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNtopOffset, 6); count++;
	    XtSetArg(args[count], XmNleftOffset, 7); count++;
	    XtSetArg(args[count], XmNrightOffset, 7); count++;
	    _host = new ComboHostFinder("iconFinder", _baseWidget);
	    XtSetValues(_host->baseWidget(), args, count);
	    bottom = _host->baseWidget();
	    _host->show();
	}
	else if (_archiveflag) {
	    bottom = _path->baseWidget();
	    _path->show();
	}

	if (_archiveflag && _hostflag) {
	    count = 0;
	    XtSetArg(args[count], XmNorientation, XmHORIZONTAL); count++;
	    XtSetArg(args[count], XmNentryAlignment, XmALIGNMENT_CENTER); count++;
	    XtSetArg(args[count], XmNpacking, XmPACK_COLUMN); count++;
	    XtSetArg(args[count], XmNradioBehavior, True); count++;
	    XtSetArg(args[count], XmNradioAlwaysOne, True); count++;
	    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNtopOffset, 0); count++;
	    XtSetArg(args[count], XmNleftOffset, 130); count++;
	    XtSetArg(args[count], XmNrightOffset, 69); count++;
	    XtSetArg(args[count], XmNwidth, 196); count++;
	    XtSetArg(args[count], XmNheight, 34); count++;
	    XtSetArg(args[count], XmNtraversalOn, False); count++;
	    Widget sourceRadioBox = XtCreateWidget("sourceRadioBox",
			xmRowColumnWidgetClass,	_baseWidget, args, count);
	    XtManageChild(sourceRadioBox);

	    count = 0;
	    XtSetArg(args[count], XmNset, True); count++;
	    _hostToggle = XtCreateWidget("hostToggle",
			xmToggleButtonWidgetClass, sourceRadioBox, args, count);
	    XtManageChild(_hostToggle);

	    count = 0;
	    XtSetArg(args[count], XmNset, False); count++;
	    _archiveToggle = XtCreateWidget("archiveToggle",
		xmToggleButtonWidgetClass, sourceRadioBox, args, count);
	    XtManageChild(_archiveToggle);
	    XtAddCallback(_archiveToggle, XmNvalueChangedCallback,
		&RunFormUI::toggleValueChangedCallback, (XtPointer) this);
	    XtAddCallback(_hostToggle, XmNvalueChangedCallback,
		&RunFormUI::toggleValueChangedCallback, (XtPointer) this);
	}
    }

    if (_deltaflag) {
	count = 0;
	XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
	if (bottom) {	// attach to previous
	    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
	    XtSetArg(args[count], XmNtopWidget, bottom); count++;
	    XtSetArg(args[count], XmNtopOffset, 13); count++;
	}
	else {	// attach to top of form
	    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
	    XtSetArg(args[count], XmNtopOffset, 5); count++;
	}
	XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
	XtSetArg(args[count], XmNleftOffset, 2); count++;
	XtSetArg(args[count], XmNwidth, 95); count++;
	XtSetArg(args[count], XmNheight, 20); count++;
	Widget intervalLabel = XmCreateLabelGadget(_baseWidget,
					"intervalLabel", args, count);
	XtManageChild(intervalLabel);

	count = 0;
	XtSetArg(args[count], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); count++;
	XtSetArg(args[count], XmNtopWidget, intervalLabel); count++;
	XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
	XtSetArg(args[count], XmNtopOffset, -5); count++;
	XtSetArg(args[count], XmNleftOffset, 100); count++;
	XtSetArg(args[count], XmNwidth, 120); count++;
	XtSetArg(args[count], XmNheight, 33); count++;
	_intervalText = XtCreateWidget("intervalText",
		xmTextFieldWidgetClass, _baseWidget, args, count);
	XtManageChild(_intervalText);
	XtAddCallback(_intervalText, XmNlosingFocusCallback,
		&RunFormUI::intervalTextLosingFocusCallback, (XtPointer) this);

	_timeMenu = new VkOptionMenu(_baseWidget);
	count = 0;
	XtSetArg(args[count], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); count++;
	XtSetArg(args[count], XmNleftAttachment, XmATTACH_WIDGET); count++;
	XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
	XtSetArg(args[count], XmNtopWidget, _intervalText); count++;
	XtSetArg(args[count], XmNleftWidget, _intervalText); count++;
	XtSetArg(args[count], XmNtopOffset, 1);	count++;
	XtSetArg(args[count], XmNleftOffset, 5); count++;
	XtSetArg(args[count], XmNrightOffset, 10); count++;
	XtSetArg(args[count], XmNwidth, 162); count++;
	XtSetArg(args[count], XmNheight, 32); count++;
	XtSetValues(_timeMenu->baseWidget(), args, count);
	_msec = _timeMenu->addAction("intervalUnitsMilliseconds",
		&RunFormUI::intervalUnitsActivateCallback, (XtPointer) this);
	_sec  = _timeMenu->addAction("intervalUnitsSeconds",
		&RunFormUI::intervalUnitsActivateCallback, (XtPointer) this);
	_min  = _timeMenu->addAction("intervalUnitsMinutes",
		&RunFormUI::intervalUnitsActivateCallback, (XtPointer) this);
	_hour = _timeMenu->addAction("intervalUnitsHours",
		&RunFormUI::intervalUnitsActivateCallback, (XtPointer) this);
	_day  = _timeMenu->addAction("intervalUnitsDays",
		&RunFormUI::intervalUnitsActivateCallback, (XtPointer) this);
	_week = _timeMenu->addAction("intervalUnitsWeeks",
		&RunFormUI::intervalUnitsActivateCallback, (XtPointer) this);
	_timeMenu->set(_sec);	/* input delta is in seconds */

	bottom = _intervalText;
    }

    if (_startflag)
	_startText = _flags.createTextField(_baseWidget, "startLabel",
		"startText", NULL, NULL, NULL, !(_archiveflag && _hostflag));
    if (_endflag)
	_endText = _flags.createTextField(_baseWidget, "endLabel",
		"endText", NULL, NULL, NULL, !(_archiveflag && _hostflag));

    createSpecs();

    _cmdText = _flags.createTextField(_baseWidget, "cmdLabel",
				"cmdText", NULL, NULL, _runProgram, False);
    _argsText = _flags.createTextField(_baseWidget, "argsLabel",
				"argsText", NULL, NULL, NULL, True);

    // and right at the end ...
    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _argsText); count++;
    XtSetArg(args[count], XmNtopOffset, 15); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNorientation, XmHORIZONTAL); count++;
    _separator = XmCreateSeparatorGadget(_baseWidget, "separator", args, count);
    XtManageChild(_separator);
}

char *
RunFormUI::getSpecToken(const char *spec, int *offset)
{
    buf[0] = '\0';
    for (int i = 0; spec[*offset+i] != '|' && spec[*offset+i] != '\0'; i++)
        buf[i] = spec[*offset+i];
    buf[i] = '\0';
    if (spec[*offset+i] == '|') i++;
    *offset += i;

    return buf;
}

void
RunFormUI::createSpecs()
{
    char	*flagstr = NULL;
    char	*labelstr = NULL;
    char	*fieldstr = NULL;
    int		position = 0;

    for (int i = 0; i < _speccount; i++) {

	if (i == 0) {
	    if (access(_specs[i], EX_OK) == 0) {
		char	actualpath[MAXPATHLEN];
		if (realpath(_specs[i], &actualpath[0]) == NULL) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "realpath failed on '%s'\n", _specs[i]);
#endif
		    strcpy(&actualpath[0], _specs[i]);
		}
		_runProgram = strdup(actualpath);
		continue;
	    }
	    else {
		sprintf(buf, "%s: cannot execute '%s'.\n\n%s.\n", pmProgname,
						_specs[i], strerror(errno));
		theErrorDialog->postAndWait(buf);
		app->terminate(1);
	    }
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "%s: Found specification #%d '%s'\n",
						pmProgname, i, _specs[i]);
#endif

	switch (_flags.queryType(_specs[i], &position)) {

	case PMRUN_TEXTFIELD:
	    flagstr = strdup(getSpecToken(_specs[i], &position));
	    labelstr = strdup(getSpecToken(_specs[i], &position));
	    fieldstr = strdup(getSpecToken(_specs[i], &position));
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "Flag:  '-%s'\n", flagstr);
		fprintf(stderr, "Label: '%s'\n", labelstr);
		fprintf(stderr, "Value: '%s'\n", fieldstr);
	    }
#endif
	    _flags.createTextField(_baseWidget, "fieldLabel", "fieldText",
					flagstr, labelstr, fieldstr, True);
	    free(flagstr);	flagstr  = NULL;
	    free(labelstr);	labelstr = NULL;
	    free(fieldstr);	fieldstr = NULL;
	    break;

	case PMRUN_FINDER:
	case PMRUN_RADIO:
	case PMRUN_CHECK:
	default:
	    pmprintf("%s: Unsupported flag type in '%s'.\n",
						pmProgname, _specs[i]);
	    pmflush();
	    break;
	}
    }
}

const char *
RunFormUI::className(void)
{
    return ("RunFormUI");
}


void
RunFormUI::archiveNameChanged(VkComponent *, void *, void *callData)
{
    char		buf[64];
    char		tzbuf[64];
    pmLogLabel		label;
    struct timeval	endtime = {0, 0};
    char		*name = (char *)callData;

    if ((pmNewContext(PM_CONTEXT_ARCHIVE, name) < 0) ||
	(pmGetArchiveLabel(&label) < 0) ||
	(pmGetArchiveEnd(&endtime) < 0)) {
	/* clear the fields in case they have text already */
	buf[0] = '\0';
	if (_startflag)
	    XmTextFieldSetString(_startText, buf);
	if (_endflag)
	    XmTextFieldSetString(_endText, buf);
	return;
    }

    if (_startflag && !hostset) {
	sprintf(buf, "@ %.24s", pmCtime(&label.ll_start.tv_sec, tzbuf));
	XmTextFieldSetString(_startText, buf);
    }
    if (_endflag && !hostset) {
	sprintf(buf, "@ %.24s", pmCtime(&endtime.tv_sec, tzbuf));
	XmTextFieldSetString(_endText, buf);
    }
    pmDestroyContext(pmWhichContext());
}

void
RunFormUI::intervalUnitsActivateCallback(Widget w, XtPointer clientData,
							XtPointer callData)
{
    RunFormUI* obj = (RunFormUI *) clientData;
    obj->intervalUnitsActivate(w, callData);
}

void
RunFormUI::intervalUnitsActivate(Widget, XtPointer)
{
    // This virtual function is called from intervalUnitsActivateCallback.
    // This function is normally overriden by a derived class.
}

void
RunFormUI::intervalTextLosingFocusCallback(Widget w, XtPointer clientData,
							XtPointer callData)
{
    RunFormUI* obj = (RunFormUI *) clientData;
    obj->intervalTextLosingFocus(w, callData);
}

void
RunFormUI::intervalTextLosingFocus(Widget, XtPointer)
{
    // This virtual function is called from intervalTextLosingFocus.
    // This function is normally overriden by a derived class.
}

void
RunFormUI::toggleValueChangedCallback(Widget w, XtPointer clientData,
							XtPointer callData)
{
    RunFormUI* obj = (RunFormUI *) clientData;
    obj->toggleValueChanged(w, callData);
}

void
RunFormUI::toggleValueChanged(Widget, XtPointer)
{
    // This virtual function is called from toggleValueChangedCallback.
    // This function is normally overriden by a derived class.
}

void
RunFormUI::setInterval()
{
    if (_deltaflag) {
	double		f = 0;
	char		*sptr = NULL;
	char		*tmp;
	VkMenuItem	*i = _timeMenu->getItem();

	sptr = XmTextFieldGetString(_intervalText);
	tmp = stripwhite(sptr);
	if (sscanf(tmp, "%lf", &f) <= 0)
	    f = 0;
	XtFree(sptr);

	if (i == _msec)		f /= 1000.0;
	else if (i == _min)	f *= 60.0;
	else if (i == _hour)	f *= 3600.0;	// 60 * 60
	else if (i == _day)	f *= 86400.0;	// 60 * 60 * 24
	else if (i == _week)	f *= 604800.0;	// 60 * 60 * 24 * 7

	if ((int)f <= 0)
	    _delta = 0.0;
	else
	    _delta = f;
    }
}

void
RunFormUI::showInterval()
{
    if (_deltaflag) {
	VkMenuItem	*i = _timeMenu->getItem();

	if (i == _msec)		sprintf(buf, "%.0f", _delta * 1000);
	else if (i == _sec)	sprintf(buf, "%.2f", _delta);
	else if (i == _min)	sprintf(buf, "%.4f", _delta / 60.0);
	else if (i == _hour)	sprintf(buf, "%.6f", _delta / 3600.0);
	else if (i == _day)	sprintf(buf, "%.8f", _delta / 86400.0);
	else if (i == _week)	sprintf(buf, "%.10f", _delta / 604800.0);

	if (_delta > 0)
	    XmTextFieldSetString(_intervalText, buf);
    }
}

char *
RunFormUI::stripwhite(char *sptr)
{
    char	*start = NULL;
    char	*end   = sptr;

    for (int i = 0; sptr[i] != '\0'; i++) {
	if (!isspace(sptr[i])) {
	    if (!start)
		start = &sptr[i];
	    end = &sptr[i];
	}
    }
    if (end < sptr+i) {
	*(++end) = '\0';
	if (!start)
	    start = end;
    }
    else if (!start)
	start = sptr;

    return start;
}


//
//	--- IMD and Oz desktop-specific code ---
//

#ifdef PMRUN_NO_IMD
void
ComboPathFinder::drop(IconData *dropped)
{
    char	actual[MAXPATHLEN];

    PathFinder::drop(dropped);
    if (realpath(getName(), &actual[0]) != NULL)
	setName(actual);
}

#else

void
ComboPathFinder::handleDropDNA(const ImdDNA& droppedDNA)
{
    char	actual[MAXPATHLEN];
    char	*text;

    ImdViewerFinder::handleDropDNA(droppedDNA);
    if ((text = fetchContent()) == NULL)
	return;
    if (realpath(text, &actual[0]) != NULL)
	updateContent(actual);
    free(text);
}

void
updateDropSite(Widget finder, DropState state)
{
    Arg			args[4];
    Cardinal		count = 0, numchildren = 0, i;
    WidgetList		children;
    XtCallbackProc	dropproc = NULL;

    XtSetArg(args[count], XmNchildren, &children); count++;
    XtSetArg(args[count], XmNnumChildren, &numchildren); count++;
    XtSetArg(args[count], XmNdropProc, &dropproc); count++;
    XtGetValues(finder, args, count);

    for (i=0; i < numchildren; ++i)
	updateDropSite(children[i], state);

    if (state == PMRUN_DISABLE_DROP && dropproc != NULL)
	XmDropSiteUnregister(finder);
    else if (state == PMRUN_ENABLE_DROP && dropproc != NULL) {
	count = 0;
	XtSetArg(args[count], XmNdropSiteActivity, XmDROP_SITE_ACTIVE); count++;
	XtSetArg(args[count], XmNdropProc, dropproc); count++;
	XmDropSiteRegister(finder, args, count);
    }
}

#endif
