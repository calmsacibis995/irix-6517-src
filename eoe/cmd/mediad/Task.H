#ifndef Task_included
#define Task_included

#include <sys/time.h>

#include "bool.H"

//  A Task is a procedure that is called at a specified time or interval,
//  or immediately when the program is otherwise idle.
//
//  To arrange for a Task to be run at a specified time, do this.
//
//	timeval when = { ... };
//	task->schedule(when);
//
//  To arrange for a Task to be run at a specified time and thereafter
//  at a specified interval, do this.
//
//	timeval when = { ... };
//	timeval interval = { ... };
//	task->schedule(when, interval);
//
//  To arrange for a Task to be run as soon as the program is idle,
//  do this.
//
//	task->schedule(Scheduler::ASAP);
//
//  A Task, once scheduled, can be cancelled.  The Boolean function
//  scheduled() reports whether the task has been scheduled.
//
//  run_tasks() and calc_timeout() are primarily for use by the Scheduler
//  class.

class Task {

public:

    typedef void (*TaskProc)(Task&, void *closure);

    static const timeval ASAP, Once;

    Task(TaskProc, void *closure);
    ~Task();

    void schedule(const timeval& when, const timeval& interval = Once);
    void cancel();
    bool scheduled()			{ return _prev || queue == this; }

    static void run_tasks();
    static timeval *calc_timeout();

private:

    //  Instance Variables

    Task *_next;
    Task *_prev;
    timeval _when;
    timeval _interval;

    TaskProc _proc;
    void *_closure;

    static Task *queue;			// Run queue
    static timeval now;			// current time of day

    void run_now();			// Run this task now.
    void enqueue();			// Put this task onto run queue
    void dequeue();			// Remove this task from run queue

};

#endif /* !Task_included */
