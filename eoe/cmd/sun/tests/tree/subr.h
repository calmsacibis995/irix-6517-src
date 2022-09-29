extern int mtestdir( char * );
extern int testdir( char * );
extern int error();
extern int checktree( int, int, int, char *, char *, int *, int *, int *, int );
extern int rmdirtree( int, int, int, int *, int *, int );
extern int dirtree( int, int, int, int, int, int, int, char *, char *, int *,
	int * );
extern int get_dirinfo( int, char *, char *, int *, int *, int *, int * );
extern int build_dirinfo( int, char *, char *, int, int, int, int );
extern int get_level( char * );
extern int check_file( char * );
extern int build_file( char *, int );
extern int make_fill_pattern( char *, char *, int );
extern int write_buffer( int, char *, int );
extern void initialize( void );
