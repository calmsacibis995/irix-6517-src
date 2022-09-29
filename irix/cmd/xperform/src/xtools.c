#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "xperform.h"

extern Display * theDpy ;
static Window fw ;
static char   widget_name_buf[1024] ;
static char   widget_class_buf[1024] ;
static int    got_xerror ;

/*-----------------------------------------------------------------*/

Window
FindWindow(void)
{
    int       i ;

	got_xerror = 0 ;
	fw         = (Window)NULL ;

    widget_name_buf[0] = widget_class_buf[0] = '\0' ;
	ListWindows(theDpy, RootWindow(theDpy, DefaultScreen(theDpy)),
		     	widget_name_buf, widget_class_buf) ;

	return fw ;
}

/*-----------------------------------------------------------------*/

int
ListWindows(Display * dpy, Window w, char * pname, char * pclass)
{
	Window       root ;
	Window       parent ;
    Window *     children = NULL ;
    unsigned int nchildren ;
    int          n ;
    char *       name = NULL ;
    XClassHint   ch ;
    int          pnamelen  ;
	int          pclasslen ;
	int wlen;

    /*  Can't put anything before XFetchName
     ***************************************
     */
    got_xerror = 0 ;
    (void)XFetchName(dpy, w, &name) ;
	if (name) {
	    wlen = strlen(theWindow);
	    if (theWindow[wlen-1] == '*') {
		if (strncmp(name, theWindow, wlen-1) == 0)
		    fw = w;
	    } else {
		if (strcmp(name, theWindow) == 0)
		    fw = w;
	    }
#ifdef DEBUG
	    if (fw)
		fprintf(stderr, "\tfound: %s (0x%lx)\n", theWindow, fw) ;
#endif /* DEBUG */
	}
	
#if 0
/*
	if( name && (strncmp(name, theWindow, strlen(theWindow)) == 0) )
*/
	if( name && (strcmp(name, theWindow) == 0) )
	{
    	/*  The window we want
     	 ***************************************
     	 */
		fw = w ;
#ifdef DEBUG
		fprintf(stderr, "\tfound: %s (0x%lx)\n", theWindow, w) ;
#endif	/*	#ifdef DEBUG	*/
	}
#endif

    ch.res_name  = NULL ;
	ch.res_class = NULL;
    if( got_xerror )	goto done ;

    if( !XQueryTree (dpy, w, &root, &parent, &children, &nchildren) )
    	goto done ;

    /*  Traverse the children
     ***************************************
     */
    pnamelen  = strlen(pname) ;
    pclasslen = strlen(pclass) ;
    for( n = 0 ; n < nchildren ; n++ ) 
	{
		pname[pnamelen]   = '\0' ;
		pclass[pclasslen] = '\0' ;
		ListWindows (dpy, children[n], pname, pclass) ;
	}

  done:
    if( ch.res_name )	XFree(ch.res_name) ;
    if( ch.res_class )	XFree(ch.res_class) ;
    if( name )			XFree((char *)name) ;
    if( children )		XFree((char *)children) ;
    return ;
}

