/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1991            */
/*********************************************/
/* FILE:    guidilg.c                        */
/* PURPOSE: module contains dialog box       */
/*          routines, this includes scroll   */
/*          bar and text editing functions   */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 4    GUIDILG.C    10-Nov-93    15:22:14 # */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"

void SimpleDialogButton( rect *bR, int code ) {

  rect tR;
   
  ProtectRect( bR );
  DupRect( bR, &tR );
  InsetRect( &tR, -3, -3 );
  if ( code == True )
    PenColor( Black );
  else if ( code == False )
    PenColor (White);
  else
    return;                        /* only call with True or False code */
  PenSize(3,3);
  FrameRect( &tR );
  PenNormal();
}

void InitDialog()
/* 
Initializes the dialog box rectangles for subsequent
dialog processing. 
*/
{
   
   point DiaCtr;
   int DiaLf;   
   int DiaTop;  
   int DiaRt;   
   int DiaBot;  

   /* start unique file numer counter at 10 */
   fileNumber = 10;

   /* center dialog box on screen */
   DiaCtr.X = scrnPort->portRect.Xmax / 2;
   DiaCtr.Y = scrnPort->portRect.Ymax / 2;

   /* set-up normal dialog boxes */

   /* compute edges if 4.50 x 2.00 inches */
   DiaLf = DiaCtr.X - picX(225);
   DiaRt = DiaCtr.X + picX(225);
   DiaTop = DiaCtr.Y - picY(100);
   DiaBot = DiaCtr.Y + picY(100);
   SetRect(&mscDlgBox.BoundRect, DiaLf,DiaTop,DiaRt,DiaBot );

   /* place cancel button */
   SetRect(&mscDlgBox.CanR, DiaRt - picX(50), DiaBot - picY(75),
                      DiaRt - picX(150), DiaBot - picY(30) );

   /* place OK button */
   SetRect(&mscDlgBox.OkR,  DiaLf + picX(50), DiaBot - picY(75),
                      DiaLf + picX(150), DiaBot - picY(30) );

   /* place icon box */
   SetRect(&mscDlgBox.BoxR,  DiaRt - picX(175), DiaBot - picY(135),
                      DiaRt - picX(75), DiaBot - picY(35) );

   /* place key rect */
   SetRect(&mscDlgBox.KeyR, DiaLf + picX(25), DiaCtr.Y - picY(30),
                      DiaRt - picX(25), DiaCtr.Y + picY(10) );

   /* place text rect */
   SetRect(&mscDlgBox.TxR,  DiaLf + picX(25), DiaTop + picY(25),
                      DiaRt - picX(25), DiaTop + picY(60) );


}

int Dialog( int diaType, char *TxtStr1, char *TxtStr2)
/*
Function Dialog displays TxtStr1 and 2 as a dialog message, 
and "Cancel" and "Ok" buttons on the screen.

    diaType =  
    diaNull     displays only the dialog message and buttons.
    diaAlert    displays an alert box with only an ok button
    diaChr      indicates that the operator may return a
                character string in RtnStr.
    diaNum      indicates that the operator may return an
                unsigned numeric string (only) in RtnStr.
    diaSNum     indicates that the operator may return a
                signed numeric string (only) in RtnStr.
    diaHex      indicates that the operator may return a
                hexidecimal string (only) in RtnStr.
    diaKey      indicates that the operator may return a
                single ASCII char at RtnStr.
    diaBox      displays only the dialog message, must be removed
                with HideDialog.


  Returns a status of "Ok" (1) if the Ok button is selected.
  Returns a status of "Cancel" (0) if the Cancel button is selected.
*/

{
   
   dialogBox *dia;
   int   x,y, eventType, hiliteButton;
   rect tR;

   dia = &mscDlgBox;

   /* pop up the box */
   ShowDialog( diaType, dia );

   /* draw the text string */
   PenColor( colors->text );
   BackColor( White );
   RasterOp(zREPz);
   TextAlign( alignCenter, alignMiddle );
   TextFace( cProportional );

   /* center it */
   x = (dia->TxR.Xmax + dia->TxR.Xmin + 1) / 2;
   y = (dia->TxR.Ymax + dia->TxR.Ymin + 1) / 2;
   MoveTo(x,y);
   ProtectRect(&dia->BoundRect);
   DrawString(TxtStr1);
   ProtectRect(NULL);

   /* if just wanted a box, exit */
   if( diaType == diaBox )
      return Ok;

   if( diaType == diaAlert || diaType == diaNull ) {
     
     /* print the second string if any */
     if( TxtStr2 != NULL ) {
       x = (dia->KeyR.Xmax + dia->KeyR.Xmin + 1) / 2;
       y = (dia->KeyR.Ymax + dia->KeyR.Ymin + 1) / 2;
       MoveTo(x,y);
       ProtectRect(&dia->BoundRect);
       DrawString(TxtStr2);
       ProtectRect(NULL);
     }

     /* Turn OK Button On (default) */
     SimpleDialogButton( &dia->OkR, True );
     hiliteButton = 1;
   }
   
   while( True ) {

      if( diaType == diaAlert || diaType == diaNull ) {

         /* get an event */
         KeyGUIEvent(True);
       }
/*      else */
         /* get a text string */
/*         DialogText( diaType, dia, RtnStr );
*/               
      /* act on the event */
      eventType = opEvent.eventType;
      x = opEvent.eventX; y = opEvent.eventY;

      /* left/right arrow keys toggle selected button */

      if ( opEvent.eventChar == 0 && (opEvent.eventScan == 75 ||
				      opEvent.eventScan == 77)) {
	if (hiliteButton == Ok) {
	  DupRect( &dia->OkR, &tR);
	  SimpleDialogButton( &tR, False );
	  DupRect( &dia->CanR, &tR);
	  SimpleDialogButton( &dia->CanR, True );
	  hiliteButton = Cancel;
	}
	else {                               /* hiliteButton == Cancel */
	  DupRect( &dia->CanR, &tR);
	  SimpleDialogButton( &dia->CanR, False );
	  DupRect( &dia->OkR, &tR);
	  SimpleDialogButton( &dia->OkR, True );
	  hiliteButton = Ok;
	}
	continue;
      }


      /* if a return key pressed, return hilighted button */
      if( eventType == evntKEYDN && opEvent.eventChar == 0x0D ) {
/*         DupRect( &dia->OkR, &tR);
         hiliteButton = Ok; */
         break;
      }

      /* if a ESC key pressed, same as cancel button */
      if( eventType == evntKEYDN && opEvent.eventChar == 0x1B ) {
         DupRect( &dia->CanR, &tR);
         hiliteButton = Cancel;
         break;
      }


      if( eventType != evntPRESS )
         continue;   /* the while( !done ) loop */

      /* which button pressed if any */
      if( diaType != diaAlert && XYInRect( x, y, &dia->CanR ) ) {
         DupRect( &dia->CanR, &tR);
         hiliteButton = Cancel;
         break;
      }
      if( XYInRect( x, y, &dia->OkR) ) {
         DupRect( &dia->OkR, &tR);
         hiliteButton = Ok;
         break;
      }

   }   /* end of while(True) */

   /* animate button */
/*   OnButton( &tR ); */


   /* pop dialog off */
   HideDialog( diaType, dia ); 

   return hiliteButton;
}


void ShowDialog( int diaType, dialogBox *dia )
/* Show "Dialog" Box */
{
   char  *canmsg, *okmsg;
   rect  tR;
   extern int grfovride;
   extern int grfovride_sav;


   canmsg = "Cancel";
   okmsg = "Ok";

   /* Save the state of the graphic over-ride register and
    * Freeze the graphics update
   */
   grfovride_sav = grfovride;
   grfovride = 1;


   /* open a window */
   OpenWindow( dia );

   DupRect( &dia->BoundRect, &tR );

   ProtectRect( &tR );

   /* draw white box with black shadow frame */
   ShadowRect( &tR, 10 );


   PenColor( Black );

   /* draw black borders */
   InsetRect(&tR, picX(15), picY(15) );
   FrameRect(&tR);

   if( diaType == diaBox ) {
      ProtectRect(NULL);
      return;
   }

   /* draw an OK button */
   DrawButton( &dia->OkR, okmsg );

   if( diaType == diaAlert ) {
      /* draw an alert icon */
      PenColor( Black );

   }
   else
      /* draw a cancel button */
      DrawButton( &dia->CanR, canmsg );

   ProtectRect(NULL);

}


void OpenWindow( dialogBox *dia )
/*
Opens a rectangular area on screen, saving the backing area 
Draws nothing.
*/
{
   
   int i, x, y;
   long  imSize;
   rect sR;
   char bufname[15];
   FILE *fp;

   DupRect( &dia->BoundRect, &sR );
   
   /* read backing image to hold buffer */
   /* add shadow size in case shadow is added */
   x = picX( 10 );
   y = picY( 10 );
   sR.Xmax += x;
   sR.Ymax += y;
 
   ProtectRect(&sR);

   /* start with no backing buffer available */
   dia->flags = 0;
   imSize = ImageSize(&sR);
   if( imSize <= 0xFFF0 ) {
      dia->holdImage = malloc( (size_t) imSize );
      if( dia->holdImage ) {
         ReadImage(&sR, dia->holdImage );
         /* indicate buffer available in memory */
         dia->flags = 1;
      }
   }
   
   /* if couldn't get into memory, try disk */
   if( dia->flags == 0 ) {

      sprintf( bufname, "GUI%u.IMG", fileNumber );
      fp = fopen( bufname, "wb" );

      if( fp ) {

         i = sR.Ymax;
         sR.Ymax = sR.Ymin + 1;
         imSize = ImageSize(&sR);
         
         /* write each raster to file */
         for( y = sR.Ymin; y < i; y++ ) {
            sR.Ymin = y;
            sR.Ymax = y+1;
            ReadImage(&sR, rasterBuf );
            fwrite( rasterBuf, (size_t) imSize, 1, fp );
         } /* end of for each Y */
            
         /* indicate buffer available on disk */
         dia->flags = fileNumber++;
         fclose( fp );

      } /* end of fp != 0 */
      
   } /* end of flags = 0 */
   

   ProtectRect(NULL);


}

void HideDialog( int diaType, dialogBox *dia )
/* Remove "Dialog" box */
{
   extern int grfovride;
   extern int grfovride_sav;

   CloseWindow( dia );

	/* Restore the state of the graphic over-ride register */
	grfovride = grfovride_sav;

}

void CloseWindow( dialogBox *dia )
/*
Restores backing image for the window created by OpenWindow
*/
{
   
   int i, y;
   long  imSize;
   char bufname[15];
   FILE *fp;
   rect tR;
   
   /* if no backing image in buffer, re-gen desk top */
   if( dia->flags == 0 ) {
      RegenDesk(); 
      return;
   }

   DupRect( &dia->BoundRect, &tR );

   /* add shadow size */
   tR.Xmax += picX(10);
   tR.Ymax += picY(10);
    
   ProtectRect(&tR);
   RasterOp(zREPz);

   /* image in memory ? */
   if( dia->flags == 1 ) {
      WriteImage(&tR, dia->holdImage);
      free( dia->holdImage );
      ProtectRect(NULL);
      return;
   }
      
   /* image on disk */
   sprintf( bufname, "GUI%u.IMG", dia->flags );
   fp = fopen( bufname, "rb" );

   if( fp ) {
      
      i = tR.Ymax;
      tR.Ymax = tR.Ymin+1;
      imSize = ImageSize(&tR);

      /* write each raster to file */
      for( y = tR.Ymin; y < i; y++ ) {
         tR.Ymin = y;
         tR.Ymax = y+1;
         fread( rasterBuf, (size_t) imSize, 1, fp );
         WriteImage(&tR, rasterBuf );
      } /* end of for each Y */

      fclose( fp );

      /* remove file */
      unlink( bufname );

   } /* end of fp != 0 */

   ProtectRect(NULL);

}

void DrawButton( rect *bR, char *str )
/*
Draw a normal (active) button in rect bR
*/
{
   int x, y;

   ProtectRect( bR );


   /* fill interior */
   BackColor( LtGray );
   EraseRect( bR );


   /* frame without corners */
   PenColor( Black );
   MoveTo( bR->Xmin+1, bR->Ymin );
   LineTo( bR->Xmax-1, bR->Ymin );
   MoveTo( bR->Xmin+1, bR->Ymax );
   LineTo( bR->Xmax-1, bR->Ymax );
   MoveTo( bR->Xmin, bR->Ymin+1 );
   LineTo( bR->Xmin, bR->Ymax-1 );
   MoveTo( bR->Xmax, bR->Ymin+1 );
   LineTo( bR->Xmax, bR->Ymax-1 );

   Button3D( bR );

   PenColor( colors->text );

   /* center text in button */
   TextAlign( alignCenter, alignMiddle );
   x = (bR->Xmax + bR->Xmin + 1 ) / 2;
   y = (bR->Ymax + bR->Ymin + 1 ) / 2;
   MoveTo( x ,y );
   DrawString( str );

   ProtectRect(NULL);

}

void OnButton( rect *bR )
/* 
Depress button while mouse down
*/
{
   /* turn button on */
   DialogButton( bR, True );
   
   if( opEvent.eventType != evntPRESS )
      delay( 5 );
   else {
      /* wait for a mouse up */
      while( True ) {            
         KeyGUIEvent(True);
         if( opEvent.eventType == evntRELEASE )
            break;
      }
   }

   /* turn button off */
   DialogButton( bR, False );

}

void DialogButton( rect *bR, int code )
/*
Modifies existing button  
If code = True button is down, False = button is up, -1 =  button is disabled 
*/
{
   rect tR;
   
   ProtectRect( bR );

   if( code == True ) {
      DupRect( bR, &tR );
      InsetRect( &tR, 1, 1 );
      PenColor( LtGray );
      FrameRect( &tR );
      PenColor( Gray );
      MoveTo( tR.Xmin, tR.Ymax );
      LineTo( tR.Xmin, tR.Ymin );
      LineTo( tR.Xmax, tR.Ymin );
      InsetRect( &tR, 1, 1 );
      PenColor( LtGray );
      FrameRect( &tR );
      InsetRect( &tR, 1, 1 );
      BackColor( LtGray );
      ScrollRect( &tR,1,2);
   }
   

   if( code == False ) {
      DupRect( bR, &tR );
      InsetRect( &tR, 3, 3 );
      BackColor( LtGray );
      ScrollRect( &tR,-1,-2);
      Button3D( bR );

   }
   
   if( code == -1 ) {
      PenColor( Black );
      BackColor( White );
      RasterOp( zORz );
      PenPattern( 3 );
      PaintRect( bR );
      PenPattern( 1 );
      RasterOp( zREPz );
   }

   ProtectRect(NULL);

}


void DlgReDraw( rect *, char *, char *, int * );
void DilgCursor( int, rect * );

int DialogText( int diaType, dialogBox *dia, char *editStr )
/*
Get dialog text until a return, ESC, okR or canR
is pressed. Starts editing contents of editStr if any. Returns input
in editStr. Checks input for valid keys depending on diaType.
editStr MUST be able to hold a MAXLEN string.

Returns ok, cancel, or 0x0D.
*/
{
   
   int eventType, curspos, exitType, curscolor;
   unsigned int curstime;
   char key, *insertptr, *bufend, *src, *dst;
   rect editRect;

   exitType = Cancel;

   DupRect( &dia->KeyR, &editRect );
   PenColor( Black );
   BackColor(White);
   RasterOp( zREPz );
   TextAlign( alignLeft, alignMiddle );

   /* clear and frame input box */
   ProtectRect( &editRect );
   EraseRect( &editRect );
   FrameRect( &editRect );
   ProtectRect(NULL);

   /* set a clip rect */
   InsetRect( &editRect, 2, 1 );
   ClipRect( &editRect );

   /* start with insertion point at end */
   insertptr = editStr + strlen( editStr);

   /* initial cursor outside of box (will be clipped) */
   curspos = editRect.Xmax + 1;
   curscolor = White;
   KeyGUIEvent( False );
   curstime = (unsigned int) opEvent.eventTime - 1;

   /* calculate a pointer to the end of the buffer space */
   bufend = editStr + MAXLEN - 1;
   if( diaType == diaKey )
      bufend = editStr + 2;

   /* draw in starting string if any */
   DlgReDraw( &editRect, editStr, insertptr, &curspos );

   /* get keys until break */
   while( True ) {
   
      /* wait for an event */
      while( !KeyGUIEvent( False ) ) {

         /* toggle cursor */
         if( (unsigned int) opEvent.eventTime > curstime ) {
            curstime += 6;
            curscolor = ~curscolor;
            PenColor( curscolor );
            DilgCursor( curspos, &editRect );
         }
         
      }
            
      eventType = opEvent.eventType;
      key = opEvent.eventChar;

      /* if not a key or mouse down event, forget it */
      if( !(eventType == evntKEYDN || eventType == evntPRESS) )
         continue;

      /* if a mouse down event, are we done ? */
      if( eventType == evntPRESS ) {

         /* if button pressed exit */
         if( XYInRect( opEvent.eventX, opEvent.eventY, &dia->CanR ) ) {
            exitType = Cancel;
            break;
         }
            
         if( XYInRect( opEvent.eventX, opEvent.eventY, &dia->OkR ) ) {
            exitType = Ok;
            break;
         }

         /* not any button we care about */
         continue;

      }

      /* if return key, we are done */
      if( key == 0x0D ) {
         exitType = key;
         break;
      }

      /* if ESC key, cancel */
      if( key == 0x1B ) {
         exitType = Cancel;
         break;
      }

      /* is it backspace or escape ? */
      switch( key ) {

         case 0x08:  /* backspace */
            /* move insert pos left, then act like delete */
            opEvent.eventChar = 0;
            insertptr--;
            if( insertptr < editStr ) {
               insertptr = editStr;
               opEvent.eventScan = 0;   /* nop special key */
               break;
            }
            opEvent.eventScan = 83;
            break;
         
#if 0
         case 0x1B:  /* escape */
            /* clear out the buffer */
            *editStr = '\0';
            insertptr = editStr;

            ProtectRect( &editRect );
            EraseRect( &editRect );
            ProtectRect(NULL);

            /* update cursor position */
            DlgReDraw( &editRect, editStr, insertptr, &curspos );
            continue;
#endif

      }


      /* is it a special key ? */
      if( opEvent.eventChar == 0 || opEvent.eventChar == 0xE0 ) {

         switch( opEvent.eventScan ) {
                       
            case 77:    /* right arrow */
               if( *insertptr == '\0' )
                  break;
               if( insertptr < bufend )
                  insertptr++;
                break;

            case 75:    /* left arrow */
               insertptr--;
               if( insertptr < editStr )
                  insertptr = editStr;
               break;

            case 83:   /* delete     */
               /* move buffer up a char */
               dst = insertptr;
               while( *dst ) {
                  *dst = *(dst + 1);
                   dst++;
               }
               break;
            
            case 71:    /* home */
            case 73:    /* or pgup */
               insertptr = editStr;
               break;

            case 79:    /* end */
            case 81:    /* or pgdn */
               insertptr =  editStr + strlen( editStr );
               break;

            default:
               key = 0;

         }   /* end of switch */

         /* redraw the buffer */
         DlgReDraw( &editRect, editStr, insertptr, &curspos );

         /* update cursor */
         PenColor( Black );
         DilgCursor( curspos, &editRect );

         continue;

      }   /* end of if special key */

      /* test if valid key for this type of dialog */
      switch( diaType ) {
            
         case diaNum:
            if( key < '0' || key > '9' ) 
               key = 0;
            break;

         case diaSNum:
            if( ( key < '0' || key > '9' ) && key != '-' )
               key = 0;
            break;
               
         case diaHex:
            key = toupper(key);
            if( key < '0' || key > 'F' || (key > '9' && key < 'A') ) 
               key = 0;
            break;
                      
         case diaFile:
            switch( key ) {
               case '>':
               case '<':
               case '|':
               case '/':
               key = 0;
               break;
            }
            break;

         case diaKey:
                        
            if( key < 0x20 || key > 0x7E )
               key = 0;
            break;
      }

      /* bad key */
      if( key == 0 ) {
         DoBeep();
         continue;
      }

      /* insert the key in the buffer */
                
      /* move buffer down a char */
      src = insertptr + strlen( insertptr ); /* end of string */
      dst = src + 1;
      if( dst < bufend ) {

         while( dst != insertptr )
            *dst-- = *src--;

         /* insert new char */
         if( insertptr < bufend )
            *insertptr++ = key;
      }

      /* redraw string */
      DlgReDraw( &editRect, editStr, insertptr, &curspos );

      /* update cursor */
      PenColor( Black );
      DilgCursor( curspos, &editRect );


   }   /* end of while( True ) */

   /* redraw to turn off cursor */
   DlgReDraw( &editRect, editStr, insertptr, &curspos );

   /* reset the clip rect */
   ClipRect( &(scrnPort->portRect) );

   return exitType;

}


void DilgCursor( int, rect * );

void DlgReDraw( rect *editRect, char *editStr, char *insertpos, int *curspos )
/*
Redraw the editStr string in the editRect, erase cursor at curspos,
and update curspos.
*/
{
   int x, y, maxchars, charwidth;
   

   /* calculate Y center, X start, and max chars to fit in rect */
   charwidth = scrnPort->txFont->chWidth;
   y = (editRect->Ymax + editRect->Ymin + 1 ) / 2;
   x = editRect->Xmin + charwidth;
   maxchars = (editRect->Xmax - x) / charwidth - 1;

   /* if insert point outside of rect, draw portion of string */
   if( insertpos > (editStr + maxchars ) )
       editStr = insertpos - maxchars;            

   /* erase old cursor */
   PenColor( White );
   DilgCursor( *curspos, editRect );

   /* draw text */
   ProtectRect( editRect );
   PenColor( colors->text );
   BackColor( White );
   MoveTo( x,y );
   DrawString( editStr );
   DrawChar(' ');  /* blank an few char positions (in case of deletion) */
   DrawChar(' ');
   DrawChar(' ');
   ProtectRect(NULL);

   /* find cursor position */
   while( editStr != insertpos ) 
      x += CharWidth( *editStr++ );

   *curspos = x;

}

void DilgCursor( int x, rect *editR )
/*
Draw the dialog box cursor 
*/
{
   int miny, maxy, minx, maxx;
   rect cR;
   
   miny = editR->Ymin;
   maxy = editR->Ymax;
   minx = x - 2;
   maxx = x + 2;

   SetRect( &cR, minx, miny, maxx, maxy );
   ProtectRect( &cR );

   MoveTo( x, miny + 2 );
   LineTo( x, maxy - 2 );

   MoveTo( minx, miny );
   LineTo( x , miny + 2 );
   LineTo( maxx, miny );

   MoveTo( minx, maxy );
   LineTo( x , maxy - 2 );
   LineTo( maxx, maxy );

   ProtectRect(NULL);

}

void DilgShowText( rect *textR, char *text )
/*
This routine is for those folks who just want to display some text in a
rectangle, in a form compatable with DialogText(). Typically used to
initialize a display with some strings before calling DialogText()
*/
{
   int x, y;
   rect tR;
   
   DupRect( textR, &tR );
   InsetRect( &tR, 2, 2 );
   ProtectRect( &tR );
   ClipRect( &tR );
   BackColor( White );
   EraseRect( &tR );
   TextAlign( alignLeft, alignMiddle );
   x = tR.Xmin + scrnPort->txFont->chWidth;
   y = (tR.Ymax + tR.Ymin + 1 ) / 2;
   MoveTo( x, y );
   RasterOp( zREPz );
   PenColor( colors->text );
   DrawString( text );
   ProtectRect(NULL);
   ClipRect( &scrnPort->portRect );

}


void DrawScrollBar( scrollBar *sb )
/*
Draw the scroll bar and initialize some of the fields
*/
{
   int cntrlSize, vert;
   rect cntrlRect;

   PenColor( Black );
   BackColor( LtGray );

   DupRect( &sb->SBRect, &cntrlRect );

   /* set flag if vertical scroll bar */
   if( (cntrlRect.Ymax - cntrlRect.Ymin ) > (cntrlRect.Xmax - cntrlRect.Xmin ) )
      vert = 01;
   else
      vert = 00;
   sb->flags = vert;

   RasterOp( zREPz );
   ProtectRect( &cntrlRect );

   /* draw 'shaft' */
   EraseRect( &cntrlRect );
   FrameRect( &cntrlRect );

   /* build up and down boxes - skip drawing these for FFSC -JCB */
   DupRect( &cntrlRect, &sb->upbox );
   DupRect( &cntrlRect, &sb->dnbox );
   if( vert ) {
      cntrlSize = picY( 30 );
      sb->upbox.Ymax = sb->upbox.Ymin + cntrlSize;
      sb->dnbox.Ymin = sb->dnbox.Ymax - cntrlSize;
   }
   else {
      cntrlSize = picX( 30 );
      sb->upbox.Xmax = sb->upbox.Xmin + cntrlSize;
      sb->dnbox.Xmin = sb->dnbox.Xmax - cntrlSize;
   }
/*

   FrameRect( &sb->upbox );
   FrameRect( &sb->dnbox ); 

   TextAlign( alignCenter, alignMiddle );
   TextFace( cProportional );
   RasterOp( zANDz );
   MoveTo( (sb->upbox.Xmax + sb->upbox.Xmin + 1 ) / 2, 
           (sb->upbox.Ymax + sb->upbox.Ymin + 1 ) / 2 );
   if( vert )
      DrawChar( uparrow );
   else
      DrawChar( lfarrow );

   MoveTo( (sb->dnbox.Xmax + sb->dnbox.Xmin + 1 ) / 2, 
           (sb->dnbox.Ymax + sb->dnbox.Ymin + 1 ) / 2 );
   if( vert ) {
     
      DrawChar( dnarrow );
   else
      DrawChar( rtarrow );

   RasterOp( zREPz );
   Button3D( &sb->upbox ); 
   Button3D( &sb->dnbox ); 
*/

   /* build thumb */
   if( vert )
      InsetRect( &cntrlRect, 1, cntrlSize + 1 );
   else
      InsetRect( &cntrlRect, cntrlSize + 1, 1 );

   DupRect( &cntrlRect, &sb->thumb );
   if( vert )
      sb->thumb.Ymax = sb->thumb.Ymin + cntrlSize;
   else
      sb->thumb.Xmax = sb->thumb.Xmin + cntrlSize;

   /* place it */
   PositionSB( sb );

   ProtectRect(NULL);
}

void PositionSB( scrollBar *sb )
/*
Find thumb position and draw it
*/
{
   int smax, smin, spos, sizethumb, vert;
   long pixunits;
   

   /* get vertical or horizontal flag */
   vert = sb->flags & 01;

   /* range of possible positions does not include controls */
   /* or size of the thumb itself */
   if( vert ) {
      sizethumb = sb->thumb.Ymax - sb->thumb.Ymin;
      /*      smin = sb->upbox.Ymax + 1;         *** up/down boxes not drawn***
      smax = sb->dnbox.Ymin - sizethumb; */
      smin =  sb->upbox.Ymin + 1;
      smax = sb->dnbox.Ymax - sizethumb -1;
   }
   else {
      sizethumb = sb->thumb.Xmax - sb->thumb.Xmin + 1;
/*      smin = sb->upbox.Xmax + 1;
      smax = sb->dnbox.Xmin - sizethumb; */
      smin = sb->upbox.Xmin + 1;
      smax = sb->dnbox.Xmax - sizethumb -1;   
   }

   /* find new position for thumb */
   if( (sb->maxval - sb->minval) == 0 )
      pixunits = 0L;
   else
      pixunits = (long) (smax - smin) * 100L / (long) (sb->maxval - sb->minval);     

   spos = (int)( pixunits * (long) (sb->curval - sb->minval) / 100L );

   /* control rect may not have been evenly divisible */
   /* force last position to end of control bar */
   if( sb->curval >= sb->maxval ) 
      spos = smax - smin;

   ProtectRect( &sb->SBRect );

   RasterOp( zREPz );
   PenColor( Black );
   BackColor( LtGray );

   /* remove old thumb (and frame) */
   sb->thumb.Xmax++;
   sb->thumb.Ymax++;
   EraseRect( &sb->thumb );
   sb->thumb.Xmax--;
   sb->thumb.Ymax--;

   /* position relative to top of thumb */
   if( vert ) {
      sb->thumb.Ymin = smin + spos;
      sb->thumb.Ymax = sb->thumb.Ymin + sizethumb;
   }
   else {
      sb->thumb.Xmin = smin + spos;
      sb->thumb.Xmax = sb->thumb.Xmin + sizethumb;
   }

   /* draw thumb */
   FillRect( &sb->thumb, 0 );
   FrameRect( &sb->thumb );
   Button3D( &sb->thumb ); 

   ProtectRect(NULL);

}

void DoScrollBar( scrollBar *sb )
/* 
Figure out what to do in this scroll bar and do it 
*/
{
   int x, y, i, thumbTrack, thumbSize, vert, lastval;
   rect tR, *ptR, cR;
   long pixunits;
   unsigned int  entertime;
   
   /* get vertical or horizontal flag */
   vert = sb->flags & 01;

   ptR = &sb->thumb;
   if( vert ) 
      thumbSize = ptR->Ymax - ptR->Ymin;
   else
      thumbSize = ptR->Xmax - ptR->Xmin;

   DupRect( &sb->SBRect, &cR );
   if( vert ) {
      InsetRect( &cR, 1, sb->upbox.Ymax - sb->upbox.Ymin + 1 );
      cR.Ymax -= thumbSize;
   }
   else {
      InsetRect( &cR, sb->upbox.Xmax - sb->upbox.Xmin + 1, 1 );
      cR.Xmax -= thumbSize;
   }
   thumbTrack = False;
   entertime = (unsigned int) opEvent.eventTime;

   /* do while no new event */
   do {        
      
      x = opEvent.eventX; y = opEvent.eventY;


      if( thumbTrack ) {

         /* center an xor frame of the thumb at y */
         DupRect( ptR, &tR );
         if( vert ) {
            tR.Ymin = y - thumbSize / 2;
            if( tR.Ymin < cR.Ymin )
               tR.Ymin = cR.Ymin;
            if( tR.Ymin > cR.Ymax )
               tR.Ymin = cR.Ymax;
            tR.Ymax = tR.Ymin + thumbSize;
         }
         else {
            tR.Xmin = x - thumbSize / 2;
            if( tR.Xmin < cR.Xmin )
               tR.Xmin = cR.Xmin;
            if( tR.Xmin > cR.Xmax )
                  tR.Xmin = cR.Xmax;
            tR.Xmax = tR.Xmin + thumbSize;
         }

         /* only redraw if necessary */
         if( EqualRect( ptR, &tR ) )
            continue;

         /* calculate new current value */
         lastval = sb->curval;

         /* find y position relative to control box */
         if( vert )
            i = tR.Ymin - cR.Ymin;
         else
            i = tR.Xmin - cR.Xmin;

         /* find pixels per unit */
         if( (sb->maxval - sb->minval) == 0 )
            sb->curval = sb->minval;
         else {
            if( vert )
               pixunits = (long) (cR.Ymax - cR.Ymin) * 100L / (long) (sb->maxval - sb->minval);
            else
               pixunits = (long) (cR.Xmax - cR.Xmin) * 100L / (long) (sb->maxval - sb->minval);

            sb->curval = sb->minval + (int) ( (long) i * 100L / pixunits );
            if( sb->curval > sb->maxval )
               sb->curval = sb->maxval;
         }

         /*  allow real-time thumb scroll */
         if(  sb->curval != lastval  ) {
            if( ptR->Ymin > tR.Ymin)
               (*sb->sbfunc)( position );
            if( ptR->Ymin < tR.Ymin)
               (*sb->sbfunc)( position );
         }

         PenColor(White);
         RasterOp( zXORz );

         HideCursor();
         FrameRect( ptR );
         FrameRect( &tR );
         DupRect( &tR, ptR );            
         ShowCursor();
         continue;

      } /* end of if thumb track */


      if( XYInRect(x,y,&sb->upbox) ) {

         HideCursor();
         InvertRect( &sb->upbox );
         (*sb->sbfunc)( up );
         InvertRect( &sb->upbox );
         ShowCursor();
         PositionSB( sb );
         
         /* wait a bit at first for easy single selection */
         while( PeekGUIEvent( 1 ) == False ) {
            if( ( (unsigned int) opEvent.eventTime - entertime ) > 4 )
            break;
         }
         continue;
      }

      if( XYInRect(x,y,&sb->dnbox) ) {
         HideCursor();
         InvertRect( &sb->dnbox );
         (*sb->sbfunc)( down );
         InvertRect( &sb->dnbox );
         ShowCursor();
         PositionSB( sb );
         /* wait a bit at first for easy single selection */
         while( PeekGUIEvent( 1 ) == False ) {
            if( ( (unsigned int) opEvent.eventTime - entertime ) > 4 )
            break;
         }
         continue;
      }
      
      if( XYInRect(x,y,&sb->thumb) ) {
         HideCursor();
         /* remove thumb */
         BackColor( LtGray );
         ptR->Xmax++;
         ptR->Ymax++;
         EraseRect( ptR );
         ptR->Xmax--;
         ptR->Ymax--;

         /* put into xor mode */
         PenColor(White);
         RasterOp( zXORz );

         /* seed xor */
         FrameRect( ptR );

         DupRect( ptR, &tR );
         thumbTrack = True;
         ShowCursor();
         continue;
      }

      /* else must be in page up-down area */
      if( XYInRect(x,y,&sb->SBRect) ) {

         if( y < sb->thumb.Ymin ) {
            (*sb->sbfunc)( pgup );
            PositionSB( sb );
            /* wait for a mouse up before proceeding */
            while( PeekGUIEvent( 1 ) == False )
               ;
            continue;
         }

         if( y > sb->thumb.Ymax ) {
            (*sb->sbfunc)( pgdown );
            PositionSB( sb );
            /* wait for a mouse up before proceeding */
            while( PeekGUIEvent( 1 ) == False )
               ;
            continue;
         }

      }

      /* must have gone out of the scroll bar rect, ignore */

   } while( !KeyGUIEvent( False ) );


   /* if we were tracking the thumb */
   if( thumbTrack ) {

      /* draw it in */
      PenColor(Black);
      RasterOp( zREPz );

      /* re-draw thumb */
      ProtectRect( &sb->SBRect );
      BackColor( LtGray );
      FillRect( ptR, 0 );
      FrameRect( ptR );
      Button3D( ptR ); 
      
      ProtectRect(NULL);
      
      (*sb->sbfunc)( position );

   }

}

void Button3D( rect *bR )
{
   rect tR;
   
   DupRect( bR, &tR );

   /* draw hilite */
   InsetRect( &tR, 1, 1 );
   PenColor( White );
   MoveTo( tR.Xmin, tR.Ymax );
   LineTo( tR.Xmin, tR.Ymin );
   LineTo( tR.Xmax, tR.Ymin );

   /* draw lowlite */
   PenColor( Gray );
   MoveTo( tR.Xmin, tR.Ymax );
   LineTo( tR.Xmax, tR.Ymax );
   LineTo( tR.Xmax, tR.Ymin );
   InsetRect( &tR, 1, 1 );
   MoveTo( tR.Xmin, tR.Ymax );
   LineTo( tR.Xmax, tR.Ymax );
   LineTo( tR.Xmax, tR.Ymin );
}



