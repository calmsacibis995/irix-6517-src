#include "Scheduler.H"

#include <errno.h>
#include <unistd.h>

#include "IOHandler.H"
#include "Log.H"
#include "Task.H"

bool Scheduler::running;

int
Scheduler::num_fds()
{
    int nrfds =   ReadHandler::nfds();
    int nwfds =  WriteHandler::nfds();
    int nxfds = ExceptHandler::nfds();

    int max = nrfds;
    if (max < nwfds)
	max = nwfds;
    if (max < nxfds)
	max = nxfds;
    return max;
}

// Scheduling priorities defined here: writable descriptors have
// highest priority, followed by exceptionable descriptors, then
// readable descriptors, then timed tasks have the lowest priority.

void
Scheduler::select()
{
    fd_set  *   readfds = ReadHandler::fdset();
    fd_set  *  writefds = WriteHandler::fdset();
    fd_set  * exceptfds = ExceptHandler::fdset();
    timeval *timeout   = Task::calc_timeout();
    int nfds = num_fds();

    if (nfds == 0 && timeout == NULL)
    {
	//  Nothing to do.
	running = false;
	return;
    }

    int status = ::select(nfds, readfds, writefds, exceptfds, timeout);

    if (status == -1 && errno != EINTR)
    {   Log::perror("select");		// Oh, no!
	::exit(1);
    }
    if (status > 0)
    {
	// I/O is ready -- find it and do it.

	 WriteHandler::run_handlers( writefds, nfds);
	ExceptHandler::run_handlers(exceptfds, nfds);
	  ReadHandler::run_handlers(  readfds, nfds);
    }

    // Check for tasks now.

    Task::run_tasks();
}
