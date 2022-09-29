/*
 *  cue.c
 *
 *  Description:
 *      Routines that implement fast forward and rewind for the CD Audio
 *      tool
 *
 *  History:
 *      rogerc      11/06/90    Created
 */

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "cue.h"
#include "display.h"


#define CUEINTERVAL 10
#define CUEQUANTUM 1

/*
 *  void ff_arm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Set up the time out function for fast-forwarding
 *
 *  Parameters:
 *      w           The fast forward button
 *      clientp     Client data
 *      call_data   ignored
 */

void ff_arm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	if (clientp->skipping && clientp->pauseid) {
		XtRemoveTimeOut( clientp->pauseid );
		clientp->pauseid = 0;
		clientp->skipping = 0;
		/*
		 * Need more up to date status than clientp->status
		 */
		CDgetstatus( clientp->cdplayer, clientp->status );
		if ((clientp->status->state == CD_PAUSED ||
		 clientp->status->state == CD_STILL) &&
		 CDtogglepause( clientp->cdplayer ))
			clientp->status->state = CD_PLAYING;
	}
		
	if (clientp->status->state == CD_PLAYING) {
		XtRemoveTimeOut( clientp->updateid );
			clientp->updateid = 0;
		clientp->ffid = XtAddTimeOut( CUEINTERVAL, ff, clientp );
		clientp->cueing = 1;
		CDtogglepause( clientp->cdplayer );
	}
}

/*
 *  void ff_disarm( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Remove the fast-forwarding time out function, thereby ending the
 *      fast forward action
 *
 *  Parameters:
 *      w           The fast forward button
 *      clientp     Client data
 *      call_data   ignored
 */

void ff_disarm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDTRACKINFO     info;
	int				sec;

	if (clientp->ffid ) {
		XtRemoveTimeOut( clientp->ffid );
		clientp->ffid = 0;
		if (clientp->cueing) {
			if (clientp->play_mode == PLAYMODE_NORMAL) {
				CDgettrackinfo( clientp->cdplayer,
				 clientp->status->track, &info );
				sec = (info.start_min + clientp->status->min) * 60
				 + info.start_sec + clientp->status->sec;
				CDplayabs( clientp->cdplayer, sec / 60, sec % 60, 0, 1 );
			}
			else
				CDplaytrackabs( clientp->cdplayer,
				 clientp->status->track, clientp->status->min,
				 clientp->status->sec, 0, 1 );
			clientp->cueing = 0;
		}
	}
	clientp->update_func( clientp, 0 );
}

/*
 *  void ff( CLIENTDATA *clientp, XtIntervalId *id )
 *
 *  Description:
 *      Perform one slice of the fast forwarding.  what we do is skip ahead
 *      CUEQUANTUM seconds every CUEINTERVAL milliseconds, and play little
 *      snippets of the CD
 *
 *  Parameters:
 *      clientp     Client data
 *      id          id of this time out
 */

void ff( CLIENTDATA *clientp, XtIntervalId *id )
{
	CDTRACKINFO info;
	int         min, sec, tmin, tsec, lmin, lsec;
	register CDSTATUS *status = clientp->status;

	/*
	 * Fast forwarded to the end of the disc
	 */
	if (status->state == CD_READY)
		return;

	sec = status->sec + status->min * 60;
	sec += CUEQUANTUM;
	min = sec / 60;
	sec %= 60;
	CDgettrackinfo( clientp->cdplayer, status->track, &info );

	if (min > info.total_min || (min == info.total_min &&
	 sec > info.total_sec)) {
		if (clientp->play_mode == PLAYMODE_NORMAL &&
		 status->track < status->last) {
			status->track++;
			status->min = 0;
			status->sec = 0;
			CDgettrackinfo( clientp->cdplayer, status->track, &info );
			status->abs_min = info.start_min;
			status->abs_sec = info.start_sec;
		}
		else if ((clientp->play_mode == PLAYMODE_PROGRAM ||
		 clientp->play_mode == PLAYMODE_SHUFFLE) &&
		 clientp->program->current < clientp->program->num_tracks - 1) {
			status->track = clientp->program->
			 tracks[++(clientp->program->current)];
			CDgettrackinfo( clientp->cdplayer, status->track, &info );
			status->abs_min = info.start_min;
			status->abs_sec = info.start_sec;
			status->min = 0;
			status->sec = 0;
		}
		else {
			clientp->play_mode == PLAYMODE_NORMAL ? 
			 CDstop( clientp->cdplayer ):
			 program_stop( clientp->cdplayer, clientp->program );
			clientp->cueing = 0;
			clientp->repeat = 0;
			CDgetstatus( clientp->cdplayer, status );
			set_status_bitmap( status->state );
		}
	}
	else {
		status->min = min;
		status->sec = sec;

		/*
		 * Absolute time
		 */
		sec = status->abs_sec + status->abs_min * 60;
		sec += CUEQUANTUM;
		status->abs_min = sec / 60;
		status->abs_sec = sec % 60;
	}
		
	draw_display( clientp );
	clientp->ffid = XtAddTimeOut( CUEINTERVAL, ff, clientp );
}

/*
 *  void
 *  rew_arm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Set up the rewind time out function, which gets rewind going.
 *
 *  Parameters:
 *      w           The rewind button
 *      clientp     Client data
 *      call_data   ignored
 */

void rew_arm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	if (clientp->skipping && clientp->pauseid) {
		XtRemoveTimeOut( clientp->pauseid );
		clientp->pauseid = 0;
		clientp->skipping = 0;
		/*
		 * Need more up to date status than clientp->status
		 */
		CDgetstatus( clientp->cdplayer, clientp->status );
		if ((clientp->status->state == CD_PAUSED ||
		 clientp->status->state == CD_STILL) &&
		 CDtogglepause( clientp->cdplayer ))
			clientp->status->state = CD_PLAYING;
	}

	if (clientp->status->state == CD_PLAYING) {
		clientp->rewid = XtAddTimeOut( CUEINTERVAL, rew, clientp );
		clientp->cueing = 1;
		CDtogglepause( clientp->cdplayer );
	}
}

/*
 *  void rew_disarm( Widget w, CLIENTDATA *clientp,
 *   XmAnyCallbackStruct *call_data )
 *
 *  Description:
 *      Remove the rewind time out, which stops the rewind action
 *
 *  Parameters:
 *      w           The rewind button
 *      clientp     Client data
 *      call_data   ignored
 */

void rew_disarm( Widget w, CLIENTDATA *clientp, XmAnyCallbackStruct *call_data )
{
	CDTRACKINFO info;
	int			sec;

	if (clientp->rewid) {
		XtRemoveTimeOut( clientp->rewid );
		clientp->rewid = 0;
		if (clientp->cueing) {
			if (clientp->play_mode == PLAYMODE_NORMAL) {
				CDgettrackinfo( clientp->cdplayer,
				 clientp->status->track, &info );
				sec = (info.start_min + clientp->status->min) * 60
				 + info.start_sec + clientp->status->sec;
				CDplayabs( clientp->cdplayer, sec / 60, sec % 60, 0, 1 );
			}
			else
				CDplaytrackabs( clientp->cdplayer,
				 clientp->status->track, clientp->status->min,
				 clientp->status->sec, 0, 1 );
			clientp->cueing = 0;
		}
	}
}

/*
 *  void rew( CLIENTDATA *clientp, XtIntervalId *id )
 *
 *  Description:
 *      Perform the rewind action.  The way this is done is we go back
 *      CUEQUANTUM seconds every CUEINTERVAL milliseconds, and play a little
 *      bit of the CD
 *
 *  Parameters:
 *      clientp         Client data
 *      id              time out id
 */

void rew( CLIENTDATA *clientp, XtIntervalId *id )
{
	CDTRACKINFO info;
	register CDSTATUS   *status = clientp->status;
	int         min, sec;

	if (status->state != CD_PLAYING)
		return;

	sec = status->sec + status->min * 60;
	sec -= CUEQUANTUM;
	min = sec / 60;
	sec %= 60;

	CDgettrackinfo( clientp->cdplayer, status->track, &info );

	if (sec < 0) {
		if (clientp->play_mode == PLAYMODE_NORMAL &&
		 status->track > status->first) {
			status->track--;
			CDgettrackinfo( clientp->cdplayer, status->track,
			 &info );
			status->min = info.total_min;
			status->sec = info.total_sec;
			sec = (info.start_min + info.total_min) * 60 + info.start_sec
			 + info.total_sec;
			status->abs_min = sec / 60;
			status->abs_sec = sec % 60;
		}
		else if ((clientp->play_mode == PLAYMODE_PROGRAM ||
		 clientp->play_mode == PLAYMODE_SHUFFLE) &&
		 clientp->program->current > 0) {
			status->track = clientp->program->
			 tracks[--(clientp->program->current)];
			CDgettrackinfo( clientp->cdplayer,
			 status->track, &info );
			status->min = info.total_min;
			status->sec = info.total_sec;
			sec = (info.start_min + info.total_min) * 60 + info.start_sec
			 + info.total_sec;
			status->abs_min = sec / 60;
		}
		else {
			clientp->play_mode == PLAYMODE_NORMAL ? 
			 CDstop( clientp->cdplayer ):
			 program_stop( clientp->cdplayer, clientp->program );
			clientp->cueing = 0;
			clientp->repeat = 0;
			CDgetstatus( clientp->cdplayer, status );
			set_status_bitmap( status->state );
		}
	}
	else {
		status->min = min;
		status->sec = sec;

		sec = status->abs_sec + status->abs_min * 60;
		sec -= CUEQUANTUM;
		status->abs_min = sec / 60;
		status->abs_sec = sec % 60;
	}

	draw_display( clientp );
	clientp->rewid = XtAddTimeOut( CUEINTERVAL, rew, clientp );
}
