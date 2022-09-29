/*
** Purpose: Check POSIX config requirements.
*/

#include <pthread.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#define ChkFatal
#include <Tst.h>


/* ------------------------------------------------------------------ */

TST_BEGIN(buildTest)
{
#	if !defined(_POSIX_VERSION) || _POSIX_VERSION < 199506L
#		error _POSIX_LOGIN_NAME_MAX earlier than 199506L
#	endif

#	if !defined(_POSIX_LOGIN_NAME_MAX) || _POSIX_LOGIN_NAME_MAX != 9
#		error _POSIX_LOGIN_NAME_MAX not 9
#	endif

#	if !defined(_POSIX_THREAD_DESTRUCTOR_ITERATIONS) \
		|| _POSIX_THREAD_DESTRUCTOR_ITERATIONS != 4
#		error _POSIX_THREAD_DESTRUCTOR_ITERATIONS not 4
#	endif

#	if !defined(_POSIX_THREAD_KEYS_MAX) || _POSIX_THREAD_KEYS_MAX != 128
#		error _POSIX_THREAD_KEYS_MAX not 128
#	endif

#	if !defined(_POSIX_THREAD_THREADS_MAX) 
		|| _POSIX_THREAD_THREADS_MAX != 64
#		error _POSIX_THREAD_THREADS_MAX not 64
#	endif

#	if !defined(_POSIX_TTY_NAME_MAX) || _POSIX_TTY_NAME_MAX != 9
#		error _POSIX_TTY_NAME_MAX not 9
#	endif


#	if !defined(_POSIX_THREADS)
#		error _POSIX_THREADS not defined
#	endif

#	if !defined(_POSIX_THREAD_SAFE_FUNCTIONS)
#		error _POSIX_THREAD_SAFE_FUNCTIONS not defined
#	endif

#	if !defined(_POSIX_THREAD_PRIO_INHERIT)
#		error _POSIX_THREAD_PRIO_INHERIT not defined
#	endif

#	if !defined(_POSIX_THREAD_PRIO_PROTECT)
#		error _POSIX_THREAD_PRIO_PROTECT not defined
#	endif

#	if !defined(_POSIX_THREAD_PROCESS_SHARED)
#		error _POSIX_THREAD_PROCESS_SHARED not defined
#	endif


	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(runTest)
{

#define	SYSCONF(name, op, val)	\
	ChkExp( sysconf(name) op val, \
		("Bad value %ld:" #name " " #op " " #val "\n", sysconf(name)));

	SYSCONF( _SC_GETGR_R_SIZE_MAX, !=, 0 );
	SYSCONF( _SC_GETPW_R_SIZE_MAX, !=, 0 );
	SYSCONF( _SC_LOGIN_NAME_MAX, >=, 9 );
	SYSCONF( _SC_TTY_NAME_MAX, >=, 9 );

	SYSCONF( _SC_THREADS, ==, 1 );
	SYSCONF( _SC_THREAD_SAFE_FUNCTIONS, ==, 1 );
	SYSCONF( _SC_THREAD_PRIORITY_SCHEDULING, ==, 1 );
	SYSCONF( _SC_THREAD_ATTR_STACKADDR, ==, 1 );
	SYSCONF( _SC_THREAD_ATTR_STACKSIZE, ==, 1 );

	SYSCONF( _SC_THREAD_DESTRUCTOR_ITERATIONS, >=,
		_POSIX_THREAD_DESTRUCTOR_ITERATIONS );
	SYSCONF( _SC_THREAD_KEYS_MAX, >=, _POSIX_THREAD_KEYS_MAX );
	SYSCONF( _SC_THREAD_STACK_MIN, >=, 0 );
	SYSCONF( _SC_THREAD_THREADS_MAX, >=, _POSIX_THREAD_THREADS_MAX );

	SYSCONF( _SC_THREAD_PRIO_INHERIT, ==, 1 );
	SYSCONF( _SC_THREAD_PRIO_PROTECT, ==, 1 );

	SYSCONF( _SC_THREAD_PROCESS_SHARED, ==, 1 );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Config parameters" )

	TST( buildTest,
		"Compile time checks", "Check symbol values" ),

	TST( runTest,
		"syscconf() checks", 0 ),

TST_FINISH
