#include "FAMonitor.H"

#include <assert.h>
#include <bstring.h>
#include <fam.h>
#include <stddef.h>
#include <unistd.h>

#include "Log.H"
#include "IOHandler.H"
#include "Scheduler.H"

FAMConnection      FAMonitor::fam;
FAMonitor::Request FAMonitor::requests[];
bool               FAMonitor::connected;
ReadHandler	   FAMonitor::event_handler(read_handler, NULL);

FAMonitor::FAMonitor(const char *path, Type type, ChangeProc proc, void *clos)
: _proc(proc), _closure(clos)
{
    //  Are we sane?

    assert(type == DIR || type == FILE);
    assert(path[0] == '/');
    init();

    if (!connected)
    {	_reqnum = -1;
	return;
    }

    //  Find unused request.

    for (_reqnum = 0; requests[_reqnum].in_use; _reqnum++)
	assert(_reqnum < MAX_REQUESTS);

    //  Send request to fam.

    FAMRequest fr = { _reqnum };
    int rc;
    if (type == DIR)
	rc = FAMMonitorDirectory2(&fam, path, &fr);
    else
	rc = FAMMonitorFile2(&fam, path, &fr);
    if (rc < 0)
    {   Log::perror("FAMMonitor failed");
	return;
    }

    //  Start monitoring.

    requests[_reqnum].in_use = true;
    requests[_reqnum].mon = this;
}

FAMonitor::~FAMonitor()
{
    if (_reqnum >= 0 && requests[_reqnum].mon == this)
	requests[_reqnum].mon = NULL;
}

///////////////////////////////////////////////////////////////////////////////

void
FAMonitor::init()
{
    static bool been_here = false;
    if (been_here)
	return;
    been_here = true;

#ifndef NDEBUG

    //  Initialize booleans to false.

    for (int i = 0; i < MAX_REQUESTS; i++)
	requests[i].in_use = false;

#endif

    // Get a FAM connection.

    connected = false;
    int tries = 1;
    while (FAMOpen2(&fam, Log::name()) < 0)
    {
	if (tries <= 10)
	{
	    Log::error("Can't connect to fam: %m.  Sleeping %d second%s",
		       tries, tries == 1 ? "" : "s");
	    sleep(tries);
	    tries++;
	}
	else
	{
	    Log::error("Can't connect to fam.");
	    return;
	}
    }
    connected = true;

    //  Set up file read event.

    event_handler.fd(FAMCONNECTION_GETFD(&fam));
    event_handler.activate();
}

void
FAMonitor::read_handler(int fam_fd, void *)
{
    assert(fam_fd == FAMCONNECTION_GETFD(&fam));
    while (FAMPending(&fam) > 0)
    {   FAMEvent event;
	int rc = FAMNextEvent(&fam, &event);
	if (rc < 0)
	{
	    //  Error on read.  Print error message.

	    Log::perror("error receiving from fam");
	}
	if (rc <= 0)
	{
	    //  End of file.  Close connection.

	    FAMClose(&fam);
	    connected = false;
	    event_handler.deactivate();
	    for (int i = 0; i < MAX_REQUESTS; i++)
		if (requests[i].in_use)
		{   FAMEvent fake_ack;
		    bzero(&fake_ack, sizeof fake_ack);
		    fake_ack.code = FAMAcknowledge;
		    requests[i].mon->post(fake_ack);
		}
	    break;
	}
	else
	{
	    //  Got an event.  Use it.

	    int index = event.fr.reqnum;
	    assert(0 <= index && index < MAX_REQUESTS);
	    requests[index].mon->post(event);
	}
    }
}
