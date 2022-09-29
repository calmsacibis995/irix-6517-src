#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/inventory/RCS/inv_stobj.c,v 1.29 1997/08/20 23:43:51 prasadb Exp $"

#include <sys/types.h>
#include <sys/file.h>
#include <sys/uuid.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/dir.h>
#include <sys/mman.h>

#include "types.h"
#include "mlog.h"
#include "inv_priv.h"



/*----------------------------------------------------------------------*/
/* stobj_insert_session                                                 */
/*                                                                      */
/* Used in reconstruction of the inventory. We add a session to this    */
/* storage object whether or not it has reached its maximum.            */
/*----------------------------------------------------------------------*/
intgen_t
stobj_insert_session( invt_idxinfo_t *idx,
		      int fd, /* kept locked EX by caller */
		      invt_sessinfo_t *s )
{
	invt_sescounter_t *sescnt = NULL;
	
	if ( GET_SESCOUNTERS( fd, &sescnt ) < 0 ) {
		INVLOCK( fd, LOCK_UN );
		return -1;
	}

	/* Check the existing sessions to make sure that we're not
	   duplicating this session */
	if ( sescnt->ic_curnum > 0 ) {
		u_int i;
		uint_t stat;
		invt_session_t	*sessions = calloc( sescnt->ic_curnum, 
				   sizeof( invt_session_t ) );
		if ( GET_REC_NOLOCK( fd, sessions, sescnt->ic_curnum *
				     sizeof( invt_session_t ), 
				     (off64_t) ( sizeof( *sescnt ) + 
         		sescnt->ic_maxnum * sizeof( invt_seshdr_t ))) < 0 ) {
			free ( sescnt );
			free ( sessions );
			return -1;
		}

		for( i = 0; i <  sescnt->ic_curnum; i++ ) {
			if ( uuid_compare( &sessions[i].s_sesid, 
					   &s->ses->s_sesid, 
					   &stat ) == 0 )
				break;

		}
		free ( sessions );
		if ( i < sescnt->ic_curnum ) {
			mlog( MLOG_DEBUG | MLOG_INV, 
			      "INV: attempt to insert an "
			      "existing session.\n"
			     );
			free ( sescnt );
			return 1;
		}

	}

	if ( sescnt->ic_curnum >= sescnt->ic_maxnum ) {
		if ( stobj_split( idx, fd, sescnt, s ) < 0 ) {
			free( sescnt );
			return -1;
		}
		free( sescnt );
		return 1;
		
	}
			
	if ( stobj_put_session( fd, sescnt, s->ses, s->seshdr, s->strms,
			        s->mfiles ) < 0 ){
		free ( sescnt);
		return -1;
	}
	
	free ( sescnt);
	return 1;
}


/*----------------------------------------------------------------------*/
/*  stobj_find_splitpoint						*/
/*                                                                      */
/* Load in all the session headers, and go thru them, one by one, 	*/
/* beginning with last seshdr. We return once we find one with a 	*/
/* different sh_time from the previous one. 				*/
/*----------------------------------------------------------------------*/


/* ARGSUSED */
u_int
stobj_find_splitpoint( int fd, invt_seshdr_t *harr, u_int ns, time_t tm )
{
	u_int i;

	if ( harr[ns-1].sh_time < tm )
		return ns;
	/* since ns > 1, our split point > 0  */
	for ( i = ns - 1; i > 0; i-- ) {
		/* these are ordered in ascending order */
		if ( harr[i].sh_time > harr[i-1].sh_time )
			return i;
	}


	/* All the entries have the same sh_time. It's not clear if that's 
	   really possible, but either way, we split at the last entry.
	   This will impact the guarantee that invindex tries to give - that
	   there's always a unique stobj for every session. */
	mlog( MLOG_VERBOSE | MLOG_INV,
	      "INV: failed to find a different sh_time to split\n"
	     );
	      
	return ns - 1;
	
	

}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
stobj_split( invt_idxinfo_t *idx, int fd, invt_sescounter_t *sescnt, 
	     invt_sessinfo_t *newsess )
{
	invt_seshdr_t 	*harr = NULL;
	u_int          	i, ix, ns = sescnt->ic_curnum;
	void        	*bufpp;
	size_t        	bufszp;
	invt_sessinfo_t sesinfo;
	invt_entry_t 	ient;

	if ( GET_SESHEADERS( fd, &harr, ns ) < 0 )
		return -1;
	
	ASSERT( harr != NULL );

	if ( ( ix = stobj_find_splitpoint( fd, harr, ns, 
				       newsess->seshdr->sh_time ) ) == 0 )
		return -1;
	
	if ( ix == ns ) {
		ient.ie_timeperiod.tp_start = ient.ie_timeperiod.tp_end = 
			 newsess->seshdr->sh_time;
	} else {
		ient.ie_timeperiod.tp_start = ient.ie_timeperiod.tp_end = 
		harr[ix].sh_time;
		if ( harr[ix - 1].sh_time >  newsess->seshdr->sh_time )
			idx->iarr[idx->index].ie_timeperiod.tp_end 
				= harr[ix - 1].sh_time;
		else
			idx->iarr[idx->index].ie_timeperiod.tp_end 
				= newsess->seshdr->sh_time;
	}

	/* Get the new stobj to put the 'spilling' sessinfo in. We know the
	   idx of the current stobj, and by definition, the new stobj
	   should come right afterwards. */
	newsess->stobjfd = idx_put_newentry( idx, &ient );
	if ( newsess->stobjfd < 0 )
		return -1;
	
	if ( ix == ns ) {
		invt_sescounter_t *scnt = NULL;
		off64_t rval;

		/* We dont need to split. So, just put the new session in
		   the new stobj, and rest in peace */
		idx->index++;
		
		if ( GET_SESCOUNTERS( newsess->stobjfd, &scnt ) < 0 )
			return -1;
		
		rval = stobj_put_session( newsess->stobjfd, scnt, 
				        newsess->ses,
				        newsess->seshdr, newsess->strms, 
				        newsess->mfiles );
		free( scnt );
		return (rval < 0)? -1: 1;
	}
		
	

	for ( i = ix; i < ns; i++ ) {
		invt_session_t session;
		bufpp = NULL;
		bufszp = 0;

		newsess->seshdr->sh_sess_off = harr[i].sh_sess_off;
		
		if ( GET_REC_NOLOCK( fd, &session, sizeof( invt_session_t ),
			     harr[i].sh_sess_off ) < 0 )
			return -1;
		if (! stobj_pack_sessinfo( fd, &session, &harr[i], &bufpp,
					   &bufszp ))
			return -1;
		/* Now we need to put this in the new StObj. So, first
		   unpack it. */
		if (! stobj_unpack_sessinfo( bufpp, bufszp, &sesinfo ) )
			return -1;

		/* There is no chance of a recursion here */
		if ( stobj_insert_session( idx, newsess->stobjfd, &sesinfo )
		     < 0 ) 
			return -1;
		
		/* Now delete that session from this StObj */
		if (! stobj_delete_sessinfo( fd, sescnt, &session, 
					     &harr[i] ) )
			return -1;
		free( bufpp );
	}
	/* Put the new session in the stobj that we just split. Make
	   sure that we use the updated sescnt and not the stale stuff
	   from disk. stobj_put_session() writes sescnt and sessinfo to
	   disk */ 
	if ( stobj_put_session( fd, sescnt, newsess->ses, newsess->seshdr, 
			        newsess->strms, newsess->mfiles ) < 0 ) {
		free ( sescnt);
		return -1;
	}	

	
	return 1;
}




/* ARGSUSED */
intgen_t
stobj_delete_mfile( int fd, inv_stream_t *strm, invt_mediafile_t *mf,
		    off64_t  mfileoff )
{

	return 1;

}




/* ARGSUSED */
bool_t
stobj_delete_sessinfo( int fd, /* kept locked EX by caller */
		       invt_sescounter_t *sescnt,
		       invt_session_t *ses, invt_seshdr_t *hdr )
{
	/* we change the sescnt here, but dont write it back to disk 
	   until later */
	sescnt->ic_curnum--;
	
	/* there is really no need to take out / zero out the deleted
	   session. The seshdr and session will be over-written, but
	   space taken by the streams and mediafiles will be wasted. */
	return BOOL_TRUE;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/


off64_t
stobj_put_session( 
	int fd, 
	invt_sescounter_t *sescnt, 
	invt_session_t *ses, 
	invt_seshdr_t *hdr,
	invt_stream_t *strms,
	invt_mediafile_t *mfiles )
{
	off64_t hoff;
	
	/* figure out the place where the header will go */
	hoff =  STOBJ_OFFSET( sescnt->ic_curnum, 0 );

	/* figure out where the session information is going to go. */
	if ( hdr->sh_sess_off < 0 )
		hdr->sh_sess_off = STOBJ_OFFSET( sescnt->ic_maxnum, 
						 sescnt->ic_curnum );	
	sescnt->ic_curnum++;

#ifdef INVT_DEBUG
	mlog ( MLOG_VERBOSE,
	       "INV: create sess #%d @ offset %d ic_eof = %d\n",
	       sescnt->ic_curnum, (int) hdr->sh_sess_off,
	       (int) sescnt->ic_eof );
#endif

	/* we need to know where the streams begin, and where the 
	   media files will begin, at the end of the streams */
	hdr->sh_streams_off = sescnt->ic_eof;

	if ( strms == NULL ) {
		sescnt->ic_eof += (off64_t)( ses->s_max_nstreams * 
					     sizeof( invt_stream_t ) );
	} else {
		u_int i;
		size_t nmf = 0;
		sescnt->ic_eof += (off64_t)( ses->s_cur_nstreams * 
					     sizeof( invt_stream_t ) );
		for (i=0; i<ses->s_cur_nstreams; i++) 
			nmf += ( size_t ) strms[i].st_nmediafiles;
		
		sescnt->ic_eof += (off64_t)( sizeof( invt_mediafile_t ) 
					     * nmf );
		if ( stobj_put_streams( fd, hdr, ses, strms, mfiles ) < 0 )
			return -1; 

	}
				  
	/* we write back the counters of this storage object first */
	if ( PUT_SESCOUNTERS( fd, sescnt ) < 0 )
		return -1;
	
	/* write the header next; lseek to the right position */
	if ( PUT_REC_NOLOCK( fd, hdr, sizeof( invt_seshdr_t ), hoff ) < 0 ) {
		return -1;
	}

	/* write the header information to the storage object */
	if ( PUT_REC_NOLOCK( fd, ses, sizeof( invt_session_t ), 
			     hdr->sh_sess_off ) < 0 ) {
		return -1;
	}
	
	if ( strms != NULL )
		stobj_sortheaders( fd, sescnt->ic_curnum );

	return hoff;
}




/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
stobj_sortheaders( int fd, u_int num )
{
	size_t sz = sizeof( invt_seshdr_t ) * num;
	invt_seshdr_t *hdrs;
#ifdef INVT_DEBUG
	int i;
#endif
	if ( num < 2 ) return 1;

	hdrs = malloc( sz );
	ASSERT( hdrs );

	if ( GET_REC_NOLOCK( fd, hdrs, sz, STOBJ_OFFSET( 0, 0 ) ) < 0 ) {
		free ( hdrs );
		return -1;
	}
#ifdef INVT_DEBUG
	printf("\nBEF\n" );
	for (i=0; i<(int)num; i++)
		printf("%ld\n", (long) hdrs[i].sh_time );
#endif	
	qsort( (void*) hdrs, (size_t) num, 
	      sizeof( invt_seshdr_t ), stobj_hdrcmp );

#ifdef INVT_DEBUG
	printf("\n\nAFT\n" );
	for (i=0; i<(int)num; i++)
		printf("%ld\n", (long) hdrs[i].sh_time );
#endif
	if ( PUT_REC_NOLOCK( fd, hdrs, sz, STOBJ_OFFSET( 0, 0 ) ) < 0 ) {
		free ( hdrs );
		return -1;
	}
	
	free ( hdrs );
	return 1;

}



/*----------------------------------------------------------------------*/
/* stobj_put_streams                                                    */
/*                                                                      */
/* used only in reconstruction.                                         */
/* puts the array of streams and the array of mediafiles in the stobj,  */
/* after adjusting their offsets.                                       */
/*----------------------------------------------------------------------*/

intgen_t
stobj_put_streams( int fd, invt_seshdr_t *hdr, invt_session_t *ses, 
		   invt_stream_t *strms,
		   invt_mediafile_t *mfiles )
{
	u_int	nstm = ses->s_cur_nstreams;
	off64_t off  = hdr->sh_streams_off;
	off64_t mfileoff = off + (off64_t)( nstm * sizeof( invt_stream_t ) );
	u_int nmfiles = 0;
	u_int i,j;

	/* fix the offsets in streams */
	for ( i = 0; i < nstm; i++ ) {
		strms[i].st_firstmfile = mfileoff +
		  (off64_t) ((size_t) nmfiles * sizeof( invt_mediafile_t ) );
		/* now fix the offsets in mediafiles */
		for ( j = 0; j < strms[i].st_nmediafiles; j++ ) {
			
			/* no need to fix the last element's next ptr */
			if ( j < strms[i].st_nmediafiles - 1 )
				mfiles[ nmfiles + j ].mf_nextmf = 
				        strms[i].st_firstmfile + 
			      (off64_t) ((j+1) * sizeof( invt_mediafile_t ));

			/* no need to fix the first element's prev ptr */
			if ( j )
				mfiles[ nmfiles + j ].mf_prevmf = 
				        strms[i].st_firstmfile + 
			    (off64_t) ((j-1) * sizeof( invt_mediafile_t ));
		}

		/* adjust the offsets of the first and the last elements 
		   in the mediafile chain */
		mfiles[ nmfiles ].mf_prevmf = 0;
		nmfiles += strms[i].st_nmediafiles;
		mfiles[ nmfiles - 1 ].mf_nextmf = 0;
		
	}

	/* first put the streams. hdr already points at the right place */
	if ( PUT_REC_NOLOCK( fd, strms, nstm * sizeof( invt_stream_t ), 
			     off ) < 0 )
		return -1;
	
	if ( PUT_REC_NOLOCK( fd, mfiles, nmfiles * sizeof( invt_mediafile_t ), 
			     mfileoff ) < 0 )
		return -1;

	return 1;
	
}




/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

void
stobj_makefname( char *fname )
{	
	/* come up with a new unique name */
	uuid_t	fn;
	uint_t stat;
	char *str;

	uuid_create( &fn, &stat );
	ASSERT( stat == uuid_s_ok ); 
	uuid_to_string( &fn, &str, &stat );
	ASSERT( stat == uuid_s_ok ); 
	
	strcpy( fname, INV_DIRPATH );
	strcat( fname, "/" );
	strcat( fname, str );
	strcat( fname, INV_STOBJ_PREFIX );

	ASSERT( (int) strlen( fname ) < INV_STRLEN );
	free ( str );
}


 




/* NOTE: this doesnt update counters or headers in the inv_index */
int
stobj_create( char *fname )
{
	int fd;	
	invt_sescounter_t sescnt;
	inv_oflag_t forwhat = INV_SEARCH_N_MOD;

#ifdef INVT_DEBUG
	mlog( MLOG_VERBOSE | MLOG_INV, "INV: creating storage obj %s\n", fname);
#endif	

	/* create the new storage object */
	if (( fd = open( fname, INV_OFLAG(forwhat) | O_EXCL | O_CREAT )) < 0 ) {
		INV_PERROR ( fname );
		memset( fname, 0, INV_STRLEN );
		return -1;
	}
	
	INVLOCK( fd, LOCK_EX );
	fchmod( fd, INV_PERMS );
	
	sescnt.ic_vernum = INV_VERSION;
	sescnt.ic_curnum = 0; /* there are no sessions as yet */
	sescnt.ic_maxnum = INVT_STOBJ_MAXSESSIONS;
	sescnt.ic_eof = SC_EOF_INITIAL_POS;

	if ( PUT_SESCOUNTERS ( fd, &sescnt ) < 0 ) {
		memset( fname, 0, INV_STRLEN );
		INVLOCK( fd, LOCK_UN );
		close( fd );
		return -1;
	}
	
	INVLOCK( fd, LOCK_UN );
	return fd;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/


intgen_t
stobj_create_session( 
	inv_sestoken_t tok, 
	int fd, /* kept locked EX by caller */
	invt_sescounter_t *sescnt, 
	invt_session_t *ses, 
	invt_seshdr_t *hdr )
{
	off64_t hoff;
	
	ASSERT( tok && sescnt && ses && hdr );

	hdr->sh_sess_off = -1;
	ses->s_cur_nstreams = 0;
	
	if ((hoff = stobj_put_session( fd, sescnt, ses, hdr, NULL, NULL )) 
	    < 0) {
		return -1;
	}

	tok->sd_sesshdr_off = hoff;
	tok->sd_session_off = hdr->sh_sess_off;

	return 1;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/* The stobj_fd in the stream token is kept locked EX by caller.        */
/*----------------------------------------------------------------------*/

intgen_t
stobj_put_mediafile( inv_stmtoken_t tok, invt_mediafile_t *mf )
{
	int  rval;
	invt_sescounter_t *sescnt = NULL;
	invt_stream_t stream;
	inv_sestoken_t sestok = tok->md_sesstok;
	int fd =  sestok->sd_invtok->d_stobj_fd;
	off64_t pos;

	/* first we need to find out where the current write-position is.
	   so, we first read the sescounter that is at the top of this
	   storage object */
	if ( GET_SESCOUNTERS( fd, &sescnt ) < 0 )
		return -1;

	pos = sescnt->ic_eof;

	/* increment the pointer to give space for this media file */
	sescnt->ic_eof += (off64_t) sizeof( invt_mediafile_t );

	if ( PUT_SESCOUNTERS( fd, sescnt ) < 0 )
		return -1;

	/* get the stream information, and update number of mediafiles.
	   we also need to link the new mediafile into the linked-list of
	   media files of this stream */

	if ( GET_REC_NOLOCK( fd, &stream, sizeof( stream ), 
			     tok->md_stream_off ) < 0 )
		return -1;

	/* We need to update the last ino of this STREAM, which is now the
	   last ino of the new mediafile. If this is the first mediafile, we
	   have to update the startino as well. Note that ino is a <ino,off>
	   tuple */
	if ( ! ( mf->mf_flag & INVT_MFILE_INVDUMP )) {
		if ( stream.st_nmediafiles == 0 )
			stream.st_startino = mf->mf_startino;
		stream.st_endino = mf->mf_endino;
	}

	stream.st_nmediafiles++;
#ifdef INVT_DEBUG
	mlog (MLOG_VERBOSE, "#################### mediafile #%d "
	      "###################\n", stream.st_nmediafiles);
#endif	
	/* add the new mediafile at the tail of the list */
	
	mf->mf_nextmf = tok->md_stream_off; 
	mf->mf_prevmf = stream.st_lastmfile;

	
	if ( tok->md_lastmfile )
		tok->md_lastmfile->mf_nextmf = pos;	
	else {
		stream.st_firstmfile = pos;
	}

	stream.st_lastmfile = pos;

	
	/* write the stream to disk */
	if ( PUT_REC_NOLOCK( fd, &stream, sizeof( stream ), 
			     tok->md_stream_off ) < 0 )
		return -1;
	
	/* write the prev media file to disk too */
	if ( tok->md_lastmfile ) {
		rval = PUT_REC_NOLOCK( fd, tok->md_lastmfile, 
				       sizeof( invt_mediafile_t ), 
				       mf->mf_prevmf );
		free (  tok->md_lastmfile );
		if ( rval < 0 ) 
			return -1;
	}

	if ( ! ( mf->mf_flag & INVT_MFILE_INVDUMP )) {
		tok->md_lastmfile = mf;
	} else {
		tok->md_lastmfile = NULL;
	}

	/* at last, write the new media file to disk */
	rval = PUT_REC_NOLOCK( fd, mf, sizeof( invt_mediafile_t ), pos );
	if ( rval < 0 ) {
		return -1;
	}

	return rval;
}








/*----------------------------------------------------------------------*/
/* get_sessinfo                                                         */
/*                                                                      */
/* get both the seshdr and session infomation.                          */
/* caller takes the responsibility of locking.                          */
/*----------------------------------------------------------------------*/

intgen_t
stobj_get_sessinfo ( inv_sestoken_t tok, invt_seshdr_t *hdr, 
		     invt_session_t *ses )
{
	int rval;
	int fd = tok->sd_invtok->d_stobj_fd; 

	/* get the session header first */
	if ( ( rval = GET_REC_NOLOCK( fd, hdr, sizeof( invt_seshdr_t ),
			     tok->sd_sesshdr_off ) ) > 0 ) {
		rval = GET_REC_NOLOCK( fd, ses, sizeof( invt_session_t ),
			     tok->sd_session_off );
	}
	
	return rval;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

bool_t
stobj_pack_sessinfo( int fd, invt_session_t *ses, invt_seshdr_t *hdr,
		     void  **bufpp, size_t *bufszp )
{
	size_t	      	stmsz;
	u_int		i, j;
	size_t		sessz;
	invt_stream_t  *strms;
	char	       *sesbuf, *sesbufcp;
	off64_t		off;
	invt_mediafile_t *mf;

	stmsz = sizeof( invt_stream_t ) * ses->s_cur_nstreams;

	/* the initial size without the mediafiles */
	sessz = strlen( INVTSESS_COOKIE ) * sizeof( char ) +
		sizeof( inv_version_t ) +
		sizeof( inv_version_t ) +  /* added to fix 64 bit alignment prob */
		sizeof( invt_session_t) + sizeof( invt_seshdr_t ) + stmsz;

	/* now get all the streams of this session */
	strms = calloc ( ses->s_cur_nstreams, sizeof( invt_stream_t ) );
	if ( GET_REC_NOLOCK( fd, strms, stmsz, hdr->sh_streams_off ) < 0 ) {
		free ( strms );
		return BOOL_FALSE;
	}
	
	for ( i = 0; i < ses->s_cur_nstreams; i++ )
		sessz += sizeof( invt_mediafile_t ) * 
			 (size_t) strms[i].st_nmediafiles;

	/* Now we know how big this entire thing is going to be */
	sesbufcp = sesbuf = calloc( 1, sessz );
	ASSERT( sesbuf );

	/* Copy everything. Note that we don't bother to adjust the offsets
	   either in the seshdr or in the mediafiles, because we don't need
	   those in order to restore this session ( since everything's 
	   contiguous ) */

	/* magic cookie that we put for sanity checking in case of an 
	   earthquake or something :) */
	strcpy( sesbuf, INVTSESS_COOKIE ); 
	sesbuf += (size_t)( strlen( INVTSESS_COOKIE ) * sizeof( char ) );
	
	/* This was originally INV_VERSION. Changed it to mean packed inventory 
	 * version number and added another inv_version_t to contain the INV_VERSION.
	 * The primary intent of this change was to make everything 64 bit aligned,
	 * but we also got the advantage of separating the packed inv version from
	 * the general inventory version
	 */
	*(inv_version_t *)sesbuf = PACKED_INV_VERSION;
	sesbuf += sizeof( inv_version_t );

	/* This has the INV_VERSION */
	*(inv_version_t *)sesbuf = INV_VERSION;
	sesbuf += sizeof( inv_version_t );

	memcpy( sesbuf, hdr, sizeof( invt_seshdr_t ) );
	sesbuf += sizeof( invt_seshdr_t );

	memcpy( sesbuf, ses, sizeof( invt_session_t ) );
	sesbuf += sizeof( invt_session_t );

	memcpy( sesbuf, strms, stmsz );	  /* all the streams */
	sesbuf += stmsz;

	mf = ( invt_mediafile_t *) sesbuf;


	/* now append all the mediafiles */
	for ( i = 0; i < ses->s_cur_nstreams; i++ ) {
		off = strms[i].st_firstmfile;
		for ( j = 0; j < strms[i].st_nmediafiles; 
		     j++, 
		     off = mf->mf_nextmf,
		     mf++ ) {
			ASSERT( off );
			if ( GET_REC_NOLOCK( fd, mf, 
					     sizeof( invt_mediafile_t ),
					     off ) <= 0 ) {
				free( strms );
				free( sesbuf );
				return BOOL_FALSE;
			}
		}
	}



	free( strms );
	*bufpp = sesbufcp;
	*bufszp = sessz;

	return BOOL_TRUE;
}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

bool_t
stobj_getsession_byuuid(
	int fd, 
	invt_seshdr_t *hdr, 
	void *sesid,
	void **buf )
{
	invt_session_t ses;
	uint	       stat;

	/* retrieve the session */
	if ( GET_REC_NOLOCK( fd, &ses, sizeof( invt_session_t ),
			     hdr->sh_sess_off ) < 0 )
		return -1;
	
	/* now see if this is the one that caller is askin for */
	if (! UUID_EQL(&ses.s_sesid, (uuid_t *)sesid, &stat)) {
		return BOOL_FALSE;	
	}

	/* yay. we found the session. so, make the session struct and 
	   put it in the buffer */
	stobj_copy_invsess(fd, hdr, &ses, (inv_session_t **)buf);

	return BOOL_TRUE;
	
}		




/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

bool_t
stobj_getsession_bylabel(
	int fd, 
	invt_seshdr_t *hdr, 
	void *seslabel,
	void **buf )
{
	invt_session_t ses;

	/* retrieve the session */
	if ( GET_REC_NOLOCK( fd, &ses, sizeof( invt_session_t ),
			     hdr->sh_sess_off ) < 0 )
		return -1;

	/* now see if this is the one that caller is askin for */
	if (! STREQL(ses.s_label, (char *)seslabel)) {
		return BOOL_FALSE;	
	}

	/* yay. we found the session. so, make the session struct and 
	   put it in the buffer */
	stobj_copy_invsess(fd, hdr, &ses, (inv_session_t **)buf);

	return BOOL_TRUE;
	
}	


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

/* ARGSUSED */
bool_t
stobj_delete_mobj(int fd, 
		  invt_seshdr_t *hdr, 
		  void *arg ,
		  void **buf )
{
	/* XXX fd needs to be locked EX, not SH */
	uuid_t *moid = *buf;
	invt_session_t ses;
	invt_stream_t  *strms;
	off64_t		off;
	invt_mediafile_t *mf, *mfiles;
	u_int 		nmfiles;
	u_int		i, j;
	bool_t 		dirty;
	uint_t		stat;

	if ( GET_REC_NOLOCK( fd, &ses, sizeof( invt_session_t ),
			     hdr->sh_sess_off ) < 0 )
		return -1;

	/* now get all the streams of this session */
	strms = calloc ( ses.s_cur_nstreams, sizeof( invt_stream_t ) );
	if ( GET_REC_NOLOCK( fd, strms, 
			     sizeof( invt_stream_t ) * ses.s_cur_nstreams, 
			     hdr->sh_streams_off ) < 0 ) {
		free ( strms );
		return BOOL_FALSE;
	}
	
	/* now look at all the mediafiles in all the streams */
	for ( i = 0; i < ses.s_cur_nstreams; i++ ) {
		off = strms[i].st_firstmfile;
		nmfiles = strms[i].st_nmediafiles;
		mfiles = mf = calloc( nmfiles, sizeof( invt_mediafile_t ) );
		for ( j = 0; j < nmfiles; 
		     j++, 
		     off = mf->mf_nextmf,
		     mf++ ) {
				
/*  The prob is that we need to keep track of where we got these mfiles from
    as we get them, or we wont know how to put them back if they are dirty.
*/
			ASSERT( off );
			if ( GET_REC_NOLOCK( fd, mf, 
					     sizeof( invt_mediafile_t ),
					     off ) <= 0 ) {
				free( strms );
				free( mfiles );
				return BOOL_FALSE;
			}
		}

		/* We have all the media files of this stream. Make another
		   pass, checking to see if we need to remove any mfiles */
 		dirty = BOOL_FALSE;

		for ( j = 0; j < nmfiles; j++ ) {
			mf = &mfiles[j];
			if ( UUID_EQL( &mf->mf_moid, moid, &stat ) ) {
				printf(" found one\n" );

/*                                dirty = BOOL_TRUE;	

				if ( j == 0 )
				       strms[i].st_firstmfile = mf->mf_nextmf;
				else
				       mfiles[j-1].mf_nextmf = mf->mf_nextmf;

				if ( j == nmfiles - 1 )
				       strms[i].st_lastmfile = ;
*/
			}

		}
		free ( mfiles );
		if ( dirty );
	}

	free ( strms );

	return BOOL_FALSE; /* ret FALSE, or it'll stop iteration */
}






/*----------------------------------------------------------------------*/
/* stobj_unpack_sessinfo                                                */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

bool_t
stobj_unpack_sessinfo(  
        void              *bufp,
        size_t             bufsz,
	invt_sessinfo_t   *s )
{
	u_int 		 i;
	char 		 *p = (char *) bufp ;
	
	ASSERT ( bufp );
	
	/* first make sure that the magic cookie at the beginning is right.
	   this isn't null-terminated */
	if (strncmp( p, INVTSESS_COOKIE, strlen( INVTSESS_COOKIE ) ) != 0) {
		mlog( MLOG_NORMAL | MLOG_INV,"INV: inv_put_session: unknown cookie\n");
		return BOOL_FALSE;
	}
	/* skip the cookie */
	p += strlen( INVTSESS_COOKIE ) * sizeof( char );

	/* Check the packing version number. In version 1 , this was the only version number.
	 * see the comment in stobj_pack_sessinfo().
	 */
	if ( *( inv_version_t *) p == PACKED_INV_VERSION_1 ) {
		char *temp_p;
		size_t		 tempsz;

	        mlog( MLOG_DEBUG | MLOG_INV,"INV: packed inventory version  = 1\n" );

		p += sizeof( inv_version_t );

		/* We hit a 64 bit alignment issue at this point leading to a
		 * SIGBUS and core dump. The best way to handle it is to 
		 * bcopy the remaining part of bufp to a new malloc'ed area
		 * which will be 64 bit aligned. This is a memory leak , but not much.
		 * Have to do this because xfsrestore does another round of
		 * unpack later , so can't disturb the original data.
		 * This is fixed in PACKED_INV_VERSION_2 by adding another (inv_version_t) to 
		 * have the INV_VERSION. This makes everything 64 bit aligned.
		 */

		tempsz = bufsz - (strlen( INVTSESS_COOKIE ) * sizeof( char )) 
			       - sizeof( inv_version_t ) ;
		temp_p = calloc(1, tempsz);
		bcopy(p, temp_p, tempsz);
		p = temp_p;
	} else if ( *( inv_version_t *) p == PACKED_INV_VERSION_2 ) {
	        mlog( MLOG_DEBUG | MLOG_INV,"INV: packed inventory version = 2\n" );

		p += sizeof( inv_version_t ); /* skip the packed inventory version */
		/* At this point , don't care about the INV_VERSION. Maybe in future */
		p += sizeof( inv_version_t ); /* skip the inventory version */
	} else {
	        mlog( MLOG_NORMAL | MLOG_INV,"INV: inv_put_session: unknown packed inventory version"
		      " %d\n", *( inv_version_t *) p);
		return BOOL_FALSE;
	} 

	/* get the seshdr and then, the remainder of the session */
	s->seshdr = (invt_seshdr_t *)p;
	s->seshdr->sh_sess_off = -1;
	p += sizeof( invt_seshdr_t );	

	s->ses = (invt_session_t *)p;
	p += sizeof( invt_session_t );

	/* the array of all the streams belonging to this session */
	s->strms = (invt_stream_t *)p;
	p += s->ses->s_cur_nstreams * sizeof( invt_stream_t );

	/* all the media files */
	s->mfiles = (invt_mediafile_t *)p;

#ifdef INVT_DELETION
	{
		int tmpfd = open( "moids", O_RDWR | O_CREAT );	
		u_int j;
		invt_mediafile_t *mmf = s->mfiles;
		for (i=0; i< s->ses->s_cur_nstreams; i++ ) {
			for (j=0; j< s->strms[ i ].st_nmediafiles; 
			     j++, mmf++ )
				put_invtrecord( tmpfd, &mmf->mf_moid, 
					 sizeof( uuid_t ), 0, SEEK_END, 0 );
		}
		close( tmpfd );
	}
#endif
	for ( i = 0; i < s->ses->s_cur_nstreams; i++ ) {
		p += (size_t) ( s->strms[ i ].st_nmediafiles )
			* sizeof( invt_mediafile_t );
	}
	
	/* sanity check the size of the buffer given to us vs. the size it 
	   should be */
	if ( (size_t) ( p - (char *) bufp ) != bufsz ) {
		mlog( MLOG_DEBUG | MLOG_INV, "p-bufp %d != bufsz %d entsz %d\n",  
		      (int)( p - (char *) bufp ), (int) bufsz,
	      (int) ( sizeof( invt_entry_t ) ) );
	}
	ASSERT( (size_t) ( p - (char *) bufp ) == bufsz );
	
	return BOOL_TRUE;
}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
stobj_make_invsess( int fd, inv_session_t **buf, invt_seshdr_t *hdr )
{
	invt_session_t ses;

	/* load in the rest of the session, but not the streams */
	if ( GET_REC_NOLOCK( fd, &ses, sizeof(ses), hdr->sh_sess_off )
	    < 0 ) {
		return -1;
	}
	
	return stobj_copy_invsess(fd, hdr, &ses, buf);
}


void
stobj_convert_mfile(inv_mediafile_t *expmf, invt_mediafile_t *mf)
{
	/* copy the mediafile into the exported structure */
	memcpy( &expmf->m_moid, &mf->mf_moid, sizeof( uuid_t ) );
	strcpy( expmf->m_label, mf->mf_label );
	expmf->m_mfile_index = mf->mf_mfileidx;
	expmf->m_startino = mf->mf_startino.ino;
	expmf->m_startino_off = mf->mf_startino.offset;
	expmf->m_endino = mf->mf_endino.ino;
	expmf->m_endino_off = mf->mf_endino.offset;
	expmf->m_size = mf->mf_size;
	expmf->m_isgood = (mf->mf_flag & INVT_MFILE_GOOD ) ?
		BOOL_TRUE : BOOL_FALSE;
	expmf->m_isinvdump = (mf->mf_flag & INVT_MFILE_INVDUMP)?
		BOOL_TRUE : BOOL_FALSE;

}

void
stobj_convert_strm(inv_stream_t *expstrm, invt_stream_t *strm)
{

	expstrm->st_interrupted = strm->st_interrupted;
	expstrm->st_startino = strm->st_startino.ino;
	expstrm->st_startino_off = 
		strm->st_startino.offset;
	expstrm->st_endino = strm->st_endino.ino;
	expstrm->st_endino_off = strm->st_endino.offset;
	strcpy( expstrm->st_cmdarg, strm->st_cmdarg );

	expstrm->st_nmediafiles = strm->st_nmediafiles;

	/* caller is responsible for initializing this */
	expstrm->st_mediafiles = NULL;
}

void
stobj_convert_session(inv_session_t *ises, invt_session_t *ses, 
		      invt_seshdr_t *hdr)
{
	ises->s_time = hdr->sh_time;
	ises->s_level = hdr->sh_level;
	ises->s_ispartial = IS_PARTIAL_SESSION( hdr )? BOOL_TRUE: BOOL_FALSE;
	ises->s_isresumed = IS_RESUMED_SESSION( hdr )? BOOL_TRUE: BOOL_FALSE;
	ises->s_nstreams = ses->s_cur_nstreams;
	memcpy( &ises->s_sesid, &ses->s_sesid, sizeof( uuid_t ) );
	memcpy( &ises->s_fsid, &ses->s_fsid, sizeof( uuid_t ) );
	strcpy( ises->s_mountpt, ses->s_mountpt );
	strcpy( ises->s_devpath, ses->s_devpath );
	strcpy( ises->s_label, ses->s_label );

	/* caller is responsible for initializing this */
	ises->s_streams = NULL;

}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
stobj_copy_invsess(int fd,  
		   invt_seshdr_t *hdr,
		   invt_session_t *ses,
		   inv_session_t **buf)
{
	inv_session_t  *ises;
	invt_stream_t  *strms;
	int i;
	invt_mediafile_t mf;

	strms = calloc ( ses->s_cur_nstreams, sizeof( invt_stream_t ) ); 

	/* load in all the stream-headers */
	if ( GET_REC_NOLOCK( fd, strms, 
			     ses->s_cur_nstreams * sizeof( invt_stream_t ), 
			     hdr->sh_streams_off ) < 0 ) {
		free (strms);
		return -1;
	}

	ises = calloc( 1, sizeof( inv_session_t ) );
	stobj_convert_session(ises, ses, hdr);
	ises->s_streams = calloc(ses->s_cur_nstreams, sizeof( inv_stream_t ));
	
	/* fill in the stream structures too */
	i = (int) ses->s_cur_nstreams;
	while ( i-- ) {
		off64_t		 off;
		u_int            j, nmf;
		
		stobj_convert_strm(&ises->s_streams[i], &strms[i]);
		nmf = strms[i].st_nmediafiles;
		off = strms[i].st_firstmfile;

		ises->s_streams[i].st_mediafiles = calloc( nmf, 
						    sizeof( inv_mediafile_t ) );
		ASSERT( ises->s_streams[i].st_mediafiles );

		for ( j = 0; j < nmf; 
		      j++, 
		      off = mf.mf_nextmf ) {
			ASSERT( off );
			if ( GET_REC_NOLOCK( fd, &mf, 
					     sizeof( invt_mediafile_t ),
					     off ) <= 0 ) {
				INV_PERROR( "stobj::make_invsess\n" );
				free( strms );
				free( ises );
				return -1;
			}

			/* copy the mediafile into the exported structure */
			stobj_convert_mfile( &ises->s_streams[i].st_mediafiles[j],
					     &mf);
			
		}
		
	}
	

	free( strms );
	*buf = ises;

	return 1;
}


/*----------------------------------------------------------------------*/
/*  Given a sessinfo structure, this makes an exportable inv_session.   */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

void
stobj_convert_sessinfo(inv_session_t **buf, invt_sessinfo_t *sinfo)
{
	inv_session_t  *ises;
	int i, j, nmf;
#ifdef INVCONVFIX
	int nstreams;
	invt_mediafile_t *mf;
#endif /* INVCONVFIX */

	ises = calloc( 1, sizeof( inv_session_t ) );

	stobj_convert_session(ises, sinfo->ses, sinfo->seshdr);
	ises->s_streams = calloc( ises->s_nstreams, sizeof( inv_stream_t ) );
#ifdef INVCONVFIX
	mf = sinfo->mfiles;
	nstreams = (int) ises->s_nstreams;
	for ( i = 0 ; i < nstreams ; i++ ) {
#else /* INVCONVFIX */
	i = (int) ises->s_nstreams;
	while ( i-- ) {
#endif /* INVCONVFIX */
		stobj_convert_strm(&ises->s_streams[i], &sinfo->strms[i]);
		nmf = (int) ises->s_streams[i].st_nmediafiles;
		ises->s_streams[i].st_mediafiles = calloc( (u_int) nmf,
						    sizeof( inv_mediafile_t ) );

		for ( j = 0; j < nmf; j++ ) {
			stobj_convert_mfile( &ises->s_streams[i].st_mediafiles[j],
#ifdef INVCONVFIX
					     mf++ );
#else /* INVCONVFIX */
					     sinfo->mfiles);
#endif /* INVCONVFIX */
		}
	}
	
	*buf = ises;
}



int
stobj_hdrcmp( const void *h1, const void *h2 )
{
	return (int) ( ((invt_seshdr_t *)h1)->sh_time - 
		       ((invt_seshdr_t *)h2)->sh_time );
}








void
DEBUG_sessprint( invt_session_t *ses )
{
	char *str = 0;
	uint_t stat;
	uuid_to_string( &ses->s_fsid, &str, &stat );

	printf("-------- session -------------\n");
	printf("label\t%s\n", ses->s_label);
	printf("mount\t%s\n", ses->s_mountpt);
	printf("devpath\t%s\n", ses->s_devpath);
	printf("strms\t%d\n", ses->s_cur_nstreams );
	printf("fsid\t%s\n", str);
	printf("-------- end -------------\n");

	free (str);
}

bool_t
mobj_eql ( inv_mediafile_t *mfp, invt_mobjinfo_t *mobj )
{
	uint_t stat;

	if ( mobj->type == INVT_MOID ) {
		if ( UUID_EQL( (uuid_t*) mobj->value, 
			      &mfp->m_moid, 
			      &stat) ) {
			return BOOL_TRUE;
		}
	} else {
		if ( STREQL( (char*) mobj->value, 
			    mfp->m_label ) ) {
			return BOOL_TRUE;
		}
	}
	
	return BOOL_FALSE;
}

  
bool_t
check_for_mobj ( inv_session_t *ses, invt_mobjinfo_t *mobj )
{
	int i;
	u_int j;
	inv_mediafile_t *mfp;

	for (i = 0; i < (int) ses->s_nstreams; i++ ) {
		for ( j = 0 ; j < ses->s_streams[i].st_nmediafiles ; j++ ) {
			mfp = &ses->s_streams[i].st_mediafiles[j];
			if (mobj_eql( mfp, mobj ))
				return BOOL_TRUE;
		}
	}

	return BOOL_FALSE; /* There are no matching media objects to print */
}




void
DEBUG_sessionprint( inv_session_t *ses, u_int ref, invt_pr_ctx_t *prctx)
{
	char *str = 0;
	int i;
	uint_t stat;
	inv_mediafile_t *mfp;
	static u_int fsidxprinted = -1;

       invt_mobjinfo_t *mobj = &prctx->mobj;

	bool_t  moidsearch = ( mobj && mobj->type != INVT_NULLTYPE );
	if ( moidsearch ){
		if (!check_for_mobj (ses, mobj))
			return;
	}
		
	if ( ref == 0 || fsidxprinted != (u_int) prctx->index ) {
		fsidxprinted = (u_int) prctx->index;

		printf("file system %d:\n", prctx->index);
		uuid_to_string( &ses->s_fsid, &str, &stat );
		printf("\tfs id:\t\t%s\n", str);
		free (str);	
	}
	
	if (prctx->depth == PR_FSONLY)
		return;

	if ((prctx->level != PR_MAXLEVEL) && (prctx->level != ses->s_level))
		return;

	printf("\tsession %d:\n", ref);
	printf("\t\tmount point:\t%s\n", ses->s_mountpt);
	printf("\t\tdevice:\t\t%s\n", ses->s_devpath);
	printf("\t\ttime:\t\t%s", ctime( &ses->s_time ));
	printf("\t\tsession label:\t\"%s\"\n", ses->s_label);
	uuid_to_string( &ses->s_sesid, &str, &stat );
	printf("\t\tsession id:\t%s\n", str);
	free( str );
	printf("\t\tlevel:\t\t%d\n", ses->s_level);
	printf("\t\tresumed:\t%s\n", ses->s_isresumed ? "YES" : "NO" );
	printf( "\t\tsubtree:\t%s\n", ses->s_ispartial ? "YES" : "NO" );
	printf("\t\tstreams:\t%d\n", ses->s_nstreams );
	
	if (prctx->depth == PR_SESSONLY )
		return;

	for (i = 0; i < (int) ses->s_nstreams; i++ ) {
		u_int j;
		printf("\t\tstream %d:\n", i );
		printf( "\t\t\tpathname:\t%s\n", ses->s_streams[i].st_cmdarg );
		printf( "\t\t\tstart:\t\tino %llu offset %lld\n",
		        ses->s_streams[i].st_startino,
		        ses->s_streams[i].st_startino_off );
		printf( "\t\t\tend:\t\tino %llu offset %lld\n",
		        ses->s_streams[i].st_endino,
		        ses->s_streams[i].st_endino_off );
		printf( "\t\t\tinterrupted:\t%s\n",
		        ses->s_streams[i].st_interrupted ? "YES" : "NO" );
		printf( "\t\t\tmedia files:\t%d\n",
		        ses->s_streams[i].st_nmediafiles);

		if (prctx->depth == PR_STRMSONLY )
			continue;

		for ( j = 0 ; j < ses->s_streams[i].st_nmediafiles ; j++ ) {
			mfp = &ses->s_streams[i].st_mediafiles[j];
			if ( moidsearch ) {
				if (! mobj_eql( mfp, mobj ) )
					continue;
			}
			printf( "\t\t\tmedia file %d:", j );
			/*
			if (mfp->m_isinvdump)
				printf(" SESSION INVENTORY");
			*/
			printf( "\n");
			printf( "\t\t\t\tmfile index:\t%d\n", mfp->m_mfile_index );
			printf( "\t\t\t\tmfile type:\t" );
			if (mfp->m_isinvdump) {
				printf( "inventory\n" );
			} else {
				printf( "data\n" );
			}
			printf( "\t\t\t\tmfile size:\t%llu\n",
				mfp->m_size);
			
			if (! mfp->m_isinvdump) {
				printf( "\t\t\t\tmfile start:"
					"\tino %llu offset %lld\n",
					mfp->m_startino,
					mfp->m_startino_off );
				printf( "\t\t\t\tmfile end:"
					"\tino %llu offset %lld\n",
					mfp->m_endino,
					mfp->m_endino_off );
			}

			printf( "\t\t\t\tmedia label:\t\"%s\"\n",
				mfp->m_label);
			uuid_to_string( &mfp->m_moid, &str, &stat );
			printf( "\t\t\t\tmedia id:\t%s\n", str );
			free( str );
		}
	}
}
