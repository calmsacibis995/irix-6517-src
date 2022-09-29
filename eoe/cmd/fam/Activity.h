#ifndef Activity_included
#define Activity_included

//  The Activity class is used to keep track of when fam is doing
//  something worthwhile, and when it isn't.  Currently, the only
//  worthwhile activity is talking to (non-internal) clients.  So an
//  Activity is enclosed in each TCP_Client.  When the last Activity
//  is destroyed, a death timer is started.  When the death timer
//  expires, fam expires too.
//
//  The duration of the timer can be set (timeout()), but there is
//  no other public interface to Activity.

class Activity {

public:

    enum { default_timeout = 5 };	// five seconds

    Activity();
    Activity(const Activity&);
    ~Activity();

    static void timeout(unsigned t)	{ idle_time = t; }

private:

    static void task(void *);

    static unsigned count;
    static unsigned idle_time;

};

#endif /* !Activity_included */
