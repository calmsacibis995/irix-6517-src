#include <unistd.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include "cdaudio.h"
#include "program.h"
#include "database.h"
#include "olddb.h"

#define MAXFIELD	128
#define MAXNAME		450
#define MAXCLASS	450

/*
 * The database is a collection of files stored, by default, in the directory
 * $HOME/.cddb.  
 */

static	const	char	album_n[] = "album";
static	const	char	album_c[] = "Album";
static	const	char	title_n[] = "title";
static	const	char	title_c[] = "Title";
static	const	char	artist_n[] = "artist";
static	const	char	artist_c[] = "Artist";
static	const	char	art_n[] = "art";
static	const	char	art_c[] = "Art";
static	const	char	thumbnail_n[] = "thumbnail";
static	const	char	thumbnail_c[] = "Thumbnail";
static	const	char	track_n[] = "track";
static	const	char	track_c[] = "Track";
static	const	char	track_count_n[] = "trackCount";
static	const	char	track_count_c[] = "TrackCount";
static	const	char	notes_n[] = "notes";
static	const	char	notes_c[] = "Notes";
static	const	char	program_n[] = "program";
static	const	char	program_c[] = "Program";
static	const	char	tracks_n[] = "tracks";
static	const	char	tracks_c[] = "Tracks";
static	const	char	toc_n[] = "toc";
static	const	char	toc_c[] = "TOC";

static	const	char	program_name_n[] = "programName";
static	const	char	program_name_c[] = "ProgramName";
static	const	char	program_count_n[] = "programCount";
static	const	char	program_count_c[] = "ProgramCount";
static	const	char	program_total_min_n[] = "totalMin";
static	const	char	program_total_min_c[] = "TotalMin";
static	const	char	program_total_sec_n[] = "totalSec";
static	const	char	program_total_sec_c[] = "TotalSec";

static	const	char	 dbdir[] = ".cddb";

static		int	 db_path_len;
static		char	*db_path;
static		char	*db_write_dir;

static		char	db_rtrn_buf[MAXPATHLEN];

/***====================================================================***/

#define	DB_ID_NTRACKS	5
#define	DB_ID_MAXLEN	20
#define	DB_MAX_UNKNOWN	-1

struct _CDData	{
	char		*id;
	char		*file;
	char		*TOC;
	XrmDatabase	 db;
	int		 maxProgram;
	int		 dbDirty;
};

#define	DB_HAS_DB	(1<<0)
#define	DB_HAS_IMAGE	(1<<1)
#define	DB_HAS_NOTES	(1<<2)

#define	DB_RDB_EXT	"rdb"
#define	DB_ART_EXT	"art"
#define	DB_NOTES_EXT	"notes"
#define	DB_THUMBNAIL_EXT "thumb"

/***====================================================================***/

static int
db_file_exists( const char *file_name )
{
struct stat64 buf;

    return (file_name) && (stat64(file_name, &buf)==0) && S_ISREG(buf.st_mode);
}

/***====================================================================***/

static int
db_dir_exists( const char *file_name )
{
struct stat64 buf;
    return (file_name) && (stat64(file_name, &buf)==0) && S_ISDIR(buf.st_mode);

}

/***====================================================================***/

int
db_init_pkg( const char *path, const char *write_dir )
{
int	rtrn = 0;

    XrmInitialize( );
    db_set_path( path );
    db_set_write_dir( write_dir );
    if ( !db_dir_exists( db_write_dir ) ) {
	rtrn|= DB_NEED_DIR;
	if ( db_file_exists( odb_get_dflt_file() ) )
	    rtrn|= DB_NEED_CONVERT;
    }
    return rtrn;
}

/***====================================================================***/

void
db_set_path( const char *path )
{
const char *tmp;
char *path_tmp;

    if (path==NULL) {
	tmp = getenv( "CDDB_PATH" );
	if (tmp==NULL)
	    tmp = db_get_dflt_dir();
    }
    else tmp = path;

    if (tmp==NULL)
	return;

    if (db_path)
	free(db_path);
    path_tmp = strdup(tmp);
    db_path_len = 1;
    db_path = strtok( path_tmp, ",:" );
    while ( path_tmp = strtok( NULL, ",:" ) ) {
	db_path_len++;
    };
    return;
}

/***====================================================================***/

void
db_set_write_dir( const char *dir )
{
    if ( db_write_dir )
	free( db_write_dir );
    if (dir) db_write_dir = strdup( dir );
    else {
	const char *tmp = getenv( "CDDB_WRITE_DIR" );
    	db_write_dir= strdup( tmp ? tmp : db_get_dflt_dir() );
    }
    return;
}

/***====================================================================***/

static int
db_init_resources( CDData *data ) 
{
static int been_here = 0;
char	dbfile[MAXPATHLEN];

    sprintf( dbfile, "%s.%s", data->file, DB_RDB_EXT );
    data->db = XrmGetFileDatabase( dbfile );
    if ((data->db)&&(data->TOC)) {
	register const char *tmp = db_get_album_info( data, toc_n, toc_c );
	if ((tmp)&&(strcmp(tmp,data->TOC)!=0)) {
	    fprintf(stderr,"Warning! TOC mismatch for %s\n",data->id);
	    fprintf(stderr,"         %s != %s\n",data->TOC,tmp);
	}
    }
    return data->db!=NULL;
}

/***====================================================================***/

static int
db_save_resources( CDData *data )
{
char	dbfile[MAXPATHLEN];

    if (data->db) {
	if (data->TOC) {
	    register const char *tmp = db_get_album_info( data, toc_n, toc_c );
	    if (!tmp)
		db_put_album_info( data, toc_n, data->TOC );
	}
	if (data->maxProgram!=DB_MAX_UNKNOWN) {
	    register const char *tmp;

	    tmp = db_get_album_info( data, program_count_n, program_count_c );
	    if ((!tmp)||(data->maxProgram>=atoi(tmp))) {
		sprintf(dbfile,"%d",data->maxProgram);
		db_put_album_info( data, program_count_n, dbfile );
	    }
	}
	sprintf(dbfile, "%s.%s", data->file, DB_RDB_EXT);
	XrmPutFileDatabase( data->db, dbfile );
	data->dbDirty = 0;	/* mark db as clean */
    }
    return 1;
}

/***====================================================================***/

static void
db_free_resources( CDData *data )
{
    XrmDestroyDatabase( data->db );
    data->db = NULL;
    return;
}

/***====================================================================***/


#define	DBID_MAP_SIZE	66

static char dbid_int_map[DBID_MAP_SIZE] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
  'U', 'V', 'W', 'X', 'Y', 'Z', '@', '_', '=', '+',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 
  'u', 'v', 'w', 'x', 'y', 'z',
};

#define	DBID_MAP(b,val)	{\
	register int v = (val);\
	if (v>=DBID_MAP_SIZE) {\
	    sprintf(b, "%02d", v);\
	    b+= 2;\
	}\
	else *b++ = dbid_int_map[v];\
}

/***====================================================================***/

const char *
db_get_dflt_dir( void )
{
static char rtrn[MAXPATHLEN];
char	*tmp;
CDData	*data;

    if ((tmp= getenv("HOME"))==NULL)
	return NULL;

    sprintf(rtrn,"%s/%s", tmp, dbdir);
    return rtrn;
}

/***====================================================================***/

const char *
db_TOC_to_id(const char *toc)
{
    return db_convert_old_id(toc);
}

/***====================================================================***/

const char *
db_convert_old_id(const char *old_id)
{
static char buf[DB_ID_MAXLEN];
const	char *old;
char	*new;
int	total,min,sec,i;
int	nIDTracks;

    new= buf;
    i= strlen(old_id);
    old= old_id;
    if (i<2) 
	return NULL;

    total= (*old++ - '0')*10;
    total+= (*old++ - '0');
    if (i != 2+(4*total)) {
	return NULL;
    }
    DBID_MAP(new, ((total>>4)&0xf));
    DBID_MAP(new, (total&0xf));

    if (total<DB_ID_NTRACKS)		nIDTracks = total;
    else if (total==DB_ID_NTRACKS)	nIDTracks = DB_ID_NTRACKS;
    else {
	const char *tmp = old;

	nIDTracks = DB_ID_NTRACKS - 1;
	for (i=0,min=0,sec=0,tmp=old;i<total;i++) {
	    min+= (*old++ - '0')*10;
	    min+= (*old++ - '0');
	    sec+= (*old++ - '0')*10;
	    sec+= (*old++ - '0');
	}
	old = tmp;
	min+= sec/60;
	sec = sec%60;

	DBID_MAP(new,min);
	DBID_MAP(new,sec);
    }


    for (i=0;i<nIDTracks;i++) {
	min = (*old++ - '0')*10;
	min+= (*old++ - '0');
	DBID_MAP(new, min); /* track total_min */
	sec = (*old++ - '0')*10;
	sec+= (*old++ - '0');
	DBID_MAP(new, sec); /* track total_sec */
    }
    *new++ = '\0';
    return buf;
}


/***====================================================================***/

const char *
db_get_id( CDPLAYER *cdplayer, CDSTATUS *status )
{
CDTRACKINFO	info;
int		min, sec, i;
char		*bufp = db_rtrn_buf, *tmp;
int		nCDTracks, nIDTracks;

    if (status->state == CD_NODISC || status->state == CD_ERROR)
	return (NULL);

    nCDTracks = status->last - status->first + 1;
    DBID_MAP( bufp, ((nCDTracks>>4)&0xf) );
    DBID_MAP( bufp, (nCDTracks&0xf) );

    if ( nCDTracks < DB_ID_NTRACKS )		nIDTracks = nCDTracks;
    else if (nCDTracks == DB_ID_NTRACKS )	nIDTracks = DB_ID_NTRACKS;
    else {
	nIDTracks = DB_ID_NTRACKS - 1;
	for (min= 0, sec= 0,i = status->first ; i <= status->last ; i++ ) {
	    CDgettrackinfo( cdplayer, i, &info );
	    min+= info.total_min;
	    sec+= info.total_sec;
	}
	min+= sec/60;
	sec = sec % 60;
	DBID_MAP( bufp, min );
	DBID_MAP( bufp, sec );
    }

    for (i = 0; i < nIDTracks; i++) {
	if (status->first+i <= status->last) {
	    CDgettrackinfo( cdplayer, status->first+i, &info );
	    DBID_MAP( bufp, info.total_min );
	    DBID_MAP( bufp, info.total_sec );
	}
    }
    *bufp++ = '\0';
    return ( db_rtrn_buf );
}

/***====================================================================***/

#define	MAX_CATALOG	(99*3+6+1)

const char *
db_get_TOC( CDPLAYER *cdplayer, CDSTATUS *status )
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

const char *
db_old_id_to_TOC(const char *old_id)
{
    return strdup(old_id);
}


/***====================================================================***/

char *
db_find_file( const char *id )
{
char	buf[MAXNAME];
char	*tmp;
int	i;

    for (i = 0, tmp = db_path ; i < db_path_len ; i++ ) {
	sprintf(buf,"%s/%s.%s", tmp, id, DB_RDB_EXT);
	if ( db_file_exists( buf ) ) {
	    sprintf( buf, "%s/%s", tmp, id);
	    return strdup( buf );
	}
	tmp = &tmp[strlen(tmp)+1];
    }
    sprintf(buf, "%s/%s", (db_write_dir?db_write_dir:db_get_dflt_dir()), id);
    return strdup(buf);
}

/***====================================================================***/

CDData	 *
db_init_from_cd( CDPLAYER *cdplayer, CDSTATUS *status )
{
const char *id = db_get_id( cdplayer, status );
const char *toc = db_get_TOC( cdplayer, status );

    if ( id )
	return db_init( id, toc );
    return NULL;
}

/***====================================================================***/

CDData	 *
db_init( const char *cd_id, const char *TOC )
{
CDData	*data;

    if (cd_id==NULL)
	return NULL;

    data= (CDData *)malloc(sizeof(CDData));
    if (data!=NULL) {
	data->dbDirty = 0;
	data->id=	strdup(cd_id);
	data->file=	db_find_file( cd_id );
	data->TOC=	(TOC?strdup(TOC):NULL);
	data->maxProgram= DB_MAX_UNKNOWN;

	db_init_resources( data );
    }
    return data;
}

/***====================================================================***/

void
db_save( CDData *data )
{
    if ((data) && (data->db) && (data->dbDirty)) {
	db_save_resources( data );
    }
    return;
}

/***====================================================================***/

void
db_close( CDData *data, int save )
{
    if (data) {
	if (data->db) {
	    if (save && data->dbDirty)
		db_save_resources( data );
	    db_free_resources( data );
	}
	if (data->id)	free(data->id);
	if (data->file)	free(data->file);
	free(data);
    }
    return;
}

/***====================================================================***/

const char *
db_get_album_info( CDData *data, const char *name, const char *class )
{
char		str_name[MAXNAME], str_class[MAXCLASS];
char		*rtrn_type;
XrmValue	 rtrn_val;

    if ( (!data) || (!data->db) || (!name) || (!class) )
	return NULL;

    sprintf( str_name, "%s.%s", album_n, name );
    sprintf( str_class, "%s.%s", album_c, class );
    if (XrmGetResource(data->db, str_name, str_class, &rtrn_type, &rtrn_val))
	 return rtrn_val.addr;
    else return NULL;
}

/***====================================================================***/

const char *
db_get_track_info( CDData *data, int num, const char *name, const char *class )
{
char		str_name[MAXNAME], str_class[MAXCLASS];
char		*rtrn_type;
XrmValue	 rtrn_val;

    if ( (!data) || (!data->db) || (!name) || (!class) )
	return NULL;

    sprintf( str_name, "%s%d.%s", track_n, num, name );
    sprintf( str_class, "%s%d.%s", track_c, num, class );
    if (XrmGetResource(data->db, str_name, str_class, &rtrn_type, &rtrn_val)) {
	return rtrn_val.addr;
    }
    else return NULL;
}

/***====================================================================***/

const char *
db_get_program_info(CDData *data, int num, const char *name, const char *class)
{
char		str_name[MAXNAME], str_class[MAXCLASS];
char		*rtrn_type;
XrmValue	 rtrn_val;

    if ( (!data) || (!data->db) || (!name) || (!class) || (num<1))
	return NULL;

    sprintf( str_name, "%s%d.%s", program_n, num, name );
    sprintf( str_class, "%s%d.%s", program_c, num, class );
    if (XrmGetResource(data->db, str_name, str_class, &rtrn_type, &rtrn_val)) {
	if ( num > data->maxProgram )
	    data->maxProgram = num;
	return  rtrn_val.addr;
    }
    else return  NULL;
}

/***====================================================================***/

int
db_get_track_count( CDData *data )
{
    if (data && data->id) {
	char	buf[4];
	int total;
	buf[0] = data->id[0];
	buf[1] = data->id[1];
	buf[2] = '\0';
	if (sscanf( buf, "%x", &total )==1);
	    return total;
    }
    return 0;
}

/***====================================================================***/

const char *
db_get_track_name( CDData *data, int track )
{
    return db_get_track_info( data, track, title_n, title_c );
}

/***====================================================================***/

const char *
db_get_track_artist( CDData *data, int track )
{
const char *artist;

    artist = db_get_track_info( data, track, artist_n, artist_c );
    return artist ? artist : db_get_artist( data );
}

/***====================================================================***/

int
db_get_track_names( CDData *data, const char **track_names, int max_track )
{
int	  i,max = -1;

	for (i = 0; i <= max_track; i++) {
	    track_names[i] = db_get_track_info( data, i, title_n, title_c );
	    if (track_names[i])
		max = i;
	}
	return max;
}

/***====================================================================***/

const char *
db_get_title( CDData *data )
{
    return db_get_album_info( data, title_n, title_c );
}

/***====================================================================***/

const char *
db_get_artist( CDData *data )
{
    return db_get_album_info( data, artist_n, artist_c );
}

/***====================================================================***/

const char *
db_get_album_art( CDData *data, int onlyIfExists )
{
const char *file;

    file= db_get_album_info( data, art_n, art_c );
    if (file==NULL) {
	sprintf(db_rtrn_buf, "%s.%s",data->file,DB_ART_EXT);
	file = db_rtrn_buf;
    }
    if ((!onlyIfExists) || (db_file_exists(file)))
	return file;
    return NULL;
}

/***====================================================================***/

const char *
db_get_album_thumbnail( CDData *data, int onlyIfExists )
{
const char *file;

    file= db_get_album_info( data, thumbnail_n, thumbnail_c );
    if (file==NULL) {
	sprintf(db_rtrn_buf, "%s.%s",data->file,DB_THUMBNAIL_EXT);
	file = db_rtrn_buf;
    }
    if ((!onlyIfExists) || (db_file_exists(file)))
	return file;
    return NULL;
}

/***====================================================================***/

const char *
db_get_notes( CDData *data, int onlyIfExists )
{
const char *file;

    file= db_get_album_info( data, notes_n, notes_c );
    if (file==NULL) {
	sprintf(db_rtrn_buf, "%s.%s",data->file,DB_NOTES_EXT);
	file = db_rtrn_buf;
    }
    if ((!onlyIfExists) || (db_file_exists(file)))
	return file;
    return NULL;
}

/***====================================================================***/

const	char	*
db_get_track_art( CDData *data, int num, int onlyIfExists )
{
const char *file;

    file= db_get_track_info( data, num, art_n, art_c );
    if (file==NULL) {
	sprintf(db_rtrn_buf, "%s-%d.%s",data->file,num,DB_ART_EXT);
	file = db_rtrn_buf;
    }
    if ((!onlyIfExists) || (db_file_exists(file)))
	return file;
    return db_get_album_art( data, onlyIfExists );
}

/***====================================================================***/

const	char	*
db_get_track_thumbnail( CDData *data, int num, int onlyIfExists )
{
const char *file;

    file= db_get_track_info( data, num, thumbnail_n, thumbnail_c );
    if (file==NULL) {
	sprintf(db_rtrn_buf, "%s-%d.%s",data->file,num,DB_THUMBNAIL_EXT);
	file = db_rtrn_buf;
    }
    if ((!onlyIfExists) || (db_file_exists(file)))
	return file;
    return db_get_album_thumbnail( data, onlyIfExists );
}

/***====================================================================***/

const	char	*
db_get_track_notes( CDData *data, int num, int onlyIfExists )
{
const char *file;

    file= db_get_track_info( data, num, notes_n, notes_c );
    if (file==NULL) {
	sprintf(db_rtrn_buf,"%s-%d.%s",data->file,num,DB_NOTES_EXT);
	file = db_rtrn_buf;
    }
    if ((!onlyIfExists) || (db_file_exists(file)))
	return file;
    return db_get_notes( data, onlyIfExists );
}

/***====================================================================***/

int
db_put_album_info( CDData *data, const char *name, const char *value )
{
char		str_name[MAXNAME];

    if ((name) && (value)) {
	data->dbDirty = 1;
	sprintf( str_name, "%s.%s", album_n, name );
	XrmPutStringResource( &data->db, str_name, value );
	return 1;
    }
    return 0;
}

/***====================================================================***/

int
db_put_track_info(CDData *data, int track, const char *name, const char *value)
{
char		str_name[MAXNAME];

    if (name && value && (track>0)) {
	data->dbDirty = 1;
	sprintf( str_name, "%s%d.%s", track_n, track, name );
	XrmPutStringResource( &data->db, str_name, value );
	return 1;
    }
    return 0;
}

/***====================================================================***/

int
db_put_program_info(CDData *data, int num, const char *name, const char *value)
{
char		str_name[MAXNAME];

    if (name && value && (num > 0) ) {
	data->dbDirty = 1;
	sprintf( str_name, "%s%d.%s", program_n, num, name );
	XrmPutStringResource( &data->db, str_name, value );
	if (num>data->maxProgram)
	    data->maxProgram = num;
	return 1;
    }
    return 0;
}

/***====================================================================***/

int
db_put_track_names( CDData *data, const char **track_names, int num_tracks )
{
int		i;

    for (i = 0; i < num_tracks; i++) {
	if (track_names[i])
	    db_put_track_info( data, i, title_n, track_names[i] );
    }

    return 1;
}

/***====================================================================***/

int
db_put_track_name( CDData *data, int track, const char *name )
{
    db_put_track_info( data, track, title_n, name );
    return 1;
}

/***====================================================================***/

int
db_put_track_artist( CDData *data, int track, const char *name )
{
    db_put_track_info( data, track, artist_n, name );
    return 1;
}

/***====================================================================***/

int
db_put_title( CDData *data, const char *title )
{
    return db_put_album_info( data, title_n, title );
}

/***====================================================================***/

int
db_put_artist( CDData *data, const char *artist )
{
    return db_put_album_info( data, artist_n, artist );
}

/***====================================================================***/

int
db_put_album_art( CDData *data, const char *art_file )
{
    return db_put_album_info( data, art_n, art_file );
}

/***====================================================================***/

int
db_put_notes( CDData *data, const char *notes_file )
{
    return db_put_album_info( data, notes_n, notes_file );
}

/***====================================================================***/

#define	MAXPROGLEN	( 100 * sizeof("100") +1 )

int
db_put_program( CDData *data, CDPROGRAM *program, int num )
{
char	str_name[MAXNAME], *id, tracks[ MAXPROGLEN ], buf[30];
int	i;

    db_put_program_info( data, num, program_name_n, program->name );

    tracks[0] = '\0';
    for (i = 0; i < program->num_tracks; i++) {
	sprintf( buf, "%d ", program->tracks[i] );
	strcat( tracks, buf );
    }

    db_put_program_info( data, num, tracks_n, tracks );

    sprintf( buf, "%d", program->num_tracks );
    db_put_program_info( data, num, track_count_n, buf );

    sprintf( buf, "%d", program->total_min );
    db_put_program_info( data, num, program_total_min_n, buf );

    sprintf( buf, "%d", program->total_sec );
    db_put_program_info( data, num, program_total_sec_n, buf );
    return 1;
}

/***====================================================================***/

const char *
db_get_program_name( CDData *data, int num )
{
    return db_get_program_info( data, num, program_name_n, program_name_c );
}


/***====================================================================***/

CDPROGRAM *
db_get_program( CDData *data, int num )
{
CDPROGRAM	*program;
char		*track,ptext[MAXPROGLEN+1];
const char	*tmp;
int		num_tracks;

    program = (CDPROGRAM *)malloc( sizeof (*program) );
    memset( program, 0, sizeof (*program) );

    tmp = db_get_program_info(data,num,program_name_n,program_name_c);
    program->name =  ( tmp ? strdup( tmp ) : NULL );

    tmp = db_get_program_info(data,num,program_total_min_n,program_total_min_c);
    program->total_min = (tmp?atoi(tmp):0);

    tmp = db_get_program_info(data,num,program_total_sec_n,program_total_sec_c);
    program->total_sec = (tmp?atoi(tmp):0);

    tmp = db_get_program_info( data, num, track_count_n, track_count_c );
    program->num_tracks = (tmp?atoi( tmp ):0);

    if (program->num_tracks)
	program->tracks = malloc( sizeof (int) * program->num_tracks );

    tmp = db_get_program_info( data, num, tracks_n, tracks_c );
    if (tmp) {
	sprintf( ptext,"%.*s", MAXPROGLEN, tmp);
	num_tracks = 0;
	for (	track = strtok( ptext, " " );
		num_tracks < program->num_tracks && track;
		track = strtok( NULL, " " )) {
	    program->tracks[num_tracks++] = atoi( track );
	}
    }

    return (program);
}

/***====================================================================***/

int
db_put_program_count( CDData *data, int num )
{
char buf[100];

    if ( num > data->maxProgram )
	data->maxProgram = num;
    sprintf(buf,"%d",num);
    return db_put_album_info( data, program_count_n, buf );
}

/***====================================================================***/

int
db_get_program_count( CDData *data )
{
const char *tmp;

    tmp = db_get_album_info( data, program_count_n, program_count_c );
    if (tmp) {
	register int rval = atoi( tmp );
	if ( rval > (data->maxProgram) )
	    data->maxProgram = rval;
	return rval;
    }
    return 0;
}

/***====================================================================***/

#include <dirent.h>

typedef struct {
	DIR	*dir;
	char	*dir_name;
} CDListState;

CDData	*
db_first_disc( const char *dir_name, void **state )
{
DIR	*dir;
CDListState *lstate;

     dir= opendir( dir_name );
     if (dir) {
	lstate= (CDListState *)malloc(sizeof(CDListState));
	if (lstate) {
	    *state = lstate;
	    lstate->dir= dir;
	    lstate->dir_name= strdup(dir_name);
	    return db_next_disc( *state );
	}
	closedir(dir);
     }
     return NULL;
}

/***====================================================================***/

CDData	*
db_next_disc( void **state )
{
CDListState *lstate = (CDListState *)*state;
CDData	*data = NULL;

    if (lstate) {
	struct dirent64 *entry;
	char	*tmp;
	do {
	    entry = readdir64( lstate->dir );
	    if (entry) {
		tmp = strrchr( entry->d_name, '.' );
		if (tmp) {
		     *tmp++ = '\0';
		}
		else continue;

		data = db_init( entry->d_name, NULL ); 
	    }
	} while ( entry && (data == NULL) );
	if (data == NULL)
	     db_done( state );
	else return data;
    }
    return NULL;
}

/***====================================================================***/

void
db_done( void **state )
{
CDListState *lstate = (CDListState *)*state;

    if (lstate) {
	if (lstate->dir)
	    closedir( lstate-> dir );
	if (lstate->dir_name)
	    free( lstate->dir_name );
	free( lstate );
	*state= NULL;
    }
}
