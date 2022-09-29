#ifndef ClientConnection_included
#define ClientConnection_included

#include <sys/param.h>
#include "NetConnection.h"
#include "Request.h"

class Event;

//  ClientConnection implements the fam server protocol.  It generates
//  fam events.  It does not parse fam requests because the API for its
//  caller would be really messy.  Instead it hands received messages
//  to its owner unparsed.
//
//  The field order is important -- the big net buffers are last.
//  Since the output buffer is twice as big as the input buffer,
//  it comes after the input buffer.

class ClientConnection : public NetConnection {

public:

    typedef void (*InputHandler)(const char *, unsigned nbytes, void *closure);

    ClientConnection(int fd, InputHandler, UnblockHandler, void *closure);

    void send_event(const Event&, Request, const char *name);

protected:

    void input_msg(const char *, unsigned);

private:

    enum { MAXMSGSIZE = MAXPATHLEN + 40 };
    enum { INBUFSIZE = MAXMSGSIZE, OUTBUFSIZE = 2 * MAXMSGSIZE };

    InputHandler ihandler;
    void *iclosure;
    char  inbuf[ INBUFSIZE];
    char outbuf[OUTBUFSIZE];

};

#endif /* !ClientConnection_included */
