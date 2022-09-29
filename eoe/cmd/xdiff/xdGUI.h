/****************************************************************************/
/*                                 xdGUI.h                                  */
/****************************************************************************/

/*
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
 *	Copyright (c) 1994 Rudy Wortel. All rights reserved.
 */

/*
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdGUI.h,v $
 *  $Revision: 1.1 $
 *  $Author: dyoung $
 *  $Date: 1994/09/02 21:20:58 $
 */

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeB.h>
#include <Xm/CascadeBG.h>
#include <Xm/RowColumn.h>
#include <Xm/Frame.h>
#include <Xm/PanedW.h>
#include <Xm/Form.h>
#include <Xm/ScrollBar.h>
#include <Xm/ArrowB.h>
#include <Xm/ScrolledW.h>
#include <Xm/List.h>
#include <Xm/DrawingA.h>
#include <Xm/FileSB.h>
#include <Xm/MessageB.h>
#include <Xm/Separator.h>
#include <Xm/SelectioB.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <X11/StringDefs.h>

/* Widget defines */
#define	XD_LEFT				( 1)
#define XD_RIGHT			( 2)
#define XD_NEXT				( 3)
#define XD_PREV				( 4)

typedef struct {
	Pixel		foregroundColour;
	Pixel		backgroundColour;
	XFontStruct	*font;
} XDattribute;

typedef struct {				/* All there is to know for one instance.	*/
	char			*appInstance;
	char			*appClass;
	XtAppContext	appContext;
	Display			*display;
	int				screen;
	GC				gc;

	int				tabWidth;
	Boolean			lineNumbers;
	Boolean			confirmWrite;
	char			*stdinName;

	XDlist			diffs;
	char			*pathNames[ 3 ];
	XDattribute		attributes[ 6 ];
	Pixel			cursorColour;
	Pixel			matchColour;
	XCharStruct		maxBounds;

	Widget			appShell;		/* A bunch of widgets.					*/
	Widget			mainWindow;
	Widget			mainForm;
	Widget			viewArea;
	Widget			textForm;
	Widget			messageLine;
	Widget			leftWindow;
	Widget			rightWindow;
	Widget			vertScroll;
	Widget			leftScroll;
	Widget			rightScroll;

	Widget			selectorShell;
	Widget			searchShell;
	Widget			fileSelector;
	Widget			confirmShell;
	Widget			messageBox;
} ApplicationData;

typedef struct {
	String name;					/* The name of this button.				*/
	XtCallbackProc inputCallback;	/* Activate Callback procedure.			*/
} xdMenuButton;

typedef struct {
	String			name;			/* The name of the pull down.			*/
	xdMenuButton	*buttons;		/* Array of the buttons for this menu.	*/
	int				n;				/* Number of buttons in the menu.		*/
} xdPullDown;
