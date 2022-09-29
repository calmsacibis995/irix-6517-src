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

#include "RunForm.h"
#include "pmapi.h"
#include "impl.h"

#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <Xm/Form.h> 
#include <Xm/RowColumn.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleB.h> 
#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/VkDialogManager.h>
#include <Vk/VkErrorDialog.h>
#include <helpapi/HelpBroker.h>


extern char	*pmProgname;
extern int	pmDebug;
extern int	errno;

extern VkApp    *app;
extern char	*header;


RunForm::RunForm(
	const char *name,
	Widget parent,
	char *host,
	char *archive,
	double delta,
	const char *start,
	const char *end,
	int numspecs,
	const char **specs )
	: RunFormUI(name, parent, host, archive, delta, start, end, numspecs, specs)
{ 
    // This constructor calls RunFormUI(parent, name)
    // which calls RunFormUI::create() to create
    // the widgets for this component. Any code added here
    // is called after the component's interface has been built

}


RunForm::~RunForm()
{
    // The base class destructors are responsible for
    // destroying all widgets and objects used in this component.
    // Only additional items created directly in this class
    // need to be freed here.

}


const char*
RunForm::className(void)
{
    return ("RunForm");
}

void
RunForm::setParent(VkDialogManager* parent)
{
    // Store a pointer to the parent VkWindow. This can
    // be useful for accessing the menubar from this class.
    _parent = parent;
}

void
RunForm::apply(Widget, XtPointer)
{
    launch();
}

void
RunForm::cancel(Widget, XtPointer)
{
    app->terminate(1);
}

void
RunForm::ok(Widget, XtPointer)
{
    if (launch() == 0)
	app->terminate(0);
}

void
RunForm::help(Widget, XtPointer)
{
    SGIHelpMsg("", "PmRunHelp", NULL);
}

void
RunForm::intervalUnitsActivate(Widget, XtPointer)
{
    showInterval();
}

void
RunForm::intervalTextLosingFocus(Widget, XtPointer)
{
    setInterval();
}


void
RunForm::toggleValueChanged(Widget w, XtPointer callData)
{
    static int		firsttime = 1;
    static Pixel	readonly = NULL;
    static Pixel	readwrite = NULL;
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *) callData;
    Arg		args[4];
    Cardinal	count;

    if (firsttime) {
	// disable drop for the finder which isn't current
	_path->disableDrop();
	firsttime = 0;
    }

    if (w == _archiveToggle && cbs->set && hostset) {
	hostset = 0;
	if (!readwrite) {
	    if (_startflag)
		readwrite = (Pixel) VkGetResource(_startText, "textFieldBackground",
					"TextFieldBackground", XmRPixel, "Black");
	    else if (_endflag)
		readwrite = (Pixel) VkGetResource(_endText, "textFieldBackground",
					"TextFieldBackground", XmRPixel, "Black");
	}
	if (_startflag || _endflag) {
	    char	*str;

	    count = 0;
	    XtSetArg(args[count], XmNeditable, True); count++;
	    XtSetArg(args[count], XmNtraversalOn, True); count++;
	    XtSetArg(args[count], XmNcursorPositionVisible, True); count++;
	    XtSetArg(args[count], XmNbackground, readwrite); count++;
	    if (_startflag)
		XtSetValues(_startText, args, count);
	    if (_endflag)
		XtSetValues(_endText, args, count);
	    if ((str = _path->fetchContent()) != NULL) {
		archiveNameChanged(NULL, NULL, (void *)str);
		free(str);
	    }
	}
	_path->show();
	_host->hide();
	_host->disableDrop();
	_path->enableDrop();
	_path->giveMeFocus();
    }
    else if (w == _hostToggle && cbs->set && !hostset) {
	hostset = 1;
	if (!readonly) {
	    if (_startflag)
		readonly = (Pixel) VkGetResource(_startText, "readOnlyBackground",
					"ReadOnlyBackground", XmRPixel, "Black");
	    else if (_endflag)
		readonly = (Pixel) VkGetResource(_endText, "readOnlyBackground",
					"ReadOnlyBackground", XmRPixel, "Black");
	}
	count = 0;
	XtSetArg(args[count], XmNeditable, False); count++;
	XtSetArg(args[count], XmNtraversalOn, False); count++;
	XtSetArg(args[count], XmNcursorPositionVisible, False); count++;
	XtSetArg(args[count], XmNbackground, readonly); count++;
	if (_startflag) {
	    XtSetValues(_startText, args, count);
	    XmTextFieldSetString(_startText, "");
	}
	if (_endflag) {
	    XtSetValues(_endText, args, count);
	    XmTextFieldSetString(_endText, "");
	}
	_host->show();
	_path->hide();
	_path->disableDrop();
	_host->enableDrop();
	_host->giveMeFocus();
    }
}

int
RunForm::launch()
{
    static char	*argv[4] = { "sh", "-c", &buf[0], NULL };
    pid_t	pid;
    int		i = 0;
    char	*tmp = NULL;
    char	*flag = NULL;
    char	*value = NULL;
    char	*cmdptr = &buf[0];
    Boolean	hostSelected = False;
    Boolean	archiveSelected = False;

    if (header != NULL && header[0] != '\0')
	cmdptr += sprintf(cmdptr, "%s ", header);

    cmdptr += sprintf(cmdptr, "%s", _runProgram);

    if (_hostflag && _archiveflag)
	hostSelected = XmToggleButtonGetState(_hostToggle);
    else if (_hostflag)
	hostSelected = True;

    if (hostSelected) {
	value = _host->fetchContent();
	tmp = stripwhite(value);
	if (value != NULL && tmp[0] != '\0') {
	    cmdptr += sprintf(cmdptr, " -h '%s'", tmp);
	    XtFree(value);
	    value = NULL;
	}
    }
    else if (_archiveflag) {
	value = _path->fetchContent();
	tmp = stripwhite(value);
	if (value != NULL && tmp[0] != '\0') {
	    cmdptr += sprintf(cmdptr, " -a '%s'", tmp);
	    archiveSelected = True;
	    XtFree(value);
	    value = NULL;
	}
    }

    if (archiveSelected && (_startflag || _endflag)) {
	char	*start = NULL;
	char	*end = NULL;
	char	*err = NULL;
	int	errflag = 0;
	struct timeval	unused;
	struct timeval	current;
	struct timeval	endtime;

	if (_startflag) {
	    tmp = XmTextFieldGetString(_startText);
	    start = strdup(stripwhite(tmp));
	    XtFree(tmp);
	}
	if (_endflag) {
	    tmp = XmTextFieldGetString(_endText);
	    end = strdup(stripwhite(tmp));
	    XtFree(tmp);
        }
	current.tv_sec = 0;		current.tv_usec = 0;
	endtime.tv_sec = INT_MAX;	endtime.tv_sec = 0;
	if (pmParseTimeWindow(start, end, NULL, NULL, &current, &endtime,
				&unused, &unused, &unused, &err) != 1) {
	    sprintf(buf, "Unexpected value in time field:\n\n");
	    strcat(buf, err);
	    theErrorDialog->postAndWait(buf);
	    errflag++;
	}
	else {
	    if (start != NULL && start[0] != '\0')
		cmdptr += sprintf(cmdptr, " -S '%s'", start);
	    if (end != NULL && end[0] != '\0')
		cmdptr += sprintf(cmdptr, " -T '%s'", end);
	}
	if (_startflag && start)
	    free(start);
	if (_endflag && end)
	    free(end);
	if (errflag)
	    return -1;
    }

    if (_deltaflag)
	cmdptr += sprintf(cmdptr, " -t '%f'", _delta);

    while ((flag = _flags.getFlag(i)) != NULL) {
	switch(_flags.getType(i)) {

	case PMRUN_TEXTFIELD:
	    value = XmTextFieldGetString(_flags.getWidget(i));
	    if (value != NULL && value[0] != '\0')
		cmdptr += sprintf(cmdptr, " %s '%s'", flag, value);
	    if (value)
		XtFree(value);
	    free(flag);
	    break;

	default:
	    pmprintf("%s: Unexpected specification (%d)\n",
					pmProgname, _flags.getType(i));
	    pmflush();
	}
	i++;
    }

    value = XmTextFieldGetString(_argsText);
    if (value != NULL && value[0] != '\0')
	cmdptr += sprintf(cmdptr, " %s", value);

    if (value)
	XtFree(value);

    argv[2] = &buf[0];

    if ((pid = fork()) < 0) {
	sprintf(buf, "%s: fork failed: %s\n", pmProgname, strerror(errno));
	theErrorDialog->postAndWait(buf);
	return -1;
    }
    else if (pid == 0) {
	if ((pid = fork()) < 0) {
	    sprintf(buf, "%s: fork failed: %s\n", pmProgname, strerror(errno));
	    theErrorDialog->postAndWait(buf);
	    return -1;
	}
	else if (pid > 0)
	    exit(0);
	/*
	 * we're the second child; our parent becomes init as soon
	 * as our real parent calls exit() in the statement above.
	 * now we continue executing, knowing that when we're done,
	 * init will reap our status.
	 */

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "%s exec: sh -c %s\n", pmProgname, buf);
#endif
	if (execvp(argv[0], argv) == -1) {
	    sprintf(buf, "%s: launch failed: %s\n", pmProgname, strerror(errno));
	    theErrorDialog->postAndWait(buf);
	}
	exit(0);
    }

    if (waitpid(pid, NULL, 0) != pid) {		// wait for first child
	sprintf(buf, "%s: waitpid error: %s", pmProgname, strerror(errno));
	theErrorDialog->postAndWait(buf);
	return -1;
    }

    return 0;
}

