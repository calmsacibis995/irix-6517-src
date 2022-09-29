#include "Directory.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Client.h"
#include "DirEntry.h"
#include "DirectoryScanner.h"
#include "Event.h"
#include "FileSystem.h"
#include "Log.h"
#include "Scheduler.h"

Directory *Directory::current_dir;

Directory::Directory(const char *name, Client *c, Request r, const Cred& cr)
    : ClientInterest(name, c, r, cr, DIRECTORY), entries(NULL), unhangPid(-1)
{
    dir_bits() = 0;
    dir_bits() |= SCANNING;
    DirectoryScanner *scanner = new DirectoryScanner(*this,
						     Event::Exists, false,
						     new_done_handler, this);
    if (!scanner->done())
	client()->enqueue_scanner(scanner);
}

void
Directory::new_done_handler(DirectoryScanner *scanner, void *closure)
{
    Directory *dir = (Directory *) closure;
    dir->dir_bits() &= ~SCANNING;
    dir->post_event(Event::EndExist);
    delete scanner;
}

Directory::~Directory()
{
    assert(!(dir_bits() & SCANNING));
    if (dir_bits() & RESCAN_SCHEDULED)
	Scheduler::remove_onetime_task(scan_task, this);
    DirEntry *q, *p = entries;
    if (p)
    {   (void) chdir();
	while (p)
	{   q = p->next;
	    delete p;
	    p = q;
	}
    }
    if (current_dir == this)
	chdir_task(NULL);
}

ClientInterest::Type
Directory::type() const
{
    assert(!(dir_bits() & SCANNING));
    return DIRECTORY;
}

void
Directory::resume()
{
    assert(!(dir_bits() & SCANNING));
    ClientInterest::resume();
    for (DirEntry *ep = entries; ep; ep = ep->next)
	if (ep->needs_scan())
	    ep->scan();
}

Interest *
Directory::find_name(const char *name)
{
    assert(!(dir_bits() & SCANNING));
    if (name[0] == '/')
	return this;
    else
	for (DirEntry *ep = entries; ep; ep = ep->next)
	    if (!strcmp(name, ep->name()))
		return ep;
    return NULL;
}

//  Directory::do_scan() scans a Directory.  There are several cases.
//
//  If monitoring is suspended, do nothing.
//
//  If the Directory is actually not a directory (e.g., a file, a
//  device or nonexistent), then it is lstat'd once, and a Changed,
//  Deleted or Created event is written if appropriate.
//
//  If a Directory changes from a directory to something else, then
//  all its entries are deleted and Deleted events are sent.
//
//  If it is a real directory, then it is lstat'd repeatedly and read
//  repeatedly until it stops changing.  This prevents fam from
//  missing files in race conditions (I hope).

void
Directory::do_scan()
{
    if (!active() || !needs_scan() || (dir_bits() & SCANNING))
	return;
    become_user();
    ChangeFlags changes = do_stat();
    if (changes && !isdir())
	post_event(Event(changes));
    Boolean scan_entries = filesystem()->dir_entries_scanned();
    dir_bits() |= SCANNING;
    DirectoryScanner *scanner = new DirectoryScanner(*this, Event::Created,
						     scan_entries,
						     scan_done_handler,
						     this);
    if (!scanner->done())
	client()->enqueue_scanner(scanner);
}

void
Directory::scan_task(void *closure)
{
    Directory *dir = (Directory *) closure;
    dir->dir_bits() &= ~RESCAN_SCHEDULED;
    dir->scan();
}

void
Directory::scan_done_handler(DirectoryScanner *scanner, void *closure)
{
    Directory *dir = (Directory *) closure;
    dir->dir_bits() &= ~SCANNING;
    delete scanner;
}

Boolean
Directory::chdir()
{
    if (current_dir == this)
	return true;

    int rc = ::chdir(name());
    if (rc < 0)
    {   Log::info("can't chdir(\"%s\"): %m", name());
	return false;
    }

    if (!current_dir)
	Scheduler::install_onetime_task(Scheduler::ASAP, chdir_task, NULL);
    current_dir = this;
    return true;
}

void
Directory::chdir_task(void *)
{
    if (current_dir)
    {   (void) ::chdir("/");
	current_dir = NULL;
    }
}

//
//  void
//  Directory::unhang()
//
//  Description:
//      This gets called after a system call has failed with oserror()
//      set to ETIMEDOUT.  This means that we can't contact the nfs
//      server.  In order to reconnect when the server becomes visible
//      again, a process has to acually hang on the mount.  We fork and
//      exec nfsunhang to do this for us.  We don't care about
//      nfsunhang's exit status because we poll nfs mounts anyway.
//
void
Directory::unhang()
{
    int status;
    if (unhangPid == -1
	|| waitpid(unhangPid, &status, WNOHANG) != 0) {

	if (access("/usr/lib/nfsunhang", X_OK) == -1) {
	    return;
	}

	unhangPid = fork();
	if (unhangPid == 0) {
	    for (int fd = getdtablehi() - 1; fd > 2; fd--) {
		close(fd);
	    }
	    execl("/usr/lib/nfsunhang", "nfsunhang", name(), NULL);
	    exit(1);
	}
	Log::debug("unhangPid: %d", unhangPid);
    }
}
