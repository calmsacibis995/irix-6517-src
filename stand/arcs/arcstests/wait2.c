
/*
 *  wait2.c - test console interrupt signal handling with jumpbuf
 */

#include <setjmp.h>
#include <saioctl.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/signal.h>

#define	AUTOBOOT_DELAY	5

static jmp_buf jb;

void intr_handler (void);

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

    if (setjmp (jb)) {
	printf ("Hit the ^C jumpbuf.\n");
	return 1;
    }

    old_handler = Signal (SIGINT, (SIGNALHANDLER)intr_handler);

    while (inittime + AUTOBOOT_DELAY > GetRelativeTime()) {
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
    longjmp (jb, 1);
}
