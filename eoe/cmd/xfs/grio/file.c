#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/file.c,v 1.12 1997/02/25 18:11:35 kanoj Exp $"

/*
 * file.c
 *	grio utility
 *
 *
 */

#include "griostat.h"

int
show_streams_with_file_match( dev_t fsdev, gr_ino_t ino)
{
	int                     i, first = 1, count = 1;
	char                    *uuidstr;
	char                    username[STR_LEN], procstr[STR_LEN];
	uint_t                  status;
	grio_stream_stat_t      grios[MAX_STREAM_STAT_COUNT];

	bzero( grios, sizeof(grio_stream_stat_t)* MAX_STREAM_STAT_COUNT);

	if ( syssgi(SGI_GRIO, GRIO_GET_ALL_STREAMS, &grios) ) {
		exit( -1);
	}


	for (i = 0 ; i < MAX_STREAM_STAT_COUNT; i++) {
		bzero( procstr, STR_LEN );
		if ( uuid_is_nil( &(grios[i].stream_id), &status ) ) {
			;
		} else {
			first = 0;
			if  (	( grios[i].fs_dev == fsdev ) &&
				( grios[i].ino == ino ) ) 	{

				uuid_to_string( &(grios[i].stream_id),
					&uuidstr, &status);

				find_proc_name( grios[i].procid, 
					username, procstr );

				printf("stream %3d id: %s, owner pid: %6d\n",
					count,
					uuidstr,
					grios[i].procid);

				printf("current user login: %s, "
				       "process args: %s \n",
					username,
					procstr);

				free( uuidstr );
				printf("\n");
				count++;
			}
		}
	}

	if (first) {
		printf("No active GRIO reservations. \n");
	}

	return( 0 );
}

int
show_file_reservations( device_list_t filelist[], int filecount)
{
	int 		i;
	grio_stats_t	grio_stats;
	struct stat64	statbuf;

	for ( i = 0; i < filecount; i++ ) {

		bzero( &grio_stats, sizeof(grio_stats_t ));

		if ( stat64( filelist[i].name, &statbuf ) == -1 ) {
			/*
			 * stat failed
			 */
			return( -1 );
		}

		if ( send_file_resvs_command( 
			statbuf.st_dev, 
			statbuf.st_ino,
			&grio_stats) ) {
			return( -1 );
		} else {
			printf("File %s: \n",filelist[i].name);
			printf("number of GRIO streams with a "
			       "\"PER_FILE_GUAR\" on this file: %d \n",
				grio_stats.gs_count);

			printf("active GRIO streams using this file:\n");
			show_streams_with_file_match( 
				statbuf.st_dev, statbuf.st_ino);
			printf("\n");
		}
	}

	return ( 0 );

}
