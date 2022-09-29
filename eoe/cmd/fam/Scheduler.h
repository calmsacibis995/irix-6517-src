#ifndef Scheduler_included
#define Scheduler_included

#include <sys/time.h>

#include "Boolean.h"

//  Scheduler is a class encapsulating a simple event and timer based
//  task scheduler.  Tasks are simply subroutines that are activated
//  by calling them.
//
//  A recurring task will be activated at fixed intervals of real
//  time.  It is unpredictable when the Scheduler will first activate
//  a recurring task.
//
//  A onetime task will be activated at a particular time.  By
//  specifying the constant ASAP, the caller can have a task run as
//  soon as the program is otherwise idle.
//
//  Events are those defined by select(2) -- file descriptors that are
//  ready to be read, written or checked for exceptional conditions.
//  A handler may be installed to be called for any event on any
//  descriptor.  Only one handler may be installed for each event.
//  The installation and removal routines return the previously
//  installed handler.
//
//  Scheduler has fixed priorities -- write events precede exception
//  events precede read events precede timed tasks.
//
//  USE: There are no instances of Scheduler; all its interface
//  routines are static, and are called, e.g., "Scheduler::loop();"
//
//  RESTRICTIONS: Because we currently need only a single recurring
//  task, multiple recurring tasks are not implemented.

class Scheduler {

public:

    typedef void (*IOHandler)(int fd, void *closure);
    typedef void (*TimedProc)(void *closure);

    //  One-time tasks.

    static void install_onetime_task(const timeval& when,
				     TimedProc, void *closure);
    static void remove_onetime_task(TimedProc, void *closure);
    static const timeval ASAP;

    //  Recurring tasks.

    static void install_recurring_task(const timeval& interval,
				       TimedProc, void *closure);
    static void remove_recurring_task(TimedProc, void *closure);

    //  I/O handlers.

    static IOHandler install_read_handler(int fd, IOHandler, void *closure);
    static IOHandler remove_read_handler(int fd);

    static IOHandler install_write_handler(int fd, IOHandler, void *closure);
    static IOHandler remove_write_handler(int fd);

    static IOHandler install_except_handler(int fd, IOHandler, void *closure);
    static IOHandler remove_except_handler(int fd);

    //  Mainline code.

    static void select();
    static void exit()			{ running = false; }
    static void loop()			{ running = true;
					  while (running) select(); }

private:

    //  Per-filedescriptor info is the set of three handlers and their
    //  closures.

    struct FDInfo {
	struct handler {
	    IOHandler handler;
	    void *closure;
	} read, write, except;
    };

    //  Per-I/O type info is the file descriptor set passed to select(),
    //  the number of bits set there, and the offset into the FDInfo
    //  for the corresponding I/O type.

    struct IOTypeInfo {
	FDInfo::handler FDInfo::*const iotype;
	unsigned int nbitsset;		// number of bits set in fds
	fd_set fds;
	IOTypeInfo(FDInfo::handler FDInfo::* a_iotype) : iotype(a_iotype) { }
    };

    struct onetime_task {
	onetime_task *next;
	timeval when;
	Scheduler::TimedProc proc;
	void *closure;
    };

    // I/O event related variables

    static IOTypeInfo read, write, except;
    static FDInfo *fdinfo;
    static unsigned int nfds;
    static unsigned int nfdinfo_alloc;

    // Timed task related variables

    static unsigned int ntasks;
    static timeval next_task_time;
    static TimedProc recurring_proc;
    static void *recurring_closure;
    static timeval recurring_interval;
    static onetime_task *first_task;
    static Boolean running;

    // I/O event related functions

    static FDInfo *fd_to_info(int fd);
    static void trim_fdinfo(int fd);
    static IOHandler install_io_handler(int fd,
					IOHandler handler, void *closure,
					IOTypeInfo *iotype);
    static IOHandler remove_io_handler(int fd, IOTypeInfo *iotype);
    static void handle_io(const fd_set *fds, FDInfo::handler FDInfo::* iotype);
    static fd_set *calc_fds(fd_set& fds, IOTypeInfo *iotype);

    // Timed task related functions

    static void do_tasks();
    static timeval *calc_timeout();

    Scheduler();			// Never instantiate a Scheduler.

};

#endif /* !Scheduler_included */
