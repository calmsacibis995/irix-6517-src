/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1991            */
/*********************************************/
/* FILE:    guimotf.c                        */
/* PURPOSE: module contains routines to      */
/*          handle menu bar selections       */
/*          using Motif like look            */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 4    GUIMOTF.C    10-Nov-93    15:25:06 # */

#include <stdio.h>
#include <stdlib.h>

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"


/* Menu System States */
#define  NOT_PULLD 1    /* only menu bar is hilited, menus not pulled   */
#define  PULL_SAME 2    /* in NOT_PULLD State, hilited selection chosen */
#define  PULLED    3    /* menus are pulled down */

extern menus  GUI_menu[]; /* menu information in application's C file */
extern int    numMenus;   /* # of menu selections (calc'd in C file)  */

static image  *holdImage;   /* pointer to menu backing hold buffer */
static int    selected;     /* Boolean for if item selected        */

/* Alt Key scan code in alphabetical order */
static char   AltKeys[] = {30,48,46,32,18,33,34,35,23,36,37,38,50,
                           49,24,25,16,19,31,20,22,47,17,45,21,44};
static char   menuAltKey[10]; /* Alt Menu Keys for menu selections */
static char   menuKey[10];    /* Menu Keys for menu selections     */
static char   itemKey[10];    /* Menu Keys for menu items          */

static int    menuState;      /* Current state of menu system */
static int    menuHiLite;     /* Current menu bar hilite item */
static int    menuID;         /* Last menu bar hilite         */
static int    itemHiLite;     /* Current menu item hilite     */
static int    itemID;         /* Last menu item hilite        */
static rect   itemRect;       /* last menu item selected      */

static int    click_exit;     /* Boolean for clicking to exit */
static int    exit_choice;    /* holds click to exit choice   */

static int    bevel;          /* thickness of bevel edge      */
static int    vmar, hmar;     /* margins of text to edge      */
static int    charHt, charWid; /* height and width of font    */        

void  HiLiteMenu  (void);
void  ShowMenu    (int);
void  HideMenu    (int);
void  ItemSelect  ( void );
void  ProcessKeys ( void );
char  MenuString  ( char * );


void InitMenu()  /* Draw menu bar and init menu items */
{
   
   int   i, j, wd, maxWd;
   int   mu_slen, mubar_ht, muitem_ht, muitem_wid;
   int   items, itbar_ht, itbar_wid;
   rect  tR, *scrnRect;
   rect  muitemR, itbarR;
   long  l, buffSize;

   scrnRect = &(scrnPort->portRect);
   charHt  = scrnPort->txFont->chHeight;
   charWid = scrnPort->txFont->chWidth;

   /* set values for Motif menus */
   bevel = charHt/5;             /* width of bevel */
   hmar = charWid-(charWid/8);   /* left/right margin from text to rect */
   vmar = charHt/8;              /* top/bottom margin from text to rect */

   muitem_ht  = (2*bevel) + (2*vmar) + charHt;  /* height of menu item */
   mubar_ht = muitem_ht + (4*bevel);    /* height of menu bar */

   SetRect( &barRect, 0,0, scrnRect->Xmax, mubar_ht );
   ProtectRect( &barRect );

   /* fill menu bar rectangle */
   PenColor( colors->bar );
   FillRect( &barRect, 1 );
   /* put 3D bevel on rectangle */
   OnBevel( &barRect );

   /* initialize menu table and menu bar entries */
   PenColor( colors->text );
   BackColor( colors->bar );
   RasterOp( zREPz );
   TextAlign( alignLeft, alignBottom );
   TextFace( cProportional );

   buffSize = 0L;

   /* for each menu */
   for( i = 0; i < numMenus; i++) {
      
      /* set menu bar string */
      mu_slen = StringWidth( GUI_menu[i].menuName );
      muitem_wid = (2*bevel) + (2*hmar) + mu_slen;
      GUI_menu[i].menuWidth = muitem_wid;
      if( i == 0 )
         SetRect( &muitemR, barRect.Xmin + (2*bevel),
                            barRect.Ymin + (2*bevel),
                            barRect.Xmin + (2*bevel) + muitem_wid,
                            barRect.Ymin + (2*bevel) + muitem_ht);
      else 
         muitemR.Xmax = muitemR.Xmin + muitem_wid;

      DupRect( &muitemR, &GUI_menu[i].menuBar );
      MoveTo( muitemR.Xmin + bevel + hmar, muitemR.Ymax - bevel - vmar );

      /* use special function to draw menu keyed string */
      menuKey[i] = MenuString(GUI_menu[i].menuName);

/*		DrawString(GUI_menu[i].menuName);		 FFSC has no use for hotkeys */

      /* change all keys to upper case */
      if( (menuKey[i] > 0x60) && (menuKey[i] < 0x7B) )
         menuKey[i] -= 0x20;

      /* plug in Alt Key scan code equivalent */
      menuAltKey[i] = AltKeys[ (menuKey[i]-65) ];

      /* calculate maximum menu item width, and menu height */
      maxWd = 0;
      for( j = 0; j < GUI_menu[i].menuCount; j++) {
         
         /* Find maximum menu entry width */
         wd = StringWidth( GUI_menu[i].menuEntry[j] );
         if( wd > maxWd )
            maxWd = wd;
      }

      /* figure height and width of menu rectangle */
      items = GUI_menu[i].menuCount;
      itbar_ht  = (2*bevel)+ (items*(4*bevel)) + (items*(2*vmar)) + (items*charHt);
      itbar_wid = (6*bevel) + (4*hmar) + maxWd;

      /* set-up menu rectangle */
      SetRect( &itbarR, muitemR.Xmin,
                        muitemR.Ymax + 1,
                        muitemR.Xmin + itbar_wid,
                        muitemR.Ymax + 1 + itbar_ht);

      DupRect( &itbarR, &GUI_menu[i].menuRect );

      /* lowlite disabled menu bar items */
      if( !( (barFlags >> i) & 1)  ) {

         DupRect( &GUI_menu[i].menuBar, &tR ); 

         PenPattern(3); PenColor( colors->bar ); RasterOp(xREPx);
         PaintRect(&tR);
         PenPattern(1); PenColor( colors->text ); RasterOp(zREPz);
      }

      /* offset to next menu area */
      OffsetRect( &muitemR, muitem_wid, 0 );

      /* calculate largest menu rect image buffer size */
      DupRect( &GUI_menu[i].menuRect, &tR );
      l = ImageSize( &tR );
      if( l > buffSize )
         buffSize = l;

   }

   ProtectRect(NULL);

   /* try to allocate a hold buffer of largest size */
   holdImage = (image *) malloc( (unsigned) buffSize );
   if( holdImage == NULL ) {
      free( holdImage );
      abend( ER_MEM, NULL );
   }

   /* init some global vars */
	menuState = NOT_PULLD;
   menuHiLite = menuID = -1;
   itemHiLite = itemID = -1;

}


int CheckIfMenu()
/*
Trivial check of events in main event loop to see if they are for 
activating the menu system 
*/
{
   int i, x, y;
   
   /* menu selection chosen with mouse ? */
   if( opEvent.eventType == evntPRESS ) {

      x = opEvent.eventX;
      y = opEvent.eventY;

      if( XYInRect( x, y, &barRect) ) { /* in menu bar rect */
      
	      for( i = 0; i < numMenus; i++) {

	         if( XYInRect( x, y, &GUI_menu[i].menuBar ) ) {
	            menuHiLite = i;  /* plug in which menu bar selection */
	            menuState = PULLED;
               itemHiLite = itemID = -1;  /* no menu items hilited */
               WaitMenuSelect();
               return True;
            }

         } /* for numMenus */

      } /* if in menu bar */

   } /* if mouse pressed */


   /* Menu System Awake Key ? */
   if( opEvent.eventScan == AWAKEMU ) {
      menuHiLite = 0;           /* hilite first menu bar selection */
      menuState = NOT_PULLD; 
      itemHiLite = itemID = -1;  /* no menu items hilited */
      WaitMenuSelect();
      return True;
   }  /* Menu System Awake Key */


   /* Alt+Menu Key ? */
   if( opEvent.eventState & ALTKEY ) {  /* trivial check if Alt key */
      for ( i=0; i<numMenus; i++ ) {  
         if( opEvent.eventScan == menuAltKey[i] ){
            menuHiLite = i;     /* plug in menu bar selection */
            menuState = PULLED; 
            itemHiLite = 0;     /* hilite the first menu item */
            itemID = -1;
            WaitMenuSelect();
            return True;
         }
      }
   } /* if Alt key */


   /* nothing pressed to activate menu system */
   return False;

} /* CheckIfMenu */


void WaitMenuSelect()
/*
Pull-down menus until the mouse button is up.
Returns info about menu and item selected if any in opselect.
*/
{

   short  entr_tim;
   rect tR;
   
   /* set some initial values */
   menuID = -1;                  /* last menu selection    */
   selected = False;             /* stay in menu system    */
   click_exit = False;           /* click to exit          */
   entr_tim = opEvent.eventTime;      /* menu system enter time */
                  

   /* Main Menu System Loop */
   /* do until something is selected, or Escape or Awake Key is hit */
   while( !selected ) {
      
      switch( menuState ) {

         /* menus not pulled, only work with highlighted menu bar */
         case NOT_PULLD:
            
            /* Escape or Awake Key hit inside menu system */
/*	         if( (opEvent.eventScan == AWAKEMU
                 && opEvent.eventTime > entr_tim )
                || opEvent.eventChar == 0x1B ) { */

	         if((opEvent.eventScan == AWAKEMU || opEvent.eventChar == 0x1B )
                 && opEvent.eventTime > entr_tim ) {

               DupRect( &GUI_menu[menuHiLite].menuBar, &tR ); 
               ProtectRect(&tR);
               OffBevel(&tR);
               ProtectRect(NULL);
               selected = True;  /* leave menu system */
            }  /* Escape or Awake Key hit inside menu system */

            /* any menu event other than Escape or Awake Key hit */
            HiLiteMenu();
            break; /* menuState == NOT_PULLD */


         /* highlited menu to be pulled */
         case PULL_SAME:
            ShowMenu(menuHiLite);
            menuState = PULLED;
            itemHiLite = 0;
            itemID = -1;
            break;  /* highlited menu to be pulled */

         /* menus pulled */
         case PULLED:
         
            /* Escape or Awake Key hit inside menu system? */
	         if( opEvent.eventScan == AWAKEMU || opEvent.eventChar == 0x1B ) {
               DupRect( &GUI_menu[menuHiLite].menuBar, &tR );
               ProtectRect(&tR);
               OffBevel(&tR);
               ProtectRect(NULL);
               HideMenu(menuHiLite);
               selected = True;  /* leave menu system */
               itemHiLite = itemID = -1;
            } /* Awake Key pressed */

            /* any menu event other than Escape or Awake Key hit */
            HiLiteMenu();

            /* menu items active */
            if( itemHiLite != -1 && ((barFlags >> menuHiLite) & 1) )
               ItemSelect();

            break;  /* menuState == PULLED */

         default:
            break;

      } /* switch on menuState */

      KeyGUIEvent( False ); /* put new event in, or update opEvent */
      ProcessKeys();               /* process keys within menu system */

   } /* end of while( !selected ) */
   /* Main Menu System Loop */


   /* if they ended up selecting a menu */
   if( menuHiLite != -1 ) {

      /* and they selected a menu item  */
      if( itemHiLite != -1) {

         /* record the selection */
         opSelect.selectID = menuSelect;
         opSelect.selMenu  = menuHiLite;
         opSelect.selItem  = itemHiLite;

         /* roll up selected menu */
         DupRect( &GUI_menu[menuHiLite].menuBar,&tR ); 
         ProtectRect(&tR);
         OffBevel(&tR);
         ProtectRect(NULL);
         HideMenu(menuHiLite);

      } /* if menu item chosen */

   } /* if menu selection chosen */
      
} /* WaitMenuSelect */


void ItemSelect()
/*
check for a menu item selected 
*/
{
   
   int  item_ht;
   rect tR;
   

   item_ht  = (2*bevel) + (2*vmar) + charHt;
   /* set a rect for a menu item */
   SetRect( &tR, GUI_menu[ menuHiLite ].menuRect.Xmin + (2*bevel),
                 GUI_menu[ menuHiLite ].menuRect.Ymin + (2*bevel),
                 GUI_menu[ menuHiLite ].menuRect.Xmax - (2*bevel),
                 GUI_menu[ menuHiLite ].menuRect.Ymin + (2*bevel) + item_ht);

   OffsetRect( &tR, 0, (itemHiLite*item_ht) + (itemHiLite*(2*bevel)) );


   if(  itemID != itemHiLite ) {
      if( itemID != -1 ) { 
         ProtectRect( &itemRect );
         OffBevel( &itemRect );
         ProtectRect(NULL);
      }

      DupRect( &tR, &itemRect ); 
      ProtectRect( &itemRect );
      OnBevel( &itemRect );
      ProtectRect(NULL);   
      itemID = itemHiLite;

   }   /* end of highlite item */

} /* ItemSelect */



void
HiLiteMenu()
/*
Highlight selected menu bar and pull down/roll up menus if state is PULLED 
*/
{
   rect tR;

   
   if( menuHiLite != menuID ) { /* menu selection changed */
      
      if( menuID != -1 ) {  /* unless first time in, unhilite last */
         DupRect( &GUI_menu[menuID].menuBar, &tR ); 
         ProtectRect(&tR);
         OffBevel(&tR);
         ProtectRect(NULL);
         if( (menuState == PULLED) && ((barFlags >> menuID) & 1) )
            HideMenu(menuID);
      }

      DupRect( &GUI_menu[menuHiLite].menuBar, &tR );
      ProtectRect(&tR);
      OnBevel(&tR);
      ProtectRect(NULL);
      menuID = menuHiLite;
      if( (menuState == PULLED) && ((barFlags >> menuID) & 1) )
         ShowMenu(menuHiLite);

   } /* menu selection changed */

} /* HiLiteMenu() */




void
ShowMenu( menuChoice )
int   menuChoice;
/*
"Pull down" menu entry 
*/
{
   int   i,flags, charHt, item_ht;
   rect  itbarR, tR;
	extern int grfovride;
	extern int grfovride_sav;
   
   if( menuChoice <= -1 )
      return;

	/* Save the state of the graphic over-ride register and 
	 * Freeze the graphics update 
	*/
	grfovride_sav = grfovride;
	grfovride = 1;

   charHt = scrnPort->txFont->lnSpacing;

   /* read in backing image (add space for shadow ) */
   DupRect( &GUI_menu[menuChoice].menuRect, &tR );
   ProtectRect(&tR); 
   ReadImage(&tR,holdImage);

   PenColor(colors->bar);
   PaintRect(&tR);
   OnBevel(&tR);

   /* Display menu items text lines */
   PenColor( colors->text );
   BackColor( colors->bar );
   RasterOp(zREPz);
   TextAlign( alignLeft, alignBottom );
   TextFace( cProportional );


   flags = GUI_menu[menuChoice].menuFlags;

   item_ht  = bevel + (2*vmar) + charHt;
   DupRect( &tR, &itbarR );

   SetRect( &tR, itbarR.Xmin + (2*bevel),
                 itbarR.Ymin + (2*bevel),
                 itbarR.Xmax - (2*bevel),
                 itbarR.Ymin + (2*bevel) + item_ht);

   for( i = 0; i < GUI_menu[ menuChoice ].menuCount; i++ ) {
      
      MoveTo( tR.Xmin + bevel + (2*hmar), tR.Ymax - bevel - vmar );
      itemKey[i] = MenuString( GUI_menu[menuChoice].menuEntry[i] );

/*		DrawString( GUI_menu[menuChoice].menuEntry[i] ); */

      /* change all keys to upper case */
      if( (itemKey[i] > 0x60) && (itemKey[i] < 0x7B) )
         itemKey[i] -= 0x20;

      /* loLight disabled menu items */
      if( (flags & 1) == 0 ) {
         PenPattern(3); PenColor( colors->bar ); RasterOp(xREPx);
         PaintRect(&tR);
         PenPattern(1); PenColor( colors->text ); RasterOp(zREPz);
      }

      flags = flags >> 1;
      OffsetRect(&tR, 0, item_ht + (2*bevel));
   }

   ProtectRect(NULL);

} /* ShowMenu */


void
HideMenu( menuChoice )
int  menuChoice;
/*
"Roll up" menu entry (unless menuChoice = -1)
*/
{
   rect tR;
	extern int grfovride;
	extern int grfovride_sav;
   
   if( menuChoice > -1 ) {

      /* replace backing image */
      DupRect( &GUI_menu[menuChoice].menuRect, &tR );
      ProtectRect(&tR);
      RasterOp(zREPz);
      WriteImage(&tR, holdImage);
      ProtectRect(NULL);

		/* Restore the state of the graphic over-ride register */
		grfovride = grfovride_sav;

   }

} /* HideMenu */



void
ProcessKeys()
/*
Basically the meat of the menu system.  This is where all the
events are checked and respectfully acted upon.  The general
case (not dependant on the menu state) is checked first, then
dependant on the menu state.
*/
{
   int i, x, y, item_ht, charHt;
   int flags, goneDn;
   rect tR;
   

   /* first check general case keys (menuState not a factor) */

   /* mouse pressed in menu bar ? */
   if( opEvent.eventButtons ) {
      goneDn = 1;
      x = opEvent.eventX;
      y = opEvent.eventY;
      charHt = scrnPort->txFont->lnSpacing;
      
      if( XYInRect(x, y, &barRect) ) {  /* trivial check if in menu bar */

         for( i = 0; i < numMenus; i++) { /* check all menu bar selections */
            
            if( XYInRect( x, y, &GUI_menu[i].menuBar ) ) { /* selection made */
               menuHiLite = i;    /* plug in which menu selection */
               itemHiLite = -1;   /* no item highlighted at this point */

               /* menu selection is highlighted and no menus pulled */
               if ( (i == menuID) && (menuState == NOT_PULLD) ) {
                  menuState = PULL_SAME;
                  return;
               }
               
               menuState = PULLED;

               /* special case of coming back up to menu bar and
                  unhighlighting first menu choice */
               if( itemHiLite != itemID ) { 
                  ProtectRect( &itemRect );
                  OffBevel( &itemRect );
                  ProtectRect(NULL);
                  itemID = itemHiLite;
               }
                  
            } /* if in menu bar selection */

         } /* for each menu bar selection */

      } /* if in menu bar */

   } /* if mouse button pressed */


   /* mouse button up */
   if( (opEvent.eventType == evntRELEASE) && (goneDn) ) {
      goneDn = 0;
      x = opEvent.eventX;
      y = opEvent.eventY;
      
      if( XYInRect( x, y, &barRect) ) {

         /* feature of being able to click on highlighted
            and pulled menu selection to exit out of menu system */
         for( i = 0; i < numMenus; i++) { /* check all menu bar selections */

            if( XYInRect( x, y, &GUI_menu[i].menuBar ) ) { /* selection made */

               if( (click_exit) && (i == exit_choice) ){

                  DupRect( &GUI_menu[menuHiLite].menuBar, &tR ); 
                  ProtectRect(&tR);
                  OffBevel(&tR);
                  ProtectRect(NULL);
                  HideMenu(menuHiLite);
                  selected = True;  /* leave menu system */
                  itemHiLite = itemID = -1;
                  click_exit = 0;
                  return;
               }
               click_exit = 1;
               exit_choice = i;
            }
         }
         itemHiLite = 0;
         itemID = -1;


      } /* if in menu bar */
      else {
         if( XYInRect( x, y, &GUI_menu[ menuHiLite ].menuRect ) ) {
            flags = GUI_menu[menuHiLite].menuFlags >> itemHiLite;
            if( flags & 1 )  { /* make sure it's selectable */
               selected = True;
               return;
            }
         }
         else {
            DupRect( &GUI_menu[menuHiLite].menuBar, &tR );
            ProtectRect(&tR);
            OffBevel(&tR);
            ProtectRect(NULL);
            HideMenu(menuHiLite);
            selected = True;  /* leave menu system */
            itemHiLite = itemID = -1;
            return;
         }
      }
   } /* if mouse button up */

   /* Alt Menu Key */      
   if ( opEvent.eventState & ALTKEY ) { /* trivial check if Alt key */
      
      for( i=0; i<numMenus; i++ ) { /* which menu bar item */
         if( opEvent.eventScan == menuAltKey[i] ) {
            if ( i == menuHiLite )
               menuState = PULL_SAME;
            else {
               menuHiLite = i;    /* plug in which menu bar item */
               menuState = PULLED;     /* set state to pulled down */
               itemHiLite = 0;
               itemID = -1;
            }
         }
      }
   } /* if Alt Menu Key */      

   /* right/left arrow */
   switch( opEvent.eventScan ) {

      case 77: /* right arrow */
         menuHiLite++;
         if ( menuHiLite == numMenus )
            menuHiLite = 0;
/*         itemHiLite = 0; */
         itemHiLite = -1;
         itemID = -1;
         break;

      case 75: /* left arrow */
         menuHiLite--;
         if ( menuHiLite < 0 )
            menuHiLite = numMenus-1;
/*         itemHiLite = 0; */
         itemHiLite = -1;
         itemID = -1;
         break;

      default:
         break;

   } /* switch on opEvent.ScanCode (right/left arrow) */

   /* end of general case keys (menuState not a factor) */

   

   switch( menuState ) {
   
      /* if menus not pulled, look for keys that will pull them */
      case NOT_PULLD:

         /* check Menu keys */
         for( i=0; i<numMenus; i++ ) { /* which menu bar item */

	         if( (opEvent.eventChar > 0x60) && (opEvent.eventChar < 0x7B) )
               opEvent.eventChar -= 0x20; /* change any lower to upper case */

            if( opEvent.eventChar == (char)menuKey[i] ) { /* menu key check */

               if( (i == menuHiLite) && ((barFlags >> i) & 1) ) /* hilited */
                  menuState = PULL_SAME;
               else { /* not hilited */

                  if ( ((barFlags >> i) & 1) ) { /* enabled */
                     menuState = PULLED;     /* set state to pulled down */
                     itemHiLite = 0;
                     itemID = -1;
                  }

                  menuHiLite = i;    /* plug in which menu bar item */

               } /* not hilited */

            } /* menu key check */

         } /* for i<numMenus */

         
         /* check for return or down arrow key on enable menu selections */
         if( ((opEvent.eventChar == 0x0D) || (opEvent.eventScan == 80))
             && ((barFlags >> menuHiLite) & 1))
            menuState = PULL_SAME;

         break; /* if menuState = NOT_PULLD */


      /* menus pulled */
      case PULLED:

      /* mouse pressed on menu item */
	   if( opEvent.eventButtons ) {  /* trivial check - mouse pressed */

         goneDn = 1;
         x = opEvent.eventX;
	      y = opEvent.eventY;
         item_ht  = bevel + (2*vmar) + charHt;
/*         item_ht  = (2*bevel) + (2*vmar) + charHt; */
         /* set a rect for a menu item */
         SetRect( &tR, GUI_menu[ menuHiLite ].menuRect.Xmin + (2*bevel),
                       GUI_menu[ menuHiLite ].menuRect.Ymin + (2*bevel),
                       GUI_menu[ menuHiLite ].menuRect.Xmax - (2*bevel),
                       GUI_menu[ menuHiLite ].menuRect.Ymin + (2*bevel) + item_ht);

         for( i = 0; i < GUI_menu[ menuHiLite ].menuCount; i++)  {
            if( XYInRect( x, y, &tR )  )
               itemHiLite = i;
            OffsetRect( &tR, 0, item_ht + (2*bevel) );
         } /* for each menu item */

         return;

      } /* if mouse pressed */


      /* check for return - select highlighted item */
	   if( opEvent.eventChar == 0x0D ) {
         flags = GUI_menu[menuHiLite].menuFlags >> itemHiLite;
         if( flags & 1 ) { /* make sure it's selectable */
            selected = True;
            return;
         }
      }


      /* check to see if a Menu Item Hot Key pressed */
      for( i = 0; i < GUI_menu[ menuHiLite ].menuCount; i++)  { 

	      if( (opEvent.eventChar > 0x60) && (opEvent.eventChar < 0x7B) )
	         opEvent.eventChar -= 0x20; /* change any lower to upper case */

	      if (( opEvent.eventChar == (char)itemKey[i] ) &&
			  ( opEvent.eventScan != 75) &&
			  ( opEvent.eventScan != 77)) {
	         itemHiLite = i;
	         flags = GUI_menu[menuHiLite].menuFlags >> itemHiLite;
	         if( flags & 1 )  /* make sure it's selectable */
               selected = True;
		      return;
	      }
      }

	   /* down/up arrow */
	   switch( opEvent.eventScan ) {

	      case 80: /* down arrow */
	         itemHiLite++;
	         if( itemHiLite == GUI_menu[menuHiLite].menuCount )
		         itemHiLite = 0;
	         break;

	      case 72: /* up arrow */
	         itemHiLite--;
	         if( itemHiLite < 0 )
		         itemHiLite = GUI_menu[menuHiLite].menuCount-1;
	         break;

	      default:
	         break;

      } /* switch on opEvent.ScanCode (down/up arrow) */

   break; /* if menuState = NOT_PULLD */

   default:
	   break;

   } /* switch on menuState */


}  /* ProcessKeys */


char
MenuString( char *srcstr )
{
   int pos;
   char  hotkey, *p;
   grafPort *currPort;


   /* first get position of and Hot Key */
   p = srcstr;
   for(pos=0; *p++ != MKCHAR; pos++)
      ;
   hotkey = *p;

   /* draw first part */
   DrawText( srcstr, 0, pos );

   /* draw hotkey part */
   GetPort( &currPort );
/*   TextFace( currPort->txFace+cUnderline );
   PenColor( colors->htkey ); */
   DrawText( srcstr, pos+1, 1 );

   /* draw last part */
/*   TextFace( currPort->txFace-cUnderline );
   PenColor( colors->text ); */
   DrawText( srcstr, pos+2, 80 );

   return hotkey;

}


/* End of File - GUIMENU.C */
