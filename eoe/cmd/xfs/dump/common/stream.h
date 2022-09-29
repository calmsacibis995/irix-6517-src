#ifndef STREAM_H
#define STREAM_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/stream.h,v 1.10 1995/06/21 22:52:01 cbullis Exp $"

/* stream.h	definitions pertaining to the dump/restore streams
 */

#define STREAM_MAX	20
	/* maximum number of simultaneous streams.
	 */

/* stream exit codes
 */
#define STREAM_EXIT_SUCCESS	    0 /* stream completed successfully */
#define STREAM_EXIT_STOP	    1 /* thread requests a stop */
#define STREAM_EXIT_ABORT	    2 /* thread requests an abort */
#define STREAM_EXIT_CORE	    3 /* thread requests a core dump */
				    
extern void stream_init( void );

extern void stream_register( pid_t pid, intgen_t streamix );

extern void stream_unregister( pid_t pid );

extern intgen_t stream_getix( pid_t pid );

extern size_t stream_cnt( void );

#endif /* STREAM_H */
