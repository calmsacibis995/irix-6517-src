#include <sys/types.h>
#include <sys/sesmgr.h>
#include <sys/t6satmp.h>
#include <sys/t6api_private.h>

#ident "$Revision: 1.3 $"

int
satmp_init_reply (int fd, satmp_esi_t esi, int flag, u_int generation)
{
	u_int esi1 = (u_int) ((esi >> 32) & 0xffffffff);
	u_int esi2 = (u_int) (esi & 0xffffffff);

	return ((int) sgi_sesmgr (SESMGR_MAKE_CMD (SESMGR_SATMP_ID, T6SATMP_INIT_REPLY_CMD), fd, esi1, esi2, flag, generation));
}
