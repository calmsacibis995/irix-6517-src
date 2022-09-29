#include "Pollster.h"

#include "Interest.h"
#include "Log.h"
#include "Scheduler.h"
#include "ServerHost.h"

Boolean		  Pollster::remote_polling_enabled = true;
timeval		  Pollster::pintvl = { DEFAULT_INTERVAL, 0 };
Set<Interest *>	  Pollster::polled_interests;
Set<ServerHost *> Pollster::polled_hosts;
Boolean		  Pollster::polling = false;

void
Pollster::watch(Interest *ip)
{
    polled_interests.insert(ip);

    if (!polling)
	start_polling();
}

void
Pollster::forget(Interest *ip)
{
    int old_size = polled_interests.size();
    (void) polled_interests.remove(ip);
    int new_size = polled_interests.size();

    if (old_size && !new_size && !polled_hosts.size())
	stop_polling();
}

void
Pollster::watch(ServerHost *ip)
{
    if (remote_polling_enabled)
    {   polled_hosts.insert(ip);

	if (!polling)
	    start_polling();
    }
}

void
Pollster::forget(ServerHost *ip)
{
    int old_size = polled_hosts.size();
    (void) polled_hosts.remove(ip);
    int new_size = polled_hosts.size();

    if (old_size && !new_size && !polled_interests.size())
	stop_polling();
}

//////////////////////////////////////////////////////////////////////////////

void
Pollster::start_polling()
{
    assert(!polling);
    assert(polled_interests.size() || polled_hosts.size());
    Log::debug("polling every %d seconds", pintvl.tv_sec);
    (void) Scheduler::install_recurring_task(pintvl, polling_task, NULL);
    polling = true;
}

void
Pollster::stop_polling()
{
    assert(polling);
    assert(!polled_interests.size());
    assert(!polled_hosts.size());
    (void) Scheduler::remove_recurring_task(polling_task, NULL);
    polling = false;
    Log::debug("will stop polling");
}

void
Pollster::polling_task(void *)
{
    timeval t0, t1;
    (void) gettimeofday(&t0, NULL);

    int ni = 0;
    for (Interest *ip = polled_interests.first();
	 ip;
	 ip = polled_interests.next(ip))
    {
	ip->poll();
	ni++;
    }

    int nh = 0;
    if (remote_polling_enabled)
	for (ServerHost *hp = polled_hosts.first();
	     hp;
	     hp = polled_hosts.next(hp))
	{
	    hp->poll();
	    nh++;
	}

    if (ni || nh)
    {   (void) gettimeofday(&t1, NULL);
	double t = t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec)/1000000.0;
	Log::debug("polled %d interest%s and %d host%s in %.3f seconds",
		   ni, ni == 1 ? "" : "s",
		   nh, nh == 1 ? "" : "s",
		   t);
    }
}
