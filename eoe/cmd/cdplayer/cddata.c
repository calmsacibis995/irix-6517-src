/*
 *	cddata.c
 *
 *	Description:
 *		Support for getting CD data from the user
 *
 *	History:
 *      rogerc      11/27/90    Created
 */
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/BulletinB.h>
#include <Xm/TextF.h>
#include <Xm/Form.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "cddata.h"
#include "database.h"

#define min(a,b)	((a) < (b) ? (a) : (b))

static struct tag_datadbox {
	Widget 	   bb, title, artist, ok, cancel, scrwin, scrollbar,
		   form, text[8], label[8];
	char       **tracks;
	int        value;
}	datadbox;

int init_data_dbox( CLIENTDATA *clientp );

/*
 *  void popup_data_dbox(Widget w, CLIENTDATA *clientp,
 *                       XmAnyCallbackStruct *call_data)
 *
 *  Description:
 *	Bring up the data dialog box so user can enter name, artist, and
 *	track titles of current cd.  This function is a callback function
 *	for the data button on the main window.
 *
 *  Parameters:
 *      w		The data button on the main window
 *      clientp		client data
 *      call_data	ignored
 */

void popup_data_dbox(Widget w, CLIENTDATA *clientp,
		     XmAnyCallbackStruct *call_data)
{
	XtManageChild(datadbox.bb);
	if (!init_data_dbox(clientp))
		XtUnmanageChild(datadbox.bb);
}

/*
 *  int init_data_dbox(CLIENTDATA *clientp)
 *
 *  Description:
 *	Initializes the data dialog box.  This involves retrieving the title,
 *	artist, and track names from the database and filling in the
 *	appropriate widgets.  Also, if there are more tracks than we have
 *	edit fields, we have to initialize the scroll bar so the user can
 *	scroll to the extra tracks.
 *
 *  Parameters:
 *      clientp			client data
 *
 *  Returns:
 *	1 if successful, 0 otherwise
 */

int init_data_dbox(CLIENTDATA *clientp)
{
	const char	*title, *artist;
	int		num_tracks, i, n;
	Arg		wargs[10];
	CDSTATUS	*status = clientp->status;
	char		buf[20];
	const char	*tracks[100];

	if (!clientp->data) {
		clientp->data = db_init_from_cd(clientp->cdplayer, 
						clientp->status);
	}
	if (!clientp->data)
		return (0);

	num_tracks = db_get_track_count(clientp->data);
	for (i = 0; i <= num_tracks; i++)
	    tracks[i] = NULL;
	db_get_track_names(clientp->data, tracks, 100);

	if (status->state == CD_NODISC || status->state == CD_ERROR)
		return (0);

	title = db_get_title(clientp->data);
	title = title ? title : "";
	XmTextFieldSetString(datadbox.title, title);

	artist = db_get_artist(clientp->data);
	artist = artist ? artist : "";
	XmTextFieldSetString(datadbox.artist, artist);

	for (i = 0; i < num_tracks && i < XtNumber(datadbox.label); i++) {
		XmString str;
		sprintf(buf, "%d:", i+status->first);
		str = XmStringCreateLtoR(buf, XmSTRING_DEFAULT_CHARSET);
		n = 0;
		XtSetArg(wargs[n], XmNlabelString, str); n++;
		XtSetValues(datadbox.label[i], wargs, n);
		XmStringFree(str);
		title = tracks[i + status->first] ?
			tracks[i + status->first]: "";
		XmTextFieldSetString(datadbox.text[i], title);
		n = 0;
		XtSetArg(wargs[n], XmNcursorPosition, 0); n++;
		XtSetValues(datadbox.text[i+status->first], wargs, n);
	}

	XtManageChildren(datadbox.label,
			 min(num_tracks, XtNumber(datadbox.label)));
	XtManageChildren(datadbox.text,
			 min(num_tracks, XtNumber(datadbox.text)));

	if (num_tracks < XtNumber( datadbox.label )) {
		XtUnmanageChildren(datadbox.label + num_tracks,
				   XtNumber(datadbox.label) - num_tracks);
		XtUnmanageChildren(datadbox.text + num_tracks,
				   XtNumber(datadbox.text) - num_tracks);
	}

	if (num_tracks > XtNumber( datadbox.label )) {
		n = 0;
		XtSetArg(wargs[n], XmNvalue, 0); n++;
		XtSetArg(wargs[n], XmNminimum, 0); n++;
		XtSetArg(wargs[n], XmNmaximum, num_tracks); n++;
		XtSetArg(wargs[n], XmNsliderSize,
			 XtNumber(datadbox.label)); n++;
		XtSetValues(datadbox.scrollbar, wargs, n);
		XtManageChild(datadbox.scrollbar);
	}
	else {
		XtUnmanageChild(datadbox.scrollbar);
	}
	datadbox.tracks = (char **)calloc(num_tracks + 1, sizeof(char *));
	if (datadbox.tracks) {
		for (i = 1 ; i <= num_tracks ; i ++) {
			datadbox.tracks[i] = tracks[i] ?
				strdup(tracks[i]) : NULL;
		}
	}
	datadbox.value = 0;
		
	return (1);
}

/*
 *  void data_ok(Widget w, CLIENTDATA *clientp,
 *               XmAnyCallbackStruct *call_data)
 *
 *  Description:
 *	Callback function for the OK button on the data dialog box.  Get the
 *	title, artist, and track names and store them in the database.
 *
 *  Parameters:
 *      w			the OK button
 *      clientp		client data
 *      call_data	ignored
 */

void data_ok( Widget w, CLIENTDATA *clientp,
	     XmAnyCallbackStruct *call_data )
{
	char		*title, *artist;
	int		n, num_tracks, i, first_entry;
	Arg		wargs[10];
	char		*tracks[100];
	CDSTATUS	*status;

	if (!clientp->data)
		return;

	title = XmTextFieldGetString(datadbox.title);
	artist = XmTextFieldGetString(datadbox.artist);
	db_put_title(clientp->data, title);
	db_put_artist(clientp->data, artist);

	status = clientp->status;

	if (!XtIsManaged(datadbox.scrollbar))
		first_entry = 0;
	else {
		n = 0;
		XtSetArg(wargs[n], XmNvalue, &first_entry); n++;
		XtGetValues(datadbox.scrollbar, wargs, n);
	}

	num_tracks = status->last - status->first + 1;
	first_entry+= status->first;

	for (i = 0; i < num_tracks && (i < XtNumber(datadbox.text)); i++) {
		if (datadbox.tracks[i+first_entry])
			free(datadbox.tracks[i + first_entry]);
		datadbox.tracks[i + first_entry]
			= XmTextFieldGetString(datadbox.text[i]);
	}
	
	db_put_track_names(clientp->data, (const char **)datadbox.tracks,
			   num_tracks + 1);
	if (datadbox.tracks) {
		for (i = 1; i <= num_tracks ; i++) {
			if (datadbox.tracks[i])
				free(datadbox.tracks[i]);
			datadbox.tracks[i]= NULL;
		}
		free(datadbox.tracks);
		datadbox.tracks = NULL;
	}
	db_save(clientp->data);
	clientp->data_changed = 1;

	XtUnmanageChild(datadbox.bb);
}

/*
 *  void data_cancel(Widget w, CLIENTDATA *clientp,
 *                   XmAnyCallbackStruct *call_data)
 *
 *  Description:
 *	callback function for the Cancel button.
 *
 *  Parameters:
 *      w			The Cancel button
 *      clientp		client data
 *      call_data	ignored
 */

void data_cancel(Widget w, CLIENTDATA *clientp,
		 XmAnyCallbackStruct *call_data)
{
	free(datadbox.tracks);
	XtUnmanageChild(datadbox.bb);
}

/*
 *  void data_nodisc(CLIENTDATA *clientp)
 *
 *  Description:
 *	Clean up if user ejects disc while dialog box is up
 *
 *  Parameters:
 *      clientp		client data
 */

void data_nodisc(CLIENTDATA *clientp)
{
	if (XtIsManaged(datadbox.bb))
		data_cancel(datadbox.cancel, clientp, NULL);
}

/*
 *  void data_scroll(Widget w, CLIENTDATA *clientp,
 *                   XmScrollBarCallbackStruct *call_data)
 *
 *  Description:
 *	callback function for the scroll bar.  Store away the track names
 *	in the track edit fields, and then fill in the edit fields to reflect
 *	the new state of the scroll bar
 *
 *  Parameters:
 *      w			the scroll bar
 *      clientp		client data
 *      call_data	ignored
 */

void data_scroll(Widget w, CLIENTDATA *clientp,
		 XmScrollBarCallbackStruct *call_data)
{
	char buf[100],*text;
	int i, n, first_entry;
	Arg wargs[10];

	first_entry = datadbox.value+clientp->status->first;
	for (i = 0; i < XtNumber(datadbox.label); i++) {
		if (datadbox.tracks[i + first_entry])
			free( datadbox.tracks[i + first_entry]);
		datadbox.tracks[i + first_entry] =
			XmTextFieldGetString(datadbox.text[i]);
	}

	first_entry = call_data->value+clientp->status->first;
	for (i = 0; i < XtNumber(datadbox.text); i++) {
		XmString str;
		text = datadbox.tracks[i + first_entry];
		if (text == NULL)
			text = strdup("");
		XmTextFieldSetString(datadbox.text[i], text);
		n = 0;
		XtSetArg(wargs[n], XmNcursorPosition, 0); n++;
		XtSetValues(datadbox.text[i], wargs, n);

		sprintf(buf, "%2d:", first_entry + i);
		n = 0;
		str = XmStringCreateLtoR(buf, XmSTRING_DEFAULT_CHARSET);
		XtSetArg(wargs[n], XmNlabelString, str); n++;
		XtSetValues(datadbox.label[i], wargs, n);
		XmStringFree(str);
	}

	datadbox.value = call_data->value;
}

/*
 *  Widget create_data_dbox(Widget parent, CLIENTDATA *clientp)
 *
 *  Description:
 *	Create the widgets that comprise the data dialog box
 *
 *  Parameters:
 *      parent		Parent of dialog box
 *      clientp		client data
 *
 *  Returns:
 *	The dialog shell
 */

Widget create_data_dbox(Widget parent, CLIENTDATA *clientp)
{
	Arg	wargs[10];
	int	n, i;
	Widget	children[40];
	char	name[30];

	n = 0;
	XtSetArg(wargs[n], XmNautoUnmanage, FALSE); n++;
	datadbox.bb = XmCreateBulletinBoardDialog(parent, "data", wargs, n);

	n = 0;
	children[n++] = datadbox.title
		= XmCreateTextField(datadbox.bb, "title", NULL, 0);
	children[n++] = datadbox.artist
		= XmCreateTextField(datadbox.bb, "artist", NULL, 0);
	children[n++] = datadbox.ok
		= XmCreatePushButton(datadbox.bb, "ok", NULL, 0);
	children[n++] = datadbox.cancel
		= XmCreatePushButton(datadbox.bb, "cancel", NULL, 0);
	children[n++] = XmCreateLabel(datadbox.bb, "titleLabel", NULL, 0);
	children[n++] = XmCreateLabel(datadbox.bb, "artistLabel", NULL, 0);
	
	XtManageChildren(children, n);

	n = 0;
	XtSetArg(wargs[n], XmNscrollingPolicy, XmAPPLICATION_DEFINED); n++;
	datadbox.scrwin
		= XmCreateScrolledWindow(datadbox.bb, "tracks", wargs, n);

	XtManageChild(datadbox.scrwin);

	datadbox.scrollbar = XmCreateScrollBar(datadbox.scrwin,
	 "scrollbar", NULL, 0);
	datadbox.form = XmCreateForm(datadbox.scrwin, "form", NULL, 0);

	XtManageChild(datadbox.scrollbar);
	XtManageChild(datadbox.form);

	n = 0;
	XtSetArg(wargs[n], XmNverticalScrollBar, datadbox.scrollbar); n++;
	XtSetArg(wargs[n], XmNworkWindow, datadbox.form); n++;
	XtSetValues(datadbox.scrwin, wargs, n);

	n = 0;
	XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNrecomputeSize, False); n++;
	datadbox.label[0] = XmCreateLabel(datadbox.form, "label0", wargs, n);

	n = 0;
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(wargs[n], XmNrightAttachment, XmATTACH_FORM); n++;
	datadbox.text[0]
		= XmCreateTextField(datadbox.form, "track0", wargs, n);

	for (i = 1; i < XtNumber(datadbox.label); i++) {
		n = 0;
		XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(wargs[n], XmNtopWidget, datadbox.text[i - 1]); n++;
		XtSetArg(wargs[n], XmNrecomputeSize, False); n++;
		sprintf(name, "label%d", i);
		datadbox.label[i]
			= XmCreateLabel(datadbox.form, name, wargs, n);

		n = 0;
		XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(wargs[n], XmNtopWidget, datadbox.text[i - 1]); n++;
		XtSetArg(wargs[n], XmNrightAttachment, XmATTACH_FORM); n++;
		sprintf(name, "track%d", i);
		datadbox.text[i]
			= XmCreateTextField(datadbox.form, name, wargs, n);
	}

	XtManageChildren(datadbox.label, XtNumber(datadbox.label));
	XtManageChildren(datadbox.text, XtNumber(datadbox.text));

	XtAddCallback(datadbox.ok, XmNactivateCallback, data_ok, clientp);
	XtAddCallback(datadbox.cancel, XmNactivateCallback,
		      data_cancel, clientp);
	XtAddCallback(datadbox.scrollbar, XmNvalueChangedCallback,
		      data_scroll, clientp);

	/*
	 * Setup OK and cancel buttons to respond properly to return
	 * and escape keys.
	 */
	n = 0;
	XtSetArg(wargs[n], XmNdefaultButton, datadbox.ok); n++;
	XtSetArg(wargs[n], XmNcancelButton, datadbox.cancel); n++;
	XtSetValues(datadbox.bb, wargs, n);

	/*
	 * Get keyboard traversal in the right order
	 */
	XmRemoveTabGroup(datadbox.scrollbar);
	XmAddTabGroup(datadbox.title);
	XmAddTabGroup(datadbox.artist);
	for (i = 0; i < XtNumber( datadbox.text ); i++) {
		XmAddTabGroup(datadbox.text[i]);
	}
	XmAddTabGroup(datadbox.scrollbar);
	XmAddTabGroup(datadbox.ok);
	XmAddTabGroup(datadbox.cancel);

	return (datadbox.bb);
}
