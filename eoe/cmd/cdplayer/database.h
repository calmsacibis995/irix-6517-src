#ifndef DATABASE_H
#define DATABASE_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef	struct _CDData	CDData;

const	char	 *db_convert_old_id(const char *old_id);
const	char	 *db_old_id_to_TOC( const char *old_id);

const	char	 *db_get_dflt_dir( void );
const	char	 *db_get_id( CDPLAYER *cdplayer, CDSTATUS *status );
const	char	 *db_get_TOC( CDPLAYER *cdplayer, CDSTATUS *status );
	CDData	 *db_init_from_cd( CDPLAYER *, CDSTATUS *);

	int	  db_init_pkg( const char *path, const char *write_dir );
	void	  db_set_path( const char *path );
	void	  db_set_write_dir( const char *dir );

	CDData	 *db_init( const char *cd_id, const char *TOC );
	void	  db_save( CDData *);
	void	  db_close( CDData *, int autoSave );

	int	  db_get_track_count( CDData * );

const char	*db_get_album_info( CDData *, const char *, const char *);
const char	*db_get_track_info( CDData *, int, const char *, const char *);
const char	*db_get_program_info(CDData *, int, const char *, const char *);

const	char	*db_get_title( CDData * );
const	char	*db_get_artist( CDData * );
const	char	*db_get_album_art( CDData *, int onlyIfExists );
const	char	*db_get_album_thumbnail( CDData *, int onlyIfExists );
const	char	*db_get_notes( CDData *, int onlyIfExists );
const	char	*db_get_track_name( CDData *, int track );
const	char	*db_get_track_artist( CDData *, int num );
const	char	*db_get_track_art( CDData *, int num, int onlyIfExists );
const	char	*db_get_track_thumbnail( CDData *,int num, int onlyIfExists );
const	char	*db_get_track_notes( CDData *, int num, int onlyIfExists );
	int	 db_get_track_names(CDData *, const char **, int max_track);
	CDPROGRAM *db_get_program( CDData *, int num );
const	char	*db_get_program_name( CDData *, int num );
	int	 db_get_program_count( CDData * );

extern	int	 db_put_album_info(CDData *, const char *, const char *);
extern	int	 db_put_track_info(CDData *, int, const char *, const char * );
extern	int	 db_put_program_info(CDData *, int, const char *, const char *);

	int	 db_put_title( CDData *, const char *title );
	int	 db_put_artist( CDData *, const char *artist );
	int	 db_put_album_art( CDData *, const char *cover_file );
	int	 db_put_album_thumbnail( CDData *, const char *nail_file );
	int	 db_put_notes( CDData *, const char *notes_file );
	int	 db_put_track_name(CDData *, int num, const char *name );
	int	 db_put_track_artist(CDData *, int num, const char *name );
	int	 db_put_track_art(CDData *, int num, const char *art_file);
	int	 db_put_track_thumbnail( CDData *, int, const char *nail_file );
	int	 db_put_track_notes( CDData *, int num,
						const char *notes_file );
	int	 db_put_track_names( CDData *, const char **, int );
	int	 db_put_program( CDData *, CDPROGRAM *program, int num );
	int	 db_put_program_count( CDData *, int num );

	CDData	*db_first_disc( const char *dir, void **state );
	CDData	*db_next_disc( void **state );
	void	 db_done( void **state );

#define	DB_NEED_DIR	(1<<0)
#define	DB_NEED_CONVERT	(1<<1)

#ifdef __cplusplus
}
#endif

#endif /* DATABASE_H */
