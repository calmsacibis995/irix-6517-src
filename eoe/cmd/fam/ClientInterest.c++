#include "ClientInterest.h"

#include <assert.h>

#include "Client.h"
#include "Event.h"
#include "FileSystem.h"
#include "FileSystemTable.h"

ClientInterest::ClientInterest(const char *name,
			       Client *c, Request r, const Cred& cr, Type type)
    : Interest(name, myfilesystem = FileSystemTable::find(name, cr)),
      myclient(c), request(r), mycred(cr)
{
    ci_bits() = ACTIVE_STATE;
    post_event(exists() ? Event::Exists : Event::Deleted);
    fs_request = myfilesystem->monitor(this, type);
}

ClientInterest::~ClientInterest()
{
    myfilesystem->cancel(this, fs_request);
    ci_bits() |= ACTIVE_STATE;
    post_event(Event::Acknowledge);
    unscan();
}

void
ClientInterest::filesystem(FileSystem *fs)
{
    myfilesystem->cancel(this, fs_request);
    myfilesystem = fs;
    fs_request = fs->monitor(this, type());
    scan();
}

void
ClientInterest::findfilesystem()
{
    FileSystem *new_fs = FileSystemTable::find(name(), mycred);
    if (new_fs != myfilesystem)
    {   myfilesystem->cancel(this, fs_request);
	myfilesystem = new_fs;
	fs_request = myfilesystem->monitor(this, type());
	scan();
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Suspend/resume.

Boolean
ClientInterest::active() const
{
    return ci_bits() & ACTIVE_STATE;
}

void
ClientInterest::suspend()
{
    if (active())
    {   ci_bits() &= ~ACTIVE_STATE;
	myfilesystem->hl_suspend(fs_request);
    }
}

void
ClientInterest::resume()
{
    Boolean was_active = active();
    if (!was_active)
	ci_bits() |= ACTIVE_STATE;	// Set state before generating events.
    do_scan();
    if (!was_active)
	myfilesystem->hl_resume(fs_request);
}

void
ClientInterest::post_event(const Event& event, const char *eventpath)
{
    assert(active());
    if (!eventpath)
	eventpath = name();
    myclient->post_event(event, request, eventpath);
}

Interest *
ClientInterest::find_name(const char *)
{
    //  Ignore name.

    return this;
}

void
ClientInterest::scan(Interest *ip)
{
    if (!ip)
	ip = this;
    ip->mark_for_scan();
    if (myclient->ready_for_events())
	ip->do_scan();
    else
	myclient->enqueue_for_scan(ip);
}

void
ClientInterest::unscan(Interest *ip)
{
    if (!ip)
	ip = this;
    if (ip->needs_scan())
	myclient->dequeue_from_scan(ip);
}

void
ClientInterest::do_scan()
{
    if (needs_scan() && active())
    {   become_user();
	Interest::do_scan();
    }
}

void
ClientInterest::notify_created(Interest *ip)
{
    if (!ip)
	ip = this;
    myfilesystem->ll_notify_created(ip);
}

void
ClientInterest::notify_deleted(Interest *ip)
{
    if (!ip)
	ip = this;
    myfilesystem->ll_notify_deleted(ip);
}
