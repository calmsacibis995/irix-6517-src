#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/inventory/RCS/inv_fstab.c,v 1.13 1997/06/05 23:25:40 prasadb Exp $"

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
#include "mlog.h"
#include "inv_priv.h"





/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/


intgen_t
fstab_getall( invt_fstab_t **arr, invt_counter_t **cnt, int *numfs,
	      inv_oflag_t forwhat )
{
	int fd;

	fd = open ( INV_FSTAB, INV_OFLAG(forwhat) );
	
	if ( fd < 0 ) 
		return -1; /* if ENOENT, fstab_put_entry will CREAT */
	
	INVLOCK( fd, LOCK_EX );
	if (( *numfs = GET_ALLHDRS_N_CNTS_NOLOCK( fd, (void**) arr, 
						 (void **)cnt, 	
						 sizeof( invt_fstab_t ), 
						 sizeof( invt_counter_t ) )
	     ) < 0 ) {
		mlog( MLOG_NORMAL | MLOG_INV, "INV: couldn't get fstab headers\n");
	}
#ifdef INVT_DEBUG	
	mlog( MLOG_NITTY | MLOG_INV, "INV: number of filesystems in fstab %d\n", 
	      *numfs );
#endif
	/* fstab is left locked EX on exit. The caller takes does
	   the unlocking */
	return fd;
}
		

/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/* caller takes the responsibility of calling this only when the FSTAB_ */
/* UPDATED flag in the inv_idbtoken is off, and also of updating the */
/* flag upon successful return from put_fstab_entry.                    */
/*----------------------------------------------------------------------*/


intgen_t
fstab_put_entry( uuid_t *fsidp, char *mntpt, char *dev, inv_oflag_t forwhat )
{
	int numfs, i, fd;
	invt_counter_t *cnt;
	invt_fstab_t *arr;
	int rval = 1;

	ASSERT( forwhat != INV_SEARCH_ONLY );

	/* fd is locked on succesful return */
	fd = fstab_getall( &arr, &cnt, &numfs, forwhat );
	if ( fd < 0 ) {
		if ( errno != ENOENT ) {
			return -1;
		}
		if ((fd = open( INV_FSTAB, INV_OFLAG(forwhat) | O_CREAT ))
		    < 0 ) {
			INV_PERROR ( INV_FSTAB );
			return -1;
		}
		
		INVLOCK( fd, LOCK_EX );
		fchmod( fd, INV_PERMS );
		
		cnt = (invt_counter_t *) malloc( sizeof ( invt_counter_t ) );

		cnt->ic_maxnum = -1;
		cnt->ic_curnum = 0;
		cnt->ic_vernum = INV_VERSION;

	} else if ( numfs > 0 ) {
		uint_t stat;
		
		for (i = 0; i < numfs; i++) {
		    if ( uuid_compare( fsidp, &arr[ i ].ft_uuid, &stat ) == 0 ) {
			
/*			if ( ( STREQL( arr[i].ft_mountpt, mntpt ) ) &&
			    ( STREQL( arr[i].ft_devpath, dev ) ) ) 
*/
				free ( arr );
				free ( cnt );
				close( fd );
				return 1;
	     
		}
	    }
	    /* entry not found. just follow thru to create a new one */
	    free ( arr );
	}

	/* make a new fstab entry and insert it at the end. the table
	   is locked EXclusively at this point */
	{
		invt_fstab_t ent;
		off64_t hoff;

		memcpy( &ent.ft_uuid, fsidp, sizeof( uuid_t ) );
		strcpy( ent.ft_mountpt, mntpt );
		strcpy( ent.ft_devpath, dev );
		
		/* increase the number of entries first */
#ifdef INVT_DEBUG
		mlog( MLOG_NITTY | MLOG_INV,"INV: putting new fstab entry for %s ....\n",
		      mntpt);
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
fstab_get_fname( void *pred, 
		 char *fname, 
		 inv_predicate_t bywhat,
		 inv_oflag_t forwhat)
{
	uuid_t *uuidp = 0;
	char *uuidstr;
	uint_t stat;
	invt_fstab_t *arr;


	if (bywhat != INV_BY_UUID) {
		int numfs, i, fd;
		invt_counter_t *cnt;

		/* on sucessful return fd is locked */
		fd = fstab_getall( &arr, &cnt, &numfs, forwhat );
		if ( fd < 0 )
			return -1;
		if ( numfs <= 0 ) {
			mlog( MLOG_NORMAL | MLOG_INV, "INV: No recorded filesystems in"
			      " inventory's fstab.\n" );
			return -1;
		}
		INVLOCK( fd, LOCK_UN );
		close ( fd );
		free ( cnt ); /* we dont need it */

		/* first get hold of the uuid for this mount point/device */

		for (i = 0; i < numfs; i++) {
			if ( ( bywhat == INV_BY_MOUNTPT && 
			     ( STREQL( arr[i].ft_mountpt, pred ) )) ||
			     ( bywhat == INV_BY_DEVPATH &&
			     ( STREQL( arr[i].ft_devpath, pred ) )) ) {

				uuidp = &arr[i].ft_uuid;
				break;
			}
		}
#ifdef INVT_DEBUG				
		if (! uuidp )
			mlog( MLOG_NORMAL | MLOG_INV,"INV: get_fname: unable to find %s"
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
	

void
fstab_DEBUG_print( invt_fstab_t *arr, int num )
{
	int i;
	char *str = 0;
	uint_t stat;

	mlog( MLOG_NORMAL | MLOG_INV, "\n\n--------- fstab ------------\n" );
	for ( i = 0; i < num; i++ ) {
		printf( "Mount\t%s\n", arr[i].ft_mountpt );
		printf( "Dev\t%s\n", arr[i].ft_devpath );
		uuid_to_string( &arr[i].ft_uuid, &str, &stat );
		printf( "FSid\t%s\n\n", str );
		free(str);
	}
	mlog( MLOG_NORMAL | MLOG_INV, "\n---------========------------\n" );
}
