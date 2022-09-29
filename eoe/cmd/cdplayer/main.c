/*
 * main.c
 *
 * Description:
 *       Main program for the cd audio tool
 *
 * History:
 *      rogerc      10/29/90    Created
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <locale.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/BulletinB.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/MessageB.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Frame.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "display.h"
#include "skip.h"
#include "cue.h"
#include "seldbox.h"
#include "progdbox.h"
#include "cddata.h"
#include "database.h"
#include "icon"

#define UPDATEINTERVAL 250
#define TIMEINTERVAL 2000

static struct mainwin_tag {
	Widget  play, stop, open, quit, skipf, skipb, ff, rew, time;
	Widget  data, repeat, shuffle, program, clear;
}   mainwin;

typedef struct _resource_struct {
	XmFontList  font_list;
	XmString    label_string;
	GC          gc;
}   RESOURCESTRUCT;

static void shuffle( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
static void normal( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
static void program( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
static void repeat ( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
void set_button_bitmap( Widget button, char *bits, Dimension width,
 Dimension height );
void play( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data );
void stop( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data );
void open_drawer( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
void time_display( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
void timer_revert( CLIENTDATA *clientp, XtIntervalId *id );
void quit_arm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data );
void quit_activate( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
void quit_disarm( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
void update( CLIENTDATA *clientp, XtIntervalId *id );
void enable_buttons( int state );
Widget create_button_label( Widget parent, char *name, Arg *create_args,
 int ncreate_args );
void draw_button_label( Widget w, RESOURCESTRUCT *r,
 XmDrawingAreaCallbackStruct *cb );
void usage( void );
static void terminate( void );
static void PostError(Widget parent, char *errmsg, int fatal);
static char * GetResourceString(Widget w, char *name, char *class, char *def);

static CDPLAYER *cdplayer;

/*
 *  main( int argc, char *argv[] )
 *
 *  Description:
 *      Contains the main loop for the CD Audio tool
 *
 *  Parameters:
 *      argc        Number of command line arguments
 *      argv        Command line argument vector
 *
 *  Returns:
 *      never
 */

main( int argc, char *argv[] )
{
	Widget          toplevel, mainForm, form, disprc, buttonform;
	Widget          separator;
	Widget          play_label, stop_label, skip_label, cue_label;
	Widget          children[30];
	CLIENTDATA      client;
	CDSTATUS        status;
	Arg             wargs[10];
	int             n, num_children, i;
	char            *dev = NULL;
	Display         *dpy;
	unsigned long   white, black;
	Dimension       height;
	Pixmap          icon;
	static XtResource   icon_resource = {
		"icon",
		"Icon",
		XmRPrimForegroundPixmap,
		sizeof (Pixmap),
		0,
		XtRImmediate,
		(caddr_t)XmUNSPECIFIED_PIXMAP
	};
	static const char *databasePath = NULL, *databaseCDir = NULL;
	static XtResource dbaseResources[] = {
	    { "databasePath", "DatabasePath", XmRString, sizeof(String),
		  (unsigned int)&databasePath, 0, NULL },
	    { "databaseCDir", "DatabaseCDir", XmRString,
		  sizeof(String), (unsigned int)&databaseCDir, 0, NULL }
	};
	static XrmOptionDescRec cmdLineOptions[] = {
	    {"-dbpath",	"*databasePath",    XrmoptionSepArg, NULL}, 
	    {"-dbcdir",	"*databaseCDir",    XrmoptionSepArg, NULL}, 
	};
	int rv;
	Pixel fg, bg;
	int error;

/*	mallopt(M_DEBUG,0xffff);*/
	memset( &client, 0, sizeof (client) );

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-dev") == 0) {
			if (i == argc - 1) {
				usage();
			}
			dev = argv[i + 1];
			break;
		}
	}

	error = 0;
	if (!(client.cdplayer = CDopen( dev, "r" ))) {
		/* Save oserror() for later use */
		error = oserror();
		if (error == 0) {
			/* Make sure it's non-zero! */
			error = ENOENT;
		}
	}

	/*
	 * Plug a security hole; this program is setuid, and it writes
	 * to the file ~/.cdplayerrc, so all kinds of awful things
	 * could happen with symbolic links if we didn't do this.
	 * cdplayer has to be setuid so that it can open /dev/scsi files.
	 */
	setuid(getuid());

	/*
	 * Don't do this i18n stuff until after setuid.
	 */
	setlocale(LC_ALL, "");
	XtSetLanguageProc(NULL, NULL, NULL);

	/*
	 * Now report error if there was one.
	 */
	if (error) {
		setoserror(error);
		perror(dev ? dev : "CDROM");
		exit(1);
	}

	toplevel = XtInitialize( argv[0], "Cdplayer", cmdLineOptions,
				XtNumber(cmdLineOptions), &argc, argv );

	client.toplevel = toplevel;

	XtGetApplicationResources(toplevel, NULL, dbaseResources,
				  XtNumber(dbaseResources), NULL, 0);
	
	rv = db_init_pkg(databasePath, databaseCDir);

	if (rv & DB_NEED_DIR) {
	    if (!databaseCDir) {
		databaseCDir = db_get_dflt_dir();
	    }
	    if (mkdir(databaseCDir, 0777) < 0) {
#ifdef DEBUG
		perror(databaseCDir);
#endif
	    }
	}

	cdplayer = client.cdplayer;
	atexit( terminate );

	CDgetstatus( client.cdplayer, &status );
	client.status = &status;

	client.data = db_init_from_cd( client.cdplayer, client.status );

	n = 0;
	XtSetArg( wargs[n], XtNiconPixmap, &icon ); n++;
	if (icon == None) {
		n = 0;
		XtSetArg( wargs[n], XtNiconPixmap,
		 XCreateBitmapFromData( XtDisplay( toplevel ),
		  XtScreen( toplevel )->root,
		  icon_bits, icon_width, icon_height ) ); n++;
		XtSetValues( toplevel, wargs, n );
	}

	n = 0;
	mainForm = XmCreateForm( toplevel, "mainForm", wargs, n );
	XtManageChild( mainForm );
	
	n = 0;
	XtSetArg( wargs[n], XmNorientation, XmVERTICAL ); n++;
	XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	disprc = XmCreateRowColumn( mainForm, "disprc", wargs, n );
	XtManageChild( disprc );

	create_display( disprc, &client );

	form = XmCreateForm( disprc, "displayButtons", NULL, 0 );
	XtManageChild( form );

	num_children = 0;

	children[num_children++] = mainwin.program
	    = XmCreatePushButton( form, "program", NULL, 0 );

	children[num_children++] = mainwin.shuffle
	    = XmCreatePushButton( form, "shuffle", NULL, 0 );

	children[num_children++] = mainwin.clear
	    = XmCreatePushButton( form, "clear", NULL, 0 );

	n = 0;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNtopWidget, mainwin.program); n++;
	children[num_children++] = mainwin.time
	    = XmCreatePushButton( form, "time", wargs, n );

	n = 0;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNtopWidget, mainwin.shuffle); n++;
	children[num_children++] = mainwin.repeat
	    = XmCreatePushButton( form, "repeat", wargs, n );

	n = 0;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNtopWidget, mainwin.clear); n++;
	children[num_children++] = mainwin.data
	    = XmCreatePushButton( form, "data", wargs, n );

	XtManageChildren( children, num_children );

	num_children = 0;

	n = 0;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	buttonform = XmCreateForm( mainForm, "controlButtons", wargs, n );
	XtManageChild( buttonform );

	n = 0;
	XtSetArg(wargs[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNrightWidget, buttonform); n++;
	XtSetValues(disprc, wargs, n);

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNrecomputeSize, False ); n++;
	children[num_children++] = play_label = XmCreateLabel( buttonform,
	 "playLabel", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNheight, &height ); n++;
	XtGetValues( play_label, wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, play_label ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNrecomputeSize, False ); n++;
	children[num_children++] = stop_label = XmCreateLabel( buttonform,
	 "stopLabel", wargs, n );

	n = 0;
	XtSetArg(wargs[n], XmNforeground, &fg); n++;
	XtSetArg(wargs[n], XmNbackground, &bg); n++;
	XtGetValues(stop_label, wargs, n);

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, play_label ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.play =
	 XmCreatePushButton( buttonform, "play", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, stop_label ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.stop =
	 XmCreatePushButton( buttonform, "stop", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, mainwin.play ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNheight, height ); n++;
	XtSetArg(wargs[n], XmNforeground, fg); n++;
	XtSetArg(wargs[n], XmNbackground, bg); n++;
	children[num_children++] = skip_label
		= create_button_label(buttonform, "skipLabel", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, skip_label ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.skipb =
	 XmCreatePushButton( buttonform, "skipb", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, skip_label ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, mainwin.skipb ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.skipf =
	 XmCreatePushButton( buttonform, "skipf", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, mainwin.skipb ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNheight, height ); n++;
	XtSetArg(wargs[n], XmNforeground, fg); n++;
	XtSetArg(wargs[n], XmNbackground, bg); n++;
	children[num_children++] = cue_label = create_button_label( buttonform,
	 "cueLabel", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, cue_label ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.rew
		= XmCreatePushButton( buttonform, "rew", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, cue_label ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, mainwin.rew ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.ff = XmCreatePushButton( buttonform,
	 "ff", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, mainwin.rew ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = separator = XmCreateSeparator( buttonform,
	 "separator", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, separator ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.quit =
	 XmCreatePushButton( buttonform, "quit", wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNtopWidget, separator ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, mainwin.quit ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = mainwin.open =
	 XmCreatePushButton( buttonform, "open", wargs, n );

	XtManageChildren( children, num_children );

	create_data_dbox( mainwin.data, &client );
	create_program_select_dialog( mainwin.program, &client );

	client.normalButton = mainwin.clear;
	client.shuffleButton = mainwin.shuffle;
	client.programButton = mainwin.program;
	client.time = mainwin.time;
	client.update_func = update;

	XtAddCallback(mainwin.play, XmNactivateCallback, play, &client);
	XtAddCallback(mainwin.stop, XmNactivateCallback, stop, &client);
	XtAddCallback(mainwin.open, XmNactivateCallback, open_drawer,
		      &client);
	XtAddCallback(mainwin.skipf, XmNactivateCallback, skipf, &client);
	XtAddCallback(mainwin.skipb, XmNactivateCallback, skipb, &client);
	XtAddCallback(mainwin.quit, XmNarmCallback, quit_arm, &client);
	XtAddCallback(mainwin.quit, XmNactivateCallback, quit_activate,
		      &client); 
	XtAddCallback(mainwin.quit, XmNdisarmCallback, quit_disarm,
		      &client);
	XtAddCallback(mainwin.ff, XmNarmCallback, ff_arm, &client);
	XtAddCallback(mainwin.ff, XmNdisarmCallback, ff_disarm, &client);
	XtAddCallback(mainwin.rew, XmNarmCallback, rew_arm, &client);
	XtAddCallback(mainwin.rew, XmNdisarmCallback, rew_disarm, &client);
	XtAddCallback(mainwin.time, XmNactivateCallback, time_display,
		      &client);
	XtAddCallback(mainwin.data, XmNactivateCallback, popup_data_dbox,
		      &client);
	XtAddCallback(mainwin.program, XmNactivateCallback, program, &client);
	XtAddCallback(mainwin.shuffle, XmNactivateCallback, shuffle, &client);
	XtAddCallback(mainwin.clear, XmNactivateCallback, normal, &client);
	XtAddCallback(mainwin.repeat, XmNactivateCallback, repeat, &client);

	XtRealizeWidget(toplevel);

	update(&client, 0);

	XtMainLoop();
}

/*
 *  static void shuffle( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      callback function for the shuffle widget.  Create a random program
 *      and set things up for playing it.
 *
 *  Parameters:
 *      w           The shuffle widget
 *      clientp     client data
 *      call_data   ignored
 */

static void 
shuffle( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDstop( clientp->cdplayer );
	clientp->play_mode = PLAYMODE_SHUFFLE;
	if (clientp->program)
		free_program( clientp->program );
	clientp->program = program_shuffle( clientp->cdplayer );
	set_playmode_text( PLAYMODE_SHUFFLE );
}

/*
 *  static void normal( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      callback function for the normal widget.  Takes us out of shuffle
 *      or program mode, stopping cd if we weren't already in normal mode
 *
 *  Parameters:
 *      w           the normal widget
 *      clientp     client data
 *      call_data   ignored
 */

static void
normal( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	if (clientp->play_mode != PLAYMODE_NORMAL) {
		program_stop( clientp->cdplayer, clientp->program );
		if (clientp->program) {
			free_program( clientp->program );
			clientp->program = 0;
		}
	}
	set_playmode_text( PLAYMODE_NORMAL );
	clientp->play_mode = PLAYMODE_NORMAL;
}

/*
 *  static void program ( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      callback function for the program widget.  Pops up the program
 *      selection dialog box to allow the user to select a program to play.
 *
 *  Parameters:
 *      w
 *      clientp     client data
 *      call_data   ignored
 */

static void
program ( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	select_program( clientp );
}

/*
 *  static void repeat ( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Toggle repeat status
 *
 *  Parameters:
 *      w               The repeat button
 *      clientp         client data
 *      call_data       ignored
 */

static void 
repeat ( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDSTATUS status;

	clientp->repeat_button = ~clientp->repeat_button;
	set_repeat( clientp->repeat_button );

	/*
	 * This makes repeat work when cdplayer is started when the CD
	 * is already playing.  Since the CD was already playing,
	 * play() might not have been called, and thus clientp->repeat
	 * might not have been set.  So we set it here if appropriate.
	 */
	if (CDgetstatus(clientp->cdplayer, &status)
	    && (status.state == CD_PLAYING
		|| status.state == CD_STILL
		|| status.state == CD_PAUSED)) {
		clientp->repeat = 1;
	}
}

/*
 *  void play( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Start play of CD if CD-ROM drive is ready
 *      Continue play of CD if CD is paused
 *      Pause CD if CD is playing
 *
 *  Parameters:
 *      w           The play button
 *      clientp     Client data
 *      call_data   ignored
 */

void
play( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDSTATUS    status;

	CDgetstatus( clientp->cdplayer, &status );
	clientp->repeat = 1;

	switch (status.state) {

	case CD_PLAYING:
	case CD_STILL:
	case CD_PAUSED:
		clientp->user_paused = status.state == CD_PLAYING ? 1 : 0;
		clientp->skipped_back = 0;
		CDtogglepause( clientp->cdplayer );
		break;

	case CD_READY:
		program_playwarn( clientp );
		select_playwarn( clientp );
		switch (clientp->play_mode) {

		default:
		case PLAYMODE_NORMAL:
			if (clientp->start_track < status.first)
				clientp->start_track = status.first;
			if (clientp->start_track > status.last)
				clientp->start_track = status.last;
			CDplay( clientp->cdplayer, clientp->start_track, 1 );
			clientp->start_track = 0;
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			program_play( clientp->cdplayer, clientp->program, 1 );
			break;
		}
		break;
	
	default:
		break;
	}
}

/*
 *  void stop( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Stop the CD
 *
 *  Parameters:
 *      w           The stop button
 *      clientp     Client data
 *      call_data   ignored
 */

void
stop( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	clientp->repeat = 0;
	if (clientp->play_mode == PLAYMODE_PROGRAM ||
	 clientp->play_mode == PLAYMODE_SHUFFLE)
		program_stop( clientp->cdplayer, clientp->program );
	CDstop( clientp->cdplayer );
}

/*
 *  void time_display( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Cycle through the various modes we have for displaying the time.
 *
 *  Parameters:
 *      w           The time button
 *      clientp     Client data
 *      call_data   ignored
 */

void
time_display( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	if (clientp->status->state == CD_READY) {
		clientp->ready_time_display =
		 (clientp->ready_time_display + 1) % (READY_LAST + 1);
		if (clientp->timeid)
			XtRemoveTimeOut( clientp->timeid );
		clientp->timeid = XtAddTimeOut( TIMEINTERVAL, timer_revert,
		 clientp );
	}
	else
		clientp->time_display = (clientp->time_display + 1) %
		 (TIME_LAST + 1);
	draw_display( clientp );
}

/*
 *  void timer_revert( CLIENTDATA *clientp, XtIntervalId *id )
 *
 *  Description:
 *      This is used when CD is stopped and user presses the time button.
 *      The display mode for the time changes until this time out gets called,
 *      at which point it reverts to the default.
 *
 *  Parameters:
 *      clientp     client data
 *      id          time out id
 */

void
timer_revert( CLIENTDATA *clientp, XtIntervalId *id )
{
	clientp->timeid = 0;
	clientp->ready_time_display = READY_NORMAL;
	draw_display( clientp );
}

/*
 *  void open_drawer( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Ejects the CD caddy from the CD-ROM drive
 *
 *  Parameters:
 *      w           the open button
 *      clientp     Client data
 *      call_data   ignored
 */

void
open_drawer( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	if (!CDeject(clientp->cdplayer) && oserror() == EBUSY) {
		/*
		 * In 5.1, it is possible that someone could mount an
		 * efs CD while we are running.  CDeject checks for
		 * this case, and returns failure and sets errno to
		 * EBUSY to tell us that the CD was mounted.  We post
		 * an error message box and exit when the user presses
		 * OK.
		 */
		PostError(clientp->toplevel,
			  GetResourceString(clientp->toplevel,
					    "cdMountedErrorMsg", 
					    "CdMountedErrorMsg",
					    "CD has been mounted"), 1);
	}

	clientp->start_track = 0;
}

/*
 *  void quit_arm( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Clear the quit flag, so that if user disarms the quit button without
 *      activating, we won't exit
 *
 *  Parameters:
 *      w           The quit button
 *      clientp     Client data
 *      call_data   ignored
 */

void
quit_arm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	clientp->quit_flag = 0;
}

/*
 *  void quit_activate( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Set the quit flag, so that as soon as the quit button is disarmed,
 *      we'll exit.
 *
 *  Parameters:
 *      w           the quit button
 *      clientp     client data
 *      call_data   ignored
 */

void
quit_activate( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	clientp->quit_flag = 1;
}

/*
 *  void quit_disarm( Widget w, CLIENTDATA  *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      If the button was activated, we exit.
 *
 *  Parameters:
 *      w           the quit button
 *      clientp     client data
 *      call_data   ignored
 */

void
quit_disarm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	if (clientp->quit_flag) {
		XtCloseDisplay( XtDisplay(w) );
		exit( 0 );
	}
}

/*
 *  void update( CLIENTDATA *clientp, XtIntervalId *id )
 *
 *  Description:
 *      This gets called every UPDATEINTERVAL milliseconds, and it checks
 *      out the state of the CD-ROM drive, updating the display if necessary
 *
 *  Parameters:
 *      clientp         Client data
 *      id              time out id
 */

void
update( CLIENTDATA *clientp, XtIntervalId *id )
{
	static  CDSTATUS    status;
	static  CDSTATUS    oldstatus;
	static  int         first_time = 1;

	/*
	 * If we're rewinding or fast-forwarding, rew and ff set the
	 * status stuff, and we don't want to take it from the CD-ROM drive.
	 */
	if (!clientp->cueing)
		CDgetstatus( clientp->cdplayer, &status );
	clientp->status = &status;

	if (oldstatus.state == CD_NODISC && status.state == CD_READY) {
		clientp->data_changed = 1;
		time_display( clientp->time, clientp, NULL );
	}

	if (status.state != oldstatus.state && (status.state == CD_NODISC
	 || status.state == CD_ERROR)) {
		/*
		 * Give each of the dialog boxes a chance to clean up
		 */
		program_nodisc( clientp );
		select_nodisc( clientp );
		data_nodisc( clientp );
		if (clientp->program) {
			free_program( clientp->program );
			clientp->program = 0;
		}
		clientp->play_mode = PLAYMODE_NORMAL;
		set_playmode_text( PLAYMODE_NORMAL );
	}

	if (status.state == CD_READY && (clientp->play_mode == PLAYMODE_PROGRAM
	 || clientp->play_mode == PLAYMODE_SHUFFLE)
	 && program_active( clientp->program )) {
		program_next( clientp->cdplayer, clientp->program );
		CDgetstatus( clientp->cdplayer, &status );
	}
	else if (status.state == CD_READY && clientp->repeat &&
	 clientp->repeat_button && !clientp->cueing) {
		if (clientp->play_mode == PLAYMODE_NORMAL)
			clientp->start_track = status.first;
		play( NULL, clientp, NULL );
		CDgetstatus( clientp->cdplayer, &status );
	}

	if ((status.state != oldstatus.state && !clientp->skipping)
	 || first_time) {
		set_status_bitmap( status.state );
		enable_buttons( status.state );
		first_time = 0;
	}

	/*
	 * In program mode, we often end up in the first second of the
	 * track after the one we've just finished playing; we never
	 * hear it, and this keeps the user from seeing it displayed.
	 */
	if (clientp->play_mode == PLAYMODE_NORMAL
	    || (status.state != CD_PLAYING
		&& status.state != CD_PAUSED && status.state != CD_STILL)
	    || status.track
	    == clientp->program->tracks[clientp->program->current]) {
		draw_display( clientp );
	}

	oldstatus = status;

	clientp->updateid = XtAddTimeOut( UPDATEINTERVAL, update, clientp );
}

/*
 *  void enable_buttons( int state )
 *
 *  Description:
 *      Enable/disable buttons as appropriate for the current state
 *
 *  Parameters:
 *      state       state of cdplayer
 */

void
enable_buttons( int state )
{
	Arg         wargs[10];
	int         n;
	static int	oldstate = -1;

	if (oldstate == state)
		return;
	oldstate = state;

	switch (state) {

	default:
	case CD_NODISC:
	case CD_ERROR:
	case CD_CDROM:
		n = 0;
		XtSetArg( wargs[n], XmNsensitive, FALSE ); n++;
		XtSetValues( mainwin.play, wargs, n );
		XtSetValues( mainwin.stop, wargs, n );
		XtSetValues( mainwin.skipf, wargs, n );
		XtSetValues( mainwin.skipb, wargs, n );
		XtSetValues( mainwin.ff, wargs, n );
		XtSetValues( mainwin.rew, wargs, n );
		XtSetValues( mainwin.time, wargs, n );
		XtSetValues( mainwin.data, wargs, n );
		XtSetValues( mainwin.repeat, wargs, n );
		XtSetValues( mainwin.program, wargs, n );
		XtSetValues( mainwin.shuffle, wargs, n );
		XtSetValues( mainwin.clear, wargs, n );

		XtSetSensitive(mainwin.open, state == CD_CDROM);

		break;

	case CD_PLAYING:
		n = 0;
		XtSetArg( wargs[n], XmNsensitive, TRUE ); n++;
		XtSetValues( mainwin.play, wargs, n );
		XtSetValues( mainwin.stop, wargs, n );
		XtSetValues( mainwin.open, wargs, n );
		XtSetValues( mainwin.skipf, wargs, n );
		XtSetValues( mainwin.skipb, wargs, n );
		XtSetValues( mainwin.ff, wargs, n );
		XtSetValues( mainwin.rew, wargs, n );
		XtSetValues( mainwin.time, wargs, n );
		XtSetValues( mainwin.data, wargs, n );
		XtSetValues( mainwin.repeat, wargs, n );
		XtSetValues( mainwin.program, wargs, n );
		XtSetValues( mainwin.shuffle, wargs, n );
		XtSetValues( mainwin.clear, wargs, n );

		n = 0;
		XtSetArg( wargs[n], XmNsensitive, FALSE ); n++;
		XtSetValues( mainwin.program, wargs, n );
		XtSetValues( mainwin.shuffle, wargs, n );
		XtSetValues( mainwin.clear, wargs, n );
		break;

	case CD_READY:
		n = 0;
		XtSetArg( wargs[n], XmNsensitive, TRUE ); n++;
		XtSetValues( mainwin.play, wargs, n );
		XtSetValues( mainwin.skipf, wargs, n );
		XtSetValues( mainwin.skipb, wargs, n );
		XtSetValues( mainwin.time, wargs, n );
		XtSetValues( mainwin.data, wargs, n );
		XtSetValues( mainwin.open, wargs, n );
		XtSetValues( mainwin.repeat, wargs, n );
		XtSetValues( mainwin.program, wargs, n );
		XtSetValues( mainwin.shuffle, wargs, n );
		XtSetValues( mainwin.clear, wargs, n );

		n = 0;
		XtSetArg( wargs[n], XmNsensitive, FALSE ); n++;
		XtSetValues( mainwin.stop, wargs, n );
		XtSetValues( mainwin.ff, wargs, n );
		XtSetValues( mainwin.rew, wargs, n );
		break;

	case CD_PAUSED:
	case CD_STILL:
		n = 0;
		XtSetArg( wargs[n], XmNsensitive, TRUE ); n++;
		XtSetValues( mainwin.play, wargs, n );
		XtSetValues( mainwin.skipf, wargs, n );
		XtSetValues( mainwin.skipb, wargs, n );
		XtSetValues( mainwin.time, wargs, n );
		XtSetValues( mainwin.data, wargs, n );
		XtSetValues( mainwin.open, wargs, n );
		XtSetValues( mainwin.stop, wargs, n );
		XtSetValues( mainwin.repeat, wargs, n );
		XtSetValues( mainwin.program, wargs, n );
		XtSetValues( mainwin.shuffle, wargs, n );
		XtSetValues( mainwin.clear, wargs, n );

		n = 0;
		XtSetArg( wargs[n], XmNsensitive, FALSE ); n++;
		XtSetValues( mainwin.ff, wargs, n );
		XtSetValues( mainwin.rew, wargs, n );
		XtSetValues( mainwin.program, wargs, n );
		XtSetValues( mainwin.shuffle, wargs, n );
		XtSetValues( mainwin.clear, wargs, n );
		break;
	}
}

Widget
create_button_label( Widget parent, char *name, Arg *create_args,
 int ncreate_args )
{
	Widget              w;
	int                 n;
	Arg                 wargs[10];
	RESOURCESTRUCT      *resource_struct;
	static XtResource   resources[] = {
		{
			XmNfontList,
			XmCFontList,
			XmRFontList,
			sizeof (XmFontList),
			XtOffset(
			 struct _resource_struct *, font_list ),
			XtRString,
			"-*-helvetica-medium-r-*-*-9-*"
		},
		{
			XmNlabelString,
			XmCLabelString,
			XmRXmString,
			sizeof (XmString),
			XtOffset(
			 struct _resource_struct *, label_string ),
			XtRString,
			""
		}
	};
	XGCValues           gcv;
	XmFontContext       context;
	XFontStruct         *font;
	XmStringCharSet     charset = XmSTRING_DEFAULT_CHARSET;
	Boolean             success;

	resource_struct = malloc(sizeof (*resource_struct));
	memset(resource_struct, 0, sizeof (*resource_struct));

	w = XmCreateDrawingArea(parent, name, create_args, ncreate_args);

	XtGetApplicationResources(w, resource_struct, resources,
				  XtNumber(resources), NULL, 0);

	success = XmFontListInitFontContext(&context,
					    resource_struct->font_list);
	XmFontListGetNextFont(context, &charset, &font);
	XmFontListFreeFontContext(context);
	n = 0;
	XtSetArg(wargs[n], XtNforeground, &gcv.foreground); n++;
	XtSetArg(wargs[n], XtNbackground, &gcv.background); n++;
	XtGetValues(w, wargs, n);
	gcv.font = font->fid;
	resource_struct->gc = XtGetGC(w, GCFont | GCForeground | GCBackground,
				      &gcv);

	XtAddCallback(w, XmNexposeCallback, draw_button_label,
		      resource_struct);

	return (w);
}

#define XMARGIN 4
#define YMARGIN 2

void
draw_button_label( Widget w, RESOURCESTRUCT *r,
 XmDrawingAreaCallbackStruct *cb )
{
	Arg         wargs[10];
	int         n;
	Dimension   width, height, x, y, text_width, text_height;
	Dimension   x1, y1, x2, y2;

	n = 0;
	XtSetArg( wargs[n], XmNwidth, &width ); n++;
	XtSetArg( wargs[n], XmNheight, &height ); n++;
	XtGetValues( w, wargs, n );

	XmStringExtent( r->font_list, r->label_string, &text_width,
	 &text_height );
	x = (width - text_width) >> 1;
	y = (height - text_height) >> 1;

	XmStringDrawImage( XtDisplay( w ), cb->window, r->font_list,
	 r->label_string, r->gc, x, y, width - y, XmALIGNMENT_BEGINNING,
	 XmSTRING_DIRECTION_L_TO_R, NULL );

	x1 = x - XMARGIN;
	y1 = height >> 1;
	x2 = width >> 2;
	y2 = y1;
	XDrawLine( XtDisplay( w ), cb->window, r->gc, x1, y1, x2, y2 );

	x1 = x2;
	y2 = height - YMARGIN;
	XDrawLine( XtDisplay( w ), cb->window, r->gc, x1, y1, x2, y2 );

	x1 = x + text_width + XMARGIN;
	y1 = height >> 1;
	x2 = (width >> 1) + (width >> 2);
	y2 = y1;
	XDrawLine( XtDisplay( w ), cb->window, r->gc, x1, y1, x2, y2 );

	x1 = x2;
	y2 = height - YMARGIN;
	XDrawLine( XtDisplay( w ), cb->window, r->gc, x1, y1, x2, y2 );
}

void
usage( void )
{
	fprintf( stderr, "cdplayer: usage: cdplayer [-dev <device>]\n" );
	exit( 1 );
}

static void
terminate( void )
{
	if (cdplayer) {
		CDclose( cdplayer );
	}
}

/*
 *  void
 *  okcb(Widget w, int fatal, XmAnyCallbackStruct *cb)
 *
 *  Description:
 *      OK callback function for error message boxes.  If fatal is
 *      non-zero, we exit, otherwise we just unmanage ourselves.
 *
 *  Parameters:
 *      w      Error message box
 *      fatal  Is this a fatal error message?
 *      cb     callback info
 */

/*ARGSUSED*/
static void
okcb(Widget w, int fatal, XmAnyCallbackStruct *cb)
{
    if (fatal) {
	exit(1);
    } else {
	XtUnmanageChild(w);
    }
}

/*
 *  static void
 *  ManageErrorBox(Widget w, void *idontcare, XEvent *event)
 *
 *  Description:
 *      Manage the error dialog box.  This is an event handler for
 *      the VisibilityNotify event of the parent of the error dialog
 *      box.
 *
 *  Parameters:
 *      w          parent of error dialog box
 *      errorBox   client data; the widget to manage
 *      event      VisibilityNotify event
 *      cont       continuation flag
 */

/*ARGSUSED*/
static void
ManageErrorBox(Widget w, XtPointer errorBox, XEvent *event,
	       Boolean *cont)
{
    XtManageChild((Widget)errorBox);
    XtRemoveEventHandler(w, VisibilityChangeMask, False,
			 ManageErrorBox, errorBox);
}

/*
 *  XmString
 *  CreateMotifString(char *str)
 *
 *  Description: A utility function to create a 
 *              big ugly bloatif compound string 
 *              from a (possibly multi-line) string.
 *
 *              Shamelessly stolen from printstatus.  Thanks Dave!
 *
 *  Returns: A compound string.  
 *          
 *  WARNINGS: I don't free or reuse the returned string, so don't forget to
 *            XmStringFree the returned string when you're done with it. 
 *
 *  CAVEATS:  Empty lines are ignored.  Put a space in (" \n") for a
 *      blank line. 
 *
 */

static XmString
CreateMotifString(char *str)
{
    XmString mstr, mnewline, mtempstr, mnextstr;
    register char *strptr, *tokptr, *dupstr;
    register int strnum;

    mnewline = XmStringSeparatorCreate();

    /*
     * Parse the input string
     */
    /*
     * Don't pass str itself to strtok, which writes NULLs into.  Make
     * a copy of the input string, which is freed below.
     */
    strptr = tokptr = dupstr = strdup(str);
    strnum = 0;
    mstr = NULL;
    while((strptr = strtok(tokptr, "\n")) != NULL) {
	tokptr = NULL; /* tell strtok to continue where we left off */
		       /* next time */
	
	/* Two cases: first line, and subsequent lines.  
	 * Don't append mnewline after every line: 
	 * for cleanliness, append it only at begin of subsequent lines.
	 */
	if (!strnum++) {
	    mstr = XmStringCreateSimple(strptr);
	} else {
	    /* Append new line, being careful not to lose storage */
	    mtempstr = mstr;
	    mstr = XmStringConcat(mstr, mnewline);
	    XmStringFree(mtempstr);
	    
	    /* Now append the new string, being careful not to lose storage */
	    mtempstr = mstr;
	    mnextstr = XmStringCreateSimple(strptr);
	    mstr = XmStringConcat(mstr, mnextstr);
	    XmStringFree(mtempstr);
	    XmStringFree(mnextstr);
	}
    }
    
    XmStringFree(mnewline);
    free(dupstr);
    if (!mstr) {
	mstr = XmStringCreateSimple("");
    }
    return mstr;
}

/*
 *  void
 *  PostError(Widget parent, char *errmsg, int fatal)
 *
 *  Description:
 *      Put up an error message box, containing the text from errmsg
 *
 *  Parameters:
 *      parent  widget to be parent of message box if it needs to be
 *              created. 
 *      errmsg  the error message to display
 *      fatal   whether to exit when OK button is pressed
 */

static void
PostError(Widget parent, char *errmsg, int fatal)
{
    XmString xstr;
    int n;
    Arg wargs[10];
    static Widget errorBox;

    if (!errorBox) {
	n = 0;
	XtSetArg(wargs[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL); n++;
	errorBox = XmCreateErrorDialog(parent, "errorBox", wargs, n);
	XtUnmanageChild(XmMessageBoxGetChild(errorBox,
					     XmDIALOG_CANCEL_BUTTON));
	XtUnmanageChild(XmMessageBoxGetChild(errorBox, XmDIALOG_HELP_BUTTON));
    }
    XtRemoveAllCallbacks(errorBox, XmNokCallback);
    XtAddCallback(errorBox, XmNokCallback, (XtCallbackProc)okcb,
		  (XtPointer)fatal);

    xstr = CreateMotifString(errmsg);
    n = 0;
    XtSetArg(wargs[n], XmNmessageString, xstr); n++;
    XtSetValues(errorBox, wargs, n);
    XmStringFree(xstr);

    /*
     * Defer managing the message box until later if necessary so that
     * it will come up in a better place
     */
    if (XtIsRealized(parent)) {
	/*
	 * toplevel widget is already up, we can put up the dialog no
	 * problem.
	 */
	XtManageChild(errorBox);
    } else {
	/*
	 * toplevel widget isn't managed.  When the toplevel widget
	 * gets managed, its VisibilityNotify event list will get
	 * called, and the error dialog will come up then.
	 */
	XtAddEventHandler(parent, VisibilityChangeMask, False,
			  ManageErrorBox, (XtPointer)errorBox);
    }
}

/*
 *  char *
 *  GetResourceString(Widget w, char *name, char *class, char *def)
 *
 *  Description:
 *      Get a resource string for a widget.
 *
 *  Parameters:
 *      w      Widget to get resource for
 *      name   resource name
 *      class  resource class
 *      def    default value
 *
 *  Returns:
 *      String value of the resource.  Note that if def is non-NULL,
 *      this will always be valid (in other words, if what you pass in
 *      as def is guaranteed to be valid, you don't have to check the
 *      return value of this function)
 */

static char *
GetResourceString(Widget w, char *name, char *class, char *def)
{
    XtResource res;
    char *str;

    res.resource_name = name;
    res.resource_class = class;
    res.resource_type = XtRString;
    res.resource_size = sizeof(char *);
    res.resource_offset = (unsigned int)&str;
    res.default_type = XtRString;
    res.default_addr = def;

    XtGetApplicationResources(w, NULL, &res, 1, NULL, 0);

    return str ? str : def;
}
