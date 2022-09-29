/* Derived from ... */
/*  %kw # %v    %n    %d    %t #             */
/* Version # 4    BARGRF.C    4-Nov-93    13:55:52 # */
/******************************************************/
/*         Metagraphics Software Corporation           */
/*             Copyright (c) 1987-1993                 */
/*******************************************************/
/* FILE:    BARGRF.C                                   */
/* PURPOSE: module contains bar graph                  */
/*******************************************************/

/* FFSC display bar graph handling */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wdlib.h>

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"

#include "ffsc.h"
#include "oobmsg.h"

extern void drawBackImage(void);
extern palData grfPal[];
extern int grfovride;
extern WDOG_ID barGraphTimeout;


#define Mid(a,b)  (a - b)/2 + b
#define MidPt(r)  Mid(r.Xmax, r.Xmin), Mid(r.Ymax, r.Ymin)

#define BARGRAPH_IDLE_TIMEOUT       (30 * sysClkRateGet())  /* 30 Seconds */

static int BACKGROUND;
static int LINES     ;
static int HEADER    ;
static int NUMBERS   ;
static int LABELS    ;
static int HIGHLIGHT ;
static int BARMAX    ;

static int BarColors[] = {1,4,14,3,11,12,6,13};

static rect grafRect;
static int lnSpacing, Xmin, Xmax, Xmid, Ymin, Ymax;

static char Header[30], vLabel[30], hLabel[30], Legend[8][30];
static int minVal, maxVal,  NumBars, Depth, lgd, startbar, nbars;
static unsigned char BgData[1024];
static unsigned int BarYmin[128];        /* really tops of bars */
int grfmode, redraw;

void initGraph() {

  grafPort  *ThisPort;

  GetPort ( &ThisPort );
  ProtectRect( &ThisPort->portRect );
  
  BACKGROUND = 8;
  LINES      = 7;
  HEADER     = 7;
  NUMBERS    = 2;
  LABELS     = 7;
  HIGHLIGHT  = 7;
  BARMAX   = 255;
  
  if( QueryColors() < 2 ) {
    NUMBERS = 1;
    LABELS  = 1;
  }
  
  PortOrigin ( True );
  Xmin  = ThisPort->portRect.Xmin;
  Ymin  = ThisPort->portRect.Ymin + barRect.Ymax;
  Xmax  = ThisPort->portRect.Xmax;
  Ymax  = ThisPort->portRect.Ymax;
  Xmid  = (Xmax - Xmin)/2 + Xmin;
  SetRect(&grafRect, Xmin, Ymin, Xmax, Ymax-26); 
  lnSpacing = ThisPort->txFont->lnSpacing;
  redraw = 1;
}

void eraseScreen() {
  RasterOp( zREPz );
  BackColor( BACKGROUND );
  /*   EraseRect( &ThisPort->portRect ); */
  EraseRect( &grafRect );
}

void Shadow ( char *Text, int Color, int X, int Y, int Hi, int Sha )
{
   int i;
   
   /*  Draw highlight (upper, left)  */
   if( Hi > 0 ) {
      RasterOp( zREPz );
      PenColor( HIGHLIGHT );
      BackColor( BACKGROUND );
      MoveTo( X-1, Y-1 );
      DrawString( Text );
    }

   /*  Draw Shadow (bottom, right)  */
   RasterOp( zANDz );
   BackColor( White );
   PenColor( Black );

   for( i = Sha ; i >= 0 ; i-- ) {
      MoveTo( X+i, Y+i );
      DrawString( Text );
    }

   /*  Draw the text  */
   MoveTo( X, Y );      TextMode( zORz );
   PenColor( Color );   BackColor( Black );
   DrawString( Text );

   RasterOp( zREPz );

}

void bargraf()
{

   rect       tmpR, Bar, BarBox;
   char       tmpstr[64];
   int        i, j, x, y, numnum, delta, hite, width, lgdWidth, ymax;

   SetRect( &BarBox, StringWidth ("WWWW") * 2 + Xmin,
	   lnSpacing * 3 + Ymin,
	   Xmax - StringWidth ("WWWW"),
	   Ymax - (lnSpacing * 2) -60);
   hite = BarBox.Ymax - BarBox.Ymin + 1;
   numnum = 5;
   delta = hite / (numnum - 1);

   if (redraw == 1) {
     eraseScreen();

     /* Zero out minima */
     for (i=0; i<128; i++)
       BarYmin[i] = BarBox.Ymax;

     /*  Print Headers  */
     TextAlign( alignCenter, alignBottom );
     Shadow( Header, HEADER, Xmid, lnSpacing * 2 + Ymin, 0, 2 );
     
     /*  Draw right side and bottom  */
     PenSize( 1, 1 );
     PenColor( LINES );
     MoveTo( BarBox.Xmin - 1, BarBox.Ymin );
     LineTo( BarBox.Xmin - 1, BarBox.Ymax );
     LineTo( BarBox.Xmax, BarBox.Ymax );
     
     /*  Print numbers down left side  */
     /*   numnum = hite / (2 * lnSpacing); */
     x = BarBox.Xmin - 4;
     if (minVal < 0) minVal = 0;
     if (maxVal < 0) maxVal = 100;
     TextAlign( alignRight, alignMiddle );
     for( y = BarBox.Ymax, i = 0 ; i < numnum ; y -= delta, i++ )  {
       sprintf( tmpstr, "%d", minVal + (i * (maxVal - minVal)/(numnum - 1)) );
       Shadow( tmpstr, NUMBERS, x, y, 0, 1 );
     }
     x = BarBox.Xmin - 50;                       /* Label Vertical Axis */
     y = BarBox.Ymin + ((BarBox.Ymax - BarBox.Ymin)/2);
     TextAlign( alignCenter, alignMiddle );
     TextPath(pathDown);
     if (strcmp(vLabel, "") != 0)
       Shadow (vLabel, LABELS, x, y, 0, 1 );
     else
       Shadow ("Activity", LABELS, x, y, 0, 1 );
     
     x = BarBox.Xmin + ((BarBox.Xmax - BarBox.Xmin)/2); 
     y = BarBox.Ymax + 30;                       /* Label Horizontal Axis */
     TextPath(pathRight);
     if (strcmp(hLabel, "") != 0)
       Shadow (hLabel, LABELS, x, y, 0, 1 );
     else
       Shadow ("Processors", LABELS, x, y, 0, 1 );
     
     lgdWidth = 0;
     for (i=0; i<Depth; i++) {                   /* Print Legend */
       lgdWidth += StringWidth(Legend[i]);
       if (i < Depth-1)
	 lgdWidth += CharWidth(' ');
     }
     x = x - lgdWidth/2;
     y += 30;
     TextAlign(alignLeft, alignMiddle);
     for (i=0; i<Depth; i++) {
       if (i > 0)
	 x += StringWidth(Legend[i-1]) + CharWidth(' ');
       Shadow(Legend[i], BarColors[i], x, y, 0, 1);
     } 
   }

   /*  Draw & label bars  */
   PenSize( penThin );
   width = (BarBox.Xmax - BarBox.Xmin) / (NumBars + 1);
   delta = width / NumBars;
   SetRect( &Bar, BarBox.Xmin + (startbar * (width + delta)) + delta, 
	   BarBox.Ymax - hite,
	   BarBox.Xmin + ((startbar + 1) * (width + delta)),
	   BarBox.Ymax );

   for( i = startbar ; i < startbar + nbars ; i++ )  {
     if (redraw == 1 && ((NumBars <= 8) || (i == NumBars-1) ||
			 (i%(NumBars/8) == 0)))
       {
	 x = Mid( Bar.Xmax, Bar.Xmin );
	 y = Bar.Ymax + 12;
	 sprintf( tmpstr, "%d", i);
	 Shadow( tmpstr, NUMBERS, x, y, 0, 1 );
       }
/*
     DupRect( &Bar, &tmpR);
     tmpR.Xmax += 1; 
*/
     RasterOp( zREPz );
     BackColor( BACKGROUND );
/*
     EraseRect( &tmpR );
     if (NumBars <= 8) {
       SetRect( &tmpR, Bar.Xmax, Bar.Ymin + 5, Bar.Xmax + 4, Bar.Ymax);
       EraseRect( &tmpR );
     } 
*/
     for (j = 0; j < Depth ; j++) {
       if (j > 0) 
	 Bar.Ymax = Bar.Ymin;
       Bar.Ymin = BarBox.Ymax -                     /* values are cumulative */
	 (hite * (int) BgData[(i * Depth) + j]/BARMAX);
       
/*       if( (size = (hite * Bars[i].size) / 100)  <  lnSpacing * 2 )
       Bar.Ymin = BarBox.Ymax - size; */

/*       PenColor( Black ); */
/*       LineTo( Bar.Xmin, Bar.Ymax ); */
       PenColor(BarColors[j]);
       PaintRect( &Bar );
       PenPattern( 1 );
       PenColor( 7 );
       MoveTo( Bar.Xmin, Bar.Ymax );
       LineTo( Bar.Xmin, Bar.Ymin );
       LineTo( Bar.Xmax - 1, Bar.Ymin );
       if (NumBars <= 8) {
	 MoveTo( Bar.Xmax, Bar.Ymin );
	 PenColor( Black );
	 LineTo( Bar.Xmax, Bar.Ymax - 1 );
       }
       /* FrameRect ( &Bar );*/
       
       /*  put a box around the label  */
/*       j = StringWidth( Bars[i].label );
       x = Mid( Bar.Xmax, Bar.Xmin );
       y = Mid( Bar.Ymax, Bar.Ymin );
       SetRect ( &tmpR, x - j/2, y - lnSpacing/2, x + j/2, y + lnSpacing/2 );
       BackColor( BACKGROUND );
       EraseRect( &tmpR );
       PenColor( Black );
       MoveTo( tmpR.Xmin, tmpR.Ymax );
       LineTo( tmpR.Xmin, tmpR.Ymin );
       LineTo( tmpR.Xmax, tmpR.Ymin );
       PenColor( 7 );
       LineTo( tmpR.Xmax, tmpR.Ymax );
       LineTo( tmpR.Xmin, tmpR.Ymax ); */
       /* FrameRect ( &tmpR );*/
       
       /*  label the bar  */
       /* Shadow( Bars[i].label, LABELS, MidPt (Bar), 0, 1 );*/
/*       TextAlign( alignCenter, alignMiddle );
       Shadow( Bars[i].label, LABELS, x, y, 0, 1 ); */
     }
       
       /*  give the bar a shadow  */
       Bar.Ymax = BarBox.Ymax;
     if (NumBars <= 8) {
       PenColor( Black );
       PenPattern( 1 );
       SetRect( &tmpR, Bar.Xmax, Bar.Ymin + 5, Bar.Xmax + 4, Bar.Ymax);
       PaintRect( &tmpR );
     }

     /* Erase what's left from before if needed */

     if (Bar.Ymin > BarYmin[i]) {
       DupRect( &Bar, &tmpR);
       tmpR.Xmax += 1;
       tmpR.Ymax = Bar.Ymin;
       tmpR.Ymin = BarYmin[i];
       EraseRect( &tmpR );

       if (NumBars <= 8) {
	 ymax = Bar.Ymin + 5;
	 if (ymax > BarBox.Ymax)
	   ymax = BarBox.Ymax;
	 SetRect( &tmpR, Bar.Xmax, BarYmin[i] + 5, Bar.Xmax + 4, ymax);
	 EraseRect( &tmpR );
       } 
       
     }
     BarYmin[i] = Bar.Ymin;
     
     /*  setup for next iteration  */
     Bar.Ymin = BarBox.Ymax - hite;
     OffsetRect( &Bar, width + delta, 0 );
   }

   if (redraw)
     redraw = 0;

   ProtectRect( NULL );


}

int doGraphCmd(unsigned char *cmd) {

  int i, j, nbytes, oldval, maxbyte, minbyte;

  if (ffscDebug.Flags & FDBF_BASEIO)
    ffscMsg("in doGraphCmd, grfmode=%d, grfovride=%d", grfmode, grfovride);
  switch( OOBMSG_CODE(cmd) ) {
  case GRAPH_START:			/* Enter graph mode */
    if (!grfovride) {
      if (OOBMSG_DATALEN(cmd) == 2) {
	eraseScreen();
	WritePalette(0,8,15,grfPal);
	grfmode=1;
	redraw=1;
	NumBars = (OOBMSG_DATA(cmd))[0];
	Depth = (OOBMSG_DATA(cmd))[1];
	lgd = 0;
	minVal = -1;
	maxVal = -1;
	strcpy(hLabel,"");
	strcpy(vLabel,"");
	strcpy(Header,"");
      }
      else {
	if (ffscDebug.Flags & FDBF_BASEIO)
	  ffscMsg("Invalid oob msg len to GRAPH_START"); 	
	return STATUS_ARGUMENT;
      }
    }
    break;
  case GRAPH_END:               	/* Leave graph mode */
    if (grfmode && !grfovride) {
      eraseScreen();
      grfmode=0;
      drawBackImage();
    }
    break;
  case GRAPH_MENU_ADD:			/* Add item to graph menu */
    break;
  case GRAPH_MENU_GET:			/* Get currently selected graph item */
    break;
  case GRAPH_LABEL_TITLE:		/* Set graph title */
    if (grfmode & !grfovride) {
      strcpy(Header, (char *) OOBMSG_DATA(cmd));
      redraw = 1;
    }
    break;
  case GRAPH_LABEL_HORIZ:		/* Set horizontal axis label */
    if (grfmode & !grfovride) {
      strcpy(hLabel, (char *) OOBMSG_DATA(cmd));
      redraw = 1;
    }
    break;
  case GRAPH_LABEL_VERT:		/* Set vertical axis label */
    if (grfmode & !grfovride) {
      strcpy(vLabel, (char *) OOBMSG_DATA(cmd));
      redraw = 1;
    }
    break;
  case GRAPH_LABEL_MIN:			/* Set minimum graph value */
    if (grfmode & !grfovride) {
      minVal = atoi((char *) OOBMSG_DATA(cmd));
      redraw = 1;
      if (ffscDebug.Flags & FDBF_BASEIO)
	ffscMsg("Got minVal %d", minVal);
    }
    break;
  case GRAPH_LABEL_MAX:			/* Set maximum graph value */
    if (grfmode & !grfovride) {
      maxVal = atoi((char *) OOBMSG_DATA(cmd));
      redraw = 1;
      if (ffscDebug.Flags & FDBF_BASEIO)
	ffscMsg("Got maxVal %d", maxVal);
    }
    break;
  case GRAPH_LEGEND_ADD:		/* Add legend label */
    if (grfmode & !grfovride) {
      strcpy(Legend[lgd++], (char *) OOBMSG_DATA(cmd));
      redraw = 1;
    }
    break;
  case GRAPH_DATA:			/* Graph data */
    if (grfmode & !grfovride) {
   	if (wdStart(barGraphTimeout, BARGRAPH_IDLE_TIMEOUT, cancelBargraphDisplay, 0) < 0)
      	ffscMsg("Unable to start/restart Bargraph timer");

      startbar = (OOBMSG_DATA(cmd))[0];
      nbars = (OOBMSG_DATA(cmd))[1];
      nbytes = nbars * Depth;
      if (OOBMSG_DATALEN(cmd) != nbytes+2) {
	if (ffscDebug.Flags & FDBF_BASEIO)
	  ffscMsg("Invalid data length in GRAPH_DATA");
	return STATUS_ARGUMENT;
      }
      else if (startbar >= NumBars) {
	if (ffscDebug.Flags & FDBF_BASEIO)
	  ffscMsg("Invalid startbar in GRAPH_DATA");
	return STATUS_ARGUMENT;
      }
      else if (startbar + nbars - 1 >= NumBars) {
	if (ffscDebug.Flags & FDBF_BASEIO)
	  ffscMsg("Invalid bar range in GRAPH_DATA");
	return STATUS_ARGUMENT;
      }
      else {
	minbyte = startbar*Depth;
	maxbyte = minbyte + nbytes;
	for (i=minbyte, j=2; i<maxbyte; i++, j++) {
	  BgData[i] = (OOBMSG_DATA(cmd))[j];
	  if (i%Depth == 0)
	    oldval = 0;
	  else {
	    if (BgData[i] < oldval) {
	      if (ffscDebug.Flags & FDBF_BASEIO)
		ffscMsg("Invalid sub-bar limit in GRAPH_DATA");
	      return STATUS_ARGUMENT;
	    }
	    else
	      oldval = BgData[i];
	  }
	}
	bargraf();
      }
    }
    break;
  default:
    return STATUS_COMMAND;
    break;
  }
  return STATUS_NONE;
}

/*
 * cancelBargraphDisplay
 * Simple function to cancel the bargraph display by drawing the backimage.
 * The redraw flag is set, to that when the next bargraph data is received, the display
 *	will change back to bargraphs.  This routine is called by a watchdog timer, after
 * no bargraph data has been received for 30 seconds.
 */
void
cancelBargraphDisplay(void)
{

	int i;

	i = write(DispReqFD,"r",2);
	if (i != 2) 
		ffscMsg("Unable to write to switch task pipe to cancel graphics display");

}

