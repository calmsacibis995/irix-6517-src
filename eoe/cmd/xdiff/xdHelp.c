/****************************************************************************/
/*                                xdHelp.c                                  */
/****************************************************************************/

/*
 *  All the help routines for xdiff the graghical file difference browser.
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
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdHelp.c,v $
 *  $Revision: 1.2 $
 *  $Author: ack $
 *  $Date: 1995/12/30 02:19:48 $
 */

#include <stdio.h>
#include <xdDiff.h>
#include <xdGUI.h>
#include <xdProto.h>
#include <Xm/MessageB.h>


/****************************************************************************/


void xdHelpDialogCallback (

/*
 *  Callback to handle button presses in the Help window.
 */

Widget dialogBox,               /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

    XmAnyCallbackStruct *cbs;

    cbs = (XmAnyCallbackStruct *) callData;

    switch( cbs->reason ){

      case XmCR_CANCEL:

        XtDestroyWidget( clientData );
        break;
    }
}


void xdShowHelpMessage (

/*
 *  Show a help message in a window.
 */

Widget parent,                  /*< Parent widget.                          */
char *widgetName             /*< Display help for this widget.           */
)
{ /**************************************************************************/

    ApplicationData *appData;
    Arg args[ 10 ];
    Widget helpShell;
    Widget helpMessage;
    Widget messageWindow;
    Widget selectionBox;
    Widget child;

    Pixel bg;

    XtVaGetValues( parent, XmNuserData, &appData, 0 );

    /* Create the help Window and it's popup shell */


    helpShell = XmCreateInformationDialog(appData->mainWindow, "helpWindow", NULL, 0);

    XtAddCallback( helpShell,
		   XmNokCallback, xdHelpDialogCallback, helpShell );

    /* remove unwanted widgets */
    if(child = XmMessageBoxGetChild( helpShell, XmDIALOG_CANCEL_BUTTON ))
	XtUnmanageChild( child );

    if(child = XmMessageBoxGetChild( helpShell, XmDIALOG_HELP_BUTTON ))
	XtUnmanageChild( child );

    if(child = XmMessageBoxGetChild( helpShell, XmDIALOG_MESSAGE_LABEL ))
	XtUnmanageChild( child );

    if(child = XmMessageBoxGetChild( helpShell, XmDIALOG_SYMBOL_LABEL ))
	XtUnmanageChild( child );

    XtSetArg( args[ 0 ], XmNscrollingPolicy, XmAUTOMATIC );
    messageWindow = XmCreateScrolledWindow( helpShell, "messageWindow", args, 1 );

    helpMessage = XmCreateLabel( messageWindow, widgetName, NULL, 0 );

    /* Force scrolled window to look right */

    XtVaGetValues( helpMessage, XmNbackground, &bg, NULL);
    XtVaSetValues( XtParent(helpMessage), XmNbackground, bg, NULL);

    XtVaSetValues( helpMessage,
            XmNalignment,           XmALIGNMENT_BEGINNING,
            0 );
    XtManageChild( helpMessage );

    XtVaSetValues( messageWindow,
            XmNworkWindow,          helpMessage,
            0 );

    XtManageChild( messageWindow );
    XtManageChild( helpShell );
}

/****************************************************************************/

void helpCallback (

/*
 *  Callback function for contextual help. The help message is based
 *  on the widgets name. This is passed as the clientData in the
 *  helpCallback.
 */

Widget w,                       /*< Widget ID.                              */
XtPointer clientData,           /*< Data from application.                  */
XtPointer callData              /*< Data from Widget Class.                 */
)
{ /**************************************************************************/

char *widgetName;

    widgetName = (char *) clientData;

    xdShowHelpMessage( w, widgetName  );
}
