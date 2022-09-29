#include "RemoteClient.h"

#include <assert.h>

RemoteClient::RemoteClient(int fd, in_addr addr)
: TCP_Client(fd), host(addr)
{
    int write_me = 0; assert(write_me);
}

RemoteClient::~RemoteClient()
{ }
