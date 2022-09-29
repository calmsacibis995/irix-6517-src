#include "DirEntry.h"

#include <assert.h>
#include <stdio.h>
#include <sys/param.h>

#include "Directory.h"

// A DirEntry may be polled iff its parent is not polled.

DirEntry::DirEntry(const char *name, Directory *p, DirEntry *nx)
    : Interest(name, p->filesystem()), parent(p), next(nx)
{ }

DirEntry::~DirEntry()
{
    unscan();
}

Boolean
DirEntry::active() const
{
    return parent->active();
}

void
DirEntry::post_event(const Event& event, const char *eventpath)
{
    assert(!eventpath);
    parent->post_event(event, name());
}

void
DirEntry::scan(Interest *ip)
{
    assert(!ip);
    parent->scan(this);
}

void
DirEntry::unscan(Interest *ip)
{
    assert(!ip);
    parent->unscan(this);
}

void
DirEntry::do_scan()
{
    if (needs_scan() && active())
    {   parent->become_user();
	if (parent->chdir())
	    Interest::do_scan();
    }
}

void
DirEntry::notify_created(Interest *ip)
{
    assert(ip == this);
    parent->notify_created(ip);
}

void 
DirEntry::notify_deleted(Interest *ip)
{
    assert(ip == this);
    parent->notify_deleted(ip);
}
