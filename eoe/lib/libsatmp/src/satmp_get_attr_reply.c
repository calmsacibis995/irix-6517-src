#include <sys/types.h>
#include <sys/sesmgr.h>
#include <sys/t6satmp.h>
#include <sys/t6api_private.h>

#ident "$Revision: 1.1 $"

int
satmp_get_attr_reply (int fd, const void *buf, size_t bufsize)
{
	return ((int) sgi_sesmgr (SESMGR_MAKE_CMD (SESMGR_SATMP_ID, T6SATMP_GET_ATTR_REPLY_CMD), fd, buf, bufsize));
}
