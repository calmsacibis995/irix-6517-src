#ifndef _INVT_PRIV_H_
#define _INVT_PRIV_H_

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/inventory_priv.h,v 1.1 1994/09/13 19:28:57 cbullis Exp $"

#include "inventory.h"


/* given a file system find that file that tells us where the sessions
   for the period of time in question are kept. */
#define SESSLOCK_FILE		INV_DIRPATH"/sesslock"

#define INVT_STOBJ_MAXSESSIONS	5
#define INVT_MAX_INVINDICES	-1
#define FSTAB_UPDATED		1
#define NEW_INVINDEX		2

#define INVT_ENDTIME		BOOL_FALSE
#define INVT_STARTTIME		BOOL_TRUE
#define INVT_DOLOCK		BOOL_TRUE
#define INVT_DONTLOCK		BOOL_FALSE

#define INVLOCK( fd, m )	flock( fd, m ) 
#define ASSERT	                assert
#define INV_PERMS               S_IRWXU | S_IRWXG | S_IRWXO

#define IS_WITHIN(ttpe, tm)	( tm >= ttpe->tp_start && tm <= ttpe->tp_end )
#define INVINDEX_HDR_OFFSET(n) 	(off64_t) ( sizeof( invt_entry_t ) * (size_t) (n)\
					   + sizeof( invt_counter_t ) )



#define GET_COUNTERS( fd, cnt ) get_counters( fd, (void **)(cnt), \
					      sizeof(invt_counter_t) )
#define GET_SESCOUNTERS( fd, cnt ) get_counters( fd, (void **)(cnt), \
						sizeof(invt_sescounter_t) )

#define PUT_COUNTERS( fd, cnt ) put_counters( fd, (void *)(cnt), \
					     sizeof( invt_counter_t ) )

#define PUT_SESCOUNTERS( fd, cnt ) put_counters( fd, (void *)(cnt), \
					     sizeof( invt_sescounter_t ) )

#define SC_EOF_INITIAL_POS	(off64_t) (sizeof( invt_sescounter_t ) + \
					 INVT_STOBJ_MAXSESSIONS * \
					 ( sizeof( invt_seshdr_t ) + \
					   sizeof( invt_session_t ) ) )


typedef struct invt_session {
	uuid_t 		 s_sesid;	/* this dump session's id: 16 bytes*/
	uuid_t		 s_fsid;	/* file system id */
	char		 s_label[INV_STRLEN];  /* session label, assigned by the
						  operator */
	char		 s_mountpt[INV_STRLEN];/* path to the mount point */
	char		 s_devpath[INV_STRLEN];/* path to the device */
	u_int		 s_cur_nstreams;/* number of streams created under this
					   session so far; protected by lock */
	u_int		 s_max_nstreams;/* number of media streams in the session */

} invt_session_t;
 
typedef struct invt_seshdr {
	time_t		sh_time;   /* time of the dump */
	u_char		sh_level;  /* dump level */
	off64_t		sh_sess_off; /* offset to the rest of the session info */
	off64_t		sh_streams_off; /* offset to start of the set of stream
					   hdrs */
} invt_seshdr_t;




/* Each session consists of a number of media streams. While it is given that
   there won't be multiple writesessions (ie. dumpsessions) concurrently,
   there can be multiple media streams operating on a single file system, 
   each writing media files within its own stream. Hence, we have a linked
   list of media files, that the stream keeps track of. */


typedef struct invt_breakpt {
	ino64_t		ino;		/* the 64bit inumber */
	off64_t		offset;		/* offset into the file */
} invt_breakpt_t;

typedef struct invt_stream {
	bool_t		st_interrupted;	/* was this stream interrupted ? */
	
	/* duplicate info from mediafiles for speed */
	invt_breakpt_t	st_startino;	/* the starting pt */
	invt_breakpt_t	st_endino;	/* where we actually ended up. this means
					   we've written upto but not including
					   this breakpoint. */
	int		st_nmediafiles; /* number of mediafiles */
	off64_t		st_firstmfile;	/* offsets to the start and end of the ..*/
	off64_t		st_lastmfile;	/* .. linked list of mediafiles */
} invt_stream_t;



typedef struct invt_mediafile {
	uuid_t		 mf_moid;	/* media object id */
	char		 mf_label[INV_STRLEN];	/* media file label */
	invt_breakpt_t	 mf_startino;	/* file that we started out with */
	invt_breakpt_t	 mf_endino;	/* the dump file we ended this 
					   media file with */
	off64_t		 mf_nextmf;	/* links to other mfiles */
	off64_t		 mf_prevmf;
} invt_mediafile_t;


/* XXX tmp */
typedef invt_mediafile_t invt_mediafileinfo_t;


typedef struct invt_desc_entry {
	int		d_invindex_fd;	/* open file descriptor of inv index */
	int		d_stobj_fd;	/* fd of storage object */
	u_char		d_update_flag;  /* indicates whether fstab was updated with
					   this file system or not and also if
					   we had to create a new invindex file */
	off64_t		d_invindex_off; /* for every session, we need a reference 
					   to its invindex entry, so that when we
					   close a session, we know which one */
	
} invt_desc_entry_t;


typedef struct invt_sesdesc_entry {
	invt_desc_entry_t      *sd_invtok;	/* generic inventory token */
	off64_t			sd_session_off;
	off64_t			sd_sesshdr_off;
	time_t			sd_sesstime;	/* time that session started. 
						   needed for closing the session */
} invt_sesdesc_entry_t;
	
struct invt_mediafile;

typedef struct invt_strdesc_entry {
	invt_sesdesc_entry_t  *md_sesstok;   /* the session token */
	off64_t		       md_stream_off;/* offset to the media stream 
						header */	
	struct invt_mediafile *md_lastmfile; /* just so that we dont have
						to get it back from disk
						when we add the next mfile
						to the linked list */

} invt_strdesc_entry_t;
	

typedef struct invt_timeperiod {
	time_t	tp_start;
	time_t	tp_end;
} invt_timeperiod_t;

typedef struct invt_entry {
	invt_timeperiod_t 	ie_timeperiod;
	char			ie_filename[INV_STRLEN];
} invt_entry_t;

typedef struct invt_counter {
	int	ic_curnum;	/* number of sessions/inv_indices recorded so far
				   = -1 if there're aren't any */
	int	ic_maxnum;	/* maximum number of sessions/inv_indices that 
				   we can record on this storage object */
} invt_counter_t;

typedef struct invt_sess_metahdr {
	u_int	ic_curnum;	/* number of sessions/inv_indices recorded so far */
	u_int	ic_maxnum;	/* maximum number of sessions/inv_indices that 
				   we can record on this storage object */
	off64_t	ic_eof; /* current end of the file, where the next
				   media file or stream will be written to */
} invt_sescounter_t;


typedef struct invt_fstab {
	uuid_t	ft_uuid;
	char	ft_mountpt[INV_STRLEN];
	char	ft_devpath[INV_STRLEN];
} invt_fstab_t;

typedef bool_t (*search_callback_t) (int, invt_seshdr_t *, u_char, void *);


#define GET_REC( fd, buf, sz, off )  \
                        get_invtrecord( fd, buf, sz, off, SEEK_SET, INVT_DOLOCK )

#define GET_REC_NOLOCK( fd, buf, sz, off )  \
                        get_invtrecord( fd, buf, sz, off, SEEK_SET, INVT_DONTLOCK )

#define GET_REC_SEEKCUR( fd, buf, sz, off )  \
                        get_invtrecord( fd, buf, sz, off, SEEK_CUR, INVT_DOLOCK )

#define GET_ALLHDRS_N_CNTS( fd, h, c, hsz, csz ) \
                        get_headerinfo( fd, h, c, hsz, csz, INVT_DOLOCK )

#define GET_ALLHDRS_N_CNTS_NOLOCK( fd, h, c, hsz, csz ) \
                        get_headerinfo( fd, h, c, hsz, csz, INVT_DONTLOCK )

#define PUT_REC( fd, buf, sz, off )  \
                        put_invtrecord( fd, buf, sz, off, SEEK_SET, INVT_DOLOCK )

#define PUT_REC_NOLOCK( fd, buf, sz, off )  \
                        put_invtrecord( fd, buf, sz, off, SEEK_SET, INVT_DONTLOCK )

#define PUT_REC_SEEKCUR( fd, buf, sz, off )  \
                        put_invtrecord( fd, buf, sz, off, SEEK_CUR, INVT_DOLOCK )

#define PUT_REC_NOLOCK_SEEKCUR( fd, buf, sz, off )  \
                        put_invtrecord( fd, buf, sz, off, SEEK_CUR, INVT_DONTLOCK )



/*---------------------------------------------------------------------------*/
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*---------------------------------------------------------------------------*/
inv_idbtoken_t
create_invindex( int fd, char *fname, char *mntpt );

intgen_t
get_invtentry( char *fname, time_t tm, invt_entry_t *buf, size_t bufsz );

intgen_t
get_invtrecord( int fd, void *buf, size_t bufsz, off64_t off, int, bool_t dolock );

intgen_t
put_invtrecord( int fd, void *buf, size_t bufsz, off64_t off, int, bool_t dolock );

intgen_t
get_fname(  void *pred, char *fname, inv_predicate_t bywhat );

inv_idbtoken_t
get_token( int fd, int objfd );

void
destroy_token( inv_idbtoken_t tok );

int
get_storageobj( int invfd, int *index );

int
create_storageobj( char *mntpt, char *fname, int index);

intgen_t
create_invindex_entry( inv_idbtoken_t *tok, int invfd, char *mntpt, 
		       bool_t firstentry );

intgen_t
create_session( inv_sestoken_t tok, int fd, invt_sescounter_t *sescnt, 
	        invt_session_t *ses, invt_seshdr_t *hdr );

int
get_curnum( int fd );

intgen_t
get_headers( int fd, void **hdrs, size_t bufsz, size_t cntsz );

intgen_t
get_counters( int fd, void **cntpp, size_t sz );

intgen_t
get_sescounters( int fd, invt_sescounter_t **cntpp );

intgen_t
get_lastheader( int fd, void **ent, size_t hdrsz,  size_t cntsz );

intgen_t
put_fstab_entry( uuid_t *fsidp, char *mntpt, char *dev );

intgen_t
get_fstab( invt_fstab_t **arr, invt_counter_t **cnt, int *numfs );

intgen_t
put_mediafile( inv_stmtoken_t tok, invt_mediafile_t *mf );

intgen_t
put_sesstime( inv_sestoken_t tok, bool_t whichtime);

intgen_t
open_invindex( void *pred, inv_predicate_t bywhat );

int
MY_FLOCK( int f, int k );

inv_sestoken_t
get_sesstoken( inv_idbtoken_t tok );

intgen_t
put_counters( int fd, void *cntp, size_t sz );

intgen_t
get_headerinfo( int fd, void **hdrs, void **cnt,
	        size_t hdrsz, size_t cntsz, bool_t doblock );

void
sess_lock( void );

void
sess_unlock( void );


intgen_t
search_invt( 
	inv_idbtoken_t tok, 
	u_char level, 
	void **buf,
	search_callback_t do_chkcriteria );

intgen_t
tm_level_lessthan( int fd, invt_seshdr_t *hdr, u_char level,
		   void **tm );

intgen_t
lastsess_level_lessthan( int fd, invt_seshdr_t *hdr, u_char level,
			 void **buf );

intgen_t
lastsess_level_equalto( int fd, invt_seshdr_t *hdr, u_char level, void **buf );

intgen_t
make_invsess( int fd, inv_session_t **buf, invt_seshdr_t *hdr );

void
DEBUG_sessionprint( inv_session_t *ses );

intgen_t
DEBUG_displayallsessions( int fd, invt_seshdr_t *hdr, u_char level, void **buf );

intgen_t
make_invdirectory( void );

bool_t
init_idb( void *pred, inv_predicate_t bywhat, char *uuname,inv_idbtoken_t *tok );

void
create_inolist_item( inv_inolist_t *cur, ino64_t ino );


#endif
