/*
 *	program.h
 *
 *	Description:
 *		Header file for program.c
 *
 *	History:
 *      rogerc      11/07/90    Created
 */

#define program_active(p) ((p)->active)
#define program_inc_current(p) ((p)->current += \
 (p)->current < ((p)->num_tracks - 1) ? 1 : 0)
#define program_dec_current(p) ((p)->current -= \
 (p)->current > 0 ? 1 : 0)
#define program_playing_last(p) ((p)->current == (p)->num_tracks - 1)

typedef struct {
	unsigned int	active : 1;
	char			*name;
	int				*tracks;
	int				num_tracks;
	int				current;
	int				total_min;
	int				total_sec;
}	CDPROGRAM;

int program_play( CDPLAYER *cdplayer, CDPROGRAM *program, int play );
int program_next( CDPLAYER *cdplayer, CDPROGRAM *program );
int program_stop( CDPLAYER *cdplayer, CDPROGRAM *program );
CDPROGRAM *program_shuffle( CDPLAYER *cdplayer );
