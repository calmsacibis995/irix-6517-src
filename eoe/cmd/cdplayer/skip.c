/*
 *	skip.c
 *
 *	Description:
 *		Routines for skipping through the tracks on a CD
 *
 *	History:
 *      rogerc      11/06/90    Created
 */

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "skip.h"

#define PAUSEINTERVAL 2000

static void unpause( CLIENTDATA *clientp, XtIntervalId *id );

/*
 *	void skipf( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
 *
 *	Description:
 *		Skip to the next track and pause for PAUSEINTERVAL seconds if playing
 *		Skip to the next track and continue pausing if paused
 *		Move clientp->start_track to next track if ready
 *
 *	Parameters:
 *		w			The skipf button
 *		clientp		Client data
 *		call_data	ignored
 */

void skipf( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDSTATUS	status;

	/*
	 * Need more up to date status than clientp->status
	 */
	CDgetstatus( clientp->cdplayer, &status );

	if (clientp->start_track < status.first)
		clientp->start_track = status.first;

	switch (status.state) {

	case CD_PAUSED:
	case CD_STILL:
	case CD_PLAYING:
		switch (clientp->play_mode) {

		case PLAYMODE_NORMAL:
		default:
			if (status.track < status.last) {
				CDplay( clientp->cdplayer, status.track + 1, 0 );
				if (clientp->pauseid) {
					XtRemoveTimeOut( clientp->pauseid );
					clientp->pauseid = 0;
					clientp->skipping = 0;
				}
				if (!clientp->user_paused) {
					clientp->pauseid = XtAddTimeOut( PAUSEINTERVAL, unpause,
					 clientp );
					clientp->skipping = 1;
				}
			}
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			if (!program_playing_last( clientp->program )) {
				program_inc_current( clientp->program );
				program_play( clientp->cdplayer, clientp->program, 0 );
				if (clientp->pauseid) {
					XtRemoveTimeOut( clientp->pauseid );
					clientp->pauseid = 0;
					clientp->skipping = 0;
				}
				if (!clientp->user_paused) {
					clientp->pauseid = XtAddTimeOut( PAUSEINTERVAL, unpause,
					 clientp );
					clientp->skipping = 1;
				}
			}
			break;
		}
		break;

	case CD_READY:
		switch (clientp->play_mode) {

		default:
		case PLAYMODE_NORMAL:
			if (clientp->start_track < status.last) {
				clientp->start_track++;
				draw_display( clientp );
			}
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			if (!program_playing_last( clientp->program )) {
				program_inc_current( clientp->program );
				draw_display( clientp );
			}
			break;
		}
		break;

	default:
		break;
	}
}

/*
 *	void skipb( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
 *
 *	Description:
 *		Skip back one track and pause for PAUSEINTERVAL milliseconds if playing
 *		Skip back and pause indefinitely if paused
 *		move clientp->start_track back one if ready
 *
 *	Parameters:
 *		w			The skipb button
 *		clientp		Client data
 *		call_data	ignored
 */

void skipb( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDSTATUS				status;
	int						start;

	/*
	 * Need more up to date status than clientp->status
	 */
	CDgetstatus( clientp->cdplayer, &status );

	switch (status.state) {

	case CD_PLAYING:
		switch (clientp->play_mode) {
		case PLAYMODE_NORMAL:
		default:
			CDplay( clientp->cdplayer, status.track, 0 );
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			program_play( clientp->cdplayer, clientp->program, 0 );
			break;
		}
		break;

	case CD_STILL:
	case CD_PAUSED:
		switch (clientp->play_mode) {
		case PLAYMODE_NORMAL:
		default:
			if (clientp->user_paused && !clientp->skipped_back) {
				start = status.track;
				clientp->skipped_back = 1;
			}
			else
				start =
				 status.track > status.first ? status.track - 1 : status.first;
			CDplay( clientp->cdplayer, start, 0 );
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			if (clientp->user_paused && !clientp->skipped_back)
				clientp->skipped_back = 1;
			else
				program_dec_current( clientp->program );
			program_play( clientp->cdplayer, clientp->program, 0 );
			break;
		}
		break;

	case CD_READY:
		switch (clientp->play_mode) {
		case PLAYMODE_NORMAL:
		default:
			if (clientp->start_track > status.first) {
				clientp->start_track--;
				draw_display( clientp );
			}
			break;

		case PLAYMODE_SHUFFLE:
		case PLAYMODE_PROGRAM:
			program_dec_current( clientp->program );
			draw_display( clientp );
			break;
		}
		break;

	default:
		break;
	}
	if (clientp->pauseid) {
		XtRemoveTimeOut( clientp->pauseid );
		clientp->pauseid = 0;
		clientp->skipping = 0;
	}
	if (!clientp->user_paused) {
		clientp->pauseid = XtAddTimeOut( PAUSEINTERVAL, unpause, clientp );
		clientp->skipping = 1;
	}
}

/*
 *	static void unpause( CLIENTDATA *clientp, XtIntervalId *id )
 *
 *	Description:
 *		Takes the CD-ROM drive out of pause state for when a skip has
 *		occurred and its time out has expired
 *
 *	Parameters:
 *		clientp		Client data
 *		id			time out id
 */

static void unpause( CLIENTDATA *clientp, XtIntervalId *id )
{
	CDSTATUS	status;

	clientp->pauseid = 0;
	clientp->skipping = 0;
	/*
	 * Need more up to date status than clientp->status
	 */
	CDgetstatus( clientp->cdplayer, &status );

	if (status.state == CD_PAUSED || status.state == CD_STILL)
		CDtogglepause( clientp->cdplayer );
}
