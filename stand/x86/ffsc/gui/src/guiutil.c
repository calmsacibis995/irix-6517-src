/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1991            */
/*********************************************/
/* FILE:    guiutil.c                        */
/* PURPOSE: module contains various utility  */
/*          routines used by all             */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 5    GUIUTIL.C    22-Mar-95    21:48:34 # */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VXWORKS
#include <dos.h>
#endif /* not VXWORKS */

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"


void delay( unsigned int interval )
/*
Wait for at least 'interval' periods of 54.9 ms.
may wait up to an additional 54.9 ms
*/
{
   
#ifdef VXWORKS
   /* compute in microseconds to increase precision */
   taskDelay(((unsigned long)interval * 54900L) / (1000000L / sysClkRateGet()));
#else /* not VXWORKS */
   union REGS my_regs;
   unsigned int ticks;

   /* may be just at boundary condition */
   interval++;

   /* get ticks at the moment */
   my_regs.REGWRD.ax = 0;
   INTCALL( 0x1A, &my_regs, &my_regs );
   ticks = my_regs.REGWRD.dx;

   /* wait for ticks to increment at least interval times */
   while( (my_regs.REGWRD.dx - ticks) < interval ) {
      my_regs.REGWRD.ax = 0;
      INTCALL( 0x1A, &my_regs, &my_regs );
   }
#endif /* VXWORKS */
            
}


void DoBeep()
{
   putchar(7);
}


int picX( int sX)
/* Convert .01"  measure to pixel X */
{
   long l;
   
   /* pixels per inch times inches/100 */
   l = resX * (long) sX / 100L;
   return (int) l;
}


int picY( int sY)
/* Convert .01" measure to pixel Y */
{  
   long l;
   
   /* pixels per inch times inches/100 */
   l = resY * (long) sY / 100L;
   return (int) l;
}


equX( int y )
/* return equivalent size x pixels for y pixels */
{
   
   y = (int ) ( (long) y * resX );
   y = (int)  ( (long) y / resY );
   return y;
}

equY( int x )
/* return equivalent size y pixels for x pixels */
{
   
   x = (int) ( (long) x * resY );
   x = (int) ( (long) x / resX );
   return x;
}


void ShadowRect( rect *shadR, int shadSize  )
{
   rect tR;
   
   DupRect( shadR, &tR );
   OffsetRect( &tR, picX( shadSize ), picY( shadSize ) );

   PenColor(Black);
   RasterOp(zREPz);
   FillRect(&tR,1);

   PenColor(White);
   FillRect(shadR,1);
   PenColor( Black );
   FrameRect(shadR);

}


void
OnBevel( rect *bevelR )
{
   int   i, bevel;
   rect  rtR, botR;
   
   bevel = scrnPort->txFont->chHeight/5;

   /* perimeter rects */
   SetRect( &botR, bevelR->Xmin,
                   bevelR->Ymax - bevel + 1,
                   bevelR->Xmax,
                   bevelR->Ymax);

   SetRect( &rtR,  bevelR->Xmax - bevel + 1,
                   bevelR->Ymin,
                   bevelR->Xmax,
                   bevelR->Ymax);
    
   PenColor(colors->low);
   FillRect( &rtR, 1 );
   FillRect( &botR, 1 );

   RasterOp(zREPz);
   PenColor(colors->high);
   for( i=0; i<bevel; i++) {
      MoveTo( bevelR->Xmin,     bevelR->Ymin + i );  /* top */
      LineTo( bevelR->Xmax - i - 1, bevelR->Ymin + i );

      MoveTo( bevelR->Xmin + i, bevelR->Ymin     );  /* left */
      LineTo( bevelR->Xmin + i, bevelR->Ymax - i - 1 );
   }

}

void
OffBevel( rect *bevelR )
{
   int   i, bevel;
   rect  rtR, botR;

   PenColor(colors->bar);
   bevel = scrnPort->txFont->chHeight/5;

   /* perimeter rects */
   SetRect( &botR, bevelR->Xmin,
                   bevelR->Ymax - bevel + 1,
                   bevelR->Xmax,
                   bevelR->Ymax);

   SetRect( &rtR,  bevelR->Xmax - bevel + 1,
                   bevelR->Ymin,
                   bevelR->Xmax,
                   bevelR->Ymax);
                 
   FillRect( &rtR, 1 );
   FillRect( &botR, 1 );

   RasterOp(zREPz);
   for( i=0; i<bevel; i++) {
      MoveTo( bevelR->Xmin,     bevelR->Ymin + i );  /* top */
      LineTo( bevelR->Xmax - i - 1, bevelR->Ymin + i );
      MoveTo( bevelR->Xmin + i, bevelR->Ymin     );  /* left */
      LineTo( bevelR->Xmin + i, bevelR->Ymax - i - 1 );
   }

}

void FlushEvents()
/* Flush Operator Event Queue */
{
   while( KeyGUIEvent( False ) )
      ;
}

int KeyGUIEvent( int wait )
/* get a GUI event via KeyEvent */

{
   int i;

/*   fprintf(stderr, "Entering KeyGUIEvent\n"); */
   if( wait ) {
      do {
         i = KeyEvent( False, &opEvent );
         IdleProcess();
      } while( !i );
/*		fprintf(stderr, "Leaving KeyGUIEvent, i is %d\n", i);*/
      return True;
   }
   else {
      i = KeyEvent( False, &opEvent );
      IdleProcess();
/*		fprintf(stderr, "Leaving KeyGUIEvent, i is %d\n", i); */
      return i;
   }
}

int PeekGUIEvent( int evno )
/* peek a GUI event via PeekEvent */
{
   int i;

   i = PeekEvent( evno, &opEvent );
   IdleProcess();
   return i;
   
}

char *DosError( int err )
/* decode err dos error code and return string */
{
   char *s;
   
   /* make positive (metawindow usually negates it) */
   if( err < 0 )
      err *= -1;

   switch( err ) {
              
      case 02:    s = "File not found";       break;
      case 03:    s = "Path not found";       break;
      case 04:    s = "Too many files open";  break;
      case 05:    s = "Access denied";        break;
      case 15:    s = "Invalid disk drive";   break;
      default:
                  sprintf( msg1, "DOS code %d", err );
                  return msg1;
   }

   return s;

}







/* End of File - GUIUTIL.C */
