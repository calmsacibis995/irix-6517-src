
/*
 *  wait.c - test console interrupt signal handling with flag
 */

#include <saioctl.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/signal.h>

#define	AUTOBOOT_DELAY	5

void intr_handler (void);

static int intr_flag;

int
main(int argc, char **argv, char **envp)
{
    int inittime = GetRelativeTime();
    SIGNALHANDLER old_handler;
    char inchar;
    ULONG nread;
    long rv;

    printf ("Testing wait mechanism.\n");
    printf ("Waiting for %d seconds.\n", AUTOBOOT_DELAY);

    intr_flag = 0;
    old_handler = Signal (SIGINT, (SIGNALHANDLER)intr_handler);

    while (inittime + AUTOBOOT_DELAY > GetRelativeTime()) {
	if (intr_flag) {
	    printf ("Hit ^C.\n");
	    rv = 1;
	    goto timeout;
	}
	    
	if (ESUCCESS == GetReadStatus(StandardIn)) {
	    if (ESUCCESS == Read (StandardIn, &inchar, 1, &nread)) {
		switch (inchar) {
		case '\n':
		case '\r':
		    printf ("Hit return.\n");
		    rv = 0;
		    goto timeout;
		case '\033':
		    /*
		    ** user can break autoboot w/ ESC or ^C
		    */
		    printf ("Hit escape.\n");
		    rv = 1;
		    goto timeout;
		}
	    }
	}
    }

timeout:

    Signal (SIGINT, old_handler);
    return (int)rv;
}

void 
intr_handler (void)
{
    intr_flag = 1;
}
