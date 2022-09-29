#include <rpc/rpc.h>
#include <netdb.h>

struct protoent * getprotobyname(const char *proto) {
	static struct protoent p;

	if (strcmp ("tcp", proto) != 0)
		return NULL;
	p.p_proto = IPPROTO_TCP;
	return &p;
}
