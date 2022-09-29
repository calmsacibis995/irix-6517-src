/****************************************************************************/
/*                                 xdMain.c                                 */
/****************************************************************************/

/*
 *  Produce a graphical difference of two files.
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
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdMain.c,v $
 *  $Revision: 1.3 $
 *  $Author: olson $
 *  $Date: 1995/03/18 01:53:32 $
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <xdDiff.h>
#include <xdGUI.h>
#include <xdProto.h>

static XtResource appResources[] = {

    {   "stdinName", "StdinName", XtRString, sizeof (String),
            XtOffsetOf( ApplicationData, stdinName ),
            XtRString, (XtPointer) "<stdin>" },

    {   "tabWidth", "TabWidth", XtRInt, sizeof (int),
            XtOffsetOf( ApplicationData, tabWidth ),
            XtRImmediate, (XtPointer) 8 },

    {   "confirmWrite", "ConfirmWrite", XtRBoolean, sizeof (Boolean),
            XtOffsetOf( ApplicationData, confirmWrite ),
            XtRImmediate, (XtPointer) True },
};

static XrmOptionDescRec appOptions[] = {
    {   "-",        NULL,               XrmoptionSkipNArgs, 0           },
    {   "-w",       NULL,               XrmoptionSkipNArgs, 0           },
    {   "-b",       NULL,               XrmoptionSkipNArgs, 0           },
    {   "-i",       NULL,               XrmoptionSkipNArgs, 0           },
    {   "-D",       NULL,               XrmoptionSkipNArgs, 0           },
    {   "-N",       "*stdinName",       XrmoptionSepArg,    NULL        },
    {   "-tabs",    "*tabWidth",        XrmoptionSepArg,    NULL        },
};

static char *xdAttributes[] = {
    "Common",
    "Only",
    "Absent",
    "Changed",
     "Selected",
    "Deleted"
};

xdMenuButton xdFileButtons[] = {
    {   "saveLeft",             saveAsLeftCallback          },
    {   "saveRight",            saveAsRightCallback         },
    {   "saveAs",               saveAsCallback              },
    {   NULL,                   NULL                        },
    {   "exit",                 exitCallback                },
};

xdMenuButton xdEditButtons[] = {
    {   "search",               searchCallback              },
    {   "searchNext",           searchNextCallback          },
    {   "searchPrevious",       searchPreviousCallback      },
    {   NULL,                   NULL                        },
    {   "scrollDown",           scrollDownCallback          },
    {   "scrollUp",             scrollUpCallback            },
    {   "cursorDown",           cursorDownCallback          },
    {   "cursorUp",             cursorUpCallback            },
    {   "cursorTop",            cursorTopCallback           },
    {   "cursorBottom",         cursorBottomCallback        },
#ifdef __RedoDiff__
    {   NULL,                   NULL                        },
    {   "redoDiff",             redoDiffCallback            },
#endif /* __RedoDiff__ */
};

xdMenuButton xdGlobalButtons[] = {
    {   "selectGlobalLeft",     selectGlobalLeftCallback    },
    {   "selectGlobalRight",    selectGlobalRightCallback   },
    {   "selectGlobalNeither",  selectGlobalNeitherCallback },
    {   "unselectGlobal",       unselectGlobalCallback      },
    {   NULL,                   NULL                        },
    {   "tabs4",                tabsAt4Callback             },
    {   "tabs8",                tabsAt8Callback             },
    {   NULL,                   NULL                        },
    {   "lineNumbers",          lineNumbersCallback         },
};

xdMenuButton xdRegionButtons[] = {
    {   "selectRegionLeft",     selectRegionLeftCallback    },
    {   "selectRegionRight",    selectRegionRightCallback   },
    {   "selectRegionNeither",  selectRegionNeitherCallback },
    {   "unselectRegion",       unselectRegionCallback      },
    {   NULL,                   NULL                        },
    {   "splitRight",           splitRightCallback          },
    {   "splitLeft",            splitLeftCallback           },
};

xdMenuButton xdLineButtons[] = {
    {   "selectLineLeft",       selectLineLeftCallback      },
    {   "selectLineRight",      selectLineRightCallback     },
    {   "selectLineNeither",    selectLineNeitherCallback   },
    {   "unselectLine",         unselectLineCallback        },
};

xdMenuButton xdHelpButtons[] = {
    {   "onContext",            helpOnContextCallback       },
    {   NULL,                   NULL                        },
    {   "aboutXdiff",           aboutXdiffCallback          },
};

xdMenuButton xdPopupButtons[] = {
    {   "nextDifference",       nextDifferenceCallback      },
    {   "previousDifference",   previousDifferenceCallback  },
    {   "nextUnselected",       nextUnselectedCallback      },
    {   "previousUnselected",   previousUnselectedCallback  },
};

xdPullDown xdMainMenuBar[] = {
    {   "fileMenu",     xdFileButtons,      XtNumber( xdFileButtons )       },
    {   "editMenu",     xdEditButtons,      XtNumber( xdEditButtons )       },
    {   "globalMenu",   xdGlobalButtons,    XtNumber( xdGlobalButtons )     },
    {   "regionMenu",   xdRegionButtons,    XtNumber( xdRegionButtons )     },
    {   "lineMenu",     xdLineButtons,      XtNumber( xdLineButtons )       },
    {   "helpMenu",     xdHelpButtons,      XtNumber( xdHelpButtons )       },
};

xdPullDown xdPopupMenu[] = {
    {   "popupMenu",    xdPopupButtons,     XtNumber( xdPopupButtons )      },
};

/****************************************************************************/

void attachScrollBarCallbacks (

/*
 *  Attach all the required callbacks to the given scroll bar.
 */

Widget sb,                      /*< Scroll Bar Widget.                      */
XtCallbackProc callback,        /*< Callback procedure.                     */
XtPointer data                  /*< User data.                              */
)
{ /**************************************************************************/

    XtAddCallback( sb, XmNtoTopCallback, callback, data );
    XtAddCallback( sb, XmNtoBottomCallback, callback, data );
    XtAddCallback( sb, XmNdragCallback, callback, data );
    XtAddCallback( sb, XmNincrementCallback, callback, data );
    XtAddCallback( sb, XmNdecrementCallback, callback, data );
    XtAddCallback( sb, XmNpageIncrementCallback, callback, data );
    XtAddCallback( sb, XmNpageDecrementCallback, callback, data );
}

/****************************************************************************/

void createMainForm (

/*
 *  Create a XmForm to contain the application widget heirarchy listed below.
 *
 *  mainForm{ viewFrame, textForm }
 *   viewFrame{ viewArea }
 *   textForm{ messageLine, spacer, verticalScroll,
 *          leftScroll, rightScroll, leftFrame, rightFrame }
 *    rightFrame{ rightWindow }
 *    rightWindow{ rightPopup }
 *    leftFrame{ rightWindow }
 *    leftWindow{ leftPopup }
 */

ApplicationData *appData        /*< All application Data.                   */
)
{ /**************************************************************************/

Widget frame;                   /* frame widget for the drawing areas.      */
Widget spacer;                  /* spacer for centrering the scroll bar.    */
Widget popUp;                   /* a popup for the text areas.              */

    appData->mainForm =
            XmCreateForm( appData->mainWindow, "mainForm", NULL, 0 );
    XtVaSetValues( appData->mainForm,
            XmNfractionBase,        100,
             XmNrubberPositioning,   True,
            0 );

    XtAddEventHandler( appData->mainForm, StructureNotifyMask, False,
            (XtEventHandler) resizeEventHandler, appData );

    frame = XmCreateFrame( appData->mainForm, "viewFrame", NULL, 0 );
    XtVaSetValues( frame,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNfractionBase,        100,
            XmNrubberPositioning,   True,
            0 );

    appData->viewArea = XmCreateDrawingArea( frame, "viewWindow", NULL, 0 );
    XtVaSetValues( appData->viewArea,
            XmNwidth,               60,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->viewArea );
    XtAddCallback( appData->viewArea,
            XmNexposeCallback, viewAreaExposeCallback, appData );
    XtAddCallback( appData->viewArea,
            XmNinputCallback, viewAreaInputCallback, appData );
    XtAddCallback( appData->viewArea,
            XmNhelpCallback, helpCallback, "viewWindow" );
    XtManageChild( frame );

    appData->textForm = XmCreateForm( appData->mainForm, "textForm", NULL, 0 );
    XtVaSetValues( appData->textForm,
            XmNrubberPositioning,   True,
            XmNfractionBase,        100,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_WIDGET,
            XmNrightWidget,         frame,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNbottomAttachment,    XmATTACH_FORM,
            0 );

    appData->messageLine =
            XmCreateLabel( appData->textForm, "messageLine", NULL, 0 );
    XtVaSetValues( appData->messageLine,
            XmNlabelString,         XmStringCreate( " ",
                                            XmSTRING_DEFAULT_CHARSET ),
            XmNalignment,           XmALIGNMENT_BEGINNING,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNtopAttachment,       XmATTACH_FORM,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->messageLine );
    XtAddCallback( appData->messageLine,
            XmNhelpCallback, helpCallback, "messageLine" );

    spacer = XmCreateLabelGadget( appData->textForm, " ", NULL, 0 );
    XtVaSetValues( spacer,
            XmNrightAttachment,     XmATTACH_POSITION,
            XmNrightPosition,       50,
            XmNwidth,               10,
            XmNheight,              20,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNsensitive,           False,
             0 );
    XtManageChild( spacer );

    appData->vertScroll =
            XmCreateScrollBar( appData->textForm, "verticalScroll", NULL, 0 );
    XtVaSetValues( appData->vertScroll,
            XmNorientation,         XmVERTICAL,
            XmNwidth,               20,
            XmNvalue,               1,
            XmNsliderSize,          1,
            XmNminimum,             1,
            XmNmaximum,             (appData->diffs.lines == 0) ?
                                            2 : appData->diffs.lines + 1,
            XmNresizeHeight,        True,
            XmNresizeWidth,         True,
            XmNleftAttachment,      XmATTACH_OPPOSITE_WIDGET,
            XmNleftWidget,          spacer,
            XmNtopAttachment,       XmATTACH_WIDGET,
            XmNtopWidget,           appData->messageLine,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        spacer,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->vertScroll );
    attachScrollBarCallbacks( appData->vertScroll,
            scrollVerticalCallback, appData );
    XtAddCallback( appData->vertScroll,
            XmNhelpCallback, helpCallback, "verticalScroll" );

    appData->leftScroll =
            XmCreateScrollBar( appData->textForm, "leftScroll", NULL, 0 );
    XtVaSetValues( appData->leftScroll,
            XmNorientation,         XmHORIZONTAL,
            XmNheight,              20,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_WIDGET,
            XmNrightWidget,         appData->vertScroll,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->leftScroll );
    attachScrollBarCallbacks( appData->leftScroll,
            scrollLeftCallback, appData );
    XtAddCallback( appData->leftScroll,
            XmNhelpCallback, helpCallback, "horizontalScroll" );

    frame = XmCreateFrame( appData->textForm, "leftFrame", NULL, 0 );
    XtVaSetValues( frame,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_WIDGET,
            XmNrightWidget,         appData->vertScroll,
            XmNtopAttachment,       XmATTACH_WIDGET,
            XmNtopWidget,           appData->messageLine,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        appData->leftScroll,
            0 );
    XtManageChild( frame );

    appData->leftWindow = XmCreateDrawingArea( frame, "leftWindow", NULL, 0 );
    XtVaSetValues( appData->leftWindow,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->leftWindow );
    XtAddCallback( appData->leftWindow,
            XmNexposeCallback, leftWindowExposeCallback, appData );
    XtAddCallback( appData->leftWindow,
             XmNinputCallback, leftWindowInputCallback, appData );
    XtAddCallback( appData->leftWindow,
            XmNhelpCallback, helpCallback, "textWindow" );

    popUp = XmCreatePopupMenu( appData->leftWindow, "leftPopup", NULL, 0 );
    createPullDown( appData, popUp, xdPopupMenu );
    XtAddEventHandler( appData->leftWindow,
            ButtonPressMask, False, postDrawingAreaMenu, popUp );

    appData->rightScroll =
            XmCreateScrollBar( appData->textForm, "rightScroll", NULL, 0 );
    XtVaSetValues( appData->rightScroll,
            XmNorientation,         XmHORIZONTAL,
            XmNheight,              20,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          appData->vertScroll,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->rightScroll );
    attachScrollBarCallbacks( appData->rightScroll,
            scrollRightCallback, appData );
    XtAddCallback( appData->rightScroll,
            XmNhelpCallback, helpCallback, "horizontalScroll" );

    frame = XmCreateFrame( appData->textForm, "rightFrame", NULL, 0 );
    XtVaSetValues( frame,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          appData->vertScroll,
            XmNtopAttachment,       XmATTACH_WIDGET,
            XmNtopWidget,           appData->messageLine,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        appData->rightScroll,
            0 );
    XtManageChild( frame );

    appData->rightWindow = XmCreateDrawingArea( frame, "rightWindow", NULL, 0 );
    XtVaSetValues( appData->rightWindow,
            XmNuserData,            appData,
            0 );
    XtManageChild( appData->rightWindow );
    XtAddCallback( appData->rightWindow,
            XmNexposeCallback, rightWindowExposeCallback, appData );
    XtAddCallback( appData->rightWindow,
            XmNinputCallback, rightWindowInputCallback, appData );
    XtAddCallback( appData->rightWindow,
            XmNhelpCallback, helpCallback, "textWindow" );

    popUp = XmCreatePopupMenu( appData->rightWindow, "rightPopup", NULL, 0 );
    createPullDown( appData, popUp, xdPopupMenu );
    XtAddEventHandler( appData->rightWindow,
            ButtonPressMask, False, postDrawingAreaMenu, popUp );

    XtManageChild( appData->textForm );
    XtManageChild( appData->mainForm );
}

/****************************************************************************/

void createPullDown (

/*
 *  Create all the buttons for a PullDown menu Pane.
 */

ApplicationData *appData,       /*< All application Data.                   */
Widget parent,                  /*< Parent XmPullDownMenu widget.           */
xdPullDown *pullDown            /*< This list of buttons in the pulldown.   */
)
{ /**************************************************************************/

Widget widget;
String accelerator;
XmString acceleratorText;
XmString string;
xdMenuButton *menuButton;
Arg argv[ 10 ];
int argc;
int button;

    for( button = 0; button < pullDown->n; ++button ){
        menuButton = &pullDown->buttons[ button ];
        if( menuButton->name == NULL ){
            widget = XmCreateSeparator( parent, "-", NULL, 0 );
        }else{
            XtSetArg( argv[ 0 ], XmNuserData, appData );
            argc = 1;
            widget = XmCreatePushButton( parent, menuButton->name, argv, argc );
            if( menuButton->inputCallback != NULL ){
                XtAddCallback( widget, XmNactivateCallback,
                        menuButton->inputCallback, appData );
            }
            accelerator = NULL;
            acceleratorText = NULL;
            XtVaGetValues( widget,
                    XmNaccelerator,         &accelerator,
                    XmNacceleratorText,     &acceleratorText,
                    0 );
            if( accelerator != NULL && acceleratorText == NULL ){
                string = XmStringCreate(
                        accelerator, XmSTRING_DEFAULT_CHARSET );
                XtVaSetValues( widget,
                        XmNacceleratorText,     string,
                        XmNuserData,            appData,
                        0 );
                XmStringFree( string );
            }
            if( !strcmp( menuButton->name, "saveRight" ) ){
                if( !(appData->diffs.flags & XD_RIGHT_WRITABLE) ){
                    XtVaSetValues( widget,
                            XmNsensitive,       False,
                            0 );
                }
            }else if( !strcmp( menuButton->name, "saveLeft" ) ){
                if( !(appData->diffs.flags & XD_LEFT_WRITABLE) ){
                    XtVaSetValues( widget,
                            XmNsensitive,       False,
                            0 );
                }
            }
        }

        XtManageChild( widget );
    }
}

/****************************************************************************/

void createMenuBar (

/*
 *  Encapsulation of the creation the Menu Bar and all of its pulldowns.
 */

ApplicationData *appData,       /*< All application Data.                   */
Widget parent,                  /*< Parent widget for the menu bar.         */
xdPullDown pulldowns[],         /*< What do you want to put in it.          */
int nPulldowns                  /*< How many are there.                     */
)
{ /**************************************************************************/

Widget menuBar;
Widget pullDown;
Widget cascadeButton;
xdPullDown *menuItem;
Arg argv[ 10 ];
int menu;

    XtSetArg( argv[ 0 ], XmNuserData, appData );
    menuBar = XmCreateMenuBar( parent, "menuBar", argv, 1 );
    for( menu = 0; menu < nPulldowns; ++menu ){
        menuItem = &pulldowns[ menu ];
        pullDown = XmCreatePulldownMenu( menuBar, menuItem->name, NULL, 0 );
        createPullDown( appData, pullDown, menuItem );

        XtSetArg( argv[ 0 ], XmNsubMenuId, pullDown );
        XtSetArg( argv[ 1 ], XmNuserData, appData );
        cascadeButton = XmCreateCascadeButton(
                menuBar, menuItem->name, argv, 2 );

        XtAddCallback( cascadeButton,
                XmNhelpCallback, helpCallback, menuItem->name );

        /* Menu specfic things */
        if( strcmp( menuItem->name, "helpMenu" ) == 0 ){
            XtVaSetValues( menuBar,
                    XmNmenuHelpWidget,  cascadeButton,
                    0 );

        }
        XtManageChild( cascadeButton );
    }

    XtAddCallback( menuBar, XmNhelpCallback, helpCallback, "menuBar" );
    XtManageChild( menuBar );
}

/****************************************************************************/

void createTransients (

/*
 *  Create and attach the transient shells.
 */

ApplicationData *appData        /*< All application Data.                   */
)
{ /**************************************************************************/

Widget button;

    /* Create the File lister and it's popup shell */
    appData->selectorShell = XtCreatePopupShell( "fileLister",
            transientShellWidgetClass, appData->mainWindow, NULL, 0 );
    XtVaSetValues( appData->selectorShell,
            XmNallowShellResize,True,
             XmNdeleteResponse,  XmUNMAP,
            0 );

    appData->fileSelector = XmCreateFileSelectionBox(
            appData->selectorShell, "fileLister", NULL, 0 );
    XtManageChild( appData->fileSelector );

    XtAddCallback( appData->fileSelector,
            XmNokCallback, listerOkCallback, appData  );
    XtAddCallback( appData->fileSelector,
            XmNcancelCallback, listerCancelCallback, appData );

    button = XmFileSelectionBoxGetChild(
            appData->fileSelector, XmDIALOG_HELP_BUTTON );
    XtUnmanageChild( button );

    /* Simple confirm shell */
    appData->confirmShell = XtCreatePopupShell( "confirmWindow",
            transientShellWidgetClass, appData->mainWindow, NULL, 0 );

    XtVaSetValues( appData->confirmShell,
            XmNdeleteResponse,  XmDO_NOTHING,
            XmNallowShellResize,True,
            0 );

    appData->messageBox = XmCreateMessageBox(
            appData->confirmShell, "messageBox", NULL, 0 );

    XtVaSetValues( appData->messageBox,
            XmNresizable,       True,
            XmNdialogType,      XmDIALOG_QUESTION,
            0 );

    XtManageChild( appData->messageBox );

    XtAddCallback( appData->messageBox,
            XmNokCallback, confirmCallback, appData );
    XtAddCallback( appData->messageBox,
            XmNcancelCallback, confirmCallback, appData );

    /* Defer the creation of the search Shell. */
    appData->searchShell = NULL;
}

/****************************************************************************/

void createApplicationWidgets (

/*
 *  Create the top level application Widgets.
 */

ApplicationData *appData        /*< All application Data.                   */
)
{ /**************************************************************************/

    appData->mainWindow =
            XmCreateMainWindow( appData->appShell, "mainWindow", NULL, 0 );

    createMenuBar( appData,
            appData->mainWindow, xdMainMenuBar, XtNumber( xdMainMenuBar ) );

    createMainForm( appData );

    createTransients( appData );

     XtManageChild( appData->mainWindow );
}

/****************************************************************************/

void xdLoadAttributes (

/*
 *  Load the fonts and colours for the various states and calculate
 *  the maximum bounds of the characters. If the resources are not specified
 *  use the common font.
 */

ApplicationData *appData        /*< All application Data.                   */
)
{ /**************************************************************************/

Display *display;
char *appClass;
XFontStruct *font;
Colormap colourMap;
XColor rgb;
Pixel foregroundColour;
Pixel backgroundColour;
char resourceName[ 100 ];
char *resourceValue;
int i;

    appData->maxBounds.width = -1;
    appData->maxBounds.ascent = -1;
    appData->maxBounds.descent = -1;

    appData->screen = DefaultScreen( appData->display );
    appData->gc = XCreateGC( appData->display,
            RootWindow( appData->display, appData->screen ), 0, 0 );

    display = XtDisplay( appData->appShell );
    appClass = appData->appClass;

    colourMap = DefaultColormap( appData->display, appData->screen );

    /* find out the fonts and colours for all the display states. */
    for( i = 0; i < XtNumber( xdAttributes ); ++i ){
        foregroundColour = 0;
        sprintf( resourceName, "fgc%s", xdAttributes[ i ] );
        resourceValue = xdGetDefault( display, appClass, resourceName );
        if( XParseColor( display, colourMap, resourceValue, &rgb ) ){
            if( XAllocColor( display, colourMap, &rgb ) ){
                foregroundColour = rgb.pixel;
            }else{
                fprintf( stderr, "Can't allocate colour for %s\n",
                        resourceName );
            }
        }else{
            fprintf( stderr, "Can't get default for %s\n", resourceName );
        }

        backgroundColour = 0;
        sprintf( resourceName, "bgc%s", xdAttributes[ i ] );
        resourceValue = xdGetDefault( display, appClass, resourceName );
        if( XParseColor( display, colourMap, resourceValue, &rgb ) ){
            if( XAllocColor( display, colourMap, &rgb ) ){
                backgroundColour = rgb.pixel;
            }else{
                fprintf( stderr, "Can't allocate colour for %s\n",
                        resourceName );
             }
        }else{
            fprintf( stderr, "Can't get default for %s\n", resourceName );
        }

        sprintf( resourceName, "font%s", xdAttributes[ i ] );
        resourceValue = xdGetDefault( display, appClass, resourceName );
        font = XLoadQueryFont( display, resourceValue );
        if( font != NULL ){
            if( font->max_bounds.width > appData->maxBounds.width ){
                appData->maxBounds.width = font->max_bounds.width;
            }
            if( font->max_bounds.ascent > appData->maxBounds.ascent ){
                appData->maxBounds.ascent = font->max_bounds.ascent;
            }
            if( font->max_bounds.descent > appData->maxBounds.descent ){
                appData->maxBounds.descent = font->max_bounds.descent;
            }
        }else{
            font = appData->attributes[ 0 ].font;
        }

        appData->attributes[ i ].foregroundColour = foregroundColour;
        appData->attributes[ i ].backgroundColour = backgroundColour;
        appData->attributes[ i ].font = font;
    }

    appData->cursorColour = -1;
    resourceValue = xdGetDefault( display, appClass, "cursorColour" );
    if( XParseColor( display, colourMap, resourceValue, &rgb ) ){
        if( XAllocColor( display, colourMap, &rgb ) ){
            appData->cursorColour = rgb.pixel;
        }
    }

    appData->matchColour = appData->cursorColour;
    resourceValue = xdGetDefault( display, appClass, "matchColour" );
    if( XParseColor( display, colourMap, resourceValue, &rgb ) ){
        if( XAllocColor( display, colourMap, &rgb ) ){
            appData->matchColour = rgb.pixel;
        }
    }
}

/****************************************************************************/

void xdUseage (

/*
 *  Print the useage string for xdiff.
 */

char *program                   /*< Name of the executable from argv[ 0 ].  */
)
{ /**************************************************************************/

    fprintf( stderr, "Usage: %s [-Dwbi] [-N name] path1 path2\n", program );
}

/****************************************************************************/

Boolean xdBuildFileNames (

/*
 *  Given two paths build the two file names. NULL means <stdin>.
 */

ApplicationData *appData        /*| All application Data.                   */
)
{ /**************************************************************************/

char fileName[ 1000 ];
struct stat statbuf[ 2 ];
char *slash;
int path;
int stdinPaths;
int dirPaths;
int filePaths;
int status;

    status = True;
    stdinPaths = 0;
    dirPaths = 0;
    filePaths = 0;

    /* make sure paths exist */
    for( path = 0; path < 2; ++path ){
        if( appData->pathNames[ path ] == NULL ){
            ++stdinPaths;
        }else{
            if( stat( appData->pathNames[ path ], &statbuf[ path ] ) == -1 ){
                fprintf( stderr, "%s: Can't open '%s'\n",
                        appData->appInstance, appData->pathNames[ path ] );
                status = False;
            }else{
                switch( statbuf[ path ].st_mode & S_IFMT ){
                case S_IFDIR:
                    ++dirPaths;
                    break;
                case S_IFREG:
                    ++filePaths;
                    break;
                default:
                    fprintf( stderr, "%s: '%s' has illegal file type\n",
                            appData->appInstance, appData->pathNames[ path ] );
                    status = False;
                    break;
                }
            }
        }
    }

    if( status == True ){
        if( stdinPaths > 1 ){
            fprintf( stderr, "%s: Only one path may be <stdin>\n",
                    appData->appInstance );
            status = False;
        }else if( dirPaths > 1 ){
            fprintf( stderr, "%s: Only one path may be a directory\n",
                    appData->appInstance );
            status = False;
        }else if( filePaths == 0 ){
            fprintf( stderr, "%s: One path must be a regular file\n",
                    appData->appInstance );
            status = False;
        }else{
            /* We must have a valid pair of path names by now. */
            if( appData->pathNames[ 0 ] == NULL ){
                appData->diffs.lfile = NULL;
            }else if( (statbuf[ 0 ].st_mode & S_IFMT) == S_IFDIR ){
                if( (slash = strrchr( appData->pathNames[ 1 ], '/' )) == NULL ){
                    sprintf( fileName, "%s/%s",
                             appData->pathNames[ 0 ], appData->pathNames[ 1 ] );
                }else{
                    sprintf( fileName, "%s%s", appData->pathNames[ 0 ], slash );
                }
                appData->diffs.lfile = strdup( fileName );
            }else{
                appData->diffs.lfile = strdup( appData->pathNames[ 0 ] );
            }

            if( appData->diffs.lfile == NULL ||
                    access( appData->diffs.lfile, 2 ) == 0 ){
                appData->diffs.flags |= XD_LEFT_WRITABLE;
            }else{
                appData->diffs.flags &= ~XD_LEFT_WRITABLE;
            }

            if( appData->pathNames[ 1 ] == NULL ){
                appData->diffs.rfile = NULL;
            }else if( (statbuf[ 1 ].st_mode & S_IFMT) == S_IFDIR ){
                if( (slash = strrchr( appData->pathNames[ 0 ], '/' )) == NULL ){
                    sprintf( fileName, "%s/%s",
                            appData->pathNames[ 1 ], appData->pathNames[ 0 ] );
                }else{
                    sprintf( fileName, "%s%s", appData->pathNames[ 1 ], slash );
                }
                appData->diffs.rfile = strdup( fileName );
            }else{
                appData->diffs.rfile = strdup( appData->pathNames[ 1 ] );
            }

            if( appData->diffs.rfile == NULL ||
                    access( appData->diffs.rfile, 2 ) == 0 ){
                appData->diffs.flags |= XD_RIGHT_WRITABLE;
            }else{
                appData->diffs.flags &= ~XD_RIGHT_WRITABLE;
            }
        }
    }

    return status;
}

/****************************************************************************/

Boolean xdParseArgs (

/*
 *  Parse the remaining arguments after X11 is done with them.
 */

ApplicationData *appData,       /*| All application Data.                   */
int argc,                       /*< Argument count.                         */
char *argv[]                    /*< Argument vector.                        */
)
{ /**************************************************************************/

int arg;
int path;
char *opt;

    path = 0;

    for( arg = 1; arg < argc; ++arg ){
        opt = argv[ arg ];
        if( *opt == '-' ){
            if( *++opt == '\0' ){
                 /* Simple '-' as an argument means use stdin for the file. */
                if( path < 2 ){
                    appData->pathNames[ path ] = NULL;
                    ++path;
                }
            }else{
                for( ; *opt != '\0'; ++opt ){
                    switch( *opt ){
                    case 'w':
                        appData->diffs.flags |= XD_IGN_TRAILING;
                        break;
                    case 'b':
                        appData->diffs.flags |= XD_IGN_WHITESPACE;
                        break;
                    case 'i':
                        appData->diffs.flags |= XD_IGN_CASE;
                        break;
                    case 'D':
                        appData->diffs.flags |= XD_EXIT_NODIFF;
                        break;
                    default:
                        fprintf( stderr, "%s: Unknown option '%c'\n",
                                appData->appInstance, *opt );
                        break;
                    }
                }
            }
        }else{
            /* Treat it as a path name */
            if( path < 2 ){
                appData->pathNames[ path ] = argv[ arg ];
                ++path;
            }
        }
    }

    return (path == 2);
}

/****************************************************************************/

int main (

/*
 *  Standard main function.
 */

int argc,                       /*< Standard main line arguments.           */
char *argv[],
char *envp[]
)
{ /**************************************************************************/

ApplicationData appData;        /* All application data.                    */
char windowTitle[ 1000 ];       /* Title for window title bar.              */
int differences;                /* How many differences are there.          */

    memset( &appData, 0, sizeof appData );
    if( (appData.appInstance = strrchr( argv[ 0 ], '/' )) == NULL ){
        appData.appInstance = argv[ 0 ];
    }else{
        ++appData.appInstance;
    }

    appData.appClass = "Xdiff";

     XtToolkitInitialize();
    appData.appContext = XtCreateApplicationContext();

    appData.display = XtOpenDisplay( appData.appContext,
            NULL, appData.appInstance, appData.appClass,
            appOptions, XtNumber( appOptions ), &argc, argv );

    if( appData.display == NULL ){
        fprintf( stderr, "%s: Can't open display\n", appData.appInstance );
        exit( -1 );
    }

    appData.appShell = XtAppCreateShell( appData.appInstance,
            appData.appClass, applicationShellWidgetClass, appData.display,
            NULL, 0 );

    XtGetApplicationResources( appData.appShell, &appData,
            appResources, XtNumber( appResources ), NULL, 0 );

    xdLoadAttributes( &appData );

    if( xdParseArgs( &appData, argc, argv ) == False ||
            xdBuildFileNames( &appData ) == False ){
        xdUseage( appData.appInstance );
        exit( -1 );
    }

    if( (differences = xdBuildDiffs( &appData.diffs )) == -1 ){
        exit( -1 );
    }

    if( !(appData.diffs.flags & XD_EXIT_NODIFF) || differences > 0 ){

        appData.lineNumbers = False;

        sprintf( windowTitle, "%.200s <-> %.200s",
                (appData.diffs.lfile == NULL) ?
                        appData.stdinName : appData.diffs.lfile,
                (appData.diffs.rfile == NULL) ?
                        appData.stdinName : appData.diffs.rfile );
        XtVaSetValues( appData.appShell, XmNtitle, windowTitle, 0 );

        createApplicationWidgets( &appData );

        XtRealizeWidget( appData.appShell );

        XtAppMainLoop( appData.appContext );
    }

    exit( 0 );
}
