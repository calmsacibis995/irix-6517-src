#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/src/RCS/sysconfig.c,v 1.1 1999/05/08 03:19:00 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>

#include <klib.h>
#include <configmon.h>

/*
 * alloc_sysconfig()
 */
sysconfig_t *
alloc_sysconfig(int flags)
{
	sysconfig_t *scp;

	scp = (sysconfig_t *)kl_alloc_block(sizeof(sysconfig_t), ALLOCFLG(flags));
	scp->hwconfig = kl_alloc_hwconfig(flags);
	scp->swconfig = alloc_swconfig(flags);
	scp->flags = flags;
	return(scp);
}

/*
 * free_sysconfig()
 */
void
free_sysconfig(sysconfig_t *scp)
{
	if (scp) {
		if (scp->hwconfig) {
			kl_free_hwconfig(scp->hwconfig);
		}
		if (scp->swconfig) {
			free_swconfig(scp->swconfig);
		}
		kl_free_block(scp);
	}
}

/*
 * update_sysconfig()
 */
void
update_sysconfig(sysconfig_t *scp)
{
    kl_update_hwconfig(scp->hwconfig);
}
