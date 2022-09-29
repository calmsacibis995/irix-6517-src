/****************************************************************************/
/*                                 xdUtil.c                                 */
/****************************************************************************/

/*
 *  Utility routines called by the callbacks in xdCallbacks.c
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
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdUtil.c,v $
 *  $Revision: 1.4 $
 *  $Author: ack $
 *  $Date: 1995/12/30 02:19:49 $
 */

#include <stdio.h>
#include <xdDiff.h>
#include <xdGUI.h>
#include <xdProto.h>
#include <unistd.h>

/****************************************************************************/

XDattribute *xdGetAttributes (

/*
 *  Determine the attributes for drawing this difference.
 */

ApplicationData *appData,       /*< All application Data.                   */
XDdiff *diff,                   /*< Which line are we talking about.        */
int side                        /*< Are we drawing the left or the right.   */
)
{ /**************************************************************************/

XDattribute *attr;

    switch( diff->selection ){
    case XD_SELECTNONE:
        switch( diff->type ){
        case XD_COMMONTEXT:
            attr = &appData->attributes[ XD_COMMON ];
            break;
        case XD_LEFTTEXT:
            if( side == XD_LEFT ){
                attr = &appData->attributes[ XD_ONLY ];
            }else{
                attr = &appData->attributes[ XD_ABSENT ];
            }
            break;
        case XD_RIGHTTEXT:
            if( side == XD_RIGHT ){
                attr = &appData->attributes[ XD_ONLY ];
            }else{
                 attr = &appData->attributes[ XD_ABSENT ];
            }
            break;
        case XD_CHANGEDTEXT:
            attr = &appData->attributes[ XD_CHANGED ];
            break;
        }
        break;
    case XD_SELECTLEFT:
        if( side == XD_LEFT ){
            attr = &appData->attributes[ XD_SELECTED ];
        }else{
            attr = &appData->attributes[ XD_DELETED ];
        }
        break;
    case XD_SELECTRIGHT:
        if( side == XD_LEFT ){
            attr = &appData->attributes[ XD_DELETED ];
        }else{
            attr = &appData->attributes[ XD_SELECTED ];
        }
        break;
    case XD_SELECTNEITHER:
        attr = &appData->attributes[ XD_DELETED ];
        break;
    default:
        attr = &appData->attributes[ XD_COMMON ];
        break;
    }

    return attr;
}

/****************************************************************************/

void xdDrawView (

/*
 *  Draw the overview of the differences in the right side of the window.
 */

ApplicationData *appData            /*< All application Data.               */
)
{ /**************************************************************************/

Display *display;
Window window;
GC gc;
Dimension windowHeight;
Dimension windowWidth;
Pixel fgc;
Pixel bgc;
double lines2screen;
int maxLines;
Dimension leftOffset, rightOffset;
Dimension oneThird, twoThirds;
Dimension leftTop, leftBottom, rightTop, rightBottom;
XDdiff *topDiff, *bottomDiff;
XDlist *diffs;
XDattribute *attr;
XPoint points[ 5 ];

    diffs = &appData->diffs;
    maxLines = (diffs->rlines > diffs->llines) ? diffs->rlines : diffs->llines;

    if( maxLines == 0 ){
         return;
    }

    gc = appData->gc;
    display = appData->display;
    window = XtWindow( appData->viewArea );

    XtVaGetValues( appData->viewArea,
            XmNforeground,      &fgc,
            XmNbackground,      &bgc,
            XmNheight,          &windowHeight,
            XmNwidth,           &windowWidth,
            0 );

    lines2screen = (double) windowHeight / (double) maxLines;
    leftOffset = (maxLines - diffs->llines) * lines2screen / 2.0;
    rightOffset = (maxLines - diffs->rlines) * lines2screen / 2.0;

    oneThird = windowWidth / 3;
    twoThirds = 2 * oneThird;


    XClearWindow( display, window );

    topDiff = diffs->first;
    while( topDiff != NULL ){

        bottomDiff = topDiff;
        while( bottomDiff->next != NULL &&
                bottomDiff->next->type == topDiff->type ){
            bottomDiff = bottomDiff->next;
        }

        switch( topDiff->type ){
        case XD_CHANGEDTEXT:
        case XD_COMMONTEXT:
            leftTop = leftOffset + (topDiff->lnuml - 1) * lines2screen;
            leftBottom = leftOffset + bottomDiff->lnuml * lines2screen;
            rightTop = rightOffset + (topDiff->lnumr - 1) * lines2screen;
            rightBottom = rightOffset + (bottomDiff->lnumr) * lines2screen;

            attr = xdGetAttributes( appData, topDiff, XD_LEFT );
            XSetForeground( display, gc, attr->backgroundColour );
            XFillRectangle( display, window, gc,
                    1, leftTop, oneThird - 1, leftBottom - leftTop );

            attr = xdGetAttributes( appData, topDiff, XD_RIGHT );
            XSetForeground( display, gc, attr->backgroundColour );
            XFillRectangle( display, window, gc,
                    twoThirds + 1, rightTop,
                    oneThird - 1, rightBottom - rightTop );
            break;
        case XD_LEFTTEXT:
            leftTop = leftOffset + (topDiff->lnuml - 1) * lines2screen;
            leftBottom = leftOffset + bottomDiff->lnuml * lines2screen;
            rightTop = rightOffset + topDiff->lnumr * lines2screen;
            rightBottom = rightBottom;

            attr = xdGetAttributes( appData, topDiff, XD_LEFT );
            XSetForeground( display, gc, attr->backgroundColour );
            XFillRectangle( display, window, gc,
                    1, leftTop, oneThird - 1, leftBottom - leftTop );
            break;
        case XD_RIGHTTEXT:
            leftTop = leftOffset + topDiff->lnuml * lines2screen;
            leftBottom = leftTop;
             rightTop = rightOffset + (topDiff->lnumr - 1) * lines2screen;
            rightBottom = rightOffset + (bottomDiff->lnumr) * lines2screen;

            attr = xdGetAttributes( appData, topDiff, XD_RIGHT );
            XSetForeground( display, gc, attr->backgroundColour );
            XFillRectangle( display, window, gc,
                    twoThirds + 1, rightTop,
                    oneThird - 1, rightBottom - rightTop );
            break;
        }

        /*
         * Draw trim at the top of the ranges. The next range will do the
         * bottom
         */
        XSetForeground( display, gc, fgc );
        XDrawLine( display, window, gc,
                1, leftTop, oneThird, leftTop );
        XDrawLine( display, window, gc,
                oneThird, leftTop, twoThirds, rightTop );
        XDrawLine( display, window, gc,
                twoThirds, rightTop, windowWidth, rightTop );

        topDiff = bottomDiff->next;
    }

    /* We need to draw the last bottom trim */
    XDrawLine( display, window, gc,
            oneThird, windowHeight - leftOffset,
            twoThirds, windowHeight - rightOffset );
    /* draw the trim rectangles on both sides */
    XSetForeground( display, gc, fgc );
    XDrawRectangle( display, window, gc,
            0, leftOffset, oneThird, windowHeight - (2 * leftOffset) - 1 );
    XDrawRectangle( display, window, gc,
            twoThirds, rightOffset,
            oneThird - 1, windowHeight - (2 * rightOffset) - 1 );

    XSetForeground( display, gc, appData->matchColour );
    /* draw all the marked lines on top */
    topDiff = diffs->first;
    while( topDiff != NULL ){
        if( topDiff->flags & XD_MATCHED_LEFT ){
            leftTop = leftOffset + (topDiff->lnuml - 1) * lines2screen;
            points[ 0 ].x = oneThird;
            points[ 0 ].y = leftTop;
            points[ 1 ].x = oneThird - 4;
            points[ 1 ].y = leftTop + 4;
            points[ 2 ].x = oneThird - 8;
            points[ 2 ].y = leftTop;
            points[ 3 ].x = oneThird - 4;
            points[ 3 ].y = leftTop - 4;
            points[ 4 ].x = oneThird;
            points[ 4 ].y = leftTop;
            XFillPolygon( display, window, gc,
                    points, 5, Convex, CoordModeOrigin );
        }
        if( topDiff->flags & XD_MATCHED_RIGHT ){
            rightTop = rightOffset + (topDiff->lnumr - 1) * lines2screen;
            points[ 0 ].x = twoThirds;
            points[ 0 ].y = rightTop;
            points[ 1 ].x = twoThirds + 4;
            points[ 1 ].y = rightTop + 4;
            points[ 2 ].x = twoThirds + 8;
            points[ 2 ].y = rightTop;
            points[ 3 ].x = twoThirds + 4;
             points[ 3 ].y = rightTop - 4;
            points[ 4 ].x = twoThirds;
            points[ 4 ].y = rightTop;
            XFillPolygon( display, window, gc,
                    points, 5, Convex, CoordModeOrigin );
        }
        topDiff = topDiff->next;
    }

    /* draw the pointers */
    if( diffs->current->type == XD_RIGHTTEXT ){
        leftBottom = leftOffset + diffs->current->lnuml * lines2screen;
    }else{
        leftBottom = leftOffset + (diffs->current->lnuml - 1) * lines2screen;
    }
    points[ 0 ].x = oneThird;
    points[ 0 ].y = leftBottom;
    points[ 1 ].x = oneThird - 8;
    points[ 1 ].y = leftBottom + 3;
    points[ 2 ].x = oneThird - 8;
    points[ 2 ].y = leftBottom - 3;
    points[ 3 ].x = oneThird;
    points[ 3 ].y = leftBottom;
    XSetForeground( display, gc, bgc );
    XFillPolygon( display, window, gc, points, 3, Convex, CoordModeOrigin );
    if( appData->cursorColour != -1 ){
        XSetForeground( display, gc, appData->cursorColour );
    }else{
        XSetForeground( display, gc, fgc );
    }
    XDrawLines( display, window, gc, points, 4, CoordModeOrigin );

    if( diffs->current->type == XD_LEFTTEXT ){
        rightBottom = rightOffset + diffs->current->lnumr * lines2screen;
    }else{
        rightBottom = rightOffset + (diffs->current->lnumr - 1) * lines2screen;
    }
    points[ 0 ].x = twoThirds;
    points[ 0 ].y = rightBottom;
    points[ 1 ].x = twoThirds + 8;
    points[ 1 ].y = rightBottom + 3;
    points[ 2 ].x = twoThirds + 8;
    points[ 2 ].y = rightBottom - 3;
    points[ 3 ].x = twoThirds;
    points[ 3 ].y = rightBottom;
    XSetForeground( display, gc, bgc );
    XFillPolygon( display, window, gc, points, 3, Convex, CoordModeOrigin );
    if( appData->cursorColour != -1 ){
        XSetForeground( display, gc, appData->cursorColour );
    }else{
        XSetForeground( display, gc, fgc );
    }
    XDrawLines( display, window, gc, points, 4, CoordModeOrigin );
}

/****************************************************************************/

int expandTabs (

/*
 *  Expand the tabs to spaces.
 */

char *tabed,
char *buf,
int digits,
int width,
int line
)
{ /**************************************************************************/

char *f;
char *t;
char format[ 100 ];
int spaces;
int count = 0;

    if( digits != -1 ){
        sprintf( format, "%%%dd:", digits );
        sprintf( buf, format, line );
        t = &buf[ digits + 1 ];
    }else{
        t = buf;
    }
    for( f = tabed; f!=NULL && *f != '\0' && count < 10000; ++f ){
        if( *f < ' ' ){
            /* control charater */
            if( *f == '\t' ){
                spaces = width - (t - &buf[ digits + 1 ]) % width;
                while( spaces-- && count < 10000){
                    *t++ = ' ';
                     count++;
                }
            }else{
                *t++ = '^';
                *t++ = '@' + *f;
		 count++;
		 count++;
            }
        }else{
            *t++ = *f;
	     count++;
        }
    }

    *t = '\0';
    return t - buf;
}

/****************************************************************************/

void xdDrawText (

/*
 *  Draw the text in the appropriate window.
 */

ApplicationData *appData,           /*< All application Data.               */
int side                            /*< Is it the left or right text.       */
)
{ /**************************************************************************/

Display *display;
Window window;
Widget drawingArea;
Widget scroll;
GC gc;
char **text;
Dimension windowHeight;
Dimension windowWidth;
Dimension lineHeight;
Pixel fgc;
Pixel bgc;
int line;
int lines;
int i;
int offset;
int len;
char *p;
char str[ 11000 ];  /* For the very long-lined files people seem to try to diff */
XDattribute *attr;
XDdiff *diff;

    if( appData->diffs.rlines == 0 && appData->diffs.llines == 0 ){
        return;
    }

    if( side == XD_LEFT ){
        drawingArea = appData->leftWindow;
        scroll = appData->leftScroll;
    }else{
        drawingArea = appData->rightWindow;
        scroll = appData->rightScroll;
    }

    window = XtWindow( drawingArea );
    display = appData->display;
    gc = appData->gc;

    XtVaGetValues( drawingArea,
            XmNforeground,      &fgc,
            XmNbackground,      &bgc,
            XmNheight,          &windowHeight,
            XmNwidth,           &windowWidth,
            0 );

    line = 1;
    XtVaGetValues( appData->vertScroll, XmNvalue, &line, 0 );

    offset = 0;
    XtVaGetValues( scroll, XmNvalue, &offset, 0 );

    offset *= -appData->maxBounds.width;

    lineHeight = appData->maxBounds.ascent + appData->maxBounds.descent;
    lines = windowHeight / lineHeight;

    diff = xdFindDiff( &appData->diffs, line );

    for( i = 0; i < lines; ++i ){

        if( (line + i - 1) > appData->diffs.lines || diff == NULL ){
            break;
        }

        p = (side == XD_LEFT) ? diff->left : diff->right;

        attr = xdGetAttributes( appData, diff, side );

        XSetForeground( display, gc, attr->backgroundColour );
        XFillRectangle( display, window, gc,
                0, (lineHeight * i), windowWidth, lineHeight );

        if( diff == appData->diffs.current ){
            if( side == XD_LEFT && (diff->flags & XD_MATCHED_LEFT) ){
                XSetForeground( display, gc, appData->matchColour );
            }else if( side == XD_RIGHT && (diff->flags & XD_MATCHED_RIGHT) ){
                XSetForeground( display, gc, appData->matchColour );
            }else{
                XSetForeground( display, gc, appData->cursorColour );
            }
            XDrawRectangle( display, window, gc,
                    0, (lineHeight * i), windowWidth - 1, lineHeight - 1 );
         }

        if( p != NULL ){
            XSetForeground( display, gc, attr->foregroundColour );
            len = expandTabs( p, str,
                    (appData->lineNumbers) ? appData->diffs.digits : -1,
                    appData->tabWidth,
                    (side == XD_LEFT) ? diff->lnuml : diff->lnumr );

            if( attr->font != NULL ){
                XSetFont( display, gc, attr->font->fid );
            }
            XDrawString( display, window, gc, offset,
                    (lineHeight * i) + appData->maxBounds.ascent, str, len );
        }

        diff = diff->next;
    }

    /* Clear the remaining area if any. */
    len = windowHeight - (lineHeight * i);
    if( len > 0 ){
        XClearArea( display, window,
                0, (lineHeight * i) + 1, windowWidth - 1, len, False );
    }
}

/****************************************************************************/

Boolean xdFileWritable (

/*
 *  Determine if we can write to one of the two files.
 */

ApplicationData *appData,           /*< All application Data.               */
int side                            /*< Is it the left or right text.       */
)
{ /**************************************************************************/

Boolean writable;
char *file;

    writable = False;
    if( side == XD_LEFT ){
        file = appData->diffs.lfile;
    }else{
        file = appData->diffs.rfile;
    }
    if( file != NULL && access( file, W_OK ) == 0 ){
        writable = True;
    }

    return writable;
}

/****************************************************************************/

void xdSetMessageLine (

/*
 *  Change the text in the message line. NULL means clear the text.
 */

ApplicationData *appData,           /*< All application Data.               */
char *messageName                   /*< Message name.                       */
)
{ /**************************************************************************/

XmString message;
char *text;

    if( messageName == NULL ){
        text = " ";
    }else{
        text = xdGetDefault( appData->display, appData->appClass, messageName );
        if( text == NULL ){
            text = messageName;
        }
    }

    message = XmStringCreate( text, XmSTRING_DEFAULT_CHARSET );
    if( message != NULL ){
        XtVaSetValues( appData->messageLine,
                XmNlabelString,     message,
                0 );
        XmStringFree( message );
    }
}

/****************************************************************************/

void xdResizeSliders (

/*
 *  After an event that changes the number of lines or the size of the
 *  displayed region this will adjust the vertical slider to reflect the
 *  change.
 */

ApplicationData *appData            /*< All application Data.               */
)
{ /**************************************************************************/

Dimension windowHeight;
double fract;
int lineHeight;
int value;
int size;
int max;
int min;

    XtVaGetValues( appData->leftWindow,
            XmNheight,              &windowHeight,
            0 );

    XtVaGetValues( appData->vertScroll,
            XmNvalue,               &value,
            XmNmaximum,             &max,
            XmNminimum,             &min,
            0 );

    lineHeight = appData->maxBounds.ascent + appData->maxBounds.descent;
    fract = (windowHeight / lineHeight) / (double) appData->diffs.lines;
    fract = (fract > 1.0) ? 1.0 : fract;

    /* deal with those nasty boundary conditions */
    size = fract * appData->diffs.lines;
    if( size < 1 ){
        size = 1;
    }
    if( value > (appData->diffs.lines - size) ){
         value = appData->diffs.lines - size + 1;
    }
    if( value < min ){
        value = min;
    }

    max = (appData->diffs.lines == 0) ? 2 : appData->diffs.lines + 1,

    XtVaSetValues( appData->vertScroll,
            XmNmaximum,             max,
            XmNsliderSize,          size,
            XmNvalue,               value,
            0 );
}

/****************************************************************************/

void xdMoveToCurrentLine (

/*
 *  Update the display to center the current line.
 */

ApplicationData *appData            /*< All application Data.               */
)
{ /**************************************************************************/

int value;
int size;

    value = 1;
    XtVaGetValues( appData->vertScroll,
            XmNvalue,           &value,
            XmNsliderSize,      &size,
            0 );

    value = appData->diffs.current->index - size / 2;

    if( value < 1 ){
        value = 1;
    }else if( value > (appData->diffs.lines - size) ){
        value = appData->diffs.lines - size + 1;
    }

    XtVaSetValues( appData->vertScroll,
            XmNvalue,           value,
            0 );

    xdDrawText( appData, XD_LEFT );
    xdDrawText( appData, XD_RIGHT );
    xdDrawView( appData );
    xdSetMessageLine( appData, NULL );
}

/****************************************************************************/

void xdFindLine (

/*
 *  Move the pointer to the desired line number.
 */

ApplicationData *appData,           /*< All application Data.               */
int line,                           /*< Desired line.                       */
int side                            /*< Is it the left or right text.       */
)
{ /**************************************************************************/

XDdiff *diff;

    diff = appData->diffs.first;
    while( diff != NULL ){
        if( side == XD_LEFT ){
            if( diff->lnuml == line ){
                break;
            }
        }else{
            if( diff->lnumr == line ){
                break;
            }
        }
        diff = diff->next;
    }

    if( diff != NULL ){
        appData->diffs.current = diff;
        xdMoveToCurrentLine( appData );
    }
}

/****************************************************************************/

void xdFindMatch (

/*
 *  Move the pointer to the next or previous match.
 */

ApplicationData *appData,           /*< All application Data.               */
int direction                       /*< Goto the previous or next diff.     */
)
{ /**************************************************************************/

XDdiff *diff;
int value;
int size;
int max;
int min;
int line;

    value = 1;
    XtVaGetValues( appData->vertScroll,
            XmNvalue,           &value,
            XmNsliderSize,      &size,
            0 );

    if( appData->diffs.current->index >= value &&
            appData->diffs.current->index < (value + size) ){
        diff = appData->diffs.current;
    }else{
        diff = xdFindDiff( &appData->diffs, value + size / 2 );
    }

    if( diff != NULL ){
        diff = (direction == XD_PREV) ? diff->prev : diff->next;
        while( diff != NULL && (diff->flags & XD_MATCHED_MASK) == 0 ){
            diff = (direction == XD_PREV) ? diff->prev : diff->next;
        }
    }

    if( diff == NULL ){
        if( direction == XD_PREV ){
             appData->diffs.current = appData->diffs.first;
        }else{
            appData->diffs.current = appData->diffs.last;
        }
    }else{
        appData->diffs.current = diff;
    }

    xdMoveToCurrentLine( appData );
}

/****************************************************************************/

void xdFindDifference (

/*
 *  Move the pointer to the next or previous difference.
 */

ApplicationData *appData,           /*< All application Data.               */
Boolean unselected,                 /*< Look for unselected regions.        */
int direction                       /*< Goto the previous or next diff.     */
)
{ /**************************************************************************/

XDdiff *diff;
int value;
int size;
int max;
int min;
int line;

    value = 1;
    XtVaGetValues( appData->vertScroll,
            XmNvalue,           &value,
            XmNsliderSize,      &size,
            0 );

    if( appData->diffs.current->index >= value &&
            appData->diffs.current->index < (value + size) ){
        diff = appData->diffs.current;
    }else{
        diff = xdFindDiff( &appData->diffs, value + size / 2 );
    }
    if( diff != NULL ){
        if( diff->type != XD_COMMONTEXT ){
            while( diff != NULL && diff->type == appData->diffs.current->type ){
                diff = (direction == XD_PREV) ? diff->prev : diff->next;
            }
        }
        while( diff != NULL ){
            if( diff->type != XD_COMMONTEXT ){
                if( unselected == True ){
                    if( diff->selection == XD_SELECTNONE ){
                        break;
                    }
                }else{
                    break;
                }
            }
            diff = (direction == XD_PREV) ? diff->prev : diff->next;
        }
    }

    if( diff == NULL ){
        if( direction == XD_PREV ){
             appData->diffs.current = appData->diffs.first;
        }else{
            appData->diffs.current = appData->diffs.last;
        }
    }else{
        appData->diffs.current = diff;
    }

    xdMoveToCurrentLine( appData );
}

/****************************************************************************/

void xdSelectGlobal (

/*
 *  Make the given selection to all differences in the list.
 */

ApplicationData *appData,           /*< All application Data.               */
int selection                       /*< Select the right, left or neither.  */
)
{ /**************************************************************************/

XDdiff *diff;

    for( diff = appData->diffs.first; diff != NULL; diff = diff->next ){
        if( diff->type != XD_COMMONTEXT ){
            diff->selection = selection;
        }
    }

    xdDrawText( appData, XD_LEFT );
    xdDrawText( appData, XD_RIGHT );
    xdDrawView( appData );

    xdSetMessageLine( appData, NULL );
}

/****************************************************************************/

void xdSelectRegion (

/*
 *  Make the given selection to all differences in the region.
 */

ApplicationData *appData,           /*< All application Data.               */
int selection                       /*< Select the right, left or neither.  */
)
{ /**************************************************************************/

XDdiff *diff;

    diff = appData->diffs.current;

    if( diff != NULL && diff->type != XD_COMMONTEXT ){

        do{
            diff->selection = selection;
            diff = diff->prev;
        }while( diff != NULL && diff->type == appData->diffs.current->type );

        diff = appData->diffs.current;

        do{
             diff->selection = selection;
            diff = diff->next;
        }while( diff != NULL && diff->type == appData->diffs.current->type );

    }

    xdDrawText( appData, XD_LEFT );
    xdDrawText( appData, XD_RIGHT );
    xdDrawView( appData );

    xdSetMessageLine( appData, NULL );
}

/****************************************************************************/

void xdSelectLine (

/*
 *  Make the given selection for the current line.
 */

ApplicationData *appData,           /*< All application Data.               */
int selection                       /*< Select the right, left or neither.  */
)
{ /**************************************************************************/

XDdiff *diff;

    diff = appData->diffs.current;

    if( diff != NULL && diff->type != XD_COMMONTEXT ){
        diff->selection = selection;
    }

    xdDrawText( appData, XD_LEFT );
    xdDrawText( appData, XD_RIGHT );
    xdDrawView( appData );

    xdSetMessageLine( appData, NULL );
}

/****************************************************************************/

int xdSetCurrentDifference (

/*
 *  Return the line number of the difference at the y coordinate given.
 */

ApplicationData *appData,           /*< All application Data.               */
int eventY                          /*< Y cordinate of a button event.      */
)
{ /**************************************************************************/

XDdiff *diff;
int topLine;
int line;
int lineHeight;

    topLine = 1;
    XtVaGetValues( appData->vertScroll, XmNvalue, &topLine, 0 );
    lineHeight = appData->maxBounds.ascent + appData->maxBounds.descent;
    line = topLine + (eventY / lineHeight);
    if( line <= appData->diffs.lines ){
        diff = xdFindDiff( &appData->diffs, line );
        appData->diffs.current = diff;
     }else{
        line = -1;
    }

    return line;
}

/****************************************************************************/

Boolean xdCheckAllSelected (

/*
 *  If any difference does not have a selection then a message shown on the
 *  message line. Return whether or not all differences have a selection.
 */

ApplicationData *appData
)
{ /**************************************************************************/

int allSelected;
XDdiff *unselected;

    unselected = xdAnyUnselected( &appData->diffs );
    if( unselected == NULL ){
        allSelected = True;
    }else{
        appData->diffs.current = unselected;

        xdMoveToCurrentLine( appData );

	XBell(appData->display, 100);

        xdSetMessageLine( appData, xdGetDefault( appData->display,
                appData->appClass, "msgNoSelection" ) );

        allSelected = False;
    }

    return allSelected;
}

/****************************************************************************/

int xdSaveFile (

/*
 *  Write the differences to the specified file.
 */

ApplicationData *appData,       /*< All application data.                   */
char *fileName,                 /*< Write to this file.                     */
int side                        /*< Which side to take for common text.     */
)
{ /**************************************************************************/

int status;
char text[ 1000 ];
char *format;

    status = xdWriteFile( fileName, &appData->diffs, side );
    if( status == EOF ){
        format = xdGetDefault(
                appData->display, appData->appClass, "msgWriteFailed" );
    }else{
        format = xdGetDefault(
                appData->display, appData->appClass, "msgWriteSucceeded" );
    }

    sprintf( text, format, (fileName == NULL) ? "<stdout>" : fileName );

    xdSetMessageLine( appData, text );

    return status;
}

/****************************************************************************/

Boolean xdSetCursorOnScreen (

/*
 *  If the cursor line is not within the range of the view then move it
 *  so that it is. Return True if the cursor was moved.
 */

ApplicationData *appData        /*< All application data.                   */
)
{ /**************************************************************************/

XDdiff *diff;
Boolean movedCursor;
int index;
int topLine;
int size;

    movedCursor = False;
    if( appData->diffs.current != NULL ){
        XtVaGetValues( appData->vertScroll,
                XmNvalue,               &topLine,
                XmNsliderSize,          &size,
                0 );

        index = appData->diffs.current->index;
        diff = NULL;
        if( index < topLine ){
            diff = xdFindDiff( &appData->diffs, topLine );
            movedCursor = True;
        }else if( index >= topLine + size ){
            diff = xdFindDiff( &appData->diffs, topLine + size - 1 );
            movedCursor = True;
        }

        if( diff != NULL ){
            appData->diffs.current = diff;
        }
    }

    return movedCursor;
}

/****************************************************************************/

void xdMoveCursor (

/*
 *  Move the cursor by the specified number of lines. Stop if you get
 *  to the end.
 */

ApplicationData *appData,       /*< All application data.                   */
int lines                       /*< How many line to move.                  */
)
{ /**************************************************************************/

XDdiff *diff;

    if( appData->diffs.current != NULL ){
        diff = appData->diffs.current;
        while( lines != 0 ){
            if( lines > 0 ){
                --lines;
                if( diff->next != NULL ){
                    diff = diff->next;
                }
            }else{
                ++lines;
                if( diff->prev != NULL ){
                    diff = diff->prev;
                }
            }
        }

        appData->diffs.current = diff;

        xdMoveToCurrentLine( appData );
        xdDrawView( appData );
        xdDrawText( appData, XD_LEFT );
        xdDrawText( appData, XD_RIGHT );
        xdSetMessageLine( appData, NULL );
    }
}
