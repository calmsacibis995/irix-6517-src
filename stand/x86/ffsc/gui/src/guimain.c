/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1991            */
/*********************************************/
/* FILE:    guimain.c                        */
/* PURPOSE: module contains main, event,     */
/*          and critical error functions     */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 6    GUIMAIN.C    22-Mar-95    13:53:44 # */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VXWORKS
#include <conio.h>
#include <dos.h>
#endif /* not VXWORKS */

#define GUIMAIN 

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"

#if      TurboC             
extern unsigned _stklen = 14000U;     /* set stack size to 14K */
#endif /*TurboC*/


#ifdef VXWORKS
void guimain( int argc, char *argv[] )      
#else /* not VXWORKS */
void main( int argc, char *argv[] )      
#endif /* VXWORKS */
{

   /* initialize */
   InitGUI( argc, argv );
   InitMenu();
   InitDialog();
   
   /* setup critical error handler */
#if MicrosoftC | WatcomC 
   _harderr( CriticalError );
#endif

#if TurboC
   harderr( CriticalError );
#endif

   /* initialize application */
   Init( argc, argv ); 
    
   /* main event loop */
   do {  /* until exit selected */

      opSelect.selectID = nullSelect;

      do {  /* until something selected */

         /* wait for an event */
         KeyGUIEvent( True );

         /* act on event */
         switch( opEvent.eventType ) {

            case evntPRESS:

               /* check if press event was in Menu Bar */
               /* might end up selecting something    */
               CheckIfMenu();
               break;
            
            case evntKEYDN:

               /* control C selects an exit */
               if( opEvent.eventChar == 03 ) {
                  opSelect.selectID = exitSelect;
                  break;
               }

	            /* check if Menu Bar key */
	            /* might end up selecting something    */
	            if( !CheckIfMenu() ) {
                  /* wasn't menu key, pass on as a key selection */
                  opSelect.selectID = keySelect;
                  opSelect.selChar = opEvent.eventChar;
               }

               break;

            default:
               break;

         }   /* end of switch */

      } while( opSelect.selectID == nullSelect ); 

      /* ok, operator selected something */
      switch ( opSelect.selectID ) {

         case menuSelect:
            ProcessMenu();
            break;

         case exitSelect:
            Close();
            break;

         case keySelect:
            break;
      }

   } while( opSelect.selectID != exitSelect );


   /* clean up and exit */
   SetDisplay( TextPg0 );
   StopGraphics();
   
}

void abend( int errnbr, char *errmsg)
/*
Clean up and print an error message and error number, then exit 
*/
{
   int i;
   char *s;

   StopMouse();
   StopEvent();
   SetDisplay(TextPg0); 
   StopGraphics();
   
   printf("\n\nAbormal End Error #%d\n", abs(errnbr) );

   s = "\0";
   switch( errnbr ) {

      case ER_NODRVR:
         s = "MetaWINDOW driver error";
         break;
      case ER_MEM:
         s = "Not Enough Memory!";
         break;
      case ER_LOAD:
         s = "Unable to load file";
         break;
   }


   if( errmsg == NULL )
      errmsg = "";

   printf("    %s %s\n", s, errmsg );
   i = QueryError();
   printf("    QueryError = %d/%d\n\n", i >> 7, i & 127 );
   exit( 8 );

}

   

#if WatcomC
   int far CriticalError( unsigned deverror, unsigned errcode, unsigned far *devhdr)
#else
   int CriticalError( int deverror,  int errcode )
#endif
/*
DOS critical error handler (INT 0x24 )
*/
{
   char *s, buf[25];

   /* re-enable interrupts */
#if  TurboC
   enable();
#endif
#if  MicrosoftC | WatcomC 
   _enable();
#endif

   switch( errcode & 0x0FF ) {
   
      case 0: s = "Write Protect";    break;
      case 1: s = "Unknown Unit";     break;
      case 2: s = "Drive not Ready";  break;
      case 4: s = "CRC Error";        break;
      case 6: s = "Seek Error";       break;
      case 7: s = "Bad Media Type";   break;
      case 8: s = "Sector Not Found"; break;
      case 10: s = "Write Fault";     break;
      case 11: s = "Read Fault";      break;
      case 12: s = "General Failure"; break;

      case 3:
      case 5:
      case 9: s = "Woo Doggies!";     break;
   }

   if( deverror & 0x8000 )
      strcpy( buf, "DOS Error" );
   else
      sprintf( buf, "Disk Error, Drive %c:", (deverror & 0x0F) + 'A' );

   /* put cursor back to normal if was watch */
   CursorStyle( curCursor );

   Dialog( diaAlert, buf, s );

   /* return a general error to the application program */
#if  TurboC
   hardretn( 31 );
#endif
#if  MicrosoftC  
   _hardretn( 31 ); 
#endif
#if  WatcomC 
   _hardresume( 31 ); 
#endif

   return 0;

}



/* End of File  -  GUIMAIN.C */


