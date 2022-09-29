/*
 *  display.c
 *
 *  Description:
 *      Code for drawing the display window of the Audio CD-ROM tool
 *
 *  History:
 *      rogerc      11/06/90    Created
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/Frame.h>
#include <Xm/BulletinB.h>
#include <Xm/Label.h>
#include <Xm/DrawingA.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "display.h"
#include "database.h"
#include "seldbox.h"
#include "util.h"

#define XMARGIN  2
#define YMARGIN  2

static struct tag_displaywin {
	Widget      rc, title, artist, track, time, play_mode, status;
	Widget      repeat, title_rc, track_rc, status_parent;
	Dimension   y;
	Pixmap      play_pixmap, pause_pixmap, status_pixmap;
	XmFontList  font_list;
	GC          gc;
} displaywin;

static struct tag_strings {
	String  noartist, notitle, shuffle, repeat, program, nodisc;
	String  dataDisc;
} res_strings;

static XtResource string_resources[] = {
	{
		"noArtistString",
		"NoArtistString",
		XtRString,
		sizeof (String),
		XtOffset( struct tag_strings *, noartist ),
		XtRString,
		"No Artist"
	},
	{
		"noTitleString",
		"NoTitleString",
		XtRString,
		sizeof (String),
		XtOffset( struct tag_strings *, notitle ),
		XtRString,
		"No Title"
	},
	{
		"shuffleString",
		"ShuffleString",
		XtRString,
		sizeof (String),
		XtOffset( struct tag_strings *, shuffle ),
		XtRString,
		"Shuffle"
	},
	{
		"repeatString",
		"RepeatString",
		XtRString,
		sizeof (String),
		XtOffset( struct tag_strings *, repeat ),
		XtRString,
		"Repeat"
	},
	{
		"programString",
		"ProgramString",
		XtRString,
		sizeof (String),
		XtOffset( struct tag_strings *, program ),
		XtRString,
		"Program"
	},
	{
		"noDiscString",
		"NoDiscString",
		XtRString,
		sizeof (String),
		XtOffset( struct tag_strings *, nodisc ),
		XtRString,
		"No disc"
	},
	{
		"dataDiscString",
		"DataDiscString",
		XtRString,
		sizeof(String),
		XtOffset(struct tag_strings *, dataDisc),
		XtRString,
		"Data Disc"
	},
};

static XtResource resources[] = {
	{XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
	 XtOffset(CLIENTDATA *, font), XtRString, "Fixed" },
};

void
draw_time( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data );

static int
first_diff( char *str1, char *str2 );

static void
change_label( Widget label, char *text );

/*
 *  void draw_display( CLIENTDATA *clientp, CDSTATUS *statusp )
 *
 *  Description:
 *      Fills in the label widgets that display information about time, title,
 *      author, and track
 *
 *  Parameters:
 *      clientp     client data
 */

void draw_display( CLIENTDATA *clientp )
{
	static const char *last_id;
	static  int     last_track;
	static  int     last_state;
	static  int     first_time = 1;
	const char      *id = 0;
	XmString        string = 0;
	Arg             wargs[10];
	int             n;
	const char      *title, *artist, *track = 0;
	int             track_num;

	/*
	 * Figure out what track to display
	 */
	if (clientp->status->state == CD_PLAYING ||
	 clientp->status->state == CD_STILL ||
	 clientp->status->state == CD_PAUSED)
		track_num = clientp->status->track;
	else if (clientp->status->state == CD_READY) {
		switch (clientp->play_mode) {

		default:
		case PLAYMODE_NORMAL:
			switch (clientp->ready_time_display) {

			case READY_TRACK:
			case READY_NORMAL:
			default:
				track_num = clientp->start_track == 0 ?
				 clientp->status->first : clientp->start_track;
				break;

			case READY_TOTAL:
				track_num = clientp->status->last -
				 clientp->status->first + 1;
				break;
			}
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			switch (clientp->ready_time_display) {

			case READY_TRACK:
			case READY_NORMAL:
			default:
				track_num = clientp->program->
				 tracks[clientp->program->current];
				break;

			case READY_TOTAL:
				track_num = clientp->program->num_tracks;
				break;
			}
			break;
		}
	}

	/*
	 * Figure out cd name, cd artist, and track name
	 */
	if (clientp->status->state == CD_NODISC
	    || clientp->status->state == CD_ERROR
	    || clientp->status->state == CD_CDROM
	    || !(id = db_get_id( clientp->cdplayer, clientp->status ))) {
		/*
		 * No disc.  We set title to "No Disc" and blanck the
		 * artist and track
		 */
		if (first_time || clientp->status->state != last_state) {
			change_label(displaywin.title,
				     clientp->status->state ==
				     CD_CDROM ?
				     res_strings.dataDisc :
				     res_strings.nodisc);
			change_label( displaywin.artist, " " );
			change_label( displaywin.track, " " );
			first_time = 0;
			last_state = clientp->status->state;
		}
	} else if (!last_id || strcmp( id, last_id ) != 0 ||
		   clientp->data_changed) {
		/*
		 * There's a new cd.  Change the title and artist
		 * strings, and get the right track string
		 */
		if ( !last_id || strcmp( id, last_id ) != 0 ||
		    !clientp->data) {
			if (clientp->data)
				db_close( clientp->data, 0 );
			clientp->data = db_init_from_cd( clientp->cdplayer,
							clientp->status);
		}
		artist = db_get_artist( clientp->data );
		title = db_get_title( clientp->data );
		track = db_get_track_name( clientp->data, track_num );
		if (!artist)	artist = res_strings.noartist;
		if (!title)	title = res_strings.notitle;
		if (!track)	track = "             ";
		change_label( displaywin.title, title );
		change_label( displaywin.artist, artist );
		change_label( displaywin.track, track );
		clientp->data_changed = 0;
	} else if (last_track != track_num) {
		/*
		 * There's a new track playing.  Change that label.
		 */
		track = db_get_track_name( clientp->data, track_num );
		if (!track) {
			track = "             ";
		}
		change_label( displaywin.track, track );
	}

	draw_time( displaywin.time, clientp, NULL );
	if (last_id) {
		free( last_id );
		last_id = NULL;
	}
	if (id != NULL) {
		last_id = strdup(id);
	}
	last_track = track_num;
	last_state = clientp->status->state;
}

/*
 *  void draw_time( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Figures out what time to display, and displays it.
 *
 *  Parameters:
 *      w           The display window
 *      clientp     Client data
 *      call_data   If this is NULL, then this function was called from
 *                  the update() function, and we only draw the parts of
 *                  the display that have changed.  Otherwise, this function
 *                  was called by X, so we need to draw the whole display.
 */

void draw_time( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	register CDSTATUS       *status;
	static char     buf1[100], buf2[100], *buf = buf1, *last_buf = buf2;
	char            *tmp;
	int             track, min, sec, diff, dir, ascent, desc, x, n, i,
					tmin, tsec, negative = 0;
	XCharStruct     char_info;
	Arg             wargs[10];
	XmString        string;
	CDTRACKINFO     info;

	status = clientp->status;

	if (status->state == CD_NODISC || status->state == CD_ERROR ||
	    status->state == CD_CDROM) {
		XClearWindow( XtDisplay( w ), XtWindow( w ) );
		return;
	}

	if (status->state == CD_PLAYING || status->state == CD_STILL ||
	 status->state == CD_PAUSED) {
		track = status->track;
		switch (clientp->time_display) {

		case TIME_TRACK:
		default:
			min = status->min;
			sec = status->sec;
			if (sec < 0) {
				sec *= -1;
				min *= -1;
				negative = 1;
			}
			break;

		case TIME_TRACKLEFT:
			CDgettrackinfo( clientp->cdplayer, track, &info );
			sec = info.total_sec + info.total_min * 60;
			sec -= (status->min * 60) + status->sec;
			min = sec / 60;
			sec %= 60;
			negative = 1;
			break;

		case TIME_CD:
			min = status->abs_min;
			sec = status->abs_sec;
			break;

		case TIME_CDLEFT:
			sec = (status->total_min - status->abs_min) * 60 +
			 status->total_sec - status->abs_sec;
			min = sec / 60;
			sec %= 60;
			negative = 1;
			break;
		}
	}
	else if (status->state == CD_READY) {
		switch (clientp->play_mode) {

		default:
		case PLAYMODE_NORMAL:
			switch (clientp->ready_time_display) {

			case READY_NORMAL:
			default:
				track = clientp->start_track == 0 ?
				 status->first : clientp->start_track;
				min = sec = 0;
				break;

			case READY_TOTAL:
				track = status->last - status->first + 1;
				min = status->total_min;
				sec = status->total_sec;
				break;

			case READY_TRACK:
				track = clientp->start_track == 0 ?
				 status->first : clientp->start_track;
				CDgettrackinfo( clientp->cdplayer,
				 track, &info );
				min = info.total_min;
				sec = info.total_sec;
				break;
			}
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			switch (clientp->ready_time_display) {

			case READY_NORMAL:
			default:
				track = clientp->program->
				 tracks[clientp->program->current];
				min = sec = 0;
				break;

			case READY_TOTAL:
				track = clientp->program->num_tracks;
				min = clientp->program->total_min;
				sec = clientp->program->total_sec;
				break;

			case READY_TRACK:
				track = clientp->program->
				 tracks[clientp->program->current];
				CDgettrackinfo( clientp->cdplayer,
				 track, &info );
				sec = info.total_sec;
				min = info.total_min;
				break;
			}
			break;
		}

	}
	else
		track = min = sec = 0;

	sprintf( buf, negative ? "%02d -%02d:%02d  " : "%02d  %02d:%02d  ",
	 track, min, sec );

	diff = 0;
	x = 0;

	if (!call_data) {
		char        tmp[100];
		XmString    string;
		Dimension   w, h;
		/*
		 * Cut down on those server requeste
		 */
		if (strcmp( buf, last_buf ) == 0)
			return;
		diff = first_diff( buf, last_buf );
		strncpy( tmp, buf, diff );
		tmp[diff] = '\0';
		string = XmStringCreateLtoR( tmp, XmSTRING_DEFAULT_CHARSET );
		XmStringExtent( displaywin.font_list, string, &w, &h );
		XmStringFree(string);
		x += w;
	}

	string = XmStringCreateSimple( buf + diff );
	XmStringDrawImage( XtDisplay( w ), XtWindow( w ), displaywin.font_list,
	 string, displaywin.gc, x, displaywin.y, 100, XmALIGNMENT_BEGINNING,
	 XmSTRING_DIRECTION_L_TO_R, NULL );
	XmStringFree(string);

	tmp = buf;
	buf = last_buf;
	last_buf = tmp;
}

Widget create_time( Widget parent, Dimension height, Pixel fg, Pixel bg )
{
	Arg                 wargs[10];
	int                 n;
	Widget              time;
	XmString            string;
	static char         *test = "99 -99:99";
	Dimension           text_width, text_height;
	XmFontList          font_list;
	static XtResource   font_list_resource = {
							XmNfontList,
							XmCFontList,
							XmRFontList,
							sizeof (XmFontList),
							0,
							XmRFontList,
							NULL
						};
	XGCValues           gcv;
	XmFontContext       context;
	XFontStruct         *font;
	XmStringCharSet     charset = XmSTRING_DEFAULT_CHARSET;

	n = 0;
	XtSetArg( wargs[0], XmNfontList, &font_list_resource.default_addr );
	n++;
	XtGetValues( displaywin.title, wargs, n );

	n = 0;
	XtSetArg(wargs[n], XmNforeground, fg); n++;
	XtSetArg(wargs[n], XmNbackground, bg); n++;
	time = XmCreateDrawingArea( parent, "time", wargs, n );
	string = XmStringCreateLtoR( test, XmSTRING_DEFAULT_CHARSET );
	XtGetApplicationResources( time, &displaywin.font_list,
	 &font_list_resource, 1, NULL, 0 );
	XmStringExtent( displaywin.font_list, string, &text_width,
	 &text_height );
	XmStringFree( string );
	n = 0;
	XtSetArg( wargs[n], XmNwidth, text_width ); n++;
	XtSetArg( wargs[n], XmNheight, height ); n++;
	XtSetValues( time, wargs, n );
	displaywin.y = (height - text_height) / 2;
	XmFontListInitFontContext( &context, displaywin.font_list );
	XmFontListGetNextFont( context, &charset, &font );
	XmFontListFreeFontContext( context );
	n = 0;
	XtSetArg( wargs[n], XtNforeground, &gcv.foreground ); n++;
	XtSetArg( wargs[n], XtNbackground, &gcv.background ); n++;
	XtGetValues( time, wargs, n );
	gcv.font = font->fid;
	displaywin.gc = XtGetGC( time, GCFont | GCForeground | GCBackground,
	 &gcv );
	return (time);
}

/*
 *  Boolean set_playmode_text( int playmode )
 *
 *  Description:
 *      Set the text in the play_mode label ("Normal", "Shuffle", or "")
 *
 *  Parameters:
 *      playmode    the playmode
 *
 *  Returns:
 *     TRUE if successful, FALSE otherwise
 */

Boolean set_playmode_text( int playmode )
{
	XmString    string;
	int         n;
	Arg         wargs[10];
	char        *text;

	/*
	 *  !!!Make these resources
	 */
	switch (playmode) {

	case PLAYMODE_NORMAL:
	default:
		text = "       ";
		break;

	case PLAYMODE_PROGRAM:
		text = res_strings.program;
		break;

	case PLAYMODE_SHUFFLE:
		text = res_strings.shuffle;
		break;
	}

	string = XmStringCreateLtoR( text, XmSTRING_DEFAULT_CHARSET );
	if (!string)
		return (False);
	n = 0;
	XtSetArg( wargs[n], XmNlabelString, string ); n++;
	XtSetValues( displaywin.play_mode, wargs, n );
	XmStringFree(string);
	return (True);
}

/*
 *  void set_repeat( Boolean repeat )
 *
 *  Description:
 *      Unmanage or manage the repeat label to tell user if program will
 *      be repeated
 *
 *  Parameters:
 *      repeat
 */

void set_repeat( Boolean repeat )
{
	char        *text;
	XmString    string;
	Arg         wargs[10];
	int         n;

	/*
	 *  !!! Make this a resource, rather than hard coded
	 */
	text = repeat ? res_strings.repeat : "      ";
	string = XmStringCreateLtoR( text, XmSTRING_DEFAULT_CHARSET );
	n = 0;
	XtSetArg( wargs[n], XmNlabelString, string ); n++;
	XtSetValues( displaywin.repeat, wargs, n );
	XmStringFree(string);
}

/*
 *  void init_status_widget( Widget w )
 *
 *  Description:
 *      Load the bitmap resources for the status label
 *
 *  Parameters:
 *      w
 */

void init_status_widget( Widget w )
{
	XGCValues           gcv;
	int                 n;
	Arg                 wargs[10];
	static XtResource   resources[] = {
		{
			"playPixmap",
			"PlayPixmap",
			XmRPrimForegroundPixmap,
			sizeof (Pixmap),
			0,
			XtRImmediate,
			(caddr_t) XmUNSPECIFIED_PIXMAP 
		},
		{
			"pausePixmap",
			"PausePixmap",
			XmRPrimForegroundPixmap,
			sizeof (Pixmap),
			0,
			XtRImmediate,
			(caddr_t) XmUNSPECIFIED_PIXMAP 
		}
	};

	XtGetApplicationResources( w, &displaywin.play_pixmap, resources, 1,
	 NULL, 0 );
	XtGetApplicationResources( w, &displaywin.pause_pixmap, resources + 1,
	 1, NULL, 0 );
}

/*
 *  void set_status_bitmap( int state )
 *
 *  Description:
 *      Set the status bitmap to indicate the state of the cd
 *
 *  Parameters:
 *      state   the current state of the cd
 */

void set_status_bitmap( int state )
{
	Pixmap  pix;
	Arg     wargs[10];
	int     n;

	if (state == CD_PLAYING)
		pix = displaywin.play_pixmap;
	else if (state == CD_PAUSED || state == CD_STILL)
		pix = displaywin.pause_pixmap;
	else
		pix = XmUNSPECIFIED_PIXMAP;

	n = 0;
	XtSetArg( wargs[n], XmNlabelPixmap, pix ); n++;
	XtSetValues( displaywin.status, wargs, n );
}

/*
 *  void create_display( Widget parent, CLIENTDATA *clientp )
 *
 *  Description:
 *      Creates all the widgets comprising the display area of the main window
 *
 *  Parameters:
 *      parent      parent of display
 *      clientp     client data
 *
 *  Returns:
 *      frame of display
 */

void create_display( Widget parent, CLIENTDATA *clientp )
{
	Widget          children[20], frame, rc, form;
	int             n, num_children;
	Arg             wargs[20];
	int             dir, ascent, desc;
	XCharStruct     char_info;
	XGCValues       gcv;
	Dimension       height;
	Pixel           fg, bg;

	frame = XmCreateFrame( parent, "frame",
	 NULL, 0 );

	n = 0;
	XtSetArg( wargs[n], XmNorientation, XmVERTICAL ); n++;
	rc = XmCreateRowColumn( frame, "display", wargs, n );

	XtGetApplicationResources( rc, &res_strings, string_resources,
	 XtNumber( string_resources ), NULL, 0 );

	XtManageChild( frame );
	XtManageChild( rc );

	num_children = 0;
	n = 0;
	XtSetArg( wargs[n], XmNrecomputeSize, FALSE ); n++;
	children[num_children++] = displaywin.title = XmCreateLabel( rc,
	 "title", wargs, n );
	children[num_children++] = displaywin.artist = XmCreateLabel( rc,
	 "artist", wargs, n );
	XtManageChildren( children, num_children );

	form = XmCreateForm( rc, "trackForm", NULL, 0 );
	XtManageChild( form );

	n = 0;
	XtSetArg(wargs[n], XmNforeground, &fg); n++;
	XtSetArg(wargs[n], XmNbackground, &bg); n++;
	XtGetValues(displaywin.title, wargs, n);

	n = 0;
	XtSetArg( wargs[n], XmNheight, &height ); n++;
	XtGetValues( displaywin.title, wargs, n );
	displaywin.time = create_time( form, height, fg, bg );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNbottomAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNresizable, False ); n++;
	XtSetValues( displaywin.time, wargs, n );
	XtManageChild( displaywin.time );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, displaywin.time ); n++;
	XtSetArg( wargs[n], XmNbottomAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNresizable, False ); n++;
	displaywin.track = XmCreateLabel( form, "track", wargs, n );
	XtManageChild( displaywin.track );

	displaywin.status_parent = XmCreateForm( rc, "statusParent", NULL, 0 );
	XtManageChild( displaywin.status_parent );

	num_children = 0;

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNbottomAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNlabelType, XmPIXMAP ); n++;
	XtSetArg( wargs[n], XmNresizable, False ); n++;
	children[num_children++] = displaywin.status = XmCreateLabel(
	 displaywin.status_parent, "status", wargs, n );
	init_status_widget( displaywin.status );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, displaywin.status ); n++;
	XtSetArg( wargs[n], XmNbottomAttachment, XmATTACH_FORM ); n++;
	children[num_children++] = displaywin.repeat = XmCreateLabel(
	 displaywin.status_parent, "repeatMode", wargs, n );
	set_repeat( False );

	n = 0;
	XtSetArg( wargs[n], XmNtopAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNrightAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNbottomAttachment, XmATTACH_FORM ); n++;
	XtSetArg( wargs[n], XmNleftAttachment, XmATTACH_WIDGET ); n++;
	XtSetArg( wargs[n], XmNleftWidget, displaywin.repeat ); n++;
	children[num_children++] = displaywin.play_mode = XmCreateLabel(
	 displaywin.status_parent, "playMode", wargs, n );

	XtManageChildren( children, num_children );

	set_playmode_text( PLAYMODE_NORMAL );

	XtAddCallback( displaywin.time, XmNexposeCallback, draw_time, clientp );
}

/*
 *  static int first_diff( char *str1, char *str2 )
 *
 *  Description:
 *      Finds the first place where str1 and str2 differ
 *
 *  Parameters:
 *      str1        string to search for differences
 *      str2        strint to search for differences
 *
 *  Returns:
 *      The index of the first difference
 */

static int first_diff( char *str1, char *str2 )
{
	int n = 0;

	while (*str1 && *str2 && *str1++ == *str2++)
		n++;
	
	return (n);
}

/*
 *  static void change_label( Widget label, char *text )
 *
 *  Description:
 *      Changes the label displayed in label.
 *
 *  Parameters:
 *      label   label to change text of
 *      text    text to put in label
 */

static void change_label( Widget label, char *text )
{
	Arg         wargs[10];
	int         n;
	XmString    str;

	str = XmStringCreateLtoR( text, XmSTRING_DEFAULT_CHARSET );

	n = 0;
	XtSetArg( wargs[n], XmNlabelString, str ); n++;
	XtSetValues( label, wargs, n );
	XmStringFree(str);
}

