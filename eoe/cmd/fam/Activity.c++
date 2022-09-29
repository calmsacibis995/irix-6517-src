#include "Activity.h"

#include <unistd.h>

#include "Log.h"
#include "Scheduler.h"

unsigned Activity::idle_time = default_timeout;
unsigned Activity::count;

Activity::Activity()
{
    if (count++ == 0)
	Scheduler::remove_onetime_task(task, NULL);
}

Activity::Activity(const Activity&)
{
    Activity();
}

Activity::~Activity()
{
    if (--count == 0)
    {   timeval t;
	(void) gettimeofday(&t, NULL);
	t.tv_sec += idle_time;
	Scheduler::install_onetime_task(t, task, NULL);
    }
}

void
Activity::task(void *)
{
    Log::debug("exiting after %d second%s of inactivity",
	       idle_time, idle_time == 1 ? "" : "s");
    Scheduler::exit();
}
