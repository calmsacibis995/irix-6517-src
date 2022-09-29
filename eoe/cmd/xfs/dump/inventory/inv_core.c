#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/inventory/RCS/inv_core.c,v 1.8 1995/07/14 21:39:06 cbullis Exp $"

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
#include "inv_priv.h"



/*----------------------------------------------------------------------*/
/*  get_counters, get_headers, get_invtrecord, put_invtrecord, ...      */
/*                                                                      */
/*  These implement low level routines that take care of disk I/Os.     */
/*  In most cases, the caller has the option of locking before calling  */
/*  the routine.                                                        */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
get_counters( int fd, void **cntpp, size_t cntsz )
{
	/* object must be locked at least SHARED by caller */
	u_int num;
	ASSERT( cntsz >= sizeof( invt_counter_t ) );

	*cntpp =  calloc( 1, cntsz);

	/* find the number of sessions and the max possible */
	if ( GET_REC_NOLOCK( fd, (void *) *cntpp, cntsz, (off64_t) 0 ) < 0 ) {
		free( *cntpp );
		*cntpp = NULL;
		return -1;
	}
	
	num = ((invt_counter_t *)(*cntpp))->ic_curnum;

	if ( ( (invt_counter_t *)(*cntpp))->ic_vernum != INV_VERSION ) {
		mlog( MLOG_NORMAL | MLOG_INV, 
		      "INV : Unknown version %d - Expected version %d \n",
		      (int) ( (invt_counter_t *)(*cntpp))->ic_vernum,
		      (int) INV_VERSION );
		ASSERT ( ((invt_counter_t *)(*cntpp))->ic_vernum ==
			INV_VERSION );
	} 

	return (intgen_t) num;
}





/*----------------------------------------------------------------------*/
/* get_headers                                                          */
/*----------------------------------------------------------------------*/

intgen_t
get_headers( int fd, void **hdrs, size_t bufsz, size_t off )
{

	*hdrs = malloc( bufsz );
	if ( *hdrs == NULL ) {
		INV_PERROR( "get_headers() - malloc(seshdrs)\n" );
		return -1;
	}
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
/* get_invtrecord                                                       */
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
		INV_PERROR( "Error in reading inventory record "
			    "(lseek failed): " );
		if ( dolock ) 
			INVLOCK( fd, LOCK_UN );
		return -1;
	}
	
	nread = read( fd, buf, bufsz );

	if (  nread != (int) bufsz ) {
		INV_PERROR( "Error in reading inventory record :" );
		if ( dolock ) 
			INVLOCK( fd, LOCK_UN );
		return -1;
	}

	if ( dolock ) 
		INVLOCK( fd, LOCK_UN );

	return nread;
}






/*----------------------------------------------------------------------*/
/* put_invtrecord                                                       */
/*----------------------------------------------------------------------*/

intgen_t
put_invtrecord( int fd, void *buf, size_t bufsz, off64_t off, 
	        int whence, bool_t dolock )
{
	int nwritten;
	
	if ( dolock )
		INVLOCK( fd, LOCK_EX );
	
	if ( lseek( fd, (off_t)off, whence ) < 0 ) {
		INV_PERROR( "Error in writing inventory record "
			    "(lseek failed): " );
		if ( dolock ) 
			INVLOCK( fd, LOCK_UN );
		return -1;
	}
	
	if (( nwritten = write( fd, buf, bufsz ) ) != (int) bufsz ) {
		INV_PERROR( "Error in writing inventory record :" );
		if ( dolock )
			INVLOCK( fd, LOCK_UN );
		return -1;
	}

	if ( dolock )
		INVLOCK( fd, LOCK_UN );
	return nwritten;
}







/*----------------------------------------------------------------------*/
/* get_headerinfo                                                       */
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



/*----------------------------------------------------------------------*/
/* get_lastheader                                                       */
/*----------------------------------------------------------------------*/

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


















