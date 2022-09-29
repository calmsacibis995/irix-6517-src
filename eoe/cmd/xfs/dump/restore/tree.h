#ifndef TREE_H
#define TREE_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/tree.h,v 1.7 1996/07/18 21:44:22 ajs Exp $"

/* tree_init - creates a new tree abstraction.
 */
extern bool_t tree_init( char *hkdir,
			 char *dstdir,
			 bool_t toconlypr,
			 bool_t ownerpr,
			 ino64_t rootino,
			 ino64_t firstino,
			 ino64_t lastino,
			 size64_t dircnt,
			 size64_t nondircnt,
			 size64_t vmsz,
			 bool_t fullpr,
			 bool_t restoredmpr );

/* tree_sync - synchronizes with an existing tree abstraction
 */
extern bool_t tree_sync( char *hkdir,
			 char *dstdir,
			 bool_t toconlypr,
			 bool_t fullpr);


/* tree_begindir - begins application of dumped directory to tree.
 * returns handle to dir node. returns by reference the dirattr
 * handle if new. caller must pre-zero (DAH_NULL).
 */
extern nh_t tree_begindir( filehdr_t *fhdrp, dah_t *dahp );

/* tree_addent - adds a directory entry; takes dirh from above call
 */
extern void tree_addent( nh_t dirh,
			 ino64_t ino,
			 size_t gen,
			 char *name,
			 size_t namelen );

/* ends application of dir
 */
extern void tree_enddir( nh_t dirh );

#ifdef TREE_CHK
/* tree_chk - do a sanity check of the tree prior to post-processing and
 * non-dir restoral. returns FALSE if corruption detected.
 */
extern bool_t tree_chk( void );
#endif /* TREE_CHK */

/* tree_marknoref - mark all nodes as no reference, not dumped dirs, and
 * clear all directory attribute handles. done at the beginning
 * of the restoral of a dump session, in order to detect directory entries
 * no longer needed.
 */
extern void tree_marknoref( void );

/* mark all nodes in tree as either selected or unselected, depending on sense
 */
extern void tree_markallsubtree( bool_t sensepr );

extern bool_t tree_subtree_parse( bool_t sensepr, char *path );

extern bool_t tree_post( char *path1, char *path2 );

extern void tree_cb_links( ino64_t ino,
			   u_int32_t biggen,
			   int32_t ctime,
			   int32_t mtime,
			   bool_t ( * funcp )( void *contextp,
					       bool_t linkpr,
					       char *path1,
					       char *path2 ),
			   void *contextp,
			   char *path1,
			   char *path2 );

/* called after all dirs have been restored. adjusts the ref flags,
 * by noting that dirents not refed because their parents were not dumped
 * are virtually reffed if their parents are refed.
 */
extern bool_t tree_adjref( void );

extern bool_t tree_setattr( char *path );
extern bool_t tree_delorph( void );
extern bool_t tree_subtree_inter( void );

#ifdef EXTATTR
extern bool_t tree_extattr( bool_t ( * cbfunc )( char *path, dah_t dah ),
			    char *path );
	/* does a depthwise bottom-up traversal of the tree, calling
	 * the supplied callback for all directories with a non-NULL dirattr
	 * handle. The callback will get called with the directory's pathname
	 * and it dirattr handle. the traversal will be aborted if the
	 * callback returns FALSE. returns FALSE if operator requests
	 * an interrupt.
	 */
#endif /* EXTATTR */

#endif /* TREE_H */
