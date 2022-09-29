#ifndef RPC_TCP_Connector_included
#define RPC_TCP_Connector_included

#include <sys/types.h>
#include <netinet/in.h>

#include "Boolean.h"

//  An RPC_TCP_Connector is an object that connects to a server.
//  Specifically, a service registered with portmapper that uses
//  TCP/IP.
//
//  The destination host, program, and version are specified when the
//  R.T.Connector is created.
//
//  An R.T.Connector, when activated, tries repeatedly to connect,
//  first to portmapper, then to the server itself.  It keeps trying
//  until it succeeds, it is deactivated, or it is destroyed.  When it
//  succeeds, it calls its ConnectHandler.
//
//  An R.T.Connector is inactive when it's created.
//
//  The R.T.Connector uses an exponentially increasing timeout.  I.e.,
//  if its first attempt fails, it waits one second and tries again.
//  If the second attempt fails, it waits two seconds.  Then it waits
//  four seconds.  Then eight.  Et cetera, up to a maximum of 1024
//  seconds (~17 minutes).

class RPC_TCP_Connector {

    typedef unsigned long ulong;
    typedef unsigned short ushort;

public:

    typedef void (*ConnectHandler)(int fd, void *closure);

    RPC_TCP_Connector(ulong program, ulong version, ulong host,
		      ConnectHandler, void *closure);
    ~RPC_TCP_Connector();

    Boolean active()			{ return state != IDLE; }
    void activate();
    void deactivate();

private:

    enum { PMAP_TIMEOUT = 60 };		// seconds
    enum { INITIAL_RETRY_INTERVAL = 1, MAX_RETRY_INTERVAL = 1024 }; // seconds
    enum State { IDLE, PMAPPING, CONNECTING, PAUSING };

    //  Instance Variables

    State state;
    int sockfd;
    sockaddr_in address;
    ulong program;
    ulong version;
    int retry_interval;
    const ConnectHandler connect_handler;
    void *closure;

    //  Private Instance Methods

    void try_to_connect();
    void try_again();

    //  Class Methods

    static void retry_task(void *closure);
    static void write_handler(int fd, void *closure);

};

#endif /* !RPCTCPConnector_included */
