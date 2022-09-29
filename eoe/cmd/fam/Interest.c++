#include "Interest.h"

#include <bstring.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

#include "Boolean.h"
#include "ChangeFlags.h"
#include "Event.h"
#include "FileSystem.h"
#include "IMon.h"
#include "Log.h"
#include "Pollster.h"
#include "timeval.h"

Interest *Interest::hashtable[];
IMon      Interest::imon(imon_handler);

Interest::Interest(const char *name, FileSystem *fs)
    : myname(strcpy(new char[strlen(name) + 1], name)),
      scan_state(OK),
      cur_exec_state(NOT_EXECUTING),
      old_exec_state(NOT_EXECUTING)
{
    bzero(&old_stat, sizeof old_stat);
    struct stat status;
    bzero(&status, sizeof status);
    IMon::Status s = imon.express(name, &status);
    if (s != IMon::OK)
    {   int rc = lstat(name, &status);
	if (rc < 0)
	{   Log::info("can't lstat %s", name);
	    bzero(&status, sizeof status);
	}
    }

    dev = status.st_dev;
    ino = status.st_ino;

    if (dev || ino)
    {
	// Insert on new chain.

	Interest **ipp = hashchain();
	hashlink = *ipp;
	*ipp = this;
    }
    else
	hashlink = NULL;

    (void) update_stat(status);

    //  Enable low-level monitoring.


      	// The NetWare filesystem is too slow to monitor, so
      	// don't even try.
    
	if ( strcmp( (char *) &status.st_fstype, "nwfs"))
		fs->ll_monitor(this, s == IMon::OK);
}


Interest::~Interest()
{
    Pollster::forget(this);
    revoke();
    delete myname;
}

void
Interest::revoke()
{
    //  Traverse our hash chain.  Delete this entry when we find it.
    //  Also check for other entries with same dev/ino.

    if (dev || ino)
    {
	    Boolean found_same = false;
	for (Interest *p, **pp = hashchain(); p = *pp; )
	    if (p == this)
		*pp = p->hashlink;	// remove this from list
	    else
	    {   if (p->ino == ino && p->dev == dev)
		    found_same = true;
		pp = &p->hashlink;	// move to next element
	    }
	if (!found_same)
	    (void) imon.revoke(name(), dev, ino);
    }
}

void
Interest::dev_ino(dev_t newdev, ino_t newino)
{
    // Remove from hash chain and revoke imon's interest.

    revoke();

    dev = newdev;
    ino = newino;

    if (newdev || newino)
    {
	// Insert on new chain.

	Interest **ipp = hashchain();
	hashlink = *ipp;
	*ipp = this;

	// Express interest.

	imon.express(name(), NULL);
    }
    else
	hashlink = NULL;
}

ChangeFlags
Interest::update_stat(struct stat& new_stat)
{
    ChangeFlags changes;
    if (dev != new_stat.st_dev)
	changes.set(ChangeFlags::DEV);
    if (ino != new_stat.st_ino)
	changes.set(ChangeFlags::INO);
    if (old_stat.mode != new_stat.st_mode)
	old_stat.mode = new_stat.st_mode;
    if (old_stat.uid != new_stat.st_uid)
    {   old_stat.uid = new_stat.st_uid;
	changes.set(ChangeFlags::UID);
    }
    if (old_stat.gid != new_stat.st_gid)
    {   old_stat.gid = new_stat.st_gid;
	changes.set(ChangeFlags::GID);
    }
    if (old_stat.size != new_stat.st_size)
    {   old_stat.size = new_stat.st_size;
	changes.set(ChangeFlags::SIZE);
    }
    if (old_stat.mtime != * (timeval *) &new_stat.st_mtim)
    {   old_stat.mtime = * (timeval *) &new_stat.st_mtim;
	changes.set(ChangeFlags::MTIME);
    }
    if (old_stat.ctime != * (timeval *) &new_stat.st_ctim)
    {   old_stat.ctime = * (timeval *) &new_stat.st_ctim;
	changes.set(ChangeFlags::CTIME);
    }
    return changes;
}

ChangeFlags
Interest::do_stat()
{
    struct stat status;
    int rc = lstat(name(), &status);
    if (rc < 0) {
	if (oserror() == ETIMEDOUT) {
	    return ChangeFlags();
	}
        bzero(&status, sizeof status);
    }

    Boolean exists = status.st_mode != 0;
    Boolean did_exist = old_stat.mode != 0;
    Boolean wasdir = (old_stat.mode & S_IFMT) == S_IFDIR;
    ChangeFlags changes = update_stat(status);

    if (exists && !did_exist)
    {
        post_event(Event::Created);
        notify_created(this);
    }
    else if (did_exist && !exists)
    {
        post_event(Event::Deleted);
        notify_deleted(this);
    }
#if 0
    else if (exists && did_exist)
    {
	// This extra exists event was causing all kinds of grief for
	// the desktop.  With this code in place, if you replace a
	// directory with a file of the same name, the desktop gets a
	// created event and a changed event.  This forces the desktop
	// to have to check each created event to see if it already
	// knows about the file.
	//
	// With this code removed, the desktop simply gets a changed
	// event, which seems perfectly appropriate.
	// --rogerc 08/21/97
        Boolean isdir = (status.st_mode & S_IFMT) == S_IFDIR;
        if (wasdir != isdir)
            post_event(Event::Exists);
    }
#endif

    //  If dev/ino changed, move this interest to the right hash chain.

    if (status.st_dev != dev || status.st_ino != ino)
        dev_ino(status.st_dev, status.st_ino);

    return changes;
}

void
Interest::do_scan()
{
    if (needs_scan() && active())
    {   needs_scan(false);
	Boolean did_exist = exists();
	ChangeFlags changes = do_stat();
	if (changes && did_exist && exists())
	    post_event(Event(changes));
	report_exec_state();
    }
}

void
Interest::report_exec_state()
{
    if (old_exec_state != cur_exec_state && active())
    {   post_event(cur_exec_state == EXECUTING ?
			  Event::Executing : Event::Exited);
	old_exec_state = cur_exec_state;
    }
}

void
Interest::imon_handler(dev_t device, ino_t inumber, int event)
{
    assert(device || inumber);

    for (Interest *p = *hashchain(device, inumber), *next = p; p; p = next)
    {	next = p->hashlink;
	if (p->ino == inumber && p->dev == device)
	{   if (event == IMon::EXEC)
	    {   p->cur_exec_state = EXECUTING;
		(void) p->report_exec_state();
	    }
	    else if (event == IMon::EXIT)
	    {   p->cur_exec_state = NOT_EXECUTING;
		(void) p->report_exec_state();
	    }
	    else
	    {   assert(event == IMon::CHANGE);
		p->scan();
	    }
	}
    }
}
