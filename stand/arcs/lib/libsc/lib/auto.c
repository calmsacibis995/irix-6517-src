#ident	"lib/libsc/lib/auto.c:  $Revision: 1.34 $"

/*
 * auto.c - autoboot after delaying 
 */

#include <setjmp.h>
#include <stringlist.h>
#include <saioctl.h>
#include <pause.h>
#include <sys/errno.h>
#include <arcs/io.h>
#include <arcs/signal.h>
#include <libsc.h>

#include <gfxgui.h>
#include <style.h>

extern char *make_bootfile(int);

static char *defaultmsg = "\n\nStarting up the system...\n\n";

static jmp_buf auto_buf;

/*ARGSUSED*/
static void
stopCallBack(struct Button *b, __scunsigned_t button)
{
	longjmp(auto_buf,1);
}

/*
 * autoboot -- jumped to early in initialization to perform autoboot seq
 *             also used by menu option #1 and the auto command
 */
int
autoboot(int delay, char *message1, char *message2)
{
    struct Dialog *d=0;
    int dogfx = isGuiMode();
    struct string_list newargv;
    char *bf;
    char *oldintr;
    char *startingmsg;
    int bfcount = 0;
    SIGNALHANDLER oldhandler;
    LONG rc;

    startingmsg = message1 ? message1 : defaultmsg;

    ioctl(StandardIn,TIOCSETXOFF,0);

    oldintr = (char *)ioctl (0, TIOCINTRCHAR, (long)"\003\033");/* ^C and ESC */
    oldhandler = Signal (SIGINT, (void (*)(void))stopCallBack);

    if (rc=setjmp(auto_buf)) {
	rc = ESUCCESS;
	goto done;
    }

    if (dogfx) {
	d = createDialog(startingmsg,DIALOGPROGRESS,DIALOGBIGFONT);
	moveObject(guiobj(d),d->gui.x1,MSGY1-((d->gui.y2-d->gui.y1)>>1));
	if (delay) {
		struct Button *b;
		b = addDialogButton(d,DIALOGSTOP);
		addButtonText(b,"Stop for Maintenance");
		addButtonCallBack(b, stopCallBack, 1);
	}
		
	drawObject(guiobj(d));
    }
    else {
	p_center();
	p_printf(startingmsg);
    }

    if (message2 && !dogfx) { 			/* optional second message */
	    p_printf (message2);
    }

    /* If requested, delay so the user can interrupt the autoboot
     */
    if (delay && (pause (delay, "\r\n", "\033") == P_INTERRUPT)) {
	rc = ESUCCESS;
	goto done;
    }

    p_ljust();

    for (bf = make_bootfile(1); bf; bf = make_bootfile(0)) {

	bfcount++;

	if(Verbose)
	    printf("Loading %s: ",bf);

	init_str(&newargv);
	if (new_str1(bf, &newargv)) {	/* arg0 is boot device */
	    rc = ENOMEM;
	    goto done;
	}

	if (!getenv("OSLoadOptions"))
	    if (new_str1("OSLoadOptions=auto", &newargv)) {
		rc = ENOMEM;
		goto done;
	    }

	setenv ("kernname", bf);
	if (rc = Execute(bf,(LONG)newargv.strcnt,newargv.strptrs, environ)) {
	    switch(rc) {
	    case EIO:
		printf("\nBoot device not responding: %s.\n", bf);
		break;
	    case ENOENT:
		printf("\nBoot file not found on device: %s.\n", bf);
		break;
	    case ENXIO:
		printf("\nNo volume header on device: %s.\n", bf);
		break;
	    default:
		perror(rc,"Unable to load bootfile");
		break;
	    }
	} else {
	    /* execution succeeded, no need to go further
	     */
	    break;
	}
    }

    /* If we get through to here, either all bootfiles failed, 
     * there were no bootfiles, or one booted and returned.
     */

    if (bf)			/* bootfile succeeded */
	rc = ESUCCESS;
    else if (bfcount)		/* no appropriate bootfile found */
	rc = EINVAL;
    else			/* no bootfiles executed properly */
	rc = EIO;

    /* Restore old interrupts handling
     */
done:
    ioctl (0, TIOCINTRCHAR, (long)oldintr);
    Signal (SIGINT, oldhandler);
    if (d && isGuiMode()) deleteObject(guiobj(d));

    return (int)rc;
}
