/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSJobSchedule.H                                    */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:21:23 1995                            */
/****************************************************************/


#ifndef BPSJOBSCHEDULE_HEADER
#define BPSJOBSCHEDULE_HEADER

#include <BPSJob.H>
#include <BPS_lib.H>
#include <BPSServer.H>
#include <stl.h>
#ifndef SCHEDULENDEBUG
#include <iostream.h>
#endif

/****************************************************************
 * CLASS NAME : BPSJobSchedule

  The BPSJobSchedule maintains a data structure of jobs ordered by
  their magic cookies. 
   
****************************************************************/


class BPSJobSchedule {
   BPSJobSchedule(BPSJobSchedule&);
   BPSJobSchedule& operator=(BPSJobSchedule&);
protected:
typedef multimap<bps_magic_cookie_t,BPSPtrWrapper<BPSJob>,BPSCookieCmp > jobmap;
typedef pair< const bps_magic_cookie_t, BPSPtrWrapper<BPSJob> > pair_type; 
   jobmap schedule_; // schedule of jobs ordered by the magic cookie
   RWLock_writer lock_;
 public:
   BPSJobSchedule(); 
   // adds a job to the job schedule
   error_t addJob(BPSJob* job);
    // deletes a job from the schedule, but does not actually delete the job
   error_t deleteJob(BPSJob*& job, bps_magic_cookie_t magic_cookie);
    // returns the job with the given time
   error_t queryTime(const bps_time_t& tv,
                    BPSJob*& job);

#ifndef SCHEDULENDEBUG
friend ostream& operator<< (ostream& os, BPSJobSchedule& schedule);
#endif
   
   bool_t rlock();
   bool_t wlock();
   bool_t unlock();
};

   


#endif
