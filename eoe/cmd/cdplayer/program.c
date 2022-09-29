/*
 * 	program.c
 *
 * 	Description:
 *		Routines to handle programming for CD-ROM audio
 *
 *	History:
 *      rogerc      11/01/90    Created
 */

#include <sys/time.h>
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
#include "database.h"

#define DEBUG 1

/*
 *  int program_play( CDPLAYER *cdplayer, CDPROGRAM *program, int play )
 *
 *  Description:
 *		Start playing program on cdplayer
 *
 *  Parameters:
 *      cdplayer		cdplayer to play program on
 *      program			program to play
 *      play			1 to start playing, 0 to start paused
 *
 *  Returns:
 *		non-zero if successful, 0 otherwise
 */

int program_play( CDPLAYER *cdplayer, CDPROGRAM *program, int play )
{
	program->active = 1;

	return (CDplaytrack( cdplayer,
	 program->tracks[program->current], play ));
}

/*
 *  int program_next( CDPLAYER *cdplayer, CDPROGRAM *program )
 *
 *  Description:
 *		Skips to the next track in the program, if we're not already on
 *		the last.
 *
 *  Parameters:
 *      cdplayer
 *      program
 *
 *  Returns:
 *		1 if skipped, 0 if we were already on last track
 */

int program_next( CDPLAYER *cdplayer, CDPROGRAM *program )
{
	if (program->current >= program->num_tracks - 1) {
		program_stop( cdplayer, program );
		return (0);
	}

	program->active = 1;
	return (CDplaytrack( cdplayer,
	 program->tracks[++(program->current)], 1 ));
}

/*
 *  int program_rew_prev( CDPLAYER *cdplayer, CDPROGRAM *program, int quantum )
 *
 *  Description:
 *		Support for the rewind function in program play mode.  Set back the
 *		clock quantum seconds, going to the previous track in the program
 *		if necessary
 *
 *  Parameters:
 *      cdplayer
 *      program
 *      quantum			Number of seconds to skip back
 *
 *  Returns:
 *		0 if we're at the beginning of the program or if CDplayabs() fails,
 *		non-zero otherwise
 */

int program_rew_prev( CDPLAYER *cdplayer, CDPROGRAM *program, int quantum )
{
	int			min, sec, smin, ssec;
	CDTRACKINFO	info;

	if (program->current == 0) {
		program_stop( cdplayer, program );
		return (0);
	}

	program->active = 1;
	if (!CDgettrackinfo( cdplayer, program->tracks[--(program->current)],
	 &info ))
		return (0);
	sec = info.total_sec + info.total_min * 60 - quantum + info.start_min * 60
	 + info.start_sec;
	min = sec / 60;
	sec %= 60;
	return (CDplayabs( cdplayer, min, sec, 0, 1 ));
}

/*
 *  int program_stop( CDPLAYER *cdplayer, CDPROGRAM *program )
 *
 *  Description:
 *		Stops play of program
 *
 *  Parameters:
 *      cdplayer
 *      program
 *
 *  Returns:
 *		Success or failure of CDstop()
 */

int program_stop( CDPLAYER *cdplayer, CDPROGRAM *program )
{
	program->active = 0;
	program->current = 0;
	return (CDstop( cdplayer ));
}

/*
 *  CDPROGRAM *program_shuffle( CDPLAYER *cdplayer )
 *
 *  Description:
 *		Creates a program consisting of all the tracks on the current
 *		disc in random order.  Each track appears exactly once.
 *
 *  Parameters:
 *      cdplayer
 *
 *  Returns:
 *		A program of the tracks on the cd in random order
 *		NULL if failure
 */

CDPROGRAM *program_shuffle( CDPLAYER *cdplayer )
{
	CDSTATUS		status;
	CDPROGRAM		*program;
	int				tracks_left, i, track, temp;
	struct timeval	t;
	struct timezone	tz;

	CDgetstatus( cdplayer, &status );

	if (status.state == CD_NODISC || status.state == CD_ERROR)
		return (NULL);

	program = (CDPROGRAM *)malloc( sizeof (*program) );

	program->current = 0;
	program->active = 0;
	program->tracks = (int *)malloc( sizeof (int) *
	 (status.last - status.first + 1) );
	program->num_tracks = status.last - status.first + 1;

	for (i = 0; i < program->num_tracks; i++)
		program->tracks[i] = status.first + i;

	if (0 == gettimeofday( &t, &tz ))
		srandom( t.tv_usec );

	for ((tracks_left = status.last - status.first + 1), (i = 0);
	 tracks_left > 1; i++, tracks_left--) {
		track = i + (random( ) % tracks_left);
		temp = program->tracks[i];
		program->tracks[i] = program->tracks[track];
		program->tracks[track] = temp;
	}
	program->total_min = status.total_min;
	program->total_sec = status.total_sec;

	return (program);
}

/*
 *  int free_program( CDPROGRAM *program )
 *
 *  Description:
 *		Free all the memory used by program
 *
 *  Parameters:
 *      program		program to free
 *
 *  Returns:
 *		1
 */

int free_program( CDPROGRAM *program )
{
	if (program->num_tracks && program->tracks)
		free( program->tracks );
	free( program );
	return (1);
}
