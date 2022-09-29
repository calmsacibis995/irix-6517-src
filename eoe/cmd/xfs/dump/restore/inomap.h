#ifndef INOMAP_H
#define INOMAP_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/inomap.h,v 1.3 1995/07/20 22:59:35 cbullis Exp $"

/* inomap.[hc] - inode map abstraction
 *
 * an inode map describes the inode numbers (inos) in a file system dump.
 * the map identifies which inos are in-use by the fs, which of those are
 * directories, and which are dumped.
 *
 * the map is represented as a list of map segments. a map segment is
 * a 64-bit starting ino and two 64-bit bitmaps. the bitmaps describe
 * the 64 inos beginning with the starting ino. two bits are available
 * for each ino.
 */

/* map state values
 */
#define MAP_INO_UNUSED	0       /* ino not in use by fs */
#define MAP_DIR_NOCHNG	1       /* dir, ino in use by fs, but not dumped */
#define MAP_NDR_NOCHNG	2       /* non-dir, ino in use by fs, but not dumped */
#define MAP_DIR_CHANGE	3       /* dir, changed since last dump */
#define MAP_NDR_CHANGE	4       /* non-dir, changed since last dump */
#define MAP_DIR_SUPPRT	5       /* dir, unchanged but needed for hierarchy */
#define MAP_NDR_NOREST	6       /* was MAP_NDR_CHANGE, but not to be restored */
#define MAP_RESERVED2	7       /* this state currently not used */

extern bool_t inomap_sync_pers( char *hkdir );
extern rv_t inomap_restore_pers( drive_t *drivep,
				 content_inode_hdr_t *scrhdrp,
				 char *hkdir );
extern void inomap_del_pers( char *hkdir );
extern void inomap_sanitize( void );
extern bool_t inomap_rst_needed( ino64_t begino, ino64_t endino );
extern void inomap_rst_add( ino64_t ino );
extern void inomap_rst_del( ino64_t ino );
extern rv_t inomap_discard( drive_t *drivep, content_inode_hdr_t *scrhdrp );
extern void inomap_cbiter( intgen_t mapstatemask,
			   bool_t ( * cbfunc )( void *ctxp, ino64_t ino ),
			   void *ctxp );

#endif /* INOMAP_H */
