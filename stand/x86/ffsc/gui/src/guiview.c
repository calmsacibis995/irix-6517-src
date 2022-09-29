/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1993            */
/*********************************************/
/* FILE:    guiview.c                        */
/* PURPOSE: contains view window support     */
/*********************************************/
/*  %kw # %v    %n    %d    %t #             */
/* Version # 2    GUIVIEW.C    12-Nov-93    12:39:36 # */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"


void DoView( void );
void ScrollView( int );
void DrawTextLine( int linenum, int linecnt );
void DrawItemList( char *name, int );
int  DoItemList( void );
int  CheckItemList( rect *itemR, int x, int y );

void DrawGUIString( char * );

void ViewFile( char *fileName, char *wname )
/*
View a text file in a scrollable view window
*/
{
   char *buf;
   dirRec dirInfo;
   FILE *fp;
   

   fp = fopen( fileName, "r" );
   if( fp == NULL ) {
      Dialog( diaAlert, "Can't open file:", fileName );
      return;
   }
   
   /* find file size */
   FileQuery( fileName, &dirInfo, 1 );

   buf = (char *)calloc( 1, (size_t)(dirInfo.fileSize + 10) );
   if( buf == NULL ) {
      Dialog( diaAlert, "Not enough memory!", NULL );
      return;
   }

   /* read translated, so DOS newlines come back = \n */
   fread( buf, (size_t) dirInfo.fileSize, 1, fp );

   fclose( fp );

   /* view as buffer of null terminated strings */
   ViewBuffer( buf, wname );

   free( buf );

   return;

}


void ViewBuffer( char *buffer, char *wname )
/*
View a text buffer in a scrollable view window
*/
{
   char *p;
   int i;
   fontRcd *userfnt;

   viewBuf = buffer;

   /* switch to system font while drawing view box */
   userfnt = scrnPort->txFont;
   SetFont( sysFont );
   DrawView( wname );
   SetFont( userfnt );

   /* count lines of text */
   p = viewBuf;
   i = 0;
   while( True ) {

      if( *p == 0 || *p == '\n' ) {
         i++;  /* count line */
         p++;  /* skip null */
         if( *p == 0 ) /* double null ? */
            break;
      } else
         p++;

   }

   /* set scroll bar limit to number of text lines (0..n) */
   viewSBar.maxval = i - 1;

   /* process events */
   DoView();

   /* close window */
   CloseWindow( &viewDlgBox );
/*   RegenDesk(); */

}


void DrawView( char *name )
/*
Draw a view window;
A view window is a GUI window with scroll bar
*/
{
   int a,b,c,line;
   grafPort *wPort;
   
   line = scrnPort->txFont->lnSpacing;

   /* size a bounding box */
   DupRect( &scrnPort->portRect, &viewDlgBox.BoundRect );
   InsetRect( &viewDlgBox.BoundRect, picX( 100 ), picY(100) );

   GUIWindow( &viewDlgBox, name );

   /* chuck the window port - we'll just be global */
   GetPort( &wPort );
   SetPort( scrnPort );
   free( wPort );

   ProtectRect( &viewDlgBox.BoundRect );


   /* frame text window  */
   DupRect( &viewDlgBox.BoundRect, &viewDlgBox.BoxR );
   viewDlgBox.BoxR.Xmin += picX(25);
   viewDlgBox.BoxR.Xmax -= picX(50);
   viewDlgBox.BoxR.Ymin += picY(60);
   a = viewDlgBox.BoxR.Ymin;
   b = viewDlgBox.BoxR.Ymax - picY(25);
   c = (b - a) / line;
   b = (c * line) + a + 3;
   viewDlgBox.BoxR.Ymax  =  b;
   FrameRect( &viewDlgBox.BoxR );

   /* set up scroll bar */
   DupRect( &viewDlgBox.BoxR, &viewSBar.SBRect );
   viewSBar.SBRect.Xmin = viewSBar.SBRect.Xmax + 1;
   viewSBar.SBRect.Xmax += picX( 25 );

   /* work in interior of BoxR */
   InsetRect( &viewDlgBox.BoxR, 1, 1 );

   viewSBar.sbfunc = ScrollView;
   viewSBar.minval = 0;
   viewSBar.curval = 0;
   viewSBar.maxval = 100;
   DrawScrollBar( &viewSBar );


   ProtectRect( NULL );


}


void DoView( )
/*
View until close box pressed.
*/
{
   int x, y;

    /* place the starting page in the box */
    ScrollView( position );


    while( True ) {

      KeyGUIEvent( True );

      /* check close box button or keypress  */
      if( CheckClose( &viewDlgBox ) )
         break;


      /* handle keyboard arrow keys */
      if( opEvent.eventChar == 0 || opEvent.eventChar == 0xE0 ) {


         /* up */
         if( opEvent.eventScan == 72 ) {
            opEvent.eventType = evntRELEASE;
            opEvent.eventX = viewSBar.upbox.Xmin;
            opEvent.eventY = viewSBar.upbox.Ymin;
            /* store a companion mouse up event */
            StoreEvent( &opEvent );
            /* make current event into a press event */
            opEvent.eventType = evntPRESS;
         }

         /* down */
         if( opEvent.eventScan == 80 ) {
            opEvent.eventType = evntRELEASE;
            opEvent.eventX = viewSBar.dnbox.Xmin;
            opEvent.eventY = viewSBar.dnbox.Ymin;
            /* store a companion mouse up event */
            StoreEvent( &opEvent );
            /* make current event into a press event */
            opEvent.eventType = evntPRESS;
         }

      }

      if( opEvent.eventType != evntPRESS )
         continue;

      x = opEvent.eventX;
      y = opEvent.eventY;

      /* check scroll bar */
      if( XYInRect( x, y, &viewSBar.SBRect ) ) 
         DoScrollBar( &viewSBar );

   }   /* end of while True */

}



void ScrollView( int code )
/*
Scroll a view window
*/
{
   
   int i, x, y;
   int pageworth, line;
   rect tR, eofbox;


   line = scrnPort->txFont->lnSpacing;
   pageworth = (viewDlgBox.BoxR.Ymax - viewDlgBox.BoxR.Ymin) / line;
   
   /* starting text position in box */
/*   x = viewDlgBox.BoxR.Xmin + 1; */
   x = viewDlgBox.BoxR.Xmin + (viewDlgBox.BoxR.Xmax - viewDlgBox.BoxR.Xmin)/2;
   y = viewDlgBox.BoxR.Ymin + line;

/*   x = (x+7) & 0xFFF8; */

   DupRect( &viewDlgBox.BoxR, &tR );

   /* adjust curval by correct amount */
   switch( code ) {
      
      case up:
         if( viewSBar.curval > viewSBar.minval )
            viewSBar.curval--;
         else
            return;
         break;

      case down:
         if( viewSBar.curval < viewSBar.maxval )
            viewSBar.curval++;
         else
            return;
         break;

      case pgup:
         viewSBar.curval -= pageworth - 1;
         if( viewSBar.curval < viewSBar.minval )
            viewSBar.curval = viewSBar.minval;
         break;

      case pgdown:
         viewSBar.curval += pageworth - 1;
         if( viewSBar.curval > viewSBar.maxval )
            viewSBar.curval = viewSBar.maxval;
         break;

      case position:
         break;

      default:
         break;

   }   /* end of switch */


   /* re draw the box */
   ProtectRect( &viewDlgBox.BoxR );
   ClipRect( &tR );

/*   BackColor( White ); */
   BackColor( 15 );

   PenColor( colors->text );
   RasterOp( zREPz );
/*   TextAlign( alignLeft, alignBottom ); */
   TextAlign( alignCenter, alignBottom );
   TextFace( cProportional );

   switch( code ) {

      case down:
         ScrollRect( &tR, 0, -line );
         i = pageworth - 1;
         MoveTo( x, y + i * line );
         DrawTextLine( viewSBar.curval + i, 1 );
         break;

      case up:
         ScrollRect( &tR, 0, line );
         MoveTo( x, y );
         DrawTextLine( viewSBar.curval, 1 );
         break;

      default:
         /* re-draw whole thing */
         i = viewSBar.curval;
         MoveTo( x, y );
         DrawTextLine( i, pageworth );
         break;

   }   /* end of switch */

   /* fill eof part of display */
   if ( (viewSBar.curval + pageworth) > viewSBar.maxval ) {
      DupRect( &viewDlgBox.BoxR, &eofbox );
      i = viewSBar.maxval - viewSBar.curval;
      eofbox.Ymin = y + line*i + line/2;
      if ( eofbox.Ymax > eofbox.Ymin )
          EraseRect( &eofbox );
          eofbox.Ymax = eofbox.Ymin;
          eofbox.Ymin -= (line/2)-1;
          EraseRect( &eofbox );
    }

    ClipRect( &scrnPort->portRect );
    ProtectRect( NULL );


}


void DrawTextLine( int linenum, int linecnt )
{
   int i, hasEscapes;
   int x,y;
   char *p, *t;
   int lineHeight;
   rect eR;


   /* check that it's not out of bounds */
   if( linenum > viewSBar.maxval )
      return;

   QueryPosn( &x, &y );
   lineHeight = scrnPort->txFont->lnSpacing;

   /* check that the count is not out of bounds */
   if( (linenum + linecnt) > (viewSBar.maxval + 1) )
      linecnt = (viewSBar.maxval - linenum) + 1;

   /* find line in question */
   p = viewBuf;
   for( i = 0; i < linenum; i++ ) {

      while( *p && *p != '\n' )
         p++;

      p++;
   }


   /* draw the lines */
   PenColor( colors->text );
   for( i = 0; i < linecnt; i++ ) {

      MoveTo( x, y );

      /* find the end of the line in buffer */
      hasEscapes = 0;
      t = p;
      while( *p && *p != '\n' ) {
         if( *p == 0x1B )
            hasEscapes = True;
         p++;
      }
         
      /* draw the string */
      if( *p == '\n' ) {
         *p = 0;
         if( hasEscapes )
            DrawGUIString( t );
         else
            DrawString( t );
         *p = '\n';
      } else
         if( hasEscapes )
            DrawGUIString( t );
         else
            DrawString( t );

      /* erase from end of string to end of text window */
      SetRect( &eR, scrnPort->pnLoc.X, (y-lineHeight)+1,
                    scrnPort->portRect.Xmax, y+1 );
      EraseRect( &eR );

      p++;

      y += lineHeight;

   }
   
}

char * ViewItemList( char *buffer, char *wname )
/*
View a text buffer in a scrollable view window
Returns pointer to selected string in buffer or NULL if none.
*/
{
   extern int grfovride;
   extern int grfovride_sav;
   char *p;
   int i, selection;
   int width, maxWidth, title = 1;
   char *space= "                                       ";

   viewBuf = buffer;

   /* Save the state of the graphic over-ride register and
    * Freeze the graphics update
   */
   grfovride_sav = grfovride;
   grfovride = 1;

   

/*   DrawItemList( wname ); */

   maxWidth = strlen(wname);

   /* count lines of text */
   p = viewBuf;
   width = strlen(p);
   if (width > maxWidth) {
     maxWidth = width;
     title = 0;
   }
   i = 0;
   while( *p ) {

      if( *++p == 0 ) {
         i++;    /* count line */
         p++;    /* skip null */
	 width = strlen(p);
	 if (width > maxWidth) {
	   maxWidth = width;	 
	   title = 0;
	 }
      }

   }
   if (title == 0)
     maxWidth += 4;
   space[maxWidth] = '\0';
   DrawItemList( wname,StringWidth(space) );
   space[maxWidth] = ' ';

   /* set scroll bar limit to number of text lines (0..n) */
   viewSBar.maxval = i - 1;

   /* process events */
   selection = DoItemList();

   /* close window */
   CloseWindow( &viewDlgBox );

	/* Restore the state of the graphic over-ride register */
	grfovride = grfovride_sav;

/*   RegenDesk(); */

   /* find selected string if any */

   if( selection < 0 ) 
      return NULL;

   p = viewBuf;

   if( selection > 0 ) {

      i = 0;
      while( *p ) {

         if( *++p == 0 ) {
            i++;    /* count line */
            p++;    /* skip null */
            if( i == selection )
               break;
         }
      }
   }

   return p;


}


void DrawItemList( char *name, int strwidth )
/*
Draw an item list window, which is a modified view window;
it also contains an OK button. 
*/
{
   int a,b,c,line,offsetX;
   grafPort *wPort;
   
   line = scrnPort->txFont->lnSpacing;

   /* size a bounding box */
   DupRect( &scrnPort->portRect, &viewDlgBox.BoundRect );
/*   InsetRect( &viewDlgBox.BoundRect, picX( 200 ), picY(200) ); */

   offsetX = (viewDlgBox.BoundRect.Xmax -
	      viewDlgBox.BoundRect.Xmin - strwidth)/2 - 25;
   InsetRect( &viewDlgBox.BoundRect, offsetX, picY(200) );

   GUIWindow( &viewDlgBox, name );

   /* chuck the window port - we'll just be global */
   GetPort( &wPort );
   SetPort( scrnPort );
   free( wPort );

   ProtectRect( &viewDlgBox.BoundRect );


   /* frame text window  */
   DupRect( &viewDlgBox.BoundRect, &viewDlgBox.BoxR );
   viewDlgBox.BoxR.Xmin += picX(25);
   viewDlgBox.BoxR.Xmax -= picX(50);
   viewDlgBox.BoxR.Ymin += picY(60);
   a = viewDlgBox.BoxR.Ymin;
   b = viewDlgBox.BoxR.Ymax - picY(25);
   c = (b - a) / line;
   viewDlgBox.BoxR.Ymax  =  (c * line) + a + 3;
   FrameRect( &viewDlgBox.BoxR );

   /* set up scroll bar */
   DupRect( &viewDlgBox.BoxR, &viewSBar.SBRect );
   viewSBar.SBRect.Xmin = viewSBar.SBRect.Xmax - 1;           /* was  + 1; */
   viewSBar.SBRect.Xmax += picX( 25 );

   /* work in interior of BoxR */
   InsetRect( &viewDlgBox.BoxR, 1, 1 );

   viewSBar.sbfunc = ScrollView;
   viewSBar.minval = 0;
   viewSBar.curval = 0;
   viewSBar.maxval = 100;
   DrawScrollBar( &viewSBar );


   /* place OK button */
/*
   SetRect(&viewDlgBox.OkR,
            viewDlgBox.BoxR.Xmin,
            viewDlgBox.BoundRect.Ymax - picY(75),
            viewDlgBox.BoxR.Xmin + picX(150),
            viewDlgBox.BoundRect.Ymax - picY(20) );

   DrawButton( &viewDlgBox.OkR, "Ok" );
*/
   ProtectRect( NULL );


}


int DoItemList( )
/*
View until close box pressed, or OK pressed.
Allow items to be selected with the mouse.
Returns index of selected item or -1 if none.
*/
{
   int x, y, i, selectedItem;
   rect hiliteR;
   unsigned int clickTime;

   /* place the starting page in the box */
   ScrollView( position );

   /* start off with no item highlighted */
    hiliteR.Xmin =  -1;
   
   /* start off with first item selected (highlighted) */
   x = viewDlgBox.BoxR.Xmin;
   y = viewDlgBox.BoxR.Ymin;
   selectedItem = CheckItemList( &hiliteR, x, y );


   /* initialize last click time */
   clickTime = (unsigned) opEvent.eventTime + dblClick;

    while( True ) {

      KeyGUIEvent( True );

      /* check close box button or Escape key  */
/*      if( CheckClose( &viewDlgBox ) ) { */
      if ( opEvent.eventChar == 0x1B ) {
         selectedItem = -1;
         break;
      }

      /* return key = ok button */
      if( opEvent.eventChar == '\r' ) {
/*         OnButton( &viewDlgBox.OkR ); */
         break;
      }

      /* handle keyboard arrow keys */
      if( opEvent.eventChar == 0 || opEvent.eventChar == 0xE0 ) {


         /* up */
         if( opEvent.eventScan == 72 ) {
            opEvent.eventType = evntRELEASE;
            opEvent.eventX = viewSBar.upbox.Xmin;
            opEvent.eventY = viewSBar.upbox.Ymin;
            /* store a companion mouse up event */
            StoreEvent( &opEvent );
            /* make current event into a press event */
            opEvent.eventType = evntPRESS;
         }

         /* down */
         if( opEvent.eventScan == 80 ) {
            opEvent.eventType = evntRELEASE;
            opEvent.eventX = viewSBar.dnbox.Xmin;
            opEvent.eventY = viewSBar.dnbox.Ymin;
            /* store a companion mouse up event */
            StoreEvent( &opEvent );
            /* make current event into a press event */
            opEvent.eventType = evntPRESS;
         }

      }

      if( opEvent.eventType != evntPRESS )
         continue;

      x = opEvent.eventX;
      y = opEvent.eventY;

      /* check ok button */
/*      if( XYInRect( x, y, &viewDlgBox.OkR ) ) {
         OnButton( &viewDlgBox.OkR );
         break;
      }
*/
      /* check scroll bar */
      if( XYInRect( x, y, &viewSBar.SBRect ) ) {

         /* unselect item */
         ProtectRect( &hiliteR );
         InvertRect( &hiliteR );
         hiliteR.Xmin =  -1;
         ProtectRect( NULL );

         DoScrollBar( &viewSBar );

         /* reselect first item in list */
         x = viewDlgBox.BoxR.Xmin;
         y = viewDlgBox.BoxR.Ymin;
         selectedItem = CheckItemList( &hiliteR, x, y );

         continue;

      }


      /* check selection area */
      if( XYInRect( x, y, &viewDlgBox.BoxR ) ) {

         i = CheckItemList( &hiliteR, x,y );

         /* did they pick something ? */
         if( i != -1 ) {
      
            /* double click on same item ? */
            if( i == selectedItem  &&
                ( (unsigned) opEvent.eventTime - clickTime ) < dblClick ) {
/*               OnButton( &viewDlgBox.OkR ); */
               break;

            }

            /* reset double click time */
            clickTime = (unsigned) opEvent.eventTime;

            selectedItem = i;

         } /* end of picked something */

      } /* end of in selection area */


   }   /* end of while True */

   return  selectedItem;
}

int CheckItemList( rect *itemR, int x, int y )
/*
Figure out which line is selected by the x,y.
Returns the index of the selected item.
Returns -1 if no valid item selected.
Sets itemR to current highlighted item rectangle, or Xmin = -1  if none.
*/

{
   
   int i, charheight, pageworth, numleft;
   rect tR;

   DupRect( &viewDlgBox.BoxR, &tR );

   ProtectRect( &tR );

   /* re-invert the old selection if any */
   if( itemR->Xmin != -1 ) 
      InvertRect( itemR );

   /* find the new selection */
   charheight = scrnPort->txFont->lnSpacing;
   pageworth = (tR.Ymax - tR.Ymin) / charheight;
   numleft =  viewSBar.maxval - viewSBar.curval + 1;
   if( numleft < pageworth )
      pageworth = numleft;

   /* make a small rect one line deep */
   tR.Ymax = tR.Ymin + charheight;

   /* search for the line selected */

   for( i = 0; i < pageworth; i++ ) {
      if( XYInRect( x, y, &tR ) )
         break;
      OffsetRect( &tR, 0, charheight );
   }
   if( i >= pageworth )  {
      /* outside of selectable items */
      itemR->Xmin = -1;
      ProtectRect(NULL);
      return -1;
   }

   /* save this as last item inverted */
   DupRect( &tR, itemR );

   InvertRect( itemR );

   ProtectRect(NULL);

   return (viewSBar.curval + i);

}



int GUIWindow( dialogBox *dia, char *name )
/*
Pops up a window with a title bar and close box.
A port is allocated and made active for the client area of the window.

*/
{
   rect tR;
   grafPort *wPort;
   pusharea savebuf;

   /* pop up a window  */
   OpenWindow( dia );

   DupRect( &(dia->BoundRect), &tR );

   ProtectRect( &tR );

   RasterOp( zREPz );
   BackColor( colors->bar );
   EraseRect( &tR );

   /* make a title bar */
   tR.Ymax = tR.Ymin + scrnPort->txFont->lnSpacing;       /*was + picY( 35 );*/
   PenColor( colors->low );
   MoveTo( tR.Xmin, tR.Ymax );   LineTo( tR.Xmax-1, tR.Ymax );
   PenColor( colors->high );
   MoveTo( tR.Xmin, tR.Ymax+1 ); LineTo( tR.Xmax-1, tR.Ymax+1 );

   TextAlign( alignCenter, alignMiddle );
   TextFace( cProportional );
   MoveTo( ( tR.Xmax + tR.Xmin ) / 2, ( tR.Ymax + tR.Ymin + 2 ) / 2 );
   PenColor( colors->text );
   DrawString( name );
   OnBevel( &(dia->BoundRect) );

   /* make a close box */
/*
   tR.Xmax = tR.Xmin + picX( 30 );
   InsetRect( &tR, 4, 4 );
   OffsetRect( &tR, 2, 2 );
   PenColor( colors->high );
   MoveTo( tR.Xmin, tR.Ymax);
   LineTo( tR.Xmin, tR.Ymin);  LineTo( tR.Xmax, tR.Ymin);
   PenColor( colors->low );
   LineTo( tR.Xmax, tR.Ymax);  LineTo( tR.Xmin, tR.Ymax);
   DupRect( &tR, &dia->CanR );

   InsetRect( &tR, 3, 3 );
   PenColor( colors->high );
   MoveTo( tR.Xmin, tR.Ymax);
   LineTo( tR.Xmin, tR.Ymin);  LineTo( tR.Xmax, tR.Ymin);
   PenColor( colors->low );
   LineTo( tR.Xmax, tR.Ymax);  LineTo( tR.Xmin, tR.Ymax);
*/
   PenColor( Black );
   


   /* size the client area */
   DupRect( &(dia->BoundRect), &tR );
   tR.Ymin += picY( 30 );
   InsetRect( &tR, 4, 4 );
   DupRect( &tR, &dia->BoxR );

   /* convert to global for PortSize and MovePortTo */
   Lcl2GblRect( &tR );

   /* build them a port */
   wPort = ( grafPort * ) malloc( sizeof( grafPort ) );
   if( wPort == NULL )
      return False;

   /* copy current port values to new port */
   PushGrafix( &savebuf );
   InitPort( wPort );
   PopGrafix( &savebuf );

   MovePortTo( tR.Xmin, tR.Ymin );
   PortSize( tR.Xmax - tR.Xmin, tR.Ymax - tR.Ymin );
   ClipRect( &(wPort->portRect) );   
/*   BackColor( White ); */
   BackColor( 15 );
   EraseRect( &(wPort->portRect) );

   ProtectRect( NULL );

   return True;

}

int CheckClose( dialogBox *dia )
/*
Checks current event for GUI window close box hit
Returns True if it was.
Dialog cancel rect is assumed to be in global coordinates.
*/
{
   point closePt;
   grafPort *curPort;

   /* exit on a ESC too */
   if( opEvent.eventChar == 0x1B )
      return True;

   /* mouse down ? */
   if( opEvent.eventType == evntPRESS )   {

      /* check close button */
      closePt.X = opEvent.eventX;
      closePt.Y = opEvent.eventY;
      
      /* convert event coords from whatever to global */
      GetPort( &curPort );
      if( curPort->portFlags & pfVirtual )
         Vir2LclPt( &closePt );
      if( !(curPort->portFlags & pfGblCoord) )
         Lcl2GblPt( &closePt );

      if( PtInRect( &closePt, &dia->CanR, 1,1 ) ) {
         return True;
      }
   }

   return False;

}


void DrawGUIString( char *s )
/* draw the text, handle escape sequences for text control */
{
   char *p, buf[100];
   grafPort *thePort;
   int color, face, defaultColor, defaultFace;

   GetPort( &thePort );
   color = defaultColor = (int) thePort->pnColor;
   face = defaultFace = thePort->txFace;

   p = buf;
   while( *s ) {

      if( *s == 0x1B ) {

         /* skip the escape */
         s++;

         /* flush out temp string */
         if( p != buf ) {
            *p = 0;
            DrawString( buf );
            p = buf;
         }

         /* handle the escape code */

         switch( *s++ ) {
            case 'B': face += cBold;      break;
            case 'I': face += cItalic;    break;
            case 'H': face += cHalftone;  break;
            case 'U': face += cUnderline; break;
            case 'S': face += cStrikeout; break;
            case 'C':
                  color = *s++ - '0';
                  if( color > 9 )
                     color -= 7;
                  break;
            case 'D': face = defaultFace; color = defaultColor; break;
         }

         TextFace( face );
         PenColor( color );
      }
      else 
         *p++ = *s++;

   }
   
   /* flush out any left */
   if( p != buf ) {
     *p = 0;
     DrawString( buf );
   }

   TextFace( defaultFace );
   PenColor( defaultColor );

}



