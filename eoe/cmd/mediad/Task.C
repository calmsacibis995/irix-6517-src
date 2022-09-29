#include "Task.H"

#include <assert.h>

#include "operators.H"

//////////////////////////////////////////////////////////////////////////////
//  Timeval operators.

timeval&
operator += (timeval& left, const timeval& right)
{
    left.tv_sec += right.tv_sec;
    left.tv_usec += right.tv_usec;
    while (left.tv_usec >= 1000000)
    {   left.tv_usec -= 1000000;
	left.tv_sec += 1;
    }
    return left;
}

timeval&
operator -= (timeval& left, const timeval& right)
{
    left.tv_sec -= right.tv_sec;
    left.tv_usec -= right.tv_usec;
    while (left.tv_usec < 0)
    {   left.tv_usec += 1000000;
	left.tv_sec -= 1;
    }
    return left;
}

inline bool
operator < (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, < );
}

inline bool
operator > (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, > );
}

inline bool
operator == (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, == );
}

inline bool
operator != (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, != );
}

//////////////////////////////////////////////////////////////////////////////

const timeval Task::ASAP = { 0, 0 };
const timeval Task::Once = { 0, 0 };
      timeval Task::now;
      Task   *Task::queue;

Task::Task(TaskProc proc, void *closure)
: _next(NULL), _prev(NULL), _proc(proc), _closure(closure)
{
    _when.tv_sec = _when.tv_usec = 0;
    _interval.tv_sec = _interval.tv_usec = 0;
}

Task::~Task()
{
    if (scheduled())
	cancel();
}

void
Task::schedule(const timeval& when, const timeval& interval)
{
    if (scheduled())
	cancel();
    _when = when;
    if (when != ASAP)
    {   (void) gettimeofday(&now, NULL);
	_when += now;
    }
    _interval = interval;
    enqueue();
}

void
Task::cancel()
{
//    assert(scheduled());
    if (scheduled())
	dequeue();
}

void
Task::run_now()
{
    //	Dequeue first; reschedule last in case task wants to change its
    //	reenqueue itself or change its scheduling.

    assert(queue == this);		// Only run 1st task in queue.
    dequeue();

    //  Run it.

    (*_proc)(*this, _closure);

    //  Reschedule it, if appropriate.

    if (!scheduled() && _interval != Once)
    {   if (_when == ASAP)
	{   (void) gettimeofday(&now, NULL);
	    _when = now;
	}
	_when += _interval;
	enqueue();
    }
}

void
Task::enqueue()
{
    Task *prev, *next;
    for (next = queue, prev = NULL; next; prev = next, next = next->_next)
	if (next->_when > _when)
	    break;

    _next = next;
    _prev = prev;
    if (next)
	next->_prev = this;
    if (prev)
	prev->_next = this;
    else
	queue = this;
    assert(scheduled());
}

void
Task::dequeue()
{
    if (_prev)
	_prev->_next = _next;
    else
	queue = _next;
    if (_next)
	_next->_prev = _prev;
    _next = _prev = NULL;
    assert(!scheduled());
}

//////////////////////////////////////////////////////////////////////////////
//  static functions

//  calc_timeout calculates the timeout to pass to select().
//  It returns:
//		NULL (if no timed tasks exist)
//		zero time (if it's time for the next task)
//		nonzero time (if it's not time yet)

timeval *
Task::calc_timeout()
{
    static timeval sleep_interval;

    if (!queue)
	return NULL;

    if (queue->_when == ASAP)
	timerclear(&sleep_interval);
    else
    {   (void) gettimeofday(&now, NULL);
	sleep_interval = queue->_when - now;
	if (sleep_interval.tv_sec < 0)
	    timerclear(&sleep_interval);
    }
    return &sleep_interval;
}

void
Task::run_tasks()
{
    while (queue)
    {   if (queue->_when == ASAP || queue->_when < now)
	    queue->run_now();
	else
	{   (void) gettimeofday(&now, NULL);
	    if (queue->_when > now)
		break;
	}
    }
}
