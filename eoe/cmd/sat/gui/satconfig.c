/*
 * satconfig - a GUI wrapper around sat_select
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sat.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/CascadeB.h>
#include <Xm/FileSB.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/SeparatoG.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>

#define	SAT_SOME_VALID_EVENT	SAT_EXEC
#define	ALL_EVENTS		-1
#define	NARGS			10
#define	OFFSET			10
#define	LOADING			True
#define	SAVING			False
#define	MAXNAMELEN		80
#define	HELP_DIR		"/usr/lib/onlineHelp/Sat/"
#define	HELP_PROG_FILE		HELP_DIR"Interface.hlp"
#define HELP_EVTS_FILE		HELP_DIR"Events.hlp"
#define	HELP_PROG		1
#define	HELP_EVTS		2
#define	DEF_FACTORY_FILE	"/etc/init.d/audit"
#define	DEF_LOCAL_FILE		"/etc/config/sat_select.options"
#define	CLEAN			False
#define	DIRTY			True

int     sat_eventtoclass( int );
char *  sat_classtostr( int );

void	create_widgets( Widget );
Widget	create_menu_bar( Widget );
void	create_toggle( Widget, int );
Widget	create_appl_dialog( Widget );
Widget	create_conf_dialog( Widget );
Widget	create_exit_dialog( Widget );
Widget	create_help_dialog( Widget, int );
Widget	create_kept_dialog( Widget );
Widget	create_npkg_dialog( Widget );
Widget	create_open_dialog( Widget );
Widget	create_over_dialog( Widget );
Widget	create_priv_dialog( Widget );
Widget	create_slct_dialog( Widget, Bool );
Widget	create_susr_dialog( Widget );
Widget	create_vers_dialog( Widget );
void	apply_changes( Bool );
void	keep_changes( );
void	parse_file( FILE * );
Bool	parse_record( char *, int *, int *, Bool );
char *	shorten_name( char * );
void	audit_dflt_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	audit_curr_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	audit_all__callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	audit_none_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	appl_callback( Widget, Widget,  XmAnyCallbackStruct * );
void	done_callback( Widget, Widget,  XmAnyCallbackStruct * );
void	evnt_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	exit_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	get__callback( Widget, Widget,  XmAnyCallbackStruct * );
void	help_callback( Widget, Widget,  XmAnyCallbackStruct * );
void	load_callback( Widget, Widget,  XmAnyCallbackStruct * );
void	over_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	put__callback( Widget, Widget,  XmAnyCallbackStruct * );
void	quit_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	rvrt_callback( Widget, caddr_t, XmAnyCallbackStruct * );
void	save_callback( Widget, Widget,  XmAnyCallbackStruct * );

Display *	display;		/* used to close display from popup */
Widget		appl_pb_w;		/* apply push button widget */
Widget		rvrt_pb_w;		/* revert push button widget */
Widget		exit_dg_w;		/* exit dialog widget */
Widget		conf_dg_w;		/* configuration error dialog widget */
Widget		kept_dg_w;		/* "changes have been kept" dialog */
Widget		open_dg_w;		/* open error dialog widget */
Widget		over_dg_w;		/* "overwrite exiting file?" dialog */
Widget		priv_dg_w;		/* privilege error dialog widget */

Bool		changes_applied;	/* changes applied to audit system */
char		toggle_fname[MAXNAMELEN];	/* toggle state file name */

int		num_toggles;			 /* event toggle counter */
Widget		evnt_tg_w	[SAT_NTYPES];	 /* check toggles */
int		toggle_to_event	[SAT_NTYPES];	 /* event list has holes */
int		event_to_toggle	[SAT_NTYPES];	 /* event list has holes */


/*
 * a GUI wrapper around a SAT tool
 */
main( int argc, char * argv[])
{
	XtAppContext	appCtx;		/* application context */
	Widget		topLevel;	/* top level shell */
	Widget		npkg_dg_w;	/* no sat package dialog widget */
	Widget		susr_dg_w;	/* superuser warning dialog widget */

	topLevel = XtAppInitialize( &appCtx, "Satconfig", NULL,	0,
		&argc, argv, NULL, NULL, 0 );
	create_widgets( topLevel );
	XtRealizeWidget( topLevel );

	if (sysconf(_SC_AUDIT) <= 0) {
		npkg_dg_w = create_npkg_dialog( topLevel );
		XtManageChild( npkg_dg_w );
	}
	else if ( -1 == satstate(SAT_SOME_VALID_EVENT)) {
		susr_dg_w = create_susr_dialog( topLevel );
		XtManageChild( susr_dg_w );
	}

	display = XtDisplay( topLevel );
	XtAppMainLoop( appCtx );
	exit( EXIT_FAILURE );		/* getting here is an error */
}


/*
 * create_widgets - build up all application widgets
 */
void
create_widgets( Widget topLevel)
{
	Widget		menu_br_w;	/* menu bar widget */
	Widget		mngr_fm_w;	/* manager form widget */
	Widget		btns_rc_w;	/* pushbuttons row+column widget */
	Widget		appl_dg_w;	/* apply dialog widget */
	Widget		quit_pb_w;	/* quit push button widget */
	Widget		horz_sb_w;	/* horizontal scroll bar widget */
	Widget		vert_sb_w;	/* vertical   scroll bar widget */
	Widget		evts_sw_w;	/* audit events scrolled window */
	Widget		evts_cb_w;	/* audit events form widget */
	Arg		args[NARGS];	/* arguments for XtSetValues */
	int		n;		/* argument counter */
	int		i;		/* sat event type index */


	/*
	 * Build a form to contain all other widgets and a menu bar
	 */
	mngr_fm_w = XmCreateForm( topLevel, "Form", NULL, 0);
	XtManageChild( mngr_fm_w );
	menu_br_w = create_menu_bar( mngr_fm_w );


	/*
	 * Build push buttons for basic functions
	 */
	btns_rc_w = XmCreateRowColumn( mngr_fm_w, "Buttons", NULL, 0 );
	XtManageChild( btns_rc_w );

	appl_pb_w = XmCreatePushButton( btns_rc_w, "Apply", NULL, 0 );
	XtManageChild( appl_pb_w );
	appl_dg_w = create_appl_dialog( appl_pb_w );
	XtAddCallback( appl_pb_w, XmNactivateCallback, (XtCallbackProc)
		appl_callback, appl_dg_w );

	/* priv dialog has no callback - it's managed upon privilege error, */
	/* in appl_callback and elsewhere */
	priv_dg_w = create_priv_dialog( appl_pb_w );

	rvrt_pb_w = XmCreatePushButton( btns_rc_w, "Revert", NULL, 0 );
	XtManageChild( rvrt_pb_w );
	XtAddCallback( rvrt_pb_w, XmNactivateCallback, (XtCallbackProc)
		rvrt_callback, NULL );

	quit_pb_w = XmCreatePushButton( btns_rc_w, "Quit", NULL, 0 );
	XtManageChild( quit_pb_w );
	XtAddCallback( quit_pb_w, XmNactivateCallback, (XtCallbackProc)
		exit_callback, NULL );

	/*
	 * desensitize (dim or gray out) apply button; nothing to apply yet
	 * desensitize revert button; nothing to revert from yet
	 */
	XtSetSensitive( appl_pb_w, False );
	XtSetSensitive( rvrt_pb_w, False );


	/*
	 * Build a scrolled audit event check box
	 */
	horz_sb_w = XmCreateScrollBar( mngr_fm_w, "horzScroll", NULL, 0 );
	vert_sb_w = XmCreateScrollBar( mngr_fm_w, "vertScroll", NULL, 0 );

	n = 0;
	XtSetArg( args[n], XmNhorizontalScrollBar,    horz_sb_w ); n++;
	XtSetArg( args[n], XmNverticalScrollBar,      vert_sb_w ); n++;
	XtSetArg( args[n], XmNtopAttachment,    XmATTACH_WIDGET ); n++;
	XtSetArg( args[n], XmNtopWidget,	menu_br_w );       n++;
	XtSetArg( args[n], XmNleftAttachment,   XmATTACH_FORM );   n++;
	XtSetArg( args[n], XmNrightAttachment,  XmATTACH_FORM );   n++;
	evts_sw_w = XmCreateScrolledWindow( mngr_fm_w, "Scroll", args, n );
	XtManageChild( evts_sw_w );

	evts_cb_w = XmCreateRowColumn( evts_sw_w, "Check", args, n );
	XtManageChild( evts_cb_w );

	num_toggles = 0;
	for (i = 0; i < SAT_NTYPES; i++ )
		create_toggle( evts_cb_w, i );

	apply_changes( CLEAN );
}
	

/*
 * create_menu_bar - build up a complete menu bar
 */
Widget
create_menu_bar( Widget parent_w )
{
	Widget		menu_br_w;	/* menu bar  widget		*/
	Widget		pane_pd_w;	/* menu pane widgets		*/
	Widget		load_pb_w;	/* load push button		*/
	Widget		load_dg_w;	/* load dialog widget		*/
	Widget		save_pb_w;	/* save push button		*/
	Widget		save_dg_w;	/* save dialog widget		*/
	Widget		exit_pb_w;	/* exit push button		*/
	Widget		dfac_pb_w;	/* audit default from factory	*/
	Widget		dloc_pb_w;	/* audit default locally chosen	*/
	Widget		curr_pb_w;	/* audit current event mask	*/
	Widget		all__pb_w;	/* audit all events push button	*/
	Widget		none_pb_w;	/* audit no  events push button	*/
	Widget		vers_pb_w;	/* version push button		*/
	Widget		vers_dg_w;	/* version dialog widget	*/
	Widget		h_pg_pb_w;	/* help w/ program push button	*/
	Widget		h_pg_dg_w;	/* help w/ program dialog widget*/
	Widget		h_ev_pb_w;	/* help w/ events push button	*/
	Widget		h_ev_dg_w;	/* help w/ events dialog widget	*/
	Widget		help_cs_w;	/* help menu cascade		*/
	Arg		args[NARGS];	/* widget resource args		*/
	int		n;		/* number of arguments		*/

	/*
	 * Build menu bar
	 */
	menu_br_w = XmCreateMenuBar( parent_w, "menuBar", NULL, 0 );


	/*
	 * Build File pull down with load, save and exit buttons
	 */
	pane_pd_w = XmCreatePulldownMenu( menu_br_w, "file_menu", NULL, 0 );
	XtCreateManagedWidget( "file_sep", xmSeparatorWidgetClass,
		pane_pd_w, NULL, 0 );

	load_pb_w = XmCreatePushButton( pane_pd_w, "load", NULL, 0 );
	XtManageChild( load_pb_w );

	load_dg_w = create_slct_dialog( load_pb_w , LOADING );
	XtAddCallback( load_pb_w, XmNactivateCallback, (XtCallbackProc)
		load_callback, load_dg_w );

	save_pb_w = XmCreatePushButton( pane_pd_w, "save", NULL, 0 );
	XtManageChild( save_pb_w );

	save_dg_w = create_slct_dialog( save_pb_w, SAVING );
	XtAddCallback( save_pb_w, XmNactivateCallback, (XtCallbackProc)
		save_callback, save_dg_w );

	/* some dialogs have no callbacks - managed in other callbacks */
	conf_dg_w = create_conf_dialog( save_pb_w );
	kept_dg_w = create_kept_dialog( save_pb_w );
	open_dg_w = create_open_dialog( save_pb_w );
	over_dg_w = create_over_dialog( save_pb_w );

	exit_pb_w = XmCreatePushButton( pane_pd_w, "exit", NULL, 0 );
	XtAddCallback( exit_pb_w, XmNactivateCallback, (XtCallbackProc)
		exit_callback, NULL );
	XtManageChild( exit_pb_w );

	n = 0;
	XtSetArg( args[n], XmNsubMenuId, pane_pd_w ); n++;
	XtCreateManagedWidget( "file", xmCascadeButtonWidgetClass,
		menu_br_w, args, n );
	
	/* exit dialog has no callback - it's managed in exit_callback */
	exit_dg_w = create_exit_dialog( exit_pb_w );


	/*
	 * Build Edit pull down
	 */
	pane_pd_w = XmCreatePulldownMenu( menu_br_w, "edit_menu", NULL, 0 );
	XtCreateManagedWidget( "edit_sep", xmSeparatorWidgetClass,
		pane_pd_w, NULL, 0 );

	dfac_pb_w = XmCreatePushButton( pane_pd_w, "def_factory", NULL, 0 );
	XtManageChild( dfac_pb_w );
	XtAddCallback( dfac_pb_w, XmNactivateCallback, (XtCallbackProc)
		audit_dflt_callback, DEF_FACTORY_FILE );

	dloc_pb_w = XmCreatePushButton( pane_pd_w, "def_local", NULL, 0 );
	XtManageChild( dloc_pb_w );
	XtAddCallback( dloc_pb_w, XmNactivateCallback, (XtCallbackProc)
		audit_dflt_callback, DEF_LOCAL_FILE );

	curr_pb_w = XmCreatePushButton( pane_pd_w, "current", NULL, 0 );
	XtManageChild( curr_pb_w );
	XtAddCallback( curr_pb_w, XmNactivateCallback, (XtCallbackProc)
		audit_curr_callback, NULL );

	all__pb_w = XmCreatePushButton( pane_pd_w, "all", NULL, 0 );
	XtManageChild( all__pb_w );
	XtAddCallback( all__pb_w, XmNactivateCallback, (XtCallbackProc)
		audit_all__callback, NULL );

	none_pb_w = XmCreatePushButton( pane_pd_w, "none", NULL, 0 );
	XtManageChild( none_pb_w );
	XtAddCallback( none_pb_w, XmNactivateCallback, (XtCallbackProc)
		audit_none_callback, NULL );

	n = 0;
	XtSetArg( args[n], XmNsubMenuId, pane_pd_w ); n++;
	XtCreateManagedWidget( "edit", xmCascadeButtonWidgetClass,
		menu_br_w, args, n );



	/*
	 * Build Help pull down
	 */
	pane_pd_w = XmCreatePulldownMenu( menu_br_w, "help_menu", NULL, 0 );
	XtCreateManagedWidget( "help_sep", xmSeparatorWidgetClass,
		pane_pd_w, NULL, 0 );

	vers_pb_w = XmCreatePushButton( pane_pd_w, "help_vers", NULL, 0 );
	XtManageChild( vers_pb_w );
	vers_dg_w = create_vers_dialog( vers_pb_w );
	XtAddCallback( vers_pb_w, XmNactivateCallback, (XtCallbackProc)
		help_callback, vers_dg_w );

	h_pg_pb_w = XmCreatePushButton( pane_pd_w, "help_prog", NULL, 0 );
	XtManageChild( h_pg_pb_w );
	h_pg_dg_w = create_help_dialog( h_pg_pb_w, HELP_PROG );
	XtAddCallback( h_pg_pb_w, XmNactivateCallback, (XtCallbackProc)
		help_callback, h_pg_dg_w );

	h_ev_pb_w = XmCreatePushButton( pane_pd_w, "help_evts", NULL, 0 );
	XtManageChild( h_ev_pb_w );
	h_ev_dg_w = create_help_dialog( h_ev_pb_w, HELP_EVTS );
	XtAddCallback( h_ev_pb_w, XmNactivateCallback, (XtCallbackProc)
		help_callback, h_ev_dg_w );

	n = 0;
	XtSetArg( args[n], XmNsubMenuId, pane_pd_w ); n++;
	help_cs_w = XmCreateCascadeButton( menu_br_w, "help", args, n );
	XtManageChild( help_cs_w );
	n = 0;
	XtSetArg( args[n], XmNmenuHelpWidget, help_cs_w ); n++;
	XtSetValues( menu_br_w, args, n );


	/*
	 * Manage and return menu bar
	 */
	XtManageChild( menu_br_w );
	return( menu_br_w );
}


/*
 * create_toggle - build a toggle and its associated widgets
 */
void
create_toggle( Widget parent, int i )
{
	Widget		sepa_sg_w;	/* separator gadget */
	Widget		clss_lb_w;	/* class name label gadget */
	Arg		args[NARGS];	/* arguments for XtSetValues */
	int		n;		/* argument counter */
	char *		eventname;	/* from sat_eventtostr() */
	XmString	XmS_eventname;	/* Xm-ish copy of eventname */
	char *		classname;	/* from sat_classtostr() */
	XmString	XmS_classname;	/* Xm-ish copy of classname */
	static	int	old_class = 0;	/* copy of current event class */
	int		class;		/* current event class */

	if (!(eventname = sat_eventtostr( i )))
		return;

	class = sat_eventtoclass( i );
	if ( old_class != class ) {
		sepa_sg_w = XmCreateSeparatorGadget( parent, "Sep", NULL, 0 );
		XtManageChild( sepa_sg_w );
		old_class = class;
		classname = sat_classtostr( class );
		n = 0;
		XmS_classname = XmStringCreateLtoR(( classname ),
			XmSTRING_DEFAULT_CHARSET );
		XtSetArg( args[n], XmNlabelString, XmS_classname ); n++;
		clss_lb_w = XmCreateLabelGadget( parent, "Class", args, n );
		XtManageChild( clss_lb_w );
		XmStringFree( XmS_classname );
		sepa_sg_w = XmCreateSeparatorGadget( parent, "Sep", NULL, 0 );
		XtManageChild( sepa_sg_w );
	}

	toggle_to_event[num_toggles] = i;
	event_to_toggle[i] = num_toggles;
	n = 0;
	XmS_eventname = XmStringCreateLtoR( shorten_name( eventname ),
		XmSTRING_DEFAULT_CHARSET );
	XtSetArg( args[n], XmNlabelString, XmS_eventname ); n++;
	XtSetArg( args[n], XmNset, satstate( i ));          n++;
	evnt_tg_w[num_toggles] = XmCreateToggleButtonGadget( parent, "Event",
		args, n );
	XtManageChild( evnt_tg_w[num_toggles] );
	XtAddCallback( evnt_tg_w[num_toggles],
		XmNarmCallback, (XtCallbackProc)evnt_callback, NULL );
	XtAddCallback( evnt_tg_w[num_toggles],
		XmNdisarmCallback, (XtCallbackProc)evnt_callback, NULL );
	XmStringFree( XmS_eventname );
	num_toggles++;
}


/*
 * create_appl_dialog - build a simple message dialog box for applying changes
 */
Widget
create_appl_dialog( Widget parent )
{
	Widget		appl_dg_w;	/* applion dialog widget */

	appl_dg_w = XmCreateInformationDialog( parent, "apply_dialog",
		NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( appl_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, appl_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		appl_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		appl_dg_w, XmDIALOG_HELP_BUTTON ));
	return( appl_dg_w );
}


/*
 * create_exit_dialog - build a simple message dialog box to confirm exit
 */
Widget
create_exit_dialog( Widget parent )
{
	Widget		exit_dg_w;	/* exit dialog widget */

	exit_dg_w = XmCreateQuestionDialog( parent, "exit_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( exit_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)quit_callback, exit_dg_w );
	XtAddCallback( XmMessageBoxGetChild( exit_dg_w, XmDIALOG_CANCEL_BUTTON),
		XmNactivateCallback, (XtCallbackProc)done_callback, exit_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		exit_dg_w, XmDIALOG_HELP_BUTTON ));
	return( exit_dg_w );
}


/*
 * create_help_dialog - build a simple message dialog box for program help
 */
Widget
create_help_dialog( Widget parent, int help_with_what )
{
	Widget		help_dg_w;	/* help dialog widget */
	Widget		help_st_w;	/* help scrolled text widget */
	Widget		help_rc_w;	/* help row column widget */
	Widget		help_pb_w;	/* help push button widget */
	Arg		args[NARGS];	/* arguments for XtSetValues */
	int		n;		/* argument counter */
	FILE *		fp;		/* help file pointer */
	char *		fname;		/* name of help file */
	char		no_file[MAXNAMELEN];	/* missing file warning */
	struct stat	stat_buf;	/* help file stat info */
	char *		help_text;	/* contents of help file */

	switch ( help_with_what ) {
	case HELP_PROG:
		fname = HELP_PROG_FILE;
		break;
	case HELP_EVTS:
		fname = HELP_EVTS_FILE;
		break;
	default:
		fname = "unknown file name";
	}

	help_dg_w = XmCreateFormDialog( parent, "help_dialog", NULL, 0 );

	help_st_w = XmCreateScrolledText( help_dg_w, "help_text", NULL, 0 );
	XtManageChild( help_st_w );
	XmTextSetEditable( help_st_w, False );

	if ( fp = fopen( fname, "r" )) {
		(void) stat( fname, &stat_buf );
		help_text = malloc( 1 + stat_buf.st_size );
		(void) fread( help_text, stat_buf.st_size, 1, fp );
		XmTextSetString( help_st_w, help_text );
	}
	else {
		sprintf( no_file, "Can't find help file: %s", fname );
		XmTextSetString( help_st_w, no_file );
	}

	n = 0;
	XtSetArg( args[n], XmNtopAttachment,    XmATTACH_WIDGET ); n++;
	XtSetArg( args[n], XmNtopWidget,	help_st_w );       n++;
	help_rc_w = XmCreateRowColumn( help_dg_w, "help_buttons", args, n );
	XtManageChild( help_rc_w );

	help_pb_w = XmCreatePushButton( help_rc_w, "help_done", NULL, 0 );
	XtManageChild( help_pb_w );
	XtAddCallback( help_pb_w, XmNactivateCallback, (XtCallbackProc)
		done_callback, help_dg_w );

	return( help_dg_w );
}


/*
 * create_conf_dialog - build a simple message dialog box for apply error
 */
Widget
create_conf_dialog( Widget parent )
{
	Widget		conf_dg_w;	/* configuration dialog widget */

	conf_dg_w = XmCreateErrorDialog( parent, "conf_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( conf_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, conf_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		conf_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		conf_dg_w, XmDIALOG_HELP_BUTTON ));
	return( conf_dg_w );
}


/*
 * create_kept_dialog - build dialog box asserting that changes have been kept
 */
Widget
create_kept_dialog( Widget parent )
{
	Widget		kept_dg_w;	/* file kept dialog widget */

	kept_dg_w = XmCreateErrorDialog( parent, "kept_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( kept_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, kept_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		kept_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		kept_dg_w, XmDIALOG_HELP_BUTTON ));
	return( kept_dg_w );
}


/*
 * create_npkg_dialog - build a simple dialog box for no sat package warning
 */
Widget
create_npkg_dialog( Widget parent )
{
	Widget		npkg_dg_w;	/* no package warning dialog widget */

	npkg_dg_w = XmCreateErrorDialog( parent, "nopkg_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( npkg_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, npkg_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		npkg_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		npkg_dg_w, XmDIALOG_HELP_BUTTON ));
	return( npkg_dg_w );
}


/*
 * create_open_dialog - build a simple message dialog box for open error
 */
Widget
create_open_dialog( Widget parent )
{
	Widget		open_dg_w;	/* open dialog widget */

	open_dg_w = XmCreateErrorDialog( parent, "open_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( open_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, open_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		open_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		open_dg_w, XmDIALOG_HELP_BUTTON ));
	return( open_dg_w );
}


/*
 * create_over_dialog - build a dialog box to confirm file overwrite
 */
Widget
create_over_dialog( Widget parent )
{
	Widget		over_dg_w;	/* overwrite dialog widget */

	over_dg_w = XmCreateQuestionDialog( parent, "over_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( over_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)over_callback, over_dg_w );
	XtAddCallback( XmMessageBoxGetChild( over_dg_w, XmDIALOG_CANCEL_BUTTON),
		XmNactivateCallback, (XtCallbackProc)done_callback, over_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		over_dg_w, XmDIALOG_HELP_BUTTON ));
	return( over_dg_w );
}


/*
 * create_priv_dialog - build a simple message dialog box for privilege error
 */
Widget
create_priv_dialog( Widget parent )
{
	Widget		priv_dg_w;	/* privilege dialog widget */

	priv_dg_w = XmCreateErrorDialog( parent, "priv_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( priv_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, priv_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		priv_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		priv_dg_w, XmDIALOG_HELP_BUTTON ));
	return( priv_dg_w );
}


/*
 * create_slct_dialog - build a file selection dialog box
 */
Widget
create_slct_dialog( Widget parent, Bool load_or_save )
{
	Widget		file_dg_w;	/* file dialog widget */

	file_dg_w = XmCreateFileSelectionDialog( parent,
		load_or_save ? "load_dialog" : "save_dialog", NULL, 0 );
	XtAddCallback( XmFileSelectionBoxGetChild(   file_dg_w,
		XmDIALOG_CANCEL_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback,  file_dg_w );
	XtAddCallback( XmFileSelectionBoxGetChild(   file_dg_w,
		XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, load_or_save ?
		(XtCallbackProc)get__callback : (XtCallbackProc)put__callback,
		file_dg_w );
	XtUnmanageChild( XmFileSelectionBoxGetChild( file_dg_w,
		XmDIALOG_HELP_BUTTON ));
	return( file_dg_w );
}


/*
 * create_susr_dialog - build a simple dialog box for superuser warning
 */
Widget
create_susr_dialog( Widget parent )
{
	Widget		susr_dg_w;	/* superuser warning dialog widget */

	susr_dg_w = XmCreateErrorDialog( parent, "suser_dialog", NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( susr_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, susr_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		susr_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		susr_dg_w, XmDIALOG_HELP_BUTTON ));
	return( susr_dg_w );
}


/*
 * create_vers_dialog - build a simple message dialog box for program version
 */
Widget
create_vers_dialog( Widget parent )
{
	Widget		vers_dg_w;	/* version dialog widget */

	vers_dg_w = XmCreateInformationDialog( parent, "version_dialog",
		NULL, 0 );
	XtAddCallback( XmMessageBoxGetChild( vers_dg_w, XmDIALOG_OK_BUTTON ),
		XmNactivateCallback, (XtCallbackProc)done_callback, vers_dg_w );
	XtUnmanageChild( XmMessageBoxGetChild(
		vers_dg_w, XmDIALOG_CANCEL_BUTTON ));
	XtUnmanageChild( XmMessageBoxGetChild(
		vers_dg_w, XmDIALOG_HELP_BUTTON ));
	return( vers_dg_w );
}


/* 
 * apply_changes - set the program to a 'clean' or 'dirty' condition
 */
void
apply_changes( Bool condition )
{

	changes_applied = (condition == CLEAN);

	/*
	 * desensitize or resensitize apply and revert buttons
	 * depending whether there is anything to apply or to revert to
	 */
	XtSetSensitive( appl_pb_w, ( condition == DIRTY ));
	XtSetSensitive( rvrt_pb_w, ( condition == DIRTY ));
}


/*
 * keep_changes - save changes to a file
 */
void
keep_changes( )
{
	Arg		args[NARGS];		/* arguments for XtSetValues */
	int		n;			/* argument counter */
	char		dg_lbl[1] = "";		/* original dialog label */
	XmString	XmS_dg_lbl = 0;		/* Xm-ish copy of dg_lbl */
	FILE *		fp;			/* keep file pointer */
	char		record	[MAXNAMELEN];	/* single saved record */
	int		i;			/* toggle index */

	if (fp = fopen( toggle_fname, "w" )) {
		for ( i = 0; i < num_toggles; i++ ) {
			strcpy( record,
				XmToggleButtonGadgetGetState( evnt_tg_w[i] )
				? "\t-on  " : "\t-off " );
			strcat( record, sat_eventtostr( toggle_to_event[i] ));
			strcat( record, "\n" );
			(void) fwrite( record, strlen( record ), 1, fp );
		}
		(void) fwrite( "\n", 1, 1, fp );
		fclose( fp );
		XtManageChild( kept_dg_w );
	}
	else {
		if ( strcmp( toggle_fname, DEF_LOCAL_FILE ))
			XtManageChild( open_dg_w );
		else
			XtManageChild( conf_dg_w );
	}

}


/*
 * parse_file - set toggle states for every record in a configuration file.
 *		this function makes daring assumptions about file structure.
 */
void
parse_file( FILE * fp )
{
	char	record	[MAXNAMELEN];
	Bool	state;
	int	event;
	Bool	repeat;	/* second or subsequent time through a record */
	int	i;

	while (fgets( record, MAXNAMELEN, fp )) {
		repeat = False;
		while ( parse_record( record, &state, &event, repeat )) {
			repeat = True;
			if ( ALL_EVENTS == event ) {
				for (i = 0; i < num_toggles; i++ )
					XmToggleButtonGadgetSetState(
					evnt_tg_w[i], state, False );
			}
			else
				XmToggleButtonGadgetSetState( evnt_tg_w[
					event_to_toggle[event]], state, False );
		}
	}
}


/*
 * parse_record - return True if record contains valid name and state;
 *		  return False otherwise (presume to be a comment)
 */
Bool
parse_record( char * record, int * state, int * event, Bool repeat )
{
	char *	state_str;
	char *	event_str;

	if (!( state_str = strtok( repeat ? NULL : record, " \t" )))
		return False;

	if (!strcmp( state_str, "-off" ))
		*state = False;
	else if (!strcmp( state_str, "-E"   ))
		*state = False;
	else if (!strcmp( state_str, "-on"  ))
		*state = True;
	else if (!strcmp( state_str, "-e"   ))
		*state = True;
	else 
		return False;

	if (!( event_str = strtok( NULL, " \t" )))
		return False;

	if (!strcmp( event_str, "-all" )) {
		*event = ALL_EVENTS;
		return True;
	}

	if (!( *event = sat_strtoevent( event_str )))
		return False;

	return True;
}


/*
 * shorten_name - shorten and tidy event name
 */
char *
shorten_name( char * full_name )
{
	static	char	shortname[MAXNAMELEN];	/* short version of full_name */
	int	i;
	int	j = 0;
	char	c;

	if ( NULL == full_name )
		return ( NULL );
	for ( i = 0; i < MAXNAMELEN; i++ ) {
		if ( '\0' == full_name[i+4] ) { /* ignore first 4 characters */
			shortname[i] = '\0';
			break;
		}
		c = shortname[i] = full_name[i+4];  /* drop "sat_" prefix */
		shortname[i] = ('_' == c) ? ' ' : c;
	}
	return ( shortname );
}


/*
 * audit_curr_callback - set switches to audit the currently set events 
 */
void
audit_curr_callback( Widget w, caddr_t cl_data, XmAnyCallbackStruct *cb_data )
{
	int	i;
	int	state;
	Bool	eperm = False;

	apply_changes( DIRTY );
	for ( i = 0; i < num_toggles; i++ ) {
		state = satstate( toggle_to_event[i] );
		if (state < 0) {
			if ( EPERM == errno )
				eperm = True;
			state = 0;
		}
		XmToggleButtonGadgetSetState( evnt_tg_w[i], state,  False );
	}
	if ( eperm )
		XtManageChild( priv_dg_w );
}


/*
 * audit_dfac_callback - set switches to audit the factory default events 
 */
void
audit_dflt_callback( Widget w, caddr_t fname, XmAnyCallbackStruct *cb_data )
{
	FILE *	fp;
	int	i;

	apply_changes( DIRTY );

	if (fp = fopen( fname, "r" )) {
		parse_file( fp );
		fclose( fp );
	}
}


/*
 * audit_all__callback - set switches to audit all events
 */
void
audit_all__callback( Widget w, caddr_t cl_data, XmAnyCallbackStruct *cb_data )
{
	int	i;

	apply_changes( DIRTY );
	for (i = 0; i < num_toggles; i++ )
		XmToggleButtonGadgetSetState( evnt_tg_w[i], True,  False );
}


/*
 * audit_none_callback - set switches to audit no events
 */
void
audit_none_callback( Widget w, caddr_t cl_data, XmAnyCallbackStruct *cb_data )
{
	int	i;

	apply_changes( DIRTY );
	for (i = 0; i < num_toggles; i++ )
		XmToggleButtonGadgetSetState( evnt_tg_w[i], False, False );
}


/*
 * appl_callback - apply changes to audit subsystem and config file
 */
void
appl_callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	int	i;
	int	err;
	Bool	eperm = False;

	/*
	 * apply changes to kernel; start auditing new events now
	 */
	apply_changes( CLEAN );
	for (i = 0; i < num_toggles; i++ ) {
		XmToggleButtonGadgetGetState( evnt_tg_w[i] ) ?
			( err = saton(  toggle_to_event[i] )) :
			( err = satoff( toggle_to_event[i] ));
		if (( err < 0 ) && ( EPERM == errno ))
			eperm = True;
	}
	XtManageChild( eperm ? priv_dg_w : dialog );

	/*
	 * apply changes to config file; audit new events with each reboot
	 */
	strcpy( toggle_fname, DEF_LOCAL_FILE );
	keep_changes();

	/*
	 * desensitize (dim or gray out) apply button; nothing left to apply
	 * desensitize revert button; nothing to revert from any more
	 */
	XtSetSensitive( appl_pb_w, False );
	XtSetSensitive( rvrt_pb_w, False );
}


/*
 * done_callback - remove a dialog
 */
void
done_callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	XtUnmanageChild( dialog );
}


/*
 * evnt_callback - when an event toggle is switched, note the dirty state
 */
void
evnt_callback( Widget widg, caddr_t client_data, XmAnyCallbackStruct *cb_data )
{
	apply_changes( DIRTY );
}


/*
 * exit_callback - when exit button is pushed, finish up program execution
 */
void
exit_callback( Widget widg, caddr_t client_data, XmAnyCallbackStruct *cb_data )
{
	if ( changes_applied ) {
		XtCloseDisplay( XtDisplay( widg ));
		exit( EXIT_SUCCESS );
	}
	else
		XtManageChild( exit_dg_w );
}


/*
 * get__callback - get data from selected file and remove the dialog
 */
void
get__callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	FILE *	fp;
	char *	fname;

	fname = XmTextGetString( XmFileSelectionBoxGetChild( dialog,
		XmDIALOG_TEXT ));

	if (fp = fopen( fname, "r" )) {
		apply_changes( DIRTY );
		parse_file( fp );
		fclose( fp );
	}
	else
		XtManageChild( open_dg_w );

	XtFree( fname );
	XtUnmanageChild( dialog );
}


/*
 * help_callback - help user with program
 */
void
help_callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	XtManageChild( dialog );
}


/*
 * load_callback - load text from a file
 */
void
load_callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	XtManageChild( dialog );
}


/*
 * over_callback - load text from a file
 */
void
over_callback( Widget widg, caddr_t cl_data, XmAnyCallbackStruct *cb_data )
{
	keep_changes();
}


/*
 * quit_callback - quit program even if text buffer not written to file
 */
void
quit_callback( Widget widg, caddr_t client_data, XmAnyCallbackStruct *cb_data )
{
	XtCloseDisplay( display );
	exit( EXIT_SUCCESS );
}


/*
 * rvrt_callback - revert toggle state to currently applied system state
 */
void
rvrt_callback( Widget widg, caddr_t client_data, XmAnyCallbackStruct *cb_data )
{
	Arg		args[NARGS];	/* arguments for XtSetValues */
	int		n;		/* argument counter */
	int		i;		/* toggle index */

	apply_changes( CLEAN );
	for ( i = 0; i < num_toggles; i++ ) {
		n = 0;
		XtSetArg( args[n], XmNset, satstate( i ));          n++;
		XtSetValues( evnt_tg_w[i], args, n );
	}
}


/*
 * put__callback - apply changes from selection box and remove the dialog
 */
void
put__callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	char *	fname;			/* save file name */
	struct	stat	stat_buf;	/* save file stat info */

	fname = XmTextGetString( XmFileSelectionBoxGetChild( dialog,
		XmDIALOG_TEXT ));
	strcpy( toggle_fname, fname );

	if (!(( -1 == stat( fname, &stat_buf )) && ( ENOENT == errno )))
		XtManageChild( over_dg_w );	/* overwrite existing file? */
	else	
		keep_changes();			/* do write a new file */

	XtFree( fname );
	XtUnmanageChild( dialog );
}


/*
 * save_callback - save text to a file
 */
void
save_callback( Widget widg, Widget dialog, XmAnyCallbackStruct *cb_data )
{
	XtManageChild( dialog );
}


#ifdef	USE_STUBS
/* .........................................................................
 *
 * Code following this dotted line is stub functions designed to make
 * the GUI work in the absence of a real audit subsystem.
 * To turn it on, change all the 2nd '*'s in the '**'s to '/'s,
 * and remove the final comment closer.
 */

int
satstate( int i ) {
	/* default state of toggles */
	static	Bool	dflt_event_state[1+SAT_NTYPES] = {
		False,				/* there is no event zero */
		/* sat_access_denied		*/	True,
		/* sat_access_failed		*/	False,
		/* sat_chdir			*/	True,
		/* sat_chroot			*/	True,
		/* sat_open			*/	True,
		/* sat_open_ro			*/	True,
		/* sat_read_symlink		*/	False,
		/* sat_file_crt_del		*/	True,
		/* sat_file_crt_del2		*/	True,
		/* sat_file_write		*/	True,
		/* sat_mount			*/	True,
		/* sat_file_attr_read		*/	False,
		/* sat_file_attr_write		*/	True,
		/* sat_exec			*/	True,
		/* sat_sysacct			*/	True,
		False,False,False,False,	/* holes in domain */
		/* sat_fchdir			*/	True,
		/* sat_fd_read			*/	False,
		/* sat_fd_read2			*/	False,
		/* sat_tty_setlabel		*/	True,
		/* sat_fd_write			*/	False,
		/* sat_fd_attr_write		*/	True,
		/* sat_pipe			*/	False,
		/* sat_dup			*/	False,
		/* sat_close			*/	False,
		False,False,False,False,False,False,
		False,False,False,False,False,	/* holes in domain */
		/* sat_fork			*/	True,
		/* sat_exit			*/	True,
		/* sat_proc_read		*/	True,
		/* sat_proc_write		*/	True,
		/* sat_proc_attr_read		*/	False,
		/* sat_proc_attr_write		*/	True,
		/* sat_proc_own_attr_write	*/	True,
		False,False,False,		/* holes in domain */
		/* sat_svipc_access		*/	False,
		/* sat_svipc_create		*/	True,
		/* sat_svipc_remove		*/	True,
		/* sat_svipc_change		*/	True,
		False,False,False,False,False,False,	/* holes in domain */
		/* sat_bsdipc_create		*/	True,
		/* sat_bsdipc_create_pair	*/	True,
		/* sat_bsdipc_shutdown		*/	False,
		/* sat_bsdipc_mac_change	*/	False,
		/* sat_bsdipc_address		*/	False,
		/* sat_bsdipc_resvport		*/	False,
		/* sat_bsdipc_deliver		*/	False,
		/* sat_bsdipc_cantfind		*/	False,
		/* sat_bsdipc_snoop_ok		*/	False,
		/* sat_bsdipc_snoop_fail	*/	False,
		/* sat_clock_set		*/	True,
		/* sat_hostname_set		*/	True,
		/* sat_domainname_set		*/	True,
		/* sat_hostid_set		*/	True,
		False,False,False,False,False,False,	/* holes in domain */
		/* sat_check_priv		*/	True,
		/* sat_control			*/	False,
		False,False,False,False,False,False,False,False, /* holes++ */
		/* sat_bsdipc_rx_ok		*/	False,
		/* sat_bsdipc_rx_range		*/	False,
		/* sat_bsdipc_rx_missing	*/	False,
		/* sat_bsdipc_tx_ok		*/	False,
		/* sat_bsdipc_tx_range		*/	False,
		/* sat_bsdipc_tx_toobig		*/	False,
		/* sat_bsdipc_if_config		*/	False,
		/* sat_bsdipc_if_invalid	*/	False,
		/* sat_bsdipc_if_setlabel	*/	False,
		False,				/* holes in domain */
		/* sat_ae_audit			*/	True,
		/* sat_ae_identity		*/	True,
		/* sat_ae_dbedit		*/	True,
		/* sat_ae_mount			*/	True,
		/* sat_ae_custom		*/	False,
		False,False,False,False,False,	/* holes in domain */
			};

	return ( dflt_event_state[i] );
}
#endif	/* USE_STUBS */
