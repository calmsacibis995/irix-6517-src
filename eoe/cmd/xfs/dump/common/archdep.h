#ifndef ARCHDEP_H
#define ARCHDEP_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/archdep.h,v 1.7 1994/08/25 04:17:36 cbullis Exp $"

/* archdep.[hc] - machine, fs, and os - dependent interfaces
 *
 * lumped here for ease of porting and updating
 */

/* fundamental page size - probably should not be hardwired, but
 * for now we will
 */
#define PGSZLOG2	12
#define PGSZ		( 1 << PGSZLOG2 )
#define PGMASK		( PGSZ - 1 )


/* fshandle_t - I just made this up, as a placeholder until
 * the real interfaces ae defined. I do know that the xfs_bigstat_t
 * is not succifient or the following interfaces; the fs must
 * be identified as well.
 */
typedef intgen_t fshandle_t;
#define FSHANDLE_NULL	( -1 )


/* archdep_getfshandle - given a pathname to a mount point,
 * returns a fshandle_t. as a side-effect, initialize simulation environment
 * for the following functions. need to map a xfs_bstat_t to a pathname,
 * so the file can be examined. this uses a lot of memory.
 */
extern fshandle_t archdep_getfshandle( char *mntpnt );


/* getdents equivalent: loads the caller's buffer with raw directory entries.
 * each entry begins with the getdententry_t described below (12 bytes). it
 * is followed by ge_namelen + 1 bytes. these contain the entry name string,
 * NULL-terminated. hence each entry occupies sizeof( archdep_getdents_entry_t )
 * + ge_namelen + 1 bytes. only complete directory entries will be supplied.
 * returns number of bytes loaded into the caller's buffer, zero
 * if done, -1 if buffer to small to fit the next dirent.
 */
struct archdep_getdents_context {
	DIR *gd_dirp;
	bool_t gd_cached;
	char gd_cachedname[ 2 * sizeof( dirent_t ) + 256 ];
	size_t gd_cachedsz;
	ino64_t gd_cachedino;
};

typedef struct archdep_getdents_context archdep_getdents_context_t; 

extern archdep_getdents_context_t *
    archdep_getdents_context_alloc( fshandle_t fhhandle, xfs_bstat_t *statp );

/* archdep_getdents_entry_t - dirent header
 * archdep_getdents( ) fills a caller-supplied buffer with variable-length
 * directory entry descriptors. Each descriptor begins with this header.
 * the overall length of the descriptor is indicated by ge_sz; this is
 * guaranteed to be a multiple of 8 bytes. the ge_name field
 * will be a NULL-terminated string, the dirent name.
 */
#define ARCHDEP_GETDENTS_ALIGN	8

struct archdep_getdents_entry {
	ino64_t ge_ino;
	u_int32_t ge_sz;
	char ge_name[ 4 ];
};

typedef struct archdep_getdents_entry archdep_getdents_entry_t;

/* fills the caller's buffer with directory entry records. returns
 * number of bytes used. if buffer too small to fit the next record,
 * return's -1. returns 0 if the directory is depleted. subsequent calls
 * after the directory is first depleted will also return 0.
 *
 * NOTE: skips "." and "..".
 */
extern intgen_t archdep_getdents( archdep_getdents_context_t *ctxp,
				  char *buf,
				  size_t bufsz );

extern void archdep_getdents_context_free( archdep_getdents_context_t *ctxp );


/* iterative interface for retrieving a file's extent map
 */
struct archdep_getbmap_context {
	intgen_t gb_fd;
};

typedef struct archdep_getbmap_context archdep_getbmap_context_t;

extern archdep_getbmap_context_t *
       archdep_getbmap_context_alloc( fshandle_t fshandle, xfs_bstat_t *statp );

extern intgen_t archdep_getbmap( archdep_getbmap_context_t *ctxp,
				 getbmap_t *bmapp );

extern void archdep_getbmap_context_free( archdep_getbmap_context_t *ctxp );


/* quietly reads the file indicated by the getbmap context. returns actual bytes
 * read, or -1 if error with errno set.
 */
extern intgen_t archdep_read( archdep_getbmap_context_t *ctxp,
			      off64_t off,
			      char *bufp,
			      size_t sz );

/* quietly reads the symlink indicated by the xfs_bstat_t. returns actual bytes
 * read, or -1 if error with errno set.
 */
extern intgen_t archdep_readlink( fshandle_t fshandle,
				  xfs_bstat_t *statp,
				  char *bufp,
				  size_t bufsz );

#endif /* ARCHDEP_H */
