#ifndef ServerConnection_included
#define ServerConnection_included

#include <string.h>
#include <sys/param.h>

#include "ClientInterest.h"
#include "NetConnection.h"
#include "Request.h"

class ChangeFlags;
class Cred;

//  A ServerConnection implements the client side of the fam protocol,
//  and it also handles network I/O. The various requests, monitor,
//  cancel, etc., can be sent.  When FAM events arrive, the
//  EventHandler is called.
//
//  ServerConnection inherits from NetConnection.  This means that
//  users of ServerConnection can test for I/O ready
//  (ready_for_input(), ready_for_output()) and that they need to
//  specify a DisconnectHandler which will be called when the server
//  goes away.

class ServerConnection : public NetConnection {

public:

    typedef void (*EventHandler)(const Event&, Request, const char *path,
				 void *closure);
    typedef void (*DisconnectHandler)(void *closure);

    ServerConnection(int fd, EventHandler, DisconnectHandler, void *closure);

    void send_monitor(ClientInterest::Type, Request,
		      const char *path, const Cred&);
    void send_cancel(Request r)		{ mprintf("C%d 0 0\n", r); }
    void send_suspend(Request r)	{ mprintf("S%d 0 0\n", r); }
    void send_resume(Request r)		{ mprintf("U%d 0 0\n", r); }
    void send_name(const char *n)	{ mprintf("N0 0 0 %s\n", n); }

protected:

    void input_msg(const char *msg, unsigned nbytes);

private:

    enum { MAXMSGSIZE = MAXPATHLEN + 40 };
    enum { INBUFSIZE = MAXMSGSIZE, OUTBUFSIZE = MAXMSGSIZE };

    EventHandler event_handler;
    DisconnectHandler disconnect_handler;
    void *closure;
    char  inbuf[ INBUFSIZE];
    char outbuf[OUTBUFSIZE];

};

#endif /* !ServerConnection_included */
