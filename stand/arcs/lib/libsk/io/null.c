/*
 * device driver for bit bucket.
 *
 *	reads always return 0 characters.
 *	writes always succeed in writing the amount requested.
 */

#include <arcs/folder.h>
#include <arcs/cfgtree.h>
#include <arcs/eiob.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>

STATUS
_null_strat(COMPONENT *self, IOBLOCK *iob)
{
	if (iob->FunctionCode == FC_READ) {
		bzero(iob->Address,iob->Count);
	}
	else if (iob->FunctionCode == FC_CLOSE)
		free(self);

	return(ESUCCESS);
}

/*ARGSUSED*/
int
nullsetup(cfgnode_t **cfg, struct eiob *eiob, char *args)
{
	cfgnode_t *n;

	if (strcmp(args,"()") != 0)
		return(EINVAL);

	n = (cfgnode_t *)malloc(sizeof(cfgnode_t));
	if (!n) return(ENOMEM);

	bzero(n,sizeof(cfgnode_t));
	n->driver = _null_strat;
	n->comp.Class = PeripheralClass;
	n->comp.Type = OtherPeripheral;
	n->comp.Flags = (IDENTIFIERFLAG)(Input|Output);
	n->comp.Version = SGI_ARCS_VERS;
	n->comp.Revision = SGI_ARCS_REV;

	*cfg = n;

	return(0);
}
