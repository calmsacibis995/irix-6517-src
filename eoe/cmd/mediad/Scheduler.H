#ifndef Scheduler_included
#define Scheduler_included

#include <sys/time.h>

#include "bool.H"

//  Scheduler is a class encapsulating a simple event and timer based
//  task scheduler.  It cooperates with the Task and IOHandler class
//  (and the IOHandler subclasses, ReadHandler, WriteHandler and
//  ExceptHandler).
//
//  To use Scheduler, create and activate some tasks and I/O handlers,
//  then implement the main loop by calling Scheduler::loop().
//  To terminate the main loop, call Scheduler::exit().
//
//  Scheduler is based on select(2), obviously.
//
//  Scheduler has fixed priorities -- write events precede exception
//  events precede read events precede timed tasks.

class Scheduler {

public:

    //  Mainline code.

    static void select();
    static void exit()			{ running = false; }
    static void loop()			{ running = true;
					  while (running) select(); }

private:

    //  Class Variable

    static bool running;

    //  Private Class Methods

    static int num_fds();

    Scheduler();			// Never instantiate a Scheduler.

};

#endif /* !Scheduler_included */
