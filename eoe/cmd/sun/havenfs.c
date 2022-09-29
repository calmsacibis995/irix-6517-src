/*
 * Exit 0 if the running kernel supports nfs, non-zero otherwise.
 *
 * $Revision: 1.7 $
 */
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/capability.h>

static void
havent_nfs(void)
{
	exit(-1);
}

int
main(void)
{
	cap_t ocap;
	cap_value_t cap_mount_mgt = CAP_MOUNT_MGT;

	(void) signal(SIGSYS, havent_nfs);
	ocap = cap_acquire(1, &cap_mount_mgt);
	if (exportfs("/dev/null", 0) < 0 && errno == ENOPKG) {
		cap_surrender(ocap);
		havent_nfs();
	}
	cap_surrender(ocap);
	return(0);
}
