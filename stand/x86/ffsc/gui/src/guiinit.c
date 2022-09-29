/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1991            */
/*********************************************/
/* FILE:    guiinit.c                        */
/* PURPOSE: module contains routines to      */
/*          initialize various things        */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 7    GUIINIT.C    4-Aug-94    17:06:24 # */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef VXWORKS
/* Normally we'd use the VxWorks #include "conio.c" file here but */
/* it doesn't work properly in this environment. */
#else
#include <dos.h>
#include <conio.h>
#endif /* not VXWORKS */
#include <ctype.h>

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"

#ifdef VXWORKS		/* declare embedded font */
extern fontRcd sys_96_10x24;       /* was sysprp72_fnt */;
#endif /* VXWORKS */

palData grfPal[8];
extern palData imgPal[];

void
InitGUI( int argc, char *argv[] )
/*
Initialize graphics library, load system font, and initialize
global variables.
*/
{
   
   int  i;
   char flName[80], *s;
   rect *scrnRect;
   dirRec dirinfo;
   palData color6[1], color5[1], color9[1], color11[1];

#ifdef VXWORKS
   /* Set default font to a reasonable embedded font */
/*   DefaultFont( & sysprp72_fnt ); */
   DefaultFont( & sys_96_10x24 );

   /* Set global to inform GUI application what the system font is. */
/*   sysFont = &sysprp72_fnt; */
   sysFont = &sys_96_10x24;

#endif

   /* find the type of card in the system and print it out */
/*   MetQuery(argc,argv); */

   /* initialize for detected card, origin upper left */
/*   i = InitGraphics( GrafixCard); */

#ifdef NCLR16
   i = InitGraphics( VGA640x480 );
#else
   i = InitGraphics( PAR640x480X );
#endif

   if( i < 0 ) {

      switch( i ) {
         
         case  -2:   s = "Graphics device not supported";
                     break;
         case  -3:   s = "Out of memory";
                     break;
         case  -4:   s = "Address mapping error";
                     break;
         case  -5:   s = "Graphics device does not respond";
                     break;
         case  -6:   s = "Can't load device driver";
                     break;
      }
      abend( ER_NODRVR, s );
   }

   SetDisplay( GrafPg0 );

   /* get pointer to the default port */
   GetPort(&scrnPort);

   /* get max screen rect */
   scrnRect = &(scrnPort->portRect);

   resX = scrnPort->portMap->pixResX;
   resY = scrnPort->portMap->pixResY;

   /* load system font */

   /* make a file name for a proportional system font */
   i =  scrnPort->portMap->devClass;
   /* use single plane fonts */
   i &= 0xFFF8;

#ifndef VXWORKS		/* VxWorks uses embedded fonts, not font files */
   sprintf(flName, "sysprp%02d.fnt", i );

   /* find size required for system font buffer */
   if( ResrcQuery( flName, &dirinfo, 1 ) == 0 )
      abend( ER_LOAD, flName );

   /* NOTE: why?  Confuses ResrcLoad since file isn't 10 bytes longer!
   dirinfo.fileSize += 10L;
   */

   sysFont = (fontRcd *) malloc( (unsigned int) dirinfo.fileSize );  
   if( sysFont == NULL )
      abend( ER_MEM, NULL );

   /* load the file */
   i = ResrcLoad( flName, (char *) sysFont, (unsigned int) dirinfo.fileSize );
   if( i <= 0 ) 
      abend( ER_LOAD, flName );

   /* make it the active font */
   SetFont( sysFont );
#endif /* VXWORKS: Since sysFont is the default, it is initially active */

   /* init some global vars */
   /* Active menu bar flags */
   barFlags    = allMenus;
   curCursor   = arrowCurs; /* start with arrow cursor */
   dblClick    = 5;  /* double click time ( 275 ms)  */

   /* initialize palette (reorganizing to have menu colors in first 8) */

   ReadPalette(0,6,6,color6);
   ReadPalette(0,5,5,color5);
   ReadPalette(0,9,9,color9);
   ReadPalette(0,11,11,color11);
   WritePalette(0,6,6,color9);
   WritePalette(0,5,5,color11);
   WritePalette(0,9,9,color6);
   WritePalette(0,11,11,color5);

  /* setup bar graph palette */
#ifdef NCLR16
   ReadPalette(0,8,15,grfPal);
#else                            /* setup color map of backing image */
   WritePalette(0,16,255,imgPal);
#endif

   /* setup color table */
   i = (int) QueryColors();
   colors = &color_tab[3];
   if( i < 256 )
      colors = &color_tab[2];
   if( i < 15 )
      colors = &color_tab[1];
   if( i < 3 )
      colors = &color_tab[0];

   /* switch into graphics mode */
/*   SetDisplay(GrafPg0); */


   /* setup mouse tracking */
/*   if( GrafixInput ) {
      InitMouse( GrafixInput );
      LimitMouse( 0, 0, scrnRect->Xmax - 5, scrnRect->Ymax - 5 );
      CursorStyle( arrowCurs ); 
      MoveCursor( scrnRect->Xmin + 32, scrnRect->Ymax / 2 );
      TrackCursor(True);
   }
   else */
      HideCursor();

   /* enable event system */
   EventQueue(cUSER);
   MaskEvent(evntPRESS+evntRELEASE+evntKEYDN);
							/* Note: stuff below was used for event bypass */
/*   EventQueue(False);		*/			/* make sure events are off */

/*   opEvent.eventTime = 0;		*/	/* initialize opEvent */
/*   opEvent.eventType = evntKEYDN;	*/	/* the only type of event we process */
/*   opEvent.eventSource = 0;
   opEvent.eventChar = 0;
   opEvent.eventScan = 0;
   opEvent.eventState = 0;
   opEvent.eventButtons = 0;
   opEvent.eventX = 0;
   opEvent.eventY = 0;
   opEvent.eventUser[0] = 0;
   opEvent.eventUser[1] = 0; */

/*   initdisplay();	*/			/* initialize switches,
						   register switch isr */

   /* clear screen */
   PenColor( colors->desktop );
   if( colors->desktop == White )
	   BackColor( Black );
   else
	   BackColor( White );
   FillRect( scrnRect, colors->deskpat );

   ShowCursor();

   /* seed random number generator */
/*   PeekEvent( 1, &opEvent );
   srand( (unsigned int) opEvent.eventTime ); */

}

void ResetGUIPalette( )
/* 
Reset palette to standard settings for GUI (same as IBM EGA defaults)
*/
{
   int maxcolors;

    static palData defpal[16] = {
	{ 0x0000, 0x0000, 0x0000 },  /* Color 0, black		    */
	{ 0x0000, 0x0000, 0xA800 },  /* Color 1, blue		    */
	{ 0x0000, 0xA800, 0x0000 },  /* Color 2, green		    */
	{ 0x0000, 0xA800, 0xA800 },  /* Color 3, cyan, turquoise    */
	{ 0xA800, 0x0000, 0x0000 },  /* Color 4, red		    */
	{ 0xA800, 0x0000, 0xA800 },  /* Color 5, magenta	    */
	{ 0xA800, 0x5400, 0x0000 },  /* Color 6, brown		    */
	{ 0xA800, 0xA800, 0xA800 },  /* Color 7, white, light grey  */
	{ 0x5400, 0x5400, 0x5400 },  /* Color 8, dark grey	    */
	{ 0x5400, 0x5400, 0xFFFF },  /* Color 9, light blue	    */
	{ 0x5400, 0xFFFF, 0x5400 },  /* Color A, light green	    */
	{ 0x5400, 0xFFFF, 0xFFFF },  /* Color B, light cyan	    */
	{ 0xFFFF, 0x5400, 0x5400 },  /* Color C, light red	    */
	{ 0xFFFF, 0x5400, 0xFFFF },  /* Color D, light magenta	    */
	{ 0xFFFF, 0xFFFF, 0x5400 },  /* Color E, yellow		    */
	{ 0xFFFF, 0xFFFF, 0xFFFF },  /* Color F, bright white	    */
    };


   /* reset the first 16 palette entries with default colors */
   WritePalette( 0, 0, 15, defpal );

   /* reset maximum palette entry to white */
   maxcolors = (int) QueryColors();
   WritePalette( 0, maxcolors, maxcolors, &defpal[15] );

}

void MergeGUIPalette( palData *DstPal )
/*
Merge the important GUI colors into a user palette
*/
{
   palData tmpPal;

	ReadPalette( 0, colors->desktop, colors->desktop, &tmpPal );
   DstPal[ colors->desktop ] = tmpPal;

	ReadPalette( 0, colors->bar,     colors->bar,     &tmpPal );
   DstPal[ colors->bar ] = tmpPal;

	ReadPalette( 0, colors->text,    colors->text,    &tmpPal );
   DstPal[ colors->text ] = tmpPal;

	ReadPalette( 0, colors->high,    colors->high,    &tmpPal );
   DstPal[ colors->high ] = tmpPal;

	ReadPalette( 0, colors->low,     colors->low,     &tmpPal );
   DstPal[ colors->low ] = tmpPal;

}

#define GRQTITLE    "Confirm Graphic Devices" 
#define GRQQUITSTR  "Program terminated.\n"
#define GRQHDRSTR   "Metagraphics Demo"


#ifdef VXWORKS
#define	getch()		(0)	/* NOTE: dummy getch() */
#define	GRQtrimMenu	1	/* prevent metquery.c from using BIOS */
#endif /* VXWORKS */

#include "metquery.c"


