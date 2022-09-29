#ifndef _SYS_PTHREAD_H_
#define _SYS_PTHREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Primitive system data types
 */
typedef unsigned int 		pthread_t;		/* pthread id */

#ifndef _PTHREAD_EXECUTIVE
typedef struct { long __D[5]; } pthread_attr_t;		/* pthread attributes */
typedef struct { long __D[8]; } pthread_mutex_t;	/* mutex data */
typedef struct { long __D[2]; } pthread_mutexattr_t;	/* mutex attributes */
typedef struct { long __D[8]; } pthread_cond_t;		/* condvar data */
typedef struct { long __D[2]; } pthread_condattr_t;	/* condvar attributes */
typedef struct { long __D[16]; } pthread_rwlock_t;	/* rwlock data */
typedef struct { long __D[4]; } pthread_rwlockattr_t;	/* rwlock attributes */
typedef int			pthread_key_t;		/* thread data key */
typedef int			pthread_once_t;		/* package init */
#endif /* !_PTHREAD_EXECUTIVE */


#ifdef __cplusplus
}
#endif

#endif /* !_SYS_PTHREAD_H_ */
