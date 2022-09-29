/****************************************************************************/
/*                              xdCallbacks.c                               */
/****************************************************************************/

/*
 *  All the callback routines for xdiff the graghical file differ.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Copyright (c) 1994 Rudy Wortel. All rights reserved.
 */

/*
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdCallbacks.c,v $
 *  $Revision: 1.3 $
 *  $Author: milt $
 *  $Date: 1995/03/24 02:33:58 $
 */

#include <stdio.h>
#include <unistd.h>
#include <xdDiff.h>
#include <xdGUI.h>
#include <xdProto.h>
#include <X11/cursorfont.h>

/****************************************************************************/

void helpOnContextCallback (

/*
 *  Call back function for contextual help. Prompt the user to click
 *  in the widget that they need help for.  Display a dialog box for
 *  the information about the widget.
 */

Widget button,                  /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

XmPushButtonCallbackStruct *cbs;
ApplicationData *appData;
Widget widget;
Cursor cursor;

    appData = (ApplicationData *) clientData;
    cbs = (XmPushButtonCallbackStruct *) callData;

    /* prompt the user to click in a widget. */
    xdSetMessageLine( appData, "msgClickWidget" );

    /* change the cursor to a question mark and wait for a click */
    cursor = XCreateFontCursor( appData->display, XC_question_arrow );
    widget = XmTrackingLocate( appData->mainWindow, cursor, False );
    xdSetMessageLine( appData, NULL );

     if( widget != NULL ){
        /* invoke the help callback for the widget. */
        cbs->reason = XmCR_HELP;
        XtCallCallbacks( widget, XmNhelpCallback, cbs );
    }

    XFreeCursor( appData->display, cursor );
}

/****************************************************************************/

void aboutXdiffCallback (

/*
 *  Call back function for to display the about message.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

    helpCallback( w, "aboutXdiff", callData );
}

/****************************************************************************/

void exitCallback (

/*
 *  Call back function for the exit button.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    XtDestroyApplicationContext( appData->appContext );

    /* Time for you to leave. */
    exit( 0 );
}

/****************************************************************************/

void saveAsCallback (

/*
 *  Callback function for the Save As button. Pop up the file
 *  selection box which we have already created.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    if( xdCheckAllSelected( appData ) ){
        XtPopup( appData->selectorShell, XtGrabNone );
    }
}

/****************************************************************************/

void confirmCallback (

/*
 *  User has selected one of the buttons in the Message Box. So go ahead and
 *  write to the file if the OK button was selected. The file name is
 *  stored in the user data resource of the confirm box.
 */

Widget confirmBox,              /*< Confirmation Message Box.               */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

XmAnyCallbackStruct *cbs;       /* Callback information.                    */
ApplicationData *appData;       /* All application data.                    */
char *fileName;                 /* Write to this file.                      */

    appData = (ApplicationData *) clientData;
    cbs = (XmAnyCallbackStruct *) callData;

    XtVaGetValues( confirmBox,
            XmNuserData,        &fileName,
            0 );

    switch( cbs->reason ){
    case XmCR_OK:
        if( strcmp( fileName, appData->diffs.rfile ) == 0 ){
            xdSaveFile( appData, fileName, XD_RIGHTTEXT );
        }else{
            xdSaveFile( appData, fileName, XD_LEFTTEXT );
        }
        break;
    case XmCR_CANCEL:
        xdSetMessageLine( appData, NULL );
        break;
    }

    XtPopdown( appData->confirmShell );
}

/****************************************************************************/

void xdConfirmSaveFile (

/*
 *  Put up a notifer to ask for confirmation of writing to a file. Save the
 *  filename in the userData of the message Box.
 */

ApplicationData *appData,       /*< All application Data.                   */
char *fileName                  /*< File name for the store.                */
)
{ /**************************************************************************/

char *msgOk;                    /* Confirmation prompt.                     */
XmString label;                 /* Components of the prompt string.         */
XmString name;
XmString message;
char *confirmName;              /* A copy of the file name.                 */

    msgOk = xdGetDefault( appData->display, appData->appClass, "msgOkSave" );

    XtVaGetValues( appData->messageBox,
            XmNuserData,        &confirmName,
            0 );

    /* Free any old name. */
    if( confirmName != NULL ){
        XtFree( confirmName );
    }

    confirmName = (fileName == NULL) ? NULL : XtNewString( fileName );

    /* build up a prompt message. */
    label = XmStringSegmentCreate( msgOk,
            XmSTRING_DEFAULT_CHARSET, XmSTRING_DIRECTION_L_TO_R, True );
    name = XmStringSegmentCreate( (fileName == NULL) ? "<stdout>" : fileName,
            XmSTRING_DEFAULT_CHARSET, XmSTRING_DIRECTION_L_TO_R, False );
    message = XmStringConcat( label, name );

    XtVaSetValues( appData->messageBox,
            XmNmessageString,   message,
            XmNuserData,        (XtPointer) confirmName,
            0 );

    XtManageChild( appData->confirmShell );

    XmStringFree( message );
    XmStringFree( label );
    XmStringFree( name );

    /* Display the message box. Wait for comfirmation. */
    XtPopup( appData->confirmShell, XtGrabNonexclusive );
}

/****************************************************************************/

void saveAsRightCallback (

/*
 *  Callback function for the 'Save As Right' button.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    if( xdCheckAllSelected( appData ) ){
        if( appData->confirmWrite ){
            xdConfirmSaveFile( appData, appData->diffs.rfile );
        }else{
            xdSaveFile( appData, appData->diffs.rfile, XD_RIGHTTEXT );
        }
    }
}

/****************************************************************************/

void saveAsLeftCallback (

/*
 *  Callback function for the Save As Left button.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    if( xdCheckAllSelected( appData ) ){
        if( appData->confirmWrite ){
            xdConfirmSaveFile( appData, appData->diffs.lfile );
        }else{
            xdSaveFile( appData, appData->diffs.lfile, XD_LEFTTEXT );
        }
    }
}

/****************************************************************************/

void selectGlobalLeftCallback (

/*
 *  Callback function to select all the differences on the left.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdSelectGlobal( appData, XD_SELECTLEFT );
}

/****************************************************************************/

void selectGlobalRightCallback (

/*
 *  Callback function to select all the differences on the right.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdSelectGlobal( appData, XD_SELECTRIGHT );
}

/****************************************************************************/

void unselectGlobalCallback (

/*
 *  Callback function to unselect all the differences.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdSelectGlobal( appData, XD_SELECTNONE );
}

/****************************************************************************/

void selectGlobalNeitherCallback (

/*
 *  Callback function to select none of the differences.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdSelectGlobal( appData, XD_SELECTNEITHER );
}

/****************************************************************************/

void tabsAt4Callback (

/*
 *  Callback function to set expansion of tabs to 4 spaces.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;
    if( appData->tabWidth != 4 ){
        appData->tabWidth = 4;
        xdDrawText( appData, XD_LEFT );
        xdDrawText( appData, XD_RIGHT );
    }
}

/****************************************************************************/

void tabsAt8Callback (

/*
 *  Callback function to set expansion of tabs to 8 spaces.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;
    if( appData->tabWidth != 8 ){
        appData->tabWidth = 8;
        xdDrawText( appData, XD_LEFT );
        xdDrawText( appData, XD_RIGHT );
    }
}

/****************************************************************************/

void postDrawingAreaMenu (

/*
 *  Post the popup menu for the drawing area.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XEvent *event,                  /*< The event that trigger this call.       */
Boolean *dispatch               /*> Continue to dispatch.                   */
)
{ /**************************************************************************/

XButtonEvent *buttonEvent;      /* Where did the button press happen.       */
Widget popup;                   /* PopUp menu for this window.              */

    /* the popup is passed as the client data. */
    popup = (Widget) clientData;

    buttonEvent = (XButtonEvent *) event;
    if( buttonEvent->button == Button3 ){
        /* place the menu so that the cursor is in the first button. */
        buttonEvent->x_root -= 10;
        buttonEvent->y_root -= 10;
        XmMenuPosition( popup, buttonEvent );

        /* peek a boo */
        XtManageChild( popup );
    }
}

/****************************************************************************/

void rightWindowExposeCallback (

/*
 *  Draw the text in the right window.
 */

Widget drawingArea,                 /*< Where to do the drawing.            */
XtPointer data,                      /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

    xdDrawText( data, XD_RIGHT );
}

/****************************************************************************/

void leftWindowExposeCallback (

/*
 *  Draw the text in the left window.
 */

Widget drawingArea,                 /*< Where to do the drawing.            */
XtPointer data,                     /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

    xdDrawText( data, XD_LEFT );
}

/****************************************************************************/

void viewAreaExposeCallback (

/*
 *  Draw the overview on the right side.
 */

Widget viewArea,                    /*< Where to do the drawing.            */
XtPointer data,                     /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

    xdDrawView( data );
}

/****************************************************************************/

void viewAreaInputCallback (

/*
 *  Process input in the View Area.
 */

Widget viewArea,                    /*< Where to do the drawing.            */
XtPointer clientData,               /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

XmDrawingAreaCallbackStruct *cbs;
ApplicationData *appData;
XDlist *diffs;
XButtonEvent *buttonEvent;
Dimension windowHeight;
Dimension windowWidth;
Dimension offset;
double lines2screen;
int maxLines;
int newLine;

    appData = (ApplicationData *) clientData;
    cbs = (XmDrawingAreaCallbackStruct *) callData;

    /* determine the mapping from window space to line number. */
    diffs = &appData->diffs;
    maxLines = (diffs->rlines > diffs->llines) ? diffs->rlines : diffs->llines;
    if( maxLines == 0 ){
        return;
    }

    XtVaGetValues( appData->viewArea,
            XmNheight,          &windowHeight,
            XmNwidth,           &windowWidth,
            0 );

    lines2screen = (double) windowHeight / (double) maxLines;

    switch( cbs->event->type ){
    case ButtonPress:
        buttonEvent = &cbs->event->xbutton;
        if( buttonEvent->x > (windowWidth / 2) ){
            /* position with respect to the right file. */
            offset = (maxLines - diffs->rlines) * lines2screen / 2.0;
            newLine = (buttonEvent->y - offset) / lines2screen + 1;
            xdFindLine( appData, newLine, XD_RIGHT );
        }else{
            /* position with respect to the left file. */
            offset = (maxLines - diffs->llines) * lines2screen / 2.0;
            newLine = (buttonEvent->y - offset) / lines2screen + 1;
            xdFindLine( appData, newLine, XD_LEFT );
        }
        break;
    }
}

/****************************************************************************/

void leftWindowInputCallback (

/*
 *  Process input in the left text area.
 */

Widget drawingArea,                 /*< Where to do the drawing.            */
XtPointer clientData,               /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

XmDrawingAreaCallbackStruct *cbs;
ApplicationData *appData;
XButtonEvent *buttonEvent;

    cbs = (XmDrawingAreaCallbackStruct *) callData;
    appData = (ApplicationData *) clientData;

    switch( cbs->event->type ){
    case ButtonPress:
        buttonEvent = &cbs->event->xbutton;
        switch( buttonEvent->button ){
        case Button1:
            /* make a selection on the current region. */
            if( xdSetCurrentDifference( appData, buttonEvent->y ) != -1 ){
                if( buttonEvent->state & ShiftMask ){
                    /* Shifted it means remove the selection. */
                     xdSelectRegion( appData, XD_SELECTNONE );
                }else{
                    xdSelectRegion( appData, XD_SELECTLEFT );
                }
            }
            break;
        case Button2:
            /* make a selection on the current line. */
            if( xdSetCurrentDifference( appData, buttonEvent->y ) != -1 ){
                if( buttonEvent->state & ShiftMask ){
                    xdSelectLine( appData, XD_SELECTNONE );
                }else{
                    xdSelectLine( appData, XD_SELECTLEFT );
                }
            }
        }
        break;

    case KeyRelease:
    case KeyPress:
    case MotionNotify:
    case ButtonRelease:
    default:
        break;
    }
}

/****************************************************************************/

void rightWindowInputCallback (

/*
 *  Process input in the right text area.
 */

Widget drawingArea,                 /*< Where to do the drawing.            */
XtPointer clientData,               /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

XmDrawingAreaCallbackStruct *cbs;
ApplicationData *appData;
XButtonEvent *buttonEvent;

    cbs = (XmDrawingAreaCallbackStruct *) callData;
    appData = (ApplicationData *) clientData;

    switch( cbs->event->type ){
    case ButtonPress:
        buttonEvent = &cbs->event->xbutton;
        switch( buttonEvent->button ){
        case Button1:
            /* make a selection on the current region. */
            if( xdSetCurrentDifference( appData, buttonEvent->y ) != -1 ){
                if( buttonEvent->state & ShiftMask ){
                    /* Shifted it means remove the selection. */
                    xdSelectRegion( appData, XD_SELECTNONE );
                }else{
                    xdSelectRegion( appData, XD_SELECTRIGHT );
                }
            }
            break;

        case Button2:
            /* make a selection on the current line. */
            if( xdSetCurrentDifference( appData, buttonEvent->y ) != -1 ){
                if( buttonEvent->state & ShiftMask ){
                    xdSelectLine( appData, XD_SELECTNONE );
                }else{
                    xdSelectLine( appData, XD_SELECTRIGHT );
                }
            }
            break;
         }
        break;

    default:
        break;
    }
}

/****************************************************************************/

void scrollVerticalCallback (

/*
 *  Process Callback events from the vertical slider. Redraw the text
 *  to reflect the position of sliders.
 */

Widget scrollBar,                   /*< Pointer the the scroll bar Widget.  */
XtPointer data,                     /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

    if( xdSetCursorOnScreen( data ) ){
        xdDrawView( data );
    }
    xdDrawText( data, XD_LEFT );
    xdDrawText( data, XD_RIGHT );

    xdSetMessageLine( data, NULL );
}

/****************************************************************************/

void scrollLeftCallback (

/*
 *  Process Callback events from the left slider. Position the text in
 *  the left window.
 */

Widget scrollBar,                   /*< Pointer the the scroll bar Widget.  */
XtPointer data,                     /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

    xdDrawText( data, XD_LEFT );
}

/****************************************************************************/

void scrollRightCallback (

/*
 *  Process Callback events from the right slider. Position the text
 *  in the right window.
 */

Widget scrollBar,                   /*< Pointer the the scroll bar Widget.  */
XtPointer data,                     /*< All application Data.               */
XtPointer callData                  /*< Data from Widget Class.             */
)
{ /**************************************************************************/

    xdDrawText( data, XD_RIGHT );
}

/****************************************************************************/

void resizeEventHandler (

/*
 *  Called when the mainForm's structure changes. We need to update the
 *  sliders to reflect this change.
 */

Widget widget,                  /*< Widget receiving the event.             */
XtPointer clientData,           /*< All application data.                   */
XEvent *event,                  /*< The event that trigger this call.       */
Boolean *dispatch               /*> Continue dispatch events.               */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdResizeSliders( appData );
    xdSetCursorOnScreen( appData );

    xdDrawText( appData, XD_LEFT );
    xdDrawText( appData, XD_RIGHT );
    xdDrawView( appData );
}

/****************************************************************************/

void listerOkCallback (

/*
 *  User has selected the OK button in the File Selection Box.
 */

Widget lister,                  /*< File Selection box                      */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

XmFileSelectionBoxCallbackStruct *cbs;  /*< Callback information.           */
ApplicationData *appData;           /*< All application Data.               */
char *fileName;

    cbs = (XmFileSelectionBoxCallbackStruct *) callData;
    appData = (ApplicationData *) clientData;

    XmStringGetLtoR( cbs->value, XmSTRING_DEFAULT_CHARSET, &fileName );

    if( *fileName != '\0' ){
        if( appData->confirmWrite ){
            XtPopdown( appData->selectorShell );
            xdConfirmSaveFile( appData, fileName );
        }else{
            if( xdSaveFile( appData, fileName, XD_LEFTTEXT ) > 0 ){
                XtPopdown( appData->selectorShell );
            }
        }
    }
}

/****************************************************************************/

void listerCancelCallback (

/*
 *  User has selected the Cancel button in the File Selection Box.
 */

Widget lister,                  /*< File Selection box                      */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

XmFileSelectionBoxCallbackStruct *cbs;  /*< Callback information.           */
ApplicationData *appData;           /*< All application Data.               */

    cbs = (XmFileSelectionBoxCallbackStruct *) callData;
    appData = (ApplicationData *) clientData;

    XtPopdown( appData->selectorShell );

    xdSetMessageLine( appData, NULL );
}

/****************************************************************************/

void nextDifferenceCallback (

/*
 *  Call back function to move to the next difference.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdFindDifference( appData, False, XD_NEXT );
}

/****************************************************************************/

void previousDifferenceCallback (

/*
 *  Call back function to move to the previous difference.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdFindDifference( appData, False, XD_PREV );
}

/****************************************************************************/

void nextUnselectedCallback (

/*
 *  Call back function to find the next unselected difference.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdFindDifference( appData, True, XD_NEXT );
}

/****************************************************************************/

void previousUnselectedCallback (

/*
 *  Call back function to find the previous unselected difference.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdFindDifference( appData, True, XD_PREV );
}

/****************************************************************************/

void selectRegionRightCallback (

/*
 *  Select the region from the right file.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectRegion( appData, XD_SELECTRIGHT );
}

/****************************************************************************/

void selectRegionLeftCallback (

/*
 *  Select the region from the left file.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectRegion( appData, XD_SELECTLEFT );
}

/****************************************************************************/

void selectRegionNeitherCallback (

/*
 *  Neither region is selected.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectRegion( appData, XD_SELECTNEITHER );
}

/****************************************************************************/

void unselectRegionCallback (

/*
 *  Remove the selection for this region.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectRegion( appData, XD_SELECTNONE );
}

/****************************************************************************/

void splitRightCallback (

/*
 *  Split a Changed text region into two Only regions. The first
 *   region listed will be from the right file.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;
    if( appData->diffs.current->type == XD_CHANGEDTEXT ){
        xdSplitDifferences( &appData->diffs, XD_RIGHTTEXT );
        xdResizeSliders( appData );
        xdMoveToCurrentLine( appData );
    }else{
        xdSetMessageLine( appData, "msgSplit" );
    }
}

/****************************************************************************/

void splitLeftCallback (

/*
 *  Split a Changed text region into two Only regions. The first
 *  region listed will be from the left file.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;
    if( appData->diffs.current->type == XD_CHANGEDTEXT ){
        xdSplitDifferences( &appData->diffs, XD_LEFTTEXT );
        xdResizeSliders( appData );
        xdMoveToCurrentLine( appData );
    }else{
        xdSetMessageLine( appData, "msgSplit" );
    }
}

/****************************************************************************/

void lineNumbersCallback (

/*
 *  Toggle the display of line numbers.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;
    appData->lineNumbers = !appData->lineNumbers;

    xdDrawText( appData, XD_LEFT );
    xdDrawText( appData, XD_RIGHT );
}

/****************************************************************************/

void selectLineRightCallback (

/*
 *  Select the line from the right file.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectLine( appData, XD_SELECTRIGHT );
}

/****************************************************************************/

void selectLineLeftCallback (

/*
 *  Select the line from the left file.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectLine( appData, XD_SELECTLEFT );
}

/****************************************************************************/

void selectLineNeitherCallback (

/*
 *  Neither line is selected.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectLine( appData, XD_SELECTNEITHER );
}

/****************************************************************************/

void unselectLineCallback (

/*
 *  Remove the selection for this line.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    xdSelectLine( appData, XD_SELECTNONE );
}

/****************************************************************************/

void cursorDownCallback (

/*
 *  Move the cursor to the next line.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;
    xdMoveCursor( appData, 1 );
}

/****************************************************************************/

void cursorUpCallback (

/*
 *  Move the cursor to the previous line.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;
    xdMoveCursor( appData, -1 );
}

/****************************************************************************/

void scrollDownCallback (

/*
 *  The text is scrolled down two lines less than one screen.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */
int size;

    appData = (ApplicationData *) clientData;
    XtVaGetValues( appData->vertScroll,
            XmNsliderSize,      &size,
            0 );
    xdMoveCursor( appData, (size < 1) ? 1 : size - 2);
}

/****************************************************************************/

void scrollUpCallback (

/*
 *  The text is scrolled up two lines less than one screen.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */
int size;

    appData = (ApplicationData *) clientData;
    XtVaGetValues( appData->vertScroll,
            XmNsliderSize,      &size,
            0 );
    xdMoveCursor( appData, (size < 1) ? -1 : -size + 2);
}

/****************************************************************************/

void cursorTopCallback (

/*
 *  Move the cursor to the top of the files.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    if( appData->diffs.current != appData->diffs.first ){
        appData->diffs.current = appData->diffs.first;
        xdMoveToCurrentLine( appData );
     }
}

/****************************************************************************/

void cursorBottomCallback (

/*
 *  Move the cursor to the bottom of the files.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;

    if( appData->diffs.current != appData->diffs.last ){
        appData->diffs.current = appData->diffs.last;
        xdMoveToCurrentLine( appData );
    }
}

#ifdef __RedoDiff__
/****************************************************************************/

void redoDiffCallback (

/*
 *  Diff the files again and re-display the result.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;           /* All application Data.                */

    appData = (ApplicationData *) clientData;
    if ( xdBuildDiffs( &appData->diffs ) == -1 ) {
        fprintf (stderr, "ERROR: xdBuildDiffs failed!\n");
    }
    resizeEventHandler (w, clientData, NULL, NULL);
}
#endif /* __RedoDiff__ */
