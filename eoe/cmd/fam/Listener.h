#ifndef Listener_included
#define Listener_included

#include "Boolean.h"

//  Listener is an object that listens for connections on a socket.
//  It registers itself with portmapper.
//
//  If a Listener was started_by_inetd, then it listens on the
//  already-open file descriptor 0.  Otherwise, it opens a new socket.
//
//  A Listener creates TCP_Client and RemoteClient objects as
//  connections come in.
//
//  FAMPROG and FAMVERS, the SUN RPC program number and version, are
//  defined here, because I didn't think of a better place to define them.

class Listener {

public:

    enum { RENDEZVOUS_FD = 0 };		// descriptor 0 opened by inetd
    enum { FAMPROG = 391002, FAMVERS = 1 };


    Listener(Boolean started_by_inetd,
	     unsigned long program = FAMPROG, unsigned long version = FAMVERS);
    ~Listener();

private:

    //  Instance Variables

    unsigned long program;
    unsigned long version;
    int rendezvous_fd;
    Boolean started_by_inetd;
    int _ugly_sock;

    //  Private Instance Methods

    void dirty_ugly_hack();
    void set_rendezvous_fd(int);

    //  Class Methods

    static void accept_client(int fd, void *closure);
    static void accept_ugly_hack(int fd, void *closure);
    static void read_ugly_hack(int fd, void *closure);

};

#endif /* !Listener_included */
