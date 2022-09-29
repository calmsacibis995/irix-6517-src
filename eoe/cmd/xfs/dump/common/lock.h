#ifndef LOCK_H
#define LOCK_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/lock.h,v 1.1 1995/06/21 22:47:16 cbullis Exp $"

extern bool_t lock_init( void );

extern void lock( void );

extern void unlock( void );

#endif /* LOCK_H */
