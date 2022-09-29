/*
 *	seldbox.c
 *
 *	Description:
 *		Support for program select dialog box
 *
 *	History:
 *      rogerc      11/26/90    Created
 */

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
#include "seldbox.h"

#define DEBUG 1

static struct tag_seldbox {
	Widget	bb, program_list, ok, cancel, new, modify, programLabel;
	int	*programs;
}	seldbox;

void select_init( CLIENTDATA *clientp, int num_programs );
void enable_seldbox_buttons( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct * call );

/*
 *  void select_program( CLIENTDATA *clientp )
 *
 *  Description:
 *	Pop up the program selection dialog box so user can select a program.
 *	If no programs are found in the database, pop up the program creation
 *	dialog box instead
 *
 *  Parameters:
 *      clientp		client data
 */

void select_program( CLIENTDATA *clientp )
{
	int		num_programs;

	num_programs = db_get_program_count( clientp->data );
	if (num_programs == 0) {
		clientp->only_program = 1;
		clientp->prog_num = 1;
		create_program( clientp );
	}
	else {
		clientp->only_program = 0;
		select_init( clientp, num_programs );
		XtManageChild( clientp->select );
	}
}

/*
 *  void select_init( CLIENTDATA *clientp, int num_programs )
 *
 *  Description:
 *	Initialize the program selection dialog box.  We fill in the list
 *	of programs with programs found in database.
 *
 *  Parameters:
 *      clientp			client data
 *      num_programs	the number of programs in the database
 */

void select_init( CLIENTDATA *clientp, int num_programs )
{
	int			i, j, n;
	XmString	*strings;
	const char		*prog_name;
	Arg			wargs[10];

	if (seldbox.programs)
		free (seldbox.programs);

	XmListDeselectAllItems( seldbox.program_list );
	XmListDeleteAllItems( seldbox.program_list );

	seldbox.programs = (int *)malloc( sizeof (int) * num_programs );
	strings = (XmString *)malloc( sizeof (XmString) * num_programs );

	j = 0;
	for (i = 1; i <= num_programs; i++) {
		prog_name = db_get_program_name( clientp->data, i );
		if (prog_name) {
			seldbox.programs[j] = i;
			strings[j++] = XmStringCreateLtoR( prog_name,
			 XmSTRING_DEFAULT_CHARSET );
#if 0
			free( prog_name );
#endif
		}
	}

	n = 0;
	XtSetArg( wargs[n], XmNitems, strings ); n++;
	XtSetArg( wargs[n], XmNitemCount, j ); n++;
	XtSetValues( seldbox.program_list, wargs, n );
	enable_seldbox_buttons( NULL, NULL, NULL );

	for (i = 0; i < j; i++) {
		XmStringFree(strings[i]);
	}

	free( strings );
}

/*
 *  void select_ok( Widget w, CLIENTDATA *clientp,
 *	 XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Call back for OK button of program selection dialog box.  Determine
 *	which program was selected, retrieve it from the database and
 *	use it to set clientp->program
 *
 *  Parameters:
 *      w			the OK button
 *      clientp		client data
 *      call_data	ignored
 */

void select_ok( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	int			*selections, num_selections;
	CDPROGRAM	*program = 0;

	if (seldbox.programs) {
		if (XmListGetSelectedPos( seldbox.program_list, &selections,
		 &num_selections )) {

			if (num_selections)
				program = db_get_program( clientp->data, 
					seldbox.programs[selections[0] - 1] );

			if (program) {
				if (clientp->program)
					free_program( clientp->program );
				clientp->program = program;
				clientp->play_mode = PLAYMODE_PROGRAM;
				set_playmode_text( PLAYMODE_PROGRAM );
			}
		}
		
		free( seldbox.programs );
		seldbox.programs = 0;
	}
	XtUnmanageChild( clientp->select );
}

/*
 *  void select_cancel( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Callback function for the Cancel button of the program selection
 *	dialog box.  Set the state back to what it was before the user
 *	pressed the program button
 *
 *  Parameters:
 *      w			The Cancel button
 *      clientp		client data
 *      call_data	ignored
 */

void select_cancel( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	if (seldbox.programs) {
		free( seldbox.programs );
		seldbox.programs = 0;
	}
	XtUnmanageChild( clientp->select );
}

/*
 *  void select_nodisc( CLIENTDATA *clientp )
 *
 *  Description:
 *	Action to take if user ejects disc
 *
 *  Parameters:
 *      clientp		client data
 */

void select_nodisc( CLIENTDATA *clientp )
{
	if (XtIsManaged( clientp->select ))
		select_cancel( seldbox.cancel, clientp, NULL );
}

/*
 *  void select_new( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	Callback function for the new button.  Bring up the program creation
 *	dialog box.
 *
 *  Parameters:
 *      w			the new button
 *      clientp		client data
 *      call_data	ignored
 */

void select_new( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	int		num_programs = db_get_program_count( clientp->data );

	clientp->prog_num = num_programs + 1;
	create_program( clientp );
}

/*
 *  void select_modify( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *	callback function for modify button.  Bring up program creation
 *	dialog box with fields set to selected program
 *
 *  Parameters:
 *      w			The modify button
 *      clientp		client data
 *      call_data	ignored
 */

void select_modify( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data )
{
	int		*selections, num_selections;

	if (seldbox.programs) {
		XmListGetSelectedPos( seldbox.program_list, &selections,
		 &num_selections );

		if (num_selections == 1)
			modify_program(clientp,
				       seldbox.programs[selections[0] - 1]);
	}
}


/*
 *  int create_program_select_dialog( Widget parent, CLIENTDATA *clientp )
 *
 *  Description:
 *	Create the widgets that comprise the program selection dialog box
 *
 *  Parameters:
 *      parent		parent of dialog box
 *      clientp		client data
 *
 *  Returns:
 *		The dialog shell of the program selection dialog box
 */

int create_program_select_dialog( Widget parent, CLIENTDATA *clientp )
{
	Arg			wargs[10];
	int			n = 0;
	Widget		children[20];

	XtSetArg( wargs[n], XmNautoUnmanage, FALSE ); n++;
	seldbox.bb = XmCreateBulletinBoardDialog( parent, "Select", wargs, n );
	clientp->select_init = select_init;

	n = 0;
	children[n++] = seldbox.ok = XmCreatePushButton( seldbox.bb, "ok",
	 NULL, 0 );
	children[n++] = seldbox.cancel = XmCreatePushButton( seldbox.bb,
	 "cancel", NULL, 0 );
	children[n++] = seldbox.new = XmCreatePushButton( seldbox.bb, "new",
	 NULL, 0 );
	children[n++] = seldbox.modify
	    = XmCreatePushButton( seldbox.bb, "modify", NULL, 0 );
	children[n++] = seldbox.programLabel = XmCreateLabel( seldbox.bb,
	 "programLabel", NULL, 0 );

	XtManageChildren( children, n );

	/*
	 * Setup OK and cancel buttons to respond properly to return
	 * and escape keys.
	 */
	n = 0;
	XtSetArg(wargs[n], XmNdefaultButton, seldbox.ok); n++;
	XtSetArg(wargs[n], XmNcancelButton, seldbox.cancel); n++;
	XtSetValues(seldbox.bb, wargs, n);

	n = 0;
	XtSetArg( wargs[n], XmNlistSizePolicy, XmCONSTANT ); n++;
	seldbox.program_list = XmCreateScrolledList( seldbox.bb, "programList",
	 wargs, n );
	XtManageChild( seldbox.program_list );

	XtAddCallback( seldbox.ok, XmNactivateCallback, select_ok, clientp );
	XtAddCallback( seldbox.cancel, XmNactivateCallback, select_cancel,
	 clientp );
	XtAddCallback( seldbox.new, XmNactivateCallback, select_new, clientp );
	XtAddCallback( seldbox.modify, XmNactivateCallback, select_modify,
	 clientp );

	XtAddCallback( seldbox.program_list, XmNbrowseSelectionCallback,
	 enable_seldbox_buttons, clientp );
	XtAddCallback( seldbox.program_list, XmNdefaultActionCallback,
	 enable_seldbox_buttons, clientp );
	XtAddCallback( seldbox.program_list, XmNextendedSelectionCallback,
	 enable_seldbox_buttons, clientp );
	XtAddCallback( seldbox.program_list, XmNmultipleSelectionCallback,
	 enable_seldbox_buttons, clientp );
	XtAddCallback( seldbox.program_list, XmNsingleSelectionCallback,
	 enable_seldbox_buttons, clientp );

	clientp->progdbox = create_program_dialog( parent, clientp );
	clientp->select = seldbox.bb;

	return (1);
}

void enable_seldbox_buttons( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct * call )
{
	Boolean		enable;
	Arg			wargs[10];
	int			n, *selections, num_selections;

	enable = XmListGetSelectedPos( seldbox.program_list, &selections,
	 &num_selections ) && num_selections == 1;
	n = 0;
	XtSetArg( wargs[n], XmNsensitive, enable ); n++;

	XtSetValues( seldbox.modify, wargs, n );
	XtSetValues( seldbox.ok, wargs, n );
}
