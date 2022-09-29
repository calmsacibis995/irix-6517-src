#include <sys/types.h>
#include <sys/sema.h>

/* ARGSUSED */
int
#if defined(IP30) || defined(IP27)
srvVidTextureXfer(void *vtx, void *ptr)
#else
mgvVidTextureXfer(void *vtx, void *ptr)
#endif
{
	return(-1);	
}
