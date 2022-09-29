#ifndef VAR_H
#define VAR_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/var.h,v 1.2 1995/08/22 14:51:28 tap Exp $"

/* var.[ch] - abstraction dealing with /var/xfsdump/
 */

extern void var_create( void );

extern void var_skip( uuid_t *dumped_fsidp, void ( *cb )( ino64_t ino ));

#endif /* VAR_H */
