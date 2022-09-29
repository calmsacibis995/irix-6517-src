/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * states.   use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * u.s. government restricted rights legend:
 * use, duplication or disclosure by the government is subject to restrictions
 * as set forth in far 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the rights
 * in technical data and computer software clause at dfars 252.227-7013 and/or
 * in similar or successor clauses in the far, or the dod or nasa far
 * supplement.  contractor/manufacturer is silicon graphics, inc.,
 * 2011 n. shoreline blvd. mountain view, ca 94039-7311.
 * 
 * the content of this work contains confidential and proprietary
 * information of silicon graphics, inc. any duplication, modification,
 * distribution, or disclosure in any form, in whole, or in part, is strictly
 * prohibited without the prior express written permission of silicon
 * graphics, inc.
 */

//
// $Id: impl.c++,v 1.15 1999/04/30 01:44:04 kenmcd Exp $
//
#include "VkPCParchiveTime.h"
#include "ArchiveTimeForm.h"
#include "ArchiveBounds.h"

#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkOptionMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkWarningDialog.h>

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
#include "../common/tv.h"

#include "pmapi.h"
#include "impl.h"

static char buf[512];

int
ArchiveTimeForm::initialize(int control, int client, pmTime *initState)
{
    int sts = 0;

    // forced initial state
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
	(VkCallbackMethod) &ArchiveTimeForm::newClientCallback);
    _clientHandler->attach(_control, XtInputReadMask);

    // create a timer and assign it's callback
    _timer = new VkPCPtimer();
    _timer->addCallback(VkPCPtimer::timerCallback, this, 
	(VkCallbackMethod) &ArchiveTimeForm::timerCallback);

    // initial replay speed
    _currentSpeed = 1.0;
    XtVaSetValues(_speedThumb,
	SgNhomePosition,	0,
	SgNunitsPerRotation,	5,
	XmNminimum,		-99, // allows speeds between 0.01 and 100 
	XmNmaximum,		99,
	XmNvalue,		0,
	NULL);
    showSpeed();

    // initialize the first-client-invariant gui widgets
    _unitsOption->set(_intervalUnitsSeconds);
    _vcrMode = VCR_STOP;
    _detailedPositions = False;
    _showYear = True;
    showInterval();


    // create the first client's input callback
    VkInput *firstClient = new VkInput();
    firstClient->attach(client, XtInputReadMask);
    firstClient->addCallback(VkInput::inputCallback, this,
	(VkCallbackMethod) &ArchiveTimeForm::clientReadyCallback, firstClient);

    addTimezone(initState->tzlabel, initState->tz);
    addTimezone("UTC", "UTC");
    setTimezone(initState->tzlabel);

    VkSetHighlightingPixmap(_vcrForward, play_off);
    VkSetHighlightingPixmap(_vcrStop, stop_on);
    VkSetHighlightingPixmap(_vcrReverse, rev_off);

    showDialog(initState->showdialog);

    _archiveBounds->addBounds(&initState->start, &initState->finish, True);

    return sts;
}

void 
ArchiveTimeForm::setIntervalUnits(void)
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
ArchiveTimeForm::showInterval()
{
    setIntervalUnits();

    _pmtvTimeVal l_tval(_state->data.delta, _state->data.vcrmode);
    float delta = l_tval.getValue(ival_units);

    sprintf(buf, ival_fmt, delta);

    XmTextSetString(_interval, buf);
}

void
ArchiveTimeForm::setInterval(char *newTime)
{
    float f;

    sscanf(newTime, "%f", &f);

    setIntervalUnits();
    _pmtvTimeVal f_tval(f, ival_units);
    _pmtvTimeVal m_tval(_state->data.finish);
    m_tval = m_tval - _pmtvTimeVal(_state->data.start);

    if (f_tval < _pmtvTimeVal(1, PM_TIME_MSEC)) {
	theErrorDialog->postAndWait("The update interval must be >= 1 millisecond.");
	showInterval();
    }
    else if (!f_tval.checkMax(_state->data.vcrmode, buf, sizeof(buf)) && 
	     !(_state->data.vcrmode & PM_XTB_FLAG)) {
	theErrorDialog->postAndWait(buf);
	showInterval();
    }
    else if ((m_tval/2) < f_tval) {
	sprintf(buf, "The time period currently covers %.2f seconds. It only makes sense to use an update interval\nthat is at most half this, hence the maximum update interval allowed is %.2f seconds.", 
	m_tval.getValue(_PMTV_UNITS_SEC), (m_tval/2).getValue(_PMTV_UNITS_SEC));
	theErrorDialog->postAndWait(buf);
	showInterval();
    }
    else
	setInterval(f_tval);
}

void
ArchiveTimeForm::setInterval(_pmtvTimeVal &ival)
{
    int sts;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "setInterval(millisecs = %f) _vcrMode=%d\n", 
	    ival.getValue( _PMTV_UNITS_MSEC), _vcrMode);
#endif
    ival.getXTB(_state->data.delta, _state->data.vcrmode);
    if (_vcrMode != VCR_STOP)
	_timer->stop();
    showInterval();

    syncAcks(1);
    if (_currentDir == VCR_REVERSE)
	_state->data.delta *= -1; // make it negative
    sts = __pmTimeBroadcast(PM_TCTL_SET, &_state->data);
    if (_currentDir == VCR_REVERSE)
	_state->data.delta *= -1; // back to positive
    if (sts < 0) {
	theErrorDialog->postAndWait("ArchiveTimeForm::setInterval: __pmTimeBroadcast failed");
    }
    if (_vcrMode != VCR_STOP)
	_timer->start(1);
}


void
ArchiveTimeForm::showPosition()
{
    double duration = __pmtimevalSub(&_state->data.finish, &_state->data.start);
    double pos = __pmtimevalSub(&_state->data.position, &_state->data.start);
    int percent;
    static int last;
    char tzbuf[64];
    char timestring[64];
    char year[16];

    if (duration == 0)
	percent = 0;
    else
	percent = (int)(100 * pos / duration);
    
    if (percent > 100)
	percent = 100;
    
    if (percent < 0)
	percent = 0;

    if (last != percent)
	XmScaleSetValue(_positionScale, percent);
    last = percent;

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
ArchiveTimeForm::setPosition(char *newPosition)
{
    struct tm anytime;
    struct timeval pos;
    char tzbuf1[64], tzbuf2[64];
    char *err;

    int sts = __pmParseCtime(newPosition, &anytime, &err);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "setPosition(char *newPosition=\"%s\")\n", newPosition);
#endif

    if (sts < 0) {
	sprintf(buf, "\"%s\" is invalid:%s", newPosition, err);
	theErrorDialog->postAndWait(buf);
    }
    else
    if (sts == 0) {
	if ((sts = __pmConvertTime(&anytime, &_state->data.start, &pos)) == 0) {
	    if (pos.tv_sec < _state->data.start.tv_sec) {
		sprintf(buf, "The new position will be the start of the time bounds (%.24s)\nbecause the requested time (%.24s) is before the start", pmCtime(&_state->data.start.tv_sec, tzbuf1), pmCtime(&pos.tv_sec, tzbuf2));
		theWarningDialog->postAndWait(buf);
		pos = _state->data.start; // struct cpy
	    }
	    else
	    if (pos.tv_sec > _state->data.finish.tv_sec) {
		sprintf(buf, "The new position will be the finish of the time bounds (%.24s)\nbecause the requested time (%.24s) is after the finish", pmCtime(&_state->data.finish.tv_sec, tzbuf1), pmCtime(&pos.tv_sec, tzbuf2));
		theWarningDialog->postAndWait(buf);
		pos = _state->data.finish; // struct cpy
	    }

	    setPosition(&pos);
	}
    }

    showPosition();
}

void
ArchiveTimeForm::setPosition(struct timeval *newPosition)
{
    int sts;

    if (_vcrMode != VCR_STOP) {
	_timer->stop();
    }
    _state->data.position = *newPosition;
    showPosition();
    syncAcks(1);

    if (_currentDir == VCR_REVERSE)
	_state->data.delta *= -1; // make it negative
    sts = __pmTimeBroadcast(PM_TCTL_SET, &_state->data);
    if (_currentDir == VCR_REVERSE)
	_state->data.delta *= -1; // back to positive
    if (sts < 0) {
	theErrorDialog->postAndWait("ArchiveTimeForm::setPosition: __pmTimeBroadcast failed");
    }
    if (_vcrMode != VCR_STOP) {
	_timer->start(1);
    }

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    char tzbuf[64];
    fprintf(stderr, "setPosition(newPosition=%.24s.%03d)\n", pmCtime(&newPosition->tv_sec, tzbuf), newPosition->tv_usec / 1000);
}
#endif
}

void
ArchiveTimeForm::setPosition(int percent)
{
    struct timeval newTime;

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "setPosition(percent=%d)\n", percent);
#endif

    if (_vcrMode != VCR_STOP)
	setVcrMode(VCR_STOP);

    double duration = __pmtimevalSub(&_state->data.finish, &_state->data.start);
    double newpos = _pmtv2double(&_state->data.start) + percent * duration / 100.0;

    _pmdouble2tv(newpos, &newTime);
    setPosition(&newTime);
}

void
ArchiveTimeForm::showSpeed()
{
    sprintf(buf, "%.2lf", _currentSpeed);
    XmTextSetString(_speed, buf);
}

void
ArchiveTimeForm::setSpeed(double newSpeed)
{
    _currentSpeed = newSpeed;
    showSpeed();
    setVcrMode(_vcrMode, 0);
}

void
ArchiveTimeForm::setSpeed(char *newSpeed)
{
    float f;
    int max;

    sscanf(newSpeed, "%f", &f);
    XtVaGetValues(_speedThumb,
	XmNmaximum,	&max,
	NULL);
    XtVaSetValues(_speedThumb,
	XmNvalue,	(int)(100 * f / max),
	NULL);
	
    setSpeed((double)f);
}

void
ArchiveTimeForm::setTimezone(char *label)
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
	if (_archiveBounds->isCreated() == True) {
	    _archiveBounds->showBounds();
	}
        showPosition();
        pmWhichZone(&tz);
	strncpy(_state->data.tz, tz, sizeof(_state->data.tz));
	_state->data.tz[sizeof(_state->data.tz)-1] = '\0';
	strncpy(_state->data.tzlabel, label, sizeof(_state->data.tzlabel));
	_state->data.tzlabel[sizeof(_state->data.tzlabel)-1] = '\0';
	syncAcks(1);
        __pmTimeBroadcast(PM_TCTL_TZ, &_state->data);

        VkPCParchiveTime *par = (VkPCParchiveTime *)_parent;
        VkSubMenu *men = par->getTimezoneRadioPane();
        VkMenuToggle *tog = (VkMenuToggle *)men->findNamedItem(label);
        if (tog != NULL) {
	    tog->setStateAndNotify(True);
        }
    }
    entered=0;

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveTimeForm::setTimezone(label=%s)\n", label);
#endif
}

/*ARGSUSED*/
void
ArchiveTimeForm::boundsChangedCB(VkCallbackObject* obj, void *clientData, void *callData)
{
    double s = ((double *)callData)[0];
    double f = ((double *)callData)[1];
    double p = _pmtv2double(&_state->data.position);

    _pmdouble2tv(s, &_state->data.start);
    _pmdouble2tv(f, &_state->data.finish);

    if (s > p)
	setPosition(&_state->data.start);
    else
    if (f < p)
	setPosition(&_state->data.finish);
    else
	showPosition();

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveTimeForm::boundsChangedCB %.2lf %.2lf\n", s, f);
#endif
}


void
ArchiveTimeForm::setBounds(struct timeval *s, struct timeval *f)
{
    _state->data.start = *s;
    _state->data.finish = *f;
    if (_archiveBounds->isCreated() == True)
	_archiveBounds->setBounds(s, f);
}

void
ArchiveTimeForm::setIndicatorState(int newState)
{
    if (newState != (_state->data.vcrmode & __PM_MODE_MASK)) {
	// Preserve any extended time base data
	_state->data.vcrmode = (_state->data.vcrmode & 0xffff0000) | newState;
	syncAcks(1);
	__pmTimeBroadcast(PM_TCTL_VCRMODE, &_state->data);
    }
}


