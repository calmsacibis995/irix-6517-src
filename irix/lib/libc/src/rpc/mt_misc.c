/*
 *	Define and initialize MT data for libnsl.
 *	The _libnsl_lock_init() function below is the library's .init handler.

#pragma ident	"@(#)mt_misc.c	1.26	97/03/31 SMI"

 */

#ifdef __STDC__
	#pragma weak authnone_lock = _authnone_lock
	#pragma weak callrpc_lock = _callrpc_lock
	#pragma weak clnt_fd_lock = _clnt_fd_lock
	#pragma weak dupreq_lock = _dupreq_lock
	#pragma weak lock_value = _lock_value
	#pragma weak portnum_lock = _portnum_lock
	#pragma weak svc_exit_mutex = _svc_exit_mutex
	#pragma weak svc_fd_lock = _svc_fd_lock
	#pragma weak svc_lock = _svc_lock
	#pragma weak svc_mutex = _svc_mutex
	#pragma weak svc_thr_fdwait = _svc_thr_fdwait
	#pragma weak svc_thr_mutex = _svc_thr_mutex
#endif
#include "synonyms.h"

#include	<rpc/rpc.h>
#include	<sys/time.h>
#include	<stdlib.h>
#include	<pthread.h>
#include	<dlfcn.h>
#include	<mplib.h>

/* protects the services list (svc.c) */
pthread_rwlock_t svc_lock = PTHREAD_RWLOCK_INITIALIZER;	

/* protects svc_fdset and xports[] */
pthread_rwlock_t svc_fd_lock = PTHREAD_RWLOCK_INITIALIZER;	

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP) && !defined(_LIBC_NONSHARED)
static pthread_rwlock_t	*rwlock_table[] = {
	&svc_lock,
	&svc_fd_lock,
};
#endif

/* auth_none.c serialization */
pthread_mutex_t authnone_lock = PTHREAD_MUTEX_INITIALIZER;

/* serializes calls to callrpc */
pthread_mutex_t callrpc_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects client-side fd lock array */
pthread_mutex_t clnt_fd_lock = PTHREAD_MUTEX_INITIALIZER;

/* dupreq variables (svc_udp.c) */
pthread_mutex_t dupreq_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects ``port'' static in bindresvport.c */
pthread_mutex_t portnum_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects thread related variables */
pthread_mutex_t svc_thr_mutex = PTHREAD_MUTEX_INITIALIZER;	

/* protects service handle free lists */
pthread_mutex_t svc_mutex = PTHREAD_MUTEX_INITIALIZER;		

/* used for clean mt exit */
pthread_mutex_t svc_exit_mutex = PTHREAD_MUTEX_INITIALIZER;	

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP) && !defined(_LIBC_NONSHARED)
static pthread_mutex_t	*mutex_table[] = {
	&authnone_lock,
	&callrpc_lock,
	&clnt_fd_lock,
	&dupreq_lock,
	&portnum_lock,
	&svc_thr_mutex,
	&svc_mutex,
	&svc_exit_mutex
};
#endif

/* threads wait on this for work */
pthread_cond_t svc_thr_fdwait = PTHREAD_COND_INITIALIZER;		

int lock_value = 0;

#if _MIPS_SIM == _ABI64
#define LIBPTHREAD "/usr/lib64/libpthread.so"
#else
#define LIBPTHREAD "/usr/lib32/libpthread.so"
#endif

int
_libc_rpc_mt_init()
{
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP) && !defined(_LIBC_NONSHARED)
	int	i;
#endif
	static void *libpthread = NULL;

	lock_value = 0;
	if (libpthread) {
		lock_value = 1;
		return (TRUE);
	}

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP) && !defined(_LIBC_NONSHARED)
	if ((libpthread = dlopen(LIBPTHREAD, RTLD_NOW)) == 0) {
		printf("_libc_rpcmt_init: dlopen failed: %s\n", dlerror());
		return (FALSE);
	}
	if (!MTLIB_ACTIVE()) {
		printf("_libc_rpcmt_init: MTLIB_ACTIVE() is false\n");
		return (FALSE);
	}
	for (i = 0; i < sizeof (mutex_table) / sizeof (mutex_table[0]); i++)
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_INIT, mutex_table[i], NULL) );
	for (i = 0; i < sizeof (rwlock_table) / sizeof (rwlock_table[0]); i++)
		MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_INIT, rwlock_table[i], NULL) );
	MTLIB_INSERT( (MTCTL_PTHREAD_COND_INIT, &svc_thr_fdwait, NULL) );

	lock_value = 1;
	return(TRUE);
#else
	return FALSE;
#endif
}
