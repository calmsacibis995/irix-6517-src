#include <stdio.h>
#include <string.h>
#include "xperform.h"
#include "xtools.h"

#define TOKEN " \t\n"
#define DEF_X	10
#define DEF_Y	10

int gPress ;
int gRelease ;

typedef struct cmd_t
{
	char * name ;
	int    id ;
} CMD ;

enum 
{ 
	ARG_NULL, ARG_WINDOW, ARG_X, 
	ARG_Y, ARG_TIME, ARG_DELAY, ARG_ICON, ARG_WAIT,
	ARG_PRESS, ARG_RELEASE,
	ARG_CLICK, ARG_DBLCLICK, ARG_SLEEP, ARG_KILL
} ;

enum
{
	TYPE_NULL, TYPE_CLICK, TYPE_KILL, TYPE_SLEEP
} ;


/*-----------------------------------------------------------------*/

int
InvalidLine(void)
{
	int rval = 1 ;

	switch( gType )
	{
		case TYPE_CLICK:
			if( theWindow[0] || !(gX < -1 || gY < -1) ) rval = 0 ;
			break ;
				
		case TYPE_KILL:
			if( theWindow[0] ) rval = 0 ;
			break ; 

		case TYPE_SLEEP:
			if( gSleep != -1 ) rval = 0 ;
			break ;

		default:
			break ;
	}

	return rval ;
}

/*-----------------------------------------------------------------*/

void
HandleLine(void)
{
	switch( gType )
	{
		case TYPE_CLICK:
#ifdef DEBUG
	fprintf(stderr, "click.\n") ;
#endif	/*	#ifdef DEBUG	*/
			if( !theWindow[0] && (gX < -1) && (gY < -1) )
				break ;
			if( gX == -1 )	gX = DEF_X ;
			if( gY == -1 )	gY = DEF_Y ;
			ButtonClick(gPress, gRelease) ;
			HandleTiming() ;
			break ;

		case TYPE_KILL:
#ifdef DEBUG
	fprintf(stderr, "kill.\n") ;
#endif	/*	#ifdef DEBUG	*/
			if( !theWindow[0] )	break ;
			KillWindow() ;
			break ; 

		case TYPE_SLEEP:
#ifdef DEBUG
	fprintf(stderr, "sleep.\n") ;
#endif	/*	#ifdef DEBUG	*/
			if( gSleep < 0 )	break ;
			sleep(gSleep) ;
			break ;
		
		default:
			break ;
	}

	return ;
}

/*-----------------------------------------------------------------*/

void
Reset(void)
{
#ifdef DEBUG
	printf("Reset()\n") ;
#endif
	gX       = -1 ;
	gY       = -1 ;
	gDelay   = DEFAULT_DELAY ;
	gTiming  = 0 ;
	gSleep   = 0 ;
	gClick   = 0 ;
	gType    = TYPE_NULL ;
	gPress   = 0 ;
	gRelease = 0 ;
	strcpy(theIcon, "") ;
	strcpy(theWindow, "") ;
}

/*-----------------------------------------------------------------*/

int
FindArg(CMD *Cmd, char *cmd)
{
	int i ;

	for( i = 0 ; Cmd[i].name ; i++ )
	{
		if( strncmp(cmd, Cmd[i].name, strlen(Cmd[i].name)) == 0 )
			return i ;
	}

	return -1 ;
}

/*-----------------------------------------------------------------*/

void
WhiteSpace(char * buf)
{
	while( *buf++ )
		if( *buf == '+' ) *buf = ' ' ;

	return ;
}

/*-----------------------------------------------------------------*/

void
HandleType(char * type)
{
	int a ;
	CMD	Cmds[] =
	{
		"click",		ARG_CLICK,
		"doubleclick",	ARG_DBLCLICK,
		"sleep",		ARG_SLEEP,
		"kill",			ARG_KILL,
		"press",		ARG_PRESS,
		"release",		ARG_RELEASE,
		(char *)NULL,	(int)NULL
	} ;

	a = FindArg(Cmds, type) ;
	if( a == -1 )	return ;

	switch( Cmds[a].id )
	{
		case ARG_CLICK:
			gType    = TYPE_CLICK ;
			gClick   = 1 ;
			gPress   = 1 ;
			gRelease = 1 ;
			break ;

		case ARG_DBLCLICK:
			gType    = TYPE_CLICK ;
			gClick   = 2 ;
			gPress   = 1 ;
			gRelease = 1 ;
			break ;

		case ARG_RELEASE:
			gType    = TYPE_CLICK ;
			gClick   = 1 ;
			gRelease = 1 ;
			break ;

		case ARG_PRESS:
			gType    = TYPE_CLICK ;
			gClick   = 1 ;
			gPress   = 1 ;
			break ;

		case ARG_SLEEP:
			gType = TYPE_SLEEP ;
			break ;
	
		case ARG_KILL:
			gType = TYPE_KILL ;
			break ;

		default:
			break ;
	}

	return ;
}

/*-----------------------------------------------------------------*/

void
HandleArg(char *arg)
{
	int a ;
	CMD	Cmds[] =
	{
		"window:",		ARG_WINDOW,
		"x:",			ARG_X,
		"y:",			ARG_Y,
		"time:",		ARG_TIME,
		"wait:",		ARG_WAIT,
		"delay:",		ARG_DELAY,
		"icon:",		ARG_ICON,
		(char *)NULL,	(int)NULL
	} ;

	a = FindArg(Cmds, arg) ;
	if( a == -1 )	return ;

	arg += strlen(Cmds[a].name) ;
	switch( Cmds[a].id )
	{
		case ARG_ICON:
			strcpy(theIcon, arg) ;
			WhiteSpace(theIcon) ;
			break ;

		case ARG_WINDOW:
			strcpy(theWindow, arg) ;
			WhiteSpace(theWindow) ;
			break ;

		case ARG_X:
			gX = atoi(arg) ;
			break ;

		case ARG_Y:
			gY = atoi(arg) ;
			break ;

		case ARG_WAIT:
			gSleep = atoi(arg) ;
			break ;

		case ARG_TIME:
			gTiming = atoi(arg) ;
			break ;

		case ARG_DELAY:
			gDelay = atoi(arg) ;
			break ;

		default:
			break ;
	}

	return ;
}

/*-----------------------------------------------------------------*/

int
Parser(char * buf)
{
	char * ptr ;
	int    i ;

    /*  Get rid of all the leading spaces
     ***************************************
     */
	while( isspace(*buf) )
		buf++ ;

	if( !*buf )
		return 1 ;

    /*  Check for ! or # and handle
     ***************************************
     */

	switch( *buf )
	{
		case '#':	/*	ignore comments	*/
			return 1 ;
		
		case '!':	/*	shell escape	*/
        	buf++ ;
        	for( i = 0 ; buf[i] ; i++ )
		    if( buf[i] == '\n' || buf[i] == '#' ) {
                	buf[i] = '\0' ;
                	break ;
		    }

#ifdef DEBUG
		fprintf(stderr, "system(%s)\n", buf);
#endif /* DEBUG */
        	system(buf) ;
			return 1 ;

		case '$':	/*	logged comments	*/
			Log(buf) ;
			return 1 ;

		case '.':	/*	End of test	*/
			return 0 ;
	
		default:
			break ;
	}
	
    /*  Otherwise ...
     ***************************************
     */
#ifdef DEBUG
	fprintf(stderr, "%s\n", buf) ;
#endif	/*	#ifdef DEBUG	*/

	ptr = strtok(buf, TOKEN) ;
	if( !ptr )	return 1 ;

	Reset() ;	/*	Reset all the globals	*/
	HandleType(ptr) ;
	while( ptr = strtok(NULL, TOKEN) )
		HandleArg(ptr) ;
	
	/*	Actually handle everything here	*/

#ifdef DEBUG
	fprintf(stderr, "theWindow: '%s'\n", theWindow) ;
	fprintf(stderr, "theIcon:   '%s'\n", theIcon) ;
	fprintf(stderr, "gX:        %d\n", gX) ;
	fprintf(stderr, "gY:        %d\n", gY) ;
	fprintf(stderr, "gType:     %d\n", gType) ;
	fprintf(stderr, "gSleep:    %d\n", gSleep) ;
	fprintf(stderr, "gDelay:    %d\n", gDelay) ;
	fprintf(stderr, "gTiming:   %d\n", gTiming) ;
	fprintf(stderr, "gClick:    %d\n", gClick) ;
	fprintf(stderr, "gPress:    %d\n", gPress) ;
	fprintf(stderr, "gRelease:  %d\n", gRelease) ;
	fprintf(stderr, "\n") ;
#endif	/*	#ifdef DEBUG	*/

	HandleLine() ;

#ifdef DEBUG
	fprintf(stderr, "ok.\n") ;
	fprintf(stderr, "---------------------------------------\n") ;
#endif	/*	#ifdef DEBUG	*/

	return 1 ;
}
