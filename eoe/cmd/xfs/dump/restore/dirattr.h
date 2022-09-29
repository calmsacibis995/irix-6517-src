#ifndef DIRATTR_H
#define DIRATTR_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/dirattr.h,v 1.4 1995/10/03 03:06:23 ack Exp $"

/* dah_t - handle to registered directory attributes
 * a special handle is reserved for the caller's convenience,
 * to indicate no directory attributes have been registered.
 */
typedef size32_t dah_t;
#define	DAH_NULL	SIZE32MAX


/* dirattr_init - creates the directory attributes registry.
 * resync indicates if an existing context should be re-opened.
 * returns FALSE if an error encountered. if NOT resync,
 * dircnt hints at number of directories to expect.
 */
extern bool_t dirattr_init( char *housekeepingdir,
			    bool_t resync,
			    u_int64_t dircnt );


/* dirattr_cleanup - removes all traces
 */
extern void dirattr_cleanup( void );


/* dirattr_add - registers a directory's attributes. knows how to interpret
 * the filehdr. returns handle for use with dirattr_get_...().
 */
extern dah_t dirattr_add( filehdr_t *fhdrp );

/* dirattr_update - modifies existing registered attributes
 */
extern void dirattr_update( dah_t dah, filehdr_t *fhdrp );

/* dirattr_del - frees dirattr no longer needed
 */
extern void dirattr_del( dah_t dah );

/* dirattr_get_... - retrieve various attributes
 */
mode_t dirattr_get_mode( dah_t dah );
uid_t dirattr_get_uid( dah_t dah );
gid_t dirattr_get_gid( dah_t dah );
time_t dirattr_get_atime( dah_t dah );
time_t dirattr_get_mtime( dah_t dah );
time_t dirattr_get_ctime( dah_t dah );
u_int32_t dirattr_get_xflags( dah_t dah );
u_int32_t dirattr_get_extsize( dah_t dah );
u_int32_t dirattr_get_dmevmask( dah_t dah );
u_int32_t dirattr_get_dmstate( dah_t dah );

#ifdef EXTATTR
/* dirattr_addextattr - record an extended attribute. second argument is
 * ptr to extattrhdr_t, with extattr name and value appended as
 * described by hdr.
 */
extern void dirattr_addextattr( dah_t dah, extattrhdr_t *ahdrp );

/* dirattr_cb_extattr - calls back for every extended attribute associated with
 * the given dah. stops iteration and returnd FALSE if cbfunc returns FALSE,
 * else returns TRUE.
 */
extern bool_t dirattr_cb_extattr( dah_t dah,
				  bool_t ( * cbfunc )( extattrhdr_t *ahdrp,
						       void *ctxp ),
				  extattrhdr_t *ahdrp,
				  void *ctxp );
#endif /* EXTATTR */

#endif /* DIRATTR_H */
