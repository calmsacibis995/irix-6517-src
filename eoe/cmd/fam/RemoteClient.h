#ifndef RemoteClient_included
#define RemoteClient_included

#include "TCP_Client.h"
#include <netinet/in.h>

//  RemoteClient is a client that has connected from another machine.
//  Typically, a RemoteClient will be another machine's fam, but it
//  doesn't have to be.
//
//  RemoteClient is not implemented yet; when it is, it should do
//  authentication at startup and again when new requests are
//  received.

class RemoteClient : public TCP_Client {

public:

    RemoteClient(int fd, in_addr host);
    ~RemoteClient();

private:

    in_addr host;

};

#endif /* !RemoteClient_included */
