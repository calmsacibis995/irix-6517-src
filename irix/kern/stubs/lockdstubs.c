#include "sys/types.h"
#include "sys/errno.h"

int lockd_blocking_thresh = 0;
int lockd_initial_count = 0;
int in_grace_period = 1;
int lockd_grace_period = 0;
int lock_share_requests = 0;
int nlm_granted_timeout = 0;

int
nfs_notify()
{
	return(ENOPKG);
}

int
lockd_sys()
{
	return(ENOPKG);
}
