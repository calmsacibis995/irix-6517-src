#ifndef NAMREG_H
#define NAMREG_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/namreg.h,v 1.3 1995/07/20 22:59:37 cbullis Exp $"

/* namreg.[hc] - directory entry registry
 *
 * provides unique directory entry ID's and storage for the entry
 * name.
 */

/* nrh_t - handle to a registered name
 */
typedef size32_t nrh_t;
#define NRH_NULL	SIZE32MAX


/* namreg_init - creates the name registry. resync is TRUE if the
 * registry should already exist, and we are resynchronizing.
 * if NOT resync, inocnt hints at how many names will be held
 */
extern bool_t namreg_init( char *housekeepingdir,
			   bool_t resync,
			   u_int64_t inocnt );


/* namreg_add - registers a name. name does not need to be null-terminated.
 * returns handle for use with namreg_get().
 */
extern nrh_t namreg_add( char *name, size_t namelen );


/* namreg_del - remove a name from the registry
 */
extern void namreg_del( nrh_t nrh );


/* namreg_get - retrieves the name identified by the index.
 * fills the buffer with the null-terminated name from the registry.
 * returns the strlen() of the name. returns -1 if the buffer is too
 * small to fit the null-terminated name. return -2 if the name
 * not in the registry. return -3 if a system call fails.
 */
extern intgen_t namreg_get( nrh_t nrh, char *bufp, size_t bufsz );

#endif /* NAMREG_H */
