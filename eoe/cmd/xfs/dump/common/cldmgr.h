#ifndef CLDMGR_H
#define CLDMGR_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/cldmgr.h,v 1.3 1995/07/02 01:47:32 cbullis Exp $"

/* cldmgr.[hc] - manages all child threads
 */

/* cldmgr_init - initializes child management
 * returns FALSE if trouble encountered.
 */
extern bool_t cldmgr_init( void );

/* cldmgr_create - creates a child thread. returns FALSE if trouble
 * encountered
 */
extern bool_t cldmgr_create( void ( * entry )( void *arg1 ),
			     u_intgen_t inh,
			     ix_t streamix,
			     char *descstr,
			     void *arg1 );

/* cldmgr_stop - asks all children to gracefully terminate, at the next
 * convenient point in their normal processing loop.
 */
extern void cldmgr_stop( void );

/* cldmgr_killall - kills all children
 */
extern void cldmgr_killall( void );

/* cldmgr_died - tells the child manager that the child died
 */
extern void cldmgr_died( pid_t pid );

/* cldmgr_stop_requested - returns TRUE if the child should gracefully
 * terminate.
 */
extern bool_t cldmgr_stop_requested( void );

/* cldmgr_pid2streamix - retrieves the stream index. returns -1 if
 * not associated with any stream.
 */
extern intgen_t cldmgr_pid2streamix( pid_t pid );

/* cldmgr_remainingcnt - returns number of children remaining
 */
extern size_t cldmgr_remainingcnt( void );

/* checks if any children serving any other streams are still alive
 */
extern bool_t cldmgr_otherstreamsremain( ix_t streamix );

#endif /* CLDMGR_H */
