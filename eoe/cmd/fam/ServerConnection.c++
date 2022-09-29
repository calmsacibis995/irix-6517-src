#include "ServerConnection.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "Cred.h"
#include "Event.h"
#include "Log.h"

ServerConnection::ServerConnection(int fd,
				   EventHandler eh, DisconnectHandler dh,
				   void *clos)
    : NetConnection(fd,
		    inbuf, sizeof inbuf, outbuf, sizeof outbuf, NULL, NULL),
      event_handler(eh), disconnect_handler(dh), closure(clos)
{
    assert(fd >= 0);
    assert(eh);
    assert(dh);
}

void
ServerConnection::input_msg(const char *msg, unsigned nbytes)
{
    if (msg == NULL)
    {   (*disconnect_handler)(closure);	// Disconnected.
	return;
    }

    if (nbytes == 0 || msg[nbytes - 1])
    {   Log::debug("protocol error");
	return;
    }

    char *p = (char *) msg;
    char opcode = *p++;
    Request request = strtol(p, &p, 10);
    Event event;
    if (opcode == 'c')
	event = Event(ChangeFlags((const char *) p, (const char **) &p));
    else
	event = Event(opcode);
    
    while (isascii(*p) && isspace(*p))
	p++;
    char name[MAXPATHLEN];
    int i;
    for (i = 0; *p; i++)
    {   if (i >= MAXPATHLEN)
	{   Log::error("path name too long (%d chars)", i);
	    return;
	}
	name[i] = *p++;
    }
    name[i ? i - 1 : 0] = '\0';		// strip the trailing newline

    (*event_handler)(event, request, name, closure);
}

void
ServerConnection::send_monitor(ClientInterest::Type code, Request request,
			       const char *path, const Cred& cr)
{
    char cred_buf[Cred::GROUP_STR_MAX];
    cr.printgroups(cred_buf);
    if (cred_buf[0])
	mprintf("%c%d %d %d %s\n%c%s", code, request, cr.uid(), cr.gid(), path,
		'\0', cred_buf);
    else
	mprintf("%c%d %d %d %s\n", code, request, cr.uid(), cr.gid(), path);
}
