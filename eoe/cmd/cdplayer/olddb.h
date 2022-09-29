#ifndef OLDDB_H
#define OLDDB_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	database.h
 *
 *	Description:
 *		Header file for database.c
 *
 *	History:
 *      rogerc      11/21/90    Created
 */

const	char	 *odb_get_dflt_file( void );
const	char	 *odb_get_id( CDPLAYER *cdplayer, CDSTATUS *status );

const	char	 *odb_first_disc( const char *file, void **state );
const	char	 *odb_next_disc( void **state );
	void	  odb_done( void **state );

	int	  odb_init( const char *file );
	int	  odb_save( const char *file );
	void	  odb_terminate( const char *file, int autoSave );

const	char	 *odb_get_track_name( const char *cd_id, int track );
	int	  odb_get_track_names( const char *cd_id, 
						const char **track_names,
						int num_tracks );
	int	  odb_free_track_names( char **track_names, int num_tracks );
	int	  odb_put_track_names(const char *cd_id, char **track_names, 
							int num_tracks);

	int	 odb_get_converted( const char *cd_id );
	void	 odb_put_converted( const char *cd_id, int converted );

const	char	*odb_get_title( const char *cd_id );
const	char	*odb_get_artist( const char *cd_id );
const	char	*odb_get_program_name( const char *cd_id, int num );
	CDPROGRAM *odb_get_program( const char *cd_id, int num );
	int	 odb_get_program_count( const char *cd_id );

	int	 odb_put_title( const char *cd_id, const char *title );
	int	 odb_put_artist( const char *cd_id, const char *artist );
	int	 odb_put_program( const char *cd_id, CDPROGRAM *prog, int num );
	int	 odb_put_program_count( const char *cd_id, int num );

#ifdef __cplusplus
}
#endif
#endif /* OLDDB_H */
