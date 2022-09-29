#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/inventory_priv.c,v 1.1 1994/09/13 19:28:54 cbullis Exp $"

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
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/dir.h>
#include "types.h"
#include "inventory_priv.h"

extern int sesslock_fd;




/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

inv_idbtoken_t
create_invindex( int fd, char *fname, char *uuname )
{
	int stobjfd;
	inv_idbtoken_t tok;

	if ((fd = open ( fname , O_RDWR | O_CREAT ) ) < 0 ) {
		perror ( fname );
		return INV_TOKEN_NULL;
	}
	
	INVLOCK( fd, LOCK_EX );
	fchmod( fd, INV_PERMS );

#ifdef INVT_DEBUG
	printf ("creating InvIndex %s\n", fname);
#endif
	
	/* create the first entry in the new inv_index */
	stobjfd = create_invindex_entry( &tok, fd, uuname, BOOL_TRUE );
	
	INVLOCK( fd, LOCK_UN );

	if ( stobjfd < 0 )
		return INV_TOKEN_NULL;
	return tok;
}






/*----------------------------------------------------------------------*/
/* put_sesstime                                                         */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
put_sesstime( inv_sestoken_t tok, bool_t whichtime)
{
	int rval;
	invt_entry_t ent;
	int fd = tok->sd_invtok->d_invindex_fd;

	INVLOCK( fd, LOCK_EX );

	rval = GET_REC_NOLOCK( fd, &ent, sizeof( invt_entry_t ),
			        tok->sd_invtok->d_invindex_off);
	if ( rval < 0 ) 
		return -1;

	ent.ie_timeperiod.tp_end = tok->sd_sesstime;
	
	if ( whichtime == INVT_STARTTIME ) {
		ent.ie_timeperiod.tp_start = tok->sd_sesstime;
	}

	rval = PUT_REC_NOLOCK_SEEKCUR( fd, &ent, sizeof( invt_entry_t ),
				       (off64_t) -(sizeof( invt_entry_t )));

	INVLOCK( fd, LOCK_UN );
	return rval;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
put_counters( int fd, void *cntp, size_t sz )
{
	ASSERT( cntp );
	
	return PUT_REC( fd, cntp, sz, (off64_t) 0 );
}





/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
get_counters( int fd, void **cntpp, size_t cntsz )
{
	/* file must be locked at least SHARED by caller */

	*cntpp =  calloc( 1, cntsz);

	/* find the number of sessions and the max possible */
	if ( GET_REC_NOLOCK( fd, (void *) *cntpp, cntsz, (off64_t) 0 ) < 0 ) {
		free( *cntpp );
		*cntpp = NULL;
		return -1;
	}
	
	return ((invt_counter_t *)(*cntpp))->ic_curnum;
}







/*----------------------------------------------------------------------*/
/* get_headers                                                          */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
get_headers( int fd, void **hdrs, size_t bufsz, size_t off )
{

	*hdrs = malloc( bufsz );

	/* file must be locked at least SHARED by caller */
	
	/* get the array of hdrs */
	if ( GET_REC_NOLOCK( fd, (void *) *hdrs, bufsz, (off64_t)off ) < 0 ) {
		free ( *hdrs );
		*hdrs = NULL;
		return -1;
	}
	
	return 1;
}





/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

int
get_curnum( int fd )
{
	invt_counter_t *cnt = NULL;
	
	int curnum = GET_COUNTERS( fd, &cnt );
	free ( cnt );
	return curnum;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
get_invtrecord( int fd, void *buf, size_t bufsz, off64_t off, 
	        int whence, bool_t dolock )
{
	int  nread;
	
	ASSERT ( fd >= 0 );
	
	if ( dolock ) 
		INVLOCK( fd, LOCK_SH );

	if ( lseek( fd, (off_t)off, whence ) < 0 ) {
		perror( "lseek: get_invtrecord sez - " );
		if ( dolock ) 
			INVLOCK( fd, LOCK_UN );
		return -1;
	}
	
	nread = read( fd, buf, bufsz );

	if (  nread != (int) bufsz ) {
#ifdef INVT_DEBUG
		printf ("read failed on fd %d (%d of %d) %x\n", 
			fd, nread, bufsz, (long) buf);
#endif		
		perror("get_invtrecord sez - ");
		if ( dolock ) 
			INVLOCK( fd, LOCK_UN );
		return -1;
	}

	if ( dolock ) 
		INVLOCK( fd, LOCK_UN );

	return nread;
}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
put_invtrecord( int fd, void *buf, size_t bufsz, off64_t off, 
	        int whence, bool_t dolock )
{
	int nwritten;
	
	if ( dolock )
		INVLOCK( fd, LOCK_EX );
	
	if ( lseek( fd, (off_t)off, whence ) < 0 ) {
		perror( "lseek: put_invtrecord sez - " );
		if ( dolock ) 
			INVLOCK( fd, LOCK_UN );
		return -1;
	}
	
	if (( nwritten = write( fd, buf, bufsz ) ) != (int) bufsz ) {
		perror( "Error in writing inventory record :" );
		if ( dolock )
			INVLOCK( fd, LOCK_UN );
		return -1;
	}

	if ( dolock )
		INVLOCK( fd, LOCK_UN );
	return nwritten;
}








/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/


intgen_t
get_headerinfo( int fd, void **hdrs, void **cnt,
	        size_t hdrsz, size_t cntsz, bool_t dolock )
{	
	int num; 

	/* get a lock on the table for reading */
	if ( dolock ) INVLOCK( fd, LOCK_SH );
	
	num = get_counters( fd, cnt, cntsz );

	/* If there are no sessions recorded yet, we're done too */
	if ( num > 0 ) {
		if ( get_headers( fd, hdrs, hdrsz * (size_t)num, cntsz ) < 0 ) {
			free ( *cnt );
			num = -1;
		}
	}

	if ( dolock ) INVLOCK( fd, LOCK_UN );
	return num;
}








/* an inventory index entry keeps track of a single storage object;
   it knows about its name (ie filename) and the timeperiod that the
   it contains dump sessions for.
   note that each file system has its own (set of) inventory indices.
*/

/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
create_invindex_entry(  
	inv_idbtoken_t *tok, 
	int invfd, char *uuname, 
	bool_t firstentry )
{
	invt_entry_t   	ent;
	int	      	fd;
	off64_t 		hoff;


	memset ( &ent, 0, sizeof( ent ) );
	
	/* initialize the start and end times to be the same */
	ent.ie_timeperiod.tp_start = ent.ie_timeperiod.tp_end = (time_t)0;

	if ( firstentry ) {

		invt_counter_t cnt;

		cnt.ic_maxnum = INVT_MAX_INVINDICES;
		cnt.ic_curnum = 1;
		

		fd = create_storageobj( uuname, ent.ie_filename, 1 );
		if ( fd < 0 ) {
			return -1;
		}

		if ( PUT_REC_NOLOCK( invfd, &cnt, sizeof( cnt ), (off64_t) 0 ) < 0 )
			return -1;

		/* XXX  we need put_header(), put_counters() */
		hoff = sizeof( invt_counter_t );

		if ( PUT_REC_NOLOCK( invfd, &ent, sizeof( ent ), hoff ) < 0)
			return -1;
	} else {
		invt_counter_t *cnt = NULL;

		if ( GET_COUNTERS( invfd, &cnt )  < 0 ) {
			return -1;
		}
		
		/* XXX check if there are too many indices. if so, create 
		   another and leave a pointer to that in here */
		
		/* create the new storage object */
		fd = create_storageobj( uuname, ent.ie_filename, 
				        ++(cnt->ic_curnum) );
		if ( fd < 0 ) {
			return -1;
		}

		if ( PUT_COUNTERS( invfd, cnt ) < 0 ) {
			return -1;
		}

		/* add the new index entry to the array, at the end */

		hoff = INVINDEX_HDR_OFFSET( cnt->ic_curnum - 1 );
		free (cnt);
#ifdef INVT_DEBUG
		printf("new stobj name %s @ offset %d\n",ent.ie_filename,(int)hoff);
#endif
		if ( PUT_REC_NOLOCK( invfd, &ent, sizeof( ent ), hoff ) < 0 )
			return -1;
		
	}

	*tok = get_token( invfd, fd );
	(*tok)->d_invindex_off = hoff;
	(*tok)->d_update_flag |= NEW_INVINDEX;
	return fd;

}







/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

		
/* NOTE: this doesnt update counters or headers in the inv_index */
int
create_storageobj( char *uuname, char *fname, int index)
{	
	char buf[7]; /* upto 009999 */
	int fd;	
	invt_sescounter_t sescnt;

	
	/* come up with a new unique name */
	sprintf( buf, "XX%d\0", index ); 
	strcpy( fname, uuname );
	strcat( fname, buf ); 
	strcat( fname, INV_STOBJ_PREFIX );

#ifdef INVT_DEBUG
	printf( "creating storage obj %s\n", fname);
#endif	
	ASSERT( (int) strlen( fname ) < INV_STRLEN );

	
	/* create the new storage object */
	if (( fd = open( fname, O_RDWR | O_EXCL | O_CREAT )) < 0 ) {
		perror ( fname );
		memset( fname, 0, INV_STRLEN );
		return -1;
	}
	
	INVLOCK( fd, LOCK_EX );
	fchmod( fd, INV_PERMS );

	sescnt.ic_curnum = 0; /* there are no sessions as yet */
	sescnt.ic_maxnum = INVT_STOBJ_MAXSESSIONS;

	sess_lock();
	sescnt.ic_eof = SC_EOF_INITIAL_POS;
	sess_unlock();

	if ( PUT_SESCOUNTERS ( fd, &sescnt ) < 0 ) {
		memset( fname, 0, INV_STRLEN );
		INVLOCK( fd, LOCK_UN );
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
create_session( 
	inv_sestoken_t tok, 
	int fd, 
	invt_sescounter_t *sescnt, 
	invt_session_t *ses, 
	invt_seshdr_t *hdr )
{
	off64_t hoff;

	/* figure out the place where the header will go */
	hoff =  (off64_t) sizeof( invt_sescounter_t ) + 
		(size_t) sescnt->ic_curnum * sizeof( invt_seshdr_t ) ;

	/* figure out where the session information is going to go. */
	hdr->sh_sess_off = sizeof( invt_sescounter_t ) + 
		         sescnt->ic_maxnum * sizeof( invt_seshdr_t ) +
			 sescnt->ic_curnum * sizeof( invt_session_t ) ;
	
	sescnt->ic_curnum++;

#ifdef INVT_DEBUG
	printf ( "create sess #%d @ offset %d ic_eof = %d\n", 
		sescnt->ic_curnum, (int) hdr->sh_sess_off, (int) sescnt->ic_eof );
#endif
	ses->s_cur_nstreams = 0;
	
	/* we need to know where the streams begin, and where the media files will
	   begin, at the end of the streams */
	sess_lock(); 
	hdr->sh_streams_off = sescnt->ic_eof;
	sescnt->ic_eof += (off64_t)( ses->s_max_nstreams * sizeof( invt_stream_t ) );
	sess_unlock();

	/* XXX Have a vector'd write do these 3 writes */
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

	tok->sd_sesshdr_off = hoff;
	tok->sd_session_off = hdr->sh_sess_off;

	return 1;
}








/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
put_mediafile( inv_stmtoken_t tok, invt_mediafile_t *mf )
{
	int rval;
	invt_sescounter_t *sescnt = NULL;
	invt_stream_t stream;
	inv_sestoken_t sestok = tok->md_sesstok;
	int fd =  sestok->sd_invtok->d_stobj_fd;
	off64_t pos;


	/* first we need to find out where the current write-position is.
	   so, we first read the sescounter that is at the top of this
	   storage object */
	sess_lock();
	rval = GET_SESCOUNTERS( fd, &sescnt );
	if ( rval < 0 ) {
		sess_unlock();
		return -1;
	}

	pos = sescnt->ic_eof;

	/* increment the pointer to give space for this media file */
	sescnt->ic_eof += (off64_t) sizeof( invt_mediafile_t );
	rval = PUT_SESCOUNTERS( fd, sescnt );
	sess_unlock();

	if ( rval < 0 )
		return -1;

	/* get the stream information, and update number of mediafiles.
	   we also need to link the new mediafile into the linked-list of
	   media files of this stream */

	rval = GET_REC_NOLOCK( fd, &stream, sizeof( stream ), tok->md_stream_off );
	if ( rval < 0 ) {
		return -1;
	}
	/* We need to update the last ino of this STREAM, which is now the
	   last ino of the new mediafile. If this is the first mediafile, we
	   have to update the startino as well. Note that ino is a <ino,off>
	   tuple */
	if ( stream.st_nmediafiles == 0 )
		stream.st_startino = mf->mf_startino;
	stream.st_endino = mf->mf_endino;

	stream.st_nmediafiles++;
#ifdef INVT_DEBUG
	printf ("#################### mediafile #%d ###################\n", 
		stream.st_nmediafiles);
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
	rval = PUT_REC_NOLOCK( fd, &stream, sizeof( stream ), tok->md_stream_off );
	if ( rval < 0 ) {
		return -1;
	}
	
	/* write the prev media file to disk too */
	if ( tok->md_lastmfile ) {
		rval = PUT_REC_NOLOCK( fd, tok->md_lastmfile, 
				       sizeof( invt_mediafile_t ), mf->mf_prevmf );
		free (  tok->md_lastmfile );
		if ( rval < 0 ) {
			return -1;
		}
	}

	tok->md_lastmfile = mf;

	/* at last, write the new media file to disk */
	rval = PUT_REC_NOLOCK( fd, mf, sizeof( invt_mediafile_t ), pos );
	if ( rval < 0 ) {
		return -1;
	}

	return rval;
}





/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

int
get_storageobj( int invfd, int *index )
{
	invt_entry_t 	*ent = 0;
	int	     	 fd;

	/* if there's space anywhere at all, then it must be in the last
	   entry */
	if ((*index = get_lastheader( invfd, (void **)&ent, 
				       sizeof(invt_entry_t),
				       sizeof(invt_counter_t) ) ) < 0 )
		return -1;
	/* what if there arent any headers, and get_lastheader rets 0 ? */
	ASSERT ( ent != NULL );	
	ASSERT ( ent->ie_filename );

	fd = open( ent->ie_filename, O_RDWR );
	if ( fd < 0 )
		perror( ent->ie_filename );
	free ( ent );

	return fd;
}

intgen_t
get_lastheader( int fd, void **ent, size_t hdrsz, size_t cntsz )
{	
	int	     	 nindices;
	void	 	 *arr = NULL;
	invt_counter_t	 *cnt = NULL;
	char 		 *pos;
	/* get the entries in the inv_index */
	if ( ( nindices = GET_ALLHDRS_N_CNTS( fd, &arr, (void **)&cnt, 
					 hdrsz, cntsz )) <= 0 ) {
		return -1;
	}
	
	/* if there's space anywhere at all, then it must be in the last
	   entry */
	*ent = malloc( hdrsz );
	pos = (char *) arr + ( (u_int)nindices - 1 ) * hdrsz;
	memcpy( *ent, pos, hdrsz );
	free ( arr );
	free ( cnt );

	return nindices;
}







/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/


intgen_t
get_fstab( invt_fstab_t **arr, invt_counter_t **cnt, int *numfs )
{
	int fd;

	fd = open (INV_FSTAB, O_RDWR);
	
	if ( fd < 0 ) {
		return -1;
	}
	
	INVLOCK( fd, LOCK_EX );
	if (( *numfs = GET_ALLHDRS_N_CNTS_NOLOCK( fd, (void**) arr, 
						 (void **)cnt, 	
						 sizeof( invt_fstab_t ), 	
						 sizeof( invt_counter_t ) )
	     ) < 0 ) {
		printf("hdrs :( \n");
	}
#ifdef INVT_DEBUG	
	printf(" number of filesystems in fstab %d\n", *numfs );
#endif
	return fd;
}



intgen_t
make_invdirectory( void )
{
	DIR *dr;
	
	if (( dr = opendir( INV_DIRPATH )) == 0 ) {
		
		if ( mkdirp( INV_DIRPATH, (mode_t)0755 ) < 0 ) {
			perror( INV_DIRPATH );
			return -1;
		}

		printf("new dir created\n");
		return 1;
	}

	
	closedir ( dr);
	return 1;
	

}
	
			

/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/* caller takes the responsibility of calling this only when the FSTAB_ */
/* UPDATED flag in the inv_idbtoken is off, and also of updating the */
/* flag upon successful return from put_fstab_entry.                    */
/*----------------------------------------------------------------------*/


intgen_t
put_fstab_entry( uuid_t *fsidp, char *mntpt, char *dev )
{
	int numfs, i, fd;
	invt_counter_t *cnt;
	invt_fstab_t *arr;
	int rval = 1;

	/* fd is locked on return */
	fd = get_fstab( &arr, &cnt, &numfs );
	if ( fd < 0 ) {
		if ( errno != ENOENT) {
			return -1;
		}
		if ((fd = open( INV_FSTAB, O_RDWR | O_CREAT )) < 0 ) {
			perror ( INV_FSTAB );
			return -1;
		}
		
		INVLOCK( fd, LOCK_EX );
		fchmod( fd, INV_PERMS );
		
		cnt = (invt_counter_t *) malloc( sizeof ( invt_counter_t ) );

		cnt->ic_maxnum = -1;
		cnt->ic_curnum = 0;

	} else if ( numfs > 0 ) {
		uint_t stat;
		
		for (i = 0; i < numfs; i++) {
			if ( uuid_compare( fsidp, &arr[ i ].ft_uuid, &stat ) == 0 ) {
				ASSERT( ( strcmp( arr[i].ft_mountpt, mntpt ) == 0 ) &&
					( strcmp( arr[i].ft_devpath, dev ) == 0 ) );
				free ( arr );
				free ( cnt );
				close( fd );
				return 1;
			}
		}
		/* entry not found. just follow thru to create a new one */
		free ( arr );
	}



	/* make a new fstab entry and insert it at the end. the tabel is locked
	   EXclusively at this point */
	{
		invt_fstab_t ent;
		off64_t hoff;

		memcpy( &ent.ft_uuid, fsidp, sizeof( uuid_t ) );
		strcpy( ent.ft_mountpt, mntpt );
		strcpy( ent.ft_devpath, dev );
		
		/* increase the number of entries first */
#ifdef INVT_DEBUG
		printf("$ putting new fstab entry for %s ..........\n", mntpt);
#endif
		cnt->ic_curnum++;
		hoff = (off64_t) ( sizeof( invt_counter_t ) + 
				 (size_t)( cnt->ic_curnum - 1 ) * 
				           sizeof( invt_fstab_t ) );

		rval = PUT_COUNTERS( fd, cnt );
		if ( rval > 0 ) {
			rval = PUT_REC_NOLOCK( fd, &ent, sizeof( ent ), hoff );
		}

	}
	INVLOCK( fd, LOCK_UN );
	free ( cnt );	
	close ( fd );
	return rval;
}







intgen_t
get_fname(  void *pred, char *fname, inv_predicate_t bywhat )
{
	uuid_t *uuidp = 0;
	char *uuidstr;
	uint_t stat;
	invt_fstab_t *arr;


	if (bywhat != INV_BY_UUID) {
		int numfs, i, fd;
		invt_counter_t *cnt;

		/* on sucessful return fd is locked */
		fd = get_fstab( &arr, &cnt, &numfs );
		if (fd < 0)
			return -1;
		ASSERT( numfs >= 0 );

		INVLOCK( fd, LOCK_UN );
		close ( fd );
		free ( cnt ); /* we dont need it */

		/* first get hold of the uuid for this mount point / device */

		for (i = 0; i < numfs; i++) {
			if ( ( bywhat == INV_BY_MOUNTPT && 
				( strcmp( arr[i].ft_mountpt, pred ) == 0 )) ||
			     ( bywhat == INV_BY_DEVPATH &&
			      ( strcmp( arr[i].ft_devpath, pred ) == 0 )) ) {

				uuidp = &arr[i].ft_uuid;
				break;
			}
		}
#ifdef INVT_DEBUG				
		if (! uuidp )
			fprintf( stderr, "INV get_fname: unable to find %s"
			 " in the inventory\n", (char *)pred);
#endif
		
	} else {
		uuidp = (uuid_t *)pred;
	}
	
	if (! uuidp )
		return -1;

	uuid_to_string( uuidp, &uuidstr, &stat );
	
	if (stat != uuid_s_ok) {
		ASSERT (0); /* XXX */
		return -1;
	}

	strncpy ( fname, INV_DIRPATH, INV_STRLEN );
	strcat ( fname, "/" );
	strcat ( fname, uuidstr);
	
	free ( uuidstr );	/* uuid library allocated this for us */
	if ( bywhat != INV_BY_UUID ) 
		free ( arr );

	ASSERT( (int) strlen( fname ) < INV_STRLEN );
	return 1;
}	
	

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*                                                                           */
/* Returns -1 if we are done with initialization, the fd if not.             */
/* The idb_token indicates whether there was an error or not.                */
/*---------------------------------------------------------------------------*/

int
init_idb( void *pred, inv_predicate_t bywhat, char *uuname, inv_idbtoken_t *tok )
{
	char fname[ INV_STRLEN ];
	int fd;

	*tok = INV_TOKEN_NULL;
	/* make sure INV_DIRPATH exists, and is writable */
	if ( make_invdirectory( ) < 0 )
		return -1;

	/* we need this for locking */
	if ( sesslock_fd < 0 ) {
		if (( sesslock_fd = open( SESSLOCK_FILE, O_RDWR | O_CREAT ))<0 ){
			perror(  SESSLOCK_FILE );
			return -1;
		}
		fchmod( sesslock_fd, INV_PERMS );
	}
	/* come up with the unique file suffix that refers this filesystem */
	if ( get_fname( pred, uuname, bywhat ) < 0 ) {
		close( sesslock_fd );
		sesslock_fd = -1;
		return -1;
	}

	( void )strcpy( fname, uuname );
	strcat ( fname, INV_INVINDEX_PREFIX );

	/* first check if the inv_index file exists: if not create it */
	if ( ( fd = open( fname, O_RDWR ) ) == -1 ) {
		if (errno != ENOENT) {
			perror ( fname );
			close( sesslock_fd );
			sesslock_fd = -1;
			return -1;
		}
		*tok = create_invindex( fd, fname, uuname ); 
		return -1; /* we are done whether token's NULL or not */
	}
	
	/* we've got to do more housekeeping stuff. so signal that */
	return fd;
}





inv_idbtoken_t
get_token( int invfd, int stobjfd  )
{
	invt_desc_entry_t *desc;

	desc = (invt_desc_entry_t *) malloc
		( sizeof( invt_desc_entry_t ) );
	
	desc->d_invindex_fd = invfd;
	desc->d_stobj_fd  = stobjfd;
	desc->d_update_flag = 0;
	desc->d_invindex_off = -1;

	if ( invfd < 0 || stobjfd < 0 ) {
		fprintf(stderr, "get_token warning: "
			"invfd = %d stobjfd = %d\n", invfd, stobjfd );
	}

	return (inv_idbtoken_t) desc; /* yukky, but ok for the time being */
}






void
destroy_token( inv_idbtoken_t tok )
{
	free ( (invt_sesdesc_entry_t *) tok );
}



inv_sestoken_t
get_sesstoken( inv_idbtoken_t tok )
{
	inv_sestoken_t stok;

	stok = (inv_sestoken_t) malloc( sizeof( invt_sesdesc_entry_t ) );
	stok->sd_invtok = tok;
	stok->sd_session_off = stok->sd_sesshdr_off = -1;
	stok->sd_sesstime = (time_t) 0;
	return stok;
}




intgen_t
open_invindex( void *pred, inv_predicate_t bywhat )
{
	char fname[INV_STRLEN];
	int invfd;

	if ( get_fname( pred, fname, bywhat ) < 0 )
		return -1;
	strcat ( fname, ".InvIndex" );
	
	invfd = open (fname, O_RDONLY);
	
	return invfd;
}




void
sess_lock( void )
{
	INVLOCK( sesslock_fd, LOCK_EX );

}




void
sess_unlock( void )
{
	INVLOCK( sesslock_fd, LOCK_UN );

}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
search_invt( 
	inv_idbtoken_t tok, 
	u_char level, 
	void **buf,
	search_callback_t do_chkcriteria )
{

	int 		fd, i;
	invt_entry_t	*iarr = NULL;
	invt_counter_t	*icnt = NULL;
	int	     	nindices;
	

	ASSERT ( tok->d_invindex_fd >= 0 );
	*buf = NULL;
	
	if ( ( nindices = GET_ALLHDRS_N_CNTS( tok->d_invindex_fd, (void **)&iarr, 
					      (void **)&icnt, 
					      sizeof( invt_entry_t ),
					      sizeof( invt_counter_t )) 
	      ) <= 0 ) {
		return -1;
	}

	free( icnt );

	/* we need to get all the invindex headers and seshdrs in reverse order */
	for (i = nindices - 1; i >= 0; i--) {
		int 			nsess;
		invt_sescounter_t 	*scnt = NULL;
		invt_seshdr_t		*sarr = NULL;
		bool_t found;

		fd = open (iarr[i].ie_filename, O_RDONLY );
		if (fd < 0) {
			perror( iarr[i].ie_filename );
			continue;
		}
		INVLOCK( fd, LOCK_SH );

		/* Now see if we can find the session that we're looking for */
		if (( nsess = GET_ALLHDRS_N_CNTS_NOLOCK( fd, (void **)&sarr, 
						  (void **)&scnt, 
						  sizeof( invt_seshdr_t ),
						  sizeof( invt_sescounter_t ))
		     ) < 0 ) {
			perror( iarr[i].ie_filename );
			INVLOCK( fd, LOCK_UN );
			close( fd );
			continue;
		}
		free ( scnt );
		while ( nsess ) {
			/* fd is kept locked until we return from the callback
			   routine */
			found = (* do_chkcriteria ) ( fd, &sarr[ --nsess ],
						      level, buf );
			if (! found ) continue;
			
			/* we found what we need; just return */
			INVLOCK( fd, LOCK_UN );
			close( fd );
			free( sarr );
			return found; /* == -1 or 1 */
		}
		
		INVLOCK( fd, LOCK_UN );
		close( fd );
	}
	
	printf( "$ Search not succeeded.\n");

	return 0;
}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
tm_level_lessthan( int fd, invt_seshdr_t *hdr, u_char level,
		   void **tm )
{
	if (hdr->sh_level < level ) {
#ifdef INVT_DEBUG
		printf( "$ found level %d < %d\n", hdr->sh_level, level );
#endif
		*tm = calloc( 1, sizeof( time_t ) );
		memcpy( *tm, &hdr->sh_time, sizeof( time_t ) );
		return 1;
	}
	return 0;
}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
lastsess_level_lessthan( int fd, invt_seshdr_t *hdr, u_char level,
			 void **buf )
{
	if (hdr->sh_level < level ) {
#ifdef INVT_DEBUG
	printf( "$ found (ses) level %d < %d \n", hdr->sh_level, level );
#endif
		return make_invsess( fd, (inv_session_t **) buf, hdr );
	}
	return 0;

}


/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
lastsess_level_equalto( int fd, invt_seshdr_t *hdr, u_char level, void **buf )
{
	if (hdr->sh_level == level ) {
#ifdef INVT_DEBUG
	printf( "$ found (ses) level %d == %d \n", hdr->sh_level, level );
#endif
		return make_invsess( fd, (inv_session_t **) buf, hdr );
	}
	return 0;


}



/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
make_invsess( int fd, inv_session_t **buf, invt_seshdr_t *hdr )
{
	invt_session_t ses;
	inv_session_t  *ises;
	invt_stream_t  *strms;
	int i;
	
	
	/* load in the rest of the session */
	if ( GET_REC_NOLOCK( fd, &ses, sizeof(ses), hdr->sh_sess_off )
	    < 0 ) {
		return -1;
	}
	
	strms = calloc ( ses.s_cur_nstreams, sizeof( invt_stream_t ) ); 

	/* load in all the stream-headers */
	if ( GET_REC_NOLOCK( fd, strms, 
			     ses.s_cur_nstreams * sizeof( invt_stream_t ), 
			     hdr->sh_streams_off ) < 0 ) {
		free (strms);
		return -1;
	}

	ises = calloc( 1, sizeof( inv_session_t ) );
	ises->s_streams = calloc( ses.s_cur_nstreams, sizeof( inv_stream_t ) );
	ises->s_time = hdr->sh_time;
	ises->s_level = hdr->sh_level;
	ises->s_nstreams = ses.s_cur_nstreams;
	memcpy( &ises->s_sesid, &ses.s_sesid, sizeof( uuid_t ) );
	memcpy( &ises->s_fsid, &ses.s_fsid, sizeof( uuid_t ) );
	strcpy( ises->s_mountpt, ses.s_mountpt );
	strcpy( ises->s_devpath, ses.s_devpath );
	strcpy( ises->s_label, ses.s_label );
	
	/* fill in the stream structures too */
	i = (int) ses.s_cur_nstreams;
	while ( i-- ) {
		ises->s_streams[i].st_interrupted = strms[i].st_interrupted;
		ises->s_streams[i].st_startino = strms[i].st_startino.ino;
		ises->s_streams[i].st_startino_off = strms[i].st_startino.offset;
		ises->s_streams[i].st_endino = strms[i].st_endino.ino;
		ises->s_streams[i].st_endino_off = strms[i].st_endino.offset;
		ises->s_streams[i].st_nmediafiles = strms[i].st_nmediafiles;
	}
	

	free( strms );
	*buf = ises;

	return 1;
}


void
create_inolist_item( inv_inolist_t *cur, ino64_t ino )
{
	cur->i_next = malloc( sizeof( inv_inolist_t ) );
	cur->i_next->i_next = NULL;
	cur->i_next->i_ino = ino;
}



intgen_t
DEBUG_displayallsessions( int fd, invt_seshdr_t *hdr, u_char level, void **buf )
{
	inv_session_t **ses = (inv_session_t **) buf;
	if ( make_invsess( fd, ses, hdr ) < 1 )
		return -1;

	DEBUG_sessionprint( *ses );
	free( (*ses)->s_streams );
	free( *ses );
	*ses = NULL;
	
	return 0;
}


void
DEBUG_sessionprint( inv_session_t *ses )
{
	int i;
	printf("-------- session -------------\n");
	printf("label\t%s\n", ses->s_label);
	printf("mount\t%s\n", ses->s_mountpt);
	printf("devpath\t%s\n", ses->s_devpath);
	printf("level\t%d\n", ses->s_level);
	printf("strms\t%d\n", ses->s_nstreams );

	for (i = 0; i < (int) ses->s_nstreams; i++ ) {
		printf("STR %d (%llu-%llu) nmedia %d\n",i+1,
		        ses->s_streams[i].st_startino,
		        ses->s_streams[i].st_endino,
		       ses->s_streams[i].st_nmediafiles);
	}
	printf("-------- end -------------\n");
}


