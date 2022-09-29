/* 
 *	modem_init.c
 *
 * This function checks that stdout is the tty port, that sash was invoked
 * from autoboot or with a file name, and that a file named "MIstring" exists
 * in the volume header of the boot disk.  If all these conditions are true,
 * then this file is opened and its contents sent to stdout, presumably to
 * perform modem initialization.
 *
 *
 * We have three special characters to process: ',', '\', and '|'.
 *
 * The first, ',', will be used in the standard AT command set style; when
 * detected, it will cause the sender to delay 1 second before continuing
 * with the output data stream.  This is typically used to cause a delay
 * so that a modem can complete a reset, or to insert delays in dialing
 * strings to allow long distance connections to be reliably made.
 *
 * The second, '\', will be used in the typical C/UNIX style as an escape
 * character; the character following will NOT be interpreted but simply
 * placed on the output stream.  This provides a way to insure that all
 * data types can be sent to the modem.
 *
 * The third, '|', is unique to our application.  We want to be able to
 * control the behavior of sash, so we need to have control of its return
 * status.  Since all characters in the file are normally sent to the modem,
 * this means that one more special type is needed.  We will use the decimal
 * digit that follows this character as the "modem_init" return value.
 * (i.e., "|1" will return 1, "|2" will return 2, etc.)  Neither this character
 * or the digit that follows will be echoed out the output stream.  If the
 * digit that follows is NOT a decimal digit, then the "|" character is simply
 * ignored and the following character sent.
 */


#include <arcs/errno.h>
#include <arcs/io.h>
#include <saioctl.h>
#include <libsc.h>

/* We enter a filename similar to: dksc(0,1,8)thefile to get a file
 * out of the volume header.  Note that we have two names here because
 * we do not know which SCSI channel we are booting from.
 * We really should walk the path here, same as for the exec
 * commands; hardcoding names is going to be a problem in new
 * machines...
 */

#define FILENAME1 "dksc(0,1,8)MIstring"
#define FILENAME2 "dksc(1,1,8)MIstring"


extern long GetRelativeTime (void);

static int console_is_gfx (void);
static void pause1 (void);

int modem_init (int argc, char **argv)
    {
    LONG rval;
    char buf[512];
    ULONG fd, cc;
    register char *bp;
    CHAR *name;
    int escFlag, barFlag;
    int retval = 0;
	int beverbose;
	char *dm;


    /* first, ar we on the bebug port ? */
    if (console_is_gfx())
	return (0);

    /* next, was sash invoked with a fileneme ? */
    if (argc < 2)
	return (0);

    if (!((strcmp(argv[1], "OSLoadOptions=auto") == 0) ||
		(strcmp(argv[1], "-a") == 0)) &&
		!((argv[1][0] != '-') && !index(argv[1],'=')))
	return (0);

	dm = getenv("diagmode");
	beverbose = dm && *dm == 'd' && *(dm+1) == 'c';

    /* we are on the TTY port, so try to do modem initialization. */

    name = FILENAME1;			/* try the first name */
    rval = Open (name, OpenReadOnly, &fd);
#ifdef FILENAME2
    if (rval != ESUCCESS)
	{
	name = FILENAME2;		/* then the second if needed */
	rval = Open (name, OpenReadOnly, &fd);
	}
#endif
    if (rval != ESUCCESS)		/* neither worked */
	{
	if(beverbose) printf ("no modem initialization file found\n");
	return (0);
	}

    pause1 ();				/* call this here to sync clock */
    escFlag = 0;			/* clear the escape flag */

    while (((rval = Read (fd, buf, sizeof(buf), &cc))==ESUCCESS) && cc)
	for (bp = buf; bp < &buf[cc]; bp++)
	    {
		if(!*bp)	/* since file usually not a multiple of 512
			bytes, there will be trailing junk, which should normally
			be \0's.  Stop when we see the first null.  At one point
			we didn't round the size in the volhdr up to a multiple of
			512 bytes, now we do, which is why this turned into an issue.
			Fix from and for Jim Foris at GE Medical; assumption is that
			this was the last block in the file */
			break;
	    if ((escFlag == 0) && (*bp == '\\'))
		{
		escFlag = 1;		/* mark this and get next character */
		continue;
		}
	    if ((escFlag == 0) && (*bp == '|'))
		{
		barFlag = 1;		/* mark this and get next character */
		continue;
		}
	    if ((escFlag == 0) && (*bp == ','))
		pause1 ();		/* interpret a "," as a delay marker */
	    else if ((barFlag == 1) && (*bp >= '0') && (*bp <= '9'))
		retval = (int)(*bp - '0'); /* get return status 0..9 */
	    else
		putchar (*bp);		/* everything else writes to stdout */
	    escFlag = barFlag = 0;	/* last, reset the flags */
	    }

    if (rval != ESUCCESS)
		printf ("modem_init: read(%s) error: %d\n", name, rval);

    Close (fd);
    return (retval);
    }


static void pause1 (void)		/* spin for 1 second */
    {
    ULONG inittime = GetRelativeTime();

    while ((inittime + 1) > GetRelativeTime())
	;
    }


static int console_is_gfx (void)
    {
    char *ans = getenv ("console");

    if ((ans == NULL) || (*ans != 'd'))
	return (1);

    return (0);
    }
