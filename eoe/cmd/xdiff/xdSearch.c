/****************************************************************************/
/*                               xdSearch.c                                 */
/****************************************************************************/

/*
 *  All the searching routines for xdiff the graghical file difference
 *  browser.
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
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdSearch.c,v $
 *  $Revision: 1.1 $
 *  $Author: dyoung $
 *  $Date: 1994/09/02 21:21:42 $
 */

#include <stdio.h>
#include <xdDiff.h>
#include <xdGUI.h>
#include <xdProto.h>

#define INIT        char *sp = instring;
#define GETC()      (*sp++)
#define PEEKC()     (*sp)
#define UNGETC(c)   (--sp)
#define RETURN(c)   return(c)
#define ERROR(c)    xdRegError( c )
#define ESIZE       1000

#include <regexp.h>

/****************************************************************************/

void xdRegError (

/*
 *  A dummy error routine for regexp routines.
 */

int errorCode                   /*< regexp error code.                      */
)
{ /**************************************************************************/

    return;
}

/****************************************************************************/

void xdSearchText (

/*
 *  Look through all the text and try matching the regular expression.
 *  Add the XD_MATCHED flag to all those that do.
 */

ApplicationData *appData,       /*< All application Data.                   */
char *patternText               /*< Match this regular expression.          */
)
{ /**************************************************************************/

XDdiff *diff;
char regExp[ ESIZE ];

    compile( patternText, regExp, &regExp[ ESIZE ], '\0' );

    diff = appData->diffs.first;
    while( diff != NULL ){
        diff->flags &= ~XD_MATCHED_MASK;
        if( diff->right != NULL ){
            if( step( diff->right, regExp ) ){
                diff->flags |= XD_MATCHED_RIGHT;
            }
        }
        if( diff->left != NULL ){
            if( step( diff->left, regExp ) ){
                diff->flags |= XD_MATCHED_LEFT;
            }
        }
        diff = diff->next;
    }
}

/****************************************************************************/

void xdSearchCallback (

/*
 *  Callback to handle the button presses in the Search window.
 */

Widget selectionBox,            /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

XmSelectionBoxCallbackStruct *cbs;
ApplicationData *appData;
char *searchText;

    cbs = (XmSelectionBoxCallbackStruct *) callData;
    appData = (ApplicationData *) clientData;

    switch( cbs->reason ){
    case XmCR_APPLY:
        XmStringGetLtoR( cbs->value, XmSTRING_DEFAULT_CHARSET, &searchText );

        if( *searchText != '\0' ){
            xdSearchText( appData, searchText );
        }
        XtFree( searchText );
        xdDrawView( appData );

        xdSetMessageLine( appData, NULL );
        break;

    case XmCR_CANCEL:
        XtPopdown( appData->searchShell );
        break;
    }
}

/****************************************************************************/

void createSearchShell (

/*
 *  Create transientShell with a DIALOG_PROMPT selection box. Use this
 *  window to specify search patterns.
 */

ApplicationData *appData        /*< All application Data.                   */
)
{ /**************************************************************************/

Arg args[ 10 ];                 /* Arguments to creation functions.         */
Widget selectionBox;            /* XmSelectionBox for general UI.           */
Widget child;                   /* General purpose widget reference.        */
Widget form;                    /* Search window form for widget management.*/
Widget frame;
Widget window;
Widget pained;
Widget form2;
Widget button1;
Widget button2;
Widget button3;

    appData->searchShell = XtCreatePopupShell( "searchWindow",
            transientShellWidgetClass, appData->mainWindow, NULL, 0 );
    XtVaSetValues( appData->searchShell,
            XmNallowShellResize,True,
            XmNdeleteResponse,  XmUNMAP,
            0 );

    form = XmCreateForm( appData->searchShell, "searchForm", NULL, 0 );
    XtVaSetValues( form,
            XmNfractionBase,        100,
            XmNrubberPositioning,   True,
            0 );

    XtSetArg( args[ 0 ], XmNdialogType, XmDIALOG_PROMPT );
    selectionBox = XmCreateSelectionBox(
            form, "searchSelection", args, 1 );

    XtVaSetValues( selectionBox,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNuserData,            appData,
            0 );

    XtAddCallback( selectionBox, XmNapplyCallback, xdSearchCallback, appData  );
    XtAddCallback( selectionBox, XmNcancelCallback, xdSearchCallback, appData );

    /* remove unwanted buttons */
    child = XmSelectionBoxGetChild( selectionBox, XmDIALOG_OK_BUTTON );
    XtUnmanageChild( child );
    child = XmSelectionBoxGetChild( selectionBox, XmDIALOG_HELP_BUTTON );
    XtUnmanageChild( child );

    child = XmSelectionBoxGetChild( selectionBox, XmDIALOG_APPLY_BUTTON );
    XtManageChild( child );

    XtManageChild( selectionBox );

#   if defined( CRAP ) /*{{ */
     frame = XmCreateFrame( form, "frame", NULL, 0 );
    XtVaSetValues( frame,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        selectionBox,
            0 );

    XtSetArg( args[ 0 ], XmNscrollingPolicy, XmAUTOMATIC );
    window = XmCreateScrolledWindow( form, "window", args, 1 );
    XtVaSetValues( window,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        selectionBox,
            0 );

    form2 = XmCreateForm( window, "otherForm", NULL, 0 );
    XtVaSetValues( form2,
            XmNfractionBase,        100,
            XmNrubberPositioning,   True,
            XmNwidth,               300,
            0 );

    pained = XmCreatePanedWindow( form2, "pained", NULL, 0 );
    XtVaSetValues( pained,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNsashIndent,          10,
            0 );

    button1 = XmCreatePushButton( pained, "button One", NULL, 0 );
    XtManageChild( button1 );
    button2 = XmCreatePushButton( pained, "button Two", NULL, 0 );
    XtManageChild( button2 );
    button3 = XmCreatePushButton( pained, "button Three", NULL, 0 );
    XtManageChild( button3 );

    XtManageChild( pained );
    XtManageChild( form2 );
    XtManageChild( window );

#   if defined( CRAP ) /*{{ */
    XtManageChild( frame );
#   endif /*}} */

#   endif /*}} */
    XtManageChild( form );
}

/****************************************************************************/

void searchCallback (

/*
 *  Callback function to display the search window.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    if( appData->searchShell == NULL ){
        createSearchShell( appData );
    }

    XtPopup( appData->searchShell, XtGrabNone );
}

/****************************************************************************/

void searchNextCallback (

/*
 *  Callback function move the cursor line to the next occurance of
 *  the search pattern.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdFindMatch( appData, XD_NEXT );
}

/****************************************************************************/

void searchPreviousCallback (

/*
 *  Callback function move the cursor line to the previous occurance of
 *  the search pattern.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

ApplicationData *appData;

    appData = (ApplicationData *) clientData;

    xdFindMatch( appData, XD_PREV );
}
