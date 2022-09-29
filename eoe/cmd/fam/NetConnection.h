#ifndef NetConnection_included
#define NetConnection_included

#include "Boolean.h"

//  NetConnection is an abstract base class that implements an event
//  driven, flow controlled reliable datagram connection over an
//  already open stream socket.
//
//  The incoming and outgoing data streams are broken into messages.
//  The format of a message is four bytes of message length (MSB
//  first) followed by "length" bytes of data.  The last byte of data
//  should be NUL ('\0').  The maximum message length is determined by
//  the buffer sizes passed to the NetConnection constructor.
//
//  The mprintf() function outputs a message using printf-style
//  formatting.  It appends a NUL byte to the message and prepends the
//  message length.  It also automatically flushes the message.  If
//  the connection has closed, mprintf() returns immediately.
//
//  When a complete message is received, the pure virtual function
//  input_msg() is called with the length and the address of the
//  message.  If EOF is read on the connection, input_msg() is called
//  with a NULL address and count (analogously to read(2) returning 0
//  bytes).
//
//  NetConnection implements flow control.  Whenever the output buffer
//  is empty, read events are accepted from the Scheduler.  Whenever
//  the output buffer can't be flushed, reading is suspended, and the
//  Scheduler watches for the connection to be writable.  This means
//  that input is only accepted when it's possible to send a reply.
//  The unblock handler, if any, is called whenever output becomes
//  unblocked.

class NetConnection {

public:

    typedef void (*UnblockHandler)(void *closure);

    NetConnection(int fd,
		  char *inbuf, unsigned isize,
		  char *outbuf, unsigned osize,
		  UnblockHandler, void *closure);

    virtual ~NetConnection();
    Boolean ready_for_output() const	{ return oready; }
    void ready_for_input(Boolean);

protected:

    virtual void input_msg(const char *data, unsigned nbytes) = 0;
    void mprintf(const char *format, ...);

private:

    typedef unsigned int Length;

    int fd;
    Boolean iready, oready;
    char *ibuf, *obuf;
    char *iend, *oend;
    char *itail, *otail;
    UnblockHandler unblock_handler;
    void *closure;

    void set_handlers(Boolean new_iready, Boolean new_oready);
    void shutdown(Boolean = true);

    //  Input

    void input();
    void deliver_input();
    static void read_handler(int fd, void *closure);

    //  Output

    void flush();
    static void write_handler(int fd, void *closure);

    NetConnection(const NetConnection&); // Do not copy
    operator = (const NetConnection&);	 //  or assign.

};

#endif /* !NetConnection_included */
