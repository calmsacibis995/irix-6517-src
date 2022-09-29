#include "Scheduler.h"

#include <assert.h>
#include <bstring.h>
#include <errno.h>
#include <unistd.h>

#include "Log.h"
#include "timeval.h"

//  Define a bunch of class-global variables.

const timeval		 Scheduler::ASAP   = { 0, 0 };
Scheduler::IOTypeInfo	 Scheduler::read   = &FDInfo::read  ;
Scheduler::IOTypeInfo	 Scheduler::write  = &FDInfo::write ;
Scheduler::IOTypeInfo	 Scheduler::except = &FDInfo::except;
unsigned int		 Scheduler::nfds;
Scheduler::FDInfo	*Scheduler::fdinfo;
unsigned int		 Scheduler::nfdinfo_alloc;

unsigned int		 Scheduler::ntasks;
Scheduler::TimedProc	 Scheduler::recurring_proc;
void			*Scheduler::recurring_closure;
timeval			 Scheduler::next_task_time;
timeval			 Scheduler::recurring_interval;
Scheduler::onetime_task	*Scheduler::first_task;
Boolean			 Scheduler::running;


//////////////////////////////////////////////////////////////////////////////
//  One time task code

void
Scheduler::install_onetime_task(const timeval& when,
				TimedProc proc, void *closure)
{
    // Log::debug("Sched install 1time %x(%x)", proc, closure);
    onetime_task **fp = &first_task;
    while (*fp && (*fp)->when < when)
	fp = &(*fp)->next;
    onetime_task *nt = new onetime_task;
    nt->next = *fp;
    *fp = nt;
    nt->when = when;
    nt->proc = proc;
    nt->closure = closure;
    ntasks++;
}

void
Scheduler::remove_onetime_task(TimedProc proc, void *closure)
{
    // Log::debug("Sched remove 1time %x(%x)", proc, closure);
    onetime_task *p, **pp = &first_task;
    while (p = *pp)
    {   if (p->proc == proc && p->closure == closure)
	{   *pp = p->next;
	    delete p;
	    ntasks--;
	    break;
	}    
	pp = &p->next;
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Recurring task code

void
Scheduler::install_recurring_task(const timeval& interval,
				  TimedProc proc, void *closure)
{
    // Log::debug("Sched install recur %x(%x) @ %g sec", proc, closure,
    //	       interval.tv_sec + interval.tv_usec / 1000000.0);
    timeval now;

    assert(!recurring_proc);
    assert(proc);
    assert(interval.tv_sec >= 0);
    assert(interval.tv_usec >= 0 && interval.tv_usec < 1000000);
    assert(interval.tv_sec || interval.tv_usec);

    recurring_proc = proc;
    recurring_closure = closure;
    recurring_interval = interval;
    ntasks++;
    (void) gettimeofday(&now);
    next_task_time = now + interval;
}

void
Scheduler::remove_recurring_task(TimedProc proc, void *closure)
{
    // Log::debug("Sched remove recur %x(%x)", proc, closure);
    assert(proc == recurring_proc);
    assert(closure == recurring_closure);

    recurring_proc = NULL;
    recurring_closure = closure;
    timerclear(&recurring_interval);
    ntasks--;
}

//  do_tasks activates all timer based tasks.  It also sets
//  next_task_time to the absolute time when it should next be
//  invoked.

void
Scheduler::do_tasks()
{
    if (ntasks)
    {   timeval now;
	(void) gettimeofday(&now, NULL);
	if (recurring_proc && now >= next_task_time)
	{
	    // Time for the next task.

	    // Log::debug("Sched do recurring %x(%x)",
	    //	       recurring_proc, recurring_closure);
	    (*recurring_proc)(recurring_closure);
	    next_task_time += recurring_interval;

	    // If the clock has jumped ahead or we've gotten too far behind,
	    // postpone the next task.

	    if (next_task_time < now)
		next_task_time = now + recurring_interval;
	}
	else
	{   timeval time_left = next_task_time - now;
	    if (recurring_interval < time_left)
	    {
		// More time left than we started with -- clock must have
		// run backward.  Reset next_task_time to sane value.

		next_task_time = now + recurring_interval;
	    }
	}

	while (first_task && first_task->when < now)
	{   TimedProc proc = first_task->proc;
	    void *closure = first_task->closure;
	    remove_onetime_task(proc, closure);
	    // Log::debug("Sched do 1time %x(%x)", proc, closure);
	    (*proc)(closure);
	}
    }
}

//  calc_timeout calculates the timeout to pass to select().
//  It returns:
//		NULL (if no timed tasks exist)
//		zero time (if it's time for the next task)
//		nonzero time (if it's not time yet)

timeval *
Scheduler::calc_timeout()
{
    static timeval sleep_interval;
    
    if (ntasks)
    {
	timeval wake_time;
	if (recurring_proc)
	    wake_time = next_task_time;
	if (!recurring_proc || first_task && first_task->when < wake_time)
	    wake_time = first_task->when;
	timeval now;
	(void) gettimeofday(&now, NULL);
	sleep_interval = wake_time - now;
	if (sleep_interval.tv_sec < 0)
	    timerclear(&sleep_interval);
	return &sleep_interval;
    }
    else
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//  I/O handler code

//  fd_to_info converts a file descriptor to a pointer into the
//  fdinfo array.  On the way, it verifies that the array has
//  been allocated far enough out.

Scheduler::FDInfo *
Scheduler::fd_to_info(int fd)
{
    assert(fd >= 0);
    if (nfds < fd + 1)
    {
	if (nfdinfo_alloc < fd + 1)
	{
	    unsigned newalloc = nfdinfo_alloc * 3 / 2 + 10;
	    if (newalloc < fd + 1)
		newalloc = fd + 1;
	    FDInfo *newinfo = new FDInfo[newalloc];
	    for (unsigned i = 0; i < nfds; i++)
		newinfo[i] = fdinfo[i];
	    delete [] fdinfo;
	    fdinfo = newinfo;
	    nfdinfo_alloc = newalloc;
	}

	// Zero all new fdinfo's.
	bzero(&fdinfo[nfds], (fd + 1 - nfds) * sizeof *fdinfo);
	nfds = fd + 1;
    }
    return &fdinfo[fd];
}

//  trim_fdinfo makes the fdinfo array smaller if its last entries
//  aren't being used.  The memory isn't actually freed unless the
//  array is completely zeroed out.

void
Scheduler::trim_fdinfo(int fd)
{
    assert(fd >= 0);
    for (FDInfo *fp = &fdinfo[nfds - 1]; nfds > 0; --nfds, --fp)
	if (fp->read.handler || fp->write.handler || fp->except.handler)
	    break;

    if (!nfds)
    {   delete [] fdinfo;
	fdinfo = NULL;
	nfdinfo_alloc = 0;
    }
}

Scheduler::IOHandler
Scheduler::install_io_handler(int fd, IOHandler handler, void *closure,
			      IOTypeInfo *iotype)
{
    assert(fd >= 0);
    assert(handler);
    FDInfo *fp = fd_to_info(fd);
    IOHandler old_handler = (fp->*(iotype->iotype)).handler;
    (fp->*(iotype->iotype)).handler = handler;
    (fp->*(iotype->iotype)).closure = closure;
    FD_SET(fd, &iotype->fds);
    iotype->nbitsset++;
    return old_handler;
}

Scheduler::IOHandler
Scheduler::remove_io_handler(int fd, IOTypeInfo *iotype)
{
    assert(fd >= 0 && fd < nfds);
    FDInfo *fp = fd_to_info(fd);
    IOHandler old_handler = (fp->*(iotype->iotype)).handler;
    (fp->*(iotype->iotype)).handler = NULL;
    (fp->*(iotype->iotype)).closure = NULL;
    trim_fdinfo(fd);
    FD_CLR(fd, &iotype->fds);
    iotype->nbitsset--;
    return old_handler;
}

Scheduler::IOHandler
Scheduler::install_read_handler(int fd, IOHandler handler, void *closure)
{
    // Log::debug("Sched install read %x(%d, %x)", handler, fd, closure);
    return install_io_handler(fd, handler, closure, &read);
}

Scheduler::IOHandler
Scheduler::remove_read_handler(int fd)
{
    // Log::debug("Sched remove read (%d, %x)", fd);
    return remove_io_handler(fd, &read);
}

Scheduler::IOHandler
Scheduler::install_write_handler(int fd, IOHandler handler, void *closure)
{
    // Log::debug("Sched install write %x(%d, %x)", handler, fd, closure);
    return install_io_handler(fd, handler, closure, &write);
}

Scheduler::IOHandler
Scheduler::remove_write_handler(int fd)
{
    // Log::debug("Sched remove write (%d, %x)", fd);
    return remove_io_handler(fd, &write);
}

Scheduler::IOHandler
Scheduler::install_except_handler(int fd, IOHandler handler, void *closure)
{
    return install_io_handler(fd, handler, closure, &except);
}

Scheduler::IOHandler
Scheduler::remove_except_handler(int fd)
{
    return remove_io_handler(fd, &except);
}

void
Scheduler::handle_io(const fd_set *fds, FDInfo::handler FDInfo::* iotype)
{
    if (fds)
	for (int fd = 0; fd < nfds; fd++)
	    if (FD_ISSET(fd, fds))
	    {   FDInfo *fp = &fdinfo[fd];
		assert(iotype == &FDInfo::read || iotype == &FDInfo::write);
		// Log::debug("Sched do %s %x(%d, %x)",
		//	   iotype == &FDInfo::read ? "read" : "write",
		//	   (fp->*iotype).handler, fd, (fp->*iotype).closure);
		(fp->*iotype).handler(fd, (fp->*iotype).closure);
		// Remember, handler may move fdinfo array.
	    }
}

fd_set *
Scheduler::calc_fds(fd_set& fds, IOTypeInfo *iotype)
{
    if (iotype->nbitsset)
    {
	// Copy only as many words as needed.

	for (int i = 0, n = howmany(nfds, NFDBITS); i < n; i++)
	    fds.fds_bits[i] = iotype->fds.fds_bits[i];
	return &fds;
    }
    else
	return NULL;
}

// Scheduling priorities defined here: writable descriptors have
// highest priority, followed by exceptionable descriptors, then
// readable descriptors, then timed tasks have the lowest priority.

void
Scheduler::select()
{
    fd_set readyfds, writyfds, exceptyfds;
    fd_set  *  readfds = calc_fds(  readyfds, &Scheduler::read  );
    fd_set  * writefds = calc_fds(  writyfds, &Scheduler::write );
    fd_set  *exceptfds = calc_fds(exceptyfds, &Scheduler::except);
    timeval *timeout   = calc_timeout();

    int status = ::select(nfds, readfds, writefds, exceptfds, timeout);

    if (status == -1 && errno != EINTR)
    {   Log::perror("select");		// Oh, no!
	::exit(1);
    }
    if (status > 0)
    {
	// I/O is ready -- find it and do it.

	handle_io( writefds, &FDInfo::write );
	handle_io(exceptfds, &FDInfo::except);
	handle_io(  readfds, &FDInfo::read  );
    }

    // Check for tasks now.

    do_tasks();
}
