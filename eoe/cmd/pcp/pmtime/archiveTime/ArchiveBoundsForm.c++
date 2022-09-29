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


/////////////////////////////////////////////////////////////
//
// Source file for ArchiveBoundsForm
//
//    This class implements the user interface created in 
//    RapidApp. Normally it is not used directly.
//    Instead the subclass, ArchiveBoundsForm is instantiated
//
//    To extend or alter the behavior of this class, you should
//    modify the ArchiveBoundsForm files
//
//    Restrict changes to those sections between
//    the "//--- Start/End editable code block" markers
//
//    This will allow RapidApp to integrate changes more easily
//
//    This class is a ViewKit user interface "component".
//    For more information on how components are used, see the
//    "ViewKit Programmers' Manual", and the RapidApp
//    User's Guide.
//
//
/////////////////////////////////////////////////////////////


#include "ArchiveBoundsForm.h" // Generated header file for this class

#include <Xm/Form.h> 
#include <Xm/Label.h> 
#include <Xm/Scale.h> 
#include <Xm/Separator.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleB.h> 
#include <Vk/VkResource.h>

#include <Vk/VkErrorDialog.h>
#include <Vk/VkWarningDialog.h>

#include <errno.h>
#include <time.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>

#include "ArchiveBounds.h"
#include "pmapi.h"
#include "impl.h"
#include "tv.h"

const char *const ArchiveBoundsForm::boundsChanged = "boundsChanged";
const char *const ArchiveBoundsForm::cancelActivate = "cancelActivate";


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  ArchiveBoundsForm::_defaultArchiveBoundsFormResources[] = {
        "*finishLabel.labelString:  Finish",
        "*manualToggle.labelString:  Manual",
        "*startLabel.labelString:  Start",
        (char*)NULL
};

ArchiveBoundsForm::ArchiveBoundsForm ( const char *name ) : VkComponent ( name ) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

}    // End Constructor

ArchiveBoundsForm::ArchiveBoundsForm ( const char *name, Widget parent ) : VkComponent ( name ) 
{ 
    _dragStartScale = _dragFinishScale = _showMilliseconds = _manual = False;
    _startTextString[0] = _finishTextString[0] = NULL;
    _startWindowTimeval.tv_sec = _epochTimeval.tv_sec = 0;
    _startWindowTimeval.tv_usec = _epochTimeval.tv_usec = 0;
    _finishWindowTimeval.tv_sec = _finalTimeval.tv_sec = 0;
    _finishWindowTimeval.tv_usec = _finalTimeval.tv_usec = 0;

    // Call creation function to build the widget tree.

     create ( parent );
}

ArchiveBoundsForm::~ArchiveBoundsForm() 
{
}

void ArchiveBoundsForm::create ( Widget parent )
{
    // Load any class-defaulted resources for this object

    setDefaultResources ( parent, _defaultArchiveBoundsFormResources  );


    // Create an unmanaged widget as the top of the widget hierarchy

    _baseWidget = _archiveBoundsMainForm2 = XtVaCreateWidget ( _name, xmFormWidgetClass, parent, 
	XmNresizePolicy,	XmRESIZE_GROW, 
	XmNwidth,		300,
	XmNheight,		210,
	(XtPointer) NULL ); 

    // install a callback to guard against unexpected widget destruction

    installDestroyHandler();


    // Create widgets used in this component
    // All variables are data members of this class

    _cancelButton = XtVaCreateManagedWidget  ( "Cancel",
                                             xmPushButtonWidgetClass,
                                             _baseWidget, 
                                             XmNtopAttachment, XmATTACH_NONE, 
                                             XmNbottomAttachment, XmATTACH_FORM, 
                                             XmNleftAttachment, XmATTACH_NONE, 
                                             XmNrightAttachment, XmATTACH_FORM, 
                                             XmNbottomOffset, 5, 
                                             XmNrightOffset, 5, 
                                             XmNwidth, 85, 
                                             XmNheight, 35, 
                                             (XtPointer) NULL ); 
    XtAddCallback ( _cancelButton,
                    XmNactivateCallback,
                    &ArchiveBoundsForm::cancelActivateCBCallback,
                    (XtPointer) this ); 

    _separator = XtVaCreateManagedWidget  ( "separator",
                                             xmSeparatorWidgetClass,
                                             _baseWidget, 
                                             XmNtopAttachment, XmATTACH_WIDGET, 
                                             XmNbottomAttachment, XmATTACH_NONE, 
                                             XmNleftAttachment, XmATTACH_FORM, 
                                             XmNrightAttachment, XmATTACH_FORM, 
                                             XmNtopOffset, 5, 
                                             XmNleftOffset, 5, 
                                             XmNrightOffset, 5, 
                                             XmNwidth, 323, 
                                             XmNheight, 10, 
                                             (XtPointer) NULL ); 


    _finishScale = XtVaCreateManagedWidget  ( "finishScale",
                                               xmScaleWidgetClass,
                                               _baseWidget, 
#if defined(sgi)
                                               XmNslidingMode, XmSLIDER, 
                                               SgNslanted, False, 
#endif
                                               XmNvalue, 100, 
                                               XmNorientation, XmHORIZONTAL, 
                                               XmNshowValue, False, 
                                               XmNtopAttachment, XmATTACH_WIDGET, 
                                               XmNbottomAttachment, XmATTACH_NONE, 
                                               XmNleftAttachment, XmATTACH_FORM, 
                                               XmNrightAttachment, XmATTACH_FORM, 
                                               XmNtopOffset, 3, 
                                               XmNleftOffset, 5, 
                                               XmNrightOffset, 5, 
                                               XmNwidth, 323, 
                                               XmNheight, 15, 
                                               (XtPointer) NULL ); 

    XtAddCallback ( _finishScale,
                    XmNvalueChangedCallback,
                    &ArchiveBoundsForm::finishScaleChangedCBCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _finishScale,
                    XmNdragCallback,
                    &ArchiveBoundsForm::finishScaleDragCBCallback,
                    (XtPointer) this ); 


    _startScale = XtVaCreateManagedWidget  ( "startScale",
                                              xmScaleWidgetClass,
                                              _baseWidget, 
                                              XmNorientation, XmHORIZONTAL, 
                                              XmNshowValue, False, 
                                              XmNtopAttachment, XmATTACH_WIDGET, 
                                              XmNbottomAttachment, XmATTACH_NONE, 
                                              XmNleftAttachment, XmATTACH_FORM, 
                                              XmNrightAttachment, XmATTACH_FORM, 
                                              XmNtopOffset, 3, 
                                              XmNleftOffset, 5, 
                                              XmNrightOffset, 5, 
                                              XmNwidth, 323, 
                                              XmNheight, 15, 
                                              (XtPointer) NULL ); 

    XtAddCallback ( _startScale,
                    XmNvalueChangedCallback,
                    &ArchiveBoundsForm::startScaleChangedCBCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _startScale,
                    XmNdragCallback,
                    &ArchiveBoundsForm::startScaleDragCBCallback,
                    (XtPointer) this ); 


    _finishText = XtVaCreateManagedWidget  ( "finishText",
                                              xmTextFieldWidgetClass,
                                              _baseWidget, 
                                              XmNtopAttachment, XmATTACH_WIDGET, 
                                              XmNbottomAttachment, XmATTACH_NONE, 
                                              XmNleftAttachment, XmATTACH_FORM, 
                                              XmNrightAttachment, XmATTACH_FORM, 
                                              XmNtopWidget, _startScale, 
                                              XmNtopOffset, 13, 
                                              XmNleftOffset, 50, 
                                              XmNrightOffset, 5, 
                                              XmNwidth, 323, 
                                              XmNheight, 35, 
                                              (XtPointer) NULL ); 

    XtAddCallback ( _finishText,
                    XmNactivateCallback,
                    &ArchiveBoundsForm::finishTextActivateCBCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _finishText,
                    XmNvalueChangedCallback,
                    &ArchiveBoundsForm::finishTextChangedCBCallback,
                    (XtPointer) this ); 


    _finishLabel = XtVaCreateManagedWidget  ( "finishLabel",
                                               xmLabelWidgetClass,
                                               _baseWidget, 
                                               XmNlabelType, XmSTRING, 
                                               XmNtopAttachment, XmATTACH_WIDGET, 
                                               XmNbottomAttachment, XmATTACH_NONE, 
                                               XmNleftAttachment, XmATTACH_FORM, 
                                               XmNrightAttachment, XmATTACH_NONE, 
                                               XmNtopWidget, _startScale, 
                                               XmNtopOffset, 13, 
                                               XmNleftOffset, 5, 
                                               XmNwidth, 47, 
                                               XmNheight, 35, 
                                               (XtPointer) NULL ); 


    _startText = XtVaCreateManagedWidget  ( "startText",
                                             xmTextFieldWidgetClass,
                                             _baseWidget, 
                                             XmNtopAttachment, XmATTACH_WIDGET, 
                                             XmNbottomAttachment, XmATTACH_NONE, 
                                             XmNleftAttachment, XmATTACH_FORM, 
                                             XmNrightAttachment, XmATTACH_FORM, 
                                             XmNtopOffset, 10, 
                                             XmNleftOffset, 50, 
                                             XmNrightOffset, 5, 
                                             XmNwidth, 323, 
                                             XmNheight, 35, 
                                             (XtPointer) NULL ); 

    XtAddCallback ( _startText,
                    XmNactivateCallback,
                    &ArchiveBoundsForm::startTextActivateCBCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _startText,
                    XmNvalueChangedCallback,
                    &ArchiveBoundsForm::startTextChangedCBCallback,
                    (XtPointer) this ); 


    _startLabel = XtVaCreateManagedWidget  ( "startLabel",
                                              xmLabelWidgetClass,
                                              _baseWidget, 
                                              XmNalignment, XmALIGNMENT_BEGINNING, 
                                              XmNlabelType, XmSTRING, 
                                              XmNtopAttachment, XmATTACH_WIDGET, 
                                              XmNbottomAttachment, XmATTACH_NONE, 
                                              XmNleftAttachment, XmATTACH_FORM, 
                                              XmNrightAttachment, XmATTACH_NONE, 
                                              XmNtopOffset, 5, 
                                              XmNleftOffset, 5, 
                                              XmNwidth, 70, 
                                              XmNheight, 40, 
                                              (XtPointer) NULL ); 


    _manualToggle = XtVaCreateManagedWidget  ( "manualToggle",
                                                xmToggleButtonWidgetClass,
                                                _baseWidget, 
                                                XmNalignment, XmALIGNMENT_BEGINNING, 
                                                XmNlabelType, XmSTRING, 
                                                XmNindicatorOn, True, 
                                                XmNtopAttachment, XmATTACH_FORM, 
                                                XmNbottomAttachment, XmATTACH_NONE, 
                                                XmNleftAttachment, XmATTACH_FORM, 
                                                XmNrightAttachment, XmATTACH_NONE, 
                                                XmNtopOffset, 5, 
                                                XmNleftOffset, 5, 
                                                XmNwidth, 90, 
                                                XmNheight, 20, 
                                                (XtPointer) NULL ); 

    XtAddCallback ( _manualToggle,
                    XmNvalueChangedCallback,
                    &ArchiveBoundsForm::manualToggleChangedCBCallback,
                    (XtPointer) this ); 


    XtVaSetValues ( _separator, 
                    XmNtopWidget, _finishScale, 
                    (XtPointer) NULL );
    XtVaSetValues ( _finishScale, 
                    XmNtopWidget, _finishText, 
                    (XtPointer) NULL );
    XtVaSetValues ( _startScale, 
                    XmNtopWidget, _startText, 
                    (XtPointer) NULL );
    XtVaSetValues ( _startText, 
                    XmNtopWidget, _manualToggle, 
                    (XtPointer) NULL );
    XtVaSetValues ( _startLabel, 
                    XmNtopWidget, _manualToggle, 
                    (XtPointer) NULL );
}

const char * ArchiveBoundsForm::className()
{
    return ("ArchiveBoundsForm");
}


/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void ArchiveBoundsForm::finishScaleChangedCBCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->finishScaleChangedCB ( w, callData );
}

void ArchiveBoundsForm::finishScaleDragCBCallback ( Widget    w,
                                                    XtPointer clientData,
                                                    XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->finishScaleDragCB ( w, callData );
}

void ArchiveBoundsForm::finishTextActivateCBCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->finishTextActivateCB ( w, callData );
}

void ArchiveBoundsForm::finishTextChangedCBCallback ( Widget    w,
                                                      XtPointer clientData,
                                                      XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->finishTextChangedCB ( w, callData );
}

void ArchiveBoundsForm::manualToggleChangedCBCallback ( Widget    w,
                                                        XtPointer clientData,
                                                        XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->manualToggleChangedCB ( w, callData );
}

void ArchiveBoundsForm::startScaleChangedCBCallback ( Widget    w,
                                                      XtPointer clientData,
                                                      XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->startScaleChangedCB ( w, callData );
}

void ArchiveBoundsForm::startScaleDragCBCallback ( Widget    w,
                                                   XtPointer clientData,
                                                   XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->startScaleDragCB ( w, callData );
}

void ArchiveBoundsForm::startTextActivateCBCallback ( Widget    w,
                                                      XtPointer clientData,
                                                      XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->startTextActivateCB ( w, callData );
}

void ArchiveBoundsForm::startTextChangedCBCallback ( Widget    w,
                                                     XtPointer clientData,
                                                     XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->startTextChangedCB ( w, callData );
}

void ArchiveBoundsForm::cancelActivateCBCallback ( Widget    w,
                                                     XtPointer clientData,
                                                     XtPointer callData ) 
{ 
    ArchiveBoundsForm* obj = ( ArchiveBoundsForm * ) clientData;
    obj->cancelActivateCB( w, callData );
}



/*ARGSUSED*/
void
ArchiveBoundsForm::finishScaleChangedCB ( Widget w, XtPointer callData )
{
    _dragFinishScale = False;
}    // End ArchiveBoundsForm::finishScaleChangedCB()


/*ARGSUSED*/
void
ArchiveBoundsForm::finishScaleDragCB ( Widget w, XtPointer callData )
{
    int s, f;

    _dragFinishScale = True;
    XmScaleGetValue(_startScale, &s);
    XmScaleGetValue(_finishScale, &f);
    if (s >= f)
	s = f - 1;
    if (s < 0)
	s = 0;
    setBounds(s, f);
}

/*ARGSUSED*/
void
ArchiveBoundsForm::finishTextActivateCB ( Widget w, XtPointer callData )
{
    struct tm tmval;
    char *msg;
    struct timeval finish;
    struct timeval origin = {0,0};
    char *s;

    if ((s = XmTextGetString(w)) == NULL) {
	fprintf(stderr, "%s: XmTextGetString failed: %s\n", pmProgname,
		strerror(errno));
	exit(1);
    }
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::finishTextActivateCB: \"%s\"\n", s);
#endif
    
    if (__pmParseCtime(s, &tmval, &msg) < 0) {
	theErrorDialog->postAndWait("Invalid finishing time");
	free(msg);
	XtFree(s);
	showBounds();
	return;
    }
    XtFree(s);

    if (__pmConvertTime(&tmval, &origin, &finish) < 0) {
	theErrorDialog->postAndWait("Invalid finishing time");
	showBounds();
	return;
    }

    setBounds(&_startWindowTimeval, &finish);
}

/*ARGSUSED*/
void
ArchiveBoundsForm::finishTextChangedCB ( Widget w, XtPointer callData )
{
}

/*ARGSUSED*/
void
ArchiveBoundsForm::manualToggleChangedCB ( Widget w, XtPointer callData )
{
    setManual(XmToggleButtonGetState(w));
}

/*ARGSUSED*/
void
ArchiveBoundsForm::startScaleChangedCB ( Widget w, XtPointer callData )
{
    _dragStartScale = False;
}

/*ARGSUSED*/
void
ArchiveBoundsForm::startScaleDragCB ( Widget w, XtPointer callData )
{
    int s, f;

    _dragStartScale = True;
    XmScaleGetValue(_startScale, &s);
    XmScaleGetValue(_finishScale, &f);
    if (s >= f)
	f = s + 1;
    if (f > 100)
	f = 100;
    setBounds(s, f);

}

/*ARGSUSED*/
void
ArchiveBoundsForm::startTextActivateCB ( Widget w, XtPointer callData )
{
    struct tm tmval;
    char *msg;
    struct timeval start;
    struct timeval origin = {0,0};
    char *s;

    if ((s = XmTextGetString(w)) == NULL) {
	fprintf(stderr, "%s: XmTextGetString failed: %s\n", pmProgname,
		strerror(errno));
	exit(1);
    }
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::startTextActivateCB: \"%s\"\n", s);
#endif
    
    if (__pmParseCtime(s, &tmval, &msg) < 0) {
	theErrorDialog->postAndWait("Invalid starting time");
	XtFree(s);
	free(msg);
	showBounds();
	return;
    }
    XtFree(s);

    if (__pmConvertTime(&tmval, &origin, &start) < 0) {
	theErrorDialog->postAndWait("Invalid starting time");
	showBounds();
	return;
    }

    setBounds(&start, &_finishWindowTimeval);
}

/*ARGSUSED*/
void ArchiveBoundsForm::startTextChangedCB ( Widget w, XtPointer callData )
{
}

static char
*_timeStr(struct timeval *t, Boolean ms, Boolean year)
{
    char buf[64];
    char sbuf[16];
    static char retbuf[64];

    // 012345678901234567890123456789
    // Fri Sep 15 14:43:28 1995
    //
    retbuf[0] = NULL;
    if (pmCtime(&t->tv_sec, buf) != NULL) {
	strcpy(retbuf, buf);
	retbuf[19] = NULL;
	if (ms == True) {
	    sprintf(sbuf, ".%03d", t->tv_usec / 1000);
	    strcat(retbuf, sbuf);
	}
	if (year == True) {
	    strcpy(sbuf, buf + 19);
	    sbuf[5] = NULL;
	    strcat(retbuf, sbuf);
	}
	strcat(retbuf, "         ");
    }

    return retbuf;
}

void
ArchiveBoundsForm::showBounds(void)
{
    char *p;
    int start_percent;
    int finish_percent;

    if (_manual == True) {
	double earliest = _pmtv2double(&_epochTimeval);
	double latest = _pmtv2double(&_finalTimeval);
	double start = _pmtv2double(&_startWindowTimeval);
	double finish = _pmtv2double(&_finishWindowTimeval);
	start_percent = 0.5 + 100.0 * (start - earliest) / (latest - earliest);
	finish_percent = 0.5 + 100.0 * (finish - earliest) / (latest - earliest);
	if (start_percent < 0)
	    start_percent = 0;
	if (finish_percent > 100.0)
	    finish_percent = 100.0;
    }
    else {
	// auto
	_startWindowTimeval = _epochTimeval;
	_finishWindowTimeval = _finalTimeval;
	start_percent = 0;
	finish_percent = 100;
    }

    p = _timeStr(&_startWindowTimeval, _showMilliseconds, _showYear);
    if (strcmp(p, _startTextString) != 0) {
	strcpy(_startTextString, p);
	XmTextSetString(_startText, _startTextString);
	XmScaleSetValue(_startScale, start_percent);
    }

    p = _timeStr(&_finishWindowTimeval, _showMilliseconds, _showYear);
    if (strcmp(p, _finishTextString) != 0) {
	strcpy(_finishTextString, p);
	XmTextSetString(_finishText, _finishTextString);
	XmScaleSetValue(_finishScale, finish_percent);
    }
}

void
ArchiveBoundsForm::setBounds(int startPercent, int finishPercent)
{
    double etd = _pmtv2double(&_epochTimeval);
    double ltd = _pmtv2double(&_finalTimeval);

    _pmdouble2tv(etd + startPercent * (ltd - etd) / 100.0, &_startWindowTimeval);
    _pmdouble2tv(etd + finishPercent * (ltd - etd) / 100.0, &_finishWindowTimeval);
    showBounds();
    notifyBoundsChanged(&_startWindowTimeval, &_finishWindowTimeval);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::setBounds: start=%d finish=%d\n", startPercent, finishPercent);
#endif
}

void
ArchiveBoundsForm::setBounds(struct timeval *st, struct timeval *ft)
{
    double std = _pmtv2double(st);
    double ftd = _pmtv2double(ft);
    double etd = _pmtv2double(&_epochTimeval);
    double ltd = _pmtv2double(&_finalTimeval);

    _startWindowTimeval = *st;
    _finishWindowTimeval = *ft;
    showBounds();
    notifyBoundsChanged(&_startWindowTimeval, &_finishWindowTimeval);

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::setBounds(manual=%d st=%.2lf, ft=%.2lf et=%.2lf lt=%.2lf)\n",
	_manual, std, ftd, etd, ltd);
#endif
}

void
ArchiveBoundsForm::addBounds(struct timeval *st, struct timeval *ft, Boolean force)
{
    double std = _pmtv2double(st);
    double ftd = _pmtv2double(ft);
    double etd = _pmtv2double(&_epochTimeval);
    double ltd = _pmtv2double(&_finalTimeval);

    if (force == True || std < etd)
	_epochTimeval = *st;
    if (force == True || ftd > ltd)
	_finalTimeval = *ft;
    if (_manual == False)
	setBounds(st, ft);
    else
	showBounds();

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::addBounds(manual=%d force=%d st=%.2lf, ft=%.2lf et=%.2lf lt=%.2lf)\n",
	_manual, force, std, ftd, etd, ltd);
#endif
}

void
ArchiveBoundsForm::setManual(Boolean manual)
{
    static Pixel        readonlybg = NULL;
    static Pixel        readonlyfg = NULL;
    static Pixel        readwrite = NULL;

    if (readonlybg == (Pixel)NULL) {
	readonlybg = (Pixel)VkGetResource(_startText, "readOnlyBackground", "ReadOnlyBackground", XmRPixel, "Gray72");
	readonlyfg = (Pixel)VkGetResource(_startText, "readOnlyForeground", "ReadOnlyForeground", XmRPixel, "Black");
    }

    if (readwrite == (Pixel)NULL) {
	readwrite = (Pixel)VkGetResource(_startText, "textFieldBackground", "TextFieldBackground", XmRPixel, "#b98e8e");
    }

    _manual = manual;

    XtVaSetValues(_startText,
	XmNeditable,			_manual,
	XmNtraversalOn,			_manual,
	XmNcursorPositionVisible,	_manual,
	XmNbackground, 			(_manual == True) ? readwrite : readonlybg,
	XmNforeground, 			readonlyfg,
	NULL);

    XtVaSetValues(_finishText,
	XmNeditable,			_manual,
	XmNtraversalOn,			_manual,
	XmNcursorPositionVisible,	_manual,
	XmNbackground, 			(_manual == True) ? readwrite : readonlybg,
	XmNforeground, 			readonlyfg,
	NULL);

    XtSetSensitive(_startScale,		_manual);
    XtSetSensitive(_finishScale,	_manual);
    showBounds();
}

Boolean ArchiveBoundsForm::isManual(void)
{
    return _manual;
}

void
ArchiveBoundsForm::getBounds(struct timeval *s, struct timeval *f)
{
    *s = _startWindowTimeval;
    *f = _finishWindowTimeval;
}

void
ArchiveBoundsForm::getBounds(double *s, double *f)
{
    *s = _pmtv2double(&_startWindowTimeval);
    *f = _pmtv2double(&_finishWindowTimeval);
}

void
ArchiveBoundsForm::showDetail(Boolean ms, Boolean year)
{
    _showMilliseconds = ms;
    _showYear = year;
    showBounds();
}

void
ArchiveBoundsForm::notifyBoundsChanged(struct timeval *s, struct timeval *f)
{
    static double clientData[2];
    clientData[0] = _pmtv2double(s);
    clientData[1] = _pmtv2double(f);
    callCallbacks(boundsChanged, (void *) &clientData);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::notifyBoundsChanged: %.2lf %.2lf\n", clientData[0], clientData[1]);
#endif
}

/*ARGSUSED*/
void
ArchiveBoundsForm::cancelActivateCB( Widget w, XtPointer callData )
{
    callCallbacks(cancelActivate, (void *)callData);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    fprintf(stderr, "ArchiveBoundsForm::cancelActivateCB\n");
#endif
}
