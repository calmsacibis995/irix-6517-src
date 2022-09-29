#ifndef UTIL_H
#define UTIL_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/rfind/include/RCS/util.h,v 1.1 1995/07/21 16:39:20 sunita Exp $"

/* util.[hc] - generally useful functions
 */

/* write_buf - converts the normal manager write method into something simpler
 *
 * the managers present a somewhat complicated write interface, for performance
 * reasons, which eliminates buffer copying. however, there are times when
 * the data to be written is already buffered, such as when writing out
 * header structures. This function takes care of the details of writing
 * the contents of such a buffer using the manager operators.
 *
 * if bufp is null, writes bufsz zeros.
 */
typedef char * ( * gwbfp_t )( void *contextp, size_t wantedsz, size_t *szp);
typedef intgen_t ( * wfp_t )( void *contextp, char *bufp, size_t bufsz );

extern intgen_t write_buf( char *bufp,
			   size_t bufsz,
			   void *contextp,
			   gwbfp_t get_write_buf_funcp,
			   wfp_t write_funcp );


/* read_buf - converts the normal manager read method into something simpler
 *
 * the managers present a somewhat complicated read interface, for performance
 * reasons, which eliminates buffer copying. however, there are times when
 * the caller wants his buffer to be filled  completely, such as when reading
 * header structures. This function takes care of the details of filling
 * such a buffer using the manager operators.
 *
 * if bufp is null, discards read data.
 *
 * returns number of bytes successfully read. returns by reference the
 * status of the first failure of the read funcp. if no read failures occur,
 * *statp will be zero.
 */
typedef char * ( *rfp_t )( void *contextp, size_t wantedsz, size_t *szp, intgen_t *statp );
typedef void ( * rrbfp_t )( void *contextp, char *bufp, size_t bufsz );

extern intgen_t read_buf( char *bufp,
			  size_t bufsz, 
			  void *contextp,
			  rfp_t read_funcp,
			  rrbfp_t return_read_buf_funcp,
			  intgen_t *statp );


#define min( a, b )	( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define max( a, b )	( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )


/* macro for determining the size of a structure member
 */
#define sizeofmember( t, m )	sizeof( ( ( t * )0 )->m )

/* macro for calculating the offset of a structure member
 */
#define offsetofmember( t, m )	( ( size_t )( char * )&( ( ( t * )0 )->m ))

/* strncpyterm - like strncpy, but guarantees the destination is null-terminated
 */
extern char *strncpyterm( char *s1, char *s2, size_t n );

/* determines if the file specified is contained in an xfs file system
 */
extern bool_t isinxfs( char *path );

/* converts a stat64_t into a xfs_bstat_t. does not fill in fields
 * where info cannot be extracted from stat64_t.
 */
extern void stat64_to_xfsbstat( xfs_bstat_t *xfsstatp, stat64_t *statp );

/* bigstat - efficient file status gatherer. presents an iterative
 * callback interface, invoking the caller's callback for each in-use
 * inode. the caller can specify the first ino, and can limit the callbacks
 * to just dirs, just non-dirs, or both. if the callback returns non-zero,
 * aborts the iteration and sets stat to the callback's return; otherwise,
 * stat is set to zero. return value non-zero if the system call fails.
 */
#define BIGSTAT_ITER_DIR	( 1 << 0 )
#define BIGSTAT_ITER_NONDIR	( 1 << 1 )
#define BIGSTAT_ITER_ALL	( ~0 )

extern intgen_t bigstat_iter( jdm_fshandle_t *fshandlep,
			      intgen_t fsid,
			      intgen_t selector,
			      ino64_t start_ino,
			      intgen_t ( * fp )( void *arg1,
						 jdm_fshandle_t *fshandlep,
						 intgen_t fsfd,
						 xfs_bstat_t *statp ),
			      void * arg1,
			      intgen_t *statp );

extern intgen_t bigstat_one( jdm_fshandle_t *fshandlep,
			     intgen_t fsid,
			     ino64_t ino,
			     xfs_bstat_t *statp );


/* calls the callback for every entry in the directory specified
 * by the stat buffer. supplies the callback with a file system
 * handler and a stat buffer, and the name from the dirent.
 *
 * NOTE: does NOT invoke callback for "." or ".."!
 *
 * if the callback returns non-zero, returns 1 with cbrval set to the
 * callback's return value. if syscall fails, returns -1 with errno set.
 * otherwise returns 0.
 */
extern intgen_t diriter( jdm_fshandle_t *fshandlep,
			 intgen_t fsfd,
			 xfs_bstat_t *statp,
			 intgen_t ( *cbfp )( void *arg1,
					     jdm_fshandle_t *fshandlep,
					     intgen_t fsfd,
					     xfs_bstat_t *statp,
					     char *namep ),
			 void *arg1,
			 intgen_t *cbrvalp );



/* ctimennl - ctime(3C) with newline removed
 */
extern char *ctimennl( const time_t *clockp );


/* macro to copy uuid structures
 */
#define COPY_UUID(uuid1,uuid2) ( bcopy(&(uuid1),&(uuid2),sizeof(uuid_t)) )
#define UUID_COMPARE( uuid1, uuid2 ) uuid_compare( &(uuid1), &(uuid2), &status)

#define COPY_LABEL( lab1, lab2) ( bcopy( lab1, lab2, GLOBAL_HDR_STRING_SZ) )

#endif /* UTIL_H */
