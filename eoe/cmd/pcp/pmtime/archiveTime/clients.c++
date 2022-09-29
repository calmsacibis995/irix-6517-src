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
// $Id: clients.c++,v 1.21 1999/04/30 01:44:04 kenmcd Exp $
//
#include "VkPCParchiveTime.h"
#include "../timer/VkPCPtimer.h"
#include "VkPCParchiveTime.h"
#include "../common/message.h"

#include "ArchiveTimeForm.h"
#include "ArchiveBounds.h"

#include "timesrvr.h"

#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
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

#include "pixmaps.h"
#include "../common/tv.h"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "pmapi.h"
#include "impl.h"

extern int fdready(int);

/*ARGSUSED*/
void
ArchiveTimeForm::newClientCallback(VkCallbackObject *obj, void *, void *calldata)
{
    int fromClient, toClient;
    pmTime initParams;
    int sts = 0;

    if (!fdready(_control)) {
        // control port is not ready!
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_PDU)
        fprintf(stderr, "newClientCallback: control port not ready on fd=%d\n", _control);
#endif
        return;
    }

    if ((sts = __pmTimeAcceptClient(&toClient, &fromClient, &initParams)) < 0) {
	if (sts == -EINVAL) {
	    theErrorDialog->postAndWait(MSG_DOWN_REV);
	    // so the client will not block in pmTimeRecv
	    close(toClient);
	}
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "newClientCallback: failed to accept new client: %s\n", pmErrStr(sts));
#endif
    }
    else {
	_pmtvTimeVal l_tval(_state->data.delta, _state->data.vcrmode);
	if (!l_tval.checkMax(initParams.vcrmode, NULL, 0)) {
	    theErrorDialog->postAndWait(MSG_NO_XTB);
	    // so the client will not block in pmTimeRecv
	    close(toClient);
	    return;
	}

	if ((sts = __pmTimeAddClient(toClient, fromClient)) < 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "newClientCallback: failed to add new client: %s\n", pmErrStr(sts));
#endif
	}
	else {
	    VkInput *client = new VkInput();
	    client->attach(fromClient, XtInputReadMask);
	    client->addCallback(VkInput::inputCallback, this,
		(VkCallbackMethod) &ArchiveTimeForm::clientReadyCallback, client);

	    // acknowledge that initialization is complete
	    // and the client can start sending messages
	    sts = __pmTimeSendPDU(toClient, PM_TCTL_ACK, &_state->data);

	    // Register the clients intial params, must be done after the ACK
	    __timsrvrRegisterClient(fromClient, &initParams);

	    // initialize the new client by broadcasting the current state to all clients
	    syncAcks(1);
	    sts = __pmTimeBroadcast(PM_TCTL_VCRMODE, &_state->data);
	    sts = __pmTimeBroadcast(PM_TCTL_TZ, &_state->data);
	    sts = __pmTimeBroadcast(PM_TCTL_SHOWDIALOG, &_state->data);
	    syncAcks(1);
	    if (_currentDir == VCR_REVERSE)
		_state->data.delta *= -1; // make it negative
	    sts = __pmTimeBroadcast(PM_TCTL_SET, &_state->data);
	    if (_currentDir == VCR_REVERSE)
		_state->data.delta *= -1; // back to positive

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "newClientCallback: success fromClient=%d toClient=%d\n", fromClient, toClient);
#endif
	}
    }
}

void
ArchiveTimeForm::addTimezone(char *label, char *tz)
{
    VkPCParchiveTime *p = (VkPCParchiveTime *)_parent;
    p->addTimezone(label, tz, False);
}

/*ARGSUSED*/
void
ArchiveTimeForm::clientReadyCallback(VkCallbackObject *obj, void *clientData, void *calldata)
{
    VkInput *in = (VkInput *)clientData;
    int fd = in->fd();
    __pmTimeClient *client = __pmTimeFindClient(fd);
    int cmd;
    int nalive = -1;
    int newdir;

    if (client == NULL) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "clientReadyCallback: couldn't find client for fd=%d\n", in->fd());
#endif
    }
    else {
	if (!fdready(fd)) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_PDU) {
	    fprintf(stderr, "clientReadyCallback: reentered? for fd=%d\n", fd);
}
#endif
	    return; // ignore this callback
	}

	// recv should never block here
	cmd = __pmTimeRecvPDU(fd, &client->data);

	if (cmd < 0) {
	    // drop this client
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "clientReadyCallback: dropped client fd=%d (%s)\n", in->fd(), pmErrStr(cmd));
#endif
	    nalive = __pmTimeDelClient(in->fd()); // this will also close fd
#if defined(__linux__)
	    // fix viewkit core dump under linux problem
	    // note: this introduces a minor memory leak for now.
	    in->remove();
#else
	    in->removeAllCallbacks();
	    delete in;
#endif
	}
	else {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "clientReadyCallback: fd=%d cmd=%s\n", in->fd(), __pmTimeCmdStr(cmd));
#endif
	    switch (cmd) {
	    case PM_TCTL_BOUNDS:
		// Let the archive bounds dialog handle this.
		// We will get a callback if any action is required.
		if (_archiveBounds->isCreated() == True)
		    _archiveBounds->addBounds(&client->data.start, &client->data.finish);
		break;

	    case PM_TCTL_TZ:
		// add to the timezone menu
		addTimezone(client->data.tzlabel, client->data.tz);
		break;

	    case PM_TCTL_ACK:
		client->wantAck = 0;
		break;

	    case PM_TCTL_SHOWDIALOG:
		showDialog(client->data.showdialog);
		break;
	    
	    case PM_TCTL_SET:
		{ // new scope for time delta objects

		// client requests a new position and/or delta
		setPosition(&client->data.position);
		newdir = (client->data.delta < 0) ? VCR_REVERSE : VCR_FORWARD;

		_pmtvTimeVal magdelta((client->data.delta < 0) ? -client->data.delta : client->data.delta, client->data.vcrmode);
		_pmtvTimeVal statedelta(_state->data.delta, _state->data.vcrmode);

		if (_currentDir != newdir || magdelta != statedelta) {
		    vcrMode newmode = _vcrMode;
		    if (_currentDir != newdir) {
			switch (_vcrMode) {
			case VCR_REV_PLAY:
			    newmode = VCR_FOR_PLAY;
			    break;
			case VCR_FOR_PLAY:
			    newmode = VCR_REV_PLAY;
			    break;
			case VCR_REV_FAST:
			    newmode = VCR_FOR_FAST;
			    break;
			case VCR_FOR_FAST:
			    newmode = VCR_REV_FAST;
			    break;
			default:
			    break;
			}
		    }
		    if (magdelta != statedelta)
			setInterval(magdelta);
		    if (newmode != _vcrMode) {
			setVcrMode(newmode, 1);
		    }
		}
		}
		break;

	    case PM_TCTL_VCRMODE:
		// client requests a new vcr mode
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
		fprintf(stderr, "\tclient requests vcrmode=%d\n", client->data.vcrmode & __PM_MODE_MASK);
#endif
		switch (client->data.vcrmode & __PM_MODE_MASK) {
		case PM_TCTL_VCRMODE_STOP:
		case PM_TCTL_VCRMODE_DRAG:
		    setVcrMode(VCR_STOP, 1);
		    break;
		case PM_TCTL_VCRMODE_PLAY:
		    setVcrMode((client->data.delta < 0) ? VCR_REV_PLAY : VCR_FOR_PLAY, 1);
		    break;
		case PM_TCTL_VCRMODE_FAST:
		    setVcrMode((client->data.delta < 0) ? VCR_REV_FAST : VCR_FOR_FAST, 1);
		}

		break;
		    
	    default: // all other commands are ignored
		break;
	    }

	}
    }

    // if there are no clients left, we might want to exit.
    if (nalive == 0) {
	theApplication->quitYourself();
    }
}

void
ArchiveTimeForm::syncAcks(int timeout)
{
    int sts = 0;
    struct timeval waitime;

    if ((sts = __pmTimeAcksPending(NULL)) > 0) {
	_runningSlow = 1;
	theApplication->busyCursor();

	waitime.tv_sec = timeout;
	waitime.tv_usec = 0;

	while (sts > 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "syncAcks: waiting for %d clients\n", sts);
#endif
	    // only process pending events for client acks
	    XtAppProcessEvent(XtWidgetToApplicationContext(theApplication->baseWidget()), XtIMAlternateInput);
	    sts = __pmTimeAcksPending(&waitime);
	}
	theApplication->normalCursor();
	_runningSlow = 0;
    }
}


void
ArchiveTimeForm::showDialog(int show, int notifyClients)
{
    _state->data.showdialog = show;
    if (show) {
	_parent->show();
	_parent->open();
	showPosition();
	setTimezone(_state->data.tzlabel);
    }
    else {
	_parent->hide();
	if (_archiveBounds->isCreated() == True)
	    _archiveBounds->hide();
    }

    if (notifyClients) {
	syncAcks(1);
	__pmTimeBroadcast(PM_TCTL_SHOWDIALOG, &_state->data);
    }

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "showDialog: %s\n", show ? "True" : "False");
#endif
}

void
ArchiveTimeForm::vcrStep(vcrDirection dir)
{
    struct timeval *pos = &_state->data.position;
    _pmtvTimeVal dpos(*pos);
    _pmtvTimeVal statedelta(_state->data.delta, _state->data.vcrmode);
    int stopped = 0;
    int sts = 0;

    syncAcks(1);

    if (dir == VCR_REVERSE) {
	if ((dpos - statedelta) < _pmtvTimeVal(_state->data.start)) {
	    setVcrMode(VCR_STOP, 1);
	    stopped++;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "vcrStep: forced STOP in REW at start\n");
#endif
	}
    }
    else {
	if (_pmtvTimeVal(_state->data.finish) < (dpos + statedelta)) {
	    setVcrMode(VCR_STOP, 1);
	    stopped++;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "vcrStep: forced STOP in FORW at finish\n");
#endif
	}
    }

    if (!stopped) {

	if (_currentDir != dir) {
	    if (dir == VCR_REVERSE)
		_state->data.delta *= -1; // make it negative

	    sts = __pmTimeBroadcast(PM_TCTL_SET, &_state->data);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    if (sts < 0)
		fprintf(stderr, "vcrStep: failed to send SET command: %s\n", pmErrStr(sts));
#endif

	    if (dir == VCR_REVERSE)
		_state->data.delta *= -1; // make it positive again
	    _currentDir = dir;
	}

	_pmtvadd(pos, dir == VCR_REVERSE ? -(_state->data.delta) : _state->data.delta, _state->data.vcrmode);
	showPosition();

	// now, finally, send the step command
	sts = __pmTimeBroadcast(PM_TCTL_STEP, &_state->data);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
	char tzbuf[64];
	if (sts < 0)
	    fprintf(stderr, "vcrStep: failed to send STEP command: %s\n", pmErrStr(sts));
	fprintf(stderr, "vcrStep %s pos=%.24s.%03d\n",
	dir == VCR_FORWARD ? "VCR_FORWARD" : "VCR_REVERSE",
	pmCtime(&_state->data.position.tv_sec, tzbuf), _state->data.position.tv_usec / 1000);
}
#endif
    }
}

static char *vcrModeString(vcrMode mode)
{
    return
    mode == VCR_STOP ? "VCR_STOP" :
    mode == VCR_FOR_PLAY ? "VCR_FOR_PLAY" :
    mode == VCR_FOR_FAST ? "VCR_FOR_FAST" :
    mode == VCR_REV_PLAY ? "VCR_REV_PLAY" :
    mode == VCR_REV_FAST ? "VCR_REV_FAST" : "unknown";
}

// Check that the combination of the update interval and the current speed
// will create a timer interval of 24 days or less.
// XtAppAddTimeOut only handles milliseconds.
//
int
ArchiveTimeForm::validateVcrMode(vcrMode newMode)
{
    _pmtvTimeVal statedelta(_state->data.delta, _state->data.vcrmode);
    statedelta = statedelta / _currentSpeed;
    int mode = PM_XTB_SET(PM_TIME_MSEC);

    switch(newMode) {
    case VCR_FOR_PLAY:
    case VCR_REV_PLAY:
	if (!statedelta.checkMax(mode, NULL, 0)) {
	    theErrorDialog->postAndWait("The value calculated from \n(update interval / Current Speed) must be <= 24 days.");
	    return 0;
	}
	break;
    default:
	break;
    }
    return 1;
}

void
ArchiveTimeForm::setVcrMode(vcrMode newMode, int updatePixmaps)
{
    //
    // Do not wait for the X event to empty. Fix for bug #533870
    // With a remote display and a short update interval in archive mode,
    // it's possible this call will not return because there will always
    // be pending events (timer callbacks in the X event queue) and hence
    // this calls effectively becomes the X main loop and we never actually
    // get to process the VCR button press!
    //
    // theApplication->handlePendingEvents(); // wait until X event queue is empty
    //
#ifdef PCP_DEBUG
    static int entered = 0;
    if (pmDebug & DBG_TRACE_APPL2) {
	entered++;

	fprintf(stderr, "ENTER (level %d) setVcrMode updatepixmaps=%d vcrMode=%s newmode=%s delta=%d\n",
	entered, updatePixmaps, vcrModeString(_vcrMode), vcrModeString(newMode), _state->data.delta);
	if (entered > 1) {
	    fprintf(stderr, "RE-ENTERED callback\n");
	}
    }
#endif

    if (!validateVcrMode(newMode))
	return;

    VkMenuItem *vcrOptionMode = _vcrOption->getItem();

    switch (newMode) {
    case VCR_STOP:
	if (updatePixmaps) {
	    VkSetHighlightingPixmap (_vcrStop, stop_on);
	    if (vcrOptionMode == _vcrOptionFast) {
		VkSetHighlightingPixmap(_vcrForward, ff_off);
		VkSetHighlightingPixmap(_vcrReverse, rw_off);
	    }
	    else
	    if (vcrOptionMode == _vcrOptionStep) {
		VkSetHighlightingPixmap(_vcrForward, step_off);
		VkSetHighlightingPixmap(_vcrReverse, rstep_off);
	    }
	    else {
		VkSetHighlightingPixmap(_vcrForward, play_off);
		VkSetHighlightingPixmap(_vcrReverse, rev_off);
	    }
	}
	_timer->stop();
	if ((_state->data.vcrmode & __PM_MODE_MASK) != PM_TCTL_VCRMODE_DRAG)
	    setIndicatorState(PM_TCTL_VCRMODE_STOP);
	break;

    case VCR_FOR_PLAY:
	if (updatePixmaps) {
	    VkSetHighlightingPixmap(_vcrForward, play_on);
	    VkSetHighlightingPixmap(_vcrStop, stop_off);
	    VkSetHighlightingPixmap(_vcrReverse, rev_off);
	}
	setIndicatorState(PM_TCTL_VCRMODE_PLAY);
	break;

    case VCR_REV_PLAY:
	if (updatePixmaps) {
	    VkSetHighlightingPixmap (_vcrReverse, rev_on);
	    VkSetHighlightingPixmap(_vcrForward, play_off);
	    VkSetHighlightingPixmap(_vcrStop, stop_off);
	}
	setIndicatorState(PM_TCTL_VCRMODE_PLAY);
	break;

    case VCR_FOR_FAST:
	if (updatePixmaps) {
	    VkSetHighlightingPixmap (_vcrForward, ff_on);
	    VkSetHighlightingPixmap(_vcrStop, stop_off);
	    VkSetHighlightingPixmap(_vcrReverse, rw_off);
	}
	setIndicatorState(PM_TCTL_VCRMODE_FAST);
	break;

    case VCR_REV_FAST:
	if (updatePixmaps) {
	    VkSetHighlightingPixmap (_vcrReverse, rw_on);
	    VkSetHighlightingPixmap(_vcrStop, stop_off);
	    VkSetHighlightingPixmap(_vcrForward, ff_off);
	}
	setIndicatorState(PM_TCTL_VCRMODE_FAST);
	break;

    default:
	break;
    }

    _vcrMode = newMode;

    if (_vcrMode != VCR_STOP) {
	// start immediately
	_timer->start(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "LEAVE (level %d) setVcrMode updatepixmaps=%d vcrMode=%s newmode=%s delta=%d\n",
	entered, updatePixmaps, vcrModeString(_vcrMode), vcrModeString(newMode), _state->data.delta);
	entered--;
    }
#endif
}

/*ARGSUSED*/
void
ArchiveTimeForm::timerCallback(VkCallbackObject *obj, void *, void *calldata)
{
    _pmtvTimeVal statedelta(_state->data.delta, _state->data.vcrmode);
    double realTimeDelta = (double)(statedelta / _currentSpeed).getValue(_PMTV_UNITS_MSEC);

    if (_runningSlow) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "timerCallback: runningSlow, ignored callback\n");
#endif
	//
	// don't try and step
	//
	return;
    }

    switch (_vcrMode) {
    case VCR_FOR_PLAY:
	vcrStep(VCR_FORWARD);
	_timer->start((int)realTimeDelta);
	break;

    case VCR_FOR_FAST:
	vcrStep(VCR_FORWARD);
	// one millisecond timer
	_timer->start(1);
	break;

    case VCR_REV_PLAY:
	vcrStep(VCR_REVERSE);
	_timer->start((int)realTimeDelta);
	break;

    case VCR_REV_FAST:
	vcrStep(VCR_REVERSE);
	// one millisecond timer
	_timer->start(1);
	break;

    default:
	_timer->stop();
	break;
    }
}

