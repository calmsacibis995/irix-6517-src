#ifndef RPCListener_included
#define RPCListener_included

#include "IOHandler.H"
#include "bool.H"

class RPCListener {

    typedef unsigned long u_long;

public:

    enum { MEDIAD_PROGRAM = 391017, MEDIAD_VERSION = 1 };
    enum { RENDEZVOUS_FD = 0 };		// descriptor 0 opened by inetd

    RPCListener(bool all_hosts,
		bool started_by_inetd,
		u_long program = MEDIAD_PROGRAM,
		u_long version = MEDIAD_VERSION);
    ~RPCListener();

private:

    bool _all_hosts;
    bool _started_by_inetd;
    u_long _program;
    u_long _version;
    ReadHandler _rendezvous_handler;

    static void accept_client(int fd, void *closure);

};

#endif /* !RPCListener_included */
