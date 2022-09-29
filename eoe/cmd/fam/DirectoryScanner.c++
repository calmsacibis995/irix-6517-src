#include "DirectoryScanner.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

#include "Client.h"
#include "Directory.h"
#include "DirEntry.h"
#include "Log.h"

//////////////////////////////////////////////////////////////////////////////

DirectoryScanner::DirectoryScanner(Directory& d,
				   const Event& e, Boolean b,
				   DoneHandler dh, void *vp)
    : directory(d), new_event(e), scan_entries(b),
      dir(NULL), openErrno(0),
      epp(&d.entries), discard(NULL),
      done_handler(dh), closure(vp)
{
    dir = opendir(d.name());
    if (dir == NULL) {
	openErrno = oserror();
    }
}

DirectoryScanner::~DirectoryScanner()
{
    if (dir)
	closedir(dir);
}

//////////////////////////////////////////////////////////////////////////////

// return address of ptr to entry matching name

DirEntry **
DirectoryScanner::match_name(DirEntry **epp, const char *name)
{
    for (DirEntry *ep; ep = *epp; epp = &ep->next)
	if (!strcmp(ep->name(), name))
	    return epp;
    return NULL;
}

Boolean
DirectoryScanner::done()
{
    // We got an nfs time out.  We'll want to try again later.
    if (openErrno == ETIMEDOUT) {
	directory.unhang();
	Log::debug("openErrno == ETIMEDOUT");
	(*done_handler)(this, closure);
	return true;
    }

    Boolean ready = directory.client()->ready_for_events();
    while (dir && ready)
    {
	struct direct *dp = readdir(dir);
	if (dp == NULL)
	{   closedir(dir);
	    dir = NULL;
	    break;
	}

	//  Ignore "." and "..".

	if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
	    continue;

	DirEntry *ep = *epp, **epp2;
	if (ep && !strcmp(dp->d_name, ep->name()))
	{
	    //  Next entry in list matches. Do not change list.

	    // Log::debug("checkdir match %s", dp->d_name);
	}
	else if (epp2 = match_name(&discard, dp->d_name))
	{
	    //  Found in discard.  Insert discarded entry before ep.

	    // Log::debug("checkdir fdisc %s", dp->d_name);
	    ep = *epp2;
	    *epp2 = ep->next;
	    ep->next = *epp;
	    *epp = ep;
	}
	else if (ep && (epp2 = match_name(&ep->next, dp->d_name)))
	{
	    //  Found further in list.  Prepend internode segment
	    //  to discard.

	    // Log::debug("checkdir furth %s", dp->d_name);
	    ep = *epp2;
	    *epp2 = discard;
	    discard = *epp;
	    *epp = ep;
	}
	else
	{
	    // New entry. Insert.

	    // Log::debug("checkdir new   %s", dp->d_name);
	    directory.become_user();
	    if (!directory.chdir())
		continue;
	    ep = new DirEntry(dp->d_name, &directory, *epp);
	    *epp = ep;
	    ep->post_event(new_event);
	    ready = directory.client()->ready_for_events();
	    epp = &ep->next;
	    continue;		// Do not scan newly created entry.
	}
	if (scan_entries)
	{   ep->scan();
	    ready = directory.client()->ready_for_events();
	}
	epp = &ep->next;
    }

    while (*epp && ready)
    {   DirEntry *ep = *epp;
	*epp = ep->next;
	ep->post_event(Event::Deleted);
	ready = directory.client()->ready_for_events();
	delete ep;
    }

    while (discard && ready)
    {   DirEntry *ep = discard;
	discard = discard->next;
	ep->post_event(Event::Deleted);
	ready = directory.client()->ready_for_events();
	delete ep;
    }
	
    if (dir || *epp || discard || !ready)
	return false;

    (*done_handler)(this, closure);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//  Memory management.  Maintain a cache of one DirectoryScanner to reduce
//  heap stirring.

DirectoryScanner *DirectoryScanner::cache;

void *
DirectoryScanner::operator new (size_t size)
{
    assert(size == sizeof (DirectoryScanner));
    DirectoryScanner *p = cache ? cache : (DirectoryScanner *) new char[size];
    if (cache)
	cache = NULL;
    return p;
}

void
DirectoryScanner::operator delete (void *p)
{
    assert(p != NULL);
    if (cache)
	delete [] cache;
    cache = (DirectoryScanner *) p;
}
