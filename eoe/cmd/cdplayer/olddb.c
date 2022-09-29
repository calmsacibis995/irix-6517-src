/*
 *	database.c
 *
 *	Description:
 *		Code to retrieve stuff from and add stuff to the database
 *
 *	Ideas:
 *		Path for look-up of databases
 *
 *	History:
 *      rogerc      11/21/90    Created
 */

#include <unistd.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include "cdaudio.h"
#include "program.h"
#include "olddb.h"

#define MAXNAME		450
#define MAXCLASS	450

/*
 * The database is a resource database stored in $HOME/.cdplayerrc.  The
 * following resource names and classes are meaningful in this database.
 *
 * Example:
 *
 *  d0902430640063006270306022604420830.title		: Led Zeppelin
 * 	d0902430640063006270306022604420830.artist		: Led Zeppelin
 * 	d0902430640063006270306022604420830.trackCount	: 9
 * 	d0902430640063006270306022604420830.track.1		: Good Times Bad Times
 * 	d0902430640063006270306022604420830.track.2		: Babe I'm Gonna Leave You
 * 	d0902430640063006270306022604420830.track.3		: You Shook Me
 * 	d0902430640063006270306022604420830.track.4		: Dazed and Confused
 * 	d0902430640063006270306022604420830.track.5		: Your Time is Gonna Come
 * 	d0902430640063006270306022604420830.track.6		: Black Mountain Side
 * 	d0902430640063006270306022604420830.track.7		: Communication Breakdown
 * 	d0902430640063006270306022604420830.track.8		: I Can't Quit You Baby
 * 	d0902430640063006270306022604420830.track.9		: How Many More Times
 *
 * 	d0902430640063006270306022604420830.programCount			: 1
 * 	d0902430640063006270306022604420830.program.1.tracks		: 1 2 3 4 5
 * 	d0902430640063006270306022604420830.program.1.programName	: Side One
 *
 *	In this example, the title of the disc is Led Zeppelin, the artist is
 *	Led Zeppelin, it has 9 tracks, the titles of which are given in 
 *	*track.1 through *track.9.  There is one program, called Side One,
 *	consisting of tracks 1 through 5.
 */

static	const	char	id_n[] = "id";
static	const	char	id_c[] = "Id";
static	const	char	title_n[] = "title";
static	const	char	title_c[] = "Title";
static	const	char	artist_n[] = "artist";
static	const	char	artist_c[] = "Artist";
static	const	char	track_n[] = "track";
static	const	char	track_c[] = "Track";
static	const	char	track_count_n[] = "trackCount";
static	const	char	track_count_c[] = "TrackCount";
static	const	char	program_n[] = "program";
static	const	char	program_c[] = "Program";
static	const	char	tracks_n[] = "tracks";
static	const	char	tracks_c[] = "Tracks";
static	const	char	program_name_n[] = "programName";
static	const	char	program_name_c[] = "ProgramName";
static	const	char	program_count_n[] = "programCount";
static	const	char	program_count_c[] = "ProgramCount";
static	const	char	program_total_min_n[] = "totalMin";
static	const	char	program_total_min_c[] = "TotalMin";
static	const	char	program_total_sec_n[] = "totalSec";
static	const	char	program_total_sec_c[] = "TotalSec";

static	const	char	 dbfilename[] = "/.cdplayerrc";
static		char	 odb_rtrn_buf[MAXPATHLEN];

XrmDatabase		db = 0;

/***====================================================================***/

const char *
odb_get_dflt_file( void )
{
char	*home = getenv( "HOME" );
static char rtrn[MAXPATHLEN];

    if (home) {
	sprintf(rtrn,"%s%s", home, dbfilename);
	return rtrn;
    }
    return NULL;
}

/***====================================================================***/

int
odb_init( const char *file )
{
const char *dbfile;

    XrmInitialize( );
    dbfile = ( file ? file : odb_get_dflt_file() );
    db = ( file ? XrmGetFileDatabase( file ) : NULL );
    return db!=NULL;
}

/***====================================================================***/

int
odb_save( const char *file )
{
const char *dbfile;

    if (db) {
	dbfile = ( file ? file : odb_get_dflt_file() );
	if ( dbfile )
	    XrmPutFileDatabase( db, dbfile );
	return 1;
    }
    return 0;
}

/***====================================================================***/

void
odb_terminate( const char *file, int autoSave )
{
    if (autoSave)
	odb_save(file);
    if (db)
	XrmDestroyDatabase( db );
    return;
}

/***====================================================================***/

const char *
odb_get_info( const char *cd_id, const char *name, const char *class )
{
char		*rtrn_type, str_name[MAXNAME], str_class[MAXCLASS];
XrmValue	 rtrn_val;

    if (!db)
	return ( NULL );

    sprintf( str_name, "d%s.%s", cd_id, name );
    sprintf( str_class, "D%s.%s", cd_id, class );

    if (db && XrmGetResource( db, str_name, str_class, &rtrn_type, &rtrn_val ))
	 return rtrn_val.addr;
    else return NULL;
}

/***====================================================================***/

const char *
odb_get_track_name( const char *cd_id, int track )
{
char	str_name[MAXNAME], str_class[MAXCLASS];

    sprintf( str_name, "%s.%d", track_n, track );
    sprintf( str_class, "%s.%d", track_c, track );
    return odb_get_info( cd_id, str_name, str_class );
}

/***====================================================================***/

int
odb_get_track_names(const char *cd_id, const char **track_names, int max_tracks)
{
int	i,max;
char	buf[100], str_name[MAXNAME], str_class[MAXCLASS],
				*pname, *pclass, *return_type;

	sprintf( str_name, "%s.", track_n );
	sprintf( str_class, "%s.", track_c );
	pname = str_name + strlen( str_name );
	pclass = str_class + strlen( str_class );

	for (i = 0, max = -1; i <= max_tracks; i++) {
	    sprintf( pname, "%d", i );
	    sprintf( pclass, "%d", i );
	    track_names[i] = odb_get_info( cd_id, str_name, str_class );
	    if (track_names[i])
		max = i;
	}

	return max;
}

/***====================================================================***/

const char *
odb_get_title( const char *cd_id )
{
    return odb_get_info( cd_id, title_n, title_c );
}

/***====================================================================***/

const char *
odb_get_artist( const char *cd_id )
{
    return odb_get_info( cd_id, artist_n, artist_c );
}

/***====================================================================***/

static int
odb_put_info( const char *cd_id, const char *name, const char *value )
{
char		str_name[MAXNAME];

    sprintf( str_name, "d%s.%s", cd_id, name );
    XrmPutStringResource( &db, str_name, value );
    return 1;
}

/***====================================================================***/

int
odb_get_converted( const char *cd_id )	
{
const char *tmp = odb_get_info( cd_id, "converted", "Converted" );
    if (tmp) {
	return atoi(tmp);
    }
    return 0;
}

/***====================================================================***/

void
odb_put_converted( const char *cd_id, int converted )
{
char buf[10];

    sprintf(buf,"%d",converted);
    odb_put_info( cd_id, "converted", buf );
    return;
}

/***====================================================================***/

int
odb_put_track_names( const char *cd_id, char **track_names, int num_tracks )
{
int		i;
char		str_name[MAXNAME], *id, *pname;

    sprintf( str_name, "d%s.%s.", cd_id, track_n );
    pname = str_name + strlen( str_name );

    for (i = 0; i < num_tracks; i++) {
	sprintf( pname, "%d", i );
	if (track_names[i])
	    odb_put_info( cd_id, str_name, track_names[i] );
    }

    return 1;
}

/***====================================================================***/

int
odb_put_title( const char *cd_id, const char *title )
{
    return odb_put_info( cd_id, title_n, title );
}

/***====================================================================***/

int
odb_put_artist( const char *cd_id, const char *artist )
{
    return odb_put_info( cd_id, artist_n, artist );
}

/***====================================================================***/

int
odb_put_program( const char *cd_id, CDPROGRAM *program, int num )
{
char	str_name[MAXNAME], *id, *tracks, buf[30];
int	i;

    sprintf( str_name, "d%s.%s.%d.%s", id, program_n, num, program_name_n );
    XrmPutStringResource( &db, str_name, program->name );

    tracks = (char *)malloc( program->num_tracks * strlen( "99 " ) + 1 );

    tracks[0] = '\0';
    for (i = 0; i < program->num_tracks; i++) {
	sprintf( buf, "%d ", program->tracks[i] );
	strcat( tracks, buf );
    }

    sprintf( str_name, "d%s.%s.%d.%s", id, program_n, num, tracks_n );
    XrmPutStringResource( &db, str_name, tracks );

    sprintf( buf, "%d", program->num_tracks );
    sprintf( str_name, "d%s.%s.%d.%s", id, program_n, num,  track_count_n );
    XrmPutStringResource( &db, str_name, buf );

    free( tracks );

    sprintf( buf, "%d", program->total_min );
    sprintf(str_name, "d%s.%s.%d.%s", id, program_n, num, program_total_min_n);
    XrmPutStringResource( &db, str_name, buf );

    sprintf( buf, "%d", program->total_sec );
    sprintf(str_name, "d%s.%s.%d.%s", id, program_n, num, program_total_sec_n);
    XrmPutStringResource( &db, str_name, buf );

    return 1;
}

/***====================================================================***/

const char *
odb_get_program_name( const char *cd_id, int num )
{
char		*id, str_name[MAXNAME], str_class[MAXCLASS], *return_type;
XrmValue	return_val;

    sprintf( str_name, "d%s.%s.%d.%s", cd_id, program_n, num, program_name_n );
    sprintf( str_class, "D%s.%s.%d.%s", cd_id, program_c, num, program_name_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val ))
	return (strdup( return_val.addr ));

    return (NULL);
}


/***====================================================================***/

CDPROGRAM *
odb_get_program( const char *cd_id, int num )
{
char		*tracks, str_name[MAXNAME], str_class[MAXCLASS], *return_type,
				*track;
XrmValue	return_val;
CDPROGRAM	*program;
int		num_tracks;

    program = (CDPROGRAM *)malloc( sizeof (*program) );
    memset( program, 0, sizeof (*program) );

    sprintf( str_name, "d%s.%s.%d.%s", cd_id, program_n, num, program_name_n );
    sprintf( str_class, "D%s.%s.%d.%s", cd_id, program_c, num, program_name_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val ))
	program->name = strdup( return_val.addr );

    sprintf( str_name, "d%s.%s.%d.%s", cd_id, program_n, num,
	 					program_total_min_n );
    sprintf( str_class, "D%s.%s.%d.%s", cd_id, program_c, num,
						program_total_min_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val ))
	program->total_min = atoi( return_val.addr );

    sprintf( str_name, "d%s.%s.%d.%s", cd_id, program_n, num,
						program_total_sec_n );
    sprintf( str_class, "D%s.%s.%d.%s", cd_id, program_c, num,
	 					program_total_sec_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val ))
	program->total_sec = atoi( return_val.addr );

    sprintf( str_name, "d%s.%s.%d.%s", cd_id, program_n, num, track_count_n );
    sprintf( str_class, "D%s.%s.%d.%s", cd_id, program_c, num, track_count_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val ))
	program->num_tracks = atoi( return_val.addr );

    if (program->num_tracks)
	program->tracks = malloc( sizeof (int) * program->num_tracks );

    sprintf( str_name, "d%s.%s.%d.%s", cd_id, program_n, num, tracks_n );
    sprintf( str_class, "D%s.%s.%d.%s", cd_id, program_c, num, tracks_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val )) {
	num_tracks = 0;
	tracks = strdup( return_val.addr );
	for (	track = strtok( tracks, " " );
		num_tracks < program->num_tracks && track;
		track = strtok( NULL, " " )) {
	    program->tracks[num_tracks++] = atoi( track );
	}
	free( tracks );
    }

    return (program);
}

/***====================================================================***/

int
odb_put_program_count( const char *cd_id, int num )
{
char	str_name[MAXNAME], buf[100];

    sprintf( str_name, "d%s.%s.%s", cd_id, program_n, program_count_n );
    sprintf( buf, "%d", num );
    XrmPutStringResource( &db, str_name, buf );

    return (1);
}

/***====================================================================***/

int
odb_get_program_count( const char *cd_id )
{
char		str_name[MAXNAME], str_class[MAXCLASS], *return_type;
XrmValue	return_val;

    sprintf( str_name, "d%s.%s.%s", cd_id, program_n, program_count_n );
    sprintf( str_class, "D%s.%s.%s", cd_id, program_c, program_count_c );

    if (XrmGetResource( db, str_name, str_class, &return_type, &return_val ))
	return (atoi( return_val.addr ));
	
    return (0);
}

/***====================================================================***/

int
odb_free_track_names( char **track_names, int num_tracks )
{
int		i;

    for (i = 0; i < num_tracks; i++)
	free( track_names[i] );

    free( track_names );

    return 1;
}

/***====================================================================***/

const char *
odb_get_id( CDPLAYER *cdplayer, CDSTATUS *status )
{
CDTRACKINFO	info;
int		min, sec, i;
char		buf[MAXNAME], *bufp = buf, *tmp;

    if (status->state == CD_NODISC || status->state == CD_ERROR)
	return (NULL);

    sprintf( bufp, "%02d", status->last - status->first + 1 );
    bufp += 2;

    for (i = status->first; i <= status->last; i++) {
	CDgettrackinfo( cdplayer, i, &info );
	sprintf( bufp, "%02d%02d", info.total_min, info.total_sec );
	bufp += 4;
    }
    *bufp++ = '\0';
    return (strdup( buf ));
}

/***====================================================================***/

#define	LIST_DISCS_CMD	"/bin/sed -e \"s/\\..*//g\" %s | /usr/bin/uniq"

typedef struct {
	FILE	*file;
	char	buf[MAXNAME];
} CDListState;

const char *
odb_first_disc( const char *fname, void **state )
{
CDListState	*lstate;
extern FILE *popen( const char *, const char *);

    lstate= (CDListState *)malloc(sizeof(CDListState));
    if (lstate) {
	*state= lstate;
	sprintf(lstate->buf,LIST_DISCS_CMD,(fname?fname:odb_get_dflt_file()));

	lstate->file = popen(lstate->buf,"r");
	return odb_next_disc( state );
    }
    return NULL;
}

/***====================================================================***/

const char *
odb_next_disc( void **state )
{
CDListState	*lstate= (CDListState *)*state;

    if (lstate && (lstate->file)) {
	if (fscanf(lstate->file,"%s",lstate->buf)==1) {
	    if (lstate->buf[0]=='d')
		return &lstate->buf[1];
	}
	odb_done( state );
    }
    return NULL;
}

/***====================================================================***/

void
odb_done( void **state )
{
CDListState *lstate= (CDListState *)*state;

    if (lstate) {
	if (lstate->file)
	    fclose(lstate->file);
	lstate->file= NULL;
	free(lstate);
    }
    *state= NULL;
    return;
}

