#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <X11/extensions/XTest.h>
#include <X11/Xlib.h>
#include "parse.h"
#include "xtools.h"
#include "icon.h"

Display * theDpy ;
char * theProgram ;
char * theDisplay ;
char * theFile ;	/*	Test Script file	*/
char * theOutput ;	/*	Log file			*/
char * theConfig ;	/*	Configuration file	*/
int    gX ;
int    gY ;
int    gDelay ;
int    gTiming ;
int    gSleep ;		/*	Time to sleep		*/
int    gClick ;  /*  Number of clicks to do  */
int    gType ;   /*  Click, Sleep, etc ..    */
char   theWindow[256] ;
char   theIcon[256] ;

/*-----------------------------------------------------------------*/

#define STATE_FILE	".xperform"
#define DEFAULT_CONFIG	"xperform.cfg"
#define DEFAULT_OUTPUT	"log"
#define DEFAULT_DELAY	20

/*-----------------------------------------------------------------*/

void ButtonClick(void) ;

/*-----------------------------------------------------------------*/

void
Usage(void)
{
	char **      cpp ;
	static char *help[] =
	{
		"    -display <displayname>      Xserver to contact",
		"    -config <filename>          config file (def=xperform.cfg)",
		"    -output <filename>          log file    (def=log)",
		"    -setup                      setup the software",
		"    -unsetup                    remove the software setup",
		"    -halt                       halt the test",
		"    -start                      start the ball rolling",	
		"",
		NULL
	} ;

	fprintf(stderr, "usage:\n         %s [-options ...]", theProgram) ;
	fprintf(stderr, " < x y [delay] | filename >\n\n") ;
    fprintf(stderr, "where options include:\n") ;
    for( cpp = help ; *cpp ; cpp++ )
        fprintf(stderr, "%s\n", *cpp) ;
    exit(1) ;
}

/*-----------------------------------------------------------------*/

void
Log(char *buf)
{
	static FILE *fp ;
	
	if (theOutput) {
		if (!fp) {
			fp = fopen(theOutput, "a") ;
			if( !fp )
			{
				fprintf(stderr, "*** error: unable to open log file ") ;
				fprintf(stderr, "\"%s\"\n", theOutput) ;
				return ;
			}
		}
		fprintf(fp, "%s", buf) ;
	}

	fprintf(stderr, "%s", buf) ;
}

/*-----------------------------------------------------------------*/

void
ParseArgs(int argc, char *argv[])
{
    int i ;

	theProgram = argv[0] ;
	if( argc < 2 )	Usage() ;
	if( argc == 2 && HandleUtil(&(argv[1][1])) == 1 )
		exit(0) ;

    for( i = 1 ; i < argc ; i++ ) 
    {
        char *arg = argv[i] ;

        if( arg[0] == '-' ) 
        {
            switch( arg[1] ) 
            {
				case 'd':	/*	display	*/
					if( ++i >= argc )	Usage() ;
					theDisplay = argv[i] ;
					break ;
		
				case 'o':	/*	output file	*/
					if( ++i >= argc )	Usage() ;
					theOutput = argv[i] ;
					break ;

				case 'c':	/*	config file	*/
					if( ++i >= argc )	Usage() ;
					theConfig = argv[i] ;
					break ;

				default:
					Usage() ;
			}
		}
		else
			break ;
	}

	/*	One leftover param is a file while
	 *	two are coordinates
	 ***************************************
	 */

	if( i < argc )
		switch( argc - i )
		{
			case 3:	/*	This is an intentional fall through	*/
				gDelay = atoi(argv[i + 2]) ;

			case 2:
				gX = atoi(argv[i]) ;
				gY = atoi(argv[i + 1]) ;
				break ;

			case 1:
				theFile = argv[i] ;
				break ;

			default:
				Usage() ;
				break ;
		}

	return ;
}

/*-----------------------------------------------------------------*/

/*
 * If the child dies, check the exit status.  GL programs often fork,
 * so we don't give up on looking for a Map event unless the exit
 * status is non-zero.
 */
void
ChildHandler(int sig)
{
    pid_t pid ;
    int   stat ;

    pid = waitpid(-1, &stat, 0) ;
    if( pid != -1 && (!WIFEXITED(stat) || WEXITSTATUS(stat) != 0) )
    {
		Log("Child exited with bad status before map event!\n") ;
        exit(1) ;
    }
}

/*-----------------------------------------------------------------*/

void
KillWindow(void)
{
	Window    w ;

	w = FindWindow() ;
	if( !w )	return ;
	
	XDestroyWindow(theDpy, w) ;
	
	return ;
}

/*-----------------------------------------------------------------*/

int
DispatchEvent(XEvent *event, struct timeval *stop)
{
	if( (event->type == MapNotify &&
	     !event->xmap.override_redirect) ||
	   (event->type == ConfigureNotify &&
	    !event->xconfigure.override_redirect) )
/*
	if( event->type == MapNotify && event->xconfigure.override_redirect == False )
*/
	{
		return 1 ;
/*
		gTiming-- ;
		if( !gTiming )
		{
			gettimeofday(stop) ;
			return 1 ;
		}
*/
	}

	return 0 ;
}

/*-----------------------------------------------------------------*/

void
TimeWindow(Display *theDpy, struct timeval *start, struct timeval *stop)
{
    XEvent         event ;
    int            msec ;
	fd_set         fdSet ;
	int            connNum ;
	struct timeval timeOut ;
	int            i ;

	connNum         = ConnectionNumber(theDpy) ;
	timeOut.tv_sec  = 60 ;	/*	Time out after trying for a minute	*/
	timeOut.tv_usec = 0 ;

    gettimeofday(start) ;

for( i = 0 ; i < gTiming ; i++ )
{
    while( 1 )
    {
		while( XPending(theDpy) )
		{
        	XNextEvent(theDpy, &event) ;
			if( DispatchEvent(&event, stop) )
			{
				goto done ;
			}
		}

		FD_ZERO(&fdSet) ;
		FD_SET(connNum, &fdSet) ;

		if( select(connNum + 1, &fdSet, 0, 0, &timeOut) > 0 )
		{
        	XNextEvent(theDpy, &event) ;
			if( DispatchEvent(&event, stop) )
			{
 				goto done ;
			}
		}
		else
		{
			Log("Error in Event loop\n") ;
			goto done ;
		}
    }

done:
	;
}
	gettimeofday(stop) ;

    msec  = stop->tv_sec  * 1000 + stop->tv_usec  / 1000 ;
    msec -= start->tv_sec * 1000 + start->tv_usec / 1000 ;

	{
		char buff[256] ;
    	sprintf(buff, "Startup time: %d milliseconds\n", msec) ;
		Log(buff) ;
	}

    return ;
}

/*-----------------------------------------------------------------*/

void
HandleTiming(void)
{
	struct sigaction act ;
	struct sigaction sa ;
	struct timeval   start ;
	struct timeval   stop ;
	
	if( !gTiming ) return ; 

	gettimeofday(&start) ;

#ifndef DUMMY
	XSelectInput(theDpy, DefaultRootWindow(theDpy),
				SubstructureNotifyMask) ;

	act.sa_handler = ChildHandler ;
	act.sa_flags   = 0 ;
	sigemptyset(&act.sa_mask) ;
	sigaction(SIGCHLD, &act, NULL) ;

	TimeWindow(theDpy, &start, &stop) ;
#endif
}

/*-----------------------------------------------------------------*/

void
ButtonClick(void)
{
	Window    w = (Window)NULL ; 
	int       x ;
	int       y ;

	if( theIcon[0] )
	{
		if( FindIcon(theIcon, &x, &y) )
		{
			gX += x ;
			gY += y ;
		}
	}
		
	if( theWindow[0] )
	{	
		w = FindWindow() ;
		if( !w )
		{
			char buff[256] ;
			sprintf(buff, "*** error: unable to find window \"%s\"\n", 
					theWindow) ;
			Log(buff) ;
			return ;
		}
	}
	
	if( w )
	{
		Window child ;
		int    rx ;
		int    ry ;

		if( XTranslateCoordinates(theDpy, w, RootWindow(theDpy, DefaultScreen(theDpy)),
									0, 0, &rx, &ry, &child) )
		{
			gX += rx ;
			gY += ry ;
		}
		else
		{
			char buff[256] ;
			sprintf(buff, "*** error: unable to translate coordinates\"%s\"\n", 
					theWindow) ;
			Log(buff) ;
			return ;
		}
	}

	while( gClick-- )
	{
#ifdef DEBUG
fprintf(stderr, "\tclicking (%d, %d)\n", gX, gY) ;
#endif /* DEBUG */
#ifndef	DUMMY
		XTestFakeMotionEvent(theDpy, 0, gX, gY, gDelay) ;
		XFlush(theDpy);
		XTestFakeButtonEvent(theDpy, Button1, 1, gDelay) ;
		XFlush(theDpy);

		if (gDelay)
		    sginap(gDelay);

		XTestFakeButtonEvent(theDpy, Button1, 0, gDelay) ;
		XFlush(theDpy);
#endif /* !DUMMY */
	}

	return ;
}

/*-----------------------------------------------------------------*/

void
DoFile(void)
{
	/*	Logic: We want first to open our
	 *	state file to figure out just what
	 *	we're supposed to do during this
	 *	iteration.  We do it and write the
	 *	new offset to the state file and
	 *	exit.
	 ***************************************
	 */

	FILE * fp ;
	char   buf[256] ;
	long   offset = 0 ;
	int    rval ;
	int    done = 0 ;
		
	/*	First, get the offset if there is 
	 *	one
	 ***************************************
	 */

	fp = fopen(STATE_FILE, "r") ;
	if( fp )
	{
		Log("\n") ;
		if( fscanf(fp, "%ld", &offset) != 1 )
		{
			char buff[256] ;
			fclose(fp) ;
			sprintf(buff, "*** error: corrupt state file \"%s\"\n",
					STATE_FILE) ;
			Log(buff) ;
			exit(1) ;
		}
		else
			fclose(fp) ;	
	}
	else
	{	
		char   buff[256] ;
		time_t t ;

		time(&t) ;
		sprintf(buff, "#\n# %s started %s#\n\n", 
				theProgram, ctime(&t)) ;
		Log(buff) ;
	}

	/*	Do the main loop now to do what
	 *	needs to be done
	 ***************************************
	 */

	fp = fopen(theFile, "r") ;
	if( !fp )
	{
		char buff[256] ;
		sprintf(buff, "*** error: unable to open script file \"%s\"\n",
				theFile) ;
		Log(buff) ;
		exit(1) ;
	}

	rval = fseek(fp, offset, SEEK_SET) ;
	if( rval )
	{
		char buff[256] ;
		sprintf(buff, "*** error: seek position out of bounds %ld in",
				offset) ;
		sprintf(buff + strlen(buf), " file \"%s\"\n", theFile) ;
		Log(buff) ;
		fclose(fp) ;
		exit(1) ;
	}

	/*	This is the main loop, I need to put an event loop here	*/

	while( fgets(buf, sizeof(buf) - 1, fp) )
	{
/*
		while( XPending(theDpy) )
*/
			XSync(theDpy, True) ;
		if( !Parser(buf) )
		{
			FILE *fstate ;

			offset = ftell(fp) ;
			fstate = fopen(STATE_FILE, "w") ;
			if( !fstate )
			{
				char buff[256] ;
				sprintf(buff, "*** error: couldn't open") ;
				sprintf(buff + strlen(buff), " \"%s\" for writing\n", STATE_FILE) ;
				fclose(fp) ;
				exit(1) ;
			}
			fprintf(fstate, "%ld\n", offset) ;
			fclose(fstate) ;
			goto done ;
		}
	}

	/*	If we get to this point, then we've
	 *	finished the script.  To indicate 
	 *	that we're done, we want to unlink
	 *	the /etc/autologin.on file.
	 ***************************************
	 */
	done = 1 ;
	unlink(STATE_FILE) ;
 	unlink("/etc/autologin.on") ; 
	{
		char   buff[256] ;
		time_t t ;

		time(&t) ;
		sprintf(buff, "#\n# %s finished %s#\n\n", 
				theProgram, ctime(&t)) ;
		Log(buff) ;
	}

done:	/*	yeah yeah yeah, I know	*/

 	system("echo > /etc/autologin.on") ; 
	fclose(fp) ;
	return ;
}

/*-----------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	gDelay    = DEFAULT_DELAY ;
 	theOutput = DEFAULT_OUTPUT ;
	theConfig = DEFAULT_CONFIG ;

	ParseArgs(argc, argv) ;

	theDpy = XOpenDisplay(theDisplay) ;

	if( theFile )
		DoFile() ;
	else
		ButtonClick() ;

	XCloseDisplay(theDpy) ;
}
