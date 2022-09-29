#include <arcs/spb.h>
#include <arcs/restart.h>
#include <sys/sbd.h>
#include <arcs/io.h>
#include <setjmp.h>

#include <style.h>
#include <gfxgui.h>

#include <libsc.h>
#include <libsk.h>

/* ------------------------------------------------------------------------- */
struct _modes {
	char *arg;
	char *modename;
	OPENMODE mode;
} modes[] = {
	"r", "OpenReadOnly", OpenReadOnly,
	"w", "OpenWriteOnly", OpenWriteOnly,
	"rw", "OpenReadWrite", OpenReadWrite,
	"cw", "CreateWriteOnly", CreateWriteOnly,
	"crw", "CreateReadWrite", CreateReadWrite,
	"sw", "SupersedeWriteOnly", SupersedeWriteOnly,
	"srw", "SupersedeReadWrite", SupersedeReadWrite,
	"od", "OpenDirectory", OpenDirectory,
	"cd", "CreateDirectory", CreateDirectory,
	0,0,0
};

opendev(int argc, char **argv, char **envp)
{
    struct _modes *m;
    char *devname;
    char *mode;
    int found = 0;
    LONG errno;
    ULONG fd;

    mode = argv[1];
    devname = argv[2];
    if (!devname || !mode || index(devname, '=')) {
	printf ("Usage: opendev [r|w|rw|cw|crw|sw|srw|d|cd] <device>\n");
	return(0);
    }

    for (m = modes; m->arg ; m++) {
	if (!strcmp(m->arg, mode)) {
		found++;
		break;
	}
    }

    if (!found) {
	printf ("Usage: opendev [r|w|rw|cw|crw|sw|srw|d|cd] <device>\n");
	return(0);
    }

    printf ("Opening %s %s.\n", devname, m->modename);

    errno = Open ((CHAR *)devname, m->mode, &fd);

    if (errno) {
	printf("FAILED: %s\n",devname);
	perror(errno, 0);
    }
    else {
	printf ("%s opened OK\n", devname);
	Close (fd);
    }
    return(0);
}

/* ------------------------------------------------------------------------- */

struct commands {
	char *arg;
	void (*func)();
} cmds[] = {
	"Halt", Halt,
	"PowerDown", PowerDown,
	"Restart", Restart,
	"Reboot", Reboot,
	"EnterInteractiveMode", EnterInteractiveMode,
	0,0
};

extern _end[];

exit_test(int argc, char **argv)
{
	struct commands *p;
	void (*f)(void) = 0;

	if (argc != 2) goto bad;

	for (p = cmds; p->arg ; p++) {
		if (!strcmp(p->arg, argv[1])) {
			f = p->func;
			break;
		}
	}

bad:
	if (!f) {
		printf("usage: exit Halt|PowerDown|Restart|Reboot|EnterInteractiveMode\n");
		return(0);
	}

	printf("Calling %s()\n",argv[1]);

	(*f)();
	/*NOTREACHED*/
}

/*-----------------------------------------------------------------------*/

allchars()
{
	int i;
	for (i=32; i <= 255; i++) {
		if (!((i-32) % 70) && (i-32))
			putchar('\n');
		putchar(i);
	}
	putchar('\n');
	return(0);
}

struct ProgressBox *prog;
static jmp_buf cancelljb;
static int percent, tenth;

/*ARGSUSED*/
void
cbhandler(struct Button *a, __scunsigned_t b)
{
	setGuiMode(0,0);
	longjmp(cancelljb,1);
}

static void
dpercent(struct Button *b, __scunsigned_t val)
{
	percent += val;
	if (percent > 100) {
		percent = 100;
		tenth = 0;
		return;
	}
	if (percent < 0) {
		percent = 0;
		tenth = 0;
		return;
	}
	changeProgressBox(prog,percent,tenth);
}
static void
dtenth(struct Button *b, __scunsigned_t val)
{
	tenth += val;
	if (tenth < 0) {
		tenth = 9;
		dpercent(b,-1);
		return;
	}
	if (tenth > 10) {
		tenth = 0;
		dpercent(b,1);
		return;
	}
	changeProgressBox(prog,percent,tenth);
}

progress()
{
#define PGHEIGHT 20
	static char *title = "Testing the progress bar";
	struct Dialog *d;
	struct Button *b, *cancel;

	percent = tenth = 0;
		
	if (setjmp(cancelljb)) {
		EnterInteractiveMode();
	}
	
	setGuiMode(1,GUI_NOLOGO);

	d = createDialog(title,DIALOGPROGRESS,DIALOGBIGFONT);
	cancel = addDialogButton(d,DIALOGCANCEL);
	addButtonCallBack(cancel,cbhandler,1);
	resizeDialog(d,0,DIALOGBUTMARGIN+PGHEIGHT);

#define DBW	30
	b = createButton(cancel->gui.x1 - DBW - BUTTONGAP, cancel->gui.y1,
			 DBW,TEXTBUTH);
	addButtonText(b,"t-");
	addButtonCallBack(b,dtenth,-1);
	b = createButton(b->gui.x1 - DBW - BUTTONGAP, cancel->gui.y1,
			 DBW,TEXTBUTH);
	addButtonText(b,"t+");
	addButtonCallBack(b,dtenth,1);
	b = createButton(b->gui.x1 - DBW - BUTTONGAP, cancel->gui.y1,
			 DBW,TEXTBUTH);
	addButtonText(b,"p-");
	addButtonCallBack(b,dpercent,-1);
	b = createButton(b->gui.x1 - DBW - BUTTONGAP, cancel->gui.y1,
			 DBW,TEXTBUTH);
	addButtonText(b,"p+");
	addButtonCallBack(b,dpercent,1);
	prog = createProgressBox(d->gui.x1+DIALOGBDW+DIALOGMARGIN,
			 cancel->gui.y2+DIALOGBUTMARGIN,
			 d->gui.x2-d->gui.x1-2*(DIALOGBDW+DIALOGMARGIN));

	guiRefresh();

	while(1)
		_scandevs();
}

