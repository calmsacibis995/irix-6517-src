#ifndef _xperform_h
#define _xperform_h


extern int    gX ;
extern int    gY ;
extern int    gDelay ;
extern int    gTiming ;
extern int    gSleep ;	/*  Time to sleep       */
extern int    gClick ;	/*	Number of clicks to do	*/
extern int    gType ;	/*	Click, Sleep, etc ..	*/
extern char   theWindow[256] ;
extern char   theIcon[256] ;
extern char * theProgram ;
extern char * theDisplay ;

#define DEFAULT_CONFIG  "xperform.cfg"
#define DEFAULT_OUTPUT  "log"
#define DEFAULT_DELAY   20

extern void KillWindow(void) ;
extern void Log(char *) ;
extern void HandleTiming(void) ;


#endif	/*	#ifndef _xperform_h	*/
