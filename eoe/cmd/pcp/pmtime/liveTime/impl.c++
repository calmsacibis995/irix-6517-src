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

//
// $Id: impl.c++,v 1.8 1999/04/30 01:44:04 kenmcd Exp $
//
#include "VkPCPliveTime.h"
#include "LiveTimeForm.h"

#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkOptionMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkErrorDialog.h>

#ifdef IRIX5_3
#include "VkPixmap.h"
#else
#include <Vk/VkPixmap.h>
#endif

#include <Xm/Text.h>
#include <Xm/Scale.h>
#include <Xm/ToggleB.h>
#include <Sgm/ThumbWheel.h>

#include "../timer/VkPCPtimer.h"
#include "pixmaps.h"
#include "tv.h"

#include "pmapi.h"
#include "impl.h"

static char buf[512];

int
LiveTimeForm::initialize(int control, int client, pmTime *initState)
{
    int sts = 0;

    // initial state is STOP. pmtime will call this->start()
    // Protect any extended time data which may be set
    initState->vcrmode = (initState->vcrmode & 0xffff0000) | PM_TCTL_VCRMODE_STOP;
    if (initState->delta < 0)
	initState->delta *= -1;

    _control = control;
    __pmTimeGetState(&_state);
    memcpy(&_state->data, initState, sizeof(pmTime));

    // create the new client handler using the control port
    _clientHandler = new VkInput();
    _clientHandler->addCallback(VkInput::inputCallback, this,
	(VkCallbackMethod) &LiveTimeForm::newClientCallback);
    _clientHandler->attach(_control, XtInputReadMask);

    // create a timer and assign it's callback
    _timer = new VkPCPtimer();
    _timer->addCallback(VkPCPtimer::timerCallback, this, 
	(VkCallbackMethod) &LiveTimeForm::timerCallback);

    // initialize the first-client-invariant gui widgets
    _unitsOption->set(_intervalUnitsSeconds);
    _vcrMode = VCR_STOP;
    _detailedPositions = False;
    _showYear = True;
    showInterval();

    // create the first client's input callback
    VkInput *newClient = new VkInput();
    newClient->attach(client, XtInputReadMask);
    newClient->addCallback(VkInput::inputCallback, this,
	(VkCallbackMethod) &LiveTimeForm::clientReadyCallback, newClient);

    addTimezone(initState->tzlabel, initState->tz);
    addTimezone("UTC", "UTC");
    setTimezone(initState->tzlabel);

    VkSetHighlightingPixmap ( _vcrStop, stop_on );
    VkSetHighlightingPixmap ( _vcrForward, live_off );

    showDialog(initState->showdialog);

    return sts;
}

void 
LiveTimeForm::setIntervalUnits(void)
{
    VkMenuItem *i = _unitsOption->getItem();
    
    if (i == _intervalUnitsMilliseconds) {
	ival_fmt = "%.0f";
	ival_units = _PMTV_UNITS_MSEC;
    }
    else
    if (i == _intervalUnitsSeconds){
	ival_fmt = "%.2f";
	ival_units = _PMTV_UNITS_SEC;
    }
    else
    if (i == _intervalUnitsMinutes) {
	ival_fmt = "%.4f";
	ival_units = _PMTV_UNITS_MIN;
    }
    else
    if (i == _intervalUnitsHours) {
	ival_fmt = "%.6f";
	ival_units = _PMTV_UNITS_HOUR;
    }
    else
    if (i == _intervalUnitsDays) {
	ival_fmt = "%.8f";
	ival_units = _PMTV_UNITS_DAY;
    }
    else
    if (i == _intervalUnitsWeeks) {
	ival_fmt = "%.10f";
	ival_units = _PMTV_UNITS_WEEK;
    }
}


void
LiveTimeForm::showInterval()
{
    setIntervalUnits();

    _pmtvTimeVal l_tval(_state->data.delta, _state->data.vcrmode);
    float delta = l_tval.getValue(ival_units);

    sprintf(buf, ival_fmt, delta);

    XmTextSetString(_interval, buf);
}

void
LiveTimeForm::setInterval(char *newTime)
{
    float f;
    int l_mode = PM_XTB_SET(PM_TIME_MSEC);

    sscanf(newTime, "%f", &f);

    setIntervalUnits();
    _pmtvTimeVal f_tval(f, ival_units);

    if (f_tval < _pmtvTimeVal(1, PM_TIME_MSEC)) {
	theErrorDialog->postAndWait("The update interval must be >= 1 millisecond.");
	showInterval();
    }
    else if (!f_tval.checkMax(l_mode, buf, sizeof(buf))) {
	// the interval for live time must always be <= 24 days, since 
	// XtAppAddTimeOut used as the timer only takes milliseconds
	// as the interval argument.
	theErrorDialog->postAndWait("The update interval must be <= 24 days.");
	showInterval();
    }
    else
	setInterval(f_tval);
}

void
LiveTimeForm::setInterval(_pmtvTimeVal &ival)
{
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "setInterval(millisecs = %f\n", ival.getValue( _PMTV_UNITS_MSEC));
#endif

    if (ival != _pmtvTimeVal(_state->data.delta, _state->data.vcrmode)) {
	ival.getXTB(_state->data.delta, _state->data.vcrmode);
	showInterval();
	syncAcks(1);
	__pmTimeBroadcast(PM_TCTL_SET, &_state->data);
	setVcrMode(_vcrMode); // reset the timer
    }
}

void
LiveTimeForm::showPosition()
{
    char timestring[64];
    char tzbuf[32];
    char year[16];

    sprintf(timestring, "%.24s", pmCtime(&_state->data.position.tv_sec, tzbuf));
    strcpy(year, &timestring[19]);
    timestring[19] = NULL;

    if (_detailedPositions) {
	sprintf(buf, ".%03d", _state->data.position.tv_usec / 1000);
	strcat(timestring, buf);
    }

    if (_showYear == True)
	strcat(timestring, year);

    XmTextSetString(_position, timestring);
}

void
LiveTimeForm::setTimezone(char *label)
{
    int handle = __pmTimeGetTimezone(label);
    static int entered = 0;
    char *tz;

    if (entered) {
	// don't recuse when setStateAndNotify is used below
	return;
    }
    entered=1;

    if (handle >= 0) {
	pmUseZone(handle);
	showPosition();
	pmWhichZone(&tz);
	strncpy(_state->data.tz, tz, sizeof(_state->data.tz));
	_state->data.tz[sizeof(_state->data.tz)-1] = '\0';
	strncpy(_state->data.tzlabel, label, sizeof(_state->data.tzlabel));
	_state->data.tzlabel[sizeof(_state->data.tzlabel)-1] = '\0';
	syncAcks(1);
	__pmTimeBroadcast(PM_TCTL_TZ, &_state->data);

	VkPCPliveTime *par = (VkPCPliveTime *)_parent;
	VkSubMenu *men = par->getTimezoneRadioPane();
	VkMenuToggle *tog = (VkMenuToggle *)men->findNamedItem(label);
	if (tog != NULL) {
	    tog->setStateAndNotify(True);
	}
    }
    entered=0;

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "LiveTimeForm::setTimezone(label=%s)\n", label);
#endif
}

void
LiveTimeForm::setIndicatorState(int newState)
{
    // This is always called with a VCRMODE value only.
    if (newState != (_state->data.vcrmode & __PM_MODE_MASK)) {
	_state->data.vcrmode = (_state->data.vcrmode & 0xffff0000) | newState;
	syncAcks(1);
	__pmTimeBroadcast(PM_TCTL_VCRMODE, &_state->data);
    }
}


