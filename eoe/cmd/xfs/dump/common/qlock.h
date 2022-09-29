#ifndef QLOCK_H
#define QLOCK_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/qlock.h,v 1.2 1995/07/29 03:40:04 cbullis Exp $"

/* qlock - quick locks abstraction
 *
 * threads may allocate quick locks using qlock_alloc, and free them with
 * qlock_free. the abstraction is initialized with qlock_init. the underlying
 * mechanism is the IRIX us lock primitive. in order to use this, a temporary
 * shared arena is created in /tmp. this will be automatically unlinked
 * when the last thread exits.
 *
 * deadlock detection is accomplished by giving an ordinal number to each
 * lock allocated, and record all locks held by each thread. locks may not
 * be acquired out of order. that is, subsequently acquired locks must have
 * a lower ordinal than all locks currently held. for convenience, the ordinals
 * of all locks to be allocated will be defined in this file.
 *
 * ADDITION: added counting semaphores. simpler to do here since same
 * shared arena can be used.
 */

#define QLOCK_ORD_CRIT	0
	/* ordinal for global critical region lock
	 */
#define QLOCK_ORD_WIN	1
	/* ordinal for win abstraction critical regions
	 */
#define QLOCK_ORD_PI	2
	/* ordinal for persistent inventory abstraction critical regions
	 */
#define QLOCK_ORD_MLOG	3
	/* ordinal for mlog lock
	 */

typedef void *qlockh_t;
#define QLOCKH_NULL	0
	/* opaque handle
	 */

extern bool_t qlock_init( bool_t miniroot );
	/* called by main to initialize abstraction. returns FALSE if
	 * utility should abort.
	 */

extern bool_t qlock_thrdinit( void );
	/* called by each thread to prepare it for participation
	 */

extern qlockh_t qlock_alloc( ix_t ord );
	/* allocates a qlock with the specified ordinal. returns
	 * NULL if lock can't be allocated.
	 */
extern void qlock_lock( qlockh_t qlockh );
	/* acquires the specified lock.
	 */
extern void qlock_unlock( qlockh_t qlockh );
	/* releases the specified lock.
	 */

typedef void *qsemh_t;
#define QSEMH_NULL	0
	/* opaque handle
	 */

extern qsemh_t qsem_alloc( size_t cnt );
	/* allocates a counting semaphore initialized to the specified
	 * count. returns a qsem handle
	 */
extern void qsem_free( qsemh_t qsemh );
	/* frees the counting semaphore
	 */
extern void qsemP( qsemh_t qsemh );
	/* "P" (decrement) op
	 */
extern void qsemV( qsemh_t qsemh );
	/* "V" (increment) op
	 */
extern bool_t qsemPwouldblock( qsemh_t qsemh );
	/* returns true if a qsemP op would block
	 */
extern size_t qsemPavail( qsemh_t qsemh );
	/* number of resources available
	 */
extern size_t qsemPblocked( qsemh_t qsemh );
	/* number of threads currently blocked on this semaphore
	 */

typedef void *qbarrierh_t;
#define QBARRIERH_NULL	0
	/* opaque handle
	 */
extern qbarrierh_t qbarrier_alloc( void );
	/* allocates a rendezvous barrier
	 */
extern void qbarrier( qbarrierh_t barrierh, size_t thrdcnt );
	/* causes thrdcnt threads to rendezvous
	 */

#endif /* QLOCK_H */
