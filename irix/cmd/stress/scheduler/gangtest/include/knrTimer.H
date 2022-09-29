#ifndef knrTIMER__HEADER
#define knrTIMER__HEADER

#include <sys/time.h>
#include <iostream.h>

/*******************************************************************************

 The following classes are wrappers around the UNIX time system
 to make my life easier when timing things.

 knrTime acts as a wrapper aroudn the time struct
 knrTimer is a timer that can be started, stopped, restarted and passed
 around. Difference in times is always reported as seconds.
******************************************************************************/
class knrTime {
protected:
  struct timeval time_;
public:
  knrTime(); // initializes time values using gettimeofday
  ~knrTime(); // does nothing
  knrTime(const knrTime& knr_time); // preserves time
  const knrTime& operator=(const knrTime& knr_time);
  double sec() const ; // returns the time as seconds
  double diff_sec(const knrTime& knr_time) const; // returns difference in 
                                                  // seconds
  friend ostream& operator<<(ostream& os, const knrTime& time);
};


class knrTimer {
protected:
  knrTime *start_;
  knrTime *end_;
public:
  knrTimer(); // starts the timer
  knrTimer(const knrTime& start_time);
  ~knrTimer();
  knrTimer(const knrTimer& timer); // new timer has start time of old timer
                             // useful for computing different times
                             // from events starting at the same 
                             // approximate time
  const knrTimer& operator=(const knrTimer& knr_time);
  void ReStart(); // restarts the timer
  void Stop(); // stops the timer
  double ElapsedTime() const; // returns the result of elapsed time
  knrTime EndTime(); // return the end time
  friend ostream& operator<<(ostream& os, const knrTimer& time);
  double diff_sec(const knrTimer& timer) const;
};

#endif  

