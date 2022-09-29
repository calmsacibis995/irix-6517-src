#include "ClientConnection.h"

#include <assert.h>

#include "Event.h"

ClientConnection::ClientConnection(int fd,
				   InputHandler inhandler,
				   UnblockHandler unhandler, void *closure)
    : NetConnection(fd,
		    inbuf, sizeof inbuf, outbuf, sizeof outbuf,
		    unhandler, closure),
      ihandler(inhandler), iclosure(closure)
{ }

void
ClientConnection::input_msg(const char *msg, unsigned nbytes)
{
    (*ihandler)(msg, nbytes, iclosure);
}

void
ClientConnection::send_event(const Event& event, Request request,
			     const char *name)
{
    // Format message.

    char code = event.code();
    if (event == Event::Changed)
	mprintf("%c%lu %s %s\n", code, request, event.changes(), name);
    else
	mprintf("%c%lu %s\n", code, request, name);
}
