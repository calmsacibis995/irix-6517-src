#ifndef PATH_H
#define PATH_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/path.h,v 1.2 1994/09/26 21:37:08 cbullis Exp $"

/* various pathname helpers
 */
extern char * path_reltoabs( char *dir, char *basedir );
extern char *path_normalize( char *path );
extern char * path_diff( char *path, char *base );
extern int path_beginswith( char *path, char *base );

#endif /* PATH_H */
