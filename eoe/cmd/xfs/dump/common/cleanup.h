#ifndef CLEANUP_H
#define CLEANUP_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/cleanup.h,v 1.4 1994/10/25 22:44:22 tap Exp $"

/* cleanup.[hc] - allows registration of callbacks to be invoked
 * on exit
 */

typedef void cleanup_t;

extern void cleanup_init( void );

extern cleanup_t *cleanup_register( void ( *funcp )( void *arg1, void *arg2 ),
				    void *arg1,
				    void *arg2 );

extern cleanup_t *cleanup_register_early( 
				    void ( *funcp )( void *arg1, void *arg2 ),
				    void *arg1,
				    void *arg2 );

extern void cleanup_cancel( cleanup_t *cleanupp );

extern void cleanup( void );
extern void cleanup_early( void );

#endif /* CLEANUP_H */
