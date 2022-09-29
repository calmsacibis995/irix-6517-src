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
// $Id: clients.c++,v 1.20 1999/04/30 01:44:04 kenmcd Exp $
//
#include "VkPCPliveTime.h"
#include "../timer/VkPCPtimer.h"
#include "../common/message.h"
#include "tv.h"
#include "timesrvr.h"
#include "LiveTimeForm.h"

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
#include <Xm/ToggleB.h>

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
LiveTimeForm::newClientCallback(VkCallbackObject *obj, void *, void *calldata)
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
	    // create the client's input callback
	    VkInput *client = new VkInput();
	    client->attach(fromClient, XtInputReadMask);
	    client->addCallback(VkInput::inputCallback, this,
		(VkCallbackMethod) &LiveTimeForm::clientReadyCallback, client);

            // acknowledge that initialization is complete
            // and the client can start sending messages
            sts = __pmTimeSendPDU(toClient, PM_TCTL_ACK, &_state->data);

	    // Register the clients intial params, must be done after the ACK
	    __timsrvrRegisterClient(fromClient, &initParams);

            // initialize the new client by broadcasting the current state to all clients
            syncAcks(1);
            sts = __pmTimeBroadcast(PM_TCTL_VCRMODE, &_state->data);
            sts = __pmTimeBroadcast(PM_TCTL_TZ, &_state->data);
            sts = __pmTimeBroadcast(PM_TCTL_SET, &_state->data);
            sts = __pmTimeBroadcast(PM_TCTL_SHOWDIALOG, &_state->data);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "newClientCallback: success fromClient=%d toClient=%d\n", fromClient, toClient);
#endif
	}
    }
}

/*ARGSUSED*/
void
LiveTimeForm::addTimezone(char *label, char *tz, Boolean setItem)
{
    VkPCPliveTime *p = (VkPCPliveTime *)_parent;
    p->addTimezone(label, tz);
}

/*ARGSUSED*/
void
LiveTimeForm::clientReadyCallback(VkCallbackObject *obj, void *clientData, void *calldata)
{
    VkInput *in = (VkInput *)clientData;
    int fd = in->fd();
    __pmTimeClient *client = __pmTimeFindClient(fd);
    int cmd;
    int nalive = -1;

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
	    switch (cmd) {
	    case PM_TCTL_BOUNDS:
		break;

	    case PM_TCTL_TZ:
		// add to the timezone menu
		addTimezone(client->data.tzlabel, client->data.tz);
		break;

	    case PM_TCTL_ACK:
		if (client->wantAck == 2) {
		    // This client has just returned from being hung.
		    // Send a SET and VCRMODE commands so it can resync.
		    client->wantAck = 0;
		    __pmTimeSendPDU(client->toClient, PM_TCTL_VCRMODE, &_state->data);
		    __pmTimeSendPDU(client->toClient, PM_TCTL_SET, &_state->data);
		}
		else
		    client->wantAck = 0;
		break;

	    case PM_TCTL_SHOWDIALOG:
		showDialog(client->data.showdialog);
		break;
	    
	    case PM_TCTL_VCRMODE:
                // client requests a new vcr mode
		// In live mode, clients may request STOP or PLAY forwards.
		// Anything else is ignored.
                switch (client->data.vcrmode & __PM_MODE_MASK) {
                case PM_TCTL_VCRMODE_STOP:
		    setVcrMode(VCR_STOP, 1);
                    break;
                case PM_TCTL_VCRMODE_PLAY:
		    setVcrMode(VCR_LIVE, 1);
                    break;
                }

                break;
		    
	    default: // all other commands ignored
		break;
	    }

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "clientReadyCallback: fd=%d cmd=%s\n", in->fd(), __pmTimeCmdStr(cmd));
#endif
	}
    }

    // if there are no clients left, we might want to exit.
    if (nalive == 0) {
	theApplication->quitYourself();
    }
}

void
LiveTimeForm::syncAcks(int timeout)
{
    int sts = 0;
    struct timeval waitime;

    if ((sts = __pmTimeAcksPending(NULL)) > 0) {
	_runningSlow = 1;
	theApplication->busyCursor();
	// wait for one update interval for clients with pending acks to respond
	if (sts > 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "syncAcks: waiting for %d clients (timeout=%ds)\n", sts, timeout);
#endif
	    // only process pending events for client acks and UI, but no timer callbacks
	    XtAppProcessEvent(XtWidgetToApplicationContext(theApplication->baseWidget()), 
		XtIMAlternateInput|XtIMXEvent);

	    _pmtvTimeVal statedelta(_state->data.delta, _state->data.vcrmode);
	    statedelta.getTimeval(waitime);
	    sts = __pmTimeAcksPending(&waitime);
	}
	theApplication->normalCursor();
	_runningSlow = 0;
    }

    // any clients with remaining acks enter the hung state (wantAck==2)
    __pmTimeSetHungClients();
}


void
LiveTimeForm::showDialog(int show, int notifyClients)
{
	_state->data.showdialog = show;
	if (show) {
	    _parent->show();
	    _parent->open();
	    showPosition();
	    setTimezone(_state->data.tzlabel);
	}
	else
	    _parent->hide();

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
LiveTimeForm::vcrStep()
{
    int sts = 0;

    syncAcks(1);

    _pmtvadd(&_state->data.position, _state->data.delta, _state->data.vcrmode);
    showPosition(); // show the same position that the clients will be getting

    // now, finally, send the step command
    sts = __pmTimeBroadcast(PM_TCTL_STEP, &_state->data);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    char tzbuf[64];
    if (sts < 0)
	fprintf(stderr, "vcrStep: failed to send STEP command: %s\n", pmErrStr(sts));
    fprintf(stderr, "vcrStep pos=%.24s\n",
    pmCtime(&_state->data.position.tv_sec, tzbuf));
}
#endif
}

void
LiveTimeForm::vcrSkip()
{
    int sts = 0;

    syncAcks(1);

    showPosition();
    _pmtvadd(&_state->data.position, _state->data.delta, _state->data.vcrmode);

    sts = __pmTimeBroadcast(PM_TCTL_SKIP, &_state->data);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    char tzbuf[64];
    if (sts < 0)
	fprintf(stderr, "vcrSkip: failed to send SKIP command: %s\n", pmErrStr(sts));
    fprintf(stderr, "vcrSkip pos=%.24s\n",
    pmCtime(&_state->data.position.tv_sec, tzbuf));
}
#endif
}



void
LiveTimeForm::setVcrMode(vcrMode newMode, int updatePixmaps)
{
    struct timeval currentTime;

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    time_t tm;
    char tzbuf[64];
    time(&tm);
    fprintf(stderr, "LiveTimeForm::setVcrMode(_vcrMode==%s newMode=%s time=%.24s)\n",
    _vcrMode == VCR_LIVE ? "VCR_LIVE" : "VCR_STOP",
    newMode == VCR_LIVE ? "VCR_LIVE" : "VCR_STOP", pmCtime(&tm, tzbuf));
}
#endif

    if (updatePixmaps) {
	switch (_vcrMode) {
	case VCR_STOP:
	    VkSetHighlightingPixmap(_vcrStop, stop_off);
	    break;

	case VCR_LIVE:
	    VkSetHighlightingPixmap(_vcrForward, live_off);
	    break;
	}
    }

    switch (newMode) {
    case VCR_STOP:
	if (updatePixmaps)
	    VkSetHighlightingPixmap (_vcrStop, stop_on);
	_timer->stop();
	setIndicatorState(PM_TCTL_VCRMODE_STOP);
	break;

    case VCR_LIVE:
	{
	if (updatePixmaps)
	    VkSetHighlightingPixmap (_vcrForward, live_on);

	//
	// we are about to step forward by delta 
	// so set the initial position realtime - delta
	//
	gettimeofday(&currentTime, NULL);

	_pmtvTimeVal c_tval = _pmtvTimeVal(currentTime) -
			      _pmtvTimeVal(_state->data.delta, _state->data.vcrmode);
	c_tval.getTimeval(_state->data.position);

	__pmTimeBroadcast(PM_TCTL_SET, &_state->data);
	setIndicatorState(PM_TCTL_VCRMODE_PLAY);
	}
	break;

    default:
	break;
    }

    _vcrMode = newMode;

    if (_vcrMode == VCR_LIVE) {
	// start immediately
	// this will step forward to the current real-time
	_timer->start(0);
    }
}

/*ARGSUSED*/
void
LiveTimeForm::timerCallback(VkCallbackObject *obj, void *, void *calldata)
{
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

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    char tzbuf[64];
    time_t tm;
    time(&tm);
    fprintf(stderr, "LiveTimeForm::timerCallback(_vcrMode==%s time=%.24s)\n",
    _vcrMode == VCR_LIVE ? "VCR_LIVE" : "VCR_STOP", pmCtime(&tm, tzbuf));
}
#endif

    switch (_vcrMode) {
    case VCR_LIVE:
	{
	struct timeval after;
	_pmtvTimeVal thisDelta;
	_pmtvTimeVal statedelta(_state->data.delta, _state->data.vcrmode);
	gettimeofday(&after, NULL);

	// compute lag in milliseconds to match delta units
	_pmtvTimeVal lag = _pmtvTimeVal(after) - _pmtvTimeVal(_state->data.position);

	if (!(lag < _pmtvTimeVal())) { // double negative cause only a < operator is available
	    // only step if NOT ahead of time
	    vcrStep();
	    lag = lag - statedelta;
	}

	if (!(lag < statedelta)) {
	    // skip (to progressively catch up)
	    vcrSkip();
	    // next timer is immediate, so leave thisDelta as 0.
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "LiveTimeForm::timerCallback: skipped (because running slow by %fms)\n", lag.getValue(_PMTV_UNITS_MSEC));
#endif
	}
	else {
	    // reduce delta to catch up to real-time
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "LiveTimeForm::timerCallback running slow by %f ms, thisDelta=%f\n", lag.getValue(_PMTV_UNITS_MSEC), thisDelta.getValue(_PMTV_UNITS_MSEC));
#endif
	    thisDelta = statedelta - lag;
	}

	// schedule the next timeout "thisDelta" ms in the future
	// For bug #655103, force this to milliseconds on the basis that the value 
	// will always fit into 32 bits for live time.
	_timer->start(thisDelta.getXTB(PM_TIME_MSEC));
	}
	break;

    default:
	_timer->stop();
	break;
    }
}

