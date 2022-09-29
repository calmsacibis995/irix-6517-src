/*
 *	progdbox.c
 *
 *	Description:
 *		Program dialog box support
 *
 *	History:
 *      rogerc      11/26/90    Created
 */

#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/List.h>
#include <Xm/BulletinB.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "database.h"
#include "progdbox.h"
#include "util.h"

static struct tag_progdbox {
	Widget	bb, cd_tracks, prog_tracks, add, remove, move_up,
		move_down, ok, cancel, trackLabel, programLabel,
		program_name, program_name_label;
} progdbox;

static void enable_progdbox_buttons( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data );
static void init_program_dbox( Widget w, CDPLAYER *cd, CDSTATUS *status );

/*
 *  void program_ok( Widget w, CLIENTDATA *clientp,
 *	 XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	This is the call-back function for the OK button on the program
 *	creation dialog box.  It reads the items out of the "Program Tracks"
 *	list, and creates a program consisting of those tracks.  Track names
 *	are mapped to track numbers by looking at the number which we
 *	previously pre-pended to the track name.
 *
 *	If the program selection dialog box was bypassed (there were no
 *	programs for this disc yet), then this function also sets the
 *	progam member of the CLIENTDATA structure to the newly created program,
 *	and sets the state appropriately.  The selection dialog box was
 *	bypassed if clientp->only_program is set to 1.
 *
 *  Parameters:
 *      w				The OK button
 *      clientp			client data
 *      call_data		Ignored
 */

void program_ok(Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data)
{
	Arg					wargs[10];
	register CDPROGRAM	*program;
	XmString			*strings;
	char				*string, *space;
	int					n, i, num_programs;
	CDTRACKINFO			info;

	program = (CDPROGRAM *)XtMalloc( sizeof (CDPROGRAM) );

	if (clientp->only_program) {
		if (clientp->program)
			free_program( clientp->program );
		clientp->program = program;
	}

	n = 0;
	XtSetArg( wargs[n], XmNitemCount, &program->num_tracks ); n++;
	XtGetValues( progdbox.prog_tracks, wargs, n );

	if (program->num_tracks) {
		program->name = XmTextFieldGetString(progdbox.program_name);
		program->tracks =
			(int *)XtMalloc(sizeof (int) * program->num_tracks);

		n = 0;
		XtSetArg( wargs[n], XmNitems, &strings ); n++;
		XtGetValues( progdbox.prog_tracks, wargs, n );

		program->total_min = program->total_sec = 0;
		for (i = 0; i < program->num_tracks; i++) {
			XmStringGetLtoR(strings[i],
					XmSTRING_DEFAULT_CHARSET, &string);
			if (space = strchr( string, ' ' ))
				*space = '\0';
			program->tracks[i] = atoi( string );
			CDgettrackinfo(clientp->cdplayer,
				       program->tracks[i], &info);
			program->total_sec +=
				info.total_sec + info.total_min * 60;
			free( string );
		}
		program->total_min = program->total_sec / 60;
		program->total_sec %= 60;
		program->current = 0;
		num_programs = db_get_program_count( clientp->data );
		db_put_program( clientp->data, program, clientp->prog_num );
		if (clientp->prog_num > num_programs)
			db_put_program_count( clientp->data, ++num_programs );
		db_save(clientp->data);
		(*clientp->select_init)( clientp, num_programs );
	}
	if (clientp->only_program) {
		clientp->play_mode = PLAYMODE_PROGRAM;
		set_playmode_text( PLAYMODE_PROGRAM );
	}

	XtUnmanageChild( clientp->progdbox );
}

/*
 *  void program_nodisc( CLIENTDATA *clientp )
 *
 *  Description:
 *	Actions to take if the disc is ejected while the dialog box is up
 *
 *  Parameters:
 *      clientp		client data
 */

void program_nodisc( CLIENTDATA *clientp )
{
	if (XtIsManaged( clientp->progdbox ))
		XtUnmanageChild( clientp->progdbox );
}

/*
 *  void program_cancel( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	This is the call-back function for the Cancel button of the program
 *	creation dialog box.  It sets the state of the radio buttons back to
 *	what they were before the program button was pressed.
 *
 *  Parameters:
 *      w				The cancel button
 *      clientp			client data
 *      call_data		ignored
 */

void program_cancel( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	XtUnmanageChild( clientp->progdbox );
}

/*
 *  void create_program( CLIENTDATA *clientp )
 *
 *  Description:
 *		Display the program creation dialog box
 *
 *  Parameters:
 *      clientp
 */

void create_program( CLIENTDATA *clientp )
{
	init_program_dbox(clientp->progdbox, clientp->cdplayer,
			  clientp->status);
	XtManageChild(clientp->progdbox);
	XtSetKeyboardFocus(clientp->progdbox, progdbox.program_name);
}

void modify_program( CLIENTDATA *clientp, int prog_num )
{
	CDPROGRAM	*program;
	char		*buf = 0;
	const char      *name;
	XmString	*strings;
	int			i, buf_len = 0, buf_min, n;
	Arg			wargs[10];

	program = db_get_program( clientp->data, prog_num );
	if (program) {
		clientp->prog_num = prog_num;
		init_program_dbox( clientp->progdbox, clientp->cdplayer,
		 clientp->status );

		XmTextFieldSetString( progdbox.program_name, program->name );
		strings = (XmString *)malloc(sizeof (XmString)
					     * program->num_tracks);

		for (i = 0; i < program->num_tracks; i++) {
			name = db_get_track_name( clientp->data, 
							program->tracks[i] );
			buf_min = (name ? strlen( name ) : 0)
				+ strlen( "99: " ) + 3;
			if (buf_len < buf_min) {
				buf = buf ? realloc( buf, buf_min ) :
				 malloc( buf_min );
				buf_len = buf_min;
			}
			name ? sprintf(buf, "%d: %s",
				       program->tracks[i], name) :
					       sprintf(buf, "%d:",
						       program->tracks[i]);
			strings[i] = XmStringCreateSimple(buf);
		}

		n = 0;
		XtSetArg( wargs[n], XmNitems, strings ); n++;
		XtSetArg( wargs[n], XmNitemCount, program->num_tracks ); n++;
		XtSetValues( progdbox.prog_tracks, wargs, n );
		for (i = 0; i < program->num_tracks; i++)
			XmStringFree( strings[i] );
		free( strings );
		if ( buf )
			free( buf );
		free_program( program );
		XtManageChild( clientp->progdbox );
	}
}

/*
 *  char **get_track_names( CDData *data, CDSTATUS *status, int *num_tracks )
 *
 *  Description:
 *      Construct a vector consisting of a name for every track on the cd.
 *      If the database does not contain a name, this function (in contrast
 *      to db_get_track_name() makes one up; it just converts the track
 *      number to a string.
 *
 *  Parameters:
 *      data        data base handle
 *	status      status info
 *      num_tracks  pointer to area to write the number of tracks
 *
 *  Returns:
 *      A vector of strings, one for each track
 */

static char **
get_track_names(CDData *data, CDSTATUS *status, int *num_tracks)
{
    int	i;
    char **track_names;
    const char *name;
    
    track_names = (char **)malloc( sizeof (char *) * status->last -
				  status->first + 1 );
    
    for (i = status->first; i <= status->last; i++) {
	name = db_get_track_name(data, i);
	track_names[i - status->first] = strdup(name ? name : "");
    }    

    *num_tracks = status->last - status->first + 1;
    return (track_names);
}

static void
free_track_names(char **track_names, int num_tracks)
{
    char **tofree = track_names;
    while (num_tracks--) {
	free(*tofree++);
    }
    free(track_names);
}

/*
 *  static void init_program_dbox( Widget w, CDPLAYER *cd, CDSTATUS *status )
 *
 *  Description:
 *		Initialize the program creation dialog box.  Fill in the 
 *		"Tracks on CD" list box with the names of the tracks on the CD.
 *		Delete everything from the "Tracks on Program" list box
 *
 *  Parameters:
 *      w			The program creation dialog box
 *      cd			CDPLAYER data structure
 *		status		status info
 */

static void init_program_dbox( Widget w, CDPLAYER *cd, CDSTATUS *status )
{
	char		**track_names, *buf;
	int			num_tracks, i, n, bufsize = 0, size;
	Arg			wargs[10];
	XmString	*strings;
	CDData		*data;

	XmListDeselectAllItems( progdbox.cd_tracks );
	XmListDeleteAllItems( progdbox.cd_tracks );

	XmListDeselectAllItems( progdbox.prog_tracks );
	XmListDeleteAllItems( progdbox.prog_tracks );

	data = db_init_from_cd( cd, status );
	if (!data)
		return;

	track_names = get_track_names(data, status, &num_tracks);

	strings = (XmString *)XtMalloc( sizeof (XmString) * num_tracks );

	bufsize = strlen( "99: " ) + strlen( track_names[0] ) + 1;
	buf = (char *)malloc( bufsize );
	for (i = 0; i < num_tracks; i++) {
		size = strlen( "99: " ) + strlen( track_names[i] ) + 1;
		if (size > bufsize) {
			bufsize = size;
			buf = realloc( buf, bufsize );
		}
		sprintf( buf, "%d: %s", status->first + i, track_names[i] );
		strings[i] = XmStringCreate( buf, XmSTRING_DEFAULT_CHARSET );
	}
	free( buf );
	n = 0;
	XtSetArg( wargs[n], XmNitems, strings ); n++;
	XtSetArg( wargs[n], XmNitemCount, num_tracks ); n++;
	XtSetValues( progdbox.cd_tracks, wargs, n );
	XmTextFieldSetString( progdbox.program_name, "" );

	for (i = 0; i < num_tracks; i++)
		XmStringFree( strings[i] );
	XtFree( strings );
	enable_progdbox_buttons( NULL, NULL, NULL );
	db_close( data, 0 );
	free_track_names(track_names, num_tracks);
}

/*
 *  void program_remove_track( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *		Delete currently selected tracks from the "Program Tracks"
 *		list box
 *
 *  Parameters:
 *      w				The remove button
 *      clientp			client data
 *      call_data		ignored
 */

void program_remove_track( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	int			*selections, num_selections;

	if (XmListGetSelectedPos( progdbox.prog_tracks, &selections,
	 &num_selections ))
		while (num_selections)
			XmListDeletePos( progdbox.prog_tracks,
			 selections[--num_selections] );
	enable_progdbox_buttons( NULL, NULL, NULL );
}

/*
 *  void program_add_track( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Adds all tracks currently selected in the "Tracks on Disc" list box
 *	to the "Tracks on Program" list box
 *
 *  Parameters:
 *      w				The add button
 *      clientp			client data
 *      call_data		ignored
 */

void program_add_track( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	int		*selections, num_selections, i, n, num_items;
	XmString	*strings, *prog_strings, *new_prog_strings;
	Arg		wargs[10];

	n = 0;
	XtSetArg( wargs[n], XmNitems, &strings ); n++;
	XtGetValues( progdbox.cd_tracks, wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNitemCount, &num_items ); n++;
	XtSetArg( wargs[n], XmNitems, &prog_strings ); n++;
	XtGetValues( progdbox.prog_tracks, wargs, n );

	if (strings && XmListGetSelectedPos( progdbox.cd_tracks, &selections,
	 &num_selections )) {

		new_prog_strings = (XmString *)XtMalloc( sizeof (XmString) *
		 (num_items + num_selections) );

		for (i = 0; i < num_items; i++)
			new_prog_strings[i] = XmStringCopy( prog_strings[i] );

		XmListDeselectAllItems( progdbox.cd_tracks );

		for (i = 0; i < num_selections; i++)
			new_prog_strings[i + num_items] = 
			 XmStringCopy( strings[selections[i] - 1] );
		n = 0;
		XtSetArg( wargs[n], XmNitems, new_prog_strings ); n++;
		XtSetArg( wargs[n], XmNitemCount,
			 num_items + num_selections ); n++;
		XtSetValues( progdbox.prog_tracks, wargs, n );

		for (i = 0; i < num_items + num_selections; i++) {
			XmStringFree(new_prog_strings);
		}
		XtFree(new_prog_strings);

		XmListDeselectAllItems( progdbox.prog_tracks );
	}
	enable_progdbox_buttons( NULL, NULL, NULL );
}

/*
 *  void program_move_up( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Moves all selections in the "Program Tracks" list box up one position,
 *	as long as the first item isn't one of them
 *
 *  Parameters:
 *      w				The move_up button
 *      clientp			client data
 *      call_data		ignored
 */

void program_move_up( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	int		*selections, num_selections, i, n, num_items;
	XmString	*strings, tmp, *new_strings;
	Arg		wargs[10];
	unsigned char	policy;
	int		top;

	n = 0;
	XtSetArg( wargs[n], XmNitems, &strings ); n++;
	XtSetArg( wargs[n], XmNitemCount, &num_items ); n++;
	XtGetValues( progdbox.prog_tracks, wargs, n );

	if (strings && XmListGetSelectedPos( progdbox.prog_tracks, &selections,
	 &num_selections ) && num_selections && selections[0] != 1) {
		new_strings =
			(XmString *)XtMalloc(sizeof (XmString) * num_items);
		XmListDeselectAllItems( progdbox.prog_tracks );
		for (i = 0; i < num_items; i++)
			new_strings[i] = XmStringCopy( strings[i] );
		for (i = 0; i < num_selections; i++) {
			tmp = new_strings[selections[i] - 2];
			new_strings[selections[i] - 2] =
				new_strings[selections[i] - 1];
			new_strings[selections[i] - 1] = tmp;
		}
		n = 0;
		XtSetArg( wargs[n], XmNselectionPolicy, &policy ); n++;
		XtGetValues( progdbox.prog_tracks, wargs, n );

		n = 0;
		XtSetArg( wargs[n], XmNitems, new_strings ); n++;
		XtSetArg( wargs[n], XmNitemCount, num_items ); n++;
		XtSetArg( wargs[n], XmNselectionPolicy,
			 XmMULTIPLE_SELECT ); n++;
		XtSetValues( progdbox.prog_tracks, wargs, n );

		for (i = 0; i < num_selections; i++)
			XmListSelectPos(progdbox.prog_tracks,
					selections[i] - 1, False);

		for (i = 0; i < num_items; i++) {
			XmStringFree(new_strings[i]);
		}
		XtFree(new_strings);

		n = 0;
		XtSetArg( wargs[n], XmNselectionPolicy, policy ); n++;
		XtSetValues( progdbox.prog_tracks, wargs, n );

		n = 0;
		XtSetArg(wargs[n], XmNtopItemPosition, &top); n++;
		XtGetValues(progdbox.prog_tracks, wargs, n);

		if (top >= selections[0]) {
			XmListSetPos(progdbox.prog_tracks,
				     selections[0] - 1);
		}
	}
	enable_progdbox_buttons( NULL, NULL, NULL );
}

/*
 *  void program_move_down( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Move all selections in the "Program Tracks" list box down one position,
 *	as long as the last item isn't selected.
 *
 *  Parameters:
 *      w				The move_down button
 *      clientp			client data
 *      call_data		ignored
 */

void program_move_down( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	int		*selections, num_selections, i, n, num_items;
	XmString	*strings, tmp, *new_strings;
	Arg		wargs[10];
	unsigned char	policy;
	int		top, visible;

	n = 0;
	XtSetArg( wargs[n], XmNitems, &strings ); n++;
	XtSetArg( wargs[n], XmNitemCount, &num_items ); n++;
	XtGetValues( progdbox.prog_tracks, wargs, n );

	if (strings && XmListGetSelectedPos( progdbox.prog_tracks, &selections,
	 &num_selections ) && num_selections && selections[num_selections - 1]
	 != num_items) {
		new_strings = (XmString *)XtMalloc(sizeof (XmString)
						   * num_items);
		XmListDeselectAllItems( progdbox.prog_tracks );
		for (i = 0; i < num_items; i++)
			new_strings[i] = XmStringCopy( strings[i] );
		for (i = num_selections - 1; i >= 0; i--) {
			tmp = new_strings[selections[i]];
			new_strings[selections[i]] =
				new_strings[selections[i] - 1];
			new_strings[selections[i] - 1] = tmp;
		}
		n = 0;
		XtSetArg( wargs[n], XmNselectionPolicy, &policy ); n++;
		XtGetValues( progdbox.prog_tracks, wargs, n );

		n = 0;
		XtSetArg( wargs[n], XmNitems, new_strings ); n++;
		XtSetArg( wargs[n], XmNitemCount, num_items ); n++;
		XtSetArg(wargs[n], XmNselectionPolicy,
			 XmMULTIPLE_SELECT); n++;
		XtSetValues( progdbox.prog_tracks, wargs, n );

		for (i = 0; i < num_selections; i++)
			XmListSelectPos(progdbox.prog_tracks,
					selections[i] + 1, False);

		n = 0;
		XtSetArg( wargs[n], XmNselectionPolicy, policy ); n++;
		XtSetValues( progdbox.prog_tracks, wargs, n );

		for (i = 0; i < num_items; i++) {
			XmStringFree(new_strings[i]);
		}
		XtFree(new_strings);

		n = 0;
		XtSetArg(wargs[n], XmNtopItemPosition, &top); n++;
		XtSetArg(wargs[n], XmNvisibleItemCount, &visible); n++;
		XtGetValues(progdbox.prog_tracks, wargs, n);

		if (top + visible - 1 <=
		    selections[num_selections - 1]) {
			XmListSetBottomPos(progdbox.prog_tracks,
					   selections[num_selections - 1] + 1);
		}
	}
	enable_progdbox_buttons( NULL, NULL, NULL );
}

/*
 *  Widget create_program_dialog( Widget parent, CLIENTDATA *clientp )
 *
 *  Description:
 *	Create all the widgets for the program creation dialog box
 *
 *  Parameters:
 *      parent		Widget to be parent of dialog box
 *      clientp		client data
 *
 *  Returns:
 *	Widget which is the dialog shell
 */

Widget create_program_dialog( Widget parent, CLIENTDATA *clientp )
{
	Arg	wargs[10];
	int	n = 0;
	Widget	children[20];

	n = 0;
	XtSetArg( wargs[n], XmNautoUnmanage, FALSE ); n++;
	progdbox.bb =
		XmCreateBulletinBoardDialog(parent, "program", wargs, n);

	n = 0;
	XtSetArg( wargs[n], XmNlistSizePolicy, XmCONSTANT ); n++;
	XtSetArg( wargs[n], XmNselectionPolicy, XmEXTENDED_SELECT ); n++;
	progdbox.cd_tracks = XmCreateScrolledList( progdbox.bb, "cdTracks",
	 wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNlistSizePolicy, XmCONSTANT ); n++;
	XtSetArg( wargs[n], XmNselectionPolicy, XmEXTENDED_SELECT ); n++;
	progdbox.prog_tracks = XmCreateScrolledList( progdbox.bb, "progTracks",
	 wargs, n );

	XtManageChild( progdbox.cd_tracks );
	XtManageChild( progdbox.prog_tracks );

	n = 0;
	children[n++] = progdbox.add = XmCreatePushButton( progdbox.bb, "add",
	 NULL, 0 );
	children[n++] = progdbox.remove = XmCreatePushButton( progdbox.bb,
	 "remove", NULL, 0 );
	children[n++] = progdbox.move_up = XmCreatePushButton( progdbox.bb,
	 "moveUp", NULL, 0 );
	children[n++] = progdbox.move_down = XmCreatePushButton( progdbox.bb,
	 "moveDown", NULL, 0 );
	children[n++] = progdbox.ok = XmCreatePushButton( progdbox.bb, "ok",
	 NULL, 0 );
	children[n++] = progdbox.cancel = XmCreatePushButton( progdbox.bb, 
	 "cancel", NULL, 0 );
	children[n++] = progdbox.trackLabel = XmCreateLabel( progdbox.bb,
	 "trackLabel", NULL, 0 );
	children[n++] = progdbox.programLabel = XmCreateLabel( progdbox.bb,
	 "programLabel", NULL, 0 );
	children[n++] = progdbox.program_name = XmCreateTextField( progdbox.bb,
	 "programName", NULL, 0 );
	children[n++] = progdbox.program_name_label =
		XmCreateLabel( progdbox.bb, "programNameLabel", NULL, 0 );

	XtManageChildren( children, n );

	XtAddCallback( progdbox.ok, XmNactivateCallback, program_ok, clientp );
	XtAddCallback( progdbox.cancel, XmNactivateCallback,
	 program_cancel, clientp );
	XtAddCallback( progdbox.add, XmNactivateCallback,
	 program_add_track, clientp );
	XtAddCallback(progdbox.remove, XmNactivateCallback,
		      program_remove_track, clientp);
	XtAddCallback( progdbox.move_up, XmNactivateCallback, program_move_up,
	 clientp );
	XtAddCallback( progdbox.move_down, XmNactivateCallback,
	 program_move_down, clientp );

	XtAddCallback( progdbox.prog_tracks, XmNbrowseSelectionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.prog_tracks, XmNdefaultActionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.prog_tracks, XmNextendedSelectionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.prog_tracks, XmNmultipleSelectionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.prog_tracks, XmNsingleSelectionCallback,
	 enable_progdbox_buttons, clientp );

	XtAddCallback( progdbox.cd_tracks, XmNbrowseSelectionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.cd_tracks, XmNdefaultActionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.cd_tracks, XmNextendedSelectionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.cd_tracks, XmNmultipleSelectionCallback,
	 enable_progdbox_buttons, clientp );
	XtAddCallback( progdbox.cd_tracks, XmNsingleSelectionCallback,
	 enable_progdbox_buttons, clientp );

	XtAddCallback( progdbox.program_name, XmNvalueChangedCallback,
	 enable_progdbox_buttons, clientp );

	/*
	 * Setup OK and cancel buttons to respond properly to return
	 * and escape keys.
	 */
	n = 0;
	XtSetArg(wargs[n], XmNdefaultButton, progdbox.ok); n++;
	XtSetArg(wargs[n], XmNcancelButton, progdbox.cancel); n++;
	XtSetValues(progdbox.bb, wargs, n);

	XmAddTabGroup(progdbox.program_name);
	XmAddTabGroup(progdbox.cd_tracks);
	XmAddTabGroup(progdbox.add);
	XmAddTabGroup(progdbox.prog_tracks);
	XmAddTabGroup(progdbox.move_up);
	XmAddTabGroup(progdbox.move_down);
	XmAddTabGroup(progdbox.remove);
	XmAddTabGroup(progdbox.ok);
	XmAddTabGroup(progdbox.cancel);

	return (progdbox.bb);
}

/*
 *  static void enable_progdbox_buttons( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Enable/Disable buttons on the program createion dialog box as
 *	appropriate for the current state of the widgets.  This function
 *	resides on numerous callback lists.
 *
 *  Parameters:
 *      w			One of the widgets that has this as a callback
 *      clientp		client data (ignored)
 *      call_data	ignored
 */

static void enable_progdbox_buttons( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	Arg	wargs[10];
	int	n, *selections, num_selections, count;
	Boolean	enable, sel;
	char	*name;

	enable = sel = XmListGetSelectedPos( progdbox.prog_tracks, &selections,
	 &num_selections ) && num_selections;

	n = 0;
	XtSetArg( wargs[n], XmNsensitive, enable ); n++;
	XtSetValues( progdbox.remove, wargs, n );

	enable = enable && selections[0] != 1;

	n = 0;
	XtSetArg( wargs[n], XmNsensitive, enable ); n++;
	XtSetValues( progdbox.move_up, wargs, n );

	n = 0;
	XtSetArg( wargs[n], XmNitemCount, &count ); n++;
	XtGetValues( progdbox.prog_tracks, wargs, n );

	name = XmTextFieldGetString( progdbox.program_name );
	enable = count != 0 && name && strlen( name ) > 0;
	free( name );

	n = 0;
	XtSetArg( wargs[n], XmNsensitive, enable ); n++;
	XtSetValues( progdbox.ok, wargs, n );

	enable = sel && selections[num_selections - 1] != count;
	n = 0;
	XtSetArg( wargs[n], XmNsensitive, enable ); n++;
	XtSetValues( progdbox.move_down, wargs, n );

	enable = XmListGetSelectedPos( progdbox.cd_tracks, &selections,
	 &num_selections ) && num_selections;

	n = 0;
	XtSetArg( wargs[n], XmNsensitive, enable ); n++;
	XtSetValues( progdbox.add, wargs, n );
}
