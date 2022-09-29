#include "InternalClient.h"

#include <assert.h>

#include "Directory.h"
#include "Event.h"
#include "File.h"
#include "Log.h"

InternalClient::InternalClient(const char *filename,
			       EventHandler h, void *closr)
    : Client("myself"), handler(h), closure(closr)
{
    assert(filename);
    assert(h);
    assert(filename[0] == '/');
    Log::debug("%s watching %s", name(), filename);
    interest = new File(filename, this, Request(0), Cred::SuperUser);
}

InternalClient::~InternalClient()
{
    delete interest;
}

Boolean
InternalClient::ready_for_events()
{
    return true;			// always ready for events
}

void
InternalClient::enqueue_for_scan(Interest *)
{
    int function_is_never_called = 0; assert(function_is_never_called);
}

void
InternalClient::dequeue_from_scan(Interest *)
{
    int function_is_never_called = 0; assert(function_is_never_called);
}

void
InternalClient::enqueue_scanner(Scanner *)
{
    int function_is_never_called = 0; assert(function_is_never_called);
}

void
InternalClient::post_event(const Event& event, Request, const char *)
{
//  Log::debug("sent %s event: \"%s\" %s", Client::name(), name, event.name());
    (*handler)(event, closure);
}
