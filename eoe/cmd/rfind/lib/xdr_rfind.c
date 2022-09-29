#include <rpc/rpc.h>
#include "rfind.h"

bool_t
xdr_rfind(XDR *xdr, struct rfindhdr *rfh)
{
	if (!xdr_u_int (xdr, &rfh->sz))
		return 0;
	if (!xdr_int (xdr, &rfh->fd))
		return 0;
	if (!xdr_opaque (xdr, rfh->bufp, rfh->sz))
		return 0;
	return 1;
}
