/******************************************************************/
/* FFSCGUI.C - THE GUI!!!                   			  */
/* This program is a "how to" sample program that can be used as  */
/* as template for any program using the Metagraphics GUI Toolkit */
/******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ioLib.h>
#include <wdlib.h>
#include "metawndo.h"

#include "guiconst.h"
#include "guiextrn.h"

#include "ffsc.h"
#include "identity.h"
#include "misc.h"

extern image backimg[];
extern palData grfPal[], imgPal[];

menus  GUI_menu[] = {
/*-------------------------------------------------------------*/
/*--- Menu Bar Name -- Items -- Enables ---- Internal Use -----*/
/*-------------------------------------------------------------*/
     {
      "&Focus", 3, 0x07, 0,{0,0,0,0},{0,0,0,0},
      {"&Rack+Bay...", "&Module...","&Partition..."},
     },

     {
      "&Action", 6, 0x3f, 0,{0,0,0,0},{0,0,0,0},
      {"Power &Up", "Power &Down", "Power &Cycle", "&NMI", "&Reset", "&Scan"},
     },

     {
      "&View", 2, 0x03, 0,{0,0,0,0},{0,0,0,0},
      {"&Graph", "&Background"},
     },

     {
      "&Configure", 1, 0x01, 0,{0,0,0,0},{0,0,0,0},  
      {"&Authority"},  
     }
};

/*     {
      "&View", 1, 0x01, 0,{0,0,0,0},{0,0,0,0},
      {"&Bar Graph"},
     },
*/
/*,

     {
      "A&bout", 1, 0x01, 0,{0,0,0,0},{0,0,0,0},
      {"&O2000" },  
     }
}; */

int numMenus = sizeof( GUI_menu ) / sizeof( menus );

/* menu codes */

#define MU_Focus        0
#define MI_RackBay	0
#define MI_Module	1
#define MI_Partition    2

#define MU_Action	1
#define MI_PwrU		0
#define MI_PwrD		1
#define MI_PwrC		2
#define MI_Nmi		3
#define MI_Rst		4
#define MI_Scan		5

#define MU_View         2
#define MI_Graph   	0
#define MI_Bkimage      1

#define MU_Config       3
#define MI_Cauth   	0

/* #define MU_About        4
#define MI_AboutO2    	0 */


/* global vars */
grafPort IdlePort;
char dispCmdFFSC[30];
char dispTarget[20] = "rack all bay all";
char actionTarget[20] = "rack all bay all";
char tgt_msg[80];
int grfovride;
int grfovride_sav;		/* grfovride save register during menu operations */
WDOG_ID barGraphTimeout;


/* prototypes for functions that carry out menu selections */
void FocusMenu( void );
void ActionMenu( void );
void ViewMenu( void );
void ConfigMenu( void );
void AboutMenu( void );
extern void bargraf( void );
extern void initGraph(void);
extern void eraseScreen(void);

void drawBackImage() {

  imageHeader *header;
  rect tR;

#ifdef NCLR16
  WritePalette(0,8,15,imgPal);
#endif

  DupRect( &scrnPort->portRect, &tR );
  tR.Ymin = barRect.Ymax;
  header = (imageHeader *) backimg;
  ffscMsg("width: %d, height: %d, align: %d, flags: %d, rowBytes: %d, bits %d, planes %d\n", 
	  header->imWidth,
	  header->imHeight,
	  header->imAlign,
	  header->imFlags,
	  header->imRowBytes,
	  header->imBits,
	  header->imPlanes);
  WriteImage(&tR, backimg);
}

void showTarget() {

  rect txtRect;
  int charWidth = StringWidth(" ");
  int tgtWidth = StringWidth(dispTarget) + StringWidth("Current target: ");
  int verWidth = StringWidth(ffscVersion);
  char spacer[100];
  int nspace, i;

  for (i=0;i<100;i++)
    spacer[i] = ' ';
  
  /* use our own private port */
  SetPort( &IdlePort );
  PenColor( colors->desktop );
  SetRect(&txtRect, 
	  scrnPort->portRect.Xmin,
	  scrnPort->portRect.Ymax -25,
	  scrnPort->portRect.Xmax,
	  scrnPort->portRect.Ymax);
  FillRect( &txtRect, colors->deskpat );
  PenColor(7);
  MoveTo(txtRect.Xmin, txtRect.Ymin - 1);
  LineTo(txtRect.Xmax, txtRect.Ymin - 1);
  PenColor(colors->text);
  BackColor(colors->bar);

  nspace = (txtRect.Xmax - txtRect.Xmin - verWidth - tgtWidth)/charWidth;
  spacer[nspace] = '\0';
  TextAlign( alignCenter, alignMiddle );
  sprintf( tgt_msg, "%s%sCurrent target: %s",ffscVersion, spacer, dispTarget);

  MoveTo( txtRect.Xmin + (txtRect.Xmax -txtRect.Xmin)/2 , 
	 txtRect.Ymin + (txtRect.Ymax -txtRect.Ymin)/2 );
  DrawString( tgt_msg );
  
  SetPort( scrnPort );    /* set back to normal port */

}


/* Initialize application, called from GUI */
void Init( int argc, char *argv[])
{
   int i;
   
   /* get command line args  */
   for( i = 1; i < argc; i++ ) {

      /* if its not some kind of option selection */
      if( *(argv[i]) != '/' && *(argv[i]) != '-' ) {

         /* you can do something with argv[i] */
         /* e.g. strcpy( filName, argv[i] );  */

         break;
      }

   }


   /* this is where you would initialize any global variables etc. */

   InitPort( &IdlePort );  /* setup a port to use during idle process */

   SetPort( scrnPort );    /* set back to normal port */

   showTarget();           /* display default target */

   initGraph();            /* initialize bar graph */

   drawBackImage();        /* show background image */
   grfovride = 0;

   /* Set up a watchdog timer for the bargraph data timeout */
   barGraphTimeout = wdCreate();
   if (barGraphTimeout == NULL) 
      ffscMsg("Unable to create bargraph timeout timer");
   


   /* display the about menu automatically */
/*   opSelect.selItem = MI_AboutO2;
   AboutMenu(); */

}


/* Close application, called from GUI */
void Close()
{
   /* confirm */
   if( (Dialog( diaNull, "Ok to quit?", NULL )) != Ok ) {
	   opSelect.selectID = nullSelect;
      return;
   }

   /* tell GUI to exit */
   opSelect.selectID = exitSelect;

}


/* what to do when waiting for events, called from GUI */
void IdleProcess()
{
  return;                  /* will use this to display graphics later */
}


/* Re-generate the desktop, called from GUI when it can't restore
the screen */
void RegenDesk()
{

   rect tR;

   DupRect( &scrnPort->portRect, &tR );
   tR.Ymin = barRect.Ymax;

   /* clear off desktop */
   PenColor( colors->desktop );
   if( colors->desktop == White )
      BackColor( Black );
   else
      BackColor( White );
   HideCursor();
   FillRect( &tR, colors->deskpat );
   ShowCursor();

   /* redraw any desktop features */

}

/* Act upon selected menu item, called from GUI */
void ProcessMenu() 
{
   switch( opSelect.selMenu) {

   case MU_Focus:
     FocusMenu();
     break;

   case MU_Action:
     ActionMenu();
     break;
		
   case MU_View:
     ViewMenu();
     break;

   case MU_Config:
     ConfigMenu();
     break;

/*   case MU_About:
     AboutMenu();
     break; */

   }

}

char notestr[] = "current target selection only applies to NMI\nrack all bay all used for other menu commands\0";


void FocusMenu()

{
	char *rack, bay[4], *baySel, *module;
/*	char racklist[] = {'a','l','l','\0','0','\0','1','\0','2','\0','3','\0','\0'}; */
	rackid_t RackList[MAX_RACKS];
	char rackIds[80], moduleIds[80];
	modulenum_t ModuleList[MAX_RACKS * MAX_BAYS];
	int nracks, nmodules, i, ptr;
	int IntTarget;

	switch( opSelect.selItem ) {

 	            case MI_Partition:{
		      int pc = 0;
		      char* thePartList = getPartitionMappings(&pc);
		      ffscMsg("Partition selected -%d found",pc);
		      rack = ViewItemList(thePartList,"select partition");
		      if(rack == NULL){
			showTarget();
			free(thePartList);
			break;
		      }
		      sprintf(dispTarget,"partition %s", rack);
		      strcpy(actionTarget,dispTarget);
		      showTarget();
		      free(thePartList);
		      break;
		    }

		case MI_RackBay:
			if ((nracks = 
			     identEnumerateRacks(RackList, MAX_RACKS)) == 0)
			  break;
			strcpy(rackIds,"all");
			ptr=4;
			for (i=0; i< nracks; i++)
			  ptr += sprintf((char *) (rackIds + ptr),"%d", 
				  (int) RackList[i]) + 1;
			rackIds[ptr] = '\0';
			rack = ViewItemList(rackIds,"select rack");
			if (rack == NULL) {
			  showTarget();
			  break;
			}
			baySel = ViewItemList("all\0upper\0lower\0",
					      "select bay");
			if (baySel == NULL) {
			  showTarget();
			  break;
			}
			bay[0] = baySel[0];
			if (strcmp(baySel, "all") != 0)
			  bay[1] = '\0';
			else {
			  bay[1] = 'l';
			  bay[2] = 'l';
			  bay[3] = '\0';
			}
 			sprintf(dispTarget,"rack %s bay %s", rack, bay);
			strcpy(actionTarget,dispTarget);
			showTarget();
/*			if (strcmp(dispTarget, "rack all bay all"))
			  ViewBuffer(notestr, "Please note!!"); */
			fprintf(stderr, "Current target, %s\n", dispTarget);
			break;

		case MI_Module:
			nmodules = identEnumerateModules(ModuleList, 
							 MAX_RACKS * MAX_BAYS);
			strcpy(moduleIds,"all");
			ptr=4;
			for (i=0; i<nmodules; i++)
			  ptr += sprintf((char *) (moduleIds + ptr),"%d", 
				  (int) ModuleList[i]) + 1;
			moduleIds[ptr] = '\0';
			module = ViewItemList(moduleIds,"select module");
			if (module == NULL) {
			  showTarget();
			  break;
			}
			sprintf(dispTarget,"module %s", module);
			if (strcmp(module, "all")){
				IntTarget = atoi(module);
				sprintf(actionTarget,"module %x", IntTarget);
			}
			else 
				strcpy(actionTarget, dispTarget);
			showTarget();
			break;

/*		case MI_Exit:
			Close();   
			break; */
	}
}

void ActionMenu()

{
	char warning[21];
	dispCmdFFSC[0] = '\0';

	sprintf(warning, "%s?", dispTarget);
	switch( opSelect.selItem ) {

		case MI_PwrU:
                        strcpy(dispCmdFFSC, actionTarget);
			strcat(dispCmdFFSC, " pwr u");
			break;

		case MI_PwrD:
			if( (Dialog( diaNull, "Power down", warning )) == Ok )
			  {
			    strcpy(dispCmdFFSC, actionTarget);
			    strcat(dispCmdFFSC, " pwr d");
			  }
			break;

		case MI_PwrC:
			if( (Dialog( diaNull, "Power cycle", warning )) == Ok )
			  {
			    strcpy(dispCmdFFSC, actionTarget);
			    strcat(dispCmdFFSC, " pwr c");
			  }
			break;

		case MI_Nmi:
			if( (Dialog( diaNull, "NMI", warning )) == Ok )
			  {
			    strcpy(dispCmdFFSC, actionTarget);
			    strcat(dispCmdFFSC, " nmi");
			  }
			break;

		case MI_Rst:
			if( (Dialog( diaNull, "Reset", warning )) == Ok )
			  {
			    strcpy(dispCmdFFSC, actionTarget);
			    strcat(dispCmdFFSC, " rst");
			  }
			break;

	        case MI_Scan:
		        if ( (Dialog( diaNull, "Scan", warning )) == Ok )
			  {
			    strcpy(dispCmdFFSC, actionTarget);
			    strcat(dispCmdFFSC, " scan");
			  }
			break;

	}
	if (strcmp(dispCmdFFSC, "")) {
/*	  fprintf(stderr, "About to issue: %s\n", dispCmdFFSC); */
	  write(D2RReqFD, dispCmdFFSC, strlen(dispCmdFFSC));
	}
}


void ViewMenu()
/*
Handle the View menu
*/
{

	extern int redraw;

	switch( opSelect.selItem ) {

		case MI_Graph:

			eraseScreen();
#ifdef NCLR16
			WritePalette(0,8,15,grfPal);
#endif
			grfovride = 0;
			redraw = 1;
			break;

		case MI_Bkimage:
			grfovride = 1;
			drawBackImage();
			break;

		default:
			break;
  }

}

void ConfigMenu()
/*
Handle the Config menu
*/
{
  char *auth;

  switch( opSelect.selItem ) {
    
  case MI_Cauth:
    auth = ViewItemList("basic\0supervisor\0service\0",
			"set authority");
    if (auth != NULL) {
      sprintf(dispCmdFFSC,"authority %s", auth);
      write(D2RReqFD, dispCmdFFSC, strlen(dispCmdFFSC));
    }
    break;
    
  default:
    break;
    
  }
  
}

/* void AboutMenu() */
/*
Handle the About menu
*/
/*{
   dialogBox abDia;
   int x, y, i, line;
   rect bR;

   switch( opSelect.selItem ) {


      case MI_AboutO2: */

         /* make a dialog box */
/*         line = scrnPort->txFont->lnSpacing; */

         /* center it */
/*         x = scrnPort->portRect.Xmax / 2;
         y = scrnPort->portRect.Ymax / 2;
         i = scrnPort->portRect.Xmax/3;
         SetRect( &bR, x - i, y - (4*line), x+i, y + (4*line ) );
*/

         /* pop up the box */
/*         DupRect( &bR, &abDia.BoundRect );
         ShowDialog( diaBox, &abDia );

         ProtectRect( &bR );
*/
         /* draw the text strings */
/*         BackColor( White );
         PenColor( colors->text );
         TextMode( zREPz );
         TextAlign( alignCenter, alignMiddle );

         y -= 2*line;
         MoveTo(x,y);
         DrawString("Silicon Graphics Inc." );
         y += line; MoveTo(x,y);
         DrawString("Welcome to Origin2000");
         y += line; MoveTo(x,y);
         DrawString("**********************");
*/
         /* wait a bit */
/*         delay( 70 ); */

         /* fade out */
/*         PenColor( Black );
         RasterOp( zORz );
         for( x = 1; x < 8; x++ ) {
            PenPattern( x );
            PaintRect( &bR );
            delay(2);
         }
         EraseRect( &bR );
         PenPattern( 1 );
         ProtectRect( NULL ); */

         /* pop dialog off */
/*         HideDialog( diaBox, &abDia );

         break;

    }


}
*/
