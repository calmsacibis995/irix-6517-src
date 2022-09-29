/* lk_test RPC interface definitions */

#ifdef RPC_HDR
%#include <sys/param.h>
%#if SVR4
%#include <sys/netconfig.h>
%#endif /* SVR4 */
#endif

/*
 * Perform file and record locking tests
 */

typedef string pathstr<MAXPATHLEN>;

enum locktype {
	LOCK_SHARED,
	LOCK_EXCLUSIVE
};

enum lktest_stats {
	FCNTL_SUCCESS,
	FCNTL_SYSERROR
};

struct lockdesc {
	long			ld_offset;			/* lock region start offset */
	long			ld_len;				/* lock region length */
	int				ld_type;			/* lock type */
	int				ld_pid;				/* pid for lock holder */
	int				ld_sysid;			/* sysid for lock hoder */
};

struct fcntlargs {
	pathstr			fa_name;
	int				fa_cmd;
	struct lockdesc	fa_lock;
};

union fcntlreply switch (lktest_stats stat) {
	case FCNTL_SUCCESS:
		struct lockdesc lock;
	case FCNTL_SYSERROR:
		int errno;
	default:
		void;
};

struct flockargs {
	pathstr		fa_name;
	int			fa_op;
};

struct lockfargs {
	pathstr		la_name;
	int			la_func;
	long		la_offset;
	int			la_size;
};

struct servopts {
	int		so_verbose;
	int		so_altverify;
	pathstr	so_directory;
};

struct verifyargs {
	pathstr			va_name;
	struct lockdesc	va_lock;
};

program LOCKTESTPROG {
	version LKTESTVERS_ONE {
		/*
		 * set the server options
		 */
		int
		LKTESTPROC_SERVOPTS(struct servopts) = 1;

		/*
		 * fcntl request
		 */
		fcntlreply
		LKTESTPROC_FCNTL(struct fcntlargs) = 2;

		/*
		 * lockf request
		 */
		int
		LKTESTPROC_LOCKF(struct lockfargs) = 3;

		/*
		 * flock request
		 */
		int
		LKTESTPROC_FLOCK(struct flockargs) = 4;

		/*
		 * check for any held locks
		 */
		bool
		LKTESTPROC_HELD(pathstr) = 5;

		/*
		 * verification
		 */
		bool
		LKTESTPROC_VERIFY(struct verifyargs) = 6;

		/*
		 * reset the server, closing all files and thus removing all locks
		 */
		int
		LKTESTPROC_RESET(void) = 7;
	} = 1;
} = 800001;
