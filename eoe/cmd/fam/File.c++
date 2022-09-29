#include "File.h"

#include "Event.h"

File::File(const char *name, Client *c, Request r, const Cred& cr)
    : ClientInterest(name, c, r, cr, FILE)
{
    post_event(Event::EndExist);
}

ClientInterest::Type
File::type() const
{
    return FILE;
}
