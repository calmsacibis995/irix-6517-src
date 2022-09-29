/* lk_test RPC interface definitions */

#ifdef RPC_HDR
%#include <sys/param.h>
#endif

/*
 * Perform n-process file and record locking tests
 */

typedef string pathstr<MAXPATHLEN>;

struct servopts {
	int		so_verbose;
	pathstr	so_directory;
};

program NPROCESSPROG {
	version NPROCESSVERS_ONE {
		/*
		 * set the server options
		 */
		int
		NPROCESSPROC_SERVOPTS(struct servopts) = 1;

		/*
		 * run processes
		 */
		int
		NPROCESSPROC_RUNPROCS(int) = 2;

		/*
		 * kill processes
		 */
		int
		NPROCESSPROC_KILLPROCS(void) = 3;

		/*
		 * reset the server
		 */
		int
		NPROCESSPROC_RESET(void) = 4;
	} = 1;
} = 800002;
